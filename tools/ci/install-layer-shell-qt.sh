#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 1 ]]; then
    echo "usage: $0 <install-prefix>" >&2
    exit 2
fi

install_prefix="$1"
ecm_version="6.14.0"
layer_shell_version="6.4.5"
temporary_directory="$(mktemp -d "${TMPDIR:-/tmp}/eclipse-layer-shell-qt.XXXXXX")"

cleanup() {
    rm -rf -- "$temporary_directory"
}
trap cleanup EXIT

download_and_verify() {
    local url="$1"
    local destination="$2"
    local checksum="$3"
    curl --fail --silent --show-error --location --retry 3 "$url" --output "$destination"
    printf '%s  %s\n' "$checksum" "$destination" | sha256sum --check --status
}

ecm_archive="$temporary_directory/extra-cmake-modules.tar.xz"
layer_shell_archive="$temporary_directory/layer-shell-qt.tar.xz"
download_and_verify \
    "https://download.kde.org/stable/frameworks/${ecm_version%.*}/extra-cmake-modules-${ecm_version}.tar.xz" \
    "$ecm_archive" \
    "d02cbbb3269b39680884abf6f14ba68f448570c554173f5249da3b8761784c13"
download_and_verify \
    "https://download.kde.org/stable/plasma/${layer_shell_version}/layer-shell-qt-${layer_shell_version}.tar.xz" \
    "$layer_shell_archive" \
    "ef6baae22114f038af89029f3f0075ee29c3b91fd49100828c4c3a32e1496e95"

mkdir -p "$temporary_directory/ecm" "$temporary_directory/layer-shell-qt"
tar --extract --file "$ecm_archive" --directory "$temporary_directory/ecm" --strip-components=1
tar --extract --file "$layer_shell_archive" \
    --directory "$temporary_directory/layer-shell-qt" --strip-components=1

cmake -S "$temporary_directory/ecm" -B "$temporary_directory/ecm-build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_prefix" \
    -DBUILD_DOC=OFF \
    -DBUILD_TESTING=OFF
cmake --build "$temporary_directory/ecm-build" --target install

qt_prefix="$(qtpaths --install-prefix)"
cmake -S "$temporary_directory/layer-shell-qt" -B "$temporary_directory/layer-shell-qt-build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_prefix" \
    -DCMAKE_PREFIX_PATH="${install_prefix};${qt_prefix}" \
    -DBUILD_TESTING=OFF
cmake --build "$temporary_directory/layer-shell-qt-build" --target install

test -f "$install_prefix/lib/cmake/LayerShellQt/LayerShellQtConfig.cmake"
test -f "$install_prefix/include/LayerShellQt/Window"
