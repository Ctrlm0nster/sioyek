#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${SIOYEK_BUILD_DIR:-$root/build/dev}"
cmake --preset dev -S "$root" -B "$build_dir" "$@"
cmake --build "$build_dir" --parallel "${SIOYEK_JOBS:-2}"
