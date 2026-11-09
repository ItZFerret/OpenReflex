# Group "engine": engine core and services (wave 2)

Range (functions of `plan/nonphysics.tsv` only): 0x00820000..0x0082ffff, 0x00840000..0x0084ffff, 0x00880000..0x0088ffff,
0x00780000..0x0078ffff, 0x00700000..0x0073ffff, 0x00760000..0x0077ffff, 0x00790000..0x007bffff, 0x007e0000..0x007effff,
0x00660000..0x0066ffff, 0x006e0000..0x006fffff, 0x004c0000..0x004fffff, 0x008b0000..0x008effff.

Status 2026-10-18: all 220 functions of the plan in these ranges are ported (6,928,769 original instructions per frame by the plan,
summed over the five worlds). Files: `ports/engine_anim.cpp` (0x0082xxxx), `ports/engine_core.cpp` (0x0084xxxx, 0x0088xxxx),
`ports/engine_misc.cpp` (the rest), helpers `ports/engine_util.h`.

## What is hot in 0x0082xxxx / 0x0084xxxx

Not the Registry / XMLReader / DevConsole classes: the skeleton animation core (88% of the group's cost, 6 functions):

| Function | What | Plan instr/frame |
|---|---|---|
| 0082efb0 SkeletonPose_BoneToModel | a bone's rotation / translation composed up the parent chain (skeleton+0x23c parent indices) | 1,583,467 |
| 00840040 | MotionAnimator sampling (keys located with 0082b930, quaternions slerped with 0042ca00) | 1,548,729 |
| 0082e250 SkeletonPose_Reset | EAX skeleton, EDX pose: identity rotation, zero translation, two flag bytes per 0x30-byte bone | 1,262,708 |
| 0082b9e0 AnimKeys_SampleRotation | two int16x4 key quaternions (x 1/32767), slerp by the track fraction, out ESI | 905,312 |
| 0082e610 | bone controller evaluation (callers: ExpressionBoneAnimator controllers, RuleTag::vf5, ...) | 547,237 |
| 0082bac0 AnimKeys_SampleTranslation | static translation or lerp of the two keys | 270,138 |

Most of it runs inside the rider animation jobs (RiderAnimation_WorkerThread -> 0082ff20) and the render-sync / scene workers.
0x00783xxx..0x00784xxx is the exe's Lua 5.1 garbage collector (luaC_step 00784da0, singlestep 00784ca0, propagatemark, traversetable,
reallymarkobject, sweeplist, markroot, atomic; names in `_re/names/9e_engine.tsv`).

## How the ports are made

The bike group's lifter (instruction-level transliteration, see `notes/bike.md`), copied and extended in
`scratchpad/engine/lift/` (`lift.py`, `abi.py`; list `engine_list.tsv` = addr, file, name, description; `gen.py` writes the three
port files; `try.py <addr>...` checks what lifts). Regenerate: `_re/.venv/bin/python scratchpad/engine/lift/gen.py`.
`engine_util.h` is a copy of `bike_util.h` (namespace `eng`, macros `ENG_*`), so the two groups do not share a header.

Extensions over the bike lifter:

- `movq`, `mul` / `div` / `idiv` r/m32 (divide errors throw), `lock cmpxchg` r/m32 (flags of `cmp eax, [m]`), `sbb r8, r8`.
- `fs:` operands: the TEB at 0x7ffd0000 (vm.cpp FS_BASE, the same address on every emulated thread: SEH prologues).
- Jump tables without a bound check (the compiler knew the range): the entries up to the first that does not point into the
  function's neighbourhood (00779440, 004f6740, NetworkStats::vf2). Two-level tables (`movzx idx, byte [idx + T]`, then
  `jmp [idx * 4 + D]`): the dword table's size is the largest byte entry + 1 (PCSession::vf12).
- `jmp dword ptr [abs]` (import thunks, IAT tail calls): `ENG_TAILJMP`.
- Worker thread procedures (`BLOCKING` in lift.py: JobPool_WorkerThread 00881900, Engine_TaskThread 0084f320): every call is
  `ENG_CALLB`. The world runtime ends a worker's run when it waits on its "go" event: the WaitForSingleObject stand-in returns pc 0
  without popping the call. `ENG_CALLB` sees that (ESP still at the return slot), writes the original's return address into the
  slot and returns 0, so the thread stops exactly where the interpreted original stops (same ESP, registers and stack).
