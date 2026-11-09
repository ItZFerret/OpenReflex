#!/bin/bash
# practice_test.sh <out dir> -- builds core/tools/practice_test against the product objects of core/build.sh (tools/product_link.sh).
# Run: <out>/practice_test "<install root>" finish 2 (PT_CLASS_OUT=<file>), then <out>/practice_test "<install root>" practice 10
# (PT_CLASS_IN=<file>; PT_POKE=1 / PT_JITTER=1: practice_test.c's header).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}
mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/practice_test" "$C"/tools/practice_test.c
echo "built $O/practice_test"
