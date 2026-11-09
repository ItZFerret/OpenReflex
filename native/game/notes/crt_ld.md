# crt_ld: the CRT DLL code of the boot (wave 6)

Wave 6, 2026-10-18. Before this, with `REFLEX_PORT=all` the boot ran no original exe code except Engine::Frame. It still ran about
133 M original CRT DLL instructions per boot (bootcov4, all 31 arcade events: 4.13 G in all). That code is msvcr90.dll (0x78520000)
and msvcp90.dll (0x78480000). It covers number parsing (atof / strtod), character classes, strstr / strncat, buffered file reads, the
heap (realloc), dynamic_cast, swprintf / sscanf, libm cos / log / exp / acos, the DLLs' own startup, and msvcp90's iostream / locale
initialisation.

With these ports, **no CRT DLL instruction runs during the boot on any of the 31 events**: every `.bin.dll` of the sweep is all zero.

## Files

- `ports/crt_ld_p90.cpp`: 154 msvcp90 functions.
- `ports/crt_ld_r90a.cpp`: 94 msvcr90 functions below 0x7856c000.
- `ports/crt_ld_r90b.cpp`: 144 msvcr90 functions from 0x7856c000.
- Group string `crt_ld`. Every port declares `CRT_OUTS`.

Together these are **392 functions** and 19,307 lifted instructions. Each sits at its own address: 163 are exports and 229 are
internal functions. None of them was already ported by crt or ld5_hi (one native per address).

They are generated, not hand-written. The generator is in the scratchpad folder `crt_ld/`:

- `xlift.py`: a copy of the crt group's lifter (scratchpad `crtw/tools`) with the additions listed below.
- `known.py`: the other groups' CRT ports, their options, and their lifted bodies.
- `discover.py <cov.pkl> extra.txt`: finds the entries.
- `ptrents.py`: finds the indirect-call entries.
- `mklist.py` and `names.py`: write `funcs_ld.tsv`.
- `gen_ld.py <cov.pkl>`: writes `ports/crt_ld_*.cpp` and reports instructions, resume points, and any resume point the coverage
  shows executed (HOT).
- `attr.py`: boot instructions per port.
- `covu.py`: unions the `.bin.dll` coverage files.

To regenerate, run `python gen_ld.py bootcov4.pkl` with the `_re/.venv` python. The output is "total 19307 insns, resumes 1 HOT 0".

## How the entries were found (DLL coverage only gives pcs)

The export below a pc is not the function containing it. Most hot code is internal (`$I10_OUTPUT+929`, `strerror_s+3238`, ...), and
msvcr90 is hot/cold split, so a backwards scan to padding gives chunk starts, not entries. A port must sit at a real entry. The
interpreter enters a native whenever *any* control transfer reaches its pc (Machine::run checks natives per pc). A port at an address
that is only reached by a jmp would therefore run with a wrong [ESP].

`discover.py` builds the entry set from these sources:

1. Executed exports.
2. Direct `call` targets that coverage shows executed. The call sites are taken from executed pcs, from the lifted bodies of the
   crt / ld5_hi ports (they run natively, so their pcs are not in the coverage), and from the new bodies. This is iterated to a
   fixpoint.
3. Indirect-call targets. These are executed pcs that a data word of the image points to:
   - msvcr90's `__xi` / `__xc` initializer tables at 78521294.. and msvcp90's at 7848120c.. (run by `_initterm`);
   - the demangler's vtable at 78522660..;
   - function pointers pushed as arguments (`_woutput_l` 78575ec1, `_winput_l` 78577eaa) or stored in globals (7854367e).

   Jump-table words (78575fe2 / 78576111 / 78576200 / 78576221 / 785781fa) were excluded: they are targets inside the lifted
   functions.
4. The two DLL entry points, `_DllMainCRTStartup` 78542d40 (msvcr90) and 784b9dd7 (msvcp90). boot.cpp calls them through
   `call_function` after the ports are installed.

The lifter then follows each entry's control flow, so tails, chunks, and code jumped into (strstr's jump into strchr+6) end up inside
the entries' bodies. The last check is that every executed pc lies in some lifted body (new or existing).

## Lifter additions (scratchpad crt_ld/xlift.py)

