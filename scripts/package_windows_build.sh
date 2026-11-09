#!/bin/bash
# The Windows x64 player of the Reflex client, packaged (notes: _re/notes/client-windows.md). Local only: it never publishes, never
# uploads, and never touches Unity/Reflex (the player is built from a batchmode clone; the DLL goes into the clone only).
#
#   scripts/package_windows_build.sh ["label"] [--no-native] [--dev] [--no-zip] [--no-check]
#
#   --no-native  use the existing <plugin dir>/reflexworld.dll as it is (else scripts/windows/build_plugin_windows.sh, incremental)
#   --dev        a development player
#   --no-zip     the folder only
#   --no-check   skip scripts/windows/check_dll_windows.sh (PE / imports / exports vs the macOS plugin / C# imports / struct sizes)
#
# Steps:
#   1. native: scripts/windows/build_plugin_windows.sh <plugin dir> (MinGW reflexworld.dll; needs the static capstone / zlib of
#      scripts/windows/build_deps_windows.sh in <plugin dir>/deps or REFLEX_WIN_DEPS), then check_dll_windows.sh (static checks).
#   2. the clone ($REFLEX_BUILD_CLONE_WIN, default ~/Library/Caches/ReflexBuild/clone-windows: Assets / ProjectSettings / Packages of
#      Unity/Reflex, rsync'd; Library stays the clone's own). The DLL is copied to the clone's Assets/Plugins/x86_64/reflexworld.dll.
#   3. player: Unity -batchmode -buildTarget Win64 ... ReflexBuild.BuildWindowsBatch into a staging folder (ReflexBuild sets the
#      plugin import settings in the clone, checks <name>_Data/Plugins/x86_64/reflexworld.dll, no dylib, no game data; ReflexBuildInfo
#      stamps it and keeps reflexworld.dll + .debug in builds/symbols/<id>/).
#   4. the staged player replaces builds/windows/<product>/ (Unity's *_DoNotShip / *_ButDontShipItWithYourGame folders removed) and is
#      zipped to builds/windows/<product>-windows-x64.zip; a blank "Built <time> - <label>.txt" beside it.
#   Nothing is run: there is no Windows machine here (what to test there: the notes).
#
# Environment: REFLEX_BUILD_SCRATCH (clone, staging, logs; default ~/Library/Caches/ReflexBuild), REFLEX_BUILD_CLONE_WIN,
# REFLEX_BUILD_OUT (instead of REFLEX/builds), REFLEX_BUILD_PRODUCT (another productName in the clone only), REFLEX_WIN_PLUGIN_DIR
# (default <builds>/windows/plugin), REFLEX_WIN_DEPS, REFLEX_UNITY, and build_plugin_windows.sh's REFLEX_PORT_FILES / _EXCLUDE.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJ="$ROOT/Unity/Reflex"
UNITY="${REFLEX_UNITY:-/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity}"
SCRATCH="${REFLEX_BUILD_SCRATCH:-$HOME/Library/Caches/ReflexBuild}"
CLONE="${REFLEX_BUILD_CLONE_WIN:-$SCRATCH/clone-windows}"
BUILDS="${REFLEX_BUILD_OUT:-$ROOT/builds}"
PLUG="${REFLEX_WIN_PLUGIN_DIR:-$BUILDS/windows/plugin}"
LABEL=""; NATIVE=1; DEV=""; ZIP=1; CHECK=1
for arg in "$@"; do
	case "$arg" in
		--no-native) NATIVE=0 ;;
		--dev) DEV="-reflexDevelopment" ;;
		--no-zip) ZIP=0 ;;
		--no-check) CHECK=0 ;;
		-h|--help) sed -n '2,30p' "$0"; exit 0 ;;
		-*) echo "[package-windows] unknown option: $arg" >&2; exit 2 ;;
		*) LABEL="$arg" ;;
	esac
done
say() { echo "[package-windows] $*"; }
mkdir -p "$SCRATCH" "$PLUG"
LOG="$SCRATCH/build-windows.log"

# 1. native
if [ $NATIVE = 1 ]; then
	say "native: build_plugin_windows.sh $PLUG"
	bash "$ROOT/scripts/windows/build_plugin_windows.sh" "$PLUG" || { say "NATIVE BUILD FAILED"; exit 1; }
