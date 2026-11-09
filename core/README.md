# OPEN REFLEX core (librxcore): MX vs ATV Reflex as readable native C, the Open Unleashed way

Like Open Unleashed's libmxacore (`~/Documents/Development/MX vs ATV RE/native`, its physics port `_re/poc/physics`), this
library IS the game for the Unity client: it reads the player's install itself, builds the track, vehicles, riders, AI and race
in its own memory, and runs the game frame. Unity only draws and reads state. **No original game code runs in the product, no
emulator is linked into it, nothing is faked: every system is the game's own logic, transliterated by hand from the decompile
into readable C11, and verified bit for bit against the original.**

Read first: this file, `~/Documents/Development/MX vs ATV RE/_re/poc/physics/README.md` (the model we follow), the subsystem notes
listed below.

## Layout

- `include/rx_api.h`  the public C API for Unity (flat, opaque handles, versioned; like mxa_api.h).
- `src/`              infrastructure: memory model (`rx_mem.h`), x86-exact float helpers (`rx_x86.h`), function registry
                      (`rx_fn.h`), arena heap (`rx_heap.h`), file readers (`src/fmt_*.c`: vfs / inflate / LZX / database / package / BXML /
                      scene / level / tables / localiz / texture / material / terrain / collis / model; API in
                      `notes/fmt.md`; test `tools/fmt_test.sh`, identical to the Python parsers).
- `game/`             the game code, one file group per subsystem, each function a hand transliteration of one original
                      function (`rx_<name>`, original address in its comment, registered with `RX_FUNCTION(addr, fn)`).
                      Each subsystem declares its functions in `game/<subsystem>.h`.
- `api/`              the API implementation (session, frame loop driver, getters).
- `oracle/`           C++ adapters that check each core function against the original inside the emulator (development
                      only; never in the product).
- `notes/`            the core's own notes (per subsystem: layouts, conventions, what is verified).
- `tools/`            tests and checks.

File prefixes per subsystem (game/<prefix>_*.c, game/<prefix>_*.h, oracle/<prefix>_*.cpp): `phys_` (mechanism, collision,
math), `world_`, `bike_`, `anim_`, `race_` (activity / race rules / AI), `loop_` (game loop, game object updates, players, input, cameras), `fx_`, `crt_` (CRT math), and `src/fmt_*`
for file readers. While others are mid-edit, build only yours: `RX_CORE_FILES="phys_*" build_cli.sh <your dir>`.

Build: `core/build.sh` -> `core/build/librxcore.dylib` (product). Oracle: `native/game/tools/build_cli.sh <out dir>` compiles the
core's `src/rx_x86.c src/rx_fn.c game/*.c` with `-DRX_ORACLE` plus `oracle/*.cpp` into the reflex_boot CLI.

## Runtime assets

The client importer extracts individual resources into a `game files` folder beside the app. `Assets/<type>/<database>/<package>/`
contains decoded game-native assets; the compact `Database` indexes and loose configuration/level files keep their original
paths. The `reflex-extracted-v1.txt` marker makes `fmt_db_read` use these files exclusively, with no package fallback. Original
packages remain supported as development/reference inputs. Neither mode needs the original EXE or CRT DLLs for core startup.
See `_re/notes/client-asset-import.md` for the shared path encoding, import validation and browsable index.

## Memory model (the Open Unleashed model)

