/* rx_mem.h -- the game's memory model in the OPEN REFLEX core (the Open Unleashed way: game objects keep MXReflex.exe's exact
 * 32-bit layout and are accessed by offset).
 *
 * A game address (rx_addr) is the original 32-bit address. Two backends, chosen at compile time:
 *  - product (default): one flat arena; address a lives at rx_base + a (rx_mem.c reserves it). No emulator anywhere.
 *  - RX_ORACLE: the same sources compiled into the emulator's test CLI (core/oracle): every access goes to the emulated
 *    machine's journaled memory, so each core function is checked call by call against the original code (native/game Verify).
 * Write game code ONLY through these accessors (never raw pointers into game memory), so both builds run the same code. */
#pragma once
#include <stdint.h>
#include <string.h>
#include "rx_x87.h"

typedef uint32_t rx_addr;
typedef struct { float v[4]; } rx_v4;   /* 4-float vector (an SSE register's lanes) */

#ifdef RX_ORACLE
uint8_t  rx_rd8(rx_addr a);   uint16_t rx_rd16(rx_addr a);  uint32_t rx_rd32(rx_addr a);  uint64_t rx_rd64(rx_addr a);
void rx_wr8(rx_addr a, uint8_t v); void rx_wr16(rx_addr a, uint16_t v); void rx_wr32(rx_addr a, uint32_t v); void rx_wr64(rx_addr a, uint64_t v);
#else
extern uint8_t* rx_base;
#ifdef RX_WATCH
/* -DRX_WATCH (development only, never a normal build; w-jobthreads3): every accessor reports to the driver's rx_watch_access
 * while rx_watch_on is set (the dependence checks of a deferred job: core/plan/QUEUE.md w-jobthreads3) */
extern int rx_watch_on;
void rx_watch_access(rx_addr a, uint32_t n, int write);
#define RX_WATCH_RD(a, n) do { if (rx_watch_on) rx_watch_access((a), (n), 0); } while (0)
#define RX_WATCH_WR(a, n) do { if (rx_watch_on) rx_watch_access((a), (n), 1); } while (0)
#else
#define RX_WATCH_RD(a, n) ((void)0)
#define RX_WATCH_WR(a, n) ((void)0)
#endif
static inline uint8_t  rx_rd8(rx_addr a)  { RX_WATCH_RD(a, 1); return rx_base[a]; }
static inline uint16_t rx_rd16(rx_addr a) { RX_WATCH_RD(a, 2); uint16_t v; memcpy(&v, rx_base + a, 2); return v; }
static inline uint32_t rx_rd32(rx_addr a) { RX_WATCH_RD(a, 4); uint32_t v; memcpy(&v, rx_base + a, 4); return v; }
static inline uint64_t rx_rd64(rx_addr a) { RX_WATCH_RD(a, 8); uint64_t v; memcpy(&v, rx_base + a, 8); return v; }
static inline void rx_wr8(rx_addr a, uint8_t v)   { RX_WATCH_WR(a, 1); rx_base[a] = v; }
static inline void rx_wr16(rx_addr a, uint16_t v) { RX_WATCH_WR(a, 2); memcpy(rx_base + a, &v, 2); }
static inline void rx_wr32(rx_addr a, uint32_t v) { RX_WATCH_WR(a, 4); memcpy(rx_base + a, &v, 4); }
static inline void rx_wr64(rx_addr a, uint64_t v) { RX_WATCH_WR(a, 8); memcpy(rx_base + a, &v, 8); }
#endif

