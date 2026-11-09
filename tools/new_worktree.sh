#!/bin/bash
# tools/new_worktree.sh <name> [base] -- a git worktree for one agent at ../REFLEX-<name> on branch <name> (from base, default release),
# with symlinks to the untracked inputs the builds and tests need (game data, test data, the capstone library of the emulator,
# the reused game-file extraction).
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"; N=${1:?name}; BASE=${2:-release}; W="$(dirname "$R")/REFLEX-$N"
git -C "$R" worktree add -q "$W" -b "$N" "$BASE"
ln -s "$R/game files" "$W/game files"
ln -s "$R/testdata" "$W/testdata"
mkdir -p "$W/native/drafts/world_runtime/lib"; ln -sf "$R/native/drafts/world_runtime/lib/libcapstone.5.dylib" "$W/native/drafts/world_runtime/lib/libcapstone.5.dylib"
mkdir -p "$W/builds"; ln -s "$R/builds/sol_garage" "$W/builds/sol_garage"; ln -s "$R/builds/windows" "$W/builds/windows"
echo "$W"
