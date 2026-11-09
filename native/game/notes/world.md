# World group: the physics world and vehicle layer around the simulations

Group string `"world"`. Files: `ports/world_quat.cpp`, `world_phys*.cpp` (physics world, 0x0076xxxx-0x0077xxxx, by the world
agent), `world_veh*.cpp` (0x0040-0x0041, 0x0046-0x0047), `world_rider*.cpp` (0x004d-0x0062), `world_wreck*.cpp`
(0x0063-0x0068, 0x006a-0x0075), `world_node*.cpp` (0x0078-0x0083, 0x0085, 0x0089-0x008b); helpers `world_util.h` (shared),
`world_lit.h`, `world_<tag>_util.h`. The sections below the first come from the helper agents' notes.

## The physics world (0x0076xxxx-0x0077xxxx)

`World_PhysicsUpdate` 00769b80 (EAX = world, float dt; ret 4): `World_PreListeners` 007699e0 (EDI = world), the simulation
clock 0042ef70 (math group), `World_SolverDispatch` 0076a010 (EAX = world), `World_PostListeners` 00769ab0 (ESI = world).
Float arguments are passed on with `fld dword / fstp dword [esp]`: bit-exact except that a signalling NaN comes out quiet
(`x87_pass`).

World layout (the physics context, `world`):

| Offset | Meaning |
|---|---|
| +0 .. +0x18ff | contact events, 100 x 64 bytes: +0 id A (object +0x34), +4 id B, +0x10..0x18 contact point, +0x20 byte type (1 = terrain second set), +0x24 / +0x28 displacement of A / B, +0x2c output, +0x30 / +0x34 feature ids |
| +0x1900 | contact event count (reset by the pre-listeners; post-listeners call every handler of +0x1934 when nonzero) |
| +0x1914 / +0x1918 | physics listeners (vfunc 0 before, vfunc 1 after the step, float dt) |
| +0x1934 / +0x1938 | contact handlers (vfunc 0 (world)) |
| +0x1940 / +0x1944 | simulations (vfunc 1 (dt) = the vehicle fixed-step scheduler 00430eb0) |
| +0x1954 / +0x1960 / +0x1964 | generic dynamic objects (vector header +0x1954; 0xe0-byte records) |
| +0x196c / +0x19ac / +0x19cc | broad-phase cursors (generic / keyed / static): [c] position, [c+0x30] grid: indices at +0, count +0x400, boxes +0x41c / +0x420 (16 bytes per object), ... |
| +0x1980 / +0x1984 | rider bodies (0x80-byte records) |
| +0x1994 / +0x19a0 / +0x19a4 | keyed objects (vector header +0x1994; 0xe0 bytes; enabled byte +0xb0, kinematic index +0x80, flags +0x84) |
| +0x19c0 / +0x19c4 | static objects (64 bytes: rotation +0, position +0x10, shape +0x28, id +0x30, enabled byte +0x38) |
| +0x19d4 / +0x19d8 | active kinematic bodies: count (max 200) and pointers |
| +0x1d00 + 0x50 k | kinematic pose: position +0, velocity +0x10, force +0x20 |
| +0x5b80 + 0x50 k | kinematic orientation (quaternion) +0, angular velocity +0x10 |
| +0x9a00 + 0x20 k | kinematic water state (+0 touching, +0x10 push direction, +0x1900 float, +0x1910 impulse) |
| +0xb300 | 200 contact-sound timers (40 x 5 floats, 0x20 apart), advanced by dt below 1 |
| +0xcc00 | contact-handler list (checked vector) compacted and sorted every step (00768750) |
| +0xcc9c | terrain (its +0x31ec: the terrain collision) |
| +0xccb0 / +0xccd0 | water shape / water bodies list |

Generic dynamic object (0xe0 bytes): +0x20 material (+0x70 / +0x80 timers, +0x90 / +0xa0 sleep thresholds, +0xc0 push clamp),
+0x2c shape (+0x10 / +0x14 flag words, vfunc 3 = bounds, vfunc 2 = bounds at a pose), +0x34 id / render object (vfunc 0x78 =
set pose), +0x40 rotation, +0x50 position, +0x60 / +0x70 reset pose, +0x80 / +0x90 / +0xa0 results taken by 007684c0,
+0xa0 linear velocity, +0xb0 / +0xc0 contact accumulators, +0xd4 in-water flag.

