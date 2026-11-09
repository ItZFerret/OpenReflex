# race.md: the race layer (activity, rules, drivers, AI riders)

This covers core/game/race_*.c (oracle adapters in core/oracle/race_*.cpp, port group "rx_race", plan core/plan/rx_race.tsv).
Verify with `RX_CORE_FILES="race_* crt_*"` and the group `rx_race`.

## Object graph
- MXGame 0xa946b8: players at +0x7460 (0x12a0-byte players, local slots at +0x153ec, count byte +0x1541c), the activity manager
  (0055f700: +4 activities, +8 count). MXRecords (career statistics) 0xa946c0 / 0xd92f10. Session 0xa93fcc (+0x98 its event
  manager). Engine 0xa95844 (+0xc config, +0x10 frame info, +0x1c + 4 * [+4] the current view set: cameras +0x19b8, count +0x19d0).
- Player P: +4 local, +0x10 kind (0 human, 1 AI), +0x74 back pointer from the driver, +0x94 activity, +0x1ac vehicle, +0x1f0 pad,
  +0x8f4 AI tuning set, +0x11c8 network player (wanted pad state bits in +0x11cc), +0x116c / +0x1170 reset delay / state.
- Driver D (BaseDriver / RaceDriver / NetworkDriver): +0x10 track data (+0x9c start node, +0x1dc lap length), +0x14 / +0x18 /
  +0x1c section / nodes, +0x60 steering sum, +0x64 slow-down, +0x70 finished, +0x74 player, +0x90 track node, +0xe8 behaviours
  (count +0xee0), +0xf1c laps, +0xf24 lap times, +0x10b4 gap, +0x10c4 finish estimate, +0x10cc distance to the line, +0x1124 frozen time.
- Activity A (MXPlayer +0x94): +0x21c state (0 Pre, 1 Countdown, 2 InProgress, 3 WindDown, 4 Post, 5 Inactive), +0x234 audio /
  level object, +0x238 session state, +0x23c kind, +0x240 the state names (std::map<int,string>), +0x268 / +0x26c drivers,
  +0x27c / +0x280 network drivers, +0x290 event manager, +0x294 clock, +0x29c / +0x2a0 countdown time / count, +0x2a8 intro end,
  +0x2b0 / +0x2b8 countdown length / steps, +0x2bc time limit, +0x2c0 finish countdown, +0x2cc start board, +0x2d0 standings[12],
  +0xfe4 pending intro, +0xfe8 session result, +0x1070 + 0x15190 * i the 12 scripted sequences (names at +0xfe330),
  +0xff070 laps of the race, +0x13e644 / +0x13e648 holeshot / final stretch done.

## The activity state machine
- Activity::vf24 (00558930) dispatches the state update: vf25 Pre, vf26 Countdown, vf28 InProgress, vf27 WindDown, vf29 Post.
- SetState (005586c0, stdcall act, state): vf30 exit (old, new), "<old> Exited" event (value new), vf36 enter (new, old),
  "<new> Entered" event (value old) when someone listens (registry 00593220), then +0x21c = new. Exits: vf31..35, enters vf37..41.
- Pre (00558ce0): waits for loading and the session, unpauses the music, finishes the intro, polls the pads' start keys, then
  starts the countdown (intro / UI property 0x94d18c) or after a 0.5 s timer (0x9cf5a0) with a session result.
- Countdown: Activity::vf26 (00559190) steps +0x2a0 down every +0x2b0 / +0x2b8 s ("Countdown Step"), then InProgress;
  MXActivityRace::vf26 (005651b0) lets the start board (+0x2cc state +0x480 0 / 4, timer +0x4f4 < 0) start the race.
- InProgress (Activity::vf28 005594a0, MXActivityRace::vf28 00565270): clocks, vf42 end check, standings, gaps, lap flags, lapping
  warnings, the finish countdown ("FinishCountdown Step").
- End check MXActivityRaceStub::vf42 (00566ee0): all humans finished -> final gaps, AI behind the last human get an estimated
  gap and finish time (00567d50), WindDown; some finished -> after the wind-down delay (+0x2a4 > +0x2ac); in session state 8
  (a timed finish) "Player Wins" when the leader's time runs out.
- Standings comparators (vf43): the stub (00567520): out-of-race last, laps, distance (1/100), time (1/100), tie by slot;
  MXActivityRace (00565570): adjusted time (time + bonus +0x10c8 - +0x140) outside InProgress.

## Events (race_event.c)
Each raiser names its payload type (type_info name 0x916388) and dispatches through FUN_004020c0 (EAX manager, EDI name, payload,
size). The payload sits on the raiser's stack frame: call raisers with RACE_CORE_CALL at the original's stack depth (the event
system can keep the address). Seen: "Countdown Step", "FinishCountdown Step", "Player Wins" (Activity*, BaseDriver*, text, int),
"Holeshot", "HoleshotActivated", "Start", "<state> Exited / Entered", "Backwards", "Lap Completed".

## ActivityAudioLogic (race_activity_audio.c)
The audio listener plays named cues (std::multimap<string, Cue*> at +0x8c, cue modifiers DefaultModifier / CountDownStep /
HumanDriver) and crowd reactions. Crowd reactions draw the GAME random generator (0x9cb860): `chance > rand * 2^-15`, so the
audio logic is part of the gameplay random sequence and is ported (not skipped). Place cues (00581570): moving up to 3rd / 2nd /
1st cheers.

### The stunt listener and the 00592b60 "unported" stop (stuntaudio, 2026-10-20)
The "PhysicsStuntBegin" subscription `build_activity_audio.c` makes for the activity's audio logic (`rx_audio_logic_vf29` -> `audio_stunt_handlers`,
logics built from vtable 0x00945d64, signature void(MXPlayer*, const StuntDescriptor*, PhysicsStuntType)) stores the *member-function pointer* `00592b60`.
That address is not a function body: it is the MSVC **vcall thunk** for vtable slot 6 (`mov eax,[ecx]; jmp [eax+0x18]`), i.e. `ActivityAudioLogic::vf6`
**005818c0** = `race_activity_audio.c` `rx_audio_driver_cheer` (the human driver's crowd cheer: excitement 0x9c47f0 through `crowd_excite`, the game LCG
draw 0x9cb860 / 0x9cb864, and by chance 0x9c4818 the reaction 2 through `crowd_react`; a null / AI driver returns at once). It is ported and its adapter
(`core/oracle/race_activity.cpp` `RX_PORT(0x005818c0, Audio_DriverCheer, "rx_race")`) is Verify-clean. **It is not audio-only**: vf6 advances the game's
random generator (the same 0x9cb860 sequence the AI and the effects draw), so a product no-op would diverge from the original; it runs the port.

The stop at 00592b60 was a **product dispatch-table gap**, not a missing port. The call chain is `rx_player_update` -> `rx_fx_trick_update` ->
`rx_fx_stunt_update` -> `rx_fx_stunt_begin` -> `rx_fx_stunt_event` ("PhysicsStuntBegin") -> `post_event` -> `rx_world_event_dispatch` (004020c0) ->
`rx_world_handler_list_call` (00592d80) -> `rx_event_proxy_call3` (0058f270, the shared proxy vf2) -> `rx_call_original([proxy+0x10])` -> 00592b60.
`rx_call_original` in the product may only call the core functions listed in `tools/gen_fnsig.py`'s table, and that scan reads a literal
`RX_FUNCTION(0x...)` **and** the function definition (it now also preprocesses every game file, so macro-generated registrations are
listed too: `-DRX_FNSIG_SCAN`, the "fnsig" unit). `build_vcall_thunks.c` registered its 19 thunks through a macro (`RX_FUNCTION(RX_ADDR_00(addr), ...)`
around a macro-generated body), so none of them were in the table: the thunk was registered (`rx_fn` finds it) but `rx_fnsig_is_int` refused it and the
product aborted "unported game function 00592b60". Fix: the file now writes the 19 thunks and their registrations out one by one (same bodies, literal
addresses), so the table carries 0x00592a70 .. 0x00592bc0. The product call is then `rx_call_original` -> `rx_vcall_thunk_592b60` ->
`rx_vcall(slot 0x18)` -> `rx_audio_driver_cheer` (lldb breakpoints on both, backtrace as above).

Evidence (builds/stuntaudio): (1) the preserved-object 12-rider clean ride that stopped at 00592b60 (`builds/inputrace2/current`, 477 racing rows /
1575 steps) relinked with **only** the table entry added produces the same 1575 rows byte for byte (state hash 1b1fa79bcbbdcb74) and now runs the handler,
stopping at the *next handler of the same event*, **00565710** `MXActivityRace`'s "PhysicsStuntBegin" listener (gameplay: sets the activity's
+0xff080 / +0xff078 and 0x9c3edc; unported). (2) The current tree's 12-rider clean ride goes **556 racing rows (race 0..555) / 1932 steps** and stops at **0055e8b0**
`Activity::vf12` (through the "VehicleEntered" delegate `rx_delegate_invoke`); its route no longer reaches a stunt before that because steerfix's A/D
routing changed the clean line from race 200. (3) Oracle per-call Verify of the handler with the new probe `core/oracle/race_stuntaudio_probe.cpp`
(frame hook 0084ed20, `RX_STUNTAUDIO_PROBE=1`): **005818c0 9 calls / 9 verified / 0 mismatches** on Concord (8 human calls covering both sides of the
chance test with 6 crowd emitters + the null guard) and **5/5/0** on Concord_ai11; both registered adapters at that address report the same. The five
reference worlds still reach it 0 times (no world raises a stunt).

## AI riders (behaviours, beh1 / beh2)
- A behaviour has +4 enabled, +8 driver; vf4 (dt) runs a frame and returns nonzero to take control, vf5 type id, vf6 reset,
  vf7 pause. The driver runs them in priority order (+0xe8, count +0xee0); the first nonzero vf4 wins (+0x2ec).
- Outputs: +0x3c0 throttle, +0x3c4..+0x3cc, +0x3d0 reverse, +0x3d8 / +0x3e0 lean, +0x60 steering sum, +0x64 throttle scale,
  AI control channels +0x2f0..+0x324 (0050ac80 off / 0050acc0 on).
- Difficulty: tuning tables in .data indexed by `player +0x8f4 + 4 * (3 * driver +0x328 + driver +0x32c)` (tuning set, row,
  column). AI_PARAM getters in race_ai_params.c read personality rows (driver +0xee8) and difficulty (+0x8f4 / +0x8f8 / +0x8fc).
- Steering / racing line: NavigateTrack picks a look-ahead point on the line side (+0x0c: forced by +0x310, else TakeLine, else
  by the lateral fraction with hysteresis) and pushes the steering toward it; a hard +-1 steer when facing backwards.
  TakeLine / SplitSection keep a lane (+0x314 / +0x310) with an edge repulsion `-(dot - 0.5) * (A / |r|^a - B / |r|^b)`;
  AvoidTrackBoundaries is the same Lennard-Jones-like repulsion from the edges. OverrideSpline follows authored splines.
- Braking: HandleBraking casts the velocity (rotated by the steering) against the edge over 30 units of nodes and sets the
  slow-down +0x64 from the hit fraction; BrakeTurn / MonitorCorner find the next corner run (node flag 0x200).
- Traffic: BehaviorSeperate rays (centre / left / right) for contact and crossing times; avoidance steers or slows; target choice,
  passing side by the room on each side, follow when slow. BlueYellowFlag moves aside for a rider with more laps behind.
  AvoidWreck reacts to a wreck warning with a random chance and delay.
- Recovery: FixForwardProgress (stuck: steer away, then reverse); Offtrack; AvoidFlipBack (throttle eased, lean when pitching back).
- Air: AirLeveling uses the landing prediction (00461980, one airborne rider per frame round robin 0051ff80) and leans fore / aft
  3 * |v . n| and sideways toward the line target.
- Catch-up (rubber band): LapPacing raises the vehicle tuning (RavenVehicle_SetTuning) in proportion to the speed deficit to the
  target lap time (class limits 0.9 / 0.95 / 1.0 and caps 50 / 100 / 150); BehaviorSuckRacing (tuning set 3 only) every 5 s
  rolls rand % 150 against the human's place and may disable a behaviour for 4 s.
- Forks: 0050a360 picks a section by per-class flags (random, line side, speed limits) with race_rand.

## Landing prediction (race_misc_landing.c, 00461980)
Ballistic flight (gravity 32.2) from the vehicle position plus (0, -2 or -1, 0) rotated by the orientation quaternion. The fall
time to the level floor (terrain +0xdc) bounds the search; slow horizontal speed: one vertical ray; otherwise 16 parabola points
make 15 segments: physics bodies first, then the terrain. Time = horizontal distance / horizontal speed. SSE 4-lane code,
reproduced lane by lane with the x86 NaN rules.

## Vehicle safe point / respawn
vf34 00466340 remembers a reset point only on the ground, upright, not crashed, inside the terrain bounds and above a speed
threshold. A reset (00550650) uses the activity's respawn point (vf19 00569010: the driver's node moved forward past "no respawn"
nodes, moved across the track toward the rider's side by half the node width) or the vehicle's own pose.

## Traps (all reproduced)
- .data constants (>= 0x9a6000) are read from memory (e.g. 0x9cd9e8 = -1, 0x9d7b30 = -2, 0xd61670 vectors, 0x9c4810 chances).
- Frames: callees keep stack addresses (events, the landing prediction, AirLeveling records): mirror the original's frame
  (RACE_FRAME_EBP / RACE_FRAME_PLAIN / RACE_ORIG_FRAME_ENTER) and call core callees through RACE_CORE_CALL(nargs, ...).
- Functions that write their own argument slots: 005381f0, 0050a360, 00558ce0, 0055dff0, 00569eb0, 00582150, 00501a30,
  00568620, 007532e0 (adapters / core write them too).
- Register-argument callees: check what each callee reads (ESI / EDI / EBX) before calling it with rx_call_original_ex
  (e.g. 007d3040 ESI, 00747810 ESI, 005e0270 EDI, 006ccbc0 EBX, 007c5120 EDI, 006817c0 ESI).
- 00462130 returns in AL only, the upper EAX bytes are the last callee's; 007f8fa0's callers read ECX.
- x87 on the main thread runs at 53-bit precision (CW 0x027f).
- 0047c190 (pose override update) passes its matrix rows on its stack to the simulation's vf10: Verify passed with another
  frame, but FreeRide's On-mode memory hash differed until the original's frame (and the constant rows it first writes) was mirrored.

## Online presence / stats (0055cf10, rx_activity_presence)
Called at the end of Activity::vf23 every frame. Every 60 s it uploads the player count (key 0x12); every 20 s in a race in progress
while the track is in view it uploads the elapsed / limit minutes (keys 0x15 / 0x16) and steps the online session clock 006decd0.
The stats object is the **PCSession** `[0x00da6cb0]` (= `[0x00da6ca0] + 0x10`, built by MXGame::Startup -> 006de390 -> ctor 006e9120,
vtable 0x009649dc); slot 12 is `PCSession::vf12` 006e9e00 = Steam presence / matchmaking + `NetworkSession::vf12` 006e3a90 (Quazal
presence), both only with the online-session word +0x1c4c. The activity's +0xfec / +0xff0 timestamps are the local part and are always
written. Product: no PCSession (offline stand-in 0), so both the presence set and the session clock are skipped (oracle original);
known-differences 15, QUEUE "presencefix". Not to be confused with the network driver replication (race_net.c), which is local pose
state and is ported.

## Cannonball rule, rule sound, network player (race_rules.c, race_sound.c, race_net.c; 2026-10-19 racedrv)
- RuleCannonball (FreeRide tunnel; added by MXActivityFreeRide::vf47 00587690): 0051a300 vf5 keeps a state (+0x10: 0 outside, 1
  inside, 2 leaving) over the cannon spline (the activity's track object +0x21e10: sections +0x130 / count +0x1d8, nodes +0x64 next /
  +0x60 prev, edges +0x10 / +0x20). Entering (0051a690) cuts the viewport's fade layer and creates the warning loop (event name
  from the world path 00581490 / 00581350, tag "CannonballWarning", silent, started). Inside, 0051a110 tests the exit, the progress
  (squared distances) drives the sound volume (0080b410) and the music channels (00807130 volume, 00807170 fade) and the white fade;
  at the exit the player is launched (0054f150: vehicle position to the point, then 00550920 resets the player where the vehicle
  is) and the driver gets vf9. Leaving stops the sound (0080ade0 / 0080ae10) and fades the channels back.
- 00407430 is the nearest point of a segment to a point (EAX A, ESI P, cdecl B; ECX out), not a point copy: the earlier port of
  00519fa0 passed no registers to it and would have faulted the first time the exit was inside (found by the probe).
- Sound events run on the audio thread ([0xd67d1c]): 0080ade0 / 0080b410 stop / set the volume of an event now (0080ae10 FMOD
  Event::stop + the audio log; 0080b490 ApplyVolume) or queue the command (0080c280, EBP of the caller saved in the queue record).
- NetworkDriver (0050e510): the network player object N (driver +0xef0) is built from replicated floats; 006feb00 snapshots it
  (also the +0x1f0 position copy), 006fed10 the controls part, 0050e7e0 applies a snapshot to the vehicle's remote pose target
  (+0x1220 / +0x1214) and turns the vehicle's network mode (vf49) on or off, 006fe210 (stdcall N, dt) runs the replication: the mean
  and minimum distance to the other network players (Quazal selection iterator) pick the send intervals (0.333 / 0.5 / 1 / 10 s,
  0.001 x table) and raise the change markers (006dcc70). The Quazal / STL layer under it is a boundary (race_net.c header).
