# rx_loop: the game loop, game object updates, players, input, cameras (core/game/loop_*)

Owner: the rx_loop agent. Work list `core/plan/rx_loop.tsv` (719 entries from the old groups engine / engine_mt / rest / ui).

## Files

| file | what |
|---|---|
| `game/loop_util.h` | engine globals (`loop_ctx`, `loop_timing`), virtual slots, typed calls of original addresses (`loop_tc*`; the registered core function, else the original), LTCG call helpers (`loop_regs` / `loop_ex`), x87-result call `loop_tc2_x87` |
| `game/loop_platform.[ch]`, loop_platform.c | the OS / CRT services the frame calls (XInput, DirectInput, cursor, QueryPerformanceCounter, SetEvent / WaitForSingleObject, critical sections, Sleep): oracle runs the original import; product calls `rx_loop_host` callbacks (Unity supplies device state), threads run synchronously |
| `game/loop_extern.h` | the loop code's calls into other subsystems' core functions, taken from the registry by address and typed by the owner's prototype (`LOOP_CORE(addr, fn)`): NULL -> the original runs |
| `game/loop_engine.[ch]`, oracle/loop_engine.cpp | Engine::Update 0084ed20, Engine::UpdateTiming 0084ea80, the scene ticker / engine task / job pool worker thread bodies (run synchronously), JobPool_Submit / WaitIdle |
| `game/loop_world.[ch]` | MXWorld::vf5 007674b0 (the world frame), the player-slot pass 00766ec0, the debug toggle 00767690 |
| `game/loop_input.[ch]` | KeyMap / AnalogKeyMap / CompoundKeyMap / CumulativeKeyMap, XenonInputManager queries, Pad / Keyboard / Mouse device updates (0x0081xxxx) |
| `game/loop_anim.[ch]` | the skeleton animation core (0x0082xxxx, 00840040): poses, keyframe tracks, the animated skeleton, the scalar bone-to-model, the RiderAnimation worker thread 0061cdb0 |
| `game/loop_entity.[ch]` | the scene's ambient entity vf19 updates (anim loops, scenario actors, crew chiefs, score tower, instanced, land animals, water, forest, terrain, billboard crowd), JobPool_Instance |
| `game/loop_cam_util.c`, `loop_cam_third.c`, `loop_cam_entity.c`, `loop_cam.h` | the game cameras: the smoothing / spring helpers, the third-person orbit / water / ground helpers, the Camera base and MXCameraEntity (basis build, copies, vf29 / vf31) |

## Conventions

- Core signature = the original ABI: `this` first (thiscall), then the stack arguments in order (int / pointer -> `uint32_t` /
  `rx_addr`, float -> `float`), LTCG register arguments after them in the order the comment names; the result as the original
  returns it. A call through an original address uses `loop_tc*` / `LOOP_CORE` (registry) so a ported callee is used directly.
- **Original stack frame.** A function that passes a local by address to other code, or whose callee keeps that address (JobPool
  copies the submitter's stack record; worker jobs keep stack vectors), must run at the original's ESP: compute `A = (rx_sp()-4) &
  ~align`, `rx_set_sp(A - frame)`, put the locals at the original's offsets, `rx_set_sp(sp0)` at the end. Engine::Update, UpdateTiming,
  the worker bodies, the scene / player passes, the anim functions and the ambient-entity job submit all do this.
- **x87 precision is 53-bit (control word 0x027f) on the main and the scene / worker threads** (measured; the README's 0x007f is
  the render thread's). x87 code ported with rx_x87 uses `bits = 53` (`MAIN_X87_BITS`, `TICKER_X87_BITS`). A 24-bit port gave 1-ulp
  errors in Engine::UpdateTiming's FPS value.
- Adapters declare as outputs only what callers read (`plan/live.tsv`); void virtual functions declare none (or `OUT_FPU` to keep
  the x87 stack balanced). Worker-thread entries use `RX_PORT_RAW` and leave the thread blocked at its wait (ESP at the return
  slot, the callee-saved registers the original holds there) returning pc 0.
- The decompile drops / invents arguments and misreads stack slots: every port is read from the disassembly.

## Threads

The frame wakes scene tickers, the job pool (2 workers), the engine task thread and the rider-animation workers through SetEvent /
WaitForSingleObject. The oracle build keeps those import calls, so the emulator runs each worker from its entry until it blocks and
the RX_PORT at the entry (Scene_TickerThread, JobPool_WorkerThread, Engine_TaskThread, RiderAnimation_WorkerThread) runs the core
body (`loop_wake_worker` = SetEvent). The product build runs the body synchronously at the wake point in the original's order
(`loop_platform.c`, `loop_wake_worker` = call the body now). Verified both ways against the original's serialized order.

## Verification

- `native/game/tools/verify.sh <boot> rx_loop` (On mode uses `REFLEX_MEMHASH=nojobs`). `scratchpad/rx_loop/vfy.sh` and `peel.sh`
  run it (and the inner-function peel) under the rx_loop emulator lock (FIFO, `lock.sh`).

## Known differences (acceptance = On identical, see native/game/notes/known-differences.md)

- **Mud / particle records (Terracross only).** Engine::Update 0084ed20 differs on Terracross_TRX_Verde_ai11 only, in 128-byte
  records at +0x6c (MudRecord_Update 00408c50 -> TerrainMaterialAt 007c5120, a dead-stack local; the fx code is unowned). State
  hash identical. known-differences.md entry 1.
- **Animation worker dead stack.** With the anim ports (loop_anim.c) on, Engine::Update's Verify shows per-frame differences in
  128-byte records at 0x13dxxxx (w lanes 3c8132b7 vs 0): the hand-written SkeletonPose_BoneToModel etc. leave a different dead w
  lane on the rider-animation worker stack, which 006b0530 (anim) copies into those records (native/game/notes/dead-stack.md). Each
  anim port verifies clean on its own (peel); the difference appears only inside the whole-frame verification and is rewritten every
  frame, so On mode is identical. Fix when 006b0530 is ported (define the w lane).

## Traps

- **JobPool ring holds dead stack.** JobPool_Submit 00881f00 copies the submitter's 0x40-byte job record (partly uninitialized
  locals) into the ring 0x00d94b10..0x00d9cb10. `REFLEX_MEMHASH=nojobs` / port.cpp's rx_* compare leave the ring payload out
  (the slot functions 0x00d9cb10.. and indices stay in).
- `verify.sh` counts the old lifted `ui` / `engine` / `rest` port at the same address too ("N ports called" counts both).
- **.data is mutable; only .rdata (ends 0x9a6000) is constant.** Constants a function loads from .data must be read from memory,
  not written as C literals: e.g. 0x9c542c (3.0 at run time), 0x9d78b4, 0x9cd99c, 0x9ddfdc, 0x9cb4a0..c8, 0xd615xx (helper A).
- **Stack addresses leak into the heap / persist.** Objects built on a stack local register themselves in global vectors (the
  collision registry 0xd6d8e0; the JobPool ring; std::map iterator addresses), so a port that builds such an object, or calls code
  that does, must run at the original's exact ESP (the RX_FRAME / `rx_set_sp` recipe). A frame that merely has room is not enough.
- **An On difference can appear only at long runs** (a rarely-hit path): peel / 300-frame On can be identical while 1800-frame On
  diverges (one extra resident page = a heap allocation the original does not make). Verify On at full length before claiming done.

## Status (2026-10-18)

87 functions ported, registered and **verified** (0 mismatches, On identical on all five worlds bar the Terracross mud record):
the whole frame (Engine::Update, UpdateTiming, MXWorld::vf5, the player-slot pass, the debug toggle), the job pool and the scene /
engine worker threads, all input (KeyMap / AnalogKeyMap / CompoundKeyMap / CumulativeKeyMap, XenonInputManager, Pad / Keyboard /
Mouse, the key-map list update), the ambient entity updates (anim loops, scenario, crew chief, score tower, instanced, land animal,
water, forest, terrain, billboard crowd, JobPool_Instance), the camera smoothing / spring / third-person helpers (loop_cam_util.c,
loop_cam_third.c — helper B), and the vehicle-glue family (loop_vehicle.c — helper A).

### Deferred (written, build, but NOT On-exact — moved to scratchpad `rx_loop/wip/`, do not register until fixed)

- **loop_anim.c** (the skeleton animation core: 0082e250 / 0082e2d0 / 0082efb0 / 0082e610 / 0082b930 / 0082b9e0 / 0082bac0 /
  00840040 / 0082ff20 / 0061cdb0). Each peel-verifies clean on its own, but with the race worlds' rider-animation jobs running,
  On mode differs in 128-byte records at 0x13cexxxx (+0x18 / +0x1c, floats ~0.014 vs 0): SkeletonPose_BoneToModel 0082efb0 produces
  a nonzero 4th (w) lane in the rotated translation where the original's packed SIMD keeps it exactly 0, and 006b0530 (anim) copies
  that lane into the persisted records. Fix: transliterate 0082efb0's SIMD quaternion-vector sandwich lane for lane (lane 3 stays
  0) instead of the scalar `quat_mul(v4_xyz(v), conj)` path. The rider worker 0061cdb0 adapter also had EAX wrong at its block.
- **loop_cam_entity.c** (the Camera base / MXCameraEntity basis build and copies: 00790ce0 / 00790f40 / 007911b0 / 00792330 /
  007923a0 / 00792380 / 00792350 / 00790cb0 / 007911e0 / 0063ddd0 / 0063dc30 / 0063de70 / 0063de80). On mode differs by 29 dwords
  on Concord (close floats): the `cam_from_forward_up` basis builder's cross-product / normalize order does not match the
  original's exactly. Fix: transliterate 00790f40 / 00790ce0 operation by operation from the disassembly (the component order of
  the two cross products and the two normalizes).
