/* rx_mem.c -- the product build's flat arena (see rx_mem.h). */
#ifndef RX_ORACLE
#include "rx_mem.h"
#include "rx_fn.h"
#include "rx_reset.h"
#include <stdio.h>
#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>
#endif
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

uint8_t* rx_base = NULL;
rx_addr rx_g_sp = 0x7ff00000u;   /* the game stack outside a parallel job batch (rx_mem.h: rx_sp / rx_set_sp) */
static const uint64_t kArena = 1ull << 32;

#ifdef _WIN32
/* Commit zero-filled arena pages on first access, like mmap's demand paging.
 * Do not swallow unrelated faults or execute attempts into game data. */
static void* g_arena_handler;
static LONG CALLBACK arena_page_fault(EXCEPTION_POINTERS* e) {
    if(e->ExceptionRecord->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION ||
       e->ExceptionRecord->NumberParameters<2 || e->ExceptionRecord->ExceptionInformation[0]==8)
        return EXCEPTION_CONTINUE_SEARCH;
    uintptr_t a=(uintptr_t)e->ExceptionRecord->ExceptionInformation[1],base=(uintptr_t)rx_base;
    if(!rx_base || a<base || (uint64_t)(a-base)>=kArena) return EXCEPTION_CONTINUE_SEARCH;
    MEMORY_BASIC_INFORMATION region;
    if(!VirtualQuery((void*)a,&region,sizeof region) || region.State!=MEM_RESERVE)
        return EXCEPTION_CONTINUE_SEARCH; /* already committed/protected: this is a real fault */
    /* w-winbench: commit the whole 64 KiB allocation granule around the fault (the arena base is granule-aligned, so the
     * granule is inside the arena). Each first-touch fault costs a kernel trap, the exception dispatcher, every vectored handler
     * ahead of this one (a host's, e.g. Unity's) and two syscalls; 16 pages per fault cut the event load's faults 16x. Committed
     * pages stay demand-zero (no physical memory until touched), so the contents are unchanged. One page when the granule
     * cannot be committed (the commit limit). */
    if(VirtualAlloc((void*)(a&~(uintptr_t)0xffff),0x10000,MEM_COMMIT,PAGE_READWRITE)) return EXCEPTION_CONTINUE_EXECUTION;
    return VirtualAlloc((void*)(a&~(uintptr_t)4095),4096,MEM_COMMIT,PAGE_READWRITE)
        ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}
#endif
int rx_mem_init(void) {
    if (rx_base) return 1;
#ifdef _WIN32
    rx_base = (uint8_t*)VirtualAlloc(NULL, (SIZE_T)kArena, MEM_RESERVE, PAGE_NOACCESS);   /* first-access commit below */
    if(rx_base){
        g_arena_handler=AddVectoredExceptionHandler(1,arena_page_fault);
        if(!g_arena_handler){VirtualFree(rx_base,0,MEM_RELEASE);rx_base=NULL;}
    }
#else
    void* p = mmap(NULL, (size_t)kArena, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    rx_base = p == MAP_FAILED ? NULL : (uint8_t*)p;
#endif
    return rx_base != NULL;
}

void rx_mem_shutdown(void) {
    if (!rx_base) return;
#ifdef _WIN32
    if(g_arena_handler){RemoveVectoredExceptionHandler(g_arena_handler);g_arena_handler=NULL;}
    VirtualFree(rx_base, 0, MEM_RELEASE);
#else
    munmap(rx_base, (size_t)kArena);
#endif
    rx_base = NULL;
}

/* RX_BACKTRACE=1: the host call stack at a fatal unported-function stop (development) */
static void rx_mem_backtrace(void) {
#if defined(__APPLE__) || defined(__linux__)
    if (!getenv("RX_BACKTRACE")) return;
    void* bt[24];
    const int n = backtrace(bt, 24);
    backtrace_symbols_fd(bt, n, 2);
#endif
}

/* The session API's fault guard (api/rx_session.c) on this thread: the stop's text for the call's error, and the guard's jump back
 * (rx_stop_hook returns when no guarded call runs on this thread: then abort() as before). Thread-local instead of a captured stderr
 * and SIGABRT: two core images in one process (the client's test-ride arena while an event loads, each image with its own arena
 * and session on its own thread) must not catch each other's stops; stderr and the signal dispositions are process-wide. */
_Thread_local char rx_stop_note[160];
void (*rx_stop_hook)(void) RX_RESET_KEEP;
static _Noreturn void rx_unported_stop(rx_addr fn) {
    fprintf(stderr, "rxcore: unported game function %08x reached\n", fn);
    snprintf(rx_stop_note, sizeof rx_stop_note, "rxcore: unported game function %08x reached", fn);
    rx_mem_backtrace();
    if (rx_stop_hook) rx_stop_hook();
    abort();
}

/* The registered functions with an integer-only signature (tools/gen_fnsig.py writes the table into every product build; weak: absent = none). */
extern const uint32_t rx_fnsig_int_addr[] __attribute__((weak));
extern const uint32_t rx_fnsig_int_count __attribute__((weak));
static int rx_fnsig_is_int(rx_addr fn) {
    if (!&rx_fnsig_int_count || !rx_fnsig_int_addr) return 0;
    uint32_t lo = 0, hi = rx_fnsig_int_count;
    while (lo < hi) { const uint32_t mid = (lo + hi) / 2; if (rx_fnsig_int_addr[mid] < fn) lo = mid + 1; else hi = mid; }
    return lo < rx_fnsig_int_count && rx_fnsig_int_addr[lo] == fn;
}

/* The product has no original to run. A function the core has registered at `fn` whose C signature is integers only (the table above) is called as the original would be:
 * the convention's registers and stack arguments are the C parameters in order (thiscall: ECX first; at most 6 stack arguments), the stack pointer is lowered by the
 * pushed arguments and the return address (the arguments stay in the dead stack), the result is the integer return (0 for void). Anything else is fatal, naming
 * the address (coverage tells what is left). */
uint32_t rx_call_original(rx_addr fn, enum rx_conv conv, uint32_t ecx, int nargs, const uint32_t* args) {
    if (nargs >= 0 && nargs <= 6 && rx_fnsig_is_int(fn)) {
        void* f = rx_fn(fn);
        if (f) {
            const rx_addr sp = rx_sp();
            uint32_t v[8] = {0, 0, 0, 0, 0, 0, 0, 0};
            int k = 0;
            if (conv == RX_THISCALL) v[k++] = ecx;
            for (int i = 0; i < nargs; i++) { v[k++] = args[i]; rx_wr32(sp - 4u * (uint32_t)nargs + 4u * (uint32_t)i, args[i]); }
            rx_set_sp(sp - 4u * (uint32_t)nargs - 4u);
            const uint32_t r = ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))f)(v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
            rx_set_sp(sp);
            return r;
        }
    }
    rx_unported_stop(fn);
}
uint32_t rx_call_original_regs(rx_addr fn, uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi, int nargs, const uint32_t* args) {
    (void)ebx; (void)ebp; (void)esi; (void)edi; (void)nargs; (void)args;
    rx_unported_stop(fn);
}
void rx_call_original_ex(rx_addr fn, const rx_regs* in, rx_regs* out, int nargs, const uint32_t* args) {
    (void)in; (void)out; (void)nargs; (void)args;
    rx_unported_stop(fn);
}
#endif
