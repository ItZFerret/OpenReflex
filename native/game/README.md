# native/game: the native Reflex (the Open Unleashed way)

The goal: MXReflex.exe's game code rewritten as native C++, so the Unity client runs no original code at all (as Open Unleashed's
`libmxacore` does for MX vs ATV Unleashed). Content keeps coming from the player's install.

## How it is converted: one original function at a time, against the original

`libreflexworld` (native/drafts/world_runtime) runs the unchanged exe. A port registers a native at an original function's address
(`REFLEX_PORT_FN`, port.h). Installed, the native replaces the original there, so the game keeps working while it is converted:

- `REFLEX_PORT=<filter>`: the selected ports run instead of the originals (`all`, groups, names or addresses; `-x` excludes).
- `REFLEX_PORT_VERIFY=1`: every call runs both on the same state (page journal): the native, then the original, which is kept.
  Memory writes and output registers are compared and counted per port (`reflex_port::report`).

A port is accepted when `tools/verify.sh <reflex_boot> <group>` passes: 0 mismatches on five worlds driven by a deterministic
random rider (Concord 1 and 12 riders, Alpine free ride, Springfield Supercross, Verde Terracross), and the natives alone give
the same bike state and whole-memory hashes as the original.

When nothing per frame runs original code, the exe's frame code is gone; loading (the boot that makes worlds) is converted the same way;
then the state can move out of the original memory layout (`guest.h` is the only access path).

## Files

- `port.h / port.cpp`: registry, install, Verify.
- `guest.h`: access to the game's state and calls into the game (`Guest::call`).
- `x86.h`: floating point exactly as the exe's SSE code (NaN rules of x86 on any host).
- `ports/<group>.cpp`: the ports, one file (or more) per group.
- `plan/*.tsv`: work lists from coverage (functions executed, cost per frame).
- `tools/build_cli.sh <dir>`: builds the reflex_boot CLI with the ports. `tools/verify.sh`: acceptance. `tools/coverage.py`,
  `coverage_union.py`: what still runs as original code (reflex_boot with `REFLEX_COVERAGE=<file>`, optional
  `REFLEX_COVERAGE_ROOT=<hex,...>` for one call tree, `REFLEX_COVERAGE_FROM=<frame>`).

## Writing a port

- Transliterate the original function: same operations in the same order, floats through `x86::` (add/sub/mul/div/sqrt with the
  x86 NaN rule), integer conversions as cvt / cvtt, memory through `Guest`. The result must be bit-identical.
- The ABI is the original's: stack arguments via `m.arg(i)` (at entry), ECX = `m.r[1]`, results in `m.r[0]` / xmm / x87,
  return `m.ret(callee_cleanup_bytes, eax)`. Custom conventions (LTCG) are common: read the callers.
- Declare the outputs callers read (`OUT_EAX`, `OUT_XMM0 << i`, `OUT_FPU`, ...). Verify always compares memory, ESP and EBX/EBP/ESI/EDI.
- Unported callees: `Guest::call(address, {args}, ecx)`.
- x87 code: `reflex_f80` (world_runtime/f80) is the interpreter's exact 80-bit arithmetic; honour the control word.
