#!/bin/bash
# lookback_probe.sh <out dir> -- builds core/tools/lookback_probe (w-lookback: the stock camera's look back across resets, respawns, menus)
# against the product objects of core/build.sh (tools/product_link.sh).
# Run: <out>/lookback_probe "<game files>/Archive" [scenario|all] [FR_Alpine 45]
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}
mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/lookback_probe" "$C"/tools/lookback_probe.c
echo "built $O/lookback_probe"
