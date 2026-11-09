# ld5_hi (wave 5, 0x800000..0x8fffff, plus the CRT DLL remainder)

Wave 5, 2026-10-18. Work list: the 45 rows of plan/wave5.tsv with group ld5_hi (functions still run as original code during the boot
or race frames of the 31 arcade events with every earlier port installed), and the msvcr90 / msvcp90 code still run in race frames.

Files:
- `ports/ld5_hi.cpp`: 45 functions lifted with tools/lift (`gen.py --list`, captured backend; only __get_sse2_info refused; list in
  the scratchpad `ld5_hi/list.tsv`). No lifter change was needed.
- `ports/ld5_hi_hand.cpp`: `__get_sse2_info` 008fa708, hand-written (the lifter refuses `pushfd`).
- `ports/ld5_hi_crt.cpp`: 25 CRT DLL functions (10 msvcr90, 15 msvcp90), lifted with the crt group's DLL lifter (a copy of scratchpad
  `crtw/tools` in scratchpad `ld5_hi/crt/`, driver `gen5.py`, list `funcs5.tsv`), group string ld5_hi, `CRT_OUTS` (every register).
- `_re/names/9e_ld5_hi.tsv`: names of the functions identified.

## What the rows are

- **Database_ReadDatabaseFile 0084b0a0** (was Fn_0084b0a0, the hottest row: ~6.8 M instructions per boot): the code after Database::vf5
  that Ghidra had no function for. ECX / EDX + 4 stack arguments (ret 16): builds `<dir><name>.dx9.database`, reads the XML (Handles,
  Packages / Package elements) and allocates the handle and package tables. Fn_0084bc70 (the Database request worker after it, critical
  sections, FUN_007fa250, the inflate entry points) and the small constructors Fn_0080fa70 / Fn_00810410 / Fn_0082b910 (`mov eax, ecx;
  mov byte [eax], 0`: 28 k calls per boot) / Fn_0082ccf0 / Fn_0082cd50 / Fn_00844100 / Fn_00806fe0 / Fn_00814710 / Fn_0084d980 /
  Fn_00859a60 are the rest of the loader-thread rows.
- Race-frame rows: FUN_0081da00 / 0081dc30 / 0081de70 / 0081e8e0 and FUN_0086b7f0 (NAT_Renne / RenneB: collision pair tests below the
  mechanism dispatch, 0086b7f0 calls 0081e8e0); FUN_0086e750 (TRX_Lynchburg / LynchburgBW), FUN_0086f2a0 (TRX_HalifaxBW);
  TextureAnimEntity::vf19 / vf22 / TextureAnimEntity_Step (FR_Cheyenne, FreeRide and WaypointRace); BehaviorRoamToWaypointGate::vf5
  (FR_Gibson/1); FUN_0080d320 (NAT_Concord, rounded to 0 instructions per frame).
- **gap rows** (executed code outside every Ghidra function): `gap_0083ffd0` is a whole function (entry called from 0082bd44: frees
  `[esi+0x3c]` with _aligned_free under a critical section and clears the object; ESI input) -> port Fn_0083ffd0. `gap_008fa77e` is
  `_RTC_SetErrorFuncW` (called from `_RTC_InitBase` 008fa385) -> port at its address. Both are lifted whole.
  The lead's corrected gap list (gap_functions.py fix, 2026-10-18): 0084c083, 0080f360, 0080f715, 0086f136, 00806eee are tails of
  Fn_0084bc70, FUN_0080f270, TextureAnimEntity::vf19, FUN_0086e750, FUN_00806d80 (Ghidra cut them short; the lifter follows the real
  control flow, so the ports contain them). 00814740 (`push edi; mov edi, ecx; call Keyboard_InitScanTable; mov eax, edi`, its address
  stored at 008145d5) and 00823a90 (a constructor: 6 fields, address at 00744199) are functions of their own -> ports Fn_00814740,
  Fn_00823a90.
- Static CRT of the exe, run once before WinMain: `__sse2_available_init` 008fa76a (initializer: `[0x00dcf760] = __get_sse2_info()`),
  `__get_sse2_info` 008fa708, `_RTC_InitBase` 008fa385, `_RTC_SetErrorFuncW` 008fa77e.

Left original on purpose: **Engine::Frame 0084f260** (the boot's frame-stop native sits at its address and does not fall through to a
port: notes/ld_engine.md, notes/loading.md). Nothing else in the range.

## __get_sse2_info and the interpreter's CPUID (surprise)

