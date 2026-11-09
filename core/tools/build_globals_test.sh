#!/bin/bash
# build_globals_test.sh <out dir> -- builds core/tools/build_globals_test (construction unit 12b); links the driver and tools/rx_canon.c against the product objects of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/build_globals_test" "$C"/tools/rx_canon.c "$C"/tools/build_globals_test.c
echo "built $O/build_globals_test"
