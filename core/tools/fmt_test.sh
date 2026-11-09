#!/bin/bash
# fmt_test.sh [scratch dir] [archive] -- builds the readers with the product flags, runs the Python reference (fmt_test.py,
# the parsers of _re/formats) and the C dump (fmt_test.c), and diffs them. Exit status 0 = identical and all self checks passed.
# Writes only into the scratch dir (default: /tmp/rx_fmt_test). Never touches the install.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; CORE="$(dirname "$HERE")"
S=${1:-/tmp/rx_fmt_test}; ARCHIVE=${2:-"$CORE/../game files/Archive"}
mkdir -p "$S/obj"
FL=(-std=c11 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -fPIC -I"$CORE/include" -I"$CORE/src")
OBJ=(); pids=()
for f in "$CORE"/src/fmt_*.c; do o="$S/obj/$(basename "$f" .c).o"; OBJ+=("$o"); clang "${FL[@]}" -c -o "$o" "$f" & pids+=($!); done
for p in "${pids[@]}"; do wait "$p"; done
clang "${FL[@]}" -o "$S/fmt_test" "$HERE/fmt_test.c" "${OBJ[@]}"
echo "== python reference"
PYTHONDONTWRITEBYTECODE=1 python3 "$HERE/fmt_test.py" "$ARCHIVE" "$S/py.txt" "$S/samples.txt" ${FMT_DUMP:+"$S/pydump"}
echo "== C readers"
[ -n "$FMT_DUMP" ] && mkdir -p "$S/cdump"
rc=0; "$S/fmt_test" "$ARCHIVE" "$S/c.txt" "$S/samples.txt" ${FMT_DUMP:+"$S/cdump"} || rc=$?
echo "== diff"
if diff -q "$S/py.txt" "$S/c.txt" >/dev/null; then
  echo "IDENTICAL: $(wc -l < "$S/c.txt") lines"
  for t in vfs db pkg assets a bxml resolve scene sub anchor table dlc mtable loc tparams height coarse soft tmat zones collis place chunk model surf tex; do
    printf '  %-8s %s\n' "$t" "$(grep -c "^$t " "$S/c.txt")"; done
else
  diff "$S/py.txt" "$S/c.txt" | head -40; echo "DIFFERENT (full diff: diff $S/py.txt $S/c.txt)"; rc=1
fi
exit $rc
