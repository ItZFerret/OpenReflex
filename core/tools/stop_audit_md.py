#!/usr/bin/env python3
"""stop_audit_md.py [out.md] -- regenerates core/plan/STOP_AUDIT.md: the classification of every site stop_audit.py lists.

The classification is the RULES table below (first matching rule wins; a site no rule matches is reported on stderr and written as
UNCLASSIFIED, so a new stop added to core/game after this audit shows up the next time the generator runs). Every rule carries its
evidence (keys into EVIDENCE) and, for NEEDS-PORT, the addresses with their sizes. The dynamic evidence (the product run once per event
the Unity shell offers, the fuzz / trick / end-of-race runs) comes from stop_audit_run.c / stop_audit_matrix.py; see EVIDENCE.
Classes: SAFE (all reachable targets are ported, or the path cannot be reached offline), OUT-OF-SCOPE (online, split-screen, Steam
presence, developer console / debug keys only), NEEDS-PORT (single-player normal play can reach an unported function or an unclosed path)."""
import os, re, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stop_audit as SA

R = SA.R
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(R, "core/plan/STOP_AUDIT.md")

EVIDENCE = {
 "S1": "the session static initialiser 006de1f0 runs at session create (build_mxgame_startup.c:62 race_static_dcf304); guard 0xdcf304 is set before any frame",
 "S2": "the series manager 005b2370 is constructed by the front-end world's FreeRide Pre (lldb backtrace rx_activity_can_start -> rx_fx_series_manager) before any event; every other first use constructs it lazily",
 "O1": "PlayerMode names 0 SplitScreen / 1 SinglePlayer / 2 Online / 3 LAN (0x9dd584); rx_event_spec.player_mode = \"SinglePlayer\" (race_misc_game.c rx_game_player_mode)",
 "N1": "offline the Quazal session objects are absent: [0xda6cb0]+0x1c4c == 0, the lock [0xda6d08] == 0, driver +0xef0 == 0 (QUEUE.md w-net section)",
 "I1": "invariant: a std::length_error / checked-iterator / index / allocation failure of the MSVC library code the original also terminates on; not a game state",
 "A1": "the session API runs one event per session and forces the MXGame state to 2 then 3 (api/rx_session.c:517,614); Unity restarts by creating a new session (OriginalWorldRuntime.cs:1076 Restart -> Create); no API reaches states 0/1 or the UI-world switch; states 9 / 10 are reached only by the Heat series' activity-area re-creation between its rounds (rx_session_career_loading_enter -> 005a8030 -> Game_StartAAACreation 0075a380, w-heat)",
 "H1": "career composition: the API installs every dependency before a career event starts (rx_career_session_start_owned, api/rx_session.c:729; career_composition.c); only a host that rejects a request reaches the abort; the series admitted by cp_series (api/career_public_session.inc) are the finite SingleEvent set + Moto14 + the Heat rows 41-46 (w-heat)",
 "H2": "replay host bridge: installed by ReplayWorker for career launches and, since w-replay (2026-10-26), for every exhibition event of the open front end (rx_session_replay_begin_event before rx_session_event_load; OpenReflexSimulation replayPresentation, OriginalWorldRuntime.TickExhibitionReplay); the vtable checks only fail for the abstract bases (_purecall 008f9fd6), every concrete vehicle / camera / simulation class registers the slot",
 "R1": "startup-only: the registry / Engine.bxml / MXTables loaders run once at session create from the install's fixed files; session create succeeds on the installed data (every run of the matrix)",
 "R2": "schema: the installed tables use column types int / string / float only (MXTables_Default + DLC: 376 / 56 / 20 columns); the aborts are for column kinds 0 / 5 / 6",
 "V1": "invariant of the vehicle damage registration (world_veh_damage_register.c): the vectors are reserved for the box count and the part entity is new; verified per call by the w-ctx probe cases splitpart / parse / extract on the live UTV, buggy and truck entries (0 mismatches)",
 "D1": "developer debug keys / debug flag 0xae1b90 (not reachable by the shell: the registry's AI-override and debug keys)",
 "M1": "dynamic, re-run 2026-10-26 on main 115312f (agent w-matrix; core/tools/stop_audit_matrix.py --life 200 --career, 5 jobs): the whole life of every event the Unity shell offers: every EventTypeLocations row of MXTables_Default + the 6 DLC levels x every vehicle class of the type (National 70, Supercross 70, Powercross 24, Terracross 84, Waypoint 78, Freestyle 24, Section Race 30, Free Ride 3) + the Free Ride challenges on all 17 shipped rows (HillClimb / Trailblazer tracks 0-2 on FR_Alpine and FR_Cheyenne, track 0 on FR_Gibson, TargetPractice x 3) + Free Ride and the challenges again with the UI property InCareerMode (rx_session_event_in_career_mode): 420 runs, each loaded, run to RACING, forced into WindDown 200 frames later (the activity's own SetState; 450 frames of Post with the replay host installed and the driver acting as the renderer, STOP_AUDIT_REPLAY=1), the activity torn down (E3) and the session destroyed. Result: 412 reach RACING (the other 8 are the HillClimb tracks 1 / 2 of FR_Alpine / FR_Cheyenne, plain and with InCareerMode, which end in Post at frame 69 before RACING: the ORIGINAL does the same, race state 4 at the world start of a baton boot, builds/w-matrix/hcworld.sh), 420 reach WindDown / Post, 420 complete (RESULT ok, session destroyed). The first pass of the day (development library, nothing aborts) stopped 18 runs in three places, all fixed by w-matrix (00559a00 Free Ride Post, 00410bc0 / 0040e550 teardown of the HillClimb / TargetPractice areas); the 40 Free Ride / challenge rows were re-run on the product objects (rx_call_original aborts) with the fixes: 40 of 40 complete. 22 natural finishes (no forced state, 17000 frames cap): Freestyle 8 and Section Race 5 end at their time limit and play the replay, TargetPractice 3 and HillClimb 3 at theirs, Trailblazer 3 runs 17000 frames without finishing (it ends at its finish trigger or off the boundary)",
 "F1": "dynamic, re-run 2026-10-26 (w-matrix; development library, an unported call is logged and the ride goes on): 72 fuzzed rides of 12000 frames (random steering / brake / trick keys / camera / reset: wrecks, resets, wheelies; throttle held most of the time) with MX, ATV, UTV, super buggy and the two truck profiles (factory types 0 / 1 / 7 / 5 / 3 / 3): National, Supercross, Powercross, Terracross, Waypoint and Free Ride (36 rides) ride the 12000 frames; Freestyle and Section Race (12 rides, no replay host) finish by themselves at their 60 s limit (frame 4158-4721) and stop at Post in 006ab6d0, the known host requirement (H2); the same 12 with the replay host (the driver acts as the renderer) play the reel to frame 12000; Free Ride with InCareerMode (6) and HillClimb track 1 / Trailblazer track 2 / TargetPractice with InCareerMode (MX + ATV, 6; the activation areas polled each frame, no random ride entered one: the move / drive tests of E3 do): 0 unported calls, 0 stops",
 "T1": "dynamic: every default aerial trick (36) started on an airborne rider through the game's own 006c98c0 (MX, ATV, UTV profiles); the unported targets it reached are listed in NEEDS-PORT",
 "E1": "dynamic: forced WindDown / Post (rxtrace_set_state 3 / 4) on National, Supercross, Powercross, Terracross, Waypoint with the clip recorder off: no stop; with the recorder on (the default) the replay bridge stopped (closed by w-replay, E2)",
 "E2": "w-replay (2026-10-26): the same forced WindDown (state 3 at race frame 1500; the activity's own WindDown timeout enters Post) with the exhibition replay presenter installed (stop_audit_run STOP_AUDIT_REPLAY=1, acting as the renderer: every request applied and painted), on the product objects (rx_call_original aborts) and on the trace build: National / Supercross / Powercross / Terracross reach Post sub-state 2 at frame 1681, Waypoint at 2011, the replay reel plays to the end of the run (4329 / 3999 frames with the replay camera valid, 14-20 clips, 0 unported calls) and the session stays healthy; without the presenter National stops at frame 1680 as before. Unity: tests/moto_career_flow run.py --arcade (arcade National from the event browser, natural 1-lap finish, the results over the playing replay, its requests painted and drained, RETURN TO MENU with the final save): PASS",
 "E3": "w-matrix (2026-10-26): teardown and the next event. The session API runs one event per session (A1); the 2nd event of a 'Continue' is a new session in the same process. core/tools/stop_audit_twice.c runs two events in one process: create, load, race, the live activity's teardown (vf3, vf6(0), vf0(1): the development hook rxtrace_teardown, the sequence of oracle/dtor_probe.cpp case a), rx_session_destroy, create, load, race, teardown, destroy: 14 pairs (National -> Freestyle -> Section Race -> HillClimb -> Trailblazer -> TargetPractice -> Waypoint -> Terracross -> Free Ride -> Supercross -> Powercross -> National, Freestyle x 2, Section Race x 2, HillClimb x 2) on the product objects: 28 of 28 events end with the destroy and the next build starts; the same teardown runs after every one of the 420 matrix runs (E3 column TEARDOWN / ENDED). activation_native_test (move, HillClimb / Trailblazer / TargetPractice): Free Ride InCareerMode -> the area entered (rx_session_activation) -> the challenge as a new session -> back to Free Ride, PASS. Found and fixed: the area entities ScrollingTextureEntity::sdtor 00410bc0 (+ 00410c10) of the HillClimb areas and MXTargetPracticeCollectable::sdtor 0040e550 of the TargetPractice areas (w-teardown's class census covered the Free Ride / Waypoint area subscenes only, not the challenge areas)",
 "TD1": "w-testride (2026-10-27): the garage's TEST DRIVE (api/test_drive_session.inc, game/garage_test_drive.c) runs on a session whose MXGarage (MXGame + 0x15570) was never loaded: its vehicle fields +0x524 / +0x528 are 0 (core/tools/test_drive_test.c: start, ride, end on MX and ATV, no stop); the oracle boot REFLEX_TEST_DRIVE reaches the original 007611b0 only through the original garage's own vehicle",
 "B1": "documented product boundary (UI / audio / network / render): the product skips it and nothing the game reads depends on it (core/plan/QUEUE.md; comment at the call)",
 "C1": "career admission: only SingleEvent series (+ Moto14 and the authored Supercross 33 / Powercross 68 / Waypoint) are offered (cp_series supported flag); championship / heat / series activity types >= 0x1f never start in a session",
 "X1": "scene / asset data of the installed levels: every level of the shell loads (matrix) so the unbuilt-class aborts do not fire; the entity types named never occur in an activity area of an installed level",
 "L1": "family query (stop_audit.py --slot, _re/out/vtables.tsv): every concrete class of the receiver family that the core constructs registers the slot; only the abstract bases (_purecall stub 008f9fd6) are unregistered",
 "Z1": "all literal targets passed through this helper are registered (checked by resolving each caller's argument)",
 "W1": "w-collide (2026-10-26): every generator of the 0085ddd0 case table is a core port (the last eight verified by oracle/phys_pair_probe2.cpp, 0 mismatches); the observed dispatch runs every pair role through the table (phys_contact_pair_observed). The four-wheel rides of the vehicle matrix (UTV / super buggy / ProLite / Pro2 on TRX_Verde, TRX_Halifax, PX_Laguna and TRX_Lynchburg x 3 fuzz seeds, 9000 race frames) reach RACING to the end with no stop and no unported call; the Lynchburg rides run the two former stop roles 0086e750 @ 0085e4ce and 0086f3d0 @ 0085eb79",
 "P1": "w-partdtor (2026-10-27): the vehicle damage parts\u0027 destruction (0047bbe0 -> 00479dd0 -> PhysicsEntity::sdtor 00403bb0 / 0041c7b0 -> 00402360 -> 00416fe0 / 00417830 / 00417c80 / 0076a9a0 / 0076aa50, Database vf22 0084ce00, the shape handle vf1 00849630) verified per call by core/oracle/wave_ctx_probe.cpp cases release / relentry / vehclear / partmove / keyedrm on truck / buggy / UTV worlds; headless stop_audit_run STOP_AUDIT_TEARDOWN=1 STOP_AUDIT_FREE_PLAYERS=1 (Powercross UTV / super buggy / trucks, 4 riders): the players freed, every entry released, 0 unported calls",
 "S1": "w-sfx (2026-10-27): sound_state.c vget() calls a virtual getter only when rx_fn(target) says the core has it (a registered port); an unregistered target is never called (the query counts a miss and returns its default), so no unported call can be reached from it",
 "N2": "w-npcs (2026-10-27): the stands crowd sections (BillboardCrowdEntity 0x1041) are built by the core since build_scene_crowd.c; verified per call in baton boots of SX_Kingston (66 sections) by core/oracle/build_scene_crowd.cpp (ctor, InitFlashes: 0 mismatches), the destructor by dtor_probe case b (72 live sections of SX_Springfield deleted, 72 verified, 0 mismatches), vf5 against the original\u0027s live objects by core/oracle/crowd_probe.cpp (SX_Kingston 66, NAT_Concord 20, FMX_BuenaVista 120 entities: every non-pointer word identical)",
 "G1": "w-teardown (2026-10-26): core/oracle/dtor_probe.cpp with the collision shapes UNPINNED runs vf3 / vf6 / vf0 of the live activity (Free Ride, Waypoint, race), the vehicle, rider, scenario and camera entities in the five worlds + FreeRide_AI_Cheyenne / WaypointRace_FR_Gibson / FreeRide_FR_Alpine under Verify, 0 mismatches; the REFLEX_ORIG_CENSUS minus the probe's own calls lists only the boundaries documented in QUEUE.md (audio registries [0xa95824] / [0xa95828] vf1, UI event-handler proxies, the entity classes outside_core in build_scene.c); the shape classes of the world's registry plus made-up StaticRays / Triangles / Composite / StaticComposite / DynamicComposite objects are deleted with flags 0 / 1 and the allocator lock flag 0 / 1 (shape_dtors: 0 mismatches)",
 "Q1": "the product's results page start (race_results_table.c rx_results_page_show, w-resultsrng / w-wpfill) admits only the offline activity types whose filler is ported: the race types 0 / 1 / 0x11 / 0x13 (the Eliminator's place column 005dc7a0 included), Freestyle 5, Waypoint 6 and Section Race 0xc, offline; rx_app_populate_results_table (00742c40 -> 00613dd0 -> 00614890) has no other product caller than rx_results_page_frame, so the party modes' fillers (types 9 / 10 / 0xe / 0xf: Tag, Snake, Infection, Graffiti, online only) are never reached",
}

