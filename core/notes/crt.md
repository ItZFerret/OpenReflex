# crt: the C runtime of the race frames (msvcr90 / msvcp90 9.0.30729.1) as readable C

Files: `game/crt.h` (the API other subsystems call), `game/crt_util.h` (internal helpers), `game/crt_math.c`,
`game/crt_string.c`, `game/crt_printf.c`, `game/crt_runtime.c`, `game/crt_thread.c`, `game/crt_cpp.c`, `game/crt_tables.h` (libm constant tables,
generated from the DLL image by scratchpad `rx_crt/tools/gentab.py`); adapters `oracle/crt_*.cpp` (+ `oracle/crt_oracle.h`),
group `rx_crt`. Verify: `RX_CORE_FILES="crt_*" native/game/tools/build_cli.sh <dir>` then
`native/game/tools/verify.sh <dir>/reflex_boot rx_crt`.

## Work list (how it was found)

Fresh DLL coverage of the five verify worlds with no ports installed (scratchpad `rx_crt/cov/run.sh` with `REFLEX_COVERAGE`,
the `.bin.dll` files): 155 functions (entries = the generated ports' addresses in `native/game/ports/crt_*.cpp`,
`ld5_hi_crt.cpp`; executed pcs grouped by a control-flow walk, `rx_crt/tools/funcs.py`; per-function disassembly with counts
`rx_crt/d <addr>`). Since the CPUID fix the SSE2 paths run: `_CIpow` -> 785a5799, memcpy / memmove -> fastcopy_I, memset ->
_VEC_memzero. `plan/remaining-crt.tsv` predates that fix.

## What is ported (entry points with adapters; internals ported inside them)

