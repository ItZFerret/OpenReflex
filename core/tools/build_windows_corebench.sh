#!/bin/bash
# build_windows_corebench.sh <rxcore build dir (core/tools/build_windows.sh output)> [out dir]
# The Windows core benchmark (core/tools/corebench.c; w-winbench): corebench.exe next to a copy of that rxcore.dll, plus
# rxcore.syms / corebench.syms for its in-process sampling profiler. Test tool only: never shipped, never linked into rxcore.dll.
# Run it on Windows, or here under CrossOver (headless, no window):
#   wine --bottle <bottle> 'Z:\<out dir>\corebench.exe' 'Z:\<repo>\game files' concord 900 'Z:\<out dir>\result.txt'
# (COREBENCH_SAMPLE_MS=20 under Wine, where suspending a thread is slow; COREBENCH_SAMPLE=0 = timing only.)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DLL_DIR="$(cd "$1" && pwd)"; OUT="${2:-$DLL_DIR/corebench}"; mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
CC=x86_64-w64-mingw32-gcc; NM=x86_64-w64-mingw32-nm; DUMP=x86_64-w64-mingw32-objdump
cp "$DLL_DIR/rxcore.dll" "$OUT/rxcore.dll"
# An older build_windows.sh linked the DLL as rxcore.dll.tmp, so its export directory (and an import of it) named rxcore.dll.tmp;
# a copy under that name satisfies such an import (a current DLL names rxcore.dll).
if grep -q 'rxcore\.dll\.tmp' <("$DUMP" -p "$DLL_DIR/rxcore.dll" | grep '^Name '); then cp "$DLL_DIR/rxcore.dll" "$OUT/rxcore.dll.tmp"; fi
"$CC" -std=c11 -O2 -g -Wall -Wextra -Wno-format-truncation -I"$ROOT/include" -DBENCH_VARIANT="\"$(basename "$DLL_DIR")\"" \
  "$ROOT/tools/corebench.c" "$OUT/rxcore.dll" -lpsapi -static -o "$OUT/corebench.exe"
syms(){   # "<rva> <name>" per text symbol, demangled, argument lists dropped
  python3 - "$1" "$2" "$NM" "$DUMP" <<'PY'
import subprocess,sys
img,out,nm,dump=sys.argv[1:5]
base=next(int(l.split()[1],16) for l in subprocess.run([dump,'-p',img],capture_output=True,text=True).stdout.split('\n') if l.startswith('ImageBase'))
rows={}
for l in subprocess.run([nm,'-C','--defined-only',img],capture_output=True,text=True).stdout.split('\n'):
    p=l.split(' ',2)
    if len(p)<3 or p[1] not in 'tT' or p[2].startswith('.'): continue
    n=p[2].replace('(anonymous namespace)::','')
    if not n.startswith('('): n=n.split('(')[0]
    n=n.replace(' ','_'); a=int(p[0],16)-base
    if a not in rows or (rows[a].startswith('_') and not n.startswith('_')): rows[a]=n
open(out,'w').write(''.join('%x %s\n'%(a,rows[a]) for a in sorted(rows)))
PY
}
# a stripped DLL keeps its symbols in rxcore.dbg (.gnu_debuglink; same image base)
if [ -f "$DLL_DIR/rxcore.dbg" ]; then syms "$DLL_DIR/rxcore.dbg" "$OUT/rxcore.syms"; else syms "$OUT/rxcore.dll" "$OUT/rxcore.syms"; fi
syms "$OUT/corebench.exe" "$OUT/corebench.syms"
echo "built $OUT/corebench.exe (+ rxcore.dll, rxcore.syms, corebench.syms)"
