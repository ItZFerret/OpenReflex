/* crt_thread.c -- msvcr90's per-thread data (_tiddata, 0x214 bytes): _getptd 785438bd / _getptd_noexit 7854383f, created on a
 * thread's first use with _calloc_crt 78583437 (_calloc_impl 78586460) and _initptd 78543753 (+ __addlocaleref 7858d9b6, the
 * _lock / _unlock 78542efa / 78542e16 pairs 13 and 12), _decode_pointer 78543607, __set_flsgetvalue 785436b6.
 *
 * The data pointer lives in the OS's thread / fiber local storage. The functions make the original's kernel32 calls, through
 * msvcr90's import slots (they are OS functions, outside the game: the oracle runs the emulator's stand-ins, the product needs
 * an OS layer that provides them): GetLastError 78521054 / SetLastError 78521070 around everything, TlsGetValue 78521030 (slot
 * [785b71e4] holds the "FlsGetValue" function, FlsGetValue / FlsSetValue themselves being absent), the FLS getter / setter,
 * GetModuleHandleW 78521008, GetProcAddress 78521010, GetCurrentThreadId 7852102c, InterlockedIncrement 7852106c,
 * EnterCriticalSection 78521048 / LeaveCriticalSection 78521044, HeapAlloc 78521170. Paths not ported (crt_unported in the
 * callers: they decline when this returns 0): __set_flsgetvalue's re-store of the getter, a failed allocation (_calloc_crt's
 * retries), locks 12 / 13 not created yet (_mtinitlocknum), the FLS setter failing. */
#include "crt.h"
#include "crt_util.h"
#include "../src/rx_heap.h"

#ifdef RX_ORACLE
static uint32_t os(uint32_t slot, int n, const uint32_t* a) { return rx_call_original(U32(0, slot), RX_STDCALL, 0, n, a); }
#else
#include "../src/rx_os.h"
static uint32_t os(uint32_t slot, int n, const uint32_t* a) { return rx_os_call(slot, n, a); }   /* product: core/src/rx_os.c */
#endif
static uint32_t os1(uint32_t slot, uint32_t a0) { return os(slot, 1, &a0); }
#ifdef RX_ORACLE
static uint32_t call1(uint32_t fn, uint32_t a0) { return rx_call_original(fn, RX_STDCALL, 0, 1, &a0); }
#endif

#ifdef RX_ORACLE
/* 78543607 _decode_pointer(p): the thread's DecodePointer (ptd + 0x1fc) when it has data, else kernel32's (GetProcAddress) */
static int crt_decode_pointer(uint32_t p, uint32_t* out) {
    const uint32_t getter = os1(0x78521030, U32(0, 0x785b71e4));
    uint32_t fn = 0; int have = 0;
    if (getter != 0 && U32(0, 0x785b71e0) != 0xffffffffu) {
        os1(0x78521030, U32(0, 0x785b71e4));                       /* the second TlsGetValue (its result is the getter again) */
        const uint32_t ptd = call1(getter, U32(0, 0x785b71e0));
        if (ptd) { fn = U32(ptd, 0x1fc); have = 1; }
    }
    if (!have) {
        const uint32_t h = os1(0x78521008, 0x785219c4u);             /* GetModuleHandleW(L"KERNEL32.DLL") */
        uint32_t hh = h;
        if (!hh) { const uint32_t w = 0x785219c4u; hh = rx_call_original(0x785420c4, RX_CDECL, 0, 1, &w); }   /* __crt_waitOnModuleHandle */
        if (hh) { const uint32_t a[2] = {hh, 0x785219e0u}; fn = os(0x78521010, 2, a); }   /* "DecodePointer" */
    }
    *out = fn ? call1(fn, p) : p;
    return 1;
}
uint32_t crt_decode_ptr(uint32_t p) { uint32_t r; crt_decode_pointer(p, &r); return r; }
#endif

static void crt_lock(int n) { os1(0x78521048, U32(0x785b70c0u + 8u * (uint32_t)n, 0)); }   /* 78542efa _lock (created: checked) */
static void crt_unlock(int n) { os1(0x78521044, U32(0x785b70c0u + 8u * (uint32_t)n, 0)); }   /* 78542e16 _unlock */