| file | functions |
|---|---|
| crt_math.c | `_copysign` 785a1254, `_finite` 785a166d, `floor` 785a2f90, `ceil` 785a2da0 (SSE2 paths and their NaN path through `___libm_error_support` 785a8fa1, w-crtnan), `_CIatan2` 7859aa6a, `_CIfmod` 7859ac8a (via `__cintrindisp2` 785a4840 / 785a3ba7 / 785a490a and the handlers 785a47ac / 785a4828 / 785a4805 / 7859ac94 / 785a3c44), `_CIpow` 7859afc0 -> 785a5780 -> kernel 785a5799 (main path + the special path 785a5ae7: +-0, the subnormal scaling, x < 0 with an integer y), `__libm_sse2_pow` 7859e9d7, `__libm_sse2_sin` 7859fb0c |
| crt_string.c | memcpy, memmove, memset, memcmp, memchr, memcpy_s, memmove_s, strlen, strcpy_s, strncpy, wcsncpy, strchr, strrchr, strstr, strpbrk, strcmp, strncmp, wcsncmp, tolower, _stricmp, _strnicmp, strtol (strtoxl 7855e17b), atol, atoi |
| crt_printf.c | sprintf, _vsnprintf, _vsnprintf_l, with _output_l 78570f34, write_char / write_multi_char / write_string, _LocaleUpdate, _isleadbyte_l, the %f chain (_cfltcvt_l, _cftof_l, _fltout2, __dtold, $I10_OUTPUT with its inlined __multtenpow12 / __ld12mul, _fptostr, _cftof2_l, _shift), _aulldvrm, _getptd / _getptd_noexit / __set_flsgetvalue |
| crt_runtime.c | malloc, free, operator new / delete, _aligned_offset_malloc, _aligned_malloc, _aligned_free, qsort (+ shortsort, swap), rand, ldiv, type_info::_name_internal_method (+ _Name_base_internal's cached path), __RTDynamicCast (+ FindCompleteObject, FindSITargetTypeInstance, FindMITargetTypeInstance, PMDtoOffset) |
| crt_thread.c | _getptd_noexit 7854383f with the creation of a thread's data (_calloc_crt / _calloc_impl, _decode_pointer, _initptd, __addlocaleref, _lock / _unlock 12 and 13), __set_flsgetvalue, __updatetmbcinfo 7856232a |
| crt_cpp.c | msvcp90 basic_string<char>: ctor x3, dtor, assign x3, operator=, compare x3, operator== / <, find x3, find_first_of, erase x2, clear, replace x4, begin, end, _Eos, _Inside, _Tidy, _Copy, _Grow, allocator::allocate, char_traits::_Copy_s / _Move_s / find, basic_string<wchar_t>::size |

The API (`crt.h`) mirrors each entry point; the comments name the address. x87 entry points take the stack values as rx_f80 and
the caller's control word; floor / ceil / pow return the double the original loads (exact as an rx_f80).

## Exactness notes

- **libm.** The dispatcher computes atan2 / fmod with control word 0x133f (64-bit precision, nearest, all masked) and restores
  the caller's word, so the caller's precision does not enter. fpatan = `rx_f80_transcendental(RX_FPATAN, |x|, |y|)`, pi - r at
  64 bits (fsubrp, DE E1), the class table is fxam's in the emulator's value model (no denormals). fprem is exact; the emulator
  completes it in one step (C2 clear), so the port computes the full truncating remainder. The pow kernels are Intel's SSE2
  libm (three table reciprocals for log2, a 2^(j/128) table and polynomials): double arithmetic in the original order; the two
  variants differ in the exponent rounding step (1.5 * 2^52 shifter vs cvtsi2sd) and the sign of the 2^-7 factor. Reading the
  dispatcher descriptor (785bb0ae) also keeps its DLL page resident like the original (the On-mode memory hash counts pages).
- **pow's special path.** `_CIpow`'s SSE2 dispatch (785a5780) is what the game takes (the main thread's word is 0x027f, MXCSR
  0x1f80, and the core sets `__sse2_available` 785bd7b0 = 1 like the DLL's startup), so the non-SSE2 x87 code 7859b01d is not
  the path behind the stop; the kernel 785a5799's **special path 785a5ae7** is (it runs whenever x is not a positive normal).
  The 12-rider W+A/D ride (race 1010) reaches it with the engine's quarter power: the clutch / speed ratio is exactly **+0**
  (`bike_force.c force_pow_quarter`, x = 0.0f, y = 0.25), where the kernel returns +0 (785a5d07 / 785a5d15). Ported from
  785a5ae7: the +-0 results with y's sign and the odd-integer sign of |x| < 0 (785a5d07 / 785a5cd6 / 785a5db9), the positive
  subnormal scaling (785a5c34: 2^64 x, mantissa / index recomputed, then the 785a5816 main path), x < 0 with an integer y
  (785a5bd6: `off` 0xbfe7f, the 0x40000 sign flag into the exponent field) and the 785a624c exact-1 result with that sign.
  The kernel's main path now takes the exponent offset / sign flag as parameters (`crt_pow_core`), so the special path reuses it
  instead of duplicating it. Equivalence: `oracle/crt_pow_probe.cpp` (`REFLEX_PORT=rx_PowProbeHook RX_POW_PROBE_FRAME=<n>` on a
  reference world) calls the original 7859afc0 and `crt_pow_kernel_raw` on 192 crafted x87 stacks: **99 ported, 0 mismatches**
  (the other 93 decline to the original: y inf / NaN / zero, x inf / NaN, x < 0 with a non-integer y, the error-support
  branches); the game's own (+0, 0.25) matches +0. The only register differences are the six +-0 with y < 0 cases, whose
  EAX / ECX / EDX come from the error support 785a8fa1 (not live to the game's callers; the adapter declares them).
  In situ, the five reference worlds call `_CIpow` 4994 / 1658 times (`_CIpow` / `__libm_sse2_pow`, Concord 1800) with 0
  mismatches, but their ratios never reach zero (no 785a5ae7 case), so the probe is the per-call check for the new branch.
- **floor / ceil** store their result into their own argument slot (`crt_floor_args`); **wcsncmp** counts its count argument
  down in place; **qsort**'s partition swap keeps its temporary byte in the high byte of its first argument slot. The adapters
  reproduce these caller-frame writes.
- **Dead stack read back.** Particle code copies a stack template with uninitialized fields into the particle pool: they pick
  up memcpy's locals (fastcopy_I's frame). memcpy / memmove / memset therefore write the original's frames (`crt_move_frame`,
  `crt_memzero_frame`), including the caller's saved registers (`crt_caller_regs`, from the adapter; 0 in the product).
- **printf** formats into a host buffer and then stores the characters in order (same final memory); a %s source overlapping
  the output declines. _flsbuf on a full _vsnprintf buffer sets errno (ptd + 8) = ERANGE: reproduced. The digit generation is
  $I10_OUTPUT's own 12-byte arithmetic (not correctly rounded in general: transliterated, not replaced).
- **Per-thread data** lives in OS TLS: `crt_ptd()` makes the original's kernel32 calls (GetLastError, TlsGetValue of slot
  [785b71e4] -> the "FlsGetValue" pointer called with [785b71e0], SetLastError; on a thread's first use HeapAlloc, the decoded
  FlsSetValue, GetModuleHandleW / GetProcAddress, InterlockedIncrement, Enter / LeaveCriticalSection, GetCurrentThreadId) through
  msvcr90's import slots. In the world runtime these are host stand-ins (runtime.cpp FLS_GET / FLS_SET); the product needs an OS
  layer providing them (lead).
