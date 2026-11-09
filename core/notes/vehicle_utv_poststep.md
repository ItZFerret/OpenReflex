# UTV poststep closure

`004c6e80` V61 calls `004d8160` only when signed frame counter ad3ed4 % 3 is
1 or 2. Its single ESI push and the callee's aligned 0x130 frame are retained.
The callee takes simulation in ESI, not ECX. Group: `rx_UtvPoststep`.

Sources: `_re/out/decomp/reflex_002_004b32a0.c`, functions 004c6e80,
004d7fa0 and 004d8160; full disassembly audited through 004d9455.

The apparent 4,852-byte simulation function is release-build debug geometry.
Every memory write in 004d8204..004d917e and 004d9195..004d944d targets ESP.
The only non-stack store in the entire function is `004d9185`, S+8020=0.
Node callbacks at the start remain real simulation work and run on all nodes,
including the root. The four wheel-offset helper calls also remain.

Geometry retained as readable quaternion evaluations: all four pivot/wheel/
contact endpoint triples, mirrored right pivots and subtracted right wheel
radius, optional flags at S+90/+8020, every contact debug endpoint pair, and
sixteen final linkage vectors S+8750..8840. Original local endpoint stores at
ESP+70/+80 are retained; overwritten arithmetic spills remain ordinary C
locals, following core/README.md's memory model. No debug-renderer stand-in
or product fallback is introduced. The original's debug renderer was already
absent; these evaluations only overwrite local vectors.

Shared authoritative helpers are owned by `vehicle_poststep_helpers.h`:
0085c6a0 node slot4 loop, 004d7fa0 wheel offset debug evaluator. Direct calls
use their declared signatures and original argument-stack widths.

Adapter declares the simulation helper's final XMM0 (the unmodified vector
S+8840). Other volatile register outputs are dead at natural callers.
Validation at handoff: C11/C++17 syntax passes; lead owns heavy build and
oracle/native verification.


## Lead verification, 2026-10-21

Accepted reference evidence and current native boundary are recorded in `HANDOFF/SESSION_2026-10-21_VEHICLE_POSES.md` and `builds/sol_vehicle_pose`. The 600-frame UTV/SuperBuggy/ProLite/Pro2 comparisons match, including whole-memory hashes. Restored probes cover articulation and post-step paths absent from natural recordings; the fixed-body node is verified separately on the real MX rider node. Native playability is a separate acceptance and is not implied by these method comparisons.
