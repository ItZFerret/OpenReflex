#!/bin/bash
# gridpair_build.sh <out dir> -- builds the product side of the paired ride (w-gridpair): core/tools/bike_parity_probe.c linked with the
# product's own objects (core/build.sh's link set, tools/product_link.sh) into <out dir>/bike_parity_probe.
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=$(mkdir -p "${1:?out dir}" && cd "$1" && pwd)
source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/bike_parity_probe" "$C/tools/bike_parity_probe.c"
echo "built $O/bike_parity_probe"
