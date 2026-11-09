#!/bin/bash
# TEST build of the Reflex client (after Open Unleashed's scripts/build_test.sh + package_build.sh). Local only: it never publishes,
# never uploads, and never installs the plugin into Unity/Reflex (the lead / the user does that).
#
#   scripts/build_test.sh ["label"] [--no-verify] [--no-native] [--no-smoke] [--windowed] [--dev]
#
#   --no-verify  build the plugin with build.sh but skip the hash verification (verify_install.sh needs the lead's scratchpad worlds)
#   --no-native  use native/drafts/world_runtime/libreflexworld.dylib as it is
#   --no-smoke   skip the player runs
#   --windowed   smoke-run the player in a 1280x720 window (real frame rates in the perf lines) instead of -batchmode
#   --dev        a development player (profiler, script debugging)
#
# Steps:
#   1. native: native/game/tools/verify_install.sh noinstall (build.sh: plugin + CLI; the five verify worlds' state / memory hashes
#      with and without the native ports; the default world byte-identical to the reference). Never installs.
#   2. the build clone: the user keeps Unity/Reflex open in the Editor, so the player is built from a batchmode clone
#      ($REFLEX_BUILD_CLONE, default ~/Library/Caches/ReflexBuild/clone: an APFS copy of the project, Library included, kept in sync
#      with rsync). The clone's Assets/Plugins/macOS get the plugin built in step 1; the user's project is not touched.
#   3. player: Reflex.Port.EditorTools.ReflexBuild.BuildMacBatch into a staging .app; ReflexBuildInfo stamps it and keeps the
#      plugin's symbols in builds/symbols/<build id>/ (scripts/symbolicate_crash.sh).
#   4. smoke (guest: the player's save is never read or written; logs in a temp folder, -reflexLogDir):
#      a. -reflexSmoke 30: the front end runs, then a Concord race with W held; "[Reflex] smoke OK" and perf lines in the session log
#      b. -reflexCrashTest native: the plugin's crash handler writes crash-<session>.txt
#      c. -reflexCreateReport: the next start finds the crash and the support ZIP holds it
#   5. the tested player replaces builds/macOS/<product>.app; a blank "Built <time> - <label>.txt" beside it.
#
# Environment: REFLEX_GAME_DIR (the install for the smoke runs; default "game files/Archive"), REFLEX_BUILD_SCRATCH (clone, staging,
# logs; default ~/Library/Caches/ReflexBuild), REFLEX_BUILD_CLONE, REFLEX_BUILD_OUT (instead of REFLEX/builds: the player, the
# symbols), REFLEX_BUILD_PRODUCT (another productName in the clone only: a test build whose PlayerPrefs / persistentDataPath stay
# apart from the real game's), REFLEX_UNITY.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJ="$ROOT/Unity/Reflex"
W="$ROOT/native/drafts/world_runtime"
UNITY="${REFLEX_UNITY:-/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity}"
SCRATCH="${REFLEX_BUILD_SCRATCH:-$HOME/Library/Caches/ReflexBuild}"
CLONE="${REFLEX_BUILD_CLONE:-$SCRATCH/clone}"
GAME="${REFLEX_GAME_DIR:-$ROOT/game files/Archive}"
BUILDS="${REFLEX_BUILD_OUT:-$ROOT/builds}"
LABEL=""; VERIFY=1; NATIVE=1; SMOKE=1; WINDOWED=0; DEV=""
for arg in "$@"; do
	case "$arg" in
		--no-verify) VERIFY=0 ;;
		--no-native) NATIVE=0 ;;
		--no-smoke) SMOKE=0 ;;
		--windowed) WINDOWED=1 ;;
		--dev) DEV="-reflexDevelopment" ;;
		-h|--help) sed -n '2,30p' "$0"; exit 0 ;;
		-*) echo "[build-test] unknown option: $arg" >&2; exit 2 ;;
		*) LABEL="$arg" ;;
	esac
