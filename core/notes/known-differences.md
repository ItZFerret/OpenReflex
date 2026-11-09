# Known differences between the core and the original (by design, documented)

Each entry: where, why the original's value cannot (and need not) be reproduced, how verification treats it.

1. **Uninitialized locals stored into game memory (dead-stack consumers).** Some original functions store a local they never
   initialized into an object; the value is whatever an earlier call left on the stack (garbage in the real game too, different
   between runs of the real game whenever the call history differs). A hand-written core function cannot reproduce dead stack.
   - 128-byte particle / mud-effect records, field +0x6c (Terracross_TRX_Verde_ai11: [13d43bec] etc.; the old "[142d776c] 1 vs 3"
     artifact; NAT_Renne / TRX_HalifaxBW / FR_Gibson_1): **REPRODUCED (w-residue 2026-10-26 for the mud records, w-residue2 2026-10-27 for
     the last consumer).** The field is the particle template's never-written +0x4c (the emitter 00695780's template copied by
     Particle_Copy 00692b50). The remaining case was SX's rx_SceneManager_Tick / rx_loop with `REFLEX_FRAME_TAIL=1` (calls 2191 / 2215 /
     2247 ..., 54 mismatching calls of 3000; core 0, original 0x3c868a1e ..): the original's last writer is Vec4Atan 0042c650's frame
     (the atan t lane) under QuatSlerp_Simd <- AnimKeys_SampleRotation <- Character_SampleKeyframes <- ScoreTowerFlagHolder::vf19
     006c1a00; the core's 006c1a00 had no `push esi`, so the sampler (and phys_quat_slerp_dead_stack's model of the slerp frames) ran 4
     bytes shallower, 16 after the alignment. loop_entity.c now pushes it (FxBind on the adapter). SX with the tail: rx_anim 0
     mismatches; rx_loop Verify 0 and On = no ports (f30baf9b814f5255 / 26329 pages).
   - JobPool ring 0x00d94b10..0x00d9cb10 (job records copied from the submitter's stack): excluded from Verify / MEMHASH for rx_*.
   - Worker stacks: only the verified call's own frames are compared.
   - (fixed 2026-10-19: the 006b0530 rider-record w lane: Matrix3_ToQuaternion now writes the original's stack cookie)
   - **rx_loop's Engine::Update residue (FIXED 2026-10-27, w-residue2):** Concord call 1697 [152414cc], SX call 1375 [1524dd5c],
     Terracross call 1201 [1524da1c] were the same 006b0530 rider-record w lane (a lerp of two vectors whose w lanes are dead stack
     of 006b0530's frame, worker stack 0x7fbffc1c on Concord). Bisected to rx_Rider_StageTwist 00636990 (rx_Engine_Update + that one
     port reproduces all three): the original's last writer is Matrix3_ToQuaternion 00401210's frame (the AltiVec byte-permute
     emulation stores eleven vectors in its 0xc0-byte frame) through Matrix3_ToQuaternionToEsi 00401740, called three times by the
     twist stage; the core wrote only the cookie, and 00401740 entered 00401210 without its `and esp, -16` (so even the cookie could
     land 0..12 bytes off). world_veh_math.c now transliterates 00401210 store by store (result read back from the last permute,
     equal to the old selection table on every verify world) and 00401740's frame (the saved EBP from the caller's register model:
     rx_world_matrix3_to_quaternion_to_fr); loop_rider.c's twist writes its push ebp / ebx / esi before the value test
     (rx_rider_stage_twist_fr, the adapter passes the machine's registers). verify.sh rx_loop: 0 mismatches on all five worlds.
   - **The database thread's dead stack on the career Heat path (FIXED 2026-10-27, w-residue2):** the 19 words w-vfactory left
     (0x130ffec8..0x130fff40 on the series 41 boot): the package unload's asset destructors ran ReflexAlignedFree / _aligned_free /
     free (and the D3D Releases) at other depths and with other saved registers than the original. ModelAsset::sdtor 007d4010,
     ModelInstance_Destroy 007d4030, the record range free 00545120, the node set release 007dd4d0, XenonSurfaceAsset::sdtor 007b2040 /
     destructor 007b2060, XenonTriangleSurface::sdtor 007b21f0 / destructor 007b22b0 now write their frames (pushes, arguments, return
     addresses) with the callee-saved registers of the fx / anim register model (the adapters bind FxBind + AnimBind), and the frees go
     through `rx_reflex_aligned_free_at` (build_alloc.c: 0078fc50's push esi and lock calls) and `crt_aligned_free_at` (crt_frames.c:
     _aligned_free's three instructions in front of free's modelled frame). Series boot On (w-heat ports + rx_career + 0075c430):
     0 words on the database thread's stack (before 19); the rest is the boot's main-thread dead stack (below) and the crt_undname
     scratch.
   - **SceneManager_AddEntity 007d2f70's own records (FIXED 2026-10-27, w-residue2):** its Verify on the series boot showed 300 of 3156
     calls with differing words (353 words) in the light event grid's 32-byte records (EventCell_PushA 007d6320 copies the event's
     never-written +0xc and the light position's w lane from Lights_Add's frame). The original's last writers at those depths are
     the earlier calls of AddEntity at the same ESP: SceneManager_Link 007d3580's frame and its sorted inserts 007f90a0 (the result
     iterator local the checked iterator advance 00842fa0 bumps), the insert's _Insert_n 007f95a0 (SEH record, cookie, the growth's
     0042bf30 / ReflexAlignedAlloc with the allocator lock's pushes, memmove_s through 00776d20, 006c8230 / 007cfd80, ReflexAlignedFree),
     Scene_RegisterEntityName 007d2500 (the name lookup 007fa250, the insert 007cfaf0 with the allocator's vf1 004eef20 and, when over
     its load, the rehash 0084df50 with memset and vf2 00775540). All of them now write the original's frames (anim_scene.c,
     anim_scene_vec.c, build_asset.c `rx_strtab_insert_at`, build_alloc.c `rx_reflex_aligned_alloc_at`; memmove_s / memset /
     _aligned_malloc run the originals in the oracle with the original's registers through the new `rx_call_original_regs`).
     007d2f70 alone on the series boot: 3156 / 3156 / 0 (before 300 mismatching calls).
   - Vehicle model node bounds (unit 6, VehicleParts_Assemble 00541310 -> 00541690): **REPRODUCED 2026-10-27 (w-vfactory).** 00541690
     stores the grown bound's centre and half extent with movaps from two frame vectors whose w words it never writes (frame +0x7c /
     +0x3c); the original's words there are what the deeper calls before it left. The last writers (watched per call): the std::string
     of VehicleLoader_FindPackage 00543db0 (msvcp90's _Copy_s wrapper 78485215: the count push / the return address 7848522b), the package
     database's name hash lookup 007fa250 (its node-key push; _stricmp's / __ascii_stricmp's EBP = the _stricmp import 78578d50), the
     material map's std::map insert 00773e80 (its saved ESI / EBX: the map's proxy / the map). The core now writes those frames as the
     original does (crt_frames.c: std::string(const char*) short / long path with __EH_prolog3, ~basic_string with free's __SEH_prolog4,
     _stricmp; build_vehicle_parts.c: 00543db0 / 00544220 / 005449d0 / 0084a890 / 007fa250 / 005c3400; 00773290 / 00773e80 / 00705de0 /
     _Buynode 008185d0 / ReflexAlignedAlloc), and the octree / event-grid copies of the light records (SceneManager_Link and Lights_Add's
     pointer pushes 00544cc0, the light list growth 007f9e70 / 007fa340, the spare matrices 007df210) get the original's residue too.
     Verify of VehicleFactory_CreateVehicle 00542b90: 0 mismatches on every class (career series 41: 28 / 28 / 0; NAT_Concord, SX_Springfield,
     ATV, UTV, Buggy, TrophyTruck: 12 / 12 / 0 each; FR_Alpine 1 / 1 / 0); VehicleParts_Assemble 00541310 28 / 28 / 0.
   - XenonOnlineContentPackage records (unit 6 found it; startup 006d3d30 -> 006d3e30): the constructor copies the caller's 0x608-byte
     content-data local into the record with rep movsd; the 15 differing words (record +0x80 .. +0x118) lie in the wide display-name
     buffer past the terminator of its swprintf'd "PC" and hold the residue of whatever startup code ran at that depth (with the
     package cache ported: 00543cd0's std::string key and Database calls in the original, other residue in the core); no string
     operation reads past the terminator. Race check: the only non-w-lane
     words besides the JobPool ring and one worker stack (construction.md 15.2 (c)).
   - ProxyObject records (00769160's frame copy, +0xb0 .. +0xcf): verify of a native four-wheel proxy (004c0c00 / 004a2f00 / 0046aef0)
     under the original vf28 reports the eight words (the original's deeper calls' residue); cleared in the same construction by the
     placement (vf62 -> DynObj_SetPose 00768460: the contact accumulators +0xb0 / +0xc0) (15.2 (b)).

2. **One lazily-loaded world-image page (0x1002b000) is not resident (FIXED 2026-10-27, w-residue2: it was a missing read).** The page
   belongs to the original's boot image and holds the boot ConVar registry (`ConVar<int>` vtable 0x937740, ...) and, on the same page,
   the checked-iterator proxy of the object registry vector 0x00d6d8e0. The first touch (found with the new diagnostic
   `REFLEX_FAULT_PAGE=<address>`: the run stops at the first touch of that lazily-loaded page and prints the stack) is 00842fa0, the
   checked iterator advance, inside the registry's resize 008499d0 <- Registry_Remove 00844170 <- PhysicsWorld_RayCastBodies 0085d360:
   it reads the iterator's proxy and the proxy's container (+0x10 end, +0xc begin) for its bounds checks. The core's
   `phys_iterator_advance` only added; it now reads and checks like the original (phys_coll_misc.c). rx_loop On on NAT_Concord_ai11 /
   SX: 29081 / 26335 pages = no ports (before 29080 / 26334), and race_misc_landing.c's raycast no longer needs the original in the
   oracle (rx_race On identical on all five worlds with the core raycast).
3. **FreeRide/Terracross particle-template saved registers (fixed 2026-10-19).** `build_activity_game.c`'s `rx_mxgame_update`
   called `rx_fx_vehicle_effects_update_all` 00691bd0 directly in the oracle, where the fx register binding (`fx_reg` /
   `fx_set_reg`) is unbound outside an `FX_PORT`; the saved EBX/EBP/ESI/EDI words written into particle templates were 0, giving
   the 874 FreeRide / 2 Terracross `+0x68/+0x6c` mismatches. Fixed by running the original 00691bd0 in the oracle (the core call
   is verified as its own rx_fx port); the product keeps the core call. `rx_loop` FreeRide is now 0 mismatches / On identical.
   Terracross's remaining ~970 `+0x6c` words are entry 1's documented mud record, not this path.

4. **FMXJudgingParameters oracle load: freed zlib blocks (2026-10-19).** In the baton Verify boot the original `MXGame::Startup`
   runs, so the core's `g_in` is not set and `rx_xml_reader_load_file` has no VFS. The oracle path now opens the file through the
   original CRT (`fopen`/`fread`/`fclose`) so the judging values and the world image match; the decompressor's private state is
   the exe's zlib, so after the read its freed `0x1ba8` inflate_state block, the tail of the 32 KB window and one 4-byte allocator
   object (540 words) hold host-zlib data instead of the exe's. The world image is byte-identical and all judging globals
   `0x9c3e3c..0x9c3e74` match; only freed memory differs. Accepted host-zlib boundary (`build_xml.c`); reproducing the freed
   contents would require running the exe's zlib only in the oracle.

5. **Surface asset loader is core-only (2026-10-19, chain).** `game/build_render_asset_surface.c` builds XenonSurfaceAsset (D3D index / vertex buffers through rx_d3d)
   in the product; the oracle runs the original loader, so there is no per-call Verify of the loader itself. Its acceptance is exact stream consumption of every
   surface of the real databases (`build_load_check <install> surface <db filter>`: 0 failures over ~12,000 assets) plus the verified `XenonSurfaceAsset::vf4`
   reads (112,422 calls). The dev databases `Ctrl_*` carry 0xcdcdcdcd descriptor words (never loaded by the game) and abort the all-database run. The asset's
   sdtor (package unload) is not ported.

6. **In-process vehicle factory: construction-time node-set residue (2026-10-19, b11reg; FIXED 2026-10-27, w-vfactory).** The node-set
   residue was the w lanes of entry 1 plus, on the career path, RavenVehicle::vf14_o94 004302c0 copying the block's node sets the wrong
   way (+0x50 over +0x10: NodeSet_CopyFrom 007dd930 takes ESI = destination, EDI = source; the original copies +0x10 over +0x50). The
   factory verifies exact on every class now (entry 1); 004302c0 3468 / 3468 / 0 on the career series boot. The 00767200 event-grid pad
   word is the same class (the light's position w lane copied from Lights_Add's frame) and is exact in the factory's Verify now.

7. **Oracle x87 NaN payloads (2026-10-19, racedrv; FIXED 2026-10-27, w-actarea).** The emulator's x87 (`f80`) kept no NaN payloads (`fld m32` /
   `fstp m32` of any NaN stored sign | 0x7fffffff, an invalid operation's indefinite came out 0xffffffff instead of 0xffc00000), and several core
   helpers mirrored that (cam_x87_bits, fx_x87_bits, ...). Both `f80` copies (native/drafts/world_runtime/f80, core/src/f80) now follow the
   hardware (Intel SDM vol. 1 4.8.3 / table 4-7): loads and stores keep the payload and quiet an SNaN, operations propagate the NaN operand
   (larger significand of two; the positive one on a tie), invalid operations give the indefinite; the mirrors follow the hardware too.
   Evidence: the track loader's fld / fstp copies of 0xffc00000 on the career series boot (00536ef0), wave_hidden_probe RX_HID_NAN=1 (lean
   0061db10: 9 mismatches before, 143 / 0 after), bike_remote_probe RX_REMOTE_X87NAN=1 (00432be0 / 004331d0 54 / 0 each). Left: an SNaN
   memory operand of an arithmetic instruction is quieted at the load (`from_f32`), so "SNaN operand vs QNaN register" picks by significand
   instead of the QNaN (not seen in the game). Boot images made before the fix hold 0x7fffffff where an x87 copy stored a NaN (e.g. the
   0x009cb448 constant's copies).

8. **Product boundaries of the chain round 2 (2026-10-20, chain; full list in plan/QUEUE.md "chain round 2").** The product has no UI manager ([0x00a93fcc] = 0), no
   sound engine and no network: the UI property store is the host's event spec (build_ui_props.c; "WaitForUI" = 1 while the UI world is in view, "InCareerMode" absent),
   the sound / music / FMOD calls of gameplay code (00805000 / 00805120 / 00805200 / 00674e90, the music player, the cannonball rule's event stop) are registered no-ops in the
   product only, the TutorialManager is absent (`+0x22180 = 0`), the entities CameraPanoramic / CameraScreenshot / StatusBoard / AmbientDirectionalLight leave no null slot in
   the scene's group array, and `rx_ui_world_create` does not take the original's start-in-a-level branch. The oracle runs the originals in all of them. A gameplay read of one
   of these results (a UI property other than the event spec's, a sound handle used by a rule) would be a difference: the reads were checked in the decompile for each.
9. **Verify differences that remain after the chain round 2 (re-measured 2026-10-27, w-residue2: baton event boot, default event and
   `REFLEX_EVENT=alpine`, the CLI of main ed4fb10 and of this branch give the same):** `rx_Activity_vf15` (2 calls, 1 verified, 0 mismatches),
   `rx_PlayerSlotSpawnStep` (4 / 3 / 0) and `rx_RecordsSetActivity` (Verify 0; On: the memory hash equals the no-port run, 97102 pages) are
   clean now. `rx_FreeRide_vf2` 0055ff70 and `rx_RaceVf2` 00564b20 differ only in the type-name undecorator's freed heap block (DNameNode
   vtables 0x78522684 ..: the crt_undname class, crt.md "Left"). `rx_VehicleWorldMove` 00464a00 (1 call): 2 words (the 562 were the
   light records of 007d2f70, reproduced by w-vfactory): a light event record's never-written +0xc and the next record's w lane (core
   0 / 0x428de9ba, original 0xbfa0ae7d / 9). The original's last writers at those depths are Vec4FloorToInt 0069b2f0 / Vec4StoreAligned
   0069b400 inside BroadphaseGrid_Remove 00882980 (from 008825f0 <- 0042af70 <- the move): 00882980 floors the box a second time into
   dead locals of its frame through its helpers (Vec4Floor 0042ccd0, Int4GreaterMask 00882170, Int4BelowFlags 008821e0, Int4Max /
   Int4Min 00882300 / 00882290), which the core's `phys_grid_remove` computed without frames. **FIXED (w-residue2):** `phys_grid_remove_fr`
   (phys_coll_grid.c) transliterates 00882980 with its frame and its helpers' frames (Int4BelowFlags read-modify-writes two dead flag
   words of the frame: their other bits stay whatever the stack held), and 008825f0 / 0042af70 pass the original's registers
   (`rx_grid_slot_clear_fr`): VehicleWorldMove 1 / 1 / 0.
10. **Product boundaries of the chain round 3 (2026-10-20, chain4; full list in plan/QUEUE.md "chain round 3").** (a) The engine task list's game
    tick `0067dd10` (called from `rx_game_tick_during_sync` 00757f90 during `Engine::Update`): it updates only the sound managers (the collision
    sound map 0067b670, `VehicleAudioManager_UpdateVehicles`, the audio event queue 0068d2c0, the UI localized sounds 0068de90) and the sound
    engine's pause flag 0xd94acc / 00804f00; nothing gameplay reads it. The oracle runs the original, the product skips the tick (the rest of
    Engine::Update - the scene tick, the job pool, the timing, the input - runs). (b) The scenario script's SoundEffect handler `0055d610`: the
    sound name's "{...}" / "Scenarios/All/<name>" resolution ends in the audio command queue (0067edc0 -> Scenario_AudioCommand); product
    nothing, the oracle the original. `#ifndef RX_ORACLE` in both places.
