#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/../.." && pwd)"

cd -- "$project_root"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

cmake --fresh --preset debug
cmake --build --preset debug --target astrea-shell_qmllint astrea-settings-ui_qmllint
