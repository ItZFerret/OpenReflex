#!/bin/bash
# rx_framerate_test.sh <out dir> -- builds core/tools/rx_framerate_test (frame-rate dependence of the product session) against the
# product objects of core/build.sh (run that first): core/build/obj/*.o + the driver.
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"
clang -std=c11 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 \
  -I"$C/include" -I"$C/src" -I"$C/game" -c -o "$O/rx_framerate_test.o" "$C/tools/rx_framerate_test.c"
clang++ -arch arm64 -o "$O/rx_framerate_test" "$O/rx_framerate_test.o" "$C"/build/obj/*.o -lz
echo "built $O/rx_framerate_test"
