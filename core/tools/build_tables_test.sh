#!/bin/bash
# build_tables_test.sh <out dir> -- builds core/tools/build_tables_test (construction unit 12a tables); links the driver and tools/rx_canon.c against the product objects of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/build_tables_test" "$C"/tools/rx_canon.c "$C"/tools/build_tables_test.c
echo "built $O/build_tables_test"