done
say() { echo "[build-test] $*"; }
mkdir -p "$SCRATCH"
LOG="$SCRATCH/build.log"

# 1. native
if [ $NATIVE = 1 ]; then
	if [ $VERIFY = 1 ]; then
		say "native: build + verify (verify_install.sh noinstall)"
		bash "$ROOT/native/game/tools/verify_install.sh" noinstall >"$SCRATCH/native.log" 2>&1 || { tail -15 "$SCRATCH/native.log"; say "NATIVE VERIFY FAILED (log: $SCRATCH/native.log)"; exit 1; }
		grep -E "ok|IDENTICAL" "$SCRATCH/native.log" | sed 's/^/  /'
	else
		say "native: build.sh"
		(cd "$W" && ./build.sh) >"$SCRATCH/native.log" 2>&1 || { grep -E " error" "$SCRATCH/native.log" | head; say "NATIVE BUILD FAILED (log: $SCRATCH/native.log)"; exit 1; }
	fi
fi
PLUGIN="$W/libreflexworld.dylib"
[ -f "$PLUGIN" ] || { say "no plugin at $PLUGIN"; exit 1; }
[[ "$(nm -gU "$PLUGIN")" == *_reflex_world_version* ]] || say "WARNING: the plugin has no reflex_world_version (built before the platform exports)"

# 2. clone
C="$CLONE/Reflex"
if [ ! -d "$C" ]; then
	say "creating the build clone $C"
	mkdir -p "$CLONE"
	cp -cR "$PROJ" "$C" 2>/dev/null || cp -R "$PROJ" "$C"
	rm -rf "$C/Temp" "$C/Logs"
fi
rsync -a --delete --exclude 'Plugins/macOS/libreflexworld.dylib' "$PROJ/Assets/" "$C/Assets/"
rsync -a "$PROJ/ProjectSettings/" "$C/ProjectSettings/"
[ -n "${REFLEX_BUILD_PRODUCT:-}" ] && sed -i '' "s/^  productName: .*/  productName: $REFLEX_BUILD_PRODUCT/" "$C/ProjectSettings/ProjectSettings.asset"
rsync -a "$PROJ/Packages/" "$C/Packages/"
cp "$PLUGIN" "$C/Assets/Plugins/macOS/.libreflexworld.new" && mv -f "$C/Assets/Plugins/macOS/.libreflexworld.new" "$C/Assets/Plugins/macOS/libreflexworld.dylib"
cp "$W/lib/libcapstone.5.dylib" "$C/Assets/Plugins/macOS/libcapstone.5.dylib" 2>/dev/null || true
[ -d "$C/Assets/StreamingAssets" ] && { say "Assets/StreamingAssets exists: no game data may ship (ReflexBuild refuses it)"; }

# 3. player
PRODUCT="$(sed -n 's/^  productName: //p' "$C/ProjectSettings/ProjectSettings.asset")"
STAGE="$SCRATCH/stage/$PRODUCT.app"
OUT="$BUILDS/macOS/$PRODUCT.app"
say "building the player ($PRODUCT${DEV:+, development})"
rm -rf "$(dirname "$STAGE")"; mkdir -p "$(dirname "$STAGE")"
REFLEX_REPO="$ROOT" "$UNITY" -batchmode -quit -projectPath "$C" -executeMethod Reflex.Port.EditorTools.ReflexBuild.BuildMacBatch \
	-reflexBuildOut "$STAGE" -reflexSymbolsDir "$BUILDS/symbols" $DEV -logFile "$LOG" >/dev/null 2>&1 || true
grep -q "\[Reflex\] build Succeeded" "$LOG" || { grep -E "error CS|\[Reflex\] build" "$LOG" | tail -20; say "BUILD FAILED (log: $LOG)"; exit 1; }
grep -E "\[Reflex\] build Succeeded" "$LOG" | sed 's/^/  /'
EXE="$STAGE/Contents/MacOS/$PRODUCT"

