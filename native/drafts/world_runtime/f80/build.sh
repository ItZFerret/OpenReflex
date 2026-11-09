#!/bin/sh
# Builds the F80 test driver for arm64 and x86_64 (strict flags, no fast-math / FMA contraction).
set -e
cd "$(dirname "$0")"
for arch in arm64 x86_64; do
  clang++ -std=c++17 -O2 -Wall -Wextra -Werror -fno-fast-math -ffp-contract=off \
    -arch $arch f80.cpp f80_test.cpp -o f80_test_$arch
done
