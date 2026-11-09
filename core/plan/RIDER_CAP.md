# More riders in a race (20+, up to 32): what it would take

**Status (w-rc-core, 2026-11-03): the core races up to 32 riders** (below, "Done"); offline FIELD SIZE (w-rc-client) and online sessions of
32 (w-rc-online, protocol 3) are done too. The investigation that follows is w-ridercap's (the
places, the costs, the plan). Read with IDEAS.md "Starter routine at the gate" and GRID_ROWS.md (w-secondrow: riders past the gate's slots
line up behind it; merged, and the rows now reach 32).

## Done (w-rc-core): the core at 32 riders

**Design: entries 0..11 stay where the original has them; riders 13..32 live in side storage** (`core/game/rider_cap.h` / `.c`). The
side storage is fixed arena memory outside the original's image and heap (0x0e000000 .. 0x0f2fffff: free in the original's process, fresh
per session like the rest of the arena). Nothing reads or writes it while an event has 12 or fewer riders, so such an event is the
original's byte for byte (proof below: whole-arena hashes). Values that mean "empty" in the original (-1 seat indices) are stored
complemented, so the zero-filled arena reads as empty without a write.

| What | The original | Riders 13..32 (rider_cap.h) |
|---|---|---|
| Player seats (MXGame + 0x7460, 12 x 0x12a0) + trailer (+0xdf80 used, +0xdf8c order list, +0xe010 leave queue) | 12 | `rx_seat` / `rx_seat_order` / `rx_seat_used` / `rx_seat_leave`: seats 12..31 at 0x0e000000; every inline `base + 0x12a0 * U32(base, 0xdf8c + 4 i)` and the 00552ad0 / 00552b90 / 00552c60 slot functions go through them |
| Standings (activity + 0x2d0, 12) | 12 | `rx_stand` / `rx_stand_set` (every standings read / write in game/ and api/: sorts, gaps, HUD, results, finish, podium, clip recorder, career) |
| Race index flags (activity + 0x304, 12), start slots (+0x340, 28) | 12 / 28 | per-activity side record (`rx_rc_act`): flags 12..31, start slots 28..31 (`rx_start_slot`; grid_rows.c's SLOT, RX_GRID_SLOTS 32) |
| The activity's driver arrays (+0x268 / +0x27c, made {12, step 0}) | the 13th push writes past the block (0055bf0e: growth 0) | grown by 20 (port addition; never reached at <= 12) |
| Rider animation worker slots (.data 0xa9408c / 0xa940bc / 0xa940ec / 0xa9411c, .bss 0xaf6b50 / 0xaf6b80 / 0xaf6c08 / 0xaf6c38) | 12 (the 13th got slot -1 and overwrote the next table) | `rx_rc_tab` side tables; job contexts RX_JOB_CTX_RIDER2 (96..115), loop_platform's TLS keys 64 |
| Scenario cast (world + 0x20150: the intro / podium RiderCharacters, 12 x 0x30 at +0x50) | 12 (the 13th record overwrote +0x290 / +0x294) | the cast's side record (`rx_cast_rec`): every cast loop (find, stop, pause, release, place, podium create / scalars / placement, intro cameras) |
| Highlight recorder: the sample buffer [0xa7f124] (13 sections x 12 riders x 150), the field snapshots (12 x 0x4c), the places list (.data 0xa7f3a8, 12) | 12 (rider 12 wrote into clip section 1; the 13th place overwrote the sample sections) | side sample buffer at 0x0f000000 with record indices from 0x5b68 up (`rx_clip_index` / `rx_clip_record`: snapshot, copy, replay pose / follow), side snapshot entries (`rx_clip_entry`), places in a side table |
| AI air-leveling table (.data 0x9e3f68, 12) | 12 | side table (AI 13+ get the landing prediction too) |
| Mud records (0xa96d90, 12 x 0x40), vehicle particle effect sets (0xc11e90, 12 x 0x17c30), damage behaviours (0xdb1410, 12 x 0x410: four-wheel classes) | 12 (13+ without) | side tables, constructed on first use (the effect sets as 0090a170 builds them, the damage entries with 004798e0); loops over "all of them" visit the side entries only once taken (`rx_rc_tab_n`); claimed only while the event has 13+ players (`rx_rc_more`: the Player List's rows), so an extra vehicle of a 12-rider event stays without, as in the original |
| Engine sound slots + the start line (host side, vehicle_audio.c / engine_startline.c) | 12 | 32 (`RX_SL_SLOTS`); slots 12.. only with 13+ players. The start line's game-RNG draws for slots 0..11 are unchanged |
| The audio logic's remembered places (+0x5c), the Eliminator ordinal's standings scan | 12 | side record / 32 |
| Online remote riders (net_remote.c) | 0..11, ids 1..11, zones 16, latch 16 | 0..31, ids 1..31, zones / latch 32, start zones -1..31 |
| The grid pass's step (0075f890: round(16 / rows)) | 33+ rows divide by zero | step 0 -> 1 (port addition; not reached at <= 32 rows) |

**API (rx_api.h).** `RX_MAX_RIDERS 32`, `RX_ORIGINAL_RIDERS 12`, `rx_session_max_riders()`. An exhibition takes `rx_event_spec.riders` up to
32 as is (above 32 is refused). Every slot-taking call accepts 0..31 (`rx_session_slots` walks 32 seats: before it walked 16 seats of a 12-seat
block; the pose / vehicle info / node flags / mud / podium / party / results slot lookups use `rx_seat_of`). Versioned exports with more
rows, the old version still served: `rx_results_page` v2 (32 rows, 4008 bytes), `rx_sound_state` v2 and `rx_vehicle_audio_state` v2 (riders
16..31 in a tail after the version-1 layout). Unchanged: `RX_MODE_RESULTS_ROWS` 12 (Freestyle / Section Race pages), `RX_PARTY_MAX_RIDERS`
12 (the party modes stay at 12). The C# side: `RxApiNative` (MaxRiders, rx_session_max_riders / set_field_size / field_size), `RxResults`,
`RxSound`, `RxVehicleAudio` mirrors moved to the new versions.

**Opt-in: FIELD SIZE.** `rx_session_set_field_size(s, n)`: 0 (default) = the game's MaxRidersEventType / MaxRidersVehicleClass tables (12
for every type and class: the career's and the shell's field sizes, the original's); 13..32 = those limits answer n
(`career_single_capacity.c`). Per session. The exhibition path needs no setting (the client picks `spec.riders`). The Alive level scripts'
`maxplayerssupported 12` is read by the client (AliveRace.cs), not by the core: the client lifts it for its FIELD SIZE choice.
**How the client turns it on:** a FIELD SIZE option (12 = the original) -> `spec.riders = n` for a quick race; for career / shell events
`RxApiNative.rx_session_set_field_size(session, n)` after create, before the event or round loads; online
`rx_session_set_remote_riders(s, n - 1)` with `spec.riders = n` and the start zones for n rows.

**Not done / limits** (riders 13..32): the scripted pose overrides (0xad81d0, 12: the intro's start-line poses for the first 12 only), the
podium's crew-chief attachment pairs (sm + 0x1734, 12) and the rider sound banks of the audio boundary (0xd92f00, graceful: no bank);
the party modes, Freestyle, Section Race and Waypoint keep their own 12-entry tables (not opened past 12); the Supercross AI at 32 in a
one-line rhythm section piles up (a playtest question, below). The client side (HUD / results / roster / drawing): w-rc-client, below;
the online stream: w-rc-online.

**Checks** (w-rc-core; Apple M5 shared with other agents' jobs, everything at nice 20, two runs at a time):

- **12 or fewer riders are the original's:** `core/tools/ridercap_identity_check.sh <out> <install> [ref]` builds this core and `ref`'s
  (default main), runs a 12-rider National (NAT_Concord), Supercross (SX_Kingston), UTV Powercross (PX_Laguna) and an 8-rider National
  through `jobthreads_hash` (RX_JOB_THREADS=1, the whole arena hashed every 600 of 4200 race frames: image incl. the side storage, heap,
  DLL, stack) and compares: **PASS against main (ba4f305a): nat, sx, px, nat8 identical, 7 hashes each**. Also: the bench's
  12-rider pose + RNG hashes equal the ones printed before the change (NAT_Concord 62b237cd749b961f).
- **Races to the finish** (`core/tools/ridercap_race_test.c`: 3 laps, MX 250, AI 95, the player idle at the gate and then given its
  laps once every computer rider finished, so the race ends by its own rule; the exhibition replay presenter served; `RX_HEAP_REDZONE=32`:
  every heap block followed by a guard checked every 600 frames). Every row: every computer rider finished, the standings hold every
  rider once in finishing order, every player slot has a vehicle, the race reached Post, the results page has a row per rider (distinct
  slots), the highlight recorder saved its 12 clips with the field snapshots of riders 13+ in side storage and every rider's samples in
  the ring, no heap block written past its end:

| Event | Riders | Finished | Winner s (its slot) | Median s | Last AI s | Results rows | Side snapshot entries | Redzone |
|---|---:|---:|---|---:|---:|---:|---:|---:|
| National NAT_Concord | 16 | 15 / 15 | 200.6 (8) | 215.0 | 241.1 | 16 | 48 | 0 |
| National NAT_Concord | 24 | 23 / 23 | 202.4 (12) | 229.1 | 246.8 | 24 | 144 | 0 |
| National NAT_Concord | 32 | 31 / 31 | 207.9 (20) | 230.3 | 274.1 | 32 | 240 | 0 |
| Supercross SX_Kingston | 16 | 15 / 15 | 361.9 (9) | 396.6 | 888.3 (one AI stuck ~5 min) | 16 | 48 | 0 |
| Supercross SX_Kingston | 24 | 23 / 23 | 369.8 (8) | 416.1 | 584.8 | 24 | 144 | 0 |
| Supercross SX_Kingston | 32 | 30 / 31 (one AI stuck on lap 2 for good) | 390.4 | 429.6 | 701.7 | - (no Post: the stuck AI) | 240 | 0 |
| Alive NAT_DLC_Quarry_A (110 %, 12-slot gate) | 16 | 15 / 15 | 330.2 (3) | 351.5 | 390.9 | 16 | 48 | 0 |
| Alive NAT_DLC_Quarry_A | 24 | 23 / 23 | 344.8 (1) | 363.4 | 404.2 | 24 | 144 | 0 |
| Alive NAT_DLC_Quarry_A | 32 | 31 / 31 | 334.2 (28) | 372.0 | 422.0 | 32 | 240 | 0 |

  The 12-rider reference at NAT_Concord on main: winner 200.6 s (16-rider run above: the same 12 seats race as before plus 4 behind the
  gate). SX_Kingston at 16: one AI was stuck for ~5 minutes (888 s; the known Kingston stuck-rider case at 12, IDEAS.md LEGEND), at 24 the
  last AI 585 s; at 32 one back-row AI (slot 30) stayed on its second lap for the rest of the 30-minute run, so that race never reached
  Post: Supercross packs at 24+ pile up more, as predicted (a playtest / per-track maximum question, not a cap bug: no stop, no heap
  write past a block). The Nationals and the Alive National at 32: every rider home within 66 / 88 s of the winner.
- **Determinism:** the 32-rider National run twice prints the same RESULT hash (standings, times, the game RNG) three times (fd1c085429012abf); the bench's pose + RNG
  hash is the same in RX_JOB_THREADS=0 and the default job threads at 12 and 32 riders. The highlight recorder's places list matches every
  rider's place 5 s into the race at 16 and 32 (0 / 0 off; after 7 s the original notes only the local human's, so the computer riders'
  entries keep their 7 s places: the "at the end" figures are expected).
- **Cost** (`ridercap_bench`, 1800 frames from GO, medians of the runs below, ms per 60 Hz frame): the machine was loaded by other agents (a 12-rider National cost 18.0 ms on one thread vs
  3.24 ms in "Measured" below), so the ratio 32 / 12 is the figure; scaled onto "Measured"'s 12-rider numbers:

  | | 12 riders | 32 riders | 32 / 12 | 32 scaled to "Measured" (predicted there) |
  |---|---:|---:|---:|---|
  | National, 1 thread (race) | 18.03 | 31.72 | 1.76 | 5.7 ms (6.3-6.5) |
  | National, game (job threads) | 6.72 | 13.55 | 2.02 | 2.9 ms (2.6-2.9) |
  | Supercross, 1 thread (race) | 22.73 | 37.04 | 1.63 | 8.4 ms (8.5 .. 13.7) |
  | Supercross, game | 9.48 | 17.12 | 1.81 | 3.8 ms (3.0-3.2) |

  About 0.12 (National) / 0.16 (Supercross) ms of one-thread CPU per rider past 12; no n^2 blow-up at 32. The crowded start (first 300
  frames after GO) costs a third more than the race at 32 (National 18.4 vs 13.6 ms with job threads): 32 riders within a few lengths.
- C# `tests/online/unity_compile.sh` osx + win: 0 errors. `sigcheck.py`: 0 findings. The changed game files compile in the oracle
  configuration (-DRX_ORACLE, syntax).

## Done (w-rc-client): the client at 32 riders (offline)

**What players see.** The race setup (RACE > a track > its setup page, EVENT tab, THE FIELD) has a **FIELD SIZE** row under AI OPPONENTS:
`12 RIDERS · ORIGINAL` by default, left / right one rider at a time from 2 to 32 (it wraps), remembered with the event's setup; the
summary's FIELD chip and the note under the row follow ("You and 23 computer riders ..."). Nothing changes unless a player picks more than
12. The row is greyed:
- with AI OPPONENTS off (`YOU ALONE`);
- on the modes whose own tables the core keeps at 12: **Waypoint, Freestyle, Section Race and the party modes** (`12 RIDERS · THIS MODE'S
  MOST`; a bigger saved choice is clamped to 12 there). Practice is you alone as before.

Supercross above 24 riders gets a warning in the note (the one-bike-wide rhythm sections pile up; w-rc-core's 32-rider Kingston race left
one back-row AI stuck), not a limit. The Alive races are Nationals / Supercross, so they take the field too: the client never limited an
offline Alive race by `maxplayerssupported 12` (only the online gate pick reads it, `AliveLevels.RaceGates`). The **championship weekend**
(w-champweekend) takes the same FIELD SIZE: its setup page's FIELD tab has the `OpenFieldSize.Row` on the event setup's own `riders`, and
every stage launches with it (`OpenSetup.LaunchRiders`, no 12 clamp since); the Supercross heat / LCQ / main split scales with the field
(`rx_champ_weekend_split`, the table in `rx_champ_weekend.h`: 12 riders = the original's 5 / 6-11 / top 2).

**Where it lives.**
- `OpenFrontEnd/OpenFieldSize.cs`: the reusable piece: `Cap(e)` (32 for the lap races, 12 for the modes above), `Clamp`, `Crowded`
  (Supercross > 24), `Value` / `Note`, and `Row(parent, event, aiOn, get, set)`, the spinner + note any page can add.
- `OpenSetup.riders` (saved), `Riders(e)` (clamped), `LaunchRiders(e)` (a weekend's stages too); `OpenShell.Setup.cs` adds the row;
  `OpenContent.Launch` / `OpenEventPrestart` hand it to `ReflexEventSelect.Launch(..., riders)` -> `ReflexWorldFiles.OpponentsFor` (up to 31
  AI) -> `OriginalWorldRuntime.OpenEventSpec` (`spec.riders` up to 32). `REFLEX_RIDERS=<n>` does the same for a diagnostic launch.
- `OpenReflexSimulation`: an exhibition takes `spec.riders` as is, clamped to `rx_session_max_riders` (a core from before w-rc-core
  loads 12); the startup waits for the riders actually loaded. `RxRiderCap.SetField` wraps `rx_session_set_field_size` for a career /
  championship field later (not called today: no offline career offers a field).
- `RxRiderCap` (Max 32 / Original 12 from `RxApiNative`) sizes every C# rider array: the roster, the startup / pacing slot reads, HUD
  standings, mud, podium, the results policy / snapshot, `Drivers`, CareerWorker, the legacy `OriginalSimulation`. The legacy plugin's
  world stays at 11 AI (its game has 12 seats).

**Results.** Big fields are offline exhibitions, so they end on our own results page (`OwnRaceResults`): every row in its scroll view,
rows packed tighter past 12 (`ou-results-big`), the local rider's row centred once the board is laid out. The original's post-race page
(career only today) pages through a bigger field 12 rows at a time (up / down, the wheel, the legend's `RIDERS 13-24`).

**Drawing a big field** (`OriginalOpponents`, only with 13+ riders; a field of 12 is drawn exactly as before):
- the riders' skinned meshes are culled off screen (bounds about the root bone instead of `updateWhenOffscreen`): no skinning for the
  rows behind the camera;
- riders farther than 220 ft drop their small bike parts (meshes under 1.2 ft), the grip IK and the per-frame vehicle shader constants
  (the bike's renderers are collected once instead of searched every frame);
- the near / far flags go to the simulation thread only when one changed (no array copy a frame);
- sound: only the 11 computer riders nearest the listener run an engine (the original's 12 emitters), with hysteresis; a culled engine
  restarts cleanly when it comes near (`OriginalVehicleSound.CullEngines`, `OriginalEngineAudio`).
- `REFLEX_BIGFIELD_LOD=0` (tests) draws a big field in full, for the A / B below.
Not done: shared meshes / materials across riders (each rider still builds its own; it would save load time and memory, not frame time,
and the per-rider mud needs its own material) and the models' LOD1/2 (LOD0 only).

**Other places checked past 12 / 16:** the HUD's standings board (a 5-row window around you: fine at 32), the position counter, the
replay recorder (a dynamic actor list; at 32 riders 53.5 KB a frame, 120 s kept in 376 MB at 60 Hz, inside its budget), the replay
cameras (any actor), the podium / intro cast arrays (32). The Free Ride pause map draws 12 rider arrows (Free Ride only, online: the
online branch's). The original intro start-line poses, the podium crew chiefs and the rider sound banks stay at 12 in the core (riders
13+ go without).

**Checks (w-rc-client).**
- C# `tests/online/unity_compile.sh` osx + win: 0 errors. `core/build.sh`: built. `tests/online/build.sh`: 601 / 601 passed (two earlier
  runs failed one or two tests that also fail on main or depend on the machine's speed: the chat filter's 2000-line timing and the track
  folder name; this branch has no online changes). The core is main's (no core change), so 12-rider events are the original's byte for
  byte by w-rc-core's identity check; `ridercap_identity_check.sh` was not rerun.
- **Races in the real player** (`tests/perf_profile/bench.py`, batch players: no window, no focus, silent, scratch save; the gate, the
  start and 20 s of live race): National NAT_Stillwater, Supercross SX_Kingston and Alive National NAT_DLC_Quarry_A (110 %, map-a7) at
  16, 24 and 32 riders: every field built (15 / 23 / 31 computer riders), the core's grid put 16 on the 16-slot gate and the rest behind it
  ("start grid: 32 riders, a gate of 16 slots: 16 place(s) behind it"), no exception in any player log. The replay recorder took 33 actors.
  Runs: `builds/perf_profile/bench/*-{national,supercross,alive}-{16,24,32}` and `*-L-*`.
- **Menus showcase** (`tests/menus_showcase`, page `ridercap`, batch): the row's values / wrap / greying / notes and a 32-row results page
  with the 19th place centred: 0 failed, three runs in a row. Shots: `builds/rc_client/shots/ridercap-*.png`.
- **Cost** (the bench's `*_field` scenes: the camera ahead of the gate, the whole field in view; Apple M5, 16 GB; the machine was shared
  with four other agents, **load average 7-15** during these runs (an earlier series at load 20-65 was thrown away); every Unity player /
  build ran alone under the machine-wide lock; National: two interleaved rounds, the median; ms):

  | | 12 riders (as before) | 24 | 32 | 32 drawn in full (`REFLEX_BIGFIELD_LOD=0`) |
  |---|---:|---:|---:|---:|
  | National, the gate frame rendered (whole field in view) | 13.9 | 14.8 | 9.2 | 21.1 |
  | Supercross, the gate frame rendered | 16.8 | 8.2 | 7.3 | - |

  The big-field rules more than pay for the extra riders at the gate: drawn the old way, 32 riders cost about 7 ms more than 12; with them,
  24 and 32 cost no more than today's 12. At the gate every rider is within 220 ft, so the far-rider rules do not apply there: the difference
  (~0.4 ms of render CPU a rider) is most likely the skinned meshes' bounds (`updateWhenOffscreen`, on for every rider of a field of 12 as
  before, recomputes each rider's bounds from its bones every frame) plus the grip IK; not isolated further. Fixed bounds for 12-rider
  fields too might save ~4 ms there (a playtest decision; not done, so a field of 12 is untouched).
  Not measurable here: the presented frame time of a windowed player (a batch player presents no frames; its main thread sat at 80-100 ms
  at every field size, the batch players' known slowness) and a clean core step (the simulation job swung 18-27 ms at every size with the
  load). For the simulation use w-rc-core's numbers above (x2.0 the game-thread core step at 32 on a quiet M5, ~+1.5 ms). A Windows
  playtest at 12 / 24 / 32 is the real check (testers already run 30-40 fps at 12).

## Done (w-rc-online): online sessions of up to 32 riders

**What hosts and players see.** ONLINE > HOST, the host page: **MAX RIDERS** goes from 2 to **32** (a party mode still 12 at most) and
says about how much upload the host's connection needs for that field (`8  ·  ~1 MBIT/S UP`, `12  ·  ~2.5`, `24  ·  ~6`, `32  ·  ~8.5 MBIT/S UP`: the worst case,
everyone in one pack). The page's text says riders past the gate line up behind it. In the lobby the board scrolls through every rider;
with **GATE PICK** on, each rider in turn picks a place: the gate's slots (16 on a Reflex track, 12 on an Alive race) and then the places
behind it, `ROW 2 · 1` .. `ROW 3 · n` (w-secondrow's numbering, now to 32 places; the chips get smaller past 16). A rider who does not
pick gets the best free place. In the race every other rider is a real rider of your own core (collisions, positions, results) up to 31
of them, drawn as its own rider and vehicle (its look) **posed as your core poses its copy** (before: posed from its own skeleton stream).
The Free Ride pause page's map draws an arrow for every rider (it had 12). A protocol 2 game (any build before this one) cannot join: it
is told "Version mismatch: this session uses online protocol 3 and your game uses protocol 2 ... Update OPEN REFLEX to join." and the
session browser marks such sessions `ANOTHER VERSION OF OPEN REFLEX` (refused cleanly, no compatibility kept: one build rides together
anyway, `OnlineProtocol.SameBuild`).

**The stream (protocol 3).**
- Every rider sends its **core record** 30 times a second (132-byte "ORR1", unchanged). The host relays them **batched** ("ORB1",
  `OnlineRecord.EncodeBatch`: 10 records a datagram, 107 bytes each: the record's floats as they are (the vec4s' w lanes too, which the
  core's remote filter carries through its corrections), only the sender's record time left out (each receiver restamps it), flags and
  wreck fields as bytes; malformed batches are dropped whole).
- **Tiers:** each rider gets its **11 nearest riders within 400 units at 20 Hz**, every other rider at 4 Hz (`OnlineHost.NearSlots`).
  A rider's relay runs once per 50 ms (its due records together: full batches), staggered by rider, the targets in a turning order each
  tick (a full shared send queue, EOS's 96 KB, would always have dropped the same last riders). Records go before skeleton frames in
  each rider's 128 KiB/s budget. Between a far rider's records its copy runs on the core's own prediction (the sender's inputs).
- **Skeleton poses only on request:** a rider says at the join and in `skeletons {ids}` which riders it draws from their skeleton poses
  (`OnlineClient.NeedSkeletons`); the host tells each rider `skeleton {send}` whether anyone wants its poses, and a rider sends them only
  then. The game asks only for riders without a copy in its core (a Free Ride rider not yet spawned or refused by the core, phase 1's
  ghosts); normally nobody sends any. `REFLEX_ONLINE_SKELETONS=1` / `-reflexOnlineSkeletons 1` draws every rider from its skeleton poses as
  protocol 2 did (the look's A / B; the bandwidth of protocol 2). A client that never says (the headless bots, the tests) gets every one.
- **The roster:** a READY / loaded / finished change sends that rider's card (and the id list) to everyone: 2 messages a rider, not N + 1
  (protocol 2: 33 x 32 messages per READY at 32). A join sends the joiner every card and its card to the others; a leave the id list. A
  round's start / end and a rules change still send every card (once per round).
- **The relay without LINQ:** one sort of the sources per rider per relay turn, reused lists, no per-tick list of peers.
- **Code:** `Online/Core/OnlineProtocol.cs` (Version 3, MaxPlayers 32, MaxGridPlaces 32, `HostUploadBytes` / `HostUploadMbit`),
  `OnlineRecord.cs` (the batch), `OnlineHost.cs` (Relay, TellSkeletons, the roster), `OnlineClient.cs` (batches, NeedSkeletons,
  SkeletonWanted); `Online/OnlineService.cs` (`ArmCoreRiders` up to 31 others, 11 in a party mode; `SkeletonLook`), `Online/OnlineRide.cs`
  (the copies' whole poses kept from each view, the puppets posed from them, the skeleton requests, no pose built while nobody wants it,
  records kept going during the held start), `OpenReflex/RxOnline.cs` (31 remote riders, 32 slots read; clamped to the loaded core's
  `rx_session_max_riders` - 1 at the arming), `OriginalWorld/OriginalOpponents.cs` (a hidden online copy's rider skeleton is evaluated every
  frame when near the camera, as a drawn opponent's), `OriginalWorld/OriginalPauseMenu.cs` (arrows 13..32 made from the first),
  `OpenFrontEnd/OpenShell.Online.cs` + `OuOnline.uss` (MAX RIDERS' upload, the big gate pick). The unwired `OnlineRecordTrack` of the
  stopped WIP was removed (smoothing far riders by dead reckoning would replace the core's own physics prediction; a playtest question).

**Bandwidth** (`tests/online/bandwidth.sh <out>`: the loopback relay of a real host, every rider sending its record and its skeleton pose
30 a second as OnlineRide does; payload bytes, "wire" adds 28 bytes of IPv4 / UDP a datagram; EOS's own framing not counted; KB = 1000 B;
pack = riders 12 units apart, spread = 120 apart; "before" = protocol 2 (497f8ef5) with its cap lifted to 32 in the copy):

| Riders | Host upload before (pack) | Host upload now, pack (wire) | Host upload now, spread (wire) | A rider's download now (pack / spread) | A rider's upload now | Host page's estimate |
|---:|---|---|---|---|---|---|
| 12 | 11.6 Mbit/s (12.4) | 2.13 Mbit/s (2.23) | 1.24 (1.30) | 24 / 14 KB/s | 3.3 KB/s | 2.2 |
| 16 | 15.8 Mbit/s (17.0) | 3.11 Mbit/s (3.24) | 1.94 (2.02) | 26 / 16 KB/s | 3.3 KB/s | 3.2 |
| 24 | 24.2 Mbit/s (26.8) | 5.39 Mbit/s (5.62) | 3.69 (3.84) | 29 / 20 KB/s | 3.3 KB/s | 5.6 |
| 32 | 32.6 Mbit/s (36.6) | 8.13 Mbit/s (8.44) | 5.90 (6.12) | 33 / 24 KB/s | 3.3 KB/s | 8.4 |

Before, every rider's download was capped by the 128 KiB/s budget (131 KB/s), so at 32 in a pack some riders got **no** records (0.0/s
for the farthest); now every rider gets its 11 nearest at 20.0 Hz and the rest at 4.0 Hz at every size, in either layout. A rider's upload
falls from 37 KB/s (record + skeleton) to 3.3 KB/s (record only). With `REFLEX_ONLINE_SKELETONS=1` the host's upload is protocol 2's again
(32.7 Mbit/s at 32) but every rider still gets every record at its tier's rate (records first). The host's reliable traffic stays small
(11 KB/s at 32: the phase every 2 s, the standings). EOS's shared unreliable queue (96 KB, `EosNet.UnreliableBacklog`): at 32 the host sends
about 17 KB per 60 Hz frame, well under it (protocol 2 sent ~68 KB a frame at 32).

**Checks (w-rc-online).**
- `tests/online/build.sh`: 664 passed, 1 failed (the custom track's folder name `Farm 14 2-...` on this machine: fails on main too).
  New (`tests/online/RiderCapTests.cs`, 64 checks): protocol 3 and the refusal of a protocol 2 / newer game; the batch codec (lossless
  but the time, 10 a datagram, malformed and 3000 bit-flipped batches); **24 and 32 riders** over the loopback with uneven host frames
  (8 .. 20 ms): all welcomed, the 33rd refused as full, nobody asked for skeleton poses, one READY = 2 messages a rider, the gate pick
  with 24 / 32 places on a 16-slot gate (rows 2 and 3, every place distinct, the start zones 0 .. n-1 for the core), every rider getting
  every rider's records (10 nearest at 17-21/s, past the 12 nearest at 3-5/s), batches as full as the tiers allow, every datagram within
  1150 bytes, the host's measured upload within 20 % of the host page's estimate; skeleton requests (only the one asked sends, only the
  asker gets them; a client that never says gets all; malformed requests refused); the spread tiers. `ORX_ONLY=ridercap` runs only these.
- **LAN with 24 and 32 bots** (`ORX_ONLY=ridercap ORX_LAN_BIG=1 sh tests/online/build.sh <out>`: the LAN transport on real TCP + UDP of this
  computer, real time, the bots joining at the host's per-address join limit): all welcomed, every pair's records over UDP, 268 (24) / 300
  (32) records a second a rider = 11 x 20 + the rest x 4, nothing dropped. This run found a real bug: on uneven frames the near riders came
  at 10-14 Hz (a rider's interval check against a relay that runs on a 50 ms grid); fixed, and the loopback tests now tick unevenly.
- C# `tests/online/unity_compile.sh` osx + win: 0 errors.
- Not run here: a Unity player (no ride with copies drawn from the core was seen), EOS, two PCs, the real internet. See QUEUE.md
  w-rc-online for the user's checklist.

## The short answer

- **The real cap today is 12, not 16.** The original game was built for 12 riders, and its memory layout still says so: the game object
  holds exactly 12 rider "seats" back to back, the race keeps its standings in a 12-entry table, and several helpers (bike damage,
  engine sound, the replay/highlight recorder) have 12 places. Offline races are 12 (you + 11 AI). Online lobbies take 16 people, but
  the core only seats 12 of them (you + 11 others); riders 13-16 are drawn as puppets: no bumping, not in the core's HUD standings.
  Asking the core for 13 riders today stops the game at the first frame ("player lookup of an empty slot").
- **32 is doable but it is real surgery on the core**, not a constant to bump: about 360 places in about 80 source files read those
  12-place tables at fixed offsets. The safe way is to leave the first 12 places exactly where the original has them and put riders
  13-32 in new storage next to them, so a race of 12 or fewer is byte-for-byte the original.
- **The simulation can afford it.** Each extra rider costs the core about **0.15-0.18 ms of CPU** per frame on an M5 (0.05-0.07 ms of the
  game's frame, the animation jobs being spread over threads). Up to 12 the cost grows in a straight line (Supercross bends up a little).
  32 riders would add roughly 1-1.5 ms to the game's simulation frame on an M5, 3-4 ms of CPU in all.
- **Online is the hard part, and it is already over budget at 16.** The host relays every rider's full skeleton to every other rider; its
  per-rider budget only fits about 4 nearby riders at full rate today. 32 riders needs a leaner stream (physics records only) and probably
  a direct (mesh) connection layout, whatever happens to the core.
- **Recommendation:** a 2-3 day spike first (seats 13-24 in side storage, a headless 16- and 24-rider AI race) to find any blocker this
  read-through missed; then 24 riders offline; then 32 offline; online last, after its stream is fixed. A cheap win on the side: the
  original's own career **Heat format** (two heats of 12, an LCQ, a main event: 24 riders in one event) is already ported and could be
  offered in quick races and online with no core surgery.

## Where the limits live

Effort: **trivial** = a constant / clamp; **small** = under a day; **medium** = days; **large** = a week or more, touches the original's layout.

### Core (C, `core/`)

| Place | What limits it | At 13+ riders today | How hard |
|---|---|---|---|
| The game's rider seats: MXGame + 0x7460, **12 MXPlayer objects of 0x12a0 bytes inline**, then the seat table's trailer (+0xdf80 12 "used" bytes, +0xdf8c 12 active-seat indices, +0xdfbc the count, +0xe010 a 12-entry leave queue); everything after it in MXGame (vehicle list +0x154b0, garage, music ...) sits at fixed offsets (`build_mxgame.c`, `build_activity_player.c`) | the original's layout | the 13th seat lookup aborts at frame 0 (00552ba5) | **large**: ~100 references to the block (`0x7460`, the `0x12a0` stride) in 48 files plus ~130 to the trailer (`0xdf8c`, `0xdfbc`, `0x1541c`, `0x153ec` ...). Plan: an accessor, seats 0-11 where they are, seats 12-31 in new storage |
| The race's standings: activity + 0x2d0, **12 driver pointers inline** (+0x300 is the next field) | the original's layout | would overwrite the next fields | **large**: ~110 references in 47 files (standings sorts, HUD, results, AI place logic). Same overflow plan |
| Start grid: the gate has 16 slots (GateStart_01..16, Alive 12); the activity keeps **28 start slots** inline (+0x340, 0x30 each: 16 gate + 12 "human" zones); the grid pass steps zones `& 0xf` with a step prime to 16 and a 16-flag "zones taken" table (`build_activity_grid.c` 0075f890) | gates + layout | riders 17+ would share zones 0-15; **33+ riders divide by zero** (the step rounds to 0) | **medium**, mostly w-secondrow's job (places past the gate); the 28-slot table needs the same overflow for 29+ |
| Bike damage: the damage manager [0x00db1410] has **12 behaviours** (0x410 each) | layout | riders 13+ have no breakable parts (answers "none") | **medium** |
| Engine sound slots: the vehicle audio manager has **12 emitters**; each also binds the start-line words (`vehicle_audio.c`, `engine_startline.h` RX_SL_SLOTS 12) | layout | riders 13+ silent, no start-line state | **medium** (the start line draws the game's random generator, so it is gameplay: keep it exact at 12) |
| Highlight / end-of-race replay recorder: one ring of 150 samples x **12 riders** x 13 sections ([0x00a7f124], `build_rider_clip.c`), the places list at **.data 0x00a7f3a8 + 4 i** (12), clip entries 0x4c per rider inside the clip, `replay_pose.c` `i < 12` | layout + the exe's data section | rider 13 writes into the next section; the places list overwrites neighbouring globals | **medium**: a bigger ring for 13+ riders, the places list moved |
| Remote riders (online): `rx_session_set_remote_riders` 0..11, unique ids 1..11, `g_zones[16]`, wreck latch [16], start zones 0..27 / at most 16 (`net_remote.c`, `net_remote_session.inc`) | port code | refused | **small** once the seats exist |
| The 12 scripted sequences (activity + 0x1070 + 0x15190 i) | a pool of named track sequences (0055e100 takes the first free one), not one per rider | more riders trigger more of them at once; a full pool skips one (as in the original) | none |
| MaxRidersEventType / MaxRidersVehicleClass tables (12, `career_single_capacity.c`) | game data | the shell never asks for more | **small**: a port setting above 12, opt-in |
| AI roster (`MXUIManager_BuildAIRoster` 0059f3e0) | game data | enough: 101 AIRider rows have bikes of each machine type (`_re/notes/vehicles.md`); the career Heat format already fields 24 | none |
| AI behaviours: separation / avoidance look at every other driver (`race_beh2_seperate.c`, O(n^2)) | none | 32 x 31 distance checks a frame: negligible | none |
| Physics: bike-to-bike contacts go through the world's pair tests; no rider-count table found | none found | measured below: close to linear up to 12, the crowded start no dearer | check in the spike at 24 / 32 |
| Port API structs (shared with C#): `RX_SOUND_RIDERS` 16, `RX_VEHICLE_AUDIO_RIDERS` 16, `RX_RESULTS_MAX_ROWS` 16, `RX_MODE_RESULTS_ROWS` 12, `RX_PARTY_MAX_RIDERS` 12, `rx_session_slots` scans 16 seats, the podium 16 | port ABI | truncated past 16 | **small** each (a version bump, the C# mirror) |

Side note: `rx_session_slots` (and `results_session.inc`) walk 16 seats although the game has 12; seats 12-15 read the bytes after the
seat block (zero today, so nothing shows). Worth a one-line fix when the seats are touched.

### Unity client (`Unity/Reflex/Assets/Scripts`)

| Place | What limits it | How hard |
|---|---|---|
| `ReflexRoster.cs` throws past 16 seats; `OpenReflexSimulation.CaptureStartup` waits for `spec.riders` seats from `rx_session_slots(..,16)` (a 17+ rider event would never finish starting) | 16 | trivial |
| Offline setup: AI count 0..11 (`ReflexWorldFiles.cs` FullGateOpponents 11, `OpenShell.Setup.cs` "you and 11"), `OriginalWorldRuntime` Min(16, ..), `CareerWorker` Min(16, ..) | 12 / 16 | small (a FIELD SIZE choice) |
| HUD standings `ReflexHudModel` scratch[16], `MaxStandings = 16`, `Drivers[16]` | 16 | trivial |
| Results: the original's results page has 12 rows (`OriginalPostRace` ListItem0..11); the modern page refuses > 16 riders (`OwnRaceResultsPolicy`) | 12 / 16 | trivial (modern page) / medium (original page) |
| Podium / intro cast `OriginalPodium.MaxRiders` 16; mud `MaxMud` 17; legacy pause map 12 dots | 16 / 12 | trivial / medium |
| Rider drawing: a new Mesh per surface and new Materials per rider, LOD0 only, skinned even off screen (`ReflexRiderModel`, `ReflexBikeModel`); 69 bones posed per rider per frame | cost, not a cap | medium (shared meshes, LODs, culling) |
| Sound: `RxVehicleAudio`/`RxSound` mirror the 16-rider C structs; the core's 12 engine emitters | 16 / 12 | medium |
| Online: `OnlineProtocol.MaxPlayers = 16`, `MaxGates = 16`, `StartZones` 0..27; only 11 others become core riders (`OnlineService.ArmCoreRiders` Take(11)) | 16 / 12 | trivial (constants, protocol version) / large (see Online) |
| Online relay budget (`OnlineHost` ByteBudget 128 KiB/s per receiver), roster bursts O(N^2), LINQ in `Relay` every tick, EOS's shared 96 KB unreliable queue | bandwidth | medium-large |
| Replays (Unity's own `ReflexReplay`): fixed byte budget (128-512 MB), ~70 MB a minute at 12 riders | memory | small (shorter replays or 30 fps at 32) |
| EOS lobby / session size | about 64 members (community reports, not in Epic's docs) | none at 32 |

### The original game data

- **Gates.** Every Reflex race gate has 16 slots; Alive gates 12 (`maxplayerssupported 12` in their level scripts). 17-32 riders need
  the second row (w-secondrow). How far back a second row can go before it reaches a turn or a jump differs per track; Supercross start
  straights are short.
- **Racing lines.** The AI follows one race line per track with lateral offsets and avoidance; nothing in it counts riders. A 32-rider
  pack on a Supercross track (one-bike-wide rhythm sections) will pile up more: Kingston already has the known stuck-rider case at 12
  (IDEAS.md LEGEND). Expect more first-turn crashes and more "stuck" resets; it needs a playtest per track type, not code.
- **Event types.** MaxRidersEventType says 12 for every type; the career Heat format (24 riders in heats of 12) is the only place the
  original puts more than 12 riders in one event.

## Measured

`core/tools/ridercap_bench.c`: NAT_Concord (National) and SX_Kingston (Supercross), MX 250, AI 95, the player on corebench's pad
script. The event runs untimed to GO (the session leaves LOADING after a few frames, but the race sits in Pre and the countdown for
another 1050-1500 frames), then 1800 racing frames (30 s) are timed: the first 300 from GO ("start": the gate drop, the pack within a
few bike lengths) and the 1500 after ("race"). Apple M5 (4 performance + 6 efficiency cores) with other agents' jobs running, so
medians of 3 runs. **1 thread** = `RX_JOB_THREADS=0`: all of the core's work on one thread, the honest CPU cost. **Game** = the
product default: the animation jobs spread over worker threads, the wall time of a frame as the game's simulation thread sees it.
Milliseconds per 60 Hz frame:

| Riders | National 1 thread (start / race) | National game | Supercross 1 thread (start / race) | Supercross game |
|---|---|---|---|---|
| 1 | 1.56 / 1.51 | 0.69 | 3.25 / 2.94 | 1.72 |
| 2 | 1.75 / 1.84 | 1.09 | 3.46 / 3.06 | 1.42 |
| 4 | 2.27 / 2.30 | 0.95 | 3.54 / 3.39 | 1.52 |
| 6 | 2.38 / 2.48 | 1.13 | 3.88 / 3.76 | 1.71 |
| 8 | 2.71 / 2.73 | 1.22 | 4.47 / 4.02 | 1.86 |
| 10 | 3.00 / 3.05 | 1.35 | 4.91 / 4.26 | 1.90 |
| 12 | 3.38 / 3.24 | 1.43 | 5.16 / 5.13 | 2.12 |

- **Per extra rider** (straight-line fit, 1 to 12 riders): **0.15 ms (National) and 0.18 ms (Supercross) of CPU on one thread**;
  0.05-0.07 ms of the game's frame with the job threads (most of a rider's cost is its animation, which the jobs spread out; the bike
  integration itself runs one bike after another in each fixed step, QUEUE.md w-jobthreads).
- **Growth:** a quadratic fit gives nothing on the National (the curve bends the other way, within noise) and a small n^2 term on
  Supercross (+0.006-0.008 ms per rider squared on one thread): not visible at 12, but it could matter at 32 (below).
- **The crowded start costs the same as the rest of the race** at 12 riders (National 3.38 vs 3.24, Supercross 5.16 vs 5.13 ms): the
  rider-to-rider contacts in a tight pack are not where the time goes, at least not yet at 12.
- **Determinism:** every rider count printed one pose + RNG hash over its 3 runs and both thread modes.
- **Memory:** the bench's resident memory is 457 MB with 1 rider and 512 MB with 12: about 5 MB of core memory per rider.
- **13 or more riders:** the core stops at the load, frame 0: "00552ba5 player lookup of an empty slot" (the 12 seats).

**Extrapolated** (not measured: the core cannot run them yet):

| | 12 (measured) | 24 | 32 |
|---|---|---|---|
| National, 1 thread | 3.2-3.4 ms | 5.1-5.2 ms | 6.3-6.5 ms |
| Supercross, 1 thread (straight line .. with the n^2 term) | 5.1 ms | 7.0 .. 9.5 ms | 8.5 .. 13.7 ms |
| National, game (job threads) | 1.4 ms | 2.1-2.3 ms | 2.6-2.9 ms |
| Supercross, game (job threads) | 2.1 ms | 2.6-2.8 ms | 3.0-3.2 ms (more if the n^2 term is real) |

So 32 riders adds roughly 1-1.5 ms to the game's simulation frame on an M5 and 3-4 ms of CPU in all (up to ~9 ms of CPU on
Supercross if its curve is real). A tester's Windows PC is slower per thread than an M5 performance core: count 1.5-2x. The spike (phase 0)
measures the real thing.

**The client** (from earlier measurements, QUEUE.md): the main thread adds about 0.13 ms per rider on the M5 (draws, skinning; w-perf);
an online puppet costs about 1 ms of main thread on a Windows tester's machine (a full bike + 69-bone rider + grip IK + name tag);
an 11-AI National held 10.5-13.5 ms of main thread on an i9-10900 (w-latguard). Twenty more riders at 0.13 ms (M5) to perhaps 0.3 ms (slower PCs:
a guess) each is +2.6-6 ms of main thread: a 60 fps budget of 16.7 ms gets tight on the testers' machines, so the rider drawing needs the LOD / culling / shared-mesh
work before 32 is comfortable. Each tracked rider also costs ~8.5 KB of copies per frame between the core and Unity
(`OpenReflexSimulation`: the bike nodes and the 69 bone matrices; the whole export took ~2.9 ms per game frame at 12 riders, w-perf,
mostly the ambient, particle and scenery tables rather than the riders). The client was not measured here (no test player was launched).

## Online bandwidth

Today (`OnlinePose.cs`, `OnlineRecord.cs`, `OnlineHost.cs`): a star, every rider sends to the host, the host relays. Per rider and frame
of relay: an MX skeleton packet set 1346 B (64-byte header + 14 B per node, 87 nodes, 2 datagrams) + a physics record 132 B = 1478 B.
Riders send 30 a second; the host relays near riders (under 400 units) at 20 Hz and far ones at 4 Hz, within 128 KiB/s per receiver.

| | 16 riders | 24 riders | 32 riders |
|---|---|---|---|
| Wanted per receiver, all near (N-1) x 20 x 1478 B | 443 KB/s | 680 KB/s | 916 KB/s |
| Budget per receiver | 131 KB/s | 131 KB/s | 131 KB/s |
| Average update rate of each other rider (budget / (N-1) / 1478) | 5.9 Hz | 3.9 Hz | 2.9 Hz |
| Host upload (N-1) x budget | 1.97 MB/s (15.7 Mbit/s) | 3.0 MB/s (24 Mbit/s) | 4.1 MB/s (32.5 Mbit/s) |
| Host download (N-1) x 30 x 1478 B | 0.67 MB/s | 1.0 MB/s | 1.37 MB/s |

So at 16 the budget already gives only the ~4 nearest riders full rate and the host needs ~16 Mbit/s of upload; at 32 it needs about
33 Mbit/s up, beyond many home connections, and EOS drops datagrams once its shared 96 KB queue fills.

What 32 would need: send the **physics record only** (132 B; the core copies of the other riders already simulate and pose them; the
skeleton stream is only for the puppet look) and batch several riders' records in one datagram:

| Records only, 132 B | 32 riders |
|---|---|
| Star, all near at 20 Hz: per client down 31 x 20 x 132 B | 82 KB/s (0.65 Mbit/s) |
| Star, host upload | 2.5 MB/s (20 Mbit/s): still too much |
| Star with tiers (8 nearest at 20 Hz, 23 others at 4 Hz): per client / host upload | 33 KB/s / 1.0 MB/s (8 Mbit/s) |
| Mesh (every rider sends its own record to the 31 others, 20 Hz): each rider up and down | 82 KB/s (0.65 Mbit/s) |

The mesh removes the host's relay cost entirely (EOS P2P connects every pair, with its relay as fallback); the star with tiers is the
smaller change. Either way the client's look of a remote rider would come from its core copy instead of the skeleton stream (so remote
tricks and crashes look as the local simulation plays them), which is a design change to check in a playtest.

## Risks

- **Faithfulness.** A race of 12 or fewer riders must stay byte-for-byte the original. The overflow design gives that by construction:
  seats, standings, damage, audio and recorder entries 0-11 stay at the original's addresses, and the new storage is only touched for
  13+. Proof as for earlier layout work: corebench hashes, `jobthreads_check.sh` whole-arena hashes, the oracle on every touched
  function, the stop-audit matrix at 12 riders, before and after.
- **Gameplay RNG past 12.** The grid pass, the AI roster draw, each AI's personality draw and the start-line words all draw the game's
  random generator per rider: 13+ riders change the random sequence of the whole race (expected; it is a different event), but a seed
  shared online must give every machine the same draws: online races must agree on the field before the load (they do today).
- **Missed 12s.** About 360 fixed-offset references were found by searching for the offsets; a 12-sized loop over riders written
  differently (`< 0xc`, a constant pointer into .data like the recorder's places list) can hide. The spike is there to flush them out:
  run 16 / 24 / 32-rider AI races to the finish on every track type with the stop audit's development build (unported paths log
  instead of abort).
- **Performance.** Core +1-1.5 ms of frame (+3-4 ms of CPU) at 32 on an M5, 1.5-2x that on testers' PCs; main thread +3-6 ms before
  the rider drawing is slimmed down. Testers already sit at 30-40 fps on some machines (pacing, GC): the per-rider GC allocations (`RxOnline.RemoteSlots` clones, foley
  closures, LINQ in the relay) grow with the field and must go first.
- **Online bandwidth** (above): the current stream does not scale; a 24 or 32 lobby must ship with the new stream.
- **Track crowding.** Supercross and narrow Nationals may not race well with 32 AI; the field size may need a per-track maximum.

## Plan

Rough effort in working days of one agent; testing included, playtests not.

| Phase | What | Effort |
|---|---|---|
| 0. Spike | Throwaway branch: seats 12-23 and standings 12-23 in side storage, the 12-entry recorder / damage / audio tables grown, the 13+ zones remapped; a headless 16- and 24-rider AI race to the finish (`ridercap_bench`, `alive_race_test` with riders > 12) under the unported-paths logging build; list every remaining stop; measure the real per-rider cost and the crowded start | 2-3 days |
| 1. Core, 24 offline | The seat accessor (~230 references), standings accessor (~110), the grid's zones and 28-slot table past 16 (with w-secondrow), damage / engine audio / recorder / places for 13+, unique ids and API struct caps to 32 (one `RX_MAX_RIDERS`), a FIELD SIZE setting over the 12 of MaxRiders (opt-in, default 12), byte-for-byte proof at <= 12 | 2-3 weeks |
| 2. Client, 24 offline | The C# caps (roster, startup wait, HUD, results policy, podium, mud, interop structs), the setup's FIELD SIZE, a scrolling results page, rider drawing LOD / culling / shared meshes, the per-rider GC fixes; a Windows playtest | 1-1.5 weeks |
| 3. 32 offline | Same code at 32 once everything reads `RX_MAX_RIDERS`; the second row's depth; per-track maximum where a track cannot hold 32; playtest | 3-5 days |
| 4. Online 24 / 32 | Remote riders 11 -> 31 in the core (after phase 1), records-only stream with batching and distance tiers (or a mesh), the protocol version, the O(N^2) roster burst, the relay's LINQ, the lobby's MAX RIDERS and gate pick past 16 (second row), LAN harness runs with 24 / 32 bots, a real-internet test of the host's upload | 2-3 weeks |

**Total:** about 6-9 weeks for 32 riders everywhere; 24 offline (phases 0-2) in about 4 weeks.

## Cheaper alternatives

1. **Heats and a main event (24 riders, no core surgery).** The original's career Heat format (rx_career_heat.h: practice qualifier,
   two heats of 12, LCQ, a main event of the heats' top 5 + LCQ top 2) is ported (w-heat). Offering it as a quick-race / online format
   puts 24 riders in an event, 12 on track at a time. Online needs the host to split the lobby into heats and seat each heat's riders.
   About 1-1.5 weeks. Bigger fields (36, 48) the same way.
2. **See-through ghost riders past 12 (online).** Already half there: online riders 13-16 are puppets today. Raise the lobby to 24 / 32
   with riders 13+ as see-through ghosts (no contact, standings from the host, which tracks every rider's laps already). Needs the
   bandwidth work of phase 4 (the skeleton stream to 32 riders is the blocker), not the core work. About 1.5-2 weeks. Which 11 others are
   solid is fixed at the load (the core's rows); "the 11 nearest are solid" would need riders to move between core rows mid-race (the
   original's join / leave path exists for Free Ride only).
3. **Ghost AI offline from a second core.** The core runs one session per process (fixed global addresses), so 12 more AI would need a
   helper process running a second headless race on the same track, its riders drawn as ghosts. Costs a second core's CPU (~3.2-5.1 ms a
   frame on one thread at 12 riders here) and ~500 MB of memory; the two packs never interact. Possible, not recommended: phase 1 is
   the honest version.
