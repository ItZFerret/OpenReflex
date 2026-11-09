#!/bin/sh
# tests/alive_bikes/build.sh <scratch dir> -- builds the MX vs ATV Alive bike importer (Content/AliveImport/*.cs: AliveBikes.cs,
# AliveBikeMaps.cs + the level importer's readers) with RfxData.cs, outside Unity: Unity 6000.6.1f1's bundled csc against .NET 8
# (UnityEngine.CoreModule referenced for the vector / matrix types only; nothing native is called), as tests/importflow/build.sh does.
# Output: <scratch>/bin/AliveBikes.dll; run it with tests/alive_bikes/run.sh.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); S=${1:?scratch dir}
U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting; D=$U/DotNetSdk
CSC=$(ls "$D"/sdk/*/Roslyn/bincore/csc.dll | head -1)
REF=$(ls -d "$D"/packs/Microsoft.NETCore.App.Ref/8.*/ref/net8.0 | head -1)
FW=$(ls "$D"/shared/Microsoft.NETCore.App | head -1)
C="$ROOT/Unity/Reflex/Assets/Scripts/Content"
mkdir -p "$S/bin"
"$D/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 -unsafe+ -optimize+ -nowarn:1701,1702,0618,0649,0169,0414 $(for f in "$REF"/*.dll; do printf -- '-r:%s ' "$f"; done) \
  -r:"$U/Managed/UnityEngine/UnityEngine.CoreModule.dll" -r:"$U/Managed/UnityEngine/UnityEngine.ScriptingModule.dll" -out:"$S/bin/AliveBikes.dll" \
  "$C/ReflexAssetImport.cs" "$C/ReflexAssetImport.Space.cs" "$C/ImportModScan.cs" "$C/ReflexStockDatabase.cs" "$C/RfxData.cs" "$C/Bxml.cs" "$C/LzxDecoder.cs" \
  "$C/FresnoBRepair.cs" "$C"/AliveImport/*.cs "$C/RxFiles.cs" "$C/ReflexDataPack.cs" "$C/../OpenReflex/RxNativeLoader.cs" "$C/../OpenReflex/RxArchiveNative.cs" \
  "$C/ReflexPaths.cs" "$C/ReflexDrives.cs" "$C/ReflexMods.cs" "$ROOT/tests/importflow/Stubs.cs" "$HERE/Program.cs"
cp "$U/Managed/UnityEngine/UnityEngine.CoreModule.dll" "$S/bin/"
printf '{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"%s"}}}\n' "$FW" > "$S/bin/AliveBikes.runtimeconfig.json"
echo "$S/bin/AliveBikes.dll"
