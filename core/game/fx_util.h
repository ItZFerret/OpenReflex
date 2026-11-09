/* fx_util.h -- helpers shared by the effects code (core/game/fx_*.c: particles, mud, terrain deformation sync, tyre tracks, the
 * placed world entities of 0x40xxxx / 0x6cxxxx): the original's stack frames at the same game addresses, the x87 copy model,
 * the kernel32 services the particle code calls (critical sections, Sleep, Interlocked*), the thread's TEB, calls of code not in
 * the core. core/notes/fx.md. */
#pragma once
#include "rx_game.h"
#include "crt.h"
#include "phys_crt.h"
#include "loop_platform.h"
#include "loop_engine.h"

/* x87 precision of the threads the effects code runs on (main, scene ticker and job pool workers): control word 0x027f, 53 bits
 * (measured: the adapters print m.fpu_cw once). */
#define FX_X87_BITS 53
#define FX_CW 0x027fu

/* ---- the x87 load / store (fld m32 / m64; fstp m32 / m64): exact for numbers, a NaN keeps its payload with the quiet bit set
 * (the hardware's rule; rx_x87 and the oracle's f80 do the same) ---- */
static inline uint32_t fx_x87_bits(uint32_t b) {
    return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) ? (b | 0x00400000u) : b;
}
static inline uint64_t fx_x87_bits64(uint64_t b) {
    return ((b & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (b & 0x000fffffffffffffull))
        ? (b | 0x0008000000000000ull) : b;
}
static inline void fx_x87_copy(rx_addr d, rx_addr s) { SU32(d, 0, fx_x87_bits(U32(s, 0))); }
static inline void fx_x87_copy64(rx_addr d, rx_addr s) { rx_wr64(d, fx_x87_bits64(rx_rd64(s))); }
static inline rx_f80 fx_ld(rx_addr a) { return rx_f80_from_f32(rx_bf(U32(a, 0))); }
static inline rx_f80 fx_f(float x) { return rx_f80_from_f32(x); }
static inline rx_f80 fx_add(rx_f80 a, rx_f80 b) { return rx_f80_add(a, b, FX_X87_BITS, 0); }
static inline rx_f80 fx_sub(rx_f80 a, rx_f80 b) { return rx_f80_sub(a, b, FX_X87_BITS, 0); }
static inline rx_f80 fx_mul(rx_f80 a, rx_f80 b) { return rx_f80_mul(a, b, FX_X87_BITS, 0); }
static inline rx_f80 fx_div(rx_f80 a, rx_f80 b) { return rx_f80_div(a, b, FX_X87_BITS, 0); }
/* fstp m32 (round to nearest) */
static inline float fx_f32(rx_f80 a) { return rx_f80_to_f32(a, 0); }
/* fistp m64 under a truncating control word (cw | 0xc00): the low dword */
static inline uint32_t fx_fistp_trunc(rx_f80 a) { return (uint32_t)(uint64_t)rx_f80_to_int(a, 3); }

/* ---- the callee-saved registers as the original holds them ----
 * Frames save the caller's EBX / EBP / ESI / EDI on the stack, and dead stack (particle templates copy uninitialized frame
 * words) can carry them into memory, so the core keeps a model of these four registers: fx_reg(r) is the value the original
 * would hold now, fx_set_reg(r, v) records an assignment the original makes before a call whose callee pushes it. Oracle: the
 * emulated CPU's registers (bound by FX_PORT: they hold the original caller's values at the adapter's entry); product: a
 * register file of its own. Every frame restores the four on leave (callee-saved). */
enum { FX_EAX, FX_ECX, FX_EDX, FX_EBX, FX_ESP, FX_EBP, FX_ESI, FX_EDI };
uint32_t fx_reg(int r);
void     fx_set_reg(int r, uint32_t v);
typedef struct { uint32_t ebx, ebp, esi, edi; } fx_saved;
static inline fx_saved fx_save(void) { fx_saved s = {fx_reg(FX_EBX), fx_reg(FX_EBP), fx_reg(FX_ESI), fx_reg(FX_EDI)}; return s; }
static inline void fx_restore(fx_saved s) { fx_set_reg(FX_EBX, s.ebx); fx_set_reg(FX_EBP, s.ebp); fx_set_reg(FX_ESI, s.esi); fx_set_reg(FX_EDI, s.edi); }

/* ---- the original's stack frame ----
 * fx_frame_enter(&f, sub, align, n, regs): `push ebp; mov ebp, esp; [and esp, -align]; sub esp, sub; push regs[0..n-1]`
 * (align 0: no and): the saved EBP and the pushed registers are written (from the model), EBP = sp0 - 4; f.A = EBP &
 * ~(align - 1), f.E = the ESP after the pushes ([esp + X] = f.E + X). fx_frame_flat(&f, sub, n, regs): `sub esp, sub; push
 * regs` without EBP (sub 0: pushes only). fx_frame_leave restores ESP and the four registers. */
typedef struct { rx_addr sp0, ebp, A, E; fx_saved in; } fx_frame;
static inline void fx_push_regs(rx_addr at, int n, const int* regs) {
    for (int i = 0; i < n; i++) SU32(at - 4u * (uint32_t)(i + 1), 0, fx_reg(regs[i]));
}
static inline void fx_frame_enter_r(fx_frame* f, uint32_t sub, uint32_t align, int n, const int* regs) {
    f->in = fx_save();
    f->sp0 = rx_sp(); f->ebp = f->sp0 - 4;
    SU32(f->ebp, 0, f->in.ebp);
    fx_set_reg(FX_EBP, f->ebp);
    f->A = align ? (f->ebp & ~(align - 1)) : f->ebp;
    fx_push_regs(f->A - sub, n, regs);
    f->E = f->A - sub - 4u * (uint32_t)n;
    rx_set_sp(f->E);
}
static inline void fx_frame_flat_r(fx_frame* f, uint32_t sub, int n, const int* regs) {
    f->in = fx_save();
    f->sp0 = rx_sp(); f->ebp = 0; f->A = f->sp0 - sub;
    fx_push_regs(f->A, n, regs);
    f->E = f->A - 4u * (uint32_t)n;
    rx_set_sp(f->E);
}
#define FX_REGS(...) (int)(sizeof((int[]){__VA_ARGS__}) / sizeof(int)), (int[]){__VA_ARGS__}
#define fx_frame_enter(f, sub, align, ...) fx_frame_enter_r((f), (sub), (align), FX_REGS(__VA_ARGS__))
#define fx_frame_enter0(f, sub, align) fx_frame_enter_r((f), (sub), (align), 0, (const int*)0)
#define fx_frame_flat(f, sub, ...) fx_frame_flat_r((f), (sub), FX_REGS(__VA_ARGS__))
static inline void fx_frame_leave(const fx_frame* f) { rx_set_sp(f->sp0); fx_restore(f->in); }

/* A call as the original makes it: the n stack arguments at ESP - 4n .. and the return address below them; ESP moved to the
 * callee's entry ESP. fx_pop(sp0) restores it (the callee popped its arguments, or the caller's add esp did). */
static inline rx_addr fx_push(int n, const uint32_t* a, rx_addr ret) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * (uint32_t)(n + 1);
    SU32(sp, 0, ret);
    for (int i = 0; i < n; i++) SU32(sp, 4u + 4u * (uint32_t)i, a[i]);
    rx_set_sp(sp);
    return sp0;
}
static inline void fx_pop(rx_addr sp0) { rx_set_sp(sp0); }
#define FX_NARGS(...) ((int)(sizeof((uint32_t[]){__VA_ARGS__}) / sizeof(uint32_t)))
/* the residue a `push args; call [import]` leaves (the import's own frame is the OS's: nothing) */
static inline void fx_import_residue(rx_addr ret, int n, const uint32_t* a) {
    const rx_addr sp = rx_sp() - 4u * (uint32_t)n;
    for (int i = 0; i < n; i++) SU32(sp, 4u * (uint32_t)i, a[i]);
    SU32(sp, -4, ret);
}

