#!/bin/sh
# tests/spawn_marker_buttons/run.sh <out dir>: compiles ButtonsTest.cs + Assets/Scripts/OriginalWorld/SpawnMarkerButtons.cs with Unity 6000.6.1f1's bundled
# csc (plain C#, no Unity) and runs it with the bundled dotnet (headless, no window). Exit 0 = PASS.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); OUT=${1:?out dir}
U=${UNITY_SCRIPTING:-/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting}
CSC=$(ls "$U"/DotNetSdk/sdk/*/Roslyn/bincore/csc.dll | head -1)
FX=$(ls -d "$U"/DotNetSdk/shared/Microsoft.NETCore.App/* | head -1)
mkdir -p "$OUT"
refs=""; for f in "$FX"/System.Runtime.dll "$FX"/System.Console.dll "$FX"/System.Collections.dll "$FX"/System.Private.CoreLib.dll; do refs="$refs -r:$f"; done
"$U/DotNetSdk/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 $refs -out:"$OUT/ButtonsTest.dll" \
  "$HERE/ButtonsTest.cs" "$ROOT/Unity/Reflex/Assets/Scripts/OriginalWorld/SpawnMarkerButtons.cs"
printf '{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"%s"}}}\n' "$(basename "$FX")" > "$OUT/ButtonsTest.runtimeconfig.json"
"$U/DotNetSdk/dotnet" exec --runtimeconfig "$OUT/ButtonsTest.runtimeconfig.json" "$OUT/ButtonsTest.dll"
