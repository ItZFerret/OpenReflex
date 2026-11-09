# rx_phys: the physics engine core

## Current collision dispatch audit (2026-10-21, ATV continuation)

The sphere/height-field case `00864ec0` is now native in
`game/phys_pair_sphere_terrain.c`, including both dispatcher orderings. It keeps
the original terrain triangle query, closest-point kernel, x/z/y distance sum,
per-lane radius comparison, normal orientation, and 30-contact clamp.
`oracle/phys_pair_sphere_terrain_probe.cpp` exercises 32 direct calls and 32
dispatcher calls with both orderings, flipped normals, zero radius, penetration,
separation, and counts above the clamp. There are 37 hit cases and 27 misses.

Acceptance: direct generator 32/32 verified with zero mismatches; dispatcher
29,351/29,351 verified with zero mismatches. On/reference state hash
`7c49eb341a2b5d03` and memory hash `805b07cf35a3e139` (26,580 pages) match exactly.
Evidence: `builds/sol_atv/logs/first-sphere-{verify,on,ref}.log`. Product integration
and ride regression are separate checks, pending the current ATV build.

The last eight generators of the dispatch table are ported (w-collide, 2026-10-26), so every case of `0085ddd0` runs
a core function and `phys_contact.c` no longer calls the original:

| generator | pair | file | notes |
|---|---|---|---|
| `008650c0` | sphere / circle (0x400) | `phys_pair_circle.c` | rim point nearest the sphere, ray from the circle's center into the sphere; CL = the flag |
| `0086f3d0` | cylinder / circle | `phys_pair_circle.c` | the disc against the cylinder (0081c590), mean of the flagged points, 00846f10 normal |
| `00864460` | sphere / ray list (0x20) | `phys_pair_rays.c` | double-precision row dots; the t0 contact block is unreachable (t1 > t0 never holds for a = \|dir\|^2 >= 0) |
| `00872c90` | ray list / height field | `phys_pair_rays.c` | TerrainSegmentQuery per segment; product zeroes the two dead w lanes as `0086f2a0` |
| `00864dd0` `0086b360` `0086f150` `00872970` | query shape (0x100) / sphere, box, cylinder, ray list | `phys_pair_query.c` | the shape's +0x20 object answers (vf8 ray query, vf17 sphere query); no class of the exe builds a 0x100 shape |

