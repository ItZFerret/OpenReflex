/* loop_util.h -- helpers shared by the game loop subsystem (core/game/loop_*.c: frame, game object updates, players, input,
 * cameras): engine globals, typed calls of original function addresses (the core implementation when one is registered, the
 * original otherwise), imports. Notes: core/notes/loop.md.
 *
 * Calling convention of the core functions (so that a call through an original address can use the registered core function):
 * the C signature mirrors the original ABI: `this` first for thiscall, then the stack arguments in order (int / pointer ->
 * uint32_t or rx_addr, float -> float), register arguments of LTCG functions after them in the order the comment names; the
 * result as the original returns it (EAX -> uint32_t, st(0) / xmm0 -> float). */
#pragma once
#include "rx_game.h"

/* ---- globals ---- */
#define LOOP_G_ENGINE_CONTEXT 0x00a95844u   /* g_EngineContext: the engine service context (XenonCore, vtable 009763a4) */
static inline rx_addr loop_ctx(void) { return U32(0, LOOP_G_ENGINE_CONTEXT); }
/* the frame timing object (context + 0x10 -> g_EngineFrameTiming 00d728d0): +0x18 dt, +0x3c time, +0x40 scaled time, +0x48 clamp */
static inline rx_addr loop_timing(void) { return U32(loop_ctx(), 0x10); }

/* a virtual slot: the function address at byte offset `off` of obj's vtable */
static inline rx_addr loop_vslot(rx_addr obj, uint32_t off) { return U32(U32(obj, 0), off); }

/* ---- direct calls of core functions at the original's stack depth ----
 * The original's `push args; call f` puts the arguments and the return address below ESP; a core function lays out its frame
 * from rx_sp() (the original's ESP at entry, pointing at the return address), and what it calls (original code that keeps stack
 * addresses, dead-stack readers) sees the same addresses only if the caller pushed the same bytes. loop_push writes the n stack
 * arguments and the return address `ret` (the original's call-site return address where known, else 0) and moves the stack
 * pointer; loop_pop(sp0) restores it. rx_call_original does the same pushes itself. */
static inline rx_addr loop_push(int n, const uint32_t* a, rx_addr ret) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * (uint32_t)(n + 1);
    SU32(sp, 0, ret);
    for (int i = 0; i < n; i++) SU32(sp, 4u + 4u * (uint32_t)i, a[i]);
    rx_set_sp(sp);
    return sp0;
}
static inline void loop_pop(rx_addr sp0) { rx_set_sp(sp0); }

/* ---- typed calls of an original address (thiscall: `self` in ECX) ----
 * Naming: loop_tc<n>_<result>: n stack arguments, all 32-bit integers / addresses; _f suffix variants take or return floats.
 * The registered core function is called at the original's stack depth (loop_push), else the original runs. */