- **loop_player.c** (helper A: MXPlayer_Update 0054af90 and its Player_* chain + the activity / records / package / table / thread
  helpers, ~54 functions). The core MXPlayer_Update chain peel-verifies clean; but On mode diverges at full length (Concord 1800
  frames: one extra resident page + hash mismatch; identical at 300 frames). Bisected to the Q4 helpers (0055dc20, Activity::vf12
  0055e820, MXActivityRace::vf52 00565640, 00567660, 00525cd0, MXRecords 00764b90 / ef0 / f40 / fc0, List_Init 005442c0,
  Records_FreeRange 00545120, Table11_Get 008841d0, Thread_Note 00884390, PackageManager_Update 00540130) — at least two contribute
  and the symptom is a heap allocation the original does not make. 00884390 / 005442c0 also fail their own peel on main-stack
  dead-stack reads. loop_vehicle.c (also helper A) is clean and kept.

### Triaged out (not ported) — presentation / platform, no gameplay-state writes

| family | n | why | rand? |
|---|---|---|---|
| UI (MXUIManager / UIPage / UIComponent / UIContainer, 0x0059xxxx..0x0060xxxx) | ~197 | Unity has its own front end and HUD; these drive pages / widgets, not vehicle / physics / race state | none direct |
| audio (SoundEmitter, MusicPlayer, VehicleAudioManager, engine audio, the audio command queue 0x0080xxxx) | ~106 | Unity has its own audio; emitters read gameplay state, do not write it | none direct |
| data / tables (Database, XMLReader / XMLBuilder, Registry, MXRecords, Table_*, 0x004dxxxx config) | ~100 | save / stats / config tables, loaded by the construction path (verified by objcmp), not the race frame | none direct |
| network / sign-in (NetworkSession, PCSession, NetStats, Quazal, 0x006exxxx / 0x0088xxxx) | ~22 | no online play in the port | none direct |
| Lua GC (luaC_* 0x00783xxx..0x00784xxx) | 18 | the exe's Lua 5.1 incremental GC; the game logic the port runs is native, not Lua | none direct |
| dev console (DevConsole, KeyMapCommand, 0x00813xxx / 0x00822xxx) | 9 | developer overlay | none direct |
| render (Renderer_EnsureDeviceReady 007ae4e0, Render_SyncRenderData 007e0fc0, 007e0190, 007ea020) | 4 | Unity renders; Engine::Update calls them through the original in the oracle, no-ops in the product | none direct |
| import thunks (MSVCR90 / KERNEL32 / XINPUT / FMOD) | 14 | the CRT agent's / platform | - |
| fragments (00630065, 007d2470 Scene_UpdateEntities, 0061e677, Catch@004186d5) | 4 | mid-function split tails / not real functions (see the ui / engine notes); 0061e677 is a physics/rider fragment | 0061e677 reaches rand |

