# Original-world runtime (libreflexworld)

Runs the **unchanged original** MXReflex.exe input update and outer physics update
over a captured live world: an x86 interpreter (`vm.cpp`) executes the original code
and the genuine VC90 runtime (msvcr90/msvcp90 9.00.30729.1), with exact x87
(`f80/`, 80-bit, precision control and rounding modes) and SSE semantics. Nothing
in the physics is re-written by hand; correctness is checked byte-for-byte against
the original executable running under the replay harness
(`_re/trace/original_replay_world_harness.c`).

Per frame (`World::frame`), as `MXWorld::vf5` does: `XenonInputManager::vf43(dt)`,
`World_PhysicsUpdate` (00769b80, EAX = context, stack dt), then the animation-job
kick (0061d060) and wait (0061d130). The rider's animation tree (including the
leg IK, `InverseKinematicsAnimator`) is evaluated by a worker thread (0061cdb0)
that the main thread blocks on; the runtime runs that worker's job to completion
when it is signalled (serialized, on its own stack, with the worker thread's own
x87 CW 0x027F / MXCSR recorded from the live thread, header words 11/12).

Rider pose (`reflex_world_rider_pose`): physics writes the driven bones' world
matrices; the remaining nodes are completed with the game's lazy node evaluator
007db890, as the renderer does. Evaluating it does not change the physics
(verified bit-identical over 600 frames). The riding rider is `RiderAnimation`
(vtable 0x958724; node set at +0x180: records +0x188, count +0x18c, LOCAL +0x198,
WORLD +0x19c, inverse bind +0x1a4); the start-gate `RiderCharacter` is torn down
after the post-spawn transition.

## Whole-frame mode (the full game loop, rendering excluded)

A world image captured at a frame boundary (`--frame-dumps`: `Engine::Update` entry,
header word 13 = engine 0xd728d0, word 14 = 'FRME') runs whole original frames:
`Engine::Update(engine)` (0084ed20): UpdateTiming (the game measures its own dt from
the performance counter, with its 5-frame smoothing and 1/60 s frame cap), input,
scene entity tickers, `MXGame::Update` (state machine -> MXWorld::vf5 -> physics and
the rider animation jobs), event dispatch, engine tasks and `SyncRenderData` (the
simulation -> render handoff). The plugin selects it automatically for such images;
`reflex_world_frame(dt)` advances the virtual performance counter by dt.

The game's worker threads run serialized (signalling a worker's event runs its job to
completion on its own stack, with that thread's recorded FPU state, static TLS block
and CRT `_tiddata`; the dump's thread table records them): animation jobs (0061cdb0),
scene tickers (007d22a0), the engine job pool (00881900), the engine task thread
(0084f320), the FMOD queue thread (00806f90) and the render-sync workers (007df370).

Not reproduced (no device): audio (FMOD commands are consumed without effect; game
callbacks carried by the audio queue still run), and GPU work: rendering itself and
the GPU-rendered terrain deformation (HFTerrainSurface::vf14, wheel ruts), so the
collision deformation grid stays as captured.

Live-game oracle (one whole frame, `_re/trace/compare_live_frame.py`, capture
`20261004T163709.393254Z-alpine-frame2`, frame-entry vs frame-exit at serial 253 with
the live clock delta): bike simulation 2,999/2,999 changed bytes reproduced, 0
mismatched; RiderAnimation 734/734; both rider skeleton sets (local + world) exact.
Unreproduced live changes are audio buffers (FMOD mix/stream memory), job-pool slot
copies, and a simulation snapshot kept by the capture proxy's own tracers.

Robustness (aggressive random driving: full throttle, hard steering and leans, brakes,
whips, nose-heavy landings and wrecks), whole frames, 60 s per run: Alpine 64/64 runs
clean, NAT_Concord 24/24 clean (after adding the bit-test and string instruction
families, critical-section creation, the Steam interface accessors and the sound
database stream loader stand-in found by these runs).

## Levels

