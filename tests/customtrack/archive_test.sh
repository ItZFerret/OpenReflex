#!/bin/sh
# archive_test.sh <scratch dir> [real archive] -- the custom tracks' archive installs (Unity/Reflex/Assets/Scripts/Content/TrackArchives.cs,
# w-fb-customtrack) outside Unity: builds TrackArchives.cs + ArchiveTest.cs with Unity 6000.6.1f1's bundled csc against .NET 8 and runs it
# with the bundled dotnet. Every archive is made in the scratch dir (zip by the test, 7z by /usr/bin/tar); the optional real archive (e.g. the
# community's "573-2014-monster-energy-cup.zip", a RAR 4 named .zip) is only read and copied into the scratch dir. Exit 0: ARCHIVES PASS.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); S=${1:?scratch dir}; REAL=${2:-}
D=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk
CSC=$(ls "$D"/sdk/*/Roslyn/bincore/csc.dll | head -1)
REF=$(ls -d "$D"/packs/Microsoft.NETCore.App.Ref/8.*/ref/net8.0 | head -1)
FW=$(ls "$D"/shared/Microsoft.NETCore.App | head -1)
mkdir -p "$S/bin"
"$D/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:exe -langversion:9 -nowarn:1701,1702 $(for f in "$REF"/*.dll; do printf -- '-r:%s ' "$f"; done) \
  -out:"$S/bin/ArchiveTest.dll" "$ROOT/Unity/Reflex/Assets/Scripts/Content/TrackArchives.cs" "$HERE/ArchiveTest.cs"
printf '{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"%s"}}}\n' "$FW" > "$S/bin/ArchiveTest.runtimeconfig.json"
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$D/dotnet" "$S/bin/ArchiveTest.dll" "$S/work" "$REAL"
