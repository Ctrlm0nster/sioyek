#!/usr/bin/env python3
"""Apply reviewed patches in a build tree, leaving the pinned submodule intact."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
parser.add_argument('destination', type=Path)
parser.add_argument('patches', nargs='+', type=Path)
args = parser.parse_args()
source, destination = args.source.resolve(), args.destination.resolve()
if source == destination or source in destination.parents or destination in source.parents:
    parser.error('source and destination must be separate trees')
if destination.exists() and not (destination / '.sioyek-patched-tree').is_file():
    parser.error('refusing to reuse an unmanaged destination')
destination.mkdir(parents=True, exist_ok=True)
(destination / '.sioyek-patched-tree').touch()

patched = set()
for patch in args.patches:
    for name in re.findall(r'^\+\+\+ b/([^\t\n]+)', patch.read_text(), re.M):
        path = Path(name)
        if path.is_absolute() or '..' in path.parts or path.parts[0] not in ('source', 'include', 'thirdparty'):
            parser.error(f'unsupported patch path: {name}')
        patched.add(path)

def copy_changed(src, dst):
    # A newly patched file may have been an unmodified dependency symlink in
    # an earlier build. Never write through that link into the submodule.
    if dst.is_symlink():
        dst.unlink()
    if not dst.exists() or src.read_bytes() != dst.read_bytes():
        shutil.copyfile(src, dst)
        shutil.copymode(src, dst)
        shutil.copystat(src, dst)

# Keep writable/generated sources separate. Large, unchanged dependency and
# font directories are read through symlinks; Make writes all objects to OUT.
def mirror(src, dst, relative=Path()):
    names = {entry.name for entry in src.iterdir() if entry.name not in ('.git', 'build')}
    for entry in dst.iterdir():
        if entry.name not in names and entry.name != '.sioyek-patched-tree':
            if entry.is_dir() and not entry.is_symlink():
                shutil.rmtree(entry)
            else:
                entry.unlink()
    for name in sorted(names):
        item, target, rel = src / name, dst / name, relative / name
        if rel in patched:
            continue
        writable = rel.parts[0] in ('source', 'include', 'generated')
        parent_of_patch = any(rel in path.parents for path in patched)
        if item.is_dir() and (writable or parent_of_patch):
            if target.is_symlink():
                target.unlink()
            target.mkdir(exist_ok=True)
            mirror(item, target, rel)
        elif writable or (item.is_file() and len(rel.parts) == 1):
            copy_changed(item, target)
        elif not target.is_symlink() or target.readlink() != item:
            if target.is_dir() and not target.is_symlink():
                shutil.rmtree(target)
            elif target.exists() or target.is_symlink():
                target.unlink()
            target.symlink_to(item, target_is_directory=item.is_dir())

mirror(source, destination)
with tempfile.TemporaryDirectory(prefix='sioyek-mupdf-patch-') as temporary:
    work = Path(temporary)
    for rel in patched:
        (work / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / rel, work / rel)
    for patch in args.patches:
        subprocess.run(['patch', '--batch', '--fuzz=0', '-p1', '-i', str(patch.resolve())],
                       cwd=work, check=True)
    for rel in patched:
        copy_changed(work / rel, destination / rel)

# Upstream MuPDF provides pre-generated headers for ICC profiles, CMaps, and JS
# so builds do not require host generator tools (xxd, etc.). Ensure these headers
# are never older than their input resources.
for pattern, res_dir in [
    ('source/fitz/icc/*.icc.h', 'resources/icc'),
    ('source/pdf/cmaps/*.h', 'resources/cmaps'),
    ('source/pdf/js/*.js.h', 'source/pdf/js'),
]:
    for header in destination.glob(pattern):
        res_file = destination / res_dir / header.name[:-2]
        if res_file.exists():
            r_mtime = res_file.stat().st_mtime
            if header.stat().st_mtime <= r_mtime:
                os.utime(header, (r_mtime + 1, r_mtime + 1))
