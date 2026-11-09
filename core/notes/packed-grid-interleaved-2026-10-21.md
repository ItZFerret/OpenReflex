# Interleaved packed-grid sample 007f8d10

Native implementation and registration in `game/world_node_water.c`, authoritative ABI in `game/world_node.h`; terrain material caller now calls it directly.

Disassembly establishes ESI = grid, ECX ignored (terrain caller happens to put grid in both). Stack arguments u, v, layer; RET12. Shared width +48, stride +38, total record bits +14, data +88; per-layer field width +68+4*layer and shift +78+4*layer. No UV clamp. Preserve original two-word extraction including masked x86 shifts and field-width mask before the final layer shift. X87 uses 53-bit main-thread arithmetic, floor's double spill, truncating fistp qword. Dead frame, argument overwrites, callee saves, floor frame stores reproduced. EDX final mask; ECX 32-fieldwidth. XMM0 reproduces floor SSE2 positive exponent and negative comparison-mask branches; original DLL disassembly inspected through REFLEX_EXE override.

Oracle adapter in `oracle/world_node_water.cpp` compares EAX/ECX/EDX/XMM0. Synthetic `world_packed_grid_probe.cpp`: hook group rx_PackedGridProbeHook, env RX_PACKED_GRID_PROBE=1, fifth 0084ed20 call, 896 original sampler calls through adapter. Patterned private data; total widths1/5/8/13/16/31/32; layers full/1/5/0 bit widths; per-layer shifts; cross-word records; exact32-bit boundary; UV below0 and above1; ECX deliberately unrelated to ESI. Additionally32 TerrainMaterialAt calls on private synthetic terrain/material/point descriptors cover the replaced branch, busy flag and negative-u XMM0 propagation. Restores machine registers; private allocations are development-only probe stimuli.

Checks performed:

```
clang -std=c11 -fsyntax-only -Icore/include -Icore/src core/game/world_node_water.c core/game/phys_terrain.c
clang++ -std=c++17 -fsyntax-only -Inative/drafts/world_runtime -Inative/game core/oracle/world_node_water.cpp core/oracle/world_packed_grid_probe.cpp
```

Both pass. Parent performs serial product/oracle builds and original oracle run; no claimed runtime acceptance yet. Probe run: existing normal race-world invocation with >=5 frames and `RX_PACKED_GRID_PROBE=1 REFLEX_PORT_VERIFY=1 REFLEX_PORT=rx_world,rx_PackedGridProbeHook`. Check sampler896 calls and0 mismatches. Normal acceptance (probe disabled): `native/game/tools/verify.sh <reflex_boot> rx_world quick` or complete five-world run. To exercise the replaced caller under Verify, select `rx_phys,rx_world` (or select exact `rx_PackedGrid_InterleavedSampleUV,rx_TerrainMaterialAt,rx_PackedGridProbeHook` for the focused synthetic run).


Central acceptance: product/oracle builds pass. Hook group corrected to thunk before final tests. 896 direct sampler and4,712 terrain caller Verify comparisons, zero mismatches;32 synthetic interleaved caller cases are included in caller count. On/reference statefc092b3cdac8235c and memorye0b5a7c213cd8ea7 match. Native StillwaterB900frames and windowed45-second12rider regression pass. See builds/sol_ports/acceptance.json and remaining-ports handoff for limits.
