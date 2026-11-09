#!/bin/bash
# build_render_test.sh <out dir> -- builds build_render_check (consumption test of the render-side loaders) and build_render_test (their
# canonical-dump driver); links the product objects of core/build.sh (tools/product_link.sh). Compare with build_render_cmp.py (see construction.md 9.6).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/build_render_check" "$C"/tools/rx_canon.c "$C"/tools/build_render_check.c
rx_link_driver "$O/build_render_test" "$C"/tools/rx_canon.c "$C"/tools/build_render_test.c
echo "built $O/build_render_check $O/build_render_test"