# 4. smoke
if [ $SMOKE = 1 ]; then
	SL="$(mktemp -d "$SCRATCH/smoke.XXXXXX")"
	MODE=(-batchmode); [ $WINDOWED = 1 ] && MODE=(-screen-fullscreen 0 -screen-width 1280 -screen-height 720)
	# a subshell, so the shell's own "Segmentation fault" notice of the crash test goes to /dev/null too
	run() { ( env REFLEX_GAME_DIR="$GAME" perl -e 'alarm shift; exec @ARGV' "$1" "$EXE" "${MODE[@]}" -reflexLogDir "$SL/logs" "${@:2}" || true ) >/dev/null 2>&1; }
	say "smoke: front end + race (30 s)"
	run 600 -reflexSmoke 30 -logFile "$SL/smoke.log"
	grep -q "\[Reflex\] smoke OK" "$SL/smoke.log" || { grep -E "\[Reflex\] smoke|Exception" "$SL/smoke.log" | head -5; say "SMOKE TEST FAILED (log: $SL/smoke.log)"; exit 1; }
	grep -o "\[Reflex\] smoke OK.*" "$SL/smoke.log" | cut -c1-240 | sed 's/^/  /'
	n=$(cat "$SL"/logs/session-*.log | grep -c "] perf t=" || true)
	[ "$n" -ge 2 ] || { say "SMOKE TEST FAILED: $n perf lines in the session log"; exit 1; }
	say "smoke: forced native crash"
	run 120 -reflexCrashTest native -logFile "$SL/crash.log"
	ls "$SL"/logs/crash-*.txt >/dev/null 2>&1 || { say "CRASH TEST FAILED: no crash-*.txt (log: $SL/crash.log)"; exit 1; }
	grep -m1 "^report:" "$SL"/logs/crash-*.txt | sed 's/^/  /'
	say "smoke: support report after the crash"
	run 120 -reflexCreateReport "$SL/report" -logFile "$SL/report.log"
	Z=$(ls "$SL"/report/Reflex-Report-*.zip 2>/dev/null | head -1)
	[ -n "$Z" ] || { say "REPORT TEST FAILED: no ZIP (log: $SL/report.log)"; exit 1; }
	LIST="$(unzip -l "$Z")"; INFO="$(unzip -p "$Z" system-info.txt)"; ALL="$(unzip -p "$Z" -x '*.dmp' 2>/dev/null)"   # whole outputs: no grep -q on a pipe (pipefail)
	[[ "$LIST" == *"crashes/crash-"* ]] || { say "REPORT TEST FAILED: the crash report is not in the ZIP ($Z)"; exit 1; }
	[[ "$INFO" == *"ended unexpectedly"* ]] || { say "REPORT TEST FAILED: the previous crash is not in system-info.txt"; exit 1; }
	[[ "$ALL" != *"$HOME/"* ]] || { say "REPORT TEST FAILED: the home folder is not redacted"; exit 1; }
	say "  report ok: $(echo "$LIST" | tail -1 | awk '{print $2}') files ($SL)"
fi

# 5. swap the tested player in
mkdir -p "$(dirname "$OUT")"
rm -rf "$OUT.old"; [ -d "$OUT" ] && mv "$OUT" "$OUT.old"
mv "$STAGE" "$OUT"; rm -rf "$OUT.old"
rm -rf "$(dirname "$STAGE")/${PRODUCT}_BackUpThisFolder_ButDontShipItWithYourGame"
rm -f "$BUILDS/macOS/"Built\ *.txt
LABEL="${LABEL//\//-}"; LABEL="${LABEL//:/-}"
: > "$BUILDS/macOS/Built $(date '+%Y-%m-%d %H.%M')${LABEL:+ - $LABEL}.txt"
say "done (local only, nothing published): $OUT"
