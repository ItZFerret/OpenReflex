# ld5_init (0x00900000..): the exe's CRT startup and static initializers (wave 5)

999 rows of plan/wave5.tsv (group ld5_init, all `boot:all`, ~53 KB of code, ~1,500 instructions per boot in total). All 999 are
ported, lifted with the shared lifter (`tools/lift/gen.py --list`, lists in the scratchpad's `ld5_init/list_*.tsv`):

| file | contents | backend | ports |
|---|---|---|---|
| ports/ld5_init_crt.cpp | CRT startup (entry, ___tmainCRTStartup, pre_c_init, pre_cpp_init, _RTC_Initialize, ...) | captured | 9 |
| ports/ld5_init_fl.cpp | ___security_init_cookie (`leave`: the captured backend has no `leave`) | fl | 1 |
| ports/ld5_init_903 / 906 / 909 / 90c / 90f.cpp | the static initializers (dynamic initializers of globals), by address | captured | 170 / 161 / 200 / 252 / 206 |

Group string `ld5_init`. Names: `_re/names/9e_ld5_init.tsv` (the CRT functions; the initializers keep `Fn_xxxxxxxx`).

## How they are reached

- `entry` (009006a7, the PE entry point; boot.cpp `Boot::start` runs it on the main thread after the DLLs' DllMain) calls
  ___security_init_cookie and jumps to ___tmainCRTStartup (009003e7).
- ___tmainCRTStartup: `__SEH_prolog4`, GetStartupInfoA, the `__native_startup_lock` InterlockedCompareExchange loop, then
  `_initterm_e(__xi_a 00917664, __xi_z 00917674)` (msvcr90; entries pre_c_init 009005c6, 008fa76a (ld5_hi's range),
  __CxxSetUnhandledExceptionFilter 009006f3) and `_initterm(__xc_a 009166e4, __xc_z 00917660)`: **991 slots, 990 non-null, every one
  is a row of this group** (pre_cpp_init 0090039c first, then 0x009033f0..0x00911d20). Then the command line scan (_acmdln,
  _ismbblead) and ReflexWinMain (008814a0, ld_engine's port), which never returns in a boot.
- msvcr90's `_initterm` / `_initterm_e` (interpreted DLL code) call each slot with `call [esi]`: the machine dispatches natives on the
  pc, so a port at a slot's address runs like any call.
- **Install order: ports are installed before the initializers run.** boot_main.cpp calls `reflex_port::install_from_env` right
  after constructing the Boot and before `boot.start()`; `start()` maps the images and runs DllMain and the entry. All 999 ports are
  called in a `REFLEX_PORT=ld5_init` boot (port report: 999 called, each once).
- atexit / _onexit: 573 initializers register a destructor with `_atexit` (008f9fbf, `__dllonexit` into the CRT); pre_cpp_init
  registers _RTC_Terminate. Those destructors only run at process exit, which no boot reaches: none is in the work list, none ported.
- Never in a race: the world runtime starts from a world image (main thread at Engine::Frame), so none of these runs in race frames.

## What the initializers are

Tiny constructors of globals: 573 call `_atexit`; common callees FUN_00811230 (216), msvcp90 std::string ctor/dtor imports
(0x9161ac / 0x9161b4, ~115 each), FUN_00417230 (92: EAX = name, ESI = object, one stack argument; vtable 00937740, strncpy of a 31-character name, the object added to the
list at 009e30e4), FUN_00593950 /
FUN_005bacd0 (61), FUN_00402b40 / 00402be0 (59), FUN_005ba330 / 005ba360 (45); 1,665 SSE instructions (constant vectors copied into
globals), 72 x87 instructions (lifted through reflex_f80). The lifter refused nothing except `leave` (security cookie, fl backend).

## CRT startup details

- gap rows: `gap_0090039c` = pre_cpp_init (an __xc_a entry Ghidra had no function for); `gap_00900708` = _RTC_Initialize (empty
  table 0099a17c..0099a180); `gap_009008ba` = _setdefaultprecision (`_controlfp_s(0, _PC_53, _MCW_PC)`); `gap_00900984` = the
  import thunk `jmp [0x9163d0]` (_controlfp_s) that Ghidra had no function for (ports/thunks.inc does not list it; ported here
  as `MSVCR90_controlfp_s`, a lifted LIFT_TAILJMP to the IAT target). The plan's 221 instructions per boot for that gap are an
  attribution artifact: the port is called once per boot.
- `_matherr` 009008e5 is `xor eax, eax; ret`, folded with `_setargv` (pre_c_init calls it, then passes it to __setusermatherr).
- ___tmainCRTStartup's SEH filter / handler blocks (00900561 _XcptFilter, 00900575) are reached only by exception dispatch; the
  lifter follows control flow from the entry, so they stay original code (no exception occurs in a boot).

## Traps

- **Main-stack return-address slots.** With `entry` / ___tmainCRTStartup ported, the main thread runs its whole life inside nested
  natives (`entry` -> LIFT_TAILJMP -> tmainCRTStartup -> LIFT_CALL ReflexWinMain). LIFT_CALL / LIFT_TAILJMP keep 0 in the return
  slot while the callee runs (notes/loading.md, "return addresses on blocked threads' stacks"): the world image differs from the
  original's in exactly 2 words of the main stack page 0x0022f000: 0x0022ff5c (ReflexWinMain's return, original 00900527) and
  0x0022ffec (entry's return, the boot's THREAD_EXIT sentinel efff0000). Dead: nothing returns there; canonical dumps and races are
  identical. Note ReflexWinMain's own port (ld_engine) already puts the main thread inside nested natives with `REFLEX_PORT=all`.
- In Verify boots `entry` and ___tmainCRTStartup are unverifiable (the native run reaches the first thread switch: WouldSwitch, the
  original runs instead). pre_c_init's callees (_RTC_Initialize, _setdefaultprecision, _matherr, the _controlfp_s thunk) are only
  called from inside pre_c_init, so a Verify boot never verifies them at top level unless pre_c_init is excluded
  (`REFLEX_PORT=ld5_init,-pre_c_init,-entry,-___tmainCRTStartup`).
- Under the exact-slice scheduler (not the conversion's reference) a slice end inside a nested call breaks a native; the main thread
  living inside natives makes that certain for `entry` (as for ReflexWinMain): use baton boots for ports, as loading.md says.

## Acceptance (2026-10-18, build from the scratchpad's ld5_init/out)

- `loading_accept.sh <boot> ld5_init 1` / `alpine`: 999 ports called; VERDICT IDENTICAL (160,456 / 140,306 nodes); images: 1 page /
  2 words (the main-stack slots above); race 52/52 samples identical, race hashes identical.
- Verify inside the boot (own sequential copy of VERIFY=1, at most 2 processes): Concord and Alpine 995 ports called, 0 mismatches,
  2 unverifiable (entry, ___tmainCRTStartup); the Verify boot's world byte-identical to the original's. With
  `ld5_init,-pre_c_init,-entry,-___tmainCRTStartup` _RTC_Initialize, _setdefaultprecision, _matherr and ___security_init_cookie
  verify (1 call each, 0 mismatches), and `MSVCR90_controlfp_s` alone verifies too (1 call, 0 mismatches); worlds identical.
- `loading_accept.sh <boot> all 1`: 1 differing node, the known dead-data buffer of loading.md (`+5835c` bfa0ae7d / 0022fc20,
  `+5836c` 00000009 / 428de9ba); race IDENTICAL, hashes identical. `... all alpine`: IDENTICAL, race identical.
- No other event families needed (every row is `boot:all`; the coverage sweep boots all 31 events with every ld5_init port called).
- `verify.sh <boot> all`: 0 mismatches except the documented transients (notes/dead-stack.md: Engine__Update [148dbd9c] on
  Concord_ai11 / Alpine / Springfield, the particle array on TRX_Verde_ai11); on: identical on all five worlds with the reference
  hashes (f1a02da0f3546f3b, 62b9d1986dc7b8ba, 9c6ce38c09cfa013, 9895014866da6382, 28623e7b63b760c9).
- Coverage (copies of bootcov3 / racecov run.sh, `-P 2`, REFLEX_PORT=all, 31 events): all 31 boot and race (race 2 at frame 1499);
  999 ld5_init ports called in every boot; tools/gap_functions.py: nothing at >= 0x00900000 runs original code in the boots (the
  whole exe leaves Engine::Frame, SavedGameManager_ThreadProc and 4 gaps of other groups), and nothing at all in the race frames.
