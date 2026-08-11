#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 1 ]]; then
    echo "usage: $0 <debug|release|clang|asan|ubsan|no-typhon>" >&2
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
    *)
        echo "unknown CMake preset: $preset" >&2
        exit 2
        ;;
esac

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/../.." && pwd)"
build_dir="$project_root/build/$preset"
runtime_dir="$(mktemp -d "${TMPDIR:-/tmp}/eclipse-$preset-runtime.XXXXXX")"

cleanup() {
    rm -rf -- "$runtime_dir"
}
trap cleanup EXIT

chmod 700 "$runtime_dir"
export XDG_RUNTIME_DIR="$runtime_dir"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

cd -- "$project_root"

cmake --fresh --preset "$preset"

if [[ "$junit_mode" == "typhon" ]]; then
    cmake --build --preset "$preset" --target \
        typhon-protocol-integration-test \
        typhon-shortcut-protocol-integration-test \
        shell-unified-runtime-integration-test
fi

cmake --build --preset "$preset"

junit_path="$build_dir/ctest.junit.xml"
ctest --preset "$preset" --output-junit "$junit_path"
python3 "$script_dir/check-ctest-junit.py" "$junit_mode" "$junit_path"
