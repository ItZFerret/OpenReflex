# Four-wheel simulation startup leaves — 2026-10-21

Hand-written native ports (no original instruction stream executed by product):

| Class | vf35 parameter scaling | vf2 placement/reset | vf16 suspension frames |
|---|---|---|---|
| UTV | 004cc610 | 004d75b0 | 004d4750 |
| Buggy | 004b1e10 | 004bce80 | 004ba270 |
| TrophyTruck | 00450ab0 | 00459400 | 004564d0 |

Sources are the matching original functions in `_re/out/decomp/reflex_*.c`, checked against original executable disassembly using `_re/tools/disasm.py`. Implementations are `vehicle_fourwheel_methods.c`, `vehicle_utv_geometry.c`, `vehicle_buggy_geometry.c`, and `vehicle_truck_geometry.c`. Oracle adapters use `rx_vehicle_fourwheel`.

Important class differences retained: Buggy runtime state offsets are 0x10 below UTV; Truck vf35 engine output begins at8668 rather than8664. UTV placement rear arm offsets use the lateral axis with opposite signs; Buggy/Truck use the vertical axis with subtraction on both sides. UTV suspension travel fallback is .1, Buggy front fallback .5. Buggy rear arm direction normalizes twice and uses the original -.35/-.9 fallback vector (separate -.7 comparison threshold). Truck front arms use normalized directions and class7-specific fallback literals; its shared rear axle uses median positions, tangent orthogonalization, original x87 atan2 and cosine polynomial quaternion construction. Deliberate duplicated/nonzero w lanes remain.

Independent source review caught two corrections before final oracle build: reset's global mouse-sensitivity rate record is unconditional (not a FLT_MAX sentinel setter), and the UTV/Buggy right-front frame assembles sqrt-travel plus reflected anchor before adding the rotated adjustment. Scalar decompile expression grouping alone was insufficient for that SSE site.

Development stimulus is `core/oracle/vehicle_fourwheel_probe.cpp`: use a booted actual original UTV/Buggy/Truck world, `REFLEX_PORT=rx_vehicle_fourwheel,rx_FourwheelMethodsProbeHook REFLEX_PORT_VERIFY=1 RX_FOURWHEEL_METHODS_PROBE=1`. Optional `RX_FOURWHEEL_METHODS_FRAME` defaults650. It saves actual simulation state and twelve pointed-to range records, calls three tuning cases, three geometry cases, and a five-body reset, then restores state. Reset stimulus suppresses external contact queues/rays only within the saved fixture. Verify call counts and differences are the evidence; the probe's completion line alone does not establish comparison success.

At writing, source has been independently reviewed but no build, fixture or oracle was run by this agent. Parent owns serial verification and will report results. vf32 contact-ray construction is still unported for all three classes (004da510/004bf400/0045b8f0); it remains an explicit boundary. These leaves do not establish complete playable support.

A read-only scan of the existing Terracross_TRX_Verde_ai11 RXWI found none of the actual ATV/UTV/Buggy/Truck simulation vtables or UTV/Buggy/Truck vehicle vtables in heap pages. Positive original construction should use actual garage selection / valid spec data, rather than assuming this snapshot contains mixed machines.

## Central oracle acceptance

Real garage-selected original Alpine worlds for UTV, SuperBuggy, ProLite and Pro2: each tuning method3calls, reset1call, geometry1559calls; all final Verify counts positive with0mismatches. On/reference state and whole-memory hashes match for each world. Evidence: `builds/sol_core_features/{utv,superbuggy,prolite,pro2}-acceptance.json` and corresponding logs. The first Buggy test caught its rear fallback z/w literal: original address977744 is-.9, distinct from its-.7 threshold. Corrected and rerun; initial failure is preserved as `logs/superbuggy-verify-first-fail.log`. Native product startup testing is separately recorded by the parent; these comparisons do not establish playable full vehicles.


## Later contact and ground-reset ports

The vf32 boundaries above are now implemented and verified, along with vehicle vf33 ground resets. See HANDOFF/SESSION_2026-10-21_REMAINING_PORTS.md and builds/sol_ports/acceptance{,-ground}.json. Native next boundaries are UTV004c1fa0, Buggy004a4270 and Truck0046c090 full model/suspension poses; four-wheel playability is still incomplete.