The circle is `CollisionShapeCircle` (type 0x400, the vehicles' body circles), the ray list `CollisionShapeStaticRays` /
`DynamicRays` (type 0x20, e.g. the body-probe rays S+0x20d0). Verified by `oracle/phys_pair_probe2.cpp` (below).

**The observed dispatch is general.** The product-only four-wheel closures (the vf13 wheel pass, the BODY probe, the
composite closure) enter `0085ddd0` through `phys_contact_dispatch_observed(_live)` with the original's real frame. Its ten
typed roles keep their closures (`collision_triangle_ray.c`); every other case goes to `phys_contact_pair_observed`
(`phys_contact.c`): the return site names the case (`pair_return_sites`), the entered generator runs as the core port with the
registers of the fx model and the stack arguments already pushed, then the RET is modelled. The generators' EAX / ECX / EDX /
XMM0 leftovers are not modelled there: the three callers reload them before any read (see the function's comment).

Work list: `plan/rx_phys.tsv` (250 functions). Sources: `game/phys_*.c` / `.h`. Oracle adapters: `oracle/phys_*.cpp` (group `rx_phys`).

Build with `RX_CORE_FILES="phys_* crt_*" native/game/tools/build_cli.sh <out>`. The `crt_*` files supply `_CIatan2`, fmod, pow and ceil
(`game/crt.h`). Verify with `native/game/tools/verify.sh <out>/reflex_boot rx_phys`.

## Layout

| Files | Contents |
|---|---|
| `phys_vec.h` | lane-wise SSE helpers: `phys_add/sub/mul/div/sqrt`, `v4_*`, `phys_hsum3/4`, the packed quaternion product, `phys_cross_ba`, `phys_normalize3` |
| `phys_contact.c`, `phys_coll_*.c`, `phys_pair*.c` | narrow phase: the dispatcher `0085ddd0` and its case table, shape helpers, and the pair generators (sphere, box, cylinder, triangles, rays, composites) |
| `phys_cylinder.c` | closest points on a cylinder (`00846c70`, `00846f10`) |
| `phys_terrain*.c` | terrain heights, normals, materials, tracks / deformation, the segment query (coarse and fine traversal) |
| `phys_constraint.c` | the SlidingRotor and FixedBody constraint vfuncs |
| `phys_anim_sample.c`, `phys_quat.c` | pose sampling, slerp, acos, atan2 |
| `phys_rigbody.c`, `phys_rig*.c` | the rider rig's rigid bodies (P = rider animation + 0x380) |
| `phys_ragdoll.c` | the ragdoll solver: substeps (`004238a0`), external forces, joint jacobians, articulated node inertias (`00422960`), force solve (`004232b0`), fixed-joint position / velocity enforcement |
| `phys_rider.c` | the crash driver at the rider-animation level (`00425600` / `00425640`): joint limits (`00420770` with the swing / twist split `00420160`), ground, bone and water contacts, speed clamp, sound, wake test, pose nodes |

## Coverage

The five test worlds never run the crash code. Their scripted drives do not crash the rider, so these functions are not called there:
the ragdoll, rig and rider functions at `0x0042xxxx`, plus `00422450` and `00423a50`. These functions were verified on
`National_NAT_Concord`, 1800 frames, `REFLEX_DRIVE=7`, which crashes the rider. With all `rx_phys` ports on, that run calls 82
ports, and all 82 have 0 mismatches.

Not ported:

- (none in the crash world any more: `00426d00` is ported, see below.)

### `00861a60`: the sphere / sphere pair (`phys_pair.c`)

`00861a60 CollisionPair_SphereVsSphere` (320 B) is the contact dispatcher's `(type 2, type 2)` case: the first unported
collision pair the 4-rider clean product ride reached (race frame 432, after dispatchfix). Ported by hand from the
disassembly (0x861a60-0x861b9d) as `phys_pair_sphere_sphere` (`phys_pair.h`, adapter `oracle/phys_pair.cpp`), and
`phys_contact.c`'s `pair_call_core` now calls it; the `k_pairs` row `(2, 2)` is unchanged (verified against the dispatcher's
case block at 0x85de67: ECX = A's position*, EDX = EAX = A, ESI = B, EDI = M; stack M, B, pB*, flag; RET 0x10).

- Math, in the original's order: `d = pB - pA`; `d.d = (x*x + y*y) + z*z`; the radius sum `(rB*rB + rA*rA) + (2*rB)*rA` per
  lane of the radii vec4s (A+0x20 / B+0x20); a miss unless all three lanes take `d.d < sum` (the mask's three words sum to
  -3). Then `dist = sqrt(d.d)` (splatted), half penetration `((rA + rB) - dist) * 0.5` per lane, the unit normal
  `d / dist` -- zeroed where `dist` is 0, the qNaN 0x92dc30 where `d.d` is infinite (`cmpneqps` masks at 0x92dc20 / 0x92dc30)
  --, point `pA + (rA - half) * n` (all four lanes), depth `2 * half` (the w lane uses the radii's w lanes), the normal negated
  when the flag's low byte is non-zero. One contact: index clamped to 0x1d, touching flag +0x6a0.
- **Leftover registers**: the adapter declares OUT_ECX | OUT_EDX. On a contact ECX = count + 1 and EDX = M + 0x30 * count; on a
  miss they are **not** the arguments: the original's compare spill leaves ECX = lane 1 of the compare mask and EDX = lanes
  2 + 1 + 0 (the first probe run failed 35 / 64 calls on exactly this). The dispatcher adapter still declares no outputs.
- Verified with the new probe `oracle/phys_pair_probe.cpp` (frame hook, group `thunk`; `builds/collisionpair/pair_probe.sh`):
  it allocates two sphere shapes and a contact set on the game heap and calls the original generator directly (group d) and
  the original dispatcher (group m; case table included) with random radii / positions / counts / flags. **00861a60: 64 calls
  / 64 verified / 0 mismatches per run, 0 in three seeds (192 calls); 0085ddd0 1064 / 1064 / 0.** The five reference worlds
  and the crash world call no pair generator at all, hence the probe.
- The other 11 pair-generator cases are still the original in `pair_call_core` (they are not reached by the ride or the crash
  path; the ride's next stops are AI behaviours, not pairs).

### `0086f2a0`: the cylinder / height-field terrain pair (`phys_pair.c`)

`0086f2a0 CollisionPair_CylinderVsHFTerrain` (301 B) is the dispatcher's `(type 8, type 0x200)` case: the live-tree 12-rider
clean ride's stop at race frame 280 (stuntrace's run). Ported by hand from the disassembly 0x86f2a0-0x86f3cc as
`phys_pair_cylinder_hfterrain` (`phys_pair.h`, adapter `oracle/phys_pair.cpp`, group `rx_phys`); `pair_call_core` calls it.
The `k_pairs` row `(8, 0x200)` was already right, checked against the case block at 0x85e281: EAX = A (cylinder), ECX = PA,
EDX = QA, ESI = B (height field), EDI = M; stack A, B, flag; RET 0xc; and the dispatcher's `flag` copy is the same word the
generator inverts.

- The cylinder's 24 world rays (`00847630` into 0x00d6d5e0: the position, then the three rings' 8 vertices) each query the
  height field: `TerrainSegmentQuery 007c6cc0` on the terrain object from `00402dc0(shape +0x20)` (the same registry lookup the
  rest of the terrain code uses). A hit adds a contact: point = the hit point, normal = the patch normal (negated per lane when
  the flag's byte is 0), depth = `|(ray end - point) . normal|`, in the original's order
  `((dy*ny) + (dx*nx)) + (dz*nz)` then `Float_Abs` and a splat. Index clamped to 0x1d, count+1, touching flag +0x6a0.
- **Frame / dead stack**: `T = ((sp0 - 4) & ~15) - 0x48 - 8` (push ebp; and esp, -16; sub 0x48; push ebx / esi). The query's
  outputs live at `T+0x10` (normal) and `T+0x20` (point), the ray at `T+0x30` / `T+0x40`; the two stack arguments at `T-4` /
  `T-8`, the query entered with ESP = `T-0xc`. The query writes only x, y, z of its two outputs, so their **w lanes are dead
  stack** and the record's w lanes are copied from them unchanged (the original copies whole `movaps` vectors); `Float_Abs`
  (`andps` with 0x977570) and the `-0.0 - v` negation are the same instructions. **The product initialises those two dead lanes
  to 0** (see "Dead w lanes in the contact records" below); the oracle build keeps the copied residue.
- Verified with the pair probe (`oracle/phys_pair_probe.cpp`, new group `c`; `builds/terrainpair/pair_probe.sh <boot> c`):
  64 calls / 64 verified / 0 mismatches per run, seeds 0 / 7 / 12345 (192 calls), 62 of 64 with contacts (on / above / below
  the surface, radii 0 .. 2.5, heights 0 .. 4, identity and random rotations, flags 0 / 1, start counts 0..2 and 29..32). The
  original's coverage under the probe (hook only, `builds/terrainpair/cov.sh`): 77 / 77 instructions, 0 never.
- The crash drive (`National_NAT_Concord`, 1800 frames, `REFLEX_DRIVE=7`, all `rx_phys` ports) runs unchanged (dispatcher 36501
  calls / 0 mismatches, memory hash `d87ea0fff7778187`), but its original never reaches this case (coverage 0 / 77 instructions),
  so the probe is the per-call verification. On mode vs none on that drive is identical (`e87afe55395e568a` /
  `d87ea0fff7778187`, 27617 pages).

### Dead w lanes in the contact records (bikestate, 2026-10-21)

The contact record is (point, normal, depth) as three vec4s, but only x, y, z are contact geometry. Generators built from the
terrain query write x, y, z with scalar stores, and the original then copies the whole 16-byte vectors into the record: the
record's w lanes are whatever the previous call left at the generator's frame addresses (a documented dead-stack trap above).
Two generators in the product's path do this:

- `0086f2a0`: the record point / normal w lanes = the frame's `T+0x2c` / `T+0x1c`;
- `0086bd80 CylinderVsCylinder`: the record point w = the frame's `+0x9c` (never written), the normal w = what `00846f10`
  leaves at `+0x6c`.

**The lanes are read.** The response math is 4-lane SSE: `007705b0 RigidBody_ContactImpulse` and `0076f440 ...ImpulsePair`
compute `tangent = n * vn - (-vp)` and the friction impulse in all lanes, and `00438180` (the body-probe correction) averages
`normal * point` per lane and adds the mean to every body's pose. A large or non-finite w lane therefore reaches live code
through the w channels of the mechanism.

**The product chain (firstride, 12 riders, W + A/D).** With the port's own stack residue the terrain-pair records carried
`FLT_MAX` (0x7f7fffff): a box generator's `00860e90` points buffer (initialised to `FLT_MAX`, faithfully) had run earlier at a
deeper frame that overlaps this pair's dead lane. `rx_world_kinematic_water_contact` (0076dd50) then takes the water contact's
normal, `vn` is finite but `n.w * vn` overflows to inf, `tangent.w` is inf, the friction and normal impulse w lanes are inf and
`inf + -inf` becomes the x86 NaN 0xffc00000 (`J.w`). That NaN goes: kinematic body velocity.w → the generic object's contact
result +0x80.w (`0076e090`) → `bike_apply_external_impulse` (004355d0) → the bike's velocity.w → `0085d100` pose integration →
the getter's `position[0][3]` at **race 287** (the rideranim2 stop). The same class in `0086bd80` (point w = 0xffffffff, a NaN)
hit the bike's body-probe list directly and made pose.w NaN at race 396 of the steer-from-205 ride.

**Fix (product only).** `phys_pair_cylinder_hfterrain` zeroes `T+0x1c` / `T+0x2c` and `phys_pair_cylinder_cylinder` zeroes
`L+0x9c` / `L+0x6c` before the record copies, under `#ifndef RX_ORACLE`. The record's live lanes are unchanged (the w is not
part of the contact). The oracle build keeps the original's residue because Verify compares the record bytes against the
original's own stack words: the probe showed the original writes pointers / 2048.0 / FLT_MAX there (`0086f2a0` c/t groups,
64/64/0 before and after), so an unconditional zeroing fails the per-call compare on dead words only.

**These w lanes are dead in the original too.** The probe build `builds/bikestate/bikestate_probe.cpp` (frame hook, original
physics, `National_NAT_Concord`, `REFLEX_DRIVE=1`, frame 1500) shows the original's own vehicle simulation with
`angvel.w = 0x7f800000 (+inf)` and `pose.w = 786f465f` (2.7e34, finite); the mass / inertia w lanes are the fourth diagonal
entry (-7.774 etc.). So the API's `rx_bike_state` rows 1..3 carry dead-channel values in the original as well (the C# bridge
reads `position[0][0..2]` / the orientation quaternion only, `OpenReflexResultsAdapter`); what must stay finite is the live
xyz and the response math, which the initialisation restores.

