#!/bin/bash
# build_event_test.sh <out dir> -- builds core/tools/build_event_test (the end-to-end driver of construction unit 8); links the driver and tools/rx_canon.c against the product objects of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/build_event_test" "$C"/tools/rx_canon.c "$C"/tools/build_event_test.c
echo "built $O/build_event_test"
