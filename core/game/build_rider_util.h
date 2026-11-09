/* build_rider_util.h -- small shared helpers of construction unit 7 (riders, core/notes/construction.md section 13): the MSVC 9
 * container pieces the constructors inline, calls of other units' functions (the core's when registered, else the original:
 * oracle only), virtual calls, the OS objects (critical sections, events, threads) and the runtime constants. Internal to
 * game/build_rider*.c. */
#pragma once
#include "rx_game.h"
#include "build_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the constant vectors the constructors copy (.data / runtime vectors written by the exe's static initializers: unit 12) */
#define RR_ZERO3      0x00a95d50u   /* zero vector (x, y, z, w) the constructors copy word by word */
#define RR_IDENTITY   0x009cb510u   /* identity matrix (16 words) */
#define RR_QIDENTITY  0x009cb550u   /* identity quaternion (0, 0, 0, 1) */
#define RR_RT_ZERO    0x00d61670u   /* runtime vector (0, 0, 0, 0) */
#define RR_RT_W1      0x00d61640u   /* runtime vector (0, 0, 0, 1) */
#define RR_RT_Y1      0x00d616a0u   /* runtime vector (0, 1, 0, 0) */
#define RR_RT_X1      0x00d615c0u   /* runtime vector (1, 0, 0, 0) */
#define RR_RT_Z1      0x00d61680u   /* runtime vector (0, 0, 1, 0) */

#define RR_ENGINE     0x00a95844u   /* XenonCore / the engine (+8 the Database) */
#define RR_GAME       0x00a946b8u   /* MXGame */
#define RR_GEARLOADER 0x00d92ef4u   /* the FactoryGearLoader (rider gear packages; +4 the MXRider_Hierarchy handle) */

static inline void rr_fatal(const char* what, uint32_t a) { fprintf(stderr, "rxcore rider: %s (%08x)\n", what, a); abort(); }
static inline void rr_copy(rx_addr dst, rx_addr src, uint32_t bytes) { for (uint32_t k = 0; k < bytes; k += 4) SU32(dst, k, U32(src, k)); }
static inline void rr_zero(rx_addr dst, uint32_t bytes) { for (uint32_t k = 0; k < bytes; k += 4) SU32(dst, k, 0); }
static inline void rr_zero8(rx_addr dst, uint32_t bytes) { for (uint32_t k = 0; k < bytes; k++) SU8(dst, k, 0); }
static inline void rr_fill(rx_addr dst, uint32_t bytes, uint32_t v) { for (uint32_t k = 0; k < bytes; k += 4) SU32(dst, k, v); }
static inline void rr_v4(rx_addr dst, rx_addr src) { rr_copy(dst, src, 16); }
static inline void rr_splat(rx_addr dst, float f) { for (uint32_t k = 0; k < 16; k += 4) SF32(dst, k, f); }
static inline void rr_set4(rx_addr dst, float a, float b, float c, float d) { SF32(dst, 0, a); SF32(dst, 4, b); SF32(dst, 8, c); SF32(dst, 0xc, d); }
/* fld m32; fstp m32: the bits, a signalling NaN quieted */
static inline uint32_t rr_x87_copy(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}
static inline rx_addr rr_alloc(uint32_t n) { return rx_reflex_aligned_alloc(n); }

/* the std::vector the constructors inline: a 4-byte container proxy (ReflexAlignedAlloc) pointing back at the vector, begin / end /
 * capacity (+0xc / +0x10 / +0x14) 0 */
static inline void rr_vec_init(rx_addr v) {
    rx_addr p = rr_alloc(4);
    if (p) SU32(p, 0, v);
    SU32(v, 0, p); SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
}
/* a game C string into a host buffer */
static inline void rr_gets(rx_addr s, char* out, uint32_t n) {
    uint32_t k = 0;
    if (s) for (; k + 1 < n; k++) { uint8_t c = U8(s, k); if (!c) break; out[k] = (char)c; }
    out[k] = 0;
}
static inline uint32_t rr_strlen(rx_addr s) { uint32_t k = 0; while (U8(s, k)) k++; return k; }

