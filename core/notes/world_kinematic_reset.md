# Lost kinematic authored-pose reset

`0076abb0` (134 original bytes) is `rx_world_kinematic_reset_authored_pose(world,body)`: ECX=world, EDX=body, RET0. The three saved-register slots are retained. It copies authored/reset position+60 and quaternion+70 into current position+40/quaternion+50, publishes both through the real render object's vf30, restores raw buoyancy byte+d0 from+d1 and axis-lock word+b4 from+b8, then checks the real keyed-vector count and refreshes its broad-phase entry through the existing `0042b000` port. `rx_world_kinematic_reset_lost` now calls it directly after deactivation. The signed `(body-begin)/e0` and empty-vector validation follow the original.

The actual captured keyed props have `VehicleDamagePhysicsEntity::vf30` at `00430560` (266 bytes), which was an additional unported dependency. Its port rotates entity-local XYZ offset+170 by the supplied quaternion, adds the given position and calls the existing `004039a0` PhysicsEntity write-back. The offset's fourth lane is masked before quaternion arithmetic. The original aligned20 frame and temporary position are retained. These actual reset callbacks publish memory; reset callers consume no register result. Oracle group `rx_KinematicReset` contains both new functions.

`world_kinematic_reset_probe.cpp` uses the actual race world's keyed vector, real VehicleDamagePhysicsEntity objects, original shapes and original grid. Up to three bodies receive four restored variants: authored/identity/rotated poses, nearby saved positions, raw lock and buoyancy values, zero/nonzero offsets and a masked NaN fourth offset lane. Each case directly invokes both entry points through registered oracle dispatch. Three bodies yield 24 total calls and 12 direct comparisons per function. Whole guest memory, added mappings, host allocator and CPU are restored after each call. No synthetic vtables or disabled broad-phase refresh substitute for this closure.

## Verified oracle acceptance (2026-10-21)

`builds/sol_collision_fpv/reset-reference-acceptance.json` accepts the 30-frame UTV Verify/On/reference suite. `0076abb0` and `00430560` each have 12 direct calls and 12 comparisons, zero mismatches. Every mode completes all 24 restored calls on three actual keyed bodies, with zero publication errors and the real broad-phase grid enabled.

All three modes agree on state hash `f1ab16cb7424de70` and whole-memory hash `29f77d2307f199cf` over 64,827 pages. On mode additionally records the nested damage-entity callbacks; the direct calls establish independent positive comparison coverage for both functions.

Use the existing UTV world prefix `builds/sol_core_features/original-utv` (without `-world.bin`). It contains six actual keyed damage entities; the fixture uses three. The default Concord AI11 capture does not satisfy this fixture lookup, so its retained failed probe log is not acceptance evidence. No synthetic fixture or extraction is needed:

```sh
for mode in verify on ref; do
  python3 builds/sol_collision_fpv/run_reference.py utv-kin-reset "$mode" \
    rx_KinematicReset rx_KinematicResetProbe RX_KIN_RESET_PROBE \
    builds/sol_core_features/original-utv
done
python3 builds/sol_collision_fpv/accept_reset_reference.py utv-kin-reset
```

`RX_KIN_RESET_FRAME` defaults to 20. These restored reset/publication checks pass; the native Bedford B continuation also passed with12riders,9,000racing frames/10,249total, finite poses and no core error; see `builds/sol_collision_fpv/fixed.json`.
