#!/bin/bash
# One canonical map is also read by gen_fnsig.py; no blanket observer defines.
RX_NATIVE_TU_SOURCES=(); RX_NATIVE_TU_DEFINES=()
while IFS=$'\t' read -r source define; do
 [ -n "$source" ] || continue
 RX_NATIVE_TU_SOURCES+=("$source"); RX_NATIVE_TU_DEFINES+=("$define")
done < tools/native_tu_flags.tsv
rx_native_tu_flags() {
 RX_NATIVE_TU_FLAGS=()
 local i
 for ((i=0;i<${#RX_NATIVE_TU_SOURCES[@]};i++));do
  if [ "$1" = "${RX_NATIVE_TU_SOURCES[$i]}" ];then RX_NATIVE_TU_FLAGS=("${RX_NATIVE_TU_DEFINES[$i]}");return;fi
 done
}
