#!/bin/sh
# tests/alive_bikes/run.sh <scratch dir> [--out DIR] [--bikes 41,36|all] SOURCE... -- builds and runs the Alive bike importer outside Unity:
# lists every Alive motorcycle of the sources (what it rides as, or why not), and converts the chosen ones into DIR/<slug> .rxmod folders.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; shift
D=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk
"$HERE/build.sh" "$S" > /dev/null
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$D/dotnet" "$S/bin/AliveBikes.dll" "$@"
