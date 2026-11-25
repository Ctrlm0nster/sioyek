#!/usr/bin/env bash
# Optional, exact-version dependency fixes for local validation. No installation.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="${SIOYEK_RUNTIME_DEPS_DIR:-$root/build/runtime-deps}"
meson="${MESON:-meson}"
jobs="${SIOYEK_JOBS:-2}"
for tool in curl sha256sum tar patch cmake ninja pkg-config qmake6 "$meson"; do
  command -v "$tool" >/dev/null || { printf 'Missing build tool: %s\n' "$tool" >&2; exit 1; }
done
qt_version="$(pkg-config --modversion Qt6Gui)"
case "$qt_version" in
  6.12.0)
    qt_source=qtbase-6.12.0
    qt_archive=qtbase-v6.12.0.tar.gz
    qt_url=https://github.com/qt/qtbase/archive/refs/tags/v6.12.0.tar.gz
    qt_checksum=dff6570582c272cbe744d2e460700318262af505460f4215276459970ae31b46
    qt_build=qt-build
    qt_install=qt-local
    ;;
  6.12.1)
    qt_source=qtbase-everywhere-src-6.12.1
    qt_archive=$qt_source.tar.xz
    qt_url=https://download.qt.io/official_releases/qt/6.12/6.12.1/submodules/$qt_archive
    qt_checksum=cd837bb9fd631e20439a04eaa0c2f252f19ad92fdf5adf10229d0fbc2fab2d53
    qt_build=qt-build-6.12.1
    qt_install=qt-local-6.12.1
    ;;
  *)
    printf 'Unsupported Qt version %s. This helper supports Qt 6.12.0 and 6.12.1.\n' "$qt_version" >&2
    exit 1
    ;;
esac
mkdir -p "$work"
work="$(cd -- "$work" && pwd)"
fetch() {
  local name="$1" url="$2" checksum="$3"
  if [[ ! -f "$work/$name" ]]; then
    curl --fail --location "$url" --output "$work/$name.part"
    mv -- "$work/$name.part" "$work/$name"
  fi
  (cd -- "$work" && printf '%s  %s\n' "$checksum" "$name" | sha256sum --check)
}
apply_patch_file() {
  local source_dir="$1" patch_file="$2"
  # Re-running the command is safe; a patch must apply exactly or already match.
  if ! patch --dry-run --force --fuzz=0 --reverse -d "$source_dir" -p1 < "$patch_file" >/dev/null 2>&1; then
    patch --force --fuzz=0 -d "$source_dir" -p1 < "$patch_file"
  fi
}
fetch "$qt_archive" "$qt_url" "$qt_checksum"
fetch wayland-1.26.0.tar.xz \
  https://gitlab.freedesktop.org/wayland/wayland/-/releases/1.26.0/downloads/wayland-1.26.0.tar.xz \
  64176eaa46e4969903e286f8e5ef8331affc17fdf03ac9b58381d2b23162b7a3
[[ -d "$work/$qt_source" ]] || tar -xf "$work/$qt_archive" -C "$work"
[[ -d "$work/wayland-1.26.0" ]] || tar -xf "$work/wayland-1.26.0.tar.xz" -C "$work"
if [[ "$qt_version" == 6.12.0 ]]; then
  apply_patch_file "$work/$qt_source" "$root/patches/qt/6.12.0-vulkan-initial-layout.patch"
fi
apply_patch_file "$work/$qt_source" "$root/patches/qt/6.12.0-wayland-compose-cleanup.patch"
apply_patch_file "$work/wayland-1.26.0" "$root/patches/wayland/1.26.0-destroyed-event-argument.patch"
cmake -S "$work/$qt_source" -B "$work/$qt_build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$work/$qt_install" \
  -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF -DFEATURE_opengl=ON \
  -DFEATURE_sql=OFF -DFEATURE_testlib=OFF -DFEATURE_system_textmarkdownreader=OFF
cmake --build "$work/$qt_build" --parallel "$jobs" \
  --target Gui WaylandClient QWaylandIntegrationPlugin
reconfigure=()
[[ ! -f "$work/wayland-build/build.ninja" ]] || reconfigure=(--reconfigure)
"$meson" setup "${reconfigure[@]}" "$work/wayland-build" "$work/wayland-1.26.0" \
  -Ddocumentation=false -Dtests=true -Db_sanitize=none
ninja -C "$work/wayland-build" -j "$jobs"
"$meson" test -C "$work/wayland-build" --print-errorlogs
printf '\nLocal dependency fixes built; no host libraries replaced. For this shell only:\n'
printf 'export LD_LIBRARY_PATH=%q:%q${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\n' "$work/$qt_build/lib" "$work/wayland-build/src"
printf 'export QT_PLUGIN_PATH=%q\n' "$(qmake6 -query QT_INSTALL_PLUGINS)"
