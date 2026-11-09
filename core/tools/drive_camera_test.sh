#!/bin/bash
# drive_camera_test.sh <out dir> -- builds core/tools/drive_camera_test (the saved camera in a race)
# links the driver against the product objects of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/drive_camera_test" "$C"/tools/drive_camera_test.c
echo "built $O/drive_camera_test"
