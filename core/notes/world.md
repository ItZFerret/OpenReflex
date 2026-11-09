# world: the physics world layer (group rx_world, work list plan/rx_world.tsv, 349 functions)

Hand-written C11 ports of the race frame's physics-world layer: the world update and its contact / collision passes, the
body records, the wreck checker / camera / crowd / collision audio share, the node hierarchy, the audio event layer, key maps
and input, water and track sampling, the geometric intersection kernels, the vehicle (RavenVehicleMotocross) per-frame
vfuncs and wheel blocks, and the rider animation's state machines. Every function is registered with `RX_FUNCTION` and has
an oracle adapter (`RX_PORT`, group "rx_world"); the prototype comments in the headers give each function's register /
stack mapping (`name=ECX`, `name=sN` = the N-th stack argument, `RET n`, `-> EAX / xmm0 / st0`).

## Build and verify

    RX_CORE_FILES="world_* phys_crt.c phys_misc.c phys_body.c crt_*.c build_alloc.c" native/game/tools/build_cli.sh <dir>
    native/game/tools/verify.sh <dir>/reflex_boot rx_world

The world files call the rx_phys runtime helpers (phys_crt.c: `_ftol2_sse`, SqrtF, the security cookie; phys_misc.c:
the pose / lag helpers; phys_body.c), the CRT API (crt.h) and build_alloc.h (ReflexAlignedAlloc), so a world-only build needs
those five units; `RX_CORE_FILES="world_*"` alone does not link. Functions of other subsystems that are not in that set are
called through the original (`WORLD_ORIG` / `WORLD_CTX` / the `*_extern.h` wrappers) until a full core build links them.
Use >= 1500 frames per world: the race starts around frame 1000 and most of the vehicle / rider code runs only after it.

## Files

| file | contents |
|---|---|
| world_util.h | shared helpers: frames at the original addresses (`world_frame_enter / _flat / _leave`, `WORLD_CALL_SP`), calls of originals with registers (`WORLD_ORIG`, `WORLD_CTX` + `WCTX(...)`), `world_x87_pass` (fld / fstp float copy), vector helpers, the world's object vectors |
| world.h, world_phys.c, world_body.c, world_extern.h | physics_update 00769b80, listeners, solver dispatch, dynamic / kinematic / rider collision passes (terrain, statics, keyed, water), integration, contact impulses, body records |
| world_wreck*.c, world_wreck.h | float listener / clamped / ranged / wrapped / short properties (0063bxxx), the terrain item grid (0066xxxx), WreckChecker (006373e0 ...), wreck camera (0064xxxx / 0065xxxx), crowd audio, clip recorder, collision audio (0067bxxx / 0068xxxx) |
| world_node.c / world_node.h | node hierarchy: Node_EvaluateWorld 007db890, Node_Invalidate, Set/Get world matrix / rotation / position |
| world_node_audio.c | FMOD event layer (0080xxxx), audio command queue 00805860 / 00805ff0, the game clock 00892170 |
| world_node_input.c | KeyMap / CompoundKeyMap / CumulativeKeyMap vfuncs, XenonInputManager queries, clip events 0083d3d0 |
| world_node_water.c | water patch / set heights 007e9dc0 / 007efef0, PackedGrid_SampleUV 007f8e00 |
| world_node_geom.c | line / cylinder / cone / box intersections 0081b6b0 .. 0081ecb0 |
| world_veh*.c, world_veh.h, world_veh_util.h | vehicle math (quaternion forms, matrices, Float_Fmod, the spline search 0041e080), physics entity glue, Raven virtuals, track marks 004607e0 / 00468120, wheel blocks 00468300, vf29 / vf35 / vf62 (world_veh_motocross.c) |
| world_rider*.c, world_rider.h, world_rider_util.h | key tracks, terrain track contacts, players, RiderAnimation updates (vf19 0061c210, locomotion 0061e3d0, state switcher 0061f160, gestures 0062axxx, look-at, end wreck 00623310) |
| world_misc.c, world_misc.h | small leaves: constant vfuncs, per-pad control maps (00548xxx), rider wreck entry points (0054exxx), activity / audio handlers, red-black tree helpers, the name registry 00593220, the handler list 00592d80, UI helpers |

## Conventions that matter for bit-exact results

- Memory only through rx_addr (F32 / U32 / SF32 / ...); no host pointers. Globals in .data (>= 0x9a6000) are read from
  memory; .rdata constants are literals with their address in a comment.
