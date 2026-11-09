#!/bin/bash
# sound_slide_b_test.sh <out dir> [install root [level]] -- builds core/tools/sound_slide_b_test (links the product objects) and, with an install root,
# runs it: the RX_SCF_SLIDE_B contacts carry slide_kind / slide_speed (w-reverb2).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/sound_slide_b_test" "$C"/tools/sound_slide_b_test.c
echo "built $O/sound_slide_b_test"
[ -n "$2" ] || exit 0
"$O/sound_slide_b_test" "$2" $3
