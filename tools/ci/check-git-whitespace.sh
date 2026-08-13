#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 pr <base> <head>" >&2
    echo "       $0 push <before> <head>" >&2
    echo "       $0 commit <commit>" >&2
}

if [[ "$#" -lt 1 ]]; then
    usage
    exit 2
fi

mode="$1"
zero_sha="0000000000000000000000000000000000000000"

run_diff_check() {
    if git diff --check "$1"; then
        return 0
    fi
    return 1
}

run_commit_check() {
    if git show --check --pretty=format: "$1"; then
        return 0
    fi
    return 1
}

case "$mode" in
    pr)
        if [[ "$#" -ne 3 ]]; then
            usage
            exit 2
        fi
        run_diff_check "$2...$3"
        ;;
    push)
        if [[ "$#" -ne 3 ]]; then
            usage
            exit 2
        fi
        if [[ "$2" == "$zero_sha" ]]; then
            run_commit_check "$3"
        else
            run_diff_check "$2..$3"
        fi
        ;;
    commit)
        if [[ "$#" -ne 2 ]]; then
            usage
            exit 2
        fi
        run_commit_check "$2"
        ;;
    *)
        usage
        exit 2
        ;;
esac
