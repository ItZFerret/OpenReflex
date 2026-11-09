Current core-only startup: `HANDOFF/SESSION_2026-10-21_CORE_STARTUP.md`. Terrain and equipped appearance snapshots now replace the legacy startup world. The additive 272-byte `rx_appearance` API exports the actual constructed equipment IDs; original struct layouts are unchanged.

# Unity bridge: the core API and the C# side

**Current continuation:** see `HANDOFF/SESSION_2026-10-21_SOL.md`. Actual standalone Unity riding and Test Runner now pass; four-body pose export and original HUD fallback are fixed. Earlier chronological status below is historical.

Status 2026-10-19 (evening): the C API (`core/include/rx_api.h`, version 2, `core/api/rx_session.c`) and a first C# bridge
(`Unity/Reflex/Assets/Scripts/OpenReflex/`) exist. The bridge is verified outside Unity against the real dylib up to the point where
the product session stops at an unported game function (no race frame has run yet, so no getter has returned race data). The Unity
client still uses the **old lifted/emulator plugin** (`libreflexworld`, `OriginalSimulation`, `ReflexWorldNative`); nothing of the old
path was touched and nothing is installed in the Unity project.

Status 2026-10-20 (unity2): the bridge is complete for the first ride. The product session now runs a race (19 frames: 9 load frames,
then countdown frames; `0086e750` is no longer reached by `core/tools/rx_api_test`), the C# bridge reads the same values as the C
driver, and there is an Assembly-CSharp adapter from `OpenReflexSimulation.Results` to the old `OriginalSimulation.Results` /
`Reflex*` types plus an opt-in path selector (`REFLEX_SIM=open`, default `legacy`). Details in "unity2" below.

Status 2026-10-20 (unityrace): the selector is **wired into `OriginalWorldRuntime`** behind `REFLEX_SIM` (unset/legacy is still
the old plugin). A full race through the adapter (create -> 9 load + 5000 race frames -> old-typed Results) runs against the
preserved camsnap dylib and matches the C driver log on every comparable value (state racing, 5009 frames, game 83.534, race
65.063, place 6, camera, terrain, particles, mud); the live tree also reaches 5000 frames. Details, the editor steps and the one
requirement (`RX_FULL_FRAME=1` in the shell) are in "unityrace" below.

Status 2026-10-20 (unityscene): the scene exports the first ride still missed are in: **ambient entities and their node matrices**
(`rx_ambient_entity` / `rx_session_ambient_entities`, the old `reflex_world_ambient`), **the event scene's live object list** and a
per-object matrix read (`rx_scene_object` / `rx_session_scene_objects` / `rx_session_scene_object_matrix`: props, gates, bales,
boards; the old `find_object` + `read` without handing out game addresses) and **per-vehicle node flags**
(`rx_session_node_flags`, the game's damage-panel bits). The C# bridge collects them (ambient only when `AttachAmbient` asks, objects
for `TrackObjects`, node flags every frame), the adapter fills the old `Results.ambient` / `objects` / node-flag fields, and a 5000
frame race through `OpenReflexSelector` matches a C driver on 1381 ambient entities / 5149 matrices / 1381 objects, the start gate's
matrix and the node-flag bytes. (Tyre-track ribbons: see the tyretracks status below.)

Status 2026-10-20 (tyretracks): the tyre-track ribbons are exported: `rx_session_tracks` (`rx_track_info`, the old
`reflex_world_tracks` 18-floats-per-triangle contract, de-indexed) and `rx_session_track_ribbons` (`rx_track_ribbon` per strip).
The core **does** build the three `TerrainTracks` rings (`rx_hfterrain_surface_init`), the wheel emitters are ported, and the
session now runs the game's own `HFTerrainSurface::vf14` (007c4fa0) after each frame at the render boundary, so the real ribbon
vertices are flushed into the core's `rx_d3d` buffers and read back. The premise of the unityscene note was wrong: 0x1062 is
`TrackSystem` (`RSToolsNS_RSDataNS_TracksEntityRSData`), which is **absent from all 58 shipped level scenes** and is not the
ribbon owner (see "Tyre-track ribbons" below). Product ride (preserved fixed wreck-checker build, `RX_FULL_FRAME=1`, W held):
1748 triangles / 5244 vertices at session frame 1309 (final tree dylib: 1762 / 5286, the same 404 local); the local bike's 404 triangles are within 0.095..3.387 m of its wheel
contact path (median 0.937 m) and match the original world's local ribbons after the known start-grid offset (median 0.669 m).

Status 2026-10-20 (firstride): the **headless first ride** is a one-command test (`builds/firstride/run.sh`) and it **runs**: through
the public API the Concord event loads, the intro and countdown play, and the local bike **rides and steers** (W held, A/D from
race 200; the steering A/B is identical for 200 frames and 184 units apart afterwards). 32 of 33 checks pass (session, bike,
gears, path, steering, race, rider bones 69 / bike nodes 18, camera 6.78..26.83 units, 2048-sample terrain, 2177 live particles,
1381 ambient entities / 5149 matrices, 2704 scene objects, the start gate, node flags, 3930 tyre-track triangles / 24 MX strips,
12 mud records, HUD / race line / standings / slots / vehicle info) and the one failure is `run.no_stop`: an unported **AI
behaviour `00526ab0`** (the 20:47 preserved build: `005278e0`) ends the ride at **race+463 / ~7.7 s**, which is the next blocker.
Details, both pinned builds and the Unity steps: `HANDOFF/FIRST_RIDE.md`.