- **Heap**: HeapAlloc / HeapFree (msvcr90's imports 78521170 / 7852116c) in the oracle (the emulator's heap is host state),
  `rx_heap_alloc(size, 8)` / `rx_heap_free` in the product.
- **msvcp90** `_Copy`'s `__EH_prolog3` reads msvcp90's `__security_cookie` (78506998): read as well (page residency).

- **msvcp90 find(const char*, off) 7848540c / replace(off, n0, const char*) 78486c52 / replace(off, n0, ptr, count) 78486aff**
  (added for the UTV proxy 004c0c00, construction unit 6): find = find(p, off, strlen(p)); replace(ptr) = replace(off, n0, p,
  strlen(p)); replace(ptr, count) is the DLL's: a source inside the string goes through replace(off, n0, *this, p - _Myptr, count)
  (replace_sub), _Xran / _Xlen run the original, the tail moves before _Grow when the string shrinks and after it when it grows.
  78486aff stores into its own off slot (_Myptr in the shrink branch, then newsize) inside 78486c52's frame: dead
  stack below the caller, not reproduced (no caller reads it). Verify (PX_Laguna UTV event, 12 vehicles, the original 004c0c00 with
  rx_crt On): 7848540c 12 / 0, 78486c52 12 / 0 (78486aff is reached only through 78486c52 there).

## Frames (crt_frames.c, w-vfactory 2026-10-27)
Game code that copies never-written locals into heap records makes the CRT's dead stack observable (the vehicle parts' node bounds, the
light event records: known-differences 1). `crt_frames.c` has the CRT calls that also write every stack word the original's code path
writes, for core callers that keep the original's stack: `crt_str_ctor_cstr_at` (std::string(const char*) 78487135: _Eos, assign,
strlen, assign(ptr, n), _Inside, _Grow, the long path's _Copy with __EH_prolog3 / allocate / _Allocate / operator new / malloc / _Tidy /
__EH_epilog3, the copy through 784859a4 / 78485215 / _Copy_s / memcpy_s / memcpy), `crt_str_dtor_at` (~basic_string 78485ea4: _Tidy,
operator delete / free with __SEH_prolog4 and HeapFree, __SEH_epilog4) and `crt_stricmp_at` (_stricmp 78578d50 -> __ascii_stricmp, the
"C" locale path). Each takes the call's return slot and the caller's EBX / EBP / ESI / EDI (`crt_regs4`). The heap calls run the oracle's
original HeapAlloc / HeapFree at the original's ESP (their argument slots and return address are rewritten after); fs:[0] is the TEB's
(0x7ffd0000). Only the paths the game takes are reproduced: the error / locale paths run the plain ports without frames.
`crt_aligned_free_at` (w-residue2): _aligned_free 78583ccf's `push ebp; push [(p & ~3) - 4]; call free` in front of free's frame
above (operator delete enters free at the same slot), for the game's ReflexAlignedFree 0078fc50 with its frame
(`rx_reflex_aligned_free_at`, build_alloc.c) and the node set release 007dd4d0. The allocation side (`rx_reflex_aligned_alloc_at`,
004eef20 / 0084df50 in build_asset.c) and the scene vectors' memmove_s / memset (anim_scene_vec.c, build_asset.c) do not model the
CRT: the oracle runs the original import at the original's ESP with the original's EBX / EBP / ESI / EDI
(`rx_call_original_regs`, src/rx_mem.h), the product the core's CRT without frames.

