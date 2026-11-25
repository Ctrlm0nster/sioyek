#!/usr/bin/env python3
"""Run pacman only in an unprivileged user namespace against a disposable root.
Requires Linux user namespaces, pacman, Python >=3.14 (zstd tar support).
Dependency resolution, hooks and GUI startup are separate tests.
Upstream/debug conflicts use explicit metadata fixtures, not upstream binaries.
"""
import hashlib
import io
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tarfile
import tempfile

if len(sys.argv) != 2:
    sys.exit('Usage: scripts/test-package-lifecycle.py package.pkg.tar.zst')
package = Path(sys.argv[1]).resolve()
if os.environ.get('SIOYEK_LIFECYCLE_NAMESPACE') != '1':
    if os.geteuid() == 0:
        sys.exit('Run as a normal user, never with sudo')
    env = dict(os.environ, SIOYEK_LIFECYCLE_NAMESPACE='1')
    sys.exit(subprocess.call(['unshare', '--user', '--map-root-user', sys.executable,
                             str(Path(__file__).resolve()), str(package)], env=env))
# Verify mapping rather than trusting the environment marker alone.
mapping = Path('/proc/self/uid_map').read_text().split()
assert len(mapping) == 3 and mapping[0] == '0' and mapping[1] != '0' and mapping[2] == '1', mapping
work = Path(tempfile.mkdtemp(prefix='sioyek-lifecycle-'))
root = work / 'root'
for name in ('var/lib/pacman', 'var/cache/pacman/pkg', 'var/log', 'etc', 'hooks', 'home/test/.config/sioyek', 'home/test/.local/share/sioyek'):
    (root / name).mkdir(parents=True, exist_ok=True)
config = work / 'pacman.conf'
config.write_text('[options]\nArchitecture = auto\nSigLevel = Never\nLocalFileSigLevel = Never\n')
base = ['pacman', '--root', str(root), '--dbpath', str(root/'var/lib/pacman'),
        '--config', str(config), '--logfile', str(root/'var/log/pacman.log'),
        '--cachedir', str(root/'var/cache/pacman/pkg'), '--hookdir', str(root/'hooks'), '--noconfirm']
def pac(*args, success=True):
    result = subprocess.run(base + list(args), capture_output=True, text=True)
    with (work/'transactions.log').open('a') as log:
        log.write(' '.join(args) + '\n' + result.stdout + result.stderr)
    assert (result.returncode == 0) == success, result.stdout + result.stderr
    return result

# These files are generated test data, never copied from the user's HOME.
prefs = root/'home/test/.config/sioyek/prefs_user.config'
prefs.write_text('zoom_inc_factor 1.5\n')
(root/'home/test/.config/sioyek/keys_user.config').write_text('next_page x\n')
for name in ('test.db', 'local.db', 'shared.db'):
    with sqlite3.connect(root/'home/test/.local/share/sioyek'/name) as db:
        db.execute('create table sentinel (bookmark text)')
        db.execute("insert into sentinel values ('preserve-test-bookmark')")
def user_hashes():
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (root/'home').rglob('*') if p.is_file()}
original = user_hashes()

def fixture(name, version='1-1', conflicts=(), provides=(), file='usr/bin/sioyek'):
    path = work/(name + '.pkg.tar.gz')
    info = f'pkgname = {name}\npkgver = {version}\npkgdesc = Lifecycle fixture\narch = x86_64\nsize = 16\n'
    info += ''.join(f'conflict = {x}\n' for x in conflicts)
    info += ''.join(f'provides = {x}\n' for x in provides)
    with tarfile.open(path, 'w:gz') as tar:
        for entry, data in (('.PKGINFO', info.encode()), (file, b'fixture only\n')):
            member = tarfile.TarInfo(entry)
            member.size = len(data)
            member.mode = 0o755 if entry == file else 0o644
            tar.addfile(member, io.BytesIO(data))
    return path

with tarfile.open(package) as tar:
    metadata = tar.extractfile('.PKGINFO').read().decode()
