#!/bin/bash
# fbhud_session_test.sh <out dir> -- builds core/tools/fbhud_session_test against the product objects of core/build.sh (tools/product_link.sh).
# Run: <out>/fbhud_session_test "<install root>" [FR_Alpine 45]   (w-fb-hud: RESET TRACK, CONTROLS IN A RACE)
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}
mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/fbhud_session_test" "$C"/tools/fbhud_session_test.c
echo "built $O/fbhud_session_test"