- **floor / ceil NaN (w-crtnan).** A NaN argument goes from the SSE2 code (ucomisd x, x unordered at 785a3023 / 785a2e33) to
  `___libm_error_support`(&x, &x, &x, 0x3ed floor / 0x3ec ceil): the _exception record {_DOMAIN, "floor" / "ceil", x, x, x} on its
  frame, the _matherr handler (the EXE's 009008e5 when its pre_c_init installed it, else msvcr90's default 78541c20; both return 0),
  errno (ptd + 8) = EDOM 33, and the quieted NaN (SNaN -> QNaN, payload kept) stored back into the argument slot and loaded as the
  result; EAX = &errno. `crt_libm_nan_error` (crt_math.c) reproduces it, with the dead stack down to A-0x60 when the caller has the
  original's stack (crt_floor_args). Probe `oracle/crt_nan_probe.cpp`: 64 raw cases (16 inputs x floor / ceil x both handlers) 0
  mismatches including the dead stack; under Verify floor 16 / 0, ceil 16 / 0, PackedGrid_SampleUV 100 / 0, TerrainMaterialAt 208 / 0
  (NaN coordinates). The x87 paths (785a9330 / 785a9256) stay unported: no thread unmasks an exception.

## Paths not ported (they run the original: `crt_unported`; `grep -n crt_unported core/game/crt_*.c`)

The game's race frames take none of these on the five worlds (and NAT_Renne) except the first `type_info::name()` of each type
(the demangler).

- math (w-crtnan2 ported the NaN / inf / domain branches of atan2, fmod, _CIpow, sin and exp: core/plan/QUEUE.md w-crtnan2; left:) NaN operands of __libm_sse2_pow's special code, atan2(inf, inf), atan2 results below 2^-1022 (the _87except error path), unmasked x87 precision
  exceptions, _adj_fprem (FDIV-bug CPUs), the x87 paths of floor / ceil / pow (taken only with unmasked exceptions), pow with
  x < 0 and a non-integer y, inf / NaN x, y = +-0 / inf / NaN, overflow / underflow and the 785a624c re-entry into the main
  path (the zero / subnormal / negative-integer-x cases of 785a5ae7 are ported, above).
- strings: invalid arguments of the _s functions (errno + _invalid_parameter), locale-aware paths (after setlocale), strtol's
  multibyte locales and ERANGE without per-thread data.
- printf: conversions other than d i u o x X p c s % f, wide characters / strings, %n, %#f, precision > 163 for %f (heap
  buffer), inf / NaN %f, a DBCS lead byte in the format, a thread without per-thread data or with a stale locale.
- runtime: heap modes other than the system heap, allocation failure (_callnewh / bad_alloc), qsort / aligned-malloc argument
  errors, __RTDynamicCast with virtual inheritance or a failed reference cast (bad_cast), the demangler.
- msvcp90: _Xran / _Xlen (out_of_range / length_error), checked-iterator failures, replace from itself.

When such a path is taken by original code, the adapter's fallback runs the original in the original's own frame (arguments
and return slot re-pushed where they are, `crt_replay_begin`), so frame-address-dependent stores (the demangler keeps
pointers into its frame in globals 785bc0fc / 785bc100) match.

## Left

- `__libm_sse2_sin` 7859fb0c (NAT_Renne / RenneB race frames, grass sway) is ported and verified on a NAT_Renne baton world
  booted for it (scratchpad `rx_crt/worlds/National_NAT_Renne`, `REFLEX_EVENT_SPEC=National,10,NAT_Renne,54,0,3,Laps,2,0`):
  6000 calls, 0 mismatches, On identical. The other SSE2 libm kernels only run in the boot.
- The demangler (`__unDName` and its ~20 internals): first type_info::name() per type, 3 calls per world.
- The demangler keeps its state in static members (785bc0e8 ..) and a heap of malloc'd blocks whose freed contents stay in the
  emulated heap: porting it means reproducing those objects byte for byte (vtables, nodes), for 3 calls per world.
- `_ftol2` / `_ftol2_sse` (exe 008fa106 / 008fa0d0) stay with rx_phys (phys_crt.c), agreed with the lead.
