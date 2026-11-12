#!/bin/sh
# tests/untamed_convert/build.sh <scratch dir> <part> -- builds one part of the MX vs ATV Untamed converter port outside Unity, against .NET 8:
#   the foundation sources (foundation.txt: Content/UntamedImport/Ux*.cs of the foundation + the Alive importer's readers they use)
#   + the part's own sources (<part>/sources.txt, repo-relative paths, one per line, '#' comments; may be missing or empty)
#   + the shared test helper (UxTest.cs) + <part>/Program.cs
# into <scratch>/bin/<name>.dll with its runtimeconfig (<name>: the part folder's last component). The part in the repo is "check" (the
# converter end to end against check/expected.txt); a local, git-ignored reference/ folder may hold more parts ("reference/<part>").
#   Windows  (Git Bash) a Roslyn csc.exe (Visual Studio Build Tools; RX_CSC overrides the path) against the installed .NET 8 runtime's
#            assemblies (RX_DOTNET_SHARED overrides the folder: ...\dotnet\shared\Microsoft.NETCore.App\8.x)
#   macOS    Unity 6000.6.1f1's bundled csc and dotnet, as tests/untamed_import/build.sh does
# The language version is 9 and unsafe code is off: what compiles here compiles in Unity 6. Prints the dll's path.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd); S=${1:?scratch dir}; PART=${2:?part (a folder of tests/untamed_convert)}
NAME=$(basename "$PART")
[ -f "$HERE/$PART/Program.cs" ] || { echo "no $HERE/$PART/Program.cs" >&2; exit 2; }
FLAGS="-nologo -nostdlib+ -target:exe -langversion:9 -optimize+ -nowarn:1701,1702,0618,0649,0169,0414"
mkdir -p "$S/bin"
LIST="$S/$NAME.sources"
: > "$LIST"
for l in "$HERE/foundation.txt" "$HERE/$PART/sources.txt"; do
  [ -f "$l" ] || continue
  tr -d '\r' < "$l" | while IFS= read -r line; do
    case "$line" in ''|'#'*) continue ;; esac
    [ -f "$ROOT/$line" ] || { echo "$l: no such file: $line" >&2; exit 2; }
    printf '%s\n' "$ROOT/$line" >> "$LIST"
  done
done
printf '%s\n' "$HERE/UxTest.cs" "$HERE/$PART/Program.cs" >> "$LIST"
case "$PART" in reference/*) [ -f "$HERE/reference/UxTestReference.cs" ] && printf '%s\n' "$HERE/reference/UxTestReference.cs" >> "$LIST" ;; esac
sort -u "$LIST" -o "$LIST"
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*)
  CSC=${RX_CSC:-"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/Roslyn/csc.exe"}
  R=${RX_DOTNET_SHARED:-$(ls -d "C:/Program Files/dotnet/shared/Microsoft.NETCore.App"/8.* | tail -1)}
  FW=$(basename "$R")
  # a response file: the paths hold spaces ("Program Files", a checkout folder)
  { echo "$FLAGS"
    for f in "$R"/*.dll; do
      case "$(basename "$f")" in *Native*) ;; System.*|Microsoft.CSharp.dll|Microsoft.VisualBasic*.dll|Microsoft.Win32.*|mscorlib.dll|netstandard.dll|WindowsBase.dll) echo "-r:\"$(cygpath -w "$f")\"";; esac
    done
    echo "-out:\"$(cygpath -w "$S/bin/$NAME.dll")\""
    while IFS= read -r f; do echo "\"$(cygpath -w "$f")\""; done < "$LIST"
  } > "$S/$NAME.rsp"
  "$CSC" -noconfig "@$(cygpath -w "$S/$NAME.rsp")"
  ;;
*)
  U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting; D=$U/DotNetSdk
  CSC=$(ls "$D"/sdk/*/Roslyn/bincore/csc.dll | head -1)
  REF=$(ls -d "$D"/packs/Microsoft.NETCore.App.Ref/8.*/ref/net8.0 | head -1)
  FW=$(ls "$D"/shared/Microsoft.NETCore.App | head -1)
  { echo "$FLAGS"
    for f in "$REF"/*.dll; do echo "-r:\"$f\""; done
    echo "-out:\"$S/bin/$NAME.dll\""
    while IFS= read -r f; do echo "\"$f\""; done < "$LIST"
  } > "$S/$NAME.rsp"
  "$D/dotnet" "$CSC" -noconfig "@$S/$NAME.rsp"
  ;;
esac
printf '{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"%s"},"configProperties":{"System.GC.Server":false,"System.GC.Concurrent":false}}}\n' "$FW" > "$S/bin/$NAME.runtimeconfig.json"
echo "$S/bin/$NAME.dll"
