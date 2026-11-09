# Group "bike": the vehicle simulation layer (0x430000..0x45ffff)

Status 2026-10-18: all 84 functions of `plan/physics-functions.tsv` in this range are ported (2,536,879 original instructions per
frame by the plan, 14% of the physics update) and pass `tools/verify.sh <boot> bike` on all five worlds (0 mismatches, "on:
identical"). No function of the range is left original: coverage with `REFLEX_PORT=bike` shows no interpreted instruction in
0x430000..0x45ffff.

## How the ports are made: instruction-level transliteration

The range is ~24,000 instructions of LTCG SSE code (the largest functions 3-10 KB each: 00447f10 tire/contact, 00441300, 004360b0,
0044c170 vf9, 0043f2e0, 0045d910). Decompiler output is not usable for exactness (it drops instructions, e.g. the `andps` mask at
00438378, and reorders lane arithmetic), so each port is transliterated from the disassembly, instruction by instruction, by a
generator (the "bike lifter", `scratchpad/bikeag/lift/lift.py` + `gen_all.py`; output: `ports/bike_<range>.cpp`):

- Registers are C++ locals (`eax`..`edi`, `esp`, `x0`..`x7` of type `bike::X`, an xmm as two 64-bit halves).
- Memory, including the stack frame, is guest memory (`rd32/wr32/ldx/stx`): callees receive pointers into the frame, so the frame
  stays where the original has it (`and esp, -16` included). Even the dead stack matches the original: after each call the slot of
  the return address is rewritten with the original's return address (`BIKE_CALL(target, ra)`).
- Every call into other code (other groups, CRT thunks, virtual calls, other bike functions) goes through `Machine::call_function`
  with the registers synced (`BIKE_CALL`), so custom conventions (args in EAX/ESI/EDI/xmm, results in xmm) keep working. A jmp to
  another function's entry is a tail call (`BIKE_TAILJMP`). The two jump tables (0044e858, 00434f23) become switches.
- Flags exist only where read: each consumer (jcc / setcc / sbb / lahf) is resolved statically to its defining instruction(s); the
  operands are captured at the definition (`fA_/fB_/fR_/fC_<addr>`), so the condition is a plain C++ comparison. Where several
  definitions reach one consumer (0044d7ba, 004460b4) the definitions materialize EFLAGS (`FL`, `fl_cc`).
- SSE arithmetic uses `bike_util.h`, which reproduces the reference's semantics exactly (vm.cpp `sse32/sse64`: NaN rules incl.
  min/max, conversions, compares, NaN-preserving cvtss2sd / cvtsd2ss). Packed add/sub/mul/div run on NEON when no NaN is involved
  (as the interpreter's fast path). NOTE: x86.h's `min/max` return the second operand for NaNs (real hardware); the reference
  interpreter returns a quieted NaN first operand (`sse32` OP_MIN/OP_MAX). The bike helpers follow the interpreter, since Verify
  compares against it.
- x87 (fld/fstp/fadd/fmul/fdiv/fcomip, used for double/extended intermediates in the tire curves and 00446340) works on the
  Machine's x87 stack with reflex_f80, exactly as vm.cpp execute_fp (precision and rounding from the control word).
- Outputs: every port reproduces all registers (EAX/ECX/EDX, all xmm, x87 stack) and declares them all (`BIKE_OUTS`): Verify checks
  the full register state, so no caller convention can be missed.

Each function's header comment gives the original name, the register inputs (live-in registers computed by `abi.py`: the LTCG
convention) and the `ret N`.

Regenerating: `_re/.venv/bin/python scratchpad/bikeag/lift/gen_all.py` (reads `bike_list.tsv`, names/descriptions from
`lift/names.tsv`); unsupported instructions or unresolvable flag uses stop the generator with the address (nothing is guessed).

