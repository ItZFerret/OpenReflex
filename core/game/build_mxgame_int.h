/* build_mxgame_int.h -- internal helpers of unit 10 (build_mxgame*.c): frames at the original's stack addresses, kernel32 through the
 * exe's import slots (product: core/src/rx_os.c stand-ins; oracle: the original import), calls of core callees at the original's
 * depth. Not an API. */
#pragma once
#include "build_mxgame.h"
#include "build_alloc.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>
#include "../src/rx_os.h"

/* ---- frames: rx_sp() at a function's entry is the address of its return address (the original's ESP at entry) ---- */
/* `push r` x pushes, `sub esp, sub`: F = the original's ESP after the prologue */
#define MX_FRAME(F, sub, pushes) const rx_addr F##_sp = rx_sp(); const rx_addr F = F##_sp - (sub) - 4u * (pushes); rx_set_sp(F)
/* `push ebp; mov ebp, esp; and esp, -align; sub esp, sub; push x pushes` */
#define MX_FRAME_ALIGN(F, align, sub, pushes) const rx_addr F##_sp = rx_sp(); \
    const rx_addr F = ((F##_sp - 4) & ~((rx_addr)(align) - 1)) - (sub) - 4u * (pushes); rx_set_sp(F)
#define MX_END(F) rx_set_sp(F##_sp)
/* a core callee with n stack arguments entered as the original's call at the current ESP (the pushes, then the return address) */
#define MX_CALL(nargs, call) do { const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 4u * (nargs) - 4u); call; rx_set_sp(sp_); } while (0)
#define MX_CALLR(var, nargs, call) do { const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 4u * (nargs) - 4u); var = (call); rx_set_sp(sp_); } while (0)
/* with extra words pushed before the call (saved registers the original pushes between its prologue and the call) */
#define MX_AT(F, off) rx_set_sp((F) - (off))

static inline void mx_unreached(uint32_t addr, const char* what) {
    fprintf(stderr, "rxcore: build_mxgame: %08x %s: path not in the reference boot (not ported)\n", addr, what);
    abort();
}

/* ---- kernel32 through the exe's import table ---- */
static inline uint32_t mx_os(uint32_t slot, int n, const uint32_t* a) { return rx_os_import(slot, n, a); }   /* src/rx_os.h */
#define MX_IAT_INIT_CS      0x00916014u
#define MX_IAT_RESUME       0x0091602cu
#define MX_IAT_PRIORITY     0x00916024u
#define MX_IAT_AFFINITY     0x00916028u
#define MX_IAT_THREAD       0x00916030u
#define MX_IAT_EVENT        0x00916038u
#define MX_IAT_LAST_ERROR   0x00916078u
#define MX_IAT_ENTER_CS     0x0091600cu
#define MX_IAT_LEAVE_CS     0x009160f8u
#define MX_IAT_SET_EVENT    0x00916034u
#define MX_IAT_WAIT         0x00916008u
static inline void mx_cs_init(rx_addr cs) { uint32_t a[1] = {cs}; mx_os(MX_IAT_INIT_CS, 1, a); }
static inline void mx_cs_enter(rx_addr cs) { uint32_t a[1] = {cs}; mx_os(MX_IAT_ENTER_CS, 1, a); }
static inline void mx_cs_leave(rx_addr cs) { uint32_t a[1] = {cs}; mx_os(MX_IAT_LEAVE_CS, 1, a); }
/* CreateEventA(0, manual, initial, name) -> handle; GetLastError when it fails (the original's call, result unused) */
static inline uint32_t mx_event_create(uint32_t manual, uint32_t initial, rx_addr name) {
    uint32_t a[4] = {0, manual, initial, name};
    uint32_t h = mx_os(MX_IAT_EVENT, 4, a);
    if (!h) mx_os(MX_IAT_LAST_ERROR, 0, NULL);
    return h;
}
/* CreateThread(0, 0, fn, param, flags, &tid) */
static inline uint32_t mx_thread_create(rx_addr fn, uint32_t param, uint32_t flags, rx_addr tid) {
    uint32_t a[6] = {0, 0, fn, param, flags, tid};
    return mx_os(MX_IAT_THREAD, 6, a);
}
static inline void mx_thread_affinity(uint32_t h, uint32_t mask) { uint32_t a[2] = {h, mask}; mx_os(MX_IAT_AFFINITY, 2, a); }
static inline void mx_thread_priority(uint32_t h, uint32_t p) { uint32_t a[2] = {h, p}; mx_os(MX_IAT_PRIORITY, 2, a); }
static inline void mx_thread_resume(uint32_t h) { uint32_t a[1] = {h}; mx_os(MX_IAT_RESUME, 1, a); }

/* the allocator lock of ReflexAlignedAlloc's inline forms (critical section 0x00d61704 when [0x00da67d8] is set) around a direct
 * _aligned_malloc */
static inline rx_addr mx_aligned_malloc_locked(uint32_t size, uint32_t align) {
    if (U8(0, 0x00da67d8)) mx_cs_enter(0x00d61704);
    rx_addr p = rx_crt_aligned_malloc(size, align);
    if (U8(0, 0x00da67d8)) mx_cs_leave(0x00d61704);
    return p;
}

/* the inlined `new T[n]` size of an n-element array of 4-byte slots: n * 4 saturated (mul / seto / neg / or) */
static inline uint32_t mx_size4(uint32_t n) { return rx_mul_sat(n, 4); }

/* a word copy (rep movsd) */
static inline void mx_copy_words(rx_addr d, rx_addr s, uint32_t n) { for (uint32_t i = 0; i < n; i++) SU32(d, 4 * i, U32(s, 4 * i)); }

/* build_mxgame_init.c: an EventHandlerProxy on a channel's event (00419700 / 00755490 / 007604b0 ... share the shape) */
void rx_mx_handler_register(rx_addr ch, rx_addr name, uint32_t size, rx_addr vt, uint32_t w3, uint32_t w4, rx_addr td, uint32_t pushes);
struct fmt_install;
void rx_mxgame_init_set_install(struct fmt_install* in);
