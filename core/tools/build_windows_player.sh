#!/bin/bash
# build_windows_player.sh -- the Windows x64 open player from this checkout: builds/windows-player/OpenReflex/ (+ builds/OpenReflex-Windows.zip).
# Builds the Windows core (core/tools/build_windows.sh), stages a copy of Unity/Reflex (the project itself is not touched), runs
# OpenReflexBuild.BuildWindowsBatch on the copy, then bundles the LGPL ffmpeg the videos need (the in-level screens decode Bink through
# it: Content/ReflexVideoSource.cs looks in <client root>/ffmpeg first) and zips the package without the debug-symbol folder.
#
# ffmpeg: BtbN/FFmpeg-Builds "ffmpeg-n9.0-latest-win64-lgpl-shared-9.0.zip" (LGPL, shared libraries), unpacked under
# builds/thirdparty/ffmpeg-win64/ (git-ignored). Fetch it once with:
#   gh release download latest -R BtbN/FFmpeg-Builds -p "ffmpeg-n9.0-latest-win64-lgpl-shared-9.0.zip" -p checksums.sha256
#   grep ffmpeg-n9.0-latest-win64-lgpl-shared-9.0.zip checksums.sha256 | shasum -a 256 -c -    (then unzip into builds/thirdparty/ffmpeg-win64)
set -euo pipefail
R="$(cd "$(dirname "$0")/../.." && pwd)"
UNITY="/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity"
DEPS="${REFLEX_WINDOWS_DEPS:-$R/builds/windows/plugin/deps}"
FF="$(ls -d "$R"/builds/thirdparty/ffmpeg-win64/ffmpeg-*-win64-lgpl-shared-* 2>/dev/null | head -1 || true)"
OUT="$R/builds/windows-player/OpenReflex"
STAGE="$R/builds/windows-stage/Reflex"

[ -n "$FF" ] && [ -f "$FF/bin/ffmpeg.exe" ] || { echo "missing the LGPL ffmpeg under builds/thirdparty/ffmpeg-win64 (see the header)"; exit 1; }
bash "$R/core/tools/build_windows.sh" "$R/builds/windows-core" "$DEPS"
mkdir -p "$STAGE"
rsync -a --delete --exclude Temp/ --exclude Logs/ --exclude Builds/ --exclude Library/ "$R/Unity/Reflex/" "$STAGE/"
rm -rf "$OUT"
REFLEX_RXCORE_PATH="$R/builds/windows-core/rxcore.dll" REFLEX_OPEN_PLAYER="$OUT/OpenReflex.exe" \
    "$UNITY" -batchmode -nographics -quit -projectPath "$STAGE" \
    -executeMethod Reflex.Port.EditorTools.OpenReflexBuild.BuildWindowsBatch -logFile "$R/builds/windows-player-build.log" || true
grep -q "\[open-build\] Succeeded" "$R/builds/windows-player-build.log" || { grep -E "error CS[0-9]+" "$R/builds/windows-player-build.log" | sort -u | head -40; echo "BUILD FAILED"; exit 1; }

# the bundled ffmpeg: the executable and the shared libraries it loads, its license, and where its source is
mkdir -p "$OUT/ffmpeg"
cp "$FF"/bin/ffmpeg.exe "$FF"/bin/*.dll "$OUT/ffmpeg/"
cp "$FF/LICENSE.txt" "$OUT/ffmpeg/LICENSE.txt"
cat > "$OUT/ffmpeg/README.txt" <<EOF
FFmpeg $(basename "$FF") (LGPL v2.1 or later, shared libraries), unmodified, from https://github.com/BtbN/FFmpeg-Builds/releases.
OPEN REFLEX runs it as a separate program to decode the game's Bink videos (in-level video screens, menus).
FFmpeg's source code: https://ffmpeg.org/download.html (the build scripts: https://github.com/BtbN/FFmpeg-Builds).
FFmpeg is a trademark of Fabrice Bellard. See LICENSE.txt.
EOF

git -C "$R" log --oneline -1 > "$OUT/BUILT_FROM.txt"
rm -f "$R/builds/OpenReflex-Windows.zip"
(cd "$R/builds/windows-player" && zip -qr -X "$R/builds/OpenReflex-Windows.zip" OpenReflex -x "OpenReflex/OpenReflex_BackUpThisFolder_ButDontShipItWithYourGame/*")
echo "BUILT $OUT and builds/OpenReflex-Windows.zip ($(cat "$OUT/BUILT_FROM.txt"))"
