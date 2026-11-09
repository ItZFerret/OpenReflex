/* crt.h -- the C runtime the game links (msvcr90.dll 9.0.30729.1 at 0x78520000, msvcp90.dll at 0x78480000), as the OPEN REFLEX
 * core's C API: each function mirrors one DLL entry point (address in its comment) with that entry point's exact semantics, so a
 * core port replaces `call [iat]` / `call <exe thunk>` by a direct call. Implemented in game/crt_*.c (notes/crt.md).
 *
 * x87 entry points (_CIpow, _CIatan2, _CIfmod): operands and result as rx_f80 (the x87 stack values: st(1) first, st(0)
 * second, as the call sites push them), cw = the caller's x87 control word (its thread's: 0x027f on the main thread). The
 * result is what the original leaves in st(0); a caller that stores it with fstp m32 / m64 rounds it with rx_f80_to_f32 / _f64.
 * floor / ceil / pow return the double the original loads (fld qword: exact as an rx_f80). */
#pragma once
#include "rx_game.h"
#include "../src/rx_x87.h"
#ifdef __cplusplus
extern "C" {
#endif

#define CRT_CW_MAIN 0x027f   /* the main thread's x87 control word (53-bit precision, round to nearest, exceptions masked) */

/* ---- math (crt_math.c) ------------------------------------------------------------------------------------------------------ */
double  crt_copysign(double x, double y);                 /* 785a1254 _copysign (result fld: exact) */
int32_t crt_finite(double x);                             /* 785a166d _finite */
double  crt_floor(double x);                              /* 785a2f90 floor, its SSE2 path (the game's: MXCSR / cw mask all); NaN: errno */
double  crt_ceil(double x);                               /* 785a2da0 ceil, likewise */
rx_f80  crt_ci_atan2(rx_f80 y, rx_f80 x, uint16_t cw);    /* 7859aa6a _CIatan2: st(1) = y, st(0) = x -> atan2(y, x), 64-bit */
rx_f80  crt_ci_fmod(rx_f80 x, rx_f80 y, uint16_t cw);     /* 7859ac8a _CIfmod: st(1) = x, st(0) = y -> fmod(x, y) (exact) */
rx_f80  crt_ci_pow(rx_f80 x, rx_f80 y, uint16_t cw);      /* 7859afc0 _CIpow: st(1) = x, st(0) = y -> x ** y (a double) */
double  crt_pow_sse2(double x, double y);                 /* 785a5799 _CIpow's SSE2 kernel (x, y already doubles) */
double  crt_libm_sse2_pow(double x, double y);            /* 7859e9d7 __libm_sse2_pow (xmm0 = x, xmm1 = y -> xmm0) */
double  crt_libm_sse2_sin(double x);
double  crt_exp_x87(double x);                            /* 7859ac2c _CIexp for a double operand: st(0) after it */
double  crt_exp_sse2(double x, int* ok);                  /* 785a4dae __libm_sse2_exp main path (the value _CIexp leaves in st(0)); *ok = 0: the original                      /* 7859fb0c __libm_sse2_sin (xmm0 -> xmm0; exe thunk 009009ae) */

/* ---- memory / strings (crt_string.c): pointers are game addresses; results as the DLL returns them ----------------------- */
rx_addr  crt_memcpy(rx_addr d, rx_addr s, uint32_t n);            /* 7855aac0 memcpy (= memmove: overlap-safe) */
rx_addr  crt_memmove(rx_addr d, rx_addr s, uint32_t n);           /* 7855afb0 memmove */
rx_addr  crt_memset(rx_addr d, int32_t c, uint32_t n);            /* 7855b320 memset */
int32_t  crt_memcmp(rx_addr a, rx_addr b, uint32_t n);            /* 78544792 memcmp: -1 / 0 / 1 */
rx_addr  crt_memchr(rx_addr p, int32_t c, uint32_t n);            /* 7855ae30 memchr */
int32_t  crt_memcpy_s(rx_addr d, uint32_t size, rx_addr s, uint32_t n);   /* 7855664b memcpy_s */
int32_t  crt_memmove_s(rx_addr d, uint32_t size, rx_addr s, uint32_t n);  /* 785566cd memmove_s */
uint32_t crt_strlen(rx_addr s);                                   /* 7855b660 strlen */
int32_t  crt_strcpy_s(rx_addr d, uint32_t size, rx_addr s);       /* 785569b2 strcpy_s */
rx_addr  crt_strncpy(rx_addr d, rx_addr s, uint32_t n);           /* 7855b830 strncpy (zero padded) */
rx_addr  crt_wcsncpy(rx_addr d, rx_addr s, uint32_t n);           /* 785571c9 wcsncpy */
rx_addr  crt_strchr(rx_addr s, int32_t c);                        /* 7855b4b0 strchr */
rx_addr  crt_strrchr(rx_addr s, int32_t c);                       /* 7855ba50 strrchr */
rx_addr  crt_strstr(rx_addr h, rx_addr n);                        /* 7855bb50 strstr */
rx_addr  crt_strpbrk(rx_addr s, rx_addr set);                     /* 7855ba00 strpbrk */
rx_addr  crt_strtok(rx_addr s, rx_addr delims);                   /* 78556bd6 strtok (the cursor in the thread's data +0x18) */
int32_t  crt_strcmp(rx_addr a, rx_addr b);                        /* 7855b580 strcmp: -1 / 0 / 1 */
int32_t  crt_strncmp(rx_addr a, rx_addr b, uint32_t n);           /* 78556789 strncmp: byte difference */
int32_t  crt_wcsncmp(rx_addr a, rx_addr b, uint32_t n);           /* 78557165 wcsncmp: unit difference */
int32_t  crt_tolower(int32_t c);                                  /* 7855e8c1 tolower */
int32_t  crt_stricmp(rx_addr a, rx_addr b);                       /* 78578d50 _stricmp: difference of the lowered bytes */
int32_t  crt_strnicmp(rx_addr a, rx_addr b, uint32_t n);          /* 78579347 _strnicmp: -1 / 0 / 1 */
int32_t  crt_strtol(rx_addr nptr, rx_addr endptr, int32_t base);  /* 7855e3af strtol */
int32_t  crt_atol(rx_addr s);                                     /* 78546074 atol */
int32_t  crt_atoi(rx_addr s);                                     /* 785460ad atoi */

/* ---- formatted output (crt_printf.c): args = the game address of the first variadic argument / the va_list -------------- */
int32_t crt_sprintf(rx_addr buf, rx_addr format, rx_addr args);                          /* 78552b7e sprintf */
int32_t crt_vsnprintf(rx_addr buf, uint32_t count, rx_addr format, rx_addr args);        /* 7855471e _vsnprintf */
int32_t crt_vsnprintf_l(rx_addr buf, uint32_t count, rx_addr format, rx_addr locale, rx_addr args);   /* 78554666 */
/* 0078fd60 (the game's wrapper; EDI = buf, ESI = size; cdecl format, ...): _vsnprintf, then the terminator at the result when
 * 0 < result < size, else at buf[size - 1] (crt_format.c); args = the game address of the first variadic argument */
void    rx_format_buffer(rx_addr buf, uint32_t size, rx_addr format, rx_addr args);
rx_addr crt_ptd(void);                     /* 7854383f _getptd_noexit: the running thread's data, created on first use (crt_thread.c) */
/* The original's thread the core's code runs as (product only: the oracle runs on the emulated thread; w-rng): the per-thread data
 * crt_ptd returns (the CRT rand's seed among it). Returns the previous one, for the restore. */
enum { CRT_THREAD_MAIN = 0, CRT_THREAD_LOADER = 1, CRT_THREADS = 2 };
int     crt_thread_switch(int thread);
rx_addr crt_ptd_of(int thread);            /* product: that thread's data, 0 before its first use (oracle: 0) */
rx_addr crt_updatetmbcinfo(void);          /* 7856232a __updatetmbcinfo */

/* ---- runtime (crt_runtime.c) ---------------------------------------------------------------------------------------------- */
rx_addr crt_malloc(uint32_t size);                                    /* 785839df malloc */
void    crt_free(rx_addr p);                                          /* 785837ee free */
rx_addr crt_operator_new(uint32_t size);                              /* 78583b39 operator new */
void    crt_operator_delete(rx_addr p);                               /* 78583ba3 operator delete */
rx_addr crt_aligned_offset_malloc(uint32_t size, uint32_t align, uint32_t offset);   /* 78583bd3 _aligned_offset_malloc */
rx_addr crt_aligned_malloc(uint32_t size, uint32_t align);            /* 78583cee _aligned_malloc */
void    crt_aligned_free(rx_addr p);                                  /* 78583ccf _aligned_free */
void    crt_qsort(rx_addr base, uint32_t num, uint32_t width, rx_addr comp);   /* 78594640 qsort (comp: game function address) */
int32_t crt_rand(void);                                               /* 78594bed rand (the seed in the per-thread data) */
typedef struct { int32_t quot, rem; } crt_ldiv_t;
crt_ldiv_t crt_ldiv(int32_t n, int32_t d);                            /* 7859442e ldiv */
void    crt_invalid_parameter_noinfo(void);                           /* 7854... _invalid_parameter_noinfo (IAT 0x916224): the checked
                                                                         STL / CRT failure report; the original ends the process */
rx_addr crt_type_info_name(rx_addr self, rx_addr node);               /* 7857bfaf type_info::_name_internal_method */
rx_addr crt_type_name_base(rx_addr self, rx_addr list);              /* 7857c2b8 _Name_base_internal (crt_undname.c) */
rx_addr crt_rtdynamiccast(rx_addr inptr, int32_t vfdelta, rx_addr src, rx_addr target, int32_t is_ref);   /* 7857e090 */

/* ---- msvcp90 std::basic_string<char> (crt_cpp.c): s = the string object (thiscall ECX); results as the members return them -- */
void     crt_str_eos(rx_addr s, uint32_t n);                                   /* 7848588a _Eos */
int      crt_str_inside(rx_addr s, rx_addr p);                                 /* 784858b0 _Inside */
void     crt_str_tidy(rx_addr s, int built, uint32_t newsize);                 /* 78485de4 _Tidy */
void     crt_str_copy(rx_addr s, uint32_t newsize, uint32_t oldlen);           /* 78485ffd _Copy */
int      crt_str_grow(rx_addr s, uint32_t newsize, int trim);                  /* 784860e1 _Grow */
rx_addr  crt_str_erase(rx_addr s, uint32_t off, uint32_t count);               /* 784859ec erase(off, count) */
rx_addr  crt_str_assign_sub(rx_addr s, rx_addr right, uint32_t roff, uint32_t count);   /* 78486316 assign(str, pos, n) */
rx_addr  crt_str_assign_n(rx_addr s, rx_addr p, uint32_t count);               /* 784863af assign(ptr, n) */
rx_addr  crt_str_assign(rx_addr s, rx_addr p);                                 /* 78486428 assign(ptr) */
rx_addr  crt_str_operator_assign(rx_addr s, rx_addr p);                        /* 784872a2 operator=(ptr) */
rx_addr  crt_str_ctor(rx_addr s);                                              /* 78485e6a basic_string() */
rx_addr  crt_str_ctor_cstr(rx_addr s, rx_addr p);                              /* 78487135 basic_string(ptr) */
rx_addr  crt_str_ctor_copy(rx_addr s, rx_addr right);                          /* 78487272 basic_string(const&) */
void     crt_str_dtor(rx_addr s);                                              /* 78485ea4 ~basic_string */
/* The same calls with the frames msvcp90 / msvcr90 leave below them (crt_frames.c): e = the call's return slot (the caller's ESP
 * after its call instruction), r = the caller's EBX / EBP / ESI / EDI at the call. For core callers that keep the original's stack
 * where later code copies uninitialized locals (dead stack residue). */
typedef struct { uint32_t ebx, ebp, esi, edi; } crt_regs4;
rx_addr  crt_str_ctor_cstr_at(rx_addr e, crt_regs4 r, rx_addr s, rx_addr p);   /* 78487135 */
void     crt_str_dtor_at(rx_addr e, crt_regs4 r, rx_addr s);                   /* 78485ea4 */
int32_t  crt_stricmp_at(rx_addr e, crt_regs4 r, rx_addr a, rx_addr b);        /* 78578d50 */
void     crt_aligned_free_at(rx_addr e, crt_regs4 r, rx_addr p);              /* 78583ccf _aligned_free */
int32_t  crt_str_compare_sub(rx_addr s, uint32_t off, uint32_t n0, rx_addr p, uint32_t count);   /* 784857ce */
int32_t  crt_str_compare_cstr(rx_addr s, rx_addr p);                           /* 78485d8d compare(ptr) */
int32_t  crt_str_compare(rx_addr s, rx_addr right);                            /* 78485d0b compare(str) */
uint32_t crt_str_eq_cstr(rx_addr s, rx_addr p);                                /* 784958f7 operator==(str, ptr) */
uint32_t crt_str_less(rx_addr l, rx_addr r);                                   /* 78495979 operator<(str, str) (AL) */
uint32_t crt_str_find(rx_addr s, rx_addr p, uint32_t off, uint32_t count);     /* 7848536a find(ptr, off, n) */
uint32_t crt_str_find_cstr(rx_addr s, rx_addr p, uint32_t off);                /* 7848540c find(ptr, off) */
uint32_t crt_str_find_str(rx_addr s, rx_addr right, uint32_t off);            /* 78485c03 find(str, off) */
uint32_t crt_str_find_first_of_ch(rx_addr s, uint8_t ch, uint32_t off);       /* 78485435 find_first_of(ch, off) */
uint32_t crt_wstr_size(rx_addr s);                                             /* 7848d935 wstring::size */
rx_addr  crt_str_replace_sub(rx_addr s, uint32_t off, uint32_t n0, rx_addr right, uint32_t roff, uint32_t count);   /* 78486811 */
rx_addr  crt_str_replace(rx_addr s, uint32_t off, uint32_t n0, rx_addr right); /* 784874d8 replace(off, n, str) */
rx_addr  crt_str_replace_n(rx_addr s, uint32_t off, uint32_t n0, rx_addr p, uint32_t count);   /* 78486aff replace(off, n, ptr, count) */
rx_addr  crt_str_replace_cstr(rx_addr s, uint32_t off, uint32_t n0, rx_addr p); /* 78486c52 replace(off, n, ptr) */
rx_addr  crt_str_begin(rx_addr s, rx_addr ret);                                /* 78485a62 begin(): iterator into *ret */
rx_addr  crt_str_end(rx_addr s, rx_addr ret);                                  /* 78485ab4 end() */
rx_addr  crt_str_erase_range(rx_addr s, rx_addr ret, rx_addr fc, rx_addr fp, rx_addr lc, rx_addr lp);   /* 78485f0e */
void     crt_str_clear(rx_addr s);                                             /* 78485f79 clear() */
rx_addr  crt_allocator_allocate(rx_addr self, uint32_t n);                     /* 78485912 allocator<char>::allocate */
rx_addr  crt_traits_copy_s(rx_addr d, uint32_t dsize, rx_addr s, uint32_t n);  /* 78484f13 char_traits<char>::_Copy_s */

/* For the oracle adapters (the original's register-level results): */
void    crt_qsort_ex(rx_addr base, uint32_t num, uint32_t width, rx_addr comp, int32_t* last_byte);   /* qsort's arg-slot byte */
int32_t crt_wcsncmp_ex(rx_addr a, rx_addr b, uint32_t n, uint32_t* n_left);   /* wcsncmp's count slot */
double  crt_floor_args(rx_addr args, uint16_t cw, uint32_t mxcsr);   /* floor on the cdecl argument slot (writes it back) */
double  crt_ceil_args(rx_addr args, uint16_t cw, uint32_t mxcsr);
/* the same with *eax = the EAX they leave (sign and exponent; &errno on the NaN path 785a3023 / 785a2e33) */
double  crt_floor_args_ex(rx_addr args, uint16_t cw, uint32_t mxcsr, uint32_t* eax);
double  crt_ceil_args_ex(rx_addr args, uint16_t cw, uint32_t mxcsr, uint32_t* eax);
rx_f80  crt_ci_atan2_ex(rx_f80 y, rx_f80 x, uint16_t cw, uint32_t regs[3]);   /* + EAX / ECX / EDX after it (regs may be NULL) */
rx_f80  crt_ci_fmod_ex(rx_f80 x, rx_f80 y, uint16_t cw, uint32_t regs[3]);
rx_f80  crt_ci_pow_ex(rx_f80 x, rx_f80 y, uint16_t cw, uint32_t mxcsr, uint32_t regs[3]);
double  crt_libm_sse2_pow_ex(double x, double y, uint64_t xmm0_out[2], uint32_t regs[3]);
double  crt_libm_sse2_pow_lanes(double x, double y, const uint64_t H[8], uint64_t xmm0_out[2], uint32_t regs[3]);   /* + the caller's xmm high lanes (special exits) */
double  crt_pow_kernel_raw(uint64_t x, uint64_t y, uint32_t regs[3], int* ok);   /* 785a5799 without the original fallback */
double  crt_libm_sse2_sin_ex(double x, uint32_t regs[3], int* ok);
/* 7859fc87 (abs(x) above the main range: inf / NaN, the x87 sin): e = the entry ESP (0: no frame), regs EAX / ECX / EDX in / out */
double  crt_libm_sse2_sin_big(double x, rx_addr e, uint16_t cw, uint32_t regs[3], int* lane1_zero, int* ok);   /* ok 0: the original's path (huge / inf / NaN) */

#ifdef __cplusplus
}
#endif