- Float arithmetic through phys_add / sub / mul / div (x86 NaN rule); vectors through phys_vec.h (v4_*); the build uses
  -ffp-contract=off. Comparisons follow the original's jump: `comiss a, b; jb` skips when a < b or unordered, so the C form
  of "not below" is `a >= b`; `ja` is `a > b`; `ucomiss; lahf; test ah, 0x44; jp` is "not exactly equal".
- Double-precision passages (cvtss2sd ... cvtsd2ss) are written in C doubles with every float rounding point where the
  original has it (vf29 / vf35 rotation `veh_rotate_d`, 0041e080). x87 passages use rx_x87.h at 53 bits (the main thread's
  control word 0x027f): Node / water cell indices (`fsub`, `fadd`, `fistp` with forced truncation), the spline segment
  rounding (`_copysign (0.5, d) + d`, `_ftol2_sse`).
- The inlined quaternion product (sign tables 0x93a1f0 / 0x93a200 / 0x93a210, conj 0x93a220) is `veh_quat_mul`; vector
  rotation `veh_rotate` (q (v, 0) conj q); the inverse rotation is `veh_quat_mul (conj q, veh_quat_mul (v, q))`.
- Stack: functions whose locals are passed to callees, or read later as stale stack, keep their frames at the original
  addresses (`world_frame_enter (size, pushes, align16)`); C-to-C calls set the callee's ESP as the original call would
  (`WORLD_CALL_SP (arg_bytes, ...)`). Never read an own argument slot (C callers do not fill it); write it where the original
  writes it and the value is read through a pointer (dt slot of 0061f160 for ClampedFloat_Add).
