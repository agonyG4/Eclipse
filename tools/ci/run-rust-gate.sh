#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/../.." && pwd)"

cd -- "$project_root"
rustc --version
cargo --version

run_rust_gate() {
    local manifest="$1"
    cargo fmt --manifest-path "$manifest" -- --check
    cargo clippy --locked --manifest-path "$manifest" --all-targets -- -D warnings
    cargo test --locked --manifest-path "$manifest"
}

run_rust_gate Spotlight/backend/Cargo.toml
run_rust_gate Settings/backend/Cargo.toml
run_rust_gate shared/backend/Cargo.toml
