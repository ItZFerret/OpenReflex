/* rx_vcall.h -- virtual / indirect thiscalls to functions that are ported (sweep A): the core function registered at the target
 * address when there is one, else the original (oracle build; in the product the fatal error naming the address = an unported
 * slot). Integer arguments only (a core function with float parameters is called through its own typed pointer at the call site).
 * The original's pushes are mimicked: the stack pointer drops by 4 * n + 4 (arguments and the return address) around the call, so
 * the callee's frame lands at the original's addresses. Core virtual functions take (rx_addr this, <stack arguments in order>) and
 * return the EAX value (README "Calls"). */
#pragma once
#include "rx_game.h"

#ifdef RX_FN_TRACE
/* -DRX_FN_TRACE (rx_fn.h): the call site (the macro below) and the argument count go into the trace; the body is unchanged */
#define rx_tcall(target, obj, n, ...) rx_tcall_t((target), (obj), (n), __VA_ARGS__, __FILE__, __LINE__)   /* variadic: the argument is often a compound literal `(uint32_t[]){a, b}` */
#define rx_vcall(obj, off, n, ...) rx_vcall_t((obj), (off), (n), __VA_ARGS__, __FILE__, __LINE__)
static inline uint32_t rx_tcall_t(rx_addr target, rx_addr obj, int n, const uint32_t* a, const char* tr_file, int tr_line) {
    static const char* const tr_name[] = {"rx_tcall/0", "rx_tcall/1", "rx_tcall/2", "rx_tcall/3", "rx_tcall/4", "rx_tcall/5", "rx_tcall/N"};
    (void)rx_fn_traced(target, tr_file, tr_line, tr_name[n >= 0 && n <= 5 ? n : 6]);
#else
static inline uint32_t rx_tcall(rx_addr target, rx_addr obj, int n, const uint32_t* a) {
#endif
    typedef uint32_t (*f0_t)(rx_addr);
    typedef uint32_t (*f1_t)(rx_addr, uint32_t);
    typedef uint32_t (*f2_t)(rx_addr, uint32_t, uint32_t);
    typedef uint32_t (*f3_t)(rx_addr, uint32_t, uint32_t, uint32_t);
    typedef uint32_t (*f4_t)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t);
    typedef uint32_t (*f5_t)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
#ifdef RX_FN_TRACE
    void* f = (rx_fn)(target);
#else
    void* f = rx_fn(target);
#endif
    if (!f) return rx_call_original(target, RX_THISCALL, obj, n, a);
    const rx_addr sp = rx_sp();
    rx_set_sp(sp - 4u * (uint32_t)n - 4u);
    for (int i = 0; i < n; i++) SU32(sp - 4u * (uint32_t)n, 4u * (uint32_t)i, a[i]);   /* the pushed arguments stay in the dead stack */
    uint32_t r = 0;
    switch (n) {
    case 0: r = ((f0_t)f)(obj); break;
    case 1: r = ((f1_t)f)(obj, a[0]); break;
    case 2: r = ((f2_t)f)(obj, a[0], a[1]); break;
    case 3: r = ((f3_t)f)(obj, a[0], a[1], a[2]); break;
    case 4: r = ((f4_t)f)(obj, a[0], a[1], a[2], a[3]); break;
    case 5: r = ((f5_t)f)(obj, a[0], a[1], a[2], a[3], a[4]); break;
    default: rx_set_sp(sp); return rx_call_original(target, RX_THISCALL, obj, n, a);
    }
    rx_set_sp(sp);
    return r;
}
/* the function at vtable byte offset `off` of obj */
#ifdef RX_FN_TRACE
static inline uint32_t rx_vcall_t(rx_addr obj, uint32_t off, int n, const uint32_t* a, const char* tr_file, int tr_line) {
    return rx_tcall_t(U32(U32(obj, 0), off), obj, n, a, tr_file, tr_line);
}
#else
static inline uint32_t rx_vcall(rx_addr obj, uint32_t off, int n, const uint32_t* a) {
    return rx_tcall(U32(U32(obj, 0), off), obj, n, a);
}
#endif
