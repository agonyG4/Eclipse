#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/../.." && pwd)"
source "$script_dir/set-build-output-env.sh"

cd -- "$project_root"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

build_dir="$(realpath -m "${ASTREA_CMAKE_BUILD_DIR:-$ASTREA_CMAKE_BUILD_ROOT/Eclipse-build}")"
case "$build_dir" in
    "$ASTREA_CMAKE_BUILD_ROOT"|"$ASTREA_CMAKE_BUILD_ROOT"/*) ;;
    *)
        echo "QML gate build directory is outside ASTREA_CMAKE_BUILD_ROOT: $build_dir" >&2
        exit 2
        ;;
esac
printf 'QML gate build directory: %s\n' "$build_dir"
printf 'Cargo target directory: %s\n' "$(realpath -m "$CARGO_TARGET_DIR")"
printf 'Temporary directory: %s\n' "$(realpath -m "$TMPDIR")"
printf 'XDG cache directory: %s\n' "$(realpath -m "$XDG_CACHE_HOME")"

cmake -S "$project_root" -B "$build_dir" -G Ninja \
    -DBUILD_TESTING=ON \
    -DASTREA_BUILD_TESTS=ON \
    -DASTREA_BUILD_SDK_ONLY=OFF \
    -DASTREA_ENABLE_LAYER_SHELL=ON \
    -DASTREA_ENABLE_TYPHON_BACKEND=ON \
    -DASTREA_ENABLE_ASAN=OFF \
    -DASTREA_ENABLE_UBSAN=OFF \
    -DASTREA_ENABLE_SANITIZERS=OFF \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build_dir" --target astrea-shell_qmllint astrea-settings-ui_qmllint