/* ---- kernel32 (the particle managers are shared with the job pool workers; the core runs every thread's work in the
 * original's serialized order, so these are the plain memory operations the stand-ins perform) ---- */
static inline uint32_t fx_interlocked_inc(rx_addr p, rx_addr ret) {          /* InterlockedIncrement (IAT 0x916080) */
    const uint32_t a[1] = {p}; fx_import_residue(ret, 1, a);
    const uint32_t v = U32(p, 0) + 1; SU32(p, 0, v); return v;
}
static inline uint32_t fx_interlocked_dec(rx_addr p, rx_addr ret) {          /* InterlockedDecrement (IAT 0x916084) */
    const uint32_t a[1] = {p}; fx_import_residue(ret, 1, a);
    const uint32_t v = U32(p, 0) - 1; SU32(p, 0, v); return v;
}
/* InterlockedCompareExchange(dst, exchange, comparand) (IAT 0x91609c): the old value */
static inline uint32_t fx_interlocked_cmpxchg(rx_addr p, uint32_t x, uint32_t c, rx_addr ret) {
    const uint32_t a[3] = {p, x, c}; fx_import_residue(ret, 3, a);
    const uint32_t v = U32(p, 0); if (v == c) SU32(p, 0, x); return v;
}
/* Enter / LeaveCriticalSection (IAT 0x91600c / 0x9160f8) with the call's residue (oracle: the import runs) */
static inline void fx_enter_cs(rx_addr cs, rx_addr ret) { const uint32_t a[1] = {cs}; fx_import_residue(ret, 1, a); loop_enter_cs(cs); }
static inline void fx_leave_cs(rx_addr cs, rx_addr ret) { const uint32_t a[1] = {cs}; fx_import_residue(ret, 1, a); loop_leave_cs(cs); }
static inline void fx_sleep(uint32_t ms, rx_addr ret) { const uint32_t a[1] = {ms}; fx_import_residue(ret, 1, a); loop_sleep(ms); }

