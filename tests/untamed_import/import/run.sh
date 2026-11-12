#!/bin/sh
# tests/untamed_import/import/run.sh <scratch dir> [quick|all|both] [--threads N]
# Builds (build.sh) and runs the MX vs ATV Untamed import test (Program.cs) outside Unity: the importer converts stadiums of the player's
# disc into a scratch Mods/Tracks and the test checks what it wrote, what it left and what it refused. See Program.cs for each check.
#   quick  (default) Norton and Lincoln: about two minutes       all  the 12 stadiums once: about three minutes, 600 MB
# Inputs (environment; a missing one makes the result INCOMPLETE):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image
#   UX_REFLEX       an original MX vs ATV Reflex install, or UX_CLIENT_LIB the Open Reflex client's "game files" folder
#   UX_DISC_FOLDER  (optional) the extracted disc: the differing-disc checks copy Lincoln's files from it into the scratch folder
# Everything is written below <scratch>; the disc and the library are only read. Exit 0: every check ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd); S=${1:?scratch dir}; shift
sh "$HERE/build.sh" "$S" > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
export UX_BUILD_EXPECTED=${UX_BUILD_EXPECTED:-$ROOT/tests/untamed_convert/build/expected_sha256.txt}
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/import.dll" "$@" --scratch "$S"
