> Current acceptance: native ATV free rides on Alpine/Gibson/Cheyenne and four-rider Concord/Freight Yard B races passed 900 racing frames each, with six bodies/50 model nodes/69 rider bones. Native wreck-checker and AI steering ABI fixes are contract tested. The user-reported water crash fixes are in the Mac bundle, with native immersion survival and windowed bike/ATV recovery verified; see `HANDOFF/SESSION_2026-10-21_ATV.md`. Earlier pending/missing statements below are chronological history. Other vehicle types remain unaccepted.

# Vehicle expansion inventory — 2026-10-21

The user explicitly expanded scope beyond MX: ATV and remaining real Reflex vehicle families are now in scope. Read-only audit of current core/client and the existing extracted tables/assets. No source port, asset mutation/extraction, build or player run by this agent. Parent owns central public-API smoke tests. Old MX-only ownership/scope guidance does not constrain the new user request.

## Actual installed catalog

Decoded the existing `Assets/tbldata/MXTables_Default/MXTables_Default/MXTables_Default.tbldata` directly with the read-only BXML reader. Also checked extracted DLC003/004/005 tables: none adds VehicleModel rows. Table presence is not simulation acceptance.

| VehicleTypeId / factory | VehicleModel rows | Single class | Hierarchy node counts | Current core status |
| --- | --- | --- | --- | --- |
|2 MX Lite /0 MotocrossBike |10 |3 |MX18 | Existing accepted MX route. |
|3 MX /0 |10 |2 |MX18 | Existing accepted MX route. |
|4 ATV /1 Atv |9 |1 (Freestyle24 also maps4) |ATV_Hierarchy50 | Factory, simulation construction, masses/parameters/proxy and six bodies exist. Class-specific tuning/update/pose still unported; not accepted runnable. |
|11 UTV /7 Rhino |12 |22 |UTV01/02_Hierarchy61 | Five-body constructor/parameters/proxy exist. Runtime leafs missing. |
|12 Super Buggy /5 Buggy |10 |19 |SB01/02_Hierarchy56 | Five-body constructor/parameters/proxy exist. Runtime leafs missing. |
|14 Pro Lite Truck /3 TrophyTruck |10 |21 |ProLite_Hierarchy57 | Same supported construction family as Pro2 with engine-class distinction. Runtime leafs missing. |
|15 Pro2 Truck /3 |10 |27 |Pro2_Hierarchy61 | Five-body constructor/parameters/proxy exist. Runtime leafs missing. |

All listed non-MX node counts fit the existing64-node client slot capacity. Pro4_Hierarchy97 exists as an asset, but type13 Pro4 has no VehicleModel rows. Types1 MXMini,5 MonsterTruck,6 Trophy,7 OffroadBuggy,8 SandRail,9 GolfCart,10 ORV,13 Pro4,16 ATVFreestyle,17 MXFreestyle have zero model rows. These are not equivalent to supported selectable vehicle products. The actual freestyle classes reuse types4/3.

## Construction vs runtime boundaries

`build_vehicle.c:rx_vehicle_factory_create`00542b90 implements factories0/1/3/5/7, matching the original factory. Factories2/4/6 have no original creation case. `build_vehicle_atv.c` and `build_vehicle_wheeled.c` already construct the actual models/proxies/physical bodies and read original parameter files (`atv_vehicle`, `utv_vehicle`, `buggy_vehicle`, truck parameters). This is not a missing import problem.

Static first-boundary candidates (read directly from original simulation vtables, checked absent literal source registration):

| Family | S vf35 during mass construction | S vf2 during placement | Other required runtime methods |
| --- | --- | --- | --- |
|ATV |0048cde0 (landed, original Verify passed) |0049c8d0 (landed, Verify passed) |S vf16 004997f0 and vf32 004a1a10 landed/verified; V vf33 004862e0 landed/verified. Remaining: S vf20 0048f9e0, vf23 0049ee00, vf19 0047c460 and main control/contact closures; V vf60 0047e850 (24KB), vf62 0047cfa0, vf29 0047d870 |
|UTV |004cc610 (476) |004d75b0 |S vf16 004d4750, vf20 004cc980, vf23 004d9460, vf32 004da510, vf19 0044f680; V vf33 004c6c40, vf60 004c1fa0 (17KB), vf62 004c0e70, vf29 004c1620 |
|Buggy |004b1e10 (476) |004bce80 |S vf16 004ba270, vf20 004b2180, vf23 004be490, vf32 004bf400, vf19 004a2510; V vf33 004ac2e0, vf60 004a4270 (30KB), vf62 004a3170, vf29 004a38e0 |
|ProLite/Pro2 |00450ab0 (476) |00459400 |S vf16 004564d0, vf20 004cc980(shared original), vf23 004d9460(shared), vf32 0045b8f0, vf19 0044f680; V vf33 00473210, vf60 0046c090 (26KB), vf62 0046b160, vf29 0046b6d0 |

Evidence order: ATV mass setup calls S+8c at build_vehicle_atv.c188. Wheeled mass setup uses corresponding vtable path. Placement build_vehicle_place.c232 calls S vf2, then V vf33, controls reset, V vf60(0), V vf62. These are source-order predictions; exact first fatal function must be recorded by public event-load smoke (an earlier body/helper can intervene). Do not label vf35-only closure as ATV support.

Core API already recognizes ATV/truck/buggy/UTV vehicle/simulation vtables in rx_session.c1080–1127 and exports six/five bodies. Garage profile validation accepts any real VehicleModel row and compatible model-part pairs; it does not enforce factory0. Client ReflexVehicles.Drawable admits0/1/3/5/7 and contains machine hierarchy/material rules; lighting agent independently audits presentation paths.

