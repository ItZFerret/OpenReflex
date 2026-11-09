#!/bin/bash
# rx_api_test.sh <out dir> -- builds core/tools/rx_api_test (the session API's first-call driver)
# links the driver against the product objects of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/rx_api_test" "$C"/tools/rx_api_test.c
echo "built $O/rx_api_test"
