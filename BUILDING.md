# Building and maintaining Sioyek on Arch Linux

CMake + Ninja is required for the Vulkan renderer and its embedded SPIR-V
shaders. The legacy qmake project stops with a migration message. Do not use
`build_linux.sh`: it is a legacy qmake script that removes `build/`.

## Dependencies

Install these packages yourself if missing (this project does not install them):

```sh
sudo pacman -S --needed base-devel git python cmake ninja pkgconf unzip qt6-base \
  qt6-declarative qt6-svg qt6-speech qt6-wayland qt6-shadertools \
  vulkan-headers vulkan-icd-loader harfbuzz sqlite zlib libjpeg-turbo
```

`qt6-wayland` supplies the native Wayland platform plugin. Qt's xcb plugin uses
X11 or XWayland; install `xorg-xwayland` if your Wayland session needs it.
A working Vulkan ICD is required: for example `vulkan-intel` for supported Intel
GPUs or `vulkan-radeon` for AMD. Keep the driver appropriate to your laptop;
these build scripts do not install or change drivers. `vulkaninfo --summary`
(from optional `vulkan-tools`) reports the selected GPU. `clang` is optional.

Rendering uses Qt's `QRhiWidget` with the Vulkan backend, including page textures,
highlights, clipping, previews and freehand geometry. MuPDF still rasterizes PDF
pages on CPU workers. Qt paints labels/icons into transparent layers composed
by Vulkan. The two GLSL sources in `pdf_viewer/shaders/vulkan/` are build inputs
only. CMake compiles them to embedded SPIR-V with `qsb`; the executable contains
no application GLSL shader payload or runtime shader compiler.

