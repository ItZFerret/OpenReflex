# rx_bike: the motocross bike simulation in the OPEN REFLEX core

Work list `core/plan/rx_bike.tsv` (83 functions). Prefix `bike_`, oracle group `rx_bike`. Rules: `core/README.md`. The
simulation object's layout and the call graph are in `native/game/notes/bike.md` (the RE notes); this file is about the port.

## Files

| File | What |
|---|---|
| `game/bike_math.h` | the inlined SSE / x87 idioms as named helpers: hsum012 / 021, normalize-or-NaN masks, the quaternion product with the sign tables 0093a1f0.., rotate / rotate_inv, cross (shufps c9 / d2), all3 mask test, the asin rational approximation, the two cosine polynomials, matrix -> quaternion, Vec3_Cross 00401630, x87 float copies (sNaN quieting), 53-bit x87 |
| `game/bike_curve.c` | 00430810 Lerp_Clamped, 00438ee0 ParamCurve_EvalCubic, 004390d0 IndexedCurve_Interp |
| `game/bike_tire.c` | 00447f10 the tire contact force (slip, grip curves, load filter) |
| `game/bike_force.c` | engine / gearbox / suspension / wheel forces; 0043e100 the force stage (vf11) |
| `game/bike_response.c` | 004360b0 contact response, angular velocity clamps, wheel spin, slot 15 |
| `game/bike_controls.c` | 0044c170 vf9 the controls (steering, lean, brakes, terrain segment query), rider off, ground direction |
| `game/bike_slot12.c` | 00443610 vf12, 0043f270 spin cap, 00440c40 stick circle -> square, 00441300 the grounded correction |
| `game/bike_air.c` | 0043f2e0 the in-air control, 00440da0 the stunt latches / stick gestures |
| `game/bike_slot13.c` | 00444b90 vf13 (wheel contact), 00445800 rolling orientation, 00446f80 rest pin, 00435b00 body probes, 00438260 wheel / triangle contact |
| `game/bike_contact.c` | terrain contact queries (00444a40, 004456d0, 00431ee0, 00438180, 0043cfe0) |
| `game/bike_post.c` | slots 16 / 17 / 32, 0043ed40 pitch spring |
| `game/bike_sim.c` | the simulation's small slots, replay record, pose override, step 00431d60 (41 functions) |
| `game/bike_vehicle.c` | RavenVehicle step listeners 0045d510 / 0045d820, 0045d910 the debug view |
| `oracle/bike_<file>.cpp` | one adapter file per game file; `oracle/bike_oracle.h` the shared adapter helpers (Scratch, xmm access, argf, check_cw) |

## Conventions

- Hand transliteration from the disassembly (`_re/.venv/bin/python _re/tools/disasm.py <addr> <n>`); the decompile
  (`_re/out/decomp`) only for structure: it drops masks and reorders. Float operations in the disassembly's order and association
  (phys_add / phys_mul ... for SSE scalar, v4_* for packed, C double for cvtss2sd .. mulsd, the x87 at 53 bits via rx_x87.h).
- Commutative operand swaps (a * b vs b * a, a + b vs b + a) are not reproduced: they only differ in which NaN payload survives.
- Locals passed by address live on the game stack: `sp = rx_sp(); F = (sp - size) & ~15; rx_set_sp(F); ... rx_set_sp(sp)`.
  Where a callee reads stack it did not write (dead stack: 007c6cc0) or the frame matters, the original's frame is rebuilt:
  A = (entry_sp - 4) & ~15 (push ebp; and esp, -16), locals at A - k (bike_controls.c ctl_terrain_segment, bike_slot13.c,
  bike_vehicle.c 0045d910). Do not use RX_FRAME_ENTER and then align F down: the locals end up below the stack pointer.
- .rdata constants (0x916000..0x9a58fb) are literals with their address in a comment; .data values (>= 0x9a6000: 009cd9ac,
  009cd958, 009cd970, 009d7894, 009ddfc8, 009cc8a8, 00d616a0 ...) are read from memory (the game writes some of them).
- Callees: other bike functions directly; the CRT through `crt.h` (crt_ci_atan2 / _fmod / _pow at CRT_CW_MAIN); physics helpers
  that have core ports directly (phys_lag_filter_update / _set, phys_mech_rotate / _translate / _set_velocity /
  _set_node_*_velocity, phys_quat_within_cone_limits, phys_remap_clamped, phys_vec4_sincos, phys_quat_slerp, phys_vec4_floor,
  rx_vehicle_set_limits); everything else through rx_call_original(_ex) with the original's registers (list below).