### The kinematic bodies' contact impulses `0076ee60` / `0076ea00` (`world_body.c`, group `rx_world`)

These are the two per-contact rigid-body impulses of the kinematic collision passes; both were still original calls in
`world_phys.c` until contactimpulse. They live in `world_body.c` (with `007705b0` / `0076f440`, whose wrappers they are) but
are registered in the `rx_world` adapter group (`oracle/world_body.cpp`).

- **`0076ee60`** (337 B): the impulse of one contact of a kinematic body O against a static object S, called from
  `0076b070 Kinematic_CollideStatic`'s contact loop (`rx_world_kinematic_collide_static`). ECX = O, EDX = S; stack: world,
  the contact record (point at +0, normal at +0x10); RET 8. Material pair (`0084fe70` on `0xda6ad0`, O +0x24 / S +0x20), inverse
  mass `00767f80` on O +0, inverse inertia from O +0x10..+0x18, and `007705b0` on the body's kinematic records
  (velocity `world + 0x1d10 + 0x50 * O[0x80]`, angular velocity `world + 0x5b90 + 0x50 * O[0x80]`) with the contact's own point
  and normal. Nothing is accumulated on the body's contact results: the records change in place and the collide loop adds the
  poses itself. Frame `push ebp; mov ebp, esp; and esp, -16; sub esp, 0x54; push ebx, esi, edi`; the callee's four vectors live at
  `[esp + 4]` (restitution), `+0x14` (inverse mass), `+0x24` (friction), `+0x34` (inverse inertia).
- **`0076ea00`** (1,117 B): the impulse of one contact of a kinematic body O against a keyed body K, called from
  `0076cd00 Kinematic_CollideKeyed` (`rx_world_kinematic_collide_keyed`). ECX = contact, EDX = K; stack: world, O; RET 8.
  Inactive O (index +0x80 < 0) returns. Then, by K's state +0x84: bit 1 set -> the two-body `0076f440` on both bodies'
  kinematic records with the inline inverse inertias of +0x10..+0x18; else only when O's `|v|^2` exceeds all of K's material
  +0xb0..+0xb8 (the compare mask's lanes 0..2; +0xbc is compared but not part of the `== -3` test) -> wake K (bit 5 sets
  K +0x84 = 0x23, else `Kinematic_Activate`), re-read O's activity and then: both active -> `0076f440` with the inverse inertias
  through `00767f90`; only K active -> `007705b0` on K against the **negated** normal; only O active (activation refused, e.g. a
  full 200-body list with no victim) -> `007705b0` on O with the contact's normal. Frame `sub esp, 0x84` with the same prologue;
  the pair's restitution / friction live at `[esp + 0x14]` / `[esp + 0x44]` in the first branch and swapped in the second, as
  the original loads them. The "only K active" arm is dead: the entry guard already proved O active and nothing changes it.

Call sites: `world_phys.c` calls them directly at the original stack depth (`WORLD_CALL_SP(8, ...)`).

**Per-call verification** (`oracle/world_contact_probe.cpp`, frame hook 0084ed20, group `thunk`, port
`rx_ContactImpulseProbeHook`, `RX_CONTACT_PROBE=s|k|sk`; script `builds/contactimpulse/contact_probe.sh`): a synthetic kinematic
world / bodies / material record on the game heap, then calls the originals through the installed ports so Verify runs the core
and the original on the same state. `0076ee60` **320 calls / 320 verified / 0 mismatches** (128 cases x 1..4 contacts: material
pairs 0..10 in both orders, masses 0.5..2000, inertias 0.25..3000, identity / rotated poses, approaching, separating, resting
velocities and tangential speeds on both sides of the friction threshold); `0076ea00` **96 calls / 96 verified / 0 mismatches**
(inactive O, K integrating, the speed gate failing, the wake-by-flag path, `Kinematic_Activate` on an inactive K, and the
activation-refused one-body path on a 200-body list). Coverage of the originals under the probe
(`builds/contactimpulse/cov.sh` + `builds/ridepath/covrange.py`): `0076ee60` **94/100** instructions, `0076ea00` **291/330**.
The never-executed instructions are the material table's first-use arm (`00850010` + `_atexit`, run once by the original boot
before any ported call - the same shared `material_pair` guard as `0076efc0` / `0076e460` / `0076e090` / `0076dd50`), the
`_invalid_parameter_noinfo` trap for out-of-range pair indices, and `0076ea00`'s dead "only K active" block (33 instructions).

**The reference worlds do not reach them** (`0076b070` / `0076cd00` are 0 calls in the five worlds and in the
`REFLEX_DRIVE=7` crash drive); the product ride only meets them late in the race (race frame 1231 / 1244). The product ride
is the route that reaches them: `builds/firstride2/run.sh` with `RX_FULL_FRAME=1` (12 riders, W + A/D) now passes the old
session frame 2409 stop and runs **2831 racing frames / 4000 steps** in the views pass (33/33 checks; steer / nosteer 1231 frames
each, bit-identical to the pre-port summaries); a 9000-step run continues to race frame 4102 and stops at the unrelated unported
`00522a70` (session frame 5280, game 88.054).

### `00426d00`: the ragdoll against the bike (`phys_crash_solver.c`)

