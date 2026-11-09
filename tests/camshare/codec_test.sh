#!/bin/sh
# The camera share codec's tests outside Unity (w-camshare; Unity 6000.6.1f1's bundled csc + dotnet, as tests/leaderboards/client_test.sh):
# Unity/Reflex/Assets/Scripts/Options/CameraShareCodec.cs (no UnityEngine) with CameraShareCodecTests.cs: round trips, section versions and upgrades,
# unknown / missing keys, cleaning, garbage and oversized codes; the client's own FPV section (ReflexFpvShare.Params.cs, w-fpvrevert: version 3 and
# its upgrades of the version 1 / version 2 codes).
#   tests/camshare/codec_test.sh <outdir>
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); OUT=${1:?outdir}
U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting
CSC=$(ls "$U"/DotNetSdk/sdk/*/Roslyn/bincore/csc.dll | head -1); RT=$(ls -d "$U"/NetCoreRuntime/shared/Microsoft.NETCore.App/* | head -1)
mkdir -p "$OUT"
REFS=$(for f in "$RT"/System*.dll "$RT"/mscorlib.dll "$RT"/netstandard.dll "$RT"/Microsoft.Win32.Primitives.dll; do echo "-r:$f"; done)
# shellcheck disable=SC2086
"$U/DotNetSdk/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 -warnaserror+ -nowarn:1591 $REFS \
  -out:"$OUT/camshare_codec_tests.dll" "$HERE/CameraShareCodecTests.cs" "$ROOT/Unity/Reflex/Assets/Scripts/Options/CameraShareCodec.cs" \
  "$ROOT/Unity/Reflex/Assets/Scripts/Options/ReflexFpvShare.Params.cs"
cat > "$OUT/camshare_codec_tests.runtimeconfig.json" <<J
{ "runtimeOptions": { "tfm": "net8.0", "framework": { "name": "Microsoft.NETCore.App", "version": "$(basename "$RT")" } } }
J
exec "$U/NetCoreRuntime/dotnet" exec "$OUT/camshare_codec_tests.dll"