Status 2026-10-21 (unityfinal): the final headless full-race test. The bridge binds every export added since unityrace and
no struct changed (196 layout checks); the one additive gap was `IReflexSimDriver.SetCamera` (the runtime-facing interface now
forwards the game's own camera setter; legacy ignores it). A 5000-frame race through `OpenReflexSelector` + the adapter with the
game's own input (W from the gate drop, A/D from race 200) and all four views matches a C driver on the same dylib
(`19d3454a...`) on **10/10 value groups and every per-frame fingerprint** (dt=0: 2127 steps; dt=1/60: 2179, firstride's
number). The ride still stops at the known unported **`7859afc0` `_CIpow`** at racing frame 1010 (session frame 2188 at
1/60). Details, values, risks and the Play steps in "unityfinal" below and `builds/unityfinal/README.md`.

Status 2026-10-21 (unitycheck): the project namespace is **`Reflex.Port.RxCore`** (was `Reflex.Port.OpenReflex`; the
Assembly-CSharp bridge is `Reflex.Port.RxCore.Bridge`, the EditMode tests `Reflex.Port.RxCore.Tests`; the asmdef
*assembly* names are unchanged), and the headless harnesses now use it. The project compiles in **editor mode** outside
Unity: `Assembly-CSharp` (all 139 `Assets/Scripts` sources, `UNITY_EDITOR` defined) and `Assembly-CSharp-Editor` (all 49
`Assets/Editor` sources) with **0 errors each**, including `Reflex.Port.Content.ReflexLevelVerify` in the runtime assembly;
the old `Prefs` (CS0234) and `ReflexLevelVerify` (CS0103) errors are gone. `builds/unityfinal/run.sh` on the current product
dylib (`69a6f872...`) runs the **full 5000-frame race with no stop**: dt=0 and dt=1/60 are both 10/10 value groups with
5000/5000 per-frame fingerprints identical to the C driver. Details in "unitycheck" below;
`builds/unitycheck/` reproduces the compile checks.


## What exists

### C API (`rx_api.h`, ABI version 2)

- Lifecycle: `rx_session_create(install_root)` / `_destroy` / `_version`, `rx_session_api_version`.
- Event: `rx_session_event_default(&spec)`, `rx_session_event_load(s, &spec)` (SetupEvent / StartEvent as `build_event_test.c`).
- Frame: `rx_session_advance`, `rx_session_advance_dt(dt)` (dt = the real time since the previous frame: it moves the session's virtual performance counter that the game's own `Engine::UpdateTiming` measures; see Input and clock below),
  `rx_session_get_state` (Loading / Countdown / Racing / Finished / Error), `rx_session_last_error`, `rx_session_frames`,
  `_frame_dt`, `_game_time`.
- Getters (real reads of game memory): `bike`, `rider_pose`, `bike_pose`, `rider_pose_slot`, `bike_pose_slot`, `camera`, `race`,
  `standings`, `slots`, `terrain` (+ `_heights`, `_deform`), `particles` (+ `_array`), `ambient` (+ `_rescan`) and
  `ambient_entities` (+ `rx_ambient_entity`), `scene_objects` / `scene_object_matrix` (+ `rx_scene_object`), `node_flags`,
  `mud` (+ `_count`), `hud`, `race_line`, `vehicle_info` (the last four added by unity2, 2026-10-20: `rx_hud_state`, `rx_particle` /
  `rx_session_particles_array`, `rx_vehicle_info`; the scene four by unityscene, 2026-10-20; ABI still 2, additive only),
  `tracks` / `track_ribbons` (tyretracks, 2026-10-20: `rx_track_info` / `rx_track_ribbon`, the live TerrainTracks ribbons).
- Water wake splats (w-water, additive, `rx_water.h`): `rx_session_water_splats(after_frame)` - the pressure splats the game queued
  per frame (Wake_AddSplatQuad / Wake_AddSplat), taken at the render boundary as the original's Wake_Step 007ed300 takes them; C#
  `RxWaterNative`, drawn by `OriginalWater`'s wake simulation.
- Added in version 2 (for the C# bridge): `rx_pad` + `rx_session_set_input` (serves the game's own DirectInput / XInput poll; see Input and clock below), `rx_session_rider_pose_slot` / `rx_session_bike_pose_slot`
  (the other riders; the old plugin had the same), `rx_session_sizeof(name)` (ABI self-check).
- Also in version 2 (additive, tyretracks, 2026-10-20): `rx_track_info` / `rx_track_ribbon`, `rx_session_tracks` /
  `rx_session_track_ribbons` - the live TerrainTracks ribbon triangles and strip descriptors (no existing struct changed).
- Fixed in version 2: `rx_session_bike` looped over four bodies and wrote 16 floats at each matrix *row* (past the end of the 320-byte
  struct). The intermediate body-0-only correction was also wrong: each field holds four body vec4s.
  As of 2026-10-21 Sol, four floats are copied per row from each separate 0xa0-stride body record.
  The independent vehicle-info comparison and actual Unity riding exposed and verify this correction.
- **Fault guard**: create, event_load and advance run under a signal guard (`run_guarded` in `rx_session.c`). `rx_call_original` on an
  unported function prints `rxcore: unported game function XXXXXXXX reached` and `abort()`s; inside Unity that would kill the editor.
  The guard catches SIGABRT / SEGV / BUS / ILL / FPE on the calling thread, `siglongjmp`s out, puts the session in
  `RX_SESSION_ERROR` and sets `last_error` to e.g. `rx_session_advance: unported game function 0069b0e0 reached (the core aborted; the
  session is unusable) (frame 1)` (the address is read from the captured stderr; stderr is swapped for the call and forwarded
  afterwards). After a fault the game state is inconsistent: only the getters, `last_error` and `destroy` are valid. Signals on other
  threads are chained to the previous handlers. Limits: a stack overflow or a fault inside stdio is not recoverable; the handlers
  are installed per call (cost: a few syscalls per frame).
- **One session per process at a time, re-creatable**: `create -> destroy -> create` works in one process (second Play without restarting
  the editor): `rx_session_destroy` releases the host resources, unmaps the arena and restores the core's static data
  (`rx_reset_restore`); a create while a session is open returns NULL. What is reset and what is not: `notes/session-reset.md`;
  test `tools/rx_api_recreate_test.sh`. macOS only for now (elsewhere the second create is refused with "restart the process").

### C# (`Assets/Scripts/OpenReflex/`, assembly `Reflex.OpenReflex`, no UnityEngine, unsafe, auto-referenced)

- `RxNativeLoader.cs`: dlopen/dlsym loader for `librxcore.dylib` (`Load(path, tempRoot)` copies to a stamped temp folder in the
  editor like the old loader; `ResolvePath(pluginDir)` honours `REFLEX_RXCORE_PATH`). Separate from `Platform/ReflexNativeLoader`.
- `RxApiTypes.cs`: the structs of `rx_api.h` field for field (C field names, fixed buffers, blittable) + `RxSessionState`.
- `RxApiNative.cs`: one wrapper per export (same names), `Check()` (API version + every struct size through `rx_session_sizeof`),
  `RxApiLayout` (managed side of the layout check).
- `OpenReflexSimulation.cs`: the worker-thread bridge. Same contract as `OriginalSimulation` (Submit / SubmitFrames / queue and merge
  / Collect / KeepResults / Recycle / RealClock / TrackSlots / BetweenFrames / WaitIdle / Dispose, `Results` with the same field
  names where the data exists), and in addition: the worker owns create + event load + destroy (`Phase` Starting / Ready / Failed /
  Stopped, `WaitReady`, `Failure`), `Results.sessionState`, and errors are results (`Results.error`, `Phase.Failed`) instead of
  exceptions. Getters run only once the state is past `Loading`. Double buffering is the old front / back / ready rotation.
  `Results` also carries `hud` / `hasHud`, `raceLine` / `raceLinePoints`, `vehicle` / `hasVehicle` and the live `particles` array
  (unity2).
- Tests: `Assets/Tests/EditMode/` (`Reflex.OpenReflex.Tests.asmdef`, `RxApiLayoutTests`, `OpenReflexSimulationTests`;
  `RxApiLayoutExpected.cs` is generated from the C layout program). **Not run inside Unity** (the editor was not started).

### Assembly-CSharp bridge (`Assets/Scripts/OpenReflexBridge/`, unity2)

The old `Reflex*` types and `OriginalSimulation` live in the predefined project assembly, which an asmdef cannot reference; these
files therefore sit in a folder with **no .asmdef** (Assembly-CSharp) and are compiled by Unity with the project.

- `OpenReflexResultsAdapter.cs`: `Create()` / `Reset()` / `Apply(oldResults, openResults)` copy one `OpenReflexSimulation.Results`
  into an `OriginalSimulation.Results` field for field: the bike (four body positions/quaternions per pose field, velocity, speed, gear, inputs, frames), rider bones,
  bike nodes, race, standings, HUD, race line, camera, vehicle info (class, bodies, 6 pose matrices), terrain patches (the pooled
  `heights` / `deform` arrays are handed to `OriginalTerrain.ApplyPatch` exactly as the old simulation handed its own), the particle
  array + info, mud (local + tracked slots), the tracked slots' poses, the presentation timeline, ambient entities, animated
  objects, node flags and the tyre-track ribbons. Fields the core does not export yet (audio) are **reset**, never left stale:
  `audio` empty. `Reset` allocates nothing, so a recycled `Results` keeps its arrays.
- `OpenReflexSelector.cs`: `IReflexSimDriver` (the `OriginalSimulation` surface the runtime uses), `LegacyReflexDriver` (forwards
  everything to `OriginalSimulation` unchanged) and `OpenReflexDriver` (forwards to `OpenReflexSimulation`, converts `ReflexPad` to
  `RxPad`, adapts each collected frame, and keeps the old KeepResults / Recycle ownership so `OriginalPresentation` can hold several
  frames). `OpenReflexSelector.Mode` reads `REFLEX_SIM` (`open` / `1` / `true` / `on` / `rx` / `core` = open; `legacy` / `0` /
  `false` / `off` / anything else, including unset = legacy, with one log line for unknown text); `Force(ReflexSimMode)` is the
  explicit override. `Create(installRoot, spec, legacyWorld, trackTerrain)` returns the selected driver; `CreateLegacy` /
  `CreateOpen` are the explicit forms.
- The old path is untouched: `OriginalSimulation.cs`, `ReflexWorldNative.cs`, every renderer and the plugin are byte-identical. To
  use the core, a consumer swaps `new OriginalSimulation(world, terrain != null)` for
  `OpenReflexSelector.Create(installRoot, spec, world, terrain != null)`; everything downstream keeps reading
  `OriginalSimulation.Results`. The open driver's old-plugin-only extras (`TrackObjects`, `AttachAudio`, `AttachAmbient`) are
  accepted and log once, because the core does not export those yet; `AttachParticles` is a no-op that returns true (the core's
  managers always run).

## What the old plugin exposes vs the new API

| old plugin (`ReflexWorldNative.cs`) | new API |
|---|---|
| create / destroy / build / error / frame | `rx_session_create` / `_destroy` / `_event_load` / `_last_error` / `_advance(_dt)` |
| bike, rider_pose, bike_pose, camera, race, race_drivers, slots, `*_pose_slot` | present (`bike`, `rider_pose`, `bike_pose`, `camera`, `race`, `standings`, `slots`, `*_pose_slot`) |
| camera key (C) / view selection | present: `rx_session_set_camera` (the game's own `Player_SetCamera`; keyboard C reaches the same function through `rx_session_set_input`) |
| terrain, terrain_heights, terrain_deform | present (dirty tracking is a coarse hash; the first call reports the whole grid) |
| particles (`ReflexParticle` array + info) | present: `rx_session_particles_array` (normal manager first, far to near, 3000 per manager) + `rx_session_particles` for the info only |
| ambient | present: `rx_session_ambient_entities` (`rx_ambient_entity`, 304 bytes as the old `ReflexAmbientEntity`) + the matrices array + `rx_session_ambient` for the info block (unityscene) |
| mud | present |
| hud | present (`rx_hud_state` / `rx_session_hud`: speed, activity state, on track, wrong way, time behind / ahead, lap hold, penalty) |
| race_line | present (`rx_session_race_line`, node centres from D+0x1c) |
| vehicle (`ReflexVehicleInfo`) | present (`rx_vehicle_info` / `rx_session_vehicle_info`: factory type, bodies, model nodes, gear + 1, rpm, speed, 6 pose matrices) |
| own node flags (`ReflexVehicles.NodeFlags`) | present (`rx_session_node_flags`: the model hierarchy's record +0xc per node, bit 0x10 hidden; unityscene) |
| props, find_object, mode / mode_objects | the scene's live object list (`rx_session_scene_objects`: class vtable, entity id, matrix, name, model name) and a per-object 4x4 read (`rx_session_scene_object_matrix`) replace `find_object` + `read` for the animated props (the start gate: vtable 0x0095fc24, id from the entry, matrix +0x30). The generic `read` (raw addresses) and `mode_objects` (the UI collectable heap scan) stay out: they need a lead decision |
| tracks | present (`rx_session_tracks` / `rx_session_track_ribbons`, `rx_track_info` / `rx_track_ribbon`: live TerrainTracks ribbons; tyretracks) |
| version / sizeof | `rx_session_api_version` / `rx_session_sizeof` |
| frame input (`keys`, `pads`, `padMask`) | `rx_session_set_input`: served to the game's own poll (notes/input-clock.md) |
| mouse (`reflex_world_mouse`), rumble (`reflex_world_rumble`) | `rx_session_set_mouse`, `rx_session_set_cursor`, `rx_session_rumble` |
| audio, audio_enable, audio_raw, audio_finished | not present |
| table*, garage, persist, remap, pages, `profile_*` (career / save), read / read_floats | not present (career etc. can stay on the old plugin) |
| crash_*, perf_stats, port*, `extract_crt*` | not applicable (the core has no emulator, no ports) |

## Planned (in order of what blocks the first ride)

1. ~~The product session reaches a race frame~~ (unity2, 2026-10-20: `rx_api_test` runs 9 load frames + 10 countdown frames; the
   earlier stop at `0086e750` is gone). The getters return live race data.
2. ~~Input / frame clock~~ done (inputclock, below); remaining: the frame still runs the head of `Engine::Update` + `MXGame::Update` (`RX_FULL_FRAME=1` = the whole
   `Engine::Update`, stops in the unported scene tick today).
4. ~~Getters the first-ride scene reads that are missing: hud, race line, particle array, per-slot vehicle info~~ done (unity2).
   ~~ambient entities / matrices, scene objects, node flags~~ done (unityscene, 2026-10-20). ~~tyre-track ribbons~~ done
   (tyretracks, 2026-10-20: `rx_session_tracks` / `_track_ribbons`). Still missing: `mode_objects` / the generic raw `read`
   (lead decision).
5. ~~Make the core re-initializable (several sessions per process)~~ done (session-reset).
6. ~~The `Assembly-CSharp` adapter and the old / new selection~~ done (unity2; `REFLEX_SIM`); audio and rumble after that.

## First ride (firstride, 2026-10-20)

`builds/firstride/run.sh` is the one-command headless first ride: it builds `firstride.c` (public API, `dlopen` like the C# loader),
runs the reference Concord event twice through `rx_session_set_input` (W held from the gate drop; A/D from race 200; the second
pass without A/D), calls every first-ride getter on sampled frames and prints 33 PASS/FAIL checks; `compare.py` checks the
steering A/B. Full frames (`RX_FULL_FRAME=1`), the game's own clock (`advance_dt(rx_session_step_dt)`).

Result on the clean 21:09 build (`1143643fe90e`; the tree was mid-edit, the dylib is preserved in `builds/firstride/backup/`):
**32/33 PASS**. The only failure is `run.no_stop`: an unported AI behaviour **`00526ab0`** ends the ride at **race+463**
(session frame 1571, game time 26.233 s; 463 completed racing frames + the frame that aborted); the 20:47 preserved build
(`373b62fbc2b2`, after steerfix / ridepath / startdiag) ends at `005278e0`, race+462. That is the next blocker.
Values: 464 racing frames, 78.44 ft/s / gear 3 / 369 units (86.41 / 4 / 485 without steering), steering identical for 200 frames
then 184 units apart, rider 69 bones / bike 18 nodes, camera 6.78..26.83, terrain 2048 samples, 2177 live particles, 1381 ambient
entities / 5149 matrices, 2704 scene objects, start gate 618.69 112.64 -378.45, node flags 18, 3930 tyre-track triangles / 24 MX
strips, 12 mud records, 13 race-line points, 12 standings / slots; the frames 0..400 are 401/401 identical to steerfix's C driver.

Gaps this test records: the **intro camera is not exported** (`rx_session_camera` returns 2 until the start-gate/third-person
camera ~245 frames before the race); `rx_session_get_state` reports FINISHED for 10 frames between the load and the countdown
(activity Inactive before Pre; no Unity consumer reads it); the deformation grid is the original's constant 0.502 (the visible
ruts are the ribbon export); audio/rumble and `mode_objects` / raw `read` are still absent. The C# side was not run in the editor.
Steps, expected visuals and logs: `HANDOFF/FIRST_RIDE.md`; logs in `builds/firstride/logs/`.

## The scene exports (unityscene) and what is still missing

### Ambient entities / matrices (`rx_session_ambient_entities`)

The old `reflex_world_ambient` is now `rx_session_ambient_entities(s, out, max, matrices, max_matrices, &info)`:
`native/drafts/world_runtime/ambient.cpp` is the spec and the implementation is in `api/rx_session.c` (the scan, the 16 class
vtables, the per-class fields, the two-pass export with crowd templates first). Fields: `object_id` (identity, the old
`.object`), `entity_id` (+0x14), `kind` (0..15, the old `KindNames` order), `flags` (1 visible +0xa2, 2 skinned, 4 instance,
8 template, 16 animated billboard, 32 completed), `bones` and `first_matrix` (the skeleton's node matrices in `matrices`),
`source` (an instance's template index), `lod_distance` (ModelAsset +0x120 / the renderer divisor), `param[4]` (billboard crowds:
phase +0x140, empty-chair ratio, is-non-animated +0x144, active flashes +0x17c; instances: [0] the LOD index +0xa6c), `matrix`
(the entity's world matrix: the skeleton's entity node, else +0x30), `model` (+0x100 handle +4), `texture` / `texture_package`
(crew chiefs, the +0x388 override map and the TextureHandle scan). Nodes the animation job left invalid are completed with the
game's own `rx_world_node_evaluate_world` (007db890), exactly what the renderer's lazy evaluation does. `info` also reports
`matrices` (the true total, even when `max_matrices` is smaller) and `objects` (every class-table object found).

Which classes exist in the product is up to the core's scene factory: the billboard crowd sections (kind 8, scene type 0x1041, the stands'
crowd) were not built until 2026-10-27 (`build_scene_crowd.c`, w-npcs: before that every stadium / grandstand stood empty in the product and
the export never listed kind 8); `core/tools/ambient_census.c` prints the entities of one event per class and model.

Before the world exists (state Loading) the call returns 0 entities and does **not** cache the scan; the old plugin's `scan_ambient`
cached the pre-race "0" and never looked again - that is why the unity2/unityrace logs showed `ambient ... entities 0` while 1381
objects existed (the C# bridge only calls getters past Loading, so it was unaffected; `rx_api_test`'s pre-race call was).

### Scene objects / `find_object` (`rx_session_scene_objects`, `rx_session_scene_object_matrix`)

`rx_session_scene_objects` walks the event scene's live lists (the unsorted `scene + 0x2c28` and the four sorted vectors
`+0x2c3c / +0x2c54 / +0x2c6c / +0x2c84`, deduplicated) and returns per entity: class vtable, entity id (+0x14), flags (+0x1c /
+0x7c), matrix (+0x30), Name (+0x18), the model instance's ModelAsset name (+0xa0/+0x60). `rx_session_scene_object_matrix`
finds a scene object by vtable + id and copies the 4x4 at a bounded offset (a matrix offset from the scene setup: the start gate's
DropGate vtable 0x0095fc24, id 1354312 in Concord, matrix at +0x30). That replaces the old `reflex_world_find_object` +
`reflex_world_read_floats` pair for `OriginalAnimatedObjects` **without exposing a game address to the caller**. The generic
`reflex_world_read` (used by water, garage, tutorial, plates, vehicle shader constants) and `reflex_world_mode_objects` (the UI
collectables of Free Ride / challenges) are not ported: both need a lead decision (the raw-address contract; the UI/collectable
subsystem).

### Node flags (`rx_session_node_flags`)

The old C# `ReflexVehicles.NodeFlags(world, vehicle, ...)` read `V+0x1328` (the model), `model+0x18` (records), `model+0x1c`
(count) and `record[i].+0xc` through `reflex_world_read`. `rx_session_node_flags(s, slot, out, max)` does the same read (slot -1 =
the local player; returns the model's node count, writes at most `max` bytes). The C# bridge collects the local bike's and every
tracked slot's flags per frame and the adapter fills `Results.bikeNodeFlags` / `slotNodeFlags`.

### Tyre-track ribbons (`rx_session_tracks` / `rx_session_track_ribbons`, tyretracks)

The old reader walked the emulator's memory-backed VB/IB stand-ins at `0xf8000000` (`TERRAIN_TRACKS` 0xa95774/78/7c) that the
plugin created so `TerrainTracks::FlushPending` writes the ribbon vertices there. The core's ribbon chain was already there
except for the per-frame sync:
- `rx_hfterrain_surface_init` (007c4890, `build_terrain.c`) constructs the three `TerrainTracks` rings (30 / 10 / 5) with their
  `rx_d3d` VB/IB buffers and the 24 strips each; `rx_terrain_tracks_create_buffers` (007cbbf0) lays out the chunk tables.
- The game's `TerrainTracks_AddPoint` (007c3510, `phys_terrain.c`), `CutStrip` (007c4560) and `AllocSlot` (007c33a0) are ported;
  the wheel emitters `Vehicle_WheelDeformAndTrackMarks` (004607e0) / `Vehicle_SlipTrackMark` (00468120) call them every vehicle
  update (`world_veh_track.c`), and the rider contact emitters (004db710 / 004db8e0) likewise.
- `TerrainTracks_FlushPending` (007cbe30) and `TerrainTracks_Reset` (007cc630) are ported in `fx_terrain.c`; they lock/unlock
  the core's memory-backed VB/IB through `rx_d3d` and copy the staged SHORT4N cross-sections exactly as the original.
- **Missing piece (added here):** nothing called `HFTerrainSurface::vf14` (007c4fa0) in the product. The original's render thread
  calls it once per rendered frame on the active terrain (deformation readback + the three flushes). `rx_session`'s frame now runs
  the game's own `rx_fx_terrain_sync(active_terrain())` after `Engine::Update` / `MXGame::Update` and after `RX_FULL_FRAME=1`'s
  whole `rx_engine_update`, i.e. at the documented render boundary. The deformation grid and the ribbon buffers are produced by
  the game's code, not by the API.
- `rx_session_tracks(s, out, max_triangles, info)` decodes the core's VB/IB through `rx_d3d_bits` the same way `World::tracks()`
  (`native/drafts/world_runtime/runtime.cpp` 1091) did: stride 8 SHORT4N, the 16-bit chunk offsets / generations, `hx/hz` = the
  terrain's `+0x44d0/4` halved, the strip de-index, degenerate and never-written vertices rejected. Output: 18 floats per triangle
  = 3 vertices * (world x, world z, side 1/0, V = 32*|w|, intensity = |s|, material type or 0); `info` carries the version,
  counts, stride/format, half extents and the six material names. `rx_session_track_ribbons` returns the set / slot / type /
  first_vertex / triangles of each strip. No guest address is exposed. The export is product-only: in the oracle build `rx_d3d_bits` is 0 by design, so the oracle
comparison drives the old plugin's own `reflex_world_tracks` instead.

**The 0x1062 premise was wrong.** `RSToolsNS_RSDataNS_TracksEntityRSData` is `TrackSystem` (RTTI vtable `0x0095b6cc`, ctor
0066b740, vf5 0066b950 loading `MaterialAsset0..6` + `TrackBXML`, vf19 0066bf60, vf0_o98 0066bfe0 drawing per-vehicle trails).
It is **absent from all 58 shipped `Database/*.dx9.scene` files** (`builds/tyretracks/logs/level_tracks_scan.txt`: 58 files, 0
entities) and it is not the owner of the TerrainTracks ribbons (those are the HFTerrainSurface's three rings above). The factory
keeps its slot empty exactly as the level data requires; implementing the unused 0xef9a0-byte class would have been dead code and
would not have produced a single ribbon. The real blocker was the missing vf14 call, now wired.

Verification (builds/tyretracks): product ride with the preserved fixed wreck-checker objects, `RX_FULL_FRAME=1`, W held, 1300
frames after load: 1748 triangles / 5244 vertices / 48 strips / 53 chunks (final tree dylib: 1762 / 5286), all finite, at the bike's path (bounds x 552.5..661.3,
z -400.3..-257.4). The local bike's four strips (404 triangles) lie 0.095..3.387 m from its wheel contact path (median 0.937 m,
99.8% <= 3 m), matching the original's local ribbons for the same event / frame after the known start-grid offset (-9.024,
-5.214): nearest-vertex median 0.669 m, p90 1.574 m, triangle-centroid median 1.198 m. The original side is `reflex_world_tracks`
on the preserved emulator world `builds/framecmp/worlds/pc1-world.bin` (2024 triangles, 480 local; its local vertices are
0.009..5.801 m from its own path, median 0.998 m). Details, CSVs and `compare.py` in `builds/tyretracks/`.

### Animated objects (`find_object` + `read`)

`OriginalAnimatedObjects` only needs a matrix per configured entry, and `rx_session_scene_object_matrix` supplies it (the adapter
tracks the entries through `OpenReflexDriver.TrackObjects` -> `OpenReflexSimulation.TrackObjects` and fills
`Results.objects` / `objectsValid`). A generic `read` is still absent by design.

## Input and clock (inputclock; details and the record mapping in `notes/input-clock.md`)

- `rx_session_set_input(keys, pads, mask)`: `keys` = 256 DirectInput bytes by DIK scan code (0x80 = down; the `OriginalWorldRuntime` array as it is), `pads` = 4 XINPUT_GAMEPAD
  (the old `ReflexPad`), `mask` = connected pads. The state is latched for every following frame. The game's own poll turns it into its records.
- `rx_session_set_mouse(dx, dy, wheel, flags)` once per display frame with the motion since the last call (Raw Input semantics: +y down, 120 per notch, RAWMOUSE button flags);
  `rx_session_set_cursor`; `rx_session_rumble(pad, &l, &r, &calls)` for `Gamepad.SetMotorSpeeds`.
- Clock: `rx_session_advance_dt(dt)` with the real elapsed seconds, or run the game's own 1/60 step with an accumulator:
  `n = rx_session_pace(s, realDt, maxSteps); for (i < n) rx_session_advance_dt(s, rx_session_step_dt(s)); alpha = rx_session_alpha(s);`. The game has no inter-frame alpha of
  its own (poses are interpolated inside the frame by the vehicle scheduler); blend the last two frames' poses with `alpha` or show the last. `rx_session_engine_dt` /
  `rx_session_game_time` read the game's own dt / clock.
- The C# wrappers (`RxApiNative`) exist for all of them; `OpenReflexSimulation` already calls `set_input` before each frame and `advance_dt` with its dt (its `RealClock`
  path passes the measured frame time, which the game now measures itself: a shorter dt just makes the game wait for its cap).

## Using it from Unity (sketch)

```csharp
var dir = System.IO.Path.Combine(Application.dataPath, "Plugins", "macOS");     // or REFLEX_RXCORE_PATH
RxNativeLoader.Load(RxNativeLoader.ResolvePath(dir), System.IO.Path.Combine(projectRoot, "Temp", "OpenReflex"));
var spec = new RxEventSpec(); RxApiNative.rx_session_event_default(ref spec);
OpenReflexSimulation.Log = Debug.LogWarning;
var sim = new OpenReflexSimulation(installRoot, spec, trackTerrain: true);      // returns at once
// each display frame: sim.SubmitFrames(...); var r = sim.Collect(); if (r?.error != null) show r.error;
```

Install the dylib into the project only when Unity is idle (the user restarts Unity: project rule). Nothing in this change copies it.

## Selecting the path (`REFLEX_SIM=open|legacy`)

The old plugin stays the default; nothing changes until the user opts in. `Assets/Scripts/OpenReflexBridge/OpenReflexSelector.cs`
(Assembly-CSharp) owns the choice:

- `REFLEX_SIM=legacy` (or unset, or anything unknown) -> `OriginalSimulation` + `libreflexworld`, exactly the old path.
- `REFLEX_SIM=open` (also `1`, `true`, `on`, `rx`, `core`) -> `OpenReflexSimulation` + `librxcore`; each collected frame is
  adapted to `OriginalSimulation.Results`, so the renderers do not branch.
- Explicit from code: `OpenReflexSelector.Force(ReflexSimMode.Open)` / `Force(null)`; `OpenReflexSelector.Describe` is one line for
  the log (`open (REFLEX_SIM='open')`).
- ~~Wiring it into the world runtime~~ **applied (unityrace, 2026-10-20)**: `OriginalWorldRuntime.Create` now constructs through
  the selector and keeps `simulation` as an `IReflexSimDriver`:

```csharp
// OriginalWorldRuntime.Create, after the (unchanged) legacy world build:
RxEventSpec eventSpec = default;
if (OpenReflexSelector.UseOpenReflex)
{
    string rx = RxNativeLoader.ResolvePath(Path.GetDirectoryName(plugin));
    if (rx == null) throw new FileNotFoundException("REFLEX_SIM=open: librxcore.dylib not found ... set REFLEX_RXCORE_PATH ...");
    RxNativeLoader.Load(rx, OpenReflexTempRoot());            // <project>/Temp/OpenReflex, stamped (never overwrite the mapped image)
    Debug.Log("OPEN REFLEX core: " + rx + "  (" + OpenReflexSelector.Describe + ")");
    eventSpec = OpenEventSpec(files);                        // only here: the legacy path must not touch librxcore
}
simulation = OpenReflexSelector.Create(Path.GetDirectoryName(exe), eventSpec, world, terrain != null);   // IReflexSimDriver
```

  `OpenEventSpec(files)` copies the same `ReflexWorldFiles` event the legacy world was built from into `rx_event_spec`
  (`type` / `typeId`, `location` / `locationId`, `trackId`, `duration` / `durationType`, `riders`, `ai_difficulty`,
  `vehicle_class`; the `opponents` / `aiSkill` / `laps` / `garage.vehicleClass` overrides Build applies are applied here too).
  It calls `rx_session_event_default` for the reference fields, so it is built **only on the open path**: with
  `REFLEX_SIM` unset / `legacy`, `librxcore` is not loaded and must not be touched.
  The core spec has no Vehicle Select / career fields yet, so those choices still only affect the legacy world.
  `OriginalOpponents.Begin` has an `IReflexSimDriver` overload (it tracks the opponent slots through the selected driver), and
  `OriginalPresentation` has one too (it forwards `KeepResults` / `Recycle`); the old `OriginalSimulation` constructors stay.
  `OriginalWorldRuntime.Simulation` returns `simulation?.Legacy`, so the old-plugin-only consumers (audio, career pages) see
  `null` on the open path and no-op instead of crashing; the scene objects and tracks come through the adapter's Results. The legacy world is still created (the terrain,
  roster, particles' textures and the rest of the scene read it); only the frames come from the core.
- **The open path needs `RX_FULL_FRAME=1`** for the race's activity tick: with it the session runs the whole
  `Engine::Update` (5009 frames = 9 load + 5000 race; state racing, race time 65.063 s); without it only the frame head +
  `MXGame::Update` run and the race stays in the countdown while the engine clock advances (measured). Set it in the shell
  that launches Unity, not with `Environment.SetEnvironmentVariable`: .NET's setter did not reach the dylib's `getenv` on
  macOS in the unityrace test (the native `getenv` kept returning NULL until the variable was in the process environment).

A dylib built before unity2 is refused by `RxApiNative.Check()` with `rx_vehicle_info: unknown to the core` (the API version stayed 2
because no existing struct changed, and the new structs are checked like the old ones): rebuild the core and use that dylib.

### To see the open path in the editor (exact steps)

1. `cd core && ./build.sh` -> `core/build/librxcore.dylib` (the live tree builds clean, unityrace 16:33).
2. Launch Unity (the editor, not batchmode) with these variables in its environment:
   - `REFLEX_SIM=open`
   - `RX_FULL_FRAME=1`
   - `REFLEX_RXCORE_PATH=/absolute/path/to/core/build/librxcore.dylib` (or put `librxcore.dylib` next to
     `libreflexworld.dylib` in `Assets/Plugins/macOS` while Unity is closed; no file is copied by this change)
   On macOS, `launchctl setenv REFLEX_SIM open` / `launchctl setenv RX_FULL_FRAME 1` /
   `launchctl setenv REFLEX_RXCORE_PATH ...` before opening the project, or start the editor binary directly with the
   variables (`REFLEX_SIM=open RX_FULL_FRAME=1 REFLEX_RXCORE_PATH=... .../Unity.app/Contents/MacOS/Unity -projectPath ...`).
3. Enter Play. The console shows `OPEN REFLEX core: ... (open (REFLEX_SIM='open'))`; the status label goes
   `LIVE ORIGINAL PHYSICS`, then the core's load/countdown/racing frames are adapted into the old `Results` (bike, rider,
   camera, race/HUD, terrain, particles, mud). The legacy world must still boot (it is what the scene's terrain / roster
   read); the old plugin stays installed.
4. Back to the old path: remove `REFLEX_SIM` (or set it to `legacy`) and restart Unity. Unset is the default and was not changed.

## Verification (how, and what it proves)

All of this ran without Unity; the editor, Play mode and batchmode were not used. Outputs are in `builds/unity/` (previous agent) and
`builds/unity2/` (unity2).

- C layout: `builds/unity/rx_api_layout.c` prints `sizeof` / `offsetof` for every struct (`layout.txt`, 14 structs after unity2;
  regenerated together with `Assets/Tests/EditMode/RxApiLayoutExpected.cs` through `builds/unity/gen_layout_cs.py`).
- C# layout: `builds/unity/standalone/build.sh` compiles the bridge with the Roslyn that ships in the Unity 6000.6.1f1 install
  (`.../Scripting/DotNetSdk/.../csc.dll`, run with the install's own `NetCoreRuntime/dotnet`; no SDK is on PATH) first for
  netstandard2.1 (the asmdef's profile, warnings on, none), then as a net8.0 program, and runs
  `OpenReflexCheck.dll layout layout.txt`: **148 checks match, 0 differ** (struct sizes + every field offset via `Marshal.OffsetOf`,
  + the API version constant).
- The EditMode test sources compile against Unity's NUnit and run by reflection (`OpenReflexTests.dll`): 7 passed, 0 failed (with
  `REFLEX_RXCORE_PATH` set; without it the native ABI test is ignored).
- Native run: `OpenReflexCheck.dll native <dylib> <install> [frames]` loads a dylib built from this tree (`core/build/librxcore.dylib`
  after unity2): ABI check "API 2, 14 struct sizes match", default event read, session created and event loaded on the worker
  (`Phase.Ready`), pre-world getters through the P/Invoke wrappers, 12 frames, no error (`builds/unity/standalone`).
- `failpath` (no library loaded): `Phase.Failed`, a failure text, error results, no exception.

unity2 (2026-10-20), all against `core/build/librxcore.dylib` built from this tree at 07:23 (sha256
`53c78c66af4247ce90bd9638073da99c72c5331572a15135a5d6a7d17efec7c9`, preserved as `builds/unity2/librxcore_unity2.dylib` because a
later `core/build.sh` failed on another agent's in-flight edit: duplicate `RX_FUNCTION` 0x0055de70 / 0x0055dff0 / 0x005a4150 between
`game/anim_scenario_race_intro.c` and `game/race_activity.c` / `game/world_misc.c`, files edited at 07:26/07:27; nothing of unity2 is
in `game/`). Logs in `builds/unity2/logs/`; `builds/unity2/run.sh` reruns everything (`REFLEX_RXCORE_LIB=<dylib>` picks one):

- `builds/unity2/compat/build.sh` compiles `OpenReflexBridge/*.cs` **against the real old types** (`OriginalWorld/OriginalSimulation.cs`,
  `ReflexWorldNative.cs`, `Platform/ReflexNativeLoader.cs`; the renderers are replaced by `Shim.cs`, whose structs are copied verbatim
  from `OriginalParticles.cs`, `Content/ReflexMud.cs`, `Content/ReflexAtv.cs`, `OriginalAnimatedObjects.cs`). Warnings on, C# 9, net8.0.
  This is the compile proof that the adapter is consumable by the existing consumers.
- `Unity2Check.dll adapter`: a synthetic open result through the adapter - every mapped field (bike, bones, nodes, race, standings,
  HUD, race line, camera, vehicle, terrain hand-off, particles, mud, slots, timeline) reads back with its value.
- `Unity2Check.dll selector`: unset / "" / `legacy` -> Legacy; `open` / ` ON ` -> Open; unknown -> Legacy (+ one log line); `Force`
  overrides. `selector.log`.
- `Unity2Check.dll api <dylib> <install> 10` repeats `rx_api_test`'s steps through the P/Invoke wrappers
  (`builds/unity2/logs/api_cs.log`); the C driver rebuilt from the same tree (`builds/unity2/api/rx_api_test`, `api_c.log`) prints the
  identical value lines: `bike speed 0.028 gear 0 frames 19 pos 0.15`, `rider bones 69, bike pose nodes 18`, `race rc 0 valid 1
  state 0 laps 0/3 place 1 riders 12`, 12 standings, 12 slots, `terrain ... samples 2048 ... gain 2.000 version 1`,
  `terrain patch 4x4 (253.20 .. 253.95)`, `particles live 0`, `mud record 0 vehicle 141fb500 rider 1420cb60 count 12`;
  only the C driver's own "(finished)" labels and the per-process arena addresses differ.
- `Unity2Check.dll bridge <dylib> <install> 19` runs the whole open path (`OpenReflexSimulation` + `OpenReflexDriver` + adapter,
  KeepResults on): 19 results, no error, session Countdown; the adapted `OriginalSimulation.Results` holds the same values as the C
  driver (`bike speed 0.028 gear 0 pos 0.15`, race `valid 1 state 0 laps 0/3 place 1 riders 12`, 12 standings, HUD `activity 1
  speed 0.163 valid 1`, race line 13 points, vehicle `type 0 bodies 4 nodes 18 gear 1 speed 0.163`, the whole 2048x2048 terrain patch
  `heights 253.20 ..`, mud `record 0 vehicle 141fb500 rider 1420cb60`), particles 0 (none yet at the gate), ambient frame null.
  `bridge_cs.log`.
- The new exports' C reference (`builds/unity2/api_ref.c`, a copy of the driver with four extra prints, same objects):
  `hud rc 0 valid 1 activity 1 on_track 1 ... speed 0.163`, `race line 13 first 621.94 112.34 -383.91`,
  `vehicle rc 0 type 0 bodies 4 nodes 18 gear 1 rpm 1500.0 speed 0.163`, `particles array 0` - equal to `api_cs.log`.
- Not verified: the bridge inside Unity (IL2CPP / Mono marshalling, the asmdef import, the EditMode run in the Test Runner);
  `Reflex.OpenReflex` and `OpenReflexBridge` in a player build; the in-race merging / Recycle paths under load (exercised only with
  one-frame jobs); the exports above on a full race (the product reaches the countdown; ambient entities and tracks are the
  remaining reads).

### unityrace (2026-10-20): the selector is wired into `OriginalWorldRuntime`; 5000-frame bridge run

Files: `Assets/Scripts/OriginalWorld/OriginalWorldRuntime.cs` (the construction line + `OpenEventSpec` / `OpenReflexTempRoot`;
`simulation` is an `IReflexSimDriver`, `Simulation` returns `simulation?.Legacy`), `OriginalPresentation.cs` (an
`IReflexSimDriver` constructor; `KeepResults` / `Recycle` go through the selected driver), `OriginalOpponents.cs` (an
`IReflexSimDriver` `Begin` overload so the opponents' slot poses / mud are tracked on either path), and
`builds/unity2/compat/Program.cs` (`race` case). Backups and logs in `builds/unityrace/` (`run.sh` reproduces everything).

- **Compile proof (legacy unchanged)**: all of `Assets/Scripts/**/*.cs` (Assembly-CSharp, 139 files, the OpenReflex asmdef built
  separately) compiled with Unity 6000.6.1f1's Roslyn against the real UnityEngine / InputSystem / package assemblies:
  **0 errors**. `REFLEX_SIM` unset takes `LegacyReflexDriver` -> `new OriginalSimulation(world, terrain != null)` exactly as before.
- **Preserved full-race dylib**: the camsnap 5000-frame object set (`builds/camsnap/api_pf/eobj`, presencefix objects + the two
  loop/cam objects) linked as `builds/unityrace/lib/librxcore_pf.dylib` (sha256 `87bb972a...`; the object set's C driver
  reproduces `builds/camsnap/api_pf_full5000.log` exactly: 5009 frames, state racing, game 83.534, race 65.063, place 6).
  `Program race <lib> <install> 5000 0 NAT_Concord` (dt = 0 -> `rx_session_advance`, the C driver's semantics) through
  `OpenReflexSelector.Create` + `OpenReflexSimulation` + `OpenReflexSimulation.Results` -> `OpenReflexResultsAdapter`:
  **5000 results, phase Ready, no error, 9 load + 5000 frames, state racing**; `compare.py` against the C log: **10/10 values
  match** (state/frames/dt/game 83.534; bike 0.763 gear 0 frames 5009 pos 0.15 0.15 0.15; rider bones 69 / bike nodes 18;
  camera 612.56 118.25 -391.42 fov 0.943; race valid 1 state 2 laps 0/3 place 6 riders 12 time 65.063 start -65.076;
  all 12 standings; terrain 2048x2048, 4x4 253.20..253.95, gain 2, version 1; particles live 0; mud record 0 vehicle 143d8840
  rider 143ebe90 count 12). The C#-only values: HUD activity 1 valid 1, race line 13, vehicle type 0 bodies 4 nodes 18 gear 1.
  The fixed-step variant (`dt=1/60`) also runs 5000 frames/state racing; it is **0.046 s behind** the C log's race time
  (65.017 vs 65.063) because `rx_session_advance` includes the game's own ~10 us busy-wait per frame while a fixed 1/60 does not.
- **Live tree (16:33)**: `core/build.sh` exit 0 (`core/build/librxcore.dylib` sha256 `81b38e29...`); the live product **reaches
  5000 frames** (no load/cue-map hang, no unported stop): state racing, game 83.534, race time 65.063, place 11, camera rc 0,
  particles live, 14 race-line points, 12 mud records. A C driver from the same live tree (`builds/unityrace/api_live`) matches the
  same state/time/place/laps. The C# bridge's last local speed / camera / particle count differed slightly from that C driver
  (0.818 vs 0.841, 643.10 118.06 -377.14 vs 642.95 118.12 -377.80, info live 1670 vs 1631). **Resolved by getterpure (below): the
  getters are pure; those two runs were different binaries** (the C driver was compiled from the live tree at run time, the bridge
  loaded `core/build/librxcore.dylib` built earlier; a later live build gives yet other values). Same binary -> identical.
- **`RX_FULL_FRAME=1` is required** for the race activity tick. With `Environment.SetEnvironmentVariable` it did not reach the
  dylib's `getenv` on macOS (the run stayed in the countdown while the clock advanced); with the variable in the shell it runs.
- **ABI / layout / tests**: `OpenReflexCheck layout` **148 checks match, 0 differ**; the EditMode sources **7 passed, 0 failed**
  (including `NativeAbiMatches` against the live dylib); `selector` OK (unset/`legacy` -> Legacy, `open`/` ON ` -> Open, unknown ->
  Legacy); `adapter` OK; `bridge` 19 against both dylibs (countdown, 69/18 poses, 12 standings, terrain patch, mud).
- **Remaining**: the bridge has not run inside Unity (the editor, Play mode and batchmode were not started, and nothing was copied
  into the project); the live per-frame getter drift above; the open path still needs the legacy world for the scene's terrain /
  roster (and the old plugin installed); audio is still not exported (ambient entities, scene objects, node flags and tracks are,
  unityscene / tyretracks); the core event spec has no Vehicle Select / career fields.

### unityscene (2026-10-20): ambient entities / matrices, scene objects, node flags - 5000-frame scene run

Files: `core/include/rx_api.h` (additive: `rx_ambient_entity`, `rx_scene_object`, `rx_session_ambient_entities`,
`rx_session_scene_objects`, `rx_session_scene_object_matrix`, `rx_session_node_flags`; ABI stays 2, every existing struct
unchanged), `core/api/rx_session.c` (the scan now collects the objects and TextureHandles instead of counting; the export; the
scene list; node flags; the pre-world scan no longer caches 0), `Unity/.../OpenReflex/RxApiTypes.cs` + `RxApiNative.cs` (the two
structs, the four wrappers, the layout list), `OpenReflex/OpenReflexSimulation.cs` (Results fields, `AttachAmbient`,
`TrackObjects(uint[] vtable, int[] id, uint[] offset)`, node flags per frame and per tracked slot), `OpenReflexBridge/` (the adapter
fills `ambient` / `objects` / node flags; the selector forwards `TrackObjects` and `AttachAmbient` instead of warning),
`builds/unity/rx_api_layout.c` + `builds/unity/layout.txt` + `Assets/Tests/EditMode/RxApiLayoutExpected.cs` (regenerated),
`builds/unityscene/` (the C drivers and logs below).

Verification (no Unity editor; the tree at the time of the run, a dylib built from it with the first-person file's missing
`#include <stdio.h>` worked around out of tree - another agent's in-flight edit):
- Layout / ABI: `rx_api_layout` -> `layout.txt`, `OpenReflexCheck layout` = **169 checks match, 0 differ** (was 148; +2 struct sizes,
  13 + 6 field offsets); `RxApiNative.Check` -> `API 2, 16 struct sizes match`; `rx_ambient_entity` 304 bytes (`object_id` 0,
  `entity_id` 4, `kind` 8, `flags` 12, `bones` 16, `first_matrix` 20, `source` 24, `lod_distance` 28, `param` 32, `matrix` 48,
  `model` 112, `texture` 176, `texture_package` 240 - the old `ReflexAmbientEntity` exactly), `rx_scene_object` 204.
- EditMode sources: **7 passed, 0 failed** (`NativeAbiMatches` against the new dylib).
- C scene driver (`builds/unityscene/compare_probe.c`; calls the exports in the same order as the C# `Collect` after every frame,
  9 load + 5000 race frames, `dt = 0` = `rx_session_advance`): `builds/unityscene/logs/c5000_scene.log`:
  `ambient 1381 entities, 5149 matrices, 1381 objects`, `node flags local 18 [0303...]`, `slot1 18`, drop gate
  `618.693 112.637 -378.454`, state racing, frames 5009, game 83.534, race 65.063, place 9, start -65.076.
- C# `Unity2Check.dll race <dylib> <install> 5000 0 NAT_Concord 1` through `OpenReflexSelector` + `OpenReflexSimulation` +
  the adapter (`builds/unityscene/logs/race5000_cs.log`): 5000 results, phase Ready, no error, 9 + 5000 frames, racing; **every
  compared value is identical**: `ambient 1381 entities, 5149 matrices, 1381 objects`, first entity `obj 26a05610 id 0 kind 0
  flags b bones 20 first 0 source -1 model 'MaleCrowd01_Pants'`, matrix sum `31707365584.555`, first / last matrix
  `-0.0276 0.9994 569.721 116.471 -415.713` / `0.1614 -0.0469`, drop gate `618.693 112.637 -378.454`, node flags `[0303...]`
  18 / slot1 18, race place 9 / time 65.063. (One earlier C# run reported the first entity's flags as `0x2b` instead of `0x0b`:
  bit 0x20 `COMPLETED`, set when the export had to finish a node the animation job still had invalid at the instant it read. It is
  a live job/read timing, not a value difference - the matrices were identical because the export completes the node itself. A
  300-frame run was compared the same way.)
- Ambient kinds in Concord: 0=102 (single-anim loops), 1=61 (scenario actors), 2=60 (spline loops), 3=13 (crew chiefs; the ones
  put to use carry their shirt texture and package, e.g. `CrewChief_ShirtColor_Red` / `MX_MarsF_Lite_THQ_Red`; an idle one has
  neither, as the game sets the override when the chief is used), 4=1140 (grandstand crowd instances, 10 templates of 106..119),
  9=1 score-tower flag holder, 10=4 track-side flag holders; 5149 node matrices (skeletons of 20 bones each, the crew chiefs 37);
  10 instances are identity (the first of each template), 1130 carry their relative seat transform. The TextureHandle scan needs
  blocks of any size (0x48+), not just the 0x100+ entity blocks: the shirt handle is a small object.
- Pre-world behaviour: `rx_session_ambient` / `_ambient_entities` / `_scene_objects` / `_node_flags` return 0 (or 1 / -1 for an
  invalid handle) before the event's world exists and do not cache; `rx_api_test`'s pre-race line stays `entities 0` and the same
  driver's post-race line now reads the 1381 entities.
- Not verified: inside Unity (IL2CPP / Mono marshalling of the 304-byte struct and the `[Out]` arrays, the asmdef import); the
  ambient exporter's cost on the worker (1381 records + 5149 matrices + string copies per frame when `AttachAmbient` is on; the old
  plugin paid the same); the scene object list on levels other than Concord.
- The 5000-frame pair ran on a dylib built from this tree (the preserved camsnap `lib/librxcore_pf.dylib` predates the exports).
  The final live `core/build/librxcore.dylib` (sha256 `e4783cb5...`, clean `core/build.sh`) passes the ABI check and exports the
  same ambient values until race frame 1556, then stops at **unported 00526b90** (frame 1565, `builds/unityscene/logs/race5000_live_cs.log`)
  - a concurrent `game/` change, not these exports.
- Remaining: the generic raw `read` and `mode_objects` (lead decision), audio, and the adapter's ambient frame on a live driver
  swap (the primitives are there; `OriginalWorldRuntime` still creates the legacy world for terrain / roster). Tyre-track
  ribbons are exported (tyretracks, 2026-10-20).

## Camera (camerafix, 2026-10-20)

`rx_session_camera` no longer reads the render-copy chain as its primary source. The getter (`core/api/rx_session.c`) now
returns the camera the game's own update writes, in this order:

1. the local player's **active camera entity** `P+0x9a8` (the start-gate camera on the grid, then the GamePlay
   `MXThirdPersonCamera`), reading its embedded `Camera` at `+0x90` (vtable `0x0095a2d8`, pos `+0x10`, right/up/forward
   `+0x20/+0x30/+0x40`, fov `+0x50`, near/far `+0x58/+0x5c`), and requiring the entity's "pose valid" `+0x2f4 = 1`;
2. during the intro fly-by (`P+0x9a8 = 0`), the world's inline `MXAnimatedCamera` at `world+0x218f0`, Camera at `+0x90`,
   requiring the entity's "pose set" byte `+0x2b0 = 1` (the object `007473f0` copies into the scene's view);
3. the render copies as a last resort (`[0xa7b1d8]`, else the static `Camera 0xd6f620`).

All three are the same fields (`+0x54` aspect is the only difference and `rx_camera` does not export it). A camera with no
lens (`fov <= 0` or `far <= near`) is rejected (return 2), so the getter never hands Unity an unposed/uninitialised
camera. The pre-world path still returns 1 / 2 as before.

Why the product had no camera: the spawn's **0054bc00** (the player's five view cameras `P+0x974..+0x984`, the start-gate
camera `P+0x99c`, the quick camera `P+0x998`) was a product no-op, so no camera entity existed for `Player_UpdateCamera`
and `Player_UpdateCameras`. It is now ported (`core/game/loop_cam_build.c`) and called at the original's site in
`core/game/build_activity_spawn.c`: the three constructors (`0063f160` factory, `00649610` third person, `00647cf0` start
gate, `00646d70` quick), the tuning apply `vf32` (`0064a330` / `00646b70` / `00647700`), the class methods the camera path
calls (`00649bd0` / `00647e00` / `00647e40` / `0063de60` / `006534e0` / `0063f110`) and the camera tuning file load
(`0063f240`/`0065bdb0`, through the ported BXML reader; called from `build_package_loaders` in `rx_session.c`).

Cameraschema (2026-10-20): the tuning schema is now ported.
- `core/game/loop_cam_schema.c` (`rx_camera_schema_init`, registered at **00908500**, the CRT static initializer that
  `rx_globals_static_init` runs in the original's slot 0x916cd0; the helpers `006596f0` construct / `00659ef0` fill):
  the nine 0x1da80-byte class records at `0x00af6d10` now hold their 14 preset-type entries (0x404 each: 94 records per
  entry, name + kind at +0x40 + the default) and all 94 x 14 setting records. `0065c200` therefore parses the bxml into
  the real tables: the GamePlay (`type 2`) third-person camera's `vf32` gives fov 54 deg (`+0x4cc` 0.942477703,
  `+0xe0`), Distance_FS_* / Angle_FS_* / LOS_* / collision / effects from `Cameras\MX_Camera.bxml` (e.g. `+0x4e4` 4,
  `+0x4e8` 10 deg, `+0x510` 2.2, `+0x5c4` 0.942477703, `+0x5c8` 1.66630054), near 2 / far 15000 from the Camera base.
- Verification: baton-boot Verify `rx_CameraSchemaInit` 1/1/0, `rx_CameraSchemaConstruct` 9/9/0,
  `rx_CameraSchemaFill` 9/9/0; On mode (the core initializer + the original parser) gives a **byte-identical** schema at
  frame 300 to the reference. `rx_MXThirdPersonCamera_vf32` (0064a330) is 4/4 and 8/8 calls, 0 mismatches with the real
  schema (it was fed zero tables before), and an On-mode boot with the core schema + core `vf32` reproduces the whole
  camera dump (all five view cameras, quick, gate, the active entity, the world camera) of the reference for frames
  500..1000 exactly.
- First person: the ctor `00641280` (`rx_first_person_camera_construct`) and the matrix-row helper `00407410`
  (`rx_cam_pose_row`) are ported and verified in the boot (1/1/0 and 184/184/0). The slot `P+0x97c` is still **not
  created**: its per-frame update `MXFirstPersonCamera::vf19 00642680` (16,142 bytes) is not ported. The original's
  boot creates the camera and calls that update 112 times (probed), so the port is the only step left; its callees are
  all ported (00623bd0, 0063d920, 0063dee0, 0063e400, Float_Abs, SqrtF, Vec3_Cross, 00407410).
- Intro camera: `0063d780` / `0063e7c0` are **gameplay** (the scenario camera's label and its path clip, not render
  setup) and are ported (chain round 3, group rx_anim). The reference boot poses the world's inline `MXAnimatedCamera`
  (`+0x2b0 = 1`, path clip with length 3.333333, clock advancing, Camera position/fov moving) and the core
  `rx_animated_camera_vf19` + `rx_animated_camera_at` reproduce it exactly in On mode. **FIXED 2026-10-21 (agent
  "introcamera")**: the product's `world+0x218f0` is posed through the whole intro. The scenario was already ticking it;
  the pose was held by `0063d7e0`'s inlined pause test (PCSession / device manager / window focus, absent in a headless
  product) and the per-step clips did not resolve because the init-thread stand-in skipped `006b68e0`'s AnimationSets
  alias table; the path fov also needs the display aspect (`graphics.cfg` 1280 x 720, the ctor leaves 640 x 480). All
  three are in `core/notes/loop.md` "Introcamera". `rx_session_camera` now returns the world camera (rc 0, finite,
  moving pose/fov) from the intro's first clip to the start-gate camera's onset; the gate / chase / first-person
  cameras are unchanged. Oracle-identical first clip (bit for bit at the same path clock) and hero fov.
