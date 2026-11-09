# Private controls plus terrain traversal

`reflex_draft_bike_controls_terrain` connects the complete recovered controls callback to the recovered terrain ray traversal. The actual current control-stack endpoints and prior point/normal vectors feed the query. The caller supplies a base grid and coarse bounds; there is no supplied hit Boolean. The original registry selector executes against an explicit single-terrain world whose identity, marker, profile and shared scalar are checked.

The exact private record layout is in `record-layout.json`: 55,756-byte input and 39,188-byte output. Output contains complete evolving simulation/control/filter/runtime bytes followed by calculated query diagnostics. Geometry template vector slots are ignored and replaced by current control state. Misses preserve prior outputs; hits replace XYZ and preserve W.

Both ARM and Intel pass 256 cases against the independent connected reference models: 52 hits, 104 misses and 100 query-skipped cases, covering all four profiles. All 2,508,032 output words match per architecture; nine arbitrary input/output overlaps and five atomic invalid-join cases pass. Evidence is `_re/trace/out/bike-controls-terrain-native-composition/report.json`.

The separate controls and terrain components have pinned original-code proofs. This new combined report is a native-versus-exact-model composition check, not a new unchanged-original connected capture. The controlled single-world profile, finite arithmetic limits and absence of live GPU deformation remain explicit. This code is private, namespaced and outside the registered public ABI.