# a rule: (kinds, file regex, function regex, message/target regex, class, evidence keys, text, addresses, priority)
RULES = []
def rule(kind, f, fn, msg, cls, ev, text, addrs="", prio=99):
    RULES.append((kind, re.compile(f), re.compile(fn), re.compile(msg), cls, ev, text, addrs, prio))
ANY = ".*"

# w-rng: the start-line keys of <Audio><EngineSoundEmitter> (00684c10) through the engine context's configuration (+0xc): its vf1 / vf17 /
# vf19 are the registry's (build_registry.c rx_registry_vf1_find 008241f0, rx_registry_vf17_floats 00824640, reg_vf19_ints 008245c0), all
# registered; the configuration is the only object at [[0xa95844] + 0xc]
rule("indirect", r"sound_state\.c", r"vget", ANY, "SAFE", "S1",
     "the getter is called only for a registered target (rx_fn); otherwise the default is returned")
rule("indirect", r"engine_startline\.c", r"cfg_find|cfg_read", ANY, "SAFE", "T1",
     "the engine emitters' start-line tunables (00684c10's StartLine keys): the configuration's vf1 find 008241f0, vf17 floats 00824640 and vf19 ints 008245c0 are ported and registered (build_registry.c); the fallback never runs")
# ------------------------------------------------------------------------------------------------------------ NEEDS-PORT
rule("indirect", r"anim_animators\.c", r"performance_apply_clip", ANY, "SAFE", "T1 Z1",
     "PerformanceAnimator::vf4 (0083e290) applies the up to 4 additive clips (0083e930, PORTED 2026-10-26 by w-stunts: anim_animators.c rx_performance_additive_clip, 114 + 16 Verify calls, 0 mismatches) and 4 override clips (0083eff0); "
     "the remaining rx_call_original_ex in performance_apply_clip is the override clip, which is unreachable: no performance asset has override=true (scan of all 259 perfdat assets)",
     "0083eff0 (1448 B) not reached", 1)
rule("indirect", r"loop_player\.c", r"pl_run", ANY, "SAFE", "T1",
     "pl_run is the always-fatal rx_call_original_ex helper of loop_player.c; rx_delegate_invoke (00554fc0, the shared vf2 of the player's MX event-handler proxies) reaches it through pl_call only for a proxy whose member function is not ported. "
     "The player's stunt-channel listeners registered at build_activity_player.c:207-220 are ALL ported now (2026-10-26, w-stunts: StuntLanded 0054d310, StuntHoldBegin 0054d1a0, StuntHoldEnd 0054d1e0, StuntTakeoff 0054d360 in loop_player.c with 00621fe0 / 006223e0 in anim_rider_play.c; "
     "stunt_events_probe: 6 ports x 60 Verify calls, 0 mismatches). The other pl_run users are literal ported targets",
     "", 2)
rule("indirect", r"race_activity\.c|powercross_countdown\.c|fx_world\.c|build_event_proxies\.c", r"rx_event_proxy_call3|rx_event_proxy_call4|rx_powercross_proxy_call|rx_fx_event_proxy_call|rx_event_proxy_call_member_int_float", ANY, "SAFE", "L1 M1 F1",
     "event-handler proxies of the activity channel call the member function stored at proxy+0x10 through rx_call_original (registered ones work); the callbacks registered and NOT ported are: Network* handlers 0055c970 0055cdf0 0055ce10 0054b950 0054b9d0 0054bb90 0054ff30 0050eaa0 00512990 (online), "
     "the Waypoint audio-cue handlers 005834f0 00583520 (WaypointActivityAudioLogic::vf3 subscribes them only when the activity's audio map holds the cue), the race cue handlers 00582d70 (RaceActivityAudioLogic's 'Restart', subscribed by every race audio logic's vf29 00582ed0 and raised by Activity::vf16 00557b40: the pause menu's RESTART, rx_session_restart_activity -> 00759d00 -> 0055f670; ported with ActivityAudioLogic::vf32's cue map walk 00581077) 00582e00 00582e20 00582e60 00583a70 00583fb0 are ported (race_activity_audio.c, w-sfx), "
     "VehicleWrecked/VehicleStopWreck driver handlers 0050c8a0 0050c8d0 (FreeRide drivers; no raiser in the core) and 005234f0 (BehaviorFixForwardProgress; wrecks occur in every fuzz run without a stop), 'Reset Ghost Overlays' thunk 006137b0 (EnteredTimedSection 00569550 is ported since: race_outrun.c), save-game handlers 00754d10 / 00754430 (Storage channel: never raised), the player stunt listeners (NEEDS-PORT row, loop_player.c pl_run)")

# ------------------------------------------------------------------------------------------------------------ abort rules
# w-ctx (2026-10-26): the sites the generator printed as UNCLASSIFIED
rule("abort", r"race_section(_driver)?\.c", r"rx_section_(race_vf2|race_vf3|race_vf23|race_vf42|driver_entered)", r"ONLINE", "OUT-OF-SCOPE", "O1 N1",
     "the networked section race (NetSectionRace link, its teardown, results and section times): online only; the offline section race has no network link ([0xda6cb0]+0x1c4c == 0)")
rule("abort", r"build_activity_race\.c", r"rx_race_stub_vf5", r"poker race audio", "OUT-OF-SCOPE", "O1",
     "activity type 0xd is PokerRace, a party mode reserved for the future online framework: no EventType row offers it (MXTables_Default EventTypeLocations has no PokerRace) and the session API cannot name it")
rule("abort", r"build_mxgame_init\.c", r"net_type", r"Network event-type instantiation", "OUT-OF-SCOPE", "O1 N1",
     "the table k_net_types lists every instantiation the Network manager's listen functions (007071a0 .. ) call; the manager exists only for an online session")
rule("abort", r"net_boundary\.h", r"rx_net_call(_ex)?", ANY, "OUT-OF-SCOPE", "O1 N1",
     "the documented online boundary helpers: fatal in the product by design (their header names the guard that keeps each caller away); the census lists their targets as boundary:online")
rule("abort", r"build_activity_clear\.c", r"rx_vehicle_audio_release", r"vehicle audio slot is assigned", "SAFE", "B1",
     "the slot (rider sound bank +0xa70 .. , 00680d37) is assigned only by the vehicle foley hook 00680b10, which build_activity_spawn.c calls under RX_ORACLE only (the audio boundary: no audio system exists in the product, world_util.h AUDIO_BOUNDARY)")
rule("abort", r"build_activity_teardown\.c", r"rx_scene_subscene_unload", r"whole scene's unload", "SAFE", "A1",
     "007d19f0 without a subscene name is the whole scene's unload of the engine's shutdown / level swap; the only call in the core passes the activity's subscene name (build_activity_teardown.c, +0x118) and the session ends by rx_session_destroy, not by a scene unload (A1)")
rule("abort", r"world_veh_damage_register\.c", r"vec16_push|names_push", r"vector full", "SAFE", "V1",
     "the composite's vectors are reserved for exactly the box count the loop runs to (008494a0 / 008496d0 / 005ba080 at construction), so push_back never reaches the reallocating branch")
rule("abort", r"world_veh_damage_register\.c", r"rx_physics_entity_attach_body", r"tracking entry freed", "SAFE", "V1",
     "a part entity is new: its tracking entry +0x158 is 0 when the body is attached, and the entity itself holds the entry's reference from then on, so neither release can reach zero")
rule("abort", r"race_challenge_payout\.c", r"table_of", r"authored table missing", "SAFE", "R1",
     "the payout script reads tables of the install's MXTables_Default (loaded at session create, R1); a table missing there would stop the session before any event")
rule("abort", ANY, ANY, r"session's static initialiser|session static initializer|job pool's static constructor", "SAFE", "S1", "guard bit clear: the unreached branch is the original's lazy initialisation")
rule("abort", ANY, ANY, r"series manager's static initialiser|series static initializer", "SAFE", "S2", "guard 0x9e275c bit clear")
rule("abort", ANY, ANY, r"networked session's lock|networked session|network player's state|NetworkNoPlayers|remote-state|network state|Network state|NetworkDriver publish|network ownership|online|Online|player joining an online|quick online|online race|online world|online level|online load|online profile|online players|online activity|online / playlist", "OUT-OF-SCOPE", "O1 N1",
     "online / LAN session or a Quazal network object (PlayerMode 2 / 3, [0xda6cb0]+0x1c4c, lock 0xda6d08, driver +0xef0)")
rule("abort", ANY, ANY, r"_invalid_parameter_noinfo|_Xlen|too long|length_error|_Xran|self-assign|invalid iterator|end iterator|iterator proxy|invalid viewport list|erase\(end\)|index$|cell index|column index|state map proxy|StringMap_Find iterator|random_shuffle of more|types vector growth|slot 0 of a null|allocation|overflow|dependency stack|property key", "SAFE", "I1",
     "checked-iterator / length_error / index / allocation invariant of the MSVC library code")
rule("abort", r"build_scene_crowd\.c", r"rx_billboard_crowd_vf5", r"Crowd shader asset", "SAFE", "X1 N2",
     "BillboardCrowdEntity::vf5 006c6180 reads the \"Crowd\" shader asset of the level (006c62b6 dereferences its handle: the original crashes without it); every installed level whose scene holds a BillboardCrowdRSData has it in its packages (core/tools/ambient_census.c over the 41 race levels: every crowd section built, 0 aborts)")
