# Private terrain query draft

`terrain_query_draft.cpp` executes the decoded original `0x007c6160` terrain-producer closure over explicitly owned base/deformation grids. It is not registered in the public ABI or Unity library.

`reflex_terrain_query_draft(input,52112,output,19224)` snapshots all input, computes privately, then commits the full output. Return0 means success,1 is a malformed pointer/size boundary, and2 is an unsupported numerical/layout/FP input. Arbitrary input/output overlap is supported. Failed calls preserve output. The resource success/pointer adapter is explicit input, not a graphics API implementation.

The current ARM64 and x86-64 builds match all1,000 original generated records,4,806,000 raw output words each, including109,288 actual count/triangle writes. Ten overlap contracts plus analytical and atomic-failure cases pass on both architectures. Report: `_re/trace/out/terrain-query-native-draft-original/report.json`.

See `_re/notes/terrain-query-original-math.md` for the record layout, exact height/triangle/W behavior, source dependencies and proof limits. This draft makes no claim about live GPU grid binding or full collision/gameplay parity.
