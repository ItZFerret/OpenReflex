# Group "anim": character and object animation, node evaluators

Ranges: 0x00830000..0x0083ffff (Character_SampleKeyframes, Blend / Additive / Motion / Switch / Performance animators,
CharacterEntity, CharacterAnim_SetAnimation), 0x006a0000..0x006bffff (Expression / Featherstone / InverseKinematics /
Procedural animators, AnimEntity* jobs, ClipRecorder, DropGate, ThirtySecondBoard, CrewChief), 0x007d0000..0x007dffff (node
helpers, SceneManager tick; not Node_EvaluateWorld and the other node functions of physics-functions.tsv, which are the world
group's), 0x00610000..0x0062ffff (RiderCharacter, RiderAnimation jobs, AnimationEntity), 0x00740000..0x0074ffff
(ScenarioAnimEntity, race / free-ride intro scenarios, Rider_SetStartLineAnimation).

Status 2026-10-18: 183 of the 191 functions of `plan/nonphysics.tsv` in these ranges are ported (7,486,138 of 7,548,949
original instructions per frame by the plan), plus 12 functions of the ranges that run but are not in the plan (see below): 195 ports.
`tools/verify.sh <boot> anim` and `math,anim`: 0 mismatches and "on: identical" on all five worlds.

## How the ports are made: the anim lifter

Every port is an instruction-level transliteration generated from the disassembly (the bike group's lifter, copied and
extended: `scratchpad/anim/lift/lift.py`, `gen_all.py`, `abi.py`; runtime header `ports/anim_util.h` = bike_util.h in namespace
`animl` plus the additions below). Output: `ports/anim_<range>.cpp` (anim_610, 620, 6a0, 6b0, 740, 7d0, 830). Registers are C++
locals, memory and the whole stack frame are guest memory, calls go through `Machine::call_function` with the registers synced
(`ANIM_CALL`, the original's return address written back into the dead slot), flags only where a consumer reads them, x87 on the
Machine's stack with reflex_f80, all registers reproduced and compared (`ANIM_OUTS` = EAX ECX EDX, all xmm, x87).
See `notes/bike.md` for the lifter's design.

Regenerating: `_re/.venv/bin/python scratchpad/anim/lift/gen_all.py` (reads `scratchpad/anim/list.tsv` minus `lift/skip.txt`,
names / comments from `lift/names.tsv`). Unsupported forms stop it with the address.

Additions to the bike lifter (all as vm.cpp executes them):
- `movq` (xmm <- m64 / xmm zeroes the upper half; m64 <- xmm low), `rsqrtps/ss`, `rcpps/ss` (the reference's exact tables,
  `rsqrt_rcp_tables.inc`), `div` / `idiv` r/m32 (faults on zero / overflow as the interpreter), `mul` r/m32 with its OF/CF
  (operator new[]'s overflow check `mul; seto`), `rep movsb` / `rep stosb`.
- `sbb` / `adc` whose own flags are read (the strcmp tail `sbb eax, eax; sbb eax, -1`): captured operands and carry, conditions
  computed from them.
- String ops (`rep movsd` ...) are flag-neutral when resolving a consumer's flag definition (00623f30: `comiss; rep movsd; jb`).
- Indirect `jmp reg` / `jmp [mem]` without an index: a tail call (`ANIM_TAILJMP`), e.g. FeatherstoneAnimator::vf4 006afa50.
- Two-level jump tables (`movzx eax, byte [eax + T8]; jmp [eax*4 + T32]`; bound from the `cmp` before the byte load).
- `fs:` operands (C++ EH frames): address FS_BASE_ (0x7ffd0000, the TEB as the runtime maps it, same base on worker threads).

## min / max NaN rule

minps / maxps / minss / maxss (and sd / pd) return the source operand when either operand is NaN, unquieted (Intel SDM). The
reference interpreter had the quieted-first-NaN rule until 2026-10-18; the lead fixed vm.cpp and patched anim_util.h's
sse32 / sse64 to match (do not regenerate anim_util.h from bike_util.h without that change).

## Not ported (left original) and why

| Function | Why |
|---|---|
| 0061cdb0 RiderAnimation_WorkerThread, 007d22a0 Scene_TickerThread | Thread entry points. The runtime runs a worker by `m.run(proc)` from SetEvent; its WaitForSingleObject on the "go" event returns pc 0 to end that run (the thread blocks). A native would continue past the wait instead. Everything they call is ported. |
| 007d2470 "Scene_UpdateEntities" | Not a function: the tail of a loop inside another function (falls through from 007d246f, no callers). A native there would be entered mid-function. |
| 006ba250 AnimEntitySingleAnimLoop::vf19, 0074ab50 ScenarioAnimEntity::vf19, 006bb340 AnimEntitySplineAnimLoop::vf19, 006bd6e0 CrewChief::vf19, 006bc040 AnimEntitySplineAnimLoopWithParticles::vf19 | The runtime's `World::set_ambient_animation` (runtime.cpp) installs its own natives at these addresses after the ports (the ambient skip, RUN_ORIGINAL for kept classes) and erases them when simulating, so a port there is never installed. Their jobs (006ba170, 0074ab10, 006bd6a0) and everything below are ported. If the ambient skip chained to an installed port instead of RUN_ORIGINAL these would work as lifted. |

## Functions ported that are not in the plan

The plan attributes code by Ghidra function starts; these run in the ranges (coverage) but have no plan row of their own:
0074ab10 ScenarioAnimEntity_Job (Ghidra merges it into 0074aab0: the 4878 instructions per frame of "0074aab0" are mostly this
job), 006bd6a0 CrewChief_Job, 006bd760 CrewChief_SetShirt, RaceIntroScenario::vf1 / vf3 (00749df0, 007497b0) and its helpers
007496d0 00749750 00749910 00749b10, 00743f00, 00835170, 0074d6c0. Left original (0 instructions per frame, not in the plan):
00749ba0 and 007498a0 (a jump table indexed by the stack argument with no bound check: the table size cannot be read from the
code), 00749600 (an 8/16-bit shift the lifter does not support).

## Verification notes

- Verify only counts a port when it is the outermost port of a call: ports nested inside another port's native run are checked
  as part of it (port.cpp phases). With the whole group selected most of the rider animation is verified under
  RiderAnimation_KickJobs (the worker job runs inside its SetEvent). Each of the 40 most expensive functions was also verified
  alone (filter = that address) on the world where it runs: 0 mismatches (e.g. Node_SetLocalRotation 4.6M calls on Concord 12,
  Character_SampleKeyframes 133K, AdditiveAnimator::vf4 on SX / Alpine / Verde, 008363b0 177K calls on SX, 0062d970 on Verde).
- Every ported function ran at least once (On mode) in the five worlds; untaken paths inside them are unverified (as with any
  lifted port).
- Allocating ports (operator new / HeapAlloc in the call tree: 007473f0, 0074cf70, 0074aab0, 00747720, 0074bd50, ...) verify since
  the lead's host-state snapshot (2026-10-18); before it, Verify reported heap-address differences and stopped with "HeapFree of
  an unknown block".

## Names (`_re/names/9e_anim.tsv`)

007dcdf0 Node_SetLocalRotation (ESI = record, arg 0 = quaternion; RET 4: the 1.9M-instruction hot spot, called ~16K times per
frame per world by the keyframe samplers), 007deaa0 NodeSet_NextUnvisited (EDI = set; depth-first cursor), 007dd930
NodeSet_CopyFrom (ESI = destination, EDI = source), 007d23a0 SceneManager_Tick, 007d1cd0 Scene_ProfileScope, 0074ab10
ScenarioAnimEntity_Job.

## Conventions seen

- Many helpers here use LTCG register conventions: Node_SetLocalRotation (ESI record), NodeSet_NextUnvisited (EDI set),
  NodeSet_CopyFrom (ESI / EDI), 007dd380 (EAX record, ECX), 00834390 (EAX animator, EDI pose buffer). Each port's header
  comment lists the live-in registers (`abi.py`).
- The animators' vf4 (Blend / Additive / Motion / Switch / Performance / IK / Procedural / Expression) is thiscall (pose
  buffer, root delta out, ...; RET 0xc) and recurses into child animators through their vtables (+0x10).
- Pose buffers: 0x30-byte bone records (quaternion, translation, two "valid" bytes at +0x20 / +0x21), released through the
  pose pool at +0x208 (critical section) / +0x228 / +0x22c of the character.
