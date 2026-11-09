# Private raw bike core bridge

`reflex_bike_core_bridge(const void*, size_t, void*, size_t)` decodes the
four-body motocross constructor graph from one raw simulation, invokes the
registered `reflex_mechanism_core`, then writes its numerical rows back into
the same state layout. This removes Python object/hex conversion from each
physics step. It introduces no public ABI or additional physics algorithm.

The input is **38,864 bytes**:

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 38,656 | Complete original S+0..9700 image |
| 38,656 | 4 | Original/canonical simulation address |
| 38,660 | 4 | Original image base, normally00400000 |
| 38,664 | 4 | Raw dt bits |
| 38,668 | 4 | Reserved, must be zero |
| 38,672 | 192 | Public `reflex_mechanism_core_input` bytes24..216 |

The final192 bytes are axis_x/y/z, global_g, sign_x/y/z, conjugate,
mask, one, two, negative_two, negative_zero, numerator and extra, in that
order. Count and gravity come from S+ec and S+d0. Output is the complete
**38,656-byte** simulation. Return codes are the public core status enum.

The adapter requires root/fixed/rotor/rotor nodes in parent-before-child
order. It verifies original embedded matrix and block headers, all body and
matrix/vector pointers, graph membership, local/result storage and disjoint
regions. The root RHS/solution alias is the sole supported raw alias.
Headers, padding, local vectors, opaque bytes, positions and accumulators
are copied from the input. Only the core's actual numerical outputs change.
The caller's input is snapshotted before calculation; arbitrary input/output
overlap works, failures leave output untouched and caller FP state is restored.

The ARM shared library is `libbike-core-bridge-arm.dylib`. It depends on the
registered `@rpath/libreflexcore.1.dylib`; its development rpath points to
`native/build-v24-canonical`. A unified Unity library can compile this source
and link the same registered core directly. No embedded executable or Wine
runtime is needed by the callable.

`verify.py` checks942 accepted connected original intervals. It computes the
force state through the registered native force stage, then gives that state
to this adapter. All captured final core fields match original; all remaining
state bytes are preserved from input. They were not independently recorded at
the core-exit boundary. ARM and Intel both pass9,103,488 full raw state words,
nine overlap layouts, eighteen malformed graph/layout/header cases, nonfinite
and unsupported-FP atomic failures, and FP restoration. The actual ARM dylib
also passes all942 records. Reports are in
`_re/trace/out/bike-core-bridge-native-original/`.