rule("abort", r"build_registry\.c|build_tables_load\.c", ANY, ANY, "SAFE", "R1", "XMLBuilder / registry / table-XML loader branch the installed Engine.bxml registry files and MXTables do not take")
rule("abort", r"build_tables_rdb\.c", ANY, r"column type 0 / 5|column type|type 0 / 2 / 5 / 6|variant type|criterion value type 5|float index|8-byte column", "SAFE", "R2", "table column kind 0 / 5 / 6 / variant type 5 (no installed table has one)")
rule("abort", r"build_tables_rdb\.c", ANY, ANY, "SAFE", "R1 I1", "table / index / row-set operation path (hinted insert fallback, erase of the end iterator, copy of a non-empty row set, unique lookup of a non-string column ...) that the reference boot and every matrix run (profile, Player tables, activity table) never take; STL-level invariants")
rule("abort", r"build_mxgame.*\.c", ANY, r"Registry virtual not ported|Quazal allocator|FMODEvent with a name|MusicPlayer with a track|save slot with contents|vector too long", "SAFE", "B1 R1", "startup / audio / save-slot boundary: no name, no track, no saved slot exists at session create (the product starts without audio and with the Unity-side profile)")
rule("abort", r"build_activity_game\.c", ANY, r"front end's start|start in a level|UI world's switch|event's restart|InputNames_Init|Lua state's static|downloadable content lookup", "SAFE", "A1 B1",
     "MXGame_Update states 0 / 1 (init thread), 9 / 10 (event restart), the UI world switch and the Lua / InputNames boundaries: the API sets the state to 2 then 3 itself and restarts by a new session; InputNames_Init and the Lua VM are product boundaries (lead decision)")
rule("abort", r"build_activity_event\.c", r"rx_start_event", r"viewing a track", "SAFE", "A1", "StartEvent while the game views a track (debug level list): the API never views a track")
rule("abort", r"build_activity_frame\.c", ANY, r"rumble", "SAFE", "B1", "the rumble effect manager [0xd611d0] is never created in the product (nothing writes it): the condition is false; rumble requests reach Unity through rx_session_rumble")
rule("abort", r"build_activity_grid\.c", r"rx_game_grid_pass", r"mode 6 start zones|free ride's mode 3", "SAFE", "A1 D1", "start zones of ActivityMode 6 (Overtake) / 3 (Ghost): the shell and the career always set the mode property to Exhibition (MXUIManager::SetupEvent default \"Exhibition\", career_properties.c has no ActivityMode key, 005b6ab0 sets \"Exhibition\")")
rule("abort", r"build_activity_grid\.c", r"rx_game_grid_pass", r"start-zone jump-table|Capture start zones", "OUT-OF-SCOPE", "M1 O1",
     "start-zone assignment by activity type: the challenge types 0x15-0x1e are ported (race_challenge_*.c, w-challenge; HillClimb / Trailblazer / TargetPractice run in the matrix); the mini games 9 10 0xe 0xf (0075ff6e: Tag, Snake, Infection, EggHunt) since w-partymodes (2026-10-30); still unported: Capture 0x10 (0075fd87): the Capture party mode has no level data (no MiniGameCTF scene in any installed level) and the factory has no class for it (the next row)")
rule("abort", r"build_activity_grid\.c", r"rx_game_grid_pass", r"mini-game's variants", "SAFE", "O1", "types 9 / 10 (Tag, Snake): online-only mini-games (project memory: Snake, Tag, CTF, Greed, Outbreak are online-only)")
rule("abort", r"build_activity_grid\.c", r"rx_race_grid_from_gate", r"without a starting gate", "SAFE", "M1 X1", "the level's \"<level>Base\" subscene has no TO_StartingGate entity: every race level of the shell has one (all 420 matrix runs reach RACING or Post on all 41 levels); only levels without a gate (none installed) abort")
rule("abort", r"build_activity_grid\.c", r"rx_race_grid_from_track", ANY, "SAFE", "A1 D1", "00564c90 is the start grid of ActivityMode 0 (Ambient) / 2 (Practice): the shell and the career always set the mode property to Exhibition (MXUIManager::SetupEvent default \"Exhibition\", career_properties.c has no ActivityMode key; 005b6ab0 sets \"Exhibition\")")
rule("abort", r"build_activity_race\.c", r"rx_activity_create", ANY, "OUT-OF-SCOPE", "M1 O1 C1",
     "the activity factory 0055ef80 now builds race (0-4 7 0x11-0x13), waypoint (6), free ride (8), Freestyle (5 0x14 and the FMX champ intro / practice / main 0x20 0x3a-0x3c), Section Race (0xc), the challenges (0x15 0x16 0x18 0x19 0x1b 0x1d; 0x17 0x1a 0x1c 0x1e make no class) and the championship types 0x1f-0x39 0x21 0x30 0x31 (2026-10-26: w-freestyle, w-section, w-challenge, w-champ); "
     "and since w-partymodes (2026-10-30) Tag (9: race_tag*.c, MXActivityTag 0056d550, reached from the Tag EventTypeLocations rows FR_Alpine / FR_Cheyenne / FR_Gibson; the online framework's party modes) and PokerRace (0xd); since w-snake (2026-10-30) Snake (10: race_snake*.c, MXActivitySnake 0056b050, the Snake rows FR_Alpine / FR_Cheyenne / FR_Gibson) and Infection (0xe: race_infection.c, MXActivityInfection 00572d00); "
     "and since w-partylast (2026-10-30) Graffiti (0xb: race_graffiti.c, MXActivityGraffiti 00563d90), EggHunt (0xf: race_egghunt.c, MXActivityEggHunt 00574730) and Capture (0x10: race_capture.c, MXActivityCapture 00576290): no installed level holds their scene entities, so the session refuses those events before they load (api/rx_session.c minigame_content_missing, rx_session_event_load 3: R126); the default branch aborts only for types above 0x3c",
     "", 20)
rule("abort", r"build_activity_race\.c", r"rx_activity_array_grow|rx_activity_create", r"activity array grown", "SAFE", "I1", "the manager array has room: one activity per area and type")
rule("abort", r"build_activity_race\.c", r"rx_activity_area_name", r"series manager", "SAFE", "C1", "activity type > 0x1e = championship / series types")
rule("abort", r"build_activity_race\.c", r"rx_race_stub_vf5", r"championship intro", "SAFE", "C1", "RaceChampIntro (type 0x1f): a series event, not admitted")
rule("abort", r"build_activity_race\.c", r"rx_race_stub_vf5", r"the poker race audio", "SAFE", "M1 O1", "PokerRace (activity type 0xd): no EventType names it (EventType table: 32 types, no Poker), the factory has no class for it (the factory row)")
rule("abort", r"build_activity_start\.c", r"act_event_type", ANY, "SAFE", "M1", "an event-type instantiation (template function address) not in build_activity_events.inc: the table lists every instantiation the exe's activity / driver constructors use (all matrix runs build their activities)")
rule("abort", r"build_activity_driver\.c", r"rx_lap_pacing_vf1", r"series activity's lap pacing", "SAFE", "C1", "activity type > 0x1e")
rule("abort", r"build_activity_driver\.c", r"rx_activity_on_player_joined", r"no free race index", "SAFE", "I1", "12 race indices; at most 12 riders (EventRiders <= 12 for the shell)")
rule("abort", r"build_activity_freeride_run\.c", ANY, r"fewer than two 0x1014", "SAFE", "M1 X1", "free ride start slots: the three FR_ levels (Alpine, Cheyenne, Gibson) have >= 2 entities of type 0x1014 in their FreeRide section (3 matrix runs); the challenge sections (\"<level>ChallengeHillClimbA\" ...) run in the matrix (17 shipped rows, M1)")
rule("abort", r"build_activity_freeride_state\.c", ANY, r"discovery collectibles", "SAFE", "C1", "InCareerMode discovery (vf54): the career's Race & Ride free ride; not an admitted series (and the UI property is absent in the product)")
rule("abort", r"build_activity_records\.c", r"gate_start", ANY, "SAFE", "A1 D1", "starting-gate start for a FreeRide (type 8) / ActivityMode 0 or 6 activity or a spectator (A+0xfe8): modes are always Exhibition, FreeRide levels have no 0x1033 gate, SpectatorMode = 0")
rule("abort", r"build_activity_records\.c", r"rx_activity_entities_start", r"0x1053|0x1062", "SAFE", "X1 M1", "scene entities of type 0x1053 (animation variant) / 0x1062 (TrackSystem) inside the activity area's section: none in any installed level (matrix: all levels start)")
rule("abort", r"build_activity_pre\.c", r"rx_activity_vf37", r"replay recorder's frame|session result's entities", "SAFE", "N1 D1", "session replay recorder ([0xda6cb0]+0x1c48, networked) / SpectatorMode result (A+0xfe8, property 0 in the product)")
rule("abort", r"build_activity_game\.c", r"rx_mxgame_update", r"the event quit while its activity area", "SAFE", "A1",
     "MXGame states 9 / 10 (the Heat series' activity area re-created between its rounds, w-heat) with the quit request +0x15b58 set: the core only ever clears +0x15b58 (build_mxgame.c, build_activity_game.c); its writer, the original's quit to the UI world, is not called by the session API (one event per session; Unity leaves an event by destroying the session)")
rule("abort", r"build_activity_game\.c", r"rx_mxgame_update", r"NeedSeriesAdvanceEvent", "SAFE", "H1",
     "state 10 of the Heat re-creation: the UI property NeedSeriesAdvanceEvent is written only by the original's Lua pages; the product's owned property store has no writer of it (no core / api source names the key 0x0094d994 except this read), so the series event's vf9 gate path runs (career_series_test 41-46, the Unity Heat flow)")
rule("abort", r"build_render_asset\.c", r"rx_shader_asset_get_io", r"shader AssetIO made on first use", "SAFE", "R1",
     "XenonShaderAsset::vf2 00794720 on an unset guard [0x00da67f4]: the session's startup registration (build_asset.c rx_database_register_all, at session create) sets every static AssetIO's guard, the shader's included, before any asset exists (w-heat)")
rule("abort", r"loop_player_destroy\.c", r"rx_mxplayer_destroy", r"suspended input", "SAFE", "A1",
     "closed (w-tutcore 2026-10-27): Player_SetInputSuspended 0054f540 is ported (loop_input_suspend.c) and the destructor calls it as the original does (a player destroyed while a page suspends its input gets its groups back). Kept for the rule numbering; no site matches",
     "")
