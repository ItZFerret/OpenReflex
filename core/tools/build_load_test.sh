#!/bin/bash
# build_load_test.sh <out dir> -- builds the asset loader tests: build_test (the asset system unit, tools/build_test.c: compare with the
# original's canonical dump) and build_load_check (every asset of a type through its loader, tools/build_load_check.c); each driver and
# tools/rx_canon.c link against the product objects of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/build_test" "$C"/tools/rx_canon.c "$C"/tools/build_test.c
rx_link_driver "$O/build_load_check" "$C"/tools/rx_canon.c "$C"/tools/build_load_check.c
echo "built $O/build_test $O/build_load_check"
