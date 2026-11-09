#!/bin/sh
# tests/rxarchive/build.sh <scratch dir> <core library> <loose library> <packed library> <mods folder> <packed mods folder>
# The client's archive readers (core/plan/RXARCHIVE.md) outside Unity: builds Content/RxFiles.cs, ReflexDataPack.cs, RfxData.cs (+ the import
# files it needs) and OpenReflex/RxNativeLoader.cs + RxArchiveNative.cs with Unity 6000.6.1f1's bundled csc against .NET 8 and runs
# ArchiveTests.cs with the bundled dotnet against a real librxcore:
#   A  library parity: every asset of a set of level and shared databases, the levels' .dx9.level / .dx9.scene and the Database listing read
#      through Rfx.Install / RxFiles from the loose library and from the packed one (Assets.rxdata + Tracks/*.rxtrack): byte for byte, timed
#   B  mod parity: every asset of every packed mod track (Mods/Tracks .rxtrack version 5) against its folder, LZX decoded, and the files'
#      SHA-256 through RxFiles.OpenRead
#   C  the client packer: a small library (two levels, an Alive one, shared assets) copied into <scratch>, packed by ReflexDataPack with the
#      loose copies removed, then every original file read back through RxFiles
# The libraries are only read. Exit 0: RXARCHIVE PASS.
#   tests/rxarchive/build.sh <scratch dir> packimport <core library> <packed library> <original install>
#      C and D alone (the packer, the import's pack and publish), with the archive handle checks (RxFiles.Release)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); S=${1:?scratch dir}; shift
U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting; D=$U/DotNetSdk
CSC=$(ls "$D"/sdk/*/Roslyn/bincore/csc.dll | head -1)
REF=$(ls -d "$D"/packs/Microsoft.NETCore.App.Ref/8.*/ref/net8.0 | head -1)
FW=$(ls "$D"/shared/Microsoft.NETCore.App | head -1)
C="$ROOT/Unity/Reflex/Assets/Scripts/Content"; O="$ROOT/Unity/Reflex/Assets/Scripts/OpenReflex"
mkdir -p "$S/bin"
"$D/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 -unsafe+ -nowarn:1701,1702,0618,0649,0169,0414 $(for f in "$REF"/*.dll; do printf -- '-r:%s ' "$f"; done) \
  -r:"$U/Managed/UnityEngine/UnityEngine.CoreModule.dll" -r:"$U/Managed/UnityEngine/UnityEngine.ScriptingModule.dll" -out:"$S/bin/ArchiveTests.dll" \
  "$C/ReflexAssetImport.cs" "$C/ReflexAssetImport.Space.cs" "$C/ImportModScan.cs" "$C/ReflexStockDatabase.cs" "$C/RfxData.cs" "$C/Bxml.cs" "$C/LzxDecoder.cs" \
  "$C/FresnoBRepair.cs" "$C"/AliveImport/*.cs "$C/RxFiles.cs" "$C/ReflexDataPack.cs" "$O/RxNativeLoader.cs" "$O/RxArchiveNative.cs" \
  "$C/ReflexPaths.cs" "$C/ReflexDrives.cs" "$C/ReflexMods.cs" "$HERE/Stubs.cs" "$HERE/ArchiveTests.cs"
cp "$U/Managed/UnityEngine/UnityEngine.CoreModule.dll" "$S/bin/"
printf '{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"%s"}}}\n' "$FW" > "$S/bin/ArchiveTests.runtimeconfig.json"
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$D/dotnet" "$S/bin/ArchiveTests.dll" "$S/work" "$@"
