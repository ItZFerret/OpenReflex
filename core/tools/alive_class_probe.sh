#!/bin/bash
# alive_class_probe.sh <out dir> -- builds core/tools/alive_class_probe.c against the product objects (core/build.sh's link set).
set -eu
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"
source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/alive_class_probe" "$C/tools/alive_class_probe.c"
echo "$O/alive_class_probe"