name = next(s.split(' = ', 1)[1] for s in metadata.splitlines() if s.startswith('pkgname = '))
assert name == 'sioyek-ctrlm0nster', name
# -dd is intentional: this root has no runtime dependencies. Conflict checking
# remains active; there are no host hooks or scriptlets in these transactions.
pac('-Udd', '--noscriptlet', str(package))
assert (root/'usr/bin/sioyek').is_file()
assert user_hashes() == original
print('PASS fresh install and user-data hashes')
debug_package = package.with_name(package.name.replace('sioyek-ctrlm0nster-', 'sioyek-ctrlm0nster-debug-', 1))
if debug_package.exists():
    pac('-Udd', '--noscriptlet', str(debug_package))
    for link in (root/'usr/lib/debug').rglob('*'):
        if link.is_symlink():
            target = link.resolve()
            assert target.is_relative_to(root) and target.exists(), link
    print('PASS actual matching debug package installation and symlink resolution')
# Same binary/resources, higher pkgrel: tests the package manager upgrade path.
version = next(s.split(' = ', 1)[1] for s in metadata.splitlines() if s.startswith('pkgver = '))
newversion = version.rsplit('-', 1)[0] + '-' + str(int(version.rsplit('-', 1)[1]) + 1)
upgrade = work/'upgrade.pkg.tar.gz'
with tarfile.open(package) as source, tarfile.open(upgrade, 'w:gz') as dest:
    for member in source:
        if member.name in ('.MTREE', '.BUILDINFO'):
            continue
        data = source.extractfile(member).read() if member.isfile() else None
        if member.name == '.PKGINFO':
            data = metadata.replace(f'pkgver = {version}\n', f'pkgver = {newversion}\n').encode()
            member.size = len(data)
        dest.addfile(member, io.BytesIO(data) if data is not None else None)
pac('-Udd', '--noscriptlet', str(upgrade))
assert newversion in pac('-Q', name).stdout
assert user_hashes() == original
print('PASS metadata-only pkgrel upgrade and user-data hashes')

for upstream in ('sioyek', 'sioyek-git'):
    # No overlapping files: this confirms metadata conflicts, not just file collisions.
    upstream_package = fixture(upstream, provides=('sioyek',), file='usr/share/lifecycle-fixture/'+upstream)
    result = pac('-Udd', '--noscriptlet', str(upstream_package), success=False)
    assert 'conflict' in (result.stdout + result.stderr).lower()
    print('PASS rejection of conflicting', upstream, 'metadata fixture')
# Debug packages are identified by ELF build IDs; distinct builds may coexist.
# An identical build ID must collide, rather than silently overwrite debug data.
for debug in ('sioyek-debug', 'sioyek-git-debug', name+'-debug'):
    path = fixture(debug, file='usr/lib/debug/.build-id/aa/lifecycle.debug')
    if debug == 'sioyek-debug':
        pac('-Udd', '--noscriptlet', str(path))
    else:
        result = pac('-Udd', '--noscriptlet', str(path), success=False)
        assert 'exists in filesystem' in result.stderr
    print('PASS debug fixture', debug)
pac('-Rdd', '--noscriptlet', 'sioyek-debug')
if debug_package.exists():
    pac('-Rdd', '--noscriptlet', name+'-debug')
pac('-Rdd', '--noscriptlet', name)
for upstream in ('sioyek', 'sioyek-git'):
    path = fixture(upstream, provides=('sioyek',), file='usr/share/lifecycle-fixture/'+upstream)
    pac('-Udd', '--noscriptlet', str(path))
    result = pac('-Udd', '--noscriptlet', str(package), success=False)
    assert 'conflict' in (result.stdout + result.stderr).lower()
    pac('-Rdd', '--noscriptlet', upstream)
    print('PASS package conflict with installed', upstream, 'metadata fixture')
assert not (root/'usr/bin/sioyek').exists()
assert user_hashes() == original
for dbfile in (root/'home').rglob('*.db'):
    with sqlite3.connect(dbfile) as db:
        assert db.execute('pragma integrity_check').fetchone()[0] == 'ok'
print('PASS package removal preserves preferences, bindings and SQLite bytes')
print('Evidence:', work)