static inline float    rx_bf(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static inline uint32_t rx_fb(float f)    { uint32_t b; memcpy(&b, &f, 4); return b; }
static inline double   rx_bd(uint64_t b) { double d; memcpy(&d, &b, 8); return d; }
static inline uint64_t rx_db(double d)   { uint64_t b; memcpy(&b, &d, 8); return b; }

/* Field access by offset, as Open Unleashed's port does: F32(obj, 0xf0) reads, SF32(obj, 0xf0, v) writes. */
#define U8(p, o)       rx_rd8((rx_addr)(p) + (o))
#define U16(p, o)      rx_rd16((rx_addr)(p) + (o))
#define U32(p, o)      rx_rd32((rx_addr)(p) + (o))
#define I32(p, o)      ((int32_t)rx_rd32((rx_addr)(p) + (o)))
#define F32(p, o)      rx_bf(rx_rd32((rx_addr)(p) + (o)))
#define F64(p, o)      rx_bd(rx_rd64((rx_addr)(p) + (o)))
#define SU8(p, o, v)   rx_wr8((rx_addr)(p) + (o), (uint8_t)(v))
#define SU16(p, o, v)  rx_wr16((rx_addr)(p) + (o), (uint16_t)(v))
#define SU32(p, o, v)  rx_wr32((rx_addr)(p) + (o), (uint32_t)(v))
#define SF32(p, o, v)  rx_wr32((rx_addr)(p) + (o), rx_fb(v))
#define SF64(p, o, v)  rx_wr64((rx_addr)(p) + (o), rx_db(v))

/* n bytes from s to d with memmove's result (the bytes as they were before the move, overlap or not). Product: one host memmove
 * in the arena (a range that wraps the 32-bit space goes byte by byte); oracle: byte by byte through the accessors. */
static inline void rx_move(rx_addr d, rx_addr s, uint32_t n) {
#ifndef RX_ORACLE
    if (d + n >= d && s + n >= s) { RX_WATCH_RD(s, n); RX_WATCH_WR(d, n); memmove(rx_base + d, rx_base + s, n); return; }
#endif
    if (d > s && d - s < n) { for (uint32_t i = n; i-- > 0;) rx_wr8(d + i, rx_rd8(s + i)); }
    else for (uint32_t i = 0; i < n; i++) rx_wr8(d + i, rx_rd8(s + i));
}

/* (rx_v4 is declared above) 4-float vectors (an SSE register's lanes; loads / stores of 16 bytes at a game address). */
static inline rx_v4 rx_ld4(rx_addr a) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_bf(rx_rd32(a + 4u * (uint32_t)i)); return r; }
static inline void rx_st4(rx_addr a, rx_v4 x) { for (int i = 0; i < 4; i++) rx_wr32(a + 4u * (uint32_t)i, rx_fb(x.v[i])); }

/* Calling original code that is not ported yet. Oracle build: runs it in the emulator (development only). Product build: a
 * fatal error naming the address (rx_mem.c) -- the product must not reach unported code; coverage finds it first. */
enum rx_conv { RX_CDECL, RX_STDCALL, RX_THISCALL };
/* Product build: an unported stop writes its text here (this thread) and calls rx_stop_hook (the session API's guard: it jumps back
 * to the guarded call; it returns when none runs on this thread) before abort(). */
extern _Thread_local char rx_stop_note[160];
extern void (*rx_stop_hook)(void);
uint32_t rx_call_original(rx_addr fn, enum rx_conv conv, uint32_t ecx, int nargs, const uint32_t* args);
/* Any convention (LTCG passes arguments in EAX / EDI / ESI / XMM registers): registers in, registers out (EAX ECX EDX EBX ESP EBP
 * ESI EDI as the original's r[0..7]; xmm as 4 floats; x87 st(0) after the call as a float, with its depth). */
/* x87: fpu_in values are pushed before the call (fpu[0] first, so the last one is st(0)); on return the callee's x87 results are
 * taken off the emulated stack (depth restored to the entry depth): fpu_depth = how many it left, st[0..] = st(0), st(1) ...,
 * st0 = st[0] as a float. */
typedef struct { uint32_t r[8]; rx_v4 xmm[8]; int fpu_in; rx_f80 fpu[8]; int fpu_depth; rx_f80 st[8]; float st0; } rx_regs;
void rx_call_original_ex(rx_addr fn, const rx_regs* in, rx_regs* out, int nargs, const uint32_t* args);
/* An original cdecl / stdcall callee entered with EBX / EBP / ESI / EDI = the given values (EAX / ECX / EDX 0) as the original's caller
 * holds them (its frames save them on the stack): the machine's callee-saved registers are put back afterwards. -> EAX */
uint32_t rx_call_original_regs(rx_addr fn, uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi, int nargs, const uint32_t* args);

