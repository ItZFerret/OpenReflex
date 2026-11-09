# Private original bike force-stage draft

This unregistered native draft composes original `0043e100` over one mutable
simulation snapshot. It runs the timer/gear/engine/control-torque branch, both
suspension paths, damping, the four runtime-G resets, tire 0, tire 1, and the
final root-torque copy in original order. Each operation reads the writes made
by earlier operations. The contact block at `S+8790` shares `S+8810`, and both
tires retain that alias without substituting intermediate reference outputs.

`force_stage_draft.cpp` includes the existing powertrain/force drafts and the
frozen private tire implementation. The latter's standalone entry point is
renamed at preprocessing time; its source is unchanged. All fixed-size field
transfers use `memcpy`. The complete input is copied before any operation, and
the `calculate` function only returns a completed state on success. The CLI is
an evidence runner, not a public transactional file API.

The four damping `table[-1]` words are read from the current state after
suspension. Parent profiles 0/1 mean x87 CW007f (24-bit precision), and profiles
2/3 mean CW027f (53-bit). The damping leaf's opposite profile convention is
translated with `profile XOR 2`; tire profiles pass through unchanged.

Run `python3.14 native/drafts/force_stage/verify.py` from the project root to
compile ARM and Intel binaries and compare all final simulation bytes to the
two accepted isolated original force-stage datasets. The verifier pins every
compiled source dependency and original input/output/report before and after,
records binary hashes, and checks emitted instructions for fused arithmetic.

This is the bounded finite numerical domain already enforced by its leaves.
The engine's host power function is verified on the recorded original cases;
it is not claimed equivalent for every possible finite input. Collision
queries, generation of contacts and controls, live callback capture, and a
consecutive gameplay loop are outside this draft. The public native ABI,
registered build, and Unity installation are unchanged.
