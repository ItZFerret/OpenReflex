/* crt_util.h -- helpers shared by the CRT ports (core/game/crt_*.c: msvcr90.dll 9.0.30729.1 at 0x78520000, msvcp90.dll at
 * 0x78480000). Internal to the crt subsystem; other subsystems include crt.h. */
#pragma once
#include "rx_game.h"
#include "../src/rx_x87.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- paths not ported -----------------------------------------------------------------------------------------------------
 * A few paths of the CRT functions are never taken by the game (error handling of invalid arguments, NaN operands, unmasked x87
 * exceptions, ...). Where such a path starts, the port runs the ORIGINAL function from its entry on the same arguments instead
 * (the port has had no side effect up to that point). crt_bypass tells the function's own oracle adapter, re-entered by that call,
 * to let the original run (RUN_ORIGINAL). Product build: rx_call_original is a fatal error naming the function. The list of these
 * places is `grep -n crt_unported core/game/crt_*.c` (notes/crt.md). */
extern rx_addr crt_bypass;
#ifdef RX_ORACLE
extern const char* crt_why;               /* diagnostics: why the last fallback happened (printed by the adapters) */
#define CRT_WHY(s) (crt_why = (s))
#else
#define CRT_WHY(s) ((void)0)
#endif
/* The entry ESP of the running adapter (oracle; 0 elsewhere). A fallback taken directly by the adapter's function runs the
 * original in the original's own frame (its arguments and return slot re-pushed where they are), so that code which keeps
 * addresses of its stack frame (the demangler) stores the same values. */
#define crt_entry_sp (rx_cpu_cur()->crt_entry_sp)   /* the running guest thread's (rx_cpu, w-jobthreads) */
typedef struct { int on; uint32_t ret; } crt_replay;
static inline crt_replay crt_replay_begin(int nargs) {
    crt_replay r = {0, 0};
    const rx_addr sp = rx_sp();
    if (crt_entry_sp == 0 || sp != crt_entry_sp) return r;
    r.on = 1; r.ret = U32(sp, 0);                 /* the caller's return address: the call's own return slot overwrites it */
    rx_set_sp(sp + 4 + 4u * (uint32_t)nargs);
    return r;
}
static inline void crt_replay_end(crt_replay r, int nargs) {
    if (!r.on) return;
    rx_set_sp(rx_sp() - 4 - 4u * (uint32_t)nargs);
    SU32(rx_sp(), 0, r.ret);
}
#ifdef RX_ORACLE
extern unsigned crt_fallbacks;            /* diagnostics: how many times a port ran the original (the probes count declines) */
#define CRT_FALLBACK() (crt_fallbacks++)
#else
#define CRT_FALLBACK() ((void)0)
#endif
static inline uint32_t crt_unported(rx_addr fn, int nargs, const uint32_t* args) {
    CRT_FALLBACK();
    crt_bypass = fn;
    const crt_replay top = crt_replay_begin(nargs);
    const uint32_t r = rx_call_original(fn, RX_CDECL, 0, nargs, args);
    crt_replay_end(top, nargs);
    crt_bypass = 0;
    return r;
}
static inline void crt_unported_ex(rx_addr fn, const rx_regs* in, rx_regs* out, int nargs, const uint32_t* args) {
    CRT_FALLBACK();
    crt_bypass = fn;
    const crt_replay top = crt_replay_begin(nargs);
    rx_call_original_ex(fn, in, out, nargs, args);
    crt_replay_end(top, nargs);
    crt_bypass = 0;
}
static inline uint32_t crt_unported_this(rx_addr fn, uint32_t self, int nargs, const uint32_t* args) {
    CRT_FALLBACK();
    crt_bypass = fn;
    const crt_replay top = crt_replay_begin(nargs);
    const uint32_t r = rx_call_original(fn, RX_THISCALL, self, nargs, args);
    crt_replay_end(top, nargs);
    crt_bypass = 0;
    return r;
}
/* ---- the original's stack frames ------------------------------------------------------------------------------------------
 * Some game code reads uninitialized locals, i.e. what the last deeper call left on the stack (a particle template copied
 * into the particle pool picks up memcpy's locals). The hot CRT functions therefore also write their frames as the original
 * does (crt_string.c). e = the address of the call's return slot: the adapter's entry ESP when called from original code, else
 * rx_sp() - 4 - 4 * nargs (a core caller keeps rx_sp() where the original's ESP was before it pushed the arguments); calls
 * nested inside another CRT port write nothing (their callers' frames are not reproduced either). The caller's callee-saved
 * registers the frames save are crt_caller_regs (set by the adapters; 0 in the product, where callers have no registers). */
