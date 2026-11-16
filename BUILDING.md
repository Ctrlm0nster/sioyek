# Building and maintaining Sioyek on Arch Linux

CMake + Ninja is the preferred Linux development workflow. The qmake scripts
remain for older packaging workflows; `build_linux.sh` removes `build/`, so do
not mix it with the development workflow below.

## Dependencies

Install these packages yourself if missing (this project does not install them):

```sh
sudo pacman -S --needed base-devel git cmake ninja pkgconf qt6-base \
  qt6-declarative qt6-svg qt6-speech qt6-wayland harfbuzz sqlite zlib
```

`qt6-wayland` supplies the native Wayland platform plugin. Qt's xcb plugin uses
X11 or XWayland; install `xorg-xwayland` if your Wayland session needs it.
A working desktop OpenGL driver is required. `clang` is optional for Clang builds.
Speech output additionally needs a configured Qt speech backend, such as
`speech-dispatcher`. PDF reading does not require an active speech service.

The checked-in MuPDF gitlink currently pins 1.26.11. Initialize its recursive
submodules, without updating their revisions:

```sh
git submodule update --init --recursive
```

CMake invokes GNU Make for the vendored MuPDF static libraries, uses system
HarfBuzz and zlib, and uses MuPDF's bundled remaining libraries. No system MuPDF
package is needed. Keep the pinned revision until API and rendering changes have
been tested. The separate top-level zlib submodule belongs to legacy workflows;
CMake uses the system zlib. Source licensing and attribution remain in LICENSE
and the respective dependency directories.

## Build and run

From the repository root:

```sh
scripts/dev-build.sh
scripts/dev-run.sh "$PWD/tutorial.pdf"
```

Equivalent direct commands:

```sh
cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/dev --parallel 2
./build/dev/sioyek "$PWD/tutorial.pdf"
```

The default job count is conservative because MuPDF and the large C++ files can
use substantial memory. Set `SIOYEK_JOBS=4` for Ninja and pass
`-DSIOYEK_MUPDF_JOBS=4` to the build script for MuPDF. Make checks its own
incremental dependencies on each build. MuPDF outputs live under the CMake
build directory; its upstream generators may also create ignored source-tree
files. No network access is needed once submodules and packages are present.

Use a separate tree when changing compiler:

```sh
SIOYEK_BUILD_DIR="$PWD/build/clang" scripts/dev-build.sh \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
SIOYEK_BUILD_DIR="$PWD/build/clang" scripts/dev-run.sh "$PWD/tutorial.pdf"
```

CMake stages defaults, shaders and the tutorial in `build/dev/resources` on each
build. The executable uses that directory when present, otherwise the configured
installation data directory. It does not need an installed `sioyek-git` package.
The run script requests a new instance so an installed reader cannot receive the
launch through IPC. User preferences and SQLite schemas are unchanged. Normal runs use your existing
Sioyek data, so use an isolated HOME for experiments (the legacy Linux database
path is `$HOME/.local/share/sioyek`, even when XDG_DATA_HOME is set):

```sh
test_home=$(mktemp -d)
HOME="$test_home" XDG_CONFIG_HOME="$test_home/.config" \
  XDG_DATA_HOME="$test_home/.local/share" \
  scripts/dev-run.sh --new-instance "$PWD/tutorial.pdf"
```

Select a platform explicitly when diagnosing display issues:

```sh
QT_QPA_PLATFORM=wayland scripts/dev-run.sh "$PWD/tutorial.pdf"
QT_QPA_PLATFORM=xcb scripts/dev-run.sh "$PWD/tutorial.pdf"
```

Qt's offscreen plugin does not provide a useful QOpenGLWidget rendering test.
Check tutorial rendering, `j`/`k` scrolling, page navigation, `/` search and
`n`/`N` results in an actual OpenGL-capable session.

## Installation

For a user-local installation, configure the prefix before building:

```sh
cmake -S . -B build/local -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build/local --parallel 2
cmake --install build/local
```