Game objects keep MXReflex.exe's exact 32-bit layout and live at their original addresses in a 4 GB arena (`rx_addr` = the
original address). Code reads and writes fields by offset: `F32(veh, 0x2374)`, `SF32(veh, 0x2374, v)`, `U32`, `I32`, `rx_ld4` /
`rx_st4` for 16-byte vectors. NEVER use raw host pointers into game memory: the same source runs in the oracle build, where every
access goes to the emulated machine. Locals are ordinary C locals; only state the original keeps in memory goes to memory. Locals the original passes BY ADDRESS
to other functions (contact sets, boxes, vectors) live on the game stack below `rx_sp()`, like the original frame
(`RX_FRAME_ENTER(F, size)` ... `RX_FRAME_LEAVE(F, size)`, rx_mem.h). Use the ORIGINAL's frame size / alignment at those calls:
callees may keep the address of a stack object (JobPool jobs store the job record's stack address in heap memory), so the
addresses must match the original's ESP there.
Globals stay at their original addresses (e.g. the game object `U32(0, 0xa946b8)`). Initial data is compiled into
`data/initial_data.inc`: constants, string literals, vtable address identifiers, initial global values and explicitly bounded
CRT data objects. Product startup restores this payload without opening an EXE, DLL or redistributable. No original machine
code runs. See `data/README.md` for provenance, exact source hashes and the development-only extraction/check procedure.
Everything the game sets up at startup by CODE (static initializers, registries, tables, runtime constants) is ported
(construction units: globals / startup); test drivers may use a stand-in until that unit lands, marked as such. Constants the original loads from .rdata are
written as C literals with the address in a comment (`1.0f /* 0x931f24 */`).

Exactness rules (bit-exact against the original is the acceptance; most of this is automatic in C):
- Single-precision arithmetic in plain C `float`, in the original's operation order and association (the decompile reorders sums:
  check the disassembly, `_re/.venv/bin/python _re/tools/disasm.py <addr> [n]`). Build flags forbid contraction / fast-math.
- NaNs: plain C differs from x86 only when NaNs are produced or propagated (arm64 default NaN 0x7fc00000 vs x86 0xffc00000,
  operand preference, LLVM folding x * -1.0f into a negation): where a NaN can arise and be stored, use `rx_adds / rx_subs /
  rx_muls / rx_divs / rx_sqrts` (rx_x86.h).
- `rx_minss / rx_maxss` for min / max, `rx_cvtt / rx_cvt` for float -> int, `rx_rcp / rx_rsqrt` for RCPSS / RSQRTSS.
- Integer semantics of x86 (wraparound is on via -fwrapv; mask shift counts; signed vs unsigned compares as the original).
- x87 code: precision depends on the THREAD the function runs on. Measured: the main thread (game loop, Engine::UpdateTiming)
  runs with control word 0x027f = 53-bit precision; worker threads have their own recorded control words (runtime.cpp
  thread_fpu; the physics / animation jobs: check). So x87 code is NOT float math in general: port it with `src/rx_x87.h`
  (exact F80 arithmetic with precision bits 24 / 53 / 64 and rounding, x87 transcendentals) using the precision of its thread:
  check `m.fpu_cw` in your adapter while verifying (bits 8-9: 00 = 24, 10 = 53, 11 = 64) and write the precision into the
  function's comment. Plain float is only exact for pure SSE code.
- CRT math (sin / cos / atan2 / pow / sqrt via msvcr90): exact ports live in `game/crt_math.c` (one owner); call those.

## Calls

- To another ported function: call it directly through the OWNER's header and prototype (`#include "phys_mechanism.h"`,
  `rx_mechanism_integrate_velocities(M, dt)`). Never cast `RX_FN` to a signature you guessed for a direct call: agents order
  register arguments differently (LTCG conventions), so only the owner's prototype is authoritative (a weak link-free form:
  `__typeof__(owner_fn)*` as in loop_extern.h's LOOP_CORE).