The solver dispatch (0076a010): 0076ac40 (kinematic vs water), 0076b070 (kinematic vs static), 0076b4a0 (generic vs static),
0076b9b0 (generic vs keyed), 0076bf60 (generic vs generic), 0076c510 (generic in water), 0076cd00 (kinematic vs keyed),
0076c880 (riders vs keyed), 0076d3c0 (generic vs terrain), 0076d9a0 (kinematic vs terrain); then slot 1 of every simulation,
0076a1e0 (reset lost kinematic bodies), and four quarter steps of the kinematic integration 0076a360 (ECX = world, xmm0 =
dt / 4; the quartered dt is written back into the dispatcher's argument slot) with 0076ac40 between them. The worker-thread split
(flag 0xa946c4) is not ported: with the flag set the native declines (RUN_ORIGINAL) and the original runs; it is clear in every
world, so the job pool (0x00881xxx, collision group) is never reached from here.

All collision passes share one pattern: bounds (shape vfunc 3) into a box on the stack, the broad phase 00882d90 / 00883020
(ESI = grid, EDX = box, ECX / stack = query), the narrow phase 0085ddd0 (ECX = a contact set on the stack: count +0, records
of 0x30 bytes (point, normal, depth) at +0x10, feature ids at +0x5b0 / +0x628 (30 each), touching flag +0x6a0), an impulse
0085dcf0 (ESI = set) and a per-contact response, then a push-out = (min + max of depth * normal over the contacts) * 0.8
(0x9cc8a8) (* 0.5 or * -1 by pass), clamped to the material's +0xc0 length, and a contact event.

### Conventions and traps

- Callees read registers set long before the call: 00882d90 / 00883020 take ESI = grid, EDX = box; 00667e40 takes ECX = shape
  and EDX = the contact set; 0042bd90 takes EAX = handle body. Every port sets EBX / ESI / EDI (and EAX / ECX / EDX) to the
  original's values at each call site.
- The contact-set buffers and the boxes must be on the guest stack at the original's addresses (`world::Frame`); callees fill
  them. 00770e80 builds a 0x50-byte record on the stack whose last 8 bytes are never written (uninitialized stack copied into the
  vehicle's nearby-object list): the record is built at the same guest address, so the bytes match.
- Bugs of the original kept: 0076b4a0 / 0076b070 / 0076cd00 keep the clamp ratio of one contact as the push scale of all later
  contacts in the call; 0076c510 reuses EBX (nonzero after a contact loop or the water search) as the next object's contact count
  / flag initializer and the narrow phase's last argument; 0076cd00 writes K's pose into O +0x40 when only K is kinematic and
  computes the event displacements from K's new velocity minus O's previous one; a non-kinematic O stops after its first pair.
- `movss xmm, m32` clears lanes 1..3 (0076a010 passes xmm0 = (dt / 4, 0, 0, 0) to 0076a360).
- Return registers: 00767f90 returns EAX = its output pointer (0076e090 uses it); 00777110 ends with ECX = the frame's security
  cookie ([0x9b45e4] ^ EBP) popped by its epilogue (00773d10 passes it on); 007686b0 returns the address arithmetic in EAX's
  upper bytes with AL = the result.
- Verify and host state: the world runtime's heap (and other host-side state) was not restored around the native run; fixed by
  the lead (port.h set_host_state). Before that, World_PhysicsUpdate failed with "HeapFree of an unknown block".
- Coverage: the verify worlds do not reach every path. Coverage of the original under Verify (REFLEX_COVERAGE together with
  REFLEX_PORT_VERIFY: the original's run is interpreted and counted) over the five verify runs plus five more with other drive
  seeds (7..11, 3000 frames) reaches the contact paths of 0076bf60, 0076d3c0, 0076b4a0, 0076b070 and most of 0076cd00, but never
  the body of 0076c510 (objects in water), the contact handling of 0076d9a0 (kinematic bodies on terrain), any of 0076c880 (the
  rider-body vector is empty) or several branches of 0076cd00. Those four are therefore ported literally (world_phys_lit3.cpp,
  exact by construction instruction for instruction, with the call-site registers right automatically); the readable hand
  ports stay in world_phys.cpp under `#ifdef WORLD_HAND_DEADPATH`. Still unexercised in the hand ports: 0076b9b0's
  "still moving" test and axis lock, 007729f0's keyed / generic result copies and its 10-results exit, 0076a360's axis lock and
  reset pass, 0076ac40's axis lock, 0076a1e0's reset call (all short, reviewed against the disassembly).

### Literal ports (world_phys_lit*.cpp)

Generated from the disassembly by `lit2cpp.py` (scratchpad wtools; runtime world_lit.h): one statement per instruction, GPRs and
xmm as locals, the guest stack used literally (push / pop / locals at the original's addresses, so even the dead stack matches),
calls through Machine::call_function with the registers synced; flags only for a jcc / setcc right after cmp / test /
arithmetic; code extent followed past Ghidra's (often truncated) function size. Verified with every register as output (EAX ECX
EDX, all xmm). They are exact but not readable: rewrite them as semantic code once the layouts above are settled.

- world_phys_lit.cpp: 0076f440 (two-body contact impulse; the older generator with the frame as a local array loaded from the
  guest frame).
- world_phys_lit2.cpp: the long SIMD responses and helpers 0076e090, 00769bd0, 0076efc0, 00773110, 0076dd50, 00773d10,
  0076e460, 00769d40, 00769e40, 00770b00, 0076f160, 0076a8b0.
- world_phys_lit3.cpp: 0076c510, 0076d9a0, 0076c880, 0076cd00, whose contact paths the test worlds never reach (see Coverage);
  their readable hand ports are in world_phys.cpp under `#ifdef WORLD_HAND_DEADPATH`.
- world_phys_lit4.cpp: functions whose scratch registers (EAX / ECX / EDX at their return) generated callers pass on: 00768460,
  007684c0, 00768550, 00768590, 00776920, 007774f0, 00777db0, 00776ef0, 00770e80 (the vehicle vf62 ports read its EAX / EDX),
  007729f0; the readable versions are in world_phys.cpp / world_phys_small.cpp under `#ifdef WORLD_HAND_READABLE`.
  (A hand 00768550 left ECX as its callee did where the original pops its saved ECX: 00770b00's ECX then differed.)

### Hand ports

Matrix3_ToQuaternion reproduces its scratch registers too (EAX = 16, DL = the last permuted byte, ECX = the cookie), since the
generated vehicle ports compare them.

00401210 Matrix3_ToQuaternion and 00401740 (its ESI-output wrapper), 0041ad80 (cursor advance; the one function of the list at
0x0041a000..0x0041afff), 0076bf60, 0076d3c0, 0076b4a0, 0076b9b0, 007699e0, 00769ab0, 0076a010, 00769b80, 0076ac40,
0076a360, 0076b070, 0076a1e0, 00768750, and in world_phys_small.cpp 00767f80, 00767f90, 007686b0, 007705b0
(register-level one-body contact impulse) and 00777110 (SEH frame not reproduced; ECX = the cookie its epilogue pops).

### Left original (whole group)

- The CRT / FMOD import thunks in the plan list (0x009009b4..0x009009d2 MSVCR90 `_CIatan2`, `floor`, `_CIpow`,
  `__libm_sse2_pow`, `_CIfmod`; 0x00900360 / 0x00900390 FMOD): `jmp [IAT]` stubs into the DLLs (NATIVE_AGENTS: call the CRT
  through Guest::call for now).
- 0076a010's worker-thread path (see above): the native declines when flag 0xa946c4 is set.
- 00806f90 Audio_QueueThread (node share): the audio worker's main loop; the runtime ends a worker job when the worker's wait
  returns 0, which a native cannot reproduce (see the node section).
- 0061e677 (rider share): not a function, the tail of 0061e3d0 (included in that port).
- Never called in the five worlds, so unverifiable (0 instructions per frame): 00554fc0, 0060f530, 00592b70, 005c3260,
  005f7ab0, 006211d0 (rider share).

---

<!-- from the veh helper agent (scratchpad/world_notes_veh.md) -->

## world / veh: vehicle code and 0x0040-0x0041 helpers (world_veh.cpp, world_veh_raven.cpp)

Range: 0x00400000..0x00419fff, 0x0041b000..0x0041ffff, 0x00460000..0x0047ffff (minus 00401210 / 00401740, world_quat.cpp).
62 functions of wtools/world_list.tsv in range (572962 instructions per frame); all 62 ported and verified (0 mismatches,
"on" identical on the five worlds).

### How the ports are written: instruction-level transliteration (lifter)

The range is ~8900 instructions (RavenVehicleMotocross vf35 / vf29 / vf62, 00468300 at 8 KB, 0041e080 ...), mostly scalar SSE
with double-precision intermediates (cvtss2sd / mulsd / cvtpd2ps), x87 argument passing (fld / fstp [esp]) and a few x87 adds,
custom LTCG conventions everywhere. Instead of hand transliteration, `wtools/vehlift.py <addr> <name>` (scratchpad) generates the
port from the disassembly, and `wtools/vehgen.py` regenerates both files (names from world_names_veh.tsv and _re/names):

- general registers are the machine's own (`uint32_t& eax = m.r[0]` ...), so every callee sees exactly the registers the original
  holds at the call site (EAX / ESI / EDI / EBX inputs set long before a call need no special handling), and callee-saved registers
  are restored by the original's own pops;
- xmm registers are local `world::X` copies (`c.x[i]`), written to the machine around every call and at the return;
- the stack is the guest stack: pushes, the original's locals and the callees' arguments at the original's addresses (the
  security cookie is stored as the original does; the `__security_check_cookie` call is skipped: it changes nothing);
- calls: `c.call(target, return_address)` = Machine::call_function with the arguments already pushed; afterwards the return
  address slot is patched to the original's return address (call_function leaves 0 there), so even uninitialized reads of the
  stack see the original's bytes. Indirect / virtual calls read the target from guest memory as the original does.
- flags: computed (vm.cpp's set_logic / set_add semantics, AF not modelled) only where a later instruction reads them
  (forward scan to the next reader / full writer); `lahf` after comiss reproduced;
- every operation is the interpreter's (vm.cpp): sse32 / sse64 with its NaN rules (min / max: the second operand unless the comparison holds, as x86; vm.cpp changed to that), cvtss2sd /
  cvtsd2ss NaN payloads, cvtps2pd / cvtpd2ps via host conversions, truncating conversions -> 0x80000000; x87 through reflex_f80
  with the control word's precision / rounding (fld / fst / fstp m32 / m64 / st(i), fadd / fsub / fmul m32 / m64 / st(i), fldz,
  fld1, fchs);
- jump tables (00461680, 00461760, 004636f0) become switches (bound from the preceding cmp / ja); code extent by recursive
  descent (tables after the code are not decoded as instructions).
- outputs: all of EAX / ECX / EDX / xmm0..7 / x87 are reproduced and declared (VEH_OUTS).

Helpers: native/game/ports/world_veh_util.h (vehx::Ctx: registers, guest memory / stack access, calls, x87; double-precision and
compare helpers mirroring vm.cpp; flag helpers and C_<cc> conditions).

To change a port by hand, edit the generated C++ (each line carries the original instruction as a comment) and stop regenerating
that function (vehgen.py EXCLUDE), or improve the lifter.

### Verification notes

- Many of these functions are only called from inside other ported functions (vf29 -> vf35 / vf62 / vf60 -> 004674e0 ->
  Vehicle_WheelDeformAndTrackMarks -> ...). In Verify mode nested ports run natively without their own count, so the group run
  shows ~30 ports called. Each level was also verified on its own (filters of the nested-only functions, three levels, then
  PhysicsHandle_Body alone): every one of the 61 ports had calls and 0 mismatches.
- 00418f30 PhysicsHandle_Body, 004039a0 PhysicsEntity::vf30, 00403b90, 0041cea0, 0041cf30 run on Concord (drive 1) only.

### 004020c0 and the host-side heap

004020c0 (event dispatch: FUN_00593220 lookup with std::string assignment, FUN_00592d80 dispatch) allocates through the
deterministic heap at 0x30000000, whose allocator state is host-side (runtime.cpp). It failed Verify until port.cpp snapshotted
the runtime's host state around the native run (set_host_state); since then it verifies (0 mismatches, five worlds).

### Identified functions (world_names_veh.tsv)

Float_Abs 00401000, Float_Abs_Thunk 00401040, Float_Fmod 00401020, Vec3_Cross 00401630 (out = ECX x EAX), Vec3_Sub 00401700
(out = [EDX] - [ECX]), Vec4_CmpLe_CR6 0041c640 (AltiVec vcmpgefp. record form emulated: flags word bits 0x80 all / 0x20 none),
Vec4_Splat 0041c6c0, Matrix3_MulVec3 0041f5c0, Matrix3_Transpose 0041f650, Matrix3_Inverse 0041f6e0.

### Traps

- Custom conventions are the norm here: 00401630 / 00401700 take EAX / ECX / EDX pointers, 004603f0 EAX, 00418f30 EAX,
  0041f030 ESI, 0041fe60 EDI, 0046a380 EAX; xmm results (xmm0) from many vfuncs. The lifter sidesteps all of it.
- 0041c640 reads xmm0 / xmm1 as inputs (no stack vector).
- Functions mixing x87 and SSE pass floats to CRT / engine callees with `fld [mem]; fstp [esp]` and take st(0) results
  (_CIpow, _CIfmod): the x87 depth is compared by Verify, so every fld / fstp is modelled.


---

<!-- from the rider helper agent (scratchpad/world_notes_rider.md) -->

## World group, rider share (0x004d0000..0x0062ffff)

### Result

101 of the 108 functions of wtools/world_list.tsv in the range are ported (222695 of 224661 instructions per frame; with the
inlined chunk 0061e677, 224661). `verify.sh <boot> world`: 0 mismatches and "on: identical" on all five worlds; same with
`math,world`.

Files: `native/game/ports/world_rider_gen.cpp` (the ports), `native/game/ports/world_rider_util.h` (helpers). Tooling in the
scratchpad: `rider_tools/gen.py` (one function), `rider_tools/genall.py <out.cpp>` (the whole share; reads `skip.txt`,
`inline.txt`, `names.txt`), `rider_tools/vset.sh <filter>` (verify on the five worlds, calls / mismatches per port summed).

### How the ports are made

Instruction-level transliteration, generated from the original code by `gen.py` (capstone, recursive descent from the entry,
jump tables read at run time from guest memory exactly as the original reads them):

- registers are C++ locals (eax..edi, esp, x0..x7, fl = EFLAGS), the stack frame stays on the guest stack at the original's
  addresses (pushes, `and esp, -16` frames, locals, arguments built for callees, security cookies), so every callee sees the
  same memory and every register at every call site is the original's (RIDER_CALL syncs all of them out and back in).
- every output register is reproduced: ports declare EAX|ECX|EDX|XMM_ALL|FPU; EBX/EBP/ESI/EDI/ESP come out as the original's.
- flags follow vm.cpp exactly (arithmetic does not touch AF, comiss clears it, imul/mul set only CF/OF, inc/dec keep CF; lahf
  reads them). A flags liveness pass emits flag updates only where a consumer reads them; a compare directly followed by its
  only consumer becomes a C++ condition (`if (int32_t(a) < int32_t(b)) goto ...`, `fb_b(x, y)` for comiss "below").
- min/max (ss/ps/sd/pd) return the source operand unless the comparison holds, NaNs unquieted (vm.cpp since 2026-10-18).
- SSE through the reference's lane semantics (the bike group's helpers: NaN rules, min/max operand order, cvt*), x87 on the
  Machine's x87 stack with reflex_f80 under the control word (precision and rounding), opcode bytes decoded as vm.cpp does.
- calls: direct, indirect (vtables, `call [IAT]` into kernel32 / msvcr90), with the return address slot holding 0 (dead
  stack); real tail jumps (`jmp edx`, `jmp [eax+0x34]`, `jmp sdtor`) pop the return address and return to the caller.

### Left original (skip.txt)

- 00554fc0, 0060f530 (event handler proxies, 0 instructions per frame): never called in the five verify worlds. (They were
  called in an earlier run only because Verify then did not restore the host heap, so the original side diverged.) Left original.
- Heap trees (00592d80, 005860e0, 00544cc0): ported since port.cpp's Verify restores the runtime's host state (heap free lists,
  QPC, audio, ...) around the native run (port.h set_host_state); before that they could not pass Verify.
- 0061e677: not a function. It is the tail of 0061e3d0 (only ever reached by its jumps, never called); the 0061e3d0 port
  includes it (inline.txt). As a separate native it cannot be verified (Verify runs the original from it as if called).
- 00592b70, 005c3260, 005f7ab0, 006211d0: never called in the five verify worlds (0 instructions per frame), so unverifiable;
  left original. (gen.py produces them: `genall.py out.cpp 00592b70,...` once a world exercises them.)

### Traps

- Port call counts in the report are only those entered from original code: a port called from another port runs natively
  inside the parent's verification and shows 0 calls (Matrix3_ToQuaternion shows 0 calls, all through 00401740). 50 of these
  ports show 0 calls with `REFLEX_PORT=world`; to see each verified at the top level, verify the zero-call set alone, then the
  rest of the zero set, until a set shows no calls at all (`vset.sh`). Done: all 98 are called and matched at the top level.
- Ghidra sizes are often short (006237a0 / 00623960 / 006235e0 are 7-12 byte "functions": jump-table dispatchers whose cases
  are the rest of the function); follow the control flow, not the size.
- LTCG conventions everywhere in 0x0061xxxx-0x0062xxxx (EAX / EDI = this, xmm0..3 arguments, ESI flags), and float arguments
  pushed through x87 (`fld [0x92ff84]; push ecx; fstp [esp]`): transliterating every register avoids caller analysis.


---

<!-- from the wreck helper agent (scratchpad/world_notes_wreck.md) -->

## World group, "wreck" share (0x00630000-0x0068ffff, 0x006a0000-0x0075ffff)

All 77 functions of wtools/world_list.tsv in the range are ported (549166 instructions per frame in the list). Files:
native/game/ports/world_wreck.cpp (float listeners, cell grid), world_wreck_checker.cpp (WreckChecker, wreck state ramps),
world_wreck_audio.cpp (AudioCollisionManager and its std::map), world_wreck_camera.cpp (MXThirdPersonCamera impacts),
world_wreck_misc.cpp (small virtuals, crowd audio, ClipRecorder, misc), helpers world_wreck_util.h.

### What the functions are

- 0x0063b830 / 0x0063b8f0: a float "property" with a std::vector of listeners {float* target, int enabled} (checked
  iterators; container proxy at +0, begin +0xc, end +0x10): set broadcasts *value to every enabled target through x87
  fld / fstp (SNaN quieting kept via reflex_f80); get returns the first enabled target's value in xmm0 (0 when none).
  0x0063b0e0..0x0063b6c0 are wrappers: a rate-limited ranged float {min, max, rate, limit, value, listeners +0x14} (0x0063b0f0
  / 0x0063b160 step it and broadcast its 0..1 position), clamped floats (+8 listeners), a wrapped float (0x0063b650) and a
  short property (0x0063b6c0). Heavy callers: 0x00619050 / 0x00619490 / 0x0061dxxx-0x00620xxx (bike sound / input state).
- 0x00667e40 and 0x00668650-0x006689c0: a uniform 2D cell grid over the level bounds ([[grid]+0x9c]+0x44: +0x24af0 ..):
  cell of a point (0x00668820, cvttss2si of the scaled offset), its 3x3 neighbourhood (0x00668710; -1 outside), the hit test of
  a point against the cells' boxes (0x00668910 / 0x006689c0, with the original's shrinking loop bound), recentring
  (0x00668690), collecting hit cells (0x00668650, appends without a terminator into the caller's -1 filled list). 0x00667e40
  probes four points (the point +- the swept extent from the caller's virtual +0xc), then for every 48-byte item of every layer
  of the hit cells whose scaled box overlaps the sweep, builds a falloff vector (polynomial of the item's +0x14 parameter, the
  trunc via cvttss2si / cvtsi2ss per lane, lane 3 of the result replaced by lane 3 of an intermediate, as the original's
  stack store does) and reports it to two receivers through 0x0085ddd0, bracketed by the layer object's virtuals +0x10 / +0x14.
- 0x00637320 WreckChecker::vf0 / 0x006373e0: the per-rider wreck check (impact timers +0x24 / +0x28, the bike's orientation
  and angular rates through its virtuals +0x94 / +0x9c / +0xe0 / +0xe4 (x87 results), thresholds scaled by 0x00424370,
  the wreck type chosen by a ladder of 0x00638150 "excess" values, the game's LCG at 0x9cb860 for the random variant, then
  0x00637ee0 / 0x00637ff0 (jump-table lookups: type -> class, class -> animation + weight) and the rider's 0x0054e8f0 /
  0x0054ed10). 0x006367b0 / 0x00636870 / 0x00632aa0: a value ramp state machine of the rider (modes 0 / 1 / 2) and its
  reaction playback.
- 0x0067bac0 AudioCollisionManager::vf0 .. 0x0067d310: collision sounds. Events (64 bytes) are copied into pooled 0x70-byte
  records, inserted in a std::map keyed by {obj0, obj1, contact byte} (0x0068e130 insert / 0x0068faf0 lower_bound /
  0x0068e510 find / 0x0068e270 erase with red-black rebalance / 0x0068fc80 ++it / 0x0067ac80 key less), merged per frame,
  played (0x0067bd30: object classes 0x1082 / 0x100f / 0x13+0x31 / 0x10 / 0xf, surface sound ids, strength mapping, rider
  crash slots 0x006817c0) and updated (0x0067c2c0 / 0x0067c350 rolling sounds). 0x0067c990 steals the loudest record when the
  event pool is exhausted (nearest-listener distance 0x00805420).
- 0x00651000 MXThirdPersonCamera::vf33, 0x00653300 vf34, 0x006510a0 / 0x00651320 / 0x006516a0 / 0x00651940: camera
  impact shake / kick from tuned curves (0x00438ee0; tables 0xb0bfc4 / 0xb0c884 / 0xb0f0c4 / 0xb0f984 indexed by rider class
  (+0x194, class 3 split by +0x170), camera mode index (0x006467f0 compares the camera's name with six strings) and a split
  flag (0xa946b8 +0x15b24 == 2)); 0x006516a0 computes two phase amplitudes with sin / cos polynomials (0x935d3c..).
- 0x006aa460 .. 0x006ab930 ClipRecorder: clip candidates (0x006aacf0 keeps / extends / replaces the best one, snapshotting
  all riders into 0xa959b0 through 0x006abbc0 / 0x006aa460) and the 100-entry event ring 0xd2f160 (0x006ab8a0 wrecks,
  0x006ab930 stunts).
- Small: 0x00679000 (ret 4), 0x00703d50 (type 0x10), 0x0065cab0 ForestSystem::vf22 (0x100f), 0x00649bc0 (0x1003),
  0x006d7870 KeyMap::vf11, 0x0075c580 / 0x0067ec50 / 0x0067eca0 (the active sound table of 0xa946b8 by mode, with / without
  the +0x20018 bank), 0x00674440 / 0x00674540 / 0x006745d0 / 0x00679420 / 0x00679540 / 0x0067abc0 (an excitement level
  0xc023xx ramped by events, reaction events played on the 0xc02470 emitters, per-slot levels), 0x006b0480 (fade restarts),
  0x006b6d80 (keyframe pick and blend 0x00840440 / 0x00840b70), 0x006b7560 (three vectors out), 0x006def20 (ticks to seconds),
  0x00702ad0 (string-keyed map find).

### Conventions and traps

- Nearly every callee here takes inputs in registers set long before the call (ESI / EDI / EBX / EAX / EDX), and virtual calls
  leave the vtable / target in EAX / EDX in either order. The audio, camera, checker and misc ports are written with
  `wreck::Regs` (world_wreck_util.h): m.r used in place as the register file, set exactly where the original sets each register,
  xmm cached and synced around every call, callee-saved registers restored at the return. Callees then see what they see in
  the original, and EAX / ECX / EDX / xmm at return come out right without per-path bookkeeping.
- Tail calls (`pop ...; jmp f`): `Regs::tailcall` calls with ESP + 4 so the callee finds the caller's return address at [ESP]
  as in the original (call_function's own return address overwrites that slot during the call; it is put back afterwards).
  Calling with ESP instead shifts the callee's frame by 4 and its stack-derived register leftovers differ (found on 0x0067b890:
  EDX 7feffc38 vs 7feffc3c).
- `lahf` after ucomiss changes AH: EAX is an output (0x006367b0's callers saw 0x4200 in EAX). `wreck::lahf_ucomiss`.
- Security-cookie functions return ECX = the cookie value [0x9b45e4] (the check leaves it there): set it and compare OUT_ECX.
- `mov ecx, dword ptr [esp + X]` reading a byte flag's whole dword (0x0068e130 passes its "go left" byte with the 3 stack bytes
  above it to 0x005860e0): keep such locals in guest memory at the original address.
- Some functions write their argument slots (0x0063b590 / 0x0063b650 pass the address of their argument, 0x0068faf0 keeps its
  candidate there, 0x0068e270 keeps the node in the map argument and advances the iterator arguments in place, 0x0067bb10 keeps
  the record pointer in its argument slot and uses that address as the map key pointer).
- 0x00674540: cvtss2sd / mulsd / cvtsd2ss: lane 1 of xmm0 keeps the high dword of the double product (reproduced).
- Jump tables (0x00637ee0 / 0x00637ff0): the native reads the table entry and maps the target; unknown targets decline
  (RUN_ORIGINAL at entry). 0x0068e270 declines when asked to erase the head (the original throws).
- Coverage: the wreck path of 0x006373e0, the event-pool steal (0x0067c990), erase (0x0068e270), 0x0063b540 / 0x0063b590 /
  0x0063b650 and a few others run rarely or never in the five worlds; they were transliterated instruction by instruction but
  have few or no verified calls.


---

<!-- from the node helper agent (scratchpad/world_notes_node.md) -->

## World group, "node" share (0x0078-0x007b, 0x007d-0x0083, 0x008519a0, 0x00892170, 0x008b98c0)

72 of the 73 listed functions are ported (519198 of 519297 instructions per frame). Files:

- `native/game/ports/world_node.cpp`: hand ports of the node hierarchy (0x007dxxxx) and PackedGrid_SampleUV.
- `native/game/ports/world_node_util.h`: the evaluator's 4x4 products (two operand orders), double helpers with the x86 NaN
  rule, f80 fist / comiss helpers.
- `native/game/ports/world_node_gen.cpp` + `world_node_xc.h`: the other 63 functions as register-level transliterations,
  generated by the scratchpad's `wtools/x2c.py` (63 functions) (list `wtools/gen_node.txt`, `wtools/gen_node.py` regenerates the file).

### Node hierarchy (0x007db7e0 .. 0x007dd3f1)

Node set: +4 traversal cursor, +8 records (16 bytes each), +0xc count, +0x18 local matrices, +0x1c world matrices (64 bytes
per record, index record +0xb), +0x30, +0x34 dirty flag. Record: +0 set, +0xb matrix index, +0xc flags (bit 0 world valid,
bit 1 visited by the 007deaa0 traversal, bit 2 cleared up the parent chain on invalidation), +0xd next sibling, +0xe first
child, +0xf parent (0xff: none). (The old header comment of Node_EvaluateWorld called +0xe "next sibling": Node_Invalidate
shows +0xe is the first child and +0xd the next sibling.)

- 007db890 Node_EvaluateWorld (thiscall rec, flag; RET 4): collects rec and its invalid ancestors, evaluates top-down. The top
  record uses form a: (w R3 + y R1) + (z R2 + x R0); the rest form b: (z R2 + x R0) + (w R3 + y R1) (operand order matters for
  NaN payloads; both kept). Flag path (never taken in the five worlds): clears bit 1 everywhere, cursor = first child,
  evaluates every record 007deaa0 (EDI = set) yields.
- 007db7e0 Node_Invalidate (ECX = record; recursive). Callers read EAX (a tail jump at 006ba23e makes it a virtual's return):
  EAX / ECX / EDX reproduced exactly (low-byte writes included).
- 007dc890 Node_SetWorld (EDI = record, ESI = world matrix): local = W x rigid inverse(parent world). The inverse: transposed
  rotation with w lanes from 0xd61660 lane 0, translation -T . row i in double ((y + x) + z, products exact), w lane from
  0xd61690 lane 3. EAX / EDX before its Node_Invalidate call matter (they pass through to the caller).
- 007dcfb0 Node_SetWorldRotation, 007dc560 Node_WorldToLocalPoint, 007dd1b0 Node_SetWorldPosition, 007dd3a0
  Node_GetWorldPosition, 007dd320 Node_GetWorldMatrix: see world_names_node.tsv.
- Records 0 (EDI / ESI / ECX == 0) paths read address 0xb; unreachable, transliterated literally.

### 007f8e00 PackedGrid_SampleUV

thiscall grid, EAX = channel c, (u, v) clamped to [0, 1] (comiss: NaN skips both clamps). x87: cell = trunc(floor(size * u)
* stride + size * v) with size +0x48[c], stride +0x38[c]; floor through the CRT thunk 0x9009cc (Guest call), fmul / fadd with
the control word's precision. Result: the n-bit field (n = +0x68[c]) at bit idx * n of the words at +0x88[c] (two words when
it spans). Writes the u / v argument slots (clamps, fnstcw, spill / word index) like the original.

### The register-level transliterations (x2c)

One C++ statement per original instruction, on the machine's own state: m.r, m.flags (CF PF ZF SF OF as vm.cpp), m.xmm,
m.fpu / fpu_cw, and the guest stack (push / pop / call write the same stack bytes; the return address slot of every call is
rewritten with the original return address after the call). So registers, flags, xmm, x87 and even the dead stack end as in
the original, whatever the (LTCG) convention. Semantics mirror the interpreter, not real hardware, where they differ:

- minss / maxss / minps / maxps / minsd / maxsd: the source operand unless the comparison holds (a NaN gives the source
  unchanged), as x86 and the current vm.cpp (it used to propagate a NaN first operand; xc follows the new rule).
- fs: segment = FS_BASE 0x7ffd0000 (vm.cpp, every thread).
- Jumps to addresses outside the decoded body (tail calls, or past int3 padding, e.g. 0080aa3e jmp 0x80aaa0, 0080b474 jmp
  0x80b490, 0080ca2f jmp 0x80c400) run the target inside the native (xc::tail: the return address slot at the native's entry
  ESP set to 0, m.run(target), slot restored) and return the original return address. Returning the target as the next pc
  works in On mode but fails Verify ("native returned to another address").
- Jump tables (jmp [table + reg * 4]) become switches (none in this share).

### Left original (99 instructions per frame)

- 00806f90 Audio_QueueThread: the audio worker's main loop. The runtime runs worker jobs synchronously and ends a job when its
  WaitForSingleObject on the go event returns 0 to stop the interpreter; inside a native that only stops the nested call and
  the native loops on (the stack runs off the worker stack: "unmapped write 7fbbfffc"). Must stay interpreted (or the
  runtime's job model must change).
- (The heap users 0078fbe0 ReflexAlignedAlloc, 0078fc50 ReflexAlignedFree, 0080da40, 0080c760, 0080c970, 0080bce0
  AudioEvent_SetChannel failed Verify while the runtime's host heap was not restored around the native run; with the lead's
  fix they verify and are ported.)

### Traps

- The dead stack matters in On mode: PackedGrid_SampleUV passed Verify everywhere, but Alpine's "on" memory hash differed after
  ~700 frames until the port also left the original's dead stack (saved EBX / EBP / ESI / EDI, the fistp qword local, the
  floor call's return address slot): later code copies uninitialized stack into heap blocks (memdiff showed stack garbage such
  as 0x7feffec8 / 0x3c8888ab in 24 heap objects). Bisect with REFLEX_MEMDUMP + tools/memdiff.py and On runs of 600 / 900
  frames (wtools/oncmp_node.sh). The x2c transliterations are immune (they write the same stack bytes).
  Same for Node_Invalidate on SX_Springfield_ai11 (1500 frames): its recursion's pushes (caller EBX / ESI / EDI) and the
  recursive calls' return addresses (0x7db83a) are now written as the original does.

- In On mode a math.cpp build in progress (another agent's) once changed the memory hash with no math port selected; build
  with only finished files when checking "on: identical".
- Ports whose nested calls run long enough to hit a thread time slice (m.slice_end) would continue early (call_function
  returns at the slice): only matters for the boot scheduler (cooperative threads); the world runtime runs workers
  synchronously.

**2026-10-18 (lead): 007f8e00 PackedGrid_SampleUV is now the lifted port (ports/world_lift.cpp).** The hand port above matched
every verified call, but in On mode the race memory differed on baton worlds of NAT_Renne, TRX_HalifaxBW and WaypointRace
FR_Gibson/1 (31 / 6 / 15 dwords: +0x6c of 128-byte particle records, 3 original vs 1 port; state hashes identical; bisected to
this one port). Its dead stack differs from the original's and a later particle update reads that stack uninitialized. The
lifter writes every stack byte the original does.
