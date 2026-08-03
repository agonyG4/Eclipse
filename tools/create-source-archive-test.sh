#!/usr/bin/env bash
set -euo pipefail

archive_tool=${1:?archive tool path is required}
test_root=$(mktemp -d)
cleanup() {
    rm -rf -- "$test_root"
}
trap cleanup EXIT

bash -n "$archive_tool"
bash -n "$0"

repo="$test_root/repo"
mkdir -p "$repo/tools" "$repo/src" "$test_root/elsewhere"
cp "$archive_tool" "$repo/tools/create-source-archive"
chmod +x "$repo/tools/create-source-archive"
printf '%s\n' 'build/' 'build-*' '*.cache' > "$repo/.gitignore"
printf '%s\n' 'source fixture' > "$repo/src/hello.txt"
git -C "$repo" init --quiet -b main
git -C "$repo" config user.email test@example.invalid
git -C "$repo" config user.name 'Archive Test'
git -C "$repo" add .
git -C "$repo" commit --quiet -m 'fixture'

archive="$test_root/source.zip"
(cd "$test_root/elsewhere" && "$repo/tools/create-source-archive" --output "$archive")

mkdir -p "$repo/build/stale" "$repo/build-debug"
(cd "$test_root/elsewhere" && "$repo/tools/create-source-archive" --output "$archive")

printf '%s\n' untracked > "$repo/untracked.txt"
if (cd "$test_root/elsewhere" && "$repo/tools/create-source-archive" --output "$archive" >/dev/null 2>&1); then
    printf '%s\n' 'Archive unexpectedly accepted an untracked worktree.' >&2
    exit 1
fi
(cd "$test_root/elsewhere" && "$repo/tools/create-source-archive" --allow-dirty --output "$archive" >/dev/null)

listing=$(unzip -Z1 "$archive")
printf '%s\n' "$listing" | grep -Fx 'Eclipse/src/hello.txt' >/dev/null
if printf '%s\n' "$listing" | grep -E '(^|/)\.git/|(^|/)build(-[^/]*)?(/|$)|untracked\.txt' >/dev/null; then
    printf '%s\n' 'Archive contains excluded content.' >&2
    exit 1
fi

printf '%s\n' 'Source archive test passed'