**Correction (w-hfterrain, 2026-10-29): the render sync has a gameplay effect.** Render_SyncRenderData 007e0fc0's per-object worker job
(Render_SyncWorkerJob 007e0c50) calls, for each vehicle, RavenVehicle::vf14_o94 004302c0 (the vehicle's vf61, then the model's node
set +0x10 copied over its render copy +0x50: NodeSet_CopyFrom 007dd930, which also clears the copy's bounds-valid bits) and vf6_o94
004302f0 (the copy's subtree bounds). The AI reads those bounds: BehaviorSeperate's target pick 00531a70 -> 00533070 -> 00533510 ->
004302f0 uses the render copy (+0x50, because the timing object's +0x54 is nonzero during a race). The product's render_sync is a no-op,
so the copy keeps the pose of its last refresh (the product refreshes it only in the damage registration, world_veh_damage_register.c):
its cached box does not change from frame to frame, its centre is ~2 ft lower and its radius 4.39 ft against the original's 4.46-4.86. On the paired Concord ride an AI then picks its pass target one frame early (gate+1.87 s). With the
job's two vehicle calls run after each product frame (test hook PROBE_RENDER_SYNC in core/tools/bike_parity_probe.c) the 11 AI stay
bit-identical to gate+11 s and the local bike for the whole 22.5 s compared (QUEUE "w-hfterrain").

**Ported (w-rendersync, 2026-10-29): `render_sync` runs Render_SyncRenderData 007e0fc0 / Render_SyncWorkerJob 007e0c50**
(loop_engine.c rx_render_sync_data / rx_render_sync_job) at the original's place (Engine::Update, per active scene, last first, after
the post-tick and the engine-task wake, before the render cameras). What the two do: under the container's locks (+0x1114, +0x112c)
and the "SyncRenderData %d" label, every +0x10c8 entity's renderable (vf25) gets vf14 (the terrain surface 007c4fa0, the
always-animated AnimEntitySingleAnimLoop); then the four render-sync workers run the job over the +0x10b4 records (last first):
vf25, vf13 (changed? the model entities and vehicles 00794f70 = 1; CharacterEntity 006180e0 / AnimationEntity 0082d1c0 read and
clear their pose-changed flag), vf14 (vehicle 004302c0: vf61 + the model's node set copied over the render copy; CharacterEntity
00618100 / AnimationEntity 0082d1a0 node-set copies; model entities 0055fd90 matrix copy), vf6 (the bounds: 004302f0,
ModelEntity 00401f50, CharacterEntity 00617f60), vf11 (the matrix: 00430340, Entity 0088fa20, 00618050, 00617da0) and the world
centre; the radius kept is max(new, record's). Render only, oracle build only: the record's centre / radius and the culling-grid
moves (007d9f00 / 007d9fc0 queued, 007da080 applied; read by the lights 007e0940 and the culling build 007e1130 / 007e14a0, none
run by the product). The product leaves the terrain surface's vf14 to the session's render hook (rx_session.c, unchanged). New
small ports: 006180e0 / 0082d1c0 (vf13_o90), 00618050 / 00617da0 (vf11_o90) in anim_entity_nodes.c. Verify / paired rides: QUEUE
"w-rendersync".

Unported in "other" that write gameplay / world state and belong to other subsystems (should be ported by their owners, called
through the registry by the loop frame): the fx terrain-deformation / particles (TerrainTracks_FlushPending 007cbe30,
ParticleManager2D_Update 00698510, DeformablePatch_ReadbackToSysmem 007cb930, HFTerrainSurface::vf14 007c4fa0 — **rx_fx, no owner
yet**); RavenSimulationMotocross::vf2 0044a650 / vf20 0043d1f0 and 0045f600 (vehicle setup — rx_bike); TutorialManager_Outcome
007520a0, PackageCache_Update 00543e50 (no gameplay state, left for later). The animation-table / character helpers (00835170,
0082e610 callees) and race drivers consume the shared rand() at runtime (census: 00835186 rx_anim skeleton sampling, 0050ff..
RaceDriver / 00565054 MXActivityRace rx_race) — not rx_loop code.

### rand() among rx_loop functions

No rx_loop function calls rand() directly (checked against imports.tsv). Transitively (direct-call graph) only 0061e677 (an
unaligned physics/rider fragment, unported) reaches it. Indirect / virtual calls are not traced; the live rand consumers in the
frame are all in rx_anim and rx_race (above). The lead should confirm the shared-rand ordering once those subsystems land.

## Session 2 (rx_loop2, 2026-10-19) -- progress log (kept current while working)

Scripts (scratchpad rx_loop2/): b.sh (build cli with loop_* crt_* phys_* anim_* race_* world_* bike_* build_alloc*), run.sh (one
world, rx_loop lock), v.sh (Verify), on.sh (On vs cached reference hashes in ref/), peel.sh (rounds dropping called rx_ names),
sum.sh (per-name totals of a peel log), names.sh (rx_ names of adapter files), callers.sh (REFLEX_CALLERS per world), d (whole-
function disassembly from functions.tsv sizes: the old `disasm | stop at ret` missed code after a first ret), c (rdata values).

Done and verified (Verify 0 mismatches all five worlds; rx_loop On identical, Terracross = the known mud record):
- loop_cam_entity.c (rewritten from the wip): 00790ce0 00790f40 007911b0 007911e0 00792330 007923a0 00792380 00792350 00790cb0
  0063ddd0 0063dc30 00790b30 0063dba0 0063de70 0063de80 0063dee0 0063df90 0063e310.
- loop_cam_player.c: 00646a50 006468a0 00646940 00646bf0 0063d7e0 0063d600 004dbe00 0063e850 (the camera path).
- loop_cam_misc.c: 00548620 006481a0 00648990 006489e0 00648aa0 00649160 00647950 00646ff0 005487d0 (+ rx_cam_lift_common:
  00650c60 was overflowing its frame into the caller: rewritten on the original's frame).
- loop_cam_tpc.c: MXThirdPersonCamera::vf19 0064c110 (25k+ calls, 0 mismatches), 00650210.
- loop_cam_shake.c: 006519b0, 00652800.
- loop_util.h: loop_push / loop_pop: direct calls of core functions now push the stack arguments and the return address like
  the original (a core callee lays out its frame from rx_sp(); without the pushes it ran 8 bytes high and a CRT locale init
  under the UI chain stored a different stack address in msvcr90 data: On differed with anim_* linked).

Traps found:
- x87 fld/fstp of a NaN: the reference (emulator f80) stored sign | 0x7fffffff (payload-less NaNs) until 2026-10-27; f80 and
  cam_x87_bits (loop_cam.h) now keep the hardware's quieted payload (known-differences.md 7).
- Camera thread x87 precision: the cameras run on the main thread, control word 0x027f (53 bits): CAM_X87_BITS was 24.
- 0063c6a0 (angle wrap) writes the lifted angle back into its argument slot (live above its final ESP for Verify).


## Session 2 acceptance (2026-10-19, rx_loop2)

Build: `RX_CORE_FILES="loop_* crt_* phys_* anim_* race_* world_* bike_* build_alloc*"` (build_alloc for rx_reflex_aligned_*; the
world_node_audio.c reference). core/build.sh (product) clean.

Landed and On-exact (whole rx_loop On all five worlds: identical, Terracross = only the documented 6-dword mud record at +0x6c,
007c51ce vs 00d94af8, known-differences.md #1 — no other difference, so the old anim-worker w-lane 0x13cexxxx difference and the
loop_cam_entity 29-dword Concord difference are both resolved):
- the whole camera group except MXFirstPersonCamera::vf19: loop_cam_entity.c (Camera base / MXCameraEntity, 18 fns, rewritten from
  the wip), loop_cam_player.c (MXPlayerCamera / MXAnimatedCamera / the camera path 0063e850, 8 fns), loop_cam_misc.c (start gate /
  replay / replay tower / quick cameras + 00548620 / 005487d0, 9 fns), loop_cam_tpc.c (MXThirdPersonCamera::vf19 0064c110 +
  00650210, the gameplay chase camera Unity reads), loop_cam_shake.c (006519b0 / 00652800).
- the skeleton animation core: loop_anim.c (10 fns, landed from the wip) + loop_rider.c (00632b00 / 00632dc0).
- loop_util.h: loop_push / loop_pop so every direct call of a core function from the loop pushes the stack arguments and the call
  site's return address at the original's depth (the CRT locale init under MXGame::Update stored a stack address in msvcr90 data,
  which made On differ once anim linked in; the anim slerp dead stack is reproduced via phys_quat_slerp_dead_stack).

Fixes that made it exact (added to the Traps section above): the camera x87 copies store a NaN as sign|0x7fffffff (the reference
f80 model), not the hardware quieted payload; CAM_X87_BITS is 53 (main thread), was 24; 0063c6a0 writes the lifted angle back to
its argument slot; the anim worker / Skeleton_Animate "mismatches" in peel.sh are the worker blocked-state + dead stack that
verify.sh tolerates (nested-as-part-of-caller) and On rewrites each frame — not acceptance failures.

### Still deferred / not done (and why)
- MXFirstPersonCamera::vf19 0064c110's sibling 00642680 (16 KB): not ported (ran out of budget); it calls 00623bd0 (rider head
  pose, rx_anim) and the same orbit/basis helpers already ported, so it is a straight transliteration for a follow-up.
- MXGame::Update 00757fb0 and 00757660 / 007f7f40 / 00753760 / 007575a0 / 00757490: these are rx_race group (plan/rx_race.tsv), a
  ~2.4 KB 12-state machine calling ~100 functions still unported in rx_build / rx_race (MXGame_SwitchToUIWorld, world-load wait,
  activity setup, UI property reads ...). Not tractable as an rx_loop item until those land; left for the race/build owners.
- the rider limb solvers 00633010 / 00635e10 / 00636990 (reach contact / release / targeting): large double-precision quaternion
  solvers (disassembly captured in scratchpad d_633010.txt / d_635e10.txt / d_636990.txt); 00632b00 / 00632dc0 done.
- loop_player.c (helper A, MXPlayer_Update chain): left in the wip; the 1800-frame heap-page divergence bisected to the Q4 helpers
  is unchanged this session.
- the pad sub-helpers 00815900 / 00815b60 / 00815fb0 / 00816400 / 008169f0 stay as rx_call_original in loop_input.c (rx_pad_update
  already verifies with them original); vehicle 0043d1f0 / 0044a650 / 0045f600 are rx_bike group (0045f600's quaternion-from-axis
  basis block is the same shape as 006519b0's, captured in d_45f600.txt).

## Session 3 (rx_loop2 / loop 2, 2026-10-19)

Build (scratchpad rx_loop2/b.sh): `RX_CORE_FILES="loop_* crt_* phys_* anim_* race_* world_* bike_* build_alloc* build_tables_rdb*
build_vehicle_tables*"` (race_activity.c now needs the table code). Acceptance scripts: acc.sh (verify.sh under the lock),
acc_all.sh (verify.sh rx_loop + the crash-drive world REFLEX_DRIVE=7 + the synthetic pad), padsim.sh, peel.sh / v.sh / on.sh.

### loop_player.c (MXPlayer_Update 0054af90 and its chain, 53 functions) -- landed
- **The 1800-frame Concord divergence was MXRecords_AddInt 00764fc0 (and LongestWheelie 00764b90): 0041aaf0 (the cell address)
  takes the column in EDI.** The wip passed EDI = 0, so the "attempted jumps" cell read column 0 and the sum became INT_MAX;
  the table's change bookkeeping then allocated the extra heap page. Found with REFLEX_MEMDUMP (dumpcmp.py: the extra page plus
  two words) and REFLEX_WORLD_WATCH on the differing cell. With EDI the run is identical.
- The whole file was then rewritten from the disassembly: every function on the original's frame (pl_frame), every direct call
  of a core function with its pushed arguments and return address (PL_AT), the other subsystems' functions through the registry
  with the owner's prototype (PL_CORE, original as fallback), the register inputs of every remaining original call checked with
  rx_loop2/rin (registers read before written). Exact-frame details: 00884390 keeps its second key in the a2 argument slot,
  AddInt / LongestWheelie pass their own argument slot as the value pointer, 0054c150 keeps the blend scale in the p slot,
  00540130 its loop index in the dt slot, 005442c0 returns the entry ECX (push ecx / pop ecx).
- 00548620 is registered by loop_cam_misc.c (rx_player_analog7_active): removed from loop_player.c.
- Delegate_Invoke 00554fc0: the only caller (the event dispatch 00592e64) reads no register: no EAX output (the delegate's
  target may be a void core function, e.g. 0054f0b0).

### loop_input.c pad helpers -- landed
00815900 (sticks), 00816400 (triggers), 00815b60 / 00815fb0 (button records), 008169f0 (vibration), and 008156e0 (the stick axis
record) ported; rx_pad_update calls them directly. The pad state layout is the original's; the product feeds it through
rx_loop_host's XInput callbacks (loop_platform: loop_xinput_get_capabilities / get_state, new loop_xinput_set_state using the
existing rx_loop_host.xinput_set_state; no host interface change). The reference worlds have no pad (the world test passes none):
oracle/loop_input.cpp has a development hook, rx_PadSimHook (group "thunk", at the XInputGetCapabilities thunk 008ea364): with
REFLEX_PADSIM=<seed> it replaces the emulator's XInput imports by a synthetic pad 0 whose state is a function of the game clock
(both verify phases see the same state). padsim.sh: peel + On against the hook alone.

### loop_rider.c limb solvers -- landed
00636990 (the stage twist: three axis turns, two bones), 00635e10 (release), 00633010 (the two-bone contact reach, 2368
instructions). All SSE single / double (cvtps2pd .. mulsd) except one x87 length (53-bit: computed in double, same rounding); the
.data constants (0x009cd9e8 -1, 0x009ddfc8 0.7071, 0x009cb4c0 the down vector, 0x009cc8a8, 0x00d94ab8) are read from memory.
The callers in rx_anim (anim_rider_b.c limb_reach_contact / limb_release, anim_ik_anim.c 00636990) still call the originals
through rx_call_original: they should call rx_rider_limb_reach_contact / rx_rider_limb_release / rx_rider_stage_twist
(loop_rider.h) or the registry.

### bike_setup.c -- RavenSimulationMotocross::vf20 0043d1f0 landed (motocross only)
The reset ground placement. 0044a650 / 0045f600 are build_vehicle_place.c's (the vehicles construction unit). The caller
race_misc_util.c rx_vehicle_effect_query (00460020) calls vf20 and 0045f600 through rx_call_original: the race owner should
use the registry (RX_FN of the vtable slot / rx_vehicle_place).

### x87 copies
loop_player / loop_input / bike_setup copy floats through the x87 the way loop_cam.h does (a NaN becomes sign | 0x7fffffff, the
reference's f80 model); bike_math.h's bike_x87_copy32 quiets instead. Identical for every non-NaN value; whoever meets a NaN
there should settle which the emulator does.

## Baseline triage (2026-10-19 15:30, lead + loop2)

The full `verify.sh rx_loop` baseline after the interrupted session showed real failures. loop2 fixed them by making the oracle
build dispatch **other units' entry points** through `rx_call_original` (so an installed port runs through its adapter with the
correct register model / dead stack, and an uninstalled one stays original):
- `loop_engine.c` `rx_engine_update` (0084ed20): the MXGame::Update dispatch (00757fb0) now goes through the guest in the
  oracle.
- `loop_engine.c` `rx_jobpool_worker_run` (00881900): the job function goes through the guest.
- `loop_engine.c` `rx_scene_ticker_run` (007d22a0) / `rx_engine_tasks_run` (0084f320): the virtual dispatch goes through the
  guest.
- `loop_world.c` `driver_joined` (0055b610): the driver-joined call goes through the guest.
Product paths are unchanged.

Results (`verify.sh rx_loop`):
- Concord: 1 word [152414cc] call 1697 (documented IK goal w lane, `006b0530` / dead-stack row 3), On identical.
- Concord_ai11: Verify 0, On one page (known-differences entry 2: the lazily-loaded ConVar registry page 0x1002b000).
- FreeRide: **Verify 0 (was 874), On identical** (was DIFFERENT).
- SX: 1 word [1524dd5c] call 1375 (same documented IK w lane), On one page (entry 2).
- Terracross: 970 (documented mud +0x6c, entry 1), On same page count, documented mud words.
- `REFLEX_DRIVE=7` Concord 1800: state identical, only the same one page.

The FreeRide root cause was **not** loop: `build_activity_game.c` called `rx_fx_vehicle_effects_update_all` 00691bd0 directly
in the oracle, where the fx register binding is unbound outside an FX_PORT; the saved-register words in particle templates were
0. Fixed by running the original 00691bd0 in the oracle (the core call remains in the product and is verified in rx_fx). See
known-differences entry 3. The remaining one-word IK mismatches and the one-page ConVar residency are documented, not loop
regressions.

## Cameraschema (2026-10-20, agent "cameraschema"): the camera tuning schema, the first-person ctor, the intro camera check

Build dir `builds/cameraschema` (oracle CLI `builds/cameraschema/cli_full`, probe CLI `builds/cameraschema/cli`,
product probe `builds/cameraschema/probe`, backups `builds/cameraschema/backup`). New files:
`core/game/loop_cam_schema.[ch]` (00908500 / 006596f0 / 00659ef0), `core/oracle/loop_cam_schema.cpp` (group rx_loop for
the initializer, rx_camschema for the two helpers: the helpers are only reached inside 00908500, so a run that installs
the parent must not install them too, or the child's core fill runs on the parent's already-filled state),
`core/oracle/camschema_probe.cpp` (development probe: dumps the 9 class records, the camera objects and the world's
animated camera at a frame). Edits: `loop_cam_build.[ch]` (00641280 MXFirstPersonCamera ctor, 00407410 the 4x4 row
helper; the file notes updated).

### The schema (what was missing, now ported)
`00908500` is an `_initterm` slot (0x009166e0 + 0x5f0) and was in `rx_globals_unported()`. The original calls
`006596f0(rec)` for the nine 0x1da80-byte class records at 0x00af6d10 (six are loaded from `Cameras\*_Camera.bxml`; the
others are unused but built identically). `006596f0` constructs 14 preset entries of 0x404 bytes (256 record pointers,
count at +0x400) and 94 blocks of 14 name records (`0x48` name/kind/float, `0xa0` name/kind/curve, `0x60`
name/kind/vec4), then calls `00659ef0`, which fills every block (names from .rdata, the kind, the default: FLT_MAX for
"not set", the parsed-value defaults for Other_*_CL_/UL_* and 1.0 vec4s for the effects bases) and appends each record to
entry `side + 2 * type` (FS/SS x FirstPerson/Visual/GamePlay/QuickLeft/QuickRight/QuickBack/Wreck). The port keeps that
structure: `cam_schema_rows[94] = {base, size, kind, fs name, ss name, defaults}` is the constructor order and the
fill's row order; the entry index multiplies the row's size. The original's `_atexit(00912970)` is `RX_ATEXIT`.

Verification (baton boot, `builds/cameraschema/verify/`): `rx_CameraSchemaInit` 1/1/0, `rx_CameraSchemaConstruct`
9/9/0, `rx_CameraSchemaFill` 9/9/0. On mode (the core initializer runs in place of the original, then the boot's
original parser loads all six bxml files): the 9 records at frame 300 are **byte-identical** to the reference
(`schema_f300` dumps). Before this the tables were empty (the parser found no entries: `vf32` set fov 0).

### The camera tuning applies with the real schema
With the schema present the third-person vf32 port is exact: `rx_MXThirdPersonCamera_vf32` (0064a330) 4/4 and 8/8 calls,
0 mismatches; `rx_MXPlayerCamera_vf32` (00646b70) 1/1/0. An On-mode boot with the core schema + core vf32 gives a camera
dump identical to the reference at every sampled frame 500..1000: the GamePlay third-person camera `slot1` has fov
0.942477703 (54 deg, `+0x4cc` / `+0xe0`), near 2, far 15000, `+0x4e4` 4, `+0x4e8` 0.174532905 (10 deg), `+0x510` 2.2,
`+0x5c4` 0.942477703, `+0x5c8` 1.66630054; the FirstPerson `slot2` 60 deg, the QuickLeft/Right/Back 50 deg, the gate
45 deg (not tuned; unchanged). The start-gate camera is unaffected.

### First-person camera: ctor ported, the slot still not created (vf19 missing)
`00641280` (464 B) is ported (`rx_first_person_camera_construct`: MXPlayerCamera, vtable 0x0095a80c, +0x36c the config,
+0x460/+0x470/+0x480 the basis rows, +0x390 / +0x394 the 20 / 80 degree limits, +0x3c4 0.5, four quaternions at
+0x3f0/+0x400/+0x410/+0x420) and 00407410 (26 B, `rx_cam_pose_row`: x87 copy of a vec3 into row EAX of a 4x4) — boot
Verify 1/1/0 and 184/184/0. The product's factory still returns 0 for type 0: the slot's per-frame update
`MXFirstPersonCamera::vf19 00642680` (16,142 bytes) is not ported. The original's boot calls it **112 times** (probe at
0x00642680, `cam 14c5b970 dt 3c888f87` repeated), so creating the slot without it would be a fatal product stop. Its
callees are all ported (00623bd0 rider head transform, 0063d920, 0063dee0 set pose, 0063e400 ground clearance,
00401000 / 0042c230, Vec3_Cross, 00407410); the decompile is `_re/out/decomp/reflex_006_00632aa0.c:8754-9622` (869
lines) and is mostly x87 quaternion / rotation blocks at the main thread's 53-bit precision (the same style as
0064c110, 788 lines). That is the exact next step; it was not attempted in this pass (16 KB of x87 transliteration).

