#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/../.." && pwd)"

cd -- "$project_root"
python3 -m unittest discover -s tools/ci/tests -p 'test_*.py'
"$script_dir/run-rust-gate.sh"
"$script_dir/run-qml-gate.sh"

for preset in debug release clang asan ubsan no-typhon no-layer-shell; do
    "$script_dir/run-cmake-gate.sh" "$preset"
done
