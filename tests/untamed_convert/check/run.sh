#!/bin/sh
# tests/untamed_convert/check/run.sh <scratch dir> [lzx|half|build|variants|determinism|cancel|all] [--levels norton,lincoln] [--core-blobs]
#                                    [--write-expected <file>]
# Builds the "check" part (../build.sh) and runs it headless: the MX vs ATV Untamed converter end to end from the player's disc and a Reflex
# library, every file it writes compared with expected.txt beside this script (SHA-256 only, no game bytes). See Program.cs for each mode.
# Inputs (environment; without the disc or the library only lzx and half run, and the result is INCOMPLETE):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image          UX_DISC_FOLDER  the extracted disc (the folder holding Database and AI)
#   UX_REFLEX       an original MX vs ATV Reflex install      UX_CLIENT_LIB   the Open Reflex client's "game files" folder
# Everything is written below <scratch> (about 2 GB for the 12 stadiums). Exit 0: every check ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; shift
sh "$HERE/../build.sh" "$S" check > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
export UX_CHECK_EXPECTED=${UX_CHECK_EXPECTED:-$HERE/expected.txt}
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/check.dll" "$@" --scratch "$S"
