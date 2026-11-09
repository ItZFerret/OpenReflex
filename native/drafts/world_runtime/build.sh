#!/bin/bash
# Builds the validation CLI and the Unity plugin (arm64 macOS).
set -euo pipefail
cd "$(dirname "$0")"
CS=../../../_re/.venv/lib/python3.14/site-packages/capstone
FLAGS=(-std=c++17 -O3 -g -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -I. -I../../game -I$CS/include)
SRC=(runtime.cpp boot.cpp career.cpp persist.cpp vm.cpp jit.cpp deform.cpp particles.cpp ambient.cpp props.cpp atv.cpp mud.cpp bink.cpp modes.cpp input.cpp pages.cpp tutorial.cpp f80/f80.cpp garage.cpp remap.cpp ../../game/port.cpp $(ls ../../game/ports/*.cpp | tr "\n" " "))
clang++ "${FLAGS[@]}" -o world_validate validate_main.cpp "${SRC[@]}" -Llib -lcapstone.5 -lz -Wl,-rpath,@executable_path/lib
clang -O2 -arch arm64 -c -o lib/libmspack/lzxd.o lib/libmspack/lzxd.c -Ilib/libmspack
clang++ "${FLAGS[@]}" -fvisibility=hidden -dynamiclib -install_name @rpath/libreflexworld.dylib -o libreflexworld.dylib plugin_api.cpp crt_extract.cpp lib/libmspack/lzxd.o "${SRC[@]}" -Llib -lcapstone.5 -lz -Wl,-rpath,@loader_path -Wl,-rpath,@loader_path/lib
clang++ "${FLAGS[@]}" -o reflex_boot boot_main.cpp "${SRC[@]}" -Llib -lcapstone.5 -lz -Wl,-rpath,@executable_path/lib
# reflex_client: the plugin driven through its C API as the Unity client drives it (client_main.cpp; native/game notes/loading.md section 9)
clang++ "${FLAGS[@]}" -o reflex_client client_main.cpp -L. -lreflexworld -Wl,-rpath,@executable_path -Wl,-rpath,@executable_path/lib
echo built world_validate libreflexworld.dylib reflex_boot reflex_client
