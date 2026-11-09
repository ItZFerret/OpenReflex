# Kinematic terrain contact impulse

`0076f2f0` is the per-contact response called by `0076d9a0` / `rx_world_kinematic_collide_terrain` for a kinematic scene body. It is a thiscall with ECX=body and stack arguments world/contact; RET8. The original is 333 bytes. The port is `rx_world_kinematic_terrain_contact_impulse` in `world_body.c`, declared in `world.h`; the terrain collision loop calls it directly. Oracle group: `rx_KinematicTerrainContact`. The liveness plan marks no live register result: persistent results are the world velocity arrays.

The aligned frame has sub54 and three saved registers, with the contact pointer at final ESP+c. Restitution, inverse mass, friction and inverse inertia are at ESP+10/+20/+30/+40. The material pair is body+24 versus terrain material0. The inverse mass is `1/body+0`; inverse inertia is raw `(1/Ix,1/Iy,1/Iz,1/Iz)`, matching DIVSS and the original SHUFPS assembly without an extra multiply. `007705b0` changes linear velocity at world+1d10+50*index and angular velocity at world+5b90+50*index, using body+80 as the index, orientation at body+50, centre at body+40, and the unmodified contact normal at contact+10. This differs from the water wrapper's negated normal and the generic-body terrain response's contact-result accumulation.

The existing exact impulse kernel supplies restitution, friction, rotational effective mass and the separating/contact thresholds. The existing native physics-property constructor supplies the original first-use material registry before pair lookup; loaded worlds normally already own it. No substitute impulse, collision query, original-code fallback or disabled response was introduced.

`world_kinematic_terrain_probe.cpp` adds a restored-state oracle fixture: `RX_KIN_TERRAIN_PROBE=1`, optional `RX_KIN_TERRAIN_FRAME=20`, and hook `rx_KinematicTerrainContactProbe`. Forty registered calls use the authored material registry: three authored material IDs plus zero/high valid coefficient stimuli, eight approaching/separating/rest/tangential cases, four finite positive mass/inertia scales, centred/off-centre points, and identity/rotated poses. Tangential speeds straddle 0.01. The fixture occupies checked mapped stack space and allocates no body or world; this wrapper consumes point/normal and body state directly, so no fake shape/vtable or terrain-ray result is needed. All guest pages, added mappings, host state and CPU are restored after every call. Logs distinguish changed and unchanged velocity results and record material coefficient bits.

## Verified oracle acceptance (2026-10-21)

`builds/sol_collision_fpv/reference-acceptance.json` accepts the 30-frame Concord AI11 Verify/On/reference suite. `0076f2f0` has 40 calls and 40 comparisons, zero mismatches. Every mode reports 40 restored fixture calls: 20 changed velocity results and 20 unchanged results, with 11 authored materials available. The three authored material IDs 0/5/10 have restitution bits `3f000000` and friction bits `3e4ccccd`; controlled zero and high coefficient cases also pass.

All three modes agree on state hash `fc092b3cdac8235c` and whole-memory hash `262c4c467ea69cef` over 67,648 pages. The acceptance parser checks comparison coverage, both response outcomes, coefficient evidence and On/reference hash identity.

Run each mode serially with the existing captured default Concord AI11 world; no extraction:

```sh
for mode in verify on ref; do
  python3 builds/sol_collision_fpv/run_reference.py concord-kin-terrain "$mode" \
    rx_KinematicTerrainContact rx_KinematicTerrainContactProbe RX_KIN_TERRAIN_PROBE
done
python3 builds/sol_collision_fpv/accept_reference.py concord-kin-terrain
```

These are restored per-contact oracle results, not Bedford full-field acceptance. The native Bedford B continuation passed with12riders,9,000racing frames/10,249total, finite poses and no core error; see `builds/sol_collision_fpv/fixed.json`.
