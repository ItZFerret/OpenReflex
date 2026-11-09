#!/bin/sh
# table_test.sh <scratch dir> <stock Database dir> <pack folder or archive>... -- the gear / kit pack readers (w-gearmods) outside Unity: builds
# Content/KitPacks.cs, RfxData.cs, Bxml.cs, LzxDecoder.cs, ReflexGameTables.cs, TrackArchives.cs, ReflexMods.cs, ReflexPaths.cs + KitPacksTest.cs
# with Unity 6000.6.1f1's bundled csc against .NET 8 (UnityEngine.CoreModule referenced for the vector types; nothing native is called) and runs
# it with the bundled dotnet. The packs are copied into <scratch>/work/Mods/Gear (the sources are only read). Exit 0: GEARMODS PASS.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); S=${1:?scratch dir}; DB=${2:?stock Database dir}; shift 2
U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting; D=$U/DotNetSdk
CSC=$(ls "$D"/sdk/*/Roslyn/bincore/csc.dll | head -1)
REF=$(ls -d "$D"/packs/Microsoft.NETCore.App.Ref/8.*/ref/net8.0 | head -1)
FW=$(ls "$D"/shared/Microsoft.NETCore.App | head -1)
C="$ROOT/Unity/Reflex/Assets/Scripts/Content"
mkdir -p "$S/bin"
"$D/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 -nowarn:1701,1702,0618 $(for f in "$REF"/*.dll; do printf -- '-r:%s ' "$f"; done) \
  -r:"$U/Managed/UnityEngine/UnityEngine.CoreModule.dll" -r:"$U/Managed/UnityEngine/UnityEngine.ScriptingModule.dll" -out:"$S/bin/KitPacksTest.dll" \
  "$C/KitPacks.cs" "$C/RfxData.cs" "$C/Bxml.cs" "$C/LzxDecoder.cs" "$C/ReflexGameTables.cs" "$C/TrackArchives.cs" "$C/ReflexMods.cs" "$C/ReflexPaths.cs" \
  "$HERE/Stubs.cs" "$HERE/KitPacksTest.cs"
cp "$U/Managed/UnityEngine/UnityEngine.CoreModule.dll" "$S/bin/"
printf '{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"%s"}}}\n' "$FW" > "$S/bin/KitPacksTest.runtimeconfig.json"
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$D/dotnet" "$S/bin/KitPacksTest.dll" "$S/work" "$DB" "$@"
