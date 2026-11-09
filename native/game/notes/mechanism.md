# mechanism: the RavenPhysics articulated-body solver (0x00852000..0x0085ffff, plus RavenPhysics:: 0x0042xxxx vfuncs)

All 91 functions of the group in `plan/physics-functions.tsv` are ported (files `ports/mechanism*.cpp`, helpers
`ports/mechanism_util.h`, cross-file kernels `ports/mechanism_kernels.h`). Verified with `tools/verify.sh <boot> mechanism`.

## Files

| File | What |
| --- | --- |
| `mechanism_util.h` | xmm lane helpers (shuf / movss / perm / bcast / and / cmp ...), `dot3` / `dot4` in the exe's reduction order, `Frame` (guest stack locals), `callr` (calls with LTCG register inputs, restores EBX / ESI / EDI), `vcall` |
| `mechanism_matrix.cpp` | Matrix3x3 / 3x4 / 4x3 / 4x4 / 4x6 / 6x4 / 5x6 and Vec3 / Vec6 virtual methods (hand-written) |
| `mechanism_kernels.cpp/.h` | straight-line matrix kernels (generated, callable from C++): 3x3 / 3x4 / 4x3 / 4x4 / 6x6 products, quaternion -> matrix, skew matrices |
| `mechanism_core.cpp` | per-node passes of the step: node slot loops, clears, force composition 0085ca40, velocities 0085d270, poses 0085d100 |
| `mechanism_solver.cpp` | preparation 0085cb70, solve 0085cf80, the shape dispatchers 00853900 / 00857590 / 00855740 / 00858fe0, 00853740, 00857b30 |
| `mechanism_constraints.cpp` | ConstraintSlidingRotor / ConstraintFixedBody vfuncs 1..9 (generated) |
| `mechanism_products.cpp` | inverses 3x3 / 4x4 / 6x6 and block products (generated) |
| `mechanism_forces.cpp` | 0085c770 force at a point, 0085c530 rotate the mechanism |
| `mechanism_misc.cpp` | 00852f40 axis rotation, 00853180 1D noise (returns XMM0), 008523b0 segment / plane, 0085dcf0 contact average |
| `mechanism_dispatch.cpp` | 0085ddd0 collision pair dispatch, 0085ee70 triangle batch test (generated, branchy) |

## Data layout (from the code)

- Mechanism M = simulation + 0xd0: +00 gravity, +1c signed node count, node i at stride 0xa0: linear body +20 (pose +20,
  velocity +30, mass +40, inverse mass +50, resolved force +60, applied force +70, impulse +80, acceleration +90), angular body
  +660 (orientation +660, angular velocity +670, inertia +680, inverse inertia +690, resolved +6a0, applied +6b0, impulse +6c0,
  acceleration +6d0). Node objects (constraints) at M+ca0 + 4 i (root i = 0).
- Node: +0c parent index, +10 child count, +14.. child indices, +30 / +34 bodies, +38 .. +68 matrix / vector objects
  (+44 articulated inertia K, +48 K^-1, +4c / +50 / +54 / +58 / +5c / +60 couplings, +64 bias b, +68 z), slots 8 / 9 return the
  child's contribution (00858fe0 into this+800 / +910 for the rotor, +bf0 / +d00 for the fixed body).
- Matrix objects: +0 vtable, +4 rows, +8 cols, rows of vec4 from +10. Block matrices hold sub-matrix objects (each with its own
  vtable) and forward to their virtual methods. Every lane is stored, W included; W often repeats lane 2 or lane 0 (see each
  kernel), never "don't care".
- Globals read at run time (not constants): 00d61670 (zero vec4 used for clears), 00d61660 (zero, pose omega W), 00d615c0 /
  00d616a0 / 00d61680 (diagonal selectors of the inertia fill), 0092db80 (xyz mask), 0093a1f0 / 200 / 210 / 220 (quaternion
  sign vectors), 0092dc20 / 0092dc30 (inf / NaN vec4 of the normalizations).

## Conventions and traps

- Almost every function has an LTCG convention: ECX / EDX / EAX / ESI / EDI / EBX inputs, stack args with caller cleanup,
  `ret` without pop although args were pushed. `scratchpad .../mech/inputs.py` lists a function's register inputs, `live.py`
  what callers read after a call.
- Callers rely on registers the callee does not touch: e.g. 00854f00 reads ECX after 00854650 (`mov eax, [ecx]`), 0085b8b0
  reads EDX after 00857c90, 0085a3d4 keeps XMM7 across 00854650, 00853900 / 00857590 preserve ECX (push / pop). A native must
  leave untouched registers untouched and restore what it changes for a call (`callr` restores EBX / ESI / EDI; the ports set
  EAX / ECX / EDX to the original's final values where those are known).
- 00853180 returns its result in XMM0 (callers do `movaps xmm7, xmm0`): the port writes XMM0..XMM4 exactly (upper lanes 0).
- 00858fe0's Ghidra size (88) is wrong: the function continues through its jump table to 00859225.
- Read-modify-write through memory: the transposes (00858220, 00858440) and the skew builders (00857ff0 / 00858090) read
  back the destination they just stored; aliasing between arguments is possible, so the ports keep every load at its program
  point (the generated code does this mechanically).
- Operand order of products matters for NaN payloads (x86 returns the first NaN operand): the kernels alternate `mul(row, x)`
  and `mul(x, row)` within one function (e.g. 00854cb0, 00854ac0's last row); the generated code keeps each one.
- 0085cf80: the node after the backward loop is node min(count - 1, 0) (EBP leaves the loop at 0, or is count - 1 when the
  loop did not run); the root solve always uses node 0.
- 0085d270 is called with the dt still live in x87 ST0 (`fst`, not `fstp`): the port does not touch the x87 stack.
- Stack locals passed to callees are placed at the original's own stack addresses (A = (ESP0 - 4) & ~15 for the
  `push ebp; and esp, -16` frames) and every call runs with ESP where the original has it before its pushes, so callees see
  the same stack as in the original (relevant for dead-stack reads).

## Generated code

Branch-free and loop-free functions were translated by symbolic execution of the original instructions (tools in the scratchpad
`.../mech/`: `sym.py` straight-line leaves, `sym2.py` with calls and stack frames, `sym3.py` with branches and jump tables; the
lists `constraints.list`, `products.list`, `dispatch.list` and `genkernels.py` regenerate the files). The output keeps every
guest load / store / call in program order per path, each lane's operation order, and groups lanes into V4 operations where all
four lanes do the same operation. It is exact by construction (and verified), but not yet "semantic" C++: the constraint vfuncs
and the 0085ddd0 dispatch are candidates for a hand-written rewrite once the state moves out of the original layout.
