# Collision group ("collision"): collision shapes, broadphase, contact pairs, terrain

Ports: `native/game/ports/collision_*.cpp` (group string `collision`), helpers `collision_util.h` (xmm lanes as the guest holds
them, guest stack frames), `collision_terrain.h` / `collision_shapes.h` (callable C++ forms of the group's functions),
`collision_lift.h` (register-level transliteration support, below).

## How the group is ported

Two forms, both exact (verify.sh: 0 mismatches, "on: identical"):

1. **Hand-written** (`collision_shapes.cpp`, `collision_grid.cpp`, `collision_terrain.cpp`, `collision_pairs.cpp`): the
   function's logic as C++ on guest memory, operation order and operand order as the original (NaN propagation follows the
   first operand), helpers of other groups called with `Guest::call`.
2. **Register-level transliterations** (`collision_lifted.cpp`, generated): one C++ statement per original instruction on
   `collision::Cpu` (registers, flags as the interpreter computes them, the original's stack frame on the guest stack). Used for
   the large LTCG functions (pair contact generators of several KB, terrain ray traversal, tracks). They reproduce every
   register, flag and stack write of the original, so they are declared with all outputs (`OUT_EAX | ECX | EDX | XMM_ALL | FPU
   | FLAGS`). Calls inside them go through `Machine::call_function` at the original's ESP (the return-address slot then gets
   the original's return address). Their declared outputs are the registers the direct callers read after the call
   (scratchpad `collision/outputs.py`: a scan of every call site; vfuncs without direct callers: eax + x87). They are meant to
   be rewritten by hand into structured C++ function by function (verify keeps them honest); the generator is the
   scratchpad's `collision/lift.py` + `gen.py` (ask the lead to move it into tools; it could serve other groups too).

Lifted (51): the pair contact generators (CollisionPair_* of the dispatcher 0085ddd0: sphere / box / cylinder against each
other, triangles and the height field, composite vs height field), their helpers (FUN_00860e90, 008639f0, 00873540, 00846f10,
00846c70, 00846230 / 00847630 world vertices, 00845b50 / 00845f90 box queries, 008472d0), Triangles_CircleContact /
Triangles_DynamicRayContact, the terrain segment query (007c6cc0 and its traversal 007c73e0, 007c7350, 007c83b0, 007c7d10),
the other terrain queries (TerrainMaterialAt 007c5120, 007c5950, 007c5aa0, 007c5c90, 007c66f0), TerrainTracks_AddPoint /
CutStrip / AllocSlotDefault, DeformablePatch_AddStamp, FUN_00840440 / 00840b70 / 00841c90 (callers 0061ca60 / 00620a60 /
006b6d80: large SSE + double kernels, not collision proper), StaticBox::vf3, StaticComposite::vf8, Composite::vf2 / vf4 / vf5,
and small container helpers (00690e00 hash lookup, 008423d0 vector push_back, 00842fa0, 007cf450, 0084fe00, 0084fe70).

## Conventions found (LTCG custom conventions; read every caller)

| Address | Name | Convention |
| --- | --- | --- |
| 008455f0 | CollisionShapeBox::vf2 | thiscall (q*, pos*, out min*, out max*), RET 0x10: world AABB, `q * h * conj(q)` of the four corner sign patterns |
| 00846700 | CollisionShapeDynamicCylinder::vf2 | as the box, axis vectors from radius +0x20 and height +0x24 |
| 00845480 | CollisionShapeCircle::vf2 | pos -/+ the radius vector +0x20 |
| 00845a70 | CollisionShapeBox::vf3 | tail call of the object's vf2 |
| 00848340 | CollisionShapeComposite::vf3 | union of the children's vf3 at their world transforms; starts at (FLT_MAX, -FLT_MAX) |
| 00848650 | CompositeChildRotation | EDI composite, ECX child index; (q*, out*) RET 8: q * child rotation (composite+0x44) |
| 008486f0 | CompositeChildPosition | EDI composite, ECX index; (q*, pos*, out*) RET 0xc: q * offset(+0x5c, w masked) * conj(q) + pos |
| 008732d0 | CompositeVsComposite | thiscall manifold M; (A*, qA*, pA*, B*, qB*, pB*, swap) RET 0x1c |
| 008656c0 / 00872860 | ShapeVsComposite(B) | ECX rotA*, EDX posA*; (M*, shapeA*, composite*, q*, pos*, swap) RET 0x18 |
| 0069b2f0 | Vec4FloorToInt | xmm0 in / out (int lanes); eax / ecx / edx = lanes 3 / 1 / 2 |
| 0069b400 | Vec4StoreAligned | xmm0, cdecl dest*: stores unless null; xmm0 keeps its low half (two movq) |
| 007c5440 | TerrainSampleWindow | ESI cell work, EDI center out; cdecl (neighbours out*, grid*, scale*) |
| 007c51e0 | TerrainCellSetup | ESI work, ECX extents*, EDX position*; cdecl (cells*): returns 1 inside the grid |
| 007c52f0 | TerrainCellOffsets | ESI work |
| 007c5e80 | TerrainHeightNormal | thiscall normal out*; (terrain*, point* in/out) RET 8 |
| 007c6160 | TerrainTriangles | ECX query point*; (terrain*, triangles*, count*, capacity, extent*) RET 0x14 |
| 00882170 / 00882290 / 00882300 | Int4GreaterMask / Int4Min / Int4Max | xmm0, xmm1 in, xmm0 out (signed int lanes) |
| 008821e0 | Int4BelowFlags | xmm0, xmm1, EDX flags*: masks xmm1 <u xmm0; flags word bit 7 all, bit 5 none |
| 00882d90 / 00883020 | BroadphaseGrid_QueryCells / QueryBoxes | ESI grid, ECX max* (d90) / EDX min* (020); stack: other corner; RET 4 |
| 008826d0 / 00882980 | BroadphaseGrid_Insert / Remove | ESI grid; stack id; RET 4 |
| 0084a1a0 | BroadphaseGrid_Update | EDI id; (object*, min*, max*) RET 0xc |
| 00848800 / 00848910 | CompositeSetChildPosition / Rotation | ESI composite; ECX world pos*, EDX body pos*, (index, q*) RET 8 / EDX index, ECX world rot*, (q*) RET 4 |
| 00845a80 | CollisionShapeBox_Corners | thiscall out[8]*: local corners (half extents times sign patterns; -1 the runtime 0x9cd9e8) |
| 00845f20 / 00846bb0 | Box / Cylinder _ContainsPoint | ECX point*; (shape*) RET 4 -> eax |
| 008454d0 | CollisionShape_ScaleToRadius | EDX shape, ECX direction out*; (v* in/out) RET 4 |
| 007c5380 | TerrainSampleCell | ESI grid, EDI cell work, ECX scale*; cdecl (out*); leaves xmm0..6 as the original |
| 007cda90 / 007cda50 | FloatList_Push / FloatPairList_Push | EAX list (+ xmm0 / two stack floats) |
| 0088bd90 / 0088bd50 / 0088be10 | Int64_Load / Assign / Add | thiscall on an int64 |
| 00843fc0 / 00843ff0, 00844050 / 00844080 | Sphere / DynamicCylinder ::vf4 (scale, saving the size) / ::vf5 (restore) | thiscall |

Broadphase grid layout: see the header of `collision_grid.cpp`.

Terrain object: base heights +0x1c, deformation surface (D3D texture stand-in) +0x240 (LockRect slot 13 with flags 0x810,
UnlockRect slot 14), gain +0xbc, stride +0x44bc, cells +0x44c8/+0x44cc (floats), world size +0x44d0/+0x44d4, spacing +0x44e8,
normal y +0x44ec. Height = ((deform - 0.5) * 2 * gain) + base * 2048.

## Traps

- **Uninitialized stack.** Several pair generators copy vec4 w lanes nobody wrote (e.g. FUN_00846f10's three result vectors
  at its frame +0x5c/+0x6c/...; FUN_0086bd80 passes them into the contact manifold). The values are whatever earlier code left
  there, and they change results (the manifold count differed). So a port that calls other code must make those calls at the
  original's ESP: `Frame(g, sub, pushes)` in collision_util.h puts the frame base where the original's is, locals at the
  original's offsets, and calls are made from there. Found with a scratchpad detector that runs a function twice with two
  different dead-stack poisons (journal-restored) and reports result differences (FUN_0086bd80: 53 of 248 calls,
  FUN_00846f10: all, on Terracross).
- **min / max with NaNs**: the ports follow the hardware rule (the second / source operand, unquieted, when either is a NaN;
  collision_util.h min_bits / max_bits). The interpreter had returned the quieted first NaN (fixed by the lead in vm.cpp,
  2026-10-18; the JIT already selected the source).
- `fld m32` / `fstp m32` pairs are copies except that a signaling NaN comes back quiet.
- Several functions read writable globals at run time: -1.0f at 0x9cd9e8, runtime vectors 0xd615c0 / 0xd61660 / 0xd61680 /
  0xd61690 / 0xd616a0 (never folded into constants).
- 00882980 floors the box a second time into dead locals (no effect; the hand port does not repeat it).
- CollisionShapeComposite::vf3's prefetch index is never advanced in its main loop (only bounds checks; kept).

## Status (2026-10-18)

All 95 functions of the group in `plan/physics-functions.tsv` are ported (7,162,106 original instructions per frame in the
plan): 44 hand-written, 51 register-level transliterations (`collision_lifted.cpp`). verify.sh "collision" and
"math,collision": 0 mismatches and "on: identical" on the five worlds. On-mode frame times equal the interpreter/JIT's.

Still original in the address ranges after `REFLEX_PORT=collision` (coverage, Terracross, frames 100..400), none of them in the
plan list: FUN_00840040 (~66k instructions per frame, next to the 00840440 / 00840b70 kernels), TerrainTracks_FlushPending
007cbe30 (~2.5k), FUN_00843350, FUN_008435f0, HFTerrainSurface::vf14 007c4fa0, HFTerrainEntity::vf19 / vf25,
DeformablePatch_ReadbackToSysmem 007cb930, the JobPool / Database / Engine code at 0x0084xxxx / 0x0088xxxx, and the
particle code at 0x0069xxxx (other groups).

Tools (scratchpad `collision/`): `lift.py` / `gen.py` (the transliterator), `outputs.py` (registers callers read after a call),
`callsites.py` (call sites with context), `probe/collision_probe.cpp` (the uninitialized-stack detector), `pverify.sh`
(verify.sh's checks with the 15 runs in parallel), `vq.sh` (Verify only, five worlds in parallel, N frames).
