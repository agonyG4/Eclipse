#!/usr/bin/env bash
# Source this file before running local compilation or test gates.

ASTREA_CMAKE_BUILD_ROOT="${ASTREA_CMAKE_BUILD_ROOT:-/mnt/Aether/Desktop/GitHub}"
if [[ "$ASTREA_CMAKE_BUILD_ROOT" != /* ]]; then
    echo "ASTREA_CMAKE_BUILD_ROOT must be an absolute path: $ASTREA_CMAKE_BUILD_ROOT" >&2
    return 2
fi
ASTREA_CMAKE_BUILD_ROOT="$(realpath -m "$ASTREA_CMAKE_BUILD_ROOT")"
case "$ASTREA_CMAKE_BUILD_ROOT" in
    /mnt/Aether/Desktop/GitHub|/mnt/Aether/Desktop/GitHub/*) ;;
    *)
        if [[ -z "${RUNNER_TEMP:-}" || "$ASTREA_CMAKE_BUILD_ROOT" != "$(realpath -m "$RUNNER_TEMP")" ]]; then
            echo "ASTREA_CMAKE_BUILD_ROOT must use Aether locally or equal RUNNER_TEMP in CI: $ASTREA_CMAKE_BUILD_ROOT" >&2
            return 2
        fi
        ;;
esac

ASTREA_CARGO_TARGET_DIR="${ASTREA_CARGO_TARGET_DIR:-$ASTREA_CMAKE_BUILD_ROOT/Eclipse-target}"
ASTREA_CARGO_HOME="${ASTREA_CARGO_HOME:-$ASTREA_CMAKE_BUILD_ROOT/cargo-home}"
ASTREA_TMPDIR="${ASTREA_TMPDIR:-$ASTREA_CMAKE_BUILD_ROOT/tmp}"
ASTREA_XDG_CACHE_HOME="${ASTREA_XDG_CACHE_HOME:-$ASTREA_CMAKE_BUILD_ROOT/cache}"

for astrea_path_name in ASTREA_CARGO_TARGET_DIR ASTREA_CARGO_HOME ASTREA_TMPDIR ASTREA_XDG_CACHE_HOME; do
    astrea_path="${!astrea_path_name}"
    if [[ "$astrea_path" != /* ]]; then
        echo "$astrea_path_name must be an absolute path: $astrea_path" >&2
        return 2
    fi
    astrea_path="$(realpath -m "$astrea_path")"
    case "$astrea_path" in
        "$ASTREA_CMAKE_BUILD_ROOT"|"$ASTREA_CMAKE_BUILD_ROOT"/*) ;;
        *)
            echo "$astrea_path_name is outside ASTREA_CMAKE_BUILD_ROOT: $astrea_path" >&2
            return 2
            ;;
    esac
    printf -v "$astrea_path_name" '%s' "$astrea_path"
done

export ASTREA_CMAKE_BUILD_ROOT ASTREA_CARGO_TARGET_DIR ASTREA_CARGO_HOME
export ASTREA_TMPDIR ASTREA_XDG_CACHE_HOME
export CARGO_TARGET_DIR="$ASTREA_CARGO_TARGET_DIR"
export CARGO_HOME="$ASTREA_CARGO_HOME"
export TMPDIR="$ASTREA_TMPDIR"
export XDG_CACHE_HOME="$ASTREA_XDG_CACHE_HOME"
export PYTHONDONTWRITEBYTECODE=1

mkdir -p -- "$CARGO_TARGET_DIR" "$CARGO_HOME" "$TMPDIR" "$XDG_CACHE_HOME"
