#!/bin/sh
# tests/importflow/build.sh <scratch dir> [stock install dir] [pack zip...] -- the game files import (w-importflow) outside Unity: builds
# Content/ReflexAssetImport.cs (+ .Space.cs), ImportModScan.cs, ReflexStockDatabase.cs, RfxData.cs, Bxml.cs, LzxDecoder.cs, FresnoBRepair.cs (+ the
# Alive readers it uses), ReflexPaths.cs, ReflexDrives.cs, ReflexMods.cs + ImportTests.cs with Unity 6000.6.1f1's bundled csc against .NET 8
# (UnityEngine.CoreModule referenced for the vector types only; nothing native is called) and runs it with the bundled dotnet.
# Without a stock install only the synthetic tests run. The install and the packs are only read: scratch copies are made in <scratch>.
# Env: IMPORTFLOW_FULL=1 also runs a full real import of the stock install into <scratch> (~20 GB written, removed afterwards).
# Exit 0: IMPORTFLOW PASS.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); S=${1:?scratch dir}; shift
U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting; D=$U/DotNetSdk
CSC=$(ls "$D"/sdk/*/Roslyn/bincore/csc.dll | head -1)
REF=$(ls -d "$D"/packs/Microsoft.NETCore.App.Ref/8.*/ref/net8.0 | head -1)
FW=$(ls "$D"/shared/Microsoft.NETCore.App | head -1)
C="$ROOT/Unity/Reflex/Assets/Scripts/Content"
mkdir -p "$S/bin"
"$D/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 -unsafe+ -nowarn:1701,1702,0618,0649,0169,0414 $(for f in "$REF"/*.dll; do printf -- '-r:%s ' "$f"; done) \
  -r:"$U/Managed/UnityEngine/UnityEngine.CoreModule.dll" -r:"$U/Managed/UnityEngine/UnityEngine.ScriptingModule.dll" -out:"$S/bin/ImportTests.dll" \
  "$C/ReflexAssetImport.cs" "$C/ReflexAssetImport.Space.cs" "$C/ImportModScan.cs" "$C/ReflexStockDatabase.cs" "$C/RfxData.cs" "$C/Bxml.cs" "$C/LzxDecoder.cs" \
  "$C/FresnoBRepair.cs" "$C"/AliveImport/*.cs "$C/RxFiles.cs" "$C/ReflexDataPack.cs" "$C/../OpenReflex/RxNativeLoader.cs" "$C/../OpenReflex/RxArchiveNative.cs" \
  "$C/ReflexPaths.cs" "$C/ReflexDrives.cs" "$C/ReflexMods.cs" "$HERE/Stubs.cs" "$HERE/ImportTests.cs"
cp "$U/Managed/UnityEngine/UnityEngine.CoreModule.dll" "$S/bin/"
printf '{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"%s"}}}\n' "$FW" > "$S/bin/ImportTests.runtimeconfig.json"
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$D/dotnet" "$S/bin/ImportTests.dll" "$S/work" "$@"