`__get_sse2_info` toggles EFLAGS.ID with pushfd / popfd, runs CPUID leaves 0 and 1 and, only if CPUID.1 EDX bit 26 (SSE2) is set, the
SEH-guarded movapd probe FUN_008fa6b8. The hand port reproduces the interpreter exactly: pushfd pushes `m.flags`, popfd keeps
`0x00244dd5 | 2` (vm.cpp M_popfd), set_logic / set_add rewrite only CF PF ZF SF OF, inc keeps CF, CPUID answers as vm.cpp M_cpuid.
**vm.cpp's CPUID leaf 1 puts its feature word 0x078bfbff in EBX and 1 in EDX** (r[3] is EBX in the interpreter's register order, the
comment says EDX), and ECX = 0x10800. So the game's SSE2 test sees no SSE2: boot coverage shows `je 008fa765` taken and FUN_008fa6b8
never runs, and `[0x00dcf760]` (the exe's `__sse2_available`) is 0 in every world. The port does the same (it must: the reference is
the interpreter). If the emulator's CPUID is ever corrected (EDX = 0x078bfbff, EBX = 0x00010800 brand/clflush, ECX = 1 SSE3), the
port's `ld5_cpuid` must change with it, and every world changes (the exe's SSE2 dispatch flag flips). Lead decision; not touched.