### Intro camera: the two functions are gameplay and ported; the product's scenario does not pose it
`0063d780` / `0063e7c0` are the scenario camera's label + path clip (gameplay, not render) and are ported (chain round 3,
group rx_anim). In the reference baton boot the world's inline `MXAnimatedCamera` (world+0x218f0) is posed: `+0x2b0 = 1`,
its path at +0x310 has a clip with length 3.333333 s, the clock advances and the Camera position/fov move. With the core
`rx_animated_camera_vf19` + `rx_animated_camera_at` in On mode the world camera's pose/fov matches the reference at every
sampled frame (0x44141525 at one frame, 0x492686629 at another - the same words). The **product** (probe
`builds/cameraschema/probe`) still leaves `+0x2b0 = 0` and the Camera at the base fov 45 deg: its intro scenario never
sets/ticks the world camera. That is the scenario unit (`rx_scenario_start` 1 call, 2 words at [1821f674]; the
CameraAnimation handler 0055d290 is reached through the script's tail-jump dispatch, which the Verify harness does not
count), not the camera code; the getter's world route (`act + 0xff090 + 0x14 -> +0x234`) is correct (the probe finds the
same world). rx_session_camera returns 2 during the product's intro because of this. **Update 2026-10-21 (agent
"introcamera"): the premise was incomplete - the scenario did tick the camera, but `0063d7e0`'s inlined pause test and the
missing AnimationSets alias table (plus the 640 x 480 display aspect) held the pose; all three are fixed, see the
"Introcamera" section at the end of this file.**

### Verification of the rest
`verify.sh cli_full rx_world quick`: 132 ports, 0 mismatches, On identical. `rx_race quick`: 48 ports, only the
documented 0055f700 RiderCharacter pair (775/1200). `rx_loop quick`: only the same documented 0084ed20 pair (the report
also lists the other probes sharing 0084ed20; shared stats, one native). `rx_CameraSchema*`, `rx_MXFirstPersonCamera_Construct`,
`rx_Camera_PoseRow` and the vf32 ports are the per-call counts above. The third-person constructor 00649610 still shows
its pre-existing 4 calls / 96 words (same result with the camerafix CLI: base-camera fov word, uninitialised heap
residue); not caused by this work. `00647e00` / `00647e40` / `00649bd0` are still not reached.

### camsnap (2026-10-20, agent "camsnap"): the camera cut-in snap 00649df0 (loop_cam_snap.c)

`00649df0` is Player_UpdateCamera's last camera-switch step: when the active entity changes and the OLD one was a blendable camera
(class 0x100d, MXStartGateCamera) while the NEW one is class 0x1003 (MXThirdPersonCamera), the new camera's cut-in transition is seeded
from the old pose. It was the product's stop one frame past 1252 race frames (plan/QUEUE.md "presencefix"). Ported in new
`core/game/loop_cam_snap.[ch]` (`rx_cam_snap`), adapter `core/oracle/loop_cam_snap.cpp` (group `rx_loop`); the `loop_player.c` call site
is switched at the original's stack depth with the call's return address `0x0054c900` (the oracle fallback `pl_call` stays).

ABI / frame: LTCG, ESI = the new camera, EDI = the old one, one stack float (`duration`), RET 4. Frame `push ebp; mov ebp, esp; and
esp, -16; sub esp, 0x10`; the first matrix row is kept in its +0 / +4 / +8 slots. The old entity's matrix comes from vtable slot 0x38
(`Entity::vf14` 0088fa20 = entity + 0x30), called **twice** (it may return a scratch matrix), through `loop_tc0r` with the original
return addresses 00649e0c / 00649e35. Writes: +0x3f4 = 1, +0x3c8 / +0x3cc / +0x3c0 = 0, +0x3c4 = duration, +0x3d0..+0x3d8 = the old
position (m1 +0x30..), +0x3e0..+0x3e8 = the old look point (m2 +0x20.. plus the m1 position; the addss operand order is kept for NaN
payloads: m2.x+m1.x, m1.y+m2.y, m2.z+m1.z), +0x3f0 = the old fov +0xe0 (x87 copy). MXThirdPersonCamera::vf19 0064c110 blends from
these fields while +0x3f4 is set (loop_cam_tpc.c).

Per-call Verify: the five worlds' frame entry is itself a port (`rx_Engine_Update` 0084ed20 -> `rx_MXWorld_vf5` 007674b0 ->
`rx_MXPlayer_Update` 0054af90 -> `rx_Player_UpdateCamera` 0054c150 call each other core-to-core), so the snap is only reached by the
guest with that enclosing chain peeled: `REFLEX_PORT="rx_loop,-0084ed20,-007674b0,-0054af90,-0054c150" REFLEX_PORT_VERIFY=1`
(`builds/camsnap/peel.sh`). There the original Player_UpdateCamera reaches the installed port: Concord 1800 / drive 1 **1/1/0**,
Concord_ai11 1500 **1/1/0**, SX 1500 **1/1/0**, Concord crash-drive (`REFLEX_DRIVE=7`, 1800) **1/1/0**; FreeRide_FR_Alpine and
Terracross 0 calls (their start-gate camera is never active: no class 0x100d -> 0x1003 switch). With `REFLEX_PORT_DEAD=1` the Concord
call is still **1/1/0**, so the port reproduces the original's frame and dead stack exactly (the two vcall return addresses included).
The peel runs' only other mismatch is the documented RiderAnimation worker dead-stack word (Concord [152414cc], SX [1524dd5c],
Terracross [1524da1c]). Standard acceptance, unchanged: `rx_loop quick` 7 ports, only the documented 0084ed20 pair (775/1200), On
identical; `rx_race quick` 48 ports, only 0055f700 (775/1200), On identical; `rx_world quick` 132 ports, 0 mismatches, On identical
(`builds/camsnap/verify_*.log`).

