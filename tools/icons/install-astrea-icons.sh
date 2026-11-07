#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# install-astrea-icons.sh — the Astrea icon themes, Astrea-light and
# Astrea-dark, from a MacOS-Like checkout (https://github.com/Scrimas/MacOS-Like).
#
# Each theme is MacTahoe (MacOS-Like's vendor/) with MacOS-Like's prebuilt app
# icons over it: one drop shadow under every tile, the common white and
# graphite tiles, brand tiles, real dark tiles in the dark theme. All of it is
# vector art, so every size is crisp.
#
# What MacOS-Like's setup.sh then bakes into fixed-size PNGs (tiles for apps
# MacTahoe has no artwork for) Astrea makes at runtime instead: the theme opts
# in with X-Astrea-AppTiles=true, and the shell's icon provider wraps every app
# icon the theme lacks in the same tile, at the exact size it presents, in the
# variant of the current appearance. Nothing to rerun after installing apps.
#
# Usage: install-astrea-icons.sh [--apply] [MACOS_LIKE_DIR]
#   MACOS_LIKE_DIR  MacOS-Like checkout (default: ~/Downloads/MacOS-Like-main)
#   --apply         also select the themes (system_icon_theme "Astrea" in
#                   ~/.config/AstreaOS/ui/theme.json, light/dark by appearance)

set -euo pipefail

APPLY=0
SRC=""
for arg in "$@"; do
  case "$arg" in
    --apply) APPLY=1 ;;
    -h|--help) sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "unknown option: $arg" >&2; exit 2 ;;
    *) SRC="$arg" ;;
  esac
done
SRC="${SRC:-$HOME/Downloads/MacOS-Like-main}"
MACTAHOE="$SRC/vendor/MacTahoe"
MACTAHOE_DARK="$SRC/vendor/MacTahoe-dark"
PREBUILT="$SRC/prebuilt"
command -v rsync >/dev/null || { echo "rsync is required" >&2; exit 1; }
for d in "$MACTAHOE" "$MACTAHOE_DARK" "$PREBUILT/Light" "$PREBUILT/Dark"; do
  [ -d "$d" ] || { echo "not a MacOS-Like checkout: $d is missing" >&2; exit 1; }
done

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
ICONS="$DATA_HOME/icons"
mkdir -p "$ICONS"

theme_installed() {
  local b
  for b in "$HOME/.icons" "$ICONS" /usr/local/share/icons /usr/share/icons; do
    [ -f "$b/$1/index.theme" ] && return 0
  done
  return 1
}

# What MacTahoe lacks (actions/, status/, mimetypes/ names) comes from the first
# of these that is installed; hicolor always closes the chain.
parent_chain() {
  local t
  for t in "$@"; do
    theme_installed "$t" && { echo "$t,hicolor"; return 0; }
  done
  echo hicolor
}

# MacTahoe's index.theme with the identity, inherit chain and tile opt-in.
write_index() {
  local dst="$1" name="$2" inherits="$3"
  awk -v name="$name" -v inherits="$inherits" '
    /^Name=/ { print "Name=" name; next }
    /^Comment=/ { print "Comment=Astrea: MacTahoe via MacOS-Like, app tiles made at runtime"; next }
    /^Inherits=/ { print "Inherits=" inherits; next }
    /^KDE-Extensions=/ { print "KDE-Extensions=.svg,.png"; next }
    /^\[Icon Theme\]/ { print; print "X-Astrea-AppTiles=true"; next }
    { print }
  ' "$MACTAHOE/index.theme" > "$dst/index.theme"
}

# Icon= lookup is case-sensitive: "vesktop" misses MacTahoe's Vesktop.svg.
# Give every mixed-case app icon a lower-case alias.
link_case_aliases() {
  local d f base lower
  for d in "$1"/apps/*/; do
    for f in "$d"*.svg; do
      [ -e "$f" ] || continue
      base="${f##*/}"; base="${base%.svg}"
      lower="${base,,}"
      if [ "$lower" != "$base" ] && [ ! -e "$d$lower.svg" ]; then
        ln -s "$base.svg" "$d$lower.svg"
      fi
    done
  done
}

build() {
  local name="$1" inherits="$2"; shift 2
  local stage="$ICONS/.$name.new" layer
  rm -rf "$stage"
  mkdir -p "$stage"
  # Later layers add and replace. Links that leave their layer (MacTahoe-dark's
  # dirs pointing back into MacTahoe) are skipped: that content is already
  # there from the layer below, as setup.sh does it.
  for layer in "$@"; do
    rsync -a --safe-links "$layer/" "$stage/"
  done
  write_index "$stage" "$name" "$inherits"
  link_case_aliases "$stage"
  gtk4-update-icon-cache -q -f "$stage" 2>/dev/null \
    || gtk-update-icon-cache -q -f "$stage" 2>/dev/null || true
  rm -rf "$ICONS/.$name.old"
  if [ -e "$ICONS/$name" ]; then mv "$ICONS/$name" "$ICONS/.$name.old"; fi
  mv "$stage" "$ICONS/$name"
  rm -rf "$ICONS/.$name.old"
  echo "  $ICONS/$name (inherits $inherits)"
}

echo "Building the Astrea icon themes from $SRC"
build Astrea-light "$(parent_chain breeze-plus breeze Papirus-Light Papirus Adwaita)" \
  "$MACTAHOE" "$PREBUILT/Light"
build Astrea-dark "$(parent_chain breeze-plus-dark breeze-dark Papirus-Dark Adwaita)" \
  "$MACTAHOE" "$MACTAHOE_DARK" "$PREBUILT/Dark"

if [ "$APPLY" -eq 1 ]; then
  CONFIG="${XDG_CONFIG_HOME:-$HOME/.config}/AstreaOS/ui/theme.json"
  mkdir -p "${CONFIG%/*}"
  python3 -I - "$CONFIG" <<'PYEOF'
import json, os, sys, tempfile
path = sys.argv[1]
try:
    with open(path, encoding='utf-8') as f:
        config = json.load(f)
except FileNotFoundError:
    config = {}
config['system_icon_theme'] = 'Astrea'
fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path))
with os.fdopen(fd, 'w', encoding='utf-8') as f:
    json.dump(config, f, indent=2)
    f.write('\n')
os.replace(tmp, path)
PYEOF
  echo "Selected: system_icon_theme = Astrea (Astrea-light / Astrea-dark by appearance)"
fi