- **pushfd / popfd / cpuid** are lifted as vm.cpp executes them:
  - pushfd pushes the flags word;
  - popfd keeps `0x00244dd5 | 2`;
  - cpuid calls `reflex_vm::cpuid` (vm.h, the interpreter's own helper; lead request, so a future CPUID correction carries over).

  They occur in the DLLs' `__get_sse2_info` copies (msvcr90 785a3563, msvcp90 784ba3aa, run once by an initializer), the same code as
  the exe's (notes/ld5_hi.md).
- **Frame helpers are lifted inline at their call sites** (`INLINE_LIFT` / `InlineLifter`). The call pushes the real return address,
  the helper's instructions follow with labels `L_<site>_<pc>`, and its `ret` pops the slot (+ imm) and continues after the call.
  The helpers are:
  - msvcp90 `__EH_prolog3` 784b952f;
  - msvcp90's own `__SEH_prolog4` 784b9ff8 / `__SEH_epilog4` 784ba03d;
  - msvcr90 `_alloca_probe_16` 78545f40, `_alloca_probe_8` 78545f56, `_chkstk` 78545f80.

  The hand-written inlines of crt_util.h (`seh_prolog4` 7858cb80, `seh_epilog4`, `eh_prolog3` 784b9567, `eh_epilog3` 784b961b)
  stay as they were.

## Traps

- **`call_at` and prologs (the lead's real-return-address change, 16:05).** CRT_CALL is now `m.call_at(target, ra)`. That stops when
  the callee returns to `ra` *with ESP above the slot*. A prolog called through it never stops: it returns with ESP below the slot,
  so the run continues into the caller. The first generated version called `__EH_prolog3` 784b952f through CRT_CALL and stopped the
  boot after 118 k instructions ("unmapped write fffffff0"). `call_at`'s esp_below list has only the exe's helpers. The DLL helpers
  above therefore have to be inlined (they could not be ports anyway: notes/crt.md).
- **Ports at entries reached by a jmp** would break: see "How the entries were found". `discover.py` takes only call targets,
  pointer-referenced code and DLL entries.
- **Do not name a scratch script `bisect.py`** next to the generator. capstone imports `random`, which imports `bisect`, and that
  picks up the script.
- 7858eba9 is a `jmp [IAT]` thunk (GetCommandLineW) that coverage showed executed, so it is a port (CRT_TAIL to the import).
- The one resume point (784b9f0e in msvcp90's `__report_gsfailure`, `mov word [..], ss`: segment registers) is the security
  cookie failure path and never runs.

## Acceptance (scratchpad crt_ld: `accept.sh` = tools/loading_accept.sh with at most 2 boots at a time and VFILTER for the Verify boot; `chain.sh` runs everything)

**Verify inside the boot.** One verified call mismatches, and it is an emulator undo gap, not a port difference.

- With filter `crt_ld`, VERIFY=1 on Concord gives 42 ports called top-level and 0 mismatches except `_DllMainCRTStartup` 78542d40
  (1 call). Its "first" difference: native `[10001158] 10001e30`, original `10001dd0`, with the command line string at 10001dd0.
  The Verify boot's world then differs from the original's.
- **Cause: boot.cpp's GetCommandLineA / GetCommandLineW stand-ins cache their heap block in a function-local `static uint32_t a`.**
  The host-state undo does not restore that static. The native run allocates the block and the undo frees it, but the original run
  gets the stale address without allocating, so its heap layout shifts by that block.
- It affects any verified call that makes the boot's first GetCommandLine call:
  - 78542d40;
  - `_CRT_INIT` 78542b94 under it;
  - the GetCommandLineW thunk 7858eba9 when those two are excluded.
- The ports themselves are exact. In On mode the canonical dump is IDENTICAL and 0 of 43,399 world pages differ.
- The Verify boots of the acceptance therefore exclude those three (`VFILTER=<filter>,-78542d40,-78542b94,-7858eba9`). Results are
  in the table below.
- Infra / lead: reset those statics in host_restore, or allocate the command lines once outside verified calls.

| event (family) | filter | Verify boot (top-level ports called, mismatches) | Verify world | canonical | image pages differ | race |
|---|---|---|---|---|---|---|
| Concord (National) | crt_ld | 51, 0 | identical | IDENTICAL | 0 / 43,399 | IDENTICAL, hashes identical |
| FR_Alpine (`alpine`, FreeRide) | crt_ld | 53, 0 | identical | IDENTICAL | 0 | IDENTICAL, hashes identical |
| SX_Springfield (Supercross) | crt_ld | 51, 0 | identical | IDENTICAL | 0 | IDENTICAL, hashes identical |
| TRX_Lynchburg (Terracross) | crt_ld | (no Verify) | | IDENTICAL | 0 | IDENTICAL, hashes identical |
| FR_Gibson/1 (WaypointRace) | crt_ld | (no Verify) | | IDENTICAL | 0 | IDENTICAL, hashes identical |
| Concord | all | 776, 2: RavenVehicleMotocross__vf31 (ld_core, the dead-data words) and Fn_005dbe50 (ld_ui spin site, "the original would switch") | identical | DIFFERENT: only the known dead-data node (0x9c413 block, +5835c bfa0ae7d / 0022fc20) | 12 (dead stack + that buffer) | IDENTICAL, hashes identical |
| FR_Alpine | all | 751, the same 2 | identical | IDENTICAL | 10 (dead stack) | IDENTICAL, hashes identical |
| SX_Springfield | all | 747, the same 2 | identical | IDENTICAL | 10 (dead stack) | IDENTICAL, hashes identical |
| TRX_Lynchburg | all | (no Verify) | | IDENTICAL | 9 (dead stack) | IDENTICAL, hashes identical |
| FR_Gibson/1 | all | (no Verify) | | IDENTICAL | 9 (dead stack) | IDENTICAL, hashes identical |

`tools/verify.sh <boot> all`: on identical with the reference hashes on all five worlds (f1a02da0f3546f3b, 62b9d1986dc7b8ba,
9c6ce38c09cfa013, 9895014866da6382, 28623e7b63b760c9). Mismatches: only the documented Engine__Update transients ([148dbd9c] on
Concord_ai11 / Alpine / SX_Springfield_ai11, one call each; Verde_ai11 16, [148dbd9c] and [1909732c]). Race frames are unaffected: no
crt_ld port is called in race frames (the race CRT coverage was already zero). crt_util.h is unchanged (no re-run of `verify.sh crt`
needed).

**Coverage** (own copy of bootcov4/run.sh in `crt_ld/cov/run.sh`, -P 2, REFLEX_PORT=all, all 31 events):

- all 31 boots start the event;
- every `.bin.dll` is all zero (0 pcs, 0 instructions in both DLLs);
- `gap_functions.py` on the 31 `.bin` lists only Engine::Frame 0084f260 (deliberate: notes/ld_engine.md).
  SavedGameManager_ThreadProc was ported by the lead in the meantime (loading.md section 8).

## What the hot internals are (VC90 CRT; purpose from their code and callers, names not certain: ports are Fn_<addr>)

| port | what | boot instructions (31 boots) |
|---|---|---|
| Fn_7859a40f | multiply-accumulate of the 12-byte mantissa (probably `__mtold12`), called per digit by Fn_78597ebd | 1.38 G |
| Fn_78597ebd | the decimal string parser state machine (probably `__strgtold12_l`) | 1.09 G |
| Fn_7859735f | 12-byte long double to double with rounding (probably `_ld12tod`) | 583 M |
| Fn_7859861a | atof's conversion driver (`_fltin2`-like: calls the two above) | 194 M |
| Crt_isalnum / isspace / isdigit / isalpha / atof_l / strstr / strncat / wcspbrk | the XML / text parsers of the loader | 600 M |
| Crt_fread_nolock_s, Crt_filbuf, Crt_read, Fn_78589399 / Fn_7858943e (`_get_osfhandle` internals), lseek, lock_file | buffered file reads (the ReadFile / SetFilePointer stand-ins are called in the same order) | 120 M |
| Fn_78586890 | realloc's heap body (`_realloc_impl`-like, HeapReAlloc / HeapAlloc stand-ins) | 23 M |
| Crt_libm_sse2_cos / log / exp / acos | SSE2 libm kernels (exact by construction, as __libm_sse2_sin in ld5_hi) | 22 M |
| Fn_78575ec1 / Fn_78577eaa | `_woutput_l` / `_winput_l` (swprintf / swscanf through their pointers) | 0.3 M |
| 784be... (55 small Fn_) | msvcp90's dynamic initializers (`locale::id`s, the std streams), via `_initterm` | 10 k |