rule("abort", r"build_activity_player\.c", r"rx_mxplayer_init", r"player init with an activity", "SAFE", "A1", "re-initialising a player that still has an activity (P+0x94): one event per session; a new session starts from empty player slots")
rule("abort", r"build_activity_player\.c", r"rx_player_active", ANY, "SAFE", "I1", "player lookup of an empty slot: every caller iterates the active count. Reproduced headless with EventRiders 16 (session stops at frame 0): the game caps the field at 12 (MaxRidersEventType / MaxRidersVehicleClass tables; the shell never offers more); riders 2-12 run (AI rosters 2, 12 checked)")
rule("abort", r"build_activity_player\.c", r"rx_player_from_player_list", r"name plate", "SAFE", "B1", "the name plate text object (P+0xaa8) is never created in the product (render only); in every matrix run the branch is skipped")
rule("abort", r"build_activity_player\.c", r"rx_players_sync", r"second scene active", "SAFE", "A1", "second scene view active: split-screen only (PlayerMode 0)")
rule("abort", r"build_activity_race\.c", r"rx_audio_logic_vf32", ANY, "SAFE", "B1", "vf32's only core callers are the audio logics' constructors (race 005825a0, Free Ride, Freestyle 005623b0), which clear +0x8c just before; the cue map is built later, by vf29 (build_activity_audio.c, w-sfx), and the restart calls vf33 (race_restart.c), not vf32: the branch is not taken")
rule("abort", r"build_activity_race\.c", r"rx_scene_request_subscene", ANY, "SAFE", "M1 X1", "a requested subscene must exist in the level's scene: every level of the shell loads (matrix)")
rule("abort", r"build_activity_race\.c", r"rx_mxworld_set_track", ANY, "SAFE", "M1 X1", "the world's racing line (006a2980/00767850) only when a track entity named after the activity's track has a path (+0x98): none in the installed levels (matrix: all start)")
rule("abort", r"build_activity_race\.c", r"rx_activity_lap_pacing", ANY, "SAFE", "M1 X1", "temp-lappacing.bxml / \"<level>-lappacing\" asset: the level's lap-pacing asset exists for every level (matrix)")
rule("abort", r"build_activity_track\.c", ANY, ANY, "SAFE", "I1", ".tdf07 suffix table index: kinds 0-14 only")
rule("abort", r"build_activity_util\.h", r"act_call_fn", r"more than 5 arguments", "SAFE", "I1", "helper arity: all callers pass <= 5 arguments")
rule("abort", r"build_activity_world\.c", r"db_call|db_vcall", ANY, "SAFE", "I1", "helper arity of the Database virtuals")
rule("abort", r"build_activity_world\.c", r"rx_scene_directory_read|rx_mxworld_load_base", ANY, "SAFE", "M1", "the level's scene file / the front end's Base subscenes exist for every level (matrix; UIScene in the startup)")
rule("abort", r"build_ui_world\.c", ANY, r"TutorialManager", "SAFE", "B1", "closed (w-tutorials 2026-10-27): the TutorialManager (MXUIWorld + 0x22180) is built and updated in the product (fx_tutorial_manager.c); build_ui_world.c has no TutorialManager stop any more. Kept for the rule numbering; no site matches")
rule("abort", r"build_driver_(dtor|reset)\.c", ANY, r"lock|network", "OUT-OF-SCOPE", "N1", "networked session lock")
rule("abort", r"activity_finish_stage\.c|waypoint_finish_scenario\.c", r"rx_activity_finish_stage_is_b|rx_waypoint_finish_vf0", r"finish", "SAFE", "L1", "slot vf10 (0x28) / vf3 (0xc) of the finish scenario: all RaceFinishScenario / WaypointFinishScenario classes register it (FMXFinishScenario vf3 0074de60 included); the RaceFinishScenario (0074a1e0 / 0074a340) and its podium are ported and run in the product (anim_scenario_finish.c, w-podium: Post sub-state 1)")
rule("abort", r"anim_scenario_rider\.c", r"rx_stl_map_erase_range", ANY, "SAFE", "I1 M1", "std::map subrange erase (00705150's loop) of the scenario cast map: the erased range is empty in every matrix run; an erase of a non-trivial range is the career teardown (see the teardown row)")
rule("abort", r"race_activity_stub\.c|gateless_race_start\.c", r"rx_race_stub_kind_setup|fill|next", r"guestSP", "SAFE", "I1", "ABI check of the original's entered ESP")
rule("abort", r"race_driver_reset\.c", ANY, r"static initialiser", "SAFE", "S1", "guard")
rule("abort", r"race_misc_util\.c", ANY, ANY, "SAFE", "I1", "vector<T> too long")
rule("abort", r"world_veh_transfer\.c", r"rx_dyn_records_remove", ANY, "SAFE", "G1",
     "guard for an unregistered records destructor (0042c080 is registered: build_vehicle_proxy.c). PORTED 2026-10-26 (w-teardown): a shape that loses its LAST reference in a dynamic / static record removal is deleted through its scalar deleting destructor: phys_shape_dtors.c 00847770 (base, DynamicRays, DynamicCylinder), 008455a0 (Box / Sphere / StaticBox / DynamicBox / StaticCylinder), 00845570, 00847c60 / 00848ec0 / 00849040 (Composite), 00844970 / 00844860 / 00844a90 / 00844f70, handle 00849830 (+ 00844c50, 00844f90, 00847d50, 00819e40, 0042b280)", "", 60)
rule("abort", r"world_veh_entity\.c", r"rx_world_keyed_records_remove", ANY, "SAFE", "G1 P1",
     "guard for an unregistered records destructor (0042c080 is registered: build_vehicle_proxy.c): the keyed-object twin (00416fe0) of rx_dyn_records_remove, reached when a vehicle damage part entity is deleted (PhysicsEntity::sdtor 00403bb0 -> 00402360)", "", 60)

# 2026-10-26 sweep (w-matrix): the sites added by the Freestyle / Section Race / challenge / teardown merges
rule("abort", r"build_activity_clear\.c", r"rx_vehicle_audio_release", r"vehicle audio slot", "SAFE", "B1 G1", "the vehicle foley slot (VehicleAudioManager [0xa95828] / the audio manager [0x00d92f00] slots) is assigned only by the spawn's oracle-only 00680b10 (build_activity_spawn.c); the product runs no vehicle foley hook, so no slot is ever assigned and the release finds none")
rule("abort", r"build_activity_teardown\.c", r"rx_scene_subscene_unload", r"whole scene's unload", "SAFE", "A1 E3", "007d19f0 without a subscene name = the whole scene's unload (a level switch / leaving the world): the API never switches the world inside a session (a new session is created; core_release resets the arena); the area teardown that a career 'Continue' runs names the area's subscene and is ported (E3)")
rule("abort", r"build_mxgame_init\.c", r"net_type", r"Network event-type instantiation", "SAFE", "R1 M1", "startup only: the table lists every Network event-type instantiation the exe's session init (006de4b0 / 006de1f0) registers; session create runs it once and succeeds on every matrix run")
rule("abort", r"race_freestyle_driver\.c", r"rx_freestyle_driver_vf7", r"AI debug override", "SAFE", "D1", "the AI's freestyle riding (0050d240, 962 bytes) runs only with the AI debug override 0xae1b90 (the registry's AI benchmark switch, 0 in the game and the product); a Freestyle event has no AI riders otherwise")
rule("abort", r"race_section(_driver)?\.c", ANY, r"ONLINE", "OUT-OF-SCOPE", "O1 N1", "the networked section race (NetSectionRace link, results, section time, teardown selection loop): guarded by session_networked() / the network manager's +0x1c3c / +0x1c48 fields, all zero offline")
rule("abort", r"net_boundary\.h", r"rx_net_call(_ex)?", ANY, "OUT-OF-SCOPE", "O1 N1", "the ONLINE BOUNDARY helper (Quazal / online content): every use names its guard; the product has no online session and the guards are false (see the header)")

# career / replay / powercross / scene: host-composed
rule("abort", r"career_.*\.c", ANY, ANY, "SAFE", "H1", "career layer dependency or host-contract guard (unclosed dependency, rejected presentation request, authored-series check)")
rule("abort", r"replay_presentation\.c", r"rx_replay_require_viewport|replay_fade_request", r"viewport request bridge required|fade request bridge required", "SAFE", "E2 H2",
     "race end: rx_post_state(2) (00569a40, every race that reaches Post) -> clip_start_playback(8) -> rx_replay_start 006aa8d0 -> rx_replay_require_viewport / rx_replay_fade_in need the replay host (rx_replay_presentation_install). "
     "CLOSED by w-replay: the host is installed for career launches AND every exhibition event (rx_session_replay_begin_event, before the load); only a host that rejects a request reaches the abort. "
     "00757660 (the viewport request): its viewport list part runs as the scene_viewports.c port and the engine timing's split flag [ctx+0x10]+0x5c (read by rx_fx_tracks_flush) is written; the render targets / screen effects (007575a0, 00757490) stay the renderer's")
rule("abort", r"replay_.*\.c", ANY, ANY, "SAFE", "H2", "replay host bridge / camera / vehicle / simulation vtable checks (abstract bases only) / scene view parameters")
rule("abort", r"powercross_countdown\.c", ANY, ANY, "SAFE", "H1", "Powercross countdown presentation component bound by the Unity host before Countdown Entered")
rule("abort", r"scene_viewports\.c", ANY, ANY, "SAFE", "I1 B1", "viewport records are CPU-only in the product (no GPU owner); allocation failures")
rule("abort", r"waypoint_activity\.c", r"rx_waypoint_activity_vf(2|18)", r"championship", "SAFE", "C1", "waypoint championship intro / restart (activity types 0x21 / 0x30 / 0x31): a series event, not admitted (note: the port tests == 0x1f where the original tests >= 0x1f; types 0x21.. would silently continue)")
rule("abort", r"waypoint_(driver|activity).*\.c|waypoint_driver_calls\.h", ANY, r"network|Network|remote", "OUT-OF-SCOPE", "N1", "Waypoint driver network ownership / state serialisation")
rule("abort", r"waypoint_driver_calls\.h", r"waypoint_behavior_update|waypoint_rule_update", ANY, "SAFE", "L1", "Behavior vf4 (0x10) / Rule vf5 (0x14) update: the Behavior classes installed on waypoint drivers register it (unported: BehaviorRoamWorld / RoamTrack / LeaveActivity / PerformStunt / Pathfinder only on free-ride AI, see appendix)")
rule("abort", r"waypoint_navigation\.c", ANY, ANY, "SAFE", "I1", "the original's IDIV fault (zero waypoints): a waypoint race has gates")
rule("abort", r"waypoint_scenario\.c", ANY, r"clip switch index", "SAFE", "I1", "index < 5 by construction")
rule("abort", r"waypoint_activity\.c", r"rx_waypoint_state_map_destroy", ANY, "SAFE", "I1", "std::map proxy")

# ------------------------------------------------------------------------------------------------------------ indirect rules
# group rules by function; targets text and class
def ind(file_re, fn_re, cls, ev, text, addrs="", prio=99):
    rule("indirect", file_re, fn_re, ANY, cls, ev, text, addrs, prio)

ind(r"loop_engine\.c", r"rx_jobpool_recover", "SAFE", "I1",
    "JobPool_Recover (00881d80) runs the jobs still queued when JobPool_WaitIdle stalls (every 11th round while jobs are pending); the serialized workers have always finished, so no job is pending (rx_jobpool_wait_idle), and a queued job is a core function (RX_FN) in any case")
ind(r"anim_animators\.c", r"rx_motion_animator_eval", "SAFE", "L1", "AssetHandle vf2 (name, slot 8): every AssetHandle class registers it")
ind(r"anim_extern\.h", r"anim_animator_eval|anim_animator_get_param", "SAFE", "L1 F1", "Animator vf4 eval (0x10) / vf11 get-param (0x2c): all ten Animator classes register both (stop_audit.py --slot)")
ind(r"anim_extern\.h", r"anim_vcall0(_f)?", "SAFE", "L1 M1 F1", "entity / scenario / vehicle / light virtuals through slot `off` (callers: anim_scene.c Lights_Add and SceneManager_Add/RemoveEntity, scenario intro): the former residual targets PhysicsEntity::vf2 0041cbe0 (world_veh_entity.c) and the FMX scenarios' vf3 / vf6 0074de60 / 0074e140 (anim_scenario_finish.c) are ported since; the Entity destructors are ported (w-teardown: anim_entity_dtors.c)")
ind(r"anim_ik\.h|anim_ik_anim\.c", r"anim_set_rate_child|ik_eval_child|rx_featherstone_animator_eval", "SAFE", "L1", "Animator vf3 (0xc) / vf4 (0x10) of the IK / Featherstone child animator: registered")
ind(r"anim_ik\.h", r"ik_call", "SAFE", "Z1 L1", "helper: literal targets 005ee840 (array_at, registered? no: 24 B leaf used by anim_ik_ent.c LeaderBoard::vf19 only) and virtual slot calls; see anim_ik_ent.c row", "")
ind(r"anim_ik\.h", r"ik_call_ex", "SAFE", "Z1", "literal targets: 006b2fb0 (IK chain solver variant: only when the byte 0x00d9d3e0 is set; nothing in the exe writes it: dead), 0083a7c0 (IK post-pose: only when the animator's memory-store state +0x2c has flags&7 == 1; only the constructor writes +0x2c = 0), 008077c0 / 008078d0 / 00807dc0 (sound-handle calls of spline events: the SoundActions that create events are an abort at load (build_scene_anim.c:180/273) and no installed level has them)", "")
ind(r"anim_rider_a\.c", r"anim_entity_hierarchy", "SAFE", "L1", "AnimationEntity vf29 (0x74): registered for AnimationEntity / RiderAnimation")
ind(r"anim_rider_stunt\.c", r"keymap_vf9", "SAFE", "L1", "KeyMap vf9 (0x24): the KeyMap classes the core builds register it (only KeyMapCommand base stubs are not)")
ind(r"anim_rider_stunt\.c", r"rx_rider_stunt_ground", "SAFE", "L1", "RavenSimulation vf33 (0x84): the five concrete simulations register it")
ind(r"anim_scenario_intro\.c", r"rx_scenario_(play_on|name_of|begin|intro_update|load_intro_script|anim_entity_(play|set_animation)|cast_(load_script|place_entity|release)|event_(read|parse)|script_run)|xml_attr|xml_read|rx_rider_set_start_line_animation",
    "SAFE", "L1 X1 M1", "scenario intro machinery: Scenario vf1/vf2 (slots 4/8) of Race/Waypoint/Freeride intro scenarios, ScenarioAnimEntity vf15/vf19/vf28 (0x3c/0x4c/0x70), the Database `anim`/`bxml` request (vf27 0x6c), the XMLReader virtuals (0xe8 attribute, 0x110 string, 0x20/0x24/0x28 iteration, 0x130/0x134 values) and the script event handler table 0x9c5120. All registered for the classes built, except: handlers 2 ClearScenarioOrigin 0055d390, 3 ModelAnimation 0055d490, 12 CrewChiefAnimation 0055dbe0, 16 VehicleAnimation 0055dcb0, 23 PlaceVehicle 0055dd40 - a scan of all 41 scenario scripts of the install finds Event types 2/12/16/23 in none and type 3 only in SCMG_LoseWalk (the finish scenario's lose-walk, a documented boundary); the cast release's destructors (006b7720 / 00618c30) run only for cast entries a script placed (none in the product's intro)")
