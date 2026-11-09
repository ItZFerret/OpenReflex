#!/bin/bash
# Pure host cache regression; no game files, renderer, session or full core build.
set -euo pipefail
terrain_test_root=$(cd "$(dirname "$0")/.." && pwd)
terrain_test_out=${1:?usage: terrain_dirty_cache_test.sh output-directory}
mkdir -p "$terrain_test_out"
clang -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
  "$terrain_test_root/tools/terrain_dirty_cache_test.c" -o "$terrain_test_out/terrain_dirty_cache_test"
"$terrain_test_out/terrain_dirty_cache_test"
# w-terrtiles: the 32 x 32 tile rectangles against a brute-force reference over thousands of frames
clang -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
  "$terrain_test_root/tools/terrain_tiles_test.c" -o "$terrain_test_out/terrain_tiles_test"
"$terrain_test_out/terrain_tiles_test"