static inline uint32_t loop_tc0(rx_addr fn, rx_addr self) {
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    if (!f) return rx_call_original(fn, RX_THISCALL, self, 0, 0);
    const rx_addr sp0 = loop_push(0, 0, 0);
    const uint32_t r = f(self);
    loop_pop(sp0);
    return r;
}
/* loop_tc0 with the call site's return address */
static inline uint32_t loop_tc0r(rx_addr fn, rx_addr self, rx_addr ret) {
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    if (!f) return rx_call_original(fn, RX_THISCALL, self, 0, 0);
    const rx_addr sp0 = loop_push(0, 0, ret);
    const uint32_t r = f(self);
    loop_pop(sp0);
    return r;
}
static inline uint32_t loop_tc1(rx_addr fn, rx_addr self, uint32_t a) {
    uint32_t (*f)(rx_addr, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t));
    uint32_t v[1] = {a};
    if (!f) return rx_call_original(fn, RX_THISCALL, self, 1, v);
    const rx_addr sp0 = loop_push(1, v, 0);
    const uint32_t r = f(self, a);
    loop_pop(sp0);
    return r;
}
static inline uint32_t loop_tc2(rx_addr fn, rx_addr self, uint32_t a, uint32_t b) {
    uint32_t (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t));
    uint32_t v[2] = {a, b};
    if (!f) return rx_call_original(fn, RX_THISCALL, self, 2, v);
    const rx_addr sp0 = loop_push(2, v, 0);
    const uint32_t r = f(self, a, b);
    loop_pop(sp0);
    return r;
}
static inline uint32_t loop_tc3(rx_addr fn, rx_addr self, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t (*f)(rx_addr, uint32_t, uint32_t, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t, uint32_t));
    uint32_t v[3] = {a, b, c};
    if (!f) return rx_call_original(fn, RX_THISCALL, self, 3, v);
    const rx_addr sp0 = loop_push(3, v, 0);
    const uint32_t r = f(self, a, b, c);
    loop_pop(sp0);
    return r;
}
static inline uint32_t loop_tc5(rx_addr fn, rx_addr self, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    uint32_t (*f)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) =
        RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t));
    uint32_t v[5] = {a, b, c, d, e};
    if (!f) return rx_call_original(fn, RX_THISCALL, self, 5, v);
    const rx_addr sp0 = loop_push(5, v, 0);
    const uint32_t r = f(self, a, b, c, d, e);
    loop_pop(sp0);
    return r;
}
/* thiscall, one float argument, no result read (entity / camera vf19-style updates: void f(this, float dt)) */
static inline void loop_tc_f(rx_addr fn, rx_addr self, float x) {
    void (*f)(rx_addr, float) = RX_FN(fn, void (*)(rx_addr, float));
    uint32_t v[1] = {rx_fb(x)};
    if (!f) { rx_call_original(fn, RX_THISCALL, self, 1, v); return; }
    const rx_addr sp0 = loop_push(1, v, 0);
    f(self, x);
    loop_pop(sp0);
}
/* thiscall (uint32_t, float), no result read */
static inline void loop_tc_uf(rx_addr fn, rx_addr self, uint32_t a, float x) {
    void (*f)(rx_addr, uint32_t, float) = RX_FN(fn, void (*)(rx_addr, uint32_t, float));
    uint32_t v[2] = {a, rx_fb(x)};
    if (!f) { rx_call_original(fn, RX_THISCALL, self, 2, v); return; }
    const rx_addr sp0 = loop_push(2, v, 0);
    f(self, a, x);
    loop_pop(sp0);
}

/* thiscall, 2 integer arguments, float result in st(0) */
static inline float loop_tc2_x87(rx_addr fn, rx_addr self, uint32_t a, uint32_t b) {
    float (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, float (*)(rx_addr, uint32_t, uint32_t));
    uint32_t v[2] = {a, b};
    if (f) {
        const rx_addr sp0 = loop_push(2, v, 0);
        const float r = f(self, a, b);
        loop_pop(sp0);
        return r;
    }
    rx_regs in, out; memset(&in, 0, sizeof in); in.r[1] = self;
    rx_call_original_ex(fn, &in, &out, 2, v);   /* takes the x87 result off the emulated stack */
    return out.st0;
}

/* Original functions with register arguments (LTCG): rx_regs indices and a zeroed register set. */
enum { LOOP_EAX, LOOP_ECX, LOOP_EDX, LOOP_EBX, LOOP_ESP, LOOP_EBP, LOOP_ESI, LOOP_EDI };
static inline rx_regs loop_regs(void) { rx_regs r; memset(&r, 0, sizeof r); return r; }
/* the original at fn with registers `in` and n stack arguments -> EAX */
static inline uint32_t loop_ex(rx_addr fn, rx_regs in, int n, const uint32_t* args) {
    rx_regs out;
    rx_call_original_ex(fn, &in, &out, n, args);
    return out.r[LOOP_EAX];
}

/* the engine context's input manager (XenonCore slot 2, 008439f0: context + 0x34; XenonInputManager, vtable 00974e0c) */
static inline rx_addr loop_input_manager(void) { return loop_tc0(loop_vslot(loop_ctx(), 0x8), loop_ctx()); }
