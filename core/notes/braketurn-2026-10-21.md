# BehaviorBrakeTurn bounded closure — 2026-10-21

Source: `_re/out/decomp/reflex_003_0050c500.c`00522eb0,00522fc0,00523230,005232a0,00523300, checked against disassembly via `_re/tools/disasm.py`. Product owner race_braketurn.c/.h, direct state dispatch integration race_beh1_small.c, oracle adapters race_braketurn.cpp, stimulus braketurn_probe.cpp, native contracts tools/braketurn_test.c. No original execution dependency introduced in product.

## Dependency closure

Existing constructor00588590/vtable00940050, attach00522e50, reset00522e70, update00522ef0 and BrakeTurn tuning reader00522c80 were present. Missing vf7 enable00522eb0 was found in the original vtable path, not the raw direct-call census. It sets enabled exactly to the argument; nonzero enters state1 and resets ramp controls; zero calls actual vf6. Completion retains actual virtual vf7(0), rather than inventing direct reset semantics. Unregistered concrete virtuals retain explicit original boundary. Existing SqrtF0042c230 owner phys_sqrtf is the only nonvirtual function dependency. Driver/vehicle/tuning reads use original layouts and table index.

## Instruction-specific semantics

- State1 ramps only for ordered amount<1. Rate+amount operand order is retained; complementary control subtracts rate. Signed reversal is SUBSS from literal negative zero, not unary negation. Both backward-input slots are written separately.
- Target delta subtraction order is x,z,y. Length squared is `(z*z+x*x)+y*y`; normalize only if ordered positive, with `1/SqrtF(len)` and original operand order. Forward dot is `(forward.z*z+forward.y*y)+forward.x*x`.
- End turn if ordered dot>=MinEndAngle or ordered MinEndSpeed>vehicle speed. Equality of speed does not finish. NaNs follow COMISS branches (unordered neither comparison succeeds). The transition timestamp is tuning pause+driver clock.
- Clutch always sets driver+3d0=1. Optional pullback branch uses behavior+c; otherwise disables through vf7.
- Wait/pullback expire only at ordered clock>timestamp, never equality or unordered. Pullback uses two separate negative-zero SUBSS operations.
- The original fld/fstp before SqrtF only copies an already positive single length squared into its argument; no retained x87 result/extended arithmetic. SqrtF returns XMM0. The stack depth remains unchanged.

## Output liveness

`native/game/plan/live.tsv` conservatively lists10005(EAX,EDX,x87) for these addresses; enable is `taken` (indirect-call baseline). Actual direct caller00522ef0 at00522f5d/00522f88/00522f94/00522fa0 reads EAX only, then pops EDI/ESI and returns. The state bodies return constant1/0. Completion vf7 callers00523293/00523395 ignore its return; enable has no x87 math and no specified result. Adapters therefore declare OUT_EAX for four state bodies and0 for vf7; they do not synthesize scratch EDX or an x87 return from a conservative census. Callee-saved register restoration remains the standard RX_PORT wrapper.

## Validation to run centrally

Product source is not marked accepted until lead compiles/tests. Source branch fixture has14 cases covering enable/reset, both clutch paths, ramp skip, normalization/zero vector, angle and speed stop/equality, unordered comparisons, signed zero, wait/pullback transitions, frame preservation and canary. It uses real arena plus fixture virtual callbacks; it is not an oracle comparison.

Oracle: enable `REFLEX_PORT=rx_BrakeTurn,rx_BrakeTurnProbeHook REFLEX_PORT_VERIFY=1 RX_BRAKETURN_PROBE=1 RX_BRAKETURN_PROBE_FRAME=650`. A live AI reference world is required. The Engine::Update hook finds the real behavior object, builds17 branch stimuli, calls each original address through its Verify adapter, and restores behavior/driver/vehicle/tuning state plus machine registers/FPU. Look for `[braketurn] complete:17leaf calls` and per-adapter zero mismatches. Missing live object is an explicit probe error, not success. Full natural race reachability and all5-state runtime acceptance are separate from these fixtures.