## Bounded ATV enable plan

1. Import an actual type4 profile through rx_session_profile_import and load FreeRide Alpine with0AI. Record first fatal address; no internal pokes or forced motorcycle branches.
2. Port ATV tuning vf35 and small scalar/controller leaves with original offsets and exact arithmetic; validate construction before placement. Shared four-wheeler analogues require source evidence, not offset guesses.
3. Close ATV placement's vf2/vf33/vf60/vf62 dependency chain with per-owner interfaces. vf60 is a substantial actual node-pose routine, not a trivial four-wheel alias.
4. Close physical frame vf16/vf20/vf23/vf32, rider animation/AI concrete virtuals and teardown reached by the same public route. Add source adapters and original oracle comparisons at each bounded cluster.
5. Accept finite six-body state, actual50-node matrices, real engine/gear values, throttle/steer/brake/reverse, recovery and return-to-front-end on one ATV. Then mixed MX/ATV AI/track classes and other vehicles. No all-platform/all-type claim from constructor or snapshot success.

## Public API fixture snapshots

`builds/sol_core_features/generate_vehicle_profiles.py` reads actual extracted tables plus the valid existing profile-cells.bin. Writes only `vehicle-profiles/{atv,utv,superbuggy,prolite,pro2}.bin` and manifest in the test directory. Raw204-byte rx_profile_cell arrays, no header, matching core/include/rx_api.h. Each uses PlayerVehicle0 with model0/4/5/3/2 respectively, rider0/custom gear copied from the validated snapshot, model's authored Default=1 parts and distinct part slots. Tuning bonuses come from each selected VehiclePartType.TuningAttribute and DefaultTuneAttribute1/2; base+bonus<=100 asserted. Braking/Steering50. Model/part compatibility, unique slots, row counts and serialized size are asserted by generator; actual native import acceptance remains the parent's check. No user save or original asset changed.

## Monster truck remnants — report explicitly

There is leftover monster-truck-related Reflex data/plumbing, but no runnable monster-truck class discovered:

- VehicleType5 row names STR_VEHICLE_MONSTERTRUCK / FactoryName MonsterTruck; zero VehicleModel rows across the four installed table assets. VehicleHierarchies has ghost MT_Hierarchy/MT02_Hierarchy entries; no matching imported model file found in the model inventory. No RavenVehicleMonsterTruck / RavenSimulationMonsterTruck function name appears in original functions.tsv.
- Managed ReflexVehicles.Factories retains index2 `MonsterTruck`; its Drawable path rejects factory2. Native build_vehicle_spec documents/preserves the factory index; actual factory00542b90 has no2 creation case, matching original.
- **Executable residual config reader exists:** core/game/build_mxgame_init.c `rx_tag_radii_read`0051c620 reads all8 original Tag collision-radius keys through address table; its index2 key0x93f070 is MonsterTruckCollisionRadius and stores radius squared at009e38f0. Startup invokes it (build_mxgame_startup.c78). Original MXRegistry.xml has MonsterTruckCollisionRadius24 and Audio/FloodingSoundDepth/MonsterTruck6.5. This is shared parameter-table plumbing, not a monster simulation/model consumer. The original decompile explicitly names the key.
- No reference project/Unleashed source was searched for this result. Do not claim monster remnants absent or delete compatibility strings/config as if they were an enabled vehicle feature.

## ATV bounded-method acceptance update

Parent completed original Verify with zero mismatches for all five landed methods: tuning0048cde0 (3 calls), reset0049c8d0 (1), suspension frames004997f0 (1,559), probes004a1a10 (1,559), and V ground reset004862e0 (4). On/reference state hash `a01204e338e8cd97` and memory hash `021d09ba5952f78a` match. See atv-methods-2026-10-21.md. These are verified bounded methods, not remaining unported gaps. Final native product build/startup smoke remains pending, and substantial runtime/placement methods remain; no playable-ATV acceptance yet.

## Final native construction checkpoint

After the verified methods were built into the product core, public profile/event tests advanced ATV to0047e850 (V60 full pose update), UTV to004da510, Buggy to004bf400, and both trucks to0045b8f0 (their contact-probe methods). Each stops explicitly on frame1; none is accepted as a playable ride. `builds/sol_core_features/vehicles-final.json` records actual results. UTV/Buggy/Truck vf35/vf2/vf16 now all have positive original Verify calls and matching On/reference hashes; the earlier static first-boundary table is historical.


## Current ATV physics coverage (eighth comparison batch)

The startup-boundary table and early bounded-method status above are historical. Current ATV S-vtable0093d054 has all36 slots registered, including parent-owned S0 deleting destructor0048c730 and S8 pending tuning0048d320. Ordinary imported-data tuning is supported; development-only pending XML stream/log reload still contains explicit original-call boundaries and is not accepted native functionality.

Dedicated ATV force/control/contact closures now have original-positive comparison coverage. Parent's eighth batch reports S11 ForceStep3,111, S9 ControlStep3,116, and isolated TireContact12,456 calls, all zero mismatches; previous accepted placement, wheel frames/probes, suspension, engine/gears, steering, relative-pose, rider-off and runtime/water groups remain accepted. See atv-physics-2026-10-21.md for exact arithmetic fixes and acceptance history.

Native startup reaches900frames without missing calls, but playable ATV acceptance is pending: the rider wrecks beside the parked vehicle and throttle is suppressed. Parent isolated a native wreck-checker ABI mismatch in ATV rx_f80 wheel-load/grip getters; runtime owner is fixing and verifying it. No forced state or substituted MX behavior is an accepted resolution. Other vehicle-family entries above are not promoted by this ATV result.
