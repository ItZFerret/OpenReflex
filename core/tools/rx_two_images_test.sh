#!/bin/bash
# rx_two_images_test.sh <install root> [level] [riders] -- builds tools/rx_two_images_test.c and runs it on two copies of
# build/librxcore.dylib (core/build.sh first): the test-ride arena's session and an event's session in one process (w-loadarena).
set -e
cd "$(dirname "$0")/.."
O=build/two_images; mkdir -p "$O"
cp -f build/librxcore.dylib "$O/librxcore-a.dylib"; cp -f build/librxcore.dylib "$O/librxcore-b.dylib"
clang -std=c11 -O2 -arch arm64 -Iinclude -o "$O/rx_two_images_test" tools/rx_two_images_test.c
"$O/rx_two_images_test" "$PWD/$O/librxcore-a.dylib" "$PWD/$O/librxcore-b.dylib" "$@"