Product: `RX_FULL_FRAME=1 rx_api_test <install> 5000` runs the full request on the last runnable tree (the presencefix build's objects
plus this port): 9 load frames, **5009 frames, state 2 (racing), game time 83.534, race time 65.063 (past the 60 s presence branch),
laps 0/3, place 6, 12 riders**, bike speed 0.763 gear 0, 69 rider bones, 18 bike pose nodes, camera rc 0 (612.56 118.25 -391.42 fov
0.943), particles rc 0, mud count 12 (`builds/camsnap/api_pf_full5000.log`). No stop: the 1252-frame 00649df0 stop is gone and nothing
replaces it in 5000 frames. The live tree at 13:4x cannot run the product for anyone: it hangs in `rx_world_activities_start ->
rx_activity_vf2 -> rx_audio_logic_vf29 -> cue_present -> rx_world_string_map_find` at load frame 0 (100% CPU; also seen by fxconstruct,
product_chain.md Round 6), and the same hang appears with this port removed from the link (`builds/camsnap/api_nosnap120.log`), so it is
not this unit. A later live build (14:18) got past the cue map and stopped at `00641f20` MXFirstPersonCamera::vf32, cameraschema's
in-flight first-person port; `core/build.sh` is clean on the final tree (`builds/camsnap/core_build3.log`).

## Firstperson (2026-10-20, agent "firstperson"): 00642680 vf19 ported; the slot is created; the pose branch is NOT exact yet

Build dir `builds/firstperson` (oracle CLI `builds/firstperson/cli`, product `builds/firstperson/api`, a no-first-person product
control `builds/firstperson/api_nofp`, logs `builds/firstperson/verify/`, backup `builds/firstperson/backup/`).

