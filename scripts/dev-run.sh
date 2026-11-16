#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${SIOYEK_BUILD_DIR:-$root/build/dev}"
if [[ ! -x "$build_dir/sioyek" ]]; then
  printf '%s\n' 'Build first with scripts/dev-build.sh' >&2
  exit 1
fi
# Keep a running system installation from receiving this development launch.
exec "$build_dir/sioyek" --new-instance "$@"