ind(r"anim_scenario_rider\.c", r"rx_rider_animation_stance2_outputs", "SAFE", "Z1", "literal 00621570 (registered)")
ind(r"anim_scenario_rider\.c", r"rx_rider_character_release_animation|rx_rider_parts_set_float", "SAFE", "L1", "registry / child virtuals (slots 4 / 0x1c): registered")
ind(r"anim_scene\.c|anim_scene_misc\.c", r"cell_grower|light_grid_add|entity_tick|volume_bounds", "SAFE", "Z1 L1", "literal 007d5e70 / 007d60f0 / 007da4d0..007da740 (registered); ticker slot 0x4c/0x54 and light vf6 (0x18): registered")
ind(r"bike_response\.c|bike_sim\.c|bike_vehicle\.c|vehicle_atv_telemetry\.c", r"sim_vcall|bike_sim_vcall|bike_moto_apply_(pose_override|pending_tuning)|bike_sim_action_call|bike_sim_im_call|bike_vehicle_post_step|rx_vehicle_forward_angular_velocity|telemetry_virtual_float", "SAFE", "L1 F1 M1",
    "RavenSimulation* virtual slots 3-34 (the five concrete simulations register all; only the abstract RavenSimulation stub 008f9fd6 is unregistered), the input manager's pad action objects (XenonInputManager vf4 0x10 / vf10 0x28 / vf45 0xb4 are unregistered: vf45 is used only in the session-pause block that never runs offline, vf4/vf10 are not called with these receivers) and RavenVehicle vf29/vf34/vf38 (registered)")
ind(r"bike_sim\.c", r"bike_sim_call_stdcall", "SAFE", "Z1 B1", "literals 008252c0 (10903 B) / 00824fe0 / 00824e70 / 0043a560 / 0043b750 run inside bike_moto_apply_pending_tuning / rx_bike_load_xml: the XML tuning read (`sol`/`pending tuning`) of the Pending-tuning path (rx_session_tuning_set); 00824fe0 is an unported reader entry point (008252c0 is ported since: build_xml_stream.c rx_xml_reader_load_stream) reached only by a pending XML tuning document (the shell does not use it); not on any matrix run", "")
ind(r"bike_sim\.c", r"bike_sim_mech_stdcall", "SAFE", "Z1", "literals 0085c6d0 / 0085cb70 / 0085c700 / 0085cf80 / 0085d300 (mechanism node slots): registered")
ind(r"build_activity_audio_teardown\.c|build_activity_race\.c|build_activity_records\.c|build_activity_util\.h|build_activity_world\.c|build_activity_track\.c|build_activity_game\.c|build_mxworld\.c|build_asset\.c|build_scene\.c|build_vehicle_pkgcache\.c|build_vehicle_util\.h|build_rider_util\.h|build_xml\.c|build_vehicle_sim\.c",
    r"rx_audio_logic_vf30|ctor_reg|rx_activity_vf1|vcall0|vcall1|rx_activity_vf50|stream_(close|read|skip)|act_call_f2|act_call_fn|act_vcall_f|db_call|db_vcall|rx_int_hash_insert|rx_database_vf_package_loaded|ext0|rr_call|rr_vcall|scene_vcall|rx_pkgcache_request(_db)?|veh_vcall(_f)?|rr_xml_vcall1|orig_regs",
    "SAFE", "L1 M1 Z1", "build-unit helper `RX_FN(slot) else rx_call_original`: Activity vf1/vf50 (0x58 / 0xd0), Database vf5/vf6/vf8/vf11/vf14/vf27 (0x14 0x18 0x20 0x2c 0x38 0x6c), stream vf1/2/4 (4 8 0x10), allocator vf1/2, scene / vehicle / rider / XMLReader slots. Registered for the concrete classes; Activity::vf6 00557a10 (83 B) and the activity destructors only in the activity-removal path (teardown row). Literal targets passed to act_call_fn / orig_regs by non-oracle callers: 0047a880 (the damage manager's part reset: ported, world_veh_damage_hit.c, w-ctx), 0078fcf0 (cheat-string test, mode 0 only; ported), MXGame init-thread steps 004dc0a0 / 0053c350 / 0053c3d0 (state 0/1, never run), session-pause block 00548ad0 / 00548b40 / 005ee840 / 0075a2b0 / 0047bc20 (ported) ([0xda6734] is 0 offline, set once to 0 at load), 00779440 Lua jobs (not run: product boundary), 0075cfb0 (online only), 007616d0 (UI manager only)")
ind(r"build_event_proxies\.c", r"rx_event_proxy_call_member_int_float", "SAFE", "L1", "proxy for float-argument handlers (stunt scoring): the handlers registered are ported")
ind(r"controller_axis_query\.c|loop_.*\.(c|h)|loop_cam.*\.c|loop_player\.c|loop_world\.c|loop_entity\.c|loop_vehicle\.c|loop_engine\.c", r"rx_controller_(stick_drive_axis|compound_available)|loop_tc.*|loop_ex|lv_run|pl_run|stdcall1|players_update|director_value|rx_player_update_reset_point|rx_character_ticker_run|rx_land_animal_update|rx_jobpool_worker_run|rx_jobpool_recover|km_enabled|km_enable|sus_im_call", "SAFE", "L1 F1 M1",
    "game-loop helpers `loop_tcN(loop_vslot(obj, off))`: KeyMap / XenonInputManager / camera (MX*Camera) / entity / vehicle / director virtuals. Unregistered candidates in constructible classes: MXFirstPersonCamera::vf30 00641dd0 (322 B, called from Player_UpdateCameras only when player state +0x988 == 4 and the camera is not the gameplay camera: not reached by the camera presets or the C-key cycle in the camera sweep), MXStartGateCamera::vf32 00648390 (581 B, start-gate camera, only with a StartGate camera entity; not built in the product), XenonInputManager vf4/vf10 (not called with these receivers), DebugKeyMap vf4 (debug); loop_input_suspend.c (w-tutcore): KeyMap vf9 / vf10 (006d7840 / 006d7860, CompoundKeyMap 00811c50 / 00811c90) and XenonInputManager vf45 / vf48 (00815170 / 00815290), all registered",
    "")
ind(r"controller_axis_query\.c|loop_hostinput\.c|loop_player\.c|fx_rdb\.c|fx_stunt\.c|fx_world\.c|fx_types\.c|crt_runtime\.c|crt_thread\.c", r"os_.*|pl_frame|rx_player_event_name|invalid_parameter|tree_next|\?|rx_fx_online_content_poll|rx_fx_package_queue_drain|crt_cmp|crt_ptd", "SAFE", "Z1 B1", "CRT / OS import slot (0x00916224 _invalid_parameter_noinfo, 0x009162bc _stricmp, 0x009161b4 / 0x00916268 / 0x00916438 / 0x00916448 kernel imports) called through rx_call_original: the import resolves to a registered core CRT function or the online-content poll (boundary)")
ind(r"crt_util\.h", r"crt_unported.*", "SAFE", "Z1 I1", "msvcr90 internals (0x785xxxxx) behind the ported CRT: reached only for NaN / Inf / out-of-range math arguments (floor/ceil of NaN, pow domain errors, sin of huge values), string-length errors (_Xlen/_Xran), a locale change, _vsnprintf formats the core does not implement, and invalid-parameter paths; none occurs in a finite-state ride (F1: 13 runs x 12000 frames)")
ind(r"fourwheel_.*\.c|fourwheel_pending\.c|pending_tuning_dependencies\.c|native_bridge\.c|guest_reader_reset\.c|guest_string_hash\.c|native_reader_load\.c|zlib_.*\.c|tdf_owner\.c|world_util\.h|phys_.*\.(c|h)|race_.*\.(c|h)|rx_vcall\.h|scene_fx_entities\.c|fx_.*\.c|vehicle_atv_.*\.c|world_phys\.c|build_.*\.c",
    r".*", "SAFE", "L1 F1 M1", "helper `RX_FN(target) else rx_call_original`: the target is a virtual slot of the receiver families listed in the appendix or a literal passed by the callers (resolved: all registered except the rows above)")

# ------------------------------------------------------------------------------------------------------------ fatal (bare abort) rules
def fat(file_re, text, cls="SAFE", ev="I1", addrs="", prio=99, msg=ANY):
    rule("fatal", file_re, ANY, msg, cls, ev, text, addrs, prio)
fat(r"collision_triangle_ray\.c", "closed (w-collide 2026-10-26): the observed dispatch's pair roles beyond the ten typed closures no longer abort in unavailable(); they run the core generator through the case table (phys_contact_pair_observed). Kept for the rule numbering; no site matches",
    "SAFE", "W1", "", 99, r"Unclosed exact native collision pair")
fat(r"phys_contact\.c", "pair-kernel table 0085ddd0: every generator is a core port (pair_kernel); the remaining aborts are the closed table's default, the observed role check (a (target, return site, argument count) that is not a case of the original's switch) and the observed dispatcher's entry-frame contract; they fire only on a call shape the original never produces. "
    "The former stops (rows R88 / R89 of the 2026-10-26 audit): the eight generators 00864460 / 00864dd0 / 008650c0 / 0086b360 / 0086f150 / 0086f3d0 / 00872970 / 00872c90 are ported (phys_pair_circle.c, phys_pair_rays.c, phys_pair_query.c) and the observed dispatch is general",
    "SAFE", "I1 W1", "", 99, ANY)