- Pre-existing, not this task: the third-person constructor `00649610` Verify shows 4 calls / 96 words of difference
  (the first at the base camera's fov word, `3f490fdb` vs 0, then uninitialised heap residue); it reproduces with the
  camerafix CLI (no schema port). `00647e00` / `00647e40` / `00649bd0` are not reached by the boot or the world test.

## View selection (cameraselect, 2026-10-20)

The shipped **Camera key is C** (DirectInput scan `0x2e` -> the game's key id `0x25` through the `Keyboard_InitScanTable`
table `[0xa95848]`; the HUD's "Camera" text names the same key). The pad binding is button `0x10` (Start). The key is a normal
game input, not a UI-only action:
`rx_session_set_input(keys[0x2e] = 0x80)` -> `Keyboard_Poll` -> the key record -> the "Camera" column of the
`ControllerRemapping` table (default keyboard id 0x25 / pad 0x10, `UISettings_Reset` 005b80b0) -> `Controller_BuildKeyMaps`
00547f30 `KeyMap_SetSlot` into the controller block's `+0xd48` (keyboard), `+0xdec` (pad) and `+0xe90` (mouse) maps, whose
compound KeyMap is at `+0xf34` -> `Player_UpdateCamera` 0054c150 (`KeyMap::vf1` 00811420, i.e. the press edge: key state 0
asks for record +0 == 2 with no modifiers) -> `Player_SetCamera` 0054c9b0. The cycle is 0 -> 1 -> 2 -> 4 -> 0 (index 3 is
skipped).

`Player_SetCamera` 0054c9b0 was still a product abort (`rx_call_original` at the camera-key site). It is now ported in
`game/loop_player.c` as `rx_player_set_camera`, together with the "Camera Changed" event 00554ce0
(`rx_player_camera_changed`: the type signature `.P6AXPAVMXPlayer@@PBD@Z` at 0x9ce4e4 is named, then the payload
`{P + 0x910 + 20 * camera, player}` (8 bytes) is dispatched through the activity's event system at `+0x290` under the name
`0x9416b0` "Camera Changed"). The key call site in `rx_player_update_camera` now calls the port at the original's call
return address (0x0054c32d). Byte-for-byte behaviour: the guard (human local player + vehicle + activity state 1..3), the old
view entity's `+0x350..+0x368` offsets when its `+0x920 + 0x14 * old` flag is clear, `P+0x988 = camera`, the replay
`+0x21de0 = 0` / `ReplayCameras_Update(0.033 s)` / director vf29 / `0xa9434f = 0`, the per-controller index
`[0x9c5410 + 4 * controller]`, the UISettings `DrivingCamera` write-back (`settings + 0x48 * controller + 4`:
0 / 1 / 3 / 2 for camera 0 / 1 / 2 / 4) + `UISettings_Apply`, then the change event.

- **API**: `rx_session_set_camera(s, mode)` (`core/api/rx_session.c`) calls the same setter. API mode 0 = the default
  third-person **GamePlay** view (the game's entity 1, the shipped `[0x9c5410]` value and the session's default), 2 = **first
  person** (entity 2). 0 ok, 1 invalid / error session, 2 no local player or the game's guard refused it. The other entities
  (0 Visual, 4 User) stay reachable only through the key's cycle, so no view is invented. Nothing changes unless it is
  called: a session that never calls it keeps the game's default `P+0x988 = 1`.
- **C#**: `RxApiNative.rx_session_set_camera(s, mode)` and `OpenReflexSimulation.SetCamera(mode)` (queued through
  `BetweenFrames`, so it runs on the session's worker before its next job); since unityfinal also
  `IReflexSimDriver.SetCamera(mode)` (the selector's runtime surface: open forwards, legacy ignores it). No `Results` field
  was added: the selected view is already visible in `Results.camera` (first person has fov 60 deg / near 0.5).
- The near plane: the task text said 2; the **FirstPerson tuning's near plane is 0.5** (60 deg / 0.5 / 15000; the oracle's
  own measured first-person row in `notes/input-settings.md` is 60.0 / 0.5). The gate / third-person cameras are 45 deg /
  near 2 / far 15000 and GamePlay 54 deg / near 2 / far 15000, unchanged.

Verification (`builds/cameraselect/`, product build via `core/tools/fp_camera_test.sh builds/cameraselect/api`;
`RX_FULL_FRAME=1 fp_camera_test "game files/Archive" 1400 api|key|default`):
- Racing starts at frame 1125; the start-gate camera owns the view until the vehicle is 10 units away or input arrives, so
  the driver holds W and the gate leaves after 34 frames.
- **API** `rx_session_set_camera(s, 2)` rc 0: `P+0x988` 1 -> 2, `[0x9c5410]` 2, the active entity becomes `P+0x97c`, and
  `rx_session_camera` returns rc 0 `pos 614.261169 117.345848 -382.068024`, **fov 1.04719746 (60 deg), near 0.5, far
  15000**, right / up / forward lengths **1.000000**, all finite (`builds/cameraselect/logs/api.log`).
- **Key**: one C press through `rx_session_set_input` (`keys[0x2e] = 0x80` for one frame, then released) gives the
  **identical** selection and the identical camera (`key.log`, `P+0x988` 1 -> 2, same pos / fov / near / far). Further
  presses now cycle on to entity 4 (User), 0 (Visual) and back to 1: `00649ed0` `MXThirdPersonCamera::vf30`, the User view's
  per-frame tweak that the key path hit, is ported (camcycle, `core/notes/loop.md` "Camcycle"; the product test's `cycle`
  mode).
- **Oracle** (`builds/cameraselect/cli`, adapters in `oracle/loop_player.cpp`; a probe calls the original setter on a live
  event-boot player, `RX_FP_PROBE_SETCAM`): `rx_Player_SetCamera` **3 calls / 3 verified / 0 mismatches** (one from
  `0054aef0` during player creation, two probe calls 1 -> 2 -> 1) and `rx_Player_CameraChanged` **1/1/0** (the event + its
  dispatch on the same state). The oracle's own first-person camera at racing frame 800 (`RX_FP_PROBE_SELECT` +
  `RX_FP_DUMP_CAM`, `builds/cameraselect/logs/oracle_fp_cam.log`): `pos 654.362854 117.07412 -375.298004` (the known grid
  offset vs the product), **fov 1.04719746, near 0.5, far 15000**, basis lengths 1.000000 (orthogonal) - the same lens as the
  product's first person, so the 0.5 is the game's FirstPerson tuning, not the product's.
- **Back**: `rx_session_set_camera(s, 0)` rc 0 -> `P+0x988` 1, the GamePlay entity 1 active, fov 0.951996505 (the moving
  chase camera), near 2, far 15000.
- **The built dylib** was also driven end to end through the exported API only (`builds/cameraselect/dylib_camera_probe.c`,
  linked against `librxcore.dylib`, run against the preserved `builds/cameraselect/librxcore_cameraselect.dylib`): during the
  **countdown** `rx_session_set_camera(2)` rc 0 at frame 861 (the setter accepts activity state 1; the game's start-gate
  camera still owns the view, fov 0.785398); at racing (frame 1125) the default/gate camera is rc 0 fov 0.785398 / near 2 /
  far 15000, `rx_session_set_camera(2)` rc 0, **first person after 34 W frames** rc 0 fov 1.04719746 / near 0.5 / far 15000,
  `rx_session_set_camera(0)` rc 0 -> third/default rc 0 fov 0.949830 / near 2
  (`builds/cameraselect/logs/dylib_probe.log`).
- **Gameplay unchanged**: the three runs (default / api / key) apply identical inputs and advance the same frames; at frame
  1205 all report bike speed 17.6739464, gear 0, race state 2, laps 0, place 2, time 1.334000 and the **same bike-pose hash
  `64c02d52`**. The rider-pose hash differs between the control and the selected runs (`1213b56f` vs `24fc97f8`; api and key
  are identical): 59 of 69 bone matrices differ by at most 2.75e-4, all finite - the game's own first-person head / aim
  springs (`0063dee0` in vf19) pose the rider, not a wreck or a physics change. The ride then stops at unported `00621100`
  (a concurrent rider-animation function) 80 racing frames later, in every mode.
- Oracle: the selection itself is verified by the `rx_loop` adapters above; the first-person pose it displays is the already
  bit-exact vf19 (fpbasis: A/B byte-identical at the same frame / state), and the tuning is cameraschema's real schema. The
  Camera-key transition had no earlier product coverage.
- **Regressions / ABI**: `verify.sh rx_loop quick` 13 ports, `rx_world quick` 132, `rx_race quick` 48, **0 mismatches, On
  identical** (`builds/cameraselect/logs/verify_*.log`); `core/build.sh` clean (`librxcore.dylib` sha256 `6d93540c...`,
  22:00); the C layout program + the C# `OpenReflexCheck layout`: **196 checks match, 0 differ**; `OpenReflexCheck native`
  loads the dylib: "API 2, 18 struct sizes match", 12 frames, no error. The C# sources compile in the Unity 6000.6.1f1
  Roslyn against netstandard2.1 and net8.0 (`builds/cameraselect/cs/build.sh`).
- Note on the oracle build: the concurrent `race_beh1_small.c` edit was mid-flight (two functions called before their
  definitions); the verification CLI used a copy with only the two forward declarations added
  (`builds/cameraselect/patched/race_beh1_small.c`). The original file was not touched.

## Getter purity (getterpure, 2026-10-20)

The unityrace drift (local speed 0.818 vs 0.841, camera 643.10/118.06/-377.14 vs 642.95/118.12/-377.80, particles live
1670 vs 1631) is **not** a getter side effect: the getters do not change gameplay. It was a comparison of two different
binaries. Reproduced with one binary, the C driver (no getters until the end) and the C# bridge (every getter after every
frame) are bit-identical.

