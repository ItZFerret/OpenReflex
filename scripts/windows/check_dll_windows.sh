#!/bin/bash
# check_dll_windows.sh [<plugin dir>] -- static checks of build_plugin_windows.sh's reflexworld.dll (no Windows machine needed; it
# runs nothing of the DLL). Default <plugin dir>: REFLEX/builds/windows/plugin.
#   1. a PE32+ x86-64 DLL
#   2. imports: only KERNEL32 and the Universal CRT (api-ms-win-crt-*), i.e. no MinGW runtime DLL (libstdc++ / libgcc / winpthread)
#   3. exports = the macOS plugin's (native/drafts/world_runtime/libreflexworld.dylib; its lzxd_* leak of lzxd.c is ignored)
#   4. every entry point the C# code imports is exported (cs_imports.py over Assets/Scripts)
#   5. the 25 public structs (plugin_api.cpp's kStructSizes) have the same sizeof with MinGW x86-64 as with clang arm64 (what
#      ReflexWorldNative.Check() compares at run time); the struct offsets come from the same header, so a size match plus no
#      `long` / `wchar_t` / `bool` in reflex_world.h's structs means the same layout
#   6. wine: when installed, loads the DLL and calls reflex_world_version / reflex_world_sizeof (not installed here: reported)
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
D="${1:-$ROOT/builds/windows/plugin}"; DLL="$D/reflexworld.dll"
W="$ROOT/native/drafts/world_runtime"; MAC="$W/libreflexworld.dylib"
OBJDUMP=x86_64-w64-mingw32-objdump; fail=0
ok() { echo "  ok    $*"; }; bad() { echo "  FAIL  $*"; fail=1; }
[ -f "$DLL" ] || { echo "no $DLL"; exit 2; }
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
echo "check $DLL"

f="$($OBJDUMP -f "$DLL")"
[[ "$f" == *"pei-x86-64"* && "$f" == *"architecture: i386:x86-64"* ]] && ok "PE32+ x86-64" || bad "not a PE32+ x86-64 image: $f"
P="$($OBJDUMP -p "$DLL")"   # whole outputs: no grep -q on a pipe (pipefail)
ch="$(echo "$P" | sed -n 's/^Characteristics[[:space:]]*0x//p')"
[ -n "$ch" ] && (( 0x$ch & 0x2000 )) && ok "DLL image (characteristics 0x$ch), $(echo "$P" | grep -c 'Export RVA') exports" || bad "not a DLL image (characteristics 0x$ch)"

imports="$(echo "$P" | sed -n 's/^[[:space:]]*DLL Name: //p' | sort -u)"
other="$(echo "$imports" | grep -viE '^(KERNEL32\.dll|api-ms-win-crt-[a-z]+-l1-1-0\.dll)$' || true)"
[ -z "$other" ] && ok "imports: KERNEL32 + $(echo "$imports" | grep -c api-ms-win-crt) UCRT api sets" || bad "imports beyond KERNEL32 / UCRT: $(echo $other)"

echo "$P" | awk '/\[Ordinal\/Name Pointer\] Table/{t=1;next} t&&/^[[:space:]]*\[/{print $NF;next} t&&/^[[:space:]]*$/{t=0}' | sort >"$T/win.txt"
if [ -f "$MAC" ]; then
	nm -gU "$MAC" | awk '{print $3}' | sed 's/^_//' | grep -v '^lzxd_' | sort >"$T/mac.txt"
	if cmp -s "$T/mac.txt" "$T/win.txt"; then ok "exports: $(wc -l <"$T/win.txt" | tr -d ' ') names, identical to the macOS plugin's (lzxd_* aside)"
	else bad "exports differ from $MAC (< macOS only, > Windows only; a dylib older than the sources?):"; diff "$T/mac.txt" "$T/win.txt" | grep '^[<>]' | sed 's/^/          /'; fi
else echo "  skip  exports vs macOS: no $MAC"; fi

python3 -I "$ROOT/scripts/windows/cs_imports.py" "$ROOT/Unity/Reflex/Assets/Scripts" >"$T/cs.txt"
missing="$(comm -23 "$T/cs.txt" "$T/win.txt")"
[ -z "$missing" ] && ok "C# imports: all $(wc -l <"$T/cs.txt" | tr -d ' ') entry points exported" || bad "C# imports not exported: $(echo $missing)"

names=$(sed -n '/kStructSizes\[\] = {/,/^};/p' "$W/plugin_api.cpp" | grep -oE 'RW_SIZE\([A-Za-z0-9_]+\)' | sed 's/RW_SIZE(//; s/)//')
{ echo '#include "reflex_world.h"'; echo 'extern "C" const int rw_sizes[] = {'; for n in $names; do echo "  (int)sizeof($n),"; done; echo '};'; } >"$T/sizes.cpp"
sizes() { "$@" -std=c++17 -I"$W" -S -o - "$T/sizes.cpp" 2>/dev/null | awk '/^_?rw_sizes:/{t=1;next} t&&/\.(long|4byte)/{print $2;next} t&&/^[^ \t.]/{t=0}'; }
ms="$(sizes clang++ -arch arm64 -target arm64-apple-macos12)"; ws="$(sizes x86_64-w64-mingw32-g++)"
n=$(echo "$names" | wc -w | tr -d ' ')
if [ -n "$ms" ] && [ "$ms" = "$ws" ] && [ "$(echo "$ws" | wc -l | tr -d ' ')" = "$n" ]; then ok "struct sizes: all $n equal (arm64 macOS = x86-64 Windows)"
else bad "struct sizes differ (name macOS Windows):"; paste <(echo "$names" | tr ' ' '\n' | grep .) <(echo "$ms") <(echo "$ws") | awk '$2!=$3' | sed 's/^/          /'; fi
lw="$(sed -n '/^typedef struct\|^struct /,/^}/p' "$W/reflex_world.h" | grep -nE '(^|[^a-z_])(long|wchar_t|bool)[[:space:]]' | grep -v 'long long' || true)"
[ -z "$lw" ] && ok "reflex_world.h structs: no long / wchar_t / bool members (LLP64-safe)" || bad "LP64 / LLP64-sensitive members in reflex_world.h: $lw"

if command -v wine >/dev/null || command -v wine64 >/dev/null; then
	echo "  todo  wine is installed: a loader test could run here (not written: wine was absent when this was made)"
else
	echo "  skip  run time: wine is not installed (not installed on purpose); load / run on a real Windows machine (notes)"
fi
[ $fail = 0 ] && echo "check: PASS (static)" || echo "check: FAIL"
exit $fail