Coverage of the ported code by the five verify worlds: 82% of the lifted instructions executed (19,980 of 24,352; per function in
`scratchpad/bikeag/lift/covlift.py`). Instruction forms that appear only in unexecuted paths: `inc/dec m32`, `jg`, `fdiv m32`,
`ucomiss xmm, m32`, `pxor`, `movdqa` (all in 0045d910 / 00431cxx / 00446752 / 00435f90), each a direct mapping of a form whose
neighbours are verified. 0045d910 (6.5 KB) runs only 3.6% of its instructions in the test worlds; 00447790 vf31 never ran there.

Besides the group run, every function was also verified alone (`verify.sh <boot> <addr> quick`, `scratchpad/bikeag/single.sh`):
74 are called on that world and all have 0 mismatches and "on: identical"; the 10 others (00430270, 00430850, 00433380, 0043cfe0,
0043f270, 0043f2e0, 00445e80, 0044bff0, 0044e0d0, 00447790) run only on the other worlds or not at all and are covered by the group run.

## Traps

- The reference for exactness is the interpreter/JIT, not real hardware (min/max NaN rule above).
- Functions here have custom conventions throughout: 004390d0 (ESI, xmm0), 00438ee0 (EDX, ESI, xmm0), 00430810 (xmm2 = t,
  two stack floats, result xmm0: clamped lerp), 004304b0..004304f0 (EAX, xmm0), 00430490..00430540 (ECX, EAX), 004308a0 (EAX
  controller, EDI block), 00431d60 (ESI = simulation, ECX), most helpers take the simulation in EDI or ESI. See each header.
- 00438260 (the most expensive function, 1.18M instructions/frame) is thiscall with EDI live too.

## The simulation object S (RavenSimulationMotocross, vtable 0093c8a4) as these functions use it

- Bodies (stride 0xa0, count S+ec, mechanism S+d0): 0 main, 1 rider, 2 front tire, 3 rear tire. Position S+f0, linear velocity
  S+100, force accumulator S+140, orientation S+730, angular velocity S+740, torque accumulator S+780 (+ i*0xa0).
- Wheel contact records W (0x150 bytes): rear S+8790 (body 3), front S+88e0 (body 2); W+fc / W+100 contact flags (rear S+8890,
  front S+89e0); S+8d98 = both wheels airborne.
- Inputs: the 64-byte input block at S+1c (simulation-input.md); the processed control block is *(S+5c30).
- S+2374 rider off the bike; S+90 resting (slot 15 with slots 25 / 26); S+2de0 / S+2de4 pose override active / mode
  (vf33 begins, vf10 sets targets, vf34 applies, 00434ef0 ends); S+2ec0 / 2ed0 / 2ee0 external impulses (004355d0).
- The step (00431d60) and its slots: bike-connected-path.md.

## Functions

Instructions per frame from plan/physics-functions.tsv. "Inputs" are the live-in registers (the LTCG convention; ESP / stack
arguments always). Names of FUN_ functions are proposals (`_re/names/9e_bike.tsv`), Fn_ where the purpose is not certain.
00438ee0's EDX live-in is a false positive (overwritten before use).