fi
DLL="$PLUG/reflexworld.dll"
[ -f "$DLL" ] || { say "no plugin at $DLL"; exit 1; }
if [ $CHECK = 1 ]; then
	bash "$ROOT/scripts/windows/check_dll_windows.sh" "$PLUG" || { say "DLL CHECK FAILED (--no-check to package anyway, e.g. when only the macOS dylib is stale)"; exit 1; }
fi

# 2. clone
C="$CLONE/Reflex"
if [ ! -d "$C" ]; then say "creating the Windows build clone $C"; mkdir -p "$C"; fi
rsync -a --delete --exclude 'Plugins/x86_64/reflexworld.dll' --exclude 'Plugins/x86_64/reflexworld.dll.meta' --exclude 'Plugins/x86_64.meta' "$PROJ/Assets/" "$C/Assets/"
rsync -a --delete "$PROJ/ProjectSettings/" "$C/ProjectSettings/"
[ -n "${REFLEX_BUILD_PRODUCT:-}" ] && sed -i '' "s/^  productName: .*/  productName: $REFLEX_BUILD_PRODUCT/" "$C/ProjectSettings/ProjectSettings.asset"
rsync -a --delete "$PROJ/Packages/" "$C/Packages/"
mkdir -p "$C/Assets/Plugins/x86_64"
cmp -s "$DLL" "$C/Assets/Plugins/x86_64/reflexworld.dll" || { cp "$DLL" "$C/Assets/Plugins/x86_64/.reflexworld.new" && mv -f "$C/Assets/Plugins/x86_64/.reflexworld.new" "$C/Assets/Plugins/x86_64/reflexworld.dll"; }
[ -d "$C/Assets/StreamingAssets" ] && say "Assets/StreamingAssets exists: no game data may ship (ReflexBuild refuses it)"

# 3. player
PRODUCT="$(sed -n 's/^  productName: //p' "$C/ProjectSettings/ProjectSettings.asset")"
STAGE="$SCRATCH/stage-windows/$PRODUCT"
OUT="$BUILDS/windows/$PRODUCT"
say "building the Windows x64 player ($PRODUCT${DEV:+, development})"
rm -rf "$(dirname "$STAGE")"; mkdir -p "$STAGE"
REFLEX_REPO="$ROOT" "$UNITY" -batchmode -quit -buildTarget Win64 -projectPath "$C" -executeMethod Reflex.Port.EditorTools.ReflexBuild.BuildWindowsBatch \
	-reflexBuildOut "$STAGE/$PRODUCT.exe" -reflexSymbolsDir "$BUILDS/symbols" -reflexPluginDebug "$PLUG/reflexworld.dll.debug" $DEV -logFile "$LOG" >/dev/null 2>&1 || true
grep -q "\[Reflex\] build Succeeded" "$LOG" || { grep -E "error CS|\[Reflex\] build" "$LOG" | tail -20; say "BUILD FAILED (log: $LOG)"; exit 1; }
grep -E "\[Reflex\] build Succeeded" "$LOG" | sed 's/^/  /'
cmp -s "$DLL" "$STAGE/${PRODUCT}_Data/Plugins/x86_64/reflexworld.dll" || { say "the player's reflexworld.dll is not the one just built"; exit 1; }
rm -rf "$STAGE"/*_BurstDebugInformation_DoNotShip "$STAGE"/*_BackUpThisFolder_ButDontShipItWithYourGame
[ -z "$(find "$STAGE" -iname '*.dylib' -o -iname '*.package' -o -iname '*.bik' -o -iname '*.fpack' | head -1)" ] || { say "the player holds a dylib or original game files"; exit 1; }

# 4. swap in + zip
mkdir -p "$BUILDS/windows"
rm -rf "$OUT.old"; [ -d "$OUT" ] && mv "$OUT" "$OUT.old"
mv "$STAGE" "$OUT"; rm -rf "$OUT.old"
if [ $ZIP = 1 ]; then
	Z="$BUILDS/windows/$PRODUCT-windows-x64.zip"; rm -f "$Z"
	(cd "$BUILDS/windows" && zip -qrX "$Z" "$PRODUCT")
	say "zip: $Z ($(du -h "$Z" | cut -f1 | tr -d ' '), sha256 $(shasum -a 256 "$Z" | cut -c1-16)...)"
fi
rm -f "$BUILDS/windows/"Built\ *.txt
LABEL="${LABEL//\//-}"; LABEL="${LABEL//:/-}"
: > "$BUILDS/windows/Built $(date '+%Y-%m-%d %H.%M')${LABEL:+ - $LABEL}.txt"
say "done (local only, nothing published, not run: no Windows machine here): $OUT"
