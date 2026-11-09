# ATV simulation continuation — 2026-10-21

Landed first batch, awaiting parent original verification:
- S19 0047c460: exact FLD float steering S+5898 into extended return (including signed zero).
- S23 0049ee00: existing Simulation_ReadControllerInput then raw shift fields4c/50 into5970/5974.
- S10 004a0e60: copies three input vectors in source order, computes original fixed tilt quaternion with cosine polynomial2, relative quaternion, x87 atan2(length xyz,w) with m32 conversion, signs double angle from forward-vector dot. SSE sums012 versus021 preserved.
- 00496310 (EDI=S,stackdt RET4): six cone-limit flags, hysteresis/stunt input rate peaks and resets, existing phys_quat_within_cone_limits/phys_remap_clamped. COMISS ordered/unordered decisions transcribed; source disassembly /tmp/atv-96310.asm.

New files game/vehicle_atv_physics.c/.h, oracle/vehicle_atv_physics.cpp and developer-only vehicle_atv_physics_probe.cpp. Oracle group rx_AtvPhysics. Probe filter `REFLEX_PORT=rx_AtvPhysics,rx_AtvPhysicsProbeHook REFLEX_PORT_VERIFY=1 RX_ATV_PHYSICS_PROBE=1 RX_ATV_PHYSICS_FRAME=650` on actual original ATV world garage0,0,-1,8,2,4. Thirteen isolated stimuli restore complete S: steering4,controller2,relativepose3 (both Y signs),contactfilters4. Additional synthetic native fixture tools/atv_physics_test.c prepared. Only clang syntax check performed by agent; parent owns builds/tests/oracle acceptance.

## Scheduled debug helper observable effects

0049d7c0 (EDX=S,RET0) is the V61 scheduled debug geometry helper, not a force kernel. Its original callees0049d550 (618bytes) and0049eae0 (788bytes) compute only stack-local debug line endpoints; eae0 returns S+index*a0 and caller discards those values. The caller's only persistent simulation write is `0049e8c4 mov dword ptr [edx+0x90e0],0`. V61 consumes no return. No floating-point environment output is part of this void interface; product SIMD calculations do not emulate status flags.

Rigorous read-only audit: Capstone detailed operands for the exact functions.tsv byte ranges (0049d550+618,0049d7c0+4879,0049eae0+788), selecting operands typeMEM with accessCS_AC_WRITE and base notESP/EBP. Result: zero external writes in either leaf, exactly0049e8c4 in caller. Caller direct calls are only those leaves and security cookie. Complete original decompile similarly has only that S store and dead locals. Proposed source preserves that sole observable simulation write. Requires original positive-call Verify before acceptance. Staged in /tmp until parent's current build freeze is released.

## Remaining closures

S20 0048f9e0 (4487bytes) is terrain/raycast pose construction. S11 wrapper dispatches seven ATV-specific kernels; its0049a340 wheel helper alone is9609bytes. S9/12/13/15 control/contact chains and their actual helper dependencies remain unported; no playable ATV claim from the bounded batch.

## Additional original force components (pending original verification)

The following are landed in separate physics files, with oracle adapters, not yet accepted as simulation parity:
- 004931a0 S17 +004a17b0: roll spring/damping integration and root-relative wheel spin diagnostics. Original assembly004a199c..19c7 multiplies only projected force by zero, then adds spring*-75 and negative body-local torqueY; decompiler parentheses incorrectly suppress the spring term. Constants -.0,-75,.96 and source order retained.
- 00498db0: four axle drive/brake torques and root reaction, preserving rear-left/front-left/rear-right/front-right update order and quaternion torque conversion.
- 0048d430: root angular-speed cap20 and child cap009c4750, with original021 horizontal sum and four-lane multiply/divide; sets90e0 only on clamp.
- 00493770: contact resistance via existing IndexedCurve_Interp, root drag and cached external wheel forces. Four contact force contributions sum in original rear-left/rear-right/front-left/front-right order; five Mechanism_AddForceAtPoint calls and angular damping retained. Source clamp preserves NaN through COMISS branches.
- 00493c70: four suspension spring/damper pairs. Root axis and angular velocity transformed with original quaternion products; spring travel uses021 sum, relative speed uses012 dot, sign chooses original compression/rebound coefficients. Wheel force at local origin and opposite root force at local wheel offset; four compression flags and rear damper diagnostics preserved.
- 00498110/00498380: five-gear torque comparisons with original cubic curves, current gear drag term, score cancellation with rounding points retained, separate power/coast shift conditions and delays. Velocity transformed to body-localZ; source min/max branches retain unordered behavior.