/* The game stack (guest ESP). Locals the original passes by address to other functions (contact sets, boxes, vectors) must live
 * at game addresses: reserve them below the stack pointer, exactly like the original's frame:
 *     rx_addr F = rx_sp() - 0x7a4; rx_set_sp(F);  ... use F + offsets ...  rx_set_sp(F + 0x7a4);
 * (or RX_FRAME_ENTER(F, 0x7a4) ... RX_FRAME_LEAVE(F, 0x7a4)). Oracle: the emulated ESP. Product: the main thread's stack region
 * 0x7fec0000..0x7ff00000 (the original's addresses), top 0x7ff00000. rx_call_original runs below the current stack pointer. */

/* The guest thread the running host code stands for (w-jobthreads, core/game/loop_jobs.h): the machine state the core keeps in host
 * memory per original thread. sp: the product's stack pointer (rx_sp); teb: the thread's TEB (fs:[0] the SEH head, fs:[0x2c] the
 * TLS array); the register residue models of the subsystems that keep one (anim_node.c, fx_particle.c, crt_util.h). The game
 * thread's is rx_cpu_main; a worker job's (loop_jobs.c) is the job's own, on whichever host thread runs it. */
typedef struct rx_cpu {
    rx_addr sp;                 /* in a parallel batch only (else rx_g_sp below) */
    rx_addr teb;
    uint32_t anim_regs[8];
    uint32_t fx_regs[8];
    uint32_t crt_caller_regs[8];
    rx_addr crt_entry_sp;
    int32_t job, cs_depth;      /* loop_jobs.c: the job's index in its parallel batch (-1: none), its lock depth */
} rx_cpu;
/* The current one: rx_cpu_main while no parallel batch runs (the game thread's, or a job's run one after another: swapped in); in a parallel
 * batch (rx_cpu_par) each host thread's own (rx_cpu_tls, set for the job it runs). No thread-local access outside a batch. */
#ifdef __cplusplus
extern "C" rx_cpu* rx_cpu_cur_fn(void);                            /* (C++: no extern thread_local wrapper) */
static inline rx_cpu* rx_cpu_cur(void) { return rx_cpu_cur_fn(); }
#else
extern rx_cpu rx_cpu_main;
extern int rx_cpu_par;
rx_cpu* rx_cpu_tls_get(void);
static inline rx_cpu* rx_cpu_cur(void) { return __builtin_expect(rx_cpu_par, 0) ? rx_cpu_tls_get() : &rx_cpu_main; }
rx_cpu* rx_cpu_cur_fn(void);
#endif
#define RX_MAIN_TEB 0x7ffd0000u
static inline rx_addr rx_teb(void) { return rx_cpu_cur()->teb; }   /* the running guest thread's TEB (the game thread's: 0x7ffd0000) */

/* The guest stack pointer (ESP). Oracle: the emulated machine's (rx_sp / rx_set_sp in the oracle). Product: one word, inline
 * (w-winbench: ~22k call sites; out of line they were ~4% of a frame on the Mac and ~8% in the Windows build): rx_g_sp (src/rx_mem.c)
 * outside a parallel batch -- the game thread's, or a worker job's while it runs one after another (loop_jobs.c swaps it in and
 * out) -- and the host thread's own rx_cpu.sp inside one (w-jobthreads; the thread-local read only there). */
#if defined(RX_ORACLE) || defined(__cplusplus)
rx_addr rx_sp(void);
void rx_set_sp(rx_addr sp);
#else
extern rx_addr rx_g_sp;
static inline rx_addr rx_sp(void) { return __builtin_expect(rx_cpu_par, 0) ? rx_cpu_tls_get()->sp : rx_g_sp; }
static inline void rx_set_sp(rx_addr sp) { if (__builtin_expect(rx_cpu_par, 0)) rx_cpu_tls_get()->sp = sp; else rx_g_sp = sp; }
#endif
#define RX_FRAME_ENTER(F, size) rx_addr F = rx_sp() - (size); rx_set_sp(F)
#define RX_FRAME_LEAVE(F, size) rx_set_sp((F) + (size))

/* Product arena: reserve the 32-bit address space once (lazily committed by the OS). */
int  rx_mem_init(void);
void rx_mem_shutdown(void);
