# MAP SCALE (2026-10-28, w-mapscale)

Port of Open Unleashed's MAP SCALE (`MX vs ATV RE/_documentation/map-scale.md`). The race setup page's **MAP SCALE** row scales the
whole track about the world origin, 50 % to 200 % in 5 % steps. The vehicles, the riders, gravity and the physics keep their real size.

- Smaller makes the arcade-sized tracks feel more realistic; bigger makes them more arcade.
- The scale is kept per track (the event's location) in the client's settings: `Race.MapScale.<location>` (ReflexSettings, the
  client's save file, not the game save). 100 % removes the key.
- On the row: left / right (pad, keyboard, the mouse on the row's arrows) step it; SELECT (A / Enter) puts it back to 100 %. The value
  shows as `135 %` (`100 %  (ORIGINAL)` at 100), with a one-line hint under it. The setup's summary shows a `MAP SCALE · NO RECORDS` chip
  when it is not 100 %.

## Base scale (w-alivegates, 2026-11-03)

A track can have a default scale of its own: the **base**. The converted MX vs ATV Alive race levels have one (the user's choice,
2026-11-03; `core/plan/ALIVE_RACING.md` "Scale"): **Nationals and short tracks 110 %, Supercross 130 %** (short tracks race as Nationals).
The game's own tracks, custom tracks and the four Free Ride-only Alive levels have none (1).

- **The row stays relative.** MAP SCALE's 100 % (ORIGINAL) is the track's default; the player steps 50 .. 200 % of it as before, and the
  world's scale is base x percent (NAT_Farm at 100 % rides at 1.1, at 120 % at 1.32). The hint under the row names the default
  ("100 % here is 110 % of the MX vs ATV Alive layout"). The base applies to the level, so the level's Free Ride rides at it too
  (confirmed by the user 2026-11-03).
- **Where the table lives:** `Content/AliveLevels.MapScaleBase` / `MapScaleBasePercent` (the registry's `race.event_type`: Supercross 130, any
  other race 110, no race 100). One place: the launch, the setup row, the online launch, the leaderboard key and the practice HUD all use it.
- **Saved values** are kept under the base they are relative to: `Race.MapScale.<level>@<base>` (`NAT_Farm@110`). A change of a default later
  starts the track at its new default; values saved before the base (plain `Race.MapScale.NAT_Farm`, a scale of Alive's size) are not read
  for such a track.
- **Records.** At its default a track counts as unscaled: its offline records, best laps, highlights, the lap log's map scale bit (16) and
  so PRACTICE and the leaderboards. At any other scale none of them, as before. Core: `rx_session_set_map_scale_base(s, base)` (before
  the event loads, with `rx_session_set_map_scale(s, base x percent)`); `rx_ms_records_off()` (`map_scale.c`: the scale differs from the
  base by more than 1e-4 relative) replaces `rx_ms_on()` in MXRecords_Join (00764170), MXHighlightTracking_AddProgress (006d7980) and the lap
  log's bit; `rx_session_map_scale_records` reports it. The geometry still follows `rx_ms_scale()` alone: a base of 1 is the original bit for
  bit. The client: `RxMapScale.Base`, `Current` (the world's), `Scaled` (Current != 1: the file-built parts), `OffDefault` (Current != Base:
  records, PRACTICE laps).
- **Online:** the session rules carry the host's percent (`mapScale`, relative); every rider's game makes base x percent of the event's
  track with the same table (riders ride together only on the same build, `OnlineProtocol.SameBuild`).
- **Leaderboards:** an Alive race's course id names its base (`NAT_Farm:national:s110`, `SX_DLC_SX01:supercross:s130`), so a change of the
  default makes new boards; the lap body and the ghost header carry the scale ridden (`mapScale`, percent); the Worker takes a lap only at
  its course's default and keeps it (`tools/leaderboard`, migration 0002; deployed live 2026-11-03, Worker version
  f4baf244, the 262 earlier laps at 100). See `core/plan/LEADERBOARDS.md` 2.2.
- **The Alive start gate** (a ScenarioAnimEntity drawn by OriginalAmbient from the core's node matrices, which keep real size) is drawn at the
  world's scale about its own place, as its GateStart slots and its bar's collision are (`core/game/alive_gate.c`).
- Offline records on Alive levels still need an EventTypeLocations row (MXRecords_Join creates the working row only for a location the
  front end's tables list; the Alive levels, location -1, have none): the base scale lets them through, the row does not exist yet.

## Where it applies

- Every arcade event the open shell's setup page starts: races (National, Supercross, ... all modes) and Free Ride, on Reflex's own
  levels, the DLC levels and the 26 imported MX vs ATV Alive levels (Free Ride). The row is on the setup page's EVENT tab.
- An arcade Free Ride at a location with Race & Ride challenges runs in the challenges' discovery mode (ReflexModes.InChallengesFreeRide).
  It still scales. A challenge started from one of its activation areas loads at 100 %.
- A RESTART or the pause's VEHICLE LIST reload keeps the world's scale.

Always 100 % (no row there; the launch passes 1):
- the MotoCareer (every series, Moto / Heat practice), the Race & Ride challenges and the activation areas,
- the MotoSkills tutorials and the garage's TEST RIDE,
- the front end's 3D menu world.

The core refuses a scale in a career session (`rx_session_set_map_scale` returns 2) and resets it to 1 when a career, a test drive or
a tutorial starts.

## Records

A scaled event keeps **no offline records, best laps or highlights** (achievements):
- `MXRecords_Join` (00764170) does nothing on a scaled track, so the event has no working row. Every MXRecords update tests the row,
  so none of them writes: best lap, best place, scores, jumps, wheelies, total time and laps. The leave's commit (00764560) and its
  highlight scan do nothing either.
- `MXHighlightTracking_AddProgress` (006d7980) returns 0, so there is no long-jump, lapping or stunt highlight.
- The core logs `MAP SCALE x: ... no offline records, best laps or highlights` when the scale is set. The client logs the same at the
  launch and in the session log.

## Core (`core/game/map_scale.h`, `map_scale.c`; API `rx_session_set_map_scale` / `rx_session_map_scale`)

`rx_session_set_map_scale(s, scale)` is called after create and before `rx_session_event_load` (0.25 .. 4). Only data whose source is
the event's own level loads scaled. The level brackets are `rx_ms_level_begin` / `_end`:
- the level database's packages (`rx_database_load_package`, the database named like the location),
- the level's subscenes (`rx_scene_load_subscene`, `<location>Base` / `<location><Activity>`),
- the level's `.tdf07` (`rx_track_load`).

The front end's world (UI_Compound), which an event session also builds, loads unscaled, and so do the vehicle, rider, animation and
AI databases.

| What | Where | How it scales |
|---|---|---|
| Terrain | `build_terrain.c` | cell spacing and the extent / bounds (+0x44d0..+0x44ec), the base heights (x s, so slopes keep their angle), the edge buffer, the texture projection. The deformation gain stays, so ruts keep their real depth. |
| Collision geometry | `build_collis.c`, `rx_ms_collision_geometry` | every kind (box list, oriented box, composite children and offsets, sphere, cylinder, triangle groups): vertices, extents, radii, bounds. This covers static collision entities, physics props, bales, step-up bars and tree trunks of the level. |
| Scene placements | `build_scene.c` `rs_matrix` | every entity's translation x s. The volume classes (Trigger, TutorialTrigger, WaypointGate, SectionRaceTrigger, TimedSectionTrigger, StartEntitySnakeTag) scale their axes too: their size is in the axes. Other axes stay a rotation. |
| World RSData fields | `build_scene.c` `rs_float` / `rs_vec3` / `scene_rs_vec3_list` | replay cameras' Close / Mid / FarRange, activation areas, zip-line and spline control points; animal homes / flee distance; the 30-second board's sign; AI avoid radius; snake area; particle draw distances |
| Track definition (.tdf07) | `build_activity_track.c` | node points (centre, edges, the two lines) before the terrain snap, segment / cumulative lengths, section lengths, the grid box, the track lengths, the start / finish / reset records (place and width, before their snap). Speeds and flags stay. |
| Start grid | `build_activity_grid.c` | the drop gate's GateStart_NN slots x max(s, 0.38) about the gate, and the nearest-node search radius x s. The gate selection's export (`rx_session_start_gates`) uses the same slots. |
| Forest | `build_forest.c` | tree positions, the cluster and forest boxes. A tree keeps its own scale, because its trunk collision is a level collis asset that is already scaled. |
| Water | `build_water.c` | surface place, extent, wave height scale / offset |

All of these are positions as the core reads them. Gates, waypoints, triggers, discoveries, challenge starts, cameras, particle
emitters, animated entities and crowd sections are all scene entities, so their placement covers them.

Scale 1 is bit-identical. Every site is guarded by `k != 1`. The whole heap after 1500 frames of a 12-rider National and 600 frames of
the Alpine Free Ride is byte-identical to main's.

Probe: `core/tools/map_scale_probe.c` (build line in its header) rides an exhibition event headless at a scale. Per run it:
- prints every slot's start position against the ground,
- drives the player toward the race line,
- prints a verdict line `MAPSCALE <s> <level> start_ok below_ground min_clearance moved_ai ai_mean_path player_moved laps`.

`rx_api_test` takes `RX_MAP_SCALE=<s>`.

## Unity (the client)

`RxMapScale.Current` (OpenReflex/RxMapScale.cs) is the scale of the world about to load:
- `ReflexEventSelect.Launch(..., mapScale)` sets it before the event scene loads; every launch but the setup page's passes 1.
- `OpenShell.Awake` resets it, so the menu world is at 1.
- `OpenReflexSimulation` hands it to the core before `rx_session_event_load`, exhibition sessions only. A library without the export
  fails the start with a message.
- `OriginalLevel.MapScale` is the scale its file-built parts were made at.

Built from the core's live state (already in the scaled world): the terrain, the tyre tracks, the deformation, vehicles, riders,
cameras, particles, props, ambient characters, the gate view.

Built from the level files, and scaled here:
- The scenery: `SceneryRoot.localScale = s`, built at the level's own matrices first. The sky dome is counter-scaled, so it keeps its
  real size. Core-driven matrices (props, drop gates, scenario-moved models) keep landing through world setters. `OriginalSceneryModels`
  compares the game's matrices with the level's moved by s.
- The scenery LOD and draw distances: the distance is divided by s (`OriginalSceneryLod`).
- The forest and grass:
  - The culling and LOD run in the level's own feet, with the camera divided by s.
  - The GPU gets the trees moved and sized by s, and the billboards' fade slope / s.
  - The grass cells, grid heights, tufts and world-to-uv follow s.
- The water: gWorld's rows x s, and the clipmap in the asset's space.
- Fog: start / end x s, for the scenery globals and the terrain material.
- The volume shadow / colour reference / map shadow projection over the scaled extent. Their baked heights are x s in the shaders
  (`RfxMapScale()`, `_RfxVolumeShadowParams.z`); the particles and the object programs use the same.
- The RT_Bump models (S x matrix, `_TerrainSize` x s).
- The Alive terrain decal rings: drawn in file feet, rects x s.
- The Alive bounce volume box x s.
- The sun billboard (its view-space size x s, so it keeps its look).
- The stands crowd sections: the section x s, and the seat quads x s (`ReflexCrowdBillboard.shader`). Characters keep their real size.
- The level sound emitters: points and min / max distance x s.

Not scaled on purpose:
- Unity's realtime shadow distances, AO and contact ranges (they are about the vehicles).
- The chase camera's offsets.
- The terrain's detail tiling. It is normalised over the terrain, so it follows the map as in Open Unleashed.

Both ORIGINAL and ENHANCED lighting read the same globals.

## QA

Headless (core): `map_scale_probe` on NAT_Concord, SX_Kingston, FR_Alpine and the Alive FR_Dunes / SX_DLC_SX01 at 50 / 100 / 150 / 200 %.
It checks:
- the start grid on the ground and spaced with the gate,
- no frame below the ground,
- every AI moving,
- AI completing laps (National 1 lap).

Player probes:
- `tests/perf_profile/bench.py run ... --env REFLEX_MAP_SCALE=<s>` (fixture PerfBench: a `MAPSCALE` log line) checks:
  - the level / core / root scales,
  - the Unity terrain against the core's own height grid at 25 points around the bike,
  - the scenery instances whose game matrix differs from the scaled level matrix,
  - the bike's height over the ground,
  - then the shots.
- `tests/menus_showcase` page `mapscale-row` (fixture `UiShowcaseMapScale.cs`): the row's steps, limits, keys, mouse, reset, per-track
  save, launch value, Free Ride and career.

## Results (2026-10-28)

- s = 1: the whole heap after 1500 frames of a 12-rider National (NAT_Concord) and 600 Free Ride frames (FR_Alpine) is byte-identical to
  main's library (b1929e5). `rx_api_test` 300 frames is identical too (addresses included). census 0 UNPORTED, sigcheck 0 findings.
- Headless (`map_scale_probe`, 6000 frames, 1 lap; Free Ride 2400): NAT_Concord, SX_Kingston and FR_Alpine at 50 / 100 / 150 / 200 %:
  - grid on the ground, slots spaced with the gate;
  - 0 frames below the ground;
  - 11 / 11 AI moving.
  AI mean path in 6000 frames:

  | | 50 % | 100 % | 150 % | 200 % |
  |---|---|---|---|---|
  | Concord (ft) | 2452 | 4330 | 4466 | 4488 |
  | Kingston (ft) | 2118 | 3984 | 4372 | 4495 |

  Concord laps: 11 / 11 AI finished at 50 %, 10 / 11 at 100 %; 9 / 11 at 200 % in 9000 frames. The Alive FR_Dunes and SX_DLC_SX01 load
  and ride at 50 / 150 / 200 %.
- Player bench (`--env REFLEX_MAP_SCALE=0.5 / 1 / 2`, NAT_Stillwater, SX_Kingston, FR_Alpine), all 9 runs:
  - the Unity terrain on the core's grid at 25 / 25 points (20 / 20 at Kingston 50 %), within 0.5 ft;
  - 0 scenery instances off the core's matrices (487 / 133 / 31 compared);
  - the bike 1.9 to 2.1 ft over the ground (Alpine: on the hovercraft deck at 50 / 100 %);
  - the live world at 57 to 60 game fps, ORIGINAL and ENHANCED.
  This line caught a real bug: the discovery-mode Free Ride had been left at 1 in the core.
- Setup row (`menus_showcase` page `mapscale-row`): PASS, 0 failed.

## Not yet / known limits
- FR_Alpine's arrival above about 140 %: the arrival scenario puts the bike at a real-size offset from its (scaled) anchor. At 200 % the
  rider therefore starts on the ground inside the scaled hovercraft's hull, not on its deck, and rides out from under it. At 50 % he lands
  on the deck.
- Covered start gates (NAT_Stillwater) at 50 %: the canopy is lower than the riders' heads, so the helmets show through its roof.
- Ambient characters keep their real size. Next to scaled stands and scenery they look big at 50 % and small at 200 %; only the stands
  crowd sections scale.
- The AI is slower on a small map: at 50 % they cover about half the distance in the same time as at 100 % (corners and jumps shrink,
  the bikes do not). They still complete laps. Above 100 % the AI ride as at 100 % (path length per time the same).
- Scenario cinematics play at real size at their scaled anchors. Examples: the podium cast stands on a podium that is scaled, and the
  intro / finish walkers. Animated level entities (birds, animals, flaggers) move with their real-size animation from a scaled place.
- Hit-discovery collectables keep their real pickup size; their (invisible) markers are drawn scaled.
- The hovercraft (Alpine, a port extra) keeps its real size.
- The camera's far plane is the core's (15000 ft). At 200 % the Alpine fog end (16000 ft) lies a little beyond it.
- The Free Ride challenges' medal distances / heights are not scaled; the challenges themselves always run at 100 %.
- Party modes (Tag, Snake, Egg Hunt) are not offered; their areas would scale.
- The old static-link test scripts (`core/tools/rx_api_test.sh`, `build_world_test.sh` / `build_world_accept.sh`) do not link on main:
  `fourwheel_active_native_bridge.c` needs the per-TU flags of `native_tu_flags.tsv`, and the career / fnsig objects are missing. The
  checks here link `rx_api_test.c` / `map_scale_probe.c` against the product dylib instead, and the heap comparison links
  `core/build/obj/*.o`.
