#!/bin/zsh
set -eu
TASK_ROOT="${0:A:h}/../../builds/experimental"
TASK_APP="$TASK_ROOT/Reflex-MTX-Experimental.app"
TASK_BIN=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$TASK_APP/Contents/Info.plist")
export REFLEX_GAME_DIR='/Users/payton/Documents/Development/REFLEX/game files/Archive'
export RX_BACKTRACE=1
exec "$TASK_APP/Contents/MacOS/$TASK_BIN" -reflexClientRoot "$TASK_ROOT" -reflexSaveDir "$TASK_ROOT/Save" -reflexMtxExperiment -reflexMtxRace -reflexMtxTrack "${REFLEX_MTX_TRACK:-McKennaRock}" -logFile "$TASK_ROOT/player.log"