`00426d00` (10,692 bytes, stdcall A, dt; RET 8) is the crash path's bike contact: it is called by `00425640`
(`phys_rider_crash_step`) between the ground and the bone contacts. Ported by hand from the disassembly into
`game/phys_crash_solver.c` (+`phys_crash_solver.h`), adapter `oracle/phys_crash_solver.cpp` (group `rx_phys`).

- Frame: `L = ((sp0 - 4) & ~15) - 0x4d4 - 12`, cookie `L+0x4dc` (the original's `push ebp; and esp,-16; sub 0x4d4; push ebx/esi/edi`).
  Per-bone arrays stay in the frame because the original reads them through computed indices: the quaternions at `L+0x370`
  (bone b at `0x370+16b`, bone 0 = the root body's), the bike node positions at `L+0x240`.
- Stages: (0) guard `A+0xa8c0 && [A+0x37c]+0x204 && ((A+0x374==0 && A+0x88==0) || A+0x98) && A+0xa8a0==0`; (1) the reference
  point of `004225d0`, the rig's gravity (`(0,-32.2,0,0)` unless the debug flag `0xd72ad0`; then the zero vec and body 0's
  velocities cleared); (2) per bike node, compose its rotation onto the rider bone that maps to it (`phys_rig_bone_of_node`)
  and normalise (the original's stores: y/z/w scaled, x raw, the length into the next element's x); (3) the node -> bone map
  validation (flag `0xd72ad1`, no result); (4) per bone pair, the relative rotation `conj(qb) qa`, the world X and Z axes of
  both rotations, the packed-asin angles between them, the two corrective world angular velocities
  `angle * axis * dt * 30` applied through `004241f0`, and the body's angular velocity damped by 0.9; (5) the root body's
  world angular velocity with the up component and the linear damp.
- **Stage 5 is unreachable in the original.** Its condition is `(A+0x374 || A+0x88) && A+0x98 == 0`; the entry guard admits
  only `(A+0x374 == 0 && A+0x88 == 0) || A+0x98 != 0`, and neither slot is written between them (the called helpers write no
  such field). Reviewed twice against the disassembly (0x426d1e-0x426d66 / 0x42901d-0x42903a). The code is ported as it
  stands; it never runs.
- Exactness notes for the next reader: the products are double precision, each term a double product of the float operands,
  rounded once to float (the `qmul` / `qapply3` helpers in the file); the dots are single precision (`mulss/addss`); the asin
  fit's `den` stays a double (an intermediate `float den` moved the result by an ulp); the p1/p2 cross products' y/z lanes are
  the address order `0x40c` / `0x408`, not the decompiler's names. The corrected per-bone array quirk (x raw) was read off
  the original's frame, not the decompiler.
- Verified per call in the crash world (`National_NAT_Concord`, 1800 frames, `REFLEX_DRIVE=7`): **00426d00 312 calls / 312
  verified / 0 mismatches**; the three callees in the same world: `004225d0` 73/73/0, `004241f0` 2774/2774/0, `004296d0`
  1387/1387/0 (each run alone so the original 00426d00 reaches them through the adapters). The On mode of the crash world
  (all `rx_phys` ports on vs none) is **identical** (state hash `e87afe55395e568a`, memory hash `d87ea0fff7778187`, 27617
  pages). The five reference worlds: `verify.sh rx_phys` 0 mismatches / On identical (nr of ports: 166/164/146/164/164).
- Call-site switches that the crash path needed (hand transliterations of the original's register/stack arguments):
  `phys_rider.c` 00426d00 -> `phys_crash_bike_solver`; 007dcfb0 / 007dd1b0 / 0041feb0 -> the node / bone ports (oracle: the
  originals, so the leftover registers the pose function returns stay the original's); 00679540 -> `rx_world_crowd_level`;
  007efef0 -> `rx_world_water_set_height` (both oracle-original in the tracked chain); `phys_ragdoll.c` 0041f430 / 0041f650 /
  0041f6e0 / 0041f800 -> `rx_world_matrix3_*` / `rx_world_block_inverse` at the original's ESP (EDX = the destination for
  0041f430, unchanged for the others, which is what 00422960 returns).
- **Product boundary (camera)**: `0042ade0` (the rising edge of A+0x8c) hands the rider speed to the active camera entity's
  vf34. The product's active-camera slot is null (the camera unit owns the view entities), so the call is the oracle's
  original and nothing in the product; nothing gameplay reads the vector.
- Product: `RX_FULL_FRAME=1 rx_api_test 1200` now runs the **full 1200 frames** (race state 2, 12 riders, laps 0/3, place 6,
  69 rider bones, 18 bike pose nodes). One frame past the request it stops in `rx_activity_presence -> stats_set` on a null
  function (a stats boundary, another unit).


Water paths: `0042a6c0` verifies on drive 7, but Concord has no water. The buoyancy branches are unexercised.

## Traps

**Dead-stack values reach results.** Several originals read stack memory that they never write:

- the w lanes of the cylinder vectors (`00846c70` / `00846f10`), which are copied into the contact manifold;
- the record lanes `+0x9c` and `+0x6c` in `0086bd80`;
- the first normal in `00429a40` (`[esp+0x44]` before the pushes);
- the frame slots that hold a bone's normal or point when its handle is null (`00429a40`, `0042a6c0`).

The ports read those words at the original's frame addresses. Build the frame base exactly as the original does:
`L = ((sp0 - 4) & ~15) - frame_size - 4 * pushes`.

**A generator's spills feed the next generator.** Verify mode keeps the original's results, so it cannot see this trap; only On
mode does. The cylinder generators copy dead w lanes (`00846f10`'s frame) into the manifold. At the same stack depth, those words
are what the previous generator spilled there.

Example: `008657d0` (BoxVsBox) had 0 mismatches in Verify but diverged in On mode on SX. Its frame has to end with the original's
spills:

- the last corner's coordinate splats (+0x170 / +0x190 / +0x1a0);
- the second edge pass's per-crossing temporaries for its last edge (+0x130..+0x1d8);
- the sums and means (+0x1e0 / +0x1f0, +0x130 / +0x160);
- the final face-weight splats.

To find the words that matter, copy the lifted port's stack residue into the native run and bisect over the offsets. The words in
SX were +0x170..+0x178.

**The Newton step (w-composite, 2026-10-29).** `0081c590`'s Newton refinement (the disc rim against a cylinder, called by
`0086bd80` CylinderVsCylinder and `0086f3d0` CylinderVsCircle) takes its step size from the frame slot `S + 0x60` (`movss xmm6,
[esp+0x60]` at `0081cb24`), which the original writes only when a determinant is exactly 0 (1e7, `0081cb1c`). Otherwise the step
is whatever the stack held there, and it changes the contact geometry and even the contact count, not only w lanes. In a composite
pair's child loop (`008732d0`) the word is left by the previous child pair: after a BoxVsCylinder (`00867ca0`) it is `00846f10`'s
frame `+0x20`, the side distance (`00846f10` called at `00869a76`; the kernel's `S + 0x60` = `F - 0x2d0` below the composite's
frame base `F`). The native `00846f10` wrote no frame, so an all-native FR_Cheyenne model-1 ride left the original at step 2288 (an
airborne body impact on a tree: 3 contacts instead of 4, other points / normals). `phys_cylinder_closest_point_normal` now leaves
the original's frame image (every word the original writes, plus its last SqrtF call's argument, return address and EBP); the
composite pair then gives the original's contacts. Found with `core/oracle/phys_composite_dead_probe.cpp`: with the dead stack
poisoned before the call the original's result does not change (it depends only on what is written inside the call); a
register-level transliteration of `008732d0` with each callee original or native shows only the dispatch matters; stack snapshots
before the dispatch, injected into the native run and bisected, give the one word; the emulator's write watch gives its writer.
The record's point w lane in the same pair is `0081d480`'s `[esp+0xc]` (d.y of the box's last edge, `0081d54b`): its port
(`rx_world_segment_cone`) leaves its frame image too. Both images were checked with `REFLEX_PORT_DEAD=1` Verify (dead stack
compared): only the unmodelled saved EBX / caller EBP words differ.

The slot is also fed from other contexts, which are not modelled yet. On National_NAT_Concord_ai11 (12 riders, turn ride) the
original's Newton step words at 0081c590's entry (a temporary log of every call, original and core) were matched by the all-native run in 3,244 of the first
3,904 calls before the change (then the call sequences part) and in 5,788 of the first 5,976 after it. One remaining writer:
`00860e90` (phys_rays4_triangles, a vec4 store at `00861090`, 94 frame stores in all) through BoxVsTriangles `00869d40` in
`00872860`'s child loop, ahead of a CylinderVsCylinder at the same depth.

**The w lanes of CompositeVsHFTerrain (w-hfterrain, 2026-10-29).** `00873010`'s Verify mismatches (335 of 2,447 calls on
National_NAT_Concord_ai11, turn ride) were all contact-record w lanes in the manifold M, which lives in the collision pass's frame
(`0076ad54` calls the dispatcher): `[7feff63c]` / `[7feff64c]` are record 0's point / normal w (`M + 0x30k + 0x1c / + 0x2c`), the
others records 1 and 2. They are written by CylinderVsHFTerrain `0086f2a0` (`0086f394` / `0086f39d`), which copies the whole
point / normal vec4s from its frame `T + 0x20` / `T + 0x10` while TerrainSegmentQuery writes only x, y, z. In `00873010`'s child
loop those two w words (`T + 0x2c` / `T + 0x1c`) are what the child's bounds call (`0087320a`, DynamicCylinder::vf2 `00846700`)
left at its frame `S + 0x7c` / `S + 0x6c`: the w lanes of p0.zwxy and p2.zwxy (p_i = a_i * (q * conj), its first quaternion
product). The original's own result does not depend on stack written before the call (the probe's P1 / P2 runs equal R). Fix:
`phys_dyncyl_bounds` writes the original's frame image (all 12 vec4 partial products of the second product and `+0x0c` = h), and
`00873010` makes its two bounds calls at the original's ESP (`phys_shape_bounds_call`: the four stack arguments below the caller's
ESP, ESP lowered by them and the return address). Verify `00873010` 335 -> 0 of 2,447. Found with
`core/oracle/phys_composite_dead_probe.cpp` (`RX_DEADPROBE_RWATCH=<call>:<address>:<file>`: the emulator's write watch during one
call's reference run). The product zeroes the two lanes in `0086f2a0` (`#ifndef RX_ORACLE`), so its live results do not change: the
paired product-vs-original ride is identical with and without the fix. EAX of `00873010` differs (the port is void) but no
caller of `0085ddd0` reads it (on every path from its 38 call sites EAX is written before it is read).
CompositeVsComposite `008732d0` kept 24 of 141,540 mismatches there: record w lanes too, from CylinderVsCylinder `0086bd80`'s
normal (`0086e066`, its `[esp+0x60]` w = `ffffffff`, a compare mask); fixed by w-parity, next paragraph.

**The contains-point masks (w-parity, 2026-10-30).** The two contains-point tests, CollisionShapeBox_ContainsPoint `00845f20` and
CollisionShapeCylinder_ContainsPoint `00846bb0` (push ebp; and esp, -16; sub esp, 0x10), store each lane-mask vector whole at their
`[esp]` to add up its three low lanes, so their frame keeps the last mask tested with its w lane (`ffffffff` when the test passed:
`00846bb0`'s masks are splats, `00845f20`'s compare the dead w lanes of the point and the box). In `0086bd80` the record normal's w
lane is A's surface point w (`L + 0x6c`), which the second `00846f10` call copies from its own never-written frame words; in a
composite pair's child loop those hold the mask that `00845f20` left when the BoxVsCylinder (`00867ca0`) before it clamped its
point (`00845b50`). The ports wrote no frame, so the w lane was older stack. Fix: `phys_box_contains_point` /
`phys_cylinder_contains_point` leave the last mask at their frame base (`contains_frame`, phys_coll_shape.c), and
`phys_box_clamp_point` (`00845b50`) makes its `00845f20` call at the original's ESP (its box argument and the return address
`00845b65` written as the original pushes them). Measured: the masks alone leave the 24 mismatches; with `00845b50`'s call at the
original's ESP they go. Verify `008732d0` on National_NAT_Concord_ai11
(turn ride, 6000 frames): 24 -> 0 of 141,540. The saved EBP below the return address is still not modelled.

