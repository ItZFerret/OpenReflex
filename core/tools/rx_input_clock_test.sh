#!/bin/bash
# rx_input_clock_test.sh <out dir> -- builds core/tools/rx_input_clock_test (product test of the session input / clock path)
# links the driver against the product objects of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/rx_input_clock_test" "$C"/tools/rx_input_clock_test.c
echo "built $O/rx_input_clock_test"
