#!/bin/sh
# tests/untamed_import/import/build.sh <scratch dir> -- builds the MX vs ATV Untamed import test outside Unity, against .NET 8, the way
# tests/untamed_convert/build.sh builds a part of the converter (same compiler, flags and language version 9, no unsafe code):
#   the converter (tests/untamed_convert/foundation.txt + build/sources.txt) + the importer (sources.txt)
#   + tests/untamed_convert/UxTest.cs + Program.cs  ->  <scratch>/bin/import.dll with its runtimeconfig
# Prints the dll's path.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd); S=${1:?scratch dir}
CONV="$ROOT/tests/untamed_convert"
FLAGS="-nologo -nostdlib+ -target:exe -langversion:9 -optimize+ -nowarn:1701,1702,0618,0649,0169,0414"
mkdir -p "$S/bin"
LIST="$S/import.sources"
: > "$LIST"
for l in "$CONV/foundation.txt" "$CONV/build/sources.txt" "$HERE/sources.txt"; do
  tr -d '\r' < "$l" | while IFS= read -r line; do
    case "$line" in ''|'#'*) continue ;; esac
    [ -f "$ROOT/$line" ] || { echo "$l: no such file: $line" >&2; exit 2; }
    printf '%s\n' "$ROOT/$line" >> "$LIST"
  done
done
printf '%s\n' "$CONV/UxTest.cs" "$HERE/Program.cs" >> "$LIST"
sort -u "$LIST" -o "$LIST"
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*)
  CSC=${RX_CSC:-"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/Roslyn/csc.exe"}
  R=${RX_DOTNET_SHARED:-$(ls -d "C:/Program Files/dotnet/shared/Microsoft.NETCore.App"/8.* | tail -1)}
  FW=$(basename "$R")
  { echo "$FLAGS"
    for f in "$R"/*.dll; do
      case "$(basename "$f")" in *Native*) ;; System.*|Microsoft.CSharp.dll|Microsoft.VisualBasic*.dll|Microsoft.Win32.*|mscorlib.dll|netstandard.dll|WindowsBase.dll) echo "-r:\"$(cygpath -w "$f")\"";; esac
    done
    echo "-out:\"$(cygpath -w "$S/bin/import.dll")\""
    while IFS= read -r f; do echo "\"$(cygpath -w "$f")\""; done < "$LIST"
  } > "$S/import.rsp"
  "$CSC" -noconfig "@$(cygpath -w "$S/import.rsp")"
  ;;
*)
  U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting; D=$U/DotNetSdk
  CSC=$(ls "$D"/sdk/*/Roslyn/bincore/csc.dll | head -1)
  REF=$(ls -d "$D"/packs/Microsoft.NETCore.App.Ref/8.*/ref/net8.0 | head -1)
  FW=$(ls "$D"/shared/Microsoft.NETCore.App | head -1)
  { echo "$FLAGS"
    for f in "$REF"/*.dll; do echo "-r:\"$f\""; done
    echo "-out:\"$S/bin/import.dll\""
    while IFS= read -r f; do echo "\"$f\""; done < "$LIST"
  } > "$S/import.rsp"
  "$D/dotnet" "$CSC" -noconfig "@$S/import.rsp"
  ;;
esac
printf '{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"%s"},"configProperties":{"System.GC.Server":false,"System.GC.Concurrent":false}}}\n' "$FW" > "$S/bin/import.runtimeconfig.json"
echo "$S/bin/import.dll"