- BehaviorSuckRacing modes 1 / 2 (005352c0 / 00535380) draw the game LCG (race_rand) for a decision time / threshold; mode 2 compares the fraction
  of the run left (00526c60: the vehicle's projection on the driver's segment, then the nodes' lengths while their class flag has 0x200) with it.
  005256d0 (lap pacing gap) moves the AI's time base +0x10bc by the gap (clamped to +-5) and a third of the activity's time-table step by tier.
- Register contracts that earlier call sites missed (all unreached offline, fixed with the ports): 0050b330 EDX = controls
  (+0x3c0), 0050e7e0 EDI = driver, 0050bc70 EAX = network object.
- Verification without a world: oracle/race_net_probe.cpp (frame hook, RX_RACE_PROBE letters, see the file header) calls the original
  addresses through the Verify path with live objects or objects built on the game heap; instruction coverage of the original under
  those runs shows what the probe reached (REFLEX_COVERAGE). Not reachable offline (documented in plan/QUEUE.md): the Quazal iterator
  loop of 006fe210 without the probe's iterator stand-ins, the change marker (006dcc70 / 00884c90 fault in their critical section
  without the session's statistics table: stand-ins that log their arguments), 006f8860 (session mode: stand-in).

## miscport: the census leftovers (race_activity_misc.c, anim_scene_misc.c, anim_scenario_misc.c, loop_cam_tuning.c)
Per-function verification counts and what no world reaches: plan/QUEUE.md "miscport". Things the next reader should know:
- Argument order traps (found by Verify): 004e21d0 / 004e2340 read their SECOND stack argument first: PlayerUIDHigh = arg1, PlayerUIDLow = arg0
  (the call site in rx_records_update passes (rec + 0x6918, rec + 0x691c)); in the criteria-building helpers the criterion's address
  (frame + 0x28) and the vector's (frame + 0x10) end up as stale pointers in freed heap blocks (the criterion's path-vector proxy), so a frame off by
  4 bytes shows as a heap difference.
- A direct call of a core function from a function that, under RX_ORACLE, calls an original: do not wrap the original in RACE_CORE_CALL (rx_call_original
  pushes its own arguments and return address; a second set 12 bytes deeper made the original's stale stack pointers differ).
- 007e0190: the entity's virtuals are called as the original does, 0x64 (volume, the object whose vf1 (+4) is the type 0 .. 3) up to four times (once per
  type test), 0x5c (event id) up to three times, 0x38 (matrix) once; vf6 of the volume takes three out pointers (centre / extent / radius). A removed
  entity absent from its list writes the slot before the array (index -1), as the original.
- 005dfc90: the std::string of the current language is read inline (capacity +0x18 < 0x10 -> the buffer at +4); the table helpers 005e3ee0 / 005e3ff0 are
  inlined. ECX / EDX at return are not modelled (the callers in the core read EAX).
- 0055c1d0 is the target of rx_players_sync's "player leaving" branch (build_activity_player.c) together with 00552c60; MXRecords_Leave (00764560) is called
  for a local human (player +4 != 0, +0x10 == 0) unless the activity's state +0x238 is 4.
- 00592ef0 (remove a listener from an event channel): while a dispatch runs (busy count +0x3c >= 1) the listener is only told (vf5), else deleted (vf0(1)).
- The XML reader (loop_cam_tuning.c): slot 4 is "the root node" and takes NO arguments although the compiler pre-pushes the next calls' arguments before it;
  slot 0x20 is first-child(node, 0, &iterator), 0x28 next(iterator value), 0x24 release(iterator value).
- misc_probe.cpp (REFLEX_PORT=<ports>,rx_MiscProbeHook REFLEX_PORT_VERIFY=1 RX_MISC_PROBE=<letters>): m state-name map, l light registry (fake entity /
  volume objects whose virtuals are natives on padding bytes of the exe), p player table clean-ups + slot free (c60 needs rows added with the game's own
  004dd460 / 004de810 / 004dea90 builders), s attribute table, r MXRecords_Leave, o podium scenario, d discovery tracker. Instruction coverage of the original
  under a probe: `builds/miscport/covp.sh` + `covq.py` / the disassembly (`dis.sh`, whole-function listing: the shared disasm tool stops where capstone does).

## aimotion (2026-10-20, agent "aimotion"): do the 12 AI riders actually ride in the product?

**No.** The AI decision code runs, but every rider is on the ground from race frame 1109 on, so the AI controls are switched off and the riders never leave the
start. The root cause is not the AI frame code: the AI data tables are never loaded in the product.

Data (`builds/aimotion/`): `dump_ai.c` (product driver: session create, default National NAT_Concord 12 riders AI 95, `rx_session_advance_dt(step_dt)`, direct
reads of the live slots / drivers / vehicles / simulations every N frames, no input), `product_ai.txt` (5000 frames, every 60), `product_ai_gate.txt` (1120
frames, every 1, the gate drop), `oracle_ai_jit.txt` / `oracle_ai_long.txt` (`REFLEX_JIT=1 REFLEX_WORLD_TEST=builds/framecmp/worlds/pc1`, `REFLEX_RACE_STATE`
every 30 / 120, 3000 / 5200 frames), `oracle_trace.txt` (`REFLEX_FRAME_TRACE`, the same fields as `dump_product`).

- Product: 12 drivers in the activity list. `rx_ai_race_update` 00510010 runs every InProgress frame (driver +0x2ec = -1 after the loop, 17 behaviours, 4 rules, the
  start stage +0xf10 and reaction +0x13c set), and the AI controls reach the vehicle (with the wreck cleared, throttle 1.0 appears and the bikes move). But at
  race frame 1109 (race time 0.017 s) every vehicle +0x280 becomes 20 (0x14) and then 19 (0x13): `rider_fall` 0054ed10. From then on the AI outputs are 0
  (00510010 / 00509370 wreck guard), RuleTrackAdvance 0051d400 skips (`vehicle +0x280 != 0`), driver +0x18 (next node) never moves and the bikes stand:
  the 5000-frame state is the one in `builds/camsnap/api_pf_full5000.log` (place 6, 65.063 s, laps 0/3, speed ~0.7, positions frozen after ~race+9 s).
- Oracle: race starts at world frame 1156. The AI speeds are 2-22 m/s at race+0.7 s, 24-50 at +1.7 s, 8.8-84.8 at +30 s; at race time 67.4 s two AI have
  1 lap (places 1-2), the W-held local is 12th with 0 laps. The local at its drop keeps velocity.y -0.46 -> -0.29 and moves forward (1.8 m/s by race+0.1 s);
  the product local, with no input, gets velocity.y -0.5 -> +1.0 and no forward motion.

**Root cause: the AIData tables are zero.** All 11 AI report start chance 0.00 (0051dcd0: table 0xa89d08, row driver +0xee8 = 3, index tier*3+level =
0x8f4*3 + 0x8f8). In RaceDriver::vf5 0050fb00 `chance < pct` therefore always holds, so every AI is put in start stage 4 (immediate launch, +0x10f8 = clock-3),
i.e. all 11 launch on the first InProgress frame while the DropGate is still swinging down (state 0 at frame 1105; state 1, 0.075 rad at 1108; state 4 at
1120; gate object 0x11882ea0, activity 0x26bc5270, from the diagnostic scan). The bikes push into the gate and get thrown up; the WreckChecker (006373e0, already
per-call verified) then sees the pitch/roll rates and declares the fall for all 12, including the input-less local. The oracle's AI launch spread (at race+0.7 s
eight AI are at 7.9-22.7 m/s while four are still at 0.4 m/s) is the original's stage mix: most AI wait for their reaction (stage 5/1/2) and launch after the
gate is down. The product's table being zero collapses that mix to stage 4 for all 11.
- Why the tables are zero: the init thread 0075ed60 loads `Database\AIData` and, after the AIData package, calls **0051e350**: the loader of the AIBehavior /
  AIAbility / AIAbilityOverride tables into 0xa89d08.. (memset 0xa24c, the 0xa89d68 chance rows, the 0xa89dc0 personality rows, the behaviour class vf1s
  0051eed0 .. 005341d0). The product's init stand-in (`api/rx_session.c` `build_package_loaders`) loads only Animations and RiderPhysics, and 0051e350 /
  0051e430 / 0051e7b0 / 0051eac0 / 0051e1d0 are not ported (grep: no core function; functions.tsv 221 / 872 / 766 / 1030 / 375 B). The tables in the product's
  .data stay zero.

**Fix landed (blocker on the way):** 00447790 `RavenSimulationMotocross::vf31` (the normalized sum of the two wheel contact normals S+8a10 + S+88c0; the
same guarded normalization as `phys_normalize3`, EAX = out as the caller reads it) in `game/bike_sim.c` (`bike_wheel_contact_normal`) + adapter
`oracle/bike_sim.cpp` (rx_bike). The enter-rest test 004324d0 calls it once a bike is settled and not wrecked; unported, the product aborts there (diagnostic:
with vehicle +0x280 cleared each frame, "unported game function 00447790 reached" at the first racing frame). It is not reached while the wreck persists.

**Remaining (the exact fix, not done):** port 0051e350 + 0051e430 / 0051e7b0 / 0051eac0 / 0051e1d0 (the AIData table loaders; the database row iterator
uses vtable +4 / +0x20 / +0x24, column +0x1c, string +0xec / +0x13c, value arrays +0xcc / +0xf0; names DriverType_* (14) / DriverAbility_* (18) /
Player%02d, grouptype "vector"; the 0xa24c-byte speed/chance block and the per-personality rows), load `Database\AIData` + package "AIData" after
RiderPhysics in `build_package_loaders` and call the loader. Until then the AI cannot ride: chance 0 forces every rider into the gate.

## raceend (2026-10-20, agent "raceend"): the finish / results chain