- **New files**: `core/game/loop_cam_firstperson.[ch]` (MXFirstPersonCamera::vf32 **00641f20** and vf19 **00642680**, registered;
  the vf19 body is a frame-slot transliteration of the disassembly: the frame `F = ((esp_entry-4)&~0xf)-0x1a0`, the locals at
  `F+offset`, the SSE float / double operations in the original's order, the x87 `fld/fstp` copies through `cam_x87_bits`, the
  callees 00646940 / 00623bd0 / 0042c230 / 00401630 / 00401000 / 00407410 / 0063dee0 / 0063d920 at the original stack depth),
  `core/oracle/loop_cam_firstperson.cpp` (adapters, group rx_loop), `core/oracle/fp_probe.cpp` (the live-state probe, group
  "thunk": at a frame it enables the local player's first-person camera `P+0x97c` (+0x2f4 = 1, +0x988 = 2, +0x9a8 = camera) so the
  game's own camera update calls the port with the pose branch taken).
- **Slot creation** (the minimal edit in the camera owner's file): `loop_cam_build.c`'s factory type 0 now allocates 0x490 and calls
  the 00641280 ctor (it returned 0 before), and `rx_player_cameras_create` (0054bc00) no longer skips slot 2. So `P+0x97c` is the
  MXFirstPersonCamera exactly where the original makes it. `00641f20` vf32 is the tuning apply of that camera (fov +0xe0, the
  +0x398 / +0x39c offsets, the head limits +0x3a0 / +0x3a4, the +0x3a8 / +0x3a9 `!= 0` bytes, +0x3ac..+0x3d8, +0x3e0 vec4, +0x3c0);
  it is required because the factory calls vf32 right after the ctor (the product stopped at it).
- **Verification (baton boot, `REFLEX_PORT=rx_MXFirstPersonCamera_vf19,rx_MXFirstPersonCamera_vf32,rx_FPProbeHook`,
  `REFLEX_PORT_VERIFY=1`)**: `00641f20` **1 call / 1 verified / 0 mismatches**; `00642680` **112 calls / 112 verified / 0
  mismatches**. BUT the boot's 112 calls all take the first instruction's early return: their camera `+0x2f4` ("pose valid") is 0
  (the UI-world/gallery camera is never selected; a probe stat of the adapter shows early 112 / posed 0). So the boot verifies the
  guard only, not the pose math.
- **The pose branch is not exact yet.** The probe (above) makes the game's own calls take the pose branch: 112 calls, 112 verified,
  **111 with mismatches** (~50 words: the camera entity's own pose +0x30..+0x68, e.g. `[14c5b9a0] 7fffffff orig bc741525`). The
  shared tail 0064643b is reached with a **different ESP per branch** (path A/C enter with ESP = B-4, branches B/D with B-12), so its
  matrix base / row-source slots shift by 8; the port parameterises this (`fp_off`) and the sound-object pointer was fixed
  (`[listentry+0x64]`, not the entry), but the remaining row sources of path A (F+0xbc in particular) are not written on that path:
  the original reads a deterministic dead-stack word there. Exactness needs a per-path stack model of the whole tail (and possibly of
  the +0x30..+0x68 entity-pose write). Do not treat the pose as verified; QUEUE's `### firstperson` says what remains.
- **No regression**: `verify.sh rx_loop quick` 7 ports, only the documented 0084ed20 pair (775/1200), On identical; `rx_race quick`
  48 ports, only 0055f700 (775/1200), On identical; `rx_world quick` 132 ports 0 mismatches, On identical. `core/build.sh` clean.
- **Product**: the first-person slot exists (lldb on the product binary at `rx_first_person_camera_vf32`: cam vtable **0095a80c**,
  `+0x340` 1, `+0xe0` = **0x3f860a91 = 60 deg**, matching the oracle's FirstPerson tuning; `+0x36c` the config, `+0x344` the
  controller) and vf32 + vf19 are reached. The live tree cannot run the session: `rx_api_test` hangs at load frame 8 in
  `rx_world_activities_start -> rx_activity_vf2 -> rx_audio_logic_vf29 -> cue_present -> rx_world_string_map_find` (also without the
  first-person slot in the link: `builds/firstperson/verify/api5_nofp.log` has the same `LBHANG` map walk), which is the concurrent
  audio work (build_activity_audio.c / world_node_audio.c / crt_cpp.c edited at 14:47-14:49; camsnap saw the same hang at 13:4x).
  The API has no first-person camera selection (`rx_session_camera` reads the active entity `P+0x9a8`; the game selects by `P+0x988`
  / the camera key), so a product getter comparison needs either the UI selection or a test driver that sets `P+0x988 = 2`.

### Firstperson round 2 (2026-10-20): tail ESP model fixed by a CFG map; head transform exact; the orientation block still diverges; vf28 is the product blocker

- **CFG ESP map**: the old linear `esp` simulation accumulated pushes from *untaken* quaternion-sqrt arms, so offsets after 00643668 were 4 too low (the shared tail looked like ESP B−12). A CFG walk (`esp_cfg.py`, worklist over both branch successors with each callee's RET N) gives the true per-instruction ESP. It shows both branches reach `0064643b` with **ESP = B**, so the tail is uniform: matrix at `F+0x140`, rows 1/2/3 at `F+0x80` / `F+0x50` / `F+0xc0`, row 0 = `F+0x60` (A/C) or `F+0xf0` (B/D). The whole body was regenerated from that map (the `builds/firstperson/backup` + the assembler in the scratchpad); the path A/C tail stores are at `F+0x50..0x68`, `0x80..0x88`, the B/D tail at `0xf0..0xf8`, and the 0063e400 ground clearance is `F+0xc0`.
- **Head transform is exact**: a probe calling the original 00623bd0 on the same state gives the *same* quat and position as the core's call (bit-for-bit: `bdd5fee1 bf363250 3f30963d bda82f41` / `43a94ac1 423aae4d 428b789c`). So the remaining divergence is inside the orientation composition.
- **Remaining pose bug**: the camera's basis vectors `+0x430 / +0x440 / +0x450` come out as NaN (`7fc00000`) in the core while the original's are finite. Traced to the quat-from-basis block: the candidate register `x2` (the normalized cross "rx") is **0** in the core, so `0.25 / sqrt(0)` = inf and the quaternion becomes NaN. The first visible wrong value is the basis vector used at `006432bf`/`006433f8` (all four candidates 0 in the core; the original's are nonzero). Next step: a thunk probe at `006433f8`/`00643541` that dumps the original's frame slots and a matching dump inside the core, then compare slot-by-slot — the register/slot reuse in the mechanical transcription of 006430xx-006435xx is the suspect (not the tail, not the head).
- **Verify**: baton boot `00641f20` **1/1/0**, `00642680` **112/112/0** (all 112 still early-return, adapter stats 112 early / 0 posed). Live probe (enables `P+0x97c`): 112 verified, **111 mismatched** (same ~50 words; position row3 is close, rows 0-2 NaN). No regression: rx_loop quick 9 ports (all the 0084ed20 hooks, documented 775/1200), rx_race quick 48 ports/2 documented, rx_world quick 132 ports 0, all On identical; core/build.sh clean.
- **Product**: the tree's audio hang is fixed (loadfix); `rx_api_test` now runs **1200 frames, rc 0**. The first-person slot exists and is selectable through the game's own field: `core/tools/fp_camera_test.c` (test-only driver, linked against the product objects; it sets `P+0x988 = 2` / `P+0x9a0 = 0` before each advance, never changes the default camera) shows the camera active (`P+0x9a8 == P+0x97c`, vtable 0095a80c) with **fov 0x3f860a91 = 1.04719746 rad (60°), near 2, far 15000** — the oracle's FirstPerson tuning, bit-exact. **But selecting it in the product aborts at `00641460` MXFirstPersonCamera::vf28** (slot 0x70, the activation; 2339 B, calls `FUN_0054d8c0` 191 B → `FUN_0054d820/870` + `00621490`, and `00623bd0`), and vf29 `00641d90` (56 B → `0054d980` 69 B) is the deactivation. Those are not part of the vf19 task and remain unported; with them the product can pose and the getter comparison becomes possible (the driver's `+0x2f4 = 1` workaround only bypasses the activation in the probe, and the pose then still shows the orientation bug above).

### Firstperson round 3 (2026-10-20): the pose branch is reachable only in the event world; the composition's x component is the remaining bug; vf28/vf29 are the product activation

- **The boot's 112 first-person calls are UI-world calls without a vehicle.** Forcing `+0x2f4 = 1` on the boot's `P+0x97c` (the
  A/B probe) makes the original's own `MXPlayerCamera::vf19` (00646940) die at `00646965`: `[controller+0x1ac] == 0`. So the UI
  world's first-person camera has no vehicle; the boot can never pose it, and the `112/112/0` stays a guard-only result. A posed
  call needs the event world's camera (after the race start), where the controller has a vehicle.
- **Where the composition goes wrong.** At the candidate block `00643541`, the original's `xmm2` is the *squared* first basis
  component (`006433a4 movaps xmm2,xmm1`; `006433b6 mulss xmm2,xmm1`), and the candidate q0..qw are built from it, the up vector
  `F+0x14`/`F+0x28` and the `+1.0/-0.0` terms. In the core, the inputs at that label are `F+0x10 = 0x00939fa3`,
  `F+0x14 = 0x00928936`, `F+0x18 = 0x0090d652` (denormal/pointer-like), `x2 = 0`, `x1 = 1.0`; the cross that should produce those
  slots (`006432bf`: out F+0x110; `006432da`: out F+0x10) yields denormals, so all four candidates are 0 and `0.25/sqrt(0)` = inf ->
  the basis `+0x430/+0x440/+0x450` NaN. The head quat and the negations are exact (probed). The fault is therefore in the
  quaternion-composition block 00642e49-006433f8 (a missed/extra operand or a wrong slot in the mechanical transcription), not in
  the tail, the head transform or the tail's ESP.
- **Probe state.** The A/B probe (oracle/fp_probe.cpp, RX_FP_AB) now guards controller/vehicle; it still aborts on the UI-world
  camera (no vehicle), which is the evidence above. For the next pass, run the SELECT probe in the *event world* (after the load)
  and A/B there; the boot's UI world cannot exercise the pose.
- **Product activation.** `fp_camera_test` selects with `P+0x988 = 2` (the game's camera key path); the slot becomes active
  (`P+0x9a8 == P+0x97c`, vtable 0095a80c, **fov 0x3f860a91 = 60°**, near 2, far 15000) but the product aborts at **00641460
  MXFirstPersonCamera::vf28** (the activation; 2339 B). Its chain is `FUN_0054d8c0` (191 B: `0054d870`/`0054d820` 73 B each,
  `00621490` -- already ported as `rx_world_rider_end_pose_override` -- and `00623bd0`), plus the camera's own `vf19` at 0.0333 and
  the head-quat composition. `00641d90` vf29 (56 B -> `0054d980` 69 B -> 0054d820/870) is the deactivation. These five small
  functions + vf28's body are the remaining product unit; all their leaves are ported except the 0054d8xx family.
- **Runs this round**: baton boot `00641f20` 1/1/0, `00642680` 112/112/0; rx_loop quick (only the 0084ed20 hooks, 775/1200),
  rx_race quick (2 documented), rx_world quick 0, all On identical; product `rx_api_test` 1200 frames rc 0 (default gate camera
  unchanged, fov 0.785); `core/build.sh` clean.

### Firstperson round 4 (2026-10-20): vf28 / vf29 / the 0054d8xx sound chain ported and wired; the product selects first person without aborting; the vf19 composition is the only pose bug left

- **New ports** (in `loop_cam_firstperson.[ch]` + oracle/loop_cam_firstperson.cpp, group rx_loop): **00641460** MXFirstPersonCamera::vf28
  (activation: vf19 at dt 0x3d088889, `+0x2f4` = 1 / `+0x380` / `+0x384` = 0, the sound state on, the cut-in fields, the head
  transform, its quaternion turned into the three basis rows +0x430 / +0x440 / +0x450 and the +0x460.. copies, the raw quat in
  +0x410 / +0x418 / +0x420 / +0x428, the class byte +0x38c); **00641d90** vf29 (deactivation); **0054d870 / 0054d820** (the player's
  view sound objects: level entity [engine + 0x19b8 + i*4] -> +0x6c -> +8 / +0xc); **0054d8c0 / 0054d980** (the sound state on / off,
  player +0x230, global 0xa94244, the rider's end-pose override). vf28 is `ret 4` (the old camera argument, unused); vf29 `ret`.
  The game's own camera-selection path (loop_player.c slots 0x70 / 0x74) dispatches through RX_FN, so registering them is the
  call-site change; the default camera is untouched.
- **Verify**: vf29 **120 calls / 120 verified / 0 mismatches** (Concord world, the SELECT probe forcing the switch); vf28 **1 call /
  1 verified / 1 mismatch** (the only differences are the pose fields written by its internal vf19 call, i.e. the composition bug
  below; the ESP / return address now match - the earlier 4-byte difference was `m.ret(0)` vs the original's `ret 4`). The
  0054d8c0 / 0054d980 writes are inside those two functions' memory comparison (called core-to-core, so their adapters see 0 calls).
  vf19 still 112/112/0 in the boot (guard-only) and, in the event world with the pose branch forced, 900 calls / 121 posed / **121
  mismatched** (NaN basis). vf32 1/1/0.
- **Product**: `fp_camera_test` now selects first person through `P+0x988 = 2` and the camera **poses without aborting**
  (`pose2b0` = 1, fov 0x3f860a91 = 60 deg, position 639.93 117.47 -379.54). Its orientation is still NaN - the vf19 composition bug
  reproduces in the product. The countdown keeps the gate camera active (`active` 0), so the getter still reports the gate; the
  first-person entity is posed regardless. `rx_api_test` 1200 frames rc 0, default camera unchanged (gate, fov 0.785).
- **Composition diagnosis**: vf28 calls the head transform at activation; there the quat is the identity (the rider pose is still
  the bind pose), so vf28's own basis rows are zero - as the original's would be - and that is not the bug. vf28's simple
  head->basis code is a correct reference for the three rows; vf19's 00642e49-006433f8 block builds the *same* kind of rows from the
  head quat and the vehicle axes and produces NaN in the event world where the head quat is valid. The next pass should compare
  vf19's block against vf28's formulas slot by slot (the candidate `xmm2` zero, 0.25/sqrt(0) = inf).
- Runs: rx_loop quick (9 hook ports only), rx_race quick (2 documented), rx_world quick 0, all On identical; core/build.sh clean
  (a concurrent api/rx_session.c edit broke it for a minute; it builds again).

### Firstperson round 5 (2026-10-20): vf28 / vf29 / the sound chain verified; the composition evidence narrowed to the 00642e49 block

- **Activation chain, verified.** vf29 **120 calls / 120 verified / 0 mismatches** (Concord, the SELECT probe). vf28 **1 call / 1
  verified / 1 mismatch**: the only differences are the pose fields written by its internal vf19 (the composition bug); its own
  fields (the sound state, the cut-in resets, the +0x410/+0x460.. copies, the class byte) and the ESP / return address match. The
  0054d8c0 / 0054d980 writes are compared inside those two calls.
- **Product**: `fp_camera_test` selects the first-person view (`P+0x988 = 2`) and the camera poses without aborting: `pose2b0 = 1`,
  fov 0x3f860a91 (60 deg), position 639.93 117.47 -379.54, near 2, far 15000. Orientation NaN (the composition bug). The countdown
  keeps the gate in the getter; the first-person entity is posed regardless. rx_api_test 1200 frames rc 0, default camera unchanged.
- **Composition evidence (Concord event world, debug dumps).**
  - The vehicle basis read at the prefix is orthonormal (x6 x7 x4 x5 x3 x2 = 0.9960 -0.0255 0.00096 0.9618 -0.2753 -0.0856; the two
    vectors are ~1.0 long, their cross is ~1.0).
  - The head quat is valid and *not unit* (0.159 -0.477 0.627 0.526, |q| = 0.9605; the original's is the same, the transform is
    bit-exact).
  - After the head-axis products the core's basis slots are `F+0x50 = F+0x20 = (-0.1018, 0.1522, -0.2001)` (length 0.27, not the
    expected ~0.92-scaled axis) and `F+0x80 = (0.00000044, 0.00000017, -0.00000011)` (denormal), written by the 00642e49 block;
    the candidate block at 00643541 therefore sees all-zero candidates and `0.25/sqrt(0)` = inf.
  - The 00642e49 block's formulas in the generated C use the head quat w (`x0 = FS(0x3c)`) and the Qa slots; the expected X axis
    from those values is (-0.319, -0.152, 0.701) but the written value is (-0.102, 0.152, -0.200), so a *slot/operand* in the
    mechanical transcription of that block is wrong (the `x0` / `x*` register reuse of the lifted code is the prime suspect), not
    an uninitialised original input. The vf28 head->basis rows (our correct port) are the reference for the same products.

### Firstperson round 6 (2026-10-20, agent "fpbasis"): the vf19 pose is bit-exact; the transcription`s constant reads, float/double copies and lahf flag tests

Build dir `builds/fpbasis` (oracle CLI `builds/fpbasis/cli`, product `builds/fpbasis/api`, probes under `builds/fpbasis/ab`, `sel`,
`wsel`, scripts `run_ab.sh` / `run_sel.sh` / `api.sh`). Backup of the file before this round: `builds/fpbasis/backup/`.

The pose bug was not one operand: the generated body of 00642680 had four classes of mechanical transcription errors, all in
`loop_cam_firstperson.c` (the branches share them; every fix is from the disassembly, no constants guessed):

1. **The game constants were read as bit patterns, not memory.** Every `movss xmm, [0x009xxxxx]` had been transcribed as
   `rx_bf(0x009xxxxx)`, which reinterprets the *address* as float bits (0x9cb4a0 -> 1.4e-38). 229 sites. That is why the
   head-quat products produced denormals and the 00643541 candidate block saw zeros (0.25/sqrt(0) = inf). They are now
   `FK(addr) = F32(0, addr)` (a game-memory read, as the original; .data above 0x9a6000 is mutable, so a literal would be wrong
   there). The only remaining `rx_bf(0x...)` is the genuine bit pattern dt 0x3d088889.
2. **12 `movapd xmmN,xmmM` copied as float.** The C wrote `xN = xM` (the float view) where the original copies the *double*; the
   next `subsd/mulsd` then used the stale double. The two that broke the pose: `x5 = x4` at 0064296b (first-stage F+0x2c) and
   `x7 = x4` at 00642c1c (second-stage F+0x2c, the S2c slot) - the F+0x2c fed the R02/R12/R22 sums. The other ten are in the
   asin/atan polynomial (0064392a/3a/72/8e, 00643a00, 00643d52), branch C (0064471b) and branch D (00645cc9, 00645f98). All 14
   `movapd` instructions in the function now map to `xNd = xMd`.
3. **`ucomiss / lahf / test ah,0x44 / jp` is not "unordered".** `test ah,0x44` sets PF from the parity of (ZF,PF); `jp` is taken iff
   *both* are clear, i.e. `a != b` **ordered**. The six sites in the largest-diagonal quat selection (006435d8, 00643666,
   00644f86, 00645020, 006456f9, 00645787) were `if (fp_unord(a,b))`; they are now `if (!fp_je(a,b))` (NaN / equal fall through,
   as the original). This alone made the candidate quat pick the wrong arm (the original's 0064375b, the core's 006435e7).
4. **9 conversion-source slips** `(float)xNd` reading the wrong double: 00643932 (x1 = (float)x4d), 00643942 (x4 = (float)x5d),
   0064397a (x6 = (float)x7d), 006439f6 (x2 = (float)x5d), 006439da/... (x0 = (float)x1d, both at 006438da/006438ea) and the four
   branch-C `x5 = (float)x7d` (0064474c, 006447ac, 00644804, 0064486a). A script now checks all 97 double->float and 393
   float->double register conversions against the disassembly (0 mismatches).

**Verification.**
- A/B probe (`RX_FP_AB_ORIG` / `RX_FP_AB_CORE` on the event world, frame 700): the original`s and the core`s frame F and camera
  are **byte-identical** (the frame: F+0x10..0x18, +0x30..0x3c, +0x50/54/58, +0x60/64/68, +0x80/84/88, +0xc0..c8, +0x110..118,
  +0x180..18c; the camera: +0x30/40/50/60 rows and the +0x430/0x440/0x450 basis).
- Event/world-image live probe (`REFLEX_WORLD_TEST=.../National_NAT_Concord`, the SELECT probe forcing +0x2f4):
  **900 calls / 900 verified / 0 mismatches** (780 posed, adapter stats); the event boot with `REFLEX_AFTER=900`: 350/350/0
  (880 posed). vf29 **120/120/0**. The baton boot: vf19 94/94/0, vf32 12/12/0. vf28 is not re-reached by these runs (0 calls);
  its only round-5 mismatch was the pose written by its internal vf19, which is now exact.
- `verify.sh rx_loop quick`: 11 ports, 0 mismatches, On identical; `rx_race quick`: 48 ports, 0, On identical;
  `rx_world quick`: 132 ports, 0, On identical; `core/build.sh` clean.
- Product: `fp_camera_test` 1400 frames: the first-person camera poses (`pose2b0` 1, fov 1.04719746 = 60 deg, near 2, far 15000)
  with a **finite, unit basis** (r 0.9648 -0.0077 0.2630, u -0.1611 0.7729 0.6138, f -0.2080 -0.6345 0.7444; the +0x430 row
  equals f) and position 615.54 117.08 -386.34 - the NaN orientation is gone. `rx_api_test` 1209 frames rc 0, default camera
  unchanged (fov 0.785398185, the start gate).

**Probe fixes (development only):** `oracle/fp_probe.cpp`'s A/B passed `P + 0x97c` (the pointer *field*) as the camera instead of
`rd(P+0x97c)`; the A/B now also dumps the vehicle basis (`dump_veh`) and can run the core alone (`RX_FP_AB_CORE`), the original
alone (`RX_FP_AB_ORIG`), or force the branch byte (`RX_FP_AB_BRANCH`). These are the tools for the next pose work.

## Camcycle (2026-10-20, agent "camcycle"): the User view's per-frame tweak `00649ed0`; the C key cycle is complete

The Camera key's cycle (`Player_UpdateCamera` 0054c150, `+0x988` 1 -> 2 -> 4 -> 0 -> 1) reached `MXThirdPersonCamera::vf30`
`00649ed0` as soon as it selected entity 4 (the "User" view): `Player_UpdateCamera` calls slot `0x78` on the active camera and
again on `P+0x984` every frame while `P+0x988 == 4`. That function was the last unported camera method in the product's key path.

- **`core/game/loop_cam_tpc.c`**: `rx_third_person_camera_vf30` 00649ed0 (ECX camera, stack controller index, dt; RET 8). The
  dolly key map moves the distance offset `+0x368` (-25 dt, at least -2.0 from .data 009d7b30 / +25 dt, at most 15.0), the H
  control turns the free-look yaw `+0x354` by -25 deg/s x dt (wrapped to (-pi, pi]), the vehicle forward `+0x130` is kept in
  `+0x3a0..+0x3a8` (copied while the vehicle is not airborne, `vf43` 466ae0 = 0, or when the stored vector is zero; in the air it
  keeps the takeoff direction), the look-point offset `+0x500..+0x508` = normalize(forward x up) x sign(yaw) x
  min(4, 15 |cos(yaw + pi/2)|) (the 6-term even polynomial at 0x935d3c..0x935d50; 4 at 0x9322c0), and the V control moves the
  `+0x35c` angle spring's target `+0x360` by 25 deg/s x dt, clamped to +-20 deg (0063c7b0 / 00508b70).
- **`core/game/loop_cam_misc.c`**: the three key-map queries it calls, `rx_camera_hcontrol` 005486c0, `rx_camera_vcontrol`
  00548710, `rx_camera_dolly` 00548760 (KeyMap vf2 at +8 on input `[0x9c52c4 + 4 i]` + 0x3f8c / 0x3d8c, 0x398c / 0x3b8c,
  0x33a0 gating 0x358c / 0x378c; +1.0 / -1.0 / 0.0). `Controller_BuildKeyMaps` does not bind the UserCam* columns, so in the
  shipped worlds these return 0; the paths are ported exactly anyway. Adapters: `core/oracle/loop_cam_tpc.cpp` /
  `loop_cam_misc.cpp`, group rx_loop.
- **Probe** (`core/oracle/fp_probe.cpp`, `RX_FP_PROBE=1 RX_FP_PROBE_USERCAM=1`): finds the local player, calls the game's
  `Player_SetCamera(4)` once (rc 1), keeps `P+0x988 = 4` and once calls the three callees at top level (so Verify sees them).
- **Verify** (Concord quick, the probe selecting the User view at race frame 781): `rx_MXThirdPersonCamera_vf30` **535 / 535 / 0**
  (both calls per frame), each callee **1 / 1 / 0**; with the probe's own side effects matched, On-mode memory hash
  `ccb1d9b06e6f7c27` (27314 pages) and the state hash identical. The first transcription error was the `vf43` polarity: copy
  the forward when `vf43 == 0` (not `!= 0`).
- **Product** (`core/tools/fp_camera_test.c` mode `cycle`, `builds/camcycle/api`, `RX_FULL_FRAME=1 ... 1400 cycle`): four C
  presses through `rx_session_set_input` give 1 -> 2 -> 4 -> 0 -> 1. Each camera is finite with a unit basis:
  entity 2 first person fov 1.04719746 (60 deg) / near 0.5 / far 15000; entity 4 user 0.950666845 / 2 / 15000; entity 0 visual
  0.894581378 / 2 / 15000; entity 1 game play 0.953325689 / 2 / 15000 (speed-dependent curve at the selection frames).
  The default run never selects (988 stays 1, the GamePlay camera); all four runs end at 1400 frames with the same gameplay:
  bike speed 28.0988579, gear 0, state 2, laps 0, place 12, time 4.585001, bike pose `2e2e07f8`, rider pose `94b5cd41`.
- **Regressions**: `verify.sh rx_loop quick` **18 ports, 0 mismatches, On identical**; `rx_world quick` 132 / 0; `rx_race quick`
  48 / 0; `core/build.sh` clean. Logs `builds/camcycle/logs/`.

## Introcamera (2026-10-21, agent "introcamera"): the intro fly-by camera is posed in the product

Build dir `builds/introcamera/` (product probe `probe_new/probe`, product driver `api2/rx_api_test`, oracle CLI `cli2`,
logs `logs/`, verify `verify/`, backups `backup/`). The camera functions were already ported; the product's intro left
`world+0x218f0` unposed (`+0x2b0 = 0`) for three separate reasons, found in this order:

1. **The run gate in `0063d7e0` (loop_cam_player.c).** The scenario *did* tick the world camera all along (`007473f0` calls
   its slot 0x4c every intro frame and `0055d290` set the path clip), but `rx_animated_camera_vf19`'s inlined pause test read
   the PCSession `[0x00da6cb0]` and the network / device manager `[0x00da6cac]` — both created by `006de390` (Quazal / Steam,
   absent in the product) — plus the window focus `0x00a9596c` (set by WM_SETFOCUS, the product has no window). With the
   pointers null the test dereferenced the arena's zero page (`U32(0,0x58)` was 0x3f000000) and returned "paused", so the
   path was never sampled. **Product rule** now matches the documented pause boundary of `006e0c50`
   (`build_activity_game.c`): a headless product run is the focused, offline game with no menu; the game's own pause value
   (`MXGame +0x745c -> +0x288`) still holds the camera. The ORACLE keeps the original test verbatim (Verify unchanged).
   This is what made `rx_session_camera` return rc 0 with a moving pose again.
2. **The AnimationSets alias table.** The intro scripts' `CameraAnimation` events name a *set* (`"CameraStartLine"`,
   `"CameraHeroBike"`, ...) resolved through `006b6a30` / the sorted table `[0xd5a9e8]`. The table is built by `006b68e0`
   from `AnimationSets.bxml` in the init thread `0075ed60`; the product's init-thread stand-in (`api/rx_session.c`,
   `build_package_loaders`) skipped it (its comment called it "not a gameplay step"). Without it every step after the first
   kept step 1's clip. Ported as `rx_anim_sets_load` (`anim_ik_ent.c`, next to `rx_anim_alias_pick`): the same parse —
   0x114-byte entries (name at +0, lower case, 0x104; +0x104 count; +0x108 pointers; +0x10c names; +0x110 per-name flag
   byte, bits 1 HasTimeline / 2 HasVehicleAnim / 4 IsLooping), sorted by `_stricmp`, growth +0x32 entries — and called at
   the original init step (before `ClipRecorder_Init`).
3. **The display aspect.** `0063e850` converts the path's horizontal fov track through the display's width / height
   (`2 atan(tan(a/2) / aspect)`). The XenonCore ctor leaves 640 x 480; ReflexWinMain's `Display_SetMode` with
   `GraphicsConfig_Load` `graphics.cfg` gives the install's 1280 x 720. `rx_engine_startup` now writes the display block
   (`ctx + 0x44 / +0x48`) from the player's config values (the GraphicsConfig parser itself is still the QUEUE build6
   stand-in). With 640 x 480 the intro fov was 33 % wide; with 1280 x 720 it matches the oracle exactly.

Two leaf functions became reachable once the path ran (both ported, registered, adapted in `oracle/anim_character.cpp`,
group rx_anim):
- `008435f0 FloatKeys_FindSegment` (`anim_character.c`): the 8-byte (time, value) segment finder `00839f40` calls for an
  interpolated value track; the direct evidence is the exact intro fov below.
- `00835170` (`rx_character_random_motion_time`): `rand()/32768 * clip duration` into the character's motion time / previous
  time (ESI = the character); the alias table now lets the intro's `PlaceEntity` handler place the CardGirl / crew chief,
  whose `0055e360` calls it for `HasVehicleAnim` clips.
`008403b0 rx_rider_bind_anim_tracks` was implemented but not registered; it is registered now and the pose-override call
site (`race_misc_pose.c`, `0047c0b0`) calls it directly instead of through `rx_call_original_ex`.

**Verification.** Product (`probe_new`, `RX_FULL_FRAME=1`, 12 riders, no input): 1115 pre-race frames; the getter returns
**rc 0 on 1104** of them (the first 11 are the load frames before the intro starts, where `+0x2b0` is still 0 -> rc 2, as in
the original), no NaN; fov 0.196 .. 0.942, position x 585.4 .. 682.0, y 115.6 .. 144.8, z -406.4 .. -273.0; the world
camera `+0x2b0 = 1`, a new clip per step (`NAT_CO_AF_Intro_Camera`, `NAT_BE_BF_Startline_04_Camera`,
`NAT_BE_BF_Hero_Bike_Camera_02` in this tree). Oracle (baton boot, same event spec `National,10,NAT_Concord,50,0,2,Laps,3,0`,
12 riders, AI 95, 1280 x 720): at the same path clocks the first clip matches **bit for bit** — t 1.067
pos 681.998718 144.760788 -272.999481 fov 0.603053212; 1.901 fov 0.530713618; 2.734 fov 0.47031492. The second clip matched
bit for bit in the run where the random set pick agreed (t 0.900001 pos 609.672058 121.053665 -362.116882 fov 0.750428617).
The hero clip's fov matches at t 0.566 (0.202418178); its position is the hero entity's base matrix (`+0x460`), which follows
the vehicle / grid — the concurrent tree moved the grid ~20 units after the matching snapshot, so the base (and so the pose)
differs there, while the path / fov pipeline is identical. The step variant is a legitimate game LCG pick (the reference
itself picks different `CameraHeroBike` variants per run; the product's headless frame skips render-only rand consumers, so the
states differ). **Regression:** `verify.sh cli2 quick` rx_anim 44 / rx_loop 18 / rx_world 132 / rx_race 48 ports, 0 mismatches,
On identical; baton-boot Verify 146 ports, only the documented `007d2f70` 81-word difference, world image identical, canonical
VERDICT IDENTICAL (12/12 samples, state hash equal; memory hash is the known heap class). `core/build.sh` clean.

`firstride` (steer pass, 2000 frames, this tree): the camera check now sees the intro fly-by (`rc 0 on 1441 frames`, distance
up to 157.8, fov 1.0538) and its `< 60` distance bound is a pre-intro-camera assumption — the racing-state fov evolution
(0.785398 .. 0.786698) is identical to the baseline. That run stops at `0086f2a0` (a cylinder-vs-HF-terrain pair through an
`rx_call_original` path) at race 283 on the concurrently edited collision tree; the intro itself completes and the camera is
fine (collision unit's triage, not the camera).

## Rideranim2 (2026-10-21, agent "rideranim2"): the rider pose setters 00621100 / 006211d0 ported; the ride passes the old stop

Build dir `builds/rideranim2` (oracle CLI `builds/rideranim2/cli`, product `builds/rideranim2/api`, product dylib
`builds/rideranim2/lib`, logs, backup of the edited files in `backup/`). cameraselect's product ride stopped at unported
`00621100` about 80 racing frames after a camera swap, in every mode; it is `RiderAnim_UpdateState`'s (0061f160) slow-roll
step: `00621100` sets the slow-roll pose (+0x5a8), `006211d0` (the same ladder's boost/crouch branch) enters the crouch pose
(+0x5a2 / +0x5a4). Both were ported in `core/game/world_rider_62.c` (owner file) with prototypes in `world_rider.h`.

- **00621100 `rx_world_rider_slow_pose`** (ESI = ra; frame `push ecx`): `+0x5bb58 = 0`; the property listener `+0x4d0` gets
  the range value of `(+0x4c0, +0x4c4)` (`-max` when max < 0, else `-min + max` when min > 0, else 0; the `comiss / jbe`
  ladders put NaN on the 0 side); the dirty byte `+0x57c` = 1; the pose property `+0x580` gets the u16 `+0x5a8` **directly**
  (not through ShortProperty_Set 0063b6c0, which is the ladder's other path); the blend output `+0x598` is snapped to
  `clamp(0)` (the same ladder as `pose_blend_reset`); chain 5 fades out over 0.25 when `+0x5bab8 == 0`.
- **006211d0 `rx_world_rider_crouch_pose`** (EAX = ra; frame `sub esp, 8; push esi`): `+0xaf54 = 1`; listener `+0x548` gets the
  range value of `(+0x538, +0x53c)`; the pose property takes `+0x5a2` when the lean threshold `+0xaf78` is above the lean
  property `+0x38c` (`FloatListeners_Get`), else `+0x5a4`; the dirty byte is set; chain 5 fades out over 0.25 (unless
  `+0x5bab8 == 0` fails); then the ride trick state: `+0x5b594 = old +0x5b590` (with `+0x5b5e8` / `+0x5b5ec` cleared when it was
  1), `+0x5b590 = 3`, `+0x5b640 = 1.5`; the blend output is snapped (`pose_blend_reset`).
- Callees: all ported (0063b830 `rx_world_float_listeners_set`, 0063b8f0 `rx_world_float_listeners_get`, 006235e0
  `rx_world_rider_chains_fade_out`). The two `0061f160` call sites are switched to direct calls at the original stack depth
  (`WORLD_CALL_SP(0, ...)`: the original `mov esi, ebx; call` / `mov eax, ebx; call` with no stack arguments).

**Verification.** The five reference worlds **never** reach either setter (the ladder needs a rider rolling at 3 .. 20 for
over a second, or a boost; world Concord 1800: `calls 0` for both). New development probe `core/oracle/rideranim2_probe.cpp`
(group `thunk`, `RX_RA2_PROBE=1 RX_RA2_FRAME=1200`): at a frame it takes the live rider animation from the workers' table
`0x00a9408c`, builds the ladder's branch state, and calls the original `0061f160` (peeled with `-0061f160`) so the original
reaches the installed setters; Verify then compares core vs original per call. Variants: the range below 0 / above 0 / 0 / NaN,
`+0x5bab8` 0 / 2 (chain fade or not; the ladder itself requires != 1), both crouch poses by the lean threshold, and
`+0x5b594` 1 / 2 (the two trick words). Counts (calls / verified / mismatches, 0 everywhere):
- Concord 1800 (`REFLEX_PORT="rx_world,rx_RiderAnim2ProbeHook,-0061f160"`): 00621100 **4/4/0**, 006211d0 **4/4/0**.
- FreeRide_FR_Alpine 1500 (same, frame 1000): 00621100 **4/4/0**, 006211d0 **4/4/0**.
- Not exercised: the null-pointer guards of the dirty byte `+0x57c` / the pose property `+0x580` (live riders always have
  both; two `if (p)` guards, reviewed against the disassembly).
- No regression: `verify.sh <cli> quick` on Concord: rx_world 132 ports, rx_anim 44, rx_race 48, rx_loop 18 - **0
  mismatches, On identical** (`builds/rideranim2/verify_*_quick.log`).

**Product.** `fp_camera_test` (the cameraselect driver, the camera swap that used to abort 80 racing frames later) runs
**1400 frames** in all three modes (default / api / key) with no stop; before the port all three aborted at frame 1205 with
`00621100` (`builds/rideranim2/api_{default,api,key}_1400.log`; the api and key runs' fingerprints are identical).
The 12-rider W + A/D first ride (`builds/firstride` steer pass, live-tree dylib `builds/rideranim2/lib/librxcore.dylib`,
`RX_FULL_FRAME=1`, 2400 frames): **1011 racing frames**, max speed 75.10, gear 3, path 643.7, no stop at the old point; the
next abort is `7859afc0 _CIpow` (CRT, race 1010), and the bike getter's position / velocity turn non-finite from race 287
(the vehicle / collision side; 287 is the frame after the old `0086f2a0` pair stop that terrainpair ported meanwhile; rider
pose reads stay finite - 0 of the 723 counted reads are the rider pose). A 2000-frame product run hangs above race 1400 in
the live tree's string-map walk (`rx_activity_vehicle_entered -> rx_race_trigger -> rx_race_holeshot ->
rx_world_event_dispatch -> rx_world_registry_find -> rx_world_string_map_find`), the same signature as the concurrent
vehicleentered agent's run - not this unit.

## trickplayback (2026-10-21): 00764f80 MXRecords_FailedJump and the 00588b50 score drop reached from the trick abort / wreck

The trick abort (fx_stunt.c 006c9190) calls **00764f80** MXRecords_FailedJump when a scored air trick is dropped (EAX = the
player's +0x1f0 index, ECX = the records base [0xa946c0]); the port is loop_player.c's `rx_records_failed_jump` (the same
0x18-byte {table, row} entry and `pl_records_add` as 00764f40 AttemptedJump, column 0x96d9cc "TotalFailedJumps"). The
scorer's wreck handler (006cb720) calls **00588b50** (fx_stunt.c `rx_fx_stunt_score_drop`): the combo score-drop event to
the manager [0xa94318] (type 0x9cbeb4, name 0x92fce0, payload the player, 4 bytes), which was the product's stop after the
first 323 jump wreck. Both are reached only in the product ride (the reference worlds never join an activity / build the
stunt manager); the long 20000-step ride runs with no unported address. details: notes/fx.md "trickplayback".
