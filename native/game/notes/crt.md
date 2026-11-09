# crt — the CRT DLLs (msvcr90.dll at 0x78520000, msvcp90.dll at 0x78480000)

The game links the VC90 runtime dynamically: msvcr90.dll 9.00.30729.1 at its preferred base 0x78520000 and msvcp90.dll
9.00.30729.1 at 0x78480000 (both relocated to those bases by runtime.cpp's `load_library`; extracted copies in the scratchpad's
`crt/`). Every race frame ran 55k–480k of their instructions (Concord 55,842 / Concord_ai11 128,201 / Alpine 327,949 /
SX_Springfield_ai11 479,798 / TRX_Verde_ai11 379,674 per frame with every other group ported; msvcp90 ~300 of that). With the
crt ports **no CRT DLL instruction runs per frame** on the five worlds (coverage with `REFLEX_PORT=all`, from frame 100:
`coverage.py` reports "+ 0 in the CRT DLLs" and the DLL counts file is all zero).

## How: a DLL lifter, every executed function transliterated

133 functions (106 msvcr90 + 27 msvcp90), ~10,700 instructions, are ports at their own addresses (exports and internal
functions alike: a port at an internal address is entered whenever a call reaches that pc). They are generated, not
hand-written:

- Generator: scratchpad `crtw/tools/` — `xlift.py` (the lifter), `gen.py` (lifts `funcs.tsv` into `ports/crt_<file>.cpp`),
  `auto.py` (appends the functions a coverage run still shows executing), `remaining.py` / `cov.sh` (coverage per function on
  the five worlds), `p/dllp.py` (the msvcp90 image). `xlift.py` reads the image through the earlier survey's
  `crtag/tools/dll.py`. Regenerate: `python3 gen.py [<cov dir>]` (prints per function the instruction count, resume points and
  any resume point the coverage shows executed).
- Runtime header: `ports/crt_util.h` (memory, SSE / SSE2 integer ops, flags, x87 helpers, the call / tail / ret / resume
  macros, the inlined SEH / EH frame helpers).

Semantics are the reference interpreter's, instruction by instruction (vm.cpp `execute` / `execute_fp`): registers in locals,
the flags word `fl` computed eagerly exactly as `set_add` / `set_logic` and each instruction case (inc/dec keep CF, rol/ror
touch CF/OF only, shift by 0 changes nothing, shld/shrd = set_logic + CF, imul/mul CF=OF, bt* CF, bsf/bsr ZF only, div leaves
flags, comisd clears AF, fcomi CF/PF/ZF), x87 on the Machine's stack through reflex_f80 with the control word (fxam, fprem,
fnstsw with TOP, fscale, f2xm1 / fyl2x / fpatan through `transcendental`, fsin/fcos clear C2), string ops with DF and the
interpreter's page-wise fast path, `fs:` = the TEB at 0x7ffd0000. Because every register comes out exactly as the original's,
all ports declare `CRT_OUTS` (EAX ECX EDX, all xmm, x87) and Verify compares them all.

