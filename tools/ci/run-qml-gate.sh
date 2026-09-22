#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/../.." && pwd)"

cd -- "$project_root"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

build_dir="${ASTREA_CMAKE_BUILD_DIR:-$project_root/build/debug}"
printf 'QML gate build directory: %s\n' "$(realpath -m "$build_dir")"

cmake --fresh -S "$project_root" -B "$build_dir" -G Ninja \
    -DBUILD_TESTING=ON \
    -DASTREA_BUILD_TESTS=ON \
    -DASTREA_ENABLE_LAYER_SHELL=ON \
    -DASTREA_ENABLE_TYPHON_BACKEND=ON \
    -DASTREA_ENABLE_ASAN=OFF \
    -DASTREA_ENABLE_UBSAN=OFF \
    -DASTREA_ENABLE_SANITIZERS=OFF \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build_dir" --target astrea-shell_qmllint astrea-settings-ui_qmllint
