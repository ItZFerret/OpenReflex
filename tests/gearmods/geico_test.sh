#!/bin/sh
# geico_test.sh <scratch dir> <Database dir> [extracted] -- the built-in GEICO kit pack (KitPacks.Geico.cs) outside Unity: builds the pack
# readers (as table_test.sh does) + GeicoTest.cs with Unity 6000.6.1f1's bundled csc against .NET 8 and runs it on the install's Database
# folder (read only). "extracted": the folder is an imported copy (reflex-extracted-v1.txt next to it). Exit 0: GEICO PASS.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); S=${1:?scratch dir}; DB=${2:?Database dir}; shift 2
U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting; D=$U/DotNetSdk
CSC=$(ls "$D"/sdk/*/Roslyn/bincore/csc.dll | head -1)
REF=$(ls -d "$D"/packs/Microsoft.NETCore.App.Ref/8.*/ref/net8.0 | head -1)
FW=$(ls "$D"/shared/Microsoft.NETCore.App | head -1)
C="$ROOT/Unity/Reflex/Assets/Scripts/Content"
mkdir -p "$S/bin"
"$D/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 -nowarn:1701,1702,0618 $(for f in "$REF"/*.dll; do printf -- '-r:%s ' "$f"; done) \
  -r:"$U/Managed/UnityEngine/UnityEngine.CoreModule.dll" -r:"$U/Managed/UnityEngine/UnityEngine.ScriptingModule.dll" -out:"$S/bin/GeicoTest.dll" \
  "$C/KitPacks.cs" "$C/KitPacks.Geico.cs" "$C/RfxData.cs" "$C/Bxml.cs" "$C/LzxDecoder.cs" "$C/ReflexGameTables.cs" "$C/TrackArchives.cs" "$C/ReflexMods.cs" "$C/ReflexPaths.cs" \
  "$HERE/Stubs.cs" "$HERE/GeicoTest.cs"
cp "$U/Managed/UnityEngine/UnityEngine.CoreModule.dll" "$S/bin/"
printf '{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"%s"}}}\n' "$FW" > "$S/bin/GeicoTest.runtimeconfig.json"
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$D/dotnet" "$S/bin/GeicoTest.dll" "$DB" "$@"
