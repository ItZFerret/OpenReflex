#!/bin/bash
# Installs the plugin into the Unity project. Nothing from the game is copied: the client reads the player's install
# at run time (ReflexInstall) and builds its world images into the user cache (ReflexWorldFiles).
# usage: install_unity.sh   (restart Unity afterwards to load the new dylib)
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../.. && pwd)"
DST="$ROOT/Unity/Reflex/Assets/Plugins/macOS"
mkdir -p "$DST"
for f in libreflexworld.dylib lib/libcapstone.5.dylib; do b=$(basename "$f"); cp "$f" "$DST/.$b.new"; mv -f "$DST/.$b.new" "$DST/$b"; done   # atomic: never overwrite a loaded dylib in place
echo "installed $(ls "$DST" | grep -v meta | tr '\n' ' ')"
