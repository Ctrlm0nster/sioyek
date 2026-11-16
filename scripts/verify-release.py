#!/usr/bin/env python3
"""Inspect a real package and its full source snapshot without installing either.
Requires Python >=3.14 (zstd tar support), Git, ldd and desktop-file-validate.
Run from the matching checkout to check every tracked recursive dependency file.
"""
import hashlib
import os
import re
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile

if len(sys.argv) != 3:
    sys.exit('Usage: scripts/verify-release.py main.pkg.tar.zst sioyek-source.tar.gz')
package, source = map(Path, sys.argv[1:])
root = Path(__file__).resolve().parents[1]
with tarfile.open(package) as tar:
    names = set(tar.getnames())
    required = ['usr/bin/sioyek', 'usr/share/sioyek/prefs.config', 'usr/share/sioyek/keys.config',
                'usr/share/sioyek/tutorial.pdf', 'usr/share/sioyek/shaders/simple.vertex',
                'usr/share/sioyek/shaders/simple.fragment', 'usr/share/applications/sioyek.desktop',
                'usr/share/pixmaps/sioyek-icon-linux.png', 'usr/share/man/man1/sioyek.1.gz',
                'usr/share/licenses/sioyek-ctrlm0nster/LICENSE',
                'usr/share/licenses/sioyek-ctrlm0nster/MuPDF-COPYING',
                'usr/share/licenses/sioyek-ctrlm0nster/JetBrainsMono-OFL.txt',
                'usr/share/licenses/sioyek-ctrlm0nster/fzf-LICENSE',
                'usr/share/licenses/sioyek-ctrlm0nster/RapidFuzz-LICENSE',
                'usr/share/licenses/sioyek-ctrlm0nster/THIRD_PARTY_NOTICES.txt',
                'usr/share/doc/sioyek-ctrlm0nster/SOURCE-MANIFEST.txt', '.BUILDINFO', '.PKGINFO']
    assert set(required) <= names, set(required)-names
    metadata = tar.extractfile('.PKGINFO').read().decode()
    assert 'pkgname = sioyek-ctrlm0nster\n' in metadata
    for conflict in ('sioyek', 'sioyek-git'):
        assert f'conflict = {conflict}\n' in metadata
    assert not any(n.startswith(('etc/', 'home/')) for n in names)
    assert '.INSTALL' not in names
    for archived, local in (
        ('usr/share/licenses/sioyek-ctrlm0nster/LICENSE', 'LICENSE'),
        ('usr/share/licenses/sioyek-ctrlm0nster/MuPDF-COPYING', 'mupdf/COPYING'),
        ('usr/share/licenses/sioyek-ctrlm0nster/JetBrainsMono-OFL.txt', 'resources/fonts/OFL.txt'),
        ('usr/share/licenses/sioyek-ctrlm0nster/fzf-LICENSE', 'fzf/LICENSE'),
        ('usr/share/licenses/sioyek-ctrlm0nster/RapidFuzz-LICENSE', 'pdf_viewer/rapidfuzz-LICENSE'),
        ('usr/share/licenses/sioyek-ctrlm0nster/THIRD_PARTY_NOTICES.txt', 'packaging/arch/THIRD_PARTY_NOTICES.txt'),
    ):
        assert tar.extractfile(archived).read() == (root/local).read_bytes(), archived
    package_manifest = tar.extractfile('usr/share/doc/sioyek-ctrlm0nster/SOURCE-MANIFEST.txt').read().decode()
    with tempfile.TemporaryDirectory(prefix='sioyek-artifact-') as tmp:
        tmp = Path(tmp)
        for name in ('usr/bin/sioyek', 'usr/share/applications/sioyek.desktop'):
            path = tmp/Path(name).name
            path.write_bytes(tar.extractfile(name).read())
        binary = tmp/'sioyek'
        binary.chmod(0o755)
        subprocess.run([str(binary), '--version'], check=True)
        deps = subprocess.check_output(['ldd', str(binary)], text=True)
        assert 'not found' not in deps, deps
        headers = subprocess.check_output(['readelf', '-h', str(binary)], text=True)
        segments = subprocess.check_output(['readelf', '-W', '-l', str(binary)], text=True)
        dynamic = subprocess.check_output(['readelf', '-d', str(binary)], text=True)
        assert re.search(r'Type:\s+DYN', headers), 'Executable is not PIE'
        assert 'GNU_RELRO' in segments and 'BIND_NOW' in dynamic, 'Full RELRO missing'
        assert re.search(r'GNU_STACK[^\n]*\sRW\s', segments), 'Stack is executable or missing its policy'
        subprocess.run(['desktop-file-validate', str(tmp/'sioyek.desktop')], check=True)
print('PASS package paths, conflicts, notices, runtime/desktop integration and ELF hardening')

# Check all tracked files of every recursive submodule, including MuPDF fonts
# and bundled libraries, against the initialized checkout. No extraction needed.
expected = {}
revisions = {}
def dependencies(repo, prefix=''):
    records = subprocess.check_output(['git', '-C', str(repo), 'ls-files', '--stage', '-z']).decode().split('\0')
    for record in filter(None, records):
        spec, name = record.split('\t', 1)
        mode, commit, stage = spec.split()
        assert stage == '0', f'Unmerged source: {prefix+name}'
        path = repo/name
        rel = prefix+name
        if mode == '160000':
            revisions[rel] = commit
            dependencies(path, rel+'/')
        elif prefix:
            expected[rel] = ('symlink', os.readlink(path)) if path.is_symlink() else ('sha256', hashlib.sha256(path.read_bytes()).digest())
dependencies(root)
seen = set()
manifest = ''
source_license = None
archive_names = set()
with tarfile.open(source, 'r|gz') as tar:
    for member in tar:
        assert member.name not in archive_names, member.name
        archive_names.add(member.name)
        assert not member.name.startswith('/') and '..' not in Path(member.name).parts, member.name
        assert '.git' not in Path(member.name).parts, member.name
        assert member.name.startswith('sioyek-source/'), member.name
        name = member.name.removeprefix('sioyek-source/')
        if name == 'LICENSE':
            source_license = tar.extractfile(member).read()
        if name == 'SOURCE-MANIFEST.txt':
            manifest = tar.extractfile(member).read().decode()
        if name in expected:
            kind, value = expected[name]
            if kind == 'symlink':
                assert member.issym() and member.linkname == value, name
            else:
                assert member.isfile() and hashlib.sha256(tar.extractfile(member).read()).digest() == value, name
            seen.add(name)
assert source_license == (root/'LICENSE').read_bytes()
assert package_manifest == manifest
assert seen == expected.keys(), expected.keys()-seen
for path, commit in revisions.items():
    assert f'{path}/ {commit}\n' in manifest, path
print(f'PASS {len(seen)} tracked dependency files/symlinks and {len(revisions)} pinned revisions in source archive')