- Virtual calls read the target from the vtable and use the core function when registered (`RX_FN`, signature
  (this, stack args...)), else run the original. **The dispatch wrapper must match the signature**: `WORLD_CTX` /
  `WORLD_VCALL` only work for an integer-signature core function (`src/rx_mem.c`'s generated table); a slot whose registered
  core function takes a float, returns in xmm0 / st(0), or has a struct result needs its typed call at the site
  (`world_vcall_f` for (this, float) by vtable offset, `world_call_ctx_f` / `WORLD_CTX_F` for a call site that also carries
  the caller's registers). The 2026-10-20 dispatchfix case: the collision shapes' vf4 (`00848af0` composite / `00843fc0`
  sphere, `(this, float scale)`), dispatched with the item's radius from Grid_ReportOverlaps' layer objects -- the integer
  path aborted as "unported" on the *registered* port. Check both registered-slot and registered-callee sites when adding
  one: `grep -n "WORLD_CTX\|WORLD_VCALL" game/*.c` against `_re/out/vtables.tsv` (class, vtable, slot, target) plus
  `tools/gen_fnsig.py`'s integer-signature rule.

## Traps found

- **Dead stack in On mode.** Verify ignores the stack below the final ESP, but later original code reads uninitialized
  stack, so a port must leave the dead stack it writes where it matters. These were all found by On-mode memory diffs
  (REFLEX_MEMDUMP + native/game/tools/memdiff.py, REFLEX_WORLD_WATCH on the slot, bisecting frames and subsets of ports):
  - A particle emitter's stack-built 128-byte record (copied by 00692b60, fields +0x60..+0x7c left uninitialized) takes
    whatever lies there: the frame of 007f8e00 PackedGrid_SampleUV (its saved EBX / EBP / ESI / EDI, the fistp index, the
    floor argument and return address, floor's stmxcsr / fnstcw slots, the argument slots rewritten to hi / word), and the
    frame of 0080c280 (the saved EBP, the five Audio_PostCommand arguments and the return address 0x80c2b6). The caller's
    EBP is therefore threaded through the audio queue chain (0080a810 / 0080a9f0 / 0080b460 / 0080ae80 / 0080bc80 /
    0068c360 / 00681840 / 00581cb0: parameter `ebp`; an EBP-frame caller passes its own frame, 0080c760 passes sys).
  - The narrow phase reads its own stale stack: 0081c590's Newton step [S + 0x60] (below) is whatever 0081b6b0 /
    0081beb0 / ... left there, and later code takes a vector's w lane from 0081c590's slot S + 0x8c (the last Newton
    gradient term; NaN once a refinement diverges). 0081b6b0, 0081beb0 and 0081c590 therefore write their complete final
    frame image (all locals, saved registers, the last callee's argument / return address / saved EBP). With that
    SX_Springfield_ai11 and Terracross_TRX_Verde_ai11 run identical; without it the bike state diverged at frame 1305 (SX).
  - The same will hold for the product: a native caller stack that differs gives different physics where the original reads
    stale stack (0081c590's step). Frame images are the documented fix; the slots are listed in each function's comment.
  - **Node_EvaluateWorld 007db890's saved registers (w-parity4, 2026-10-31).** Its prologue (push ebp; and esp, -16; sub esp,
    0x144; push ebx / esi / edi) leaves the caller's EBP at entry ESP - 4 and EBX / ESI / EDI at the aligned frame's base
    `((entry - 4) & ~15) - 0x150` + 8 / + 4 / + 0; on the subtree path its 007deaa0 calls (at its frame base, return addresses
    007dbc6f, then 007dbdb5) push EBX = the first child record and ESI = the record, then the evaluated record's local matrix
    offset (+0xb * 64). The port wrote none of them (Verify with REFLEX_PORT_DEAD=1: every call differed, about 4 words) and the
    oracle in On mode (all groups) left the original at k 1074 of the paired Concord ride. The word that matters is the saved
    **EDI**: in one call (the 119,004th through the adapter, from FUN_007deb60's `call 0x7db890` at 007deb96 on the physics
    stack, ESP 0022f268; EDI = the node walk object 18fda9f0) it lands at 0022f110, which a later CylinderVsCylinder's
    0081c590 reads as its Newton step `[S + 0x60]` (the original: 18fda9f0 = 6.6e-24 as a float; the port's leftover: 0.1847).
    Fix: `rx_world_node_evaluate_world` makes the 007deaa0 calls at the original's ESP with those EBX / ESI
    (`rx_nodeset_next_unvisited_regs`), and the adapter writes the prologue's words (`rx_world_node_evaluate_world_saved_regs`,
    m.r[]); native callers do not model the registers (the product's path to that depth differs anyway). Found by
    bisecting the adapter's calls (a scratch adapter skipping the four words in a range of calls: one call alone reproduces
    k 1074, a second group 119,040..119,092 gives k 1075) and logging the Newton step reads at that address in both runs.
    Verify (`REFLEX_PORT_DEAD=1`, paired ride's oracle, 2,400 frames): 007db890 2,275,711 calls, 0 mismatches; 007deaa0
    10,712,632, 0. The On-mode oracle then matches the original to k 9278 (core/plan/QUEUE.md "## w-parity4").

- 0081c590 (disc rim vs cylinder): the Newton step factor is an uninitialised frame slot ([S + 0x60]) unless a step sees a
  zero determinant (then 1e7, sticky). The port reads the same game address, so it matches in Verify (the original's caller
  left that stack); a fully native product will see whatever its own stack holds there.
- 0041e080: the best sample's parameter starts uninitialised ([S + 0x34]) and is read when no sample beats 100.
- 00468300 reuses its parameter blocks: the slip-mark block (+0x1a0) becomes the TerrainTracks group block with fields left
  from the slip marks and the splash vector; every local is kept at its frame address so the stale fields match.
- 0062a490: the wobble 0062aa30 leaves |value| in xmm0, which the caller subtracts 0.1 from for the IK weight (else 0).
- 0061e3d0 / 0061c210 / 0061f160 pass dt through fld / fstp (`world_x87_pass`) to callees but store the raw value.
- 0054ed10 saves the caller's ESI (the wreck type in WreckChecker_Update).
- CompositeSetChildPosition / Rotation (00848800 / 00848910) take ESI = the composite.
- KeyMap "key" arguments are pad (player slot) indices; the device behind 0xa95844 is the XenonInputManager.
- 00814c00 / 00814e60 keep a compare that can never hold ((which | 0x8000) == 2); kept as written.
- Lead rules: no duplicate registrations across subsystems (00401700 belongs to rx_phys, 0078fbe0 / 0078fc50 to
  build_alloc).

## Dispatch gap: the collision shape vf4 (dispatchfix, 2026-10-20)

`00667e40 Grid_ReportOverlaps` brackets each terrain item's narrow phase with the layer object's vfuncs +0x10 (vf4, with
`world_x87_pass` of the item's radius) and +0x14 (vf5). The layer object is a `RavenPhysics::CollisionShape`; its vf4 is
`CollisionShapeComposite::vf4` `00848af0` (also static / dynamic composite) or `CollisionShapeSphere::vf4` `00843fc0`, both
**registered** ports whose C signature is `(rx_addr self, float scale)` -- alone among the shape vf4s (box / triangles /
heightfield / rays = shared `00679000`, a registered integer-signature empty virtual; cylinder `00844050` is not ported).
`world_call_ctx`'s product branch sends any registered target through `rx_call_original`, whose generated table
(`tools/gen_fnsig.py` -> `rx_mem.c`) holds integer signatures only, so the product aborted with "unported game function
00848af0" at the first tree/rock whose layer object was a composite (the 4-rider clean ride, race+427).

Fix: `world_call_ctx_f` / `WORLD_CTX_F` in world_util.h -- for `00848af0` / `00843fc0` it calls the registered port with its
own prototype at the original's stack depth (the argument written in the original's pushed slot at [esp], the stack pointer
lowered by argument + return address, as `world_vcall_f` does for (this, float)); every other target falls through to
`world_call_ctx` unchanged (00679000 stays on the integer path, an unported 00844050 still aborts by name). The oracle build
keeps `rx_call_original_ex` so the original runs and its adapter verifies the port. Call site: world_wreck.c
`rx_world_grid_report_overlaps` (passes `rx_bf(radius)`, the x87-quieted bits as a float).

Verify: no reference world reaches the port (`rx_phys` / `rx_world` show 0 calls for 00848af0 / 00667e40 -- the worlds' drive
never enters a composite terrain layer), so `core/oracle/phys_coll_probe.cpp` builds the shapes on the game heap and calls
the original through the installed port (group `rx_phys`):
`REFLEX_PORT=rx_phys,rx_PhysCollProbeHook REFLEX_PORT_VERIFY=1 RX_PHYSCOLL_PROBE=csp`: 00848af0 **96 calls / 96 verified /
0 mismatches** (0..3 children, ordinary and special scales, stub children and nested real spheres), 00843fc0 **16/16/0**.
Product: the 4-rider clean ride's 1689 data rows (the pre-fix run's whole log, through the old abort) are byte-identical to the pre-fix run and the
ride continues to race+432 (434 racing rows; the next step aborts), where it stops at the genuinely unported `00861a60 CollisionPair_SphereVsSphere`.

Scan for the same class (dispatch wrappers + registered non-integer signatures, 2026-10-20): every `WORLD_CTX` /
`WORLD_VCALL` / `WORLD_ORIG(_EX)` / `rx_call_original(_ex)` site in game/ was matched against the registered signatures
(gen_fnsig's integer rule) and `_re/out/vtables.tsv` (slot -> target per class). The registered non-integer targets reached
this way are only these shape vf4s; the other known float/vtable dispatches are already typed at their sites:
world_misc.c `km_value`, world_node_input.c `sub_keymap_value` (both `RX_FN(float (*)(rx_addr,uint32_t,uint32_t))` first),
world_wreck_checker.c `vehicle_rate` (00466b70 / 00466bd0 explicit, then a typed pointer), world_vcall_f /
world_phys.c:89 (sim vf1), world_veh_motocross.c vf60 / vf35 / RiderAnimation vf19, and build_activity_game.c's fx tick
calls (oracle-only, the product calls the port directly). Constant-address wrapper sites with a non-integer port
(build_activity_game.c:65 `006de690`, :498 `00691bd0`) sit under `#ifdef RX_ORACLE`. The next ride stops are unported, not
dispatch gaps: `005278e0` (BehaviorNavigateTrack mode 1), `00592b60` (stunt audio), `00861a60` (sphere-sphere pair).
Residual risk: `rx_vcall` / `rx_tcall` (rx_vcall.h) still *silently* calls any registered target through an integer
prototype -- a float-arg or float-returning virtual added to a slot used by `WORLD_VCALL` would compute garbage instead of
aborting; use `world_vcall_f` / `world_call_ctx_f` for those.

Empirical check of the ride path (builds/dispatchfix/scan, a logging copy of src/rx_mem.c, same behaviour otherwise): every
`rx_call_original` / `rx_call_original_ex` the product makes on the 12/W, 4/W and 4/clean routes is logged. 434,898
dispatches, every one with a registered integer-signature target (`reg=1 int=1`); **0 registered non-integer dispatches**,
i.e. no other site of this class on these paths. (The last, unported stop -- `00861a60` on 4/clean, `005278e0` on the W
routes -- is the ride's own error line; its log line is lost to the abort's interleaved stderr writes.) Tool: `builds/dispatchfix/scan_dispatch.py` (the static list) and `dispatch_scan_summary.txt` (the counts).

## Status

All 349 functions of plan/rx_world.tsv are ported (00401700, 0078fbe0 and 0078fc50 are registered by their owners; the world
code uses phys_vec3_sub's arithmetic locally and build_alloc's rx_reflex_aligned_alloc / rx_reflex_aligned_free). Verify per function (layered by name,
five worlds, verify.sh frame counts): 0 mismatches for every function called; functions never called in the five worlds are
listed in the agent report.
