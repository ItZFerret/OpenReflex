# Product chain: getting the product (no oracle) to run frames (agent "chain", 2026-10-19)

Drivers: `core/tools/rx_api_test.sh <abs dir>` + `<dir>/rx_api_test "../game files/Archive" 10` (run from `core/`; `RX_API_TEST_WATCHDOG=<s>` raises the 300 s
alarm, needed on a loaded machine) and `core/tools/build_event_test.sh`. A named stop is `rxcore: unported game function <addr> reached` (rx_call_original in
the product); `lldb -b -o "breakpoint set -n abort" -o run -o "bt 8" -- <exe> ...` gives the caller (the C frames are named; optimised builds hide locals). In lldb,
game memory is `(char*)rx_base + addr`.

## Where the product stands

Startup, event load, StartEvent, the load frames, the vehicle and the rider spawn (animation tree, rider model, vehicle loader, mud / number plate callback) run in
the product. The first `MXGame::Update` stops in the UI world's FreeRide (below). No race frame has run in the product yet.

## What was ported (all hand transliterations; addresses in the code comments; verification in core/plan/QUEUE.md "chain")

| stop | functions | files |
|---|---|---|
| animator parameter lookup | 0083ce40 / 00834570 / 0083d430 / 008d8db0 (vf11) | game/anim_animators.c, oracle/anim_animators.cpp |
| particle effect bind | 0069b0e0, 007e3400 | game/fx_bind.[ch], oracle/fx_bind.cpp |
| surface assets | SurfaceAssetIO::vf3 007947b0, 00843ac0, 007b20f0, 007b2250, 007b2330, vf4 00794f90 | game/build_render_asset_surface.c, build_render_asset.h, build_load.c (loader table), oracle/build_render_asset_surface.cpp |
| mud / number plate callback | 0054a2e0, 0054bfd0, 005510a0, 00408fb0, 004089d0 | game/fx_mud_bind.[ch], oracle/fx_mud_bind.cpp |
| scene entity / model instance | 0080ddf0, 007f1810 | game/build_scene_entity.[ch], oracle/build_scene_entity.cpp; build_rider_model.c calls them |
| animation entity | 0082d1a0 | game/anim_entity_nodes.[ch], oracle/anim_entity_nodes.cpp |
| listener vectors | 0063b360, 0063b3f0 | game/build_rider_misc.[ch], oracle/build_rider_misc.cpp |

### Surface asset layout (recovered from 007b20f0 / 007b2330; verified by consuming every real surface exactly)
File: 16-byte header {magic 0x99bbcc00, version (4), count, 0}, `count` 0x160-byte descriptors, then per descriptor of kind 0 / 1 (kind >= 2 morph
surfaces read nothing here): skin data (descriptor +0xfc bytes, version >= 2 only; version < 2 forces it to 0), index data (+0x14 bytes, INDEX32 when
flags +8 bit 3, else INDEX16), then the vertex streams (descriptor +0xb8 streams, byte sizes at +0xbc..). Descriptor: +0 kind, +8 flags (bit 1 -> record +4),
+0xc / +0x10 / +0x14 = primitive count / vertex count / index bytes, +0x18 the D3D vertex element list (deduplicated through the renderer's declaration cache
007afd60 = rx_vertex_decl_get). Runtime object: 0x1c bytes {vtable 0x971cdc, +0x10 count, +0x14 the 0xcc-byte surface records (4-byte count in front),
+0x18 the descriptors}; record: +4 flag, +0x14 4 / 5, +0x18 index buffer, +0x1c declaration, +0x24.. vertex buffers (+0x64.. their strides), +0xa4 descriptor,
+0xc4 / +0xc8 skin data. D3D buffers are created through rx_d3d (device slots 26 / 27, Lock 11, Unlock 12), as the texture loader's.

## Corrections to earlier "product boundaries"
`007f1810` (ModelInstance::SetModel) and `0080ddf0` (Entity base description) are NOT render-only: the scene's bounds read the instance's handle (+0x60) and
the entity's scene (+0x10) / matrix (+0x30) are gameplay state. They are ported now; `build_rider_model.c` no longer stubs them in the product.

## The stop: the front end (read core/plan/QUEUE.md "chain" item 7 first)
In the original the player is spawned in the UI world's FreeRide activity before the event; the event's world switch later moves the vehicle. The product has no
front end, so api/rx_session.c has a marked stand-in (`front_end_free_ride`) that does what the front end did with the game's own functions (FreeRide row in
"Activity List UI World" 0x00adffe0, area update, setup / spawn / join), and the product stops at `00560110 MXActivityFreeRide::vf5`. The remaining FreeRide
class methods (vf2 0055ff70, vf3 00560080, vf4 00560070, vf5 00560110 and their callees 007d2820 / 005605e0 / 00560cd0 / 0055ffd0 / 007d2da0 / 00591f20, the audio
logic vf3 00582330) are all reached by the FreeRide_FR_Alpine reference world, so `verify.sh <cli> rx_race` verifies them per call. Port every scene-entity branch
(the product's UI scene is empty, the oracle's is not); where the original calls UI / audio, use the act_ui_call boundary (oracle original, product nothing) and
record it in QUEUE.md. After the join the remaining chain is: Game_EnterRealWorldAudio (ported), state 4..0xb of MXGame::Update (ported), the event world's
spawn / join (ported), then the race frames (the verified rx_race / rx_world / rx_bike frame code).

## Verification notes
- Per-call Verify of the new ports ran in the baton boot (`builds/chain/boot_port.sh <cli> <filters> <outdir> 1`), where the boot reaches them (counts in QUEUE.md).
  Not reached in any boot / world: 008d8db0, 007e3400 (only inside 0069b0e0), 0054a2e0 and the mud bind pair, 0063b360 / 0063b3f0 in the combined run (10 calls each in
  the rx_build_rider-only run).
- The surface loader itself is core-only (the oracle runs the original): checked by `build_load_test.sh` + `build_load_check` (stream consumption) and by the boot's
  own use (112,422 verified vf4 calls over the loaded surfaces).
- Pre-existing, not caused here: `rx_build11` On mode stops the baton boot at 00686290; `rx_SceneManager_AddEntity` 47 words in the baton boot (identical with the
  pre-task CLI); the documented 00564b20 / 00556ec0 / 00767200 / 0053e920 / 0053eab0 / 006d3d30 dead-data mismatches.

