"""Check dependency patch staging never writes through links into original data."""
from pathlib import Path
import difflib
import subprocess
import sys
import tempfile
import unittest

script = Path(sys.argv.pop()).resolve()

class StagingTest(unittest.TestCase):
    def test_patch_changes_preserve_originals_and_incremental_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            work = Path(temp)
            source, destination = work / 'original', work / 'staged'
            first = Path('source/fitz/first.c')
            second = Path('thirdparty/example/second.c')
            for rel in (first, second):
                (source / rel).parent.mkdir(parents=True, exist_ok=True)
                (source / rel).write_text('original\n')
            patches = []
            for index, rel in enumerate((first, second)):
                patch = work / f'{index}.patch'
                patch.write_text(''.join(difflib.unified_diff(
                    ['original\n'], ['patched\n'], fromfile=f'a/{rel}', tofile=f'b/{rel}')))
                patches.append(patch)
            def prepare(selected):
                subprocess.run([sys.executable, str(script), str(source), str(destination),
                                *map(str, selected)], check=True, capture_output=True)
            prepare(patches[:1])
            # Expand from an entirely linked dependency to one real directory,
            # then patch a previously linked file within that directory.
            third = Path('thirdparty/example/third.c')
            (source / third).write_text('original\n')
            prepare(patches)
            self.assertEqual((destination / second).read_text(), 'patched\n')
            fourth_patch = work / 'third.patch'
            fourth_patch.write_text(patches[1].read_text().replace(str(second), str(third)))
            prepare([*patches, fourth_patch])
            for rel in (first, second, third):
                self.assertEqual((source / rel).read_text(), 'original\n')
                self.assertEqual((destination / rel).read_text(), 'patched\n')
            times = [(destination / rel).stat().st_mtime_ns for rel in (first, second, third)]
            prepare([*patches, fourth_patch])
            self.assertEqual(times, [(destination / rel).stat().st_mtime_ns
                                     for rel in (first, second, third)])
            prepare(patches[:1])
            self.assertEqual((destination / second).read_text(), 'original\n')
            self.assertEqual((destination / third).read_text(), 'original\n')
            unmanaged = work / 'unmanaged'
            unmanaged.mkdir()
            sentinel = unmanaged / 'keep'
            sentinel.write_text('keep\n')
            result = subprocess.run([sys.executable, str(script), str(source), str(unmanaged),
                                     str(patches[0])], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(sentinel.read_text(), 'keep\n')

if __name__ == '__main__':
    unittest.main()
