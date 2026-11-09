# RavenVehicleUTV model pose

`004c1fa0` (`rx_UtvPose`): readable hand port of V60, sourced from
`_re/out/decomp/reflex_002_004b32a0.c` and disassembly 004c1fa0..004c6339.
No generated lifted implementation used. Product registration has the virtual
this-only signature; the oracle calls `_regs` to retain incoming EBX/EDI for
Node_Invalidate's saved stack slots. Those values affect dead scratch only.

The aligned 0x24c frame has one ESI save. Model-node setters receive the original
scratch addresses (+130 for evaluated rotations, +140 body, +1f0 front wheels,
+230 rear wheels). Simulation is +1540. Read inputs: body +7c0, centre +36e0,
base +180, front positions +220/+2c0 and rotation +860, rear positions +360/+400
and rotation +9a0, front/rear suspension pivots +3f90/+41f0, lengths +3fe4/+4244,
front steering quaternion +5630, rear visual mode +55f0. All eight scalar
matrix-to-quaternion conversions and cosine-polynomial range reductions preserve
original arithmetic ordering. Repeated endpoints update +8750/+8760 each corner.
Rear mode 0 and 1 write the shock angle; other modes retain the original stale
stack angle behavior rather than inventing a default.

Validation: C11 syntax checked. Heavy product/oracle builds and verification are
owned by the lead and have not yet run for this unit.


## Lead verification, 2026-10-21

Accepted reference evidence and current native boundary are recorded in `HANDOFF/SESSION_2026-10-21_VEHICLE_POSES.md` and `builds/sol_vehicle_pose`. The 600-frame UTV/SuperBuggy/ProLite/Pro2 comparisons match, including whole-memory hashes. Restored probes cover articulation and post-step paths absent from natural recordings; the fixed-body node is verified separately on the real MX rider node. Native playability is a separate acceptance and is not implied by these method comparisons.
