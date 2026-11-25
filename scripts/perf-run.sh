#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
profile="$root/build/perf/profile"

# Preserve X11 authentication when it comes from the login home.
if [[ -z "${XAUTHORITY:-}" && -f "$HOME/.Xauthority" ]]; then
  export XAUTHORITY="$HOME/.Xauthority"
fi

# Sioyek stores its databases under HOME even when XDG_DATA_HOME is set.
export HOME="$profile"
export XDG_CONFIG_HOME="$profile/.config"
export XDG_CONFIG_DIRS="$profile/empty-system-config"
export XDG_DATA_HOME="$profile/.local/share"
export XDG_CACHE_HOME="$profile/.cache"
export XDG_STATE_HOME="$profile/.local/state"

(umask 077; mkdir -p -- "$XDG_CONFIG_HOME" "$XDG_CONFIG_DIRS" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME")

if [[ $# -eq 0 ]]; then
  set -- "$root/tutorial.pdf"
fi

exec "$root/scripts/dev-run.sh" "$@"
