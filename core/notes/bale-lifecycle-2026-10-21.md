# Bale lifecycle and the core client presentation — 2026-10-21

The original moves start-chute bales when a vehicle exits the bale's named trigger, not when a client lap counter reaches a guessed value. No repositioning, lap editing or visibility heuristic belongs in the renderer.

## Original source path

- Load: `build_scene_props.c` 006c0e70/006c0650 read `LinkedTriggerID`, subscribe to VehicleExited, calculate placed and five-metres-below positions. PopUp target +1d0 is placed; PopDown target +1d0 is below.
- Activity start: `build_activity_records.c` 006c0f80/006c0760, compared with decompile `reflex_007_006a1740.c`. Category8 hides both. Otherwise modes2/3 begin PopUp visible, all other modes begin hidden. PopDown is hidden for modes2/3/4, visible otherwise. The original also puts physics positions in place and disables hidden bodies. These decisions already exist in the core.
- Trigger occupancy: `fx_entity.c` 0040a870 clears seen flags, 0040a8a0 marks occupants, 0040a980 prunes departed occupants and 00419820 posts VehicleExited with vehicle and trigger.
- Event callbacks: `world_scene_handlers.c` 006c14a0 raises a matching PopUp if allowed by the vehicle's activity mode/player/category. 006c0b60 lowers matching PopDown only in its owning active activity, immediately deactivating collision and clearing body+b0. It is possible for a lowering bale to remain visible while its collision is already disabled.
- Motion: `fx_world.c` 006c1250/006c08f0 move toward +1d0 using the original per-tick +1e0 step. Completion snaps body position, sets body enabled/disabled and ModelInstance+a2 drawn/hidden. PhysicsEntity004039a0 writes position/quaternion back to world matrix+30.

Existing oracle evidence is recorded in `core/plan/QUEUE.md` handlers2: both callbacks have 400/400 verified calls each in Concord and SX, with all instructions covered and no mismatches. The randomized callback probe is not evidence of naturally driving a complete lap.

## Presentation gap and bounded fix

The current core front end leaves OriginalProps without a legacy simulation. Its getter therefore never collects physics prop poses or bale visibility. Scene object snapshots expose a matrix but omit ModelInstance+a2; static scenery keeps file placements.

New additive `rx_session_props` returns 80-byte read-only records with class, scene entity id, kind, matrix and flags: actual visibility1, changed from initial matrix2, first snapshot4, valid body8, body enabled16. It caches the scene-owned physics prop list after loading, validates primary/renderable vtable pairs per snapshot, and reads only those cached props every call. Runtime vehicle debris is deliberately outside this scene prop snapshot. Count queries and bounded output permit exact capacity allocation. No address is exposed to the client.

OriginalProps queues this API through the core simulation worker, matches scenery by original scene entity id, applies changed matrix/visibility through its existing renderer/LOD hiding path, and avoids legacy plugin exports on the core path. It allocates update matrices only when matrix or visibility changes and counts matched ids once. Scrolling texture core exports remain outside this change; existing file/clock fallback is retained.

## Validation

`core/tools/rx_api_props_test.c` is a bounded public-API test: invalid/preload calls, 80-byte ABI, count/truncation guard canaries, same-frame repeat reads without advancing the simulation, body flag invariants, finite matrices, and initial matrix parity with the existing scene snapshot. It runs actual startup plus a bounded number of real racing frames, reports naturally observed motion, and does not require or invent a lap/trigger transition. Both native changed source and this test passed syntax checks with warnings enabled; the lead owns integrated build and runtime validation.

For natural transition acceptance, run existing real vehicle input through the exact scene LinkedTriggerID volume, log the first before/after flag/matrix changes by id, verify the body's enabled flag follows the source transition, then inspect the same bale in Unity. A full lap requires a real lap marker crossing and increment from the race getter. A callback fixture or a short straight throttle ride cannot establish that result.
