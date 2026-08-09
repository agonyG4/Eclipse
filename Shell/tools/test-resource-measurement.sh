#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
output=$("$script_dir/measure-shell-resources.sh" \
    --pid "$$" --pid "$PPID" --interval 0.01 --repeat 2 --typhon-connections 1)

header=$(printf '%s\n' "$output" | sed -n '1p')
test "$header" = "process_count rss_kib pss_kib private_clean_kib private_dirty_kib private_kib threads fds cpu_percent typhon_shell_connections"
test "$(printf '%s\n' "$output" | tail -n +2 | wc -l)" -eq 2
process_count=$(printf '%s\n' "$output" | tail -n 1 | awk '{print $1}')
last_field=$(printf '%s\n' "$output" | tail -n 1 | awk '{print $10}')
test "$process_count" = 2
test "$last_field" = 1

echo "resource measurement: metrics and repeated sampling passed"
