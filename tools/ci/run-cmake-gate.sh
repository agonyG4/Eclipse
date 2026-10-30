#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 1 ]]; then
    echo "usage: $0 <debug|release|clang|asan|ubsan|no-typhon|no-layer-shell>" >&2
    exit 2
fi

preset="$1"
case "$preset" in
    debug|release|clang|asan|ubsan)
        junit_mode="typhon"
        ;;
    no-typhon)
        junit_mode="no-typhon"
        ;;
    no-layer-shell)
        junit_mode="typhon"
        ;;
    *)
        echo "unknown CMake preset: $preset" >&2
        exit 2
        ;;
esac

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/../.." && pwd)"
source "$script_dir/set-build-output-env.sh"

build_dir="$(python3 "$script_dir/preset-binary-dir.py" "$project_root" "$preset")"
runtime_dir="$(mktemp -d "$ASTREA_TMPDIR/eclipse-$preset-runtime.XXXXXX")"

cleanup() {
    rm -rf -- "$runtime_dir"
}
trap cleanup EXIT

chmod 700 "$runtime_dir"
export XDG_RUNTIME_DIR="$runtime_dir"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

cd -- "$project_root"

printf 'CMake binary directory: %s\n' "$build_dir"
printf 'Cargo target directory: %s\n' "$ASTREA_CARGO_TARGET_DIR"
printf 'Cargo home: %s\n' "$CARGO_HOME"
printf 'Temporary directory: %s\n' "$ASTREA_TMPDIR"
printf 'XDG cache directory: %s\n' "$ASTREA_XDG_CACHE_HOME"
cmake --preset "$preset" -B "$build_dir"

if [[ "$junit_mode" == "typhon" ]]; then
cmake --build "$build_dir" --target \
        typhon-protocol-integration-test \
        typhon-shortcut-protocol-integration-test \
        shell-unified-runtime-integration-test
fi

cmake --build "$build_dir"

junit_path="$build_dir/ctest.junit.xml"
ctest --test-dir "$build_dir" --output-on-failure --parallel 1 \
    --output-junit "$junit_path"
python3 "$script_dir/check-ctest-junit.py" --mode "$junit_mode" "$junit_path"
