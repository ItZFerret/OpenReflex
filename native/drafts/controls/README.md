# Private original controls draft

This executable runs the pinned instruction-derived `0044c170` controls closure,
including its `0044e0d0` alternate mode, filters, curve helpers, manual gears,
direction calculation, and `00402dc0` registry selection. It is isolated from
registered CMake targets, the public C ABI, and Unity.

`verify.py` compiles ARM and Intel with contraction and fast math disabled,
compares each complete output record against fixed original execution, and
checks source/vector hashes before and after. Both architectures pass the
32-call pilot and 1,000 generated calls: 74,744 mutable output words in the
generated set, or 9,789,000 words when all preserved state and query records are
counted. Each also passes 6,000 generated exact arithmetic checks and 12 directed
checks. Evidence is `_re/trace/out/bike-controls-native-draft-original/report.json`.

The private input record is 39,136 bytes: simulation `0x9700`, control `0x100`,
three external filter records (36 bytes), 176 bytes of explicit runtime globals
in ascending address order, then little-endian `dt`, profile, and query outcome.
The 39,156-byte result contains the same state/control/filter/global spans and an
eight-word query record (count, start XYZ, end XYZ, hit). Both normal and alternate
parents enter once and evolve a single mutable memory image. Pointed filters may
alias one another or an inline filter; writes are sequential. Filter overlap
with pointer identity words is rejected explicitly.

The profile's high bit selects x87 PC24 (`007f`) or PC53 (`027f`). Its low bit is
repeated metadata; this closure does not execute the game's integer conversion
adapter. SSE operations use separate binary32/binary64 operations. x87 multiply
uses a 106-bit integer significand product with one nearest-even rounding. Add
uses exact signed integer alignment and rejects exponent gaps over 70. Both
helpers reject values outside the private binary64 exponent representation;
this is narrower than general x87 and covers the fixed original cases. The
float-input `fmod` wrapper uses integer modular shifts, preserving signed zero.

The explicit query outcome is required input, never an implicit no-hit default.
Original registry selection and segment construction execute. The owned world
and scene records match the original harness; a hit writes the specified
synthetic XYZ point `(1,2,3)` and normal `(0,1,0)`. A miss preserves both result
spans. This does not implement terrain traversal or claim live controls parity.
The frozen original evidence compares parent state and query XYZ/return, not
uninitialized query scratch bytes. The complete native record uses that same
scope.

The generated opcode enum includes unused operations inherited from the frozen
tire interpreter. None occur outside the pinned 2,481-instruction controls
program. The native executable accepts at most 10,000 records and rejects
unmapped memory, unowned writes, unsupported arithmetic ranges, invalid query
identity/count, and unbalanced x87 state. It is a validation draft, not a stable
application interface.