## Round 2 (2026-10-20): the faithful UI world and the world transfer
The marked stand-in `front_end_free_ride` is gone: `rx_ui_world_create` (build_ui_world.c) builds the UI world as FUN_0075ed60 does and the UI world's FreeRide activity runs its
own states before the event's `MXGame_EnterRealWorld` (`rx_game_enter_real_world`: the UI world's activities and players leave through 00766930, the vehicle and rider move
into the event world: RavenVehicleMotocross::vf31 00464a00 / 0061d160 = world_veh_transfer.c). How to run / debug: `builds/chain/run_api.sh` (build + run, log builds/chain/api_run.log),
`RX_BACKTRACE=1 RX_API_TEST_WATCHDOG=120 builds/chain/api/rx_api_test "game files/Archive" 10` prints the host stack at a fatal stop (lldb does not attach reliably here),
`builds/chain/boot_port.sh <cli> <groups> <outdir> v` is the baton boot Verify, `builds/chain/onrun.sh <filter>` the Concord On-mode hash, `builds/chain/vf4scan.py` classified the
273 EventHandlerProxy vf4 bodies (shape -> one generated C macro per address).
Calling convention facts learned: (1) in the product `rx_call_original` is fatal unless the function is registered with an integer-only signature (tools/gen_fnsig.py writes the
table; floats, doubles, structs and register arguments need a typed call at the call site: `race_thiscall`, `world_vcall_f`, `rx_tcall`); (2) tail-jump thunks
(`mov eax,[ecx]; jmp [eax+n]`, `jmp edx` with the caller's arguments) cannot be verified by a native (the jump leaves the verified call): they are registered in the product only; (3) a
generated handler family can be written as one macro per shape (build_event_proxy_equals.c) and registered with `RX_FUNCTION(RX_ADDR_00(addr), ...)`; (4) the event world's scene entities
subscribe to the activity channel in their vf5 and the subscribers' handlers run at the first "Activity Init" of the event world: every class's handlers are the next stops
(DropGate, ScoreTowerFlagHolder and TrackSideFlagHolder done; ThirtySecondBoard 006b7b90 -> 006b8ab0, PopUpBale / PopDownBale 006c14a0 / 006c0b60, the rider clip 006ab290 .. 006ab4a0 next).

### handlers (2026-10-20, agent "handlers2"): the scene-entity handlers verified; the handler universe closed
`core/game/world_scene_handlers.c` (21 ports; the code is the previous agent's, ABI re-audited line by line against the disassembly: frames, stack arguments, callee conventions, the
jump table of 006c14a0, the x87-free float compare of 006c14a0 / 006c0b60, the `idiv` ring cursor, the 0x18-byte handler keys of the removal templates), adapters
`core/oracle/world_scene_handlers.cpp` (group rx_world) and the probe `core/oracle/handlers_probe.cpp`. Probe: `REFLEX_PORT=rx_world,rx_HandlersProbeHook REFLEX_PORT_VERIFY=1
RX_HANDLERS_PROBE=<groups>` on a reference world at a chosen frame; groups b (bales), c (rider clip), u (unload + removal templates), e (profile statistics), h (the scene's entity-class
histogram, a diagnostic). Every port 0 mismatches, calls/verified: PopUpBale 400/400, PopDownBale 400/400, clip 006ab290 / 380 / 410 / 4a0 150/150 each, removal templates 006c4450 / 4330 /
3c70 / 4a40 / 3930 24/24 each, vf9 / unlisten 006c0f50 8, 006c0730 7, 006c1890 10, 006c1830 1, 006c2d40 10, 006c2ce0 4, 006b7b60 1, statistics 005411f0 100/100, 0050de10 200/200,
007538c0 400/400 (the freestyle branch included: the probe puts a FreestyleDriver fixture — a block whose vtable word is 0x0093fafc — into the human driver slot). `REFLEX_COVERAGE`
of the Concord + FreeRide probe runs: every instruction of the 21 functions executed. `verify.sh <cli> rx_world` (five worlds): 0 mismatches, On identical.

The handler universe (enumerated, not guessed): every caller of the event registration templates 0x006c3800..0x006c4c00 in MXReflex.exe, the live scene's entity classes (probe group h)
and the vtable -> class map. The MX event world (NAT_Concord) holds 8 PopUpBale, 7 PopDownBale, 1 DropGate, 1 ScoreTowerFlagHolder, 4 TrackSideFlagHolder, 1 ThirtySecondBoard, 13
CrewChief; SX adds 1 LeaderBoard (render only, no subscriptions). Owners: bales 006c14a0 / 006c0b60 (world_scene_handlers.c), DropGate 006bdb50 / 006bdb90 / 006bed90 / 006c3d90
(world_dropgate.c), ScoreTower 006c18e0 / 006c1910 / 006c1990 / 006c1b30 / 006c1b60, TrackSide 006c18e0 / 006c2d90 / 006c2e60 / 006c2f60 / 006c3020 and the CrewChief's First-Human
006c18e0 (world_flag_holders.c), ThirtySecondBoard 006b7b90 (+ 006b8ab0) (world_board_handlers.c), the register / unregister templates in build_scene_props.c, world_flag_holders.c,
world_dropgate.c and this file.
Not ported, with the check (product boundary, render / camera; the entity classes are in build_scene.c's `outside_core` lists, so the product creates no instance): HoleShotLight 0x1034
(006beea0 / 006bef80, subscriptions 006beff0 / 006bf060, handlers 006bf240 "VehicleEntered" / holeshot lamp, 006bf0c0 "Activity Init", 006c3590 "Activity UnInit"), LeaderLight 0x1035
(006bffa0 / 006c0080, 006c0100 / 006c0170, 006c0310 / 006bf0c0 / 006c3590), TripleLight 0x1036 (006c3370 / 006c3450, 006c34c0 / 006c3530, 006c3590 / 006c3630), SXSeatedCameraMan
0x106b camera (006c1d40 / 006c1f90, 006c1fa0 / 006c2000, 006c2060 / 006c2090 / 006c20a0 / 006c20d0). Their handlers only switch the entity's own model animation / texture / hide flag
(and the holeshot lamp), and the "HoleshotActivated" event they raise is also raised by the activity itself (race_activity.c:1537, ported); nothing in the simulation reads the
entity's state. Also not ported: AnimEntityTrigger 0x1055 (006bd010 / 006bd2e0 -> 006bd420, subscriptions 006c3a90 / 006c3b50; absent from every reference world and not built by
build_scene.c), and the particle-emitter registrations 006c39a0 / 006c3a00 of the AnimEntity*WithParticles classes 0x1058..0x105a (FX, not built). LeaderBoard has no subscriptions
(006bf600 calls none of the templates).
Product gap found: ClipRecorder_Init 006aa660 (build_rider_clip.c) has no product caller — only the oracle adapter — so the four stunt listeners are registered only when the unported
init thread 0075ed60 runs; the driver must call `rx_clip_init()` at the original's place. The bales' replication 006f7910 / 006f7a10 is the network boundary (the Quazal reference
handle 0x00d60d54, null offline; nothing reads the replicated record) and 00401ea0 (ModelEntity::vf9, render-list removal) runs through rx_call_original because the fx code keeps its
callee-saved registers in the fx register model that only fx adapters bind.


## Round 3 (2026-10-20, agent "chain4"): the intro cinematic, the countdown and the first race frames

Build dir `builds/chain4/` (oracle CLI `builds/chain4/cli`, product driver `builds/chain4/api`; `builds/chain4/run_api.sh` builds and runs it with
`RX_BACKTRACE=1`, log `builds/chain4/api_run.log`; `builds/chain4/probe/probe.c` is a development probe that links the API + the arena and prints the
activity / scenario / board fields each frame; `builds/chain4/tools/dump_script.c` dumps a scenario intro script's events through the fmt readers).

### Where the product stands (end of round 3)

`rx_session_advance` (with `RX_FULL_FRAME=1`, now the frame the product needs for the ride) runs the whole `Engine::Update`: the timing, the input
poll, `MXGame::Update`, the scene tickers (the entities' vf19 jobs), the job pool, and reaches **the event world's intro scenario, its countdown and
the race itself**:

- frames 0..9: the load (9 frames) as before (the UI world, the world transfer, the event world's construction);
- frame ~10: the race activity's Pre state starts the intro scenario (`rx_scenario_start` 00747810), which plays the four intro steps of the national
  event (Intro 3.33 s, StartLine 3.67 s, HeroBike 3.0 s, 30SecGirl 4.0 s; the step-5 "HeroVehicle" script does not exist for a national event and
  the step machine skips it) and ends at step 0 (~15 s);
- the Pre update then starts the countdown (SetState 1: `rx_activity_pre` -> `rx_activity_set_state` -> `rx_activity_enter_state` -> vf31 / vf38);
- the countdown enter sets the start board's countdown (`rand() * 6 + 3` s into board +0x4f4) and the riders' start reactions (`RaceDriver::vf5`);
- the scene tick drives the board's countdown; at 0 the countdown update (`MXActivityRace::vf26` 005651b0) starts the race (InProgress, state 2):
  **the first race frame is ~frame 1050, game time ~17.8 s** (the probe: `act+21c 2`, race state 2, start timer -0.03);
- the race then runs ~56 frames and stops at **frame 1106 (game time 18.49 s) at `00426d00`**, the rider-ragdoll-vs-bike crash solver (10,692 bytes,
  `plan/QUEUE.md` "crash-only, port later"): the local rider is thrown into the crash path ~0.7 s after the start. That is the current stop; it is a
  genuinely large function (the ragdoll contact/impulse kernels of the crash) and was not attempted (see "Blockers").

Getters at the last frame: the race state 2 (racing), 12 riders/slots, the board's start timer, the bike's simulation live (`speed 0.377`), the
terrain 2048 samples, the mud records, the rider pose (69 bones), 18 bike pose nodes.

### The full frame (RX_FULL_FRAME=1) is what the ride needs

The partial frame (the engine head + `MXGame::Update`) does not tick the scene entities, so the start board's countdown never runs and the race never
starts. With the full frame it does. The `Engine::Update` task list's remaining block was the game's audio tick `0067dd10` (the sound manager's
collision map, `VehicleAudioManager_UpdateVehicles`, the audio event queue, the UI localized sounds and the sound engine's pause flag): it is now a
documented **audio boundary** (`rx_game_tick_during_sync` in `race_misc_game.c`: the oracle runs the original, the product skips it; nothing gameplay
reads what it writes). Everything else in `rx_engine_update` now runs (the scene tick's vector assignments 007f9570 / 007fa2b0 switched to the ports,
the crew chief's job 006bd6a0 ported).

The default (`rx_session_advance` without the env var) still runs the partial frame: it runs 600 frames (log `builds/chain4/api_run600_default.log`,
session state countdown at frame 609, game time 10.19 s) but the board's countdown never runs there, so the race never starts (the original's frame is
`Engine::Update`; the partial one is a stop-gap). **switching the default is left to the lead** together with the
input / clock owners, because the frame comparison below is against the verification worlds' 1800-frame runs, not against the product's event (see
"Not done").

### Stops in order, with the fix (all in this round)

Every port is a hand transliteration (address in its comment); "Verify" = the per-call oracle check where a world / boot reaches it.

| # | stop | ported | where | verification |
|---|---|---|---|---|
| 1 | `0086e750` cylinder vs segment list (type 0x8 vs 0x20) | `phys_pair_cylinder_segments` | game/phys_pair_segments.[ch] | adapter oracle/phys_pair_segments.cpp (rx_phys); the pair runs in the bike's placement in every race world |
| 2 | `00747810` Scenario_Start | `rx_scenario_start` | anim_scenario_race_intro.c | rx_anim |
| 3 | `00829460` XMLReader::vf69 (attribute matrices) | `rx_xml_vf_attr_mat4` | build_xml.c | rx_anim (the boot's script parse) |
| 4 | `0055d300` SetScenarioOrigin | `rx_scenario_event_origin` | anim_scenario_race_intro.c | rx_anim |
| 5 | `0074ab50` ScenarioAnimEntity::vf19 (a float vcall, not in the integer table: typed at the site) | `rx_scenario_anim_entity_update` (already ported) | anim_scenario_intro.c call site | rx_anim |
| 6 | `00843350` / `008433b0` the bone name cache find / insert | `rx_bone_cache_find/_insert` | anim_character.c | rx_anim (adapter in oracle/anim_character.cpp) |
| 7 | `0055d7b0` CrowdReaction -> `006744f0` (name -> crowd event) | `rx_scenario_event_crowd`, `rx_world_crowd_event_name` | anim_scenario_race_intro.c | rx_anim |
| 8 | `0055d7e0` / `0055d950` particle / firework triggers -> `006924a0` | `rx_scenario_event_particle/_firework`, `rx_fx_triggered_effect_start` | anim_scenario_race_intro.c | rx_anim |
| 9 | `0063d7e0` MXAnimatedCamera::vf19 (a float vcall: typed at the site) | `rx_animated_camera_vf19` (already ported) | anim_scenario_intro.c call site | rx_anim |
| 10 | `007dd4d0` node set release | `rx_nodeset_release` | anim_node.c | rx_anim |
| 11 | `006893b0` rider map release -> `00705150` map erase | `rx_rider_tree_release`, `rx_stl_map_erase_range` | anim_scenario_rider.c | rx_anim |
| 12 | `008220e0` IMemoryAllocator::vf0 (deleting dtor) | `rx_memory_allocator_dtor` | build_alloc.c | rx_race |
| 13 | `007f1aa0` model instance data release | `rx_model_instance_release` | build_scene_entity.c | rx_build_rider |
| 14 | `00565040` MXActivityRace::vf38 (countdown enter), `00568340` its stub vf38, `005695b0` the vehicles' maps | `rx_race_vf38`, `rx_race_stub_vf38`, `rx_race_stub_reset_vehicle_maps` | race_activity_stub.c | rx_anim |
| 15 | `0048d2f0` the simulation's position set (shared by the moto / ATV tables) | `rx_sim_set_position` | bike_sim.c | rx_bike |
| 16 | `0050fb00` RaceDriver::vf5 (start reaction) -> `0051dcd0` | `rx_driver_vf5`, `rx_driver_start_chance` | race_driver_update.c | rx_race |
| 17 | `00757f90`'s tick `0067dd10` (audio) | product boundary | race_misc_game.c | - (oracle original) |
| 18 | `006bd6a0` CrewChief_Job | `rx_crew_chief_job` | loop_entity.c | rx_loop |
| 19 | `00773110` World_BoxFreeOfDynamics called through `tracked_call` | switched to the owner's port (`world_body.c`) | phys_rider.c | rx_world/rx_phys |
| 20 | `005798f0` the race activity's vf31 (scenario teardown) | `rx_race_stub_vf31` | race_activity_stub.c | rx_anim |
| 21 | `0082af60` ActionContext deleting dtor | `rx_action_context_dtor` | anim_character.c | rx_anim |
| 22 | `007f9570` / `007fa2b0` the vector storage alloc / range move (`rx_vector_assign`'s two `_ex` sites) | `rx_vector_storage_alloc`, `rx_vector_move_range` | race_misc_util.c | rx_race |
| 23 | **`00426d00`** the ragdoll vs bike crash solver (10,692 bytes) | NOT ported (reported) | - | - |

The scenario script's own event handlers that were already ported are reused (`0074a9f0`, `0074aab0`, `0074b080`, `0074b450/480/4f0`, `0074b610`,
`0074be10`, `0074b970`, `007d2e70`, `006922c0`, `0055d870`, `0055d9d0`, `0055dc20`, `00548bf0/c40/f20/f70/fc0/00549050`, `005798f0`'s callees
0084ea00 / 0055e2b0 / 0074bd50 / 0055d240, `0055e100`, `006b6a30`, `0078fd60` -> `rx_format_buffer`, `007de980/9e0`, `Node_EvaluateWorld`).

Corrections to existing ports / boundaries in this round:

1. **`rx_activity_make_locator` (0055dac0)** took the *record* as its first C argument, but the registry dispatch of the scenario script's handler
   table passes ECX (the activity) first: the signature is now `(act, rec)` (act unused), as the original reads the record from the stack
   (`[ebp+8]`). Its adapter passes `m.r[1], m.arg(0)`. Without this the product dispatched the activity as the event record.
2. **`UIProperty "WaitForUI"`** (`build_ui_props.c`, the product's property store) returned 1 while `g+0x18 < 7`, which is true both during the load
   *and* in the running game: the race activity's Pre update therefore never started the countdown. It is now 1 only while the game is in one of the
   load states (`g+0x18 != 2`, 2 = playing): the front end's loading page is up during the load and gone after the world transfer (the original's
   pages set the property; nothing else reads it).
3. The scenario script's handlers `0055dac0` / `0055dff0` / `0055de70` were already ported (as `rx_activity_make_locator` / `_show_object` /
   `_place_at_locator`); they are used through the table dispatch, not re-ported. `0055d610` (SoundEffect) is an audio boundary (the sound name
   resolution ends in the audio command queue; product nothing, oracle original, `#ifndef RX_ORACLE`).
4. `rx_character_sample_keyframes` is called at the original's ESP from the crew chief's job (the job record's dt in xmm1).

### What the intro scripts need (from the asset dump)

`NAT_CO_AF_Intro` (10 events), `_StartLine` (8), `_HeroBike` (6), `_30SecGirl` (10), the `SCMG_*` FX / audio sequences: the event types 0, 1, 5, 6, 7,
8, 9, 10, 11, 18, 19, 20, 21, 22 (the handlers of 5, 7, 8, 9, 10, 11, 16, 18, 19, 20, 21, 22 are now all available; 12 / 13 / 23 are not reached by a
national event). `NAT_CO_AF_HeroVehicle` does not exist (a national event's class step is 5, the script load fails and the step machine keeps going).

### Verification

- `core/build.sh` clean; the oracle CLI (`builds/chain4/cli/reflex_boot`) builds with every new adapter registered (no duplicate addresses; the
  two collisions found — `005a4150` world_misc.c, `0055dac0` race_activity.c — were resolved by reusing the existing ports).
- The baton boot with `REFLEX_PORT=rx_anim` in Verify mode (`loading_accept.sh`, `builds/chain4/la/`): the boot starts the event (ref / port /
  Verify all succeed), 146 ports called, the Verify boot's world image identical to the original's, the race compare IDENTICAL (12/12, state hash
  equal), and these mismatching ports: `rx_Scenario_Start` 1 call / 1 mismatch (2 words at [1821f674] + [1821f678] - a STACK address, and the
  original's first word equals the core's second: most likely the frame image, the port keeps its locals in C; needs a look),
  `rx_IntroScenario_Update` 007473f0 64/63 (heap floats at 0x10a4efc0.. - a chain round 2 port, first boot Verify; either the handlers it now
  dispatches natively or pre-existing), `rx_SceneManager_AddEntity` 007d2f70 2879/81 (the documented baseline).
- **Attribution caveat**: the oracle CLI also compiles the older *lifted* ports (native/game/ports), which register several of the same
  addresses the core does (e.g. `00749d00 RaceIntroScenario__vf0` in group ld5_mid, 0074eaf0, 00843350 ...). In the boot's port table the
  last registration wins for the *name*, so for those addresses the per-call count shown belongs to the lifted adapter, not to this round's
  adapter: only 00843350 (14,904 calls / 0 mismatches), 008433b0 (69 / 0) and 00829460 (6 / 0) are attributable as this round's. A Verify run
  that gives every core adapter its own address needs a CLI without the lifted port files (the lead's call; the product build has only the core
  ports). The scenario ports' behaviour in the product is exercised by the ride itself (the intro plays to its end).
- `builds/chain4/tools/dump_script` validated the script parser against the real assets (all the intro scripts' events listed above).
- **Reference worlds (`verify.sh <cli> quick`, Concord, 1200 frames) — READ THIS**: rx_phys 130 ports / 0 mismatches / **On identical** and rx_bike
  20 / 0 / **On identical** are unchanged. rx_anim 44 ports / 4 with mismatches, rx_race 48 / 2, rx_loop 6 / 6 — and the On-mode whole-memory hash
  DIFFERS for those three groups (the *state* hash is equal: 1f6611fa80b80831 in both). The same run with the pre-task CLI (builds/chain3/cli,
  03:06) is 0 mismatches / On identical for rx_loop, so the difference is introduced by this round's ports. The differing words are in the audio
  command blocks (0xd67dc0 / 0xd6f640 ...: the queue of world_node_audio.c) with ulp-level float differences, i.e. a *pose / parameter* difference
  carried into the queued audio commands; in the rx_loop group the only new port is the crew chief's job 006bd6a0 (the original runs that job on a
  job-pool *worker thread*, at the submit point of the next frame; the port runs it in the caller's thread at the submit - the sampling time and
  the x87/thread context differ). rx_anim's mismatching ports are 007473f0 (rx_IntroScenario_Update, 775/780 in the worlds too) and 0074cf70
  (rx_ScenarioScript_Run: 17 of 3040 calls "native threw: execution outside mapped code 95e90000" - a corrupted handler-table target in the race
  worlds' sequences, TO FIX) - the scenario script path is also entered by the race worlds' sequences, not only by the intro. **Open for the
  loop/anim/race owners: (a) the crew chief job's thread context (or an accepted known difference for the audio queue), (b) the script run's 17
  exceptions in the race worlds, (c) rx_Scenario_Start's 2 stack words.** The product itself is not affected by (a)/(b) in the frames it runs (its
  stop is the ragdoll), but the acceptance gates must be clean before the full-frame default is switched.

### Blockers / not done

1. **`00426d00` (the ragdoll-vs-bike crash contact solver, 10,692 bytes) is the ride's blocker.** The product reaches racing and then the local rider
   enters the crash path. Porting it is a crash-physics unit of its own (its callees 004225d0 / 004241f0 / 004296d0 are also unported; the QUEUE has
   carried it as "crash-only, port later"). It is not a boundary: the crash state feeds the rider's ragdoll, the camera and the race rules.
2. **The frame comparison against the oracle was not made for the product's event.** The product's event starts through the front end and the intro,
   while the verification worlds (`verify.sh`) start a race directly: their frame counts and inputs differ, so a frame-for-frame comparison needs
   either (a) the oracle booting the same event and running it with the host clock / input the product uses (the baton boot + `REFLEX_DRIVE` is the
   closest; its canonical world is a construction snapshot, not a frame trace), or (b) a product-side memory image / state hash (the arena is
   readable in-process, but the API exposes no image). The next agent should use `loading_accept.sh <cli> rx_anim 1 <frames> 1` for the world
   compare of the *construction* (that part is identical) and build a state-hash export for the ride.
3. `RX_FULL_FRAME=1` is not the default: see above; the frame is ready for the ride (the audio tick is the only boundary in it), but the lead should
   make the switch with the frame-comparison result in hand.
4. The scenario intro's UI / audio calls remain the documented boundaries (`00757660` viewports, `0059b860` GotoHub, the sound handler 0055d610).
5. Not reached / not ported (reviewed, no world reaches them yet): the FMX / Waypoint intro variants' handlers (12 CrewChiefAnimation, 13
   CrewChiefPosition, 23 PlaceVehicle), `0074fad0` (the race finish / results scenario vf), the career series steps (the series manager is absent).

## Round 4 (2026-10-20, agent "framecmp"): the product's race frames against the original's, frame by frame

Tool `builds/framecmp/` (README there): the product's session is dumped per frame **through the existing getters** (`rx_session_race`,
`_bike`, `_vehicle_info`, `_bike_pose`, `_rider_pose`, `_standings`, `_camera`) and the ORACLE's world test dumps the same guest fields
straight from the original objects (`boot_main.cpp`'s `REFLEX_FRAME_TRACE`, added this round, env-gated; pre-change file
`builds/framecmp/backup/boot_main.cpp.orig`). The oracle is the same original event through the baton boot
(`REFLEX_EVENT_SPEC="National,10,NAT_Concord,50,0,2,Laps,3,0" REFLEX_RIDERS=12 REFLEX_AI=95`, no ports) and its world test; alignment is by
the raw activity state (`A+0x21c`): the Countdown frames (state 1) and the InProgress frame (state 2), not absolute frame numbers (the
oracle world is captured ~56 frames into the intro).

The compare uses the same frame clock as the oracle (`rx_session_advance_dt(rx_session_step_dt)` = the world test's 166667 x 100 ns) and
the same input (W from the gate drop; `--hold-w` holds it from frame 1 for the input check). Both rand streams are aligned for the Pre
frames (`FRAMECMP_SEED` / `REFLEX_SEED`, see below).

**What matches (concrete counts, live product tree 09:2x against `worlds/pc1`, 182 frames = 181 Countdown + the gate-drop frame):**
- race record (`A+0x21c` state, laps, place, riders, lap count, lap / last / best / race time, start-board timer, lap progress, reset
  timer): **2366 words, 0 differing**;
- game time advances **3.016621 s in both** over the 182 frames; the engine dt word `0x3c888897` is identical every frame;
- with W held from frame 1, the vehicle's 8-word input block (`S+0x1c`) is **1456 / 1456 words identical, 0 differences** (set_input ->
  the game's poll -> key maps -> the simulation input, in a product race frame);
- the camera is not comparable: `rx_session_camera` returns rc 2 in the product (its render camera is not set up).

**The first difference, and its cause (both before any frame logic):** the local rider starts at a **different gate**: the activity's
grid slot is **7 in the product, 5 in the oracle** (slot record at `A+0x340 + 0x30*i`, `[D+0x144]`; first word 615.68 vs 605.90), and
the bike's simulation position (`S+0xf0`, word 0) is `0x4419eb68` (615.678223) vs `0x441779bf` (605.902283), **10.1 units** apart, one
gate, constant through the whole intro and countdown. This is not a rand artifact (setup seeds 0 / unseeded give the same product slot;
the oracle's world image is byte-identical with and without the setup seed): it is a real difference in the event construction /
placement. Consequently the first per-frame difference is **countdown+0, `b0[0]` word 0 (`S+0xf0`)**, and every pose record differs from
there (bike body orientations/positions, 150,693 / 201,110 rider-bone words, 39,312 / 52,598 bike-model-node words, render bodies,
velocity, rpm/speed — the engine at the gate: product 1500 rpm idle vs oracle 157.07) and the single InProgress frame (race+0: 1136 /
1639 words, all downstream of the position).

**Rand streams:** both generators are seeded with `GetTickCount()` (0 in both runs), but the paths consume different numbers of
`rand()` calls: unseeded, the CRT LCG state at the countdown entry is at **index 10 (product) vs 63 (oracle)** of the sequence from seed
0, so the countdown's `rand()*6+3` differs (4.08 s / 245 frames vs 6.13 s / 368 frames). With the common Pre-frame seed both countdowns
are **181 frames** (the countdown code is faithful). Within that frame the product then ends at LCG **index 2 vs 14**: the original
draws **12 more rand() calls** (12 = the rider count; the drivers' start reactions are the likely caller) than the product — a
concrete, separate difference for the race owners.

**Correction to round 3:** the product does not run ~56 racing frames. The InProgress state is reached on the **last good frame**
(session frame 1044 in the seeded run / 1108 unseeded) and the next frame is the `00426d00` stop, so exactly one InProgress frame exists
(the round-3 "~frame 1050, ~56 frames" came from the probe's frame offset and an older tree). `builds/framecmp/run_cmp.sh` is ready to
re-run as soon as `00426d00` / the gate placement land; the oracle trace already covers 331 InProgress frames.

Product-build caveat of the run: the comparison used the live tree at 09:2x; at 08:35 the tree stopped at frame 20 in
`rx_character_anim_set_animation`'s `rx_call_original_ex(0x00843350)` (fixed by chainfix meanwhile).

### clipfix (2026-10-20, agent "clipfix"): ClipRecorder_Init wired into the product

`ClipRecorder_Init 006aa660` had no product caller (only the oracle adapter), so the four rider-clip stunt listeners were registered only when the
unported init thread ran. Its **only** original call site is `0075ed60` at **0075edb9** (a scan of MXReflex.exe's `.text` for `call rel32` to
006aa660 finds exactly this one): after `FUN_00540610` (0075edaf) and `FUN_006b68e0` (0075edb4, the AnimationSets bml load and animation-set
build), before the first database load (0075edbe). It is **not** in the UI-world tail: the product's `rx_ui_world_create` starts at FUN_00756060,
i.e. the deliberately chosen start of the tail, so the clip init belongs to the earlier init-thread step the product mirrors in `api/rx_session.c`
(`build_package_loaders`).

- The port was already there and faithful (build_rider_clip.c; re-read against the disassembly in this task: state 0, ClipRecorder_AllocBuffer
  006abc40, activity 0, the three timers, the 0x35d0-stride clip-slot ids [0xd3275c..0xd5ad1c), and the four `FUN_006ac430` registrations with
  ECX = event name / stack = callback). No code change was needed in it.
- Product wiring: `build_package_loaders` calls `rx_clip_init()` immediately after `rx_vehicle_list_init_loader` (00540610) and before the
  Animations / RiderPhysics database loads - the original's position relative to every step this stand-in performs. `FUN_006b68e0` sits between
  the two in the original; it is an AnimationSets asset load the stand-in does not run, and ClipRecorder_Init does not depend on it.
- Oracle unchanged: the adapter `oracle/build_rider.cpp` (group `rx_build_rider`) was already registered and the oracle boot still runs the
  original under it. `api/` is not compiled into the oracle CLI, so the product call cannot double-call there. (The lifted port
  `native/game/ports/ld_entities_68.cpp` also registers 006aa660, group `ld_entities`; that is the usual lifted-port overlap for integration,
  like 00547f30.)

Verification (build dir `builds/clipfix/`, backups `builds/clipfix/backup/`):

- Product: `rx_api_test` (builds/clipfix/api, `RX_FULL_FRAME=1`, 10 frames, Concord) runs as before and the four listeners are in the stunt
  channel [0x00a94304]'s lists: events 0x0095ba28 / 0x00960790 / 0x00960780 / 0x00942784 each `size 1`, handler objects with vtable 0x0095eb50
  and callbacks 006ab290 / 006ab380 / 006ab410 / 006ab4a0. Proven with a temporary `RX_CLIP_LOG` diagnostic (marked TEST-ONLY, removed again;
  log `builds/clipfix/logs/api_edit_run.log`); the rest of the run is identical to the pre-change run except heap addresses (the ring allocation
  shifts them). The 10 frames only reach the countdown, so this proves registration, not a stunt dispatch; the handlers themselves are
  handlers2's verified ports (probe 150/150 each).
- Oracle per call: baton-boot Verify (`builds/chain/boot_port.sh builds/clipfix/cli/reflex_boot rx_build_rider <out> v`):
  `rx_ClipRecorderInit 006aa660 calls 1 verified 1 mismatches 0`. The boot's only mismatches are the documented 0053e920 / 0053eab0 dead-stack
  baseline.
- No regression: `verify.sh builds/clipfix/cli/reflex_boot rx_world quick` -> Concord 132 ports called, 0 mismatches, On identical;
  `rx_race quick` -> 48 ports, only the documented pre-existing `rx_ActivityManager_Update 0055f700` (776/1200, same count and one-page On
  difference as `builds/handlers2/verify_other_quick.log`); `core/build.sh` clean (`builds/clipfix/logs/core_build.log`).
- Session recreate: `rx_api_recreate_test` (built by hand with the generated `rx_fnsig.c`; the tool's .sh does not add it - pre-existing script gap)
  3 iterations in one process and a fresh process give the same arena hashes at create / load / final (`5e755e8c1915125e` / `0afda32db0b5d5a8` /
  `879c523d23dc43af`), so registering the listeners again after the arena reset is consistent.
- Remaining: `ClipRecorder_Destroy 006aa6f0` (the four unregisters through 006ac4f0) is not ported or called (session teardown; the product resets
  the arena).

### chain4 (2026-10-20, agent "chain4"): the On-mode differences this round introduced - root cause and fix

The On-mode differences that round 3 introduced (rx_anim / rx_race / rx_loop DIFFERENT where the pre-task CLI was identical; audio command
blocks 0xd67dc0 / 0xd6f640 differing by ulps) had **one root cause**: the typed virtual call to `0063d7e0 MXAnimatedCamera::vf19`
(`rx_animated_camera_vf19`, loop_cam_player.c).

- The original call site in `007473f0` (the intro update) is `push ecx; fstp dword ptr [esp]; call [vt+0x4c]`: the float dt lives in the
  **argument slot**. The port reads its dt from that slot (`const rx_addr arg = sp0 + 4u; ... SF32(arg, 0, 0.0f)` / `F32(arg, 0)`), exactly
  as the original's `[ebp+8]`.
- `0074ab50 ScenarioAnimEntity::vf19` had been switched from `rx_call_original` to a typed C call (`void (*)(rx_addr, float)`) that only
  lowered `rx_sp` by 8 and passed the value in an SSE register: the slot itself was **never written**, so the port ran with the stale
  dwords of the previous frame. The camera pose then differed by a few ulps every frame, which propagated to the animated camera / cast
  matrices and to the audio command blocks (0xd67dc0 = IconRenderEvent's transform, 0xd6f640 = the engine Camera) - and, in the oracle,
  made the core camera run where the original used to run.
- Fix (anim_scenario_intro.c): write the argument (and the return-address slot) as the original pushes do before the typed call -
  `SU32(s - 4, 0, dtq); SU32(s - 8, 0, 0); rx_set_sp(s - 8); f(crowd, bits_f32(dtq)); rx_set_sp(s);` - and the same for the dt-0 call of
  `0074ab50`. No guard, no #ifdef: the call is now the original's call, with the original's stack.

Two further real bugs found while bisecting the remaining differences (both in this round's new ports):

- `00749df0 RaceIntroScenario::vf1` (`rx_race_intro_vf1`) read the chest node's index as a **dword** (`U32(rec, 0xb)`) where the original
  does `movzx eax, byte ptr [esi+0xb]`. With the following bytes nonzero the node offset `index * 0x40 + [[rec]+0x1c]` became
  0xd9854130 -> `unmapped read d9854130+4` and the rx_anim On run stopped at frame ~357. Fixed to `U8(rec, 0xb)`.
- `00749910` (`rx_race_intro_place_chiefs`) now also writes the original's frame locals (+0x10 camtab, advanced by 4 per driver,
  +0x14 human, +0x18 driver count, +0x1c the cast), which the original leaves in its frame (dead-stack reads) - same values, same slots.

Attributable per-call Verify (an oracle CLI whose `native/game/ports` excludes the five lifted files that also register these addresses -
`ai_gen_55.cpp anim_740.cpp ld5_mid_70.cpp ld_game_74.cpp rest_5.cpp` - so every count below belongs to the core port; build dir
`builds/chain4/snapcli`, baton boot, `REFLEX_PORT=rx_anim REFLEX_PORT_VERIFY=1`):

| port | calls | mismatches |
|---|---|---|
| rx_Scenario_Start 00747810 | 1 | 1 (2 words of the frame's own stack) |
| rx_IntroScenario_Update 007473f0 | 64 (boot) / 780 (race) | 0 (boot) / 3 (race) |
| rx_ScenarioScript_Run 0074cf70 | 252 (boot) / 3040 (race) | 0 (+2 unverifiable) / 17 (`native threw: execution outside mapped code 95e90000`) |
| rx_ScenarioAnimEntity_Job 0074ab10 | 4025 | 0 |
| rx_BoneCache_Find 00843350 | 14904 | 0 |
| rx_BoneCache_Insert 008433b0 | 69 | 0 |
| rx_Scenario_PlaceChiefs 00749910 / vf1 00749df0 / card flags 007496d0 | 0 in the boot | (they run in the race worlds: see below) |

The 2-word `rx_Scenario_Start` difference is the same 2 dwords in both runs of the Verify pair on the frame's own stack area (No
heap/state difference), i.e. the dead-stack class; the port's writes to the scenario object match.

Measured with the same CLI (`builds/chain4/snapcli`), one `verify.sh` at a time (see known-differences 13 for the wall-clock caveat):

| group | five worlds |
|---|---|
| rx_phys | 166/164/146/164/164 ports, **0 mismatches, On identical** in all five |
| rx_bike | 30/30/18/22/26 ports, **0 mismatches, On identical** in all five |
| rx_fx | 114/108/110/112/110 ports, **0 mismatches, On identical** in all five |
| rx_loop | Concord **identical**; FreeRide identical; ai11 / SX one-page difference with an *identical* state hash (the documented handlers2 class); TRX state-equal, memory-hash difference |
| rx_race | FreeRide identical; Concord / ai11 / SX / TRX **state hash differs** (see below) |
| rx_anim | FreeRide identical; Concord / ai11 / SX stop on `execution outside mapped code 95e90000`; TRX state-equal, memory-hash difference |

Remaining, reported rather than papered over:

- `0055f700 ActivityManager_Update` (776/1200) and `0084ed20 Engine::Update` (775/1200) Verify mismatches, and the one-page On difference
  of the race worlds: **pre-existing**, documented by cleanup2 / handlers2 (`builds/handlers2/verify_other_quick.log`, QUEUE "chain round 3").
  They are not caused by this round's ports.
- `rx_anim` On stops on the race worlds with `execution outside mapped code 95e90000` (a dispatch slot), the same value `rx_ScenarioScript_Run`
  reports as its 17/3040 pre-existing Verify mismatch. Open: the activity object's vtable slot 8 (+0x20) reads 0x95e90000 where the image has
  0x56a020 (`00558e82`, rx_activity_pre's vf8), i.e. a native wrote a dispatch value into the activity/its table earlier in the frame.
- `007473f0` (3/780) and `00749910` write the intro cast matrix with `y = Y - 0.24`, `z = Y` where the original writes `y = Y`, `z = Z`
  (the same 2 dwords at 0x1821f674 / 0x1821f678). Each port alone reproduces it; each of their callees alone is clean; one shared step
  still differs. Open.
- `rx_race` changes the *state hash* on four worlds while `rx_loop` does not. The `rx_race` natives make the frame faster, and the game's dt
  is the host clock (known-differences 13), so this measurement must be repeated with a pinned clock before it is called a port difference.
  **Repeated (clockpin, 2026-10-20) with `REFLEX_FIXED_CLOCK=1` on the same tree: all five `rx_race` On runs are identical** (state and
  memory hashes equal), and the unpinned run of the same CLI is identical too; the remaining `0055f700` Verify residue is the two
  `RiderCharacter` words, not a state difference. See `native/game/notes/clock-pin.md`.
- `phys_rider.c` / `phys_crash_solver.c`: crashport moved the `00425640 phys_rider_crash_step` body into `phys_crash_solver.c` while this
  round was running; the copy of `phys_rider.c` restored here no longer defines it (it only calls it, as phys_rider.h declares), so the
  tree links (`core/build.sh` clean) - crashport should re-apply any other edit of that file they had in flight.

## Round 6 (fxconstruct, 2026-10-20): the FX scene construction unit; the UI-world scene reaches the oracle's 68 tickers

The gridfix round localized the start-zone difference to the FX scene classes the factory left empty (`build_scene.c` `outside_core`). This round
built them (notes/fx.md "The particle asset and the scene FX entities"; `core/game/build_particle.c`, `scene_fx_entities.c`,
`fx_worldparticles.c`, `scene_viewports.c`; loader registration in `build_load.c`; the class registration call in `build_scene.c`).

Result, product UI world at the state-6 grid pass (`builds/fxconstruct/probe_build/probe`, RX_FULL_FRAME=1, the probe prints the scene's
+0x2c10 ticker list): **68 tickers, the same classes and order as the oracle** — 4 StatusBoardEntity (00939de4), 19 ScrollingTextureEntity
(00939ba4), 1 OverlayColorEntity (0093971c), 15 MXTrigger (009376cc), **24 PlaceableParticleEffect (0095e32c)**, 9 ScrollingTextureEntity,
**1 ParticleEntity (0095dfbc)**, **1 AudioVolumeMixEntity (0095d3d4)**, 1 HFTerrainEntity (00971d6c), 1 ResetMarkerFlagEntity (00944054).
The 24 effects bind into the loaded 33-record `UI_Compound_Particles` particle asset (e.g. `Road_Flare` = index 15) and tick: their emitters
draw the game LCG (006944c0 / 00695780).

**Not yet accepted: the game LCG count at the grid pass.** The oracle's is 2239/2240 (zone draw at 2240, gridfix). Measured with the new
ports:

| run | players (scene +0x19d0) | dt | count at the state-6 grid pass |
|---|---|---|---|
| product, before this round | 0 (no viewport list) | 1/30 | 1829 (no FX at all) |
| product, FX only | 0 | 1/30 | 1949 (24 rate draws/tick, n = total/0 = 0 on arm64: no particles) |
| product, FX + one viewport (00757660's list) | 1 | 1/60 | 4652 |
| oracle (watch trace, gridfix) | 1 | 1/60 | 2239 |
| oracle (fxconstruct GRIDSTATE boot, 450 frames) | 1 | 1/60 | 2204 at the state-6 pass, **pause 0** |

The per-tick behaviour matches the oracle's block: the oracle's 410 units between the noise init (1829) and the grid are 34 emitter rate draws
(0069587b), 22 particle builds (00694509 + 0069456e, 17 count units each) and one rider init (0061a32a). One full UI-scene tick with
players = 1 at 1/60 produces the same shape from our port (24 rate draws + the 12 Road_Flare particles = 228 units); the engine ticks a
scene twice per frame (`007d21d0` at the frame start ticks the previous ticker list, `007d23a0` rebuilds it and ticks again), so the product's
five pre-grid frames draw ~10-11 ticks (≈2823 units) while the oracle drew ≈1.4 ticks before its grid pass. The difference is therefore the
**number of UI-scene ticks before the grid** — the product's session keeps the UI scene active through its synchronous UI-world / load frames
(measured with GRIDSTATE, cli3: the oracle's UI-world grid pass at startup has the engine timing's pause word ([engine + 0x10] + 0xc) = **1**
and the UI world's +0x20014 = 0, so the front end's emission gate is closed and the watch trace shows no LCG draws from startup to the noise
init / the load; at the state-6 grid pass it is **pause 0** with count **2204** in that boot. The product's timing pause is 0 from the start
(it does not reproduce the front end's paused frames), and its ResumeFromUI runs at the load request, before the frame loop, so its UI scene
emits on 5 pre-grid frames; the original's resume is the UI's loading-page completion at the state-5 transition, ~2 ticks before the grid.
The product has the front-end pause now (build_ui_world.c calls rx_mxworld_set_paused(w, 1) as MXGame::vf10(1) does; ResumeFromUI clears it),
but the resume point still needs to move to the state-5 transition for the count to match), not the FX construction. The renderer-set particle rate global is the second suspect: PlatformParticleManager2D::Render
(00698890) writes [0x9c46bc] = min(1, (target/players/dt)^2), which ParticleManager2D_Update copies to [0x9c46b8]; the product has no renderer
so it stays 1.0 (the oracle's value at the load is still to be read).

Consequence: the product's start zone at the grid is still drawn from a later LCG state (probe: zone 8 unseeded with players=1; the oracle 5
with `REFLEX_SEED_SETUP=0`). The required 2239/2240 needs the pre-grid tick/pause flow of the front end / load (the loop/engine unit's
`Engine::Update` gating and the session's UI-world active flags), or an agreed reduction of the oracle's front-end frames. Reported to the lead.

Framecmp (`builds/framecmp/run_cmp.sh --frames 1400`) with these ports: the product trace ran 115 frames then spent >10 min in
`rx_world_activities_start` -> `rx_activity_vf2` -> `rx_audio_logic_vf29` -> `cue_present` -> `rx_world_string_map_find` (100% CPU, no
progress; machine load average ~120 from another agent's verify runs at the time). Not attributed to this unit (the cue map is the
activities' audio port) but not re-run to completion; the product driver `rx_api_test` (10 frames, RX_FULL_FRAME=1) builds and runs.

### camsnap (2026-10-20, agent "camsnap"): the 1252-frame camera stop, `00649df0`, and the 5000-frame product run

The presencefix request's next stop (`plan/QUEUE.md` "presencefix": 1252 race frames, game time 21.071) is the camera cut-in snap
**00649df0**: Player_UpdateCamera, when the active camera entity switches from a blendable one (class 0x100d, the start-gate camera) to
an MXThirdPersonCamera (class 0x1003), seeds the new camera's transition (+0x3f4 active, +0x3c0..+0x3cc time / duration / elapsed /
fraction, +0x3d0..+0x3d8 the old position, +0x3e0..+0x3e8 the old look point, +0x3f0 the old fov) from the old entity's matrix
(vtable slot 0x38, `Entity::vf14` 0088fa20 = entity + 0x30, called twice) and its +0xe0 fov. Camera-only local code; it was not a
boundary (no network / UI / file access).

- Port: new `core/game/loop_cam_snap.[ch]` (`rx_cam_snap`, hand transliteration from the 219-byte disassembly, original frame
  `and esp,-16; sub esp,0x10`, the vcalls at the original return addresses 00649e0c / 00649e35, addss operand order kept, the fov
  through an x87 copy); adapter `core/oracle/loop_cam_snap.cpp` (group `rx_loop`, OUT_FPU); the call site in `loop_player.c` (was
  `pl_call` at line 775) is now the core call at the original's stack depth with return address 0x0054c900 (the oracle fallback stays).
- Per-call Verify (baton worlds, peel of the enclosing frame ports: `REFLEX_PORT="rx_loop,-0084ed20,-007674b0,-0054af90,-0054c150"`,
  `builds/camsnap/peel.sh`): **Concord 1/1/0, Concord_ai11 1/1/0, SX 1/1/0, Concord crash-drive (REFLEX_DRIVE=7) 1/1/0**; FreeRide
  and Terracross 0 calls (no 0x100d -> 0x1003 switch). With `REFLEX_PORT_DEAD=1` the Concord call is still 1/1/0 (dead stack exact).
  The peel runs' only other mismatch is the documented RiderAnimation worker dead-stack word. Acceptance: `rx_loop quick` only the
  documented 0084ed20 pair (775/1200) / On identical; `rx_race quick` 48 ports, only 0055f700 / On identical; `rx_world quick` 132
  ports, 0 / On identical.
- Product: `RX_FULL_FRAME=1 rx_api_test ... 5000` on the last runnable tree (the presencefix build's objects + this port; log
  `builds/camsnap/api_pf_full5000.log`) runs **5009 frames to the end of the request**, state 2 (racing), game time 83.534, race time
  65.063 (past the 60 s presence branch), place 6, 12 riders, 69 rider bones, 18 bike pose nodes, camera rc 0 (612.56 118.25 -391.42
  fov 0.943), particles rc 0, mud count 12: the 1252 stop is gone and no new stop appears.
- Live-tree blocker (not this unit): the current tree hangs at load frame 0 in `rx_world_activities_start -> rx_activity_vf2 ->
  rx_audio_logic_vf29 -> cue_present -> rx_world_string_map_find` (`crt_str_less` loops on a bad key; the same path fxconstruct saw in
  its framecmp run above). A link of the current objects with this port removed hangs identically (`builds/camsnap/api_nosnap120.log`).
  The 5000-frame product result above therefore uses the 11:55 presencefix object set; re-run the live-tree build once the audio cue
  map / scene-FX round settles. (A later live build, 14:18, got past the cue map and stopped at `00641f20` MXFirstPersonCamera::vf32,
  cameraschema's in-flight first-person port; `core/build.sh` is clean on the final tree, `builds/camsnap/core_build3.log`.)

## Ride test under input (inputrace, 2026-10-20): the product runs 5409 frames, the start crash still blocks riding

`builds/inputrace/` (README, `ride.c`, `cmp_ride.py`) drives the last runnable product (camsnap/presencefix objects; the live tree
was mid-edit) with `RX_FULL_FRAME=1`, `rx_session_advance_dt(1/60)` and `rx_session_set_input` every frame: 5400 steps / 5409
session frames, state 2, no stop, game time 90.188 s. It does **not ride**: the simulation input block is live for two racing
frames and then zero (throttle 27 / 5400 frames, gear 0, speed <= 5.408, 125 units of path, no lap; the chase camera follows at
6.4..18.7 units). The cause is the start crash from chain round 3: the local rider is thrown on the second InProgress frame (vehicle
`V+0x280` = 0x13), so `L+0x1f0` = 19 and `bike_vehicle_pre_step` (0045d510) takes its clear-inputs branch (the mode-2 pose-override
fallback does not apply: the product's override is 0 / mode 1). The oracle (`REFLEX_DRIVE=7`, same input) is not wrecked, keeps the
block live and reaches 59.7 ft/s / gear 2 in the same window (76.8 ft/s / gear 3 with W held); when the oracle enters a mode-1
override at f=1281 its block also zeroes, confirming the branch. `00426d00` is ported (crashport), so no abort - the crash executes.
Comparison (`cmp_ride.py`, 202 aligned frames): input differs at race+1 word 2 / race+2 word 0; speed and velocity from race+0; race
state / laps / riders identical, place 6 vs 5; the position offset is the known grid slot 7 vs 5. Pacing: `rx_session_pace(1/30)`
gives exactly 2 x 1/60 steps, alpha 0, engine dt word 3c888897 every racing frame, the paced log byte-identical to the plain run and
across processes. Next: the start-gate/rider physics (the ride blocker), then re-run 5400 on the live tree.

## Round 7 (loadfix, 2026-10-20): the post-fxconstruct product stall and the load-frame pause/grid

Two problems after fxconstruct: the product stopped inside the first load frame (`rx_world_activities_start` -> the race activity's
audio logic -> `cue_present` -> `rx_world_string_map_find`), and the state-6 grid pass drew the game LCG far too often (4652 vs the
oracle's 2204/2239). Build dir `builds/loadfix/` (product `api/`, oracle CLI `cli/`, probe `probe_build/`, logs).

### The stall: a missing thread TLS block, exposed by the new FX entities

Reproduced on the tree of the day: `RX_FULL_FRAME=1 rx_api_test <install> 1200` stops with SIGALRM at "load frame 0"; the sampled stack
is `rx_mxgame_update` -> `rx_game_enter_real_world` -> `rx_world_activities_start` -> `rx_race_vf2` -> `rx_activity_vf2` ->
`rx_audio_logic_vf29` -> `rx_race_audio_logic_vf3` -> `cue_present` -> `rx_world_string_map_find` -> `rx_world_string_map_lower_bound`
-> `crt_str_less`. The root cause is **not** in the cue map:

- `fx_particle.c`'s port of Particle_UpdateJob 006925f0 writes the job's own thread-local scratch through `fx_tls_block()`
  (`fx_util.h`: `[fs:[0x2c]] + 8 = job, + 4 = particle index`). The product's thread stand-in (`loop_platform.c`'s
  `loop_wake_worker` = `body(arg)`) never created the Windows loader's TEB/TLS, so `fx_tls_block()` returned 0 and
  `SU32(0, 8, job)` wrote the job record's **stack address into guest address 8**. Found with a hardware watchpoint on guest 8
  (`rx_base + 8`): the writer is `rx_fx_particle_update_job + 252`, the body runs from `rx_jobpool_submit` ->
  `rx_jobpool_worker_run` (the first particle jobs appear only with fxconstruct's 24 PlaceableParticleEffect entities).
- Later in the same frame `rx_audio_logic_vf29`'s product-only "no audio manager" branch computes `set = U32(mgr, 8)` with
  `mgr = [0xd92f14] = 0`, i.e. it reads guest address 8 (the chain-round-2 fallback assumed it is zero). It picked up the stack
  address, `cues = set + 0x94`, and the cue lookup walked a "map" at a stack address (`head = 0`, root = guest `[4]` = 0x62 = the
  PE header's e_lfanew) until x = 0; `[x + 0x2d]` at guest 0 is 0, so `lower_bound` never stops (the original loops forever too on a
  malformed tree).
- Fix (`loop_platform.c`, product branch): the loader's static TLS per emulated thread - fs:[0x2c] (TEB 0x7ffd0000) -> a 64-entry
  pointer array -> the thread's 0x810-byte copy of MXReflex.exe's .tls (TLS directory: 0x9d0000..0x9d0810, index 0x00dcf74c, raw
  bytes all zero). `loop_wake_worker` points fs:[0x2c] at the worker's array for the body and restores the caller's after (the
  emulator's runtime switches TEB + 0x2c per thread the same way). The particle job's scratch then lands in the block, and guest 8
  stays 0 (the audio fallback's own map is used, which is valid).
- The product then ran 9 load frames and reached race frame 617, where it stopped at **`unported game function 00000000`**:
  `rx_fx_triggered_effect_start` (006924a0) called the effect's controller as `rx_vcall(U32(e, 0x90), ...)` - the *vtable address*
  stored at e+0x90 - while the original does `lea ecx, [eax + 0x90]` (the controller object embedded at e+0x90, its vtable at
  [e+0x90]). Latent until fxconstruct built the scene's TriggeredPlaceableParticleEffect entities (0x102f): before, the scenario's
  "Emitter" lookup returned 0 and the other branch ran. Fixed the call site (anim_scenario_race_intro.c) - no guard, the original's
  object. The product now completes **1200 full frames** (9 load frames, state racing, game time 20.188 s; log
  `builds/loadfix/api1200_final.log`).

### The load-frame pause and the grid

The oracle evidence (fxconstruct's GRIDSTATE boot, `builds/fxconstruct/gs/out4.txt`): at the front end `pause 1` (the engine timing
word `[0xa95844]+0x10 + 0xc`) with the UI world's `+0x20014 = 0`; at the state-6 grid pass `pause 0, uipause 0`. So the front end's
pause is the **session pause block's timing word** (MXGame::Update's 006e0c50 path writes `timing+0xc = [0xda6734]`), not
`MXGame::vf10(1)`'s world pause (which would set the UI world's +0x20014 too). The 006e0c50 block's resume branch (`[0xda6734] = 0`
and `rx_mxworld_set_paused(uiworld, 0)`) runs when 006e0c50 goes 1 -> 0, at the load's end.

The product's `006e0c50` is a product boundary (returns 0: no input device table [0xda6cac], no renderer's device byte, and the
window focus flag [0xa9596c] is never set) and its request path (`rx_ui_request_level`, as the original's 0059bdc8) calls
ResumeFromUI(0), which in the original does *nothing* here (uipause is already 0). The product's build_ui_world had used
`rx_mxworld_set_paused(w, 1)` (uipause = 1), so that ResumeFromUI cleared the pause before the first frame and the UI scene emitted on
every load frame. Now:

- `build_ui_world.c` sets the timing word only (`timing+0xc = 1`; the oracle's `pause 1, uipause 0`), so the request path cannot
  clear it.
- `build_activity_game.c` case 5, when the loader is idle (the load's end, before the grid), makes the session-pause transition the
  original's block makes: `[0xda6734] = 0` and `rx_mxworld_set_paused(uiworld, 0)`.

Measured with `builds/loadfix/probe_build/probe` (RX_FULL_FRAME=1, dt 1/60; it prints the UI scene's ticker list, the game LCG and
the Player List's StartZone at the state-6 pass):

| run | pause in states 3..5 | LCG at the state-6 grid pass | local StartZone |
|---|---|---|---|
| product before (13:46 pause + request-path resume) | 0 | 4652 | 7 |
| product, front end never resumed (timing word alone) | 1 | 1829 | 7 |
| **product now** (timing word, cleared at the state-5 end) | 1 -> 0 in the last frame | **2091** | **11** |
| resume at the state-4 end (tried) | 1 -> 0 two frames early | 2557 | - |
| oracle GRIDSTATE (state-6 pass) | 1 -> 0 | 2204 | 5 |
| oracle watch trace (grid draw) | | 2239/2240 | 5 |

The pause window is now the oracle's (no draws while the front end is paused; pause 0 at the state-6 pass; one UI-scene tick lands
before the grid). The remaining 148 units (2239 - 2091) are the **FX emitters' per-tick draw count**, not the window: the oracle's
pre-grid 410 units are exactly two UI-scene ticks of 17 rate draws + 11 particle builds (17 + 11*17 = 204 each, `watch_counter.txt`
counts 2035..2238) plus one rider init draw, while the product's one tick draws 24 rate + 14 builds (262 units). Two of the product's
ticks would be ~524 (the state-4 window measured 728 over two frames) - so no pause window reproduces 2239 until the emitter
per-tick counts match (fxconstruct's open acceptance item; the scene has the same 24 PlaceableParticleEffect entities, but 7 more of
them draw every tick than the oracle's; the ticker interval +0x20 is 0 in both ctors - 006964d0 stores xmm0 = 0 too - so the
difference is in the emitter's active gate, 006944c0 / 0069587b, not in the scene's tick periods).

Framecmp (`cmp_frames.py`, 1400 frames both sides; product trace `builds/framecmp/logs/product_trace.txt`): the product now runs
**357 InProgress frames** (the 00426d00 crash solver runs) and the countdown is 181/181 frames in both, but the grid is still one
gate off: countdown+0 `b0[0]` w0 = `0x442002c1` (640.043) vs the oracle's `0x441779bf` (605.902), i.e. the local StartZone 11 vs 5
(product slot from the LCG count 2091). Everything else follows from that first word.

### Verification of this round

`core/build.sh` clean. Oracle CLI `builds/loadfix/cli/reflex_boot` (all core files, adapters) `verify.sh quick` on Concord:
rx_world 132 ports / 0 mismatches / On identical; rx_fx 108 / 0 / identical; rx_race 48 / only the documented
`rx_ActivityManager_Update` 0055f700 words (775, the two RiderCharacter words) / identical; rx_loop 9 ports / the documented
`Engine_Update` 775 words / On identical (byte-identical to `builds/fxconstruct/logs/verify_loop_quick.log`); rx_anim 44 ports / the
documented `rx_IntroScenario_Update` 007473f0 3-word cast-matrix difference ([1821f674]/[1821f678], chain round 3's open item) and the
documented On memory-hash difference. The four changed sites are listed in QUEUE "### loadfix".

### scenariofix (2026-10-20, agent "scenariofix"): the intro script's dispatch and the crew-chief camera pose

Two deterministic differences clockpin left on rx_anim, both in the scenario intro, both root-caused and now clean.

- **`0074cf70` `95e90000` (call 2101, every race world; it also ended the rx_anim On runs).** The scenario handler type 9
  (FireworkEffectTrigger) calls `006924a0 rx_fx_triggered_effect_start`; that function's "creator" vcall used the *field* `U32(e, 0x90)`
  as the object. `e+0x90` is the object's secondary vtable pointer (`TriggeredPlaceableParticleEffect::vftable_o90` 0x0095e280, set by
  006923c0), so the callee took `[0x95e280]` (0x0069b200, the secondary dtor thunk) as a vtable and called its `+0x14` — the code bytes
  at 0x0069b214 = 0x95e90000. The original is `lea ecx,[eax+0x90]`; the fix (`rx_vcall(e + 0x90, ...)`) is in the tree (QUEUE "### loadfix"
  item 2). Verified here: `0074cf70` **3040/3040/0** (Concord 1800) and **250/0 (+2 unverifiable)** in the baton boot; it is no longer a
  Verify mismatch *or* an On stop.
- **`007473f0` 3/780 two-dword cast matrix (`y = Y - 0.24`, `z = Y`).** The write is the crew-chief camera pose of `00749910`: the
  original's store `movss [esp+0x68], xmm1` at 00749ab6 happens **after** `push ecx` at 00749aaf, so it writes the frame's +0x64 — the
  pose matrix's **y** — and the camera takes `(veh.x, veh.y + 0.24, veh.z)`. `rx_race_intro_place_chiefs` wrote +0x68 (z) instead, so
  its pose was `(veh.x, veh.y, veh.y + 0.24)`; the oracle's two words are exactly the port's z (= the original's y) and the port's y
  (= the original's y - 0.24). One-line fix (`SF32(E, 0x64, phys_add(F32(0, 0x009cb14c), y))`, disassembly in the comment,
  `game/anim_scenario_race_intro.c`): `007473f0` **780/780/0** (Concord) and **64/64/0** (baton boot).

Acceptance with the fixed CLI (`builds/scenariofix/cli`, `verify.sh` one group at a time, `REFLEX_FIXED_CLOCK=1` and unpinned): rx_anim
**0 mismatches and On identical in all five worlds** (54/52/32/54/54 ports; the Terracross mud memory-hash difference is gone as well —
`0073899e...`, 29376 pages), pinned and unpinned. rx_race: only the documented `0055f700` two-word residue, On identical in all five
(pinned and unpinned). rx_loop: only the documented `0084ed20` aliases, On identical (ai11 / SX one ConVar page 29080/26334,
Terracross same pages with the mud hash — known-differences 1 / 2), pinned; the unpinned quick world is identical. rx_world pinned:
170 / 168 / 134 / 146 / 178 ports called, **0 mismatches, On identical in all five** (`world_concord.log`, `world_sx.log`,
`world_trx.log`; the first three from the full matrix). The remaining rx_race/rx_loop residue is still only the documented
`0055f700`/`0084ed20` RiderCharacter pair (object localized: RiderCharacter 0x0095881c, +0x70 playing and the +0x90 base's +0x10 bit
0x10000; the original starts the rider's animation where the core's run does not — QUEUE "### scenariofix" 3, open). `core/build.sh`
clean.

## Ride test under input: the start crash is the wreck checker's ABI (startdiag, 2026-10-20)

`builds/startdiag/` (README with the table and the logs) answers inputrace's ride blocker. The local rider is **not** in or
against the start gate and **not** hit by an AI bike: on the first race frame the bike is still at its grid position, the
DropGate is 8.9 units ahead with collision on and has swung 7.5 cm (0.075 rad), and the gate's state / angle / part
collision flag / 4x4 matrix are **word-identical** in the product and the oracle (`3ca931f0 3f7f47b0 bd93836a ...`).

The rider is thrown by its own `WreckChecker_Update` (006373e0). The checker scales four wreck thresholds by the two wheel
grip factors (`P+0x1c` / `P+0x24` by the front, `P+0x20` / `P+0x2c` by the rear), which it reads through the vehicle vcall
`00466bd0` — an **x87 st(0)** return (`rx_f80 rx_world_raven_grip_factor` in `core/game/world_veh_raven.c`, verified). The
port's `vehicle_rate` helper called it through a `float (*)(rx_addr, uint32_t)` pointer, so on arm64 the 24-byte struct
return came back through memory and the checker read whatever was in `s0`: a private copy of the checker that prints the
word shows -5.18e-22, 4.35e17, 1.1e29, NaN; after the fix, 0.9991..0.99998 (the real grip). With the thresholds zeroed /
NaN, the type selection's `F32(P,0x1c) == 0` branch picks **0x13 (19)** for most riders and the zeroed roll threshold 0x14
(20) for the rest, and the weight (`speed * 1e-5`) is below 1e-5, so `rider_start_wreck` + `rider_fall` run immediately for
**all 12 riders in the same frame**. `rider_fall` writes `V+0x280` (= `L+0x1f0`) and `S+0x2374`, and `0045d510` then clears
the input block every frame. The `e0` vcall (`00466b70`) had the sibling error: the core prototype takes the original's
pushed argument slot as a third argument, the old 2-argument call left it uninitialised (a wild 4-byte store).

Fix (`core/game/world_wreck_checker.c`, `vehicle_rate`; backup `builds/startdiag/backup/`): call the owner's functions
with their own prototypes — `rx_world_raven_wheel_value(veh, arg, sp - 4)` and `rx_f80_to_f32(rx_world_raven_grip_factor(
veh, arg), 0)` (the caller's `fstp dword`). Under `-DRX_ORACLE` the compiled object is byte-identical to the pre-fix one
(`__text` 9580 bytes; the function's disassembly is identical), so the oracle path cannot change.

Product (preserved `builds/camsnap/api_pf/eobj` + the fixed checker, `RX_FULL_FRAME=1`, W held): 1409 frames from 1108,
**no wreck**, speed 0.118 -> 7.1 (race 60) -> 34.2 (120) -> 47.7 / gear 1 (150) -> 58.5 / gear 2 (179) -> 71.9 / gear 3
(250), 212 units; the oracle with W held reaches 50.8 / gear 1, 61.8 / gear 2, 67.9 / gear 3 in the same window. Before the
fix the same run gives max 5.29, gear 0, 20.1 units and `V+0x280` = 19 from race+1. With 4 riders the fixed product runs
**504 racing frames** (max 87.4, gear 4, 500.7 units) and stops at unported `00550b70` (player reset, terrain material 7);
the 12-rider runs stop earlier at unported `00529190` (`rx_behavior_offtrack_update`, AI-only) because the AI now stay
upright too and, with the AIData tables still zero, ride off the track — that is the aimotion / aidata work.

Oracle dump `builds/startdiag/oracle_start.tsv` (first InProgress f 1156, same gate matrix as the product at f 1109) came
from a temporary read-only `REFLEX_START_DIAG` block in `native/drafts/world_runtime/boot_main.cpp`; the shared file was
restored byte for byte (md5 ac21b10422ba4de1f1e1c6c5065ed751, edited copy in `builds/startdiag/backup/`).

## Round 8 (fxgate, 2026-10-20): the UI scene's extra active effects and the corrected grid LCG accounting

Build dir `builds/fxgate/` (oracle CLI `cli/`, product driver `api/`, probe `probe_build/probe`, logs `logs/`, backups `backup/`). The task
was the last 148 LCG units at the state-6 grid pass (loadfix: product 2091 / local StartZone 11 vs oracle 2239/2240 / zone 5, framecmp
countdown+0 `b0[0]` 0x442002c1 vs 0x441779bf), with the premise that the oracle's pre-grid block is two UI-scene ticks of 17 rate draws + 11
builds (204 each).

**The premise is wrong; the measurements.** A new env-gated boot diagnostic (`REFLEX_FX_TRACE`, the natives at 00696940 / 00695780 /
0069587b / 006944c0 / 00696810 / 006968e0 / 0084ed20, plus the existing `REFLEX_GRID_HOOK`) gives, for the reference baton boot
(`National,10,NAT_Concord,50,0,2,Laps,3,0`, 12 riders, AI 95):

- Between the Noise1D table build (game LCG count 1829) and the state-6 grid pass (count 2238; the zone draw is 2239/2240) the oracle runs
  **33 Engine::Update frames**, one UI-scene tick each, and the whole 409-unit block is **one emitter: the UI world's idle vehicle effect
  set** (0069b060 VehicleParticleEffectEmitter::vf3 -> 00695780; the emitter states are the static set records 0xc121c0 ...): 22 particle builds
  (17 units each: 00694509 +1 then 0069456e +16) + 34 rate draws (0069587b) + one rider init (0061a32a) = 22*17 + 34 + 1 = 409. Per frame:
  1 build and 1-2 rate draws (18-19 units), not 17 + 11.
- The scene's **24 PlaceableParticleEffect (0x102a) entities draw nothing in the oracle**: all have their controller disabled (`+0xbc` = 0 at
  every tick) because `TutorialManager_Construct` 0074fd10 calls `TutorialManager_ShowLessonObjects(manager, -1)` (00751c40) at boot, which
  turns off every "TUT<k>" effect through the controller's vf5 (006968e0). The trace shows 24 vf7 (00696810) then 24 vf5(0) from 0075206e.
- The product has no TutorialManager (the documented boundary, MXUIWorld +0x22180 = 0), so the pass never ran: all 24 stayed enabled and drew
  a rate + spawn every UI tick (24 rate draws + 14 builds = 262 units per tick - loadfix's number).

**Fixes.** (1) `core/game/fx_tutorial.c` + `oracle/fx_tutorial.cpp` (group rx_fx) port 00751c40, and `rx_mxuiworld_start` calls it with
lesson -1 at the original's site. Per-call Verify in the baton boot: 1 call / 1 verified (only the function's own dead stack words differ);
the Verify boot's world image is **byte-identical** to the reference's - the scene/effect writes are exact. (2) The FX gate's timing pause
word is 0 in the oracle from MXGame state 3 on (measured), not only at the state-6 pass; the product's stand-in cleared it at the state-5 end.
`rx_ui_request_level` (0059bd60) now makes the loading page's `App.Pause(false)` clear (Lua binding 007424a0) in the product at the request
site. Both are product-side; `#ifndef RX_ORACLE` (the oracle runs the originals).

**Result.** The product at the state-6 grid pass now has the oracle's FX state: 24/24 placeables `active 0`, no scene FX draws, grid LCG
**1829** (was 2091) and local StartZone **7** (was 11; the oracle 5). So the gate is fixed, but the acceptance is **not** reached: the product
is 409 units short because it has only ~1-4 pre-grid frames, not 33.

**What remains (not the gate, not a pause window in the gate's sense):** the original's state 5 is the loader's "Subscene Creation" job
(Database vf14) and lasts **31 frames** (FXFRAME: state 3 at frame 306, state 4 at 307, state 5 at 308..339, the grid at 339/340); the product
constructs the world synchronously in case 4 (`rx_world_construct`, construction.md 16.4) and spends **1 frame** in state 5, so the 30-32
UI-scene ticks that draw the idle vehicle's effect set do not exist. Running the world construction as the original's job (or pacing the load
frames) is the load/world-construction unit's work; with it the count becomes 1829 + ~409 = the oracle's 2238 and the zone follows. The pause
clear above is a prerequisite (otherwise those frames would not draw).

Product runs (`RX_FULL_FRAME=1 rx_api_test <install> N`, live tree `builds/fxgate/api`, 19:07 build): 1400 -> **1409 frames** (9 load + 1400
race), state 2 racing, game time 23.522; 5000 -> stops at race frame 1558 at unported **005278e0** (a race behaviour mode 1,
race_beh1_small.c; the same stop occurs with the pause edit reverted, so it is the current tree's race-side stop - the startdiag entry
"unported 00529190/00550b70" class, aidata/aimotion). `core/build.sh` clean. (While `core/game/loop_cam_firstperson.c` was mid-edit by
another agent 18:45-19:07 the product build skipped it and stopped at load frame 1 in `rx_camera_factory` -> 00641f20; the diagnostic copy
`builds/fxgate/fppatch/loop_cam_firstperson.c` + `build_api_fp.sh` in this dir were used for the interim runs and are kept for the record.)

## The ride on the fixed checker, and after ridepath (inputrace2, 2026-10-20)

`builds/inputrace2/` (README, logs, `ride.c` = inputrace's driver + `--riders/--ai/--hold` and particle columns; no `core/game` file
edited) reruns the 5400-step host-input ride on `builds/startdiag/eobj_fixed` (preserved camsnap objects + the fixed wreck checker;
checker object md5 `f66eb3395b16de08cda38001bb653e30` = the current fixed source). Both grids ride; every run ends at an unported
function, none at the input layer.

| grid / input | steps | racing rows | max speed | gear | path | stop |
|---|---|---|---|---|---|---|
| 12, W held | 1452 | 354 | 78.33 | 3 | 281.5 | `00529190` (AI offtrack aim), race+353 |
| 4, W held | 1766 | 505 | 87.43 | 4 | 507.0 | `00550b70` (player reset / terrain mat 7), race+504 |
| 12, clean | 1462 | 364 | 83.66 | 4 | 359.5 | `00529190`, race+363 |
| 4, clean | 1689 | 428 | 87.23 | 4 | 448.3 | `00848af0` on the next step, race+427 |

No laps (`lap_time` = `race_time` 6..8 s, `best_lap` unset `7f7fffff`), no wreck, camera 6.78..21.4 units (26.8 clean), no dust
(preserved particle managers 0). **Oracle W-held** (331 racing frames): the input block is identical **2648/2648 words**; the first
differing word is speed at race+0 (`3df9ecd9` 0.1220 vs `3ec4b012` 0.3842) and it is the known grid slot 7 vs 5: product
`615.359 114.322 -385.692` vs oracle `605.903 114.406 -389.569` (10.22 units). The speed curves are one curve shifted 10 race frames
(RMS 0.19/0.38/0.51 ft/s over 0..60/60..150/150..250), gear shifts 143/168/204 vs 135/159/194; at +150/+180/+200 product
47.27/58.50/65.10 vs oracle 50.75/61.85/67.86. From +250 the lines part once the start offset and the still-unported AIData accumulate.

**After ridepath** (`00529190` 18:49, `00550b70` 18:50; current tree rebuilt with `build_current.sh`; the mid-edit
`loop_cam_firstperson.c` first needed the documented patched copy, then compiled again at 19:07 and the rebuild was byte-identical
for the 12-rider W log): 12/W **456 racing rows**, max 87.71, gear 4, 433.4 units, stop `005278e0`
(AI navigate mode 1) at race+455; 4/W 471 rows, `005278e0`; 12/clean 477 rows, `00592b60` (stunt audio handler); 4/clean unchanged
(same hash `f14d8b00b15ae994`, `00848af0`). The current build's input block is identical to the fixed build's for all 1452 rows,
and its speed / position / velocity through race+234 (an AI interaction then changes the local velocity; the first difference of
any field is the AI-driven `place` at race+19); dust now appears (2725 live, 606 normal).

Input: `W`/`S` are exact (the W-held block matches), but `A`/`D` reach `rider_reflex_x` (+0x34), not `steering_x` (+0x2c): the UI
settings block `[0xd92efc]` is 0, so `Controller_BuildKeyMaps` gets handedness 0 (*left*) instead of the original default 1 (*right*),
and the left/off routing sends the keyboard steer keys to reflex and ignores the arrow keys (measured: A -> word 6 -1, D -> +1, arrows
nothing; key-map slots themselves correct). The missing profile bootstrap settings block (`00743030` / `005b80b0` on `*(u32*)0xd92efc`,
`_re/notes/profile-bootstrap.md`) is construction, not gameplay. inputrace's `--drive N` also applies its first draw one racing row
earlier than `REFLEX_DRIVE` (`race >= 2`); after that shift the remaining difference is the same A/D routing.

Pacing: fixed build 726 x 2 steps, alpha 0, paced == 1/60 byte for byte, hash `73cfdd96c3eb51a0` twice; current build 777 x 2, hash
`987d9fb87dcf2e4a` twice. Engine dt is the cap `3c888897` every racing frame. Next stops to port: `005278e0`, `00592b60`, `0076ee60`,
and the `00848af0` dispatch gap in `world_call_ctx` (a *registered* `CollisionShapeComposite::vf4` that the integer-only path cannot call).

## Ride path: the offtrack return and the reset over a reset material (ridepath, 2026-10-20)

startdiag's two stops are ported: `00529190` + `00529330` (BehaviorOfftrack state 1 / states 2..4, core/game/race_beh1_small.c) and
`00550b70` (`rx_player_reset_terrain`, core/game/race_player.c); call sites switched in race_beh1_small.c and loop_player.c. Per-call
Verify with `core/oracle/race_ridepath_probe.cpp` (no reference world runs the AI drivers: 0 original instructions in the driver code
under REFLEX_COVERAGE): 00529150 12/12/0, 00529190 15/15/0, 00529330 42/42/0, 00550b70 2/2/0. Details: notes/race.md "ridepath".

Product (live-tree build `builds/ridepath/api/startdiag`, `RX_FULL_FRAME=1`, W held, `--frames 2400`):
- **12 riders** (log `builds/ridepath/full12_2400.log`): 1563 frames (8 load, 1100 intro + countdown), **455 racing frames**, no
  wreck (`V+0x280` 0 every frame). Local speed 0.118 -> 7.105 (race 60) -> 34.184 (120) -> 47.673 / gear 1 (150) -> 58.501 / 2
  (179) -> 65.372 / 2 (200) -> 74.133 / 3 (250) -> 82.245 / 4 (350), max 87.714, 428.3 units; the oracle W-held run is 50.77 / 1 at
  race+150, 61.8 / 2 at +180, 67.9 / 3 at +200 (the ~6% gap is the known grid-slot 7-vs-5 offset). All 12 riders ride (AI velocities
  33-67 at the final frame) - the AIData tables load in the live tree (`api/rx_session.c:298`), so the AI now follow the racing line.
- **4 riders** (log `builds/ridepath/full4_2400.log`): 470 racing frames, max 87.428, gear 4, 454.6 units, same stop.
- **Next stop (both): 005278e0** (`BehaviorNavigateTrack` mode 1, the racing line, 1760 B; also 00528bd0 384 B, and mode 2
  00527fc0 2426 B + 00528a70 348 B). The local rider's own path is clean to the end of both runs; only the AI's navigate mode 1
  aborts. That is the next ride-path unit (it is already on this file's "next stops" list).
- No regression (final CLI): rx_race quick **48 ports / 0 mismatches / On identical**, rx_loop quick **11 ports / 0 / identical**
  (the earlier documented 0055f700 / 0084ed20 pair is absent with this CLI), `core/build.sh` clean.

## The 00848af0 dispatch gap on the grid path (dispatchfix, 2026-10-20)

inputrace2's 4-rider clean ride stopped on the **next step after 428 racing rows** at `00848af0`, a *registered*
`RavenPhysics::CollisionShapeComposite::vf4`. Root cause was the dispatch, not a missing port: `00667e40 Grid_ReportOverlaps`
brackets each terrain item's narrow phase with the layer collision object's vf4 (slot +0x10, the item's radius, x87-quieted
through `world_x87_pass`) and vf5 (+0x14); the vf4 target is `00848af0` (composite) or `00843fc0` (sphere), whose core
signatures are `(rx_addr self, float scale)`. `world_call_ctx` (`core/game/world_util.h`) sent every registered target
through `rx_call_original`, whose product table (`tools/gen_fnsig.py`, `src/rx_mem.c`) holds integer signatures only, so a
registered port with a float argument was reported as "unported".

Fix (world unit only; src/* untouched): `world_call_ctx_f` / `WORLD_CTX_F` in `world_util.h` call `00848af0` / `00843fc0`
with their own prototype at the original's stack depth (the argument in the original's pushed slot at [esp], SP lowered by
argument + return address, the same shape as `world_vcall_f`), and fall through to `world_call_ctx` for every other target
(the shared empty vf4 `00679000` keeps the integer path; the unported cylinder `00844050` still aborts by name). The oracle
build is unchanged there (`#ifndef RX_ORACLE`): it runs the original so the adapter verifies the port. One call site:
`world_wreck.c rx_world_grid_report_overlaps` now passes `rx_bf(radius)` (a float) instead of the integer bits.

Verification:
- Oracle: no reference world reaches `00667e40` (0 calls in the five worlds, `rx_world` / `rx_phys`), so
  `core/oracle/phys_coll_probe.cpp` (new, development only: a frame hook on 0084ed20, group `rx_phys`) builds composites on the
  game heap and calls the original through the installed port:
  `REFLEX_PORT=rx_phys,rx_PhysCollProbeHook REFLEX_PORT_VERIFY=1 RX_PHYSCOLL_PROBE=csp` ->
  **00848af0 96 calls / 96 verified / 0 mismatches** (0..3 children, ordinary / 0 / -0 / inf / sNaN / qNaN scales, stub
  children and nested real spheres), **00843fc0 16/16/0**.
- Product (`builds/dispatchfix/current`, `RX_FULL_FRAME=1`, 4 riders, clean input, 5400 requested): **the pre-fix log's 1689 data rows (through the old stop) match byte for byte**, and the ride now **continues to race+432** (1695 steps, 434 racing rows; the next step aborts,
  455.4 units, gear 4, same engine dt), stopping at the genuinely unported `00861a60 CollisionPair_SphereVsSphere`
  (registered=0 in the census). The other routes are unchanged: 12/W and 4/W stop at `005278e0`, 12/clean at `00592b60`
  (all unported), so no new dispatch gap follows on these paths.
- No regression: rx_world quick 132 ports / 0 mismatches / On identical, rx_race quick 48/0/identical, rx_loop quick
  12/0/identical, rx_phys quick 130/0/identical; `core/build.sh` clean.

Same-class scan (recorded in notes/world.md "Dispatch gap"): every `WORLD_CTX` / `WORLD_VCALL` / `WORLD_ORIG(_EX)` /
`rx_call_original(_ex)` site was matched against the registered signatures and `_re/out/vtables.tsv`. Only the two shape
vf4s are registered non-integer targets reached that way; the other float/vtable dispatches are already typed at their
sites (world_misc `km_value`, world_node_input `sub_keymap_value`, world_wreck_checker `vehicle_rate`, `world_vcall_f`, the
motocross vf60/vf35/rider vf19 calls, build_activity_game's oracle-only fx calls). Residual: `rx_vcall`/`rx_tcall` would
still *silently* call a float-signature registered target through an integer prototype; use the typed helpers.

## Round 9 (gridsubscene, 2026-10-20): the Database vf14 "Subscene Creation" job paced as the original does; what the pre-grid frames really are

Task: reach the oracle's pre-grid game LCG (2239/2240, local StartZone 5) by making the product build the event world through the
Database's asynchronous job (vf14) instead of the synchronous `rx_world_construct` call. Build dir `builds/gridsubscene/` (product
`api/`, oracle CLI `cli_ver/` = the shared tree with this round's core changes, instrumented CLI `cli/`, probe `probe_build/`,
backups `backup/`). The measurements below **correct the fxgate attribution**: the 33 pre-grid frames are not the subscene job.

### What the original does (measured)

Instrumented baton boot (`boot_main.cpp`'s env-gated `REFLEX_LOAD_TRACE` / `REFLEX_LOAD_SETS`, restored afterwards; instrumented
copies in `builds/gridsubscene/backup/*.instrumented`), reference event `National,10,NAT_Concord,50,0,2,Laps,3,0`, 12 riders, AI 95:

| frame (end) | game state | game LCG count | note |
|---|---|---|---|
| 305 | 3 | 24 | the level request (the packages / tables load; 1830 by the end of 306) |
| 306 | 3 | 1830 | the player's vehicle spawn binds the UI world's FX set 0 (veh, 33 records); the Noise1D count 1829 is passed here |
| 307 | 4 | 1849 | +19 (one UI-scene tick: 1 build 17 + 2 singles) |
| 308 | 5 | 1850 | +1 |
| 309 | 5 | 1868 | +18 |
| 310..339 | 6 | 1886 .. 2238 | 30 frames, 21 x +18, 1 x +19, 11 x +1 |
| 340 | 7 | 2258 | the grid pass happened during 339/340: the zone draw is 2239/2240 -> StartZone 5 |

The 408 units from f307 to f339 = **22 builds x 17 + 34 single draws** (the UI world's idle vehicle effect set, as fxgate counted).

**State 5 is 2 frames, not 31.** The 30 frames fxgate called "state 5" are state 6: `MXGame::Update` case 6 asks
`FUN_0075f2b0` (the players' readiness) every frame and it returns the loading page's *accept key* state (offline branch:
`FUN_00599110` = the first pad/keyboard device whose UI accept key is held). The baton boot presses Enter 30 frames into state 6
(`boot.cpp`: `off = at6 - 30`). Measured with an env-gated `REFLEX_ACCEPT_AT` (boot.cpp, restored afterwards):
- accept on the first state-6 frame -> state 7 at 311, the grid at LCG ~1887;
- accept after 30 frames (the default) -> state 7 at 340, the grid at 2239/2240 (zone 5);
- no accept key -> state 6 forever (400 frames, the event never starts).
So the 409 units are **the loading page's key wait**, a UI/input gate; they are not the subscene construction.

The subscene job itself: `Database::vf14 0084c0f0` queues record {kind 5, progress, `FUN_00759f00` at +0x18c, game at +0x190} in the
loader ring and signals the loader thread (`Database+0xf8`); `MXGame::Update` case 5 polls `Database::vf11 0084c090` (queue empty).
The loader thread (`Database_LoaderThreadProc 0084c760`, case 5) calls the record's function; the construction's audio round trip
(`00766110`: `Audio_PostCommand(1)` + `WaitForSingleObject`) is serviced during the next main-thread frame, so the job completes
between frames 309 and 310 -> the 2-frame state 5.

### What changed in the product

- `game/build_asset.c` / `.h`: new `rx_database_vf_submit_job` (vf14 0084c0f0, registered; the oracle branch runs the original as
  vf4 / vf15 do), `rx_database_ring_free` (FUN_0084c5b0) and `rx_database_loader_step` (the loader thread's queue turn: +0xf0 = +0xf4,
  each record from +0xec served as `fn(arg, progress)`, cleared, +0xec advanced). `rx_database_request` no longer advances the ring
  cursor past a queued job. `vf11` was already ported (`build_db_query.c`).
- `game/build_activity_game.c`: state 4 now calls **Database vf14** (`act_vcall(core+8, 0x38, ...)`) in both builds (the product's
  synchronous `rx_world_construct` call is gone); the product's frame end calls `rx_database_loader_step` after `ACT_END`, i.e. the
  loader thread's turn between frames. The world is used only when state 5 sees vf11 idle, as the original.
- `game/fx_worldparticles.c` + `scene_fx_entities.h`: **00691cc0** ported (`rx_vehicle_effects_slot_bind`, EDI = the vehicle): the
  first free vehicle effect set gets the active scene's ParticleEntity (+0x220) and the vehicle (+0), rebound before/after the store.
  `build_activity_spawn.c` calls it in the product (the oracle runs the original). Without it the product's effect sets stayed empty
  (`set+0 == 0`) through the whole game.

Product load frames now: 9 (states 3/3/4/5/6/7/8/11/2); state 5 is **1 frame** (the product has no audio thread, so the construction
does not block on 00766110's audio round trip - the documented audio boundary), and the world is built at the end of the state-4
frame.

### Why the acceptance is still not reached (two blockers, both outside the job pacing)

1. **The vehicle effect records are empty: the ParticleAttribAsset loader is not ported.** The oracle's UI world set 0 holds 33 bound
   records from `UI_Compound_Particles` (partattr handle, data +0x10 = **370** emitter records; 33 match the local vehicle's
   +0x194/+0x198 kind). The product's set now has the vehicle (after 00691cc0) but `n = 0`: the asset handle's +0x44 data is 0 because
   `ParticleAttribAssetIO::vf3 006940a0` and its parser `FUN_00693870` (1784 B of BXML: `Emitter` count + 370 x 0xa0 records:
   name, shape/zone/distance-zone flags, offset, VehicleType/EngineClass/Tire maps, the five trigger blocks, TireInAir/TireInWater,
   the five emit modifiers) are not ported. `builds/gridsubscene/UI_Compound_Particles.partattr` (extracted read-only with
   `_re/formats/package_extract.py`) and `bxdump.c` are the reference for that port; the record layout is the one
   `fx_worldparticles.c` already documents. The event world's `Nat_Concord_A` has 171 records.
2. **The loading page's accept wait (30 frames).** The product's front end is a documented UI boundary: `rx_game_players_ready`
   (0075f2b0) takes the "the product's front end has accepted" branch (`act_ui_call(00599110) = 0`) and goes straight to the grid.
   Matching the reference boot needs the page's key gate (the original's own `FUN_00599110` over the product's key maps) and a driver
   that sends the same accept key; that makes every product test driver press a key, so it is a lead decision, not a silent change.

With both fixed the product would draw the same per-frame pattern (2 x 18 + 1 x 1 over three frames) from frame 3 on and land on
2239/2240; today it is 1829 (+0), StartZone 7, `b0[0]` `0x4419eb68`.

### Verification (this round)

- Product: `RX_FULL_FRAME=1 rx_api_test <install> 1400` -> **1409 frames**, state 2 racing, no stop; terrain/ambient/particles getters
  as before. `builds/framecmp/run_cmp.sh --frames 1400`: countdown **181/181**, **357 racing frames** in the product (oracle 431 in the
  same 1400-frame window), game time advance identical (8.949863 s over the compared frames); grid slot **7** (`b0[0]`
  `0x4419eb68` / 615.678223) vs oracle **5** (`0x441779bf` / 605.902283). Log `builds/gridsubscene/framecmp_1400.log`.
- Oracle CLI `builds/gridsubscene/cli_ver` (shared tree): `verify.sh ... quick` rx_world 132 / 0 mismatches / On identical, rx_race
  48 / 0 / identical, rx_fx 108 / 0 / identical. Baton boot `loading_accept.sh rx_build11` (VERIFY=1): 98 ports, the 6 documented
  dead-stack mismatches, Verify boot world identical, 11 differing nodes (known baseline), race compare 52/52 IDENTICAL.
- `core/build.sh` clean (see the agent report for the final run; other agents were editing concurrently).

### Round 9 follow-up (partattr, 2026-10-20): blocker 1 closed -- the vehicle effect sets hold the oracle's records

Round 9's blocker 1 (`ParticleAttribAssetIO::vf3 006940a0` / `FUN_00693870` unported, the handle's +0x44 data 0, set 0 n = 0) is fixed:
`core/game/build_partattr.[ch]` ports the loader, the parser and its helpers (enum lookups, the trigger block, the record initializer,
the destructor) and `build_load.c` registers it as the `partattr` loader. Details and the record layout: `core/notes/fx.md` "The partattr
asset (partattr)".

Measured (reference assets extracted read-only with `_re/formats/package_extract.py`; `builds/partattr/`):

- `UI_Compound_Particles`: **370** records, `Nat_Concord_A`: **171**; both streams consumed exactly; 43 partattr assets of the install,
  0 failures. Every record field of both assets equals the original's loaded asset object in `builds/gridsubscene/la/ref/c-world.bin`
  (370 / 0 and 171 / 0 written-byte differences).
- Product: the UI world's set 0 = the local vehicle with **33 records** at the state-6 grid pass (the oracle's count); the event world's
  set 0 = **20 records**, the reference world image's own set 0 (n = 20, same order). The controller/manager/holder fields match.
- **Grid LCG 1829 -> 1887** (local StartZone 7 -> 1), the oracle 2239/2240. The +58 units are the UI world's idle-vehicle effect ticks
  (the oracle's per-tick shape; `Exhaust` 30/20, 1 build 17 + 1 rate). The remaining **352** units are the 30-frame loading-page accept
  gate -- still the separate lead decision of Round 9, not changed.
- Ride (RX_FULL_FRAME=1, W from the first racing frame, 1400 frames, no stop): the wheel dust / roost records emit (Low_Ground_Dirt,
  Hard Hit, Pig Pen, Roost_StartGate: ~190 wheel particles; `Exhaust` ~116), live particles ~1900-2200 stationary -> ~2900 riding.
- No regression: rx_fx 108 / rx_world 132 / rx_race 48 ports, 0 mismatches, On identical (Concord quick); `core/build.sh` clean; the
  product driver 1400 frames.

## Camcycle (2026-10-20, agent "camcycle"): the full C key view cycle in the product

The product's Camera key path (input -> `Player_UpdateCamera` -> `Player_SetCamera`) already selected the first person; selecting
the User view (entity 4) stopped at the unported per-frame tweak `MXThirdPersonCamera::vf30` 00649ed0. It is now ported
(`core/game/loop_cam_tpc.c`, with its three key-map queries 005486c0 / 00548710 / 00548760 in `loop_cam_misc.c`), so the shipped
cycle 1 -> 2 -> 4 -> 0 -> 1 runs end to end through the game's own code.

`core/tools/fp_camera_test.c` has a `cycle` mode (four C presses through `rx_session_set_input`, a release frame between them):
`RX_FULL_FRAME=1 builds/camcycle/api/fp_camera_test "game files/Archive" 1400 cycle` logs each step in
`builds/camcycle/logs/cycle.log` - 0x988 2, 4, 0, 1 and a finite, unit-basis camera with the game's lens for every view (first
person 60 deg / 0.5 / 15000; the three third-person views 2 / 15000). The default run (`default`, no selection) keeps the GamePlay
entity 1 camera and all four runs end with the same gameplay: speed 28.0988579, place 12, race time 4.585001, bike pose
`2e2e07f8`, rider pose `94b5cd41` at frame 1400.

Oracle: the User-view probe (`RX_FP_PROBE_USERCAM=1`) exercises the real call site; `rx_MXThirdPersonCamera_vf30` 535 / 535 / 0 and
the three callees 1 / 1 / 0, On identical; `rx_loop quick` 18 ports / 0, `rx_world quick` 132 / 0, `rx_race quick` 48 / 0;
`core/build.sh` clean.

## Introcamera (2026-10-21, agent "introcamera"): the intro fly-by camera is live in the product

The product's intro scenario ticked `world+0x218f0` but its pose stayed 0; the getter therefore returned 2 until the start-gate
camera appeared. Three root causes (details, values and verification in `loop.md` "Introcamera"):

| # | gap | fix |
|---|-----|-----|
| 1 | `0063d7e0`'s inlined pause test reads the PCSession `[0xda6cb0]` / device manager `[0xda6cac]` (006de390, Quazal/Steam) and the window focus `0xa9596c`; all absent in the product, so it read the zero page and held the camera still | product-only rule in `rx_animated_camera_vf19`: a headless run is the focused offline game (the same boundary as `006e0c50`); the game's own pause value still holds it. ORACLE unchanged |
| 2 | the per-step clips are animation-set names (`CameraStartLine`, `CameraHeroBike`) resolved through `006b6a30` / `[0xd5a9e8]`; the table (`006b68e0` + `AnimationSets.bxml`) was not built by the init-thread stand-in | new `rx_anim_sets_load` (`anim_ik_ent.c`, same entry layout / lowercasing / flags / `_stricmp` sort), called in `build_package_loaders` before `ClipRecorder_Init` (`api/rx_session.c`) |
| 3 | the path fov converts through the display aspect (`0063e850`); the ctor leaves 640 x 480, ReflexWinMain's `graphics.cfg` is 1280 x 720 | `rx_engine_startup` sets the display block from the config (1280 x 720; GraphicsConfig_Load itself still the build6 stand-in) |

New ports the path needed: `008435f0 FloatKeys_FindSegment` (the interpolated value track `00839f40` uses), `00835170`
(random motion start time for the intro's placed cast, ESI = character), and `008403b0 rx_rider_bind_anim_tracks` registered
(the pose override `0047c0b0` called it through `rx_call_original_ex`). Adapters: `oracle/anim_character.cpp`.

Product: rc 0 on 1104 / 1115 pre-race frames (the 11 are the load frames, `+0x2b0 = 0`), finite, fov and pose moving through
every step; the animation sets resolve (the step clips change per step). Oracle: the first clip matches bit for bit at the same
path clocks (t 1.067 / 1.901 / 2.734), the second did too when the random set pick agreed; the hero clip's fov matches, its
base is the hero entity / grid (the concurrent tree's grid shift moves it). No regression: rx_anim / rx_loop / rx_world /
rx_race quick 0 mismatches, On identical; baton boot VERDICT IDENTICAL; `core/build.sh` clean.

## Round 10 (holeshotfix, 2026-10-21): the holeshot dispatch's null UI channel; the product's 1-lap race reaches Post and the records

Task: the 1-rider / 1-lap product race (W held, full frame) hung at race 8 s in the event-map lookup. Build dir
`builds/holeshotfix/` (product `api/finishrun`, oracle CLI `cli/`, logs, backups). Details and the stop table are in
`core/notes/race.md` "holeshotfix".

**Root cause (not a malformed map: the map object was missing).** `rx_race_holeshot` (00569eb0) raises "HoleshotActivated" on
`[0xa93fcc] + 0x98`, the MXUIManager's EventSystem::Channel "UI" (the manager pointer 0xa93fcc is written by the UI manager's
base constructor 005da520 at 005da6ac; the channel is built at 005da6cb..005da739). The product has no UI manager
(`[0xa93fcc] = 0`, documented), so the dispatch chain was 0: `rx_world_registry_find(0, name)` reads a "map" at 8, `head = 0`,
and `rx_world_string_map_lower_bound` starts at `x = U32(0,4) = 0` with `U8(0,0x2d) = 0`, never reaching a nil node - the zero
page is not a std::map. Fixed by constructing the channel the manager's constructor makes (`rx_event_channel_create("UI")`, the
four events the game raises on it registered with their original signature type_infos) in `build_ui_world.c`'s UI-manager
stand-in and routing the three raisers through `rx_ui_event_channel()` (oracle: `[0xa93fcc]+0x98`, unchanged). The event is
dispatched; it has no UI subscribers. Also found: `rx_show_page_wipe`'s product branch passed the name as the payload pointer
and size 0 (the oracle branch was right); it now writes the name's slot and passes (slot, 4).

**The rest of the 1-lap ride.** Ported `006cc230` (stunt scorer's FlowJudging "VehicleEntered" handler), `006ab560`
(ClipRecorder_FillerClip, the recorder tick's only missing callee), `0076f160` (Terrain_ContactResponse), `005595b0`
(Activity::vf33 InProgress exit), the race audio logic's finish-state handlers `00583130 / 00583170 / 00582070 / 005848a0 /
005820d0` + vf37 `00582220` (music-player calls = the documented audio boundary), `0050f5d0` (RaceDriver::vf4) and `0054e280`
(the player finish reset); switched `loop_entity.c`'s `006c1a90` call site to the registered score-tower port.

**Product (1 rider, 1 lap, W held, `RX_FULL_FRAME=1`).** 9 load frames, racing at step ~1200 (game 20.2 s), **FINISHED at step
9906 (session frame 9916, game 165.304)** state 3; the wind-down wipe -> **activity 4 (sub 2)**, the full 16000-frame run has
**no stop**. Records slot 0: `TotalEvents=1 TotalLaps=1 TotalPlacing=1 TotalScore=0 TotalTime=146.399994 BestLapTime=FLT_MAX
BestPlace=1 BestScore=0` (local standing: place 1, laps 1, finished 1, 146.384 s). The oracle's 1-lap image run (finishui) is
`1 / 1 / 1 / 82 / 162.550003 / FLT_MAX / 1 / 82`: the time difference is the race-clock start (the oracle's captured world
image skips the front end, both cross the line at the same game time), and the score is `MXPlayer+0xcec`, which no offline
race code writes (only the network snapshot 0050cf00 and the freestyle finish 005626e0 do) - the oracle's 82.891670 is
load-state (profile / saved game) already in its world image. Reported for the profile owner.

**Verification.** `core/build.sh` clean; `verify.sh <cli> rx_world quick` 132 / 0 / On identical, `rx_race quick` 48 / 0 /
identical; the 1-lap captured-world runs with and without the ports (`REFLEX_WORLD_TEST=builds/finishui/world1lap/c`, W
held, 20000 frames) are in `plan/QUEUE.md` "### holeshotfix" (hashes).

## Round 11 (bikestate, 2026-10-21): the bike state's non-finite w lane after the terrain pair; the 12-rider ride passes race 287

Task: rideranim2's 12-rider W + A/D ride runs 1011 racing frames, but the bike getter's position / velocity turn non-finite
from race 287 (the frame after the terrain pair `0086f2a0`). Build dir `builds/bikestate/` (product `prod/`, oracle CLI
`cli/`, private instrumented copy `diagsrc/` + `diagbuild/`, probe `bikestate_probe.cpp`, logs). Details in
`core/notes/phys.md` "Dead w lanes in the contact records".

**Diagnosis (instrumented private product build, `builds/bikestate/diagsrc`).** The watchpoints on the simulation's body-0
words put the first non-finite bike word at `orientation[1][3]` (angular velocity w, race 210): `bike_air_control` (0043f2e0)
multiplies the 4th lane by the `AirBorne_AngularRateDamping` parameter's 4th lane, which is the parameter default -7.774
because the XML vec3 reader writes only three lanes. That is faithful and dead (the oracle probe shows the original's own
`angvel.w = +inf`); it never becomes NaN by itself. The **NaN** (0xffc00000) arrived later: the terrain pair's contact record
w lanes were `FLT_MAX` (dead stack: the query writes xyz only), the kinematic water impulse overflowed
(`n.w * vn = inf`, `inf + -inf = NaN`), and the NaN travelled through the water body's velocity.w, the generic object's
contact result +0x80.w and `bike_apply_external_impulse` into the bike's velocity.w → pose.w at race 287. A second generator,
`0086bd80` (cylinder / cylinder), copied `0xffffffff` into its record point w, which the body-probe correction added to the
pose: position.w NaN at race 396 of the steer-from-205 ride.

**Fix.** Product-only initialisation of the two dead w lanes of `phys_pair_cylinder_hfterrain` (`T+0x1c`, `T+0x2c`) and of
`phys_pair_cylinder_cylinder` (`L+0x9c`, `L+0x6c`) before the record copies (`#ifndef RX_ORACLE`). Live lanes untouched;
the oracle build keeps the residue so the pair's per-call Verify stays exact (`builds/terrainpair/pair_probe.sh`: 0086f2a0
64/64/0 direct and 1064/1064/0 dispatcher, seeds 0/7/12345).

**Product.** 12-rider W + A/D (firstride, live product dylib): **1231 racing frames** of 2400 steps (was 1011, stop 7859afc0
`_CIpow` at race 1010), no stop, `bike.getter` PASS, `rider.bones` PASS (was 723 non-finite pose reads), 0 non-finite
position/velocity words in the whole TSV. The next stop is the unrelated **unported `0076ee60`** (a static contact response
reached at race 1231), not a NaN. The same build with steering from race 215 runs **2031 racing frames** of 3200 steps with 0
non-finite words (the >1400-frame check). The camera check still reports its documented 157.84-unit max distance (the intro
camera during the countdown; camcycle/rideranim2's check, not this unit).

**Verification.** `verify.sh <cli> quick` on the final CLI: rx_phys 130 / 0, rx_world 132 / 0, rx_race 48 / 0, rx_loop 20 / 0,
rx_anim 44 / 0, On identical on all five (state `1f6611fa80b80831`, memory `1487a52f208b4b26`); `core/build.sh` clean.
