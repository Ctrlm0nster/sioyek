#!/usr/bin/env python3
"""Snapshot working sources and exact recursive gitlinks for offline makepkg."""
import argparse
import gzip
import hashlib
import io
import os
from pathlib import Path
import re
import subprocess
import tarfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=root / 'build/arch')
parser.add_argument('--release', action='store_true', help='require a clean, fully pinned checkout')
args = parser.parse_args()
out = args.output.resolve()
def git(path, *args):
    return subprocess.check_output(['git', '-C', str(path), *args])

if git(root, 'rev-parse', '--is-shallow-repository').strip() == b'true':
    raise SystemExit('Full history required for versioning: git fetch --unshallow')
if args.release:
    if git(root, 'status', '--porcelain', '--untracked-files=all', '--ignore-submodules=none').strip():
        raise SystemExit('Release snapshot requires a clean parent and recursive submodules')
    if any(line[:1] != b' ' for line in git(root, 'submodule', 'status', '--recursive').splitlines()):
        raise SystemExit('Release snapshot requires initialized submodules at recorded gitlinks')
out.mkdir(parents=True, exist_ok=True)

# Include visible uncommitted additions in the parent; dependencies use tracked
# files only, so ignored/generated MuPDF files cannot enter a release snapshot.
files = []
manifest = []
def collect(repo, prefix='', parent=False):
    revision = git(repo, 'rev-parse', 'HEAD').decode().strip()
    manifest.append(f'{prefix or "."} {revision}')
    if git(repo, 'diff', '--name-only', 'HEAD').strip():
        manifest.append(f'{prefix or "."}: tracked working changes included')
    names = git(repo, 'ls-files', '-z', '--cached', *(['--others', '--exclude-standard'] if parent else []))
    for name in sorted(set(names.decode().split('\0')) - {''}):
        path = repo / name
        rel = prefix + name
        if path.is_dir() and not path.is_symlink():
            if (path / '.git').exists():
                collect(path, rel + '/')
            else:
                raise SystemExit(f'Missing submodule {rel}: git submodule update --init --recursive')
        elif path.exists() or path.is_symlink():
            files.append((path, rel))
        elif parent:
            manifest.append(f'{rel}: omitted (deleted in working tree)')
        else:
            raise SystemExit(f'Missing tracked source: {rel}')
collect(root, parent=True)
base = (root / 'VERSION').read_text().strip()
if not re.fullmatch(r'\d+\.\d+\.\d+', base):
    raise SystemExit('VERSION must contain MAJOR.MINOR.PATCH')
count = git(root, 'rev-list', '--count', 'HEAD').decode().strip()
rev = git(root, 'rev-parse', '--short=12', 'HEAD').decode().strip()
version = f'{base}.r{count}.g{rev}'
epoch = int(os.environ.get('SOURCE_DATE_EPOCH', git(root, 'show', '-s', '--format=%ct', 'HEAD').decode().strip()))
archive = out / 'sioyek-source.tar.gz'
with archive.open('wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', filename='', mtime=epoch) as gz, tarfile.open(fileobj=gz, mode='w|') as tar:
    for path, rel in sorted(files, key=lambda item: item[1]):
        info = tar.gettarinfo(str(path), arcname='sioyek-source/' + rel)
        info.uid = info.gid = 0
        info.uname = info.gname = ''
        info.mtime = epoch
        if info.isfile():
            info.mode = 0o755 if path.stat().st_mode & 0o100 else 0o644
            with path.open('rb') as source:
                tar.addfile(info, source)
        else:
            if info.issym():
                info.mode = 0o777
            tar.addfile(info)
    data = ('\n'.join(manifest) + '\n').encode()
    info = tarfile.TarInfo('sioyek-source/SOURCE-MANIFEST.txt')
    info.size = len(data)
    info.mtime = epoch
    info.mode = 0o644
    tar.addfile(info, io.BytesIO(data))
with archive.open('rb') as source:
    checksum = hashlib.file_digest(source, 'sha256').hexdigest()
recipe = (root / 'packaging/arch/PKGBUILD').read_text()
recipe = re.sub(r'^pkgver=.*$', f'pkgver={version}', recipe, flags=re.M)
recipe = recipe.replace('GENERATED_BY_PREPARE_SCRIPT', checksum)
(out / 'PKGBUILD').write_text(recipe)
print(f'Prepared {version}: {archive}\nSHA256 {checksum}')
print('Review snapshot contents before sharing; unignored parent additions are included.')
