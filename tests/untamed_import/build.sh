#!/bin/sh
# tests/untamed_import/build.sh <scratch dir> -- builds the MX vs ATV Untamed disc check (Content/UntamedImport/UntamedImporter.cs + the Alive
# importer's disc readers it uses) with Program.cs, outside Unity, against .NET 8:
#   macOS    Unity 6000.6.1f1's bundled csc and dotnet, as tests/alive_bikes/build.sh does
#   Windows  (Git Bash) a Roslyn csc.exe (Visual Studio Build Tools; RX_CSC overrides the path) against the installed .NET 8 runtime's
#            assemblies (RX_DOTNET_SHARED overrides the folder: ...\dotnet\shared\Microsoft.NETCore.App\8.x)
# Output: <scratch>/bin/UntamedImport.dll; run it with tests/untamed_import/run.sh.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); S=${1:?scratch dir}
C="$ROOT/Unity/Reflex/Assets/Scripts/Content"
FLAGS="-nologo -nostdlib+ -target:exe -langversion:9 -unsafe+ -optimize+ -nowarn:1701,1702,0618,0649,0169,0414"
mkdir -p "$S/bin"
set -- "$C/UntamedImport/UntamedImporter.cs" "$C/AliveImport/AliveContainers.cs" "$C/AliveImport/AliveImportCore.cs" "$C/LzxDecoder.cs" \
  "$C/ReflexPaths.cs" "$C/ReflexDrives.cs" "$HERE/Program.cs"
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*)
  CSC=${RX_CSC:-"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/Roslyn/csc.exe"}
  R=${RX_DOTNET_SHARED:-$(ls -d "C:/Program Files/dotnet/shared/Microsoft.NETCore.App"/8.* | tail -1)}
  FW=$(basename "$R")
  # a response file: the paths hold spaces ("Program Files")
  { echo "$FLAGS"
    for f in "$R"/*.dll; do
      case "$(basename "$f")" in *Native*) ;; System.*|Microsoft.CSharp.dll|Microsoft.VisualBasic*.dll|Microsoft.Win32.*|mscorlib.dll|netstandard.dll|WindowsBase.dll) echo "-r:\"$(cygpath -w "$f")\"";; esac
    done
    echo "-out:\"$(cygpath -w "$S/bin/UntamedImport.dll")\""
    for f in "$@"; do echo "\"$(cygpath -w "$f")\""; done
  } > "$S/build.rsp"
  "$CSC" -noconfig "@$(cygpath -w "$S/build.rsp")"
  ;;
*)
  U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting; D=$U/DotNetSdk
  CSC=$(ls "$D"/sdk/*/Roslyn/bincore/csc.dll | head -1)
  REF=$(ls -d "$D"/packs/Microsoft.NETCore.App.Ref/8.*/ref/net8.0 | head -1)
  FW=$(ls "$D"/shared/Microsoft.NETCore.App | head -1)
  # shellcheck disable=SC2046,SC2086
  "$D/dotnet" "$CSC" -noconfig $FLAGS $(for f in "$REF"/*.dll; do printf -- '-r:%s ' "$f"; done) -out:"$S/bin/UntamedImport.dll" "$@"
  ;;
esac
printf '{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"%s"}}}\n' "$FW" > "$S/bin/UntamedImport.runtimeconfig.json"
echo "$S/bin/UntamedImport.dll"