QRhi requires `Qt6::GuiPrivate`, whose binary interface is tied to the Qt build.
Qt does not guarantee binary compatibility for private QRhi headers across versions, even
for patch releases ([Qt QRhi Documentation](https://doc.qt.io/qt-6/qrhi.html)).
**Rebuild Sioyek after Qt updates.** The Arch binary package records the exact
Qt version (`qt6-base=X.Y.Z`) used at build time to prevent silently loading incompatible
internals. When Arch updates Qt, see [PACKAGING.md](PACKAGING.md) for the upgrade procedure.
This edition supports desktop Linux only. Windows and macOS build/deployment code
has been removed; CMake rejects other target platforms.
Speech output additionally needs a configured Qt speech backend, such as
`speech-dispatcher`. PDF reading does not require an active speech service.

The checked-in MuPDF gitlink currently pins 1.26.11. Initialize its recursive
submodules, without updating their revisions:

```sh
git submodule update --init --recursive
```

CMake invokes GNU Make for the vendored MuPDF static libraries, uses system
HarfBuzz, zlib and libjpeg-turbo, and uses MuPDF's bundled remaining libraries. No system MuPDF
package is needed. Keep the pinned revision until API and rendering changes have
been tested. The legacy top-level zlib submodule has been removed; CMake uses
the system zlib. JPEG uses MuPDF's supported `USE_SYSTEM_LIBJPEG=yes`
configuration: the bundled IJG 9f decoder triggers signed-shift UBSan errors on
the tutorial's images. This selection does not change the MuPDF pin.
CMake also uses MuPDF's supported
`FZ_POINTER_ALIGN_MOD=FZ_MEMORY_BLOCK_ALIGN_MOD` override for the library and its
consumers. MuPDF's x86-64 default packs pointers on four-byte boundaries, which
triggers UBSan when a complex path in a Type 3 font needs eight-byte alignment.
Native alignment keeps display-list and pool accesses valid in every build;
this is independent of the hardening/sanitizer switches. The flags fingerprint
selects fresh MuPDF objects when this setting changes. PDF and user database
formats are unaffected.
CMake also applies out-of-tree source fixes from `patches/mupdf/`, including
`1.26.11-svg-const-qualifier.patch` which preserves const qualifiers in SVG style parsing
under GCC 15/16 default C23 mode, preventing `-Wdiscarded-qualifiers` compiler warnings.
Source licensing and attribution remain in LICENSE
and the respective dependency directories.

## Build and run

From the repository root:

```sh
scripts/dev-build.sh
scripts/dev-run.sh "$PWD/tutorial.pdf"
```

### Mise task runner

The repository provides a complete developer task runner in `mise.toml` covering the entire lifecycle: build, run, test, package, and benchmark.

To use it, trust the configuration once:

```sh
mise trust ./mise.toml
```

View all available tasks at any time:

```sh
mise tasks
```

#### 1. Development & Build

- **`mise run build`**: Builds the development/debug binary using CMake and Ninja (`scripts/dev-build.sh`) into `${SIOYEK_BUILD_DIR:-build/dev}`.
- **`mise run build-release`** (alias: **`release`**): Configures and compiles an optimized release binary using CMake and Ninja into `${SIOYEK_RELEASE_DIR:-build/release}`.
- **`mise run docs`**: Generates Doxygen C++ API reference and architecture documentation into `build/doxygen/html/index.html`.
- **`mise run clean`**: Cleans build artifacts via CMake/Ninja across `build/dev` and `build/release`, and removes intermediate Python bytecode caches (`__pycache__`), benchmark caches, package build directories, and Doxygen documentation (`build/doxygen`).

```sh
mise run build
mise run build-release
mise run docs
mise run clean
```

#### 2. Execution

- **`mise run reader [-- <arguments>]`**: Launches the development reader (`scripts/dev-run.sh`) with `--new-instance` protection.

```sh
mise run reader -- "$PWD/tutorial.pdf"
```

##### After a Qt update

If Qt has stayed at the version Sioyek was built against and the reader is
working, you can keep using that build. After a full Arch system upgrade that
changes Qt, check the installed version, then rebuild and launch your local copy
from the repository root.

```sh
pkg-config --modversion Qt6Core
mise run build && mise run reader
```

For example, the version check prints `6.12.1` after upgrading to Arch's
`qt6-base 6.12.1-1`. The local reader uses your existing Sioyek profile,
including preferences, keybindings, bookmarks, and reading history.

If you normally open Sioyek from the desktop launcher, run `mise run package`
and install the newly built main package from `build/arch/` using the
[Arch package instructions](PACKAGING.md). The `reader` task launches
`build/dev/sioyek` by default. A full benchmark is optional for routine use.

#### 3. Testing

- **`mise run test`** (alias: **`check`**): Runs CTest unit tests in the development build directory (`renderer_response_state`, `modern_api_compatibility`, `mupdf_source_staging`, `mupdf_runtime`, `vulkan_rendering`, `qt_vulkan_lifecycle`). Additional CTest arguments can be passed after `--`.
- **`mise run test-all`** (alias: **`check-all`**): Runs CTest plus all standalone regression test suites sequentially:
  1. CTest unit tests
  2. Fresh default profile regression test (`scripts/test-default-profile.py`)
  3. Render worker queue and scheduling test (`scripts/test-render-workers.py`)
  4. Same-width overview regression test (`scripts/test-overview-same-width.py`)
  5. Event readiness regression test (`scripts/test-event-readiness.py`)
  6. Native Wayland keyboard smoke test (`scripts/test-wayland.py`, automatically runs when Hyprland and `wtype` are active)
- **`mise run test-overview`**: Runs the same-width overview regression test on `build/dev/sioyek`.
- **`mise run test-readiness`**: Compares event-driven render readiness between baseline (`/usr/bin/sioyek`) and candidate (`build/dev/sioyek`).
- **`mise run test-workers`**: Tests render worker scheduling, dynamic gamma, and cancellation.
- **`mise run test-wayland [-- --onscreen]`**: Runs native Wayland keyboard input smoke test on Hyprland (`--onscreen` captures compositor pixel updates with grim).

```sh
mise run test
mise run test-all
mise run test-overview
mise run test-readiness
mise run test-workers
mise run test-wayland
```

#### 4. Packaging & Validation

- **`mise run package [-- <makepkg-flags>]`**: Prepares a complete, reproducible source archive snapshot via `scripts/prepare-arch-package.py` and builds the Arch Linux package in `build/arch` using `makepkg`.
- **`mise run test-package [-- <pkg-path>]`**: Runs the pacman package lifecycle test in an isolated, unprivileged user namespace (`scripts/test-package-lifecycle.py`). Automatically selects the latest non-debug package from `build/arch/` if no argument is passed.
- **`mise run verify-release [-- <pkg> <src>]`**: Runs release verification (`scripts/verify-release.py`) checking package paths, conflicts, ELF hardening (PIE, Full RELRO, non-exec stack), desktop integration, and exact submodule checksums against `sioyek-source.tar.gz`.

```sh
mise run package
mise run test-package
mise run verify-release
```

#### 5. Benchmarking & Fixtures

- **`mise run fixtures`**: Generates synthetic deterministic benchmark fixtures (`large-text.pdf`, `annotated.pdf`, `image-only.pdf`) in `build/benchmark-fixtures`.
- **`mise run bench-scan [-- <args>]`**: Runs the rendering benchmark on the synthetic image scan fixture. When invoked without arguments, it benchmarks `/usr/bin/sioyek` against `build/dev/sioyek` in normal mode.
- **`mise run bench-textbook [-- <args>]`**: Runs the rendering benchmark on `textbook_of_engineering_matematics.pdf`.
- **`mise run bench-search [-- <args>]`**: Runs the search responsiveness, indexing, and contention benchmark suite (`scripts/benchmark-search.py`).
- **`mise run bench-workers [-- <args>]`**: Runs worker thread sweep and idle scheduling benchmark (`scripts/benchmark-workers.py`).

```sh
mise run fixtures
mise run bench-scan
mise run bench-textbook
mise run bench-search
mise run bench-workers
```

#### Environment Overrides

The mise tasks honor the following environment variables:

- `SIOYEK_BUILD_DIR`: Development build directory (default: `build/dev`)
- `SIOYEK_RELEASE_DIR`: Release build directory (default: `build/release`)
- `SIOYEK_BASELINE_BIN`: Baseline executable path for comparisons (default: `/usr/bin/sioyek`)
- `SIOYEK_CANDIDATE_BIN`: Candidate executable path for comparisons (default: `${SIOYEK_BUILD_DIR:-build/dev}/sioyek`)
- `SIOYEK_JOBS`: Parallel compilation jobs for Ninja (default: 2)

### Direct CMake commands

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

CMake stages defaults and the tutorial in `build/dev/resources` on each
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

Qt's offscreen platform plugin does not provide this Vulkan widget test.
Check tutorial rendering, `j`/`k` scrolling, page navigation, `/` search and
`n`/`N` results in a Wayland or X11 session. On Hyprland, the optional
`python3 scripts/test-wayland.py "$PWD/build/dev/sioyek"` checks native keyboard
input with `wtype` and captures additional rendering scenes in a temporary
profile. It focuses the test window and restores previous focus; avoid typing
while it runs.
Add `--onscreen` (requires optional `grim` and Python Pillow) to capture the
test reader's fullscreen compositor output. This checks status text updates,
command editor appearance and clean dismissal without triggering a Qt framebuffer
grab, which could hide a missing repaint. It captures only the test window's
region and restores previous focus.

To build and run the GPU regression test in your desktop session:

```sh
cmake -S . -B build/dev -G Ninja -DSIOYEK_BUILD_TESTS=ON -DSIOYEK_BUILD_GPU_TESTS=ON
cmake --build build/dev --parallel 2
SIOYEK_VULKAN_VALIDATION=1 ctest --test-dir build/dev --output-on-failure
```

The validation setting requires optional `vulkan-validation-layers`. Leave it
unset for ordinary use. The GPU test checks framebuffer pixels for color modes,
blending, stencil/depth clears, painter ordering and resize. CI uses Xvfb and
Mesa's software Vulkan ICD; that complements testing on a real laptop GPU.

The canvas acquires its CPU painter lazily through `overlayPainter()`. Scene code
must acquire it again when painting after `beginGpuSection()`; that boundary
composes the pending layer while preserving painter state. GPU-only frames create
no painter and skip overlay scans/clears. Nonempty layers are cropped for upload,
and only their painted pixel bounds are cleared. The regression test covers
clipping/transforms across boundaries, text/images, disappearing layers and resize.
Run it with `QT_SCALE_FACTOR=1.5` as well when changing overlay coordinates.

### Record a session with Linux perf

Build the optimized reader with debug symbols, then record from the repository
root. Close the reader to finish recording.

```sh
mise run build
mise run perf
perf report -i build/perf/sioyek.data
```

The launcher opens `tutorial.pdf` by default. To test another document, use
`mise run perf -- "/path/to/document.pdf"`. The reader starts in a new
instance and saves its preferences, databases, and cache under
`build/perf/profile`. Reuse that directory to test with the same reader state.
For a fresh session, move the directory aside after closing the reader.

### Worker scheduling and performance checks

The four MuPDF render workers and the background search worker wait on condition
variables instead of polling every 100 ms. Queue changes, deferred pixmap cleanup
and shutdown notify the workers. Queue/busy state uses the same mutex as each
wait predicate. Closing a view cancels its queued search and waits for an active
search to release its borrowed result pointers before destroying them. Cancellation
finishes the current page; it does not interrupt MuPDF in the middle of a page.
Render readiness checks exact visible slices when the IPC command arrives and
when a render completion is handled on the GUI thread. A 100 ms timer is the
fallback. Search progress updates can refresh the UI independently.

Directional lookahead is enabled by default. During sequential reading, the
renderer can prepare one adjacent page in the direction of travel. Visible
requests take priority. Navigation changes can discard stale queued lookahead,
while an in-flight raster may still complete. Speculative rasters stay as CPU
pixmaps until a view draws them. Set `directional_lookahead 0` in
`prefs_user.config` to disable it.

In a native Wayland comparison of `bdd77cd1` and `698d5680`, the sampled
sequential turns on a scan took 33–73 ms before lookahead and 16–24 ms after.
The dense-textbook rapid-jump median fell from 73 to 31 ms, with P95 moving
from 73 to 68 ms. The tested scan's median rapid-jump wait stayed near 40 ms,
and sampled framebuffer captures matched. These timings describe the tested
workloads and include IPC readiness, with display presentation measured separately
only if compositor feedback is collected.

In-flight render/search operations retain their own MuPDF document references;
clearing the cache releases only its references. An epoch check prevents an open
that overlapped invalidation from publishing an outdated cached document. Gamma
and search-case settings are captured in requests. UI cancellation waits before
clearing shared results, and workers protect progress updates with the result
mutex. `num_cached_pages 0` keeps no minimum set of cached pages; unused responses
still expire according to the existing cache timeout. Failed responses leave the
pending state, and invalid entries cannot receive a retry's completed pixels.
While an exact-zoom render is pending, fallback textures must match the document,
page, slice layout, display scale, gamma and annotation setting. Choose the
nearest absolute zoom difference, preferring higher resolution on a tie; reject
invalid, pending or empty responses. `renderer_response_state` tests this policy.

Document frames update the canvas and individual UI children. Avoid adding a
whole-window `update()` to `validate_render()`: Qt then fills/composes the parent
background on every scroll frame. Child widgets schedule their own repaints;
the optional onscreen test above checks that status and command widgets still
update and disappear correctly.

Exercise non-indexed search, request replacement and idle/busy shutdown with
temporary profiles in a working desktop session:

```sh
python3 scripts/test-render-workers.py "$PWD/build/dev/sioyek" --allow-qt-vuid
python3 scripts/test-render-workers.py "$PWD/build/sanitize/sioyek" --allow-qt-vuid
```

The scripts reject Vulkan validation and sanitizer diagnostics. When running
against unpatched system Qt 6 with `vulkan-validation-layers`, pass `--allow-qt-vuid`
to ignore upstream Qt bug `VUID-VkImageCreateInfo-initialLayout-12478` (or `--no-validation`
to run without validation layers). Alternatively, use the optional local runtime
fixes below to test with a fully patched Qt. `SIOYEK_TEST_LIBRARY_PATH`
also works with this test. It does not inject keyboard events.
It covers gamma changes during search, cancellation, zero-cache startup and
renaming a copied tutorial, in addition to queued-work/shutdown cases. The
display-independent `renderer_response_state` CTest checks failed-render recovery
and matching completed work to the correct pending response. CI runs the worker
script on the sanitizer reader through Xvfb with software Vulkan.

For a controlled before/after measurement, preserve an optimized baseline binary
**and its adjacent `resources/` directory** before changing code. Then run the two
optimized binaries with the same libraries, PDF, display and validation settings,
without concurrent builds or tests:

```sh
python3 scripts/benchmark-workers.py \
  --baseline /path/to/before/sioyek --candidate "$PWD/build/dev/sioyek" \
  --pdf "$PWD/tutorial.pdf" --output build/perf-workers --rounds 3
python3 scripts/benchmark-rendering.py \
  --baseline /path/to/before/sioyek --candidate "$PWD/build/dev/sioyek" \
  --pdf "$PWD/tutorial.pdf" --output build/perf-rendering --rounds 3 --validation
```

Both use temporary profiles, alternate trial order and save raw JSON, reader logs
and framebuffer PNGs. Equal logical and framebuffer dimensions are required across
trials. The worker benchmark disables indexed search and measures time to the first
result, render readiness and per-thread idle context switches. Readiness
timings end at the IPC reply. Framebuffer captures verify output after the reply
and do not measure when the compositor presented the frame. Avoid mouse
and keyboard interaction with the readers during measurements; selection overlays
can change the workload and captured pixels.
The rendering benchmark periodically verifies the requested view and rejects
unexpected offsets, zoom, selections, search or ruler state; its JSON records
these checks and the settled capture state. Compare the saved PNGs as well.
The rendering benchmark measures process CPU during paced scrolling and separately
times synchronous framebuffer readback plus PNG encoding. These are not FPS,
GPU timestamps or energy measurements. Validation defaults to enabled. A historical
OpenGL binary can be supplied with `--opengl`/`--vulkan`, but other source/dependency
differences make that a reference comparison rather than an isolated backend test.

Generate a bounded, deterministic corpus without downloading documents or using
personal PDFs:

```sh
python3 scripts/create-benchmark-fixtures.py build/benchmark-fixtures
python3 scripts/benchmark-rendering.py \
  --baseline /path/to/before/sioyek --candidate "$PWD/build/dev/sioyek" \
  --pdf "$PWD/build/benchmark-fixtures/annotated.pdf" \
  --output build/perf-annotated --rounds 3 --normal-only --validation
```

The corpus contains 240 text pages, 32 synthetic scan-like image pages and 32
pages with 1,152 native PDF highlight annotations. The manifest records sizes and
SHA-256 hashes; generation refuses to replace differing existing files. These
fixtures do not represent every real-world PDF or application overlay workload.
Repeat with `large-text.pdf`, `image-only.pdf` and the tutorial. Use
`--skip-search` for image-only documents in the worker benchmark (there is no
text/OCR). `--baseline` alone records a fresh baseline; `--normal-only` shortens
workload sweeps. Startup-to-index-ready includes explicit cache warmup and IPC
waits, so it is not a pure indexing-time measurement.

For memory growth across pages, add `--page-turns 31` to the rendering benchmark
for the 32-page scan fixture. The report records Linux process RSS and peak RSS
at each turn, after a three-second idle period, and while revisiting up to five
early pages. Render wait includes IPC and the GUI polling interval. To tune the
decoded-resource cache, set `mupdf_store_mib` in `prefs_user.config` and restart.
The default is 64 MiB. This controls when MuPDF starts evicting decoded resources
such as images and fonts. Process RSS includes the rendered-page cache, Vulkan
and other allocations.

For fast navigation and rapid jumping without waiting between moves, add `--rapid-jumps 8`.
This executes rapid sequences of forward and backward jumps without waiting across
multiple destination pages (`--rapid-destinations 4`), then measures the median and
95th-percentile render waits for the final visible page, along with an isolated direct-jump
baseline to verify there is no queue penalty caused by stale requests.

For search responsiveness, background indexing, and render contention on large documents:

```sh
python3 scripts/benchmark-search.py \
  --baseline /usr/bin/sioyek --candidate "$PWD/build/dev/sioyek" \
  --text-pdf "$PWD/textbook_of_engineering_matematics.pdf" \
  --mixed-pdf "$PWD/build/real-documents/copies/02.pdf" \
  --output build/perf-search --rounds 3
```

This benchmark evaluates three aspects:
1. **Background Indexing**: Measures startup-to-index-ready duration and post-indexing memory usage (RSS).
2. **Search Responsiveness**: Compares unindexed time-to-first-match (page-by-page traversal) against indexed in-memory super-fast search across early and deep terms.
3. **Render Contention**: Measures page-turn render settle latency while background search actively traverses the document versus an idle reader baseline.

### Local fixes for Qt 6.12.x and Wayland 1.26.0

The unmodified system Qt 6.12.0 with validation layers 1.4.363.1 reports
`VUID-VkImageCreateInfo-initialLayout-12478`. Qt's
[Vulkan texture implementation](https://github.com/qt/qtbase/blob/v6.12.0/src/gui/rhi/qrhivulkan.cpp)
creates optimal-tiled images with `VK_IMAGE_LAYOUT_PREINITIALIZED` (in
`QVkTexture::create()`); the validation layer requires linear tiling for that
initial layout. This is inside the system Qt library, including widget-owned
textures. `patches/qt/6.12.0-vulkan-initial-layout.patch` changes both creation
and tracked initial layout to `UNDEFINED`, which matches optimal tiling.
[Qt 6.12.1 fixes this upstream](https://qt-project.atlassian.net/browse/QTBUG-150039).
The helper selects the source archive that matches the installed Qt and
applies the Vulkan patch only on 6.12.0. Both versions still use the
Wayland keyboard-compose cleanup for local leak testing.

The native Wayland exit leak also reproduces in a minimal Qt Vulkan widget.
`patches/wayland/1.26.0-destroyed-event-argument.patch` releases an event's
proxy reference before replacing an already-destroyed object argument with null.
Without that release, final event cleanup loses the pointer it must unref.
The renderer fixture now exercises a normal Qt event loop and window close.
Typing exposes another Qt leak: `QWaylandInputContext` never released its XKB
compose state/table. `patches/qt/6.12.0-wayland-compose-cleanup.patch` adds those
matching unrefs; the display's borrowed XKB context retains its original owner.

These are explicit, optional dependency builds, with exact source checksums.
They never replace host libraries and are restricted to Qt 6.12.0 and 6.12.1
because the application uses Qt private APIs. The helper needs `curl`, `patch`,
Meson, libffi, Expat and libxml2 development files in addition to normal build
dependencies. If Meson is absent, use an isolated environment:

```sh
python3 -m venv build/runtime-tools
build/runtime-tools/bin/pip install meson==1.9.1
MESON="$PWD/build/runtime-tools/bin/meson" scripts/build-runtime-deps.sh
```

Use the resulting libraries only for the test command or development process:

```sh
runtime="$PWD/build/runtime-deps"
LD_LIBRARY_PATH="$runtime/qt-build/lib:$runtime/wayland-build/src" \
  QT_PLUGIN_PATH="$(qmake6 -query QT_INSTALL_PLUGINS)" \
  QT_QPA_PLATFORM=wayland QT_FORCE_STDERR_LOGGING=1 SIOYEK_VULKAN_VALIDATION=1 \
  ctest --test-dir build/sanitize --verbose
```

The same environment can prefix `scripts/dev-run.sh` or the temporary-profile
`scripts/test-wayland.py` command. On Qt 6.12.1, use `qt-build-6.12.1/lib`
in the library path. Test harnesses enforce zero Vulkan validation errors by
default; when testing against unpatched system Qt 6.12.0, pass `--allow-qt-vuid`
to allow the known texture diagnostic. In this workspace, the
tested Qt 6.12.0 dependency tree is `build/runtime-blockers`; set `runtime` to
that path to reuse it.
Arch packages use the system libraries. CI fails on Vulkan validation errors
with a diagnostic-specific exception for the Qt 6.12.0 texture issue above.
The sanitized Xvfb job keeps the Lavapipe ICD mapped through the exit check to
preserve its allocation roots. Validation, leak detection and UBSan stay enabled.

For the Python smoke tests, `SIOYEK_TEST_LIBRARY_PATH` sets the reader's library
path without affecting tools such as `wtype`. This matters when testing
instrumented dependency libraries against an instrumented reader:

```sh
runtime="$PWD/build/runtime-blockers"
SIOYEK_TEST_LIBRARY_PATH="$runtime/jpeg-sanitize-checked:$runtime/qt-build/lib:$runtime/wayland-sanitize/src" \
  QT_PLUGIN_PATH="$(qmake6 -query QT_INSTALL_PLUGINS)" \
  QT_FORCE_STDERR_LOGGING=1 SIOYEK_VULKAN_VALIDATION=1 \
  python3 scripts/test-wayland.py "$PWD/build/sanitize/sioyek"
```

The `jpeg-sanitize-checked` and `wayland-sanitize` directories above are the
additional local diagnostic builds used for this investigation; the dependency
helper builds `qt-build` and `wayland-build`. Those normal local libraries are
also suitable for the smoke tests without instrumenting the dependencies.

Legacy OpenGL `*.vertex`/`*.fragment` shaders have been removed. Active Vulkan
GLSL sources are `pdf_viewer/shaders/vulkan/canvas.vert` and `canvas.frag`; edit
those files and rebuild to change the shaders. Only compiled SPIR-V packages are
embedded. Shader sources are neither staged beside the executable nor installed
under `/usr/share/sioyek`; they remain in corresponding-source archives for
reproducible builds and GPL compliance. Old benchmark/source artifacts under
ignored `build/` directories are historical snapshots, not current runtime data.

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
release, test both display backends and real PDFs, search, SyncTeX,
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

## C++ reference with Doxygen

Sioyek provides comprehensive Doxygen documentation covering project architecture,
the multi-threaded MuPDF concurrency engine, Qt 6 QRhi Vulkan hardware presentation,
and event-driven render readiness.

### Prerequisites

Ensure `doxygen` (and optionally `graphviz` for dot diagrams) is installed:

```sh
sudo pacman -S --needed doxygen graphviz
```

If Graphviz `dot` is present on the system, Doxygen generates UML class inheritance
hierarchies, call graphs, caller graphs, and directory dependency diagrams. If `dot`
is absent, documentation builds continue cleanly without diagrams.

### Generating documentation

Generate the documentation using the Mise task runner:

```sh
mise run docs
```

Or invoke Doxygen directly from the repository root:

```sh
doxygen Doxyfile
```

### Viewing documentation

HTML documentation is emitted to `build/doxygen/html/index.html`. Open it with any
browser:

```sh
xdg-open build/doxygen/html/index.html
```

Or host it locally:

```sh
python3 -m http.server -d build/doxygen/html 8080
```

The documentation main page links directly to the detailed architecture overview
in `docs/architecture.md`, indexing classes and interfaces across `pdf_viewer/`:
- `PdfRenderer`: Condition-variable worker pools, thread-local MuPDF context cloning, speculative pre-rendering lookahead, and LRU cache eviction.
- `PdfViewWidget`: Viewport calculations, dynamic page slicing, zoom transformations, and floating overview portals.
- `VulkanCanvas`: Qt 6 `QRhiWidget` integration, pre-compiled SPIR-V pipeline caches, and transparent `QPainter` overlay composition.
- `Document`: Thread-safe MuPDF `fz_document` handle management, reference counting, and page geometry.
- `MainWidget`: Central modal dispatcher, Unix domain socket IPC, and event-driven render readiness notifications.

### Cleaning documentation

To delete generated documentation and caches:

```sh
mise run clean
```

Or manually remove the directory:

```sh
rm -rf build/doxygen
```

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

Reviewed patches under `patches/mupdf/` guard nullable objects before forming
member addresses (including absent Type 3 glyph display lists), use unsigned
arithmetic for the text alpha shift, skip sorting
an empty layer list, and replace LittleCMS's incompatible function-pointer cast
with a typed adapter. CMake applies them to a separate `mupdf-source` build tree
using `scripts/prepare-mupdf.py`; recursive submodule pins remain unchanged.
Repeated builds preserve unchanged timestamps. Tests cover patch transitions
without modifying original files, null cleanup, archive reference counts, layer
loading, rendering and text extraction from all six tutorial pages, and search.
The MuPDF runtime test also records/replays large pointer-bearing and small flat
packed paths and checks their pixels. This synthetic case reproduces the
alignment failure without private lecture PDFs and runs in the existing CTest/CI
suite. Null display-list keep/drop calls cover Type 3 font cleanup as well.
Test additional copied PDFs with a word on their first page, or an explicit
empty query for image-only documents:

```sh
./build/sanitize/sioyek_mupdf_test /path/to/copy.pdf 'expected first-page word'
./build/sanitize/sioyek_mupdf_test /path/to/image-only-copy.pdf ''
```

The helper renders and extracts text from every page. Set
`SIOYEK_MUPDF_TEST_TRACE=1` to log page numbers while diagnosing failures.
System JPEG is ordinarily uninstrumented; local validation also used an
independently instrumented libjpeg-turbo 3.2.1 build, without sanitizer exclusions.
JPEG output comparison at 612×792 found five tutorial pages byte-identical. Page
2 differed only within its JPEG illustration (maximum channel difference 38/255;
mean absolute channel difference across the page 0.273/255). Decoder output is
therefore visually comparable, but not byte-identical for JPEG images.

There is no valid universal "all flags" combination: TSan and ASan are mutually
exclusive, MSan requires an instrumented dependency stack, and full libstdc++ debug
mode changes ABI. Those configurations are not silently combined with Qt binaries.
Use hardened optimized packages for normal reading and the sanitizer build for QA.
Sanitizers detect only exercised bugs; passing a smoke test is not a memory-safety
proof. Existing upstream C-library diagnostics are kept visible separately from
the application's warnings-as-errors policy.
