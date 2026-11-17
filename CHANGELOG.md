# Changelog

Fork: https://github.com/Ctrlm0nster/sioyek
Upstream: https://github.com/ahrm/sioyek (ahrm and contributors)

## [3.0.0-dev] — Vulkan & Linux Modernization

Current development packaging revision: 5. No release tag or public assets yet.

- Default MuPDF's shared decoded-resource cache to 64 MiB to reduce retained
  memory while reading image-heavy PDFs. Set `mupdf_store_mib` and restart to
  tune the memory and page-revisit tradeoff. Add sequential page traversal and
  revisit measurements to the rendering benchmark.

- Use MuPDF's native packed-pointer alignment in all builds and propagate the
  same definition to its consumers. Fix the UBSan alignment failure triggered by
  complex paths in Type 3 glyphs without changing the pinned dependency or
  disabling checks. Add a synthetic packed-path replay/pixel regression and
  allow all-page runtime checks on copied PDFs with document-specific queries.
  Guard null display-list keep/drop calls during Type 3 font cleanup and cover
  them in the existing runtime regression.
- Avoid redundant whole-window repaints when updating the Vulkan document canvas;
  retain child widget updates and check status/command UI with opt-in compositor
  screenshots. Correct closest-zoom fallback selection and require matching
  gamma, display scale, annotation and slice settings. Add cache-policy regressions
  and deterministic text, image-only and native-annotation benchmark fixtures.
- Retain in-flight MuPDF document references across cache invalidation, guard
  cache publication during document I/O, and release render/search resources on
  error paths. Cancel background searches before replacing/clearing view state
  and synchronize progress updates. Handle a zero-page cache without invalid
  indexing; exclude failed/invalid responses from retry completion. Add response
  state tests and sanitized GUI coverage for cancellation, invalidation and zero
  cache startup; run worker regressions in CI with a software Vulkan ICD.
- Create the Vulkan painter overlay only when scene code needs it. Skip overlay
  clears and alpha scans for GPU-only PDF frames; clear painted pixel bounds
  after composition. Preserve painter state across GPU boundaries and test
  text/images, clipping, disappearing layers, resize and fractional scaling.
- Replace render/search worker polling with condition-variable waits; synchronize
  queue/busy state and wake workers for deferred pixmap cleanup and shutdown.
  Release unused search-page loads and evicted render results. Cancel and wait
  for a view's active search before destroying its borrowed result storage.
  Add temporary-profile worker stress tests and repeatable before/after benchmarks
  with validation enabled, framebuffer-size checks and raw measurement evidence.
- Target desktop Linux only. Remove Windows/macOS builds, runtime DLLs, icons,
  native integration branches, legacy deployment workflows and PowerShell-only
  TTS helpers. Keep the Linux Qt speech backend, configuration/data formats and
  upstream dependency sources intact. Document the CMake/Ninja workflow.

- Fix runtime cleanup for command-line parsers, secondary IPC shared-memory
  handles, input trees, document views, generated contents trees and renderer
  caches/worker contexts, uncached title-detection text pages and SQLite
  connections. Join workers before destroying windows and unwind
  normal application owners when quitting from an inactive window.
- Keep MuPDF 1.26.11 pinned and apply reviewed fixes in a separate build tree:
  nullable member addresses, text alpha shifts, empty layer sorting and a typed
  LittleCMS callback. Select supported system libjpeg-turbo decoding after
  testing all tutorial pages against the bundled IJG decoder.
- Provide optional, checksummed local Qt 6.12.0/Wayland 1.26.0 builds for Vulkan
  image-layout validation and Wayland proxy/keyboard-compose cleanup fixes.
  Host libraries remain unchanged. Add independent Qt lifetime and MuPDF runtime
  regressions; CI now rejects Vulkan validation errors and builds pinned MuPDF
  under sanitizers.

- Replace application OpenGL calls with Vulkan rendering through Qt QRhiWidget;
  embed build-time SPIR-V shaders, separate MuPDF images from device resources,
  and retain the existing document layout and input commands.
- Fix redundant Vulkan painter-layer copies/uploads: reuse CPU image storage,
  skip transparent layers and crop nonempty layers to their painted bounds.
  Test disappearing layers and single-pixel edge content at fractional scaling.
- Remove 20 obsolete OpenGL shaders and all runtime shader-source staging,
  embedding and installation. Keep the two Vulkan GLSL build inputs in source
  archives; application shader packages contain only compiled SPIR-V.
- Add Vulkan framebuffer regression tests and software Vulkan CI coverage.
  Require a Vulkan ICD and Qt Shader Tools; binary packages pin their Qt version
  because QRhi uses QtGuiPrivate. Legacy qmake builds now point to CMake.
- Prefer CMake + Ninja for out-of-source Linux development; build pinned MuPDF
  static libraries through GNU Make with separate output per build tree.
- Fix missing returns, unsafe polymorphic destruction, unchecked file opens,
  reference parsing/file-size overflow warnings and uninitialized selection JSON.
  Add Linux compiler hardening and separate ASan/UBSan instrumentation, including
  pinned MuPDF, with fingerprinted dependency outputs.
- Replace deprecated Qt hash/pointer-event APIs and C++ iterator inheritance;
  enforce deprecation-free application compilation and test checksum/UTF-8
  compatibility during package checks.
- Require Qt 6.7 or newer and preserve C++20, keyboard-driven reading and existing
  configuration/database formats.
- Stage defaults and tutorial for development runs; locate installed
  resources using the configured prefix or relative installed layout.
- Add build/run scripts and documented Arch dependencies/upstream merge workflow.
- Restore verified fuzzy-matching license notices and install bundled code/font
  notices with the binary package; omit unused Windows DLLs from Arch exports.
- Add a replacement Arch package, `sioyek-ctrlm0nster`, with explicit `sioyek` and
  `sioyek-git` conflicts. Executable name and branding remain Sioyek.
- Export checksummed, offline-buildable snapshots including pinned recursive
  MuPDF dependencies, a revision manifest and dependency license notices.
- Record direct Arch runtime dependencies on libgcc/libstdc++; retain Qt SVG
  plugins required at runtime. Increment package release to 2 for this recipe fix.
- Test native Wayland keyboard input and isolated pacman installation lifecycle
  without modifying personal data or the host package database.
- Add package/source verification, release cleanliness checks, full-history
  versioning, and real GCC/Clang package artifact validation in CI.
- Disable inherited automatic publishing workflows; retain them as reference.

No public fork release has been tagged or published. Existing compiler warnings
remain in bundled dependencies. See RELEASE.md for prior release checks; the
Vulkan backend requires new validation before publishing a release.