- x87: the simulation thread runs with CW 0x027f (53-bit precision); check_cw in the adapters asserts it.

## Adapters

`RX_PORT(addr, Name, "rx_bike", outputs)`. `Scratch s(m)` saves / restores the scratch registers around the native; then the
adapter sets the registers the original leaves and its callers read (native/game/plan/live.tsv), e.g. the security cookie in ECX
(00447f10, 00441300, 0045d910), mask lanes in EAX / EDX (0043f270, 0043f2e0), the ring index / &q in EDX (00438260). Void virtual
slots declare no outputs.

## Verifying

Build (the bike code needs the physics / CRT / loop helpers it calls):

    RX_CORE_FILES="bike_* phys_quat* phys_misc* phys_math* phys_mech* phys_crt* phys_body* phys_rig* crt* loop_vehicle* loop_util*" \
      native/game/tools/build_cli.sh <out>
    native/game/tools/verify.sh <out>/reflex_boot rx_bike

Filters must name ports `rx_<Name>`: a hex address also installs the old lifted port ("bike" group) at the same address.
Nested ports are not verified (the outer native runs them natively); verify a function alone to check it.

## State (2026-10-19)

All 83 ported and verified (Verify 0 mismatches, On identical) in the five worlds, except where a path never runs there:
- 0043cfe0 is never called in the test worlds.
- 0045d910 runs only mode 0 there; modes 1..3 were checked with a temporary hook forcing the mode (On-mode memory hash identical
  to the original over 300 frames each); the developer key path (mode cycling) has not run.
- 00440da0: EDX on return is the cone tests' CRT leftover; not reproduced (the rx_phys port of 0042de70 does not either).

## Calls still through rx_call_original (integration candidates)

Core ports exist for: 00402dc0 (world_veh_entity), 0085ddd0 / 0085c9d0 / 0085c770 / 0085ca40 / 0085d100 / 0085d270 / 0085d300 /
0085d330 (phys mechanism), 0042db20, 0042bd90, 007c6cc0 (needs the original's stack, see above), 007c6160, 007c5e80, 007db7e0,
007684c0, 0075c580, 00623310, 00548660. Not ported anywhere yet: 0085c8b0, 00790540, 005485e0, 004343e0, 00432be0, 00432680,
0042fed0, 0042d7f0, the vehicle / input virtual slots, KERNEL32 critical sections, OutputDebugString.

## Traps

- x87 CW is 0x027f on the simulation thread (53-bit), not 0x007f.
- `fld m32; fstp m32` quiets a signalling NaN: bike_x87_copyf.
- `lahf; test ah, 0x44; jnp / jp` after ucomiss: jnp = exactly one of ZF / PF = ordered equal; jp = not equal or unordered.
  An `lahf` changes AH of EAX: 00440da0's returned EAX carries it.
- The verify oracle keeps the original's state: a native's writes never persist in Verify mode (test hooks must use On mode).
- Two core files register 00401700 (phys_mech.c phys_vec3_sub and world_veh_math.c rx_world_vec3_sub): linking both aborts at
  start ("registered twice"). The bike code therefore has its own Vec3_Cross (bike_vec3_cross) and does not link world_veh_math.

## FREER AIR (port addition, w-freeair 2026-10-29)

An OPEN REFLEX option, off by default: `rx_session_set_free_air(s, mode)` (`include/rx_api.h`), `game/free_air.h` / `.c`, gated in
`bike_air.c` (MX 0043f2e0) and `vehicle_atv_controls.c` (ATV 00494910). The Unity row is OPTIONS > OPEN REFLEX > FREER AIR.

What pulls a bike back in the original's air model (measured with `tools/bike_parity_probe.c` on FR_Alpine's second jump, 2.3 s of
air, the w-physparity whip scripts; per 7.5 ms step):

