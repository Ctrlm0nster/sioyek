# Changelog

Fork: https://github.com/Ctrlm0nster/sioyek
Upstream: https://github.com/ahrm/sioyek (ahrm and contributors)

## Unreleased — first development release candidate

RC preflight packaging revision: 3. No release tag or public assets yet.

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
- Stage defaults, shaders and tutorial for development runs; locate installed
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
remain. See phase validation reports for executed checks and their limits.