11. **Unported branch, reported (chain round 3):** `00705150`'s node-by-node subrange erase (00706030, the MSVC std::map iterator erase with the
    `std::string` / `_CxxThrowException` "invalid map/set<T> iterator" paths, 667 bytes, plus 00706dd0 / 00705020 / 00690540). Its whole-map form
    (free the header's subtree, relink, clear the count) is ported; the only product caller (`006893b0`, the rider character's map release) passes
    the whole range. The loop branch aborts naming 00706030 rather than guessing its iterator bookkeeping.
12. **Typed virtual calls and the argument slot (2026-10-20, chain4): the round-3 On-mode regression, and the rule it leaves.** An original
    `push arg; call [vt+off]` puts an argument in the **callee's argument slot**; some ports read their float from there, exactly as the
    original does (`0063d7e0` `rx_animated_camera_vf19` reads `sp0 + 4` for its dt because the restart rewrites it). A typed C call
    (`void (*)(rx_addr, float)`) passes the value in an SSE register and only lowers `rx_sp`: the slot keeps the previous frame's dwords, so
    the port runs with a stale dt. That was the whole cause of round 3's On-mode differences (the animated camera pose, the cast matrices and
    the audio command blocks 0xd67dc0 / 0xd6f640 at ulp level). Rule: when switching a call site from `rx_call_original` to a typed call of a
    port, write the argument bits into the slot the `push` wrote (`SU32(sp - 4, 0, bits)` with the return-address slot below) — as
    `anim_scenario_intro.c` now does for `0063d7e0` and `0074ab50`, and as `rs_site` / `loop_push` do for the integer calls. The same class
    was found in `race_activity_stub.c`'s `006235e0` / `006237a0` calls (now pushed through `stub_site`).