| Address | Port | Instr/frame | Inputs, ret | What it is |
| --- | --- | ---: | --- | --- |
| 00438260 | Wheel_TriangleContactTest | 1,180,397 | ecx edi; ret 20 | Wheel shape against a triangle array (5 callers: bike, ATV, truck): culls by bounds, tests in wheel-local space, keeps up to 8 candidates, averages them back to world space. ECX = out contact point, EDI = out normal; stack: body quaternion, body position, wheel extent vec4, triangles, count. EAX = hit. |
| 00447f10 | BikeTire_ContactForce | 211,107 | edi; ret 8 | Tire/contact force of one wheel (called twice by slot 11 with W = S+8790 rear, S+88e0 front). EDI = S, stack dt, W. Curves 438ee0 / tables 4390d0, control block *(S+5c30); adds force/torque through 85c770; writes W+80..e8, S+8da0, diagnostics S+8e80..8f10. |
| 00438ee0 | ParamCurve_EvalCubic | 162,890 | edx esi xmm0; ret 0 | Bounded cubic parameter curve: ESI = curve (count 2..10 at +0, knots, values, end slopes), xmm0 = x, result xmm0; outside the knots the end value. |
| 00435b00 | Simulation_BodyProbeContacts | 145,165 | esi; ret 0 | Body (non-wheel) probe contacts vs terrain / objects (bike, ATV, truck). ESI = S. Probe points from slot 32, terrain via 7c5e80, objects 85ddd0; writes S+1320 points, 1640, corrections 17d0, normals 1960, flags 1af0/1b54/1bb8. |
| 00441300 | Fn_00441300 | 116,926 | esi; ret 4 | Grounded branch of slot 12 (S+2374 == 0, S+8d98 == 0). ESI = S, stack dt. Wheel-anchored points S+8dc0/8dd0, rewrites root angular / linear velocity (85c750, 85c730), contact gates 8890/89e0 and more; semantics not recovered. |
| 0044c170 | RavenSimulationMotocross_vf9 | 80,747 | ecx; ret 4 | Slot 9, the control update before the force reset (ECX = S, stack dt): input transfer, gear pulses, filters, curves, optional terrain ray, control quaternion; 44e0d0 with the rider off (bike-controls-recovery.md). |
| 0043c590 | BikeSuspension_SpringDamperForces | 49,301 | edi; ret 0 | Front and rear suspension spring/damper between root and wheels (EDI = S): travel along the axis, (dist - rest) * k, damping by velocity sign, travel flags 8898/889c/89e8/89ec; force to wheel and reaction to root through 85c770. |
| 00430ef0 | RavenSimulation_UpdateFixedSteps | 48,958 | ecx; ret 4 | Fixed-step scheduler (ECX = S, stack frameDt): ceil((frameDt - lead) / fixedDt) steps capped by S+b8, 431d60 per step, then interpolation (physics-timing.md). |
| 004390d0 | IndexedCurve_Interp | 44,186 | esi xmm0; ret 0 | Indexed table interpolation: xmm0 = x (scaled by [92e330]), ESI = table; index with half rounding (8fa0d0), lerp 430810; result xmm0. |
| 0044ef70 | RavenSimulationMotocross_vf32 | 43,996 | ecx; ret 0 | Slot 32 (ECX = S): the twelve local body-probe vectors S+14b0..1560 and their history. |
| 0043e200 | RavenSimulationMotocross_vf16 | 31,245 | ecx; ret 0 | Slot 16, after pose integration (ECX = S): front steering quaternion and rear anchor direction of the tire nodes (S+6eb0, 8a80, 77d0, 8a90). |
| 00447810 | BikeWheels_SpinRelativeCorrection | 22,206 | edi; ret 0 | Slot 15 helper (EDI = S): front/rear wheel spin relative to the chassis; locked to the chassis while braking slowly, otherwise scaled by a curve of its length; returns the all-locked flag. |
| 00444b90 | RavenSimulationMotocross_vf13 | 19,913 | ecx; ret 4 | Slot 13 (ECX = S, stack dt): wheel contacts (444a40, objects 85ddd0, records 42db20) for front then rear, airborne flag S+8d98, then body probes 435b00, position correction 4456d0, wheel orientation 445800, resting tail 446f80. |
| 00445800 | BikeWheels_UpdateRollingOrientation | 19,768 | esi; ret 4 | Slot 13 helper (ESI = S, stack dt): six contact-point filters S+8e40..8e54; while a wheel touches and the bike is not resting, integrates the wheel quaternions S+870 / S+910 from the rolling displacement; then node slot 6. |
| 00430810 | Lerp_Clamped | 18,751 | xmm2; ret 0 | xmm2 = t, stack a, b: t <= 0 -> a, t >= 1 -> b, else a + (b - a) * t; result xmm0. |
| 00434d00 | Fn_00434d00 | 18,240 | esi; ret 0 | Called by every RavenVehicle*::vf29 (ESI = S): clears S+10, then ORs a mask per body whose quaternion lanes or summed state vectors equal +-[adfdd0]; returns the flag (likely a divergence check). |
| 00431ee0 | BikeWheel_ContactPositionCorrection | 18,041 | ecx edx; ret 4 | Per contacting wheel (ECX = W, EDX = S, stack out vec4): penetration W+50 - W+60 projected onto the normal W+40 (fully / partly by its Y), scaled by [9cd970], gated against the up axis. |
| 004360b0 | BikeSim_ContactResponse | 17,472 | ecx edi; ret 0 | First call of slot 15, shared by all vehicle simulations' vf15 (ECX = S): resolves contact impulses into body linear / angular velocities; world 402dc0, event queue 75c580, records 42bd90. |
| 00433550 | ReplayRecord_Encode | 16,405 | esi; ret 4 | ESI = out record (V+0x12c0), stack S: per-frame replay record (half4 + residuals, time, inputs, flags; replay.md). |
| 00446340 | BikeEngine_DriveForces | 16,389 | edi; ret 4 | Engine / transmission step from slot 11 (EDI = S, stack dt): engine speed S+8abc, load S+8ad8, clutch blend (_CIpow), gear ratio, drive / drag curves, coupled / blended / free modes; calls 446a40 and 85c8b0. |
| 00443610 | RavenSimulationMotocross_vf12 | 16,149 | ecx; ret 4 | Slot 12, after velocity integration (ECX = S, stack dt): rider off 43f270, airborne 43f2e0, else grounded 441300 (with 440c40, atan2); copies wheel anchor points. |
| 00445e80 | BikeGear_AutoSelect | 15,251 | edi; ret 0 | Gear choice while throttle > 0 (EDI = S): scores the 5 ratios by the torque curve against root-local forward speed and shifts when the timer S+8aec passes the thresholds. |
| 0043e9f0 | BikeControls_SmoothGroundDirection | 15,021 | -; ret 8 | Stack (S, dt): on the ground with /control+0x68/ >= 0.02, a reference direction from the wheel contact normals or the up axis, smoothed by three filters into S+8b50. |
| 0043c1a0 | RavenSimulationMotocross_vf17 | 14,694 | ecx; ret 4 | Slot 17 (ECX = S, stack dt): 43ed40, wheel spin relative to the chassis into S+8a30 / 8a34. |
| 00444a40 | BikeWheel_QueryTerrainContact | 13,266 | esi; ret 4 | One wheel's terrain contact (ESI = W, stack S): terrain 402dc0, up to 400 triangles into [ad4c30] via 7c6160, tested with 438260; on a hit the contact record via 42db20. |
| 00446a40 | BikeEngine_CouplingSpring | 12,386 | -; ret 12 | Drivetrain torsional coupling (stack S, factor, dt): stiffness 1000 .. 500 by factor, driven by root-local Y of S+160; rate S+8ae0 (x0.97), angle S+8adc. |
| 0043ed40 | Fn_0043ed40 | 12,245 | -; ret 8 | Slot 17 helper (stack S, dt): damped scalar spring S+8d94 (rate) / S+8d90 (angle) driven by root-local Y of S+160; meaning unclear. |
| 00447cd0 | BikeWheels_ApplyAxleTorques | 12,010 | -; ret 4 | Stack S: control +8 into the rear-wheel torque S+960 (reaction on the root S+780), control +0xc into the front wheel S+8c0 with its reaction rotated into the root frame. |
| 0043efa0 | BikeWheel_DragForces | 11,380 | edi; ret 0 | EDI = S, rider on: per grounded wheel a curve (4390d0) times load, along the wheel body's velocity; root drag -S+100 * S+2f00 and the control vector +0x20; applied to bodies 0, 3, 2 through 85c770. |
| 00443c30 | RavenSimulationMotocross_vf15 | 11,186 | ecx; ret 4 | Slot 15, before pose integration (ECX = S, stack dt): contact response 4360b0, angular velocity caps 443af0, node slot 7 (85d330), wheel spin 447810, resting state S+90 (slots 25 / 26). |
| 0043f2e0 | Fn_0043f2e0 | 10,283 | -; ret 8 | Airborne branch of slot 12 (stack S, dt): in-air stunt / gesture state S+8d40..8d6c, stick mapping 440c40, curves, atan2 / sqrt; rotates the mechanism (85c530) and rewrites root angular velocity. Control law not recovered. |
| 00443af0 | Bike_ClampAngularVelocities | 9,712 | -; ret 4 | Stack S: root angular velocity S+740 capped at length 20, bodies 1.. at 120 (all lanes rescaled); S+80c0 = 1 when clamped. |
| 00431d60 | RavenSimulation_IntegrateStep | 9,647 | ecx esi; ret 4 | One fixed physics step (ESI = S, stack fixedDt): replay follow, slot 9, force reset 85c9d0, 4355d0, 435320, slot 11, mechanism forces / constraints, velocity integration, slots 12 / 15, pose integration, slots 16 / 14 / 13 / 17, node slot 6. |
| 00438180 | BodyProbe_ApplyAverageCorrection | 8,651 | ecx; ret 0 | ECX = S: averages the corrections of flagged body probes (S+17d0, flags S+1af0) and body contacts S+21c0, then translates every body by it. |
| 00446f80 | Fn_00446f80 | 8,007 | edi; ret 0 | Slot 13 tail while resting (EDI = S): compares the wheel geometry with the saved contact points S+8b00 / 8b10 and rotates / translates the mechanism (85c530 / 85c500) to keep the resting bike pinned. |
| 004456d0 | BikeWheel_ApplyContactCorrection | 7,431 | edx; ret 0 | Slot 13 position correction (EDX = S): averages the per-wheel corrections (431ee0, or 43cfe0 with the rider off) and translates all bodies; then 438180. |
| 00440c40 | Stick_CircleToSquare | 6,738 | ecx esi edi; ret 8 | Stack (x, y), outputs through EDI and ESI: maps the round stick range onto a square (r = /(x, y)/, angle by atan2, per 90-degree sector). |
| 0044c010 | RavenSimulationMotocross_vf27 | 6,223 | ecx; ret 0 | Slot 27 (ECX = S): 1 if resting and the rotated axis [d615c0] is nearly perpendicular to up. |
| 0043b4a0 | RavenSimulationMotocross_vf4 | 5,796 | ecx; ret 0 | Slot 4 (ECX = S): 1 if a body-probe contact flag is set, or an external impulse with root speed < 4 while airborne. |
| 004321a0 | RavenSimulation_vf28 | 4,689 | ecx; ret 0 | Slot 28 (ECX = S): 1 on any driving activity, rider off or a large external impulse. |
| 0043e100 | RavenSimulationMotocross_vf11 | 4,593 | ecx; ret 4 | Slot 11, the force stage (ECX = S, stack dt): gear timer, gear choice 445e80 / 4460f0, engine 446340, axle torques 447cd0, suspension 43c590, drag 43efa0, tire forces 447f10 for rear then front. |
| 004355d0 | Simulation_ApplyExternalImpulse | 4,431 | -; ret 4 | Stack S: fetches and clears the external object's accumulated impulses (7684c0) into S+2ec0 / 2ed0 / 2ee0, clamps the angular part and adds them to every body's velocities. |
| 00433380 | RavenSimulation_vf24 | 4,228 | ecx; ret 4 | Slot 24, set input (ECX = S, stack 64-byte block): copies it to S+1c, zeroes non-finite axes, brake-hold and reverse rules. |
| 004324d0 | RavenSimulation_vf25 | 3,991 | ecx; ret 0 | Slot 25, enter-rest test (ECX = S). |
| 0045d510 | RavenVehicle_vf0_o90 | 3,247 | ecx; ret 4 | Vehicle pre-step listener (ECX = vehicle+0x90, stack dt): rider-off forcing, then input routing by simulation type (435030 + slot 23, Simulation_ReadControllerInput, or slot 24). |
| 004460f0 | BikeGear_SelectDownshift | 2,994 | -; ret 4 | Gear choice while throttle <= 0 (stack S): wheel rate from forward speed and radius S+48ec, engine limit S+5014, scores the 5 ratios, shifts down after the timer. |
| 0043c440 | RavenSimulationMotocross_vf7 | 2,398 | ecx; ret 4 | Slot 7 (ECX = S, stack dt): saves or restores the root pose / velocity (S+8c28, S+8c30..), timer S+68. |
| 0045d820 | RavenVehicle_vf1_o90 | 2,163 | ecx; ret 4 | Vehicle post-step listener (ECX = vehicle+0x90, stack dt): clears S+2f00, motion flags, vehicle update (vtable +0x74), distance travelled. |
| 00440da0 | Fn_00440da0 | 1,634 | edi; ret 4 | Stack dt, EDI = S: airborne stunt latches S+8c60.. / 8cd0.. and stick gesture flags S+8d48.. used by 43f2e0; semantics unclear. |
| 0043b5e0 | RavenSimulationMotocross_vf5 | 1,430 | ecx; ret 0 | Slot 5 (ECX = S): OR of the body-probe contact flags (some only when S+8c20 == 0). |
| 00432360 | RavenSimulation_vf26 | 1,326 | ecx; ret 0 | Slot 26, leave-rest test (ECX = S). |
| 00447750 | RavenSimulationMotocross_vf30 | 1,209 | ecx; ret 0 | Slot 30 (ECX = S): slot 29 and all four wheel contact scalars S+8850/8854/89a0/89a4 nonzero. |
| 00430eb0 | RavenSimulation_vf1 | 912 | ecx; ret 4 | Slot 1 (ECX = S, stack frameDt): pending tuning reload (slot 8), then UpdateFixedSteps when S+b0. |
| 00435320 | Simulation_ApplyDebugKeyForces | 878 | ecx; ret 0 | ECX = S, local simulations only: six debug input codes push body S+2eac with force S+2ea8 / torque S+2ea4. |
| 0043b640 | RavenSimulationMotocross_vf8 | 874 | ecx; ret 0 | Slot 8 (ECX = S): applies a pending tuning XML (global 9e3100, critical section ad7538) to the parameter block S+2f10 and the mechanism masses (43b750). |
| 00447730 | RavenSimulationMotocross_vf29 | 855 | ecx; ret 0 | Slot 29 (ECX = S): 1 when both wheel contact flags (S+8890, S+89e0) are set. |
| 004300a0 | ModelNodeRef_SetMatrix | 798 | ecx; ret 4 | ECX = node reference, stack 4x4 matrix: stored in the owner's matrix table, then 7db7e0 marks the hierarchy dirty. |
| 004308a0 | Simulation_ReadControllerInput | 693 | eax edi; ret 0 | EAX = controller index, EDI = 64-byte input block: samples the action axes and buttons (simulation-input.md). |
| 00430850 | Simulation_InitInputBlock | 627 | eax; ret 0 | EAX = 64-byte input block: all zero except +0x38 (brake-hold latch) = 1. |
| 00430500 | RiderRecord_SetVec60 | 532 | ecx eax; ret 0 | ECX = record, EAX = vec3 source: record+0x60..0x68 (x87 copies). |
| 00430520 | RiderRecord_SetVec70 | 532 | ecx eax; ret 0 | ECX = record, EAX = vec3 source: record+0x70..0x78. |
| 00430540 | RiderRecord_SetVec80 | 532 | ecx eax; ret 0 | ECX = record, EAX = vec3 source: record+0x80..0x88. |
| 00435030 | Simulation_HandleDebugKeys | 485 | edi; ret 0 | EDI = S: developer keys (animation test mode, gravity toggle, push-tool force / body selection, ...), printf messages. |
| 0044e0d0 | BikeControls_UpdateRiderOff | 417 | edi; ret 4 | Slot 9's path with the rider off (EDI = S, stack dt): zeroes the control block fields, drag direction from the root velocity, curve lookups, 43e9f0, control quaternion. |
| 0044bff0 | RavenSimulationMotocross_vf24 | 330 | ecx; ret 4 | Slot 24 (ECX = S, stack 64-byte input block): RavenSimulation::vf24, mirrors steering into the control block. |
| 004301f0 | rtti_shared_vf_004301f0 | 302 | ecx; ret 4 | Shared slot (8 classes, ECX = this, stack out): copies the vec3 this+0xc0..0xc8. |
| 00430490 | RiderRecord_SetVec90 | 266 | ecx eax; ret 0 | ECX = record, EAX = vec3 source: record+0x90..0x98. |
| 0045d910 | Fn_0045d910 | 250 | edi; ret 4 | Called by every RavenVehicle*::vf29 for local simulations (EDI = vehicle, stack dt): a smoothed global quaternion / position in one of 5 modes (global a9418c, debug key 0x64); consumer unclear. |
| 00433120 | Simulation_GetReplayState | 190 | ecx; ret 4 | ECX = S, stack out record: position, quaternion, velocity, angular velocity, time, flags and the 7 input axes at full precision. |
| 004304f0 | RiderRecord_SetFloatAC | 152 | eax xmm0; ret 0 | EAX = record, xmm0 -> record+0xac. |
| 004392d0 | RavenSimulationMotocross_vf3 | 152 | ecx; ret 0 | Slot 3 (ECX = S): clears S+89e4 and S+8894 (wheel contact words). |
| 004304b0 | RiderRecord_SetFloatA0 | 76 | eax xmm0; ret 0 | EAX = record, xmm0 -> record+0xa0. |
| 004304c0 | RiderRecord_SetFloatA4 | 76 | eax xmm0; ret 0 | EAX = record, xmm0 -> record+0xa4. |
| 004304d0 | RiderRecord_SetFloatA8 | 76 | eax xmm0; ret 0 | EAX = record, xmm0 -> record+0xa8. |
| 004304e0 | RiderRecord_SetFloat88 | 76 | eax xmm0; ret 0 | EAX = record, xmm0 -> record+0x88. |
| 0044bfb0 | RavenSimulationMotocross_vf23 | 72 | ecx; ret 4 | Slot 23 (ECX = S, stack controller): Simulation_ReadControllerInput into S+1c, mirrors steering / shift axes into the control block. |
| 0044e570 | RavenSimulationMotocross_vf10 | 62 | ecx; ret 4 | Slot 10 (ECX = S, stack 3 vec4): pose override targets S+2df0 / 2e00 / 2e10 and the steer angle S+2e70. |
| 0043cfe0 | BikeWheel_ContactPositionCorrectionRiderOff | 56 | ecx edx; ret 4 | Rider-off variant of 431ee0 (ECX = W, EDX = S, stack out vec4). |
| 0043f270 | Simulation_ClampRootAngularVelocity | 47 | -; ret 4 | Stack S: root angular velocity S+740 capped at length 10 (all lanes rescaled). |
| 0044e830 | RavenSimulationMotocross_vf34 | 43 | ecx; ret 0 | Slot 34 (ECX = S): applies the pose override by mode S+2de4 (jump table), copies targets to previous. |
| 00430270 | RavenVehicle_vf22 | 5 | -; ret 0 | RavenVehicle slot 22: returns 0x1082. |
| 00434ef0 | Simulation_EndPoseOverride | 1 | edi; ret 0 | EDI = S: ends a pose override (S+2de0); mode 2 moves the mechanism back to the saved pose and zeroes velocities. |
| 00446cd0 | RavenSimulationMotocross_vf33 | 0 | ecx; ret 0 | Slot 33 (ECX = S): begins a pose override (S+2de0 = 1): saves the root pose, resets the targets; mode 2 also saves contact points. |
| 00447790 | RavenSimulationMotocross_vf31 | 0 | ecx; ret 4 | Slot 31 (ECX = S, stack out vec4): normalized sum of the two wheel contact normals S+8a10 + S+88c0. |

## Lead edits after the wave

- 0045d510 RavenVehicle_vf0_o90 outputs set to 0 in bike_45c.cpp (hand edit: carry it into the generator's config when it is
  regenerated). Its only caller, World_PhysicsPreListeners 007699e0, reads no register after the call. BIKE_OUTS flagged the
  scratch registers left by collision's hand-written BroadphaseGrid_Update 0084a1a0, which no caller reads.
