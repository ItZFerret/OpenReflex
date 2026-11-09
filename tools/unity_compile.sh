#!/bin/sh
# Compiles the player's script assemblies outside the editor with Unity 6000.6.1f1's bundled csc, as a player build would (asmdefs first):
# Reflex.OpenReflex, then Assembly-CSharp (Assets without Editor / Tests and the asmdef folders). The package assemblies come from an
# imported staged project (LIB: <stage>/Library/ScriptAssemblies).
#   LIB=... tools/unity_compile.sh <outdir> [osx|win]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/.." && pwd); OUT=${1:?outdir}; TARGET=${2:-osx}
U=${UNITY_SCRIPTING:-/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting}
LIB=${LIB:-$ROOT/Unity/Reflex/Library/ScriptAssemblies}
CSC=$(ls "$U"/DotNetSdk/sdk/*/Roslyn/bincore/csc.dll | head -1)
mkdir -p "$OUT"
A="$ROOT/Unity/Reflex/Assets"
PLAT="UNITY_STANDALONE_OSX"; [ "$TARGET" = win ] && PLAT="UNITY_STANDALONE_WIN;UNITY_64"
DEFS="-define:UNITY_5_3_OR_NEWER;UNITY_2021_3_OR_NEWER;UNITY_2022_3_OR_NEWER;UNITY_2023_1_OR_NEWER;UNITY_6000_0_OR_NEWER;UNITY_STANDALONE;$PLAT;ENABLE_INPUT_SYSTEM"
{
  for f in "$U"/Managed/UnityEngine/*.dll; do echo "-r:$f"; done
  for f in "$LIB"/*.dll; do case $f in *Assembly-CSharp*|*Editor*|*Tests*|*Reflex.*|*Epic.OnlineServices*) ;; *) echo "-r:$f";; esac; done
  echo "-r:$U/NetStandard/ref/2.1.0/netstandard.dll"
} > "$OUT/refs.rsp"
csc() { "$U/DotNetSdk/dotnet" "$CSC" -nologo -noconfig -nostdlib+ -target:library -langversion:9 -unsafe+ -nowarn:0618,0414,0649,0169,0219,0162,0168,1591 "$DEFS" @"$OUT/refs.rsp" "$@"; }
n=0
step() { name=$1; shift; if csc "$@" > "$OUT/$name.log" 2>&1; then echo "$name: 0 errors"; else n=$((n+1)); echo "$name: FAILED"; grep -E "error CS" "$OUT/$name.log" | sort -u | head -${ERRS:-40}; fi; }
find "$A/Scripts/OpenReflex" -name "*.cs" > "$OUT/openreflex.rsp"
step Reflex.OpenReflex -out:"$OUT/Reflex.OpenReflex.dll" @"$OUT/openreflex.rsp"
(cd "$ROOT/Unity/Reflex" && find Assets -name "*.cs" -not -path "*/Editor/*" -not -path "Assets/Scripts/OpenReflex/*" -not -path "Assets/Tests/*" | sed "s|^|$ROOT/Unity/Reflex/|") > "$OUT/csharp.rsp"
step Assembly-CSharp -out:"$OUT/Assembly-CSharp.dll" -r:"$OUT/Reflex.OpenReflex.dll" @"$OUT/csharp.rsp"
echo "$TARGET: $n assemblies failed"
exit $n