13. **Frame-count dependence of the race-world acceptance runs (2026-10-20, chain4; corrected by clockpin).** `REFLEX_AFTER` counts
    frames, and the chain-4 note compared runs that also differed in frame count: `tracks version 3600 deform 74` (state hash 1f6611fa...,
    four times in a row on an idle machine) is the **1200-frame** run and `tracks version 5400 deform 397` (state hash a936214c...) is
    the **1800-frame** run (`verify_rx_anim.log` vs `final2_rx_anim.log`). `tracks version` is bumped three times per rendered frame
    (three `TerrainTracks` buffers unlock), so it is a frame counter, not a simulated-time counter; the oracle's virtual clock never
    reads the host in either path (the boot advances by instructions, the world runtime by its caller's ticks). Re-measured with one
    binary and one world image: 1200 frames gives `3600 / 1f6611fa / 74` and 1800 gives `5400 / a936214c / 397`, idle and under CPU
    load alike; an unset boot world is byte-identical to the pre-change CLI's. Consequences: (a) an "On identical" verdict is still
    meaningful (ref == nat in the same invocation), (b) a "DIFFERENT" verdict must be compared on the same frame count, and (c) a
    five-world matrix must be run one verify.sh at a time and compared on the same baseline. An opt-in pinned clock now exists:
    `REFLEX_FIXED_CLOCK=1` (or `=<hz>`) makes every frame exactly 1/60 s in the baton boot too and leaves the default path byte-identical
    (`native/game/notes/clock-pin.md`, `builds/clockpin/`).
14. **Remaining after chain round 4 (reported, not papered over):** (a) **FIXED (riderchar):** `0055f700 ActivityManager_Update`
    (776/1200) and `0084ed20 Engine::Update` (775/1200) Verify mismatches at two `RiderCharacter` words (+0x70 `0` vs `1`, +0x10
    `0x170001ff` vs `0x170101ff`) — the pre-existing residue cleanup2 documents (`builds/handlers2/verify_other_quick.log`, QUEUE
    "chain round 4" neighbours); not from that round's ports (the same words appear with only the rx_race or only the rx_loop ports on
    the 06:37 snapshot). Object localized (scenariofix): RiderCharacter 0x0095881c, its playing flag and the +0x90 base's +0x10 bit
    0x10000. The original does not start anything in the manager update: the riders are set up at the boot's event start and the core
    *cleared* them every Pre frame (`005681e0`'s `0074b4f0` call was outside the `intro == 0` branch; disassembly `00568220 jne
    0056827d`). Fixed in `core/game/race_activity_stub.c`; QUEUE "### riderchar", race.md "riderchar". 0 mismatches and On identical on
    all five worlds for rx_race; rx_loop keeps only its documented words. (b) **FIXED (scenariofix):
    `0074cf70 rx_ScenarioScript_Run` `execution outside mapped code 95e90000`.** `006924a0 rx_fx_triggered_effect_start` used
    `rx_vcall(U32(e, 0x90), ...)` (the object's secondary vtable pointer) instead of `lea ecx,[eax+0x90]`; the vtable slot +0x14 read
    0x95e90000 from the thunk bytes at 0x0069b214. Now 0074cf70 3040/0 (race worlds) and 250/0 in the baton boot; rx_anim On is identical
    on all five worlds. (c) **FIXED (scenariofix): `007473f0`'s 2-dword cast matrix.** The original's `00749ab6` stores the pose's
    `y + 0.24` **after** `push ecx`, i.e. into the frame's +0x64 (the y slot); `rx_race_intro_place_chiefs` stored it at +0x68 (z). One
    line fixed (`SF32(E, 0x64, ...)`); 007473f0 780/0 and the boot 64/0.
15. **Online presence / session clock: the PCSession boundary (2026-10-20, presencefix).** `rx_activity_presence` (0055cf10) calls the
    session's slot 12 (`[0x00da6cb0]`, the PCSession that MXGame::Startup 00756e10 builds through 006de390) to upload presence / stats:
    target `PCSession::vf12` 006e9e00 (SteamUser / SteamMatchmaking presence record, then `NetworkSession::vf12` 006e3a90 = Quazal
    presence: 00890e60 / 00891400 / 00895290), both gated on the online-session word `+0x1c4c`, so offline both return without touching
    game state. The product has no PCSession (the offline stand-in is `[0x00da6cb0] = 0`): `stats_set` runs the original in the oracle
    and skips it in the product when the session is that stand-in; the activity's local presence clocks (+0xfec / +0xff0) are still
    written. The same block's session clock `006decd0` (`rx_fx_session_clock`, Quazal ref handle around `[0xd60d54]`, 0088bcd0 /
    00703070 / 0088bac0 / 006fbfe0) is also a no-op with the null session object in the product; the oracle runs the original. Nothing
    gameplay reads either result. Verified by the product's full 1200-frame run (state 2), the next product stop is the camera snap
    00649df0 (a loop/camera unit port, not a boundary); the oracle acceptance is unchanged (`rx_race` / `rx_loop` / `rx_world` quick:
    only the documented 0055f700 words, On identical). Details: plan/QUEUE.md "presencefix".
16. **The product's integer dispatch table and macro registrations (2026-10-20, stuntaudio + fnsig; no behavioural difference).** `rx_call_original` in
    the product can only call the functions `tools/gen_fnsig.py` lists; at the time of the stop the scan required a literal `RX_FUNCTION(0x...)`
    plus the function definition, so registrations written through macros were invisible (`build_vcall_thunks.c`'s 19 event vcall thunks; also
    `build_event_proxy_equals.c`, `build_scene_start.c` `START_CTOR`, `race_ai_params.c` `AI_PARAM`). The symptom was the event delegate
    `00592b60` (`ActivityAudioLogic` vtable slot 6 = `vf6` 005818c0) aborting as "unported" on the 12-rider clean ride; the thunk was registered,
    only the table entry was missing. Fixed first by writing the 19 thunks and their `RX_FUNCTION` lines literally (same bodies). **The fnsig unit
    fixed the class of gap**: `gen_fnsig.py` preprocesses every game file with `-DRX_FNSIG_SCAN` (rx_fn.h turns `RX_FUNCTION` into a marker), so
    macro families land in the table automatically (3996 registrations, 3453 integer entries, 543 genuine non-integer / definition-less skips),
    and `builds/fnsig/fnsig_check.c` proves at runtime that the integer + skip lists partition the registry. The handler itself is
    **not** a boundary: `rx_audio_driver_cheer` advances the game RNG (0x9cb860 / 0x9cb864) and the crowd state, so the product runs the port; the
    only sound-engine boundary in it is the existing `AudioEvent_Play` queue (`rx_world_event_play`, item 8). Oracle unchanged (product-only file):
    rx_race / rx_loop / rx_world quick 0 mismatches, On identical; per-call probe 005818c0 9/9/0. Details: plan/QUEUE.md
    "stuntaudio" and "fnsig", race.md "The stunt listener".


7. **Vehicle damage registration (0047bb90 / 00479a00, w-ctx 2026-10-26): msvcp90 stack residue and the first stream use.** (a) The part descriptions are parsed by a
   `std::istringstream` in the original; the core reads the tokens itself and only reproduces the stream's heap blocks. The 16-byte vectors the original copies from its parse
   frame (the part's offset, its boxes' centres) carry a fourth lane the parse never writes, and the boxes' name strings copy their 16-byte buffers past the terminator: those
   words hold the residue of msvcp90's frames in the original (heap pointers, stack addresses) and whatever the stack held in the core. Verify of 0047bb90 / 00479a00 on a live
   world (probe case bind): 26-28 words per call, all of these two kinds (the lifted port Fn_00479a00 has the same words); nothing reads them. (b) msvcp90 initialises its numeric
   facets lazily at the process' first extraction from a stream: that call allocates and keeps a few blocks (8, 24, 1, 6, 5, 1 bytes ...), so in a baton Verify boot the very first
   registration of the process shifts every later allocation of the original by 0x90 against the core. After that first call the heap states agree (the probe cases, run on worlds
   past the first registration, are exact).

17. **Vehicle damage parts moved into another world (0047a460 / 0047bc20, w-partdtor 2026-10-27): keyed-record padding.** A part's new body is built
   by 0041cd90 -> 007697e0, which assembles the 0xe0-byte keyed record in its frame and pushes a copy into the world's keyed vector (007738e0 ->
   00774740). The record's padding bytes (+0x88..+0x8f, +0xb3, +0xbc..+0xbf, +0xd2..+0xd4: exactly the bytes the compiler-generated assignment
   00417c80 does not copy, so nothing reads them) are never written by the original's initialisers and keep its frame's residue (the
   0x92492493 divide-by-0xe0 products of the callers' bounds checks); the core's 007697e0 writes them as 0. Verify of 0047a460 / 0047bc20 (probe
   case partmove): the move into the UI world (1 of the 3 calls per world: tt / buggy / UTV), 29 / 19 / 29 words, all at these offsets (the
   lifted Fn_0047bc20 has the same words); the moves within the event world and back, and the first registration (00479ef0), are exact.
   (w-residue2: still open; re-measured on a UTV baton world (PX_Laguna, REFLEX_GARAGE=4,1,3,4,45,11, probe case partmove): 0047bc20 4 calls /
   1 mismatch, 29 words of the records in the UI world's vector. Reproducing them means 007697e0 leaving the record's padding unwritten on
   a frame whose earlier contents are the original's (the callers' frames up from 0047bc20 / 0041cd90 at the original's addresses).)

18. **The career series boot's main-thread dead stack (w-residue2 2026-10-27; reported, not reproduced).** The boot's memory hash /
   REFLEX_MEMDUMP (unlike verify.sh's world tests, which leave the world runtime's stacks out) include the boot's main-thread stack
   0x0022xxxx, so the series 41 boot's On check (w-heat ports + rx_career + 0075c430) shows dead words below the main thread's final ESP
   (0x0022ff04) that no code reads (every heap / data word matches; Verify of every port 0 mismatches but the crt_undname call):
   `rx_HeatPostEvent` (MXSeriesEventHeat::vf13 005adde0) alone: 369 words 0x0022f094..0x0022fc94 (the original's last writer is the
   standings' 0xa0-byte record vector push_back 004f0be0 -> 00507410 -> 00418800 -> ReflexAlignedAlloc -> malloc / HeapAlloc frames,
   reached from the standings update 005b43c0 / 005b6460 in vf13's subtree; the core's career standings code fills the tables without
   those frames); `rx_HeatAdvance` (vf12 005adb00) alone: 41 words 0x0022e524..0x0022e5c4 (the results UI's time text "--:--.--",
   UITextComponent::vf35 005cc670 -> 005cc480 (alloca 0x1004) -> 005cc4f0 -> 005e05a0 -> 005e0e10 -> 0078ffb0, ran 8 bytes lower in
   the original; the virtual clock is the same at the end (REFLEX_BOOT_CLOCK=1: 118574400 in both runs, baton scheduling advances it
   per QueryPerformanceCounter read, not by instructions, and REFLEX_FIXED_CLOCK=1 gives the same 41 words), so it is not the clock
   (w-vfactory's guess) but a frame shape of the core's vf12 path seen through the UI code that runs later at that depth; not
   attributed further); `rx_SeriesPlayerTable` 005b6840 alone: 49 words. Reproducing them means the career table code's frames
   (allocations with ReflexAlignedAlloc's / malloc's frames, the UI calls' depths); not done: dead stack of the boot's main thread only.