- Runtime patches inside a function (`HOOKS` in lift.py): the world runtime installs a native at 0x007e1082, inside
  Render_SyncRenderData 007e0fc0 (set_ambient_animation: the render-sync worker phase is skipped and its end state written). The
  port looks the address up in `m.natives` at that instruction and, like the interpreter, runs it and resumes at the pc it returns
  (0x007e1101); without the patch (REFLEX_KEEP_RENDER_SYNC=1) the worker phase runs. Both verified. When the emulator is gone this
  check goes with it. No other runtime hook (runtime.cpp, pages.cpp, modes.cpp, tutorial.cpp, plugin_api.cpp, boot*.cpp) lies inside
  a function of the group (scratchpad/engine/lift/hooks.py).

## Verification

- Verify nests: a port called inside another verified port runs as a native and is not counted on its own. Engine::Update 0084ed20
  is the whole frame, so verifying it verifies (almost) everything through it, one call per frame. The inner ports were also
  verified on their own on the five worlds by excluding the outer ones (`scratchpad/engine/inner.sh <boot> <filter>`).
- **Group "engine_mt"** (5 functions): the ports that run emulated worker threads inside them: Engine::Update 0084ed20,
  MXWorld::vf5 007674b0 (rider animation kick / wait), JobPool_Submit 00881f00 and JobPool_WaitIdle 00881e20 (SetEvent runs the pool
  workers), Render_SyncRenderData 007e0fc0 (the render-sync workers when REFLEX_KEEP_RENDER_SYNC=1). They are exact: `verify.sh
  <boot> engine,engine_mt` passes. But together with ports that do not reproduce the original's dead stack (math), Verify reports
  differences in the worker stacks (0x7fbc0000..0x7fc00000): Verify's memory compare leaves out only the dead stack below the
  verified call's own ESP, while the worker stacks a host port's call runs on are dead after the worker blocks and are compared in
  full. Example (math,engine with Engine::Update): `[7fbffdd4] 11c4d890 orig 7fbffdf8`, and on Alpine 0x7fbefd20.. zeros vs locals.
  On mode is identical (the memory hash leaves the stacks out). A Verify change (leave the worker stack regions out of the compare,
  or everything below each worker's ESP at its block) would let "engine_mt" join "engine"; reported to the lead.
- Engine::Update, Engine::UpdateTiming (QueryPerformanceCounter busy wait), MXWorld::vf5 (allocates), JobPool_Submit / WaitIdle
  (SetEvent runs the pool workers) verify only since Verify snapshots the emulator's host-side state (2026-10-18 lead change: heap,
  virtual QPC, worker switches, events, critical sections). Before that, MXWorld::vf5 differed in heap addresses (the native's run
  had advanced the deterministic heap). The natives make the same import calls in the same order (they are transliterations), so
  the stand-ins see the same calls in On mode.

## Traps

- The 0x0082 animation functions are LTCG: 0082e250 (EAX, EDX), 0082e2d0 (EDI), 0082b930 (ECX, xmm0, ret 4), 0082b9e0 (ECX, EDX,
  ESI), 0082bac0 (ECX, EDX, EDI, ret 4), 0082efb0 (ECX, EDX, EDI, ret 8), 0082e610 (ECX, EAX, EDI, ret 4), 0082ff20 (ECX, EDI),
  00840040 (ECX, ESI, ret 4). Each port's header comment lists its live-in registers (abi.py).
- In the verify.sh configuration (REFLEX_AMBIENT_OFF with all classes kept) the render-sync worker phase does not run, so callers of
  the bone chain code there are only exercised with REFLEX_KEEP_RENDER_SYNC=1.
- Coverage of the lifted code in the test worlds is partial (one 30-frame Concord-12 sample: 27% of the 15,967 lifted
  instructions; many functions are rare paths: loading, network, sign-in, tables). Forms that ran only in unexecuted paths are direct
  mappings of verified forms (lifter), except the new `div`, `sbb r8, r8` and `fs:` ones, which are simple.
