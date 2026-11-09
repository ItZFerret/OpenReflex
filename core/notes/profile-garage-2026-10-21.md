# Garage snapshot and FreeRide construction, 2026-10-21

The product garage boundary imports real PlayerRider, PlayerRiderGear, PlayerVehicle,
PlayerVehicleParts and PlayerVehicleTuning row snapshots. It changes no physics or
vehicle-tuning equations. Original construction consumes those rows through
EnsureLocalPlayers 0059e030, vehicle/rider owner-table lookups, and
VehicleSpec_ApplyTuning 00543130. The public session boundary and managed persistence
are documented by the parent session handoff.

## Native profile boundary

`build_uiprofile.c` validates the snapshot against loaded native schemas and install
IDs before any mutation: bounded terminated fields, contiguous rows, unique cells,
valid selections/references, model-compatible parts, unique equipped gear and part
types, the pro rider's actual default gear set, tuning model limits, and unique
vehicle/tuning IDs. PlayerRider must have one existing row; provided integer/string
cells update with existing Rdb setters and omitted camera float cells remain intact.
The other four tables contain integers and use original clear/add-row operations.

The newly reached Rdb clear paths in `build_tables_rdb.c` follow 004f91e0 and
004f9260: clear the integer unique tree; float dispatch shares the integer clear;
string clear destroys its vector elements and clears searchable / unique string
trees. Existing generic tree-clear and string destructors are reused.

Live tuning updates the selected player-owned vehicle's PlayerVehicleTuning row and
calls the existing 00543130 port. The first six inputs are bonuses, the final two
are absolute Braking/Steering values (0/50/100). Each bonus pair preserves the
existing available points; base plus bonus cannot exceed 100. Readback returns the
actual final spec values at +0xa0..+0xbc. Session access must occur between frames.

## FreeRide construction and intro additions

Alpine's pickup entity 0x1059 was absent from the factory. The following data-only
construction and lifecycle paths were transliterated from the original decompile
(and register arguments checked where ambiguous):

- `build_scene_anim.c`, factory/header wiring in `build_scene.c` and
  `build_scene_int.h`: 006bbde0 spline-loop-with-particles constructor, 006bbe20
  loader, 006c39a0 ring and 006c3a00 sphere attachable constructors. They reuse the
  existing spline-loop model/animation loader, hierarchy lookup, emitter defaults,
  and effect bind. Existing 006bc040 updates and FreeRide pickup hiding remain.
- `race_beh2_perform_init.c`: BehaviorPerformStunt 0052aa20/0052aa80/0052aa60 and
  BehaviorRoamWorld 0052ee80/0052eec0/0052ed50 lifecycle, with 00508920 activity-name
  lookup. The original AI-only target selection is retained; human initialization
  returns without consuming random state.
- `race_beh2_avoid_init.c`: BehaviorAvoidObjects 00520a70/00520b90 lifecycle, reusing
  scene group query and existing driver rule getters.
- `build_activity_freeride_state.c`: 0055ebc0 reads the original intro-done field.
- `anim_scenario_race_intro.c`: FreerideIntroScenario 0074d170/0074d060/0074d440
  startup, advance and skip predicates, using existing cast attachment, rider pause,
  scenario simulation flags, and session state.
- `race_activity.c`: FreeRide pre calls existing rx_scenario_start instead of the
  original register-call trampoline; the original UI transition remains an oracle
  boundary as in the race intro.
- `anim_scenario_intro.c`: attachment matrix calls dispatch to registered core
  virtuals; the already-ported RiderCharacter::vf19 float argument is dispatched
  with its typed signature and original float bits.

No AI stunt or avoidance update state machine was replaced or approximated. Those
unported AI paths remain explicit failures if a broader event reaches them. Optional
original replay-recording and career collectible award paths also remain outside
this acceptance. No entity-skipping stub was added.

## Validation and limits

Parent-controlled serial native builds used at most two compiler workers. `nice`
reported setpriority unavailable in this sandbox; commands still completed.

`core/tools/rx_api_profile_test.c` supports RX_TEST_FREERIDE=1, selecting the original
FreeRide event domain (table id 9, engine enum 8), FR_Alpine/location 45, one rider,
vehicle class -1. The final run used the extracted-only asset root and captured real
profile. It passed invalid imports, duplicate equipment types, pre-load profile
import, actual selected ownership ID, effective tuning/canaries, invalid/live tuning,
1200 normal frames through the authentic intro, fresh-session recreate and reimport.
No Enter/intro-skip pulse or state override was used. Final state: RACING, speed
61.833, frame 1202, game time 20.0665. Evidence:
`builds/sol_garage/logs/freeride_profile_contract.log`.

These newly reached lifecycle functions have not run through a new oracle matrix;
this is native contract and runtime acceptance of a single local-human Alpine
FreeRide configuration. Other vehicle/event/AI configurations and scene-by-scene
unload are not proven by it. Parent Unity acceptance separately validates visible
pause/tuning and front-end persistence; stopping native advance is not an independent
native pause API.

A subsequent visible run reached an old direct-original register trampoline for
00548810 from `anim_rider_a_ik.c`. This already has the exact native implementation
`rx_fx_stunt_gesture_dir` in `fx_stunt_input.c`: the real steer/lean KeyMaps' input-7
axis pair. The IK caller now includes its owner header and calls it at the same
guest call site; no look input is suppressed. After this wiring fix, the complete
1200-frame Alpine contract again passed (`logs/look_input_freeride_contract.log`).
The native test also supports optional RX_TEST_LOOK keyboard reflex pulses; that
optional variant was prepared but not run before returning the CPU slot for the
parent's visible acceptance. The regular run does not establish dedicated branch
coverage of this intermittent IK path.