Test: `builds/getterpure/getter_probe.c` (linked against a dylib; modes `none` / `all` / one getter group per run), plus
`builds/getterpure/dl_probe.c` (dlopen; the product dylib hides the arena helpers). Both run the reference event, then print
every getter at full precision and the C# bridge's values; `getter_probe` also prints a guest state hash (`world hash pre`)
before the final getter calls: FNV-1a over the exe's writable sections (0x009a6000..0x00dcf76c, 0x00dd0000..0x00dd0811) and
every live heap block (guest bytes, so process independent), i.e. the verify memory-hash idea on the product arena.
`builds/getterpure/readme.md` has the commands and logs.

- **Preserved full-race dylib** (`builds/unityrace/lib/librxcore_pf.dylib`, 5000 frames, `RX_FULL_FRAME=1`): `none` and `all`
  have the **identical values** (bike speed 0.762747407, place 6, race time 65.0630035, camera 612.555359 118.246635
  -391.417938, particles 0). `none` pre hash `6a43fd846de4a3d4`, `all` pre hash `cf350756890f8301`; after the final getter
  calls `none` becomes `cf350756890f8301` too. The whole difference is the pose getters' lazy node evaluation
  (`rx_world_node_evaluate_world`: node world matrices, record flag +0xc bit 0, node set +0x34) - derived state the renderer
  computes anyway. One end call and 5000 per-frame calls converge to the same arena bytes, and the race is identical.
