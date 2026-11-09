#!/bin/sh
# The music shuffle bags' tests outside Unity (w-fb-hud; Unity 6000.6.1f1's bundled csc + dotnet, as tests/camshare/codec_test.sh):
# Unity/Reflex/Assets/Scripts/Content/ReflexShuffleBag.cs (no UnityEngine) with BagTests.cs: whole rounds, no entry again before every entry
# played, never the last one first in a new round, a level's playlist drawn from the shared round, menus and races kept apart, seeds.
#   tests/music_bag/bag_test.sh <outdir>
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); OUT=${1:?outdir}
U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting
CSC=$(ls "$U"/DotNetSdk/sdk/*/Roslyn/bincore/csc.dll | head -1); RT=$(ls -d "$U"/NetCoreRuntime/shared/Microsoft.NETCore.App/* | head -1)
mkdir -p "$OUT"
REFS=$(for f in "$RT"/System*.dll "$RT"/mscorlib.dll "$RT"/netstandard.dll "$RT"/Microsoft.Win32.Primitives.dll; do echo "-r:$f"; done)
# shellcheck disable=SC2086
"$U/DotNetSdk/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 -warnaserror+ -nowarn:1591 $REFS \
  -out:"$OUT/music_bag_tests.dll" "$HERE/BagTests.cs" "$ROOT/Unity/Reflex/Assets/Scripts/Content/ReflexShuffleBag.cs"
cat > "$OUT/music_bag_tests.runtimeconfig.json" <<J
{ "runtimeOptions": { "tfm": "net8.0", "framework": { "name": "Microsoft.NETCore.App", "version": "$(basename "$RT")" } } }
J
exec "$U/NetCoreRuntime/dotnet" exec "$OUT/music_bag_tests.dll"
