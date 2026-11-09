#!/bin/bash
# fp_camera_test.sh <out dir> -- builds core/tools/fp_camera_test (the first-person camera driver)
# links the driver against the product objects of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/fp_camera_test" "$C"/tools/fp_camera_test.c
echo "built $O/fp_camera_test"