/* ---- calls of other units' functions ---- */
/* a function with the standard stack convention (cdecl / stdcall / thiscall: `this` first): the core's when registered */
typedef uint32_t (*rr_fn0)(void);
typedef uint32_t (*rr_fn1)(uint32_t);
typedef uint32_t (*rr_fn2)(uint32_t, uint32_t);
typedef uint32_t (*rr_fn3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (*rr_fn4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (*rr_fn5)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (*rr_fn6)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
static inline uint32_t rr_dispatch(void* f, int n, const uint32_t* a, uint32_t self, int has_self) {
    uint32_t v[7]; int k = 0;
    if (has_self) v[k++] = self;
    for (int i = 0; i < n; i++) v[k++] = a[i];
    switch (k) {
    case 0: return ((rr_fn0)f)();
    case 1: return ((rr_fn1)f)(v[0]);
    case 2: return ((rr_fn2)f)(v[0], v[1]);
    case 3: return ((rr_fn3)f)(v[0], v[1], v[2]);
    case 4: return ((rr_fn4)f)(v[0], v[1], v[2], v[3]);
    case 5: return ((rr_fn5)f)(v[0], v[1], v[2], v[3], v[4]);
    case 6: return ((rr_fn6)f)(v[0], v[1], v[2], v[3], v[4], v[5]);
    }
    rr_fatal("call with too many arguments", 0);
    return 0;
}
/* a virtual call: the vtable entry at `off` with signature (this, stack arguments) */
static inline uint32_t rr_vcall(rx_addr obj, uint32_t off, int n, const uint32_t* a) {
    rx_addr target = U32(U32(obj, 0), off);
    void* f = RX_FN_INTS(target, n + 1);
    if (!f) return rx_call_original(target, RX_THISCALL, obj, n, a);
    return rr_dispatch(f, n, a, obj, 1);
}
/* a direct call of a standard-convention function of another unit */
static inline uint32_t rr_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a) {
    void* f = RX_FN_INTS(fn, n + (conv == RX_THISCALL));
    if (!f) return rx_call_original(fn, conv, ecx, n, a);
    return rr_dispatch(f, n, a, ecx, conv == RX_THISCALL);
}
/* an original with a register convention (LTCG) of another unit, not ported yet */
static inline void rr_regs_clear(rx_regs* r) { memset(r, 0, sizeof *r); }

/* ---- the Database (engine + 8): its virtuals ---- */
static inline rx_addr rr_database(void) { return U32(U32(0, RR_ENGINE), 8); }

/* ---- OS objects ----
 * Oracle: the KERNEL32 imports through the exe's IAT (the boot's stand-ins). Product: the core builds on one thread; a critical
 * section is left as InitializeCriticalSection leaves it in the reference boot ({0, -1, 0, 0, 0, 0}) and enter / leave are no-ops. */
#define RR_IAT_ENTER_CS      0x0091600cu
#define RR_IAT_LEAVE_CS      0x009160f8u
#define RR_IAT_INIT_CS       0x00916014u
static inline void rr_os_cs_init(rx_addr cs) {
#ifdef RX_ORACLE
    uint32_t a = cs; rx_call_original(U32(0, RR_IAT_INIT_CS), RX_STDCALL, 0, 1, &a);
#else
    SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 0);
#endif
}
static inline void rr_os_cs_enter(rx_addr cs) {
#ifdef RX_ORACLE
    uint32_t a = cs; rx_call_original(U32(0, RR_IAT_ENTER_CS), RX_STDCALL, 0, 1, &a);
#else
    (void)cs;
#endif
}
static inline void rr_os_cs_leave(rx_addr cs) {
#ifdef RX_ORACLE
    uint32_t a = cs; rx_call_original(U32(0, RR_IAT_LEAVE_CS), RX_STDCALL, 0, 1, &a);
#else
    (void)cs;
#endif
}
