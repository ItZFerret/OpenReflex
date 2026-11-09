# Original wheel-contact numeric draft

Unregistered native draft for `00438260` (test supplied triangles) and
`0042db20` (transform contact into wheel state). There are no public ABI,
CMake, Unity, observer, or original-game changes in this directory.

The Python reference executes pinned non-executable instruction data with
exact rational binary32 arithmetic. Its shared SSE instruction machinery is
read-only from the earlier tire reference. The native draft uses the same
separate contact program data with strict single-precision operations and
explicit packed comparison/NaN behavior. It stages every output before commit.
The local byte API accepts input/output overlap; pointers and lengths are
checked before executing. No tree of captured intermediate answers is used.

`reflex_contact_draft(mode,input,input_bytes,output,output_bytes)` returns0
for success,1 for invalid layout/argument,2 for unsupported FP/numerical data.
Mode0 is contact, mode1 transform. This is a private development interface.

| Mode | Input | Output |
| --- | --- | --- |
| contact | 19,332 bytes: count, body quaternion, position, radius/extents, old point, old normal, runtime axisX/numerator/G, capacity400 triangles (three vec4 each) | 36 bytes: boolean, final point, final normal |
| transform | 464 bytes: complete0x150-byte wheel record, body position, world normal, world point, body quaternion, root quaternion, three runtime globals | Complete0x150-byte wheel record; nine mutable vec4 spans |

The contact inputs describe generated finite geometry. They are not exported
terrain query answers. Empty/fully culled contact tests preserve seeded point
and normal; other false-return paths can change the point as scratch. Always
consume/compare the full raw outputs, even when the boolean is false. The
eight-slot contact ring is reduced in physical slot order. The transformer
preserves every byte outside30/40/50/60/70/90/120/130/140 vec4 spans.

All directly meaningful input float words and final numerical outputs must be
finite. Opaque wheel fields and unused capacity triangles are not interpreted.
Masked intermediate invalid values follow the original packed SSE value
semantics. Nearest/even rounding with gradual underflow is required; sticky
exception flags are outside the equivalence claim. No pow/x87/import adapter
is reached in this two-function closure. The shared-style interpreter retains
some unused tire opcodes, but the pinned contact program cannot reach them.

Original proof: `_re/trace/out/original-math-contact-generated/report.json`.
One thousand calls per function pass exact Python/original comparison, all
source/opaque/global checks and aligned buffer canaries. Native proof:
`_re/trace/out/bike-contact-native-draft-original/report.json`. Both ARM and
Intel reproduce all2,000 calls,93,000 raw words each, including42,616 actual
mutable/return words. The remaining compared words verify unchanged state.
Each architecture also passes18 overlapping-buffer cases and malformed,
nonfinite, late-invalid and unsupported-FP atomic-rejection contracts.
Nine Python analytical/domain tests cover winding, tangency, ring overwrite,
maximum capacity, false-return scratch mutation and exact transformed state.

The base proof uses canonical runtime globals and bounded positions/radii,
normalized quaternion samples, both windings and multi-triangle cases. It is
finite sample evidence, not an exhaustive proof for all binary32 inputs.
Original terrain deformation, triangle production, object shape queries,
slot13 body collision, later position/orientation corrections and consecutive
bike-step behavior remain separate work. See
`_re/notes/bike-intervening-callbacks.md` for those dependencies.

Supplemental irregular-triangle evidence:
`_re/trace/out/original-math-contact-irregular-generated/report.json` and
`_re/trace/out/bike-contact-native-draft-irregular/report.json`. Another1,000
original contact calls match both unchanged native binaries, including274
false-return point-scratch mutations,674 hits and11 ring-overflow cases.
Combined proof:3,000 original calls,102,000 raw compared words and50,104
actual mutable/return words per architecture. The supplemental reference and
native sources retain the base proof's hashes.