- Virtual functions (address-taken, so MSVC keeps the standard convention): the core signature is always
  `(rx_addr this, <stack arguments in order>)` returning the EAX value (or float for an x87 / xmm return, documented in the
  owner's header); `RX_FN(slot_address, type)` with that signature is safe for virtual dispatch.
- Virtual calls: read the slot (`U32(U32(obj, 0), 4 * slot)`), then `RX_FN(addr, type)` gives the core function (or NULL).
- To a function not ported yet (someone else's, or not done): `rx_call_original(addr, conv, ecx, n, args)` or
  `rx_call_original_ex(addr, &in, &out, n, args)` for register conventions. In the oracle build this runs the original; in the
  product it is a fatal error naming the address. `grep -rn rx_call_original core/game` is the list of what is still missing.
  Replace them with direct calls as functions land.

## Verification (the oracle): every function, every call, bit for bit

`native/game/tools/verify.sh <reflex_boot> rx_<group>` runs the five reference race worlds (scratchpad `boot/worlds`) with the
group's adapters in Verify mode: at every call the core function runs on the emulated state, then the original on the same state;
memory writes and declared output registers are compared (dead stack excluded). Then On mode: core functions alone vs none, state
and whole-memory hashes. Acceptance per function: called in the worlds, 0 mismatches, On identical. Functions the five worlds do
not reach: build more worlds (native/game/notes/loading.md: baton boots with REFLEX_EVENT_SPEC from scratchpad `events.txt`).

Adapters (`oracle/<subsystem>.cpp`): `RX_PORT(addr, Name, "rx_<group>", outputs) { ... }` takes the original's arguments from
`m.r[]` / `m.arg(i)` / xmm / x87, calls the core function, writes the result registers the original's callers read
(`plan/live.tsv` liveness: Verify compares declared outputs only where live), returns `m.ret(cleanup)`. ABI glue only. RX_PORT restores EBX / EBP / ESI / EDI after the body (callee-saved); RX_PORT_RAW does not.

Construction (building the world from files) is verified differently: the core's objects after loading are compared field by
field against the same objects in the emulator's world image of the same event (tool: `core/tools/objcmp`, owned by the lead).

## Knowledge to use (do not rediscover)

- Decompile: `_re/out/decomp/*.c`; functions `_re/out/functions.tsv`; names `_re/names/*.tsv`; `_re/tools/fn.py <addr|name>`.
- Notes: `_re/notes/*.md` (158 files: formats, physics-timing, mechanism / constraint / integration kernels, terrain query / ray,
  terrain-deformation, bike-*, race-state, start-gate, game-camera, mud, rt-bump, props, vehicles, atv ...),
  `native/game/notes/*.md` (per subsystem: layouts, calling conventions, traps: mechanism.md, collision.md, world.md, bike.md,
  anim.md, ai.md, engine.md, fx.md ...), `_re/formats/*` (format specs and Python parsers).
- Existing readable code to reuse: `native/src/*.cpp` (bit-exact tested kernels: scheduler, velocity, pose, force, matrix family,
  inverses, constraint solve, preparation, mechanism core, bike force / powertrain / postpose / poststep drafts),
  hand-written ports in `native/game/ports/` (collision_grid / shapes / terrain / pairs, mechanism_*, world_phys*, world_node,
  world_quat, world_wreck*, math*.cpp: guest-memory C++ with the exact semantics, easy to transliterate),
  `native/drafts/world_runtime/deform.cpp` (terrain deformation, own data), Unity's C# readers
  (`Unity/Reflex/Assets/Scripts/Content/*.cs`) and the Python parsers for the file formats.
- The lifted ports (`native/game/ports/*` generated) are NOT a source: they are machine output. Read the decompile / disassembly.

## Ownership and process

Agents own subsystems (game/<subsystem>*.c, game/<subsystem>.h, oracle/<subsystem>.cpp, notes/<subsystem>.md). Shared files
(src/*, include/rx_api.h, api/*, oracle/oracle.*, build scripts, this README) belong to the lead: ask in your report. Work list per
subsystem: `core/plan/*.tsv`. One function done = transliterated, registered, adapter written, verified with 0 mismatches, and `core/build.sh` (the
product library, every game file) still builds without errors: no debugging code in game files (diagnostics go in the oracle
adapters or under `#ifdef RX_ORACLE`).
