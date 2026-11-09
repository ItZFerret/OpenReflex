# Private complete terrain-only contact stage

`reflex_slot13_draft(const void* input, size_t input_bytes, void* output,
size_t output_bytes)` is a private C symbol, not a registered public ABI.
Compile with `REFLEX_SLOT13_NO_MAIN` to link it. All interpreter symbols are
inside `reflex_slot13_private`, allowing linkage with other private stages.
Build with strict float operations, contraction disabled, and rounding access.
The callable snapshots all input before publishing output, permits arbitrary
input/output overlap, rejects unsupported FP controls and restores the caller's
FP environment, including sticky status. Return0 succeeds,1 means malformed
buffer shape/null,2 means unsupported input or numerical domain. No partial
output is published on failure.

The record uses little-endian original uint32 words and byte spans:

| Input offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 28 | dt bits, profile, nx, nz, stride, base present, lock success |
| 28 | 20 | float world width/depth, spacing, deformation scale, normal scale |
| 48 | 38656 | Complete S+0..9700 |
| 38704 | 256 | Control block, preserved |
| 38960 | 72 | Six original three-float filters |
| 39032 | 152 | Runtime spans in RuntimeAddresses/RuntimeSizes order |
| 39184 | 16384 | Base-grid storage including explicit border/padding |
| 55568 | 16384 | Deformation-grid storage including explicit border/padding |
| 71952 | 19200 | Seeded400-triangle output capacity |

Total input91152 bytes. Output58360 bytes contains S, control, filters,
runtime spans, full triangle capacity, then six uint32 telemetry values:
locks, unlocks, last flags, null rectangle, last returned-pointer Boolean,
final triangle count. Output offsets are0,38656,38912,38984,39136,58336.

Canonical memory addresses are S10000000, control20000000, six filters30000000,
terrain31000000, scene32000000, resource33000000, base34000000, deformation35000000.
The runtime registry is explicitly one matching terrain. Required state pointers
S+d78/d7c and their body words28/2c/30/34 use canonical S addresses; vtable words
retain original addresses. Filters point inside the72-byte owned filter region.
The descriptor start/end at S+130c/1310 must be equal. Four bodies and1..12 body
probes are bounded; proof cases retain all12 original constructor probes.
Actual slot32=44ef70, rotor/fixed node6, wheel triangle producer/test/transform,
body sample, position correction and six filters all execute. A linked resource
success/null-pointer profile supplies owned deformation bytes. It supplies no
contact or height-query answer.

The source and fixed original instruction data are private staging code. See
`verify.py` for recorded ARM/Intel comparisons to accepted original outputs,
and `contract.cpp` for full record overlap, atomic failures and FP restoration.
This does not establish dynamic-object collisions, live graphics-resource
binding, or a complete consecutive game loop.
