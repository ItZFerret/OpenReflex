# STOP_AUDIT: where the product can still stop (agent w-audit, 2026-10-26; dynamic evidence re-run by w-matrix after the Freestyle / Section Race / challenge / teardown merges)

Audit of every place where librxcore (no RX_ORACLE) can still stop beyond the literal-address census (`census_original_calls.py`). Tools: `core/tools/stop_audit.py` (lists the sites), `core/tools/stop_audit_md.py` (this file's classification rules + generator), `core/tools/stop_audit_run.c` + `stop_audit_matrix.py` (the product run once per event the shell offers, with `--life` the whole life of each: RACING, WindDown, Post with the replay host, teardown, session end) + `stop_audit_twice.c` (two events in one process); a development build adds fuzz, trick, camera and end-of-race drivers; build it with `core/tools/stop_audit_trace.sh <core build dir> <out dir>`).

## Counts

| kind | SAFE | OUT-OF-SCOPE | NEEDS-PORT | total |
|---|---|---|---|---|
| indirect | 230 | 0 | 0 | 230 |
| abort | 353 | 137 | 0 | 490 |
| fatal | 424 | 0 | 0 | 424 |
| all | 1007 | 137 | 0 | 1144 |

`indirect` = rx_call_original(_ex) with a non-literal target; `abort` = act_unreached / `<prefix>_unreached` / "path not in the reference boot"; `fatal` = a third kind found during the audit: a bare `abort()` (often after `fprintf(stderr, "rxcore: ... not ported")`, "Unclosed ... dependency", ABI-frame checks). The census of the brief counts 183 + 192; this tool counts 218 + 349 because it also lists the helper definitions' call sites in headers (`*_util.h`), macro-free wrappers and every `unreached()`-style helper (build_tables_rdb.c, build_registry.c). Sites inside `#ifdef RX_ORACLE` are skipped.

## NEEDS-PORT (most likely to be met in normal play first)

| # | what | reached by | addresses to port (size) | evidence |
|---|---|---|---|---|

### Closed in the last passes (agents w-stunts, w-replay, w-teardown, w-freestyle, w-section, w-challenge and w-matrix, 2026-10-26)

| # | what | ported | verification |
|---|---|---|---|
| 9 | Macro-hidden physics, rider, camera, vehicle and race targets (the census could not see the WORLD_CTX / pl_call / act_call_fn / orig_regs / tracked_call / tree_insert forms) | w-hidden: 00769f40 00768650 00417070 00774c30 0061db10 006217a0 00621850 006514e0 00646e80 00476340; w-ctx: 0076dec0 0076e6c0 (kinematic push contact kernels), 00460070, 00548960 (+ 00548850 005488b0), 0047a970 0047a880 0047ba80 00691c30 00697b10 006971f0 (vehicle damage hits), 0047aff0 00681750 (the part spring), 0047bb90 00479a00 00479ce0 00479ef0 00479680 0047ae00 0041cb60 0041cd90 007697e0 007dda20 007ddfa0 00847c80 00849080 (the registration of the trucks' / buggy's / UTV's detachable parts), 00512c80 0053ac80, 00623060, 007d2c80, the four std::map rotations 00500480 004ffe80 00765c90 00765ce0, 00892120 | core/oracle/wave_hidden_probe.cpp and wave_ctx_probe.cpp (core/plan/QUEUE.md w-hidden / w-ctx): per call, memory and registers, 0 mismatches; the rest of the extended census (core/tools/census_original_calls.py) is boundary:audio / online / ui / not-reached with the evidence at each call |
| 7 | Freestyle, Section Race and the Free Ride challenges (HillClimb, Trailblazer, TargetPractice, BigAir, Pathfinder, StepUp): the activity factory 0055ef98 and their classes | w-freestyle (110 functions: MXActivityFreestyle, the stub, FreestyleDriver 0050c900 chain, FMX scenarios, rules; race_activity_freestyle.c, race_freestyle_driver.c, race_freestyle_rules.c, anim_scenario_fmx.c), w-section (the 19 vtable functions of MXActivitySectionRace 0056a210, SectionRaceDriver 00512db0 chain, SectionRaceZone; race_section.c, race_section_driver.c), w-challenge (race_challenge*.c: MXActivityChallenge 0057aa30 and the HillClimb / Trailblazer / TargetPractice / BigAir / Pathfinder / StepUp classes, the grid's start zones 0075f9fc..) | QUEUE.md sections of the three agents (probe cases and Verify per call, 0 mismatches); product: M1 / E3 (2026-10-26, w-matrix): 24 Freestyle, 30 Section Race and 17 challenge rows (+ InCareerMode) run to RACING, WindDown and Post with the replay host, are torn down and ended; 14 two-event sessions |
| 10 | Free Ride Post (Activity::vf29 00559a00, the base class's Post update) and the teardown of the challenge areas (ScrollingTextureEntity 00410bc0 + 00410c10 of the HillClimb areas, MXTargetPracticeCollectable 0040e550 of the TargetPractice areas) - found by w-matrix's lifecycle matrix | 00559a00: race_champ.c rx_activity_vf29 (shared body with the heat's 0057c010); 00410bc0 / 00410c10 / 0040e550: anim_entity_dtors.c | oracle: race_champ_cases.cpp activity_vf29 / _results / _results_main: 3 Verify calls, 0 mismatches; dtor_probe case o (ids 0x1078 / 0x106e, vf0(0), vf0(1), vf9, vf2): 00410bc0 2/2, 00410c10 2/2, 0040e550 2/2, 0 mismatches; verify.sh rx_anim and rx_race on the 5 reference worlds: 0 mismatches, On identical; product: the 40 Free Ride / challenge rows complete on the product objects, 28 of 28 sequential events end |
| 1 | StuntLanded / StuntHold / StuntTakeoff listeners | 0054d310 (+ 00621fe0), 0054d1a0, 0054d1e0, 0054d360 (+ 006223e0): loop_player.c, anim_rider_play.c | core/oracle/stunt_events_probe.cpp: 6 ports x 60 Verify calls on the Concord player, 0 mismatches |
| 2 | PerformanceAnimator additive clip | 0083e930: anim_animators.c rx_performance_additive_clip | core/oracle/performance_clip_probe.cpp (ATV world, garage 0,0,-1,8,2,4): 114 Verify calls with random poses, weights 1e-9 .. 1.5, 0 mismatches |
| 6 | Free Ride with AI riders | 0052aac0 + states 0052ae10 .. 0052b410, 0052ab80, 0052b0a0, 0051dff0, 0051e040, 0078fcf0 (race_beh2_perform_stunt.c); 0052fcf0 + 0052efa0 00530dc0 00530590 0052f0e0 0052fca0 0052f4a0 0052f990 0052f510 00530740 (race_beh2_roam_world.c, with 00538a60 now returning t); 00555630 | FreeRide_AI_Cheyenne (6 riders, 5 AI): 60000 + 21170 Verify calls of the two updates, probes perform_stunt_probe / roam_world_probe drive every state: 0 mismatches; On identical |
| 8 | Kinematic handle list sort > 32 entries | 00777580 00777c70 00778190 00777d20 00777d60 00778200 00778660: world_body.c | core/oracle/body_sort_probe.cpp: 600 cases (n = 2 .. 600, six orderings, ideal n / 40 / 0 / -1), 0 mismatches |
| 3 | Replay at the end of a race without a replay host (w-replay) | no new function: the exhibition replay presenter (rx_session_replay_begin_event / _end_event, api/replay_session.inc; Unity ReplayWorker.BeginEventWorker, OpenReflexSimulation replayPresentation, OriginalWorldRuntime.TickExhibitionReplay -> the career's OwnReplayRenderer) for every event of the open front end; 00757660's product branch in rx_replay_require_viewport now runs the viewport list port and the split-flag write (render targets / screen effects stay the renderer's) | headless E2 (five event types reach Post 2 and play the reel, product objects, 0 unported calls); Unity: run.py --arcade (arcade National, natural 1-lap finish, replay + results); career flow re-run |
| 5 | Activity / vehicle / rider destruction (career 'Continue', next event): the collision shapes and the scene entities (w-teardown) | phys_shape_dtors.c: 00847770 008455a0 00845570 00844970 00844860 00844a90 (+00844c50) 00844f70 (+00844f90) 00847c60 (+00847d50) 00848ec0 (+00819e40) 00849040 (+0042b280) 00849830, the shape release at every shape-reference site (build_collision.c, build_scene_props.c, build_scene_forest.c, build_vehicle_proxy.c, build_rider_ik.c); phys_static_objects.c: 00773a60 007747f0 00774d10 00773ad0 007772d0 00773e10 007735e0 00773db0 008218c0 0081ff30 0081fe40 0081f690 (+ 00845010 shape bounds); anim_entity_dtors.c: 00406ff0 00401fd0 00793930 00407110 (+004072c0) 00407130 00409970 0063d560 (+0063e6a0) 00648dd0 (+00648df0) 00692190 00696580 (+006965a0) 006ba5f0 006bbff0 006bd520 (+006bd570) 00819000 (+00819050) 00819100; FreeRideActivityAudioLogic vf30 00582260 / vf4 005823b0 (+0058b130); 0084dd30 (the scene's name table erase: the core passed the key slot's address as the string, so an entity's name was never erased). BOUNDARIES (documented in QUEUE.md): the entity classes outside_core in build_scene.c (LocalizedPackageReferenceEntity 00416950 / 00416c30 / 00416d70, the activity audio manager entities 0068d9d0 / 0068dcc0 / 0068dce0) are never built by the product; the audio registries [0xa95824] / [0xa95828] (AudioCollisionManager::vf1 0067b920, VehicleAudioManager::vf1 00680e80, 00680e50) are null in the product; the UI event-handler proxies 005ef970 / 005effa0 / 0060edb0 / 006104e0 belong to UI pages that the product never creates. 006de4b0 (the Network channel) was already ported (c94bf55) and runs at session create | core/oracle/dtor_probe.cpp (shapes unpinned; cases a v r s d c h f), Verify, 0 mismatches: FreeRide_AI_Cheyenne, WaypointRace_FR_Gibson, FreeRide_FR_Alpine (activity vf3 + vf6 + vf0; 86 area entities in FR_CheyenneFreeRide), National_NAT_Concord and the other four reference worlds (race area vf3 + vf6 + vf0); shapes: 639 shapes in 9 classes of the registry x flags 0 / 1 x lock 0 / 1, StaticRays / Triangles / Composite / StaticComposite / DynamicComposite made up in guest memory |

Not stops (checked): the finish scenario / podium (RaceFinishScenario, 0074a1e0 / 0074a340) is ported and runs in the product (w-podium; Post sub-state 1); the 24 scenario-script event handlers: only SCMG_LoseWalk (the finish lose-walk) uses an unported one (type 3); every ordinary race, free ride and waypoint event of the shell (all 404 runs of the event matrix and the 72 fuzz rides, all vehicle profiles) reaches RACING.

## Method and limits

* Static: every site read in context; indirect targets from the vtable (`_re/out/vtables.tsv`, `stop_audit.py --slot 0xNN`) intersected with the core's registrations (`gen_fnsig.py --report`: 4819 registered, 4152 callable through `rx_call_original`; **`rx_call_original_ex` is always fatal in the product**); helpers that take the target as a parameter were resolved through their callers (this found the literal targets 0083e930, 006b2fb0, 0083a7c0, 0047a880, 0053f030 ... that the census cannot see); callbacks stored in event proxies were resolved through their registration calls.
* Dynamic: `stop_audit_run` drives the real `rx_session_*` API. (1) The event matrix (M1, `stop_audit_matrix.py --life 200 --career`): 420 runs, every EventTypeLocations row of MXTables_Default (+ the 6 DLC levels) x every vehicle class of the type + the 17 shipped challenge rows + Free Ride / the challenges with InCareerMode, each run to RACING, forced into WindDown, Post with the replay host, torn down and ended; 412 reach RACING (the other 8 end at once in the original too), 420 complete. Two events in one process: `stop_audit_twice` (E3). (2) A development build of librxcore whose `rx_call_original(_ex)` logs `RXTRACE-UNPORTED fn=... caller_off=...` and returns instead of aborting, used for: fuzzed rides (72 x 12000 frames, F1), a sweep of the 36 default aerial tricks (MX / ATV / UTV), the four camera presets, 49 four-wheel runs (ATV, UTV, super buggy, ProLite, Pro2 profiles from builds/sol_core_features/vehicle-profiles), forced WindDown / Post (the whole matrix), AI rosters of 2-16 riders, and class censuses (415 live vtables per race).
* Limits: no career or Unity-side flow was run by the audit itself (those use the host contract rows; w-replay ran the replay host headless (E2) and the Unity arcade probe; the career's own Moto 1 -> Moto 2 transition is tests/moto_career_flow, Unity); no rider finished a lap race by itself (WindDown / Post were forced with the activity's own SetState; Freestyle, Section Race, HillClimb and TargetPractice were also run to their own natural end, M1); the second event of a session is a new session (A1): the teardown of E3 is the activity's vf3 / vf6 / vf0 called by a development hook, not by the game's own EndEvent path (which the API never runs); no random ride entered an activation area (the move / drive tests do); tricks start through the game's own 006c98c0 but the gesture machine is not driven; virtual calls through wrappers with a variable receiver were classified by receiver family, not call by call (appendix lists the residual unported targets of the constructible in-scope classes).

* w-online5 (2026-10-30, by hand): the changed core files have no audited site (`stop_audit.py`: the same 693 lines before and after):
  `game/net_remote.c` (the mid-event join / leave and the remote tuning: no act_unreached / abort / indirect call of its own),
  `api/net_remote_session.inc` (the join and the tuning run under the session's fault guard: a fault is a session error, not a stop),
  `game/vehicle_utv_pose.c` (a guarded sqrt, no site). The join reaches, from a new caller, sites of other files that a mid-event
  remote player does not meet: 0075c44c (the session's static initialiser: run at boot), 0075c3c1 (the viewports with the second scene:
  a remote player has no viewport), 0055bb9a (no free race index: the id is the race index, 1 .. 11), 0055bbfe (an online activity: the
  product's session word is 0), 0054976d (a new slot's player with an activity). Exercised headless (`net_remote4w_test latejoin`, six
  vehicle families, joins / leaves / a rejoin) and in the oracle (`net_join_probe.cpp`) without a stop.

## Evidence keys

* **S1** w-sfx (2026-10-27): sound_state.c vget() calls a virtual getter only when rx_fn(target) says the core has it (a registered port); an unregistered target is never called (the query counts a miss and returns its default), so no unported call can be reached from it
* **S2** the series manager 005b2370 is constructed by the front-end world's FreeRide Pre (lldb backtrace rx_activity_can_start -> rx_fx_series_manager) before any event; every other first use constructs it lazily
* **O1** PlayerMode names 0 SplitScreen / 1 SinglePlayer / 2 Online / 3 LAN (0x9dd584); rx_event_spec.player_mode = "SinglePlayer" (race_misc_game.c rx_game_player_mode)
* **N1** offline the Quazal session objects are absent: [0xda6cb0]+0x1c4c == 0, the lock [0xda6d08] == 0, driver +0xef0 == 0 (QUEUE.md w-net section)
* **I1** invariant: a std::length_error / checked-iterator / index / allocation failure of the MSVC library code the original also terminates on; not a game state
* **A1** the session API runs one event per session and forces the MXGame state to 2 then 3 (api/rx_session.c:517,614); Unity restarts by creating a new session (OriginalWorldRuntime.cs:1076 Restart -> Create); no API reaches states 0/1 or the UI-world switch; states 9 / 10 are reached only by the Heat series' activity-area re-creation between its rounds (rx_session_career_loading_enter -> 005a8030 -> Game_StartAAACreation 0075a380, w-heat)
* **H1** career composition: the API installs every dependency before a career event starts (rx_career_session_start_owned, api/rx_session.c:729; career_composition.c); only a host that rejects a request reaches the abort; the series admitted by cp_series (api/career_public_session.inc) are the finite SingleEvent set + Moto14 + the Heat rows 41-46 (w-heat)
* **H2** replay host bridge: installed by ReplayWorker for career launches and, since w-replay (2026-10-26), for every exhibition event of the open front end (rx_session_replay_begin_event before rx_session_event_load; OpenReflexSimulation replayPresentation, OriginalWorldRuntime.TickExhibitionReplay); the vtable checks only fail for the abstract bases (_purecall 008f9fd6), every concrete vehicle / camera / simulation class registers the slot
* **R1** startup-only: the registry / Engine.bxml / MXTables loaders run once at session create from the install's fixed files; session create succeeds on the installed data (every run of the matrix)
* **R2** schema: the installed tables use column types int / string / float only (MXTables_Default + DLC: 376 / 56 / 20 columns); the aborts are for column kinds 0 / 5 / 6
* **V1** invariant of the vehicle damage registration (world_veh_damage_register.c): the vectors are reserved for the box count and the part entity is new; verified per call by the w-ctx probe cases splitpart / parse / extract on the live UTV, buggy and truck entries (0 mismatches)
* **D1** developer debug keys / debug flag 0xae1b90 (not reachable by the shell: the registry's AI-override and debug keys)
* **M1** dynamic, re-run 2026-10-26 on main 115312f (agent w-matrix; core/tools/stop_audit_matrix.py --life 200 --career, 5 jobs): the whole life of every event the Unity shell offers: every EventTypeLocations row of MXTables_Default + the 6 DLC levels x every vehicle class of the type (National 70, Supercross 70, Powercross 24, Terracross 84, Waypoint 78, Freestyle 24, Section Race 30, Free Ride 3) + the Free Ride challenges on all 17 shipped rows (HillClimb / Trailblazer tracks 0-2 on FR_Alpine and FR_Cheyenne, track 0 on FR_Gibson, TargetPractice x 3) + Free Ride and the challenges again with the UI property InCareerMode (rx_session_event_in_career_mode): 420 runs, each loaded, run to RACING, forced into WindDown 200 frames later (the activity's own SetState; 450 frames of Post with the replay host installed and the driver acting as the renderer, STOP_AUDIT_REPLAY=1), the activity torn down (E3) and the session destroyed. Result: 412 reach RACING (the other 8 are the HillClimb tracks 1 / 2 of FR_Alpine / FR_Cheyenne, plain and with InCareerMode, which end in Post at frame 69 before RACING: the ORIGINAL does the same, race state 4 at the world start of a baton boot, builds/w-matrix/hcworld.sh), 420 reach WindDown / Post, 420 complete (RESULT ok, session destroyed). The first pass of the day (development library, nothing aborts) stopped 18 runs in three places, all fixed by w-matrix (00559a00 Free Ride Post, 00410bc0 / 0040e550 teardown of the HillClimb / TargetPractice areas); the 40 Free Ride / challenge rows were re-run on the product objects (rx_call_original aborts) with the fixes: 40 of 40 complete. 22 natural finishes (no forced state, 17000 frames cap): Freestyle 8 and Section Race 5 end at their time limit and play the replay, TargetPractice 3 and HillClimb 3 at theirs, Trailblazer 3 runs 17000 frames without finishing (it ends at its finish trigger or off the boundary)
* **F1** dynamic, re-run 2026-10-26 (w-matrix; development library, an unported call is logged and the ride goes on): 72 fuzzed rides of 12000 frames (random steering / brake / trick keys / camera / reset: wrecks, resets, wheelies; throttle held most of the time) with MX, ATV, UTV, super buggy and the two truck profiles (factory types 0 / 1 / 7 / 5 / 3 / 3): National, Supercross, Powercross, Terracross, Waypoint and Free Ride (36 rides) ride the 12000 frames; Freestyle and Section Race (12 rides, no replay host) finish by themselves at their 60 s limit (frame 4158-4721) and stop at Post in 006ab6d0, the known host requirement (H2); the same 12 with the replay host (the driver acts as the renderer) play the reel to frame 12000; Free Ride with InCareerMode (6) and HillClimb track 1 / Trailblazer track 2 / TargetPractice with InCareerMode (MX + ATV, 6; the activation areas polled each frame, no random ride entered one: the move / drive tests of E3 do): 0 unported calls, 0 stops
* **T1** dynamic: every default aerial trick (36) started on an airborne rider through the game's own 006c98c0 (MX, ATV, UTV profiles); the unported targets it reached are listed in NEEDS-PORT
* **E1** dynamic: forced WindDown / Post (rxtrace_set_state 3 / 4) on National, Supercross, Powercross, Terracross, Waypoint with the clip recorder off: no stop; with the recorder on (the default) the replay bridge stopped (closed by w-replay, E2)
* **E2** w-replay (2026-10-26): the same forced WindDown (state 3 at race frame 1500; the activity's own WindDown timeout enters Post) with the exhibition replay presenter installed (stop_audit_run STOP_AUDIT_REPLAY=1, acting as the renderer: every request applied and painted), on the product objects (rx_call_original aborts) and on the trace build: National / Supercross / Powercross / Terracross reach Post sub-state 2 at frame 1681, Waypoint at 2011, the replay reel plays to the end of the run (4329 / 3999 frames with the replay camera valid, 14-20 clips, 0 unported calls) and the session stays healthy; without the presenter National stops at frame 1680 as before. Unity: tests/moto_career_flow run.py --arcade (arcade National from the event browser, natural 1-lap finish, the results over the playing replay, its requests painted and drained, RETURN TO MENU with the final save): PASS
* **E3** w-matrix (2026-10-26): teardown and the next event. The session API runs one event per session (A1); the 2nd event of a 'Continue' is a new session in the same process. core/tools/stop_audit_twice.c runs two events in one process: create, load, race, the live activity's teardown (vf3, vf6(0), vf0(1): the development hook rxtrace_teardown, the sequence of oracle/dtor_probe.cpp case a), rx_session_destroy, create, load, race, teardown, destroy: 14 pairs (National -> Freestyle -> Section Race -> HillClimb -> Trailblazer -> TargetPractice -> Waypoint -> Terracross -> Free Ride -> Supercross -> Powercross -> National, Freestyle x 2, Section Race x 2, HillClimb x 2) on the product objects: 28 of 28 events end with the destroy and the next build starts; the same teardown runs after every one of the 420 matrix runs (E3 column TEARDOWN / ENDED). activation_native_test (move, HillClimb / Trailblazer / TargetPractice): Free Ride InCareerMode -> the area entered (rx_session_activation) -> the challenge as a new session -> back to Free Ride, PASS. Found and fixed: the area entities ScrollingTextureEntity::sdtor 00410bc0 (+ 00410c10) of the HillClimb areas and MXTargetPracticeCollectable::sdtor 0040e550 of the TargetPractice areas (w-teardown's class census covered the Free Ride / Waypoint area subscenes only, not the challenge areas)
* **TD1** w-testride (2026-10-27): the garage's TEST DRIVE (api/test_drive_session.inc, game/garage_test_drive.c) runs on a session whose MXGarage (MXGame + 0x15570) was never loaded: its vehicle fields +0x524 / +0x528 are 0 (core/tools/test_drive_test.c: start, ride, end on MX and ATV, no stop); the oracle boot REFLEX_TEST_DRIVE reaches the original 007611b0 only through the original garage's own vehicle
* **B1** documented product boundary (UI / audio / network / render): the product skips it and nothing the game reads depends on it (core/plan/QUEUE.md; comment at the call)
* **C1** career admission: only SingleEvent series (+ Moto14 and the authored Supercross 33 / Powercross 68 / Waypoint) are offered (cp_series supported flag); championship / heat / series activity types >= 0x1f never start in a session
* **X1** scene / asset data of the installed levels: every level of the shell loads (matrix) so the unbuilt-class aborts do not fire; the entity types named never occur in an activity area of an installed level
* **L1** family query (stop_audit.py --slot, _re/out/vtables.tsv): every concrete class of the receiver family that the core constructs registers the slot; only the abstract bases (_purecall stub 008f9fd6) are unregistered
* **Z1** all literal targets passed through this helper are registered (checked by resolving each caller's argument)
* **W1** w-collide (2026-10-26): every generator of the 0085ddd0 case table is a core port (the last eight verified by oracle/phys_pair_probe2.cpp, 0 mismatches); the observed dispatch runs every pair role through the table (phys_contact_pair_observed). The four-wheel rides of the vehicle matrix (UTV / super buggy / ProLite / Pro2 on TRX_Verde, TRX_Halifax, PX_Laguna and TRX_Lynchburg x 3 fuzz seeds, 9000 race frames) reach RACING to the end with no stop and no unported call; the Lynchburg rides run the two former stop roles 0086e750 @ 0085e4ce and 0086f3d0 @ 0085eb79
* **P1** w-partdtor (2026-10-27): the vehicle damage parts' destruction (0047bbe0 -> 00479dd0 -> PhysicsEntity::sdtor 00403bb0 / 0041c7b0 -> 00402360 -> 00416fe0 / 00417830 / 00417c80 / 0076a9a0 / 0076aa50, Database vf22 0084ce00, the shape handle vf1 00849630) verified per call by core/oracle/wave_ctx_probe.cpp cases release / relentry / vehclear / partmove / keyedrm on truck / buggy / UTV worlds; headless stop_audit_run STOP_AUDIT_TEARDOWN=1 STOP_AUDIT_FREE_PLAYERS=1 (Powercross UTV / super buggy / trucks, 4 riders): the players freed, every entry released, 0 unported calls
* **N2** w-npcs (2026-10-27): the stands crowd sections (BillboardCrowdEntity 0x1041) are built by the core since build_scene_crowd.c; verified per call in baton boots of SX_Kingston (66 sections) by core/oracle/build_scene_crowd.cpp (ctor, InitFlashes: 0 mismatches), the destructor by dtor_probe case b (72 live sections of SX_Springfield deleted, 72 verified, 0 mismatches), vf5 against the original's live objects by core/oracle/crowd_probe.cpp (SX_Kingston 66, NAT_Concord 20, FMX_BuenaVista 120 entities: every non-pointer word identical)
* **G1** w-teardown (2026-10-26): core/oracle/dtor_probe.cpp with the collision shapes UNPINNED runs vf3 / vf6 / vf0 of the live activity (Free Ride, Waypoint, race), the vehicle, rider, scenario and camera entities in the five worlds + FreeRide_AI_Cheyenne / WaypointRace_FR_Gibson / FreeRide_FR_Alpine under Verify, 0 mismatches; the REFLEX_ORIG_CENSUS minus the probe's own calls lists only the boundaries documented in QUEUE.md (audio registries [0xa95824] / [0xa95828] vf1, UI event-handler proxies, the entity classes outside_core in build_scene.c); the shape classes of the world's registry plus made-up StaticRays / Triangles / Composite / StaticComposite / DynamicComposite objects are deleted with flags 0 / 1 and the allocator lock flag 0 / 1 (shape_dtors: 0 mismatches)
* **Q1** the product's results page start (race_results_table.c rx_results_page_show, w-resultsrng / w-wpfill / w-partyresults) admits the activity types whose filler is ported: the race types 0 / 1 / 0x11 / 0x13 (the Eliminator's place column 005dc7a0 included), Freestyle 5, Waypoint 6, Section Race 0xc and since w-partyresults the party modes Tag 9 (00614dc0), Snake 10 (00614f00) and Infection 0xe (00615070), offline and as the online framework's host / client (the session's Quazal flag is never set); the original's page Lua calls App.PopulateResultsTable for Tag the same 15 UI ticks after "Show Activity Results" as for a race (results_probe trace, w-partyresults); PokerRace 0xd (no filler) is not admitted: after its "Show Activity Results" the post-event overlay's handler 005f42e0 never runs, so the original never fills its table (measured: Post sub-state 2 reached, no page start); EggHunt's filler 006151a0 is ported and verified by probe, the type is not admitted (no EggHunt runs: R32); every 00614890 type now has its port, so the dispatcher has no stop left; other types fill no cells

## Rule notes (the reasoning behind a row's rule id)

| rule | class | evidence | note | addresses |
|---|---|---|---|---|
| R01 | SAFE | S1 | the getter is called only for a registered target (rx_fn); otherwise the default is returned |  |
| R02 | SAFE | T1 | the engine emitters' start-line tunables (00684c10's StartLine keys): the configuration's vf1 find 008241f0, vf17 floats 00824640 and vf19 ints 008245c0 are ported and registered (build_registry.c); the fallback never runs |  |
| R03 | SAFE | T1 Z1 | PerformanceAnimator::vf4 (0083e290) applies the up to 4 additive clips (0083e930, PORTED 2026-10-26 by w-stunts: anim_animators.c rx_performance_additive_clip, 114 + 16 Verify calls, 0 mismatches) and 4 override clips (0083eff0); the remaining rx_call_original_ex in performance_apply_clip is the override clip, which is unreachable: no performance asset has override=true (scan of all 259 perfdat assets) | 0083eff0 (1448 B) not reached |
| R04 | SAFE | T1 | pl_run is the always-fatal rx_call_original_ex helper of loop_player.c; rx_delegate_invoke (00554fc0, the shared vf2 of the player's MX event-handler proxies) reaches it through pl_call only for a proxy whose member function is not ported. The player's stunt-channel listeners registered at build_activity_player.c:207-220 are ALL ported now (2026-10-26, w-stunts: StuntLanded 0054d310, StuntHoldBegin 0054d1a0, StuntHoldEnd 0054d1e0, StuntTakeoff 0054d360 in loop_player.c with 00621fe0 / 006223e0 in anim_rider_play.c; stunt_events_probe: 6 ports x 60 Verify calls, 0 mismatches). The other pl_run users are literal ported targets |  |
| R05 | SAFE | L1 M1 F1 | event-handler proxies of the activity channel call the member function stored at proxy+0x10 through rx_call_original (registered ones work); the callbacks registered and NOT ported are: Network* handlers 0055c970 0055cdf0 0055ce10 0054b950 0054b9d0 0054bb90 0054ff30 0050eaa0 00512990 (online), the Waypoint audio-cue handlers 005834f0 00583520 (WaypointActivityAudioLogic::vf3 subscribes them only when the activity's audio map holds the cue), the race cue handlers 00582d70 (RaceActivityAudioLogic's 'Restart', subscribed by every race audio logic's vf29 00582ed0 and raised by Activity::vf16 00557b40: the pause menu's RESTART, rx_session_restart_activity -> 00759d00 -> 0055f670; ported with ActivityAudioLogic::vf32's cue map walk 00581077) 00582e00 00582e20 00582e60 00583a70 00583fb0 are ported (race_activity_audio.c, w-sfx), VehicleWrecked/VehicleStopWreck driver handlers 0050c8a0 0050c8d0 (FreeRide drivers; no raiser in the core) and 005234f0 (BehaviorFixForwardProgress; wrecks occur in every fuzz run without a stop), 'Reset Ghost Overlays' thunk 006137b0 (EnteredTimedSection 00569550 is ported since: race_outrun.c), save-game handlers 00754d10 / 00754430 (Storage channel: never raised), the player stunt listeners (NEEDS-PORT row, loop_player.c pl_run) |  |
| R06 | OUT-OF-SCOPE | O1 N1 | the networked section race (NetSectionRace link, its teardown, results and section times): online only; the offline section race has no network link ([0xda6cb0]+0x1c4c == 0) |  |
| R08 | OUT-OF-SCOPE | O1 N1 | the table k_net_types lists every instantiation the Network manager's listen functions (007071a0 .. ) call; the manager exists only for an online session |  |
| R09 | OUT-OF-SCOPE | O1 N1 | the documented online boundary helpers: fatal in the product by design (their header names the guard that keeps each caller away); the census lists their targets as boundary:online |  |
| R10 | SAFE | B1 | the slot (rider sound bank +0xa70 .. , 00680d37) is assigned only by the vehicle foley hook 00680b10, which build_activity_spawn.c calls under RX_ORACLE only (the audio boundary: no audio system exists in the product, world_util.h AUDIO_BOUNDARY) |  |
| R100 | SAFE | Z1 L1 | literal 007d5e70 / 007d60f0 / 007da4d0..007da740 (registered); ticker slot 0x4c/0x54 and light vf6 (0x18): registered |  |
| R101 | SAFE | L1 F1 M1 | RavenSimulation* virtual slots 3-34 (the five concrete simulations register all; only the abstract RavenSimulation stub 008f9fd6 is unregistered), the input manager's pad action objects (XenonInputManager vf4 0x10 / vf10 0x28 / vf45 0xb4 are unregistered: vf45 is used only in the session-pause block that never runs offline, vf4/vf10 are not called with these receivers) and RavenVehicle vf29/vf34/vf38 (registered) |  |
| R103 | SAFE | Z1 | literals 0085c6d0 / 0085cb70 / 0085c700 / 0085cf80 / 0085d300 (mechanism node slots): registered |  |
| R104 | SAFE | L1 M1 Z1 | build-unit helper `RX_FN(slot) else rx_call_original`: Activity vf1/vf50 (0x58 / 0xd0), Database vf5/vf6/vf8/vf11/vf14/vf27 (0x14 0x18 0x20 0x2c 0x38 0x6c), stream vf1/2/4 (4 8 0x10), allocator vf1/2, scene / vehicle / rider / XMLReader slots. Registered for the concrete classes; Activity::vf6 00557a10 (83 B) and the activity destructors only in the activity-removal path (teardown row). Literal targets passed to act_call_fn / orig_regs by non-oracle callers: 0047a880 (the damage manager's part reset: ported, world_veh_damage_hit.c, w-ctx), 0078fcf0 (cheat-string test, mode 0 only; ported), MXGame init-thread steps 004dc0a0 / 0053c350 / 0053c3d0 (state 0/1, never run), session-pause block 00548ad0 / 00548b40 / 005ee840 / 0075a2b0 / 0047bc20 (ported) ([0xda6734] is 0 offline, set once to 0 at load), 00779440 Lua jobs (not run: product boundary), 0075cfb0 (online only), 007616d0 (UI manager only) |  |
| R106 | SAFE | L1 F1 M1 | game-loop helpers `loop_tcN(loop_vslot(obj, off))`: KeyMap / XenonInputManager / camera (MX*Camera) / entity / vehicle / director virtuals. Unregistered candidates in constructible classes: MXFirstPersonCamera::vf30 00641dd0 (322 B, called from Player_UpdateCameras only when player state +0x988 == 4 and the camera is not the gameplay camera: not reached by the camera presets or the C-key cycle in the camera sweep), MXStartGateCamera::vf32 00648390 (581 B, start-gate camera, only with a StartGate camera entity; not built in the product), XenonInputManager vf4/vf10 (not called with these receivers), DebugKeyMap vf4 (debug); loop_input_suspend.c (w-tutcore): KeyMap vf9 / vf10 (006d7840 / 006d7860, CompoundKeyMap 00811c50 / 00811c90) and XenonInputManager vf45 / vf48 (00815170 / 00815290), all registered |  |
| R107 | SAFE | Z1 B1 | CRT / OS import slot (0x00916224 _invalid_parameter_noinfo, 0x009162bc _stricmp, 0x009161b4 / 0x00916268 / 0x00916438 / 0x00916448 kernel imports) called through rx_call_original: the import resolves to a registered core CRT function or the online-content poll (boundary) |  |
| R108 | SAFE | Z1 I1 | msvcr90 internals (0x785xxxxx) behind the ported CRT: reached only for NaN / Inf / out-of-range math arguments (floor/ceil of NaN, pow domain errors, sin of huge values), string-length errors (_Xlen/_Xran), a locale change, _vsnprintf formats the core does not implement, and invalid-parameter paths; none occurs in a finite-state ride (F1: 13 runs x 12000 frames) |  |
| R109 | SAFE | L1 F1 M1 | helper `RX_FN(target) else rx_call_original`: the target is a virtual slot of the receiver families listed in the appendix or a literal passed by the callers (resolved: all registered except the rows above) |  |
| R11 | SAFE | A1 | 007d19f0 without a subscene name is the whole scene's unload of the engine's shutdown / level swap; the only call in the core passes the activity's subscene name (build_activity_teardown.c, +0x118) and the session ends by rx_session_destroy, not by a scene unload (A1) |  |
| R111 | SAFE | I1 W1 | pair-kernel table 0085ddd0: every generator is a core port (pair_kernel); the remaining aborts are the closed table's default, the observed role check (a (target, return site, argument count) that is not a case of the original's switch) and the observed dispatcher's entry-frame contract; they fire only on a call shape the original never produces. The former stops (rows R88 / R89 of the 2026-10-26 audit): the eight generators 00864460 / 00864dd0 / 008650c0 / 0086b360 / 0086f150 / 0086f3d0 / 00872970 / 00872c90 are ported (phys_pair_circle.c, phys_pair_rays.c, phys_pair_query.c) and the observed dispatch is general |  |
| R112 | SAFE | F1 I1 W1 | four-wheel / collision native closure: ABI frame checks and 'unclosed dependency / role' contracts of the exact native kernels (each role compared with the original's return site, ESP, EBP, register values): they fire only on a call shape the closure was not written for; the 63 runs of the four-wheel vehicle matrix (UTV, buggy, ProLite, Pro2 trucks, ATV on Terracross / Powercross / Waypoint / FreeRide / National / Supercross) cover the closure; its pair roles: W1 |  |
| R113 | SAFE | M1 X1 I1 | asset / scene / model / vehicle construction: an asset kind, model version, collision geometry kind, scene entity class or XML feature the installed data does not use, or an allocation / length error. Every level and vehicle of the shell loads (matrix, vehicle matrix), so these do not fire for installed content |  |
| R114 | SAFE | I1 F1 | frame / ESP / register contract of an exact transliteration; fires only on a call shape the original never produces |  |
| R115 | SAFE | I1 | frame / ABI contract or 'not ported' construction branch |  |
| R116 | SAFE | L1 F1 M1 | the finish scenarios' tail call of their own vf3 (RaceFinishScenario::vf0 0074a1e0 `jmp [vtable + 0xc]`): the receiver is the scenario whose vf0 runs, so the target is RaceFinishScenario::vf3 0074a110 (rx_finish_select_next, registered); w-podium |  |
| R117 | SAFE | TD1 | the garage's own vehicle (007611b0): the product's test drive has no garage loaded, so the guard is never true |  |
| R12 | SAFE | V1 | the composite's vectors are reserved for exactly the box count the loop runs to (008494a0 / 008496d0 / 005ba080 at construction), so push_back never reaches the reallocating branch |  |
| R13 | SAFE | V1 | a part entity is new: its tracking entry +0x158 is 0 when the body is attached, and the entity itself holds the entry's reference from then on, so neither release can reach zero |  |
| R14 | SAFE | R1 | the payout script reads tables of the install's MXTables_Default (loaded at session create, R1); a table missing there would stop the session before any event |  |
| R15 | SAFE | S1 | guard bit clear: the unreached branch is the original's lazy initialisation |  |
| R16 | SAFE | S2 | guard 0x9e275c bit clear |  |
| R17 | OUT-OF-SCOPE | O1 N1 | online / LAN session or a Quazal network object (PlayerMode 2 / 3, [0xda6cb0]+0x1c4c, lock 0xda6d08, driver +0xef0) |  |
| R18 | SAFE | I1 | checked-iterator / length_error / index / allocation invariant of the MSVC library code |  |
| R19 | SAFE | X1 N2 | BillboardCrowdEntity::vf5 006c6180 reads the "Crowd" shader asset of the level (006c62b6 dereferences its handle: the original crashes without it); every installed level whose scene holds a BillboardCrowdRSData has it in its packages (core/tools/ambient_census.c over the 41 race levels: every crowd section built, 0 aborts) |  |
| R20 | SAFE | R1 | XMLBuilder / registry / table-XML loader branch the installed Engine.bxml registry files and MXTables do not take |  |
| R21 | SAFE | R2 | table column kind 0 / 5 / 6 / variant type 5 (no installed table has one) |  |
| R22 | SAFE | R1 I1 | table / index / row-set operation path (hinted insert fallback, erase of the end iterator, copy of a non-empty row set, unique lookup of a non-string column ...) that the reference boot and every matrix run (profile, Player tables, activity table) never take; STL-level invariants |  |
| R23 | SAFE | B1 R1 | startup / audio / save-slot boundary: no name, no track, no saved slot exists at session create (the product starts without audio and with the Unity-side profile) |  |
| R24 | SAFE | A1 B1 | MXGame_Update states 0 / 1 (init thread), 9 / 10 (event restart), the UI world switch and the Lua / InputNames boundaries: the API sets the state to 2 then 3 itself and restarts by a new session; InputNames_Init and the Lua VM are product boundaries (lead decision) |  |
| R25 | SAFE | A1 | StartEvent while the game views a track (debug level list): the API never views a track |  |
| R26 | SAFE | B1 | the rumble effect manager [0xd611d0] is never created in the product (nothing writes it): the condition is false; rumble requests reach Unity through rx_session_rumble |  |
| R27 | SAFE | A1 D1 | start zones of ActivityMode 6 (Overtake) / 3 (Ghost): the shell and the career always set the mode property to Exhibition (MXUIManager::SetupEvent default "Exhibition", career_properties.c has no ActivityMode key, 005b6ab0 sets "Exhibition") |  |
| R28 | OUT-OF-SCOPE | M1 O1 | start-zone assignment by activity type: the challenge types 0x15-0x1e are ported (race_challenge_*.c, w-challenge; HillClimb / Trailblazer / TargetPractice run in the matrix); the mini games 9 10 0xe 0xf (0075ff6e: Tag, Snake, Infection, EggHunt) since w-partymodes (2026-10-30); still unported: Capture 0x10 (0075fd87): the Capture party mode has no level data (no MiniGameCTF scene in any installed level) and the factory has no class for it (the next row) |  |
| R30 | SAFE | M1 X1 | the level's "<level>Base" subscene has no TO_StartingGate entity: every race level of the shell has one (all 420 matrix runs reach RACING or Post on all 41 levels); only levels without a gate (none installed) abort |  |
| R31 | SAFE | A1 D1 | 00564c90 is the start grid of ActivityMode 0 (Ambient) / 2 (Practice): the shell and the career always set the mode property to Exhibition (MXUIManager::SetupEvent default "Exhibition", career_properties.c has no ActivityMode key; 005b6ab0 sets "Exhibition") |  |
| R32 | OUT-OF-SCOPE | M1 O1 C1 | the activity factory 0055ef80 now builds race (0-4 7 0x11-0x13), waypoint (6), free ride (8), Freestyle (5 0x14 and the FMX champ intro / practice / main 0x20 0x3a-0x3c), Section Race (0xc), the challenges (0x15 0x16 0x18 0x19 0x1b 0x1d; 0x17 0x1a 0x1c 0x1e make no class) and the championship types 0x1f-0x39 0x21 0x30 0x31 (2026-10-26: w-freestyle, w-section, w-challenge, w-champ); and since w-partymodes (2026-10-30) Tag (9: race_tag*.c, MXActivityTag 0056d550, reached from the Tag EventTypeLocations rows FR_Alpine / FR_Cheyenne / FR_Gibson; the online framework's party modes) and PokerRace (0xd); since w-snake (2026-10-30) Snake (10: race_snake*.c, MXActivitySnake 0056b050, the Snake rows FR_Alpine / FR_Cheyenne / FR_Gibson) and Infection (0xe: race_infection.c, MXActivityInfection 00572d00); and since w-partylast (2026-10-30) Graffiti (0xb: race_graffiti.c, MXActivityGraffiti 00563d90), EggHunt (0xf: race_egghunt.c, MXActivityEggHunt 00574730) and Capture (0x10: race_capture.c, MXActivityCapture 00576290): no installed level holds their scene entities, so the session refuses those events before they load (api/rx_session.c minigame_content_missing, rx_session_event_load 3: R126); the default branch aborts only for types above 0x3c |  |
| R35 | SAFE | C1 | RaceChampIntro (type 0x1f): a series event, not admitted |  |
| R37 | SAFE | M1 | an event-type instantiation (template function address) not in build_activity_events.inc: the table lists every instantiation the exe's activity / driver constructors use (all matrix runs build their activities) |  |
| R39 | SAFE | I1 | 12 race indices; at most 12 riders (EventRiders <= 12 for the shell) |  |
| R40 | SAFE | M1 X1 | free ride start slots: the three FR_ levels (Alpine, Cheyenne, Gibson) have >= 2 entities of type 0x1014 in their FreeRide section (3 matrix runs); the challenge sections ("<level>ChallengeHillClimbA" ...) run in the matrix (17 shipped rows, M1) |  |
| R43 | SAFE | X1 M1 | scene entities of type 0x1053 (animation variant) / 0x1062 (TrackSystem) inside the activity area's section: none in any installed level (matrix: all levels start) |  |
| R44 | SAFE | N1 D1 | session replay recorder ([0xda6cb0]+0x1c48, networked) / SpectatorMode result (A+0xfe8, property 0 in the product) |  |
| R45 | SAFE | A1 | MXGame states 9 / 10 (the Heat series' activity area re-created between its rounds, w-heat) with the quit request +0x15b58 set: the core only ever clears +0x15b58 (build_mxgame.c, build_activity_game.c); its writer, the original's quit to the UI world, is not called by the session API (one event per session; Unity leaves an event by destroying the session) |  |
| R46 | SAFE | H1 | state 10 of the Heat re-creation: the UI property NeedSeriesAdvanceEvent is written only by the original's Lua pages; the product's owned property store has no writer of it (no core / api source names the key 0x0094d994 except this read), so the series event's vf9 gate path runs (career_series_test 41-46, the Unity Heat flow) |  |
| R47 | SAFE | R1 | XenonShaderAsset::vf2 00794720 on an unset guard [0x00da67f4]: the session's startup registration (build_asset.c rx_database_register_all, at session create) sets every static AssetIO's guard, the shader's included, before any asset exists (w-heat) |  |
| R49 | SAFE | A1 | re-initialising a player that still has an activity (P+0x94): one event per session; a new session starts from empty player slots |  |
| R50 | SAFE | I1 | player lookup of an empty slot: every caller iterates the active count. Reproduced headless with EventRiders 16 (session stops at frame 0): the game caps the field at 12 (MaxRidersEventType / MaxRidersVehicleClass tables; the shell never offers more); riders 2-12 run (AI rosters 2, 12 checked) |  |
| R51 | SAFE | B1 | the name plate text object (P+0xaa8) is never created in the product (render only); in every matrix run the branch is skipped |  |
| R52 | SAFE | A1 | second scene view active: split-screen only (PlayerMode 0) |  |
| R53 | SAFE | B1 | vf32's only core callers are the audio logics' constructors (race 005825a0, Free Ride, Freestyle 005623b0), which clear +0x8c just before; the cue map is built later, by vf29 (build_activity_audio.c, w-sfx), and the restart calls vf33 (race_restart.c), not vf32: the branch is not taken |  |
| R54 | SAFE | M1 X1 | a requested subscene must exist in the level's scene: every level of the shell loads (matrix) |  |
| R55 | SAFE | M1 X1 | the world's racing line (006a2980/00767850) only when a track entity named after the activity's track has a path (+0x98): none in the installed levels (matrix: all start) |  |
| R56 | SAFE | M1 X1 | temp-lappacing.bxml / "<level>-lappacing" asset: the level's lap-pacing asset exists for every level (matrix) |  |
| R57 | SAFE | I1 | .tdf07 suffix table index: kinds 0-14 only |  |
| R58 | SAFE | I1 | helper arity: all callers pass <= 5 arguments |  |
| R59 | SAFE | I1 | helper arity of the Database virtuals |  |
| R60 | SAFE | M1 | the level's scene file / the front end's Base subscenes exist for every level (matrix; UIScene in the startup) |  |
| R63 | SAFE | L1 | slot vf10 (0x28) / vf3 (0xc) of the finish scenario: all RaceFinishScenario / WaypointFinishScenario classes register it (FMXFinishScenario vf3 0074de60 included); the RaceFinishScenario (0074a1e0 / 0074a340) and its podium are ported and run in the product (anim_scenario_finish.c, w-podium: Post sub-state 1) |  |
| R64 | SAFE | I1 M1 | std::map subrange erase (00705150's loop) of the scenario cast map: the erased range is empty in every matrix run; an erase of a non-trivial range is the career teardown (see the teardown row) |  |
| R65 | SAFE | I1 | ABI check of the original's entered ESP |  |
| R68 | SAFE | G1 | guard for an unregistered records destructor (0042c080 is registered: build_vehicle_proxy.c). PORTED 2026-10-26 (w-teardown): a shape that loses its LAST reference in a dynamic / static record removal is deleted through its scalar deleting destructor: phys_shape_dtors.c 00847770 (base, DynamicRays, DynamicCylinder), 008455a0 (Box / Sphere / StaticBox / DynamicBox / StaticCylinder), 00845570, 00847c60 / 00848ec0 / 00849040 (Composite), 00844970 / 00844860 / 00844a90 / 00844f70, handle 00849830 (+ 00844c50, 00844f90, 00847d50, 00819e40, 0042b280) |  |
| R69 | SAFE | G1 P1 | guard for an unregistered records destructor (0042c080 is registered: build_vehicle_proxy.c): the keyed-object twin (00416fe0) of rx_dyn_records_remove, reached when a vehicle damage part entity is deleted (PhysicsEntity::sdtor 00403bb0 -> 00402360) |  |
| R73 | SAFE | D1 | the AI's freestyle riding (0050d240, 962 bytes) runs only with the AI debug override 0xae1b90 (the registry's AI benchmark switch, 0 in the game and the product); a Freestyle event has no AI riders otherwise |  |
| R76 | SAFE | H1 | career layer dependency or host-contract guard (unclosed dependency, rejected presentation request, authored-series check) |  |
| R77 | SAFE | E2 H2 | race end: rx_post_state(2) (00569a40, every race that reaches Post) -> clip_start_playback(8) -> rx_replay_start 006aa8d0 -> rx_replay_require_viewport / rx_replay_fade_in need the replay host (rx_replay_presentation_install). CLOSED by w-replay: the host is installed for career launches AND every exhibition event (rx_session_replay_begin_event, before the load); only a host that rejects a request reaches the abort. 00757660 (the viewport request): its viewport list part runs as the scene_viewports.c port and the engine timing's split flag [ctx+0x10]+0x5c (read by rx_fx_tracks_flush) is written; the render targets / screen effects (007575a0, 00757490) stay the renderer's |  |
| R78 | SAFE | H2 | replay host bridge / camera / vehicle / simulation vtable checks (abstract bases only) / scene view parameters |  |
| R79 | SAFE | H1 | Powercross countdown presentation component bound by the Unity host before Countdown Entered |  |
| R80 | SAFE | I1 B1 | viewport records are CPU-only in the product (no GPU owner); allocation failures |  |
| R81 | SAFE | C1 | waypoint championship intro / restart (activity types 0x21 / 0x30 / 0x31): a series event, not admitted (note: the port tests == 0x1f where the original tests >= 0x1f; types 0x21.. would silently continue) |  |
| R82 | OUT-OF-SCOPE | N1 | Waypoint driver network ownership / state serialisation |  |
| R83 | SAFE | L1 | Behavior vf4 (0x10) / Rule vf5 (0x14) update: the Behavior classes installed on waypoint drivers register it (unported: BehaviorRoamWorld / RoamTrack / LeaveActivity / PerformStunt / Pathfinder only on free-ride AI, see appendix) |  |
| R84 | SAFE | I1 | the original's IDIV fault (zero waypoints): a waypoint race has gates |  |
| R87 | SAFE | I1 | JobPool_Recover (00881d80) runs the jobs still queued when JobPool_WaitIdle stalls (every 11th round while jobs are pending); the serialized workers have always finished, so no job is pending (rx_jobpool_wait_idle), and a queued job is a core function (RX_FN) in any case |  |
| R88 | SAFE | L1 | AssetHandle vf2 (name, slot 8): every AssetHandle class registers it |  |
| R89 | SAFE | L1 F1 | Animator vf4 eval (0x10) / vf11 get-param (0x2c): all ten Animator classes register both (stop_audit.py --slot) |  |
| R90 | SAFE | L1 M1 F1 | entity / scenario / vehicle / light virtuals through slot `off` (callers: anim_scene.c Lights_Add and SceneManager_Add/RemoveEntity, scenario intro): the former residual targets PhysicsEntity::vf2 0041cbe0 (world_veh_entity.c) and the FMX scenarios' vf3 / vf6 0074de60 / 0074e140 (anim_scenario_finish.c) are ported since; the Entity destructors are ported (w-teardown: anim_entity_dtors.c) |  |
| R91 | SAFE | L1 | Animator vf3 (0xc) / vf4 (0x10) of the IK / Featherstone child animator: registered |  |
| R92 | SAFE | Z1 L1 | helper: literal targets 005ee840 (array_at, registered? no: 24 B leaf used by anim_ik_ent.c LeaderBoard::vf19 only) and virtual slot calls; see anim_ik_ent.c row |  |
| R94 | SAFE | L1 | AnimationEntity vf29 (0x74): registered for AnimationEntity / RiderAnimation |  |
| R95 | SAFE | L1 | KeyMap vf9 (0x24): the KeyMap classes the core builds register it (only KeyMapCommand base stubs are not) |  |
| R96 | SAFE | L1 | RavenSimulation vf33 (0x84): the five concrete simulations register it |  |
| R97 | SAFE | L1 X1 M1 | scenario intro machinery: Scenario vf1/vf2 (slots 4/8) of Race/Waypoint/Freeride intro scenarios, ScenarioAnimEntity vf15/vf19/vf28 (0x3c/0x4c/0x70), the Database `anim`/`bxml` request (vf27 0x6c), the XMLReader virtuals (0xe8 attribute, 0x110 string, 0x20/0x24/0x28 iteration, 0x130/0x134 values) and the script event handler table 0x9c5120. All registered for the classes built, except: handlers 2 ClearScenarioOrigin 0055d390, 3 ModelAnimation 0055d490, 12 CrewChiefAnimation 0055dbe0, 16 VehicleAnimation 0055dcb0, 23 PlaceVehicle 0055dd40 - a scan of all 41 scenario scripts of the install finds Event types 2/12/16/23 in none and type 3 only in SCMG_LoseWalk (the finish scenario's lose-walk, a documented boundary); the cast release's destructors (006b7720 / 00618c30) run only for cast entries a script placed (none in the product's intro) |  |
| R98 | SAFE | Z1 | literal 00621570 (registered) |  |
| R99 | SAFE | L1 | registry / child virtuals (slots 4 / 0x1c): registered |  |
| R118 | OUT-OF-SCOPE | O1 N1 | the networked Tag party mode (Quazal: the TagBall duplicated object +0xff380, the drivers' network objects +0xef0, the session's start zones, the networked teardown / spectator / round end): guarded by session_networked() / +0xff380 / +0xef0, all zero offline; the online framework's host runs the offline code and a client takes the host's state through party_net.c (w-partymodes) |  |
| R119 | OUT-OF-SCOPE | O1 N1 | the networked PokerRace (Quazal: the NetPokerRace duplicated object +0x13e8d0 that carries the community cards, the hands and the choices; the drivers' network objects +0xef0; the networked teardown / finish): guarded by session_networked() / +0x13e8d0 / +0xef0, all zero offline; the online framework's host runs the offline code and a client takes the host's cards through party_net.c (w-partymodes) |  |
| R120 | OUT-OF-SCOPE | O1 N1 | the networked Infection party mode (Quazal: the network snake object +0xff1e0 that carries the boundary's radius, the drivers' network objects +0xef0 with the infections and scores, the session's start zones, the networked round end): guarded by session_networked() / +0xff1e0 / +0xef0, all zero offline; the online framework's host runs the offline code and a client takes the host's infections through party_net.c (w-snake) |  |
| R121 | OUT-OF-SCOPE | O1 N1 | the networked Snake party mode (Quazal: the NetSnake object +0xff0c4 that carries the trails, the cuts and the boundary's radius; the drivers' network objects +0xef0; the session's start zones, the networked teardown / spectator / round end): guarded by session_networked() / +0xff0c4 / +0xef0, all zero offline; the online framework's host runs the offline code and a client takes the host's cuts through party_net.c (w-snake) |  |
| R122 | SAFE | I1 | an Infection round starts with no driver: the activity always has the local player's driver (vf44 builds one per player) |  |
| R123 | SAFE | M1 | a water boundary (UseWaterAsBoundary of the round's start zone): offline the zone is the activity row's MiniGameStartZone, one per Snake level (FR_Alpine / FR_Cheyenne / FR_Gibson); none has the flag (party_snake_test and stop_audit_run fuzz 20000 frames on each level run the shrink, w-snake) |  |
| R124 | SAFE | I1 | a trail shape other than the constant 0x9308cc: the trail constructor 0069e3a0 stores 0x9308cc (+0x100) and nothing writes another |  |
| R125 | OUT-OF-SCOPE | O1 N1 | the networked EggHunt / Graffiti / Capture party modes (Quazal: the NetEggHunt +0x100128 / NetGraffiti +0xff084 / NetCTF +0xff434 duplicated objects that carry the eggs' collections, the capture points and the flags' poses and scores; the drivers' network objects +0xef0; the session's start zones, teams, spectators and round ends): guarded by session_networked() / those objects / +0xef0, all zero offline; the online framework has no messages for these modes (w-partylast) |  |
| R126 | SAFE | I1 M1 | a mode's start where the original faults for missing scene content (EggHunt 0057492d: no "GreedModel"; Capture 005765f1 / 0057666e: no "RedFlag" / "BlueFlag"; Graffiti 00564151: no StartEntityFreestyle / StartEntityFreeRide, a division by zero): rx_session_event_load refuses those events first when the level's scene file lacks the entities (minigame_content_missing, return 3; party_minigames_test: every mode on FR_Alpine / FR_Cheyenne / FR_Gibson refused, the session then loads Tag); no other path builds these activity types (no EventTypeLocations row, the career and the online framework never name them) |  |
| R127 | SAFE | I1 | a GraffitiCapturePoint without its Graffiti activity (+0xb80), an activity without drivers, or a failed allocation of a handler proxy: the original reads a null pointer there (00564970 / the caller's +0xef0 / 0041927b); the points exist only in a level's MiniGameGraffiti subscene, which only a Graffiti event loads, and MXActivityGraffiti::vf2 links every point of its area before any trigger or stunt event reaches one, its drivers made at the activity's start (vf44) |  |

## Appendix: residual unported virtual targets of the constructible in-scope classes (primary vtables, classes whose vtable the core stores)

From `stop_audit.py --slot` over `_re/out/vtables.tsv`, slot offsets used by the core's virtual-call wrappers (940 call sites). Everything not listed is registered. Debug-only, Freestyle-only and online-only entries are marked.

| class | slot | function (size) | reached by | status |
|---|---|---|---|---|
| Activity / MXActivityRace / RaceStub / FreeRide | 0x0 sdtor | 00556640 (30) 00564af0 (36) 005659c0 (30) 0055fec0 (30) | activity removal (rx_activity_area_update), teardown | ported (w-stunts) |
| MXActivityFreeRide / MXActivityWaypointRace | 0xc vf3 | 00560080 (136) 00578b30 (208) | activity removal | ported (w-stunts) |
| Activity (all) | 0x18 vf6 | 00557a10 (83) | activity removal | ported |
| MXActivityRace / Waypoint | 0x38 vf14, 0xb4 vf45 | 005693d0 (382), 00567ac0 (67), 00557570 (496) | debug camera / spectate keys (race_driver_update.c debug_keys) | ported since (the debug keys stay OUT-OF-SCOPE) |
| MXActivityRace / Waypoint / FreeRide | 0x40 vf16, 0x50 vf20, 0x24 vf9, 0x74 vf29, 0x8c vf35, 0xb8 vf46, 0xe8 vf58 | 00557b40 (392) 00578cc0 (81) 0055d120 (284) 0057a3e0 (190) 0055eb70 (17) 00559a00 (64) 0057e720 (60) 00564f90 (152) 00559a80 (52) | no core call site applies these slots to activity objects (the same slot numbers are used on KeyMap / camera / entity receivers); 00559a00, 0057e720 hit only by a forced FreeRide WindDown | SAFE (all ported since except 00564f90) |
| RavenVehicle / Motocross | 0x0 sdtor | 0045d3a0 (30) 00462560 (107) | vehicle delete in teardown (build_activity_teardown.c:232/350) | ported (w-stunts, with vf9 00462960 and the simulation) |
| RiderAnimation / ScenarioAnimEntity / Entity (87 classes) | 0x0 sdtor | 00618c30 (71) 006b7720 (110) 00406ff0 (67) ... | deletes at teardown; scenario cast release only for placed cast entries | RiderAnimation / ScenarioAnimEntity ported (w-stunts); the other entity classes ported (w-teardown: anim_entity_dtors.c, item 5), except the outside_core classes the product never builds |
| PhysicsEntity bales (PopUp/PopDown/StepUp bar) | 0x8 vf2 | 0041cbe0 (114) | SceneManager_RemoveEntity of a bale (scripted PlaceEntity of a bale; no installed script) | ported since (world_veh_entity.c) |
| Animator classes (Blend, Additive, Switch, Motion, Performance, IK, Expression, Procedural, Featherstone) | 0x14 vf5 .. 0x24 vf9 | e.g. 006b5540 (422) 006b56f0 (127) 00833a40 (2023) 00834740 (625) 0083d490 (2642) 0083b470 (1557) | threaded / memory-store / debug animator interface; the core calls only vf3, vf4, vf10, vf11 | SAFE |
| BaseScenario family | 0x0 vf0, 0x4 vf1, 0xc vf3, 0x18 vf6 | RaceFinishScenario 0074a1e0 (344) 0074a340 (494); FMX*Scenario 0074d700 0074de60 0074dab0 0074dbc0 0074dcf0 0074df30 0074e050 0074e140 0074fae0 | Post sub-state 1 (the finish scenario and the podium) and the Freestyle FMX scenarios | ported (anim_scenario_finish.c: RaceFinishScenario w-podium, the FMX scenarios w-freestyle) |
| Behavior classes | 0x10 vf4 update | BehaviorLeaveActivity 005265a0 (457), PathfinderRoam 00536400 (95), RoamTrack 0052d0a0 (252); PerformStunt 0052aac0 and RoamWorld 0052fcf0 are ported (w-stunts) | installed on free-ride AI drivers only (rx_free_ride_vf47); mode 0 race cheat string (LeaveActivity) | NEEDS-PORT (API-only: EventRiders > 1 in a Free Ride) |
| Rule classes | 0x14 vf5 update | RuleCapture, Infection, ChallengeBoundary, InFreestyleCrowd, OutOfSnakeBounds, Offtrack (0051abc0 719 B) | RuleOfftrack is installed only by the PokerRace (ported: 0051ab70 / 0051aba0 / 0051abc0, race_poker.c) and RuleTag 0051bd70 (race_tag_driver.c), both w-partymodes; the others belong to mini-games / challenges | SAFE / challenge port |
| MXFirstPersonCamera | 0x78 vf30 | 00641dd0 (322) | Player_UpdateCameras when player state +0x988 == 4 and not the gameplay camera; not reached by the C-key cycle / four presets | SAFE |
| MXStartGateCamera | 0x80 vf32 | 00648390 (581) | start-gate camera entity: not built in the product | SAFE |
| XenonInputManager | 0x10 vf4, 0x28 vf10, 0xb4 vf45 | 008155f0 (26) 00815680 (8) 00815170 (51) | vf45 only in the session-pause block ([0xda6734] is 0 offline) | SAFE (00815170 ported since: loop_input_suspend.c) |
| Database | 0x18 vf6 | 0084cd30 (167) | build_activity_game.c:303 (state 3 path with an async load request on a path the product does not take) | SAFE |
| BaseDriver / NetworkDriver / FreeRideDriver | 0x10 vf4 | 00509330 (52) 0050bee0 (65) | debug camera keys; forced FreeRide WindDown | OUT-OF-SCOPE (debug) / SAFE (0050bee0 ported since: race_challenge_activity.c) |
| CollisionEntity, DropGate, PhysicsEntity ... | 0xc vf3 | 008b7c00 (7) | 7-byte stub (xor eax,eax; ret); world_vcall_f(... 0xc) is called on rider-attached entities only | SAFE |

## Hidden-targets table (`stop_audit.py --hidden`: literals the census cannot see)

| address | size | via | class | note |
|---|---|---|---|---|
| 00892db0 | 56 | world_wreck_misc.c:214 (WORLD_CTX) | OUT-OF-SCOPE | Quazal session clock (online) |
| 0053f030 | 242 | race_loader.c:50 (loader_finish) | SAFE | pending vehicle loader list empty (all runs) |
| 0053f130 | 242 | race_loader.c:55 (loader_finish) | SAFE | pending rider loader list empty (all runs) |
| 006b2fb0 | 9032 | anim_ik.c:770 (ik_call_ex) | SAFE | dead switch byte 0xd9d3e0 (nothing writes it) |
| 00779440 | 18 | build_activity_game.c:612 (orig_regs) | SAFE | Lua jobs: product boundary |
| 008077c0 | 205 | anim_ik_ent.c:223 (ik_call_ex) | SAFE | sound handle of the rider crash sound / spline sound events: the handle [A+0x2f0] is never set; SoundActions are an abort at load |
| 008078d0 | 160 | anim_ik_ent.c:213 (ik_call_ex) | SAFE | sound handle of the rider crash sound / spline sound events: the handle [A+0x2f0] is never set; SoundActions are an abort at load |
| 00807dc0 | 164 | anim_ik_ent.c:218 (ik_call_ex) | SAFE | sound handle of the rider crash sound / spline sound events: the handle [A+0x2f0] is never set; SoundActions are an abort at load |
| 0083a7c0 | 1777 | anim_ik.c:780 (ik_call_ex) | SAFE | IK memory-store state +0x2c never set |
| 0083eff0 | 1448 | anim_animators.c:631 (performance_apply_clip) | SAFE | override clips: no perfdat asset has override=true |
| 008b9940 | 103 | world_node_audio.c:759 (WORLD_ORIG) | SAFE | audio node / wreck-audio (FMOD) path: world_node_audio.c / world_wreck_audio.c run only for audio nodes (none are built: audio boundary) |

## Sites

| file:line | kind | function: condition / target | class | evidence (rule) | addresses to port |
|---|---|---|---|---|---|
| build_activity_driver.c:351 | abort | rx_network_driver_construct: the networked session's lock (00402420) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_driver.c:648 | abort | rx_activity_on_player_joined: a player joining an online activity | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_driver.c:711 | abort | rx_race_driver_vf1: the networked session's lock (00402420) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_event.c:482 | abort | rx_setup_event: the online / playlist event setup | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_event.c:731 | abort | rx_start_event: the quick online event | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_event.c:783 | abort | rx_start_event: the online profile event start (0059c950) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_event.c:790 | abort | rx_start_event: the online event start | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_game.c:271 | abort | rx_mxgame_update: an online level request | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_game.c:277 | abort | rx_mxgame_update: an online level load (0075f5a0) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_game.c:379 | abort | rx_mxgame_update: the online players' load state | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_game.c:391 | abort | rx_mxgame_update: the online race start | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_game.c:407 | abort | rx_mxgame_update: NetworkNoPlayers | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_game.c:777 | abort | rx_game_enter_real_world: the online world switch | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_grid.c:240 | abort | rx_game_update_load_wait: the online load wait | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_grid.c:289 | abort | rx_game_players_ready: the online players' readiness | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_grid.c:493 | abort | rx_game_grid_pass: the Capture start zones (jump table 0x7602f8[3]) | OUT-OF-SCOPE | M1 O1 (R28) |  |
| build_activity_player.c:408 | abort | rx_player_from_player_list: online player setup | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_pre.c:97 | abort | rx_session_pre_timer: the networked session's timer event | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_pre.c:142 | abort | rx_activity_vf37: the networked session's players (00585b20) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_activity_race.c:718 | abort | rx_activity_create: activity type %u\n | OUT-OF-SCOPE | M1 O1 C1 (R32) |  |
| build_activity_race.c:721 | abort | rx_activity_create: the activity array grown (007cff20) | OUT-OF-SCOPE | M1 O1 C1 (R32) |  |
| build_activity_start.c:234 | abort | rx_activity_vf2: an online activity's session handlers (006def80) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_driver_reset.c:139 | abort | rx_network_driver_vf3: the networked session's lock (00402420) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| build_mxgame_init.c:290 | abort | net_type: a Network event-type instantiation not in the table | OUT-OF-SCOPE | O1 N1 (R08) |  |
| career_property_bridge.c:9 | abort | rx_career_offline_session_metadata: career presence requires offline stand-in, online session unclosed | OUT-OF-SCOPE | O1 N1 (R17) |  |
| fx_world.c:513 | abort | rx_player_leave_spectate: the online spectator return | OUT-OF-SCOPE | O1 N1 (R17) |  |
| net_boundary.h:32 | abort | rx_net_call: rx_net_unreached(fn, what); | OUT-OF-SCOPE | O1 N1 (R09) |  |
| net_boundary.h:37 | abort | rx_net_call_ex: rx_net_unreached(fn, what); | OUT-OF-SCOPE | O1 N1 (R09) |  |
| race_activity_freestyle.c:728 | abort | rx_freestyle_stub_vf42: the Freestyle time limit of an online session (006de1c0) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| race_capture.c:367 | abort | rx_capture_score: ONLINE: the host sends the score (006f12d0) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_capture.c:417 | abort | rx_capture_vf2: ONLINE: the host's network CTF object (008879b0 / 00887780) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_capture.c:555 | abort | rx_capture_vf28: ONLINE: the network CTF object found or made (006e4fa0 / 008879b0) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_capture.c:557 | abort | rx_capture_vf28: ONLINE: the flags' poses sent or taken (the network CTF object) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_capture.c:580 | abort | rx_capture_vf38: ONLINE: a spectator rejoins the session's Capture | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_capture.c:624 | abort | rx_capture_vf44: ONLINE: the team of a session's player (its profile's +0x139) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_capture.c:817 | abort | rx_capture_driver_vf9: ONLINE: the carrier's flag event sent to the host (006f1190) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_capture.c:976 | abort | rx_capture_rule_vf5: ONLINE: the host's flag-return message (006f1230) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_driver_reset.c:131 | abort | rx_race_driver_vf3: the networked session's lock (00402420) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| race_driver_reset.c:135 | abort | rx_race_driver_vf3: the network player's state (0050bad0) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| race_egghunt.c:300 | abort | rx_egghunt_vf2: ONLINE: the host's network egg hunt object (008879b0 / 00887780) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_egghunt.c:315 | abort | rx_egghunt_vf3: ONLINE: the session's NetEggHunt objects unlinked (00585e10) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_egghunt.c:453 | abort | rx_egghunt_vf28: ONLINE: the network egg hunt object found or made (008879b0 / 006e4fa0) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_egghunt.c:472 | abort | rx_egghunt_vf28: ONLINE: an egg's collection sent to the host (006f4c50) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_egghunt.c:596 | abort | rx_egghunt_place_round: ONLINE: the session's start zone (006f7510) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_freestyle_driver.c:44 | abort | net_publish_scores: a Freestyle driver's network score report (online session only) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| race_freestyle_driver.c:114 | abort | rx_freestyle_driver_vf2: a Freestyle driver's network player state (online session only) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| race_freestyle_driver.c:330 | abort | rx_freestyle_driver_vf7: a remote Freestyle driver's network tallies (online session only) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| race_freestyle_driver.c:379 | abort | rx_freestyle_driver_vf14: a Freestyle driver's network tallies (online session only) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| race_freestyle_driver.c:393 | abort | rx_freestyle_driver_vf16: a Freestyle driver's network score report (online session only) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| race_graffiti.c:285 | abort | rx_graffiti_vf2: ONLINE: the host's network graffiti object (008879b0 / 00887780) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_graffiti.c:363 | abort | rx_graffiti_vf3: ONLINE: the session's NetGraffiti objects unlinked (00585bb0) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_graffiti.c:398 | abort | rx_graffiti_vf23: ONLINE: the network graffiti object found or made (006e4fa0 / 008879b0) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_graffiti.c:423 | abort | rx_graffiti_vf40: ONLINE: the session's round message | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_graffiti.c:453 | abort | rx_graffiti_vf42: ONLINE: the networked graffiti round end | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_graffiti_point.c:632 | abort | rx_graffiti_point_score: ONLINE: a capture sent to the host's NetGraffiti object (006f0690) | OUT-OF-SCOPE | O1 N1 (R125) |  |
| race_infection.c:233 | abort | rx_infection_vf2: ONLINE: the host's network snake object (008879b0 / 00887780) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:284 | abort | rx_infection_vf15: ONLINE: the round's first infected by network id | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:322 | abort | rx_infection_vf16: ONLINE: the session's start zone of a player (006f7330) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:353 | abort | rx_infection_vf23: ONLINE: the session's network snake object (006e4fa0 / 008879b0) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:355 | abort | rx_infection_vf23: ONLINE: the network snake object's update (00555540 / 005088c0) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:477 | abort | rx_infection_vf42: ONLINE: the networked infection round end | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:579 | abort | rx_infection_place_round: ONLINE: the session's start zone (006f7510) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:698 | abort | rx_infection_boundary_reset: ONLINE: the boundary's radius sent (the network snake object) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:716 | abort | rx_infection_boundary_shrink: ONLINE: the boundary's radius sent every 3 s | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:781 | abort | rx_infection_driver_vf1: ONLINE: the infection driver's score sent (0050bbd0 / 006fea10) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:798 | abort | rx_infection_driver_vf2: ONLINE: the infection driver's network link released (007006e0) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:823 | abort | rx_infection_driver_vf3: ONLINE: the infection driver's score sent (0050bbd0 / 006fea10) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:912 | abort | rx_infection_driver_vf7: ONLINE: the host's infection score (00700310 / 006fed10) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_infection.c:939 | abort | rx_infection_driver_vf9: ONLINE: the infection sent to the session's stations (006fd970) | OUT-OF-SCOPE | O1 N1 (R120) |  |
| race_net_session.c:33 | abort | quazal: the Quazal network layer (the product has no online session) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| race_poker.c:266 | abort | rx_poker_race_vf2: ONLINE: the host's NetPokerRace object (008879b0) | OUT-OF-SCOPE | O1 N1 (R119) |  |
| race_poker.c:278 | abort | rx_poker_race_vf3: ONLINE: the networked poker race teardown (the selection iterator loop) | OUT-OF-SCOPE | O1 N1 (R119) |  |
| race_poker.c:413 | abort | rx_poker_race_vf38: ONLINE: the flop sent (006f4310) | OUT-OF-SCOPE | O1 N1 (R119) |  |
| race_poker.c:427 | abort | rx_poker_race_vf42: ONLINE: a client's finished drivers | OUT-OF-SCOPE | O1 N1 (R119) |  |
| race_poker.c:556 | abort | rx_poker_lap_completed: ONLINE: the community cards sent (006f4310) | OUT-OF-SCOPE | O1 N1 (R119) |  |
| race_poker.c:574 | abort | rx_poker_lap_completed: ONLINE: the hand card sent (006f4390) | OUT-OF-SCOPE | O1 N1 (R119) |  |
| race_poker.c:581 | abort | rx_poker_lap_completed: ONLINE: the chosen card sent (006f4390) | OUT-OF-SCOPE | O1 N1 (R119) |  |
| race_poker.c:597 | abort | rx_poker_lap_completed: ONLINE: the offered cards sent (006f4400) | OUT-OF-SCOPE | O1 N1 (R119) |  |
| race_poker.c:631 | abort | rx_poker_driver_pick: ONLINE: the picked card sent (006f4390) | OUT-OF-SCOPE | O1 N1 (R119) |  |
| race_poker.c:750 | abort | rx_poker_race_vf28: ONLINE: the NetPokerRace object made (host) or found (client) | OUT-OF-SCOPE | O1 N1 (R119) |  |
| race_poker.c:902 | abort | rx_race_driver_vf16: ONLINE: the race driver's network state (0050bad0) | OUT-OF-SCOPE | O1 N1 (R17) |  |
| race_section.c:182 | abort | rx_section_race_vf2: ONLINE: the networked section race (NetSectionRace link) | OUT-OF-SCOPE | O1 N1 (R06) |  |
| race_section.c:216 | abort | rx_section_race_vf3: ONLINE: the networked section race teardown (the selection iterator loop) | OUT-OF-SCOPE | O1 N1 (R06) |  |
| race_section.c:294 | abort | rx_section_race_vf23: ONLINE: the networked section race's NetSectionRace link | OUT-OF-SCOPE | O1 N1 (R06) |  |
| race_section.c:365 | abort | rx_section_race_vf42: ONLINE: the networked section race's results | OUT-OF-SCOPE | O1 N1 (R06) |  |
| race_section_driver.c:228 | abort | rx_section_driver_entered: ONLINE: a networked section race's section time (NetSectionRace update) | OUT-OF-SCOPE | O1 N1 (R06) |  |
| race_snake.c:258 | abort | rx_snake_vf2: ONLINE: the host's NetSnake object (008879b0 / 00887780) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:278 | abort | rx_snake_vf3: ONLINE: the networked snake teardown (the selection iterator loop) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:376 | abort | rx_snake_vf16: ONLINE: the session's start zone of a player (006f7330) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:381 | abort | rx_snake_vf16: ONLINE: the NetSnake object's round reset (00884ba0) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:414 | abort | rx_snake_vf23: ONLINE: the session's NetSnake object (006e4fa0 / 008879b0) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:416 | abort | rx_snake_vf23: ONLINE: the NetSnake object's update (00555540 / 005088c0) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:483 | abort | rx_snake_vf27: ONLINE: a spectator brought back into the race (0055c800 / 006fe110 / 006f3400) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:600 | abort | rx_snake_vf42: ONLINE: the networked snake round end | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:639 | abort | rx_snake_vf42: ONLINE: the round end sent with the NetSnake object (006fda80) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:713 | abort | rx_snake_place_round: ONLINE: the session's start zone (006f7510) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:827 | abort | rx_snake_boundary_reset: ONLINE: the boundary's radius sent (the NetSnake object) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake.c:845 | abort | rx_snake_boundary_shrink: ONLINE: the boundary's radius sent every 3 s | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:167 | abort | rx_snake_driver_vf1: ONLINE: the snake driver's score sent (0050bb90 / 006fea10) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:187 | abort | rx_snake_driver_vf2: ONLINE: the snake driver's network link released (007006e0) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:238 | abort | rx_snake_driver_vf3: ONLINE: the snake driver's score sent (0050bb90 / 006fea10) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:393 | abort | rx_snake_driver_vf7: ONLINE: the host's snake values (00700310 / 006fed10) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:409 | abort | rx_snake_driver_vf7: ONLINE: the driver's network handle (004024c0) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:411 | abort | rx_snake_driver_vf7: ONLINE: the cut sent with the NetSnake object (006fd310) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:429 | abort | rx_snake_driver_vf7: ONLINE: the driver's network handle (004024c0) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:431 | abort | rx_snake_driver_vf7: ONLINE: the cut sent with the NetSnake object (006fd310) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:443 | abort | rx_snake_driver_vf7: ONLINE: the driver's network handle (004024c0) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:448 | abort | rx_snake_driver_vf7: ONLINE: the cut sent with the NetSnake object (006fd310) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_snake_driver.c:502 | abort | rx_snake_driver_vf16: ONLINE: the snake driver's network kind change (0050bb90) | OUT-OF-SCOPE | O1 N1 (R121) |  |
| race_tag.c:231 | abort | rx_tag_net_round_complete: ONLINE: a null network handle (00884390) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:252 | abort | rx_tag_ball_physics: ONLINE: a client's network ball (kinematic off, 0041ce60) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:314 | abort | rx_tag_place_round: ONLINE: the session's start zone (006f7510) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:466 | abort | rx_tag_vf2: ONLINE: the host's network TagBall object (008879b0) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:519 | abort | rx_tag_vf3: ONLINE: the networked tag teardown (the selection iterator loop) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:569 | abort | rx_tag_vf16: ONLINE: the session's start zone of a player (006f7330) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:619 | abort | rx_tag_vf18: ONLINE: the network TagBall object's reset | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:874 | abort | rx_tag_vf28: ONLINE: the network TagBall object found (host 008879b0 / client 006e4fa0) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:876 | abort | rx_tag_vf28: ONLINE: the network TagBall's holder and pose sync | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:954 | abort | rx_tag_vf27: ONLINE: a networked spectator rejoins the tag match | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:1037 | abort | rx_tag_vf42: ONLINE: the networked tag round end | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag.c:1080 | abort | rx_tag_vf42: ONLINE: the round's end sent (007001a0) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag_driver.c:95 | abort | rx_tag_driver_vf1: ONLINE: the tag driver's score sent (0050bb90 / 006fea10) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag_driver.c:108 | abort | rx_tag_driver_vf2: ONLINE: the tag driver's network link released (007006e0) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag_driver.c:146 | abort | rx_tag_driver_vf3: ONLINE: the tag driver's score sent (0050bb90 / 006fea10) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag_driver.c:245 | abort | rx_tag_driver_vf7: ONLINE: the host's tag time and score (00700310 / 006fed10) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag_driver.c:270 | abort | rx_tag_driver_vf9: ONLINE: the ball's network request for the holder (006ffdb0) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag_driver.c:274 | abort | rx_tag_driver_vf9: ONLINE: the ball's network request (006ffdb0) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag_driver.c:287 | abort | rx_tag_driver_vf16: ONLINE: the tag driver's network kind change (0050bb90) | OUT-OF-SCOPE | O1 N1 (R118) |  |
| race_tag_driver.c:399 | abort | rx_tag_rule_vf5: ONLINE: the host's grab check for a networked driver | OUT-OF-SCOPE | O1 N1 (R118) |  |
| waypoint_driver_advance.c:19 | abort | rx_waypoint_driver_vf9: Waypoint advancement network state serialization is not composed | OUT-OF-SCOPE | O1 N1 (R17) |  |
| waypoint_driver_advance.c:30 | abort | rx_waypoint_driver_vf9: Waypoint gate network notification is not composed | OUT-OF-SCOPE | N1 (R82) |  |
| waypoint_driver_advance.c:42 | abort | rx_waypoint_driver_vf9: Waypoint lap network notification is not composed | OUT-OF-SCOPE | N1 (R82) |  |
| waypoint_driver_events.c:22 | abort | remove_binding: Waypoint driver network state serialization is not composed | OUT-OF-SCOPE | O1 N1 (R17) |  |
| waypoint_driver_network.c:7 | abort | ?: NetworkDriver publish/ghost reset requires real network ownership | OUT-OF-SCOPE | O1 N1 (R17) |  |
| waypoint_driver_reset.c:23 | abort | rx_waypoint_driver_vf3: Waypoint reset network state serialization is not composed | OUT-OF-SCOPE | O1 N1 (R17) |  |
| waypoint_driver_update.c:11 | abort | rx_waypoint_driver_update_rules: Waypoint rule update network ownership is not composed | OUT-OF-SCOPE | O1 N1 (R17) |  |
| waypoint_driver_update.c:26 | abort | rx_waypoint_driver_vf7: Waypoint remote-state receive is not composed | OUT-OF-SCOPE | O1 N1 (R17) |  |
| activity_finish_stage.c:9 | abort | rx_activity_finish_stage_is_b: unported actual finish scenario vf10 tail target | SAFE | L1 (R63) |  |
| anim_animators.c:228 | indirect | rx_motion_animator_eval: `U32(U32(U32(self, 0x28), 0), 8)` | SAFE | L1 (R88) |  |
| anim_animators.c:511 | indirect | performance_apply_clip: `fn` | SAFE | T1 Z1 (R03) |  |
| anim_extern.h:94 | indirect | anim_animator_eval: `target <- U32(U32(animator, 0), 0x10)` | SAFE | L1 F1 (R89) |  |
| anim_extern.h:185 | indirect | anim_animator_get_param: `target <- U32(U32(animator, 0), 0x2c)` | SAFE | L1 F1 (R89) |  |
| anim_extern.h:227 | indirect | anim_vcall0: `target <- U32(U32(obj, 0), off)` | SAFE | L1 M1 F1 (R90) |  |
| anim_extern.h:233 | indirect | anim_vcall0_f: `target <- U32(U32(obj, 0), off)` | SAFE | L1 M1 F1 (R90) |  |
| anim_ik.h:43 | indirect | ik_call_ex: `fn` | SAFE | Z1 L1 (R92) |  |
| anim_ik.h:48 | indirect | ik_call: `fn` | SAFE | Z1 L1 (R92) |  |
| anim_ik.h:81 | indirect | ik_eval_child: `target <- U32(U32(animator, 0), 0x10)` | SAFE | L1 (R91) |  |
| anim_ik.h:129 | indirect | anim_set_rate_child: `target <- U32(U32(child, 0), 0xc)` | SAFE | L1 (R91) |  |
| anim_ik_anim.c:162 | indirect | rx_featherstone_animator_eval: `target <- U32(U32(child, 0), 0x10)` | SAFE | L1 (R91) |  |
| anim_rider_a.c:23 | indirect | anim_entity_hierarchy: `target <- U32(U32(self, 0), 0x74)` | SAFE | L1 (R94) |  |
| anim_rider_stunt.c:130 | indirect | keymap_vf9: `fn <- U32(U32(km, 0), 0x24)` | SAFE | L1 (R95) |  |
| anim_rider_stunt.c:294 | indirect | rx_rider_stunt_ground: `fn <- U32(U32(sim, 0), 0x84)` | SAFE | L1 (R96) |  |
| anim_scenario_finish.c:136 | indirect | tail_call_vf3: `target <- U32(U32(sc, 0), 0xc)` | SAFE | L1 F1 M1 (R116) |  |
| anim_scenario_fmx.c:208 | abort | rx_fmx_intro_vf0: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| anim_scenario_intro.c:157 | indirect | rx_scenario_play_on: `U32(U32(t, 0), 0x114)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:167 | indirect | rx_scenario_name_of: `U32(U32(t, 0), 0x110)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:217 | indirect | rx_scenario_anim_entity_set_animation: `target <- U32(U32(e, 0), 0x4c)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:235 | indirect | rx_scenario_anim_entity_play: `U32(U32(mgr, 0), 0x6c)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:238 | indirect | rx_scenario_anim_entity_play: `U32(U32(e, 0), 0x3c)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:320 | indirect | rx_scenario_load_intro_script: `U32(U32(mgr, 0), 0x6c)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:338 | indirect | rx_scenario_begin: `U32(U32(sc, 0), 4)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:350 | indirect | xml_attr: `U32(U32(p, 0), 0xe8)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:353 | indirect | xml_read: `U32(U32(p, 0), off)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:432 | indirect | rx_scenario_event_read: `U32(U32(p, 0), 0x110)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:441 | indirect | rx_scenario_event_read: `U32(U32(p, 0), 0x110)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:469 | indirect | rx_scenario_event_parse: `U32(U32(p, 0), 0x110)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:509 | indirect | rx_scenario_cast_load_script: `U32(U32(p, 0), 0x20)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:520 | indirect | rx_scenario_cast_load_script: `U32(U32(p, 0), 0x28)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:523 | indirect | rx_scenario_cast_load_script: `U32(U32(p, 0), 0x24)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:542 | indirect | rx_scenario_script_run: `h <- U32(0, 0x009c5120 + 4u * U32(ev, 0))` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:553 | indirect | rx_scenario_script_run: `h <- U32(0, 0x009c5120 + 4u * U32(ev, 0))` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:745 | indirect | rx_scenario_intro_update: `U32(U32(sc, 0), 8)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:760 | indirect | rx_scenario_intro_update: `U32(U32(in, 0), 4)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:780 | indirect | rx_scenario_intro_update: `target <- U32(U32(crowd, 0), 0x4c)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:1056 | indirect | rx_scenario_cast_release: `U32(U32(x, 0), 0)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:1091 | indirect | rx_scenario_cast_place_entity: `U32(U32(place, 0), 0x3c)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:1094 | indirect | rx_scenario_cast_place_entity: `U32(U32(w, 0), 0x50)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:1099 | indirect | rx_scenario_cast_place_entity: `U32(U32(veh, 0), 0x80)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:1111 | indirect | rx_scenario_cast_place_entity: `U32(U32(mgr, 0), 0x6c)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:1127 | indirect | rx_scenario_cast_place_entity: `fn <- U32(U32(e, 0), 0x40)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_intro.c:1180 | indirect | rx_rider_set_start_line_animation: `target <- U32(U32(e, 0), 0x4c)` | SAFE | L1 X1 M1 (R97) |  |
| anim_scenario_rider.c:108 | indirect | rx_rider_parts_set_float: `target <- U32(U32(c, 0), 0x1c)` | SAFE | L1 (R99) |  |
| anim_scenario_rider.c:256 | indirect | rx_rider_animation_stance2_outputs: `fn` | SAFE | Z1 (R98) |  |
| anim_scenario_rider.c:459 | indirect | rx_rider_character_release_animation: `target <- U32(U32(reg, 0), 4)` | SAFE | L1 (R99) |  |
| anim_scenario_rider.c:609 | fatal | rx_rider_list_grow_check: rxcore: list<T> too long (005c0900, std::length_error)\n | SAFE | I1 F1 (R114) |  |
| anim_scenario_rider.c:860 | abort | rx_stl_map_erase_range: a std::map subrange erase (00705150's iterator loop) | SAFE | I1 M1 (R64) |  |
| anim_scene.c:134 | indirect | entity_tick: `target <- U32(U32(e, 0), off)` | SAFE | Z1 L1 (R100) |  |
| anim_scene.c:656 | indirect | cell_grower: `fn` | SAFE | Z1 L1 (R100) |  |
| anim_scene.c:749 | indirect | light_grid_add: `fn` | SAFE | Z1 L1 (R100) |  |
| anim_scene_misc.c:124 | indirect | volume_bounds: `target <- U32(U32(v, 0), 0x18)` | SAFE | Z1 L1 (R100) |  |
| anim_scene_vec.c:68 | fatal | vec_reallocate_at: rxcore: 007f95a0: vector too long (std::length_error)\n | SAFE | I1 F1 (R114) |  |
| anim_scene_vec.c:75 | fatal | vec_reallocate_at: rxcore: 007f95a0: vector too long (std::length_error)\n | SAFE | I1 F1 (R114) |  |
| bike_response.c:31 | fatal | response_dependency: shared response dependency stack drift %08x\n | SAFE | I1 F1 (R114) |  |
| bike_response.c:377 | indirect | sim_vcall: `U32(U32(S, 0), slot_offset)` | SAFE | L1 F1 M1 (R101) |  |
| bike_sim.c:54 | indirect | bike_sim_vcall: `fn <- U32(vt, off)` | SAFE | L1 F1 M1 (R101) |  |
| bike_sim.c:430 | indirect | bike_moto_apply_pose_override: `fn <- U32(vt, 8)` | SAFE | L1 F1 M1 (R101) |  |
| bike_sim.c:544 | indirect | bike_sim_action_call: `fn <- U32(vt, off)` | SAFE | L1 F1 M1 (R101) |  |
| bike_sim.c:815 | indirect | bike_moto_apply_pending_tuning: `fn <- U32(vt, 0)` | SAFE | L1 F1 M1 (R101) |  |
| bike_sim.c:885 | indirect | bike_sim_im_call: `fn <- U32(vt, off)` | SAFE | L1 F1 M1 (R101) |  |
| bike_sim.c:1072 | indirect | bike_sim_mech_stdcall: `fn` | SAFE | Z1 (R103) |  |
| bike_slot13.c:393 | fatal | wheel_triangle_contact_kernel: if (end > 0x100000000ull // (sizes[i] && addresses[i] < entry && end > bottom)) abort(); | SAFE | I1 F1 (R114) |  |
| bike_slot13.c:515 | fatal | wheel_triangle_contact_kernel: if (cookie != U32(0x9b45e4, 0)) abort(); | SAFE | I1 F1 (R114) |  |
| bike_slot13.c:535 | fatal | wheel_triangle_contact_kernel: if (cookie != U32(0x9b45e4, 0)) abort(); | SAFE | I1 F1 (R114) |  |
| bike_vehicle.c:119 | indirect | bike_vehicle_post_step: `U32(U32(V, 0), 0x74)` | SAFE | L1 F1 M1 (R101) |  |
| body_native_dependencies.c:15 | fatal | bad: Unclosed BODY native role%u target%08x caller%08x\n | SAFE | F1 I1 W1 (R112) |  |
| body_native_dependencies.c:18 | fatal | retained: if(rx_sp()!=entry//U32(entry,0)!=ra)abort(); | SAFE | F1 I1 W1 (R112) |  |
| body_native_dependencies.c:57 | fatal | rx_body_probe_dependency: crt_invalid_parameter_noinfo();abort(); | SAFE | F1 I1 W1 (R112) |  |
| buggy_controls_dependencies.c:63 | fatal | rx_buggy_controls_dependency: default:abort(); /* Closed static list; no fallback value or original execution. */ | SAFE | F1 I1 W1 (R112) |  |
| buggy_engine_dependencies.c:14 | fatal | rx_buggy_engine_dependency: if(!site(ra,curves,4)//expected!=entry+4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| buggy_engine_dependencies.c:23 | fatal | rx_buggy_engine_dependency: if(!site(ra,lerps,4)//expected!=entry+4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| buggy_engine_dependencies.c:25 | fatal | rx_buggy_engine_dependency: }else abort(); /* Closed audited static sites; never a substitute return. */ | SAFE | F1 I1 W1 (R112) |  |
| buggy_engine_dependencies.c:26 | fatal | rx_buggy_engine_dependency: if(rx_sp()!=entry//expected!=entry+cleanup)abort(); | SAFE | F1 I1 W1 (R112) |  |
| buggy_engine_dependencies.c:30 | fatal | rx_buggy_engine_power_dependency: rx_addr entry=rx_sp();if(expected!=entry+4//U32(entry,0)!=0x4b918b)abort(); | SAFE | F1 I1 W1 (R112) |  |
| buggy_engine_dependencies.c:36 | fatal | rx_buggy_engine_power_dependency: if(rx_sp()!=entry)abort();for(unsigned i=0;i<3;i++)fx_set_reg((int)i,regs[i]); | SAFE | F1 I1 W1 (R112) |  |
| build_activity_audio_teardown.c:119 | indirect | rx_audio_logic_vf30: `target <- U32(U32(l, 0), 0x10)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_clear.c:395 | abort | rx_particle_manager2d_clear: the job pool's static constructor (00881a70) not run | SAFE | S1 (R15) |  |
| build_activity_clear.c:481 | abort | rx_vehicle_audio_release: a vehicle audio slot is assigned (the product runs no vehicle foley hook) | SAFE | B1 (R10) |  |
| build_activity_driver.c:38 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_driver.c:41 | abort | series_static: the series manager's static initialiser (MXSeriesManager_Construct) | SAFE | S2 (R16) |  |
| build_activity_driver.c:485 | abort | rx_player_start_zone: row index (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| build_activity_driver.c:640 | abort | rx_activity_on_player_joined: no free race index (12 taken) | SAFE | I1 (R39) |  |
| build_activity_event.c:254 | abort | ivec_init: vector _Xlen | SAFE | I1 (R18) |  |
| build_activity_event.c:285 | abort | random_shuffle: random_shuffle of more than 0x7fff elements | SAFE | I1 (R18) |  |
| build_activity_event.c:358 | abort | rx_ai_roster_build: types vector growth | SAFE | I1 (R18) |  |
| build_activity_event.c:536 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_event.c:547 | abort | rx_tbl_set_cell_float: column index (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| build_activity_event.c:578 | abort | rx_player_list_request_event: row index (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| build_activity_event.c:596 | abort | rx_activity_rows_keep: row index (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| build_activity_event.c:789 | abort | rx_start_event: StartEvent while viewing a track | SAFE | A1 (R25) |  |
| build_activity_frame.c:19 | abort | rx_engine_frame_mark: the job pool's static constructor (00881a70) not run | SAFE | S1 (R15) |  |
| build_activity_frame.c:35 | abort | rx_rumble_frame: controller rumble effects (0081f3d0 / 0081f310: input) | SAFE | B1 (R26) |  |
| build_activity_freeride_run.c:29 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_freeride_run.c:248 | abort | rx_free_ride_start_slots: a free ride start with fewer than two 0x1014 entities (star / line / default patterns) | SAFE | M1 X1 (R40) |  |
| build_activity_freeride_state.c:14 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_game.c:48 | indirect | orig_regs: `fn` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_game.c:73 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_game.c:153 | abort | players_unload: column index (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| build_activity_game.c:259 | abort | rx_mxgame_update: the front end's start (MXGame_SwitchToUIWorld: UI) | SAFE | A1 B1 (R24) |  |
| build_activity_game.c:260 | abort | rx_mxgame_update: the start in a level (no front end) | SAFE | A1 B1 (R24) |  |
| build_activity_game.c:275 | abort | rx_mxgame_update: the UI world's switch (MXGame_SwitchToUIWorld) | SAFE | A1 B1 (R24) |  |
| build_activity_game.c:300 | abort | rx_mxgame_update: the downloadable content lookup | SAFE | A1 B1 (R24) |  |
| build_activity_game.c:428 | abort | rx_mxgame_update: the event quit while its activity area is re-created (MXGame_SwitchToUIWorld) | SAFE | A1 (R45) |  |
| build_activity_game.c:441 | abort | rx_mxgame_update: the event quit while its activity area loads (MXGame_SwitchToUIWorld) | SAFE | A1 (R45) |  |
| build_activity_game.c:494 | abort | rx_mxgame_update: NeedSeriesAdvanceEvent (a UI page asked for the series' next race) | SAFE | H1 (R46) |  |
| build_activity_game.c:566 | abort | rx_mxgame_update: InputNames_Init (input unit) | SAFE | A1 B1 (R24) |  |
| build_activity_game.c:609 | abort | rx_mxgame_update: the Lua state's static initializer (00744180) | SAFE | A1 B1 (R24) |  |
| build_activity_grid.c:197 | abort | rx_race_grid_from_gate: a start grid without a starting gate (the start line / start node paths) | SAFE | M1 X1 (R30) |  |
| build_activity_grid.c:206 | abort | rx_race_grid_from_track: the mode 0 / 2 start grid (00564c90) | SAFE | A1 D1 (R31) |  |
| build_activity_grid.c:268 | abort | rx_game_can_begin_world_load: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_grid.c:287 | abort | rx_game_players_ready: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_grid.c:376 | abort | rx_game_grid_pass: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_grid.c:409 | abort | rx_game_grid_pass: the mode 6 start zones (0075f640) | SAFE | A1 D1 (R27) |  |
| build_activity_grid.c:415 | abort | rx_game_grid_pass: the free ride's mode 3 start zones | SAFE | A1 D1 (R27) |  |
| build_activity_grid.c:454 | fatal | rx_game_grid_pass: if (rows == 0) { raise(SIGFPE); abort(); } | SAFE | I1 F1 (R114) |  |
| build_activity_grid.c:545 | abort | session_presence: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_grid.c:575 | abort | rx_game_race_start: the series manager's static initialiser (MXSeriesManager_Construct) | SAFE | S2 (R16) |  |
| build_activity_player.c:36 | abort | rx_event_handler_list_push_back: list<T> too long | SAFE | I1 (R18) |  |
| build_activity_player.c:55 | abort | session_channel: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_player.c:245 | abort | rx_mxplayer_init: player init with an activity | SAFE | A1 (R49) |  |
| build_activity_player.c:264 | abort | rx_player_slot_alloc: slot 0 of a null array | SAFE | I1 (R18) |  |
| build_activity_player.c:280 | abort | rx_player_active: player lookup of an empty slot / kind | SAFE | I1 (R50) |  |
| build_activity_player.c:384 | abort | rx_player_from_player_list: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_player.c:440 | abort | rx_player_from_player_list: the name plate text in the product | SAFE | B1 (R51) |  |
| build_activity_player.c:562 | abort | rx_players_sync: the viewports with the second scene active | SAFE | A1 (R52) |  |
| build_activity_pre.c:28 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_pre.c:147 | abort | rx_activity_vf37: the replay recorder's frame (006df530) | SAFE | N1 D1 (R44) |  |
| build_activity_pre.c:149 | abort | rx_activity_vf37: a session result's entities (005584f0) | SAFE | N1 D1 (R44) |  |
| build_activity_race.c:69 | indirect | vcall1: `fn <- U32(U32(obj, 0), slot)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_race.c:75 | indirect | vcall0: `fn <- U32(U32(obj, 0), slot)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_race.c:82 | indirect | ctor_reg: `fn` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_race.c:119 | abort | tree_insert_at: map/set<T> too long | SAFE | I1 (R18) |  |
| build_activity_race.c:322 | abort | rx_event_type_register_at: StringMap_Find iterator (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| build_activity_race.c:467 | abort | rx_audio_logic_vf32: ActivityAudioLogic::vf32 with a cue map | SAFE | B1 (R53) |  |
| build_activity_race.c:749 | indirect | rx_activity_vf1: `fn <- U32(U32(A, 0), 0x58)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_race.c:855 | abort | rx_scene_request_subscene: a subscene request without the event's level (rx_event_set_level) | SAFE | M1 X1 (R54) |  |
| build_activity_race.c:859 | abort | rx_scene_request_subscene: a listed subscene missing from the level's scene | SAFE | M1 X1 (R54) |  |
| build_activity_race.c:939 | abort | rx_mxworld_set_track: a racing line released (00767850) | SAFE | M1 X1 (R55) |  |
| build_activity_race.c:942 | abort | rx_mxworld_set_track: the track's racing line (006a2980) | SAFE | M1 X1 (R55) |  |
| build_activity_race.c:963 | abort | rx_activity_lap_pacing: the lap pacing from temp-lappacing.bxml (no level) | SAFE | M1 X1 (R56) |  |
| build_activity_race.c:972 | abort | rx_activity_lap_pacing: a lap pacing asset without its reader | SAFE | M1 X1 (R56) |  |
| build_activity_race.c:1038 | abort | rx_race_stub_vf5: the championship intro (the series manager) | SAFE | C1 (R35) |  |
| build_activity_race.c:1090 | abort | col_cell: row index (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| build_activity_race.c:1153 | abort | rx_game_setup_activity_area: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_race.c:1206 | abort | rx_mxworld_vf3: row index (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| build_activity_records.c:137 | abort | rx_activity_entities_start: type 0x1053 entities' start (006bc170) | SAFE | X1 M1 (R43) |  |
| build_activity_records.c:147 | abort | rx_activity_entities_start: type 0x1062 entities' start (00555e00) | SAFE | X1 M1 (R43) |  |
| build_activity_records.c:166 | indirect | rx_activity_vf50: `fn <- U32(U32(A, 0), 0xd0)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_start.c:48 | abort | act_event_type: an event-type instantiation not in build_activity_events.inc | SAFE | M1 (R37) |  |
| build_activity_start.c:113 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_activity_start.c:116 | abort | series_static: the series manager's static initialiser (MXSeriesManager_Construct) | SAFE | S2 (R16) |  |
| build_activity_teardown.c:556 | abort | rx_scene_subscene_unload: the whole scene's unload (007d19f0 without a subscene name) | SAFE | A1 (R11) |  |
| build_activity_track.c:74 | abort | track_suffix: a .tdf07 suffix beyond the table (0x009c54cc) | SAFE | I1 (R57) |  |
| build_activity_track.c:212 | indirect | stream_read: `fn <- U32(U32(s, 0), 8)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_track.c:218 | indirect | stream_skip: `fn <- U32(U32(s, 0), 0x10)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_track.c:224 | indirect | stream_close: `fn <- U32(U32(s, 0), 4)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_util.h:23 | fatal | act_unreached: rxcore: build_activity: %08x %s: path not in the reference boot (not ported)\n | SAFE | I1 F1 (R114) |  |
| build_activity_util.h:46 | abort | act_column: column index (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| build_activity_util.h:62 | indirect | act_call_fn: `fn` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_util.h:72 | abort | act_call_fn: a call with more than 5 arguments | SAFE | I1 (R58) |  |
| build_activity_util.h:80 | abort | act_call_fn: a call with more than 5 arguments | SAFE | I1 (R58) |  |
| build_activity_util.h:94 | indirect | act_vcall_f: `fn <- U32(U32(obj, 0), slot)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_util.h:100 | indirect | act_call_f2: `fn` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_world.c:57 | abort | db_call: a Database virtual with an unexpected argument count | SAFE | I1 (R59) |  |
| build_activity_world.c:62 | indirect | db_call: `fn <- U32(U32(db, 0), slot)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_world.c:73 | abort | db_vcall: a Database virtual with an unexpected argument count | SAFE | I1 (R59) |  |
| build_activity_world.c:78 | indirect | db_vcall: `fn <- U32(U32(db, 0), slot)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_world.c:206 | indirect | rx_int_hash_insert: `afn <- U32(U32(alloc, 0), 4)` | SAFE | L1 M1 Z1 (R104) |  |
| build_activity_world.c:225 | abort | rx_scene_directory_read: the level's scene file (rx_event_set_level) | SAFE | M1 (R60) |  |
| build_activity_world.c:259 | abort | rx_mxworld_load_base: the front end's Base subscenes (007d19f0) | SAFE | M1 (R60) |  |
| build_asset.c:83 | fatal | alloc_vf1: rxcore: table allocator %08x is not a StdMemoryAllocator\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:234 | fatal | strtab_rehash_at: rxcore: table allocator %08x is not a StdMemoryAllocator\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:323 | fatal | rx_strtab_insert_at: rxcore: table allocator %08x is not a StdMemoryAllocator\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:437 | fatal | rx_asset_set_loader: rxcore: rx_asset_set_loader: no AssetIO with vtable %08x\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:446 | fatal | rx_asset_io_new_handle: rxcore: AssetIO %08x (vtable %08x) unknown\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:458 | fatal | handle_tag: rxcore: handle %08x has an unknown class (vtable %08x)\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:616 | fatal | type_entry: rxcore: asset type %.8s is not registered\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:678 | fatal | rx_database_load_asset: rxcore: Database::vf30 reload of %08x: AssetIO::vf4 not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:731 | fatal | rx_asset_io_destroy: rxcore: asset %08x: destructor %08x (vtable %08x slot 0) is not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:747 | fatal | rx_database_release_asset: rxcore: Database::vf31: asset %08x: vf2 %08x (vtable %08x) is not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:752 | fatal | rx_database_release_asset: rxcore: Database::vf31: AssetIO::vf4 %08x (vtable %08x) is not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:896 | fatal | rx_database_load_package: rxcore: package %s/%s: the fmt reader does not match\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:905 | fatal | rx_database_load_package: rxcore: read %s/%s %s: %s\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:922 | fatal | rx_database_load: rxcore: rx_database_load in the oracle build\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:925 | fatal | rx_database_load_package: rxcore: rx_database_load_package in the oracle build\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:974 | fatal | rx_database_vf_load_db: rxcore: Database::vf4 %s without an install (rx_database_set_install)\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:987 | fatal | rx_database_vf_load_package: rxcore: Database::vf15 %s/%s without an install (rx_database_set_install)\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:1045 | indirect | rx_database_vf_package_loaded: `t <- U32(U32(db, 0), 0x20)` | SAFE | L1 M1 Z1 (R104) |  |
| build_asset.c:1137 | fatal | rx_database_loader_step: rxcore: the Database loader job %08x is not registered\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset.c:1144 | fatal | rx_database_loader_step: rxcore: the Database loader record %u has kind %u (a request the product serves at once)\n | SAFE | M1 X1 I1 (R113) |  |
| build_asset_types.c:124 | fatal | rx_asset_handle_type: rxcore: asset handle vf1 %08x not ported (build_asset_types.c)\n | SAFE | M1 X1 I1 (R113) |  |
| build_collis.c:285 | fatal | rx_collision_geometry_load: rxcore: collision geometry kind %u is not ported (FUN_0081a1e0)\n | SAFE | M1 X1 I1 (R113) |  |
| build_collision.c:21 | fatal | ?: rxcore: build_collision: %s\n | SAFE | M1 X1 I1 (R113) |  |
| build_driver_dtor.c:35 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_driver_reset.c:36 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| build_event_proxies.c:25 | indirect | rx_event_proxy_call_member_int_float: `fn <- U32(proxy, 0x10), obj = U32(proxy, 0xc)` | SAFE | L1 M1 F1 (R05) |  |
| build_forest.c:35 | fatal | ?: rxcore: forest asset: %s\n | SAFE | M1 X1 I1 (R113) |  |
| build_model.c:401 | fatal | rx_model_asset_io_create: rxcore: model version %u is not ported (FUN_007d4f00 / ModelAsset_LoadVariant)\n | SAFE | M1 X1 I1 (R113) |  |
| build_mxgame.c:33 | abort | rx_quazal_alloc: Quazal allocator hook other than 00401f90 | SAFE | B1 R1 (R23) |  |
| build_mxgame.c:246 | abort | rx_fmod_event_construct: FMODEvent with a name (0080a6f0) | SAFE | B1 R1 (R23) |  |
| build_mxgame_event.c:130 | abort | save_slot_clear: save slot with contents | SAFE | B1 R1 (R23) |  |
| build_mxgame_init.c:23 | abort | reg3: Registry virtual not ported | SAFE | B1 R1 (R23) |  |
| build_mxgame_int.h:27 | fatal | mx_unreached: rxcore: build_mxgame: %08x %s: path not in the reference boot (not ported)\n | SAFE | I1 (R115) |  |
| build_mxgame_scene.c:24 | abort | rx_vecp_reserve_ecx: vector too long | SAFE | I1 (R18) |  |
| build_mxgame_startup.c:19 | abort | reg3: Registry virtual not ported | SAFE | B1 R1 (R23) |  |
| build_mxgame_startup.c:101 | abort | mxgame_startup: MusicPlayer with a track at startup (00677510) | SAFE | B1 R1 (R23) |  |
| build_mxworld.c:62 | fatal | cmd_insert_at: rxcore: command map too long\n | SAFE | M1 X1 I1 (R113) |  |
| build_mxworld.c:108 | fatal | cmd_insert: rxcore: the script command registry [0x00d6925c] is not built (startup)\n | SAFE | M1 X1 I1 (R113) |  |
| build_mxworld.c:263 | indirect | ext0: `fn` | SAFE | L1 M1 Z1 (R104) |  |
| build_registry.c:19 | fatal | unreached: rxcore: build_registry: %08x %s: path not in the reference boot (not ported)\n | SAFE | I1 (R115) |  |
| build_registry.c:90 | abort | new_entry: hash node allocation | SAFE | I1 (R18) |  |
| build_registry.c:111 | abort | hash_insert_int: registry record inserted twice | SAFE | R1 (R20) |  |
| build_registry.c:178 | abort | set_name: a name of 0x100 characters | SAFE | R1 (R20) |  |
| build_registry.c:179 | abort | set_name: a node renamed | SAFE | R1 (R20) |  |
| build_registry.c:186 | abort | node_new: node allocation | SAFE | I1 (R18) |  |
| build_registry.c:193 | abort | rx_xmlb_add_node: a second root | SAFE | R1 (R20) |  |
| build_registry.c:215 | abort | set_string: a string value of an attribute-flagged node | SAFE | R1 (R20) |  |
| build_registry.c:216 | abort | set_string: a string value over a string | SAFE | R1 (R20) |  |
| build_registry.c:224 | abort | set_array: a typed value over a string | SAFE | R1 (R20) |  |
| build_registry.c:226 | abort | set_array: value allocation | SAFE | I1 (R18) |  |
| build_registry.c:244 | abort | set_attr_string: an attribute value over a string | SAFE | R1 (R20) |  |
| build_registry.c:271 | abort | build_node: a BXML node with flag 1 | SAFE | R1 (R20) |  |
| build_registry.c:279 | abort | build_node: a BXML value type 2 / 4 / 6..9 / 12 | SAFE | R1 (R20) |  |
| build_registry.c:284 | abort | build_node: a non-string BXML attribute | SAFE | R1 (R20) |  |
| build_registry.c:301 | abort | rx_xmlb_load: load into a built document | SAFE | R1 (R20) |  |
| build_registry.c:304 | abort | rx_xmlb_load: a short BXML header | SAFE | R1 (R20) |  |
| build_registry.c:310 | abort | rx_xmlb_load: a short compressed BXML | SAFE | R1 (R20) |  |
| build_registry.c:316 | abort | rx_xmlb_load: a broken compressed BXML | SAFE | R1 (R20) |  |
| build_registry.c:323 | abort | rx_xmlb_load: an uncompressed BXML | SAFE | R1 (R20) |  |
| build_registry.c:328 | abort | rx_xmlb_load: short strings | SAFE | R1 (R20) |  |
| build_registry.c:335 | abort | rx_xmlb_load: short data | SAFE | R1 (R20) |  |
| build_registry.c:339 | abort | rx_xmlb_load: BXML values before version 0x3ea | SAFE | R1 (R20) |  |
| build_registry.c:340 | abort | rx_xmlb_load: short values | SAFE | R1 (R20) |  |
| build_registry.c:341 | abort | rx_xmlb_load: big-endian BXML | SAFE | R1 (R20) |  |
| build_registry.c:343 | abort | rx_xmlb_load: a BXML without nodes | SAFE | R1 (R20) |  |
| build_registry.c:345 | abort | rx_xmlb_load: short nodes | SAFE | R1 (R20) |  |
| build_registry.c:346 | abort | rx_xmlb_load: big-endian BXML | SAFE | R1 (R20) |  |
| build_registry.c:349 | abort | rx_xmlb_load: a BXML external data section | SAFE | R1 (R20) |  |
| build_registry.c:371 | abort | rx_xmlb_load: first child without a name | SAFE | R1 (R20) |  |
| build_registry.c:433 | abort | parse_list: a short value list | SAFE | R1 (R20) |  |
| build_registry.c:441 | abort | parse_list: a bool other than F / f / 0 / T | SAFE | R1 (R20) |  |
| build_registry.c:444 | abort | parse_list: a short value list | SAFE | R1 (R20) |  |
| build_registry.c:445 | abort | parse_list: a value list of more than one value | SAFE | R1 (R20) |  |
| build_registry.c:450 | abort | typed_copy: a value array shorter than asked | SAFE | R1 (R20) |  |
| build_registry.c:459 | abort | xb_vf51_floats: floats of another type | SAFE | R1 (R20) |  |
| build_registry.c:467 | abort | xb_vf52_ints: ints of type 0 or another type | SAFE | R1 (R20) |  |
| build_registry.c:473 | abort | xb_vf53_uints: uints of a typed value | SAFE | R1 (R20) |  |
| build_registry.c:478 | abort | xb_vf54_string: the string of a typed value | SAFE | R1 (R20) |  |
| build_registry.c:485 | abort | xb_vf56_cstr: the string of a typed value | SAFE | R1 (R20) |  |
| build_registry.c:494 | abort | xb_vf57_bools: bools of another type | SAFE | R1 (R20) |  |
| build_registry.c:525 | abort | xb_vf48_vec3: vec3 of a typed value | SAFE | R1 (R20) |  |
| build_registry.c:548 | abort | new_record: a second record block | SAFE | R1 (R20) |  |
| build_registry.c:586 | abort | add_document: allocation | SAFE | I1 (R18) |  |
| build_registry.c:592 | abort | add_document: allocation | SAFE | I1 (R18) |  |
| build_registry.c:612 | abort | rx_registry_construct: allocation | SAFE | I1 (R18) |  |
| build_registry.c:624 | abort | rx_registry_load_file: a registry file missing | SAFE | R1 (R20) |  |
| build_registry.c:627 | abort | rx_registry_load_file: allocation | SAFE | I1 (R18) |  |
| build_registry.c:641 | abort | rx_registry_startup: Engine.bxml | SAFE | R1 (R20) |  |
| build_registry.c:646 | abort | rx_registry_startup: the XMLReader cursor virtuals | SAFE | R1 (R20) |  |
| build_registry.c:663 | abort | next_token: a path with a leading separator | SAFE | R1 (R20) |  |
| build_registry.c:664 | abort | next_token: an empty path rest | SAFE | R1 (R20) |  |
| build_render_asset.c:23 | fatal | shader_programs: rxcore: shader asset: program name of %u bytes\n | SAFE | M1 X1 I1 (R113) |  |
| build_render_asset.c:133 | abort | rx_shader_asset_get_io: the shader AssetIO made on first use (its _atexit registration) | SAFE | R1 (R47) |  |
| build_render_asset_surface.c:215 | fatal | rx_surface_asset_destroy: rxcore: XenonSurfaceAsset: the surface destructor %08x is not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_render_asset_texture.c:13 | fatal | ?: rxcore: texture asset: %s\n | SAFE | M1 X1 I1 (R113) |  |
| build_rider_util.h:26 | fatal | ?: rxcore rider: %s (%08x)\n | SAFE | M1 X1 I1 (R113) |  |
| build_rider_util.h:85 | indirect | rr_vcall: `target <- U32(U32(obj, 0), off)` | SAFE | L1 M1 Z1 (R104) |  |
| build_rider_util.h:91 | indirect | rr_call: `fn` | SAFE | L1 M1 Z1 (R104) |  |
| build_scene.c:27 | fatal | ?: rxcore: scene: %s %08x\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene.c:117 | fatal | rs_asset: rxcore: scene: entity %u: no asset field %s\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene.c:160 | fatal | scene_rs_asset_list: rxcore: scene: list %s: asset missing\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene.c:258 | fatal | scene_vcall: rxcore: scene: vcall of slot %#x of object %08x (vtable %08x) has no function\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene.c:261 | indirect | scene_vcall: `target <- U32(U32(obj, 0), off)` | SAFE | L1 M1 Z1 (R104) |  |
| build_scene.c:616 | fatal | rx_scene_create_entity: rxcore: scene: entity type %#x is not built yet\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene_anim.c:117 | fatal | character_anim_release: rxcore: scene: helper object of a re-initialised character is not built\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene_anim.c:182 | fatal | rx_anim_single_loop_vf5: rxcore: scene: AnimEntitySingleAnimLoop SoundActions (FUN_006b9d50) is not built\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene_anim.c:275 | fatal | rx_anim_spline_loop_vf5: rxcore: scene: AnimEntitySplineAnimLoop SoundActions (FUN_006baae0) is not built\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene_anim.c:1113 | fatal | rx_instanced_anim_vf5: rxcore: scene: g_InstanceTransform shader parameter is not built\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene_crowd.c:99 | abort | flashes_append: vector<> too long | SAFE | I1 (R18) |  |
| build_scene_crowd.c:104 | abort | flashes_append: vector<> allocation failure (bad_alloc) | SAFE | I1 (R18) |  |
| build_scene_crowd.c:158 | abort | rx_billboard_crowd_vf5: LOD vector empty (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| build_scene_crowd.c:170 | abort | rx_billboard_crowd_vf5: the Crowd shader asset is not loaded | SAFE | X1 N2 (R19) |  |
| build_scene_forest.c:17 | fatal | ?: rxcore: forest: %s %08x\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene_misc.c:90 | fatal | rx_water_entity_vf5: rxcore: scene: the water asset is not built (WaterEntity %08x)\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene_misc.c:104 | fatal | rx_water_entity_vf25: rxcore: scene: WaterEntity_Reload (007e4090) is not built\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene_props.c:22 | fatal | ?: rxcore: scene props: %s %08x\n | SAFE | M1 X1 I1 (R113) |  |
| build_scene_trigger.c:568 | fatal | rx_trigger_level_config: rxcore: scene: LevelConfig '%s' matches no exhibition level\n | SAFE | M1 X1 I1 (R113) |  |
| build_tables_load.c:17 | fatal | unreached: rxcore: build_tables: %08x %s: path not in the reference boot (not ported)\n | SAFE | M1 X1 I1 (R113) |  |
| build_tables_load.c:29 | fatal | xv: rxcore: build_tables: XMLReader virtual %08x (+%x) not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_tables_load.c:60 | abort | rx_tbl_xml_vf_children: child count of no node | SAFE | R1 (R20) |  |
| build_tables_load.c:102 | abort | load_columns: a table without <Columns> (the columns of the database's table of that name) | SAFE | R1 (R20) |  |
| build_tables_rdb.c:29 | fatal | unreached: rxcore: build_tables: %08x %s: path not in the reference boot (not ported)\n | SAFE | I1 (R115) |  |
| build_tables_rdb.c:80 | abort | str_grow: string _Xlen | SAFE | I1 (R18) |  |
| build_tables_rdb.c:87 | abort | rx_tbl_str_assign_sub: string _Xran | SAFE | I1 (R18) |  |
| build_tables_rdb.c:90 | abort | rx_tbl_str_assign_sub: string self-assign | SAFE | I1 (R18) |  |
| build_tables_rdb.c:240 | abort | tree_insert_at: tree _Xlen | SAFE | I1 (R18) |  |
| build_tables_rdb.c:316 | abort | tree_insert_hint: hinted insert falling back to insert(val) | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:323 | abort | tree_erase: erase(end) | SAFE | I1 (R18) |  |
| build_tables_rdb.c:570 | abort | grow_cap: vector _Xlen | SAFE | I1 (R18) |  |
| build_tables_rdb.c:588 | abort | vec_reserve4: vector _Xlen | SAFE | I1 (R18) |  |
| build_tables_rdb.c:621 | abort | vec_str_reserve: vector _Xlen | SAFE | I1 (R18) |  |
| build_tables_rdb.c:630 | abort | rx_tbl_vector_reserve_ptr: vector _Xlen | SAFE | I1 (R18) |  |
| build_tables_rdb.c:700 | abort | rx_tbl_column_reserve: reserve of an 8-byte column with rows | SAFE | R2 (R21) |  |
| build_tables_rdb.c:701 | abort | rx_tbl_column_reserve: column type 0 / 5 | SAFE | R2 (R21) |  |
| build_tables_rdb.c:738 | abort | append_float: float column index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:775 | abort | append_float: column type 0 / 5 / 6 | SAFE | R2 (R21) |  |
| build_tables_rdb.c:807 | abort | variant_destroy: variant type 5 | SAFE | R2 (R21) |  |
| build_tables_rdb.c:823 | abort | variant_destroy: variant type | SAFE | R2 (R21) |  |
| build_tables_rdb.c:908 | abort | rx_tbl_add_int64_column: an 8-byte column added to a table with rows | SAFE | R2 (R21) |  |
| build_tables_rdb.c:925 | abort | rx_tbl_set_int_body: cell index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:931 | abort | rx_tbl_set_int_body: searchable index without the old value | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:962 | abort | rx_tbl_set_float: float column index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:964 | abort | rx_tbl_set_float: cell index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:975 | abort | rx_tbl_set_string_body: cell index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:981 | abort | rx_tbl_set_string_body: searchable index without the old value | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1017 | abort | rx_tbl_set_cell_int: column index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1094 | abort | column_erase_row: row erase of a column of type 0 / 5 / 6 | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1128 | abort | rx_tbl_table_clear_rows: row clear of a column of type 0 / 5 / 6 | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1135 | abort | rx_tbl_database_clear_rows: Database_ClearRows of a loaded database | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1176 | abort | column_destroy_storage: destroy of a column of type 0 / 5 / 6 | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1285 | abort | column_copy_storage: copy of a column of type 0 / 5 / 6 | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1305 | abort | column_copy_storage: copy of a float index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1307 | abort | column_copy_storage: copy over an existing index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1313 | abort | column_copy_storage: index rebuild of a string column with rows | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1365 | abort | rx_tbl_restore_player_defaults: restore over an existing table | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1410 | abort | rx_tbl_crit_destroy: criterion value type 5 | SAFE | R2 (R21) |  |
| build_tables_rdb.c:1462 | abort | crit_reserve: vector _Xlen | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1468 | abort | crit_reserve: reserve of a non-empty criteria vector | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1492 | abort | crit_resolve: a criterion through two parent tables | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1493 | abort | crit_resolve: column index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1589 | abort | cell_matches: cell index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1604 | abort | cell_matches: a string criterion other than == | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1607 | abort | cell_matches: a criterion on a column of type 0 / 2 / 5 / 6 | SAFE | R2 (R21) |  |
| build_tables_rdb.c:1614 | abort | parent_ref: parent column index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1615 | abort | parent_ref: a parent link of a non-int column | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1616 | abort | parent_ref: cell index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1636 | abort | rx_tbl_query: column index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1647 | abort | rx_tbl_query: a unique lookup of a non-string column | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1667 | abort | rx_tbl_query: an index lookup of unsupported cell type 0 / 2 / 5 / 6 | SAFE | R2 (R21) |  |
| build_tables_rdb.c:1691 | abort | rx_tbl_query: column index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1706 | abort | rx_tbl_query: column index | SAFE | I1 (R18) |  |
| build_tables_rdb.c:1787 | abort | rx_tbl_delete_rows: delete-all of a column of type 0 / 5 / 6 | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1826 | abort | set_cell_variant: a cell of type 0 / 5 / 6 from a criterion | SAFE | R1 I1 (R22) |  |
| build_tables_rdb.c:1896 | abort | rx_tbl_append_values_row: unsupported snapshot row column type | SAFE | R2 (R21) |  |
| build_tables_schema.c:60 | fatal | run_schema_on: default: abort(); | SAFE | I1 (R115) |  |
| build_tables_startup.c:44 | fatal | rx_tbl_load_tables_file: rxcore: build_tables: %s: not read\n | SAFE | M1 X1 I1 (R113) |  |
| build_tables_startup.c:146 | fatal | xv: rxcore: build_tables: XMLReader virtual +%x not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_tables_startup.c:164 | fatal | rx_tbl_mxregistry_load: rxcore: build_tables: database %s not loaded\n | SAFE | M1 X1 I1 (R113) |  |
| build_tables_startup.c:192 | fatal | rx_tbl_mxregistry_load: rxcore: build_tables: MXRegistry.bxml not read\n | SAFE | M1 X1 I1 (R113) |  |
| build_tables_startup.c:203 | fatal | rx_tbl_load_dlc_tables: rxcore: build_tables: %s not loaded\n | SAFE | M1 X1 I1 (R113) |  |
| build_terrain.c:30 | fatal | rx_asset_handle: rxcore: no %s handle %s (Database::vf27 creation not ported)\n | SAFE | M1 X1 I1 (R113) |  |
| build_terrain.c:35 | fatal | rx_rs_asset_handle: rxcore: entity %u: no asset field %s\n | SAFE | M1 X1 I1 (R113) |  |
| build_terrain.c:159 | fatal | rx_hfterrain_copy_heights: rxcore: height map is not a texture\n | SAFE | M1 X1 I1 (R113) |  |
| build_terrain_asset.c:144 | fatal | rx_terrain_collision_asset_io_create: rxcore: TerrainCollisionAsset in XML form is not ported (FUN_0041d350)\n | SAFE | F1 I1 W1 (R112) |  |
| build_vehicle.c:231 | fatal | model_instance_copy: rxcore: 004300e0 over a used instance: not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle.c:333 | fatal | rx_vehicle_moto_create_proxy: rxcore: 00462b90: proxy released\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle.c:338 | fatal | rx_vehicle_moto_create_proxy: rxcore: 00462b90: proxy freed\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle_atv.c:32 | fatal | ?: rxcore: ATV: %s (%08x)\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle_parts.c:46 | fatal | ?: rxcore: vehicle parts: %s (%08x)\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle_pkgcache.c:173 | indirect | rx_pkgcache_request_db: `t <- U32(U32(db, 0), async ? 0x14 : 0x10)` | SAFE | L1 M1 Z1 (R104) |  |
| build_vehicle_pkgcache.c:221 | indirect | rx_pkgcache_request: `t <- U32(U32(db, 0), 0x40)` | SAFE | L1 M1 Z1 (R104) |  |
| build_vehicle_proxy.c:23 | fatal | ?: rxcore: %s\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle_sim.c:27 | fatal | vcall0: rxcore: virtual %08x (slot %u of %08x) not in the core\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle_sim.c:197 | fatal | vec_reserve_100x50: rxcore: 004db290 with elements: not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle_sim.c:385 | fatal | params_load_file: rxcore: 0043a3f0 engine class %u\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle_spec.c:33 | fatal | ?: rxcore: vehicle specs: %s (%08x)\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle_tree.c:9 | fatal | ?: rxcore: std::map: %s (%08x)\n | SAFE | M1 X1 I1 (R113) |  |
| build_vehicle_util.h:35 | indirect | veh_vcall: `t <- U32(U32(obj, 0), off)` | SAFE | L1 M1 Z1 (R104) |  |
| build_vehicle_util.h:60 | indirect | veh_vcall_f: `t <- U32(U32(obj, 0), off)` | SAFE | L1 M1 Z1 (R104) |  |
| build_vehicle_wheeled.c:36 | fatal | ?: rxcore: four-wheel vehicle: %s (%08x)\n | SAFE | M1 X1 I1 (R113) |  |
| build_water.c:27 | fatal | ?: rxcore: water asset: %s\n | SAFE | M1 X1 I1 (R113) |  |
| build_world.c:39 | fatal | alloc_ptrs: rxcore: vector of %u pointers (length_error)\n | SAFE | M1 X1 I1 (R113) |  |
| build_world.c:48 | fatal | vec_insert_end_u32: rxcore: vector too long\n | SAFE | M1 X1 I1 (R113) |  |
| build_world.c:89 | fatal | vec_reserve: rxcore: reserve of a non-empty vector (%08x)\n | SAFE | M1 X1 I1 (R113) |  |
| build_world.c:436 | fatal | phys_props_build: rxcore: PhysicsProperties: no document (rx_phys_props_set_install)\n | SAFE | M1 X1 I1 (R113) |  |
| build_world.c:455 | fatal | phys_props_build: rxcore: PhysicsProperties: id %u out of range\n | SAFE | M1 X1 I1 (R113) |  |
| build_world.c:611 | fatal | rx_physics_world_construct: rxcore: physics worker thread requested (0xa946c4)\n | SAFE | M1 X1 I1 (R113) |  |
| build_world.c:668 | fatal | rx_physics_world_finalize: rxcore: the forest's collision (FUN_00667cc0) is the forest unit's\n | SAFE | M1 X1 I1 (R113) |  |
| build_xml.c:71 | fatal | strings_table: rxcore: XMLReader string table rehash not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_xml.c:124 | fatal | rx_xml_reader_load: rxcore: BXML big-endian data not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_xml.c:187 | fatal | rx_xml_reader_load: rxcore: BXML version %x not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_xml.c:224 | fatal | rx_xml_reader_load: rxcore: BXML external data section (flag 2) not ported\n | SAFE | M1 X1 I1 (R113) |  |
| build_xml.c:483 | indirect | rr_xml_vcall1: `t <- U32(U32(r, 0), off)` | SAFE | L1 M1 Z1 (R104) |  |
| build_xml_stream.c:102 | fatal | next_chunk: rxcore: CompressedInputStream with no chunk (the original's idiv faults)\n | SAFE | I1 (R115) |  |
| career_aaa_loading.c:29 | abort | rx_aaa_loading_page_enter: the AAA loading page of an exhibition event (the front end's event start) | SAFE | H1 (R76) |  |
| career_composition.c:26 | abort | ?: static void require(int yes,uint32_t pc,const char*what){if(!yes)act_unreached(pc,what);} | SAFE | H1 (R76) |  |
| career_heat.c:41 | abort | need: unclosed career Heat dependency | SAFE | H1 (R76) |  |
| career_heat.c:429 | abort | rx_career_heat_event_call: not the original Heat event | SAFE | H1 (R76) |  |
| career_heat.c:455 | abort | rx_career_heat_event_call: Heat slot requires its typed arity | SAFE | H1 (R76) |  |
| career_lifecycle.c:7 | abort | ?: unclosed career lifecycle dependency | SAFE | H1 (R76) |  |
| career_moto.c:5 | abort | ?: Moto manager missing | SAFE | H1 (R76) |  |
| career_moto.c:6 | abort | ?: Moto players missing | SAFE | H1 (R76) |  |
| career_moto.c:7 | abort | ?: Moto presentation missing | SAFE | H1 (R76) |  |
| career_moto.c:8 | abort | ?: Moto presentation missing | SAFE | H1 (R76) |  |
| career_moto.c:20 | abort | rx_career_moto_gate_result: Moto state dependencies missing | SAFE | H1 (R76) |  |
| career_moto.c:55 | abort | rx_career_moto_post_event: Moto state dependency missing | SAFE | H1 (R76) |  |
| career_moto.c:66 | abort | rx_career_moto_post_event: Moto standings missing | SAFE | H1 (R76) |  |
| career_moto.c:75 | abort | rx_career_moto_post_event: Moto post presentation missing | SAFE | H1 (R76) |  |
| career_moto_events.c:44 | abort | rx_career_moto_holeshot: Moto holeshot manager missing | SAFE | H1 (R76) |  |
| career_moto_registry.c:6 | abort | rx_career_moto_event_call: not original Moto event | SAFE | H1 (R76) |  |
| career_moto_registry.c:30 | abort | rx_career_moto_event_call: Moto slot requires its real typed arity | SAFE | H1 (R76) |  |
| career_moto_standings.c:5 | abort | ?: Moto standings manager | SAFE | H1 (R76) |  |
| career_moto_standings.c:24 | abort | rx_career_event_sort_drivers: Moto driver sort players | SAFE | H1 (R76) |  |
| career_owned_results.c:23 | abort | rx_career_owned_results_bind: owned career results bind requires actual manager/lifetime | SAFE | H1 (R76) |  |
| career_owned_results.c:49 | abort | observe: stale owned career result binding | SAFE | H1 (R76) |  |
| career_owned_results.c:51 | abort | observe: owned result requires Tour or Arcade | SAFE | H1 (R76) |  |
| career_owned_results.c:56 | abort | observe: owned result without a current series event | SAFE | H1 (R76) |  |
| career_owned_results.c:59 | abort | observe: owned result supports the Single, Moto and Heat events | SAFE | H1 (R76) |  |
| career_owned_results.c:63 | abort | observe: Moto result source is not a loaded Moto series' practice or moto | SAFE | H1 (R76) |  |
| career_owned_results.c:68 | abort | observe: Heat result source is not a loaded Heat series' practice, heat, LCQ or main event | SAFE | H1 (R76) |  |
| career_owned_results.c:79 | abort | observe: owned career result source is not current genuine post activity | SAFE | H1 (R76) |  |
| career_owned_results.c:81 | abort | observe: career result channel not bound | SAFE | H1 (R76) |  |
| career_owned_results.c:103 | abort | rx_career_owned_results_dispatch: Show Activity Results event signature mismatch | SAFE | H1 (R76) |  |
| career_players.c:7 | abort | ?: unclosed career player dependency | SAFE | H1 (R76) |  |
| career_presentation_binding.c:20 | abort | rx_career_presentation_unbind: cannot unbind during owned presentation dispatch | SAFE | H1 (R76) |  |
| career_presentation_binding.c:26 | abort | rx_career_presentation_observe_fade: owned FadeOut notification rejected | SAFE | H1 (R76) |  |
| career_presentation_binding.c:31 | abort | rx_career_presentation_dispatch_fade_complete: owned FadeComplete channel unavailable | SAFE | H1 (R76) |  |
| career_presentation_binding.c:42 | abort | rx_career_presentation_observe_page_wipe: owned ShowPageWipe notification rejected | SAFE | H1 (R76) |  |
| career_presentation_binding.c:46 | abort | dispatch_page_wipe: owned PageWipe has no name | SAFE | H1 (R76) |  |
| career_presentation_binding.c:48 | abort | dispatch_page_wipe: owned PageWipe name is not bounded | SAFE | H1 (R76) |  |
| career_presentation_binding.c:50 | abort | dispatch_page_wipe: owned PageWipe channel unavailable | SAFE | H1 (R76) |  |
| career_properties.c:23 | abort | ?: property key address overflow | SAFE | I1 (R18) |  |
| career_properties.c:34 | abort | store: property allocation overflow | SAFE | I1 (R18) |  |
| career_properties.c:35 | abort | store: property allocation failed | SAFE | I1 (R18) |  |
| career_race_start.c:10 | abort | rx_career_begin_race: unbound career race-start composition | SAFE | H1 (R76) |  |
| career_refresh.c:34 | abort | rx_career_refresh_player_models: unclosed career vehicle identity resolver | SAFE | H1 (R76) |  |
| career_refresh.c:115 | abort | rx_career_refresh: unclosed career UI-property writer | SAFE | H1 (R76) |  |
| career_routes.c:6 | abort | ?: unclosed career route dependency | SAFE | H1 (R76) |  |
| career_routes.c:58 | abort | rx_career_request_level: career level preparation failed; no loading success | SAFE | H1 (R76) |  |
| career_rules.c:8 | abort | column: PlayerEvent column index | SAFE | I1 (R18) |  |
| career_rules.c:43 | abort | rx_player_event_status_set: PlayerEvent Status column index | SAFE | I1 (R18) |  |
| career_rules.c:83 | abort | rx_owned_rules_bind: owned Rules binding invalid | SAFE | H1 (R76) |  |
| career_rules.c:91 | abort | rx_owned_rules_select: owned Rules host unavailable | SAFE | H1 (R76) |  |
| career_rules.c:125 | abort | rx_owned_rules_select: RulesType exceeds original buffer | SAFE | H1 (R76) |  |
| career_rules.c:138 | abort | rx_owned_rules_show: owned Rules popup request rejected | SAFE | H1 (R76) |  |
| career_single.c:12 | abort | need: unclosed career Single dependency | SAFE | H1 (R76) |  |
| career_single.c:39 | abort | rx_career_event_driver_row: invalid iterator proxy | SAFE | I1 (R18) |  |
| career_single.c:41 | abort | rx_career_event_driver_row: end iterator dereference | SAFE | I1 (R18) |  |
| career_single_capacity.c:36 | abort | rx_career_event_rider_limit: career event capacity needs actual UI property owner | SAFE | H1 (R76) |  |
| career_single_completion.c:10 | abort | completion_manager: career completion manager | SAFE | H1 (R76) |  |
| career_single_completion.c:43 | abort | rx_career_event_complete_results: career completion player table | SAFE | H1 (R76) |  |
| career_single_completion.c:104 | abort | rx_career_event_advance_victory: career completion UI state | SAFE | H1 (R76) |  |
| career_single_completion.c:112 | abort | rx_career_event_advance_victory: career completion UI state | SAFE | H1 (R76) |  |
| career_single_completion.c:118 | abort | rx_career_event_advance_victory: career completion player table | SAFE | H1 (R76) |  |
| career_single_completion.c:128 | abort | rx_career_event_advance_victory: invalid completion iterator | SAFE | H1 (R76) |  |
| career_single_completion.c:139 | abort | rx_career_event_post_result: career result player table | SAFE | H1 (R76) |  |
| career_single_completion.c:149 | abort | rx_career_event_post_result: career advance popup event | SAFE | H1 (R76) |  |
| career_single_completion.c:158 | abort | rx_career_event_post_result: invalid requested result iterator | SAFE | H1 (R76) |  |
| career_single_events.c:17 | abort | career_ui_channel: career UI channel not constructed | SAFE | H1 (R76) |  |
| career_single_events.c:96 | abort | rx_career_proxy_call_member_void: unregistered native career event callback | SAFE | H1 (R76) |  |
| career_single_lifecycle.c:11 | abort | event_manager: career manager dependency | SAFE | H1 (R76) |  |
| career_single_lifecycle.c:123 | abort | rx_career_event_next_sub_event: career event player table | SAFE | H1 (R76) |  |
| career_single_lifecycle.c:139 | abort | rx_career_event_next_sub_event: career area routing | SAFE | H1 (R76) |  |
| career_single_lifecycle.c:169 | abort | rx_career_event_assign_gate: gate device has no original player row | SAFE | H1 (R76) |  |
| career_single_prepare.c:16 | abort | rx_career_event_prepare: career preparation dependencies | SAFE | H1 (R76) |  |
| career_single_registry.c:11 | abort | deps: Single native dependencies not installed | SAFE | H1 (R76) |  |
| career_single_registry.c:19 | abort | rx_career_single_install_dependencies: incomplete Single native composition | SAFE | H1 (R76) |  |
| career_single_registry.c:42 | abort | rx_career_single_event_call: unsupported career event class (Single only) | SAFE | H1 (R76) |  |
| career_single_registry.c:71 | abort | rx_career_single_event_call: unclosed Single event virtual slot | SAFE | H1 (R76) |  |
| career_start.c:9 | abort | ?: unclosed career start dependency | SAFE | H1 (R76) |  |
| career_transition_host.c:6 | abort | rx_owned_loading_bind: owned loading binding invalid | SAFE | H1 (R76) |  |
| career_transition_host.c:11 | abort | rx_owned_loading_native_entry_completed: owned loading page entry has no host | SAFE | H1 (R76) |  |
| collision_triangle_ray.c:18 | fatal | triangle_pair_role: if(target!=0x86fac0//(ra!=0x85e3ba&&ra!=0x85e4f9))abort(); /* routed here only for its two | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:20 | fatal | triangle_pair_role: if(nargs!=5//U32(entry,0)!=ra//expected!=entry+24//bp<4//((bp&~7u)-16)!=expected)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:26 | fatal | triangle_pair_role: if((first&&(ta!=16//tb!=32))//(!first&&(ta!=32//tb!=16)))abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:27 | fatal | triangle_pair_role: if(((U32(A,12)&tb)/(U32(B,12)&ta))//(U32(B,16)&U32(A,20))//(U32(B,20)&U32(A,16)))abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:28 | fatal | triangle_pair_role: if(fx_reg(FX_EAX)!=A//fx_reg(FX_EBX)!=0//fx_reg(FX_ESI)!=M//fx_reg(FX_EDI)!=M)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:32 | fatal | triangle_pair_role: if(fx_reg(FX_ECX)!=PS//fx_reg(FX_EDX)!=QT)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:33 | fatal | triangle_pair_role: for(unsigned i=0;i<5;i++)if(U32(entry,4+4*i)!=args[i])abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:38 | fatal | triangle_pair_role: if(first){if(compare_width!=32//compare_left!=30//compare_right!=30)abort();} | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:39 | fatal | triangle_pair_role: else if(compare_width!=8//compare_left!=(flag&255u)//compare_right!=0)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:47 | fatal | triangle_pair_role: if(rx_sp()!=entry//U32(entry,0)!=ra//fx_reg(FX_EBX)!=0//fx_reg(FX_ESI)!=M//fx_reg(FX_EDI)! | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:59 | fatal | triangle_circle_declared: U32(entry,0)!=ra//expected!=entry+28//bp<4//((bp&~7u)-16)!=expected)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:62 | fatal | triangle_circle_declared: if((forward&&(ta!=0x10//tb!=0x400))//(!forward&&(ta!=0x400//tb!=0x10)))abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:63 | fatal | triangle_circle_declared: if((U32(A,12)&tb)//(U32(B,12)&ta)//(U32(B,16)&U32(A,20))//(U32(B,20)&U32(A,16)))abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:64 | fatal | triangle_circle_declared: if(forward){if(width!=32//left!=0x400//right!=0x400)abort();} | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:65 | fatal | triangle_circle_declared: else if(width!=8//left!=(flag&255u)//right!=0)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:69 | fatal | triangle_circle_declared: fx_reg(FX_EBX)!=0//fx_reg(FX_ESI)!=B//fx_reg(FX_EDI)!=M)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:70 | fatal | triangle_circle_declared: for(unsigned i=0;i<6;++i)if(U32(entry,4+4*i)!=args[i])abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:72 | fatal | triangle_circle_declared: if(rx_sp()!=entry//U32(entry,0)!=ra//fx_reg(FX_EBX)!=0//fx_reg(FX_ESI)!=B//fx_reg(FX_EDI)! | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:93 | fatal | complete_pair_role: if(nargs!=((composite//rodbox)?6u:5u)//U32(entry,0)!=ra//expected!=entry+4+4*nargs//bp<4// | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:96 | fatal | complete_pair_role: if((U32(A,12)&tb)//(U32(B,12)&ta)//(U32(B,16)&U32(A,20))//(U32(B,20)&U32(A,16)))abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:99 | fatal | complete_pair_role: if(ta!=(ra==0x85e590?32u:0x400u)//tb!=0x8000//width!=32//left!=tb//right!=tb)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:102 | fatal | complete_pair_role: if(ta!=0x8000//tb!=(ra==0x85e9a6?32u:0x400u)//width!=8//left!=(flag&255u)//right!=0)abort( | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:105 | fatal | complete_pair_role: if(ta!=4//tb!=0x400//width!=32//left!=tb//right!=tb)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:108 | fatal | complete_pair_role: if(ta!=0x400//tb!=4//width!=8//left!=(flag&255u)//right!=0)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:111 | fatal | complete_pair_role: if(ta!=32//tb!=4//width!=8//left!=(flag&255u)//right!=0)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:114 | fatal | complete_pair_role: if(ta!=4//tb!=32//width!=32//left!=30//right!=30)abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:117 | fatal | complete_pair_role: if(fx_reg(FX_EAX)!=eax//fx_reg(FX_ECX)!=ecx//fx_reg(FX_EDX)!=edx//fx_reg(FX_EBX)!=0//fx_re | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:118 | fatal | complete_pair_role: for(unsigned i=0;i<nargs;i++)if(U32(entry,4+4*i)!=args[i])abort(); | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:125 | fatal | complete_pair_role: if(rx_sp()!=entry//U32(entry,0)!=ra//fx_reg(FX_EBX)!=0//fx_reg(FX_ESI)!=B//fx_reg(FX_EDI)! | SAFE | I1 (R115) |  |
| collision_triangle_ray.c:129 | fatal | complete_pair_role: void rx_collision_dispatch_pair_observed_live(rx_addr target,rx_addr ra,unsigned nargs,rx_ | SAFE | I1 (R115) |  |
| composite_pair_dependencies.c:11 | fatal | ?: Unclosed native composite dependency %08x caller%08x\n | SAFE | F1 I1 W1 (R112) |  |
| contact_correction_native.c:8 | fatal | ?: static rx_addr frame(rx_addr ra,unsigned pop){rx_addr e=rx_sp();if(U32(e,0)!=ra)abort();re | SAFE | F1 I1 W1 (R112) |  |
| contact_correction_native.c:11 | fatal | rx_fourwheel_correction_wheel: rx_addr e=rx_sp(),expected=frame(ra,8);if(!valid//fx_reg(FX_ECX)!=w//fx_reg(FX_EDX)!=s//fx | SAFE | F1 I1 W1 (R112) |  |
| contact_correction_native.c:12 | fatal | rx_fourwheel_correction_wheel: rx_addr result=rx_contact_wheel_correction_exact(w,s,out);if(rx_sp()!=e)abort();rx_set_sp( | SAFE | F1 I1 W1 (R112) |  |
| contact_correction_native.c:15 | fatal | rx_fourwheel_correction_body_probe: rx_addr e=rx_sp(),expected=frame(ra,4);if((ra!=0x4cf4a2&&ra!=0x4b43a2)//fx_reg(FX_ECX)!=s) | SAFE | F1 I1 W1 (R112) |  |
| contact_correction_native.c:18 | fatal | rx_fourwheel_correction_cookie: rx_addr e=rx_sp(),expected=frame(ra,4);if((ra!=0x4cf4ad&&ra!=0x4b43ad)//fx_reg(FX_ECX)!=co | SAFE | F1 I1 W1 (R112) |  |
| contact_dependencies.c:66 | fatal | rx_contact_indexed_frame: fx_set_reg(FX_EDI,edi);if(rx_sp()!=entry)abort();return value; | SAFE | F1 I1 W1 (R112) |  |
| contact_dependencies.c:80 | fatal | rx_contact_force_at_point_frame: fx_set_reg(FX_EAX,result);if(rx_sp()!=entry)abort();return result; | SAFE | F1 I1 W1 (R112) |  |
| contact_dependencies.c:86 | fatal | rx_contact_dependency_exact: if(target==0x4390d0){if(expected!=entry+4//!site(ra,curves,8))abort();value=rx_contact_ind | SAFE | F1 I1 W1 (R112) |  |
| contact_dependencies.c:87 | fatal | rx_contact_dependency_exact: else if(target==0x85c770){if(expected!=entry+8//!site(ra,forces,5))abort();rx_contact_forc | SAFE | F1 I1 W1 (R112) |  |
| contact_dependencies.c:88 | fatal | rx_contact_dependency_exact: else if(target==0x8f9efe){if(expected!=entry+4//ra!=0x452f9a)abort();phys_security_check_c | SAFE | F1 I1 W1 (R112) |  |
| contact_dependencies.c:89 | fatal | rx_contact_dependency_exact: else abort(); | SAFE | F1 I1 W1 (R112) |  |
| contact_dependencies.c:90 | fatal | rx_contact_dependency_exact: if(rx_sp()!=entry)abort();rx_set_sp(expected);return value; | SAFE | F1 I1 W1 (R112) |  |
| contact_family_dependencies.c:12 | fatal | rx_fourwheel_contact_dependency: if(!rx_family_contact_site(U32(entry,0),target)//expected!=entry+cleanup)abort(); | SAFE | F1 I1 W1 (R112) |  |
| contact_family_dependencies.c:17 | fatal | rx_fourwheel_contact_dependency: else abort(); | SAFE | F1 I1 W1 (R112) |  |
| contact_family_dependencies.c:18 | fatal | rx_fourwheel_contact_dependency: if(rx_sp()!=entry)abort();rx_set_sp(expected);return value; | SAFE | F1 I1 W1 (R112) |  |
| contact_frames.c:19 | fatal | rx_contact_wheel_correction_exact: rx_addr entry=rx_sp();if(fx_reg(FX_ECX)!=w//fx_reg(FX_EDX)!=s//U32(entry,4)!=out)abort(); | SAFE | F1 I1 W1 (R112) |  |
| contact_frames.c:23 | fatal | rx_contact_wheel_correction_exact: if((uint64_t)out<f.A+8&&(uint64_t)out+16>f.E)abort(); | SAFE | F1 I1 W1 (R112) |  |
| contact_frames.c:24 | fatal | rx_contact_wheel_correction_exact: if(((uint64_t)out<s+0x8800ull&&(uint64_t)out+16>s)//((uint64_t)out<w+0x150ull&&(uint64_t)o | SAFE | F1 I1 W1 (R112) |  |
| contact_frames.c:25 | fatal | rx_contact_wheel_correction_exact: contact_frame c={current,f.E,0,0};current=&c;rx_v4 value=bike_wheel_position_correction(w, | SAFE | F1 I1 W1 (R112) |  |
| contact_frames.c:28 | fatal | rx_contact_body_average_exact: rx_addr entry=rx_sp();if(fx_reg(FX_ECX)!=s)abort();fx_frame f;fx_frame_enter(&f,0x1c,16,FX | SAFE | F1 I1 W1 (R112) |  |
| contact_frames.c:29 | fatal | rx_contact_body_average_exact: bike_body_probe_apply_average_correction(s);current=c.previous;fx_frame_leave(&f);if(rx_sp | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:18 | fatal | ?: rx_addr rx_contact_pose_base(void){if(!pose)abort();return pose->b;} | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:20 | fatal | rx_contact_matrix_q1_spills: if(!matrix)return;if(matrix->kind!=0//matrix->seen++)abort();rx_addr b=matrix->b; | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:24 | fatal | rx_contact_matrix_q2_spills: if(!matrix)return;if(matrix->kind!=2//matrix->seen++)abort();SF32(matrix->b,0,wz);SF32(mat | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:29 | fatal | quaternion_matrix: if(c.seen!=1)abort();matrix=c.previous;fx_set_reg(FX_EAX,q);fx_frame_leave(&f);if(rx_sp()! | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:34 | fatal | rx_contact_pose_matrix: if(!pose//pose->role++!=role)abort();rx_addr b=pose->b;static const uint32_t ra[]={0x85a95 | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:35 | fatal | rx_contact_pose_matrix: if(role>=5)abort();uint32_t arg=role==1?out:role<3?input:source; | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:38 | fatal | rx_contact_pose_matrix: if(role==4){if(rx_sp()!=b-4)abort();rx_set_sp(b);}else if(rx_sp()!=b)abort(); | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:42 | fatal | rx_contact_pose_matrix: rx_addr caller=rx_sp();(void)fx_push(1,&arg,ra[role]);rx_addr entry=rx_sp();if(entry!=call | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:44 | fatal | rx_contact_pose_matrix: if(rx_sp()!=entry)abort();rx_set_sp(entry+(role<3?8:4)); | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:46 | fatal | rx_contact_pose_matrix: void rx_contact_pose_relative(rx_v4 qb){if(!pose//pose->role!=5//rx_sp()!=pose->b-4)abort( | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:48 | fatal | rx_contact_pose_normalization: if(!pose//rx_sp()!=pose->b)abort();rx_st4(pose->b+0x20,u);rx_st4(pose->b+0x10,finite);fx_s | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:51 | fatal | rx_contact_rotor_pose_exact: rx_addr entry=rx_sp();if(fx_reg(FX_ECX)!=self)abort();fx_frame f;fx_frame_enter(&f,0xfc,16 | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:53 | fatal | rx_contact_rotor_pose_exact: rx_addr result=phys_rotor_project_pose(self);if(c.role!=5//rx_sp()!=f.E)abort();fx_set_reg | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:54 | fatal | rx_contact_rotor_pose_exact: fx_set_reg(FX_ESI,f.in.esi);rx_set_sp(f.E+4);fx_set_reg(FX_ECX,U32(f.E,0xfc)^(f.E+4));SU32 | SAFE | F1 I1 W1 (R112) |  |
| contact_pose_frames.c:55 | fatal | rx_contact_rotor_pose_exact: pose=c.previous;fx_frame_leave(&f);if(rx_sp()!=entry)abort();return result; | SAFE | F1 I1 W1 (R112) |  |
| controller_axis_query.c:9 | indirect | rx_controller_stick_drive_axis: `U32(U32(action,0),0x28)` | SAFE | L1 F1 M1 (R106) |  |
| controller_axis_query.c:17 | indirect | rx_controller_stick_drive_axis: `U32(U32(action,0),0x10)` | SAFE | L1 F1 M1 (R106) |  |
| controller_axis_query.c:30 | indirect | rx_controller_compound_available: `U32(U32(child,0),0x28)` | SAFE | L1 F1 M1 (R106) |  |
| crt_runtime.c:22 | fatal | crt_invalid_parameter_noinfo: rxcore: _invalid_parameter_noinfo: a checked index / iterator failed (the original terminates here)\ | SAFE | M1 X1 I1 (R113) |  |
| crt_runtime.c:96 | indirect | crt_cmp: `comp` | SAFE | Z1 B1 (R107) |  |
| crt_undname.c:35 | fatal | lock: rxcore: crt_undname: lock %u not created\n | SAFE | M1 X1 I1 (R113) |  |
| crt_undname.c:45 | fatal | mtinitlocknum: rxcore: crt_undname: _mtinitlocknum before the CRT heap\n | SAFE | M1 X1 I1 (R113) |  |
| crt_undname.c:47 | fatal | mtinitlocknum: rxcore: crt_undname: _malloc_crt failed\n | SAFE | M1 X1 I1 (R113) |  |
| crt_undname.c:138 | fatal | undname: rxcore: crt_undname: decorated name '%s' outside the ported grammar\n | SAFE | M1 X1 I1 (R113) |  |
| crt_util.h:44 | indirect | crt_unported: `fn` | SAFE | Z1 I1 (R108) |  |
| crt_util.h:52 | indirect | crt_unported_ex: `fn` | SAFE | Z1 I1 (R108) |  |
| crt_util.h:59 | indirect | crt_unported_this: `fn` | SAFE | Z1 I1 (R108) |  |
| engine_startline.c:263 | indirect | cfg_find: `fn <- U32(U32(cfg, 0), 4)` | SAFE | T1 (R02) |  |
| engine_startline.c:271 | indirect | cfg_read: `fn <- U32(U32(cfg, 0), slot)` | SAFE | T1 (R02) |  |
| family_contact_dispatch_native.c:22 | fatal | bad: rxcore: finite familyvf13 role%u caller%08x entry%08x EDI%08x ESI%08x\n | SAFE | F1 I1 W1 (R112) |  |
| family_contact_dispatch_native.c:28 | fatal | bad: unsigned rx_family_vf13_count(unsigned role){if(role>=9)abort();return counts[role];} | SAFE | F1 I1 W1 (R112) |  |
| family_contact_dispatch_native.c:112 | fatal | rx_fourwheel_contact_dispatch_dependency: crt_invalid_parameter_noinfo();abort(); | SAFE | F1 I1 W1 (R112) |  |
| family_response_frames.c:14 | fatal | family_status: abort(); | SAFE | F1 I1 W1 (R112) |  |
| family_response_frames.c:20 | fatal | rx_family_response_clamp_frame: if (U32(entry, 4) != simulation // expected_sp != entry + 8) abort(); | SAFE | F1 I1 W1 (R112) |  |
| family_response_frames.c:29 | fatal | rx_family_response_clamp_frame: if (rx_sp() != entry) abort(); | SAFE | F1 I1 W1 (R112) |  |
| family_response_frames.c:44 | fatal | rx_family_response_velocity_frame: if (mechanism < 0xd0) abort(); | SAFE | F1 I1 W1 (R112) |  |
| family_response_frames.c:47 | fatal | rx_family_response_velocity_frame: fx_reg(FX_ESI) != vector // vector != 0x00d61670) abort(); | SAFE | F1 I1 W1 (R112) |  |
| family_response_frames.c:51 | fatal | rx_family_response_velocity_frame: if (rx_sp() != entry) abort(); | SAFE | F1 I1 W1 (R112) |  |
| family_tyre_dependencies.c:100 | fatal | dependency: if(!allowed(family,target,U32(entry,0))//expected!=entry+cleanup)abort(); | SAFE | F1 I1 W1 (R112) |  |
| family_tyre_dependencies.c:129 | fatal | dependency: }else abort(); /* No value substituted for an unknown/private ABI. */ | SAFE | F1 I1 W1 (R112) |  |
| family_tyre_dependencies.c:130 | fatal | dependency: if(rx_sp()!=entry//expected!=entry+cleanup)abort();rx_set_sp(expected);return result; | SAFE | F1 I1 W1 (R112) |  |
| filtered_contact_native.c:8 | fatal | rx_fourwheel_filtered_publish: rx_addr entry=rx_sp(),vt=U32(body,0);if(!valid//U32(entry,0)!=ra//fx_reg(FX_ECX)!=body//fx | SAFE | F1 I1 W1 (R112) |  |
| filtered_contact_native.c:9 | fatal | rx_fourwheel_filtered_publish: (void)rx_contact_rotor_pose_exact(body);if(rx_sp()!=entry)abort();rx_set_sp(entry+4); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_active_native_bridge.c:15 | fatal | rx_fourwheel_active_dependency: if(expected!=entry+4)abort(); /* Every audited private target is RET0. */ | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_active_native_bridge.c:17 | fatal | rx_fourwheel_active_dependency: if(!family_site(ra,0x4d1025))abort();SU32(entry-4,0,fx_reg(FX_EBP)); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_active_native_bridge.c:23 | fatal | rx_fourwheel_active_dependency: if(!family_site(ra,0x4d1078)&&!family_site(ra,0x4d1151)&&!family_site(ra,0x4d11d8)&&!famil | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_active_native_bridge.c:26 | fatal | rx_fourwheel_active_dependency: if(!family_site(ra,0x4d1536)&&!family_site(ra,0x4d154c)&&!family_site(ra,0x4d1562)&&!famil | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_active_native_bridge.c:29 | fatal | rx_fourwheel_active_dependency: if(!family_site(ra,0x4d14c9))abort();result=phys_quat_slerp(x0,x1,x2); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_active_native_bridge.c:34 | fatal | rx_fourwheel_active_dependency: if(!family_site(ra,0x4d1523))abort();rx_addr mechanism=fx_reg(FX_EDX),rotation=fx_reg(FX_E | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_active_native_bridge.c:43 | fatal | rx_fourwheel_active_dependency: if(!family_site(ra,0x4d166c))abort();phys_security_check_cookie(fx_reg(FX_ECX)); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_active_native_bridge.c:44 | fatal | rx_fourwheel_active_dependency: }else abort(); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_active_native_bridge.c:45 | fatal | rx_fourwheel_active_dependency: if(rx_sp()!=entry)abort();rx_set_sp(expected);return result; | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_contact_correction.c:13 | fatal | wheel: if(rx_sp()!=expected)abort();return rx_ld4(out); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_contact_correction.c:33 | fatal | correction: fx_set_reg(FX_ECX,s);rx_addr expected=fx_push(0,NULL,base+0x192);rx_fourwheel_correction_b | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_contact_correction.c:34 | fatal | correction: fx_set_reg(FX_ECX,U32(b,0x6c)^b);expected=fx_push(0,NULL,base+0x19d);rx_fourwheel_correcti | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_contact_state.c:20 | fatal | grounded: if(fx_reg(FX_EDX)!=target)abort(); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_controls_dependencies_native.c:63 | fatal | rx_utv_controls_dependency: default:abort(); /* Closed static list; no fallback value or original execution. */ | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_filtered_contact.c:32 | fatal | publish: rx_addr expected=fx_push(0,NULL,ra);rx_fourwheel_filtered_publish(body,ra);if(rx_sp()!=exp | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_guest_reader_binding.c:7 | indirect | ?: `fn` | SAFE | L1 F1 M1 (R109) |  |
| fourwheel_guest_reader_binding.c:7 | fatal | ?: void rx_fourwheel_reader_unresolved(rx_addr fn){rx_regs in={0};rx_call_original_ex(fn,&in, | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_param_load.c:37 | indirect | database_handle: `target` | SAFE | L1 F1 M1 (R109) |  |
| fourwheel_params.c:35 | indirect | call_target: `fn` | SAFE | L1 F1 M1 (R109) |  |
| fourwheel_pending.c:52 | indirect | dependency: `target` | SAFE | L1 F1 M1 (R109) |  |
| fourwheel_rest_dependencies_native_test.c:9 | fatal | rx_fourwheel_rest_contact_dependency: if(family>1//ra!=(family?0x4b411a:0x4ce91a)//expected!=entry+4//U32(U32(self,0),0x74)!=tar | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_dependencies_native_test.c:18 | indirect | rx_fourwheel_rest_contact_dependency: `target <- fx_reg(FX_EDX),self=fx_reg(FX_ECX)` | SAFE | L1 F1 M1 (R109) |  |
| fourwheel_rest_dependencies_native_test.c:18 | fatal | rx_fourwheel_rest_contact_dependency: rx_set_sp(expected);rx_call_original_ex(target,&in,&out,0,NULL);abort(); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_dependencies_native_test.c:20 | fatal | rx_fourwheel_rest_contact_dependency: if(rx_sp()!=entry)abort();rx_set_sp(expected); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:16 | fatal | ?: static rest_frame* active(unsigned leave){if(!current//current->leave!=leave)abort();retur | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:17 | fatal | ?: static unsigned actual_family(rx_addr s){uint32_t vt=U32(s,0);if(vt==0x93d24c)return 1;if( | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:22 | indirect | unknown: `target` | SAFE | L1 F1 M1 (R109) |  |
| fourwheel_rest_frames.c:22 | fatal | unknown: rx_set_sp(caller);rx_call_original_ex(target,&in,&out,argc,args);abort(); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:25 | fatal | rx_fourwheel_rest_frames_vcall: if(!current//current->self!=self)abort();rest_frame*c=current;unsigned family=actual_famil | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:26 | fatal | rx_fourwheel_rest_frames_vcall: if(c->leave){if(slot!=0x78//argc//c->call_index++)abort();ra=0x432373;} | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:27 | fatal | rx_fourwheel_rest_frames_vcall: else if(c->call_index==0){if(slot!=0x74//argc)abort();++c->call_index;ra=0x43253e;} | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:28 | fatal | rx_fourwheel_rest_frames_vcall: else if(c->call_index==1){if(slot!=0x78//argc)abort();++c->call_index;ra=0x432549;fx_set_r | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:29 | fatal | rx_fourwheel_rest_frames_vcall: else{if(c->call_index!=2//slot!=0x7c//argc!=1//!args//args[0]!=b+0x30)abort();++c->call_in | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:36 | fatal | rx_fourwheel_rest_frames_vcall: else{unknown(target,b,argc,args);abort();} | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:37 | fatal | rx_fourwheel_rest_frames_vcall: if(rx_sp()!=entry)abort();fx_set_reg(FX_EAX,value);rx_set_sp(b);return value; | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:39 | fatal | rx_fourwheel_rest_frames_vcall: void rx_fourwheel_rest_frames_speed(float speed,float elapsed){rest_frame*c=active(0);if(c | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:41 | fatal | rx_fourwheel_rest_frames_vcall: void rx_fourwheel_rest_frames_stopped(void){rest_frame*c=active(0);if(c->call_index!=2)abo | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:42 | fatal | rx_fourwheel_rest_frames_vcall: rx_addr rx_fourwheel_rest_frames_normal_output(void){rest_frame*c=active(0);if(c->call_ind | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:43 | fatal | rx_fourwheel_rest_frames_vcall: void rx_fourwheel_rest_frames_normal_result(int above_limit){rest_frame*c=active(0);if(!c- | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:47 | fatal | canonical_constants: if(U32(0,0x931f24)!=rx_fb(1.f)//U32(0,0x9322cc)!=rx_fb(.5f)//U32(0,0x92fe48)!=rx_fb(3.f)// | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:53 | fatal | rx_fourwheel_enter_rest_frame: uint32_t result=bike_sim_enter_rest_test(self);if(c.call_index<2//c.call_index>3)abort();f | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:57 | fatal | rx_fourwheel_enter_rest_frame: fx_set_reg(FX_ECX,U32(b,0x4c)^(b+12));SU32(b,8,c.stopped?0x432620:0x432673);rx_set_sp(b+8) | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_rest_frames.c:62 | fatal | rx_fourwheel_leave_rest_frame: uint32_t result=bike_sim_leave_rest_test(self);if(c.call_index!=1)abort();fx_set_reg(FX_EC | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_safe_pose_native.c:14 | fatal | rx_fourwheel_safe_pose_dependency: if(family>2//role>3)abort();const rx_addr entry=rx_sp();const uint32_t ra=U32(entry,0);con | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_safe_pose_native.c:15 | fatal | rx_fourwheel_safe_pose_dependency: if(target!=targets[family][role]//ra!=entries[family]+offsets[role]//expected!=entry+4)abo | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_safe_pose_native.c:17 | fatal | rx_fourwheel_safe_pose_dependency: rx_addr v=fx_reg(FX_ECX);if(U32(U32(v,0),role?0xb8:0xb4)!=target//fx_reg(FX_EDX)!=target)a | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_safe_pose_native.c:21 | fatal | rx_fourwheel_safe_pose_dependency: }else if(role==2){if(fx_reg(FX_EDX)!=0x9e30d4)abort();(void)rx_query_lookup_frame(fx_reg(F | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_safe_pose_native.c:23 | fatal | rx_fourwheel_safe_pose_dependency: if(fx_reg(FX_EAX)!=0)abort();rx_regs in=fx_regs(),out; | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_safe_pose_native.c:27 | fatal | rx_fourwheel_safe_pose_dependency: if(rx_sp()!=entry//U32(entry,0)!=ra//fx_reg(FX_EBX)!=old.ebx//fx_reg(FX_EBP)!=old.ebp//fx_ | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_snapshot_native.c:8 | fatal | rx_fourwheel_snapshot_dependency: if(family>2//method>1//site>=(method?9:6)//kind>2)abort(); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_snapshot_native.c:10 | fatal | rx_fourwheel_snapshot_dependency: if(ra!=return_sites[method][family][site])abort(); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_snapshot_native.c:12 | fatal | rx_fourwheel_snapshot_dependency: if(target!=U32(U32(self,0),slot)//(kind==1)!=(slot==0x94//slot==0x90)//(kind==2)!=(slot==0 | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_snapshot_native.c:13 | fatal | rx_fourwheel_snapshot_dependency: snapshot: missing native child %08x at %08x\n | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_snapshot_native.c:18 | fatal | rx_fourwheel_snapshot_dependency: if(rx_sp()!=entry//U32(entry,0)!=ra//fx_reg(FX_EBX)!=old.ebx//fx_reg(FX_EBP)!=old.ebp//fx_ | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_solve_wrapper.c:21 | fatal | solve: if(rx_sp()!=entry)abort();rx_set_sp(b); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_suspension_dependencies.c:13 | fatal | rx_family_suspension_dependency: if(family>2)abort();rx_addr entry=rx_sp();uint32_t ra=U32(entry,0);unsigned role=9; | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_suspension_dependencies.c:15 | fatal | rx_family_suspension_dependency: if(role==9//expected!=entry+(role==8?4:8))abort(); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_suspension_dependencies.c:18 | fatal | rx_family_suspension_dependency: if(rx_sp()!=entry)abort();rx_set_sp(expected); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_update_native.c:39 | fatal | retained: if(rx_sp()!=entry//U32(entry,0)!=ra//fx_reg(FX_EBX)!=old.ebx//fx_reg(FX_EBP)!=old.ebp//fx_ | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_update_native.c:42 | fatal | rx_fourwheel_update_dependency: if(family>2//site>=22//(site==14&&family!=1))abort(); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_update_native.c:48 | fatal | rx_fourwheel_update_dependency: if(U32(U32(fx_reg(FX_ECX),0),offset)!=required//fx_reg(FX_EDX)!=required)abort(); | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_update_native.c:51 | fatal | rx_fourwheel_update_dependency: if(!required//required!=target//ra!=returnSites[family][site]//expected!=entry+4+cleanups[ | SAFE | F1 I1 W1 (R112) |  |
| fourwheel_update_native.c:92 | fatal | rx_fourwheel_update_dependency: default:abort(); | SAFE | F1 I1 W1 (R112) |  |
| fx_animal.c:95 | indirect | vcall1: `fn <- U32(U32(self, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_effect.c:20 | indirect | vcall1: `fn <- U32(U32(vt_obj, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_effect.c:158 | indirect | rx_fx_placeable_effect_tick: `fn <- U32(U32(e, 0), 0x38)` | SAFE | L1 F1 M1 (R109) |  |
| fx_effect.c:168 | indirect | rx_fx_placeable_effect_tick: `fn <- U32(U32(em, 0), 0)` | SAFE | L1 F1 M1 (R109) |  |
| fx_emit.c:28 | indirect | vcall0: `fn <- U32(U32(obj, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_emit.c:718 | indirect | rx_fx_emitter_gate: `fn <- U32(U32(mgr, 0), 0x14)` | SAFE | L1 F1 M1 (R109) |  |
| fx_emit.c:741 | indirect | rx_fx_emitter_gate: `fn <- U32(U32(mgr, 0), 0x10)` | SAFE | L1 F1 M1 (R109) |  |
| fx_emit.c:770 | indirect | spawn_call: `fn <- U32(U32(mgr, 0), 4)` | SAFE | L1 F1 M1 (R109) |  |
| fx_emit.c:957 | indirect | rx_fx_attachable_emitter_vf3: `fn <- U32(U32(em, 0), 0)` | SAFE | L1 F1 M1 (R109) |  |
| fx_emit.c:976 | indirect | rx_fx_vehicle_emitter_vf3: `fn <- U32(U32(em, 0), 0)` | SAFE | L1 F1 M1 (R109) |  |
| fx_entity.c:236 | indirect | vcall1: `fn <- U32(U32(obj, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_entity.c:245 | indirect | vcall0: `fn <- U32(U32(obj, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_rdb.c:17 | indirect | ?: `U32(0, 0x00916224)` | SAFE | Z1 B1 (R107) |  |
| fx_rdb.c:447 | indirect | rx_fx_model_entity_vf9: `fn <- U32(U32(L, 0), 4)` | SAFE | L1 F1 M1 (R109) |  |
| fx_stunt.c:32 | indirect | veh_air: `fn <- U32(U32(veh, 0), 0xac)` | SAFE | L1 F1 M1 (R109) |  |
| fx_stunt.c:227 | indirect | rx_fx_stunt_wanted: `fn <- U32(U32(v, 0), 0xe8)` | SAFE | L1 F1 M1 (R109) |  |
| fx_stunt.c:1043 | indirect | tree_next: `U32(0, 0x00916224)` | SAFE | Z1 B1 (R107) |  |
| fx_stunt.c:1450 | indirect | rx_fx_stunt_score: `fn <- U32(U32(v, 0), 0x44)` | SAFE | L1 F1 M1 (R109) |  |
| fx_stunt.c:1787 | indirect | rx_fx_stunt_begins: `fn <- U32(U32(v, 0), 0x44)` | SAFE | L1 F1 M1 (R109) |  |
| fx_stunt.c:1841 | indirect | rx_fx_stunt_ends: `fn <- U32(U32(v, 0), 0x44)` | SAFE | L1 F1 M1 (R109) |  |
| fx_stunt_input.c:205 | indirect | keymap_value: `fn <- U32(U32(km, 0), 0x10)` | SAFE | L1 F1 M1 (R109) |  |
| fx_terrain.c:338 | indirect | rx_fx_mud_update: `fn <- U32(U32(V, 0), 0x44)` | SAFE | L1 F1 M1 (R109) |  |
| fx_terrain.c:353 | indirect | rx_fx_mud_update: `fn <- U32(U32(V, 0), 0xac)` | SAFE | L1 F1 M1 (R109) |  |
| fx_tree.c:186 | fatal | rx_fx_tree_insert_at: rxcore: map/set<T> too long (005ba570)\n | SAFE | M1 X1 I1 (R113) |  |
| fx_tutorial.c:91 | indirect | controller_enable: `fn <- U32(U32(e + 0x90, 0), 0x14)` | SAFE | L1 F1 M1 (R109) |  |
| fx_tutorial_manager.c:90 | indirect | tut_keymap_value: `fn <- U32(U32(km, 0), 0x10)` | SAFE | L1 F1 M1 (R109) |  |
| fx_tutorial_manager.c:160 | indirect | tut_mappings: `U32(U32(db, 0), 0x6c)` | SAFE | L1 F1 M1 (R109) |  |
| fx_tutorial_manager.c:165 | indirect | tut_mappings: `U32(U32(x, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_types.c:16 | indirect | call_this0: `fn` | SAFE | L1 F1 M1 (R109) |  |
| fx_types.c:293 | indirect | db_vcall: `fn <- U32(U32(db, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_vector.c:17 | fatal | ?: rxcore: %s (fx_vector.c)\n | SAFE | M1 X1 I1 (R113) |  |
| fx_vehicle.c:134 | indirect | emitter_call: `fn <- U32(U32(em, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_vehicle.c:138 | indirect | emitter_call: `fn <- U32(U32(em, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_vehicle.c:216 | indirect | rx_fx_vehicle_effect_update: `fn <- U32(U32(em, 0), 0xc)` | SAFE | L1 F1 M1 (R109) |  |
| fx_vehicle.c:262 | indirect | rx_fx_vehicle_effects_update: `fn <- U32(U32(veh, 0), 0xcc)` | SAFE | L1 F1 M1 (R109) |  |
| fx_world.c:33 | indirect | vcall1: `fn <- U32(U32(obj, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_world.c:309 | indirect | rx_fx_event_proxy_call: `fn <- U32(p, 0x10), obj = U32(p, 0x14) + U32(p, 0xc)` | SAFE | L1 M1 F1 (R05) |  |
| fx_world.c:348 | indirect | rx_fx_online_content_poll: `U32(0, 0x00916268)` | SAFE | Z1 B1 (R107) |  |
| fx_world.c:364 | indirect | rx_fx_online_content_poll: `U32(U32(obj, 0), 0x14)` | SAFE | Z1 B1 (R107) |  |
| fx_world.c:555 | indirect | rx_fx_session_tick: `fn <- U32(U32(o, 0), 8)` | SAFE | L1 F1 M1 (R109) |  |
| fx_world.c:572 | indirect | rx_fx_session_tick: `U32(U32(o, 0), 0x88)` | SAFE | L1 F1 M1 (R109) |  |
| fx_world.c:648 | fatal | rx_fx_session_clock: rxcore: fx: 006decd0 the online session's clock: a Quazal session object exists (the product has no  | SAFE | I1 F1 (R114) |  |
| fx_worldparticles.c:18 | indirect | vcall1: `fn <- U32(U32(obj, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_worldparticles.c:28 | indirect | vcallf: `fn <- U32(U32(obj, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| fx_worldparticles.c:61 | indirect | rx_vehicle_effect_bind_record: `fn <- U32(U32(ctl, 0), 4)` | SAFE | L1 F1 M1 (R109) |  |
| fx_worldparticles.c:66 | indirect | rx_vehicle_effect_bind_record: `fn <- U32(U32(ctl, 0), 0x10)` | SAFE | L1 F1 M1 (R109) |  |
| fx_worldparticles.c:142 | indirect | rx_particle_controller_release: `fn <- U32(U32(mgr, 0), 0x14)` | SAFE | L1 F1 M1 (R109) |  |
| garage_test_drive.c:76 | abort | pl_locked_cell: row index (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| garage_test_drive.c:183 | abort | td_garage_vehicle: the garage's vehicle (the product never loads the garage) | SAFE | TD1 (R117) |  |
| gateless_race_start.c:15 | abort | fill: row helper changed entered guestSP | SAFE | I1 (R65) |  |
| gateless_race_start.c:21 | abort | next: next-row helper changed entered guestSP | SAFE | I1 (R65) |  |
| guest_reader_reset.c:52 | indirect | vcall: `fn` | SAFE | L1 F1 M1 (R109) |  |
| guest_string_hash.c:48 | indirect | allocator_alloc_at: `fn <- U32(vt,4)` | SAFE | L1 F1 M1 (R109) |  |
| guest_string_hash.c:58 | indirect | allocator_free_at: `fn <- U32(vt,8)` | SAFE | L1 F1 M1 (R109) |  |
| height_dependencies.c:8 | fatal | rx_terrain_height_dependency: rx_addr entry=rx_sp();uint32_t ra=U32(entry,0);rx_v4 live;if(expected!=entry+4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| height_dependencies.c:9 | fatal | rx_terrain_height_dependency: if(role==RX_HEIGHT_CELL_SETUP){if(ra!=0x7c5f8b)abort();(void)phys_terrain_cell_setup_exact | SAFE | F1 I1 W1 (R112) |  |
| height_dependencies.c:10 | fatal | rx_terrain_height_dependency: else if(role==RX_HEIGHT_CELL_OFFSETS){if(ra!=0x7c5fc2)abort();phys_terrain_cell_offsets_ex | SAFE | F1 I1 W1 (R112) |  |
| height_dependencies.c:12 | fatal | rx_terrain_height_dependency: if(ra!=(role==RX_HEIGHT_SAMPLE_BASE?0x7c5fd6u:0x7c601au))abort(); | SAFE | F1 I1 W1 (R112) |  |
| height_dependencies.c:14 | fatal | rx_terrain_height_dependency: }else abort(); | SAFE | F1 I1 W1 (R112) |  |
| height_dependencies.c:15 | fatal | rx_terrain_height_dependency: if(rx_sp()!=entry)abort();rx_set_sp(expected);return live; | SAFE | F1 I1 W1 (R112) |  |
| loop_engine.c:213 | indirect | rx_jobpool_worker_run: `fn <- U32(pool, 0x8030 + 4 * r)` | SAFE | L1 F1 M1 (R106) |  |
| loop_engine.c:301 | indirect | rx_jobpool_recover: `fn <- U32(slot, 0)` | SAFE | I1 (R87) |  |
| loop_engine.c:341 | indirect | stdcall1: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_entity.c:284 | indirect | rx_character_ticker_run: `fn <- loop_vslot(e, pass ? 0x78 : 0x74)` | SAFE | L1 F1 M1 (R106) |  |
| loop_entity.c:321 | indirect | rx_land_animal_update: `fn <- loop_vslot(e, 0x74)` | SAFE | L1 F1 M1 (R106) |  |
| loop_hostinput.c:180 | indirect | os_def_window_proc: `U32(0, slot)` | SAFE | Z1 B1 (R107) |  |
| loop_input_suspend.c:22 | indirect | km_enabled: `fn <- loop_vslot(km, 0x28)` | SAFE | L1 F1 M1 (R106) |  |
| loop_input_suspend.c:32 | indirect | km_enable: `fn <- loop_vslot(km, 0x24)` | SAFE | L1 F1 M1 (R106) |  |
| loop_input_suspend.c:110 | indirect | sus_im_call: `fn <- loop_vslot(im, off)` | SAFE | L1 F1 M1 (R106) |  |
| loop_player.c:69 | indirect | pl_run: `fn` | SAFE | T1 (R04) |  |
| loop_player.c:99 | indirect | pl_frame: `U32(0, 0x00916224)` | SAFE | Z1 B1 (R107) |  |
| loop_player.c:731 | indirect | rx_player_event_name: `U32(0, 0x009162bc)` | SAFE | Z1 B1 (R107) |  |
| loop_player.c:1355 | indirect | rx_player_update_reset_point: `fn <- loop_vslot(v, 0x88)` | SAFE | L1 F1 M1 (R106) |  |
| loop_player_destroy.c:163 | abort | rx_mxplayer_destroy: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| loop_util.h:41 | indirect | loop_tc0: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_util.h:50 | indirect | loop_tc0r: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_util.h:59 | indirect | loop_tc1: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_util.h:68 | indirect | loop_tc2: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_util.h:77 | indirect | loop_tc3: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_util.h:87 | indirect | loop_tc5: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_util.h:97 | indirect | loop_tc_f: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_util.h:106 | indirect | loop_tc_uf: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_util.h:123 | indirect | loop_tc2_x87: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_util.h:133 | indirect | loop_ex: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_vehicle.c:31 | indirect | lv_run: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_vehicle.c:590 | fatal | rx_collision_rays_vs_box_observed: if(!live)abort(); | SAFE | I1 F1 (R114) |  |
| loop_world.c:51 | indirect | players_update: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_world.c:53 | indirect | players_update: `fn` | SAFE | L1 F1 M1 (R106) |  |
| loop_world.c:291 | indirect | director_value: `fn <- loop_vslot(d, 0x94)` | SAFE | L1 F1 M1 (R106) |  |
| native_bridge.c:22 | indirect | ?: `fn` | SAFE | L1 F1 M1 (R109) |  |
| native_bridge.c:22 | fatal | ?: static void unresolved(rx_addr fn){rx_regs in={0};rx_call_original_ex(fn,&in,0,0,0);abort( | SAFE | I1 F1 (R114) |  |
| native_bridge.c:51 | fatal | rx_native_compressed_dependency: native compressed exact call frame mismatch target=%08x entry=%08x actual=%08x expected=%08x\n | SAFE | I1 F1 (R114) |  |
| native_bridge.c:56 | fatal | rx_native_compressed_dependency: native compressed original IDIV fault\n | SAFE | I1 F1 (R114) |  |
| native_curve_dependencies.c:11 | fatal | ?: uint32_t rx_family_native_curve_count(unsigned i){if(i>=4)abort();return site_counts[i];} | SAFE | F1 I1 W1 (R112) |  |
| native_curve_dependencies.c:16 | fatal | rx_family_response_curve_native: if(!found//expected!=entry+4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| native_curve_dependencies.c:20 | fatal | rx_family_response_curve_native: if(entry!=base-4u//expected!=base//fx_reg(FX_ESI)!=fx_reg(FX_EDI)+0x3d2c)abort(); | SAFE | F1 I1 W1 (R112) |  |
| native_curve_dependencies.c:31 | fatal | rx_family_response_curve_native: if(rx_sp()!=entry)abort();rx_set_sp(expected);return result; | SAFE | F1 I1 W1 (R112) |  |
| native_reader_load.c:144 | indirect | custom_alloc_at: `fn <- U32(vt,4)` | SAFE | L1 F1 M1 (R109) |  |
| native_reader_load.c:154 | indirect | custom_free_at: `fn <- U32(vt,8)` | SAFE | L1 F1 M1 (R109) |  |
| native_response_dependencies.c:11 | fatal | ?: shared response native dependency contract %08x\n | SAFE | F1 I1 W1 (R112) |  |
| native_response_dependencies.c:15 | fatal | ?: uint32_t rx_shared_response_dependency_count(unsigned i){if(i>=3)abort();return dependency | SAFE | F1 I1 W1 (R112) |  |
| native_role_dependencies.c:11 | fatal | ?: rxcore: family vf15 typed role contract %u\n | SAFE | F1 I1 W1 (R112) |  |
| net_boundary.h:16 | fatal | rx_net_unreached: rxcore: online boundary: %08x %s: the product has no online session (not ported)\n | SAFE | I1 (R115) |  |
| pending_tuning_dependencies.c:12 | fatal | ?: pending producer native dependency contract %08x\n | SAFE | I1 F1 (R114) |  |
| pending_tuning_dependencies.c:13 | fatal | ?: pending producer native dependency contract %08x\n | SAFE | I1 F1 (R114) |  |
| pending_tuning_dependencies.c:32 | indirect | rx_pending_dependency: `fn` | SAFE | L1 F1 M1 (R109) |  |
| pending_tuning_dependencies.c:34 | fatal | rx_pending_dependency: default:abort(); | SAFE | I1 F1 (R114) |  |
| phys_coll_misc.c:47 | fatal | phys_vector_reallocate: rxcore: vector too long (std::length_error)\n | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_misc.c:56 | fatal | phys_vector_reallocate: rxcore: vector too long (std::length_error)\n | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_pair.c:123 | fatal | composite_call: if(rx_sp()!=before)abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_pair.c:152 | fatal | composite_b_kernel: if(live)composite_call(0x916224,0x8728d4,0,0,live);else abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_pair.c:189 | fatal | composite_b_kernel: uint32_t phys_pair_shape_vs_composite_b_observed(rx_addr rotA,rx_addr posA,rx_addr M,rx_ad | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_shape.c:22 | indirect | phys_shape_bounds: `fn` | SAFE | L1 F1 M1 (R109) |  |
| phys_coll_shape.c:95 | fatal | child_rotation_kernel: SU32(f.E-4,0,0x848673);rx_set_sp(f.E-4);crt_invalid_parameter_noinfo();abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_shape.c:107 | fatal | child_rotation_kernel: void phys_composite_child_rotation_observed(rx_addr comp,uint32_t index,rx_addr q,rx_addr  | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_shape.c:119 | fatal | child_position_kernel: SU32(f.E-4,0,0x848713);rx_set_sp(f.E-4);crt_invalid_parameter_noinfo();abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_shape.c:132 | fatal | child_position_kernel: void phys_composite_child_position_observed(rx_addr comp,uint32_t index,rx_addr q,rx_addr  | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_shape2.c:112 | indirect | phys_composite_scale: `slot <- U32(U32(child, 0), 0x10)` | SAFE | L1 F1 M1 (R109) |  |
| phys_coll_shape2.c:133 | indirect | phys_composite_restore: `slot <- U32(U32(child, 0), 0x14)` | SAFE | L1 F1 M1 (R109) |  |
| phys_coll_terrain.c:54 | fatal | terrain_cell_setup: if(!live//fx_reg(FX_ESI)!=work//fx_reg(FX_ECX)!=dims//fx_reg(FX_EDX)!=pos//U32(entry,4)!=c | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:56 | fatal | terrain_cell_setup: if(U32(entry,0)!=0x7c5f8b)abort();rx_height_c_enter(0); | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:93 | fatal | terrain_cell_offsets: if(!live//fx_reg(FX_ESI)!=work)abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:95 | fatal | terrain_cell_offsets: if(U32(entry,0)!=0x7c5fc2)abort();rx_height_c_enter(1); | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:133 | fatal | terrain_sample_window: if(!live//fx_reg(FX_ESI)!=work//fx_reg(FX_EDI)!=out_center//U32(entry,4)!=out_nbr//U32(ent | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:135 | fatal | terrain_sample_window: uint32_t caller=U32(entry,0);if(caller!=0x7c5fd6&&caller!=0x7c601a)abort();rx_height_c_ent | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:152 | fatal | terrain_sample_window: indices=window_floor_to_int_entered(coordinate,F+0x10);if(rx_sp()!=F-8)abort();rx_set_sp(F | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:153 | fatal | terrain_sample_window: SU32(F-8,0,0x7c54f6);rx_set_sp(F-8);window_store_entered(indices,F+0x10);if(rx_sp()!=F-4)a | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:186 | fatal | terrain_sample_window: if(exact){fx_set_reg(FX_ECX,cookie^(F+4));if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))abort();fx_se | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:222 | fatal | terrain_height_normal: const rx_addr entry=rx_sp();fx_frame frame;rx_addr F;if(exact&&!live)abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:229 | fatal | terrain_height_normal: for(unsigned i=0;i<3;i++)if(addresses[i]<entry&&addresses[i]+sizes[i]>F-0x200)abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:324 | fatal | terrain_frame_disjoint: if((uint64_t)p<entry&&(uint64_t)p+n>(uint64_t)F-0x40u)abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:329 | fatal | terrain_triangles_kernel: if(U32(entry_sp,4)!=terrain//U32(entry_sp,8)!=out//U32(entry_sp,12)!=count_p//U32(entry_sp | SAFE | F1 I1 W1 (R112) |  |
| phys_coll_terrain.c:339 | fatal | terrain_triangles_kernel: if(exact){const uint32_t c[][2]={{0x931f24,0x3f800000},{0x9322cc,0x3f000000},{0x92e330,0x4 | SAFE | F1 I1 W1 (R112) |  |
| phys_constraint.c:248 | fatal | rotor_project_velocity: for (unsigned i=0; i<5; ++i) if (p[i] < entry && p[i] + n[i] > bottom) abort(); | SAFE | I1 F1 (R114) |  |
| phys_contact.c:154 | fatal | pair_kernel: abort(); | SAFE | I1 W1 (R111) |  |
| phys_contact.c:204 | fatal | phys_contact_pair_observed: rxcore: observed pair role target %08x caller %08x nargs %u: not a case of 0085ddd0\n | SAFE | I1 W1 (R111) |  |
| phys_contact.c:211 | fatal | phys_contact_pair_observed: if (rx_sp() != entry // U32(entry, 0) != return_pc) abort(); | SAFE | I1 W1 (R111) |  |
| phys_contact.c:235 | fatal | contact_dispatch_kernel: rx_addr entry=rx_sp();const uint32_t input[]={A,QA,PA,B,QB,PB,flag};if(fx_reg(FX_ECX)!=M)a | SAFE | I1 W1 (R111) |  |
| phys_contact.c:253 | fatal | contact_dispatch_kernel: if(sizeof pair_return_sites/sizeof pair_return_sites[0]!=sizeof k_pairs/sizeof k_pairs[0]) | SAFE | I1 W1 (R111) |  |
| phys_contact.c:257 | fatal | contact_dispatch_kernel: if(live)rx_collision_dispatch_pair_observed_live(c->fn,pair_return_sites[k],c->nargs,calle | SAFE | I1 W1 (R111) |  |
| phys_crt.c:112 | fatal | rx_report_gsfailure: rxcore: fatal: stack security cookie check failed (___report_gsfailure 008fa43c), return address %08 | SAFE | I1 F1 (R114) |  |
| phys_crt.c:141 | fatal | rx_rtc_stack_failure: rxcore: fatal: Run-Time Check Failure #2 - %s (_RTC_StackFailure 008faa92, return address %08x)\n | SAFE | I1 F1 (R114) |  |
| phys_mech.c:75 | fatal | nodes_vcall_observed: if (slot != 7 // U32(entry, 4) != M) abort(); | SAFE | I1 F1 (R114) |  |
| phys_mech.c:89 | fatal | nodes_vcall_observed: if (target != 0x85ac20) abort(); /* genuine SlidingRotor vf7 only */ | SAFE | I1 F1 (R114) |  |
| phys_mech.c:93 | fatal | nodes_vcall_observed: if (rx_sp() != entry - 16) abort(); | SAFE | I1 F1 (R114) |  |
| phys_mech_extern.h:15 | indirect | mx_call_regs: `fn` | SAFE | L1 F1 M1 (R109) |  |
| phys_mech_matrix.c:19 | indirect | phys_vcall0: `fn <- vslot_addr(obj, slot)` | SAFE | L1 F1 M1 (R109) |  |
| phys_mech_matrix.c:29 | indirect | phys_vcall1: `fn <- vslot_addr(obj, slot)` | SAFE | L1 F1 M1 (R109) |  |
| phys_mech_matrix.c:39 | indirect | phys_vcall2: `fn <- vslot_addr(obj, slot)` | SAFE | L1 F1 M1 (R109) |  |
| phys_misc.c:90 | indirect | phys_simulation_clock_advance: `U32(U32(context, 0), 8)` | SAFE | L1 F1 M1 (R109) |  |
| phys_misc.c:93 | indirect | phys_simulation_clock_advance: `U32(U32(clock, 0), 0x7c)` | SAFE | L1 F1 M1 (R109) |  |
| phys_pair2.c:132 | fatal | r6enter: if(e<0x5d0//fx_reg(FX_ECX)!=ends//fx_reg(FX_EDX)!=a//U32(e,4)!=b//U32(e,8)!=c//U32(e,12)!= | SAFE | F1 I1 W1 (R112) |  |
| phys_pair2.c:133 | fatal | r6enter: for(unsigned i=0;i<5;i++)if((in[i]&15u)//(uint64_t)in[i]+96>0x100000000ull//r6overlap(in[i | SAFE | F1 I1 W1 (R112) |  |
| phys_pair2.c:135 | fatal | r6enter: if((out[i]&(i?15u:3u))//(uint64_t)out[i]+size[i]>0x100000000ull//r6overlap(out[i],size[i], | SAFE | F1 I1 W1 (R112) |  |
| phys_pair2.c:136 | fatal | r6enter: for(unsigned j=0;j<5;j++)if(r6overlap(out[i],size[i],in[j],96))abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_pair2.c:137 | fatal | r6enter: for(unsigned j=0;j<i;j++)if(r6overlap(out[i],size[i],out[j],size[j]))abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_pair2.c:140 | fatal | r6enter: for(unsigned j=0;j<8;j++)if(r6overlap(out[i],size[i],constants[j],16))abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_pair2.c:143 | fatal | r6enter: if(f->B!=B)abort();r6u(f,0x5bc,U32(0,0x9b45e4)^(B+12)); | SAFE | F1 I1 W1 (R112) |  |
| phys_pair2.c:146 | fatal | r6leave: if(!f->active)return answer;rx_addr B=f->B;if(rx_sp()!=B)abort(); | SAFE | F1 I1 W1 (R112) |  |
| phys_pair_query.c:50 | indirect | query_vcall: `target <- U32(U32(object, 0), slot)` | SAFE | L1 F1 M1 (R109) |  |
| phys_rider.c:34 | indirect | tracked_call: `fn` | SAFE | L1 F1 M1 (R109) |  |
| phys_rig.c:95 | indirect | phys_rig_camera_speed: `U32(U32(camera, 0), 0x88)` | SAFE | L1 F1 M1 (R109) |  |
| player_reset_completion.c:9 | fatal | camera_matrix: fn f=(fn)rx_fn(target);if(!f)abort(); | SAFE | I1 (R115) |  |
| player_reset_completion.c:31 | fatal | rx_player_reset_completion: rx_addr target=U32(U32(sim,0),0x50);place_fn place=(place_fn)rx_fn(target);if(!place)abort | SAFE | I1 (R115) |  |
| player_snapshot.c:140 | fatal | rx_player_snapshot_import: if(!values//!storage//(longest&&!text_buffer))abort(); | SAFE | I1 (R115) |  |
| player_snapshot.c:156 | fatal | rx_player_snapshot_import: if(!rx_tbl_snapshot_unique_valid(table))abort(); | SAFE | I1 (R115) |  |
| powercross_countdown.c:16 | abort | ?: countdown presentation component lifetime mismatch | SAFE | H1 (R79) |  |
| powercross_countdown.c:57 | indirect | rx_powercross_proxy_call: `callback <- U32(h,0x10)` | SAFE | L1 M1 F1 (R05) |  |
| powercross_countdown.c:92 | abort | rx_powercross_countdown_bind: countdown requires actual offline Powercross activity | SAFE | H1 (R79) |  |
| powercross_countdown.c:93 | abort | rx_powercross_countdown_bind: countdown dashboard must bind before Countdown Entered | SAFE | H1 (R79) |  |
| powercross_countdown.c:95 | abort | rx_powercross_countdown_bind: countdown backing allocation failed | SAFE | I1 (R18) |  |
| query_dependencies.c:23 | fatal | rx_query_lookup_frame: fx_set_reg(FX_EAX,result);if(rx_sp()!=entry)abort();return result; | SAFE | F1 I1 W1 (R112) |  |
| query_dependencies.c:34 | fatal | rx_query_probe_setup_frame: if(rx_sp()!=entry)abort(); | SAFE | F1 I1 W1 (R112) |  |
| query_dependencies.c:41 | fatal | site: unsigned rx_query_complete_native_calls(unsigned role){if(role>=5)abort();return calls[rol | SAFE | F1 I1 W1 (R112) |  |
| query_dependencies.c:49 | fatal | rx_fourwheel_query_dependency: if(!site(ra,0x190)//expected!=entry+4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| query_dependencies.c:53 | fatal | rx_fourwheel_query_dependency: cleanup=24;if(!site(ra,0x19a)//expected!=entry+cleanup)abort(); | SAFE | F1 I1 W1 (R112) |  |
| query_dependencies.c:57 | fatal | rx_fourwheel_query_dependency: cleanup=24;if(!site(ra,0x1d7)//expected!=entry+cleanup)abort(); | SAFE | F1 I1 W1 (R112) |  |
| query_dependencies.c:61 | fatal | rx_fourwheel_query_dependency: cleanup=20;if(!site(ra,0x1fc)//expected!=entry+cleanup)abort(); | SAFE | F1 I1 W1 (R112) |  |
| query_dependencies.c:65 | fatal | rx_fourwheel_query_dependency: if((!site(ra,0x1b3)&&!site(ra,0x209))//expected!=entry+4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| query_dependencies.c:68 | fatal | rx_fourwheel_query_dependency: Unknown typed fourwheel query dependency %08x caller%08x\n | SAFE | F1 I1 W1 (R112) |  |
| query_dependencies.c:70 | fatal | rx_fourwheel_query_dependency: Unknown typed fourwheel query dependency %08x caller%08x\n | SAFE | F1 I1 W1 (R112) |  |
| race_activity.c:954 | indirect | rx_activity_manager_update: `fn <- race_vfn(act, 23)` | SAFE | L1 F1 M1 (R109) |  |
| race_activity.c:1049 | indirect | rx_event_proxy_call3: `U32(proxy, 0x10)` | SAFE | L1 M1 F1 (R05) |  |
| race_activity.c:1459 | indirect | rx_activity_start_sequence: `race_vfn(eng, 27)` | SAFE | L1 F1 M1 (R109) |  |
| race_activity.c:1499 | indirect | rx_event_proxy_call4: `U32(proxy, 0x10)` | SAFE | L1 M1 F1 (R05) |  |
| race_activity.c:1508 | indirect | rx_event_proxy_call5: `U32(proxy, 0x10)` | SAFE | L1 F1 M1 (R109) |  |
| race_activity.c:2351 | indirect | rx_activity_setup_object: `race_vfn(eng, 27)` | SAFE | L1 F1 M1 (R109) |  |
| race_activity_audio.c:50 | indirect | modifier_call: `fn` | SAFE | L1 F1 M1 (R109) |  |
| race_activity_freestyle.c:94 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_activity_freestyle.c:958 | abort | rx_freestyle_highlight: the series manager's static initialiser (MXSeriesManager_Construct) | SAFE | S2 (R16) |  |
| race_activity_stub.c:98 | abort | rx_race_stub_kind_setup: gate-less parent changed entered guestSP | SAFE | I1 (R65) |  |
| race_activity_stub.c:370 | abort | lap_presence: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_activity_stub.c:394 | abort | rx_activity_lap_completed: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_braketurn.c:13 | indirect | reset: `fn <- U32(U32(b,0),0x18)` | SAFE | L1 F1 M1 (R109) |  |
| race_braketurn.c:18 | indirect | disable: `fn <- U32(U32(b,0),0x1c)` | SAFE | L1 F1 M1 (R109) |  |
| race_capture.c:81 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_capture.c:400 | abort | rx_capture_vf2: Capture: no flag entity in the area subscene (the level lacks the mode's content) | SAFE | I1 M1 (R126) |  |
| race_capture.c:761 | indirect | rx_capture_driver_ai: `fn <- U32(U32(b, 0), 0x10)` | SAFE | L1 F1 M1 (R109) |  |
| race_challenge_activity.c:123 | indirect | challenge_value_bits: `fn <- U32(U32(A, 0), 0xf8)` | SAFE | L1 F1 M1 (R109) |  |
| race_challenge_payout.c:97 | abort | table_of: challenge payout: authored table missing | SAFE | R1 (R14) |  |
| race_champ.c:686 | indirect | rx_activity_vf20: `race_vfn(sim, 20)` | SAFE | L1 F1 M1 (R109) |  |
| race_champ.c:887 | indirect | rx_race_stub_vf9: `fn <- U32(U32(scenario, 0), 0x24)` | SAFE | L1 F1 M1 (R109) |  |
| race_driver_reset.c:130 | abort | rx_race_driver_vf3: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_egghunt.c:82 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_egghunt.c:111 | indirect | tail_vcall1: `fn <- U32(U32(obj, 0), slot)` | SAFE | L1 F1 M1 (R109) |  |
| race_egghunt.c:261 | abort | rx_egghunt_vf2: EggHunt: no \"GreedModel\" entity in the area subscene (the level lacks the mode's content) | SAFE | I1 M1 (R126) |  |
| race_extern.h:36 | indirect | race_thiscall: `fn` | SAFE | L1 F1 M1 (R109) |  |
| race_finish_select.c:171 | indirect | rx_finish_select_previous: `fn <- U32(vt, 0x20)` | SAFE | L1 F1 M1 (R109) |  |
| race_freestyle_driver.c:318 | abort | rx_freestyle_driver_vf7: a Freestyle AI rider (the AI debug override 0xae1b90) | SAFE | D1 (R73) |  |
| race_graffiti.c:64 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_graffiti.c:328 | abort | rx_graffiti_vf2: Graffiti: no StartEntityFreestyle / StartEntityFreeRide entity in the loaded subscenes (the level lacks the mo | SAFE | I1 M1 (R126) |  |
| race_graffiti_point.c:65 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_graffiti_point.c:76 | abort | point_driver: a GraffitiCapturePoint outside a Graffiti activity (the original reads a null activity in 00564970) | SAFE | I1 (R127) |  |
| race_graffiti_point.c:79 | abort | point_driver: a GraffitiCapturePoint's activity without drivers (the original reads a null driver) | SAFE | I1 (R127) |  |
| race_graffiti_point.c:98 | abort | point_listen: the handler proxy's allocation failed (the original reads a null proxy) | SAFE | I1 (R18) |  |
| race_graffiti_point.c:189 | indirect | rx_event_proxy_call_member_player_float: `fn <- U32(proxy, 0x10), obj = U32(proxy, 0x14) + U32(proxy, 0xc)` | SAFE | L1 F1 M1 (R109) |  |
| race_infection.c:75 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_infection.c:286 | abort | rx_infection_vf15: the round's first infected of no driver (integer division by zero) | SAFE | I1 (R122) |  |
| race_infection.c:869 | indirect | rx_infection_driver_ai: `fn <- U32(U32(b, 0), 0x10)` | SAFE | L1 F1 M1 (R109) |  |
| race_loader.c:21 | indirect | loader_finish: `fn` | SAFE | L1 F1 M1 (R109) |  |
| race_misc_util.c:107 | indirect | rx_vehicle_effect_query: `race_vfn(c, 20)` | SAFE | L1 F1 M1 (R109) |  |
| race_misc_util.c:170 | abort | rx_vector_storage_alloc: the MSVC vector<T> too long throw | SAFE | I1 (R18) |  |
| race_misc_vehicle.c:107 | indirect | vehicle_x87: `fn <- race_vfn(v, slot)` | SAFE | L1 F1 M1 (R109) |  |
| race_net.c:80 | indirect | rx_net_stamp: `hook <- U32(0, 0x9df898)` | SAFE | L1 F1 M1 (R109) |  |
| race_player.c:71 | indirect | activity_reset_point: `fn <- race_vfn(act, 19)` | SAFE | L1 F1 M1 (R109) |  |
| race_poker.c:53 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_section.c:49 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_section_driver.c:30 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_snake.c:70 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_snake.c:858 | abort | rx_snake_water_rise: a Snake start zone with UseWaterAsBoundary (the water's rise 007f0200; no shipped zone has it) | SAFE | M1 (R123) |  |
| race_snake_driver.c:46 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_snake_trail.c:363 | abort | shape_call: a trail shape other than 0x9308cc (the pointer [0x9cd974] has no writer) | SAFE | I1 (R124) |  |
| race_tag.c:68 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_tag_driver.c:44 | abort | session_static: the session's static initialiser has not run | SAFE | S1 (R15) |  |
| race_tag_driver.c:179 | indirect | rx_tag_driver_ai: `fn <- U32(U32(b, 0), 0x10)` | SAFE | L1 F1 M1 (R109) |  |
| rays6_typed_dependency.c:10 | fatal | rx_tri_segment_rays_dependency: if((!triangles&&!segments)//expected!=entry+4//fx_reg(FX_ECX)!=B+0x200//fx_reg(FX_EDX)!=(t | SAFE | F1 I1 W1 (R112) |  |
| rays6_typed_dependency.c:12 | fatal | rx_tri_segment_rays_dependency: for(unsigned i=0;i<6;i++)if(U32(entry,4+4*i)!=args[i])abort(); | SAFE | F1 I1 W1 (R112) |  |
| rays6_typed_dependency.c:16 | fatal | rx_tri_segment_rays_dependency: if(rx_sp()!=entry//U32(entry,0)!=ra//fx_reg(FX_EAX)!=result)abort();rx_set_sp(expected);re | SAFE | F1 I1 W1 (R112) |  |
| replay_damage.c:143 | abort | replay_damage_vcall_out: replay damage typed output owner | SAFE | H2 (R78) |  |
| replay_damage.c:167 | abort | rx_replay_damage_activate: replay damage physics matrix getter | SAFE | H2 (R78) |  |
| replay_follow.c:27 | abort | rx_replay_follow_begin: ReplayFollow Begin simulation vf2 | SAFE | H2 (R78) |  |
| replay_follow.c:89 | abort | rx_replay_follow_step: ReplayFollow Step simulation vf2 | SAFE | H2 (R78) |  |
| replay_presentation.c:41 | abort | rx_replay_require_viewport: owned replay viewport request bridge required | SAFE | E2 H2 (R77) |  |
| replay_presentation.c:53 | abort | replay_fade_request: owned replay requires actual scene view parameters | SAFE | H2 (R78) |  |
| replay_presentation.c:58 | abort | replay_fade_request: owned replay existing effect parameters missing | SAFE | H2 (R78) |  |
| replay_presentation.c:67 | abort | replay_fade_request: owned replay unsupported effect blend/mask | SAFE | H2 (R78) |  |
| replay_presentation.c:83 | abort | replay_fade_request: owned replay fade request bridge required | SAFE | E2 H2 (R77) |  |
| replay_restore.c:69 | abort | rx_replay_select_camera: replay camera vf28 | SAFE | H2 (R78) |  |
| replay_restore.c:75 | abort | rx_replay_select_camera: replay camera vf19 | SAFE | H2 (R78) |  |
| replay_setup.c:46 | abort | rx_replay_setup_clip: replay vehicle vf49 | SAFE | H2 (R78) |  |
| rider_pose_override.c:9 | fatal | listeners: if(rx_sp()!=before)abort(); | SAFE | I1 (R115) |  |
| rider_pose_override.c:82 | fatal | rx_rider_pose_override: if(rx_sp()!=before-4)abort();rx_set_sp(before); | SAFE | I1 (R115) |  |
| rider_pose_override_native.c:7 | fatal | rx_rider_override_listeners: if((ra!=0x61c164&&ra!=0x61f08d)//U32(e,0)!=ra//U32(e,4)!=list//U32(e,8)!=value)abort(); | SAFE | I1 (R115) |  |
| rider_pose_override_native.c:9 | fatal | rx_rider_override_listeners: if(rx_sp()!=e//U32(e,0)!=ra//fx_reg(FX_EBX)!=in.ebx//fx_reg(FX_EBP)!=in.ebp//fx_reg(FX_ESI | SAFE | I1 (R115) |  |
| rodbox_live.c:14 | fatal | rx_collision_0400_box_live: if(!live)abort(); | SAFE | I1 (R115) |  |
| rodbox_live.c:18 | fatal | rx_collision_0400_box_live: fx_reg(FX_ESI)!=saved.esi//fx_reg(FX_EDI)!=saved.edi)abort(); | SAFE | I1 (R115) |  |
| rx_vcall.h:18 | indirect | rx_tcall: `target` | SAFE | L1 F1 M1 (R109) |  |
| rx_vcall.h:30 | indirect | rx_tcall: `target` | SAFE | L1 F1 M1 (R109) |  |
| scene_fx_entities.c:97 | indirect | entity_world_matrix: `fn <- U32(U32(e, 0), 0x38)` | SAFE | L1 F1 M1 (R109) |  |
| scene_fx_entities.c:180 | indirect | vcall1: `fn <- U32(U32(obj, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| scene_fx_entities.c:242 | indirect | rx_placeable_effect_vf9: `fn <- U32(U32(mgr, 0), 0x14)` | SAFE | L1 F1 M1 (R109) |  |
| scene_viewports.c:39 | abort | viewport_record_name: viewport Camera allocation failed | SAFE | I1 (R18) |  |
| scene_viewports.c:56 | abort | viewport_require_cpu_owners: native GPU viewport owner is outside the installed managed renderer | SAFE | I1 B1 (R80) |  |
| scene_viewports.c:75 | abort | scene_clear_cpu_viewports: invalid viewport list during CPU owner release | SAFE | I1 (R18) |  |
| scene_viewports.c:100 | abort | scene_add_viewport: viewport parameter allocation failed | SAFE | I1 (R18) |  |
| scene_viewports.c:119 | abort | rx_scene_viewports_create: invalid viewport list during replacement | SAFE | I1 (R18) |  |
| sound_state.c:63 | indirect | vget: `target <- U32(U32(obj, 0), off)` | SAFE | S1 (R01) |  |
| tdf_owner.c:70 | indirect | child_delete_at: `fn` | SAFE | L1 F1 M1 (R109) |  |
| terrain_cell_frames.h:16 | fatal | ?: static inline void cell_disjoint(rx_addr p,uint64_t bytes,rx_addr bottom,rx_addr entry){if | SAFE | F1 I1 W1 (R112) |  |
| terrain_cell_frames.h:19 | fatal | cell_constants: for(unsigned i=0;i<(floor?4u:1u);i++)if(U32(c[i][0],0)!=c[i][1])abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_cell_frames.h:22 | fatal | cell_vec_ctor: if(ra!=0x7c5248&&ra!=0x7c5323)abort();rx_addr caller=rx_sp();uint32_t arg=out; | SAFE | F1 I1 W1 (R112) |  |
| terrain_cell_frames.h:24 | fatal | cell_vec_ctor: if(out&15)abort();rx_world_vec4_from_scalars(out,value.v[0],value.v[1],value.v[2],value.v[ | SAFE | F1 I1 W1 (R112) |  |
| terrain_cell_frames.h:25 | fatal | cell_vec_ctor: fx_set_reg(FX_EAX,out);fx_frame_leave(&f);if(rx_sp()!=entry)abort();rx_set_sp(caller); | SAFE | F1 I1 W1 (R112) |  |
| terrain_cell_frames.h:31 | fatal | cell_floor_frame: fx_frame_leave(&f);if(rx_sp()!=entry)abort();rx_set_sp(caller);return result; | SAFE | F1 I1 W1 (R112) |  |
| terrain_cell_frames.h:34 | fatal | cell_cookie_leave: if(ra!=0x7c52c2&&ra!=0x7c52e0&&ra!=0x7c5373)abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_cell_frames.h:35 | fatal | cell_cookie_leave: fx_set_reg(FX_ECX,U32(f->E,0x1c)^f->E);if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_frames.h:10 | fatal | height_vec_pair: if(ra!=0x7c5ee7&&ra!=0x7c5f48&&ra!=0x7c5f68)abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_frames.h:12 | fatal | height_vec_pair: fx_frame f;fx_frame_enter0(&f,0,16);if(out&15)abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_frames.h:14 | fatal | height_vec_pair: fx_frame_leave(&f);if(rx_sp()!=entry)abort();rx_set_sp(caller); | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_frames.h:17 | fatal | height_splat_entered: rx_addr entry=rx_sp();if(U32(entry,0)!=0x7c609b//U32(entry,4)!=out//expected!=entry+12)abo | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_frames.h:18 | fatal | height_splat_entered: uint32_t raw=U32(entry,8);fx_frame f;fx_frame_enter0(&f,0,16);if(out&15)abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_frames.h:26 | fatal | height_cookie_leave: if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))abort(); /* never enter typed cookie failure fallback * | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_texture_native.c:11 | fatal | rx_terrain_height_texture: if(slot==13){n=4;if(ra!=0x7c5f21//expected!=entry+20//U32(entry,4)!=tex//U32(entry,8)!=fx_ | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_texture_native.c:12 | fatal | rx_terrain_height_texture: else if(slot==14){n=1;if((ra!=0x7c5fa0&&ra!=0x7c6143)//expected!=entry+8//U32(entry,4)!=te | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_texture_native.c:13 | fatal | rx_terrain_height_texture: else abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_texture_native.c:14 | fatal | rx_terrain_height_texture: if(!rx_fn(target))abort();uint32_t args[4];for(unsigned i=0;i<n;i++)args[i]=U32(entry,4+4* | SAFE | F1 I1 W1 (R112) |  |
| terrain_height_texture_native.c:15 | fatal | rx_terrain_height_texture: uint32_t hr=rx_d3d_call(tex,slot,n,args);if(rx_sp()!=entry)abort();fx_set_reg(FX_EAX,hr);r | SAFE | F1 I1 W1 (R112) |  |
| terrain_math_frames.h:10 | fatal | terrain_floor_frame: if(U32(rx_sp(),0)!=(integer?0x7c63aa:0x7c6318))abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_math_frames.h:17 | fatal | terrain_store_frame: if(U32(rx_sp(),0)!=0x7c63b2//U32(rx_sp(),4)!=dest)abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_math_frames.h:19 | fatal | terrain_store_frame: if(dest&15)abort();rx_v4 result=phys_vec4_store_aligned(x,dest);if(dest)fx_set_reg(FX_EAX, | SAFE | F1 I1 W1 (R112) |  |
| terrain_texture_native.c:11 | fatal | rx_terrain_texture_exact: if(slot==13){n=4;if(ra!=0x7c6208//expected!=entry+20//U32(entry,4)!=texture//U32(entry,12) | SAFE | F1 I1 W1 (R112) |  |
| terrain_texture_native.c:12 | fatal | rx_terrain_texture_exact: else if(slot==14){n=1;if(expected!=entry+8//U32(entry,4)!=texture)abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_texture_native.c:13 | fatal | rx_terrain_texture_exact: if(ra==0x7c66c2){if(fx_reg(FX_EAX)!=vt//fx_reg(FX_ECX)!=target)abort();} | SAFE | F1 I1 W1 (R112) |  |
| terrain_texture_native.c:14 | fatal | rx_terrain_texture_exact: else if(ra==0x7c63e7){if(fx_reg(FX_ECX)!=vt//fx_reg(FX_EDX)!=target)abort();} | SAFE | F1 I1 W1 (R112) |  |
| terrain_texture_native.c:15 | fatal | rx_terrain_texture_exact: else if(ra==0x7c66d9){if(fx_reg(FX_EDX)!=vt//fx_reg(FX_EAX)!=target)abort();} | SAFE | F1 I1 W1 (R112) |  |
| terrain_texture_native.c:16 | fatal | rx_terrain_texture_exact: else abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_texture_native.c:17 | fatal | rx_terrain_texture_exact: }else abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_texture_native.c:18 | fatal | rx_terrain_texture_exact: if(!rx_fn(target))abort(); /* Never enter rx_d3d_call's original-only fallback. */ | SAFE | F1 I1 W1 (R112) |  |
| terrain_texture_native.c:20 | fatal | rx_terrain_texture_exact: uint32_t hr=rx_d3d_call(texture,slot,(int)n,args);if(rx_sp()!=entry)abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_window_frames.h:15 | fatal | window_floor_to_int_entered: if(U32(entry,0)!=0x7c54ee//entry!=parent-12//dest!=parent+0x10//U32(entry,4)!=dest//U32(en | SAFE | F1 I1 W1 (R112) |  |
| terrain_window_frames.h:19 | fatal | window_floor_to_int_entered: fx_frame_leave(&f);if(rx_sp()!=entry)abort();rx_set_sp(entry+4);return result; | SAFE | F1 I1 W1 (R112) |  |
| terrain_window_frames.h:23 | fatal | window_store_entered: if(U32(entry,0)!=0x7c54f6//entry!=parent-8//dest!=parent+0x10//U32(entry,4)!=dest)abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_window_frames.h:25 | fatal | window_store_entered: fx_set_reg(FX_EAX,dest);rx_st4(f.E,value);if(dest&15)abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_window_frames.h:27 | fatal | window_store_entered: fx_set_reg(FX_ECX,U32(f.E,0x1c)^f.E);if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))abort(); | SAFE | F1 I1 W1 (R112) |  |
| terrain_window_frames.h:29 | fatal | window_store_entered: fx_frame_leave(&f);if(rx_sp()!=entry)abort();rx_set_sp(entry+4); /* RET0, dest argument re | SAFE | F1 I1 W1 (R112) |  |
| triangles_segments_frames.h:9 | fatal | rx_ts_domain: if(B<0x5e0//entry<=B)abort(); | SAFE | F1 I1 W1 (R112) |  |
| triangles_segments_frames.h:13 | fatal | rx_ts_domain: if(rx_ts_overlap(M,0x6a4,low,scratch)//(uint64_t)M+0x6a4>0x100000000ull)abort(); | SAFE | F1 I1 W1 (R112) |  |
| triangles_segments_frames.h:14 | fatal | rx_ts_domain: for(unsigned i=0;i<8;i++)if((uint64_t)a[i]+n[i]>0x100000000ull//rx_ts_overlap(a[i],n[i],lo | SAFE | F1 I1 W1 (R112) |  |
| triangles_segments_frames.h:20 | fatal | rx_tri_segment_frame_enter: if(fx_reg(FX_ECX)!=PS//fx_reg(FX_EDX)!=QT//fx_reg(FX_ESI)!=M//U32(entry,4)!=T//U32(entry,8 | SAFE | F1 I1 W1 (R112) |  |
| triangles_segments_frames.h:35 | fatal | rx_tri_segment_frame_leave: if(rx_sp()!=B)abort(); | SAFE | F1 I1 W1 (R112) |  |
| triangles_segments_frames.h:38 | fatal | rx_tri_segment_frame_leave: if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))abort(); | SAFE | F1 I1 W1 (R112) |  |
| truck_controls_dependencies.c:63 | fatal | rx_truck_controls_dependency: default:abort(); /* Closed static list; no fallback value or original execution. */ | SAFE | F1 I1 W1 (R112) |  |
| truck_engine_dependencies.c:19 | fatal | rx_truck_engine_dependency: if(expected!=entry+4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| truck_engine_dependencies.c:38 | fatal | rx_truck_engine_dependency: }else abort(); | SAFE | F1 I1 W1 (R112) |  |
| truck_engine_dependencies.c:39 | fatal | rx_truck_engine_dependency: if(rx_sp()!=entry)abort();rx_set_sp(expected);return result; | SAFE | F1 I1 W1 (R112) |  |
| truck_engine_dependencies.c:44 | fatal | rx_truck_engine_power_dependency: if(expected!=entry+4//U32(entry,0)!=0x4557eb)abort(); | SAFE | F1 I1 W1 (R112) |  |
| truck_engine_dependencies.c:47 | fatal | rx_truck_engine_power_dependency: if(rx_sp()!=entry)abort(); | SAFE | F1 I1 W1 (R112) |  |
| truck_engine_dependencies.c:54 | fatal | rx_truck_engine_fmod_dependency: if(expected!=entry+4//U32(entry,0)!=0x455e94)abort(); | SAFE | F1 I1 W1 (R112) |  |
| truck_engine_dependencies.c:64 | fatal | rx_truck_engine_fmod_dependency: if(rx_sp()!=frame.E-4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| truck_engine_dependencies.c:66 | fatal | rx_truck_engine_fmod_dependency: if(rx_sp()!=entry)abort();rx_set_sp(expected);return result; | SAFE | F1 I1 W1 (R112) |  |
| utv_engine_dependencies.c:14 | fatal | rx_utv_engine_dependency: if(!site(ra,curves,4)//expected!=entry+4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| utv_engine_dependencies.c:23 | fatal | rx_utv_engine_dependency: if(!site(ra,lerps,4)//expected!=entry+4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| utv_engine_dependencies.c:26 | fatal | rx_utv_engine_dependency: cleanup=12;if(!site(ra,torques,8)//expected!=entry+cleanup)abort(); | SAFE | F1 I1 W1 (R112) |  |
| utv_engine_dependencies.c:29 | fatal | rx_utv_engine_dependency: if(ra!=0x4d4186//expected!=entry+4)abort();phys_security_check_cookie(fx_reg(FX_ECX)); | SAFE | F1 I1 W1 (R112) |  |
| utv_engine_dependencies.c:30 | fatal | rx_utv_engine_dependency: }else abort(); /* Closed audited static sites; never a substitute return. */ | SAFE | F1 I1 W1 (R112) |  |
| utv_engine_dependencies.c:31 | fatal | rx_utv_engine_dependency: if(rx_sp()!=entry//expected!=entry+cleanup)abort(); | SAFE | F1 I1 W1 (R112) |  |
| utv_engine_dependencies.c:35 | fatal | rx_utv_engine_power_dependency: rx_addr entry=rx_sp();if(expected!=entry+4//U32(entry,0)!=0x4d394c)abort(); | SAFE | F1 I1 W1 (R112) |  |
| utv_engine_dependencies.c:41 | fatal | rx_utv_engine_power_dependency: if(rx_sp()!=entry)abort();for(unsigned i=0;i<3;i++)fx_set_reg((int)i,regs[i]); | SAFE | F1 I1 W1 (R112) |  |
| utv_force_gear_dependencies.c:11 | fatal | rx_utv_force_gear_curve: if(entry+4!=expected//!rx_family_gear_site(ra))abort(); | SAFE | F1 I1 W1 (R112) |  |
| utv_force_gear_dependencies.c:17 | fatal | rx_utv_force_gear_curve: if(rx_sp()!=entry)abort(); /* Typed owner itself has no guest RET. */ | SAFE | F1 I1 W1 (R112) |  |
| utv_response_curve_dependencies.c:11 | fatal | ?: uint32_t rx_response_native_curve_count(unsigned i){if(i>=4)abort();return site_counts[i]; | SAFE | F1 I1 W1 (R112) |  |
| utv_response_curve_dependencies.c:16 | fatal | rx_utv_contact_response_curve: if(!found//expected!=entry+4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| utv_response_curve_dependencies.c:27 | fatal | rx_utv_contact_response_curve: if(rx_sp()!=entry)abort();rx_set_sp(expected);return result; | SAFE | F1 I1 W1 (R112) |  |
| utv_response_frames.c:10 | fatal | rx_utv_response_clamp_frame: rx_addr entry=rx_sp();if(U32(entry,0)!=0x4cdd39//U32(entry,4)!=s//expected!=entry+8)abort( | SAFE | F1 I1 W1 (R112) |  |
| utv_response_frames.c:17 | fatal | rx_utv_response_clamp_frame: if(rx_sp()!=entry)abort(); | SAFE | F1 I1 W1 (R112) |  |
| utv_response_frames.c:24 | fatal | rx_utv_response_velocity_frame: rx_addr entry=rx_sp();if(U32(entry,0)!=0x4cddae//expected!=entry+4//fx_reg(FX_EDX)!=m//fx_ | SAFE | F1 I1 W1 (R112) |  |
| utv_response_frames.c:28 | fatal | rx_utv_response_velocity_frame: if(rx_sp()!=entry)abort();int32_t count=I32(m,0x1c); | SAFE | F1 I1 W1 (R112) |  |
| utv_response_frames.c:35 | fatal | rx_utv_response_helper_frame: rx_addr entry=rx_sp();if(U32(entry,0)!=0x4cdd4a//expected!=entry+4//fx_reg(FX_EDI)!=s)abor | SAFE | F1 I1 W1 (R112) |  |
| utv_response_frames.c:36 | fatal | rx_utv_response_helper_frame: rx_utv_contact_response_helper_exact(s);if(rx_sp()!=entry)abort();rx_set_sp(expected); | SAFE | F1 I1 W1 (R112) |  |
| utv_response_roles.c:11 | fatal | ?: rxcore: UTVvf15 typed role contract %u\n | SAFE | F1 I1 W1 (R112) |  |
| vehicle_atv_telemetry.c:74 | indirect | telemetry_virtual_float: `target <- U32(U32(v,0),off)` | SAFE | L1 F1 M1 (R101) |  |
| vehicle_atv_telemetry.c:111 | indirect | rx_vehicle_forward_angular_velocity: `target <- U32(U32(v,0),0x98)` | SAFE | L1 F1 M1 (R101) |  |
| vehicle_fourwheel_ground_place.c:93 | fatal | fw20_reset: if(!f) abort(); | SAFE | F1 I1 W1 (R112) |  |
| vehicle_fourwheel_probes.c:47 | fatal | utv_probe_kernel: if(observed){if(!live//fx_reg(FX_ECX)!=s)abort(); | SAFE | F1 I1 W1 (R112) |  |
| vehicle_fourwheel_probes.c:49 | fatal | utv_probe_kernel: if(s<entry&&s+0x2100>F-4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| vehicle_fourwheel_probes.c:119 | fatal | probe_enter: if(!live//fx_reg(FX_ECX)!=s)abort(); | SAFE | F1 I1 W1 (R112) |  |
| vehicle_fourwheel_probes.c:121 | fatal | probe_enter: if(s<f.entry&&s+0x5600>f.F-4)abort(); | SAFE | F1 I1 W1 (R112) |  |
| vehicle_utv_runtime.c:106 | fatal | rx_utv_project_body_positions_exact: rx_addr entry=rx_sp();if(fx_reg(FX_ECX)!=s)abort(); | SAFE | F1 I1 W1 (R112) |  |
| vehicle_utv_runtime.c:108 | fatal | rx_utv_project_body_positions_exact: if(rx_sp()!=entry)abort(); | SAFE | F1 I1 W1 (R112) |  |
| waypoint_activity.c:230 | abort | rx_waypoint_activity_vf2: waypoint championship introduction setup | SAFE | C1 (R81) |  |
| waypoint_activity.c:352 | abort | rx_waypoint_activity_vf18: waypoint championship restart | SAFE | C1 (R81) |  |
| waypoint_activity.c:440 | abort | rx_waypoint_state_map_destroy: invalid activity state map proxy | SAFE | I1 (R18) |  |
| waypoint_activity_runtime.c:92 | abort | rx_waypoint_activity_vf42: session static initializer has not run | SAFE | S1 (R15) |  |
| waypoint_activity_runtime.c:118 | abort | rx_waypoint_activity_vf42: session static initializer has not run | SAFE | S1 (R15) |  |
| waypoint_driver_calls.h:7 | abort | waypoint_behavior_update: Waypoint behavior float update owner is not composed | SAFE | L1 (R83) |  |
| waypoint_driver_calls.h:13 | abort | waypoint_rule_update: Waypoint rule float update owner is not composed | SAFE | L1 (R83) |  |
| waypoint_finish_scenario.c:70 | abort | rx_waypoint_activity_vf7: session static initializer has not run | SAFE | S1 (R15) |  |
| waypoint_finish_scenario.c:84 | abort | rx_waypoint_finish_vf3: series static initializer has not run | SAFE | S2 (R16) |  |
| waypoint_finish_scenario.c:123 | abort | rx_waypoint_finish_vf0: unported Waypoint finish selection tail target | SAFE | L1 (R63) |  |
| waypoint_navigation.c:13 | abort | rx_waypoint_navigation_vf6: actual waypoint navigation IDIV fault | SAFE | I1 (R84) |  |
| waypoint_scenario.c:17 | abort | rx_waypoint_chief_clip: invalid waypoint chief clip switch index | SAFE | I1 (R18) |  |
| waypoint_scenario.c:27 | abort | rx_waypoint_camera_clip: invalid waypoint camera clip switch index | SAFE | I1 (R18) |  |
| waypoint_scenario.c:49 | abort | rx_waypoint_intro_vf0: session static initializer has not run | SAFE | S1 (R15) |  |
| world_body.c:839 | fatal | rx_world_body_record_vector_insert: rxcore: vector too long (std::length_error, 00851560)\n | SAFE | I1 F1 (R114) |  |
| world_phys.c:48 | indirect | call_listeners: `U32(U32(listener, 0), slot)` | SAFE | L1 F1 M1 (R109) |  |
| world_phys.c:96 | indirect | sim_step: `U32(U32(sim, 0), 4)` | SAFE | L1 F1 M1 (R109) |  |
| world_rider_62.c:948 | fatal | rx_world_rider_animation_vf19: if(rx_sp()!=override_sp-8)abort();rx_set_sp(override_sp); | SAFE | I1 F1 (R114) |  |
| world_util.h:106 | indirect | world_vcall_f: `target <- U32(U32(obj, 0), off)` | SAFE | L1 F1 M1 (R109) |  |
| world_util.h:134 | indirect | world_call_ctx: `fn` | SAFE | L1 F1 M1 (R109) |  |
| world_util.h:140 | indirect | world_call_ctx: `fn` | SAFE | L1 F1 M1 (R109) |  |
| world_veh_damage_register.c:74 | fatal | stream_open: rxcore: damage part description of %u characters\n | SAFE | I1 F1 (R114) |  |
| world_veh_damage_register.c:151 | abort | vec16_push: a collision composite's vector full (reserved for every box) | SAFE | V1 (R12) |  |
| world_veh_damage_register.c:159 | abort | names_push: a composite's name vector full (reserved for every box) | SAFE | V1 (R12) |  |
| world_veh_damage_register.c:278 | abort | rx_physics_entity_attach_body: a part entity's previous tracking entry freed | SAFE | V1 (R13) |  |
| world_veh_damage_register.c:416 | abort | rx_damage_split_part: a part entity's body index (_invalid_parameter_noinfo) | SAFE | I1 (R18) |  |
| world_veh_entity.c:129 | fatal | rx_world_physics_entity_vf19: rxcore: PhysicsEntity::vf19 with an audio emitter (%08x): the audio layer is not part of the product | SAFE | I1 F1 (R114) |  |
| world_veh_entity.c:139 | fatal | rx_world_physics_entity_disable: rxcore: PhysicsEntity::vf2 with an audio emitter (%08x): the audio layer is not part of the product\ | SAFE | I1 F1 (R114) |  |
| world_veh_entity.c:312 | abort | rx_world_keyed_records_remove: the records' destructors | SAFE | G1 P1 (R69) |  |
| world_veh_entity.c:379 | fatal | rx_physics_entity_destroy: rxcore: PhysicsEntity destructor with an audio emitter (%08x): the audio layer is not part of the pr | SAFE | I1 F1 (R114) |  |
| world_veh_transfer.c:132 | abort | rx_dyn_records_remove: the records' destructors | SAFE | G1 (R68) |  |
| zlib_lifecycle.c:25 | fatal | rx_guest_zlib_rtc: rxcore: zlib RTC stack imbalance %08x/%08x; original _RTC_Failure008faa57 unavailable\n | SAFE | I1 F1 (R114) |  |
| zlib_lifecycle.c:53 | indirect | callback: `target` | SAFE | L1 F1 M1 (R109) |  |
| zlib_lifecycle.c:60 | fatal | callback: rxcore: native zlib callback altered guest callee ESP\n | SAFE | I1 F1 (R114) |  |
| zlib_window.c:35 | indirect | window_alloc: `callback <- U32(stream,0x20)` | SAFE | L1 F1 M1 (R109) |  |
| zlib_window.c:36 | fatal | window_alloc: rxcore: updatewindow callback callee ESP changed\n | SAFE | I1 F1 (R114) |  |
