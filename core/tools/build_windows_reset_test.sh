#!/bin/bash
# Tiny test only, separate from the whole core. Run the output on Windows.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=${1:?output directory required};mkdir -p "$OUT";OUT="$(cd "$OUT" && pwd)"
for source in src/rx_reset_win.c tools/windows_reset_test.c;do
 name=$(basename "$source" .c)
 x86_64-w64-mingw32-gcc -std=c11 -O2 -fdata-sections -Isrc -c "$source" -o "$OUT/$name.raw"
 python3 tools/windows_core_sections.py "$OUT/$name.raw" "$OUT/$name.o"
done
x86_64-w64-mingw32-gcc "$OUT/rx_reset_win.o" "$OUT/windows_reset_test.o" -static -o "$OUT/windows_reset_test.exe"
