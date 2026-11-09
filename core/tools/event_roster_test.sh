#!/bin/bash
# event_roster_test.sh <out dir> -- builds core/tools/event_roster_test (w-opencross: the vehicle classes of an event's field, headless) against the product objects of core/build.sh (tools/product_link.sh).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/event_roster_test" "$C"/tools/event_roster_test.c
echo "built $O/event_roster_test"