System packaging can use `-DCMAKE_INSTALL_PREFIX=/usr` and
`DESTDIR="$PWD/build/package" cmake --install build/dev`. Configure the same
prefix for build and packaging. Qt libraries are supplied by the distribution,
not copied into the installation. Shaders/defaults/tutorial, desktop entry,
icon and man page are installed normally.

## Following upstream

Keep local changes committed on your own branch before merging. Add upstream
once, then fetch and inspect changes:

```sh
git remote add upstream https://github.com/ahrm/sioyek.git
git fetch upstream
git log --oneline HEAD..upstream/development
git diff HEAD...upstream/development -- CMakeLists.txt pdf_viewer .gitmodules
```

Merge deliberately, then synchronize the recorded submodules and rebuild:

```sh
git merge upstream/development
git submodule update --init --recursive
scripts/dev-build.sh
```

Do not use `git submodule update --remote` as a routine update: it moves beyond
the tested gitlinks. Review MuPDF API changes separately. Before publishing a
fork release, test both display backends and real PDFs, search, SyncTeX,
bookmarks, highlights and persistence using copies of existing user data.

For an installable Arch package, release versioning and CI policy, see
[PACKAGING.md](PACKAGING.md).

## API compatibility checks

Application builds disable Qt APIs deprecated in the selected Qt version and
reject C++ deprecated-declaration diagnostics. Warnings are not suppressed.
Enable the focused checksum/UTF-8 tests with:

```sh
scripts/dev-build.sh -DSIOYEK_BUILD_TESTS=ON
ctest --test-dir build/dev --output-on-failure
```

These checks cover the existing tutorial checksum (used for persisted document
identity), empty/chunked file hashing and multilingual UTF-8 iteration. Arch
package builds enable and run them in `check()`. Previously built package archives
must be rebuilt to include subsequent source changes.

## Hardened and sanitizer builds

Linux C++ builds enable `SIOYEK_HARDENING=ON` and
`SIOYEK_WARNINGS_AS_ERRORS=ON` by default. Supported flags include strong stack
protection, stack-clash protection, control-flow protection (on supported targets),
zero initialization of automatic variables, strict flexible-array handling for
application C++, PIE, RELRO/NOW, a non-executable stack, `_GLIBCXX_ASSERTIONS` and
`_FORTIFY_SOURCE=3` for optimized builds. The explicit flags cover GCC's installed
`-fhardened` set while also supporting Clang. A missing build type defaults to
RelWithDebInfo. Debug builds skip Fortify because it requires optimization.

Compiler diagnostics reject application C++ warnings, with explicit checks for
format security, missing returns, uninitialized values, null dereferences, array
bounds and unsafe polymorphic destruction. Deprecated Qt declarations remain
disabled. This is a correctness/security warning profile, not every optional style,
conversion or unused-parameter diagnostic. No warning-suppression flags are used.

For runtime checking, use a separate directory:

```sh
cmake -S . -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS_DEBUG='-O1 -g' \
  -DSIOYEK_BUILD_TESTS=ON -DSIOYEK_SANITIZERS=ON
cmake --build build/sanitize --parallel 2
ctest --test-dir build/sanitize --output-on-failure
SIOYEK_BUILD_DIR="$PWD/build/sanitize" scripts/dev-run.sh "$PWD/tutorial.pdf"
```

ASan/UBSan are non-recovering, with frame pointers retained. The application,
fzf and the pinned MuPDF libraries receive instrumentation. System Qt, SQLite,
HarfBuzz and zlib are distribution binaries and are not rebuilt/instrumented here.
MuPDF output directories include a safety-flag fingerprint so changing hardening
or sanitizer settings cannot silently reuse the other configuration's archives.
Its older bundled C libraries retain their trailing-array policy; the application
C++ strict-flexible-array flag is not imposed on those libraries.

There is no valid universal "all flags" combination: TSan and ASan are mutually
exclusive, MSan requires an instrumented dependency stack, and full libstdc++ debug
mode changes ABI. Those configurations are not silently combined with Qt binaries.
Use hardened optimized packages for normal reading and the sanitizer build for QA.
Sanitizers detect only exercised bugs; passing a smoke test is not a memory-safety
proof. Existing upstream C-library diagnostics are kept visible separately from
the application's warnings-as-errors policy.