fat(r"fourwheel_.*|utv_.*|buggy_.*|truck_.*|family_.*|contact_.*|terrain_.*|query_.*|height_.*|body_native.*|composite_pair.*|filtered_contact.*|rays6.*|triangles_segments.*|native_(role|response|curve).*|vehicle_fourwheel.*|phys_pair2\.c|phys_coll_.*|contact_correction.*|fx_vehicle_contact.*", "four-wheel / collision native closure: ABI frame checks and 'unclosed dependency / role' contracts of the exact native kernels (each role compared with the original's return site, ESP, EBP, register values): they fire only on a call shape the closure was not written for; the 63 runs of the four-wheel vehicle matrix (UTV, buggy, ProLite, Pro2 trucks, ATV on Terracross / Powercross / Waypoint / FreeRide / National / Supercross) cover the closure; its pair roles: W1", "SAFE", "F1 I1 W1")
fat(r"build_asset\.c|build_asset_types\.c|build_model\.c|build_collis\.c|build_collision\.c|build_terrain.*\.c|build_water\.c|build_forest\.c|build_scene.*\.c|build_render_asset.*\.c|build_xml\.c|build_vehicle.*\.c|build_world\.c|build_mxworld\.c|build_tables_startup\.c|build_tables_load\.c|build_rider_util\.h|build_vehicle_proxy\.c|build_vehicle_tree\.c|build_vehicle_spec\.c|fx_vector\.c|fx_tree\.c|crt_.*\.c|phys_coll_misc\.c", "asset / scene / model / vehicle construction: an asset kind, model version, collision geometry kind, scene entity class or XML feature the installed data does not use, or an allocation / length error. Every level and vehicle of the shell loads (matrix, vehicle matrix), so these do not fire for installed content", "SAFE", "M1 X1 I1")
fat(r"bike_response\.c|bike_slot13\.c|native_bridge\.c|pending_tuning_dependencies\.c|zlib_.*\.c|race_.*\.c|loop_.*\.c|anim_.*\.c|fx_.*\.c|world_.*\.c|vehicle_.*\.c|phys_.*\.(c|h)|career_.*|replay_.*|waypoint_.*|build_activity.*", "frame / ESP / register contract of an exact transliteration; fires only on a call shape the original never produces", "SAFE", "I1 F1")
fat(ANY, "frame / ABI contract or 'not ported' construction branch", "SAFE", "I1")
ind(r"anim_scenario_finish\.c", r"tail_call_vf3", "SAFE", "L1 F1 M1",
    "the finish scenarios' tail call of their own vf3 (RaceFinishScenario::vf0 0074a1e0 `jmp [vtable + 0xc]`): the receiver is the scenario whose vf0 runs, so the target is RaceFinishScenario::vf3 0074a110 (rx_finish_select_next, registered); w-podium")
rule("abort", r"garage_test_drive\.c", r"td_garage_vehicle", r"garage's vehicle", "SAFE", "TD1",
     "the garage's own vehicle (007611b0): the product's test drive has no garage loaded, so the guard is never true")

# ------------------------------------------------------------------------------------------------------------ the engine
NEEDS_PORT_ITEMS = [
 # (rank, short title, where it is reached, addresses, evidence)
]


CLOSED_ITEMS = [
 ("9", "Macro-hidden physics, rider, camera, vehicle and race targets (the census could not see the WORLD_CTX / pl_call / act_call_fn / orig_regs / tracked_call / tree_insert forms)", "w-hidden: 00769f40 00768650 00417070 00774c30 0061db10 006217a0 00621850 006514e0 00646e80 00476340; w-ctx: 0076dec0 0076e6c0 (kinematic push contact kernels), 00460070, 00548960 (+ 00548850 005488b0), 0047a970 0047a880 0047ba80 00691c30 00697b10 006971f0 (vehicle damage hits), 0047aff0 00681750 (the part spring), 0047bb90 00479a00 00479ce0 00479ef0 00479680 0047ae00 0041cb60 0041cd90 007697e0 007dda20 007ddfa0 00847c80 00849080 (the registration of the trucks' / buggy's / UTV's detachable parts), 00512c80 0053ac80, 00623060, 007d2c80, the four std::map rotations 00500480 004ffe80 00765c90 00765ce0, 00892120", "core/oracle/wave_hidden_probe.cpp and wave_ctx_probe.cpp (core/plan/QUEUE.md w-hidden / w-ctx): per call, memory and registers, 0 mismatches; the rest of the extended census (core/tools/census_original_calls.py) is boundary:audio / online / ui / not-reached with the evidence at each call"),
 ("7", "Freestyle, Section Race and the Free Ride challenges (HillClimb, Trailblazer, TargetPractice, BigAir, Pathfinder, StepUp): the activity factory 0055ef98 and their classes", "w-freestyle (110 functions: MXActivityFreestyle, the stub, FreestyleDriver 0050c900 chain, FMX scenarios, rules; race_activity_freestyle.c, race_freestyle_driver.c, race_freestyle_rules.c, anim_scenario_fmx.c), w-section (the 19 vtable functions of MXActivitySectionRace 0056a210, SectionRaceDriver 00512db0 chain, SectionRaceZone; race_section.c, race_section_driver.c), w-challenge (race_challenge*.c: MXActivityChallenge 0057aa30 and the HillClimb / Trailblazer / TargetPractice / BigAir / Pathfinder / StepUp classes, the grid's start zones 0075f9fc..)", "QUEUE.md sections of the three agents (probe cases and Verify per call, 0 mismatches); product: M1 / E3 (2026-10-26, w-matrix): 24 Freestyle, 30 Section Race and 17 challenge rows (+ InCareerMode) run to RACING, WindDown and Post with the replay host, are torn down and ended; 14 two-event sessions"),
 ("10", "Free Ride Post (Activity::vf29 00559a00, the base class's Post update) and the teardown of the challenge areas (ScrollingTextureEntity 00410bc0 + 00410c10 of the HillClimb areas, MXTargetPracticeCollectable 0040e550 of the TargetPractice areas) - found by w-matrix's lifecycle matrix", "00559a00: race_champ.c rx_activity_vf29 (shared body with the heat's 0057c010); 00410bc0 / 00410c10 / 0040e550: anim_entity_dtors.c", "oracle: race_champ_cases.cpp activity_vf29 / _results / _results_main: 3 Verify calls, 0 mismatches; dtor_probe case o (ids 0x1078 / 0x106e, vf0(0), vf0(1), vf9, vf2): 00410bc0 2/2, 00410c10 2/2, 0040e550 2/2, 0 mismatches; verify.sh rx_anim and rx_race on the 5 reference worlds: 0 mismatches, On identical; product: the 40 Free Ride / challenge rows complete on the product objects, 28 of 28 sequential events end"),
 ("1", "StuntLanded / StuntHold / StuntTakeoff listeners", "0054d310 (+ 00621fe0), 0054d1a0, 0054d1e0, 0054d360 (+ 006223e0): loop_player.c, anim_rider_play.c", "core/oracle/stunt_events_probe.cpp: 6 ports x 60 Verify calls on the Concord player, 0 mismatches"),
 ("2", "PerformanceAnimator additive clip", "0083e930: anim_animators.c rx_performance_additive_clip", "core/oracle/performance_clip_probe.cpp (ATV world, garage 0,0,-1,8,2,4): 114 Verify calls with random poses, weights 1e-9 .. 1.5, 0 mismatches"),
 ("6", "Free Ride with AI riders", "0052aac0 + states 0052ae10 .. 0052b410, 0052ab80, 0052b0a0, 0051dff0, 0051e040, 0078fcf0 (race_beh2_perform_stunt.c); 0052fcf0 + 0052efa0 00530dc0 00530590 0052f0e0 0052fca0 0052f4a0 0052f990 0052f510 00530740 (race_beh2_roam_world.c, with 00538a60 now returning t); 00555630", "FreeRide_AI_Cheyenne (6 riders, 5 AI): 60000 + 21170 Verify calls of the two updates, probes perform_stunt_probe / roam_world_probe drive every state: 0 mismatches; On identical"),
 ("8", "Kinematic handle list sort > 32 entries", "00777580 00777c70 00778190 00777d20 00777d60 00778200 00778660: world_body.c", "core/oracle/body_sort_probe.cpp: 600 cases (n = 2 .. 600, six orderings, ideal n / 40 / 0 / -1), 0 mismatches"),
 ("3", "Replay at the end of a race without a replay host (w-replay)", "no new function: the exhibition replay presenter (rx_session_replay_begin_event / _end_event, api/replay_session.inc; Unity ReplayWorker.BeginEventWorker, OpenReflexSimulation replayPresentation, OriginalWorldRuntime.TickExhibitionReplay -> the career's OwnReplayRenderer) for every event of the open front end; 00757660's product branch in rx_replay_require_viewport now runs the viewport list port and the split-flag write (render targets / screen effects stay the renderer's)", "headless E2 (five event types reach Post 2 and play the reel, product objects, 0 unported calls); Unity: run.py --arcade (arcade National, natural 1-lap finish, replay + results); career flow re-run"),
 ("5", "Activity / vehicle / rider destruction (career 'Continue', next event): the collision shapes and the scene entities (w-teardown)", "phys_shape_dtors.c: 00847770 008455a0 00845570 00844970 00844860 00844a90 (+00844c50) 00844f70 (+00844f90) 00847c60 (+00847d50) 00848ec0 (+00819e40) 00849040 (+0042b280) 00849830, the shape release at every shape-reference site (build_collision.c, build_scene_props.c, build_scene_forest.c, build_vehicle_proxy.c, build_rider_ik.c); phys_static_objects.c: 00773a60 007747f0 00774d10 00773ad0 007772d0 00773e10 007735e0 00773db0 008218c0 0081ff30 0081fe40 0081f690 (+ 00845010 shape bounds); anim_entity_dtors.c: 00406ff0 00401fd0 00793930 00407110 (+004072c0) 00407130 00409970 0063d560 (+0063e6a0) 00648dd0 (+00648df0) 00692190 00696580 (+006965a0) 006ba5f0 006bbff0 006bd520 (+006bd570) 00819000 (+00819050) 00819100; FreeRideActivityAudioLogic vf30 00582260 / vf4 005823b0 (+0058b130); 0084dd30 (the scene's name table erase: the core passed the key slot's address as the string, so an entity's name was never erased). BOUNDARIES (documented in QUEUE.md): the entity classes outside_core in build_scene.c (LocalizedPackageReferenceEntity 00416950 / 00416c30 / 00416d70, the activity audio manager entities 0068d9d0 / 0068dcc0 / 0068dce0) are never built by the product; the audio registries [0xa95824] / [0xa95828] (AudioCollisionManager::vf1 0067b920, VehicleAudioManager::vf1 00680e80, 00680e50) are null in the product; the UI event-handler proxies 005ef970 / 005effa0 / 0060edb0 / 006104e0 belong to UI pages that the product never creates. 006de4b0 (the Network channel) was already ported (c94bf55) and runs at session create", "core/oracle/dtor_probe.cpp (shapes unpinned; cases a v r s d c h f), Verify, 0 mismatches: FreeRide_AI_Cheyenne, WaypointRace_FR_Gibson, FreeRide_FR_Alpine (activity vf3 + vf6 + vf0; 86 area entities in FR_CheyenneFreeRide), National_NAT_Concord and the other four reference worlds (race area vf3 + vf6 + vf0); shapes: 639 shapes in 9 classes of the registry x flags 0 / 1 x lock 0 / 1, StaticRays / Triangles / Composite / StaticComposite / DynamicComposite made up in guest memory"),
]

HIDDEN = {
 0x00777580: ("SAFE", "kinematic handle list sort partition: ported (w-stunts)"),
 0x00777d20: ("SAFE", "same sort: ported"), 0x00777d60: ("SAFE", "same sort: ported"),
 0x0083e930: ("SAFE", "ATV additive clip: ported (w-stunts)"),
 0x0083eff0: ("SAFE", "override clips: no perfdat asset has override=true"),
 0x00769f40: ("NEEDS-PORT", "kinematic push contact set (#9)"), 0x0076dec0: ("NEEDS-PORT", "contact set loop (#9)"), 0x0076e6c0: ("NEEDS-PORT", "contact set loop (#9)"),
 0x00768650: ("NEEDS-PORT", "kinematic list >= 200 (#9)"), 0x00417070: ("NEEDS-PORT", "same path (#9)"), 0x00774c30: ("NEEDS-PORT", "body-record vector growth (#9)"),
 0x0061db10: ("NEEDS-PORT", "rider animation mode 7 / +0x5b2e0 (#9)"), 0x006217a0: ("NEEDS-PORT", "rider animation mode 2 (#9)"),
 0x006514e0: ("NEEDS-PORT", "camera impact kick when cam+0x5f8 (#9)"), 0x00646e80: ("NEEDS-PORT", "player camera +0x994 >= 0 && +0x998 (#9)"),
 0x00476340: ("NEEDS-PORT", "vehicle effect block +0x16f0 (#9)"),
 0x00460070: ("SAFE", "only after bike_state_divergence_check reports a diverged (non-finite) state"),
 0x0047a970: ("SAFE", "damage-manager target (+0x3fc) set by the registration 0047bb90 (ported, w-ctx): ported"), 0x0047a880: ("SAFE", "part states reset: ported (w-ctx)"),
 0x00548960: ("SAFE", "scripted wreck type: [0xaf6c00] set by developer script only"),
 0x006b2fb0: ("SAFE", "dead switch byte 0xd9d3e0 (nothing writes it)"), 0x0083a7c0: ("SAFE", "IK memory-store state +0x2c never set"),
 0x008252c0: ("SAFE", "XML tuning document (rx_session_tuning_set path) not used by the shell"), 0x00824fe0: ("SAFE", "same"),
 0x0053f030: ("SAFE", "pending vehicle loader list empty (all runs)"), 0x0053f130: ("SAFE", "pending rider loader list empty (all runs)"),
 0x00850010: ("SAFE", "material pair table built at startup (flag 0xdce7d4 set)"),
 0x00892db0: ("OUT-OF-SCOPE", "Quazal session clock (online)"), 0x0075cfb0: ("OUT-OF-SCOPE", "online only"), 0x007616d0: ("SAFE", "UI manager only"),
 0x0078fcf0: ("SAFE", "ActivityMode 0 cheat string test"), 0x00779440: ("SAFE", "Lua jobs: product boundary"),
 0x004dc0a0: ("SAFE", "MXGame state 0/1 (A1)"), 0x0053c350: ("SAFE", "MXGame state 0/1 (A1)"), 0x0053c3d0: ("SAFE", "MXGame state 0/1 (A1)"),
 0x00548ad0: ("SAFE", "session pause block ([0xda6734] == 0 offline)"), 0x00548b40: ("SAFE", "same"), 0x0075a2b0: ("SAFE", "load abort (UI quit)"),
 0x007e4090: ("SAFE", "WaterEntity_Reload: water asset not built (abort at load)"),

}
for _a in (0x00804e40, 0x00807890, 0x00807970, 0x00807c30, 0x008077c0, 0x008078d0, 0x00807dc0):
    HIDDEN[_a] = ("SAFE", "sound handle of the rider crash sound / spline sound events: the handle [A+0x2f0] is never set; SoundActions are an abort at load")
