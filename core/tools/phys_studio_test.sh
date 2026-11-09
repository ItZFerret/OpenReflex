#!/bin/bash
# phys_studio_test.sh <out dir> -- builds core/tools/phys_studio_test and core/tools/jobthreads_hash (with JT_PHYS) against the product
# objects of core/build.sh (tools/product_link.sh). The checks themselves: core/plan/PHYSICS_STUDIO.md "Proofs".
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}
mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/phys_studio_test" "$C"/tools/phys_studio_test.c
rx_link_driver "$O/jobthreads_hash" "$C"/tools/jobthreads_hash.c
echo "built $O/phys_studio_test $O/jobthreads_hash"
