# lift — the one shared lifter

`native/game/tools/lift/` is the single maintained x86→native lifter and `native/game/lift_rt.h` the one runtime header the
generated code uses. It replaces the ten diverging lifter copies that lived in the session scratchpad (bikeag/lift,
engine/lift, fxag/lift, anim/lift, ui_tools, ai_tools, collision, world's x2c/lit2cpp, math_work/tr2, mech/sym). See
`tools/lift/README.md` for how to run it.

## What it is

An instruction-level transliterator: it walks a function's real control flow from the entry (not Ghidra's size), emits one
C++ statement per x86 instruction on guest registers held in locals (`eax..edi`, `esp`, `fl`, `x0..x7`), and keeps the
stack frame on the guest stack so callees see the same memory. Semantics are the interpreter's (`vm.cpp`): SSE via
`sse32`/`sse64` (x86 NaN + min/max rules, rcp/rsqrt tables), x87 via `reflex_f80` with the control word, the cvt rules.
It refuses (with the address) anything it cannot translate exactly — never guesses.

## Two flag backends, one header

The ten copies used two different flag strategies; the lifter keeps both, selected per group:

- `lift.py` (captured): flags only where a consumer reads them, resolved statically to the defining instruction — fused
  compare→branch, or captured per-instruction (`fA_/fB_/fK_`), or materialized into one word `FL` where read on several
  paths. Used for `bike`, `engine`, `fx`, `anim`.
- `lift_fl.py` (materialized `fl`): one EFLAGS word updated only where read; handles `sahf`, `lahf`, `leave`, `xchg`,
  `shld`/`shrd`, `cmov`, `fcom`/`fcomp` into the status word, `fnstsw` with TOP, string ops with the direction flag. Used
  for `ui` (and suits `ai`).

Both emit against `lift_rt.h` (namespace `lift`, macros `LIFT_ENTRY`/`LIFT_CALL`/`LIFT_CALLB`/`LIFT_TAILJMP`/`LIFT_RET`/
`LIFT_PUSH`/`LIFT_POP`/`LIFT_OUTS`). `lift_rt.h` is the union of the old per-group headers (bike SSE/x87/captured-flag
helpers + fx/anim rcp/rsqrt tables + anim integer-division helpers + ui materialized-EFLAGS helpers + the blocking-call
macro). The superseded `ports/{bike,engine,fx,anim,ui}_util.h` are deleted (nothing includes them).

## Union of capabilities merged in

From the copies: real control flow with bounded / two-level / unbounded jump tables; every SSE/x87/MMX/integer form;
`fs:` → the TEB at `FS_BASE`; register-synced calls of any convention with the original return address written into the
dead slot; tail jumps (`jmp f`/`jmp reg`/`jmp [IAT]`/`jmp [reg+disp]`); blocking worker-thread calls (`LIFT_CALLB`, the
ENG_CALLB semantics); runtime hooks inside a function (`HOOKS`, looked up in `m.natives` and resumed at the pc returned);
import calls through the IAT; `mul`/`div`/`idiv`, `cmpxchg`, adc/sbb carry-chains, rep movs/stos (b/d).

## Proof (regeneration)

`gen.py --from-ports` regenerated every port of `bike`, `engine`, `fx`, `anim` (captured backend) and `ui` (fl backend)
into the same file names, keeping each group's group string, port names, declared outputs and comments. The regenerated
code is functionally identical to what the per-group copies produced (the only textual differences are cleaner spellings —
`movsd_m` for a zero-extending `movq`, `udiv32`/`sdiv32` helpers, the named `FS_BASE` constant, and trailing-comment column
alignment, since `LIFT_` is two characters longer than the old `BIKE_`/`UI_` prefixes). `tools/verify.sh <boot> all` then
passes with `on: identical` and the reference hashes on all five worlds (Concord f1a02da0f3546f3b, Concord_ai11
62b9d1986dc7b8ba, Alpine 9c6ce38c09cfa013, SX_Springfield_ai11 9895014866da6382, TRX_Verde_ai11 28623e7b63b760c9); the only
verify-mode mismatches are the known dead-stack transients in `dead-stack.md`.

## liveness.py

`tools/liveness.py` (→ `plan/live.tsv` → `live.inc`) gained an opt-in `--must-define` refinement (outs2.py's whole-program
idea, as a sound lower bound: a call to a known function kills only the registers that callee writes on its straight-line
prefix, so a value kept in a volatile register across an LTCG call stays live-out). It is OFF by default so the committed
`live.inc` is unchanged; regenerate with it only together with a full `verify.sh` re-run, because it can only add live bits
(strictly stricter Verify) and would expose any hand-written port in another group that declares a register its native
leaves differently.

## Not yet moved

`gen.py` can also regenerate the other lifted groups, but they were left on their committed files this pass (they already
pass): `ai` (fl backend), the world literal ports and `world_node_gen` (a register-level `m.r[]` style, `world_node_xc.h`),
`math_gen` (tr2 Cpu style), `collision_lifted` (collision_lift.h). Moving them means porting their util headers' remaining
specifics into `lift_rt.h` / adding a third emit style, then re-verifying each.
