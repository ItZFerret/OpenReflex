# Group "rest" (wave 3a): the last original exe code of the race frame

Scope: every exe function that still ran as original code per race frame with every port on (plan/remaining-frame.tsv), except the
import thunks (`jmp [IAT]`, MSVCR90 / KERNEL32 / FMOD: the CRT agent's). Status 2026-10-18: 190 ports (189 lifted + 1 hand-written).
With them, a coverage run with REFLEX_PORT=all over the five verify worlds leaves only import thunks in the exe (see "Coverage").

Files: `ports/rest_4.cpp` .. `rest_8.cpp` (lifted, one file per 0x00N00000 range, generated: do not edit by hand), `ports/rest_hand.cpp`
(`__alloca_probe`), helpers `ports/rest_util.h` (a copy of engine_util.h, namespace `rst`, `RST_*` macros incl. `RST_CALLB`, plus the
fx group's rcp / rsqrt tables and `FS_BASE`). Names: `_re/names/9e_rest.tsv`.

## How the list was made

remaining-frame.tsv attributes executed pcs to the nearest functions.tsv entry, which is misleading in two ways:

- **Functions Ghidra missed.** Code after int3 padding (or a ret) that Ghidra folded into the previous function: qsort comparators,
  vtable-only functions, state ticks. E.g. "0067ad40 1531/frame" is really 0067ad80 (VehicleEngineAudio_CompareDistance, called by the
  CRT's qsort); "006881c0" is 006886a0 / 00688990 (engine sound state ticks); "00686ec0" is 00687890 / 00687b20 / 00687ea0 / 006881a0 /
  006881b0 / ...; "0068b860" is 0068b940. Most of the "ported but still in the list" entries of other groups (0055d870, 005c0240,
  007dd450, 006cc0d0, 0055d240, ...) are such neighbours: the port at the Ghidra entry is fine, the executed code is another function.
- **Jump-table cases of a ported dispatcher.** 0080c2c0 (world group) is `jmp [ecx*4 + 0x80c3d0]` and its port tail-runs the target
  (`xc::tail`), so the cases (80c2c7, 80c31a, 80c336, 80c3a9, 80c3b4, 80c3ba, 80c3c0, 80c3cb, 80c3ce) ran as original code. Each case
  is a complete function from its entry ([esp] = the dispatcher's return address) to its ret, so each got its own port
  (AudioCmdArgs_*: Audio_PostCommand's event-argument packer, notes in 9e_rest.tsv / engine-audio.md 6.4).

Tool: scratchpad `rest/entries2.py <coverage dir>` covers the executed pcs with lifted functions (lowest uncovered executed pc = an
entry, its lifter walk = covered) and flags ported / mid-function entries. The list it produced plus the plan's not-executed entries
(ported anyway, exact by construction), plus 9 rare functions a coverage run from frame 0 found (load / scenario start: 00749600,
0067ecf0 Scenario_AudioCommand, 0067edc0, 00677750 MusicPlayer_StartScenarioMusic, 0055d300, 007847a0, 00892120, 0088bda0, 0088bdd0)
is scratchpad `rest/rest_list.tsv`.

## How the ports are made

The engine group's lifter (instruction-level transliteration), copied to scratchpad `rest/lift/` (`lift.py`, `abi.py`, fx's `outs.py`
for the declared outputs, `gen.py` writes the five files; regenerate with `_re/.venv/bin/python scratchpad/rest/lift/gen.py`).
Additions over the engine copy: `jmp [reg + disp]` (vcall thunks `mov eax, [ecx]; jmp [eax + N]`, e.g. 00592b60) as `RST_TAILJMP`
through the loaded pointer; the fx lifter's sbb with a consumed CF (strcmp tails `sbb ecx, ecx; sbb ecx, -1`), rcp / rsqrt,
fnstcw / fldcw, int3; 8 / 16-bit shifts and rol / ror with unread flags (00749600).

- **Worker thread procedures** (`BLOCKING` in lift.py): Scene_TickerThread 007d22a0, RiderAnimation_WorkerThread 0061cdb0,
  Audio_QueueThread 00806f90 make every call with `RST_CALLB` (engine notes: the WaitForSingleObject stand-in returns pc 0 without
  popping when the worker blocks on its go event; the native writes the original's return address into the slot and returns 0, so the
  thread stops exactly where the original stops). The runtime's SetEvent runs a worker from its entry each time (m.run(proc)), so
  the native at the entry is used for every job.
- **Ambient updates** (BillboardCrowdEntity / AnimEntity* / ScenarioAnimEntity / CrewChief / LandAnimalProximity /
  InstancedAnimationEntity / ScoreTowerFlagHolder vf19, ParticleManager2D_Update): plain lifted ports; the runtime's class skips fall
  through to them (reflex_port::installed). AnimEntitySingleAnimLoopWithParticles::vf19 006ba520 is ported but no test world runs it.
- No runtime hook lies inside any function of the group (only at entries; checked like engine's hooks.py).

## Traps (for the lead: shared-code issues found)

1. **vm.cpp: the Verify stop is skipped by chained interpretation.** Machine::run checks `pc == stop_pc && esp > stop_esp` only at the
   top of its loop; the inner "follow decoded fall-through / taken chains" loop continues into a cached `taken` link without returning
   there. Verify interprets the original with the JIT off, so when the original's `ret` has a cached link to its return address (set
   while the same code ran as original elsewhere, e.g. inside another port's original run), the original runs past its return
   address to the end of the frame. Seen: `verify.sh <boot> rest` with the official build fails on BaseScenario::vf11 0055eb40
   (a 6-instruction leaf: "esp 7feffba0 orig 7fefff00", then WORLD STOPPED) once FUN_0054af90 (MXPlayer_Update) is verified too.
   Fix (one line, vm.cpp in the chain loop): `... && !special_[npc >> 12] && npc != stop_pc)`. My acceptance runs use a scratch copy
   of vm.cpp with that change (scratchpad `rest/rt/vm.cpp`, build `rest/build_rt.sh`). It cannot hide mismatches (it only ends the
   original run where it should end), so other groups' passing results stay valid; failing ones may be this.
2. **runtime.cpp: two runtime natives do not fall through to ports.** `m.natives[TRACKS_REBUILD]` (007cbe30, renders the deformation
   then returns RUN_ORIGINAL) and `m.natives[TERRAIN_GPU_SYNC]` (007c4fa0 HFTerrainSurface::vf14, RUN_ORIGINAL when the deformation
   model is loaded) are installed with m.natives directly before the ports, so install() leaves them in place and the original runs.
   The ports TerrainTracks_FlushPending / HFTerrainSurface__vf14 exist and verify, but only with the change
   `const Native* p = reflex_port::installed(m, X); return p ? (*p)(m) : RUN_ORIGINAL;` in place of `return RUN_ORIGINAL` (scratchpad
   `rest/rt/runtime.cpp`). With the official runtime those two (about 11,000 instructions per frame over the five worlds) stay original.
3. **`__alloca_probe` cannot be verified top-level.** It returns with ESP below its entry ESP (it allocates), so Verify's stop
   condition (`esp > stop_esp`) never holds and the original runs on to the end of the frame. Its port is in group **"rest_alloca"**
   (not "rest"), so `verify.sh <boot> rest` does not verify it from original callers; with `all` it only ever runs nested inside its
   (ported) callers, whose Verify then covers it, and the On hashes are identical. A Verify fix: stop at the return address when ESP
   is anywhere above the entry frame's argument area, or a per-port "stop at ret_addr regardless of ESP" flag.

## Verification

- Verify nests (engine notes): ports called inside another verified port run natively and are not counted on their own. scratchpad
  `rest/peel.sh <boot> <dir>` repeats Verify-only runs of the five worlds, each round excluding the ports already called top-level:
  172 of the 190 ports were verified on their own, 0 mismatches, in 6 rounds. The 18 never called in the five worlds are rare paths
  (plan entries with 0 instructions per frame, the unused 80c2c0 cases, 006ba520, __alloca_probe which only runs nested).
- `verify.sh <test boot> rest`: 0 mismatches and "on: identical" on all five worlds (51-64 ports called per world); `math,rest` too.
- `verify.sh <test boot> all`: only Engine::Update is top-level; its one mismatch on Concord_ai11 is the known transient dead-stack
  case of notes/dead-stack.md ([148dbd9c], 006b0530, Concord_ai11 / Alpine / Springfield: 1 call each); on identical everywhere.
  Terracross shows 121 Engine::Update mismatches ([142d776c] 00000001 orig 00000003, 128-byte records 0x142d6xxx..0x142dbxxx, calls
  859..): pre-existing, not the rest group's: the same with `all,-rest,-rest_alloca` and with the official build.

## Coverage

REFLEX_PORT=all, coverage from frame 0, the five verify worlds (scratchpad `rest/cov.sh`): with the test build (both runtime patches)
the exe code left is only import thunks (55-668 instructions per frame per world): MSVCR90 memset / memcpy / floor / _CIfmod / _CIatan2 / _CIpow / __libm_sse2_pow / __RTDynamicCast, KERNEL32
GetCurrentThreadId (008aebc0, and the jmp-to-thunk stub 008a0500), FMOD Event::getParameter / EventParameter::getRange.
With the official runtime, TerrainTracks_FlushPending (about 10,700 per frame over the five worlds) and HFTerrainSurface::vf14 (110)
also stay original (trap 2).
