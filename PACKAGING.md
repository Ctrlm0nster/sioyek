# Independent Arch packages

The package is named `sioyek-ctrlm0nster`. Branding, executable (`sioyek`), desktop
entry, resource paths and user data formats remain unchanged. Because these paths
are shared with upstream packages, it **conflicts with `sioyek` and `sioyek-git`**
and provides `sioyek`. It is a replacement, not a side-by-side installation.
Pacman will ask about removing the conflicting package when installing this one.
Never use `--overwrite` to bypass conflicts.

## Build without installing

Install missing dependencies yourself only after reviewing the command in
[BUILDING.md](BUILDING.md). `base-devel` is required for makepkg; snapshot creation
also requires Python and Git. No system MuPDF is used. This checkout's recursively
pinned MuPDF sources and all of their tracked dependencies are bundled in a
checksummed source archive. Builds need no network access after snapshot creation.

From the repository root, as your normal user:

```sh
git submodule update --init --recursive
python3 scripts/prepare-arch-package.py
cd build/arch
less PKGBUILD
makepkg -s
```

`makepkg -s` can offer to install missing dependencies. Decline if you do not want
a system transaction. Use `makepkg` without `-s` to fail on missing dependencies
without offering installation. Neither command installs the resulting package.
The package and optional debug package appear in `build/arch/`; use the main
`sioyek-ctrlm0nster-*.pkg.tar.zst`, not the `-debug-` package, for installation.

`packaging/arch/PKGBUILD` is the recipe template. The helper writes a concrete
recipe, version, source archive and SHA256 checksum into `build/arch`. Do not run
the template directly. Both generated files are needed to reproduce the source
input on another Arch machine. The helper includes tracked working-tree changes
and unignored parent-repository additions, excludes Git metadata and ignored build
outputs, and recursively exports tracked dependency files at their checked-out
revisions. Review `tar -tzf build/arch/sioyek-source.tar.gz` before sharing it.
`SOURCE-MANIFEST.txt` records the parent and recursive dependency commits and
tracked dirty status. Use clean commits for distributable releases; snapshots of
uncommitted work are for local testing.

MuPDF is built by GNU Make under the CMake build tree. The recipe disables LTO
across the separately built static archives. Qt libraries remain system-provided.
GPL-3.0 and attribution are retained; package metadata also records bundled
MuPDF's AGPL license and includes both top-level license texts. The complete
snapshot retains dependency license notices and corresponding source.

## Inspect and test the actual package

```sh
cd build/arch
pacman -Qip ./sioyek-ctrlm0nster-2*.pkg.tar.zst
bsdtar -tf ./sioyek-ctrlm0nster-2*.pkg.tar.zst
# Set this explicitly if several package versions exist:
pkg=$(find "$PWD" -maxdepth 1 -name 'sioyek-ctrlm0nster-2*.pkg.tar.zst' | sort -V | tail -n 1)
root=$(mktemp -d)
bsdtar -xf "$pkg" -C "$root"
profile=$(mktemp -d)
HOME="$profile" XDG_CONFIG_HOME="$profile/.config" \
  XDG_DATA_HOME="$profile/.local/share" XDG_CACHE_HOME="$profile/.cache" \
  QT_QPA_PLATFORM=wayland "$root/usr/bin/sioyek" --new-instance
```

The default tutorial should open from the extracted package. Startup logs should
resolve defaults, tutorial and shaders under `$root/usr/bin/../share/sioyek`.
The relative path is computed from CMake's configured bindir and datadir. There
is no dependency on an existing `/usr/share/sioyek`. Repeat with
`QT_QPA_PLATFORM=xcb` to test XWayland. `namcap "$pkg"` is an optional packaging
lint when namcap is installed.

On Hyprland, an optional targeted native keyboard test is available:

```sh
python3 scripts/test-wayland.py "$root/usr/bin/sioyek"
```

Run that command from the repository root. It needs `wtype` and `hyprctl`, focuses
a temporary native Wayland reader, checks `x` (custom next-page binding), `j`,
`/sioyek` + Enter, and restores prior focus. Do not type elsewhere during the test.
It creates a fresh HOME and retains its log/screenshot under `/tmp`. Failure to
find a client or a virtual-keyboard protocol is a test-environment limitation,
not evidence of an application keyboard bug. Other compositors need a manual
keyboard check; this test does not change compositor configuration.