/* A CRT call whose frame the CRT port writes (memcpy / memmove / memset: crt_util.h): the arguments and the return address as
 * the original pushes them, rx_sp() left at the ESP before the pushes (where the CRT expects it) and the callee-saved registers
 * its frame saves from the model (crt_caller_regs, restored after). */
#ifndef crt_caller_regs
#define crt_caller_regs (rx_cpu_cur()->crt_caller_regs)   /* the running guest thread's (rx_cpu, w-jobthreads) */
#endif
#define FX_CRT(n, args, ret, call) do {                                                                       \
    const rx_addr s_ = fx_push((n), (args), (ret)); rx_set_sp(s_);                                            \
    uint32_t keep_[8]; for (int i_ = 0; i_ < 8; i_++) { keep_[i_] = crt_caller_regs[i_]; crt_caller_regs[i_] = 0; } \
    crt_caller_regs[FX_EBX] = fx_reg(FX_EBX); crt_caller_regs[FX_EBP] = fx_reg(FX_EBP);                     \
    crt_caller_regs[FX_ESI] = fx_reg(FX_ESI); crt_caller_regs[FX_EDI] = fx_reg(FX_EDI);                     \
    call;                                                                                                     \
    for (int i_ = 0; i_ < 8; i_++) crt_caller_regs[i_] = keep_[i_];                                           \
    rx_set_sp(s_);                                                                                            \
} while (0)

/* type_info::_name_internal_method (IAT 0x916388; thiscall ti, the root node 0x9e1a7c; RET 4) as `push 0x9e1a7c; mov ecx, ti;
 * call` makes it at the current ESP: the argument and the return address written, rx_sp() left at the ESP before the push (the
 * CRT port's first-name path enters _Name_base_internal from there) */
static inline rx_addr fx_type_name(rx_addr ti, rx_addr ret) {
    const uint32_t a[1] = {0x009e1a7cu};
    const rx_addr s = fx_push(1, a, ret); rx_set_sp(s);
#ifdef RX_ORACLE
    /* the first name of a type (the undecorated name computed and cached on the CRT heap): the CRT port's block differs from
     * msvcr90's (004172e0's Verify), so the oracle runs the import there */
    if (U32(ti, 4) == 0) { const rx_addr r = rx_call_original(U32(0, 0x00916388), RX_THISCALL, ti, 1, a); rx_set_sp(s); return r; }
#endif
    const rx_addr r = crt_type_info_name(ti, 0x009e1a7cu);
    rx_set_sp(s);
    return r;
}

