#!/bin/bash
# build_mac_player.sh -- the playable macOS open player from this checkout, at a fixed path: builds/player/OpenReflex.app
# (the Desktop shortcut points there, so a rebuild keeps it working). Builds the core, stages a copy of Unity/Reflex (the
# project itself is not touched; an open editor is not disturbed), then runs OpenReflexBuild.BuildMacBatch on the copy.
# No test hooks, no probes: the same player a user runs.
set -euo pipefail
R="$(cd "$(dirname "$0")/../.." && pwd)"
UNITY="/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity"
OUT="$R/builds/player"
STAGE="$OUT/stage/Reflex"
APP="$OUT/OpenReflex.app"

bash "$R/core/build.sh"
mkdir -p "$STAGE"
# the stage keeps its own Library (import cache) between builds; the first build seeds it from the test player's stage when there is one
if [ ! -d "$STAGE/Library" ] && [ -d "$R/builds/moto_flow/stage/Reflex/Library" ]; then cp -R "$R/builds/moto_flow/stage/Reflex/Library" "$STAGE/Library"; fi
rsync -a --delete --exclude Temp/ --exclude Logs/ --exclude Builds/ --exclude Library/ "$R/Unity/Reflex/" "$STAGE/"

# build next to the final path, then swap, so a failed build leaves the previous player in place
rm -rf "$OUT/OpenReflex.new.app"
REFLEX_RXCORE_PATH="$R/core/build/librxcore.dylib" REFLEX_OPEN_PLAYER="$OUT/OpenReflex.new.app" \
    "$UNITY" -batchmode -nographics -quit -projectPath "$STAGE" \
    -executeMethod Reflex.Port.EditorTools.OpenReflexBuild.BuildMacBatch -logFile "$OUT/build.log" || true
if ! grep -q "\[open-build\] Succeeded" "$OUT/build.log" || [ ! -d "$OUT/OpenReflex.new.app" ]; then
    grep -E "error CS[0-9]+" "$OUT/build.log" | sort -u | head -40 || true
    echo "BUILD FAILED (log: $OUT/build.log)"; exit 1
fi
# the core is copied in after Unity signs the bundle: sign it again (ad hoc) so macOS accepts the sealed resources
codesign --force --deep --sign - "$OUT/OpenReflex.new.app"
rm -rf "$APP" "$OUT/OpenReflex.new_BackUpThisFolder_ButDontShipItWithYourGame"; mv "$OUT/OpenReflex.new.app" "$APP"
git -C "$R" log --oneline -1 > "$OUT/BUILT_FROM.txt"
echo "BUILT $APP ($(cat "$OUT/BUILT_FROM.txt"))"