/* 7858d9b6 __addlocaleref: InterlockedIncrement on the locale's reference counts */
static void crt_addlocaleref(rx_addr li) {
    os1(0x7852106c, li);
    if (U32(li, 0xb0)) os1(0x7852106c, U32(li, 0xb0));
    if (U32(li, 0xb8)) os1(0x7852106c, U32(li, 0xb8));
    if (U32(li, 0xb4)) os1(0x7852106c, U32(li, 0xb4));
    if (U32(li, 0xc0)) os1(0x7852106c, U32(li, 0xc0));
    for (uint32_t i = 0, b = li + 0x50; i < 6; i++, b += 0x10) {
        if (U32(b, -8) != 0x785ba2b8u && U32(b, 0) != 0) os1(0x7852106c, U32(b, 0));
        if (U32(b, -4) != 0 && U32(b, 4) != 0) os1(0x7852106c, U32(b, 4));
    }
    os1(0x7852106c, U32(li, 0xd4) + 0xb4);
}

/* 78543753 _initptd(ptd, locinfo) */
static void crt_initptd(rx_addr ptd, rx_addr locinfo) {
#ifdef RX_ORACLE
    uint32_t h = os1(0x78521008, 0x785219c4u);                         /* GetModuleHandleW(L"KERNEL32.DLL") */
    if (!h) { const uint32_t w = 0x785219c4u; h = rx_call_original(0x785420c4, RX_CDECL, 0, 1, &w); }   /* __crt_waitOnModuleHandle */
#else
    const uint32_t h = 0;   /* product: Encode / DecodePointer are the identity (no kernel32 function pointers to store) */
#endif
    SU32(ptd, 0x5c, 0x78522d98u);                                      /* _pxcptacttab = _XcptActTab */
    SU32(ptd, 0x14, 1);                                                /* _holdrand */
    if (h) {
        { const uint32_t a[2] = {h, 0x785219b4u}; SU32(ptd, 0x1f8, os(0x78521010, 2, a)); }   /* EncodePointer */
        { const uint32_t a[2] = {h, 0x785219e0u}; SU32(ptd, 0x1fc, os(0x78521010, 2, a)); }   /* DecodePointer */
    }
    SU32(ptd, 0x70, 1);                                                /* _ownlocale */
    SU8(ptd, 0xc8, 0x43); SU8(ptd, 0x14b, 0x43);                       /* 'C' */
    SU32(ptd, 0x68, 0x785b7760u);                                      /* ptmbcinfo = &__initialmbcinfo */
    crt_lock(13); os1(0x7852106c, U32(ptd, 0x68)); crt_unlock(13);
    crt_lock(12);
    SU32(ptd, 0x6c, locinfo ? locinfo : U32(0, 0x785ba398));          /* ptlocinfo (__ptlocinfo) */
    crt_addlocaleref(U32(ptd, 0x6c));
    crt_unlock(12);
}

/* 7854383f _getptd_noexit: the data of the running thread, created when it has none. 0: the original must run. */
#ifndef RX_ORACLE
/* Product: the core runs single-threaded, so the per-thread data is that of the original's thread the running code belongs to (w-rng):
 * the main thread's, or the level loader's while the core runs what the original runs on its loader thread (the scene's subscene loads,
 * the water asset's ocean spectrum: crt_thread_switch). Each is created on first use exactly as _getptd_noexit creates it (zeroed
 * 0x214-byte block, _initptd: _holdrand 1, _thandle / _tid), without the FLS / TLS indirection. The CRT's rand() keeps its seed there
 * (+0x14), so the loader thread's draws (AnimEntity*::vf5, LandAnimalProximity::vf5, Ocean_InitSpectrum) are a stream of their own
 * from 1, as in the original, and leave the main thread's stream (the race's draws) alone. */