for _a in (0x0067c670, 0x0067c990, 0x00805780, 0x00806110, 0x00806c10, 0x00807360, 0x0080a890, 0x0080aaa0, 0x0080aca0, 0x0080aee0, 0x0080b030, 0x0080b360, 0x0080b490, 0x0080b540, 0x0080b5e0, 0x0080bd40, 0x00892120, 0x008b9940):
    HIDDEN[_a] = ("SAFE", "audio node / wreck-audio (FMOD) path: world_node_audio.c / world_wreck_audio.c run only for audio nodes (none are built: audio boundary)")

# w-partymodes (2026-10-30): the Tag party mode's Quazal sites (appended last: the rule ids above stay as the table has them)
rule("abort", r"race_tag(_driver|_ball)?\.c", ANY, r"ONLINE", "OUT-OF-SCOPE", "O1 N1",
     "the networked Tag party mode (Quazal: the TagBall duplicated object +0xff380, the drivers' network objects +0xef0, the session's start zones, the networked teardown / spectator / round end): guarded by session_networked() / +0xff380 / +0xef0, all zero offline; the online framework's host runs the offline code and a client takes the host's state through party_net.c (w-partymodes)")

# w-partymodes (2026-10-30): the PokerRace's Quazal sites (appended last, as the Tag rule)
rule("abort", r"race_poker\.c", ANY, r"ONLINE", "OUT-OF-SCOPE", "O1 N1",
     "the networked PokerRace (Quazal: the NetPokerRace duplicated object +0x13e8d0 that carries the community cards, the hands and the choices; the drivers' network objects +0xef0; the networked teardown / finish): guarded by session_networked() / +0x13e8d0 / +0xef0, all zero offline; the online framework's host runs the offline code and a client takes the host's cards through party_net.c (w-partymodes)")

# w-snake (2026-10-30): the Infection and Snake party modes' Quazal sites and their offline-unreachable branches (appended last, as above)
rule("abort", r"race_infection\.c", ANY, r"ONLINE", "OUT-OF-SCOPE", "O1 N1",
     "the networked Infection party mode (Quazal: the network snake object +0xff1e0 that carries the boundary's radius, the drivers' network objects +0xef0 with the infections and scores, the session's start zones, the networked round end): guarded by session_networked() / +0xff1e0 / +0xef0, all zero offline; the online framework's host runs the offline code and a client takes the host's infections through party_net.c (w-snake)")
rule("abort", r"race_snake(_driver)?\.c", ANY, r"ONLINE", "OUT-OF-SCOPE", "O1 N1",
     "the networked Snake party mode (Quazal: the NetSnake object +0xff0c4 that carries the trails, the cuts and the boundary's radius; the drivers' network objects +0xef0; the session's start zones, the networked teardown / spectator / round end): guarded by session_networked() / +0xff0c4 / +0xef0, all zero offline; the online framework's host runs the offline code and a client takes the host's cuts through party_net.c (w-snake)")
rule("abort", r"race_infection\.c", r"rx_infection_vf15", r"first infected of no driver", "SAFE", "I1",
     "an Infection round starts with no driver: the activity always has the local player's driver (vf44 builds one per player)")
rule("abort", r"race_snake\.c", r"rx_snake_water_rise", ANY, "SAFE", "M1",
     "a water boundary (UseWaterAsBoundary of the round's start zone): offline the zone is the activity row's MiniGameStartZone, one per Snake level (FR_Alpine / FR_Cheyenne / FR_Gibson); none has the flag (party_snake_test and stop_audit_run fuzz 20000 frames on each level run the shrink, w-snake)")
rule("abort", r"race_snake_trail\.c", r"shape_call", ANY, "SAFE", "I1",
     "a trail shape other than the constant 0x9308cc: the trail constructor 0069e3a0 stores 0x9308cc (+0x100) and nothing writes another")

# w-partylast (2026-10-30): the EggHunt, Graffiti and Capture party modes' Quazal sites and their content stops (appended last, as above)
rule("abort", r"race_egghunt\.c|race_graffiti(_point)?\.c|race_capture\.c", ANY, r"ONLINE", "OUT-OF-SCOPE", "O1 N1",
     "the networked EggHunt / Graffiti / Capture party modes (Quazal: the NetEggHunt +0x100128 / NetGraffiti +0xff084 / NetCTF +0xff434 duplicated objects that carry the eggs' collections, the capture points and the flags' poses and scores; the drivers' network objects +0xef0; the session's start zones, teams, spectators and round ends): guarded by session_networked() / those objects / +0xef0, all zero offline; the online framework has no messages for these modes (w-partylast)")
rule("abort", r"race_egghunt\.c|race_graffiti\.c|race_capture\.c", r"rx_(egghunt|graffiti|capture)_vf2", r"lacks the mode's content", "SAFE", "I1 M1",
     "a mode's start where the original faults for missing scene content (EggHunt 0057492d: no \"GreedModel\"; Capture 005765f1 / 0057666e: no \"RedFlag\" / \"BlueFlag\"; Graffiti 00564151: no StartEntityFreestyle / StartEntityFreeRide, a division by zero): rx_session_event_load refuses those events first when the level's scene file lacks the entities (minigame_content_missing, return 3; party_minigames_test: every mode on FR_Alpine / FR_Cheyenne / FR_Gibson refused, the session then loads Tag); no other path builds these activity types (no EventTypeLocations row, the career and the online framework never name them)")

# w-graffiti (2026-10-30): the GraffitiCapturePoint entity's offline-unreachable null reads and its proxy call's fallback (appended last, as above)
rule("abort", r"race_graffiti_point\.c", r"point_driver|point_listen", ANY, "SAFE", "I1",
     "a GraffitiCapturePoint without its Graffiti activity (+0xb80), an activity without drivers, or a failed allocation of a handler proxy: the original reads a null pointer there (00564970 / the caller's +0xef0 / 0041927b); the points exist only in a level's MiniGameGraffiti subscene, which only a Graffiti event loads, and MXActivityGraffiti::vf2 links every point of its area before any trigger or stunt event reaches one, its drivers made at the activity's start (vf44)")

def short_target(f, fn, kind, msg, via):
    if kind == "indirect":
        s = msg + ((" <- " + via) if via else "")
        return ("`" + s[:90] + "`")
    return msg[:110].replace("|", "/")