- Calls go through `m.call_function` (ported or original callee, same stack); the original return address is written back
  into the dead slot. Flags are synced across calls both ways (785a3db5 returns ZF to `_CIpow`'s `je`).
- Indirect jumps dispatch on their runtime target: known targets (jump tables, `ijmp_tables` in funcs.tsv) are gotos, anything
  else resumes the original there (`crt::resume_original`: the interpreter runs from that pc with 0 in the function's own return
  slot until its ret, then the slot is restored — works in On and Verify).
- Instructions the interpreter or the lifter does not support resume the original at their pc. None of them is executed:
  `rcr` in `_aulldvrm`'s 64-bit-divisor path (the interpreter has no rcr: never reached), `fnsave` in pow's error path, the
  int3 after `_invalid_parameter` calls, and disassembly of table data. `gen.py <cov dir>` flags any executed one ("HOT").
- Calls to an import thunk (`jmp [iat]`, msvcp90 → msvcr90) call the IAT target directly.

## Traps (for anyone lifting DLL or SEH code)

- **Entry below other code**: a function whose walk reaches code at lower addresses (memset's SSE path at 78545e73) must start
  with `goto L_entry` — the first lifted line is the lowest address, not the entry.
- **`call_function` pushes 0 as the return address.** Code that reads its own return address sees 0: `__SEH_epilog4` (7858cbc5)
  and msvcp90's `__EH_epilog3` (784b961b) `pop ecx` it and return via `push ecx; ret`, so ECX after the call would be 0 instead
  of the return address (seen as `ecx 00000000 orig 785838da` in `free` / `operator delete`).
- **SEH / EH prologs cannot be ports**: `__SEH_prolog4` (7858cb80) and `__EH_prolog3` (784b9567) return with ESP far below
  their entry, so Verify's original run (which stops at the real return address only once ESP is above the entry ESP) never
  stops and runs on into the caller (world stopped at 7857e09c, `__RTDynamicCast`). Both pairs are therefore inlined at their
  call sites in lifted code (`seh_prolog4` / `seh_epilog4` / `eh_prolog3` / `eh_epilog3` in crt_util.h, with the real return
  address, same stack / TEB / flags); they stay original where unported code calls them.
- **Jump tables**: memmove / memcpy index tables negatively (backward copies) and some tables start with a misaligned dword;
  bounds come from a preceding `and idx, imm` / `cmp idx, imm`, else every code pointer within ±64..128 entries of the table
  near the function is a case (extra cases are dead; the runtime target picks the label).
- **libm dispatcher**: `_CIatan2` / `_CIfmod` load a descriptor in EDX and jump into the shared `__cintrindisp2` code (785a4840);
  785a3ba7 classifies both operands with fxam + xlat and `jmp [ebx]` through the descriptor's handler table at EDX+0x10
  (785bb0b0 atan2, 785ba8c0 fmod). Those targets (and `_CIfmod`'s fprem loop at 7859ac94) are inlined into 785a3ba7's port.
- With `REFLEX_PORT=crt`, only ports called from original code are verified individually (38–44 per world: the exports the exe
  calls); nested ones (`_output_l`, `write_char`, `$I10_OUTPUT`, the dispatcher pieces ...) run natively inside them and are
  checked as part of their caller. Some (`__RTDynamicCast` & co, the demangler, strtol) are first called after frame 1200, so
  `verify.sh <boot> crt quick` does not reach them; the full run does (40–46 ports called per world).
- With `REFLEX_PORT=all` the frame's top function is a port (Engine__Update, engine_mt), so `verify.sh all` verifies only that
  one (whole frame) and the crt ports inside it run natively; `verify.sh crt` is the per-function check.

## The ports

- `crt_printf.cpp`: `sprintf`, `_vsnprintf`, `_vsnprintf_l`, `_output_l` (78570f34) and its `write_char` / `write_multi_char` /
  `write_string`, `_LocaleUpdate::_LocaleUpdate` (7855c094), `_isleadbyte_l`, the float path `_cfltcvt_l` → `_cftof_l` →
  `_cftof2_l`, `_fltout2`, `__dtold`, `shift`, `_fptostr`, `$I10_OUTPUT` (MSVC's own long-double digit generation: the output
  text is bit-identical by construction).
- `crt_math.cpp`: `_copysign`, `_finite`, `floor`, `ceil` (+ `_ctrlfp` 785a6900, `_frnd` 785a9662), `_CIatan2`, `_CIfmod`,
  `_CIpow` (+ its internals 7859b01d, 785a3db5, 785a3d70, the dispatcher pieces 785a3ba7 / 785a490a / 785a47fc),
  `__libm_sse2_pow` (611 instructions of SSE2 table code).
- `crt_string.cpp`: memset / memcpy / memmove (with their SSE2 paths) / memcmp / memchr / memcpy_s / memmove_s / strcpy_s /
  strlen / strncpy / wcsncpy / strchr / strcmp / strrchr / strpbrk / strncmp / wcsncmp / tolower / _stricmp / _strnicmp.
- `crt_runtime.cpp`: `_getptd`, `_getptd_noexit` (7854383f), `__set_flsgetvalue` (they make the same GetLastError /
  TlsGetValue / FlsGetValue / SetLastError stand-in calls), `__security_check_cookie`, malloc / free / _aligned_* / operator
  new / delete (HeapAlloc / HeapFree stand-ins), qsort (+ shortsort, swap; it calls back into the game), type_info name
  methods, rand, ldiv, `_aulldvrm`.
- `crt_rtti.cpp`: `__RTDynamicCast` and its internals, `__unDName` / `__unDNameHelper` and the demangler pieces executed,
  `_lock` / `_unlock`, strtol (`strtoxl` 7855e17b), atol, atoi.
- `crt_cpp.cpp` (msvcp90): `std::basic_string<char>` compare (4 overloads) / operator== / operator< / assign (3) / operator= /
  `_Grow` / `_Inside` / `_Eos` / `_Tidy` / `_Copy` / find / find_first_of / constructors / destructor, `char_traits::_Copy_s` /
  `find`, `allocator::allocate`, `wstring::size`.

Names: exports keep their CRT name (cleaned); internals use the VC90 CRT source names where certain, else `Fn_<addr>`. These
are DLL functions, not exe functions, so nothing goes to `_re/names` (the Ghidra project is the exe's).

## Left

Per frame: nothing (both DLLs at 0 instructions per frame on the five worlds). Not covered: CRT code that only runs while
booting / loading or on paths the five worlds never take (those run original, as before); the SEH / EH prologs called by
still-unported callers. msvcr90 functions the exe calls outside the race frame are not ported yet; `auto.py <cov dir>` on a
coverage run of the boot adds them to the list.
