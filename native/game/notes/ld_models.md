# ld_models (0x780000..0x7c0000) and ld_render (0x7c0000..0x7f0000): wave 4, native loading

Both groups are lifted with the shared lifter (`tools/lift/gen.py --list`), most expensive first (plan/loading-waves.tsv rows with
`ported == 0`). Lists: the scratchpad's `ldmr/ld_models.tsv`, `ldmr/ld_render.tsv` (addr, name, group, file).

| file | backend | ports |
|---|---|---|
| ports/ld_models.cpp | captured (lift.py) | 364 |
| ports/ld_models_fl.cpp | fl (lift_fl.py) | 1 (HDRPost_Adapt: `inc` flags read on several paths) |
| ports/ld_render.cpp | captured | 176 |
| ports/ld_render_fl.cpp | fl | 4 (WaterMesh_Build, WaterMesh_BuildRingRows, WaterClipmap_Draw, Fn_007e9b20: `inc` flags) |

Not ported: **Renderer_BuildGammaRamp (007aff40)**: x87 `fldln2` / `fyl2x` (the interpreter's `transcendental()` in vm.cpp is
not exposed to ports; a gamma-ramp build, called a handful of times). Stays original.

Not ported on purpose: three work-list "functions" are labels inside other functions (a port at such an address fires whenever
the interpreter's pc reaches it, also by fall-through or a loop jump, and Verify cannot treat it as a call):
- **Scene_UpdateEntities (007d2470)**: the loop tail `inc ebx; cmp ebx, esi; jl 7d2410` of SceneManager_Tick (007d23a0, ported by
  ld_render's race wave: its port covers it). As a port it ran exact in On mode (the rest of the function to its `ret`), but race
  Verify reported 1800/1800 mismatches ("native returned to another address": the original was run to [esp], which is not a
  return address there). The Ghidra name/comment for 007d2470 should move to 007d23a0's loop (lead: names).
- **00790061, 00790072**: one instruction after 00790060 (`push eax`) / two after 00790070 (`push ebx; push esi`); 0 callers.

Names: `ModelGroup_DispatchJob` (007a59e0) and `ModelGroup_WorkerThread` (007a5a70) (`_re/names/9e_ld_models.tsv`); the rest keep
their Ghidra names (`Fn_xxxxxxxx` for FUN_). Group strings `ld_models` / `ld_render`.

## The two thread / spin functions

- **ModelGroup_DispatchJob (007a59e0)**: `do for i in 0..2: if WaitForSingleObject(end[i], 0) != WAIT_TIMEOUT break; while (none)`,
  then the job goes to slot i, `EnterCriticalSection(0xd62834)`, `++[0xa95734]`, `LeaveCriticalSection`, `SetEvent(start[i])`
  (GetLastError on failure). The lifted port keeps the zero-timeout wait loop verbatim (each poll is a `LIFT_CALL` of the IAT
  target), so under baton every failed poll yields exactly as the original's does: same calls, same order.
- **ModelGroup_WorkerThread (007a5a70)** and **Render_SyncWorkerThread (007df370)**: thread procedures. Added to `lift.py BLOCKING`
  (their calls are `LIFT_CALLB`): in the race runtime a worker's wait stand-in ends the thread's run by returning pc 0 without
  popping the call; under baton the stand-ins block and return normally, and LIFT_CALLB behaves as LIFT_CALL.

## Lifter changes made for these groups (tools/lift)

1. **Entry not at the lowest address (bug, both backends)**: when an internal `jmp` reaches code *below* the entry (e.g. 007875e0
   tail-jumps to 00787550, which Ghidra does not list as a function, so it is lifted inline), the emitter wrote the blocks in address
   order with no jump to the entry: the port started executing at the lowest block. lift.py now emits `goto L_<entry>;` first (and
   the label); lift_fl.py likewise (and no longer moves the entry label to the top in that case). Found by bisecting a boot crash
   (Lua `luaD_reallocstack`-like 007875e0, and 007ad8e0). A scan of every committed lifter port (walker min address < entry) found
   only these two plus Scene_UpdateEntities (already in the fixed form): no other group was affected.
2. `prefetcht0/1/2/nta`: no-ops (as vm.cpp), both backends (Ocean_Update, 007d7f90, 007d9320, 007deb60).
3. lift_fl.py: `rep movsw / movsb / stosw / stosb` with the direction flag (DeformablePatch_Render; it is now lifted by the
   captured backend anyway after another agent's lift.py change).
4. `BLOCKING += {0x7df370, 0x7a5a70}`.

## Traps

- Other agents edit tools/lift concurrently: re-lifting the same list later can give different (fixed) code (e.g. Scene_UpdateEntities
  went from `LIFT_TAILJMP(0x7d1cd0)` to the inline form; 007ea610 gained a captured `sar` operand). Regenerate the whole file, never
  mix.
- Do not name a scratch script `bisect.py` next to anything that imports capstone (shadows the stdlib module).

## Acceptance (2026-10-18, regenerated with the lifter's unsigned imul fix, built with -fwrapv)

- `loading_accept.sh <boot> ld_models,ld_render 1` and `... alpine`: canonical VERDICT IDENTICAL, race IDENTICAL, race hashes
  identical. 510 (Concord) / 540 (Alpine) of the 545 ports are called during the boot.
- `loading_accept.sh <boot> all 1`: 1 differing node = the known dead-data buffer of loading.md (`+5835c` bfa0ae7d vs 0022fc20);
  race identical. `... all alpine`: IDENTICAL.
- World images: 15 pages / 15 words differ in both events: the top pages of the 12 render-sync worker stacks and the 3 ModelGroup
  worker stacks. Each is the return-address slot of the worker's blocked `WaitForSingleObject(go, INFINITE)` (e.g. 0x121fffc4:
  original 007df396, port 0; 0x137fffb4: 007a5a8f vs 0): LIFT_CALL / LIFT_CALLB keep 0 in the slot while the callee runs, and the
  world is captured while the worker is blocked inside the call. Dead: the race runtime restarts workers from their entry on a
  fresh stack. Any blocking lifted thread procedure shows this (engine pool, forest cull, ...).
- Race `verify.sh <boot> math,ld_models,ld_render`: 0 mismatches, on identical (only ~10 of these ports run in race frames).