| mechanism | where | what it does | FREER AIR |
|---|---|---|---|
| trick hold | air control, `hold` | after a cone latch (pitch S+8c60.., roll S+8cd0..) set, steering stick past 0.9 the *other* way: no stick control, the bodies slerped toward S+8c10 at 0.012 | bit 1 removes it: the stick keeps control |
| auto-level ("settle") | air control, else-branch | 0.25 s of stick use (S+8b74), then every step without a stick: slerp toward S+8c10 at 0.008 | bit 2 removes it: the bike keeps its angle |
| take-off orientation S+8c10 | re-aimed every air step | level, nose along the velocity: the target of both slerps | unchanged (no slerp uses it any more) |
| assist fades S+8bd4 / 8bd8 | 1 at take-off, -2 / -1.5 per s with a stick, -2.5 per s without | blend the stick mix from rate (flick) terms to position terms; never refill in the air | unchanged (normal control) |
| air lag filters S+8b78 / 8b8c / 8ba0 | 0.1 s | smooth the stick mix into roll / pitch / yaw | unchanged (normal control) |
| AngularRateDamping S+5680 | every air step | w *= 0.985 (MX 250 model 1: time constant 0.5 s); sets the stick's rotation rate (steady state = input / 0.015) | unchanged |
| OnTakeoffDamping S+56e0 | first air step | w *= (0.85, 0.25, 0.8) | unchanged |
| angular velocity cap | every air step | |w| <= 10 rad/s (stick whips reach ~3) | unchanged |

Both slerps rotate the bodies directly (phys_quat_slerp + phys_mech_rotate), not the angular velocity: they are what "snaps the bike
out of a whip". Without them the stick's own model is all there is: a counter-steer turns the bike back at the stick's rate (slower
than the 0.012 snap: a 0.55 s full counter only slows a 3 rad/s whip and turns it back ~15 deg), and a released bike coasts on its
angular velocity (about 0.5 s x w) and then stays. Hands-off jumps (no stick use) are not changed by either bit (the auto-level only
starts after 0.25 s of stick use, the hold needs a latch and a stick past 0.9).

Gate: `rx_fa_for(S)` reads one host static; only when it is non-zero it walks the local players (MXGame +0x153ec, human ones,
vehicle +0x234 == S). Mode 0 is the original bit for bit (product traces and the 12-rider pair lines hashed against main; the
oracle's Verify of 0043f2e0 / the whole step against the original). Computer riders never get it (the gate compares S with the
human local players' simulations only). The flag is a host static: reset with the core's statics at destroy (session-reset.md), settable any
time between frames.

### AUTO LEVEL / COUNTER-STEER SNAP strengths (w-airsliders)

The two mode bits became two strengths, `rx_session_set_air_helpers(s, auto_level, snap)` (0 .. 1; the options' sliders, default 100 %):
1 = the original's helper, 0 = removed (= bit 2 NO_SETTLE for the auto-level, bit 1 NO_HOLD for the snap), in between the helper still runs
with its per-step slerp blend scaled linearly (auto-level 0.008 x level, trick hold 0.012 x snap: `rx_fa_settle_rate` / `rx_fa_hold_rate`), so the
bike is turned back that much slower. At 1 no flag is set and the helper's code takes the original's path untouched (the blend constant is never
multiplied); `rx_session_set_free_air(mode)` stays: a set bit = strength 0, a clear bit = 1. At a partial snap the hold still takes the stick away
for the step (as the original's does) and turns the bike back at the scaled rate, so a low snap gives a weak, stick-less correction; the
slider's useful range is near the ends. The gate, the local-human-only rule and the AI are as FREER AIR's. AIR LOOSENESS is independent of both.

### AIR LOOSENESS (w-freeair2)

`rx_session_set_free_air_looseness(s, L)` (0 .. 1; the options' AIR LOOSENESS slider; w-airsliders: default 0 %, always available, the FREER AIR toggle is gone). Same
gate and players as FREER AIR (`rx_fa_for` returns RX_FA_LOOSE); the factors at 100 % are `rx_fa_curve_value` in `free_air.c`:
- coast damping: AngularRateDamping eased as 1 - (1 - d)(1 - 0.3 L) (0.985 -> 0.9895 per step, time constant 0.50 -> 0.71 s), **only while
  no stick is held after stick use in this jump** (S+8b74 > 0): the held stick's rate and hands-off jumps stay the original's;
- stick lag: the three lag filters run with tau (1 - 0.2 L) (0.1 -> 0.08 s; `rx_fa_lag_update` = 0042d6b0 with the eased tau, F+4 kept);
- stick authority: the stick mix's increment x (1 + 0.1 L);
- take-off damping (S+56e0): factor 0, kept. Easing it let the lip's kick through: a hands-off jump went from 1.45 to 4.2 rad/s and
  landed 78 deg sideways, and a part-stick whip spiked to 1.8 x its plateau rate.
Composition with a physics editor (w-physstudio): every factor eases the value the simulation holds (S+5680, the filters' F+4), so an
edited damping or lag is loosened from its edited value; edits of rx_fa_curve_value itself would change the 100 % end.
