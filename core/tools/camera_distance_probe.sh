#!/bin/bash
# camera_distance_probe.sh <out dir> -- builds core/tools/camera_distance_probe (the game camera's distance to the bike, w-camfix) and
# core/tools/career_series_test (CST_DRIVING_CAMERA: the saved camera across a career series' races), linked against the product objects
# of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/camera_distance_probe" "$C"/tools/camera_distance_probe.c
rx_link_driver "$O/career_series_test" "$C"/tools/career_series_test.c
echo "built $O/camera_distance_probe $O/career_series_test"