Provenance: functions above in `_re/out/decomp/reflex_001_00459400.c`; exact disassembly dumps in /tmp/atv-s17.asm,atv-a17b0.asm,atv-98db0.asm,atv-93770.asm,atv-93c70.asm,atv-98110.asm,atv-98380.asm. Shared vector helpers are source-matched in bike_math.h; parameter curves and Mechanism_AddForceAtPoint already ported. Small clang syntax checks pass; they establish neither mathematical parity nor playability. Parent owns positive original Verify and public-session tests.

## First batch original comparison

Parent's `builds/sol_atv/logs/first-physics-verify.log` reports zero mismatches for all11 positive entrypoints: ground placement3, steering4, controller184, relative pose1215, filters704, roll spring3, wheel spin1559, axle torque1559, spin clamp1558, resistance1558, debug poststep3. State hash matches reference `a01204e338e8cd97`. Full page dumps subsequently prove the only difference is original-only resident page `1002b000`, the previously documented raycast/ConVar page; every shared page is byte-identical, including the job ring. Evidence: `builds/sol_atv/physics-leaves-memory-comparison.json` and `physics-leaves-dump-{on,ref}.log`. This accepts the first batch with that documented residency exception; newer suspension/gear/engine work remains separately pending. It does not establish a playable native ATV.

Second drive verification (1400 frames): original-positive gear selection power1430/coast1629 calls passed with zero mismatches, alongside all eleven first-batch entrypoints. Suspension00493c70 produced3111 mismatches and remains unaccepted; no approximate force correction has been applied. A developer-only RX_ATV_SUSPENSION_DIAG diagnostic now compares front-left axis/spring/speed/local point/angular/force against original instruction hooks0049407b/004940d3. Parent separately proved the first-batch On/reference memory difference consists only of original lazy page1002b000, with zero differing words elsewhere.

Engine/clutch004985e0 candidate is now present in vehicle_atv_physics_engine.c and adapter. Its thirteen explicit original probe cases cover the real baseline and all three clutch modes across four rear-contact patterns, including reverse and clutch endpoints. Original Verify acceptance is pending; syntax/source review alone is not acceptance.

Suspension diagnosis: original/native front-left axis, spring, dot speed, local point and force match bit for bit. Mismatches are root reaction and rear wheel forces. Original0049448a and00494757 construct rear spring anchors with SHUFPS0xa4 from frame7e90/87f0 plus broadcast009778a4 (bitsc0266666, -2.6): x/y come from the frame; z/w are the constant. The first implementation incorrectly used each frame's original z. Rear-only anchor correction landed; original rerun still required.

Ackermann helper00499220 (ESI=S) candidate landed in vehicle_atv_physics_steering.c with six explicit original probe cases, including its small-angle threshold and QNaN. It computes dedicated left/right steering quaternions from original front/rear wheel distances, higher-order trig polynomial, and original rational atan. No generic steering substitution. Parent syntax check is not original acceptance.

Fourth drive Verify: parent confirmed all16 accepted physics entrypoints, including suspension3111, engine3124 and Ackermann3117, zero mismatches. State00d6e8c5df222184, memoryeac22e90c60b39a1.

