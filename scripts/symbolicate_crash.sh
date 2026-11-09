#!/bin/bash
# Symbolicate a libreflexworld crash report (crash-<session>.txt from the player's log folder or a support ZIP's crashes/).
#
#   scripts/symbolicate_crash.sh <crash-*.txt> [symbols dir]
#
# The report's "plugin:" line gives the plugin's LC_UUID ("id") and load address; its backtrace lists "libreflexworld.dylib + 0x..."
# frames. The matching plugin is looked up in builds/symbols/*/plugin-id.txt (written by Editor/Build/ReflexBuildInfo.cs for every
# player build), else the current native/drafts/world_runtime/libreflexworld.dylib if its UUID matches. Frames are resolved with
# atos (the dSYM when the plugin was compiled with -g, else the dylib's own local symbol table: function names, no lines).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPORT="${1:?usage: symbolicate_crash.sh <crash report .txt> [symbols dir]}"
SYMS="${2:-}"

line=$(grep -m1 -E '^(plugin|core):' "$REPORT" || true)
ID=$(echo "$line" | sed -nE 's/.*, id ([^,]+), loaded at.*/\1/p')
BASE=$(echo "$line" | sed -nE 's/.*loaded at (0x[0-9a-fA-F]+).*/\1/p')
[ -n "$ID" ] || { echo "not a libreflexworld crash report (no 'plugin:' line): $REPORT" >&2; exit 1; }

uuid_of() { dwarfdump --uuid "$1" 2>/dev/null | awk '{print $2; exit}'; }

LIB=""
if [ -n "$SYMS" ]; then
	for c in "$SYMS/libreflexworld.dylib.dSYM" "$SYMS/libreflexworld.dylib"; do [ -e "$c" ] && { LIB="$c"; break; }; done
else
	for d in "$ROOT"/builds/symbols/*/; do
		[ -f "$d/plugin-id.txt" ] || continue
		if grep -qi "^$ID\$" "$d/plugin-id.txt"; then
			# a dSYM without DWARF (no -g) has no symbols at all: prefer the dylib then
			if [ -d "$d/libreflexworld.dylib.dSYM" ] && dwarfdump --debug-info "$d/libreflexworld.dylib.dSYM" 2>/dev/null | grep -q DW_TAG_subprogram; then LIB="$d/libreflexworld.dylib.dSYM"
			else LIB="$d/libreflexworld.dylib"; fi
			SYMS="$d"; break
		fi
	done
	if [ -z "$LIB" ]; then
		cur="$ROOT/native/drafts/world_runtime/libreflexworld.dylib"
		if [ -f "$cur" ] && [ "$(uuid_of "$cur")" = "$ID" ]; then LIB="$cur"; SYMS="$(dirname "$cur") (current build)"; fi
	fi
fi
[ -n "$LIB" ] || { echo "no symbols for plugin id $ID (looked in builds/symbols/*/plugin-id.txt and the current native build)" >&2; exit 2; }
echo "plugin id $ID, symbols: $SYMS"

frames=$(grep -E '^\s+#[0-9]+ 0x[0-9a-fA-F]+ libreflexworld\.dylib \+ 0x[0-9a-fA-F]+' "$REPORT" || true)
[ -n "$frames" ] || { echo "no libreflexworld frames in the backtrace"; exit 0; }
echo "$frames" | while read -r f; do
	a=$(echo "$f" | awk '{print $2}')
	echo "$f"
	echo "        -> $(atos -arch arm64 -o "$LIB" -l "$BASE" -i "$a" 2>/dev/null | grep -v '^$' | paste -sd';' - | sed 's/;/  <-inlined in- /g')"
done
