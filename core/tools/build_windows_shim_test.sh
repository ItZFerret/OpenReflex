#!/bin/bash
# build_windows_shim_test.sh <out dir> -- windows_shim_test.exe + windows_shim_test_dll.dll: the pthread shim that rxcore.dll's
# emulated thread-locals use (src/win/rx_pthread_shim.c), linked like rxcore.dll (static runtime, no winpthreads). Run the exe
# on Windows with the DLL next to it; it prints PASS / FAIL per check. The build fails if winpthreads is linked into either.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=${1:?output directory required};mkdir -p "$OUT";OUT="$(cd "$OUT" && pwd)"
CC=x86_64-w64-mingw32-gcc
FLAGS=(-std=c11 -O2 -fdata-sections -Wall -Wextra -D_WIN32_WINNT=0x0602 -Isrc)
"$CC" "${FLAGS[@]}" -c src/win/rx_pthread_shim.c -o "$OUT/rx_pthread_shim.raw"
python3 tools/windows_core_sections.py "$OUT/rx_pthread_shim.raw" "$OUT/rx_pthread_shim.o"   # the sections rxcore.dll gets
rm -f "$OUT/rx_pthread_shim.raw"
"$CC" "${FLAGS[@]}" -c tools/windows_shim_test.c -o "$OUT/windows_shim_test.o"
"$CC" "${FLAGS[@]}" -c tools/windows_shim_test_dll.c -o "$OUT/windows_shim_test_dll.o"
"$CC" "$OUT/windows_shim_test.o" "$OUT/rx_pthread_shim.o" -static -static-libgcc -Wl,-Map,"$OUT/windows_shim_test.map" -o "$OUT/windows_shim_test.exe"
"$CC" -shared "$OUT/windows_shim_test_dll.o" "$OUT/rx_pthread_shim.o" -static -static-libgcc -Wl,-Map,"$OUT/windows_shim_test_dll.map" -o "$OUT/windows_shim_test_dll.dll"
for f in windows_shim_test windows_shim_test_dll;do
 if grep -E '(libwinpthread|libpthread)[^ ]*\.a\(' "$OUT/$f.map" | head -3 | grep .;then echo "winpthreads linked into $f" >&2;exit 1;fi
 grep -q 'libgcc_eh\.a(emutls\.o)' "$OUT/$f.map" || { echo "$f does not use emulated TLS (nothing to test)" >&2;exit 1; }
done
for f in windows_shim_test.exe windows_shim_test_dll.dll;do
 x86_64-w64-mingw32-objdump -p "$OUT/$f" | awk '/DLL Name:/{d=$3;next} /vma:.*Member-Name/{on=1;next} on&&NF==0{on=0} on&&NF>=4{print d, $4}' >"$OUT/$f.imports.txt"
 for name in SuspendThread GetThreadContext SetThreadContext ResumeThread OpenProcess DuplicateHandle SetSystemTime SetProcessAffinityMask GetProcessTimes;do
  if awk -v n="$name" '$2==n{found=1}END{exit !found}' "$OUT/$f.imports.txt";then echo "forbidden import $name in $f" >&2;exit 1;fi
 done
done
echo "built $OUT/windows_shim_test.exe and windows_shim_test_dll.dll (run the exe on Windows, the DLL next to it)"
