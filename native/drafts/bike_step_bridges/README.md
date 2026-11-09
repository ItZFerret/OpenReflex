# Private whole-state bike bridges

`reflex_draft_bike_step_bridge` provides five callable stages for the developing
continuous bike runtime. It consumes the current complete motocross state and
explicit controls/globals; it does not load a recorded intermediate result.
The registered plugin and public `reflex_core.h` ABI remain unchanged.

| Stage | Original boundary | Result written into the complete state |
| --- | --- | --- |
| 1 | `0085d100` | Four body positions and orientations |
| 2 | slot 16, `0043e200` | Steering anchors and local pose vectors |
| 3 | `0085d300`, ascending node slot 6 | Fixed joint and two rotor body poses |
| 4 | slot 17, `0043c1a0` | Post-contact scalar state |
| 5 | `00431e66..00431edd` | Speed-filter blend and history |

The exact input record is 39,096 bytes: state `0x9700`, controls `0x100`,
the original simulation address, raw binary32 `dt`, then the 176-byte globals
structure in `bike_step_bridges.h`. The output is the complete `0x9700` state.
The simulation address resolves in-state node/body pointers; pointed world
objects are not accessed by these five stages. The tested profile is the
original four-body motocross graph (root, fixed node, two rotor nodes).
Node6 requires the exact constructor node locations and parent/own body bindings.
Arbitrary internal pointer aliases are rejected; this is distinct from supported
overlap between the API input and output buffers.

All inputs are copied before calculation and output commits once, allowing
unaligned, in-place, and partially overlapping buffers. Invalid arguments,
unsupported floating-point controls, or invalid/numerically unsupported state
leave the output unchanged. Caller floating-point controls and exception flags
are restored on success and failure. These stages use finite binary32
arithmetic; the original x87 precision setting does not select a different
arithmetic path here.

## Verification

Run `python3.14 native/drafts/bike_step_bridges/verify.py` from the project.
It first performs a fresh protocol/parent audit of the accepted capture
`_re/trace/out/20261001T235505.956913Z-alpine-bike-step-pilot`, then pairs actual
original whole-state boundaries. Expected bytes come directly from the original
capture, not the Python numerical model.

Both ARM64 and Intel match all 549 calls per stage (2,745 calls and 26,527,680
complete-state words per architecture), including 120 active calls per stage.
The replay also checks overlap, atomic rejection, unsupported rounding, and
preservation of pre-existing floating-point exception flags. Reports, compiler
commands, complete local import-dependency fingerprints, and input/output hashes
are saved in `_re/trace/out/bike-step-bridges-native-live/report.json`.
The universal private library is
`native/build/libreflex_bike_step_bridges_draft.dylib`.

These comparisons establish five individual adjacent-stage mappings. They do
not establish a complete frame, reconstruction of external world state, or
continuous gameplay equivalence.
