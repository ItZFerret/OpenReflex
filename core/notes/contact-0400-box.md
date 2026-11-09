# Mask0x0400 versus box contact closure

Source ownership: `game/phys_pair_rodbox.c/.h`, `oracle/phys_pair_rodbox.cpp`, `oracle/rodbox_probe.cpp`; minimal direct-call routing in `phys_contact.c`.

The Renne frame8 stop0086b7f0 constructs one contact from twelve intersections against six box faces. It rotates the original global axis into the relative frame, replaces exact-zero components with the original1e-5 bit pattern, sums valid face intersections, averages the point, clamps it to the box surface, rotates point/normal back and appends to the capped manifold. It does not replace this collision type with another shape.

Dependency closure comprises0081e8e0 (face sums),0081da00/0081dc30/0081de70 (X/Y/Z face root pairs). The primitives always store both roots and validity weights, including invalid-root arithmetic; the sum deliberately retains NaN*0 behavior. SSE arithmetic uses the existing x86-exact helpers and quaternion operation order. x87 only copies scalar arguments, for which signaling NaNs are explicitly quieted. Unused vector w lanes remain untouched when the original does not write them.

Frames: pair aligned16/sub0x84/three pushes; sum aligned16/sub0x104/three pushes; face primitives sub0xc/two pushes. Pointer-valued temporaries retain original game-stack addresses. The pair adapter declares liveECX/EDX: finalECX is the security cookie; EDX is the manifold on contact and the stack count pointer otherwise. The sum adapter declaresEDX=count pointer. Face result registers are dead to the caller.

Probe selection: `REFLEX_PORT=rx_phys,rx_RodBoxProbeHook REFLEX_PORT_VERIFY=1 RX_RODBOX_PROBE=1`, with optional `RX_RODBOX_PROBE_FRAME` (default5). Use the established existing-world oracle CLI invocation. At the selected original physics update, the probe supplies192 cases to each of five functions (960 independent calls): finite random inputs, axis zeros/signed zero, tangencies, rotated frames, flip flags and manifold count29/30 saturation. It restores registers/FPU size and continues the original world. This is development-only stimulus, not fake gameplay progress.

Initial source passed warnings-enabled C syntax checks. The coordinated validation results are recorded below.

## Initial coordinated acceptance

The lead built the complete core and isolated oracle CLI. Logs under `builds/sol_core_features/logs/rod-verify.log` show192 calls/192 verified/0 mismatches for each of the five functions (960 probe calls). `rod-on.log` and `rod-ref.log` both end in whole-memory hash `cf018fd73bf8c5a5` over26,569 pages with register summary `7fefff00 00000000`.

Subsequent independent review identified an output-alias ordering difference: original0081ec31 captures the old count after five weighted points, before storing the sixth. Source now retains that order, including count aliases to sum.x/y/z. The probe adds those three cases (face-sum195, other functions192,963 total). Expanded coordinated Verify now passes all963 calls:192 each for the three face primitives and contact kernel,195 for face-sum, with zero mismatches. Current `builds/sol_core_features/logs/rod-verify.log` includes the three alias cases; `rod-on.log` and `rod-ref.log` both report state hash `7c49eb341a2b5d03` and whole-memory hash `159d5a49aec2505a` over26,569 pages. The actual contact caller uses disjoint sum/count buffers.


## Native track acceptance

After this closure and the subsequent buoyancy angular correction, `builds/sol_core_features/logs/native-new-NAT_Renne.log` and `native-new-NAT_RenneB.log` each pass900 actual racing frames: Renne enters RACING at1331 and finishes at2231; RenneB enters at1421 and finishes at2321. These are runtime progress checks, not a full-lap claim. The lead also completed a40-second windowed Renne smoke run (`builds/sol_core_features/logs/player_clean_renne.log`).
