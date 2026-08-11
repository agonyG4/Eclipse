#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/../.." && pwd)"

cd -- "$project_root"
rustc --version
cargo --version

cargo fmt --manifest-path Spotlight/backend/Cargo.toml -- --check
cargo clippy --locked --manifest-path Spotlight/backend/Cargo.toml --all-targets -- -D warnings
cargo test --locked --manifest-path Spotlight/backend/Cargo.toml
