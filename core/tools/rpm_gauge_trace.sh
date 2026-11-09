#!/bin/bash
# rpm_gauge_trace.sh <out dir> -- builds core/tools/rpm_gauge_trace (w-rpmgauge: the MODERN HUD's rev gauge against the engine's real rpm,
# headless) against the product objects of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/rpm_gauge_trace" "$C"/tools/rpm_gauge_trace.c
echo "built $O/rpm_gauge_trace"
