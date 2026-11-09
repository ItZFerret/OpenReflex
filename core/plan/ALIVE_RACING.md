# MX vs ATV Alive tracks: racing with AI

Branch `w-aliveai` (2026-11-02). Goal: the converted Alive levels (`core/plan/ALIVE_TRACKS.md`, `_re/formats/ALIVE_CONVERSION.md`)
race with Reflex's AI, not only Free Ride. Everything here is a **PORT ADDITION**: Reflex tracks' AI, grids and events are unchanged.

**Status:** all **22** race-capable Alive levels (16 Nationals incl. JS7 and Quarry, 4 Short Tracks, 2 Supercross) convert, install and
race headless with 11 AI riders to the finish with standings (results below). They are listed in the front end (RACE > MX vs ATV Alive).
The 4 Free Ride-only levels (FR_Dunes, FR_Quarry, FR_DLC_JSComp, FR_DLC_Hills) have no race data in Alive. Free Ride on all 26 is
unchanged (byte-identical Base / FreeRide scene sections, the strict native regression identical to map-a5 on today's core).

**w-alivegates (2026-11-03):** the start gate is drawn, holds the riders with a collision wall and drops with Alive's own animation at GO,
in sync with the race start and with Reflex's DROP_GATE sound; the races ride at their **default MAP SCALE** (Nationals and short tracks
110 %, Supercross 130 %, shown as 100 %) and post to PRACTICE / the leaderboards there. See "7. Start gate" and "Scale".

**w-secondrow (2026-11-03):** riders past the gate's 12 slots line up in rows behind it (`GRID_ROWS.md`; "8. Second row"): online
sessions on Alive races take up to 16 riders. Offline nothing changes (the core races 12).

Overlay: `builds/alive/combined/map-a6-race` (and `builds/alive/<level>/map-a6-race` over `builds/alive/<level>/conversion-a6`), over
`builds/alive/base-clean` (see "Base" under Verification).

## 1. What a Reflex race needs from a level, and where the core reads it

