# Buoyancy angular correction0084f700

The corrected Renne contact closure progressed into actual RACING (first frame1331), then encountered0084f700 at frame2051 before completing its900-racing-frame target. This is a real buoyant kinematic-body angular-force routine, called only with body+d0 enabled.

`phys_buoyancy.c/.h` implements original0084f700: rotate global up through body orientations+50/+70, calculate their dot and normalized cross, evaluate the original mixed-single/double acos rational fit with exact intermediate float roundings, scale/project restoring torque, transform to local coordinates, add damping/force, and decay angular state. It uses original mutable stiffness/damping/decay globals009df494/498/49c and threshold009ddfc8. Normalization preserves the zero and infinite-length bitmask behavior. Local scalar-double helpers preserve x86 NaN operand precedence and real-indefinite bits. No host acos/asin replaces the original fit.

Minimal `world_phys.c` routing calls this owner directly through its header at the original one-argument stack depth; no opaque original call remains at that site. The oracle adapter takes ESI=state and stack0=body, RET4, with no live output registers. The separate hook `rx_BuoyancyProbeHook`, controlled by `RX_BUOYANCY_PROBE=1` and optional `RX_BUOYANCY_PROBE_FRAME` (default5), provides160 calls covering finite random orientations/forces, identity, zero, fixed tilt, SNaN, infinity and signed zero. It preserves/restores gains and caller registers before continuing the original world.

C syntax checks passed with warnings enabled. Coordinated oracle Verify passes160/160 calls with zero mismatches (`builds/sol_core_features/logs/buoy-verify.log`). Port-On and reference runs (`buoy-on.log`, `buoy-ref.log` in the same directory) have identical simulation state and whole-memory hash `059c118ad7e22dc6` over26,569 pages.


Native Renne and RenneB now each pass900 actual racing frames, recorded in `builds/sol_core_features/logs/native-new-NAT_Renne.log` and `native-new-NAT_RenneB.log`. The lead's40-second windowed Renne smoke also passed (`builds/sol_core_features/logs/player_clean_renne.log`). These validate progression through the former stop, without claiming a completed lap or exhaustive track coverage.