**The bounds frames of CompositeVsComposite (w-parity2, 2026-10-30).** `008732d0` calls each child's bounds vfunc (`008733b6` /
`00873436`) with four stack arguments below its frame base `F` (rotation, position, min, max) and the return address: the bounds
function's frame (`00846700` for a DynamicCylinder: the quaternion products of `phys_dyncyl_bounds`) lies 0x14 bytes lower than when
the port called it at `F`. The dispatcher and the pair generators of the child pairs run below it and read their dead stack, so
`phys_pair_composite_vs_composite` now calls `phys_shape_bounds_call` (as `00873010` does since w-hfterrain). Measured on the paired
Concord ride (CylinderVsCylinder `0086bd80` of the slot 2 / slot 3 wheels at gate+54.3 s, the stack below its entry ESP logged on both
sides, `core/tools/parity/cyl_log.inc`): the 0x90 bytes under the entry now equal the original's (208 differing words -> 172); the
ride itself is unchanged. The word that decides that contact is still different: `0081c590`'s Newton step `[S + 0x60]` (= the
generator's `L - 0x70`, its entry ESP - 0x27c): written by `00860e90` (phys_rays4_triangles, the vec4 store at `00861090`) inside
BoxVsTriangles `00869d40` in `00872860`'s child loop (the bike's static-triangle collision, earlier in the same frame; the
emulator's write watch, `RX_PAIR_WATCH`); the port of `00860e90` writes no frame. With the original's word written there (a test
hook: `PARITY_CYFIX`, bisected to the one word) the product matches the original one frame further. Calling CylinderVsTriangles
`0086e090`'s bounds (`0086e23f`) at the original's ESP as well moves that writer's leftover, but not to the original's value (it is
`00860e90`'s), and it changes the 12-rider National freeride_hash_trace from frame 4190: not taken by w-parity2. The fix is `00860e90`'s
frame image (w-parity3, next paragraph); the bounds move is taken there too.

**The frame of `00860e90` (w-parity3, 2026-10-30).** phys_rays4_triangles (push ebp; and esp, -16; sub esp, 0x2d8; push ebx / edi:
`E = ((entry ESP - 4) & ~15) - 0x2e0`) keeps the four rays, their SoA starts / unit directions, the running best / hit / normal
accumulators and the last tested triangle's temporaries in its frame; the port now leaves them as the original does (`r4_frame` in
phys_tri.c: each slot's last value, written once at the end, so the per-triangle loop stays in registers): the vec4 slots of the
prologue, the loop and the hit epilogue, the loop counter `+0x24` (0), the normals argument `+0x2c`, the cookie `+0x2dc`, the record
words of the five Vec4_CmpLe_CR6 (`0041c640`) call sites (`[out] = ([out] & 0xffffff0f) | 0x80 (all lanes) | 0x20 (none)`: the old
bits stay), the last CR6 call's own frame below `E` (argument, return address, EBP, mask vector, EBX = triangle + 0x20, ESI, EDI)
and `__security_check_cookie`'s return address `00861a53` at `+0x04`. The callers write what the original's call sites leave:
`rays4_call` (phys_pair.c) stores the five arguments and the return address (`00869f1e` / `00869f4f` / `00869f80` in BoxVsTriangles,
`0086b620` / `0086b6a2` / `0086b724` in BoxVsHFTerrain) and, after the call, the caller's EDI (`+0x00`: the mesh T / the triangle
count) and EBP (entry ESP - 4) through `phys_rays4_saved_regs` (the oracle adapter writes them from the machine's registers). The
word CylinderVsCylinder `0086bd80` read at the paired ride's k 3447 is `+0x230` (Dz). Verify with `REFLEX_PORT_DEAD=1` (the paired
ride's oracle, 2,960 frames): 15,939 calls, 0 mismatches with the dead stack compared (before the saved-register words: 5,010 with
only the caller's EBP / EDI different). With it the paired ride (core/tools/parity/pair.sh) is identical to the original over a
40,000-frame reference (39,140 aligned frames, to gate+649.2 s; it stopped at k 3447 before).

**CylinderVsTriangles' bounds and edge-call frames (w-parity3).** `0086e090` calls its shape's bounds vfunc (`0086e23f`) with four
stack arguments and then `00873540` (Edges12_Triangles) twice with six (return addresses `0086e270` / `0086e4f1`): the port now
makes all three calls at the original's ESP with the original's arguments, and leaves its own frame words (PT `+0x14`, the first
set's hit cursor `+0x18`, QT `+0x1c`, the cookie `+0x21c`, `__security_check_cookie`'s return address `0086e749` over the saved EBX,
DynamicCylinder::vf2's saved ESI = C). `00873540` (push ebp; and esp, -16; sub esp, 0xc24; push ebx / esi / edi: `E12 =
((entry ESP - 4) & ~15) - 0xc30`) keeps all its per-edge quantities in frame arrays; `phys_edges12_triangles` now leaves them as
the original does (`e12_frame`: t, u, D, v, 1/det, T, P, det, Q, u + v, |edge| rows; the flag arrays; the u / v rows of hit edges at
`+0xaa0` / `+0xb60`; the last mask; the pointer words; the cookie and the cookie check's return address), the caller writing its
ESI / EDI / EBP (`phys_edges12_saved_regs`; ESI is C, then the record count after a hit of the first set). Verify with
`REFLEX_PORT_DEAD=1` (paired ride oracle, 4,360 frames): `00873540` 7,444 calls, 0 mismatches with the dead stack compared;
`0086e090` 3,722 calls, only its caller's ESI / EBP words left (before: about 260 words per call, from the bounds frame at the old
ESP, the edge frame and the arguments). Why the National freeride_hash_trace moves from frame 4200 (its image hash only; the local
bike's state is identical): at frame 4190 two AI bikes' wheels touch (CylinderVsCylinder under CompositeVsComposite) right after a
CylinderVsTriangles at the same depth that found no hit, and the Newton step word (`entry ESP - 0x27c` = `E12 + 0xb80`: edge 2's
recorded v when that edge hit, else the bounds frame's `+0x20`, `s2` of its fourth axis vector) is now the bounds' `+0x20` as in the
original's layout, where the port used to leave its `+0x10` (`Y`) there (logged with a scratch dispatch hook and the generators'
entry ESPs).

**The frame of the fine terrain walk (w-parity4, 2026-10-31).** Two more Newton-step writers, both found from the oracle side
(On mode, all port groups, paired Concord ride). (1) k 1074: the saved EDI of Node_EvaluateWorld `007db890` (core/notes/world.md
traps). (2) k 9278 (gate+151.5 s, AI slot 10): rx_phys alone and rx_world alone each reproduced it (with rx_world alone the
native Kinematic_CollideWater `0076ac40` calls the whole native collision subtree directly). A scratch log of every `0081c590`
Newton step read (`[S + 0x60]`, address and value, with the engine clock) in three On runs (only `0081c590` On = the original's
words; rx_phys + `0081c590`; rx_world) differs first at the read at `0022f130` in oracle frame 10471/10472: the original's word
is `000057f6`, both port runs read `bf24e733` (a Node_EvaluateWorld form-b spill from earlier). The emulator's write watch
(`RX_PAIR_WATCH=<frame>:0022f130:<file>`, plain oracle) names the writer: TerrainSegment_FineTraversal `007c83b0` (under
TerrainSegmentQuery, CylinderVsHFTerrain, CompositeVsHFTerrain and the water pass's dispatch), `movsd [esp + 0x20]` at `007c858a`:
the x count's double `|Q.x - cx| - 0.49999999999`, whose low dword the Newton step takes. The port wrote no frame. Fix
(phys_terrain_ray.c): `phys_terrain_fine_traversal` writes every frame slot as the original does (the list is in its comment:
the counts' doubles, the walk's boundaries, the z pieces' end (x, z) over the double at `+0x20 / +0x24`, the pieces' start /
end, the segment record), the four pushed arguments and the return address of each piece call (`007c8753`, `007c8822`,
`007c899d`, `007c8a74`), and its caller writes the saved EBX (the terrain) / EBP (`phys_terrain_fine_traversal_saved_regs`; the
coarse walk, which also leaves its own pushes and return addresses `007c7911` / `007c7a14` / `007c7be1` / `007c7cee`, or the
oracle adapter). Verify with `REFLEX_PORT_DEAD=1`: the frame from the return slot down to L - 0x14 equals the original's; the
remaining dead words are TerrainSegment_CellIntersect `007c7d10`'s own frame and the CRT calls' (`_CIfmod`, `_copysign`,
`_ftol2_sse`), not modelled. With it the On-mode oracle is identical to the original past k 9278 (core/plan/QUEUE.md
"## w-parity4").

**The render pass's residue (w-aidrift, 2026-10-31): the one Newton-step source the product cannot have.** When the first
CylinderVsCylinder `0086bd80` of a frame reaches `0081c590` before any of that frame's physics has written its step slot (entry ESP
- 0x27c, `0022f110` on the original's main stack), the slot holds what the previous frame's **Engine::Render** `0084f060` left
there. Measured on the paired Concord ride with only the event start pinned (pair.sh `EVENT=1 PAIR_RESEED_EVENT=only`, where the
riders take other lines than with the per-frame RESEED): AI slots 9 / 11's wheels touch from k 770; the original's words are
-0.7131 / -0.7141 / -0.7199 / 0.1772 (`3e3568ea`) / 0.1787 ..., the product's 2048.0 (`45000000`) or a compare mask (`ffffffff`);
at k 774 (product frame 1635, oracle 1979) the step changes the contact (point x `44025530` vs `44025619`, depth 7.6e-6 vs 0.0152)
and slots 9 / 11 part by +-0.006 ft (equal and opposite; controls, LCG and CRT still identical; the LCG follows at k 821). The
emulator's write watch (`RX_PAIR_WATCH=1978:0022f110:<file>`; a scratch CLI also logged the exact instruction, the registers and the
whole call chain) names the last writer before the read: `007a2757` (`movss [ecx - 8], xmm0`) in FUN_007a2300, the transposed copy of
a skinning matrix, under Engine::Frame -> Engine::Render -> UIContainer::vf12 -> `007af510` -> `007e0860` -> `007afb00` -> `00793fb0`
-> InstancedAnimationEntity::vf0_o98 `00413840` -> `007cfe20` -> Model_RenderInstance `007f5680` -> XenonTriangleSurface::vf2
`007b2530` -> vf3 `007b2570` -> `007a54a0` -> `007a53a0` -> `007a4fd0` -> [vtable + 0x44]: the CPU side of the last instanced
animation entity drawn in frame k 773. Nothing writes the slot between that store and the read (the watch over both frames).

Why this is not a product bug, and not portable: the product has no render pass (RENDER BOUNDARY, loop_engine.c `engine_render`:
Unity draws), so the slot keeps what its own frame left; the word depends on what the camera sees and on the draw order of the
scene's instanced animation entities. It is not even the real game's value: the oracle's Direct3D 9 device is the boot's recording
stand-in (boot_devices.inc: host natives that never touch the guest stack, nothing drawn), while in a real process every device
call after that store (the remaining draws, Renderer_EndFrame `007ae3a0`'s EndScene and the swap chain's Present) runs d3d9.dll and
the driver on the same stack. Proof that this residue is the whole difference: (1) the oracle's word written at that one slot at
every CylinderVsCylinder call of the product (`PARITY_CYFIX=<oracle CY log> PARITY_CYIDX=all PARITY_CYLO=-27c PARITY_CYHI=-278`,
core/tools/parity/cyl_log.inc) gives an identical ride over all 1,540 aligned frames; (2) both sides' main stack below the frame
poisoned with the same word at every frame start (pair.sh `POISON=<word>`: product PROBE_STACK_POISON, oracle
RX_PAIR_STACK_POISON at Engine::Update's entry) gives an identical ride (5,140 of 5,140 aligned frames with N=6000), while the
poisoned original against the plain original parts at k 774 itself: the original's own trajectory depends on its render residue.
With the per-frame RESEED the riders take other lines: 14 of the 248 CylinderVsCylinder calls of the EVENT=1 ride still read a
render or other cross-frame word on the original's side (6.7e-24, -0.158, ...; the product's: `ffffffff`), but those calls'
results do not change that ride (identical over all aligned frames): it stays identical by chance. Verify on the pin-only ride (oracle, 2,400 frames): `0086bd80` 0 / 244,
`0081c590` 0 / 976 (also with `REFLEX_PORT_DEAD=1`), `008732d0` 0 / 122,577, `0085ddd0` 0 / 2,530,010.

**Rider_CrashUpdate `00425600`'s ECX is dead (w-parity2).** The one Verify mismatch of 2,239 (ECX 18a5f810 vs bf8255cd, the
crash sound's audio-boundary calls leave it) has no reader: the only caller, RiderAnimation::vf19 `0061c210`, pushes ECX at
`0061c581` as the placeholder of 00425800's float argument and overwrites it with `fstp [esp]` at once, and `00425800` loads ECX
(`00425864`: `mov ecx, [esp + 0x14]`) before reading it. An ABI artefact with no effect; the adapter's OUT_ECX is kept (it
documents the register, and 00425640's ECX is live).

**ECX / EDX of the pair generators are live.** The dispatcher returns straight after the generator, and CompositeVsComposite (the
original) reads the EDX left. EDX is M after a contact, or the last helper's EDX / 0x92df24 without one.

**The On-mode memory hash covers the dead stack.** Originals called through `rx_call_original_ex` save the caller's EBX / EBP / ESI /
EDI, and sometimes EAX / ECX, in their frames. Zeroed input registers therefore change the hash. For example, `007c5120` made
Alpine's On mode differ until the caller's registers were passed through. Track the registers the original has at each call:
start from the adapter's `m.r`, then copy each call's output registers into the next call's input.

**Set `fpu_in = 0` after `in = out`.** The output struct of `rx_call_original_ex` does not set `fpu_in`. If it is copied into the
next call's input, the emulator pushes garbage x87 values and overflows the x87 stack.

**Nested native calls need the original's ESP.** A native callee computes its frame from `rx_sp()`. Use
`CALL_AT_SP(cur, nargs, ...)`, which sets the entry ESP to `cur - 4n - 4`. `rx_call_original_ex` pushes its own arguments, so call
it with the stack pointer at the frame base.

Example: the CompositeVsComposite dispatcher call ran at the wrong ESP and gave 9 / 1 mismatches on SX / Terracross.

**Operand order.** Multiplication and addition are commutative exactly. Only these matter:

- the tree shape of a sum;
- the order of a subtraction;
- NaN propagation.

`maxps` / `minps` keep the second operand unless the first is greater / smaller. `phys_max` / `phys_min` follow the same rule.

**Mask sums.** A three-lane test is true when `lb(2) + lb(1) + lb(0) == -3`. A splatted compare is false for NaN.

**`comiss` + `jb`.** The jump is taken on less-than or unordered. `ucomiss` + `lahf; test ah, 0x44; jp` skips the branch unless
the operands are equal and ordered.

**x87.**

- The main thread uses control word 0x027f (53-bit precision).
- `_CIatan2` goes through `crt_ci_atan2(y = st1, x = st0, CRT_CW_MAIN)`, then is stored as a float.
- The cell-count rounding in the terrain code is an x87 add at 53 bits.

**Writable "constants".** The exe writes these at run time, so read them from memory:

- 0x9cd9e8 = -1, 0x9d7b30 = -2, 0x9ddfdc = 16;
- the unit, zero and one vectors at 0xd615c0 / 0xd616a0 / 0xd61680 / 0xd61670 / 0xd61650 / 0xd61660 / 0xd61690;
- 0x9cd9e4 = 4.5;
- the scalar 0xa95d50.

Some double constants are not the float they look like. For example, 0x977d48 = 0.18000000342726707 in `00425b20`, which is not
`(double)0.18f`. Read them with `rx_bd(rx_rd64(addr))`.

**cvttss2si.** Out-of-range and NaN inputs give `INT_MIN`; see `trunc_ss` in `phys_rider.c`. Both the packed and the scalar cosines
reduce the angle through it.

**Tiny pure helpers ported in place.** These are `008070a0` (sound gain) and `Float_Abs 00401000` (an and with 0x977570).

## MSVC 9 std::vector<T*>: `_Insert_n` 0080d280 / `_Reallocate` 0080d860 (phys_coll_misc.c)

The vector object is `{+0x00 proxy, +0x04 ..., +0x0c _Myfirst, +0x10 _Mylast, +0x14 _Myend}`; the checked iterator is
`{proxy +0, position +4}`, and `[proxy]` is the vector address. `_Insert_n` (EAX = vector; stack: result iterator buffer,
value*, `_Where` proxy, `_Where` position; RET 0x10) checks `_Myfirst <= _Mylast` and the `_Where` proxy (when the element
range is non-empty), computes the element index and calls `_Reallocate`. `_Reallocate` (vector, value*, proxy, position)
does both forms: with room it copies the last element to the new end through `memmove_s`, shifts the tail up one slot
(`memmove_s`, IAT 0x916200) and fills the hole; otherwise it grows to `capacity + capacity / 2` (at least `size + 1`),
allocates `4 * n` through 0042bf30 -> ReflexAlignedAlloc 0078fbe0, `_Umove`s `[first, where)` and `[where, last)` around
one `_Uninitialized_fill_n`, frees the old block (0078fc50) and updates the three pointers. `phys_vector_push_back`
(008423d0) calls it when the vector is full. `_invalid_parameter_noinfo` (IAT 0x916224) is `crt_invalid_parameter_noinfo`.
Verified with 244 `_Insert_n` calls during the baton Concord event build (0 mismatches) and 509 direct `_Reallocate` calls
(both the grow and the in-place branch, 0 mismatches); `_Xlen`/length_error is unreachable.

### `0085c8b0`: a force in the mechanism body's local frame (sol_ride_stop, 2026-10-21)

The current-tree 24,000-step W+A/D ride stopped at race frame 16,705 in `0085c8b0` (239 bytes), reached by the brake arm
of `00446340 BikeEngine_DriveForces`. Hand port `phys_mech_add_body_force` in `phys_mech.c`, prototype in `phys_mech.h`,
adapter `oracle/phys_mech.cpp` (rx_phys); `bike_force.c` and `bike_sim.c` now call the owner's function directly.

- ABI: ECX = mechanism M; stack body index, force vec4 pointer; RET 8. EAX = M + index * 0xa0, ECX = the force pointer,
  EDX unchanged. The original's aligned frame saves only EBP: no locals, addressed stack data, calls or x87 operations.
- Disassembly 0x85c8b0..0x85c99e is authoritative: load q from M + 0x660 + 0xa0 * index, conjugate it by multiplication
  with (-1,-1,-1,1), mask force.w with 0x92db80, then q * ((F & xyz) * conj(q)), add all four resulting lanes to
  M + 0x70 + 0xa0 * index. Existing `quat_mul` has the exact packed shuffle/product/add order, including NaN operand priority.
- Oracle argument probe `oracle/phys_local_force_probe.cpp` (hook `rx_LocalForceProbeHook`, `RX_LOCAL_FORCE_PROBE=1`,
  Concord frame 300) uses private mechanism storage, all four body indices, identity/quarter-turn orientations, finite forces,
  signed zeros, infinities, signaling/quiet NaNs, and aliases of the input force to the accumulator or quaternion.
  **256 calls / 256 verified / 0 mismatches**. Original-only coverage **67/67 instructions**, 0 never.
- Evidence and scripts: `builds/sol_ride_stop/` (`probe.sh`, `ride.sh`, `logs/probe_verify.log`, `logs/coverage.txt`).
  Oracle binary copied from the final flag-verification build (same current physics sources), SHA256
  `f6bed7fb87ee08e0a69cf6b23d21acb9a4a80618fd25ebc6618fe0dc79179165`; original install remains read-only.
- Product ride using parent's clean final dylib SHA256 `feffca311c546d73e6b381db35f4640517021fbdd91fe0e858daa38664f9a302`:
  **24,000 steps / 22,831 racing frames / session frame 24,009 / game 400.130 s**, **33/33 checks pass**, stop none.
  Maximum speed 80.9997, gear 4, path 8750.436; final position (280.9984, 140.8739, -489.7126), state hash
  `d581c691ba157eb7`. The prior race-16,705 stop is gone; no following physics stop occurs in this run.
- No regression: `rx_phys quick` **130 ports / 0 mismatches / On identical** (state `1f6611fa80b80831`, memory
  `1487a52f208b4b26`).
- `rx_bike quick`: **20 ports / 0 mismatches / On identical**, same reference hashes. The focused local-force probe's
  On-vs-original run is also identical (state and memory hashes in `logs/probe_on.log` / `logs/coverage.log`).
- Final getter-fix regression (parent's `final-pose` dylib, SHA256
  `abecbfce714824646d548a804e5fbc94dfc512c7a99502d7ae5281a2585842cb`): another **24,000 steps / 22,831 racing frames,
  33/33 checks, no stop**, same summary and state hash. **24,000/24,000 complete per-frame TSV rows are bit-identical** to
  the preceding ride, including the requested first 5,000 rows; body-0 position/velocity, speed/gear, race/camera/input,
  deformation/path/steering values all match. Logs `ride_final_pose.out/.tsv`, `compare_first5000.txt`, `compare_final24000.txt`.
