#!/bin/sh
# Compiles the engine-logic harness with Unity 6000.6.1f1's bundled csc and writes a runnable folder: build.sh <outdir>
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../../.." && pwd); OUT=${1:?outdir}
U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting
CSC=$(ls "$U"/DotNetSdk/sdk/*/Roslyn/bincore/csc.dll | head -1); RT=$(ls -d "$U"/NetCoreRuntime/shared/Microsoft.NETCore.App/* | head -1)
mkdir -p "$OUT"
S=$ROOT/Unity/Reflex/Assets/Scripts
"$U/DotNetSdk/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 -unsafe+ -nowarn:0618,0414,0649,0169,0219,0162,0168 \
  $(for f in "$RT"/System*.dll "$RT"/mscorlib.dll "$RT"/netstandard.dll; do echo "-r:$f"; done) \
  -out:"$OUT/engine_logic.dll" "$HERE/Stubs.cs" "$HERE/Program.cs" "$S/OriginalWorld/OriginalEngineAudio.cs" "$S/OpenReflex/RxVehicleAudio.cs" "$S/OpenReflex/RxNativeLoader.cs"
cat > "$OUT/engine_logic.runtimeconfig.json" <<J
{ "runtimeOptions": { "tfm": "net8.0", "framework": { "name": "Microsoft.NETCore.App", "version": "$(basename "$RT")" } } }
J
echo "run: $U/NetCoreRuntime/dotnet exec $OUT/engine_logic.dll <trace.csv> <events.csv> <states.csv> <MXRegistry dir>"
