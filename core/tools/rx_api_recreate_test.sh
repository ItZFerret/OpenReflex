#!/bin/bash
# rx_api_recreate_test.sh <out dir> [install root [iterations (3) [frames (10) [level]]]] -- builds core/tools/rx_api_recreate_test (create ->
# event load -> frames -> destroy, repeated in one process; links the product objects of core/build.sh) and, with an install root, runs
# it twice: N iterations in one process, and a fresh process with one iteration. Passes when every iteration equals iteration 1 and
# iteration 1 equals the fresh process's (the first create is not changed by the reset machinery either).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/rx_api_recreate_test" "$C"/tools/rx_api_recreate_test.c
echo "built $O/rx_api_recreate_test"
[ -n "$2" ] || exit 0
ROOT=$2; N=${3:-3}; FR=${4:-10}; LV=$5
"$O/rx_api_recreate_test" "$ROOT" "$N" "$FR" $LV | tee "$O/recreate_multi.log"
"$O/rx_api_recreate_test" "$ROOT" 1 "$FR" $LV > "$O/recreate_fresh.log"
a=$(grep '^RESULT iter=1 ' "$O/recreate_multi.log"); b=$(grep '^RESULT iter=1 ' "$O/recreate_fresh.log")
if [ -n "$a" ] && [ "$a" = "$b" ]; then echo "recreate: iteration 1 equals a fresh process"; else echo "recreate: iteration 1 DIFFERS from a fresh process"; echo "$a"; echo "$b"; exit 1; fi
