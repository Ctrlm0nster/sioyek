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
if [[ "$(pkg-config --modversion Qt6Gui)" != 6.12.0 ]]; then
  printf '%s\n' 'These Qt private-ABI patches were tested with Qt 6.12.0 only. Review before using another version.' >&2
  exit 1
fi
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
  if ! patch --dry-run --batch --fuzz=0 --reverse -d "$source_dir" -p1 < "$patch_file" >/dev/null 2>&1; then
    patch --batch --fuzz=0 -d "$source_dir" -p1 < "$patch_file"
  fi
}
fetch qtbase-v6.12.0.tar.gz \
  https://github.com/qt/qtbase/archive/refs/tags/v6.12.0.tar.gz \
  dff6570582c272cbe744d2e460700318262af505460f4215276459970ae31b46
fetch wayland-1.26.0.tar.xz \
  https://gitlab.freedesktop.org/wayland/wayland/-/releases/1.26.0/downloads/wayland-1.26.0.tar.xz \
  64176eaa46e4969903e286f8e5ef8331affc17fdf03ac9b58381d2b23162b7a3
[[ -d "$work/qtbase-6.12.0" ]] || tar -xf "$work/qtbase-v6.12.0.tar.gz" -C "$work"
[[ -d "$work/wayland-1.26.0" ]] || tar -xf "$work/wayland-1.26.0.tar.xz" -C "$work"
apply_patch_file "$work/qtbase-6.12.0" "$root/patches/qt/6.12.0-vulkan-initial-layout.patch"
apply_patch_file "$work/qtbase-6.12.0" "$root/patches/qt/6.12.0-wayland-compose-cleanup.patch"
apply_patch_file "$work/wayland-1.26.0" "$root/patches/wayland/1.26.0-destroyed-event-argument.patch"
cmake -S "$work/qtbase-6.12.0" -B "$work/qt-build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$work/qt-local" \
  -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF -DFEATURE_opengl=ON \
  -DFEATURE_sql=OFF -DFEATURE_testlib=OFF -DFEATURE_system_textmarkdownreader=OFF
cmake --build "$work/qt-build" --parallel "$jobs" \
  --target Gui WaylandClient QWaylandIntegrationPlugin
reconfigure=()
[[ ! -f "$work/wayland-build/build.ninja" ]] || reconfigure=(--reconfigure)
"$meson" setup "${reconfigure[@]}" "$work/wayland-build" "$work/wayland-1.26.0" \
  -Ddocumentation=false -Dtests=true -Db_sanitize=none
ninja -C "$work/wayland-build" -j "$jobs"
"$meson" test -C "$work/wayland-build" --print-errorlogs
printf '\nLocal dependency fixes built; no host libraries replaced. For this shell only:\n'
printf 'export LD_LIBRARY_PATH=%q:%q${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\n' "$work/qt-build/lib" "$work/wayland-build/src"
printf 'export QT_PLUGIN_PATH=%q\n' "$(qmake6 -query QT_INSTALL_PLUGINS)"