static rx_addr g_ptd[CRT_THREADS];
static int g_thread;
rx_addr crt_ptd(void) {
    rx_addr* p = &g_ptd[g_thread];
    if (!*p) {
        *p = rx_heap_alloc(0x214, 8);
        crt_initptd(*p, 0);
        SU32(*p, 4, 0xffffffffu);                                      /* _thandle */
        SU32(*p, 0, os(0x7852102c, 0, NULL));                          /* _tid */
    }
    return *p;
}
rx_addr crt_ptd_of(int thread) { return thread >= 0 && thread < CRT_THREADS ? g_ptd[thread] : 0; }
int crt_thread_switch(int thread) {
    const int old = g_thread;
    if (thread >= 0 && thread < CRT_THREADS) g_thread = thread;
    return old;
}
#else
int crt_thread_switch(int thread) { (void)thread; return CRT_THREAD_MAIN; }   /* the oracle runs on the emulated thread itself */
rx_addr crt_ptd_of(int thread) { (void)thread; return 0; }
#endif
#ifdef RX_ORACLE
rx_addr crt_ptd(void) {
    const uint32_t saved = os(0x78521054, 0, NULL);                    /* GetLastError */
    rx_addr ptd = 0;
    const uint32_t getter = os1(0x78521030, U32(0, 0x785b71e4));       /* __set_flsgetvalue: TlsGetValue */
    if (!getter) { CRT_WHY("__set_flsgetvalue: getter not stored"); goto out; }
    ptd = call1(getter, U32(0, 0x785b71e0));                           /* FlsGetValue(__flsindex) */
    if (!ptd) {
        if (U32(0, 0x785bd680) != 1) { CRT_WHY("_calloc_impl: heap mode"); goto out; }
        if (!U32(0x785b70c0u + 8 * 13, 0) || !U32(0x785b70c0u + 8 * 12, 0)) { CRT_WHY("_initptd: locks not created"); goto out; }
        const uint32_t a[3] = {U32(0, 0x785bc160), 8, 0x214};          /* _calloc_impl(1, 0x214): HeapAlloc(HEAP_ZERO_MEMORY) */
#ifdef RX_ORACLE
        ptd = os(0x78521170, 3, a);
#else
        (void)a; ptd = rx_heap_alloc(0x214, 8);
#endif
        if (!ptd) { CRT_WHY("_calloc_crt failed"); goto out; }
        uint32_t setter;
        if (!crt_decode_pointer(U32(0, 0x785bbca0), &setter)) { ptd = 0; goto out; }   /* (the block stays allocated) */
        const uint32_t sa[2] = {U32(0, 0x785b71e0), ptd};
        if (!rx_call_original(setter, RX_STDCALL, 0, 2, sa)) { CRT_WHY("FlsSetValue failed"); crt_free(ptd); ptd = 0; goto out; }
        crt_initptd(ptd, 0);
        SU32(ptd, 4, 0xffffffffu);                                     /* _thandle */
        SU32(ptd, 0, os(0x7852102c, 0, NULL));                         /* _tid = GetCurrentThreadId() */
    }
out:
    os1(0x78521070, saved);                                            /* SetLastError */
    return ptd;
}
#endif

/* 7856232a __updatetmbcinfo: the thread's multibyte info replaced by the global one (__ptmbcinfo 785b7b88) unless the thread
 * owns its locale: the old one released (InterlockedDecrement 78521074; freed at 0 unless it is __initialmbcinfo), the new one
 * referenced, under _lock(13). Returns the thread's mbcinfo, 0 when the original must run. */
rx_addr crt_updatetmbcinfo(void) {
    const rx_addr ptd = crt_ptd();                                      /* its _getptd */
    if (!ptd) return 0;
    if ((U32(ptd, 0x70) & U32(0, 0x785ba2b4)) && U32(ptd, 0x6c)) return U32(ptd, 0x68);
    crt_lock(13);
    rx_addr mb = U32(ptd, 0x68);
    if (mb != U32(0, 0x785b7b88)) {
        if (mb && os1(0x78521074, mb) == 0 && mb != 0x785b7760u) crt_free(mb);
        SU32(ptd, 0x68, U32(0, 0x785b7b88));
        mb = U32(0, 0x785b7b88);
        os1(0x7852106c, mb);
    }
    crt_unlock(13);
    return mb;
}