- **After the fixes** (dylib `builds/getterpure/lib/librxcore_gp.dylib` = the camsnap objects + the current
  `api/rx_session.c` and `game/race_aidata.c`; sha256 `030c6bf7...`): `none`, `all` and every single-getter mode
  (`bike`, `pose`, `camera`, `particles`, `terrain`, `ambient`, `mud`, `race`, `standings`, `slots`, `hud`, `veh`, `line`,
  `slotpose`) at 5000 frames all end with the same gameplay values (bike 0.762747407, place 6, race 65.0630035, camera
  612.435364 118.232681 -391.13092; the camera offset from the numbers above is the current api's concurrent additions
  (aidata's `rx_aidata_load`), not a getter), and every mode's post hash converges to `eb49ee892df75001`; `none` pre hash
  `3376b849d995e98f`, `all` pre `eb49ee89...` (after the final getter call `none` becomes `eb49ee89...` too). Repeated calls in
  one frame: **0 getters differ** (`twice differ 0`; before the fix, `terrain` was the only one).
- **Live dylib** (`builds/getterpure/lib/librxcore_live_1720.dylib`, the 17:20 `core/build/librxcore.dylib`, sha256 of the
  original `cbc08e0c...`): `dl_probe` `none` / `all` / `input` (only `rx_session_set_input`) / `allinput` are **identical**:
  bike 0.776433289, place 9, camera 635.77887 118.167137 -383.901978, particles live 1670 normal 425. The C# bridge against
  the same dylib prints the same values. The 16:33 numbers (0.841/1631 and 0.818/1670) differ from the 17:20 build (0.776/1670),
  i.e. the live game code changed between the dylib the bridge loaded and the source the C driver was compiled from.
- **Bridge vs C driver, same dylib**: `Unity2Check.dll race <fixed dylib> <install> 5000 0 NAT_Concord 1` against the C driver
  `builds/getterpure/rx_api_test` -> `builds/unityrace/compare.py`: **10/10 values match, 0 differ** (`compare_gp.txt`:
  state/frames/game 83.534; bike 0.763 gear 0 frames 5009; 69/18 poses; camera 612.44 118.23 -391.13; race place 6 time 65.063;
  12 standings; terrain 2048/2/1 and the 4x4 patch; particles 0; mud record 0 count 12).
- **Current live tree (17:44 build, after `core/build.sh`)**: the full race now stops at race frame 1176 in unported **004392e0**
  (`RavenSimulationMotocross::vf19`) - a new code path from a concurrent change, not the getters: `dl_probe` `none` and `all`
  stop at the same frame (1176) with identical values (bike 0.437456757, place 11, particles 1937), and the bridge reports the
  same error. The 5000-frame measurements above use the preserved object set and the 17:20 live copy, which run the whole race.
  The final `core/build.sh` of this change (18:07) is clean (`librxcore.dylib` sha256 `7dc5eef5...`, log
  `builds/getterpure/logs/core_build_final.log`).

What each getter does to memory (all of it session or derived state, nothing the simulation reads back):

- `rider_pose` / `bike_pose` / `_slot`: `rx_world_node_evaluate_world(rec, 0)` per record (subtree = 0, so only the record and
  its invalid ancestors are computed): world = local x parent for invalid records, sets record flag bit 0 and the node set's
  dirty word +0x34. Repeated calls are identical (the flags are already valid). Evidence above: per-frame evaluation changes
  no gameplay value and no final arena byte compared with one evaluation at the end.
- `terrain`: session-only deformation version/hash and, since this change, a per-game-frame dirty-rectangle cache
  (`terrain_frame` + `terrain_dirty_*` in `struct rx_session`): two calls in the same frame now return the same rectangle and
  version (it used to report the change once and "nothing" the second time). `active_terrain()` is now read-only: an
  uninitialised function-local static list (guard 0x009e30e0 bit 0 clear) simply has no entries; the old code set the guard
  and count 0 there, which `rx_hfterrain_register` would do on the game's own first use anyway.
- `ambient`: the object scan and the texture-handle cache are one-shot session state (`ambient_scanned` / host arrays);
  `ambient_rescan` resets it deliberately. `scene_objects` / `scene_object_matrix` / `node_flags` (unityscene) only read or
  cache the entity list in the session.
- `camera`, `race`, `hud`, `race_line`, `standings`, `slots`, `vehicle_info`, `particles` / `_array`, `mud` / `_count`:
  pure reads of game memory (the particle export sorts a temporary host array; the camera getter takes the active camera
  entity, the intro camera or the render copy, read-only).

`rx_session_terrain`'s dirty rectangle is still a **delivery**: a caller that skips frames gets the accumulated change
reported on its next call in a later frame (the whole grid, because the core has no per-texel dirty tracking yet).

**Tile rectangles (w-terrtiles, additive).** `rx_session_terrain_tiles(s, rx_terrain_rect* out, max)` reports the same delivery as up to
16 rectangles of whole 32 x 32 tiles instead of one bounding rectangle (riders' ruts are far apart: the bounding rectangle was
45-100 k texels a frame for 50-90 changed texels). `terrain_dirty_update_hint` marks a byte per tile for every word it finds changed
(a bitmap that is all zero between updates), then `terrain_dirty_tile_rects` joins runs of tiles per tile row and extends a run into
the rectangle above it with the same columns (an exact cover), and `terrain_dirty_reduce` merges the pair whose bounding box adds the
least area while there are more than 16 (then any overlapping pair). A new generation (first call, resync, reload) is the whole grid
as one rectangle; no change is 0 rectangles. The call runs `rx_session_terrain` itself, so it may be called alone and a second call
in a frame returns the same rectangles. A `max` below the count merges further to fit. `rx_session_terrain` is unchanged (the
single rectangle is still what the hint scan found). Tests: `core/tools/terrain_tiles_test.c` (brute-force reference, 16 k frames),
`core/tools/terrain_tiles_trace.c` (per-frame hashes against any librxcore: the baseline and the new core give identical traces).
Tracks: `rx_session_tracks` (count / triangles) and `rx_session_track_ribbons` of one frame share one decode (`tracks_cache` in the
session, keyed by tracks_version + frame + terrain; `RX_TRACKS_NOCACHE=1` turns it off for A/B).

## Final full-race test (unityfinal, 2026-10-21)

Build dir `builds/unityfinal/` (C driver, C# harness, comparator, `run.sh`, logs). No Unity editor / Play mode / batchmode; no
`core/game` edit; nothing copied into the project.

- **Dylib tested**: `core/build/librxcore.dylib`, sha256
  `19d3454ae1cb4b13d9fd4319800c2c0ff185c87684105b9f5ff7ee3027999de2` (the 00:59 clean `core/build.sh` product, the latest
  runnable build; loaded through dlopen exactly as the C# loader does).
- **Bindings**: every export added since unityrace is bound and used - `rx_session_set_camera` (plus the Camera key through
  `rx_session_set_input`), `rx_session_ambient_entities` (records + node matrices + info), `rx_session_scene_objects` /
  `_scene_object_matrix`, `rx_session_node_flags`, `rx_session_tracks` / `_track_ribbons` (`rx_track_info` /
  `rx_track_ribbon`), `rx_session_hud`, `_race_line`, `_vehicle_info`, `_particles_array`. `rx_api.h`'s 50 `rx_session_*`
  exports (all but the `rx_session_state` enum) have a wrapper each; `RxApiLayout.Structs` covers all 18 structs. **No struct
  changed** (layout stays 196 checks: 18 sizes + 177 offsets + the API version) and no wrapper was missing.
- **One additive bridge change**: `IReflexSimDriver.SetCamera(int mode)` (`OpenReflexBridge/OpenReflexSelector.cs`). The
  runtime holds `IReflexSimDriver`, so the new setter was only reachable through `driver.Open`; the open driver now forwards to
  `OpenReflexSimulation.SetCamera` (queued through `BetweenFrames`, i.e. before the next frame), the legacy driver ignores it
  (its cameras are the old plugin's). `REFLEX_SIM` unset/legacy is unchanged; the whole Assembly-CSharp compiles (139 sources,
  0 errors).

### The run

`builds/unityfinal/harness` = the real path (`OpenReflexSelector` Force Open -> `OpenReflexSimulation` ->
`OpenReflexResultsAdapter` -> old `OriginalSimulation.Results`, `KeepResults`/`Recycle`, `AttachAmbient`, `TrackSlots(1..11)`,
the start-gate `TrackObjects`). Input is the game's own path, firstride's sequence: W from the gate drop, A/D from race frame
200 (period 160 square sine). Views: race 100 API `set_camera(2)` (first person), 250 / 400 / 550 the **Camera key** (DIK
0x2e, the 2 -> 4 -> 0 -> 1 cycle), 700 API first person again, 900 API back to the default third person - all four shipped
views (GamePlay 1, FirstPerson 2, User 4, Visual 0) are exercised. After every frame it calls the bridge's `Collect` getters
(and `cdriver.c` the same via the C API), and once after race frame 800 the exports the old `Results` do not carry
(`RxTrackInfo`/`RxTrackRibbon`, `RxSlot`, the scene object list, terrain info). Both sides print the same canonical
`final key value` block and a per-frame FNV fingerprint; `compare.py` diffs both.

| | dt=0 (`rx_session_advance`, the C driver's semantics) | dt=1/60 (what `OriginalWorldRuntime` submits) |
|---|---|---|
| steps / results | 2127 (9 load + 9 early-Finished + 1106 countdown + 1 transition + 1011 racing) | 2179 (9 load + 9 early-Finished + 1159 countdown + 1 transition + 1010 racing; the next racing frame aborts) |
| last completed racing frame | 1010 (1011 racing frames) | 1009 (1010 racing frames) |
| session frames / game time at the stop | 2136 / 35.640870 | 2188 / 36.499620 |
| stop | unported `7859afc0` (`_CIpow`): `rx_session_advance: unported game function 7859afc0 reached (the core aborted; the session is unusable) (frame 2136)` | the same at frame 2188 (firstride's number) |
| bike | 19.694 ft/s at the stop, max 75.521 at race 228, gear 0, pos 522.092 116.050 -95.203, 2 non-finite matrix-pad words (`position[3]`, `position[7]`) from race 309, xyz/speed finite | 9.987, max 75.098, gear 0, pos 592.378 116.039 -177.178, the same two pad words |
| rider / bike pose | 69 bones, 0 non-finite, hash `9a22c2b7ec5b6633`; 18 nodes, hash `3c38435b73fef50b` | 69 bones, 0 non-finite; 18 nodes |
| race | valid 1, state 2 (InProgress), 0/3 laps, place 12, 12 riders, time 16.854 | valid 1, state 2, 0/3, place 12, 12 riders, time 16.833 |
| camera | first person at 102/702 (fov 1.047197, near 0.5), key User at 252 (1.067337), Visual at 402 (0.872976), GamePlay at 552 (0.904652), API back to third at 902 (0.984535); 2116 rc-0 frames, fov 0.195962..1.067414 | same views (1.047197 / 1.064726 / 0.879732 / 0.952960 / 0.946823); 2168 rc-0 frames |
| terrain | whole grid 2048x2048, 253.1992..253.9492, deform 0.501961, gain 2, version 1 | same |
| particles | last 2292, max 2810, info live 2918 / normal 2292 / additive 0 / soft 1, `Nat_Concord_A` | last 2782, max 2820, live 2898 |
| ambient | 1381 entities / 5149 matrices / 1381 objects, first `MaleCrowd01_Pants` (obj 26ad69a0, 20 bones) | 1381 / 5149 / 1381 |
| scene objects | 2704; start gate (vtable 0x0095fc24, id 1354312) 618.6926 112.6366 -378.4535 | same |
| node flags | local 18 `0303...03`, slot 1 18 | same |
| tyre tracks (probe at race 800) | 7722 triangles / 23166 verts / 62 ribbons / 108 chunks / 24 slots / 3 sets, stride 24, fmt 1, v1926, `Generic`, ribbon0 type 1 200 triangles | 7750 / 23250 / 48 ribbons / 100 chunks, v1979 |
| HUD / race line / vehicle | valid 1, activity 1, on track, not wrong way, 21.531 ft/s; 4 line points; type 0, 4 bodies, 18 nodes, gear 1, 12104.4 rpm | wrong way at the stop, 7.740; 5 points; type 0, 4 bodies, 18 nodes |
| mud | 12 records, record 0 amount 0.075910 | 12, record 0 amount 0.095744 |
| slots | 12 (probe) | 12 |

**Comparison**: `compare.py` (10 groups + the per-frame fingerprints) against the C driver on the same dylib:
**`RESULT: ALL MATCH` for both dt variants** - 2127/2127 and 2179/2179 frames with 0 fingerprint mismatches
(`builds/unityfinal/logs/compare5000_dt0.txt` / `_dt60.txt`). Same binary -> identical values, including the guest addresses
in the ambient / mud records (the arena mapping is deterministic).

The **stop** is the known vehicle/collision bug: the bike matrix's padding words (`position[3]`, `position[7]`,
`velocity[3]`) go non-finite at racing frame 309 (dt=0 run) while the xyz and speed stay finite (the harness counts
`bike_nonfinite_max 2` over position[0..15] + velocity[0..2] + speed; the rider pose stays finite), and `7859afc0` `_CIpow` is
reached on the advance after the last completed racing frame (1010). `builds/firstride/firstride` on the same
dylib (no camera selection, its own 1/60 step) stops at the same place: 2179 steps, session frame 2188, race frame 1010,
bike pos 592.3782 116.0386 -177.1776, max 75.0978, race 16.833, 32/33 checks (the known non-finite-bone sampling, the
pre-introcamera distance bound, no_stop). The view selection does not change the ride (same stop with the camera schedule
disabled). **Update (unitycheck, 2026-10-21): on the current dylib (`69a6f872...`) this stop is gone** - the crtpow
`_CIpow` special path and the later game work let the same harness complete the requested 5000 results with no error
(see "unitycheck" below).

### Tests

- C layout vs C# `Marshal`: **196 checks match, 0 differ** (`builds/unityfinal/layout.txt`, regenerated from the current
  `rx_api.h`; identical to `builds/unity/layout.txt`; unchanged since cameraselect).
- ABI against the dylib: **API 2, 18 struct sizes match** (`RxApiNative.Check`).
- EditMode sources (Unity's NUnit, run by reflection): **7 passed, 0 failed, 0 ignored** (incl. `NativeAbiMatches`).
- Legacy compile: all **139** Assembly-CSharp sources, **0 errors** (Unity 6000.6.1f1 Roslyn, no editor).
- `builds/unityfinal/run.sh` reproduces all of it.

### Remaining risks (nothing ran inside Unity)

The editor / Play mode / batchmode were not started, nothing was copied into the project: IL2CPP / Mono marshalling of the
304-byte `rx_ambient_entity` and the `[Out]` arrays, the asmdef import, the Test Runner and a player build are unverified.
`RX_FULL_FRAME=1` must be in the Unity process environment (not `Environment.SetEnvironmentVariable`). The open path still
needs the legacy world (terrain / roster / textures) and the installed old plugin; `Simulation` is null and audio / career
consumers no-op. The stop above ends the session with `Results.error` + `Phase.Failed` (the fault guard turns the abort into
an error, not an editor crash); the session is unusable afterwards. `AttachAmbient` + 11 tracked slots cost real worker time
(1381 records + 5149 matrices + 11 pose reads per frame). The generic raw `read`, `mode_objects`, audio and rumble are still
absent (lead decisions), and the core event spec has no Vehicle Select / career fields.

**Play steps**: `cd core && ./build.sh`; quit Unity; launch it with `REFLEX_SIM=open`, `RX_FULL_FRAME=1`,
`REFLEX_RXCORE_PATH=<abs path>/librxcore.dylib` in its environment (launchctl or the editor binary directly); Play ->
`OPEN REFLEX core: ...` in the console, load/countdown/race from the core, `Results.error` at the stop; unset `REFLEX_SIM`
(or `legacy`) and restart for the old path. Do not copy the dylib while Unity is running.

## Harness and editor-mode compile after the namespace rename (unitycheck, 2026-10-21)

No Unity editor / Play mode / batchmode was started and nothing was copied into the project. Build dir
`builds/unitycheck/` (scripts, logs, backups of every edited file in `builds/unitycheck/backup/`); the race run reuses
`builds/unityfinal/run.sh` and writes its usual logs.

### Namespace and assemblies

- C# namespace `Reflex.Port.RxCore` (`Assets/Scripts/OpenReflex/*.cs`), bridge `Reflex.Port.RxCore.Bridge`
  (`OpenReflexBridge/*.cs`), tests `Reflex.Port.RxCore.Tests` (`Assets/Tests/EditMode`). The asmdef *names* stay
  `Reflex.OpenReflex` / `Reflex.OpenReflex.Tests` (assembly file names and the runtime-facing assembly are unchanged);
  `rootNamespace` now names the new namespace. The rename removes the collision with
  `Reflex.Port.OpenFrontEnd.OpenReflex` (the `OpenReflex.Prefs` key) that made `Assets/Editor/OpenFrontEnd/OpenFrontEndTests.cs`
  fail with CS0234. No C# file in `Assets/` still names the old namespace.
- Harness sources updated to the new namespace: `builds/unityfinal/harness/Program.cs`, `builds/unity/standalone/Program.cs`,
  `builds/unity2/compat/Program.cs`, `builds/unity/gen_layout_cs.py` (the generator now writes `Reflex.Port.RxCore.Tests`;
  `Assets/Tests/EditMode/RxApiLayoutExpected.cs` already had it). `builds/unityscene/*` needed no change; the old strings left
  under `builds/unityrace/backup` and `builds/unity2/backup` are historical copies and were not edited.
- `builds/unityfinal/compile_runtime.sh`, `builds/unityrace/compile_runtime.sh` and the copy under
  `builds/unityfinal/compile/` now leave every *project* assembly out of the `Library/ScriptAssemblies` reference list
  (`Assembly-CSharp*`, `Reflex.OpenReflex*`) and reference only the `Reflex.OpenReflex.dll` built from source, so the stale
  `Library/ScriptAssemblies/Reflex.OpenReflex.dll` (06:14, not built from the current sources) can no longer shadow the
  current tree. The layout and EditMode builds already compile the bridge from source and were unaffected.

### Editor-mode compile (`builds/unitycheck/compile_editor.sh`)

Unity 6000.6.1f1's Roslyn (`.../DotNetSdk/sdk/8.0.318/Roslyn/bincore/csc.dll` run with the install's own
`NetCoreRuntime/dotnet`), no editor, no SDK on PATH, C# 9, warnings on:

| assembly | sources | defines | result |
|---|---|---|---|
| `Reflex.OpenReflex.dll` (the asmdef) | 4 | netstandard2.1 profile | **0 errors** |
| `Assembly-CSharp.dll` | 139 (`Assets/Scripts`, OpenReflex excluded: own asmdef) | `UNITY_EDITOR`, `UNITY_5_3_OR_NEWER` | **0 errors** |
| `Assembly-CSharp-Editor.dll` | 49 (`Assets/Editor`) | `UNITY_EDITOR`, `UNITY_5_3_OR_NEWER` | **0 errors** |

- The editor defines `UNITY_EDITOR` for the predefined assemblies. `Assets/Scripts/Content/ReflexLevelVerify.cs` is wrapped in
  `#if UNITY_EDITOR`, so the editor-mode runtime assembly contains `Reflex.Port.Content.ReflexLevelVerify`; the old unityfinal
  legacy compile defined only `UNITY_5_3_OR_NEWER`, which is why its editor check reported CS0103 for that type.
- `builds/unitycheck/binding/BindingCheck.cs` names `typeof(Reflex.Port.Content.ReflexLevelVerify)` and
  `typeof(Reflex.Port.Tests.WorldContentTests)`; it compiles and runs: **`ReflexLevelVerify` in `Assembly-CSharp`,
  `WorldContentTests` in `Assembly-CSharp-Editor`**, i.e. the editor assembly really resolves the runtime type.
- Both logs have **0 `error CS`** and **no** `Prefs` CS0234 / `ReflexLevelVerify` CS0103 line
  (`builds/unitycheck/out/assembly_csharp_build.log`, `..._editor_build.log`). The rest is warnings only (the same obsolete
  `FindObjectsSortMode` / unused-field warnings Unity shows).
- The non-editor legacy compile (unityfinal step 5, `UNITY_5_3_OR_NEWER`, no `UNITY_EDITOR`) stays green: 139 sources,
  0 errors, so the default/legacy path also compiles with the project assemblies built from source.

### `builds/unityfinal/run.sh` on the current dylib

Dylib: `core/build/librxcore.dylib`, sha256
`69a6f872c8e3f2580a194f6205016743fb3315ceec3d1383dc3283477df5563b` (`./build.sh` at 10:11 reproduces the same hash;
`builds/unitycheck/backup/librxcore_before.dylib` is the copy the run loaded). `RX_FULL_FRAME=1`; the C driver and the C#
harness `dlopen` the same file.

| check | result |
|---|---|
| C layout vs C# `Marshal` | **196 checks match, 0 differ** |
| ABI against the dylib | **API 2, 18 struct sizes match** |
| EditMode sources (Unity NUnit, run by reflection) | **7 passed, 0 failed, 0 ignored** (incl. `NativeAbiMatches`) |
| legacy compile (Assembly-CSharp) | **139 sources, 0 errors** |
| firstride regression | 5000 steps, **3831 racing frames, no stop**, state racing, game 83.520 |
| full race dt=0 (`rx_session_advance`) | **5000 results / 5009 session frames**, phase Ready, no error, state racing, game 83.534, race 64.747, place 12 |
| full race dt=1/60 (what `OriginalWorldRuntime` submits) | **5000 results / 5009 frames**, no error, game 83.520, race 63.850, place 12 |
| comparison, both dt variants | **10/10 value groups PASS; per-frame fingerprints 5000/5000, 0 mismatches** (`logs/compare5000_dt0.txt`, `_dt60.txt`) |

The unityfinal stop (`7859afc0` `_CIpow`) is **gone** on this dylib (crtpow's port plus later game work): there is no abort
and the bike/rider report `nonfinite_max 0`. The run ends at racing frame 3883 (dt=0) / 3830 (dt=1/60) only because the
harness requested 5000 results while the race was still running (state racing, 0 of 3 laps). Values at the end (dt=0):
bike 9.070 ft/s, max 75.521 at race 228, gear 1, pos 518.399 170.465 342.443; rider 69 bones hash `93ebeb3265abb513`,
bike 18 nodes hash `3203a5dae4785efd`; race valid 1 / state 2 / 0 of 3 laps / place 12 / 12 riders with 12 standings.

Cameras (both variants, the schedule above): race 102 first person fov **1.047197** near 0.5, 252 Camera-key User
**1.067337**, 402 Visual **0.872976**, 552 GamePlay **0.904652**, 702 first person, 902 API back to third **0.984535**;
4989 rc-0 frames, fov 0.195962..1.067414. Terrain 2048x2048, 253.1992..253.9492, deform 0.501961, gain 2, version 1.
Particles info live 2904 / normal 141, `Nat_Concord_A`. Ambient **1381 entities / 5149 matrices / 1381 objects**, first
`MaleCrowd01_Pants`; start gate 618.6926 112.6366 -378.4535; node flags 18 `0303..03` (local and slot 1). Tyre tracks at
the race-800 probe: 7722 triangles / 23166 verts / 62 ribbons / 108 chunks / 24 slots / 3 sets, stride 24, fmt 1, version
1926; scene objects 2704; 12 slots; HUD valid 1; 64 race-line points; vehicle type 0, 4 bodies, 18 nodes; 12 mud records.
dt=1/60 has its own expected frame-by-frame values (7750 track triangles, version 1979, 2943 live particles) and matches its
own C driver 10/10 + every fingerprint. The comparator's `2e-6` relative tolerance absorbs a few last-digit differences in
the text block (e.g. start gate `618.692566` C vs `618.692600` C#, `rpm` `10668.650391` vs `10668.650000`); the per-frame
fingerprints are exact.

`builds/unity2/compat` was rebuilt with the new namespace and its `selector`, `adapter` and 19-frame `bridge` cases pass
(bridge: phase Ready, no error, countdown values, terrain / particles / mud present).

### Remaining risks (nothing ran inside Unity)

- IL2CPP / Mono marshalling of the 304-byte `rx_ambient_entity`, the `[Out]` arrays and the `RxApiLayout` offsets; the
  asmdef import (the `rootNamespace` rename changes no assembly file name, but Unity must recompile); the Test Runner (the
  EditMode sources run here by reflection, not in Unity); a player build.
- `RX_FULL_FRAME=1` must be in the Unity process environment (launchctl / the editor binary directly; not
  `Environment.SetEnvironmentVariable`, which did not reach the dylib's `getenv` on macOS).
- The open path still needs the legacy world (terrain / roster / textures) and the installed old plugin; `Simulation` is
  null on the open path and audio / career consumers no-op. Audio, rumble, the generic raw `read` and `mode_objects` are
  still absent (lead decisions); the bridge resets the fields it cannot fill.
- `AttachAmbient` + 11 tracked slots per frame cost real worker time (1381 records + 5149 matrices + 11 pose reads per
  frame), as on the old plugin.
- The fault guard turns an unported function into `Results.error` + `Phase.Failed` rather than an editor crash; the session
  is unusable afterwards and Play must be restarted.
- The C# harness compiles the real old types through `builds/unity2/compat/Shim.cs` and the Unity install's
  `UnityEngine.CoreModule`, so it exercises the same source but not Unity's compiled assemblies. The editor-mode compile is
  the project-level compile proof; it is not an editor import.

### Play steps (unchanged; not run by this task)

1. `cd core && ./build.sh` -> `core/build/librxcore.dylib` (clean tree, 10:11: sha256 `69a6f872...`).
2. Quit Unity (never copy the dylib while it is running). Launch the editor with `REFLEX_SIM=open`, `RX_FULL_FRAME=1` and
   `REFLEX_RXCORE_PATH=<abs path>/librxcore.dylib` in its environment (`launchctl setenv ...` or the editor binary directly).
3. Play: the console shows `OPEN REFLEX core: ... (open (REFLEX_SIM='open'))`; the load / countdown / race frames come from
   the core through the adapter. `Results.error` appears only if the session reaches an unported function (the harness
   race does not; a different human ride can).
4. Back to the old path: unset `REFLEX_SIM` (or set it to `legacy`) and restart Unity. Do not copy the dylib into the project.