def main():
    sites = SA.sites()
    rows = []; unk = []
    rule_ids = {id(r): "R%02d" % (i + 1) for i, r in enumerate(RULES)}
    for s in sites:
        f, ln, kind, fn, msg, via = s
        hit = None
        base = os.path.basename(f)
        for r in RULES:
            k, fr, fnr, mr, cls, ev, text, addrs, prio = r
            if k != kind or not fr.search(base) or not fnr.search(fn or ""): continue
            tx = msg if kind != "indirect" else (msg + " " + via)
            if not mr.search(tx): continue
            hit = r; break
        if hit is None:
            unk.append(s); rows.append((s, "UNCLASSIFIED", "", "", "", 0, "")); continue
        k, fr, fnr, mr, cls, ev, text, addrs, prio = hit
        rows.append((s, cls, ev, text, addrs, prio, rule_ids[id(hit)]))
    for s in unk: print("UNCLASSIFIED", s[0], s[1], s[2], s[3], s[4][:60], file=sys.stderr)
    order = {"NEEDS-PORT": 0, "UNCLASSIFIED": 1, "OUT-OF-SCOPE": 2, "SAFE": 3}
    rows.sort(key=lambda r: (order[r[1]], r[5] if r[1] == "NEEDS-PORT" else 0, r[0][0], r[0][1]))
    cnt = collections.Counter((r[0][2], r[1]) for r in rows)
    o = []
    w = o.append
    w("# STOP_AUDIT: where the product can still stop (agent w-audit, 2026-10-26; dynamic evidence re-run by w-matrix after the Freestyle / Section Race / challenge / teardown merges)\n")
    w("Audit of every place where librxcore (no RX_ORACLE) can still stop beyond the literal-address census (`census_original_calls.py`). "
      "Tools: `core/tools/stop_audit.py` (lists the sites), `core/tools/stop_audit_md.py` (this file's classification rules + generator), "
      "`core/tools/stop_audit_run.c` + `stop_audit_matrix.py` (the product run once per event the shell offers, with `--life` the whole life of each: RACING, WindDown, Post with the replay host, teardown, session end) + `stop_audit_twice.c` (two events in one process); a development build adds fuzz, trick, camera and end-of-race drivers; build it with `core/tools/stop_audit_trace.sh <core build dir> <out dir>`).\n")
    w("## Counts\n")
    w("| kind | SAFE | OUT-OF-SCOPE | NEEDS-PORT | total |\n|---|---|---|---|---|")
    for kind in ("indirect", "abort", "fatal"):
        a = [cnt.get((kind, c), 0) for c in ("SAFE", "OUT-OF-SCOPE", "NEEDS-PORT")]
        w("| %s | %d | %d | %d | %d |" % (kind, a[0], a[1], a[2], sum(a)))
    tot = [sum(cnt.get((k, c), 0) for k in ("indirect", "abort", "fatal")) for c in ("SAFE", "OUT-OF-SCOPE", "NEEDS-PORT")]
    w("| all | %d | %d | %d | %d |\n" % (tot[0], tot[1], tot[2], sum(tot)))
    w("`indirect` = rx_call_original(_ex) with a non-literal target; `abort` = act_unreached / `<prefix>_unreached` / \"path not in the reference boot\"; "
      "`fatal` = a third kind found during the audit: a bare `abort()` (often after `fprintf(stderr, \"rxcore: ... not ported\")`, \"Unclosed ... dependency\", ABI-frame checks). "
      "The census of the brief counts 183 + 192; this tool counts 218 + 349 because it also lists the helper definitions' call sites in headers (`*_util.h`), macro-free wrappers and every `unreached()`-style helper "
      "(build_tables_rdb.c, build_registry.c). Sites inside `#ifdef RX_ORACLE` are skipped.\n")
    w("## NEEDS-PORT (most likely to be met in normal play first)\n")
    w("| # | what | reached by | addresses to port (size) | evidence |\n|---|---|---|---|---|")
    for (n, title, where, addrs, ev) in NEEDS_PORT_ITEMS:
        w("| %d | %s | %s | %s | %s |" % (n, title, where, addrs, ev))
    w("")
    w("### Closed in the last passes (agents w-stunts, w-replay, w-teardown, w-freestyle, w-section, w-challenge and w-matrix, 2026-10-26)\n")
    w("| # | what | ported | verification |\n|---|---|---|---|")
    for row in CLOSED_ITEMS: w("| %s | %s | %s | %s |" % row)
    w("")
    w("Not stops (checked): the finish scenario / podium (RaceFinishScenario, 0074a1e0 / 0074a340) is ported and runs in the product (w-podium; Post sub-state 1); "
      "the 24 scenario-script event handlers: only SCMG_LoseWalk (the finish lose-walk) uses an unported one (type 3); every ordinary race, free ride and waypoint event of the shell (all 404 runs of the event matrix and the 72 fuzz rides, all vehicle profiles) reaches RACING.\n")
    w("## Method and limits\n")
    w("* Static: every site read in context; indirect targets from the vtable (`_re/out/vtables.tsv`, `stop_audit.py --slot 0xNN`) intersected with the core's registrations "
      "(`gen_fnsig.py --report`: 4819 registered, 4152 callable through `rx_call_original`; **`rx_call_original_ex` is always fatal in the product**); helpers that take the target as a parameter were resolved through their callers "
      "(this found the literal targets 0083e930, 006b2fb0, 0083a7c0, 0047a880, 0053f030 ... that the census cannot see); callbacks stored in event proxies were resolved through their registration calls.\n"
      "* Dynamic: `stop_audit_run` drives the real `rx_session_*` API. (1) The event matrix (M1, `stop_audit_matrix.py --life 200 --career`): 420 runs, every EventTypeLocations row of MXTables_Default (+ the 6 DLC levels) x every vehicle class of the type + the 17 shipped challenge rows + Free Ride / the challenges with InCareerMode, each run to RACING, forced into WindDown, Post with the replay host, torn down and ended; 412 reach RACING (the other 8 end at once in the original too), 420 complete. Two events in one process: `stop_audit_twice` (E3). "
      "(2) A development build of librxcore whose `rx_call_original(_ex)` logs `RXTRACE-UNPORTED fn=... caller_off=...` and returns instead of aborting, used for: fuzzed rides (72 x 12000 frames, F1), a sweep of the 36 default aerial tricks (MX / ATV / UTV), the four camera presets, 49 four-wheel runs "
      "(ATV, UTV, super buggy, ProLite, Pro2 profiles from builds/sol_core_features/vehicle-profiles), forced WindDown / Post (the whole matrix), AI rosters of 2-16 riders, and class censuses (415 live vtables per race).\n"
      "* Limits: no career or Unity-side flow was run by the audit itself (those use the host contract rows; w-replay ran the replay host headless (E2) and the Unity arcade probe; the career's own Moto 1 -> Moto 2 transition is tests/moto_career_flow, Unity); no rider finished a lap race by itself (WindDown / Post were forced with the activity's own SetState; Freestyle, Section Race, HillClimb and TargetPractice were also run to their own natural end, M1); the second event of a session is a new session (A1): the teardown of E3 is the activity's vf3 / vf6 / vf0 called by a development hook, not by the game's own EndEvent path (which the API never runs); no random ride entered an activation area (the move / drive tests do); tricks start through the game's own 006c98c0 but the gesture machine is not driven; "
      "virtual calls through wrappers with a variable receiver were classified by receiver family, not call by call (appendix lists the residual unported targets of the constructible in-scope classes).\n")
    w("## Evidence keys\n")
    for k, v in EVIDENCE.items(): w("* **%s** %s" % (k, v))
    w("")
    w("## Rule notes (the reasoning behind a row's rule id)\n")
    w("| rule | class | evidence | note | addresses |\n|---|---|---|---|---|")
    used = collections.OrderedDict()
    for r in rows:
        if r[6] and r[6] not in used: used[r[6]] = r
    allr = {rule_ids[id(r)]: r for r in RULES}
    for rid in sorted(used):
        k, fr, fnr, mr, cls, ev, text, addrs, prio = allr[rid]
        w("| %s | %s | %s | %s | %s |" % (rid, cls, ev, text.replace("|", "/"), addrs.replace("|", "/")))
    w("")
    w("## Appendix: residual unported virtual targets of the constructible in-scope classes (primary vtables, classes whose vtable the core stores)\n")
    w("From `stop_audit.py --slot` over `_re/out/vtables.tsv`, slot offsets used by the core's virtual-call wrappers (940 call sites). Everything not listed is registered. "
      "Debug-only, Freestyle-only and online-only entries are marked.\n")
    w("| class | slot | function (size) | reached by | status |\n|---|---|---|---|---|")
    for r in [
     ("Activity / MXActivityRace / RaceStub / FreeRide", "0x0 sdtor", "00556640 (30) 00564af0 (36) 005659c0 (30) 0055fec0 (30)", "activity removal (rx_activity_area_update), teardown", "ported (w-stunts)"),
     ("MXActivityFreeRide / MXActivityWaypointRace", "0xc vf3", "00560080 (136) 00578b30 (208)", "activity removal", "ported (w-stunts)"),
     ("Activity (all)", "0x18 vf6", "00557a10 (83)", "activity removal", "ported"),
     ("MXActivityRace / Waypoint", "0x38 vf14, 0xb4 vf45", "005693d0 (382), 00567ac0 (67), 00557570 (496)", "debug camera / spectate keys (race_driver_update.c debug_keys)", "ported since (the debug keys stay OUT-OF-SCOPE)"),
     ("MXActivityRace / Waypoint / FreeRide", "0x40 vf16, 0x50 vf20, 0x24 vf9, 0x74 vf29, 0x8c vf35, 0xb8 vf46, 0xe8 vf58", "00557b40 (392) 00578cc0 (81) 0055d120 (284) 0057a3e0 (190) 0055eb70 (17) 00559a00 (64) 0057e720 (60) 00564f90 (152) 00559a80 (52)", "no core call site applies these slots to activity objects (the same slot numbers are used on KeyMap / camera / entity receivers); 00559a00, 0057e720 hit only by a forced FreeRide WindDown", "SAFE (all ported since except 00564f90)"),
     ("RavenVehicle / Motocross", "0x0 sdtor", "0045d3a0 (30) 00462560 (107)", "vehicle delete in teardown (build_activity_teardown.c:232/350)", "ported (w-stunts, with vf9 00462960 and the simulation)"),
     ("RiderAnimation / ScenarioAnimEntity / Entity (87 classes)", "0x0 sdtor", "00618c30 (71) 006b7720 (110) 00406ff0 (67) ...", "deletes at teardown; scenario cast release only for placed cast entries", "RiderAnimation / ScenarioAnimEntity ported (w-stunts); the other entity classes ported (w-teardown: anim_entity_dtors.c, item 5), except the outside_core classes the product never builds"),
     ("PhysicsEntity bales (PopUp/PopDown/StepUp bar)", "0x8 vf2", "0041cbe0 (114)", "SceneManager_RemoveEntity of a bale (scripted PlaceEntity of a bale; no installed script)", "ported since (world_veh_entity.c)"),
     ("Animator classes (Blend, Additive, Switch, Motion, Performance, IK, Expression, Procedural, Featherstone)", "0x14 vf5 .. 0x24 vf9", "e.g. 006b5540 (422) 006b56f0 (127) 00833a40 (2023) 00834740 (625) 0083d490 (2642) 0083b470 (1557)", "threaded / memory-store / debug animator interface; the core calls only vf3, vf4, vf10, vf11", "SAFE"),
     ("BaseScenario family", "0x0 vf0, 0x4 vf1, 0xc vf3, 0x18 vf6", "RaceFinishScenario 0074a1e0 (344) 0074a340 (494); FMX*Scenario 0074d700 0074de60 0074dab0 0074dbc0 0074dcf0 0074df30 0074e050 0074e140 0074fae0", "Post sub-state 1 (the finish scenario and the podium) and the Freestyle FMX scenarios", "ported (anim_scenario_finish.c: RaceFinishScenario w-podium, the FMX scenarios w-freestyle)"),
     ("Behavior classes", "0x10 vf4 update", "BehaviorLeaveActivity 005265a0 (457), PathfinderRoam 00536400 (95), RoamTrack 0052d0a0 (252); PerformStunt 0052aac0 and RoamWorld 0052fcf0 are ported (w-stunts)", "installed on free-ride AI drivers only (rx_free_ride_vf47); mode 0 race cheat string (LeaveActivity)", "NEEDS-PORT (API-only: EventRiders > 1 in a Free Ride)"),
     ("Rule classes", "0x14 vf5 update", "RuleCapture, Tag, Infection, ChallengeBoundary, InFreestyleCrowd, OutOfSnakeBounds, Offtrack (0051abc0 719 B)", "RuleOfftrack is not installed (the time-penalty variant is); the others belong to mini-games / challenges", "SAFE / challenge port"),
     ("MXFirstPersonCamera", "0x78 vf30", "00641dd0 (322)", "Player_UpdateCameras when player state +0x988 == 4 and not the gameplay camera; not reached by the C-key cycle / four presets", "SAFE"),
     ("MXStartGateCamera", "0x80 vf32", "00648390 (581)", "start-gate camera entity: not built in the product", "SAFE"),
     ("XenonInputManager", "0x10 vf4, 0x28 vf10, 0xb4 vf45", "008155f0 (26) 00815680 (8) 00815170 (51)", "vf45 only in the session-pause block ([0xda6734] is 0 offline)", "SAFE (00815170 ported since: loop_input_suspend.c)"),
     ("Database", "0x18 vf6", "0084cd30 (167)", "build_activity_game.c:303 (state 3 path with an async load request on a path the product does not take)", "SAFE"),
     ("BaseDriver / NetworkDriver / FreeRideDriver", "0x10 vf4", "00509330 (52) 0050bee0 (65)", "debug camera keys; forced FreeRide WindDown", "OUT-OF-SCOPE (debug) / SAFE (0050bee0 ported since: race_challenge_activity.c)"),
     ("CollisionEntity, DropGate, PhysicsEntity ...", "0xc vf3", "008b7c00 (7)", "7-byte stub (xor eax,eax; ret); world_vcall_f(... 0xc) is called on rider-attached entities only", "SAFE"),
    ]: w("| %s | %s | %s | %s | %s |" % r)
    w("")
    w("## Hidden-targets table (`stop_audit.py --hidden`: literals the census cannot see)\n")
    w("| address | size | via | class | note |\n|---|---|---|---|---|")
    sz = {}
    try:
        for l in open(os.path.join(R, "_re/out/functions.tsv"), encoding="latin-1").readlines()[1:]:
            c = l.split("\t"); sz[int(c[0], 16)] = int(c[3] or 0)
    except OSError: pass
    hrows = SA.hidden_targets()
    for ad, via in sorted(hrows, key=lambda x: ({"NEEDS-PORT": 0, "OUT-OF-SCOPE": 1, "SAFE": 2}.get(HIDDEN.get(x[0], ("SAFE",))[0], 3), x[0])):
        cl, nt = HIDDEN.get(ad, ("UNCLASSIFIED", ""))
        if cl == "UNCLASSIFIED": print("UNCLASSIFIED hidden %08x" % ad, file=sys.stderr)
        w("| %08x | %s | %s | %s | %s |" % (ad, sz.get(ad, "?"), "; ".join("%s (%s)" % v for v in via[:3]).replace("core/game/", ""), cl, nt))
    w("")
    w("## Sites\n")
    w("| file:line | kind | function: condition / target | class | evidence (rule) | addresses to port |\n|---|---|---|---|---|---|")
    for (s, cls, ev, text, addrs, prio, rid) in rows:
        f, ln, kind, fn, msg, via = s
        a = addrs if cls == "NEEDS-PORT" else ""
        w("| %s:%d | %s | %s: %s | %s | %s (%s) | %s |" % (f.replace("core/game/", ""), ln, kind, fn, short_target(f, fn, kind, msg, via), cls, ev, rid, a.replace("|", "/")))
    w("")
    open(OUT, "w").write("\n".join(o))
    print(sum(1 for r in rows), dict(collections.Counter(r[1] for r in rows)), file=sys.stderr)

if __name__ == "__main__":
    main()