/* The running thread's static TLS block (fs:[0x2c] -> the TLS pointer array, [0] = the exe's block). The TEB is at 0x7ffd0000
 * (the world runtime's; it switches TEB + 0x2c per thread). */
#define FX_TEB rx_teb()
static inline rx_addr fx_tls_block(void) { return U32(U32(FX_TEB, 0x2c), 0); }

/* ---- calls into code that is not in the core (oracle: the original; product: fatal, the coverage list) ---- */
/* registers for rx_call_original_ex: EAX / ECX / EDX zero, EBX / EBP / ESI / EDI from the model */
static inline rx_regs fx_regs(void) {
    rx_regs r; memset(&r, 0, sizeof r);
    r.r[FX_EBX] = fx_reg(FX_EBX); r.r[FX_EBP] = fx_reg(FX_EBP); r.r[FX_ESI] = fx_reg(FX_ESI); r.r[FX_EDI] = fx_reg(FX_EDI);
    return r;
}
#define FX_ORIG(fn, conv, ecx, ...) rx_call_original((fn), (conv), (ecx), FX_NARGS(__VA_ARGS__), (uint32_t[]){__VA_ARGS__})
#define FX_ORIG0(fn, conv, ecx) rx_call_original((fn), (conv), (ecx), 0, (const uint32_t*)0)

/* ---- engine globals ---- */
#define FX_G_ENGINE 0x00a95844u     /* g_EngineContext (XenonCore) */
#define FX_G_GAME   0x00a946b8u     /* the MXGame object */
static inline rx_addr fx_ctx(void) { return U32(0, FX_G_ENGINE); }
static inline rx_addr fx_timing(void) { return U32(fx_ctx(), 0x10); }
/* 0075c580 (EDX = game): the level world (game +0x5a54 while scene slot 0 runs, game +0x20c8 while slot 1 runs, else 0). Written
 * inline (rx_world registers it): the call leaves its return address and the pushed ESI (`esi` = the caller's) below ESP. */
static inline rx_addr fx_level_world(rx_addr game, rx_addr ret, uint32_t esi) {
    const rx_addr sp = rx_sp(); SU32(sp, -4, ret); SU32(sp, -8, esi);
    const rx_addr c = fx_ctx();
    if (U32(U32(c, 0x20), 0x34) == 1) return U32(game, 0x5a54);
    if (U32(U32(c, 0x1c), 0x34) == 1) return U32(game, 0x20c8);
    return 0;
}
/* the active scene world: context + 0x1c + 4 * [context + 4] */
static inline rx_addr fx_active_scene(void) { const rx_addr c = fx_ctx(); return U32(c, 0x1c + 4u * U32(c, 4)); }

/* JobPool_Submit 00881f00 (pool, job record, function; RET 0xc) called at the current ESP with the call site's return address
 * `ret`: the core's (rx_loop, loop_engine.c) does the work and leaves the dead stack of the original's frame (saved EBX / ESI from
 * the register model, the critical-section arguments and return addresses, the first argument slot). */
static inline void fx_jobpool_submit(rx_addr pool, rx_addr rec, rx_addr fn, rx_addr ret) {
    const uint32_t a[3] = {pool, rec, fn};
    const rx_addr s0 = fx_push(3, a, ret);
    rx_jobpool_submit(pool, rec, fn);
    fx_pop(s0);
}
/* the residue of a leaf that pushes `n` callee-saved registers right at entry (called at the current ESP, return address ret) */
static inline void fx_leaf_residue(rx_addr ret, int n, const int* regs) {
    const rx_addr sp = rx_sp(); SU32(sp, -4, ret); fx_push_regs(sp - 4, n, regs);
}
#define FX_LEAF_RESIDUE(ret, ...) fx_leaf_residue((ret), FX_REGS(__VA_ARGS__))
