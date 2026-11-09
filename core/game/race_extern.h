/* race_extern.h -- the race code's calls into functions it does not own (physics / math = rx_phys, world = rx_world, bike = rx_bike,
 * animation = rx_anim, game loop / players / input / cameras = rx_loop, engine / UI / sound) that are not in the core yet: each one
 * runs the original through rx_call_original (oracle build) until its owner ports it. One wrapper per callee, named after what it
 * does, so the call sites read like the game code and this file is the list of what the race code still borrows. */
#pragma once
#include "rx_game.h"
#include "race.h"
#include "crt.h"

/* A direct call of a core function as if the original had pushed `arg_bytes` of arguments and its return address. */
#define RACE_CALL_SP(arg_bytes, call) do { rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - (arg_bytes) - 4); call; rx_set_sp(sp_); } while (0)

/* Virtual dispatch: the function at vtable slot `slot` of `obj`. */
static inline rx_addr race_vfn(rx_addr obj, uint32_t slot) { return U32(U32(obj, 0), 4u * slot); }

/* A thiscall with up to 4 dword arguments to a function that is not (or not necessarily) in the core. */
static inline uint32_t race_thiscall(rx_addr fn, rx_addr self, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t a[4] = {a0, a1, a2, a3};
#ifndef RX_ORACLE   /* the product has no original to run: a function the core has registered at that address (a virtual's target) is called as the original would be (n pushed arguments) */
    void* f = RX_FN_INTS(fn, n + 1);
    if (f) {
        const rx_addr sp = rx_sp();
        uint32_t r = 0;
        rx_set_sp(sp - 4u * (uint32_t)n - 4u);
        switch (n) {
        case 0: r = ((uint32_t (*)(rx_addr))f)(self); break;
        case 1: r = ((uint32_t (*)(rx_addr, uint32_t))f)(self, a0); break;
        case 2: r = ((uint32_t (*)(rx_addr, uint32_t, uint32_t))f)(self, a0, a1); break;
        case 3: r = ((uint32_t (*)(rx_addr, uint32_t, uint32_t, uint32_t))f)(self, a0, a1, a2); break;
        default: r = ((uint32_t (*)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t))f)(self, a0, a1, a2, a3); break;
        }
        rx_set_sp(sp);
        return r;
    }
#endif
    return rx_call_original(fn, RX_THISCALL, self, n, a);
}

/* CRT pow (msvcr90 __libm_sse2_pow: xmm0 = base, xmm1 = exponent, doubles; result xmm0): the core's CRT port (crt.h). */
static inline double race_pow(double base, double exponent) { return crt_libm_sse2_pow(base, exponent); }

/* x87 at the main thread's precision (control word 0x027f: 53-bit mantissa, round to nearest): a chain of float loads, fmul and
 * fadd stored as a float equals the same chain in double arithmetic rounded once to float (the values here never leave double's
 * exponent range). Used for the original's `fld; fmul st0, st0; ... faddp; fstp` length^2 sums. */
static inline float race_x87_sumsq3(float a, float b, float c) {
    return (float)(((double)a * a + (double)b * b) + (double)c * c);
}

/* A 16-byte aligned frame on the game stack (the original's `and esp, -16` frames: callees store vectors with movaps). */
#define RACE_FRAME16_ENTER(F, size) const rx_addr F##_sp = rx_sp(); const rx_addr F = (F##_sp - (size)) & ~15u; rx_set_sp(F)
#define RACE_FRAME16_LEAVE(F) rx_set_sp(F##_sp)

/* The engine's profiler scope (inlined in the drivers' and the game's updates): with the stats system on (0xda6d08) the scope of
 * stat 3 (FUN_00402420 on 0x9df584; the shared vf 007a0760 that names it returns 0), else the default 0xda6cf0; entered and left
 * through FUN_00883d80 / 00883d90 while profiling is not disabled (byte 0x9df580). */
static inline rx_addr race_profile_begin(void) {
    rx_addr scope = 0x00da6cf0;
    if (U32(0, 0xda6d08) != 0) {
        uint32_t a[2] = {3, 0};
        rx_addr st = rx_call_original(0x00402420, RX_THISCALL, 0x009df584, 2, a);
        scope = (st != 0 && U32(st, 0xc) != 0) ? U32(st, 8) + 0x3c : 0x3c;
    }
    if (U8(0, 0x9df580) == 0) {
        void (*f)(rx_addr) = RX_FN(0x00883d80, void (*)(rx_addr));   /* the lock enter (loop_player.c) now that it is ported */
        if (f) RACE_CALL_SP(0, f(scope)); else rx_call_original(0x00883d80, RX_THISCALL, scope, 0, 0);
    }
    return scope;
}
static inline void race_profile_end(rx_addr scope) {
    if (U8(0, 0x9df580) == 0) {
        void (*f)(rx_addr) = RX_FN(0x00883d90, void (*)(rx_addr));   /* the lock leave (loop_player.c) now that it is ported */
        if (f) RACE_CALL_SP(0, f(scope)); else rx_call_original(0x00883d90, RX_THISCALL, scope, 0, 0);
    }
}

/* CRT rand() (msvcr90 through the IAT 0x91624c): the C library's sequence, separate from the game's own LCG (race_rand). */
static inline int32_t race_crt_rand(void) { return crt_rand(); }   /* rand (IAT 0x91624c) */

/* The original's frame for functions whose callees keep stack addresses (heap records pointing into the caller's frame): the
 * stack pointer as the original has it after `push ebp; mov ebp, esp; and esp, -16; sub esp, sub; push x n` (entered through the
 * adapter, rx_sp() is the original's entry ESP), and the entry ESP a core callee would have been called with (n dword arguments
 * and the return address below the current stack pointer). */
#define RACE_ORIG_FRAME_ENTER(F, sub, pushes) const rx_addr F##_sp = rx_sp(); \
    const rx_addr F = ((F##_sp - 4) & ~15u) - (sub) - 4u * (pushes); rx_set_sp(F)
#define RACE_ORIG_FRAME_LEAVE(F) rx_set_sp(F##_sp)
#define RACE_CORE_CALL(nargs, call) do { const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 4u * (nargs) - 4u); call; rx_set_sp(sp_); } while (0)
/* The same for `push ebp; mov ebp, esp; and esp, -align; sub esp, sub; push x n` frames with other alignments (8), and for frames
 * without a frame pointer (`sub esp, sub; push x n`). */
#define RACE_FRAME_EBP(F, align, sub, pushes) const rx_addr F##_sp = rx_sp(); \
    const rx_addr F = ((F##_sp - 4) & ~((rx_addr)(align) - 1)) - (sub) - 4u * (pushes); rx_set_sp(F)
#define RACE_FRAME_PLAIN(F, sub, pushes) const rx_addr F##_sp = rx_sp(); \
    const rx_addr F = F##_sp - (sub) - 4u * (pushes); rx_set_sp(F)
#define RACE_FRAME_END(F) rx_set_sp(F##_sp)
