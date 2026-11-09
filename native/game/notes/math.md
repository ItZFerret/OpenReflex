# math group: notes (math agent, 2026-10-18)

Range: 0x00420000..0x0042ffff without `RavenPhysics::` (mechanism agent), and 0x008f0000..0x008fffff (the exe's own CRT pieces;
the MSVCR90 thunks `MSVCR90.DLL::memset` / `memcpy` are imports and stay as they are).

## Files

- `ports/math_util.h`: shared helpers of the group.
  - `X` / `Regs`: register-exact SSE. An xmm register is four raw lanes; there are packed, scalar and double ops with the x86
    NaN rule, shuffles, compares, conversions and `cvtss2sd` / `cvtsd2ss` with x86 NaN payloads.
  - `Flags` / `f_*`: EFLAGS for transliterated integer code.
  - `Cpu` / `CPU_REFS`: the original's registers as locals, with the stack in guest memory. Calls go through the machine with every
    register synced, `tail` handles tail jumps, and x87 uses `fst_` / `fpush` / `frc` over the machine's f80 stack.
  - `Stack`: writes the original's dead-stack bytes (see dead-stack.md).
  - `call_regs`: calls an original from a Regs-level port.
- `ports/math.cpp`: hand-written SIMD kernels.
  - 0042c240 `Vec4Sin_RangeReduced`: the lead's port, now a `vec4_sin(Regs&)` so the other kernels can call it.
  - 0042c650 `Vec4Atan`, 0042c880 `Vec4Atan2`, 0042ca00 `QuatSlerp_Simd`, 0042ccd0 `Vec4Floor`.
- `ports/math_quat.cpp`: 0042ce40 `QuatSlerp_Packed`, the packed animation quaternion slerp.
- `ports/math_crt.cpp`:
  - 008fa106 `_ftol2` and 008fa0d0 `_ftol2_sse` (f80).
  - 008f9efe `__security_check_cookie`.
  - The libm wrappers 0042c230 `SqrtF`, 0042c1d0 `CeilF` and 0042c1f0 `PowF`.
- `ports/math_rider.cpp`: 00426a50, RiderAnimation::vf19's bone pose capture.
- `ports/math_vec.cpp`: 0042db20, packed quaternion / cross product code with memory operands.
- `ports/math_gen.cpp`: the rest of the range, transliterated one instruction per line. These are the rider / bike skeleton
  bookkeeping, std::vector-style accessors with `_invalid_parameter_noinfo` checks, SimulationClock_Advance, the 00425600 ->
  00425640 -> ... rider update tree, and others.
  - Each line is one original instruction, shown in the line's comment.
  - Registers are `Cpu` locals. Memory, the stack included, is guest memory, so dead-stack effects match too.
  - Jumps are `goto`s and calls go through `k.call`.
  - They are exact by construction. They are not yet readable C++: each still has to be lifted into structured code.

## Conventions found (LTCG)

- The SIMD kernels pass everything in xmm registers: atan in xmm0, atan2 as xmm0 = y and xmm1 = x, slerp as q0, q1 and t in
  xmm0-2. Results come back in xmm0, and xmm1-7 are scratch. The callers only read xmm0, but the ports reproduce all eight
  registers anyway (`OUT_XMM_ALL`), so the ports can be composed.
- 0042ce40 takes EAX = packed quaternion a, ESI = packed quaternion b and EDI = output, plus a stack float t (cdecl).
- 00426a50 takes EDI = the object. It calls Node_EvaluateWorld and 00401740 (Matrix3_ToQuaternion into [ESI]) with the world
  matrix in its own stack frame.
- 0042db20 takes ECX and EDX as pointers plus 4 stack arguments, and returns with `ret 0x10`.
- `_ftol2_sse` is the path the game takes: the SSE2 flag at 0xdcf760 is set, so `_ftol2` itself is never called. That port is
  only exercised through `_ftol2_sse`'s fallback.

## Traps

- **Dead stack is not always dead.** QuatSlerp_Simd's locals and its nested atan2 / atan frames are read later as uninitialized
  data by some code. That code copies one stack temporary into every element of an array at 0x14280880..0x142d3xxx. Without
  those writes the memory hash differs, although Verify shows 0 mismatches and the bike state is identical. So the
  `vec4_atan` / `vec4_atan2` / `quat_slerp` / `vec4_floor` ports write their frames with `Stack`. See dead-stack.md.
- **Ghidra's function sizes can be short.** 00423df0, 00423b80, 00425b20, 004223a0, 00426930 and 00426d00 lose their last
  instructions (mov esp, ebp / pop ebp / ret) in functions.tsv. The transliterations are generated over the real extent: from the
  entry to a ret / jmp with no pending forward branch target.
- **Ports called from a native are not verified.** In Verify mode the calls a native makes run the original, so their own ports
  are neither installed nor counted. To verify them one by one, also run `verify.sh <boot> math,-00425600`, which keeps the top
  of the rider tree original.
- **Tail jumps.** 00425600 ends in `jmp 004252a0`. Returning the jump target from a native breaks Verify ("native returned to
  another address"). `Cpu::tail` pops the return address, runs the callee as a call and then returns to the caller.
- **The IAT is read at run time.** Calls through `[0x916224]` (`_invalid_parameter_noinfo`) and the other import slots read the
  slot from guest memory, never from the file image.
- **.data constants are read from guest memory.** These are 0x9cd9e8 (-1.0), 0x9c6400 (1/32767), 0x9ddfc8 (cos 45°), 0x9cf5a8,
  0xd615c0 and 0xd61670.

## Left unported

- 008fa230 `__alloca_probe` (about 21 instr/frame, found by the whole-frame coverage). The port was written and is exact in On
  mode: memory hash identical. Verify cannot check it, though. It returns the caller's return address in EAX and copies it to
  the new stack top, and in Verify the original runs under the harness's return sentinel (0). So EAX and that word always differ
  (eax 005cc67a vs 0). Kept original for now; the port is in the math agent's report if the harness learns to hand the original
  the real return address.
- `MSVCR90.DLL::memset` / `memcpy` (008fa06a / 008fa070): import thunks, not game code.

## Declared outputs

- Ports whose call tree stays inside the math group, or reaches only originals and CRT imports, declare
  `OUT_XMM_ALL | EAX | ECX | EDX`. They reproduce every register exactly.
- Ports that call code of another group declare only what their callers really read. That is 20 math_gen functions plus
  QuatSlerp_Packed and 00426a50: outputs 0, except OUT_EAX for 00424340 and 0042de70. The reason: their final scratch registers
  are whatever that callee's port leaves. Example: collision's BroadphaseGrid_Update 0084a1a0 (outputs 0) leaves floats where
  the original leaves cvttps2dq ints, and that showed up as mismatches in 00429910 / 0042b000 in the lead's merged run.
- How the caller reads were found: the math agent's live.py scratch tool, which scans every call site for registers read before
  being written, conservatively. Each hit was then checked by hand. Almost all were partial writes (`movss xmm, xmm`,
  `cvtss2sd`, `cvtsi2ss`), the gs-failure path, or `push ecx` used to reserve a slot.