**Corrected 2026-10-18 evening (native switch agent):** vm.h `reflex_vm::cpuid` (the one CPUID of the interpreter; vm.cpp M_cpuid,
`ld5_cpuid` and crt_ld's lifted probes call it) answers leaf 1 as a Core 2 does: EAX 0x6fb, EBX 0x00010800, ECX 1 (SSE3), EDX 0x078bfbff.
Now `__get_sse2_info` runs the movapd probe FUN_008fa6b8 (no fault in the interpreter: it returns to 008fa75c, 008fa765 never runs),
`[0x00dcf760]` = 1 in every world, `_ftol2_sse` takes its SSE2 path (Concord baton boot: 508,587 calls, `_ftol2` 0) and msvcr90's own
probe (785a3563, crt_ld's port) finds SSE2 as well (its memcpy / memset / libm dispatch can take their SSE2 paths). Every world and reference hash changed with
it: loading.md section 9.

## CRT DLL code in race frames (scratchpad racecov, attributed with the .dll.tsv exports; `ld5_hi/dllattr.py`, `crt/ents.py`)

| what | instructions | events | port |
|---|---|---|---|
| `__libm_sse2_sin` 7859fb0c | 292 per frame | NAT_Renne, RenneB (the grass sway) | Libm_Sse2_Sin |
| `_initptd` 78543753 + its `__finally` funclets 78543828 / 78543831, `__addlocaleref` 7858d9b6, `__updatetmbcinfo` 7856232a (+ funclet 785623c5), `_decode_pointer`, `_calloc_crt`, `_calloc_impl` 78586460 | once per run (~1,000 in all) | FR_Alpine, FR_Gibson, NAT_Renne | InitPtd, ..., CallocImpl |
| `__SEH_prolog4` 7858cb80 / `__SEH_epilog4` 7858cbc5 (the `__dllonexit` attribution) | inside the above | | inlined (crt_util.h seh_prolog4 / seh_epilog4), never a port (notes/crt.md) |
| msvcp90 std::string members: `_Move_s`, `_Pdif`, erase (2), begin, end, find(string, pos), clear, replace (2), the checked-iterator / `_Traits_helper` internals 784850c1 78485235 7848592c 78485985 784859c8 | ~3 per frame | FR_Alpine, FR_Cheyenne, NAT_Renne, RenneB | String_* / Fn_784* |

The thread-start code runs when a thread first calls `_getptd` (the crt group's `_getptd_noexit` port calls `_calloc_crt` and
`_initptd` for a thread without per-thread data).

**__libm_sse2_sin is exact by construction**: its main path is pure SSE2 double arithmetic (`mulsd / addsd / subsd / mulpd / addpd /
unpck*pd / cvtsd2si / pextrw`, a 64-entry table at 785319e0); the lifter emits each instruction with the interpreter's own SSE semantics
(sse64 with the x86 NaN rules), so it computes the bits the interpreter computes; the huge / non-finite argument paths call `sin`
(x87, through call_function, unchanged) or multiply by a constant. Verified per call in NAT_Renne race frames (section below).

**The boot's CRT code is not measured**: boot coverage (boot_main.cpp, the boot path) sets only `Machine::pc_counts`, not `dll_counts`,
so `REFLEX_COVERAGE` in a boot writes no `.dll` file (bootcov3 has none; neither has this group's own boot sweep). The CRT code the boot
runs (thread start, file I/O, heap, string / printf, ...) is therefore not in any list. Lead: set `boot.machine().dll_counts` (and write
`<file>.dll` / `.dll.tsv` in CoverageOut, as the world test does) to get it; the crt group's `auto.py` then lists the functions.

## Acceptance (2026-10-18; scratchpad ld5_hi: accept.sh = tools/loading_accept.sh with at most 2 boots at a time, racever.sh = the
race-frame check of a filter on a boot's world with verify.sh's settings: Verify per port, then On ports vs none)

`loading_accept.sh <boot> ld5_hi <event>` with VERIFY=1, plus racever.sh ld5_hi on the original baton world:

| event (family) | Verify boot | canonical | race | race-frame Verify (ports, calls) | race On |
|---|---|---|---|---|---|
| Concord (National) | 25 ports, 0 mismatches, world identical | IDENTICAL, 0 pages differ | IDENTICAL, hashes identical | FUN_0080d320 2 | identical |
| NAT_Renne (National, grass) | 21 ports, 0 mismatches | IDENTICAL, 0 pages | IDENTICAL | FUN_0086b7f0 5,704 (0081da00..0081e8e0 inside), Libm_Sse2_Sin 6,000, InitPtd, DecodePointer, UpdateTmbcinfo, CallocCrt, String_* : 0 mismatches | identical |
| FR_Alpine (`alpine`, FreeRide) | 18 ports, 0 mismatches | IDENTICAL, 0 pages | IDENTICAL | InitPtd .. String_Replace (7 ports) 0 mismatches | identical |
| SX_Manchester (Supercross) | 23 ports, 0 mismatches | IDENTICAL, 0 pages | IDENTICAL | none called | identical |
| TRX_Lynchburg (Terracross) | 22 ports, 0 mismatches | IDENTICAL, 0 pages | IDENTICAL | FUN_0086e750 4 | identical |
| TRX_HalifaxBW (Terracross) | 22 ports, 0 mismatches | IDENTICAL, 0 pages | IDENTICAL | FUN_0086f2a0 274 | identical |
| FR_Gibson/1 (WaypointRace) | 22 ports, 0 mismatches | IDENTICAL, 0 pages | IDENTICAL | BehaviorRoamToWaypointGate::vf5 1 | identical |
| FR_Cheyenne (FreeRide) | 23 ports, 0 mismatches | IDENTICAL, 0 pages | IDENTICAL | TextureAnimEntity::vf19 1,500 / vf22 13, String_* | identical |

The one "unverifiable" port in every Verify boot is Fn_0084bc70 (2 calls that block on the loader's waits: run original, not a mismatch).

Filter `all` (every port in the tree, incl. the other wave-5 groups' files as of 15:00), no VERIFY: canonical IDENTICAL on all of the
above except Concord's one documented dead-data node (the 0x9c413-byte block, +5835c bfa0ae7d / 0022fc20, loading.md); races IDENTICAL
with identical hashes everywhere. Race frames with `all` on these baton worlds: Verify differences only inside Engine__Update (the whole
frame verified as one call: the particle-record 1 / 3 artifact of dead-stack.md, and single float words like [152414cc]); **On mode
differs in the memory hash on NAT_Renne (31 dwords), TRX_HalifaxBW (6) and FR_Gibson/1 (15)** with identical state hashes: every word is
`00000003` (none) vs `00000001` (ports) at +0x6c of 128-byte records (13d5c5ec, 13d5c66c, ...: the particle array of dead-stack.md's
Verde_ai11 note). Identical with `all,-ld5_hi`, so not this group; it shows that the particle difference is not only a Verify artifact on
these worlds (lead / fx group: the "On mode identical" argument in dead-stack.md holds on the five verify worlds only).

`verify.sh <boot> all`: on identical with the reference hashes on all five worlds (f1a02da0f3546f3b, 62b9d1986dc7b8ba,
9c6ce38c09cfa013, 9895014866da6382, 28623e7b63b760c9); mismatches only the documented transients inside Engine__Update ([148dbd9c]
006b0530 on Concord_ai11 / Alpine / SX_Springfield_ai11, 1 call each, the 121 particle records on Verde_ai11).

Coverage (own copies of bootcov3 / racecov run.sh, -P 2, REFLEX_PORT=all): boot of all 31 events: `gap_functions.py` lists 2 functions
in the whole exe: Engine::Frame 0084f260 (this range, deliberate) and SavedGameManager_ThreadProc 005a7f10 (ld5_lo's range); race 1,500 frames of all 31 events: 0 original instructions
(exe and both CRT DLLs: every .bin.dll is all zero).
