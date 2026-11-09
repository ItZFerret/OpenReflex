#!/bin/sh
# Builds and runs the path tests outside Unity (Unity 6000.6.1f1's bundled csc + dotnet), like tests/online/build.sh: the pure path logic of
# the install / import / mods code (Content/ReflexPaths.cs, Content/ReflexDrives.cs: System only) and YOUR MUSIC's tags, scan and queue (Content/ReflexSongLibrary.cs, synthetic tags) fed Windows-style paths (any drive letter,
# UNC, mounted ISO / optical / USB drives, spaces and non-ASCII names) on any machine.
#   tests/paths/build.sh <outdir>      build + run (exit 0: all passed)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); OUT=${1:?outdir}
U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting
CSC=$(ls "$U"/DotNetSdk/sdk/*/Roslyn/bincore/csc.dll | head -1); RT=$(ls -d "$U"/NetCoreRuntime/shared/Microsoft.NETCore.App/* | head -1)
mkdir -p "$OUT"
C="$ROOT/Unity/Reflex/Assets/Scripts/Content"
REFS=$(for f in "$RT"/System*.dll "$RT"/mscorlib.dll "$RT"/netstandard.dll; do echo "-r:$f"; done)
# shellcheck disable=SC2086
"$U/DotNetSdk/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 -warnaserror+ -nowarn:1591 $REFS \
  -out:"$OUT/path_tests.dll" "$HERE/PathTests.cs" "$C/ReflexPaths.cs" "$C/ReflexDrives.cs" "$C/ReflexSongLibrary.cs"
cat > "$OUT/path_tests.runtimeconfig.json" <<J
{ "runtimeOptions": { "tfm": "net8.0", "framework": { "name": "Microsoft.NETCore.App", "version": "$(basename "$RT")" } } }
J
"$U/NetCoreRuntime/dotnet" exec "$OUT/path_tests.dll"
