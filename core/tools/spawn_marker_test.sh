#!/bin/bash
# spawn_marker_test.sh <out dir> -- builds core/tools/spawn_marker_test against the product objects of core/build.sh (tools/product_link.sh).
# Run: <out>/spawn_marker_test "<game files>/Archive's install root" [FreeRide 9 FR_Alpine 45 | National 7 NAT_Concord 1]
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}
mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/spawn_marker_test" "$C"/tools/spawn_marker_test.c
echo "built $O/spawn_marker_test"
