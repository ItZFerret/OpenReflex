# rx_anim: rider / character animation in the OPEN REFLEX core

Work list `core/plan/rx_anim.tsv` (183 functions). Prefix `anim_`, oracle group `rx_anim`. Rules: `core/README.md`.

## Files

| File | What |
|---|---|
| `game/anim.h` | declarations of everything in `anim_*.c` (one `rx_<name>` per original function, address in the comment) |
| `game/anim_extern.h` | wrappers for the callees the animation code does not own (rx_call_original until ported), shared helpers (frames, x87 copy, animator dispatch) |
| `game/anim_node.c` | node hierarchy helpers 0x007dxxxx |
| `game/anim_animators.c` | animators 0x0083xxxx (Blend / Additive / Motion / Switch / Performance) |
| `game/anim_character.c` | character animation state 0x00834-0x0083a (keyframe sampling, motion slots, blends, transitions) |
| `game/anim_scene.c` | SceneManager ticks, entity lists / lookups, event grid, lights, model instance teardown 0x007dxxxx |
| `game/anim_rider_a.c`, `anim_rider_a_ik.c`, `anim_rider_a_math.h` | rider head / noise wobble / nod, lean, limb aiming (00623bd0..00629a60, 006259c0, 00626740) |
| `game/anim_rider_b.c` | rider limb states 0062aed0 and their modes: reach 0062d970, rest 0062fa90, swing blends, yaw, targets |
| `game/anim_ik.c`, `anim_ik_anim.c`, `anim_ik_ent.c` | IK animator + CCD / two-bone solvers, Procedural / Expression / Featherstone animators, spline / loop / board / gate / leader-board entities, the alias table 006b6a30 |
| `game/anim_scenario_rider.c` | CharacterEntity / RiderCharacter virtuals, rider animation worker kick / wait (+ the product's worker pass), RA state resets, rider std::list helpers |
| `game/anim_scenario_clip.c` | the replay clip recorder 0x006aa-0x006ac |
| `game/anim_scenario_intro.c` | scenario animation manager (SM), the intro scripts (XML events), intro / free-ride intro scenarios 0x00747-0x0074d |
| `game/anim_math.h` | the inlined math idioms: sign, range reduction, sin / cos polynomials, packed quaternion keys, slerp weights, acos, sincos |
| `oracle/anim_<file>.cpp` | one adapter file per game file (`ANIM_PORT`, oracle/anim_oracle.h) |
| `oracle/anim_oracle.h/.cpp` | `ANIM_PORT` = RX_PORT that binds the machine for `anim_reg_get / anim_reg_set` |

## Conventions (how the code is written)

- Each function: a hand transliteration from the disassembly (`_re/tools/disasm.py <addr> <n>`), the decompile only as a guide
  (`_re/tools/fn.py <addr>`: its comments tagged [E] / [K] come from earlier agents and are mostly right). Float operation order
  and association exactly as the disassembly (SSE single precision = C float; `cvtps2pd ... mulsd` = C double).
- Callees not in the core: a wrapper in `anim_extern.h` (shared) or a `static` wrapper in the file that uses it, named after what
  the callee does, with its register convention (`rx_call_original_ex` with `anim_regs()`, fields `r[ANIM_EAX..ANIM_EDI]`, `xmm[i]`).
  Trivial exact leaves are written inline: SqrtF 0042c230 = `sqrtf`, Float_Abs 00401000 / Float_Abs_Thunk 00401040 = `fabsf`.
- Locals the original passes by address live on the game stack in a frame laid out like the original's:
  `ANIM_FRAME_ENTER(A, below)` (A = the original's ESP after `and esp, -16`, stack pointer A - below = its ESP at the calls),
  locals at `A - k`, `ANIM_FRAME_LEAVE(A)` before returning. Then callees run on the original's stack addresses (dead-stack safe).
- Child animators: `anim_animator_eval(animator, ch, delta, pose, ret)` and `anim_animator_get_param(animator, name, out, ret)`
  (core function if registered, else the original): the argument pushes and the call site's return address `ret` are written
  to the game stack either way (`ANIM_PUSH`), so the child's frame lands where the original's does and the dead stack holds
  what the original's call leaves (an argument slot of PerformanceAnimator's child call is read back as a dead lane later).
- The stack pointer at every call must be the original's (`ANIM_CALL_FRAME(arg bytes)` around direct calls between core
  functions; frames with `ANIM_FRAME_ENTER(A, below)` where `below` = the original's ESP before it pushes the first argument).
- Registers: `anim_reg_get / anim_reg_set` (oracle: the emulated CPU, bound by ANIM_PORT; product: a file of its own).
  ANIM_FRAME_ENTER / ANIM_FRAME8_ENTER push the caller's EBP at sp0 - 4 and set EBP = sp0 - 4 (`push ebp; mov ebp, esp`),
  ANIM_FRAME_LEAVE restores it; `anim_regs()` starts from the current EBX / EBP / ESI / EDI (EAX / ECX / EDX zero), so originals
  called through rx_call_original_ex save the same register values on the stack as under the original caller.
- Where an original's dead stack is read back later (see Traps), the call sites also write what the original leaves: the
  call's return address (`anim_ret_slot`), the pushed arguments, and the callee-saved registers the callee saves (set them to
  the original's values at that call: e.g. `anim_node_invalidate_from`, anim_character.c `node_set_rotation_at`).
- Imports: `anim_enter_cs` / `anim_leave_cs` (KERNEL32 Enter/LeaveCriticalSection through the IAT 0x91600c / 0x9160f8).
- x87 `fld m32; fstp m32` copies: `anim_x87_restore_f32` (SNaN -> QNaN).
- NaN rule: plain C float differs from SSE only for NaNs; where a NaN can arise and be stored use rx_adds / rx_subs / rx_muls /
  rx_divs / rx_sqrts (src/rx_x86.h). x87 code with double constants or transcendentals: src/rx_x87.h.

## Adapters

`RX_PORT(addr, Name, "rx_anim", outputs)`: arguments from `m.r[]` (EAX ECX EDX EBX ESP EBP ESI EDI = 0..7), `m.arg(i)`, xmm lanes
(`rx_oracle::f32(uint32_t(m.xmm[i].lo))`), result registers, `return m.ret(cleanup)`. Callee-saved registers are restored by
RX_PORT. Declare as outputs only what callers read (a void thiscall vf4 declares 0). Float results returned on the x87 stack:
`m.fpu.push_back(reflex_f80::from_f32(bits))` (the x87 depth is always compared).

## Verifying

- Build: `RX_CORE_FILES="anim_* crt_* loop_* phys_* world_* race_* bike_* build_alloc*" native/game/tools/build_cli.sh <dir>`
  (the CRT, the loop platform layer and rx_world's Node_EvaluateWorld are linked in; the product's NodeSet_Init is build_model.c's
  rx_nodeset_clone, the oracle keeps the original there). A shared header change rebuilds everything: ~5 min.
- Quick, one world, Verify only: `scratchpad/rx_anim/v1.sh <reflex_boot> <filter> [world] [frames] [drive]` (filter: `rx_anim`
  or port names `rx_Character_SampleKeyframes,...` -- not hex addresses: a hex filter also installs the lifted `Fn_` port of
  the same address and the per-address statistics are shared; worlds National_NAT_Concord:1, National_NAT_Concord_ai11:2, FreeRide_FR_Alpine:3,
  SX_Springfield_ai11:4, Terracross_TRX_Verde_ai11:5). 300 frames of Concord take ~10 s.
- Verify counts a port only when it is the outermost port of a call; a nested one is checked as part of its caller. Verify an
  inner function alone with a filter of its name. `scratchpad/rx_anim/lay.sh <boot> <names> <world> <frames> <drive>` runs the
  layers: each run drops the ports that were called, so the nested ones get checked alone in the next.
- A port nested under another (KickJobs -> the rider animation worker -> animators; SceneManager_Tick -> entity ticks ->
  character sampling) is compared as a whole, including what later original code reads from the dead stack the nested natives
  left: bisect with `<outer>,<group>` filters (one adapter file's names at a time) and `REFLEX_WORLD_WATCH=<addr>:<file>` (every
  interpreted write covering addr, with its pc; the core's own writes are not logged).
- Acceptance: `native/game/tools/verify.sh <reflex_boot> rx_anim` (five worlds, Verify + On). Slow (~1 h).
- At most 2 emulator runs at a time for the whole rx_anim team: v1.sh and `scratchpad/rx_anim/emu.sh` (verify.sh) take a slot.

## Memory layouts found

### Rider limb state L (ProceduralAnimator +0x5c / +0x60, 0062aed0)
+0x00 mode (1 reach 0062d970, 2 rest 0062fa90, 3 / 4 swing blends 0062c840 / 0062b710, 5 / 6 yaw 0062b3f0 / 0062b0e0), +0x04
side (1 = right), +0x10 bone translation result, +0x20 / +0x24 / +0x28 the three bones (hand or foot, elbow or knee, shoulder or
hip), +0x2c sub-mode, +0x30 target (swing 0062ab30 / rest 0062ae60), +0x40 weight w, +0x4c yaw sign, +0xb8 swing blend weight,
+0xbc rider animation (+0x1b4 rider: +0x144 bone controller argument, +0x2f4 a limit RangedFloat, +0x5b578 frozen), +0xc4 timer
step, +0xc8 / +0xcc reloads, +0xd0 timer, +0xd4 / +0xd5 contact now / before, +0xe0 contact point.
Reach / rest: the bones' model positions (00632b00); bone 1 turns by its angle off the x (or y) axis, bones 2 / 3 about x cross
bone 3's position; acos and sin / cos inlined (anim_math.h `anim_acos_approx`, `anim_sincos`), rotation products of 4x4
matrices with their constant 0 / 1 entries multiplied out (the sums k = 0..3 in order), Matrix3_ToQuaternionToEsi, then bone x q
in double precision, renormalized (a zero length gives zero). 0062fa90 spans 0x62fa90..0x632a9d (do not register 0x630065).

### Scenario animation manager SM (level world +0x20150) and the intro scenario
SM: +0x40 activity, +0x50 12 cast entries of 0x30 (+0 scenario entity, +4 rider), +0x290 rider camera, +0x294 actors (count
+0x744), +0x748 props (+0x16e8), +0x16ec cameras (+0x172c), +0x1730 active camera, +0x1734 12 attachments (entity, owner).
Intro scenario: +0x10 clock, +0x14 activity, +0x20 script: +0x15180 event count (max 300), +0x15184 duration, +0x15188 clock,
+0x1518c handler flag; events of 0x120 (+0 type 0..23, +4 time, +8 / +0x48 / +0x88 names, +0xc8 int, +0xd0 matrix, +0x110 int,
+0x114 float), handlers 0x9c5120. +0x151b4 state, +0x151b8 fade pending, +0x151bc fade lead, +0x151c0 chosen rider, +0x151c4 alias
to avoid. Script name: `<series>_<track>_<kind>` (00747940: series by the event type +0x23c, track table by level name).

### Rider animation (RA, the worker's object) and its worker
Workers: 0xa94080 jobs in flight, 0xa9408c riders, 0xa940bc go events, 0xaf6b50 exit, 0xaf6b80 exited, lock 0xaf6c98.
RA (from rider_a): +0x144 RiderCharacter, +0xaee4 head bone, +0xaeec wobble node, +0x5b300 11 wobble records (0x30), +0x5b510 nod
node, +0x5b514 nod clock (+= +0x5b538, nod at +0x5b518), +0x5bb30 wobble amount, +0x5bb5c job pending, +0x5b1fc posed.
Lean controller (ProceduralAnimator +0x58) and limb aiming controller (+0x54): see game/anim_rider_a*.c comments.

### Node hierarchy (world-owned, used here)
Node record 16 bytes: +0x00 hierarchy, +0x0b local matrix index, +0x0c flags (bit 0 world valid), +0x0d / +0x0e / +0x0f
tree links (0xff none). Hierarchy: +0x08 records, +0x18 local matrices (0x40 each: rows x y z, translation row +0x30), +0x34 dirty.

### Character (pose owner; RiderCharacter / CharacterEntity)
+0x208 CRITICAL_SECTION of the pose pool, +0x228 pool poses, +0x22c pool used flags (dword each), +0x230 pool size, +0x234 bone count.

### Pose
bone count x 0x30-byte records: quaternion +0x00 (x y z w), translation +0x10 (x y z), +0x20 rotation valid, +0x21 translation
valid (bytes). Reset (0082e250): identity (0x9cb550), translation = the 0xa95d50 vector (zero), valid 0.

### Animators (vtable slot 4 = evaluate)
- BlendAnimator: +0x30 / +0x40 root deltas of the two children, +0x50 / +0x54 their pool poses, +0x58 / +0x5c children,
  +0x68 weight (< 1e-7: first child only, > 0.9999999: second only, 00834390).
- AdditiveAnimator: +0x2c base child, +0x30 additive child; result rotation = base * additive (double precision product),
  translation = base + additive.

## Traps

- Dead stack is part of the game state when original code reads it back: a 4-lane vector whose lane 3 was never written (the
  limb aiming 006259c0 stores dir.w, from the stale +0x1c of a local bone record, to +0x7c: it holds the EBP some earlier callee
  saved there); particle records copied from uninitialized fields of a stack template (they hold Node_Invalidate's saved EBX and
  return address, and Node_SetLocalRotation's matrix local; on Verde the clip recorder's memcpy frames: ClipRecorder_Record ->
  CommitCandidate -> SaveClip -> CopySamples -> memcpy, each frame at the original's depth with its saved registers, arguments
  and return addresses, crt_memcpy's own frame from crt_caller_regs set to CopySamples' EBX / EBP / ESI / EDI). Hence: frames at the original's addresses, the saved EBP, locals
  stored like the original's, call-site return addresses and callee-saved registers where it matters (see Conventions).
- x87 precision: the animation worker threads run at 53-bit (control word 0x027f); x87 arithmetic through rx_x87.h at that
  precision; `fld m32; fstp m32` copies quiet a signalling NaN (`anim_x87_copy_f32` / `anim_x87_restore_f32`).
- LLVM folds `x * -1.0f` into a negation: use `rx_muls(x, -1.0f)` where the original multiplies (NaN sign).
- Callers' register arguments of LTCG functions (EAX / ECX / EDX / ESI / EDI) are in each function's comment; some functions
  pass a caller's register straight through (0074be10: EDX to 006b6a30).
- Node_SetLocalRotation (007dcdf0) keeps the old translation and restores it after Node_Invalidate ran on the full matrix.
- The product build runs the rider animation worker's pass in place (anim_scenario_rider.c `rider_animation_worker_pass`
  through loop_wake_worker); the oracle keeps SetEvent and the emulated worker thread.

- Boot-only paths (verify with a baton boot: `REFLEX_BOOT_SCHED=baton REFLEX_EVENT=1 REFLEX_RIDERS=12 REFLEX_AI=95`, Verify with
  `REFLEX_PORT_VERIFY=1`, On by comparing the `REFLEX_WORLD_OUT` images; scratchpad/rx_anim/heapdiff.py leaves the thread stacks
  out): Lights_Add 007dfdd0 asked the light id again with the two float arguments already stored below the stack pointer, so the
  call's return address overwrote one (fixed: the call runs below them, as the original's `sub esp, 8` does); light event records
  copy a dead word of 007da4d0 / 007da5a0's frame, i.e. what NodeSet_CopyFrom's memcpy calls (their size argument),
  Node_UpdateSubtreeBounds' locals and its NodeSet_NextUnvisited calls left there; all now written as the original's.
  In baton boots the worker threads' stacks are ordinary memory (not the world test's 0x7fbc0000 range).
- Matrix3_ToQuaternion 00401210 (rx_world) writes its stack cookie: 006b0530 reads that slot back as a stale w lane.

## Status

All 183 functions of plan/rx_anim.tsv are transliterated, registered and have adapters (plus 0074ab10, the job merged into
0074aab0 by the plan). Acceptance (native/game/tools/verify.sh rx_anim, build `RX_CORE_FILES="anim_* crt_* loop_platform*"`):
Verify 0 mismatches and On identical on all five worlds; core/build.sh (product) builds clean. Layered per-port verification
(lay.sh, port names, five worlds): 155 of the 177 adapters showed calls, all 0 mismatches. Not seen in that combined run:
CharacterEntity_vf17, the event grid A-list functions, Lights_Add, Octree_ReplaceItem, the RiderList
helpers, SceneManager_AddEntity / Link (never called in the five worlds), and Character_SampleBlend3 (nested under SceneManager_Tick on SX), FreerideIntroScenario vf1 / vf3 / vf6,
Rider_SetStartLineAnimation, LeaderBoard_* (called, verified 0 mismatches in their own filtered runs: blend3 / intro on SX / Alpine,
leader board on SX).