Dedicated traction0049a340 and S11 dispatcher00490b80 then landed. Twelve explicit traction probe cases passed, but natural driving isolated4147/12456 traction mismatches, beginning rear-left call6275 with a1-ULP force difference. Original disassembly0049b34e..35d and0049b383..399 multiplies coefficient*curve before load; the initial transcription incorrectly associated load*coefficient first. Corrected both friction-base products to load*(coefficient*curve), preserving original SSE association. New Verify pending.

Full S9 0049ee30 and rider-off004a0ab0 candidates landed for original verification/native boundary checks. They use original dedicated steering/heading and wobble programs, brake/gear/clutch controls, rising terrain query, per-wheel friction material inputs and rider-offset filters. No generic controller replacement or no-op physics. C syntax checks passed; no playability claim follows from syntax.

Sixth drive/probe verification: parent reports S7 00493630, relative capture S33 0048e8a0, relative application S34 004a1250 (523 calls), and shared S27 raw getter00432190 all zero mismatches. S9 0049ee30 had23/3113 mismatches, first one in59d0 heading filter; original00977678 is3c8efa34, one ULP below the initial degree-conversion literal. Exact bits now landed. A separately audited negative reflex branch also now preserves abs(reflex)*(angle*.2) and has two explicit mirrored-sign probe cases. New original rerun pending.

Natural traction still differs after the coefficient grouping correction; do not treat its12 passing focused cases as whole-function acceptance. A developer-only nullable observer and read-only original instruction hooks now compare the limits, demanded impulses, slide contribution, clipping and final force at the first natural mismatch (call6275). Run isolated rx_Atv_TireContact Verify with RX_ATV_TIRE_DIAG=1 and RX_ATV_TIRE_DIAG_START=6275. Observer is null in the product; it writes no game memory.

Historical static coverage at the sixth batch was 34/36 slots. Parent subsequently registered deleting destructor S0 0048c730 in vehicle_atv_destroy.c and pending-tuning dispatcher S8 0048d320 in vehicle_atv_leaves.c: current static S-vtable coverage is 36/36. The development-only pending XML reload path still has explicit original-call boundaries (008252c0 guest stream,0042fed0/00790540 log sink); ordinary imported-tuning startup does not exercise that path. Registration is not a playability claim.

Seventh drive/probe Verify: parent confirmed S9 0049ee30 control3115 calls zero mismatches after exact degree multiplier/reflex grouping, with other new tests accepted.

The tire intermediate trace isolated first natural mismatch6275/6276: stage1 base limits and stage2 rolling impulse/demand match bit for bit, while the clipped rolling impulse differs. Original0049aa45/aa53 saves the wheel body's linear velocity at[F+1e0]. Original0049bbb8..bbf8 uses its dot with the rolling axis as the speed input for sliding friction curves. The initial candidate instead reused the contact-point rolling speed, which includes angular velocity. Corrected curve input to wheel linear velocity; impulse computation retains contact-point velocity. Loaded rear probes now set angularX=8 to make these speeds observably different. Eighth original rerun pending. Original stage3 hook moved to0049c363 to observe both rear-slide and ordinary paths.


## Eighth comparison acceptance and current product boundary

Parent reports the eighth original Verify batch passed all physics, including S11 ForceStep00490b80 (3,111 calls), S9 ControlStep0049ee30 (3,116), and isolated dedicated TireContact0049a340 (12,456), with zero mismatches. This accepts the corrected wheel-linear-velocity friction input and the loaded focused stimuli alongside natural driving. Prior suspension, engine, gear, steering, placement, relative-pose, controller and debug-helper acceptance remains applicable. No physics approximation or forced rider-state workaround was introduced.

Native public-session startup advances without missing calls for 900 frames, but that does not establish a playable ATV: the rider enters the wreck state and throttle remains zero. Parent identified a separate native ABI defect in world_wreck_checker.c vehicle_rate: ATV wheel-load004867b0 and grip00486850 return rx_f80, while the generic fallback called them through a float-returning prototype. The runtime owner is correcting and verifying this boundary. Product riding/recovery acceptance remains pending; this note does not claim the ABI correction has already passed.
