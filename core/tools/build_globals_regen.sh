#!/bin/bash
# build_globals_regen.sh -- regenerates game/build_globals_gen.c and oracle/build_globals_ports.cpp from MXReflex.exe (needs _re/.venv/bin/python with capstone / pefile)
cd "$(dirname "$0")/.."
../_re/.venv/bin/python tools/build_globals_gen.py "../game files/Archive/MXReflex.exe" ../_re/out/functions.tsv game/build_globals_gen.c \
  --func 00812170:rx_input_names_init,00843950:rx_xenon_core_ctor