## Install, update and uninstall (explicit user actions)

Back up your existing Sioyek data before switching packages. In particular retain
`~/.config/sioyek` and `~/.local/share/sioyek`, including SQLite databases. Close
all readers before copying databases or replacing the executable.

```sh
# Install the specific main package built above (review any conflict prompt):
sudo pacman -U /absolute/path/to/sioyek-ctrlm0nster-VERSION-1-x86_64.pkg.tar.zst
```

To update, follow the upstream procedure below, regenerate the snapshot and
recipe, run `makepkg -s` again and install the new main package with `pacman -U`.
Use `makepkg -sf` only when intentionally rebuilding an already existing package
version; increment `pkgrel` for a changed packaging recipe with the same sources.
Nothing automatically publishes or installs packages.

```sh
# Remove the package only, preserving dependencies and personal Sioyek data:
sudo pacman -R sioyek-ctrlm0nster
```

This package has no scripts that create, migrate or delete personal configuration.
Pacman does not own your home-directory preferences, bookmarks or databases.
Reinstall your preferred upstream package separately if desired.

## Versioning and upstream synchronization

`VERSION` records the upstream-compatible base (`2.0.0`). Snapshot package versions
are `BASE.r<parent-commit-count>.g<12-character-commit>`; for example,
`2.0.0.r2100.g1e2f80373537`. This avoids depending on upstream tags or MuPDF tags.
Use a merge-based history and do not rewrite published history so the revision
count increases. For the same source revision, increment `pkgrel` for packaging
fixes. Local dirty snapshots share the parent version: do not distribute them as
immutable releases. Their checksum identifies the exact input, but they are not
ordered as separate releases. Commit changes before a distributable snapshot;
then optionally tag that commit as `fork/BASE.rCOUNT.gHASH-pkgPKGREL` yourself. This process
never makes a commit or tag automatically. The application's displayed upstream
version remains unchanged for now.

Before updating upstream, commit your fork work yourself and make a backup of
user data. Start from a clean tree on your maintenance branch:

```sh
# Once only, if this remote does not exist:
git remote add upstream https://github.com/ahrm/sioyek.git
git fetch upstream
git log --oneline HEAD..upstream/development
git diff HEAD...upstream/development -- CMakeLists.txt pdf_viewer .gitmodules
# After review:
git merge upstream/development
git submodule update --init --recursive
scripts/dev-build.sh
```

Do not use `git submodule update --remote`; preserve the upstream-tested gitlinks
unless separately testing a dependency upgrade. Review conflicts in packaging and
CI rather than accepting upstream release automation wholesale. Update `VERSION`
only when adopting a newer upstream base. Build the package and repeat tutorial,
keyboard, search, scrolling and copied-data persistence checks before release.

## CI policy and reproducibility limits

`linux-cmake.yml` has read-only token permissions, disables persisted checkout
credentials, pins checkout to a verified commit SHA, and sets concurrency/time
limits. It builds and verifies real GCC/Clang snapshot packages as an unprivileged
user, retaining packages, source inputs, checksums and logs. Full-history checkout
preserves revision-count versions. Compiler selection is checked in CMakeCache. Package installation in CI is limited to disposable runner containers.
The legacy release workflows are retained under `.github/legacy-workflows` and
are inactive; there is no automatic release publishing workflow.

Arch CI deliberately follows the rolling `archlinux:base-devel` container and
performs a full `pacman -Syu` transaction, not partial upgrades. It detects current
Arch compatibility but is **not a bit-for-bit reproducible environment**. Dependency
versions are printed in CI and makepkg records `.BUILDINFO`. Release reproduction
requires preserving the source archive/checksum, PKGBUILD, container digest and
exact package versions (or a matching Arch Linux Archive date). A clean Arch
`devtools` chroot build is recommended before publishing; it was not assumed to
be available on the development host. No CI secrets or release-write permissions
are needed. New workflows must be reviewed before enabling publishing.

References: [Arch PKGBUILD manual](https://man.archlinux.org/man/PKGBUILD.5.en),
[GitHub Actions security guidance](https://docs.github.com/en/actions/reference/security/secure-use).

See [RELEASE.md](RELEASE.md) for release gates, signing, debug-package interactions
and cadence, and [CHANGELOG.md](CHANGELOG.md) for fork-specific changes.