The oracle finishes the Concord National (12 riders, 3 laps): `builds/raceend/logs/world_32000.out` / `race_state_32000.txt`
(REFLEX_WORLD_TEST=builds/framecmp/worlds/pc1, 32,000 frames). The AI finish at race times 202.28 .. 230.83 s; the input-less local
human (W held) finishes at **473.884 s**; the activity enters **state 3 (WindDown)** at ~frame 29640 and **state 4 (Post)** at
~frame 29820, 180 frames (3.0 s = Activity::vf40's +0x2c4) later through the UI page wipe. The results page (Post sub-state 3) is not
reached by 30,200 frames; the records are written at the Post enter (below).

### What the original executes (REFLEX_COVERAGE from frame 29000, `cov_finish2.bin`; instruction counts of the originals)

| address | function | count |
|---|---|---|
| 00559720 | Activity::vf27 (WindDown update) | 6144 |
| 00565260 | MXActivityRace::vf27 (thunk) | 955 |
| 00559600 / 00568f60 | Activity::vf40 / stub vf40 (WindDown enter) | 21 / 13 |
| 00565500 | MXActivityRace::vf34 (WindDown exit) | 17 |
| 00568d50 | MXActivityRaceStub::vf29 (Post update) | 6275 |
| 00569a90 | MXActivityRace_UpdatePost | 10258 |
| 00569a40 | FUN_00569a40 (Post sub-state / results event) | 24 |
| 00559900 / 00568da0 | Activity::vf41 / stub vf41 (Post enter + records) | 442 / 240 |
| 0055e990 / 0055e9e0 | "ShowPageWipe" / the page-wipe callback | 252 / 232 |
| 0055a440 / 0055a470 | players' +0x214 = 0 / result flags | 92 / 16124 |
| 00764fc0 / 00764cb0 / 00764d80 / 00764e50 / 00765020 | MXRecords AddInt / Placing / Score / BestLap / AddTotalTime | 504 / 61 / 62 / 51 / 35 |
| 00567d50 | FUN_00567d50 estimated gap | **0** (the local human finished last, no AI behind it) |
| 00569770 | FUN_00569770 finishing-order arrays | **0** (session state != 7) |
| 00568fb0 / 0057e870 | Post exit | **0** (no state 5 in the run) |
| 00587300 / 00587ea0 | "Post State Complete" / "Post Scenarios Start/End" raisers | **0** |
| 0074a1e0 / 0074a340 / 0074a110 / 0074a540 | RaceFinishScenario vf0 / vf1 / vf3 / vf6 | 384 / 589 / 33 / 360 |
| 0074a660 / 0074a770 / 0074a630 / 0074f980 | RaceResultsScenario | **0** (sub-state 3 not reached) |

### Ports (`game/race_finish.c/.h`, adapters `oracle/race_finish.cpp`, group rx_race)

`rx_activity_wind_down` 00559720, `rx_race_wind_down` 00565260, `rx_activity_wind_down_enter` 00559600,
`rx_race_stub_wind_down_enter` 00568f60, `rx_race_wind_down_exit` 00565500, `rx_race_stub_post_exit` 00568fb0
(+ `rx_race_post_exit_thunk` 0057e870), `rx_activity_post_enter` 00559900, `rx_race_stub_post_enter` 00568da0
(+ `rx_race_post_enter_thunk` 00565560), `rx_race_stub_post` 00568d50, `rx_race_update_post` 00569a90,
`rx_post_state` 00569a40, `rx_activity_players_zero` 0055a440, `rx_activity_result_flags` 0055a470,
`rx_show_page_wipe` 0055e990, `rx_page_wipe_transition` 0055e9e0, `rx_event_activity` 00587300,
`rx_event_scenario` 00587ea0, `rx_race_estimate_gap` 00567d50, `rx_race_standings_reset` 00569770,
`rx_records_placing` 00764cb0, `rx_records_score` 00764d80, `rx_records_best_lap` 00764e50,
`rx_records_add_total_time` 00765020. Call sites: `rx_race_stub_check_finish`'s `estimate_gap` (was `rx_call_original_ex`),
and the vtable dispatch (`rx_activity_state_update` / `rx_activity_exit_state` / `rx_activity_enter_state`) resolves the new
registered functions. The two race-class thunks 00565560 / 0057e870 are registered because the vtable slots point at the `jmp`
thunks, not at the bodies.

### Verification

- Long world per-call Verify (12-rider Concord, filter = the 26 names above): see the final numbers in the QUEUE entry
  `### raceend`. The first run (before two fixes) exposed: (1) 00565500 inverted its `+0x22148` test (wrote 0 instead of 5 /
  vice versa); (2) `00765020` keeps the sum in its **first argument slot** (the ref pointer's dword at entry ESP+4, `F+0x14` after
  its four pushes), not in the ref structure. Both corrected.
- Probe (`oracle/raceend_probe.cpp`, `REFLEX_PORT=...,rx_RaceEndProbeHook`, `RX_RACEEND_PROBE=1 RX_RACEEND_PROBE_FRAME=1200`):
  at frame 1200 of the reference world it calls each address with the live activity (state 3 poked for the page-wipe callback,
  `+0x2c4` / `+0xff064` / `+0x13e640` run out, the Post sub-states 0..3, a live pair of drivers for 00567d50, records slot 0).
  Result (`builds/raceend/logs/probe6.out`): **every port 1/1/0** (00569a90 4/4/0, 0055e9e0 3/3/0), including 00567d50, 00569770,
  00765020; the two event raisers are **0 calls** (calling them at a mid-race state crashes the original: 008519a9 through
  00587325 / 004020d9, their UI handlers need the finish page), so they are code-reviewed twice, not per-call verified.
- Not reached by the natural run: 00567d50 (needs an AI behind the last human), 00569770 (session state 7), 00568fb0 / 0057e870
  (state 5), 00587300 / 00587ea0, the RaceResultsScenario. They are either probe-verified (the first three) or reviewed.

### Boundaries / not ported

- **UI page wipe** (0055e990's "ShowPageWipe" on `[0xa93fcc]+0x98`): the product has no UI manager; the dispatch is skipped there,
  `+0xff06c` is written as the original. The oracle branch calls the original 004020c0 for the UI channel (the core event system is
  the same; this avoids the UI handler's Lua-heap allocation residue in Verify). The page-wipe callback 0055e9e0 itself is ported
  and is what enters Post.
- **Profile / Steam**: 006d8420 (highlight start in Activity::vf41; its highlights 0x17 .. 0x1d are online-only) stays a product no-op,
  oracle original. 00569930 MXActivityRace_FinishHighlights is ported since "finishhl" (2026-10-29, `game/race_finish_highlights.c`,
  with 006d8530 and the body-panel checks 0047baf0 / 0047bed0 / 0047bf00): the finish adds the profile highlight progress 0x2c / 0x26 /
  0x28 / 0x29 through 006d7980.
- **Replay**: 006aa8d0 / 006aa990 ClipRecorder_Start/StopPlayback are product no-ops, oracle originals (the product records
  nothing; the replay recorder is a separate unit).
- **Finish / results scenarios** (superseded 2026-10-26, w-podium: RaceFinishScenario vf0 / vf1 / vf6, FMXFinishScenario and
  FMXResultsScenario are ported in anim_scenario_finish.c and run in the product; RaceResultsScenario and the shared helpers were
  ported before; see core/plan/QUEUE.md "w-podium"). The original text: RaceFinishScenario (0074a1e0 / 0074a340 / 0074a110 / 0074a540, plus 0074e230 / 0074f1a0 /
  0074f510 / 0074f260 / 0074f590 / 0074a070) and RaceResultsScenario (0074a660 / 0074a770 / 0074a630 / 0074f980 / 0074f0f0) are
  **not ported** (presentation: podium riders' visibility, model flags, scenario events, crew-chief animations). The core's
  `rx_race_update_post` calls `rx_scenario_start` / `rx_scenario_intro_update` (ported) which dispatch these through the scenario
  vtables: the oracle runs the originals; the product cannot reach Post (the UI page wipe callback is absent) so it does not abort
  there. If the UI boundary is ever lifted, porting 0074a1e0 is the next stop.

### The product's end-of-race path

The session API already maps the finish: `api/rx_session.c update_state` returns `RX_SESSION_FINISHED` when the local driver's
+0x70 is set or the activity state is 3 / 4 / 5. With these ports the first finish stop in the product is gone:
`rx_race_stub_check_finish` (00566ee0) sets state 3 -> enter vf40 (00568f60 -> 00559600) -> the session reports FINISHED while the
activity is in WindDown; vf27 (00565260 -> 00559720) then ticks the 3 s wind-down, sorts the standings and calls the page wipe
(product: no UI, `+0xff06c` set). **It stays in state 3**: `build_activity_start.c` does not subscribe the PageWipeTransition
handler in the product (no UI manager), so 0055e9e0 never runs and Post (records, results page) is not entered. The standings
getter (`rx_session_standings`) still returns the finished order and times; the records (`g_MXRecords` rows) are not written in the
product. The product's ride cannot finish yet for an unrelated reason: the local rider is thrown on the second InProgress frame
(start crash; startdiag/race units), so no product run reaches the finish.

## ridepath (2026-10-20, agent "ridepath"): the BehaviorOfftrack return, the reset over a reset material, and where the ride stops now

Follow-up to startdiag: with the wreck checker fixed the AI stay upright and leave the track (AIData was still zero then), so the
product stopped in the AI's offtrack behaviour; the 4-rider run stopped in the local player's reset-over-material path. Both are
ported now (core/game, adapters group `rx_race`), plus the probe that verifies them per call.

- **00529190 `rx_behavior_offtrack_aim`** (race_beh1_small.c, BehaviorOfftrack state 1, 414 B): the vehicle's flat heading
  (+0x130 / +0x138, normalized) against the direction to the rule's return point - the track rule's when there is one (+0x1c, its
  point at +0x10 + 0x30 / +0x38), else the gate rule's (+0x20 + 0x90); the point's x / z go to the behaviour's +0x28 / +0x2c (the
  z through the original's whole-dword store). Heading toward it (`dot >= 0.2`, 0x92e388; `comiss` + `jbe`, so unordered counts as
  toward) -> state 3, else 2. The `len2 != 0` normalizations are C `!= 0.0f` (the original's `ucomiss` + `lahf` / `test ah,0x44` /
  `jnp` takes the sqrt path on unordered exactly like `NaN != 0`). With **neither** rule the original reads its own scratch slot
  `[esp+4]` (the heading length^2 **word**) as the point's address; watch() (00529150) only enters state 1 with one of the two, so
  it is unreachable - kept as `src = (rx_addr)rx_fb(l2)`.
- **00529330 `rx_behavior_offtrack_return`** (race_beh1_small.c, states 2..4, 1401 B, takes control): (1) the node run: drop the
  next node (+0x18) when the vehicle is past it (the node's direction +0x50..+0x58 against `vehicle - node > 0`; `jbe` = a NaN does
  not advance) and advance the section (+0x10) when the run ends (`section = *(*(section+8))`, prev = `*(section+0)`, next =
  `*(prev+0x64)`). (2) the aim: state 3 re-tests the heading against the point kept at +0x28 / +0x2c (`dot >= 0.2` keeps state 3 and
  the point - the *unnormalized* delta stays in the frame locals, the original does not store the normalized one), otherwise state 2;
  states 3 / 4 take the track's look-ahead point (0050a650 `rx_track_look_ahead`: EAX node, EDI out position, stack out direction +
  the AI tuning distance `0xa04b38`), state 2 the flat one (0050a5f0 with ESI = the aim slot). (3) the steering: the offset from the
  look-ahead direction (the AI record `0x9e3468` + 16*index: `(c / 1^a) - (b / 1^d)` with the CRT pow, base 1.0 from 0x977638;
  index = `player +0x8f4 + 4*(3*driver+0x328 + driver+0x32c)`), x / z normalized and scaled, rotated into the vehicle frame
  (005084f0); with the aim direction not mostly ahead (`dot < 0.98`, 0x9cd9bc) or the vehicle more than 3 units (0x92fe48) off the
  line of its heading (the matrix's x row, vehicle vf14), the driver's steer bias +0x60 += the rotated x * the heading factor
  (0050b150) and, unless airborne (vf43), +0x64 = 0.3 (0x931da4) above 58.6664 (0x9d78a4) speed else 0. (4) back on the track (the
  rule's +0x50 / +0x10) -> state 0 and the `+0x0c` sub-object's vf6 (the vcall the original makes at the end). Frame
  `RACE_FRAME_EBP(F, 16, 0x44, 3)`: the heading at +0x18, the vehicle at +0x20, the aim at +0x28, the look-ahead direction at +0x30,
  its point / the rotated vector at +0x40 (the pushed ebx / esi / edi live at F+0..+8, exactly the original's `sub esp,0x44` + three
  pushes).
- **00550b70 `rx_player_reset_terrain`** (race_player.c, 330 B): the reset over a "reset" terrain material (0054f3e0, material 7).
  The same body as 00550650 (`rx_player_respawn`, race_activity.c) except the reason is **6**: unless a reset already runs
  (+0x116c above 0; `comiss` / `ja`, so a NaN runs it), the activity's respawn point (vf19 (player, position, direction, up)) or,
  without one, where the vehicle is (position +0x200, the forward / up axes of the quaternion +0x210 with the constant 2.0
  0x92e330), then 005501f0 (`rx_player_reset_to`, EDX = position, ECX = orientation) and +0x1170 = 6.
- Call sites switched: race_beh1_small.c `rx_behavior_offtrack_update` (the B1_EX0 / B1_EX of 00529190 / 00529330 -> direct calls;
  the update returns 1 for states 2..4, 0 for 1), loop_player.c `rx_player_check_reset_terrain` (`pl_call0` -> `PL_CORE` +
  `PL_AT(0, 0, 0x0054f4a8, ...)` at the original call site).
- **Per-call verification** (`core/oracle/race_ridepath_probe.cpp`, group name `rx_RidePathProbeHook`, `RX_RIDE_PROBE=<letters>`,
  `RX_RIDE_PROBE_FRAME=<n>`; one world run: `builds/ridepath/rp.sh National_NAT_Concord_ai11 700 2 REFLEX_PORT=rx_race,rx_RidePathProbeHook
  REFLEX_PORT_VERIFY=1 RX_RIDE_PROBE=or RX_RIDE_PROBE_FRAME=650`, log `builds/ridepath/probe_o4.log`): no reference world runs the AI
  driver update at all (REFLEX_COVERAGE of Concord_ai11 1400 frames: 0 original instructions in 0x0050xxxx..0x0054xxxx driver code;
  the activity reaches state 2 but the drivers are never updated), so the probe calls the ports on the live driver objects at one
  frame: **00529150 12/12/0, 00529190 15/15/0, 00529330 42/42/0, 00550b70 2/2/0**. The AI's own offtrack state machine is run as the
  driver loop would (watch -> state 1), then on the first behaviour the two aim outcomes the stationary field never produces (the
  rule's point 20 units ahead / behind the vehicle: state 3 with the point kept, state 2 with the look-ahead) and the "still off the
  track" tail (the gate rule's +0x10 cleared -> the state stays). The off-track rule pointer (+0x1c) is **null** on every driver in
  these worlds (only the gate rule +0x20 is set), so the aim reads the gate rule's point (0 until the rule runs). The probe then
  builds the missing inputs: a track-rule block on the game heap (+0x10 with the point 20 units ahead, +0x50 on track), the section
  advance (a section's last node with the vehicle put past it), the return-to-track tail's vf6 on +0x0c (a live behaviour) and the
  fast-vehicle +0x64 branch (the vehicle's +0x150 at 100), and 00550b70 twice (with the activity's respawn point and with the
  driver's progress node cleared, the vehicle-quaternion fallback). Instruction coverage of the originals under the probe
  (`builds/ridepath/covrange.py` on the REFLEX_COVERAGE file, log `probe_o7.log`): **00529190 79/80** (only the unreachable
  `[esp+4]`-as-address path), **00529330 341/348** (7 = int3 padding), **00550b70 77/77**.
- **Product ride** (startdiag harness, live-tree product build `builds/ridepath/api/startdiag`, `build_full.sh`;
  `RX_FULL_FRAME=1 ... --frames 2400 --holdw`, logs `builds/ridepath/full12_2400.log` / `full4_2400.log`): the old stops are gone.
  12 riders: 455 racing frames, local speed 0.118 -> 47.673 / gear 1 at race+150 (oracle W-held 50.77 / 1), 58.501 / 2 at +179
  (61.8 / 2), 65.4 / 2 at +200, 74.1 / 3 at +250, 82.2 / 4 at +350, max 87.71, 428 units, `V+0x280` 0 every frame (no wreck). 4 riders:
  470 racing frames, max 87.43, gear 4, 454.6 units. All 12 riders ride (AI velocities 33-67 at the last frame; AIData now loads,
  `api/rx_session.c:298 rx_aidata_load`), and **both runs now stop at unported 005278e0** (BehaviorNavigateTrack mode 1 - the racing
  line, 1760 B; its helpers 00528bd0 384 B, and mode 2 = 00527fc0 2426 B with 00528a70 348 B). That is the next ride-path unit (the
  same address product_chain.md's "next stops" list has); the local rider never reaches it, only the AI.
- Acceptance of this unit (final CLI of the tree, `builds/ridepath/cli/reflex_boot`): `verify.sh ... rx_race quick` **48 ports, 0
  mismatches, On identical** (`builds/ridepath/verify_race_final.log`); `rx_loop quick` **11 ports, 0 mismatches, On identical**
  (`verify_loop_final.log`) - the previously documented 0055f700 RiderCharacter pair does not appear with this CLI (another unit's
  change), the earlier run of this task did show it and nothing here touches it; `core/build.sh` clean
  (`builds/ridepath/core_build2.log`, EXIT 0).

## riderchar (2026-10-20): the Pre frame of the race activity stopped the intro cast every frame

**The `0055f700` / `0084ed20` two-word `RiderCharacter` residue is fixed** (`+0x70` playing and `+0xa2`, the third byte of the
`+0xa0` dword, visible). It was not a missing start: the riders' characters are built and set up during the boot's event start
(`0053eab0` -> `00624750`; the cast's `0055e360` through `0055de70` / `0074be10` sets `+0x70 = 1`, `+0xa2 = 1` and loads the
`"anim"` asset), and the world image the five reference worlds start from already has them playing (Concord: `1` / `0x170101ff` at
call 1), so the core **cleared** them every frame while the intro ran.

- **Root cause**: `005681e0 MXActivityRaceStub::vf25` (the Pre frame) called `0074b4f0` (`ScenarioCast_StopAll` -> `entity_stop`:
  `+0x70 = 0`, `+0xa2 = 0`) **outside** the `intro == 0` branch. The original has it inside (disassembly
  `00568220 jne 0056827d`: the `call 0074b4f0` at **00568278** is skipped while an intro runs; the same block holds the camera
  reset 0074b480, the `+0x203e0` hide and 0074b450). So the core stopped the 12 intro riders (`RiderCharacter`, vtable 0x0095881c)
  every Pre frame, while the original leaves them playing: the diff was `+0x70` `0` vs `1` and `+0xa0` `0x170001ff` vs
  `0x170101ff` on every call for the ~775 intro frames (Concord 776 / ai11 784 / SX 916 / Terracross 1495 mismatching
  `Engine::Update` calls before the fix).
- **Evidence**: a temporary core-write watch (`rx_wr32/rx_wr8` hook in `core/oracle/oracle.cpp`, removed) on `0x1842cd90` named
  `rx_scenario_cast_stop_all + 68` <- `rx_race_stub_vf25 + 472`; the disassembly of `005681e0` (above); `REFLEX_COVERAGE` of the
  original world run: `0074b4f0` / `0074b450` execute **4** times in 1800 frames, not 1800, and no instruction writes `+0x70` in the
  original's frames (the watch on the reference run logs 0 writes).
- **Fix** (`core/game/race_activity_stub.c`, `rx_race_stub_vf25`): the `rx_scenario_cast_stop_all` call moved into the `intro == 0`
  block, at the original's order after `rx_scenario_anims_stop_actors` (00568268 / 00568278). One line + the comment; no guard.
- **Acceptance** (build dir `builds/riderchar`, CLI `builds/riderchar/cli`):
  - `verify.sh rx_race` five worlds: `rx_ActivityManager_Update 0055f700` **1800/1800/0** (Concord) and 0 mismatches on all five
    (50 / 48 / 42 / 48 / 50 ports called), **On identical in all five** (state hash `a936214c...` Concord,
    `fd1d4b45...`, `1ef28933...`, `fe8060ee...`, `0f1c6fa1...`).
  - `verify.sh rx_loop` five worlds: the residual differences are exactly the documented ones - Concord 1 word `[152414cc]`
    (animation-worker IK w lane), ai11 Verify 0 + On one ConVar page (known-differences 2), FreeRide 0 / On identical, SX 1 word
    `[1524dd5c]`, Terracross 970 mud `+0x6c` words (known-differences 1). The RiderCharacter words are gone from all five.
  - `verify.sh rx_anim` (54/52/32/54/54 ports) and `rx_world` (170/168/134/146/178 ports): 0 mismatches, On identical, five worlds.
  - Baton boot (`loading_accept.sh <cli> rx_build11`, VERIFY=1, `builds/riderchar/la`): 98 ports, the 6 documented dead-stack
    mismatches (`00564b20` / `0055ff70` / `00767200` in two registry spellings), Verify-boot world identical, canonical world
    11 differing nodes / 99.62% (known-differences 6, same page list as before), race_compare 52/52 identical.
  - Product: `rx_api_test` (RX_FULL_FRAME=1, 120 frames) runs to `destroy`; `core/build.sh` clean.
- Disk files: `builds/riderchar/logs/{verify_rx_race,verify_rx_loop,verify_rx_anim,verify_rx_world,la_build11,api_run,core_build}.log`.

## aimonitor (2026-10-20, agent "aimonitor"): the AI behavior stops at race+463 and the next one - MonitorCorner states 1 / 2, then HandleBraking modes 1 / 2

The 12-rider first ride (builds/firstride, W+A/D, `RX_FULL_FRAME=1`) stopped at `00526ab0`
(`BehaviorMonitorCorner` state 1) at race+463. The five reference worlds never update the AI drivers (the activity stays at the
grid), so these functions are verified by the ride-path probe (group `c` / `b` in `oracle/race_ridepath_probe.cpp`), not by the
worlds.

**Ports** (game files `race_beh1_small.c`, `race_beh1_brake.c`; adapters `oracle/race_beh1.cpp`, group rx_race):

- `00526ab0` `rx_behavior_monitor_corner_wait` (state 1): stay in state 1 while the driver has not passed the corner's entry node
  (`b + 0x14 != driver + 0x18`) **and** a corner is still in reach of the driver's next node (`00526b40` with the tuning distance
  `0x9e44d8`); otherwise state 2. Nothing else is written (it does not steer: NavigateTrack does).
- `00526b00` `rx_behavior_monitor_corner_hold` (state 2): while the driver's current node (`driver + 0x18`) carries the vehicle
  class's corner flag `0x200` (node `+ 0x68 + 4 * (vehicle +0x198 + 4 * vehicle +0x194)`) the behaviour waits; at the first plain
  node it tail-jumps its own vf6 (`00526950`, `rx_monitor_corner_vf6`: clears `+0x0c .. +0x20`, i.e. back to state 0).
- `00524a40` `rx_behavior_braking_cast_center` / `00524af0` `rx_behavior_braking_cast_past`: the cast cycles of HandleBraking
  modes 1 / 2 (the next stop after the monitor corner, race+479). They repeat 005249a0's record block (two casts, the mean fraction
  when both recorded sides match, else 1.01 / side 0) but the recorded side becomes **0** when the mode casts at the centre line:
  mode 1 when `[b+0x4c]+0x1c == [b+0x50]+0xc` (the mode object's side is the edge side being cast); mode 2 when `00526dd0` says the
  vehicle is past `[b+0x4c]+0x1c` of the mode object's driver. The tail differs from 005249a0: it applies the fraction when
  `b+0x44 != 0` **or** the recorded side is 0 (`|| ebx == 0`), i.e. it brakes for the centre line without a common edge.
- `00526dd0` `rx_behavior_node_side` (EAX = the mode object - driver at `+8`, stack = the side int, RET 4): `cross * side > 0`,
  `cross = (veh.x - node.x) * (dirZ * len) - (veh.z - node.z) * (dirX * len)` with the driver's current node's direction
  (`+0x50 / +0x58`) and length (`+0x4f0`), `comiss 0 / jbe` (unordered = 0). Also called by LapPacing when its `+0x4c` object's
  `+0x0c == 2`.

**Verification** (oracle CLI `builds/aimonitor/cli`, `REFLEX_PORT=rx_race,rx_RidePathProbeHook REFLEX_PORT_VERIFY=1
RX_RIDE_PROBE=cb`, world National_NAT_Concord_ai11 frame 700, logs `builds/aimonitor/logs/probe_cb_final.log`): per call
`00526ab0 3/3/0`, `00526b00 2/2/0`, `00524a40 5/5/0`, `00524af0 5/5/0`, `00526dd0 3/3/0`. Original instruction coverage under the
probe (`builds/aimonitor/logs/probe_cb_cov.log`, `REFLEX_COVERAGE`, `builds/ridepath/covrange.py`): 20/20, 15/15, 60/60, 64/64,
32/32 executed (only the int3 padding outside the ranges is never). The probe builds the corner state on the live monitor behaviour
(the driver's nodes, the 0x200 class flag, the corner entry) and the cast state on the live braking behaviour (the `+0x4c` mode
object's `+0x0c` / `+0x1c`, the `+0x50` edge side, the cast count / recorded sides / fraction / edge), and calls both branches of
each decision.

**Product** (live-tree dylib `builds/aimonitor/prod/librxcore.dylib` and, re-run on the final clean `core/build/librxcore.dylib`,
sha256 `97095820b18b5a5657c1ff3520ace5b5e39bd3b0b1c7016a186c49c9163c2266`; `builds/firstride/run.sh --lib ... --frames 2400`,
logs `builds/firstride/logs/aim3.*`): the steer pass ran **557 racing frames** (no-steer 559) with the same state hashes as the prod
dylib (`df926944b7354bf2` / `ab8b8587ddcc405a`), 31 of 33 checks pass, the steering A/B 6/6. The progress after each port: 463 ->
monitor corner -> **480 / 483** racing frames (next stop `00524a40` at race+479 / 482) -> brake modes -> **557 / 559** (game time
27.783 / 27.816 s). The next stop is **`0055e8b0`**, a
**different unit**: the activity's "VehicleEntered" handler (`build_activity_start.c:254`, subscribed on the vehicle channel
`0x00a89bc8` at `00587180`): a dynamic cast of the vehicle (import `0x916234`, type descriptor `0x9419a8`), a search of the
activity's drivers (`+0x2d0 + 4 i`, driver `+0x74` player `+0x1ac` vehicle), the `activity +0x23c == 5` / human (`+4`) test, then
`0055e100` or the activity's vtable `+0xec`. Belongs to the activity (race_activity.c / the "VehicleEntered" subscription), not to
the AI.
The second failing check is `camera` (distance 6.78..752.97 over 1644 rc-0 frames): the intro / flyby camera is now exported during
the countdown (752.97 at race -1) and exceeds the check's 30-unit bound. It appears with the camera unit's edit made between my two
product builds (`loop_cam_player.c` 21:33), not with these ports (the brake modes only run from race start on); `rx_session_camera`
itself is unchanged.

**No regression** (`builds/aimonitor/cli/reflex_boot`): `verify.sh rx_race quick` 48 ports, `rx_loop quick` 15, `rx_world quick`
132: 0 mismatches and On identical in all three (state hash `1f6611fa80b80831`, memory hash `1487a52f208b4b26`); logs
`builds/aimonitor/logs/verify_rx_{race,loop,world}_final.log` (final clean CLI).

## aibrake (2026-10-20, agent "aibrake"): the mode dispatch verified, and the 4-rider clean ride's next stop

Independent second verification of the HandleBraking work above (the ports were already in the live tree when this unit
started; they are `race_beh1_brake.c` 00524a40 / 00524af0, `race_beh1_small.c` 00526dd0, adapters `oracle/race_beh1.cpp`)
from a second build (`builds/aibrake/cli`, `builds/aibrake/current`) and a check of the **vf4 switch itself**:

- `oracle/aibrake_probe.cpp` (new; group `rx_AIBrakeProbeHook`, frame hook 0084ed20,
  `REFLEX_PORT=rx_race,rx_AIBrakeProbeHook REFLEX_PORT_VERIFY=1 RX_AIBRAKE_PROBE=1 RX_AIBRAKE_PROBE_FRAME=650`) calls
  **00524900** on a live BehaviorHandleBraking with the state each branch reads (enabled +4, speed +0x150 40 / 10, the +0x48
  pause, the +0x4c mode +0xc 0/1/2 +0x1c side, the +0x50 edge side, the cast count / records / braking edge, driver +0x64):
  **8/8/0** (disabled, mode 0, mode 1 edge, mode 1 centre, mode 2 edge, mode 2 past -- the case asks 00526dd0 first --, slow,
  paused); builds/aibrake/logs/probe_a3.log. Instruction coverage of the original 00524900 under it: **51/51**
  (`builds/aibrake/cov_a3.bin`).
- Per call again with the ride-path probe group `b` (`RX_RIDE_PROBE=b`, same world/frame): **00524a40 5/5/0, 00524af0 5/5/0,
  00526dd0 3/3/0** (logs/probe_b1.log); instruction coverage of the originals (`builds/ainav/covrange.py`, cov_b.bin):
  **00524a40 60/60, 00524af0 64/64, 00526dd0 32/33** (the one never = int3 padding).
- **4-rider clean product ride**, two builds of the live tree (`RX_FULL_FRAME=1 --frames 5400 --riders 4 --ai 95`):
  - `builds/aibrake/current` (tree of 21:38, log `ride4_clean.tsv`): **530 racing rows, last completed race frame 528**
    (was 477), 1791 steps / session frame 1800, max speed 82.77, gear 4, 517 units of path; the next stop was `00844050`
    at the attempt of race frame 529. All 1740 rows this log shares with collisionpair's `ride4_clean5400b.tsv` are
    **identical outside the camera columns** (fields 21-25: this tree's camera getter returns a camera during the countdown
    where the 21:26 build returned none / camrc 2 - another unit's change), so the switch changes nothing before the old stop.
  - `builds/aibrake/current3` (the 21:58 tree, log `ride4_clean3.tsv`): **570 racing rows, last completed race frame 568**,
    1823 steps / session frame 1832, max speed 82.64, gear 4, 557 units. **Next stop `0055e8b0`** at the attempt of race
    frame 569: the activity's "VehicleEntered" handler (the driver search / `activity +0x23c == 5` / the vtable +0xec
    dispatch), i.e. the activity unit's (race_activity.c) - exactly aimonitor's next stop, not an AI behaviour.
- `00844050`'s port already existed (`phys_coll_shape.c` `phys_dyncyl_scale`); the 21:38 abort was the call-site path, and the
  typed float call (`world_util.h` `world_fn_float1` lists 00844050, `world_wreck.c:412` `WORLD_CTX_F`) landed at 21:51 in
  the collision-shape unit: the 21:58 build passes it. Rows of the two rides differ beyond the camera once the tree's other
  ports (21:44-21:58) change the field.
- Acceptance: `rx_race quick` 48 ports / `rx_loop quick` 15 / `rx_world quick` 132, **0 mismatches and On identical**
  (logs `builds/aibrake/logs/verify_{race,loop,world}_quick.log`; state hash `1f6611fa80b80831`, memory
  `1487a52f208b4b26`); `core/build.sh` clean (`builds/aibrake/logs/core_build.log`, EXIT 0).

## ainav (2026-10-20, agent "ainav"): BehaviorNavigateTrack modes 1 / 2 (005278e0 / 00527fc0) and their offset helpers (00528bd0 / 00528a70)

The ride-path follow-up: both the 12- and 4-rider product rides passed the offtrack/reset ports and stopped in the AI's racing-line
behaviour mode 1. The `+0x34` object is the key to both modes: `BehaviorNavigateTrack::vf3` (00527280) sets it to
`rx_driver_behavior_12(driver)` = the driver's **BehaviorMonitorCorner** (vf5 id 0x12; 0053b960 finds it by vf5). So the navigate
mode is the monitor's `+0x0c` state: **1** = a corner was found (00526a30), **2** = watching it (00526b00); its `+0x14` is the
corner's first node, `+0x18` the last, `+0x1c` the turn direction (+-1; 005380f0 only ever returns +-1), `+0x20` the run length
(00526bf0), `+0x10` the driver's section. Both modes are the monitor's corner line; mode 0 (00527360) is the free racing line.

Files: `core/game/race_beh1_nav.c` (4 ports), `core/game/race_beh1.h`, `core/game/race_beh1_small.c` (the vf4 dispatch of
005278e0 / 00527fc0 now direct calls at the original's pushed-argument depth), `core/oracle/race_beh1.cpp` (4 adapters, group
`rx_race`), `core/oracle/race_ridepath_probe.cpp` (probe group letter `n`). Build dir `builds/ainav` (oracle CLI `cli`, private
probe CLI `cli_p`, product `api`; backups `backup/`).

- **005278e0 mode 1** (stack behaviour; RET 4; frame `push ebp; and esp,-16; sub esp,0x64; push ebx / esi / edi`, and the same
  0x64-byte frame as 00527360): the rider's flat heading / position (normalized hx/hz at F+0x20/0x24, position F+0x28/0x2c) and the
  side pick **inlined** (00528d50's body *without* the airborne gate: vf43 is called and its result discarded; the forced +0x310,
  the take-line-ready +0x2c, the +0x314 lane and the 00509e30 across fraction with the 0x9e4238 tuning); the **3D look-ahead**
  (`0050a650`, tuning 0x9e35e8) along the corner's first node when the racing-line point (monitor +0x14) is nearer than 0xa04958
  to the vehicle's projection on the driver's segment (+0x18 -> +0x1c); the nearest node (start {driver +0x14, corner node},
  `005381f0`, budget 0x9c4750); the node direction's sideways vector (-dirz, dirx); target = nearest + sideways * offset where the
  offset is **00528bd0** (the corner node's class flag 0x400 clear) or **00528940** (set); then the +0x28 hard-turn state machine:
  state -1 / 1 writes the driver's +0x60 and clears the state when dot > -0.35 (0x9cd9b4); state 0 with dot < -0.75 (0x9cd9b0)
  hard-turns (+-1 by the across fraction > 0.5, 0x9322cc) and otherwise adds the pow-difference push (0xa049b8 row, base 1.0
  through the CRT pow) rotated into the vehicle frame (005084f0) times the heading factor (0050b150). The +0x40 random feeds the
  offset; +0x30/+0x38/+0x40/+0x48 carry the node position / sideways vector as the original does.
- **00528bd0** (EAX = monitor +0x1c, EDX = node, ESI = behaviour; stack side, random; RET 8) -> xmm0: the corner line offset. The
  edge of the node the corner turns away from (+0x10 for the -1 side, +0x20 for +1; the side the behaviour already has takes it),
  distance * (tuning 0x9e3f98 when the sides agree, 0x9e42f8 otherwise) +- the random times the ability factor (player +0x8f4 <= 3;
  +0x8f8 0 / 1 / 2 -> 0x9322c8 / 0x935860 / 0x9c4b4c), signed by the edge. Unreachable else: with a direction that is neither -1
  nor 1 the original reads its own uninitialised frame slot [esp+0xc] as the tuning value, the edge pointer and the sign; the port
  uses 0 (the monitor's direction is only +-1) and the path is the documented dead one.
- **00527fc0 mode 2** (stack behaviour; RET 4; frame `sub esp,0x84`, 3 pushes): the full 00528d50 side pick (airborne gate:
  with the bike in the air (vf43 word +0x8d98) and no +0x38 linked behaviour / [+0x38+0x4c] it skips the lateral part;
  `rx_behavior_navigate_pick_side(b, 0, 0)` is that exact body); the **side lock +0x10**: refreshed from +0xc when it is still 0,
  when the speed is below 0x9cd9b8 or when the monitor's remaining run fraction (1 - 00526c60 / monitor +0x20) is above 0x935aa8
  (the lock keeps the side near the apex at speed; the original's `jbe` order leaves it unset otherwise); the 3D look-ahead from
  the driver's next node (tuning 0x9e4538); the nearest node from {driver +0x14, driver +0x1c} at that point. **Corner node**
  (`+0x200` class flag): the node's own direction's sideways vector, the offset from **00528bd0** when the *driver's previous*
  node's 0x400 flag is clear (EAX = monitor +0x1c, side +0x10, random +0x44), else **00528940**; target = node + sideways * offset.
  **Plain node**: the flat look-ahead (`0050a5f0`, tuning 0x9e45f8) and, when the previous node's 0x400 flag is set, that point is
  the target; otherwise a second nearest-node query and **00528a70** (EAX = the picked side +0xc, side = monitor +0x1c, random
  +0x48), target = look point + sideways * offset. Then the same +0x28 hard-turn machine; after the steering push a second dot
  (the heading against the *absolute* target direction) sets the driver's +0x68: +-1 when |dot| < 0x931da8 (0.6), else
  |dot| * 0x977af8 (steer > 0) / 0x977af4 (steer <= 0).
- **00528a70** (same convention as 00528bd0) -> xmm0: mode 2's offset: one table for both sides (0x9e3288), the random times
  0x9322c8 / 0x92ff84 / 0x935860 for the ability classes 0 / 1 / 2, and the edge chosen by the EAX side (the call sites pass the
  behaviour side in EAX and the monitor direction on the stack in the corner branch, the reverse in the non-corner branch). The
  same unreachable else and the same float order (sqrt(dx^2 + dz^2 + dy^2) * sign * value).
- **Dead paths** (documented, not guessed): the helpers' `cur` / `side` not being +-1 (the monitor direction is +-1, 005380f0)
  and 00528940's side not being +-1 (the original reads a zeroed local there, so its callers must pass +-1); 00527fc0's inlined
  pick has no side==0 path (the +0x10 lock is +-1 when it reaches the offset). Every other instruction of the four originals runs
  under the probe.
- **Per-call Verify** (probe `rx_RidePathProbeHook`, group `n`; `builds/ainav/rp.sh National_NAT_Concord_ai11 700 2
  REFLEX_PORT=rx_race,rx_RidePathProbeHook REFLEX_PORT_VERIFY=1 RX_RIDE_PROBE=onr RX_RIDE_PROBE_FRAME=650`, log
  `builds/ainav/probe_onr.log`): **005278e0 18/18/0, 00527fc0 43/43/0, 00528bd0 7/7/0, 00528a70 7/7/0** (and the line offset
  00528940 3/3/0 for its side-switch branches). Same counts with 0 mismatches on SX_Springfield_ai11 (`probe_n_sx.log`). The probe
  builds the monitor corner on the live behaviour (state 1 / 2, the driver's nodes, +0x1c +-1, length 40, the 3 randoms, the side,
  the driver's forced side +0x310, the take-line +0x2c, the previous node's 0x400 and the run nodes' 0x200 flags, the airborne
  word, the hard-turn edges, the speed below / above 0x9cd9b8) and calls the originals on those objects.
- **Instruction coverage** of the originals under the probe (`REFLEX_COVERAGE`, `builds/ainav/covrange.py` on `cov_n9.bin`):
  **005278e0 441/441**, **00527fc0** all of its own instructions (the range 0x527fc0..0x528d50 shows only the helpers' dead else
  and the int3 padding): 528bd0 100/103 (3 dead), 528a70 92/99 (3 dead + 4 int3), 00528940 279/295 (2 dead + 14 int3).
- **Product ride** (live tree, `RX_FULL_FRAME=1`, `builds/ainav/api/ride` = `builds/inputrace2/ride.c` linked by
  `builds/ainav/build_product.sh`; W held, 12 riders, 1/60 s steps): the old stop is gone. The tree moved while this ran (other
  units' edits), so the measured runs are:
  - 21:41 tree (riderchar's `008435f0` wrapper switch in `anim_extern.h`): **526 racing frames** (frames 1108..1633), max 87.71 /
    gear 4 / 428 units, stop **00844050** (CollisionShapeDynamicCylinder::vf4 — the composite scale the grid-report reaches; the
    world dispatch unit's, listed as unported in `world_util.h`).
  - with that cylinder pair supplied by the **private exact ports** `builds/ainav/patched/phys_cyl_ports.c` (not in core/game;
    verified per call through `patched/phys_cyl_probe.cpp`, `rx_PhysCylProbe`, 16/16/0 with NaN / inf / -0 scales): **555 racing
    frames** (frames 1108..1662, race time 9.234 s), local max **87.7143** (word 42af6dbd, frame 1520), **gear 4**, path **561.7**
    units, place 3/12, no wreck, all 12 riders alive with velocities 30..72 and positions spread from (D=0) to z +152
    (`builds/ainav/sd12_w.log`: slot 0 473.9 151.0 148.5; 1 502.2 147.3 56.5; 2 495.6 146.6 106.1; 3 490.7 144.5 48.5; 4 505.5 124.1
    -35.6; 5 529.0 114.7 -103.9; 6 427.7 139.3 152.9; 7 556.6 114.0 -122.5; 8 558.9 114.0 -133.8; 9 475.1 143.3 111.1;
    10 435.7 138.3 142.2; 11 468.9 143.6 135.3), laps 0 (only ~9 s of race time). Next stop **0055e8b0** (the activity's
    "VehicleEntered" handler; aimonitor's same stop; activity/scenario unit).
  - 4 riders, same private build: **564 racing frames** (race time 10.0 s), max 87.43 / gear 4 / path 607.0, place 4/4, stop
    0055e8b0.
  - The current tree (21:5x, after further edits): the W-held ride stops earlier in **0076ee60** (the static-contact impulse,
    `rx_world_kinematic_collide_static`, 337 B; world-phys unit, listed in `product_chain.md`'s next stops) at 254 racing frames —
    another unit's gap, not the AI line.
- **Local vs the oracle** (same W-held input, `builds/inputrace/logs/oracle_hw_input.txt`, 331 racing frames): the input block is
  identical (2648/2648 words, inputrace2); the product's gear shifts lag 8..10 frames (143/168/204 vs 135/159/194 — the known grid
  slot 7 vs 5, 10 units) and by +250 the product is at 74.15 / gear 3 vs the oracle's 73.60 / gear 3 (the AI now load and interact,
  so the lines part later than the old AIData-zero runs). The AI have no oracle counterpart: the reference worlds never update the
  drivers (0 original instructions in the driver range); the probe is their verification.
- **No regression**: `verify.sh <cli> rx_race quick` 48 ports / 0 / On identical; `rx_loop quick` 14 / 0 / identical;
  `rx_world quick` 132 / 0 / identical (`builds/ainav/verify_rx_*_quick.log`); `core/build.sh` clean
  (`builds/ainav/core_build.log`).
- **For the lead / owners**: (1) `00844050` / `00844080` (CollisionShapeDynamicCylinder vf4 / vf5, 47 + 13 B) are the next
  collision-shape scale pair the composite scale calls; the private ports in `builds/ainav/patched/` are the exact bodies if the
  world dispatch unit wants them. (2) `0076ee60` (static contact impulse) and `0055e8b0` + `0055e100` (VehicleEntered ->
  StartLine/HoleShot script) are the next ride stops on the current tree (world-phys and activity/scenario units). (3) The ride
  measurement uses `builds/inputrace2/ride.c` and the private cylinder ports; the live tree alone cannot pass 00844050.

## stuntrace (2026-10-20, agent "stuntrace"): the race's three stunt handlers, and what the 12-rider ride does now

The preserved 12-rider reproducer that stopped at `00592b60` (QUEUE "### stuntaudio") now passes that handler and stops at the
next listener of the same event: **00565710**, MXActivityRace's "PhysicsStuntBegin". It is ported, with its two siblings on the
same stunts channel (0x00a94304), which the same subscription block registers.

### What they are (disassembly 00565710 .. 00565820)

`rx_race_vf2` (00564b20, `build_activity_start.c`) subscribes three handlers of the activity on the stunts channel:

| address | name | subscription | shape |
|---|---|---|---|
| 00565770 | "StuntBegin" | 00588130 | ECX = activity, {player, descriptor}, RET 8 |
| 005657d0 | "StuntFailed" | 00588130 | ECX = activity, {player, descriptor}, RET 8 |
| 00565710 | "PhysicsStuntBegin" | 005881f0 | ECX = activity, {player, descriptor, type}, RET 0xc |

All three are 20 instructions, no prologue, no callee: `eax = [esp+4]` (the player); if `player + 0x10 != 0` (AI) return.
Otherwise (a local human):

- **00565710 / 00565770**: `eax = [activity + 0xff07c]` (the level the music fade builds from, 100;
  `rx_race_stub_construct` writes it) -> `activity + 0xff080 = eax`; `0x009c3edc = eax` unless the activity's audio logic
  (`+0x234`) has `+0x22148` clear (the flag the original computes with `neg edx; sbb edx,edx; and edx,eax`), then
  `activity + 0xff078 = 4.0f` (0x9322c0).
- **005657d0**: `activity + 0xff080 = 5`; `0x009c3edc = 5` (or 0 when muted); `activity + 0xff078 = 0.0f` (xorps).

`+0xff078` is the fade timer `MXActivityRace::vf28` 00565270 reads every InProgress frame
(`level = (I32(+0xff07c) - 5) * (timer * 0.25) + 5`, timer -= dt), so a stunt starts the full-level fade and a failed one drops
to 5 at once. **Gameplay, not presentation**: `0x009c3edc` is read by the billboard crowd's cheer chance in `loop_entity.c`
(`rx_billboard_crowd_update` 006c6650: `roll < 500 - 0x009c3edc`), and `+0xff078` / `+0xff080` are read by the race update, so
the handlers are ported rather than skipped.

### Ports / registration

- `core/game/race_stunt.[ch]`: `rx_race_physics_stunt_begin` 00565710, `rx_race_stunt_begin` 00565770,
  `rx_race_stunt_failed` 005657d0 (the shared level helper is `stunt_music_level`).
- `core/oracle/race_stunt.cpp`: three `RX_PORT`s in group `rx_race` (0 outputs: the handlers write memory only;
  `m.ret(0xc)` / `m.ret(8)`).
- Registration site: unchanged. `rx_race_vf2` already passes the original addresses to `act_subscribe`, and the proxy
  (`rx_event_proxy_call3` 0058f270 -> `rx_call_original([proxy + 0x10])`) resolves them; the literal `RX_FUNCTION(0x...)`
  lines with integer-only signatures (and, since the fnsig unit, every macro-generated registration) put them in the product's
  dispatch table (`tools/gen_fnsig.py`), exactly like stuntaudio's vcall thunks. No call site change is possible or needed (the
  target is a member-function pointer).
- `core/oracle/race_stunt_probe.cpp` (frame hook 0084ed20, group `thunk`, port name `rx_StuntHandlerProbeHook`,
  `RX_STUNTPROBE=1 RX_STUNTPROBE_FRAME=<n>`): calls the original handlers at one frame on the live event activity with
  {live audio logic as is, `+0x22148` set, `+0x22148` clear, no audio logic (`+0x234` = 0), AI early return}. The live worlds have
  no AI player slot (`ai11` finds none either), so the AI branch uses a zeroed fixture block from the game allocator (0078fbe0)
  with `+0x10 = 1`; the +0x22148 / +0x234 pokes are restored after each call.

### Verification

- Per-call Verify (`REFLEX_PORT=rx_race,rx_StuntHandlerProbeHook REFLEX_PORT_VERIFY=1`): Concord frame 900 and
  Concord_ai11 frame 1000, **00565710 5/5/0, 00565770 5/5/0, 005657d0 5/5/0** each (unmuted, muted, no audio logic, live
  audio logic, AI return). Instruction coverage under the probe (`REFLEX_COVERAGE` + `builds/ridepath/covrange.py`):
  **20/20, 20/20, 20/20** (every instruction of all three originals; both arms of each branch).
- Preserved reproducer (the 19:10 `/builds/inputrace2/current` object set, which stops at 00565710 after **477 racing rows /
  1575 steps**, state hash `1b1fa79bcbbdcb74`, log `builds/stuntaudio/oldfix12.tsv`): relinked with only
  `race_stunt.c.o` and a dispatch table that adds the three addresses (`builds/stuntrace/repro/build.sh`) it logs the **first
  1575 rows byte for byte** and continues to **482 racing rows / 1580 steps**, next stop **005278e0** (BehaviorNavigateTrack
  mode 1 - unported in that old object set; ported by ainav in the live tree). Loaded from `builds/stuntrace/repro12.tsv`.
- Live tree (`builds/inputrace2/build_current.sh builds/stuntrace/current`, `RX_FULL_FRAME=1`, 12 riders, AI 95, 2200 frames):
  **281 racing rows, step 1398, stop 0086f2a0** (the narrow phase pair cylinder `0x0008` vs height-field terrain `0x0200`,
  `phys_contact.c` k_pairs; one of the 11 unported pair cases listed by collisionpair). A link of the same objects **without**
  `race_stunt.c.o` and without the three table entries produces an
  identical log (state hash `d32b65568cfa8e29`), so the stop is another unit's (the tree's route moved after 21:58: ainav saw
  0076ee60 at 254 rows). The old 556-row stop **0055e8b0** ("VehicleEntered") is now ported in the live tree
  (`rx_activity_vehicle_entered`, and the current `fnsig` table carries 0x0055e8b0), but this run does not reach it.
- Regression (final CLI `builds/stuntrace/cli`, logs `builds/stuntrace/verify_*_quick.log`): `rx_race quick` 48 ports / 0
  mismatches / On identical; `rx_loop quick` and `rx_world quick` as reported in QUEUE "### stuntrace"; `core/build.sh` clean.

### Integration note

The oracle-only lifted port `native/game/ports/rest_5.cpp` registers 00565710 and 005657d0 (group `rest`). In the CLI the core
objects are linked after the lifted ones, so the core adapters win; a `REFLEX_PORT=all` integration run should exclude or
align that file (the same class as the 00547f30 / 00696480 duplicates).

## finishui (2026-10-21, agent "finishui"): the product finish transition (state 3 -> 4) and the records it writes

Task: the product entered WindDown (state 3), ticked the 3 s, sorted the standings and requested the "WinddownEnd" page wipe,
but stayed in state 3 because the PageWipeTransition handler was never subscribed (no UI manager). Make the game's own transition
run in the product without faking results. Build dir `builds/finishui/` (oracle CLI `builds/finishui/cli`, product
`builds/finishui/api`, logs `builds/finishui/logs/`, backups `builds/finishui/backup/`).

### What changed

- **`core/game/race_finish.c` `rx_show_page_wipe` (0055e990), product only.** The product has no UI manager ([0xa93fcc] = 0), so
  it cannot raise "ShowPageWipe" on the manager's channel. For the finish page (`name == "WinddownEnd"`) it now runs the game's
  own page-wipe callback `rx_page_wipe_transition` (0055e9e0) after writing +0xff06c = 1: the callback is what the UI raises when
  the wipe ends, it is the only consumer of the wipe, and nothing gameplay reads the animation. The callback sees state 3 and
  calls `SetState(4)` (Activity::vf34 exit + vf41 enter), exactly the original transition. Other page wipes (the intro fade,
  name 0x0095b7c4) are unchanged.
- **`core/game/race_finish.c` `rx_race_update_post` (00569a90), product only.** Post sub-state 0 would start the finish scenario
  (RaceFinishScenario vtable 0x009433d4: 0074a1e0 / 0074a340 / 0074a110 / 0074a540 and helpers), which is presentation (podium
  riders, model flags, crew-chief animations, replay playback) and not ported. The product reports it as "not started" (the
  original's own fallback: `rx_post_state(act, 2)`, the result flags). The records / standings were written by the Post enter
  (00568da0) **before** this update, so nothing gameplay reads is skipped. The oracle still starts the original scenario.
- **`core/game/phys_coll_shape.c` + `core/oracle/phys_coll.cpp`**: `phys_dyncyl_scale` 00844050 and `phys_dyncyl_restore`
  00844080 (CollisionShapeDynamicCylinder::vf4 / vf5), the next stop of the 1-rider product load (`world_wreck`'s shape scale
  dispatch); `world_util.h`'s `world_fn_float1` now types 00844050 as well. Five stores / two x87 copies; the restore quiets a
  signalling NaN like `fld / fstp`.
- The 00835170 and 0055e8b0 call sites became direct core calls in the live tree while this task ran (concurrent edit); they are
  not this unit's changes.

### Oracle evidence (the game's own finish, natural)

- **12 riders, 3 laps, Concord (the raceend world `builds/framecmp/worlds/pc1`), W held, no REFLEX_DRIVE**, 33,000 frames
  (`builds/finishui/oracle_pc1w/`). The local human finishes last at 502.268 s; the activity enters **state 4** (Post) at race
  frame ~31440 with 12 finishers. The probe on RaceFinishScenario::vf0 (frame 0 of the scenario, `REFLEX_FINISHUI_PROBE=1`) prints
  the state and the records **after** the Post enter:
  `TotalEvents=1 TotalLaps=3 TotalPlacing=12 TotalScore=71 TotalTime=502.284027 BestLapTime=189.399017 BestPlace=12 BestScore=71`
  for records slot 0. The world then stops in the original *presentation* scenario (0074784d, unmapped read) - the finish
  transition and the records are already done before it.
- **1 rider, 1 lap, Concord** (a world booted with `REFLEX_EVENT_SPEC=National,10,NAT_Concord,50,0,2,Laps,1,0 REFLEX_RIDERS=1`,
  `builds/finishui/world1lap/c`), W held, 20,000 frames (`builds/finishui/oracle_1lap/`): **state 4** at 165.733 s, one finisher,
  records slot 0 `TotalEvents=1 TotalLaps=1 TotalPlacing=1 TotalScore=82 TotalTime=162.550003 BestLapTime=FLT_MAX BestPlace=1
  BestScore=82`. (BestLapTime stays FLT_MAX because a 1-lap race's only lap time is never stored in driver +0x10c4 - the original's
  own value.) Same presentation-scenario stop after the dump.
- The oracle branch of `rx_show_page_wipe` / `rx_race_update_post` is unchanged; the long world's transition is the original's
  (the state-3 -> 4 transition and the records above).

### Product verification

The product's finish path is the code above; a product race cannot reach it yet in this tree:

- **1 rider, 1 lap, Concord** (`builds/finishui/api/finishrun`, `RX_FULL_FRAME=1`, W held, `builds/finishui/logs/finishrun_1lap_*.log`):
  the load and the countdown complete, the race starts at step 1200 (game 20.2 s), the local rides; at race 8 s (step ~1690) the
  run **hangs** in the activity's "Holeshot" event dispatch: `rx_race_holeshot` -> `rx_world_event_dispatch` ->
  `rx_world_registry_find` -> `rx_world_string_map_find` -> `rx_world_string_map_lower_bound`, looping on a 0 child of the
  channel's event map (the map lookup of a name with no handler). This is a pre-existing event-registry defect of the tree, not
  the finish path; a sample of the live process names every frame of the loop (`builds/finishui/logs/hang_sample.txt`).
- The Product's 12-rider routes stop earlier on their own unported behaviours (AI / physics pairs); no product route reaches a
  finish in this tree. The state-3 -> 4 -> records path is therefore verified against the original (above) and by the per-call
  ports, but **not yet observed in a product race**; once the ride reaches the finish, `rx_show_page_wipe` enters Post through the
  game's callback and `rx_race_stub_post_enter` writes the rows (the code path is the same one the oracle exercised).

### Regressions / verify

- New ports per-call: `rx_phys` probe group `d` (`RX_PHYSCOLL_PROBE=d`), 24 calls each: **00844050 24/24/0**, **00844080 24/24/0**
  (`builds/finishui/probe/out.txt`).
- `verify.sh <cli> rx_race quick`: 48 ports, 0 mismatches, On identical. `rx_loop quick`: 18 / 0 / identical. `rx_world quick`:
  132 / 0 / identical (`builds/finishui/logs/verify_*_quick.log`).
- `core/build.sh` clean (final run in the report).

### Boundaries / remaining

- The wipe animation (UI presentation) is absent; the product runs its callback immediately, so Post starts at the 3.0 s timer,
  not a few frames later after a wipe. Nothing gameplay reads the animation; +0xff06c is written / cleared by the same two
  functions.
- The RaceFinishScenario is a documented presentation boundary (file header comment).
- **Remaining:** the product ride must reach the finish: the tree's next blockers are the "Holeshot" event-map hang
  (`rx_world_string_map_lower_bound`) and the AI/physics stops on the 12-rider routes. When they land, the product state is
  expected to be 4 (Post), session FINISHED, records as above; state 5 (Inactive) needs the results page, which is UI.

## vehicleentered (2026-10-21, agent "vehicleentered"): the activity's "VehicleEntered" handler 0055e8b0 ported; the 12-rider ride passes 559 and runs to race 1011

The ride's stop after the AI behaviours was `0055e8b0`, the handler `00587180` subscribes to the vehicle channel
`0x00a89bc8` ("VehicleEntered"). The earlier reading (aimonitor's entry) called it a dynamic cast of the vehicle; the
disassembly shows otherwise: import `0x916234` is **`strncmp`**, and `0x9419a8` is the string **"SCR_"**. The payload the
proxy `00554fc0` passes is `{vehicle, trigger}` (the event's 2-dword record), and the handler is about *named triggers*.

**Function** (`core/game/race_activity.c`, `rx_activity_vehicle_entered`, registered `RX_FUNCTION(0x0055e8b0)`; declaration
in `race_activity.h`; adapter `oracle/race_activity.cpp` `RX_PORT(0x0055e8b0, Activity_VehicleEntered, "rx_race", 0)`):

- `strncmp(trigger + 0xc0, "SCR_", 4) != 0` -> **any other trigger**: the activity's vf59 (`00569f40`), which is the race's
  "HoleShot" / "FinalStretch" trigger logic (ported: `rx_race_trigger`).
- `"SCR_..."` -> a scripted-sequence trigger: search the activity's drivers (`+0x2d0 + 4 i`, driver `+0x74`, player's vehicle
  `+0x1ac == vehicle`) for the vehicle's driver; if `activity +0x23c == 5` (Inactive) and that driver is **not** local (`+4 == 0`),
  return; otherwise `0055e100(activity, trigger + 0xc0 + 4, 0)` - the sequence name is the trigger name **after** the 4-byte
  "SCR_" prefix (the original's `add eax, 4`; the `+0x14 .. skip "SCR_"` detail is what the first probe missed, see below).
- Frame: `push ecx; push ebx, ebp, esi, edi` (`RACE_FRAME_PLAIN(F, 4, 4)`); at both calls the original's ESP is `E0-0x14`
  (the strncmp arguments popped), so `race_thiscall(..., 2, ...)` / `RACE_CORE_CALL(3, ...)` from `F` give the original's
  callee entries `E0-0x20` / `E0-0x24`. EAX is dead (the proxy ignores it): no outputs declared.

**Verification** (`oracle/race_vehicleentered_probe.cpp`, new; frame hook `0084ed20`, group `thunk`; run
`REFLEX_PORT=rx_race,rx_VehicleEnteredProbeHook REFLEX_PORT_VERIFY=1 RX_VE_PROBE=1 RX_VE_FRAME=700` on
National_NAT_Concord, oracle CLI `builds/vehicleentered/cli`): **7 calls / 7 verified / 0 mismatches**
(`builds/vehicleentered/probe2.out`). The probe builds a fake trigger on a spare guest page (the handler reads only its name at
`+0xc0`) and calls the live human's activity / vehicle / driver: (1) "StartZone" -> vf59, neither name matches;
(2) "SCR_Probe" -> the sequence load fails; (3) "SCR_<the activity's first authored sequence>" (`SCMG_30SecGirl_FX`) -> the
match/restart branch of `0055e100` (returns 1); (4) kind 5 on a local driver -> the sequence starts anyway; (5) kind 5 with the
player's `+4` cleared -> the early return; (6) "HoleShot" -> the holeshot (activity `+0x13e644` becomes 1: `00569eb0` runs); (7)
"FinalStretch" -> the last-lap check. Call (3) is the one that discriminates the `+4`: with the name passed as
`trigger + 0xc0` the sequence does not match and the activity's `+0x15188 / +0x1518c` writes differ.
Note: the five reference worlds and the baton boot do not raise "VehicleEntered" (0 calls in all), so this probe is the per-call
verification, like the ride-path probes.

**Where it is registered / dispatched.** The subscription in `build_activity_start.c:254` (00587180 stores the member-function
pointer `0x0055e8b0` in the proxy) needed no change: the product dispatch `rx_delegate_invoke` -> `rx_call_original([proxy+0x10])`
now finds the registered port. Comment updated there. The ride's trigger is the **"HoleShot"** one; lldb on the product at the
stop shows `rx_activity_vehicle_entered -> rx_race_trigger -> rx_race_holeshot -> rx_world_event_dispatch` (the holeshot records,
the driver's `+0x7c`, the "Holeshot"/"HoleshotActivated" events, then vf53 `00569e10`, which starts "HoleShotScript" when the race
scripts are enabled). Those callees were already ported but never reached by a product ride before.

**Product** (`builds/firstride`, W+A/D, `RX_FULL_FRAME=1`, 2400 frames, live-tree dylibs): the ride now **passes 559** and the
old stop is gone. A/B on the same tree, with only the `RX_FUNCTION(0x0055e8b0)` registration removed
(`builds/vehicleentered/ab`, private copy): **567 racing frames**, stop `0055e8b0` at race 566. With the port
(`builds/vehicleentered/prod4`, tree of 00:5x): **1011 racing frames** (game 36.5 s), 30/33 checks pass, steering 530 frames
(L 265 / R 265), path 643.7, max speed 75.1, gear 3; the final clean `core/build/librxcore.dylib`
(sha256 `19d3454a...999de2`) reproduces it exactly (state hash `904adf620a6e558b`, 2179 steps); next stop
**`7859afc0` `_CIpow`** at race 1010 (CRT x87; the tree's
non-finite bike position/velocity from race 287 is the vehicle/collision unit's - see rideranim2's note). The remaining failed
checks are not this port: `camera` (the flyby camera exported during the countdown, camera unit) and `rider.bones` (723
non-finite pose reads, the same vehicle/collision divergence).

**No regression** (final live tree, `builds/vehicleentered/cli`): `verify.sh quick` on Concord - `rx_race` 48 ports,
`rx_loop` 18, `rx_world` 132: **0 mismatches, On identical** (state hash `1f6611fa80b80831`, memory `1487a52f208b4b26`);
`core/build.sh` clean (`builds/vehicleentered/core_build.log`).

**Not ported:** `00563ce0` (`MXActivityFreestyleStub::vf53`; only the Freestyle class's vtable, not `MXActivityRace`'s slot 53 =
`00569e10`) is not on this path.

## aidataverify (2026-10-21, agent "aidataverify"): the AIData loaders against the original, and the start chances / stages they produce

The AIData unit's session errored after its code landed (`race_aidata.c`, wired into the product's init stand-in after RiderPhysics);
this pass verified the landed code, fixed one missing original write, and measured the resulting AI start state and ride. Build dir
`builds/aidataverify/` (oracle CLI `builds/aidataverify/cli`; product `builds/aidataverify/ridecur2` = live tree; logs
`builds/aidataverify/logs/`; `stages.py` + `oracle_stages.sh` read the original's AI state out of a `REFLEX_MEMDUMP` image).

### The asset load (0075ed60, measured from its disassembly)

`0075ed60` loads databases through vtable +0x14 with the directory string "Database\\" (0x93e434), in this order: Animations
(0x96d734), RiderPhysics (0x96d740), CameraTweaks (0x96d750), **AIData (0x96d760)**, SnakeDatabase (0x96d768). Then the +0x38 job
"InitSoftFontCallback" (0x96d778, callback 0075ed40), StuntDatabase (0x96d790), the level name (game +0x34), and "Music" (0x95bd30,
vf +0x1c check / +0x14 load). The packages follow (vf +0x40): Animations/Animations at 0075ef7f, RiderPhysics/RiderPhysics at
0075ef9d, SnakeDatabase/SnakePackage at 0075efbb, then **the AIData package alone through vf +0x3c (0x96d760) at 0075f009 and
0051e350 at 0075f01a**, and StuntDatabase/StuntData at 0075f059. The product's stand-in (`api/rx_session.c build_package_loaders`)
loads db+package pairs for Animations, RiderPhysics, AIData and then runs `rx_aidata_load()`: the same relative order for the
subset it has (CameraTweaks/SnakeDatabase/StuntDatabase are other units' missing steps), and the AIData package is complete before
the loader either way.

### 0051e350 and the three loaders

`0051e350` (191 B) takes the engine Database `[0xa95844]+8`, asks it for handles with the "bxml" tag (0x6c6d7862) - AIBehavior
0x93f108 and AIAbility 0x93f114 with flag 1, AIAbilityOverride 0x93f120 with flag 0 (optional; a null handle returns) - marks each
handle's +0x4c in use, and passes `handle[0x44] - 0x460` (the XMLReader) to 0051eac0 / 0051e430 / 0051e7b0. It has no null check on
the first two handles: the original does `mov eax,[esi+0x44]; mov [esi+0x4c],1` regardless, and the port keeps that (a missing
database faults the same way). The XMLReader attribute cursor the loaders need is the reader's second pool (vf59 00828b50 / vf60
00828c00 / vf61 00828c30: top +0x180, free slots +0x40+4*top, entries +0x90+12*slot {node, index, name}); it is local to
`race_aidata.c` because these loaders are its only caller (`build_xml.c` owns the reader's other virtuals).

`0051eac0` (1030 B): root (vf +4) -> rows; the row name is copied into a 0x104 buffer (vf +0xa0) and compared with the original's
inline byte loop (case-sensitive strcmp) against the 13 behaviour names (GeneralBehaviors 0x93f190, NavigateTrack 0x93f1a4,
AvoidTrackBoundaries 0x93f1b4, TakeLine 0x93ed8c, SplitSection 0x93f1cc, AvoidObjects 0x93f1dc, Seperate 0x93f1ec, BrakeTurn
0x93f1f8, HandleBraking 0x93f204, AirLeveling 0x93f214, Offtrack 0x93f01c, OverrideSpline 0x93f220, SuckRacing 0x93f230). Each
vf1 iterates its row's children (`grouptype` string for AvoidObjects) and calls the matching value reader; the readers walk two
groups x three rows x four leaves and write the flat arrays at 0x9e3xxx / 0xa04xxx (for example GeneralBehaviorValues' Pullback
0x9e43b8, PushForward 0x9e3ff8, PushForwardOnTurn 0x9e4478, MaxAngleBeforeSlowdown 0x9e4838; the boundary / center / offtrack /
spline / separate / takeline tables are listed in the file). HandleSuckingValues (00534ee0) is a pure walk: the archive's
AIBehavior has no such group, so it is never called.

`0051e430` (872 B): the personality / chance block. It clears 0xa24c bytes at 0xa89d08 (0051e48a - the one write the landed port
missed; see below), then walks DriverType_ rows: the row name is compared with _stricmp (IAT 0x9162bc) against the 14-entry table
0x9cd7d8. Each DriverAbility_ row (0x9cd810, 18 entries) sets its presence byte at block+0xb8 (0xa89dc0), then its four difficulty
rows (the ability row's children, in order) each read their Tier1 / Tier2 / Tier3 children (0x93f158 / 0x93f174 / 0x93f17c): one
float each, or two when the Tier1 node's `grouptype` contains "vector" (strstr 0x91621c). The values go to block+0, +8, +0x10 for
the first difficulty row, +0x18 for the next, and so on (dst starts at 0xa89d10 + block, so Tier1 lands at block+0 - the file's
`rx_xml_vf_node_floats(..., dst - 8, n)`); four rows = the first 0x30 bytes. After the rows the loader runs 0051e1d0 for slots
0..10 (ECX), then releases and advances. The block is 0xbc: 0x30 of tier values, 11 explicit slots of 8 bytes at +0x60
(0xa89d68 + block + slot*8), presence at +0xb8.

`0051e7b0` (766 B): the optional override asset; DriverType -> DriverAbility -> the ability's group -> Player01..Player11
(sprintf "Player%02d" 0x93f184 into a 16-byte stack buffer, then vf +0x1c child by name); when the child's grouptype contains
"vector" it writes two floats at 0xa89d68 + block + (player-1)*8, else one. The archive's AIData package has it (the loader ran
once in the boot).

`0051e1d0` (375 B, EAX type, ECX slot): for every ability, copies the first value of each difficulty row into the explicit slot
(0xa89d68 + block + slot*8; the presence row's +0xb8 is not touched), with the compiler's strength-reduced 8-way unroll: the extra
`+4` copies of the 4th/12th ability of a chunk take their value from the *next* ability's row head (the port reproduces the exact
offsets, including that artifact). 374 calls per AIData load = 34 present ability rows x 11 slots.

### Verification

Baton boot of Concord event 1 (the front-end default event - the loader runs in the init thread regardless of the rider
count) with `REFLEX_BOOT_SCHED=baton REFLEX_EVENT=1 REFLEX_PORT_VERIFY=1`:

- `0051e350` **1 call / 1 verified / 0 mismatches** with the whole chain registered: the original init thread (0075ed60) calls the
  adapter, the core runs AIBehavior -> AIAbility -> AIAbilityOverride, and Verify compares the core's memory effect with the
  original's on the same entry state. Ref (no ports), core and Verify boots produce **byte-identical world images**
  (`5c8c85234be7dcb3c5014bf667dc7066`) and canonical dumps (`c1e55b1a738c6b284429fd46370b67b5`; world_compare.py: 160456
  identical nodes, 0 differ, 9805 unreached matched, VERDICT: IDENTICAL). The Verify boot's image equals the original's, i.e. the
  boot's host-state undo is exact through this chain.
- The three loaders with the original 0051e350 as the caller (`REFLEX_PORT='rx_aidata,-rx_AiDataLoad'`): `0051eac0` 1/1/0,
  `0051e430` 1/1/0, `0051e7b0` 1/1/0.
- The preserved `builds/aidata/la_final2` run (18:29, `race_aidata.c` md5 `aeb5663d862f87ea9ba4a83a55de8940`, unchanged until this
  pass): `0051e1d0` **374 calls / 374 verified / 0 mismatches**, 15 of the 16 value readers **1/1/0 each** (00534ee0
  HandleSuckingValues 0 calls: the archive's AIBehavior has no such group), port / ref / Verify images and canon identical. The
  readers are verified with the original vf1s as their callers, i.e. per call at every level below the loader.
- Dead-stack-only differences (no state effect, hence 0 mismatches): the original also reads the root node's name into a dead
  buffer (0051eb07 / 0051e478 / 0051e7fd) and sprintf's "DriverType_%s" / "DriverAbility_%s" into dead buffers before comparing
  the **raw** row names (0051e4ef, 0051e58d, 0051e85f, 0051e8ef - the formatted result is never read); the port compares the raw
  name directly. The local frame sizes differ (0x210 / 0x638 / 0x73c original vs 0x108 / 0x640 / 0x740) because they are only a
  local layout: the reader keeps slot indices, not stack addresses, so no callee retains a frame address.

### The missing write (fixed)

`0051e430` did not clear 0xa24c bytes at 0xa89d08 (0051e48a). The EXE's .data block is zero, so the shipped image and the
verified result were unchanged, but it is a real original side effect (a second load, or any dirty table, must be cleared like
the original). Added `crt_memset(0x00a89d08, 0, 0xa24c)` at the original's position (the root-name read the original does just before
it is a dead stack write and is not in the port; the memset is the first observable write, before the rows). Backup `builds/aidataverify/backup/race_aidata.c.before`. Re-verified after the
edit: 0051e350 1/1/0, 0051e430 1/1/0, world images still identical, and the product ride log is byte-identical
(md5 `18347a2fdb36ef7de16794406aac3f93`).

### Start chances and stages (the point of the tables)

The chance a rider starts is 0051dcd0: table 0xa89d68 (block +0x60) when the player's explicit index +0x8fc is not -1, else
0xa89d08 + type*0xc7c + (tier*3 + level)*8, with type = driver +0xee8, tier/level = player +0x8f4/+0x8f8. 0050fb00 draws the
board's percentage and puts `chance < pct` riders in stage 4 (launch when the clock passes +0x10f8), the rest in stage 5 -> 6
(wait for the reaction +0x13c).

Measured in the original (pc1 reference world, 12 riders, `REFLEX_DRIVE=1`, `REFLEX_MEMDUMP` at frames 1100/1130/1150/1156/1170/1200,
`builds/aidataverify/stages.py`):

- during the countdown (race state 1, board 0.94 .. 0.008) the 11 AI are **9 in stage 4 and 2 in stage 6** (slots 10/11) - the mix
  is real, no all-stage-4; chances 8 / 10 / 12 / 15 / 17 / 20 by driver tier and level; the AI's clock is 19.267..20.200 and the
  stage-4 launch times (+0x10f8) 17.4..20.0, the stage-6 reaction times (+0x13c) 20.3..21.0.
- race state 2 starts at frame 1156. At 1170 one AI has launched (stage 0); at 1200 seven have (7 stage 0, 3 stage 4, 1 stage 6).
  Stage 5 never appears at a frame boundary: 00511760 case 5 sets 6 and returns the same frame. Stages 1 / 2 are 0050fb00's
  no-board branch (not reached here).
- the local (human) driver's +0xf10 is not a stage (it holds 0x17cf0000 in the world), so only the 11 AI count.

Product (live tree, 12 riders AI 95): first racing frame **1178**; at that frame **10 stage 4 + 1 stage 6** (slot 5), chances
12,12,10,10,10,8,10,12,8,15,20 for slots 1..11 and 0 for the human slot 0; every AI is stage 0 by ~frame 1250. The earlier
check's "5/1/2" is the waiting-branch family (5 -> 6, and 1/2 without the board), not a count: the measured split is
9 stage 4 / 2 stage 6 in the original and 10 / 1 in the product, both driven by the same table and the same RNG shape.

### The ride (live tree, 12 riders, `RX_FULL_FRAME=1`, 1/60 s)

- AIData tables: the chance rows are nonzero (the values above) and the 0xa89d08 personality block is byte-identical to the
  original's (world image md5 above); the AI read those 0x9e... tuning arrays when they steer, brake and pick a line.
- 1700-frame product dump (no input, every frame): all 11 AI ride - speeds 35.7..79.9 at race+244 (original 36.0..77.9) and 35.1..64.8 at
  race+802 (original 35.0..65.7), places 1..11 with the idle local 12th, 0 laps at 13.7 s, and `wreck 0` for all 12 vehicles on every
  frame, gate included. No gate wreck, no launch into the gate.
- clean input ride (W from the drop, A/D after race+200, brake windows; 4000 steps requested): **1890 steps, session frame 1899,
  race frame 771, stop unported `0054ec80`** (Player_UpdateWreckRecovery's WreckRecoverySuccess child), max 75.44, gear 3, path
  396.1, place 12, 0 laps, state hash `f7cbfdedf472cbc8`. W held: **3110 steps, session 3119, race 1991 (33.2 s), the same
  `0054ec80`**, max 83.94, gear 4, path 1615.3, state hash `48d71716fe0fbc4b`. The earlier `0086f2a0` stop is passed.
- stale-object-set finding (23:27 objects, for the UI-channel owner): the W ride **hung** at race+543 in the holeshot dispatch -
  `00569eb0` -> `004020c0` -> `00593220` -> `00702ad0` -> `008519a0` -> `78495979 crt_str_less` - because there is no UI manager
  (`[0xa93fcc] = 0`, measured) and that object set used the oracle form `[0xa93fcc] + 0x98 = 0x98` for the channel, so
  registry_find walked a bogus string map. The live tree's `build_ui_world.c` stand-in returns its own channel (0x11892a80, 4
  registered event types, valid map), and the same W ride passes the frame and reaches race+1991.

### Regression

`native/game/tools/verify.sh builds/aidataverify/cli/reflex_boot rx_race quick` -> **48 ports called, 0 with mismatches, On
identical** (state hash `1f6611fa80b80831`, memory `1487a52f208b4b26` over 27272 pages); `rx_world quick` -> **132 / 0 /
identical**; `core/build.sh` clean. No game file other than `race_aidata.c` (the added memset) was edited.

## holeshotfix (2026-10-21, agent "holeshotfix"): the holeshot dispatch's missing UI channel; the product's 1-lap race reaches Post and writes the records

Task: the 1-rider / 1-lap product race (W held, `RX_FULL_FRAME=1`) hung at race 8 s in
`rx_race_holeshot -> rx_world_event_dispatch -> rx_world_registry_find -> rx_world_string_map_find ->
rx_world_string_map_lower_bound -> crt_str_less` (builds/finishui/logs/hang_sample.txt), and the finish (state 3 -> 4, the
records) had only been verified in the oracle. Build dir `builds/holeshotfix/` (product `api/`, oracle CLI `cli/`, logs,
backups of every edited file in `backup/`).

### Root cause: the dispatch's channel is the UI manager's, and the product has no UI manager

The event-name map is not malformed - **the map object is missing**. `00569eb0 rx_race_holeshot` raises "HoleshotActivated" on
`[0xa93fcc] + 0x98` (`mov eax, [0xa93fcc]; mov ebp, [eax+0x98]`; 00569eb9..00569ee7). `0xa93fcc` is the MXUIManager pointer
(00402c20 reads it; its base constructor 005da520 writes it at 005da6ac), and `+0x98` is the EventSystem::Channel "UI" that the
same constructor builds at 005da6cb..005da739 (00593900 the name, 005930a0 the base constructor, 007ce320 the list head,
0x00946258 the vtable, 00593520 the registry insert). The product has no UI manager (`[0xa93fcc] = 0`, a documented boundary),
so the chain argument was **0**: `rx_world_registry_find(0, name)` forms `map = 8`, `head = U32(8, 0x18)` = 0, and
`rx_world_string_map_lower_bound` starts at `x = U32(0, 4)` = 0 with `U8(0, 0x2d) = 0`; the zero page is not a tree, so
`x` never reaches a nil node and `crt_str_less(0xc, key)` loops forever. Sampled live (lldb): `[0xa93fcc] = 0` at the hang and
the frame is `rx_race_holeshot + 152`.

The original never sees this: its UI manager's channel exists and MXUIManager::vf42 (005999f0) subscribes the UI's handlers to
it ("HoleshotActivated" -> 005c0df0, "FreeRide Started" -> 00586ca0, "ShowPageWipe" / "PageWipeTransition" -> 00592820). The
fix is the construction the product was missing, not a guard:

- `core/game/build_ui_world.c` (`rx_ui_world_create`, next to the UI manager's `g_MXRecords` stand-in) constructs the same
  channel with the game's own `rx_event_channel_create(0x0094bf04 "UI")` and registers the four events the game's own code
  raises on it with their original signature type_infos: `HoleshotActivated` 0x009420d0 / 0x009cbeb4,
  `FreeRide Started` 0x00941f90 / 0x009cf3f0, `ShowPageWipe` 0x00941e9c / 0x009cdb68, `PageWipeTransition` 0x00941d7c /
  0x009cdb68 (the UI's handlers are what is absent, so the handler lists are empty; the events are registered so the lookup
  finds a well-formed event with an empty list). `rx_ui_event_channel()` returns it in the product and `[0xa93fcc]+0x98` in
  the oracle (identical to the original there).
- The three raisers use it: `rx_race_holeshot` (00569eb0), `rx_free_ride_in_progress` (005609d0, the 5 s "FreeRide Started"
  notice) and `rx_show_page_wipe` (0055e990; the oracle path is unchanged, the product raises the event on the stand-in and
  still runs the game's page-wipe callback for "WinddownEnd").

### The rest of the 1-lap ride (each stop on the way to the finish, ported or switched)

| stop | what | where / verification |
|---|---|---|
| `006cc230` | the stunt scorer's "VehicleEntered" handler on the vehicle trigger channel [0x00a89bc8]: only the scorer's own player / vehicle and a "FlowJudging" trigger; a repeat adds the flow judging time (0x9c3e54) to +0x34 while InProgress (006ccf90 inline), the trigger joins the 0x28-entry list +0x84, +0x80 = it | `fx_stunt.c rx_fx_stunt_vehicle_entered`, adapter `oracle/fx_stunt.cpp` (rx_fx) |
| `006ab560` | ClipRecorder_FillerClip: no candidate and the first human driver on the bike -> a type 1 candidate (006aaca0) | `anim_scenario_clip.c rx_clip_recorder_filler_clip`, adapter (rx_anim); the recorder tick 006aab70 now calls it at the original stack depth |
| `0076f160` | Terrain_ContactResponse: 0076efc0's body with the material pair (A +0x24, 0) and the inverse inertia merged without the extra * 1 | `world_body.c rx_world_terrain_contact_response`, adapter (rx_world); `world_phys.c`'s contact loop calls it directly (`WORLD_CALL_SP(4, ...)`) instead of `WORLD_CTX` |
| `005595b0` | Activity::vf33 (InProgress exit): every standing driver's player's scorer (+0x74 + 0xc70) refreshed (006cc8b0) | `race_activity.c rx_activity_in_progress_exit`, adapter (rx_race); reached through the vtable's integer table |
| `00583130` `00583170` `00582070` `005848a0` `005820d0` `00582220` | the Race activity audio logic's finish-state handlers: "InProgress Exited" (+ music stop 00677510 for state 0/1/5), "WindDown Entered" (+ the race music 00581760), "WindDown Exited", "Post Entered" (+ the music pick, vf37, the music stop), "Post Exited" (+ MusicPlayer_FadeOut 00677600), and vf37 00582220 (the second music entity's tracks stopped, 006770d0) | `race_activity_audio.c` / `build_activity_freeride_run.c`, adapters (rx_race / rx_build11). The music-player calls are the documented audio boundary (oracle original, product nothing); the cues still go through rx_audio_cue. |
| `0050f5d0` | RaceDriver::vf4: the player's controls reset (006c9100), the player finish reset (0054e280: 0054e890, +0x1168/+0x116c = 0, a wrecked vehicle's controls released 0045f590, the +0x99c object's vf29), the simulation's pose override ended (00434ef0), +0x3c0/+0x3cc = 0 | `race_driver_update.c rx_race_driver_vf4` + `race_player.c rx_player_finish_reset`, adapters (rx_race) |
| `006c1a90` | ScoreTowerFlagHolder's flag-state apply (a jump table over the state, ECX = 0 here) | already ported (`world_flag_holders.c rx_score_tower_apply_state`); `loop_entity.c`'s `loop_ex` call site switched to the port |

### Product result (1 rider, 1 lap, W held, full frame)

`builds/holeshotfix/api/finishrun "../game files/Archive" 16000` (RX_FULL_FRAME=1): 9 load frames; racing at step ~1200
(game 20.2 s); **FINISHED at step 9906 (session frame 9916, game 165.304)**, activity state 3; the 3 s wind-down then runs
`rx_show_page_wipe("WinddownEnd")` -> the page-wipe callback -> `SetState(4)`; **activity state 4 (sub 2)** from step ~10200
on and **the run completes all 16000 frames with no stop** (end state FINISHED, activity 4, game 266.829). Records slot 0:
`TotalEvents=1 TotalLaps=1 TotalPlacing=1 TotalScore=0 TotalTime=146.399994 BestLapTime=FLT_MAX BestPlace=1 BestScore=0`;
the local standing is place 1, laps 1, finished 1, time 146.384.

Two record fields differ from the finishui oracle's 1-lap run (`1 / 1 / 1 / 82 / 162.550003 / FLT_MAX / 1 / 82`):

- **TotalTime**: the race clocks start at different points. The oracle's 1-lap run was booted from the captured world image
  `builds/finishui/world1lap/c` (the front end already skipped, the race starts ~3.2 s of game time), the product runs the
  front end + intro + countdown and starts at game ~20.2 s; both cross the line at the **same game time** (165.7 vs 165.3 s),
  so the 146.4 vs 162.55 s is the start offset, not a finish-path difference.
- **TotalScore / BestScore** (CORRECTED by agent "profilestats", see the section below): the record writer stores
  `(int)MXRecords_Score(player + 0xcec)`. The direct stores to +0xcec are indeed only the network snapshot
  (FreestyleDriver::vf7 0050cf00, online) and the freestyle activity's finish (005626e0, kind 8), but the score is written
  **through the scorer base**: the stunt scorer is `player + 0xc70` and its total score is `P + 0x7c` (= player + 0xcec).
  `Activity::vf33` (the InProgress exit, 005595b0) refreshes that scorer (006cc8b0) at the finish, which recomputes
  `+0x7c` from the race's physics stunts. The captured world image's player +0xcec is **0** (probe read in this pass:
  `101f4d90+0xcec = 0`), so the oracle's 82 is not profile state: the oracle's 1-lap ride scores 3 physics stunts (whip,
  wheelie, wheelie) during the race. The product's 0 was a missing initialization: the init thread's StuntTable load
  (006c8480 / 006c8b70 / 006c8670) was not in the product stand-in, so no physics stunt resolved and the scorer's refresh
  summed 0. Ported in this pass (`core/game/fx_stunt_table.c`); the product now writes 81 (6 stunts) - the remaining 1
  point is the ride's stunt set, detailed below.

### Verification

- `core/build.sh` clean.
- Oracle CLI `builds/holeshotfix/cli` (all core files, adapters): `verify.sh ... rx_world quick` **132 ports / 0 mismatches /
  On identical**; `rx_race quick` 48 / 0 / identical; the 1-lap world-image run with the ports (On) is in the QUEUE entry.
- `verify.sh ... rx_race quick` **48 / 0 / On identical**, `rx_fx quick` **108 / 0 / identical**, `rx_anim quick` **44 / 0 /
  identical**, `rx_loop quick` **19 / 0 / identical** (`logs/verify_three_quick.log`, `logs/verify_loop_quick.log`).
- The finish-path ports are not reached by the reference worlds (no world completes a race in 1200-1800 frames), and the
  1-lap world-image Verify boot reports 0 calls for every new port (20000 frames, `logs/oracle_1lap_D2.log`): not reached by
  the harness, reviewed against the disassembly. The state evidence is the On-mode equality: the 20k-frame run with every
  holeshotfix port (C) and with all of them excluded (C3) has the same state and memory hashes
  (`c1f0a00b7bcf33eb` / `7b73273d02b4d00d`), i.e. none of the new ports changes the oracle's state.

## wreckrecovery (2026-10-21, agent "wreckrecovery"): WreckRecoverySuccess 0054ec80 and its callees ported; the 12-rider rides pass race 1991 / 771 and run 6000 frames with no stop

The rides stopped at unported `0054ec80` (`Player_UpdateWreckRecovery` 0054ea30's success child: W at race 1991, clean at
race 771). Build dir `builds/wreckrecovery/` (oracle CLI `cli/`, product driver `api/` + `ride/`, logs, backups of every edited
file in `backup/`).

### What 0054ec80 is (disassembly 0054ec80..0054ecfe)

`push ecx / ebx / esi / edi` (0x10), stack argument p (`mov ebx,[esp+0xc]` after two pushes), `ret 4`:

1. `0054e890` (EAX = p) clears the wreck fields; then `[p+0x1b4]` (the rider animation) if set -> `00621d70` (stop animation).
2. `006ab9b0` (EAX = p) logs the recovery in the clip recorder's ring.
3. A **local** player (`+4 != 0`, `+0x10 == 0`) whose pad slot `+0x1f0 >= 0`: EAX = the driver's mode `[p+0x94]+0x23c`,
   ESI = the slot, EDI = `[0xd92edc]` -> `006d83d0` (highlight progress).
4. `[p+0x1ac]` (the vehicle) if set -> `00554da0`: event `0x00941260` with the vehicle as sender, the driver
   (`[p+0x94] + 0x290`) as dispatcher and the driver as argument.
5. `[p+0x11d0]++`.

`006ab9b0 ClipRecorder_LogRecover` (EAX = p, `push ebx/esi/edi`): in a live race (`[0xa942b4] == 1`) the rider index in the
race (0055a050), the id from the racing list `+0x268` (or the waiting list `+0x27c` for `+0x908`), a ring entry of type 7 at
`0xd2f160 + (cursor<<7)` whose time is the race's `+0x294` copied through x87 (`fld/fstp`: no value word, unlike LogWreck /
LogStunt) and the cursor wraps at 100; for an active local rider (`+4`, `+0x10 == 0`) with recording on (`0xd5a9a4`) a
**type 5** candidate `ClipRecorder_Candidate` (006aacf0) over `race +0x294 - [0x9c40d0] .. + [0x9c40d4]`.
`006d83d0` (EAX = mode, ESI = slot): the byte switch table 0x6d8400 picks the 0x2a add for modes 0, 1, 2, 6, 0xc, 0x11, 0x13
(a mode above 0x13 skips it) and 0x2b is always added, both through `MXHighlightTracking_AddProgress` 006d7980 (the profile /
Steam boundary: oracle original, product nothing, the `highlight_progress` idiom of fx_stunt.c).

### Ports and files

- `core/game/loop_player.c`: `rx_player_wreck_recovery_success` (0054ec80, registered), at the original's frame (the four
  pushes); `rx_player_update_wreck_recovery`'s call site now calls it directly at the original depth (`PL_AT(1, {p},
  0x0054ec0a, ...)`). Prototype in `loop_player.h`.
- `core/game/world_wreck_misc.c`: `rx_world_clip_recorder_log_recover` (006ab9b0, registered; prototype `world_wreck.h`). It
  reuses the file's `race_rider_index` / `rx_world_clip_recorder_candidate`; the ring entry writes only time / type / id.
- `core/game/fx_stunt.c`: `rx_fx_highlight_recover` (006d83d0, registered; prototype `fx.h`), next to `highlight_progress`.
- Adapters: `oracle/loop_player.cpp` (rx_loop), `oracle/world_wreck_misc.cpp` (rx_world), `oracle/fx_stunt.cpp` (rx_fx).
- Probe: `oracle/wreckrecovery_probe.cpp` (group `thunk`, hook at Engine::Update 0084ed20).

### Per-call Verify (0 mismatches): `RX_WR_PROBE=1 RX_WR_MODE=n`, Concord, `REFLEX_PORT_VERIFY=1`

The crash drive 7 reaches the **caller** (0054ea30 1800 calls, its stick read 0054ee80 3) but never the
recovery: the drive's stick never moves 0.25 in the mode's direction inside the wreck clock, so 0054ec80 is 0 calls there
(the world's local player merely has a pending wreck clock). The probe takes the live local human player (`kind 0` in the
players list at `[0xa946b8]+0x7460`), builds the state (a wreck clock `+0xb38`, mode 2 "y rising" `+0xb44`, the AI flag
`+0x10 = 1` so 0054ee80 returns the fixed axes `+0xb50/+0xb54` with y above `+0xb4c`, the accumulator `+0xb40`), and calls the
**original** 0054ea30 (or the original 0054ec80 for the branches 0054ea30 cannot set), so the original reaches the installed
core port. Every touched word is restored.

| port | variants | calls / verified / mismatches |
|---|---|---|
| 0054ec80 (mode 1, 0054ea30 not installed) | rider animation present, none (`+0x1b4 = 0`), the waiting-list id (`+0x908 = 1`) | **3 / 3 / 0** |
| 006ab9b0 (mode 2, 0054ec80 not installed) | recording on local (candidate branch), AI (ring only), recording off, `+4 = 0`; slot -1 skips the highlight branch | **4 / 4 / 0** (the nested 006aacf0 candidate call also 1/1/0) |
| 006d83d0 (mode 3, 0054ec80 not installed) | driver modes 0, 1, 2, 6, 0xc, 0x13 (0x2a+0x2b), 0x14 and 3 (0x2b only), slot -1 (no call) | **8 / 8 / 0** |

Why mode 2 uses slot -1 and mode 3 has a stand-in: `006d7980` cannot run in the world - the world image has no profile
database (`[0xd92edc]` holds stale bytes; `Database_FindTable("Highlight")` faults), which is the same profile boundary the
fxport QUEUE entry documents. Mode 3 therefore replaces 006d7980 with a **counting stand-in** both runs see (the racedrv
marker-stand-in device): it records `(index, slot)` of every call in a probe-owned heap block, so a dispatch difference would
show up in Verify's memory compare; the logged pairs are identical in the Native and Original phases for every variant.

### Product (live tree, `RX_FULL_FRAME=1`, 1/60 s, 12 riders AI 95)

`builds/wreckrecovery/ride/ride` (`builds/inputrace2/ride.c` against the live tree): the previous stops (W race 1991 / clean
race 771) are gone; both rides **complete all 6000 requested steps** (race frame 4881, game 100.19 s, no stop):

- W held: max 83.94 (frame 1536), gear 4, path 4023.1, no stop, state hash `b6b62d3f12d0e379`.
- clean input: max 75.44, gear 3, path 2610.8, no stop, state hash `d3059e253cf5b460`.
- `rx_api_test` (`RX_FULL_FRAME=1`, 3000 race frames): runs 3000/3000, state 2, 12 riders, race time 31.4, bike/rider/camera
  getters valid (no stop).

**Next stop (12000 requested): step 10873 (W) / 10990 (clean) - race frame ~9755 / ~9872, game 181.42 / 183.37 s - at unported
`00508460`**, a 20-byte leaf called from `Player_UpdateIdleRider` 0054cec0 (`rx_player_update_idle_rider` is ported; the call
is its `pl_call0` original): the vehicle's ground normal `[v+0x140 / +0x144 / +0x148]` copied into the out
vector through x87 `fld/fstp` (ECX = the vehicle, EAX = out; EDX / EDI unused). Not a wreck / recovery function; reached
through loop_player.c's `pl_call0` wrapper, which `census_original_calls.py` does not scan. The idle-rider fidget path
reaches it only after ~30 s of idle riding. No stop before race frame 9755
once the recovery path is in.

### Regression

`verify.sh builds/wreckrecovery/cli/reflex_boot <group> quick`: rx_race **48 ports / 0 mismatches / On identical**, rx_loop
**20 / 0 / identical**, rx_world **132 / 0 / identical**, rx_fx **108 / 0 / identical**; `core/build.sh` clean. The two fx and
world files got the one new function each; loop_player.c only the new port and the switched call site.


## profilestats (2026-10-21, agent "profilestats"): MXPlayer+0xcec is the stunt score, and its missing init is the StuntTable load

Task: the 1-rider / 1-lap product records were `1 / 1 / 1 / 0 / 146.399994 / FLT_MAX / 1 / 0` while the oracle's were
`1 / 1 / 1 / 82 / 162.550003 / FLT_MAX / 1 / 82`; the task asked where the original sets `MXPlayer+0xcec`. Build dir
`builds/profilestats/` (oracle CLI `cli/`, product driver `api/profrun` + `api/rx_api_test`, logs, probe
`core/oracle/profilestats_probe.cpp`). Every edited file backed up in `backup/`.

### The source of the score (not profile / saved game)

- The only **direct** stores to `player+0xcec` in the exe are FreestyleDriver::vf7 0050cf00 (the online snapshot) and
  MXActivityFreestyleStub::vf14 005626e0 (the freestyle finish). But the stunt scorer is embedded at `player+0xc70` and its
  total score is **`P+0x7c` = player+0xcec**; `006cc8b0` (called from `Activity::vf33` 005595b0, the InProgress exit, and
  from 006cab80's pending refresh) recomputes it as `2 * (the five ratings)`, clamped [0, 100].
- Evidence: the captured world image `builds/finishui/world1lap/c-world.bin` has **player `101f4d90` (game+0x7460) +0xcec =
  0** - the earlier holeshotfix note's "already 82 in the image / profile state" is wrong (re-checked in this pass). In the
  world-image run the original writes the score during the race: `00568da0` reads `+0xcec = 82.383339` (probe) and 006cbdb0
  ("StuntEnds", the handler that increments the scorer's `+0x418` trick count) fires **3 times**: `whip` at scorer air
  13.752 / ground 26.031, `wheelie` at 24.818 / 52.398, `wheelie` at 27.715 / 60.101. The recent-trick list (+0x364) is
  `whip`, `wheelie`.
- So TotalScore/BestScore are the race's physics-stunt score, computed by the game's own `006cc8b0` at the finish; no
  profile / saved-game row is involved (the product still has none, as documented).

### Why the product was 0: the StuntTable was never loaded

- The trick tracker `006c9770` gates on the ConVar **"StuntsEnabled"** (`0x00d5ab14`, value at +0x24 = 0x00d5ab38; the
  static initializer 0090a980 is ported, and both builds have it = 1). The physics-stunt path then looks a stunt up **by
  name** (`006cd000` kind -> `Wheelie` / `Stoppie` / `Backflip` / `ThreeSixty` / `Whip`, `006c8570` searches the descriptor
  table at `0x00d5aad4`, count `0x00d5aad8`). In the oracle world image the table has **64 rows** (the StuntTable BXML of
  StuntDatabase/StuntData); in the product it was **pointer 0 / count 0**, so no physics stunt resolved and the scorer's
  refresh summed 0 at the InProgress exit.
- The original builds the table in its **init thread 0075ed60** at 0075f103: `006c8480` (the 100-record allocation and the
  four per-tier vectors), `006c8b70` (open the "StuntTable" bxml asset, parse every `<Stunt>` row with `006c8670`, copy it
  into the table with `00622050`, bucket by Tier with `007cff20`), `006ca5a0` (the aerial / ground input mapping files).
  The product's init stand-in (`api/rx_session.c build_package_loaders`) loaded only Animations / RiderPhysics / AIData;
  StuntDatabase and these calls were missing ("other units' missing steps" in the aidataverify note).

### Ported in this pass

- `core/game/fx_stunt_table.c` / `.h`: `006c8480` `rx_stunt_tables_init`, `006cd6e0` `rx_stunt_table_alloc`,
  `006c8b70` `rx_stunt_table_load`, `006c8670` `rx_stunt_row_read`, `006ce1a0` `rx_stunt_descriptor_grow`,
  `00622050` `rx_stunt_record_copy`; `00617e80` the record constructor (`stunt_record_ctor`) and `007cff20` reuses the
  existing `rx_iarr_reserve` (build_rider_gear.c) - not registered twice. The row parser reads the same 12 children
  (name attribute, Anim, LocString, Tier, Points, Difficulty, Aerial, Physics (also the validity return), AerialOrGround-
  Equivalent, SteeringDisabled, IgnoreBrake, IgnoreThrottle), uses the game's tolower table 0x9b77c8 and the original
  `%s`/`_vsnprintf` wrapper, and leaves the fields 00622050 does not copy (`+0x7c`, `+0x8c`, `+0xb4`, `+0xb8`, `+0xbc`) as
  allocation residue - in the oracle image they hold values no loader writes (0/1/2, 5, 0x24a2...), so they are dead; a
  byte compare of the written fields is the acceptance.
- `core/game/fx_stunt.c`: `006caaf0` `rx_fx_stunt_combo_end` (the combo end: the score `(tricks*10 - style - +0x28 - +0x2c) /
  tricks` -> 006cd870 "StuntFinalScore", else "StuntFailed") and `006cd870` `rx_fx_stunt_final_score`; the two
  `rx_call_original_ex(006caaf0)` sites now call the port. This was the next product stop: with the table loaded the first
  trick resolves, the combo ends, and the ride aborted at step 3180 (`unported 006caaf0`). Ported + adapters; no abort.
- `core/api/rx_session.c`: StuntDatabase / StuntData loaded after AIData and before the table calls (the original's
  0075f059 / 0075f103 order).

### Verification

- **Table**: product dump (64 rows) vs the oracle world image's 64 records at `0x13b8e8d0`: name (lowercased), Anim,
  LocString string, AerialOrGroundEquivalent, Tier, Points, Difficulty (float), Aerial, Physics, row index and the three
  flags are **identical in all 64 rows** (0 mismatches). Pointers differ (different heap), and the dead residue fields
  differ by design.
- **Product finish** (`api/profrun "../game files/Archive" 12000 1 1`, `RX_FULL_FRAME=1`, W held): FINISHED at step 9906
  (game 165.304), activity 4, records slot 0 `TotalEvents=1 TotalLaps=1 TotalPlacing=1 **TotalScore=81**
  TotalTime=146.399994 BestLapTime=FLT_MAX BestPlace=1 **BestScore=81**`; the player's +0xcec = 81.125 (6 tricks; the
  scorer's ratings 8.0781 / 9.0844 / 4.5 / 8.9 / 10.0; recent `whip`, `wheelie`).
- **Oracle**: 82.383339 -> 82 with 3 tricks (whip, wheelie, wheelie; ratings 8.6104 / 8.9813 / 4.5 / 9.1 / 10.0).
  TotalTime is the documented start offset (the product runs the front end, the oracle image starts later; both finish at
  game ~165.3 / 165.7).
- **Remaining difference (ride, not initialization)**: the product counts 6 stunts (all whip/wheelie in the recent list) at
  scorer ground 20.7 / 50.1 / 56.0 / 64.7 / 73.8 / 90.8 vs the oracle's 3 at 26.0 / 52.4 / 60.1. The score follows the
  game's own formula from that set (81.125 vs 82.383), so the 1-point gap is the ride's physics-stunt set - a
  vehicle/trajectory difference, not a score or profile initialization. With the table present the values are the game's,
  not 0.
- **No regression** (oracle CLI `builds/profilestats/cli`): `verify.sh quick` rx_fx **108 / 0 / On identical**, rx_race
  **48 / 0 / identical**, rx_world **132 / 0 / identical** (`logs/verify_*_quick.log`); the 1-lap world-image Verify run
  with `REFLEX_PORT=rx_fx` completes 12000 frames with 0 mismatches (the new table ports 0 calls: the original init thread
  loads the table there). `api/rx_api_test` (12 riders, 3000 frames) exit 0. `core/build.sh` clean.
- **Remaining / boundaries**: `006ca5a0` / `006ca3f0` (the AerialStuntInputMappings / GroundStuntInputMappings loaders) are
  not ported; the arrays they build at 0xa94310 / 0xa94314 are zero-filled in the original too (006ca2f0 only prints), so
  the input-driven trick path is empty either way, but the product's `[0xa94310] = 0` is only safe because address 0 reads
  as zero. `006c9aa0` (the gesture matcher called from `rx_fx_trick_input`) is still an `rx_call_original_ex` - the product
  would abort if a stick gesture completed (not reached in a W-only ride); porting it + 006ca5a0 is the next step for
  stick tricks. The dead table residue fields (+0x7c etc.) are not initialized (same as the original).

## groundnormal (2026-10-21, agent "groundnormal"): the idle rider's fidget callees 00508460 / 0061ea90; the rides pass race 9508 and stop at the lens emitter 0063c040

The 12-rider rides stopped at unported `00508460` (the wreckrecovery entry: W step 10873 / race 9755, clean step 10990 /
9872 on that tree). It is the **only remaining unported callee of `Player_UpdateIdleRider` 0054cec0** (the other, `0061ea90`,
was unported too); both are ported here. Build dir `builds/groundnormal/` (oracle CLI `cli/`, product `api/rx_api_test` +
`ride/` (06:43 tree) and `ride2/` (final tree), probe `groundnormal_probe.cpp`, logs, `backup/` with every edited file).

### What they are

- **00508460 Body_Up (ECX = body, EAX = out; RET 0; 20 B)** - `core/game/world_rider_62.c`, next to its sibling 00508480
  `Body_Forward`: three `fld dword [ecx+0x140/0x144/0x148] / fstp [eax]/[eax+4]/[eax+8]` copies (`world_x87_pass`: a
  signalling NaN comes out quiet). Prototype `world_rider.h` (`rx_world_body_up`), adapter `oracle/world_rider.cpp` (rx_world).
- **0061ea90 RiderAnim_StartIdleGesture (ESI = ra; RET 0; 47 B)** - `core/game/world_rider_61.c`, before the gesture state
  machine 0061eac0: `FloatListeners_Get(ra + 0x38c)` (0063b8f0, ported) and, when the value is strictly between 0.2
  (`0x92e388`) and 0.8 (`0x9cc8a8`), `+0x5b6d4 = 1` (the state 0061eac0 starts a gesture from). Frame `push ecx`
  (`world_frame_flat(1, 0)`), the getter called at the original's `WORLD_CALL_SP(4, ...)` depth. Prototype in
  `world_rider.h`, adapter in `oracle/world_rider.cpp`.
- **Call sites** (`loop_player.c` `rx_player_update_idle_rider`): `PL_CORE` + `PL_AT` with the original's return addresses
  (`0x0054d078` for 00508460, `0x0054d0cf` for 0061ea90) and the `pl_call0` original as the not-ported fallback.

### Per-call Verify: `oracle/groundnormal_probe.cpp` (0 mismatches)

The five reference worlds do not ride (the local player stands at the grid), so the fidget's 30 s idle clock never runs there
and both ports are 0 calls in a plain Verify. The probe (frame hook `0084ed20`, group `thunk`, `rx_GroundNormalProbeHook`,
`RX_IDLE_PROBE=1 RX_IDLE_FRAME=1200`) takes the live local human player and builds exactly the state 0054cec0's tail reads:
the vehicle's `0xa96dc0` speed-table entry at 1.0, the rider's clip `+0x580` = its state `+0x5a0`, `+0x10b8` = 30 s,
`+0x10b0` = 1, `+0x10b4` = 0, the bike's vf43 word `+0x8d98` nonzero and its vf37 up vector `+0x100..+0x10c` = (0, 1, 0, 0)
(the grid image's live lanes there are stale: Concord's up vector is garbage, ai11's `+0x8d98` is 0). It then calls the
**original** 0054cec0 so the original reaches the installed ports. Variants: (1) the drawn LCG value's 0x10000 bit clear
(only 00508460; the live listener value), (2) bit set + the `+0x38c` target 0.5 (the gesture starts), (3) bit set + target 0.0
(no gesture), (4) bit set + the ground normal's y forced to 0.5 below `0x009cd99c` (body-up copy only, no RNG, no gesture).

| world (frame 1200) | 00508460 | 0061ea90 |
|---|---|---|
| National_NAT_Concord | **4 calls / 4 verified / 0 mismatches** | **2 / 2 / 0** |
| National_NAT_Concord_ai11 | **4 / 4 / 0** | **2 / 2 / 0** |

### Product rides (12 riders, AI 95, `RX_FULL_FRAME=1`, 1/60 s, 12000 steps requested)

Both rides pass the previous stop and continue. A/B on the live tree with **only** the registration removed
(`builds/groundnormal/ab/`, a private core copy): the W ride stops at the first fidget call, `unported 00508460` at step
10626 (race 9508), and with 00508460 registered but **0061ea90's** registration removed it stops at `unported 0061ea90` at the
same step 10626 (the RNG bit is set at this first fidget). With both ports (`ride2/ride`, built on the final tree,
`builds/groundnormal/logs/*_final.*`):

- **W held: 10639 steps / session frame 10648, race frame 9521, game 177.517 s**; max 83.9374 (word `42a7dff7`, frame
  1536), gear 4, path 5432.820, place 12/12, 0 laps, state hash `f8093269c9b87d69` - the fidget completes (the idle clock
  resets) and the ride continues 13 frames.
- **Clean input: 10747 steps / session frame 10756, race frame 9629, game 179.317 s**; max 75.4390 (word `4296e0c4`, frame
  1356), gear 3, path 3751.095, place 12/12, state hash `f5e09583c2fb8bd4`.
- **Next stop: `0063c040`** in both rides - `AttachableParticleEffect_ParticleEmitter::vf3` (198 B), the rider's lens-effect
  object at `rider + 0x5b6dc` (constructed by 0063bff0, vtable `0x00958908`) that the started gesture attaches to a bone
  (`+0x100` = the node in 0061eac0). That is the **fx / particles unit**, not an idle-rider function: not ported here.
- `rx_api_test` (`RX_FULL_FRAME=1`, 3000 race frames): runs to `destroy`, exit 0.

### No regression

`verify.sh builds/groundnormal/cli/reflex_boot <group> quick` (final CLI): rx_race **48 ports / 0 mismatches / On identical**,
rx_loop **23 / 0 / identical**, rx_world **132 / 0 / identical** (state hash `1f6611fa80b80831`, memory
`1487a52f208b4b26`); `core/build.sh` clean. The loop count moved 20 -> 22 -> 23 during this task from other units' ports
(not this one: rx_loop gains no function here).

## flagbehavior (2026-10-21, resumed verification)

`race_beh1_small.c` ports `00522a70 BehaviorBlueYellowFlag_Move`, used by vf4 `005225c0` state 1: steering ±0.75,
throttle −0.2, and the 45-unit / 0.707 heading reset. The earlier probe `f` verified **7/7/0** leaf calls plus
**1/1/0** composed vf4, with **78/78 original instructions** (`builds/flagbehavior/logs/probe_f3.log`, `cov_f.bin`).

`race_beh1_progress.c` ports the stuck-driver recovery callees: race-type `00524050`, steer-away `00523a70`,
back-up `00523ca0`, side selection `00523ed0`, and type-6 proximity `005240d0`. The vf4 `00523570` uses direct calls at
all six original call sites via `B1_CALL_SP`; reset vf6 `005234a0` uses the shared race-type port. Back-up initializes
writable globals `0xdb4698 = 0.407` and `0xdb4694 = 20.0`, guarded by bits 0/1 in `0xdb469c`.

The resumed probe `g` uses `[driver+0x1c]` when `[behaviour+0x5c]` is initially null, holding a `Poke` for the whole probe.
It also sets saved-position Y to vehicle Y and holds the type-0 flag across its geometry cases. Without those two fixes,
nominal 3/12/20-unit test points had a stale Y offset or used type 1, hiding the type-0 heading test. Correct geometry
exposed two mismatches: `00523c7e` compares **0.707 > dot**, so a sideways type-0 recovery finishes and an aligned one
continues. The C comparison now matches the instruction. A third mismatch was the back-up steering NaN payload:
the original's `fld m32; fstp m32` before SqrtF converts it to `0x7fffffff`; the port now performs that conversion with
`rx_f80_from_f32` / `rx_f80_to_f32` before sqrt. SSE operation order remains the original's.

Final per-call Verify on `National_NAT_Concord_ai11`, frame 300 of 400, in
`builds/sol_flag_verify/logs/probe_g_final.log`:

| Original | Calls / verified / mismatches |
|---|---|
| `00524050` race type | **4 / 4 / 0** |
| `00523a70` steer away | **11 / 11 / 0** |
| `00523ca0` back up | **6 / 6 / 0** |
| `00523ed0` side | **5 / 5 / 0** |
| `005240d0` near | **3 / 3 / 0** |
| `00523570` composed vf4 | **5 / 5 / 0** |

The five composed cases cover both primary and both secondary manoeuvre stages plus a stuck type-6 driver taking the
near/side-selection path, exercising all six switched sites. Leaf cases include distance thresholds, opposite/zero/NaN
headings, both side results, the lazy globals, and near/far/non-type-6 proximity.

Original-only coverage (ports disabled except the frame probe) is in `builds/sol_flag_verify/logs/cov_g.bin` and
`coverage.txt`: **132/132** steer-away instructions, **127/128** back-up, **100/100** side selection,
**14/23** race-type, **45/54** near. The back-up exception `00523e46` is unreachable: it clamps `abs(dot)` to zero only
if zero is strictly greater, which cannot hold even for NaN. The nine omitted instructions in each type function are
exclusively the kind ≥ `0x1f` series-manager path (`005b2370` / `_atexit` / `005b6a30`), outside MX scope. Those calls remain
fatal if reached in the product. All instructions reachable within the tested non-series scope execute.

No regression: the resumed CLI's Concord quick verifies pass for **rx_race 48 ports**, **rx_loop 26**, and
**rx_world 132**, all **0 mismatches** and **On identical** (state `1f6611fa80b80831`, memory `1487a52f208b4b26`). Logs
are `builds/sol_flag_verify/logs/{race,loop,world}_quick.log`. Product rebuild and the current-tree 24,000-step ride are
reported by the integrating ride-stop task, which owns `0085c8b0`; this verification task does not duplicate that port.

Focused probe `g` **On mode also matches the original-only run exactly**: state `c97ba22478e04bb6`, memory
`19e9077c0a759773` over 27,786 pages, frame 400 (`logs/probe_g_on.log` versus `logs/cov_g.bin.log`).
