#!/bin/bash
# product_link.sh -- sourced by the test-driver build scripts. Test drivers link against the objects core/build.sh builds (its source list,
# per-file defines from native_tu_flags.tsv, the generated rx_fnsig.c), so they cannot drift from the product's link set.
#   rx_product_objs                      -- runs core/build.sh (incremental) into ${RX_PRODUCT_DIR:-core/build}; sets RX_PRODUCT_OBJS
#   rx_link_driver <out exe> <src>...    -- compiles each driver source (product flags) into $O/drv and links them with RX_PRODUCT_OBJS
# Needs C (the core dir) and O (the script's out dir) set by the caller.
RX_DRIVER_CFLAGS=(-std=c11 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -I"$C/include" -I"$C/src" -I"$C/game" -I"$C/api")
rx_product_objs() {
  local pd=${RX_PRODUCT_DIR:-$C/build}   # build.sh names its objects after the dir argument as given: always pass the same spelling (default: none, like a plain `bash core/build.sh`)
  bash "$C/build.sh" ${RX_PRODUCT_DIR:+"$RX_PRODUCT_DIR"} > "$O/product_build.log" 2>&1 || { echo "core/build.sh failed (log: $O/product_build.log)"; tail -20 "$O/product_build.log"; return 1; }
  RX_PRODUCT_OBJS=(); local l
  while IFS= read -r l; do case $l in /*) RX_PRODUCT_OBJS+=("$l");; *) RX_PRODUCT_OBJS+=("$C/$l");; esac; done < "$pd/objs.list"   # build.sh's own link set, not obj/*.o (stale objects of removed sources stay there)
}
rx_link_driver() {
  local exe=$1 f o drv=(); shift; mkdir -p "$O/drv"
  for f in "$@"; do
    o="$O/drv/$(basename "$f").o"; drv+=("$o")
    clang "${RX_DRIVER_CFLAGS[@]}" -c -o "$o" "$f" || return 1
  done
  clang++ -arch arm64 -o "$exe" "${drv[@]}" "${RX_PRODUCT_OBJS[@]}" -lz
}