#ifndef crt_caller_regs
#define crt_caller_regs (rx_cpu_cur()->crt_caller_regs)   /* EAX ECX EDX EBX ESP EBP ESI EDI at the call (the running guest thread's: rx_cpu) */
#endif
/* A direct call from core code to a CRT port with the callee-saved registers the original caller has at that call (what the CRT
 * frames save): CRT_WITH_REGS(ebx, ebp, esi, edi, r = crt_str_less(a, b)); rx_sp() must be the original's ESP before its pushes. */
#define CRT_WITH_REGS(ebx_, ebp_, esi_, edi_, call) do { \
    const uint32_t crt_saved_[4] = {crt_caller_regs[3], crt_caller_regs[5], crt_caller_regs[6], crt_caller_regs[7]}; \
    crt_caller_regs[3] = (ebx_); crt_caller_regs[5] = (ebp_); crt_caller_regs[6] = (esi_); crt_caller_regs[7] = (edi_); \
    call; \
    crt_caller_regs[3] = crt_saved_[0]; crt_caller_regs[5] = crt_saved_[1]; crt_caller_regs[6] = crt_saved_[2]; crt_caller_regs[7] = crt_saved_[3]; \
} while (0)
static inline rx_addr crt_frame_e(int nargs) {
    const rx_addr sp = rx_sp();
    if (crt_entry_sp == 0) return sp - 4 - 4u * (uint32_t)nargs;
    return sp == crt_entry_sp ? sp : 0;
}
#ifdef RX_ORACLE
uint32_t crt_decode_ptr(uint32_t p);       /* 78543607 _decode_pointer (crt_thread.c; the oracle's encoded CRT pointers) */
#endif
/* an x87 function on the original: fpu_in values pushed (fpu[0] first), the result st(0) back */
rx_f80 crt_unported_x87(rx_addr fn, int n_in, const rx_f80* in);

/* ---- floating point -------------------------------------------------------------------------------------------------------- */
#define CRT_SSE2 0x785bd7b0u   /* msvcr90's __sse2_available (set by its startup when CPUID reports SSE2) */

/* An 80-bit value as the x87 loads it from 10 bytes (fld tbyte): mantissa + 15-bit biased exponent + sign. */
static inline rx_f80 crt_f80_raw(uint64_t mant, uint16_t se) {
    rx_f80 r; r.sign = (uint8_t)(se >> 15); const int e = se & 0x7fff;
    if (e == 0x7fff) { r.kind = (mant << 1) ? 2 : 1; r.mant = 0; r.exp = 0; return r; }
    r.kind = 0;
    if (mant == 0) { r.mant = 0; r.exp = 0; return r; }
    int32_t ex = e - 16383; if (e == 0) ex = -16382;
    while (!(mant >> 63)) { mant <<= 1; ex--; }
    r.mant = mant; r.exp = ex; return r;
}
static inline rx_f80 crt_f80_neg(rx_f80 a) { a.sign ^= 1; return a; }
static inline rx_f80 crt_f80_abs(rx_f80 a) { a.sign = 0; return a; }
/* fxam's class as the emulator reports it (no denormals in its value model): 0 number, 1 zero, 2 NaN, 3 infinity */
static inline int crt_f80_class(rx_f80 a) { return a.kind == 2 ? 2 : a.kind == 1 ? 3 : a.mant == 0 ? 1 : 0; }
static inline int crt_isnan64(uint64_t u) { return (u & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }

#ifdef __cplusplus
}
#endif