| Level | World image | Deformation grid | Notes |
|---|---|---|---|
| FR_Alpine (free ride) | `alpine-world.bin` from `20261004T163709.393254Z-alpine-frame2` serial 253 | `alpine-deform.bin` (= live surface) | terrain spacing 2 |
| NAT_Concord (National, 1 rider, AI off) | `concord-world.bin` from `20261004T192509.288833Z-concord-gate` serial 322 (`--full-dump-skip 1`): starts in race state Pre on the gate, Countdown at ~frame 840, gate drop ~frame 1120 (board time is random) | `concord-deform.bin` (`terrain/deform-000.bin`) | terrain spacing 1; heightfield equals the live grid |
| NAT_Concord mid-race | `concord-midrace-world.bin` from `20261004T173239.255417Z-concord-frame1` serial 231 (already InProgress, race clock 18.2 s) | `concord-midrace-deform.bin` | |

National capture: `runtime/run.py ... --event national --world NAT_Concord --bootstrap freeride --confirm-loading --frame-dumps`
(the bootstrap's event is now read from the ini: type, ids, level, activity type, vehicle class).

## Plugin outputs beyond the bike state

All read from the emulated process after each frame (no re-implementation):

| Export | Source |
|---|---|
| `reflex_world_race` | race state machine `A+0x21c` (0 Pre, 1 Countdown, 2 InProgress, 3 WindDown, 4 Post; Activity::vf24 0x558930 dispatches on it), laps/times/place per `_re/notes/race-state.md`, start board timer `[A+0x2cc]+0x4f4` |
| `reflex_world_bike_pose` | 18 node world matrices of the player's bike model: `V=[P+0x1ac]` (RavenVehicleMotocross), model `[V+0x1328]`, hierarchy model+0x10 (records +8, count +0xc, world +0x1c), completed with the lazy evaluator 7db890 (suspension travel, steering, wheel spin) |
| `reflex_world_vehicle` | a slot's vehicle class and simulation bodies (`atv.cpp`, `_re/notes/atv.md`): RavenVehicleMotocross 0x93cd84 / RavenSimulationMotocross 0x93c8a4 (4 bodies) or RavenVehicleATV 0x93d0ec / RavenSimulationATV 0x93d054 (6 bodies: frame, rider, four wheels); S = [V+0x234]; gear (MX S+0x8ac0, ATV S+0x96f0), rpm, speed, all render poses. An ATV's `reflex_world_bike_pose(_slot)` returns the 50 ATV_Hierarchy nodes; the boot and the plugin find either simulation class. The machines (`vehicles.h`, `_re/notes/vehicles.md`): RavenVehicleTrophyTruck 0x93cee4 / RavenSimulationTrophyTruck 0x93c93c (Pro Lite, Pro 2), RavenVehicleBuggy 0x93d2e4 / 0x93d24c (Super Buggy), RavenVehicleUTV 0x93d4d4 / 0x93d43c (UTV), 5 bodies each (chassis, four wheels), `type` = the factory index 3 / 5 / 7 |
| `reflex_world_camera` | the render camera `[0xa7b1d8]` (intro fly-by, start gate, chase), see `_re/notes/game-camera.md` |
| `reflex_world_terrain_deform` | the live deformation surface (the terrain shader's DeformSampler s10) |
| `reflex_world_tracks` | tyre-track decal triangles: the three TerrainTracks (0xa95774/78/7c) get memory-backed IDirect3D{Vertex,Index}Buffer9 stand-ins at 0xf8000000, the original `TerrainTracks_FlushPending` (0x7cbe30) writes the exact SHORT4N ribbon vertices and strip indices there, and the export walks them like `TerrainTracks_Draw` (0x7cc4f0); triangles touching never-written (all-zero) vertices are dropped. Unity draws them with ps_hft_tracks into a world-space multiplier texture (`OriginalTracks`) |
| `reflex_world_rider_pose` | 69 rider node matrices |
| `reflex_world_audio*` | all game sound (opt-in; commands carry the 3D position, volume, mute/pause and the wrapper's load state so events already playing at capture can be resumed; sub-command 3 "get event" marks the wrapper loaded (state 5), without which the game never plays its on-demand 2D voice events: gate drop, announcer, crew chief). Originally the engine: the game's FMOD event commands (type 0x13: play/stop/parameter) for a client that plays the original waves (`_re/formats/fsb_export.py`) and reports one-shot ends; the runtime then calls the game's EVENT_FINISHED callback 0x80b7d0 so the engine-sound state machine (0x685610) advances (`_re/notes/engine-audio.md`). Verified not to affect the bike state (bit-identical over 2000 frames with and without) |
| `reflex_world_race_drivers` | every rider of the race activity (`A+0x268` driver list): player slot from `D+0x74`, place `D+0x78`, laps `D+0xf1c`, finished `D+0x70`, race clock `D+0x134` (stops at that rider's finish), best lap `D+0x10c4`; the post-race table lists them in place order |
| `reflex_world_slots` / `*_pose_slot` / `reflex_world_table*` | the race's player slots (P = G+0x7460+slot*0x12a0, type human/AI, Rider/Vehicle List rows) with per-slot bike and rider poses, and the running game's tables (names, AI riders, vehicles, gear) for OriginalOpponents |
| `reflex_world_particles` | the live particles of the two static managers (0xc11bd0 normal, 0xc11d80 additive), with sort order, size, pivot, rotation, colours and atlas frame as `PlatformParticleManager2D::Render` 698890 draws them (`particles.cpp`); `reflex_world_particles_enable` runs the manager update while the other ambient skips stay |
| `reflex_world_ambient` (+ `_mask`) | ambient animated entities (`ambient.cpp`, `_re/names/76_ambient.tsv`): crew chiefs, pit crews, flaggers, loop / spline / scenario animations, roaming people, the instanced grandstand crowd (template + relative matrix +0xa00), billboard crowd sections (Crowd shader constants). CharacterEntity skeletons are the node set at +0x210 (world matrices +0x22c, record n = the entity placement); nodes the animation left invalid are completed host-side in 007db890's exact SSE order (export equal to the full-ambient run). `reflex_world_ambient_mask` keeps the vf19 updates of chosen classes with ambient off (bike hashes unchanged for 0x3bf on Concord / Kingston / Alpine; 0x40, the spline-carried particle emitters, draws from the CRT rand() and changes Alpine) |
| `reflex_world_find_object` | locates a game object by vtable + identifying dword (start gate: DropGate 0x95fc24, entity id at +0x14; its world matrix at +0x30 is read each frame, `_re/notes/start-gate.md`) |

Note on "nonfinite" state: positions and velocities are SSE float4s whose **w lane is padding**; after some
contacts (e.g. hitting a tyre stack) the original's vector math leaves NaN/Inf there. This is original behaviour
(the byte-exact validation compares those bytes too). Consumers must check x/y/z only — a stress run of 8 x 6000
frames (random aggressive input, fixed and hitchy dt) on Concord and Alpine has no nonfinite x/y/z or quaternion.

Original keyboard bindings (probed through the input path): W throttle, S both brakes, A/D steer,
arrow keys rider lean (right stick: whips, preload), X reverse.

## Stand-ins (the only non-original parts)

| Original dependency | Stand-in |
|---|---|
| Terrain deformation surface LockRect/UnlockRect | maps the supplied 2048x2048 R32F grid (`alpine-deform.bin`) |
| DirectInput keyboard GetDeviceState/Acquire | the 256 key states passed to `reflex_world_frame` |
| XInput #2/#3/#4/#8 | the 4 `ReflexPad` entries / `pad_mask` |
| XInputSetState (#3) | recorded per pad for the client's rumble (`input.cpp`, `reflex_world_rumble`) |
| Raw Input mouse (WM_INPUT, GetRawInputData, DefWindowProcA) | `reflex_world_mouse` posts one WM_INPUT through the game's window procedure 00880dc0 between frames (`input.cpp`, `_re/notes/input-settings.md`) |
| winmm timeGetTime | fixed 0x00100000 (the physics takes dt from the caller) |
| user32 cursor calls | fixed client rect 1280x720, cursor 0,0 |
| Enter/LeaveCriticalSection | balanced no-ops (single thread) |
| Get/SetLastError, TlsGetValue (CRT `_getptd_noexit`) | the game thread's `_tiddata` recorded in the world image (errno, `rand()` seed); a worker touching CRT thread data is reported |
| SetEvent / WaitForSingleObject / Sleep (animation workers) | serialized worker execution (above) |
| GetCurrentThreadId | the recorded live thread id of the running thread |
| QueryPerformanceCounter/Frequency, GetTickCount, timeGetTime | virtual 10 MHz clock advanced by the caller's dt (whole frames); fixed in physics-only frames |
| `REFLEX_FIXED_CLOCK=1` / `=<hz>` (opt-in, oracle) | pins the whole-frame clock instead: every frame advances exactly 1/hz s (166667 x 100 ns at 60 Hz); unset = the clock above, byte for byte (native/game notes/clock-pin.md) |
| HeapAlloc/HeapFree/HeapSize/HeapReAlloc | deterministic first-fit heap at 0x30000000 (live blocks freed as no-ops) |
| Interlocked*, CreateEventA, CloseHandle, ResetEvent | single-threaded semantics / fresh handles |
| MultiByteToWideChar / WideCharToMultiByte | Windows-1252 |
| GetModuleHandleW / GetProcAddress / DecodePointer (CRT new-thread path) | the CRT's FlsSetValue/FlsGetValue reach the per-thread data stand-ins |
| steam_api interface accessors / callbacks | no Steam client: SteamUser, SteamUserStats, ... return null (the live sign-in manager stays in state 0); callback registration is a no-op |
| Database async loader requests (Database+0xf8, thread 0084c760) | sound-database streams are not serviced (stay pending) |
| InitializeCriticalSection(AndSpinCount) / DeleteCriticalSection | fresh RTL_CRITICAL_SECTION |
| FMOD Ex / FMOD Event imports and the EventSystem object | succeed without effect (exact stdcall sizes from fmod_eventL.dll's C wrappers) |
| IDirect3DDevice9 (update-time calls) | null device, all 119 methods with exact stdcall sizes |

Instruction notes: `rsqrtps/ss` and `rcpps/ss` use exact tables of the approximation
as executed by the oracle (Rosetta 2), verified on all 2^32 inputs
(`rsqrt_rcp_tables.inc`). MMX is supported for the one helper that uses it
(0041c580), which requires an empty x87 stack and ends with `emms`.

Any other import stops the world with `unmodelled import <name> (return <addr>)`.

## World image

`_re/trace/pack_world.py` packs a full-world dump (`full-<serial>-entry.bin`, from the
terrain-v26 proxy) into `RXWI`: every dumped page, zlib per page, decompressed on first
touch. Nothing is dropped. Dump header words: 2 context, 3 dt, 5 x87 CW, 6 MXCSR,
7 msvcr90 base, 8 msvcp90 base, 9 game-thread `_tiddata`. The packer also records the
live bike simulation (vtable 0x93c8a4, S+0xec = 4, referenced; unreferenced copies exist).

Current Unity seed: `_re/trace/out/20261004T084810.864550Z-alpine-seed4`, serial 276
(after the post-spawn vf10 blend transition, which ended at serial 188). Header word 10
(set by the packer) is the RiderAnimation object.

## Validation (native vs unchanged original, all changed pages compared)

`_re/trace/validate_native.py CAPTURE SERIAL [--seed N --frames N --dt HEX | --frames-file F] --crt msvcr90.dll --output DIR`

| Seed dump | Run | Result |
|---|---|---|
| 265 | 3 random key seeds x 40 frames, dump dt | 0 mismatched bytes |
| 269 | 2 random key seeds x 300 frames at 1/60 s | 0 mismatched bytes |
| 269 | 900-frame scripted drive (throttle, steering, brake) at 1/60 s | 0 mismatched bytes |
| 276 | with animation jobs: 900-frame drive + 2 random x 300 frames at 1/60 s | 0 mismatched bytes |

The only excluded words are the stand-in pointers themselves (deformation resource
vtable, keyboard device vtable), reported by `world_validate` as `stand_in_words`.
The installed Unity files reproduce the 900-frame run byte-identically.

Speed (after the interpreter optimizations: flat decode cache, fall-through chaining,
inline memory/operand fast paths, NEON packed-single arithmetic with exact NaN fallback):
~160M instructions/s; ~6 ms per 1/60 s physics+animation frame, ~11 ms per whole frame.

## Block JIT (`jit.cpp`, on by default; `REFLEX_NO_JIT=1` interprets)

Hot straight-line code (up to 48 instructions, ending at a control transfer or before a native/import address) is
translated to AArch64 with the interpreter's exact semantics; `vm.cpp` / `fast.inc` stay the reference.
- **Guest state:** registers and x86 flags stay in the `Machine`. Guest xmm registers are cached in v16..v23 within a
  block; they are written back before any C++ call and at exits.
- **Flags:** a first pass finds which instructions are translated. The x86 flags that are never read afterwards in the
  block are then not computed.
- **Memory:** accesses take an inline path through `Memory::page_table()` / `write_table()`, else
  `Memory::read/write`.
- **Untranslated forms:** they call the interpreter's handler. x87 loads and stores have their own entries.
- **x87 fast paths** (2026-10-16): those entries move normal values between F80 and float / double inline, and fadd / fsub / fmul /
  fdiv(r) of operands that are exact doubles use the host's double arithmetic when the result is provably f80's (PC 53 nearest; PC 24
  unless the double lands exactly on a 24-bit midpoint inexactly; PC 64 only when exact); everything else goes to f80.cpp
  (`REFLEX_X87_SLOW=1` forces it). fpatan / fyl2x / fyl2xp1 results are memoized per thread (vm.cpp; pure functions of the operands
  and rounding; ~94% hits on a 12-rider National). Checked: 400M random operand pairs against f80.cpp (0 mismatches), bike hashes and
  the whole-memory hash unchanged; 4-6% fewer host instructions per frame.
- **NaN:** SSE arithmetic uses the host's IEEE results unless a result lane is NaN; then the handler runs, because x86
  NaN rules differ.
- **Dispatch:** blocks chain directly, or through a 64K-entry lookup cache. A block that declines (MXCSR, or a slice end
  in boot mode) returns its own pc to the dispatcher.
- **Boot mode** (`BootConfig::jit`, `jit_exact_slices`): no block runs past `slice_end`, so threads switch at the
  interpreter's exact instruction counts. World images built with the JIT are byte-identical to interpreted builds:
  13 s instead of 39 s for a National.
- **Measured:** bike-state hashes are unchanged on every level tested. About 1 ms per single-rider frame and 8-12 ms
  with 11 AI riders (interpreter: 4-5 ms and 40-70 ms).
- **Debugging:**
  - `REFLEX_JIT_HISTO` (forms still run by the handler)
  - `REFLEX_JIT_MAP` (host code → guest block, for `sample` profiles)
  - `REFLEX_JIT_DISABLE=mn,...`
  - `REFLEX_SLICE_LOG` / `REFLEX_SLICE` (boot differential checks)

`f80/` transcendentals: fpatan / fyl2x first try 256-bit precision. The result is accepted only when every value within
the error radius rounds alike; otherwise they recompute at 576 bits. This gives identical results, which was checked
on 180k vectors against the previous implementation.

## Bink videos (`bink.cpp`, `reflex_bink_*`)

The install's `Videos/*.bik` are decoded by the install's own `binkw32.dll`, loaded on a small Machine of its own per player
(kernel32 / user32 / winmm stand-ins, no threads: the DLL then services its IO and sound from the caller) and driven exactly
as the game's XenonBinkVideo does (BinkOpen BINKNOFRAMEBUFFERS | BINKALPHA, our own frame buffers, BinkDoFrame / BinkShouldSkip /
BinkNextFrame / BinkWait on the caller's clock). The BINKSND sound interface is implemented natively and hands the PCM to the
caller. Unity: `Content/ReflexVideo.cs` (OriginalVideo), `FrontEnd/FrontEndVideo.cs`, shader `Reflex/Bink Video` (the game's
ps_2_0 YUV / YUVA programs). JIT and interpreter decode identical planes and PCM. See `_re/notes/bink.md`.

## Build / install

```
./build.sh                      # world_validate + libreflexworld.dylib + reflex_boot (arm64)
./install_unity.sh              # Plugins/macOS/{libreflexworld,libcapstone.5}.dylib (atomic rename)
```

Nothing from the game is installed. The Unity client reads the player's install at run time: MXReflex.exe, the VC90
runtime from Installers/vcredist_x86.exe, and the world images it builds by booting the game (cached per user).
Capstone is BSD-licensed (`lib/CAPSTONE-LICENSE.TXT`).
