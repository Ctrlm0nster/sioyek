# Development releases

This is a fork of [ahrm/Sioyek](https://github.com/ahrm/sioyek), retaining upstream
attribution, GPL-3.0 text and branding. Dependency notices, including MuPDF's AGPL
text, remain in the source snapshot and package notices. This document is a manual
release procedure; no command here has been run to tag, sign or publish a release.

## Verification status and known issues

The three original runtime blockers have reviewed local fixes. Qt 6.12.0's
optimal-image initial layout and Wayland keyboard-compose cleanup are patched
in an optional local Qt build. Wayland 1.26.0's discarded-event proxy reference
is patched in an optional local client build. System libraries remain unchanged;
ordinary packages and remote CI still require compatible distribution fixes.
See [BUILDING.md](BUILDING.md) for exact versions, checksums and local commands.

Local validation on 2026-11-16 passed all five regression tests under GCC,
Clang, and Clang ASan/UBSan/LeakSanitizer. Vulkan validation remained enabled.
The instrumented checks also used local instrumented Wayland and libjpeg-turbo
libraries. All six tutorial pages were rendered and their text extracted.
The complete native Wayland keyboard/navigation/search, color modes, rotation,
two-page view, scratchpad and shutdown smoke test passed with the original
shutdown timeout and without validation or sanitizer reports. Detailed evidence
is retained in ignored `build/runtime-blockers/`.
Fresh defaults and synthetic split-database versions 0/1/2, including repeat
starts, passed the instrumented Wayland profile checks. Integrity, bookmarks,
marks, highlights and reading history were preserved in all generated fixtures;
historical personal profiles were not used.

MuPDF remains pinned to 1.26.11. Reviewed source patches are applied out of tree;
no recursive gitlink was moved. The supported system-JPEG configuration replaces
the bundled decoder after an explicit tutorial comparison. Five pages are
byte-identical; page 2 differs only within its JPEG illustration. Application
cleanup now releases IPC/parser owners, worker contexts, cached text/images,
input/contents trees, views and SQLite connections, without changing user formats.

The normal patched Wayland upstream suite passed 26/26. Its optional fully
instrumented upstream suite did not pass: 15 passed, eight failed on duplicate
client/server protocol globals reported by ASan, and three timed out. Those
checks were not disabled. The application regressions and keyboard smoke test
passed with the instrumented client library; do not describe the entire
instrumented Wayland upstream suite as clean.

Earlier OpenGL clean-chroot/package-lifecycle evidence predates the Vulkan
migration. Repeat package, clean-checkout, chroot and lifecycle gates for the
exact candidate. GitHub Actions changes were checked locally for syntax and
have not run remotely; no publication is authorized by these local results.

Licensing provenance and notice restoration are recorded in [LICENSES.md](LICENSES.md).

## Checklist (each candidate)

- [ ] Commit reviewed changes with maintainer approval. Require a clean maintenance branch and
      initialized recursive submodules at the recorded gitlinks. No dirty snapshots
      may be published as immutable releases.
- [ ] Review `CHANGELOG.md`, `VERSION`, PKGBUILD `pkgrel`, and upstream attribution.
- [ ] Review dependency/source changes; do not move gitlinks with `--remote`.
- [ ] Generate the source archive and concrete recipe with the clean-release gate:
      `python3 scripts/prepare-arch-package.py --release`.
- [ ] Inspect the source manifest and archive listing; exclude secrets and local
      additions. Run `makepkg -s` as a normal user in `build/arch`.
- [ ] Run namcap on the concrete PKGBUILD and main/debug packages. Address errors;
      explain retained warnings. `qt6-svg` is used as a runtime plugin, despite
      namcap's unused-dependency warning.
- [ ] Run `scripts/verify-release.py MAIN_PACKAGE SOURCE_ARCHIVE` from the matching
      checkout. It verifies every tracked recursive dependency file and gitlink,
      package resources, licensing paths, runtime resolution and desktop entry.
- [ ] Run `scripts/test-package-lifecycle.py MAIN_PACKAGE` as a normal user. This
      creates a disposable pacman root in an unprivileged user namespace. It uses
      generated user data and metadata fixtures for upstream/debug conflicts.
- [ ] Build in a clean Arch devtools chroot and repeat package checks. Example,
      when devtools and permission for the disposable chroot are available:

      ```sh
      CHROOT="$PWD/build/chroot"
      mkdir -p "$CHROOT"
      # These privileged commands are user actions, not automatic setup:
      sudo mkarchroot "$CHROOT/root" base-devel
      (cd build/arch && makechrootpkg -c -r "$CHROOT")
      ```

      The source archive is already local. Review any additional chroot/network
      setup required by the installed devtools version. Never point this at `/`.
- [ ] Check remote CI for the exact candidate commit: both GCC and Clang artifact
      jobs must pass. A skipped namespace test must be covered locally. Download
      the actual artifacts; do not treat a successful configure as a release test.
- [ ] Extract the package and test tutorial, `j`/`k`, page movement, `/` search,
      scrolling, preferences and persistence using a temporary HOME on Wayland and
      XWayland. See PACKAGING.md and `scripts/test-wayland.py` for native input.
- [ ] Test fresh defaults and synthetic migrations with
      `scripts/test-default-profile.py /absolute/path/to/extracted/usr/bin/sioyek`.
- [ ] Test a copy of an existing profile and historical database, never live user
      files. Back up and close readers before copying SQLite databases. Check
      bookmarks, highlights, last-document state and integrity across upgrade.
- [ ] Retain main/debug packages, concrete PKGBUILD, `.SRCINFO`, source archive,
      revision manifest, `.BUILDINFO`, compiler/dependency versions and QA logs.
- [ ] Set a real PACKAGER identity in your makepkg configuration. Sign packages
      and checksums with your release key; verify signatures with a separate test
      keyring. Do not put private keys or passphrases in the repository or CI.
- [ ] Create an annotated/signed development tag yourself after approval, then
      publish a prerelease with the changelog and testing limits. Keep complete
      corresponding source available alongside binaries. Never replace a published
      asset in place; increment the version or pkgrel and issue a new candidate.

## Verify a clean candidate checkout

After the reviewed commit, clone it into a new ignored build directory and
initialize only its recorded submodules. Do not create a synthetic commit to
bypass the release gate. For example, from the repository root:

```sh
rc_commit=$(git rev-parse HEAD)
git clone --no-local "$PWD" "$PWD/build/rc-clean-checkout"
git -C build/rc-clean-checkout checkout --detach "$rc_commit"
git -C build/rc-clean-checkout submodule update --init --recursive
cd build/rc-clean-checkout
python3 scripts/prepare-arch-package.py --release --output build/snapshot-a
python3 scripts/prepare-arch-package.py --release --output build/snapshot-b
cmp build/snapshot-a/sioyek-source.tar.gz build/snapshot-b/sioyek-source.tar.gz
cmp build/snapshot-a/PKGBUILD build/snapshot-b/PKGBUILD
(cd build/snapshot-a && makepkg --log)
```

Submodule initialization may require network access. Repeat the clean-chroot
check on those exact source inputs; obtain permission before any missing host
package installation or privileged setup. Equal source archives do not establish
bit-for-bit binary reproduction without a matching toolchain and dependencies.

Push, tag, signing and publication require separate approval. After an approved
push, check both real-package compiler jobs and the focused sanitizer job for the
exact commit on GitHub. The workflow uploads candidates but does not publish them.

## Version and tag policy

`VERSION` holds the upstream-compatible base version, currently `2.0.0`.
Package versions use `BASE.rCOUNT.gHASH-PKGREL` with a 12-character parent commit
hash and full-history revision count. Source changes require a new commit/version;
packaging-only fixes to the same source version increment `pkgrel`. The helper
rejects shallow repositories and `--release` rejects tracked/untracked parent
changes, dirty recursive submodules, and moved/uninitialized gitlinks.

Merge upstream rather than rewriting published history. Never rebase an already
published maintenance branch, force-move release tags, or recycle a package version
for different source bytes. Local dirty snapshots share the parent version and are
only test artifacts. The GUI retains its upstream version string for now.

Tag the exact clean candidate commit, e.g. `fork/BASE.rCOUNT.gHASH-pkgPKGREL-rc1`. Include
the pkgrel so a packaging correction has its own immutable tag. Tag names do not
trigger publishing. Annotated tags are required; signed tags are preferred. Tags,
key generation and publishing require the maintainer's explicit action.

## Checksums and package signing

Work in a directory containing exactly one candidate's curated assets. Set the
key fingerprint yourself; never copy a key ID from an untrusted release page.

```sh
# Checksums cover concrete source inputs and package files, including debug:
sha256sum -- *.pkg.tar.zst sioyek-source.tar.gz PKGBUILD .SRCINFO > SHA256SUMS
# Maintainer action with an existing private release key:
gpg --local-user YOUR_FULL_FINGERPRINT --detach-sign SHA256SUMS
for package in *.pkg.tar.zst; do
  gpg --local-user YOUR_FULL_FINGERPRINT --detach-sign "$package"
done
# Verify before upload (no upload occurs here):
gpg --verify SHA256SUMS.sig SHA256SUMS
sha256sum -c SHA256SUMS
```

Users should obtain the public key fingerprint through an independently trusted
channel, then verify in their own keyring before installation:

```sh
gpg --import maintainer-public-key.asc
gpg --fingerprint YOUR_FULL_FINGERPRINT
gpg --verify SHA256SUMS.sig SHA256SUMS
sha256sum -c SHA256SUMS
gpg --verify sioyek-ctrlm0nster-VERSION-x86_64.pkg.tar.zst.sig \
             sioyek-ctrlm0nster-VERSION-x86_64.pkg.tar.zst
```

A checksum alone detects corruption, not publisher identity. Pacman has its own
trusted keyring. If using pacman signature enforcement, the user must separately
review/import and locally trust the verified key with `pacman-key`; never globally
disable signature verification or weaken the host pacman configuration. No release
key or real release signature was created during QA.

## Arch installation, update and removal

See PACKAGING.md for dependency installation, local builds, extraction and full
commands. After signature verification and backing up/closing existing readers:

```sh
sudo pacman -U ./sioyek-ctrlm0nster-VERSION-x86_64.pkg.tar.zst
# Update with the same command and the new candidate's explicit filename.
# Remove only the package; home-directory data is retained:
sudo pacman -R sioyek-ctrlm0nster
```

The fork intentionally conflicts with `sioyek`/`sioyek-git` because it owns the same
executable and resources. It does not silently overwrite either. Debug packages
have their own ownership: different ELF build IDs can coexist, but identical build
IDs can cause file conflicts. Inspect installed debug packages before switching:

```sh
pacman -Q sioyek sioyek-git sioyek-debug sioyek-git-debug sioyek-ctrlm0nster-debug
```

If replacing the old main and debug pair, explicitly remove the old debug package
first after reviewing ownership; then install the new main/debug pair together.
Do not pass `--overwrite`. Debug installation is optional for normal reading.
Removal of `sioyek-ctrlm0nster` does not automatically remove its separate debug
package; remove that explicitly if installed. Never delete `~/.config/sioyek` or
`~/.local/share/sioyek` as part of package cleanup.

## Upstream and dependency maintenance

Use the clean-tree fetch/review/merge sequence in PACKAGING.md. Keep fork build,
packaging and CI changes reviewable; inspect upstream release automation before
adopting it. Initialize only the newly recorded submodules after a merge. For a
MuPDF update, review release notes/API changes and dependency gitlinks, build both
compilers, repeat rendering/search/input/persistence tests and clean-chroot package
QA, and record the tested revision in the changelog. Do not bundle an untested
moving branch.

Security-sensitive input processing includes MuPDF and its PDF parsers, font
engines, image codecs, compression libraries, JavaScript engine (MuJS), and any
bundled XML/HTML parsers. SQLite stores persistent reader data; Qt and system
HarfBuzz/zlib are also runtime dependencies. Statically bundled fixes require a
new fork build; updating system libraries alone does not update those copies.
Review MuPDF/Artifex security advisories and Arch notices regularly, then test
updates deliberately rather than blindly advancing every submodule.

Aim for one development candidate per month while active, plus small urgent
security releases after validation. Skip a release when required gates fail.
Keep CI's rolling-Arch compatibility testing distinct from a reproducible release
environment: preserve exact package versions and a container digest/Arch Archive
date for releases. No compiler-output cache is currently used: Make's vendored
build does not track every flag change, so a broad cache risks stale binaries.
Fresh matrix jobs are simpler than maintaining a fragile cache key.

References: [Arch devtools](https://man.archlinux.org/man/extra/devtools/devtools.7.en),
[makechrootpkg](https://man.archlinux.org/man/extra/devtools/makechrootpkg.1.en),
[Arch package signing](https://wiki.archlinux.org/title/Pacman/Package_signing).