| Need | Reflex data | Read by |
|---|---|---|
| Racing line, node graph, sections, forks | `<level>-<suffix>` **.tdf07** asset (`tdf`; suffix 0x009c54cc: `nat`, `sx`, ...): sections (+0x130) with next / previous lists, nodes (0x500 bytes): centre / left / right edge points, length, cumulative length | 00536ef0 `rx_track_load` (build_activity_track.c) into the activity's +0x890 block; NavigateTrack / TakeLine / SplitSection / AvoidTrackBoundaries (race_beh1_nav.c, race_beh2_line.c), forks 0050a360 |
| Per-class node data (row = vehicle factory +0x194: 0 MX, 1 ATV, ...; column = engine class +0x198) | node grids (8 x 4 dwords): +0x68 flags, +0xe8 line speed (-1 none), +0x168 AI channel, +0x2e8 / +0x368 / +0x3e8 lean-recovery triple, +0x468 steering throttle scale, +0x4e8 | race_driver_update.c (throttle / brake / lean / button from flags 0x8, 0x20, 0x800, 0x8000, the line speed), race_driver_ai.c (+0x468), race_beh1_pace.c (+0xe8, flags 0x4000 / 4), race_beh1_small.c / race_braketurn.c (corners: 0x200), race_beh2_seperate.c (0x2000 cancel), race_rules.c (0x1000 air, 0x10000 / 0x20000 shortcut markers: off-track and shortcut penalties), race_activity.c (respawn: 0x2), race_freeride_respawn.c |
| Section speed limits | the section grids +0x18 / +0x98 and the nodes' +0xe8 line speeds | as above |
| Start / finish, lap counting, checkpoints | the tdf: the start record (tag 8, block +0x10: centre snapped to the terrain, direction), records 9 / 0xa / 0x23 / 0x2b, the five (section, node) markers; the lap is the section loop back over the start / finish | race_section.c, race_section_driver.c (progress, laps, places), race_finish.c |
| Start grid | the scene entity named **"TO_StartingGate"** (Reflex: a CollisionEntity) whose model has **GateStart_01 .. GateStart_16**; the start zones from the grid pass (0075f890: a random first zone, a step prime to 16) | 005666d0 / 005663b0 (build_activity_grid.c), 0055b610 (build_activity_driver.c: a rider's slot) |
| Gate drop | DropGate entity (`DropGateRSData`: the bar, `Editor_Default` collision, swung 0.53 rad in 7 ticks on "Countdown Step" 0, its collision off then) | the race start; the riders are held by the start itself (they creep about 1.2 ft before GO, gate or not) |
| Lap pacing targets per tier | `<level>-lappacing` BXML (Reflex: AIData.dx9.database): LapPacing / `<Type>0` / `Atv`, `MotocrossBike` / `MXEngineClass_<size>` / NormalTime, Pro, Hard, Normal, Easy | 00556720 `rx_activity_lap_pacing` (activity +0xbc4 / +0xe44), LapPacing behaviour (race_beh1_pace.c: catch-up / slow-down, 005256d0), the driver reset (race_driver_reset.c +0x10bc) |
| AI skill / personality | tables in the executable's .data (tiers, rows, skill tables 0x9e3ff8 / 0x9e43b8 / 0x9e4478) | race_ai_params.c, race_driver_update.c |
| Track boundaries / resets | the terrain zone material (value 7: reset reason 6), the tdf (Offtrack, respawn point 00569010 from the driver's node), scene Trigger entities | phys / race_activity.c / race_rules.c |
| The race's own scenery | the activity subscene `<level>Exhibition<Type>` (00555e60), loaded only by that race; optional (Reflex nationals have none) | 007564f0 (build_activity_race.c) |
| The event | the front end's EventTypeLocations row; the core takes any `rx_event_spec` (location id -1 works: the custom tracks and Alive Free Ride use it) | build_activity_event.c |

## 2. What Alive's race data is

Every race level has exactly one race activity resource `<level><Activity>0` (`activity`, BXML): `National0` (12 disc Nationals, JS7 A / B,
Quarry A / B), `ShortTrack0` (4 short tracks, "Rhythm Racing"), `Supercross0` (SX01 / SX02). Its configuration names the script
(`National.script`, ...), `tdfassetname`, `startzonesetup StartGateModel`, `startzoneassignment Random`, `maxplayerssupported 12`,
the intro / podium cutscene states, plus a cause-and-effect library (post effects for wrecks). NAT_Southwest* / NAT_Tropic* leave
`tdfassetname` empty: the level's only `tdf` is the race's. Alive's race-only subscenes (`National0` ...) hold only a
RaceActivityAudioManager: the race data is not in them.

| Alive | Per track type | Converts to |
|---|---|---|
| **Track definition** `tdf` (`<level>-nat`, `-sx`, `-st`; also `NAT_Northwest`, `NAT_NorthwestB`, `NAT_NorthwestC-nat`, ST_Southwest's `NAT_Southwest-st`) | National: track type 0, 160 .. 365 nodes, 2 958 .. 8 940 ft; ShortTrack: type 0, 45 .. 156 nodes, 524 .. 1 096 ft; Supercross: type 1, 166 / 225 nodes, ~1 750 ft | Reflex's .tdf07: **the same tag stream**. Only the asset's size word is big endian (`alive_race.convert_tdf`); installed as `<level>-nat` / `<level>-sx` (a short track races as a National) |
| tdf differences | all 22: per-class grids have **2 rows** (MX, ATV) instead of 8; node tags 0x1b / 0x21 / 0x22 / 0x28 / **0x2e** absent; Alive adds node grids 0x32 .. 0x37, **0x3a .. 0x3f** (triples like Reflex's 0x2f / 0x30 / 0x31: a percentage, a minimum and a maximum speed, on about a third of the nodes), node word 0x38 (an editor id), block word 0x39; flags reuse Reflex's bits but **never set 0x8 / 0x800** (Reflex's jump lean bias, ~1/3 of Concord's nodes) and add 0x40000 / 0x80000 / 0x100000 | Reflex's parser reads it as is: rows 2 .. 7 stay 0 (only MX / ATV race), absent fields stay 0 ("none" for every reader: +0x468 0 = the default steering scale), unknown tags are skipped by length. `walk_tdf` checks every stream ends exactly where 00536ef0 ends |
| **Start gate**: "TO_StartingGate", a ScenarioAnimEntity (Reflex's class 0x104f too), model `GL_TO_StartGate` (JS7: `FR_JSComp_StartGate`) with **12** GateStart nodes and the bar on node `DropGate` (JS7 `FR_JSComp_DropGate`); celib `GateDrop`: on "InProgress" entered it animates the gate with `GL_TO_StartGate_Drop` and plays `UI_02/UI/GATE_DROP`; the gate has no collision of its own (`Gate_Bit`, a CollisionRSData in Base, is the floor plates under the riders) | all 22 (12 riders) | the entity, unchanged, in the new race subscene `<level>ExhibitionNational` / `ExhibitionSupercross` (alive_install). Core: a 12-node gate is the grid (below), shown, given its bar's collision and dropped (7. Start gate) |
| **Lap pacing** (same tree as Reflex; engines `MX125cc`, `MX250cc`, `MX450f`, `ATV250f`, `ATV450f`) | AIData.xenon.database: the 12 disc Nationals; the level's own: Quarry A / B (named `NAT_Quarry_A/B-lappacing`), SX01; **none**: JS7 A / B, SX02, the 4 short tracks | Reflex's schema (`alive_race.convert_lap_pacing`): engines renamed by displacement, `MXEngineClass_125cc` / `250cc`, 450 -> `500cc` (Reflex's open slot); installed as `<level>-lappacing` in the level's Base package |
| AIAbility / AIAbilityOverride / AIBehavior (AIData) | global | not used: Reflex's AI keeps its own tables |
| Start / finish | the tdf records (start, finish); `NAT_TR_Gate_Finish`, finish flags: scenery | as is |
| Reset triggers (PlayerTrigger / Trigger: `ResetTrigger_BehindGate` ...) | most levels | still deferred (Free Ride had them deferred too); the terrain's reset material works |
| Intro / podium / lose scenarios (NatIntro, NatPodium, ScenarioAnim actors, `*_AF_*` BXML) | all | not converted: Reflex's own race flow runs |

## 3. Gaps and what was added

1. **Gate with 12 slots** (core, build_activity_grid.c `alive_gate_pack`): when GateStart_13 is missing (no Reflex gate: all have 16;
   the original then takes the start-line path, unported) the 12 slots are the grid (+0x880 12), and the start zones the grid pass spread
   over 0 .. 15 are ranked onto 0 .. 11 in their order (every rider a gate of his own). Before the ranking two riders shared a gate.
   (w-secondrow: now `grid_rows.c`, any gate of 1 .. 15 slots, riders past them behind it: 8. Second row.)
2. **Size word** of the tdf, **lap pacing schema**, **installed names** and the **race subscene**: converter / installer (Python + C#,
   byte parity).
3. **Front end**: `ReflexEvents.Build` adds a race per Alive level whose registry entry carries `race` (the original's first event of the
   type lends rules, classes, durations; location -1; `aliveRace`), listed in RACE under the **MX vs ATV Alive** filter
   (`OpenShell.Browser`, `OpenEvent.Alive`).
4. **Not ported / known differences**
   - **AI jump data.** Alive moved its AI's jump hints out of Reflex's node flags 0x8 / 0x800 (lean bias) into new triples
     (0x3a .. 0x3f; 0x2f .. 0x31 too on a few nodes). Reflex's AI reads neither the new tags nor flags Alive never sets, so it rides
     Alive jumps without lean hints: more crashes on the jump-heavy tracks (Farm_B, Quarry_B, Northwest_C, JS7 B; see results).
     Mapping Alive's triples onto Reflex's lean bias is the main AI-quality item (plan step 2).
   - **No lap pacing** on JS7 A / B, SX02 and the short tracks: the activity's time table stays 0, so the LapPacing behaviour's needed
     speed has no finite target (read from the code, not measured: the riders count as behind pace, the catch-up tuning on, as at ALL
     TIME on Reflex's tracks). They race and finish; the pacing per difficulty is not authored there (plan step 3).
   - ~~The gate does not drop~~: done in w-alivegates (7. Start gate).
   - Intro / podium cutscenes, crowd reactions, Alive's race audio manager, reset triggers: not converted.
   - Only MX and ATV classes have per-class data (Alive has no trucks / buggies / UTVs). The template classes the front end offers
     are the National / Supercross ones (MX, MX Lite, ATV).
   - Online: an Alive race is an ordinary event to the host picker; not tested online (the clients need the same import).

## 4. Results (headless, `core/tools/alive_race_test.c`, 11 AI + idle player, 3 laps, MX class 2)

`builds/alive/combined/map-a6-race`, AI 95 (AMATEUR) and 120 (ALL TIME). Every race: 11 / 11 finished, standings complete, no stuck
rider (longest continuous stall under 5 ft/s after the start in seconds). Crashes = rising edges of the vehicle's wreck state per rider.

| Level | Type | Track ft | Pacing Pro / Normal (MX 250cc) | AI 95: mean best lap s | winner s | crashes / rider | longest stall s | AI 120: mean best lap s | winner s | crashes / rider | longest stall s |
|---|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|
| NAT_DLC_JSComp_A | National | 5400 | none | 109.8 | 311.8 | 4.55 | 2.6 | 105.2 | 299.7 | 2.82 | 1.8 |
| NAT_DLC_JSComp_B | National | 8940 | none | 177.3 | 514.4 | 7.91 | 2.0 | 168.4 | 494.1 | 3.73 | 3.3 |
| NAT_DLC_Quarry_A | National | 4459 | 79 / 99 | 108.7 | 310.2 | 4.00 | 1.7 | 102.8 | 297.9 | 2.64 | 3.5 |
| NAT_DLC_Quarry_B | National | 5246 | 95 / 115 | 127.7 | 396.7 | 6.27 | 5.7 | 123.9 | 381.8 | 6.00 | 5.0 |
| NAT_Farm | National | 4319 | 75 / 95 | 102.9 | 301.4 | 3.18 | 1.9 | 98.8 | 293.2 | 1.64 | 1.2 |
| NAT_Farm_B | National | 5223 | 88 / 108 | 116.1 | 335.5 | 7.09 | 3.5 | 109.2 | 320.0 | 5.45 | 3.4 |
| NAT_Farm_C | National | 4810 | 85 / 105 | 116.7 | 346.4 | 5.45 | 4.9 | 109.7 | 316.6 | 2.27 | 2.1 |
| NAT_Northwest | National | 2958 | 39 / 59 | 58.9 | 169.4 | 2.27 | 1.9 | 53.4 | 157.9 | 1.27 | 0.8 |
| NAT_Northwest_B | National | 4776 | 79 / 99 | 107.3 | 307.3 | 6.27 | 3.4 | 101.7 | 295.3 | 3.82 | 2.1 |
| NAT_Northwest_C | National | 5821 | 106 / 126 | 137.2 | 393.6 | 7.00 | 3.5 | 132.9 | 387.8 | 5.82 | 2.6 |
| NAT_Southwest | National | 4196 | 58 / 78 | 82.2 | 238.1 | 4.45 | 1.8 | 77.1 | 221.1 | 2.73 | 1.8 |
| NAT_Southwest_B | National | 5096 | 76 / 96 | 103.7 | 300.2 | 5.55 | 3.6 | 95.9 | 283.8 | 3.55 | 1.1 |
| NAT_Southwest_C | National | 4545 | 75 / 95 | 100.0 | 288.2 | 6.00 | 1.8 | 96.1 | 285.7 | 5.27 | 3.4 |
| NAT_Tropic | National | 3858 | 46 / 66 | 64.2 | 186.9 | 2.36 | 2.5 | 60.7 | 178.4 | 0.91 | 0.5 |
| NAT_Tropic_B | National | 4424 | 60 / 80 | 88.3 | 245.2 | 5.36 | 3.4 | 82.4 | 231.3 | 4.18 | 1.3 |
| NAT_Tropic_C | National | 5921 | 100 / 120 | 128.0 | 360.4 | 4.91 | 3.4 | 124.7 | 359.6 | 4.00 | 2.5 |
| ST_Farm | Short track | 782 | none | 25.6 | 68.1 | 1.45 | 6.9 | 23.7 | 69.7 | 1.18 | 3.5 |
| ST_Northwest | Short track | 524 | none | 16.4 | 49.1 | 0.91 | 1.6 | 16.3 | 46.0 | 0.64 | 2.1 |
| ST_Southwest | Short track | 873 | none | 21.1 | 57.1 | 1.36 | 3.3 | 19.8 | 54.0 | 1.18 | 2.5 |
| ST_Tropic | Short track | 1096 | none | 26.7 | 73.9 | 1.45 | 3.5 | 26.0 | 72.6 | 0.73 | 1.1 |
| SX_DLC_SX01 | SX | 1742 | 36 / 46 | 51.7 | 143.5 | 3.73 | 4.0 | 50.2 | 142.6 | 2.64 | 3.6 |
| SX_DLC_SX02 | SX | 1766 | none | 59.3 | 167.3 | 5.09 | 5.1 | 53.2 | 159.5 | 3.09 | 4.8 |

Summary: 44 / 44 races finished by all 11 AI; the longest stall anywhere 6.9 s (ST_Farm, a pile-up after the gate). Crashes per rider over
3 laps: Nationals 2.3 .. 7.9 at AI 95 (median 5.4), 0.9 .. 6.0 at 120 (median 3.6); short tracks 0.6 .. 1.5; Supercross 2.6 .. 5.1. The
jump-heavy tracks crash most (Farm_B, Quarry_B, Northwest_C, JS7 B): see "AI jump data".

Reflex reference, the same tool and settings (AI 95): NAT_Concord best lap 70.5 s, 2.27 crashes / rider; NAT_Bedford 103.1 s, 3.09;
SX_Kingston 135.0 s, 6.18 crashes and one rider stuck 197 s (the known Kingston hang, IDEAS.md LEGEND).

The AI's best laps sit near Alive's own "Normal" pacing targets (NAT_Farm 103 vs 95, NAT_Tropic 66 vs 66, NAT_Northwest 60 vs 59),
a sign the lines and the pacing data mean what they meant in Alive. MX Lite (class 3), ATV (class 1) and SX01 on MX Lite also
race to the finish (NAT_Farm 2 laps, measured before the gate ranking fix: MX Lite 107.9 s mean best; ATV 145.0 s with 7.8 crashes per
rider: the ATV rows of Alive's data are thinner, worth a playtest).

## 5. Remaining plan (rough effort)

| Step | What | Effort |
|---|---|---|
| 1 | Playtest the 22 races in the player (menus, gate, HUD, results, replay); fix what shows up (gate model drawn as an ambient actor, the race camera, HUD names) | 0.5 - 1 day |
| 2 | **AI jump data**: decode Alive's 0x3a .. 0x3f triples (and 0x2f .. 0x31 use) against the tracks' jumps, map them onto Reflex's lean bias flags 0x8 / 0x800 / the +0x2e8 triple at conversion (a converter step, no core change), re-measure crashes | 1 - 2 days |
| 3 | Lap pacing for the 7 levels without it: derive from track length and the Alive nationals' ft/s per tier (or the sibling track's), as a converter step | 0.5 day |
| 4 | ~~The gate drop~~: done (w-alivegates, 7. Start gate) | - |
| 5 | Reset triggers (PlayerTrigger / Trigger) in the race subscene, if the playtest shows riders escaping the track | 0.5 day |
| 6 | Alive's intro / podium scenarios (optional, cosmetic) | 2+ days |
| 7 | Online races on Alive levels (both sides imported; the event key is "National/<level>") | 0.5 day test |

Everything converter-side must keep the C# importer byte-identical (tests/alive_tracks/importer_parity) and `test_alive_formats.py` green.

## 6. Verification (2026-11-02)

- `test_alive_formats.py` 23 tests (4 new `RaceTests`: the tdf stream on synthetic and all 22 real definitions, the lap pacing schema,
  the race subscene leaving Base / FreeRide unchanged); `test_sliderock_convert.py` 13.
- Importer parity (.NET 8): 12 levels byte-identical (NAT_Farm, SX_DLC_SX01, ST_Farm, FR_Dunes, NAT_Southwest, NAT_Tropic_C,
  NAT_DLC_Quarry_A, NAT_DLC_JSComp_A, SX_DLC_SX02, NAT_Northwest, ST_Southwest, FR_DLC_Hills); the player's Mono: NAT_Tropic, SX_DLC_SX01.
- conversion-a6 vs conversion-a5: every converted resource identical apart from the new tdf; installed Base / FreeRide scene sections
  byte-identical on all 26.
- Strict native Free Ride regression on map-a6-race: 26 / 26 PASS, identical to map-a5 on the same core (three Northwest levels moved
  slightly against the recorded `regression-a5` from main's core changes since 2026-10-28; map-a5 on today's core gives the same).
- `core/build.sh` clean, census 0 UNPORTED, sigcheck 0 findings; unity_compile.sh osx / win 0 errors. No player was run (no windowed
  players: the user's standing request).
- **Base:** `builds/sol_garage/game files` now holds the user's own 26 imported Alive levels (the in-client importer, 2026-10-31), so
  alive_install refuses it. `builds/alive/base-clean` is a symlink view of it without them (every file the alive-import manifests list);
  nothing of the base changed.

## 7. Start gate (w-alivegates, 2026-11-03; core/game/alive_gate.c, PORT ADDITION)

**What Alive does.** Its gate is one ScenarioAnimEntity, "TO_StartingGate" (model `GL_TO_StartGate`, JS7 `FR_JSComp_StartGate`): 12
GateStart nodes and the bar, a plate leaning back over the front wheels (x -32.4 .. 32.3, from (y 0.25, z 0.1) at its hinge up to (y 2.0,
z -3.0); the slots are at z -7.25 facing +z), on its node `DropGate` (JS7 `FR_JSComp_DropGate`). The cause-and-effect library `GateDrop`
plays `GL_TO_StartGate_Drop` on it and the sound `UI_02/UI/GATE_DROP` as the race enters InProgress. The animation (track format, keys
(time, u16 angle in pi / 4096, axis)) turns the bar about the gate's -x: 0 deg for 2 frames, 1.2, 4.9, 11.2, 20.0, **31.2 deg at 0.2 s**, a
bounce to 22 deg, 28.9 deg at its end (0.4 s, held): the same drop as Reflex's DropGate (0.53 rad = 30.4 deg in 7 ticks, a bounce, 12
ticks). JS7's own `FR_JSComp_StartingGate_Drop` is the same on its node (its celib names `GL_TO_StartGate_Drop`, which has no node there).
Alive's gate has no collision (its `Gate_Bit` is the floor plates); Reflex's DropGate collides only through `Editor_Default` (a small box
list at its hinge). In both games the race's start holds the riders: they creep about 1.2 ft before GO, gate or no gate.

**Here** (only for a ScenarioAnimEntity named TO_StartingGate: no Reflex level has one; Reflex levels are unchanged):
- **Drawn:** the grid pass that finds the 12-slot gate (`build_activity_grid.c` 005663b0) attaches it: shown at its model's pose (the bar
  up; a scenario entity is hidden until a scenario plays it), drawn by the client from the core's node matrices (OriginalAmbient), at the
  world's MAP SCALE about its place (`OriginalAmbient.AliveStartGate`). The race intro's scenario takes every ScenarioAnimEntity as its cast
  and stops (hides) them all as the Countdown begins (anim_scenario_intro.c entity_stop): the gate is shown again there (the first renders
  had the bar missing until GO); on main the gate was hidden from the Countdown on.
- **Blocks:** a wall of static collision the width of the gate, z -2.2 .. -1.2, y 0.25 .. 2.05 in the gate's frame (x MAP SCALE), in the
  level's physics world (a kind 11 collis made in memory; its owner the level's Gate_Bit CollisionEntity, for the contacts' audio material).
  A box along the overhanging plate, or a wall reaching back to its top edge (z -3.0), overlapped the player's front tyre (the player is
  placed ~1.9 ft further up than the AI) and threw the rider 50 ft at GO; at z -2.2 a front tyre stops square against it where it meets the
  plate.
- **Drops in sync:** as Activity_SetState enters InProgress (the frame Reflex posts "Countdown Step" 0, both the board's start and the
  network countdown), Alive's animation plays on the gate (a cut from its first key = the up pose, not looping: it holds its last key). The
  collision goes off when the bar is down (the 0.2 s key, 12 frames, counted in the gate's update: it stops while paused; Reflex clears its
  own at its down angle). Start states as Reflex's 006bdbe0: a gated race starts closed; a free ride, mode 0 / 6 or a session result open.
- **Sound:** Reflex's gate sound is an activity cue (`ACTIVITY_NATIONALS` / `ACTIVITY_SUPERCROSS` `/ACTIVITY/DROP_GATE`) from a Reflex level's
  ActivityAudioManager entity; an Alive level has none, so its races had no cue at all. The drop records Reflex's cue for the host
  (`rx_sound_cue_text`), the same path the client plays Reflex's from. (The other activity cues, HOLESHOT_WON, the finish ..., are still
  missing on Alive races: no ActivityAudioManager.)
- **Gate selection:** Reflex offers the 3D Gate Selection page (camera, cards, arrow) only in the MotoCareer (Heat / Moto); Alive races are
  arcade / online events, so it does not apply. Online's gate pick (a 2D list) now offers an Alive race's 12 gates (`OnlineHost.EventGates`
  = `AliveLevels.RaceGates`), the host's rider count is capped at 12 there, and picked zones 0 .. 11 are kept by the grid (alive_gate_pack
  used to rank them: gate 6 alone became gate 1). (w-secondrow: the cap is gone, riders 13 .. 16 pick places behind the gate: 8.)

**Evidence (headless, `core/tools/alive_gate_test.c`, 12 riders, the player at full throttle from the first frame, at the default scales):**
all 22 race levels: the bar up and shown (ambient flags visible, 0 frames hidden before GO); the riders' furthest creep before GO 1.19 .. 1.32 ft (main without
the gate: 1.18 on NAT_Farm, Concord 1.19); GO at frame 368, the bar past 1 deg at GO + 4 frames (JS7 + 2), 28.9 deg at the end; the collision
off at GO + 13 frames; Reflex's DROP_GATE cue at GO; 12 / 12 riders 30 ft past their slot 6 s after GO. The player's start matches main to
the foot (NAT_Farm 1.0: 283.5 ft in 6 s, both). Holding the collision on for 3 s after GO (RX_GATE_HOLD=180): NAT_Farm 11 riders stay at
1.9 .. 3.0 ft (one AI reset itself out after pressing the wall for seconds), SX_DLC_SX01 at 130 % all 12 at 3.5 .. 4.0 ft, JS7 A at 110 %
all 12 at 2.2 .. 2.9 ft; with the collision off (RX_GATE_OPEN=1) the same 3 s take them 40 .. 113 ft. Full AI races (alive_race_test, 3
laps, AI 95): NAT_Farm 1.0 identical to the table above (winner 301.4 s, 3.18 crashes per rider); at the defaults NAT_Farm 110 % 11 / 11
(winner 312.8 s, best lap 109.0 s, 3.55 crashes), ST_Farm 110 % 11 / 11 (71.1 s, 1.82), JS7 A 110 % 11 / 11 (338.3 s, 5.45), SX_DLC_SX01
130 % 11 / 11 (156.5 s, 4.00).

**Renders** (`tests/alivegates/run.py`, the lighting showcase player in -batchmode, muted, at the defaults; REFLEX-lead
`builds/alivegates/runs/gates3/`, `sheet.png`: rows NAT_Farm 110 %, SX_DLC_SX01 130 %, NAT_DLC_JSComp_A 110 %; columns the Countdown from in
front and along the bar, 0.1 s after GO, 0.45 s, 0.6 s): the bar up and leaning over the front wheels, dropping, then flat under the
front wheels as the riders roll over it; the gate is drawn at the track's scale, the 12 riders in its 12 slots. The player logs
`[OriginalAmbient] the Alive start gate ...` (its model, nodes, materials, drawn / hidden).

**No re-import.** Everything is in the core and the client: an import with race data (the importer or alive_install since 2026-11-02) gets
the gate; one from before has no race at all (as before).

## 8. Second row (w-secondrow, 2026-11-03; core/game/grid_rows.c, PORT ADDITION; the general logic: `GRID_ROWS.md`)

`alive_gate_pack` became `grid_rows.c`'s zone packing (the same for up to 12 riders: a 12-rider race is byte-identical to before); riders
past the 12 slots take places behind the gate (row 2: 11 places behind the gaps, from the middle out). The space behind Alive's gates is
not empty, so each place is searched for a clear spot (measured on all 22 race levels at their default scales):
- **Farm, Farm_B, Farm_C:** a 1.2 ft rail runs across 11 .. 13 ft behind the slots; bikes straddling it at the plain depth (10.2 ft) were
  thrown 60 .. 110 ft into the air when the start let go, riders placed behind it stuck on it. The row stands in front of it (7.2 .. 8.2 ft,
  between the gate's bikes). The middle lane (under the start tower) has no clear spot: last in the row.
- **Northwest A / B / C, Southwest A / B / C:** the gate stands on a 1.27 ft pad that ends ~7.6 ft behind the slots. The row stands behind
  its edge with 2 ft of run-up (13.2 .. 13.5 ft); the AI ride up the edge (at 12 ft one rider stalled 3 s against it, at 13.5 none).
- **Tropic A / B / C, Southwest, SX01, SX02:** a post / a tower's leg in the double middle gap: the middle place moves 2.9 .. 3.8 ft across.
- **JS7 A / B:** the starter's dog house (JS_RedBull_DogHouse, one-sided collision, open to the front) fills the middle gap: no clear spot
  there, the row's last lane.
- Quarry, short tracks, the other levels: the plain place (10.2 ft; SX 10.5 ft at 130 %).

The bar: the wall in front of the gate's riders (7. Start gate) is off 13 frames after GO, before a row-2 rider reaches it (~1 s); none hit
it in the runs below. Results (`GRID_ROWS.md` "Measured"): 7 AI on the gate + 4 behind it, 3 laps: all 11 finished on NAT_Farm,
NAT_Northwest, NAT_Tropic, ST_Farm, SX_DLC_SX01, JS7 A; longest stall 4.1 s; row-2 riders 132 .. 168 ft from their places 6 s after GO.
Online: an Alive session takes up to 16 riders; riders 13 .. 16 pick "ROW 2 · n". Offline Alive races stay at 12 riders (11 AI): the core's
cap (w-ridercap `RIDER_CAP.md`), not the gate.

## Decisions (the user, 2026-11-02)
- Alive short tracks race as Nationals (Reflex has no short-track event type): accepted.
- Alive races stay allowed online (the host's event picker). Leaderboards / PRACTICE on Alive tracks: not decided yet.
- (2026-11-03) Default MAP SCALE: Nationals 110 %, Supercross 130 %, shown as 100 %; at it the races post to PRACTICE / the leaderboards
  ("these post to the leaderboards"); the board names the default so a change later doesn't mix runs. Short tracks: 110 % (they race as
  Nationals; nothing found against it: ST_Farm races 11 / 11 at 110 %, 1.8 crashes per rider). The base is the level's: its Free Ride rides
  at it too (one saved value per track; the user confirmed it, 2026-11-03). The leaderboard Worker with migration 0002 is live (2026-11-03).

## Scale (measured 2026-11-02; tools and raw runs were in $TMPDIR/alivescale, not kept)
- Both games model people, bikes and props at the same scale (MX wheelbase 4.79 ft in both, gate slot pitch 5.0 ft in both). The
  converter's 1:1 is right for the world.
- Alive's layouts are tighter: track width 0.72x Reflex on Nationals (39.5 vs 54.6 ft median), 0.66x on Supercross (31 vs 47);
  SX features 0.72-0.8x. Alive's mx450f differs in 42 of 100 shared keys: peak torque 0.72x, top speed 98 vs 108 ft/s, airborne drag
  about 10x (a ballistic model: the Alive bike flies 9-18 % shorter at the same takeoff speed; Alive's use of the curve unverified).
- MAP SCALE sweep (12 tracks x 0.80-1.40, 11 AI at 95, 3 laps): Reflex controls best at 1.0; Alive tracks best at about 1.0-1.2,
  worse below 0.9 (landing overshoots). The jump-heavy Alive Nationals stay at about 7 crashes per rider at every scale: the missing
  AI jump hints, not scale. Lap times rise 3-7 s per lap per +0.1.
- Recommendation (low-moderate confidence): Nationals and short tracks 110 %, Supercross 115 %. Alive's lap-pace targets favour 1.0.
- **Taken (the user, 2026-11-03): Nationals and short tracks 110 %, Supercross 130 %, as the tracks' defaults** (`MAP_SCALE.md` "Base scale":
  shown as 100 %, the records and the leaderboards at it, online the same on every rider's game).
- Seen in passing: Reflex's SX_FresnoB aborted the core on load in the headless tool (signal 6 at frame 2, location id -1); not investigated.
