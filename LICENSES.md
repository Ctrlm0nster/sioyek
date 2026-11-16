# Third-party source and license inventory

Upstream remains [ahrm/Sioyek](https://github.com/ahrm/sioyek), with the original
GPL-3.0 license and attribution retained. No rebranding or license text rewrite is
part of this candidate. MuPDF remains pinned to the repository gitlink and carries
its AGPL-3.0 notice; package metadata records both GPL and AGPL licensing.

The source snapshot exports all tracked files of all recursive dependencies, not
just the C/C++ files currently compiled. This retains font resources, build files,
licenses, copyright notices and subdependency sources. Representative paths:

| Component | Notice/source location |
| --- | --- |
| Sioyek | LICENSE, README.md, pdf_viewer/ |
| MuPDF | mupdf/COPYING, mupdf/include/, mupdf/source/, mupdf/resources/ |
| FreeType | mupdf/thirdparty/freetype/LICENSE.TXT |
| HarfBuzz | mupdf/thirdparty/harfbuzz/COPYING (system version linked) |
| MuJS | mupdf/thirdparty/mujs/COPYING |
| JBIG2Dec | mupdf/thirdparty/jbig2dec/COPYING, LICENSE |
| Little CMS | mupdf/thirdparty/lcms2/LICENSE and plugin notices |
| Brotli | mupdf/thirdparty/brotli/LICENSE |
| OpenJPEG | mupdf/thirdparty/openjpeg/LICENSE |
| Gumbo | mupdf/thirdparty/gumbo-parser/COPYING |
| Extract | mupdf/thirdparty/extract/COPYING |
| zlib | mupdf/thirdparty/zlib/LICENSE (system version linked) |
| Additional bundled components | respective thirdparty directories and notices |

The package includes Sioyek LICENSE and MuPDF COPYING under
/usr/share/licenses/sioyek-ctrlm0nster. The full corresponding source snapshot and
concrete build recipe must accompany release binaries; two installed notice files
alone are not a complete source distribution. Distribution Qt/SQLite/HarfBuzz/zlib
libraries are linked from the system rather than copied into the package.

`scripts/verify-release.py` checks installed notice bytes, matching manifests and
all tracked recursive dependency files against the matching checkout. This is
concrete source/notice verification, not an independent legal certification. GNU's
[license FAQ](https://www.gnu.org/licenses/gpl-faq.html) and
[AGPL text](https://www.gnu.org/licenses/agpl-3.0.html) describe GPLv3/AGPLv3
combinations and corresponding-source requirements; preserve both sets of terms.
No signing identity or release signature has been created by the preparation work.

## Notice restoration evidence

The FZF C source and header match telescope-fzf-native.nvim revision
`6c921ca12321edaa773e324ef64ea301a1d0da62` byte-for-byte at Sioyek's original
import (`4f9866bb`). Its MIT license is restored under fzf/LICENSE. The RapidFuzz
header matches development revision `1a29b162cd374a86a980dbb3772de9f08c9bc6da`
except for four removed internal includes that make the amalgamation standalone;
its upstream MIT notice is restored under pdf_viewer/rapidfuzz-LICENSE. The stale
`v1.0.2` label alone was not used as proof of an exact release match.

The binary package now includes JetBrains Mono's OFL, FZF and RapidFuzz notices,
UTF-8/SyncTeX/FTS header notices, and the available bundled MuPDF library/font
notice files with component paths retained. The existing windows_runtime DLLs
remain untouched in Git but are excluded from the Arch source export: they are
not used by this Linux build, and their redistribution was not audited here.
