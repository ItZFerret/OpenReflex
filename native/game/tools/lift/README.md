# native/game/tools/lift — the shared x86 → native C++ lifter

One maintained lifter. It transliterates an MXReflex.exe function into a `REFLEX_PORT_FN` body, instruction by instruction:
the same operations in the same order, on guest memory, with the interpreter-exact semantics of `vm.cpp` (SSE through
`sse32` / `sse64` with the x86 NaN and min/max rules, x87 through `reflex_f80` with the control word, the rsqrt / rcp
tables, the cvt rules). The generated code uses one shared runtime header, `native/game/lift_rt.h`. It replaces the ten
diverging per-group lifter copies that used to live in the session scratchpad (bikeag, engine, fxag, anim, ui_tools, …).

## Files

- `lift.py` — the translator (the "captured-flags" backend). Follows real control flow from the entry (not Ghidra's
  function sizes); every instruction form; flags only where a consumer reads them, resolved statically to the defining
  instruction (fused compare → branch), or captured per-instruction / materialized into one EFLAGS word (`FL`) where
  flags are read on several paths. Refuses (raises `LiftError` with the address) anything it cannot translate exactly.
- `lift_fl.py` — the "materialized-fl" backend (adapted from the ui/ai lifter). Keeps a single EFLAGS word `fl`, updated
  only where a consumer reads it. Needed where flags come from `sahf`, or from `lahf` / complex multi-source paths that
  the captured backend cannot fold; also handles `leave`, `xchg`, `shld`/`shrd`, `cmov`, `fcom`/`fcomp` into the status
  word, `fnstsw` with TOP, string ops with the direction flag. The ui group is lifted with this backend.
- `abi.py` — per-function register inputs (the live-in of its calling convention) and `ret` cleanup bytes, for the
  `//   original …; register inputs: …; ret …` annotation.
- `outs.py` — whole-program register dataflow (must-define / entry-live / live-out fixpoints over the original code):
  the output registers a caller can read after each function returns, as `port.h` Outputs bits. Use it to set a port's
  declared outputs tightly where the generous default would be wrong (address-taken functions whose native path leaves
  different scratch EAX/EDX — see the `0` on `RavenVehicle_vf0_o90`).
- `gen.py` — the driver.

Both backends emit against `lift_rt.h` (namespace `lift`, macros `LIFT_ENTRY` / `LIFT_CALL` / `LIFT_CALLB` /
`LIFT_TAILJMP` / `LIFT_RET` / `LIFT_PUSH` / `LIFT_POP` / `LIFT_OUTS`).

## Supported

- Real control flow: the walker follows branches, conditional tail calls, and `jmp` tables — bounded (`cmp idx, N` / `ja`),
  two-level (`movzx idx, byte [idx+T]` then `jmp [idx*4+D]`), and unbounded (the compiler knew the range: entries up to the
  first that does not point into the function's neighbourhood). Ghidra's short function sizes are ignored.
- Every instruction form any of the old copies supported: the full SSE32/SSE64 set (add/sub/mul/div/min/max/sqrt, rcp/rsqrt
  tables, cmp predicates, shuffles, unpacks, conversions, `comiss`/`ucomiss`), x87 through `reflex_f80` (arith incl. the
  opcode-byte reg/st forms, fld/fst/fist, fcomi/fcom families, fnstcw/fldcw, fnstsw with TOP), MMX `movq`/`cvtpi2ps`/`emms`,
  integer arith with flags where read (`add/sub/cmp/and/or/xor/test/inc/dec/neg/shl/shr/sar/imul/mul/div/idiv/adc/sbb`
  incl. carry-chains, `cmpxchg`; captured backend also `shld`/`shrd r/m32, r32, imm8|cl` and 8/16-bit shifts, both only where
  no consumer reads their flags), string moves (`rep movsd/stosd/movsb/stosb`), `lea`, `movzx`/`movsx`, `cdq`, `lahf`.
- `fs:` operands → the TEB at `FS_BASE` (SEH prologues).
- Calls: direct and register/indirect (`LIFT_CALL`), the original return address written back into the dead slot; tail jumps
  (`jmp f`, `jmp reg`, `jmp [IAT]`, `jmp [reg+disp]` vtable slots → `LIFT_TAILJMP`); blocking worker-thread calls
  (`LIFT_CALLB`, the ENG_CALLB semantics: the wait stand-in ends the thread's run without popping the call); import calls
  through the IAT (via `LIFT_CALL` of the thunk). Runtime hooks inside a function (`HOOKS` in `lift.py`: an address where the
  world runtime may install a native, e.g. the render-sync worker phase) are looked up in `m.natives` and resumed at the pc
  they return.

- Flags of 32-bit shifts by an immediate count (CF = the last bit out, OF for count 1, as vm.cpp), `rep stosw` / `rep movsw`,
  single `movsb` / `movsw` (captured backend, ld_inflate wave).
- Callees that read the caller's flags at entry (`FLAGS_IN`, both backends: `__RTC_CheckEsp` 008fa3bd, called after
  `cmp esi, esp` in /RTCs-built code such as zlib): the call is a flags consumer and the EFLAGS word is materialized into `fl`
  (m.flags) before it. Without this the callee sees stale flags and reports an ESP failure (RaiseException).
- Callees that reuse their return-address slot (`NO_RA`: `__SEH_prolog4` 008fa554 stores the EH4 encoded scope table there): the
  call does not write the return address back afterwards.

- Stack-depth tracking in the walker (`stack_step`, `ret_cleanup`): a `jmp` to another functions.tsv entry is a tail call only
  when the stack can be back at the entry depth; while this function's frame pointer is set up, or the known esp offset is not 0,
  the target is a label inside the function (Ghidra splits SEH catch continuations off, e.g. 004f498c in 004f48e0) and is lifted
  inline. (captured backend; lift_fl.py still treats every such jmp as a tail call.)
- `imul r, r/m[, imm]` is emitted as an unsigned 32-bit multiply (the old `int32_t * int32_t` form is signed-overflow UB that clang
  -O3 exploits: notes/ld_assets.md). Rotates by `cl`; `wait` / `fwait`.

A function the lifter cannot translate exactly is refused with its address — it is never guessed. In `gen.py --from-ports`
mode a refused function keeps its committed port (never dropped).

## Lifting a list

Loading-path wave (≈5,700 functions, `native/game/plan/boot-alpine-functions.tsv`):

    # list.tsv: addr<TAB>name<TAB>group<TAB>file  (one line per function)
    python3 gen.py --list list.tsv --out ../../ports/<file>.cpp --group <group>

Each line becomes a port at its address; declared outputs default to `LIFT_OUTS` (compared only where live, via
`plan/live.tsv` → `live.inc`), tighten with `outs.py` where needed. Refused functions are printed and left out.

Re-lifting an existing group's ports in place (keeps names, group string, declared outputs, comments; detects and copies
through any hand-written port verbatim; rewrites the `*_util.h` include to `lift_rt.h`):

    python3 gen.py --from-ports bike_430.cpp bike_434.cpp …          # captured backend (default)
    python3 gen.py --from-ports ui_pages.cpp … --backend fl          # fl backend (ui / ai)
    python3 gen.py --check --from-ports …                            # report counts / failures, write nothing

## Verifying

    tools/build_cli.sh <out-dir>
    tools/verify.sh <reflex_boot> all        # 0 mismatches (bar the known dead-stack transients, notes/dead-stack.md),
                                             # and "on: identical" with the reference hashes on all five worlds

Groups currently regenerated by this lifter: `bike`, `engine`, `fx`, `anim` (captured backend) and `ui` (fl backend).
