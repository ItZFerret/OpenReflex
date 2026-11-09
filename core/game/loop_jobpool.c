/* loop_jobpool.c -- the engine's job pool constructor (00881a70, ECX = the pool 0x00d94ae0; see loop_engine.c for the pool's
 * layout): its two critical sections (+0 and +0x18), the four per-worker arrays (thread handle / id +0x884c, 8 bytes each; wake
 * events +0x887c; started ("Sleep") events +0x8894; quit events +0x88ac; 4 bytes each, 6 entries zeroed by 004dc090), the
 * counters, the two workers' named events ("Wake%d" auto-reset, "Kill%d" auto-reset, "Sleep%d" manual-reset, signalled) and their
 * suspended threads (JobPool_WorkerThread 00881900 with the worker index, affinity masks 2 and 4, priority 0, resumed), then
 * under the pool's lock the worker mask +0x8844 = 0x3f and the worker count +0x8848 (2: bits 0 and 1). The system calls go
 * through the imports (the oracle's emulated ones, the product's rx_os stand-ins). notes/loop.md. */
#include "fx.h"
#include "fx_util.h"
#include "../src/rx_os.h"

#define IAT_INIT_CS    0x00916014u
#define IAT_ENTER_CS   0x0091600cu
#define IAT_LEAVE_CS   0x009160f8u
#define IAT_CREATE_EVT 0x00916038u
#define IAT_CREATE_THR 0x00916030u
#define IAT_AFFINITY   0x00916028u
#define IAT_PRIORITY   0x00916024u
#define IAT_RESUME     0x0091602cu
#define IAT_LAST_ERR   0x00916078u

/* 004dc090 (ECX = element; EAX = ECX): the element constructor of the pool's handle arrays: the dword is 0. */
rx_addr rx_jobpool_element_ctor(rx_addr e) {
    SU32(e, 0, 0);
    return e;
}
RX_FUNCTION(0x004dc090, rx_jobpool_element_ctor)

/* an import call: the arguments and the return address stay in the dead stack after it (rx_call_original writes its own return slot,
 * so the residue is written after the call) */
static uint32_t os_call(uint32_t slot, rx_addr ret, int n, const uint32_t* a) {
    const uint32_t r = rx_os_import(slot, n, a);
    fx_import_residue(ret, n, a);
    return r;
}

rx_addr rx_jobpool_construct(rx_addr pool) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    const rx_addr s1 = sp0 - 0x4c, E = s1 - 16;
    SU32(sp0, -4u, U32(0, 0x009b45e4) ^ s1);
    SU32(s1, -4u, sv.ebx); SU32(s1, -8u, sv.ebp); SU32(s1, -0xcu, sv.esi); SU32(s1, -0x10u, sv.edi);
    rx_set_sp(E);
    fx_set_reg(FX_EDI, U32(0, IAT_INIT_CS)); fx_set_reg(FX_ESI, pool);
    { const uint32_t a[1] = {pool}; os_call(IAT_INIT_CS, 0x00881a8d, 1, a); }
    { const uint32_t a[1] = {pool + 0x18}; os_call(IAT_INIT_CS, 0x00881a93, 1, a); }
    const rx_addr ebx = pool + 0x884c, ebp = pool + 0x887c;
    fx_set_reg(FX_EBX, ebx);
    { const uint32_t a[4] = {ebx, 8, 6, 0x004dc090}; const rx_addr s = fx_push(4, a, 0x00881aa8); rx_fx_vector_ctor_iterator(ebx, 8, 6, 0x004dc090); fx_pop(s); }
    fx_set_reg(FX_EBP, ebp);
    { const uint32_t a[4] = {ebp, 4, 6, 0x004dc090}; const rx_addr s = fx_push(4, a, 0x00881abd); rx_fx_vector_ctor_iterator(ebp, 4, 6, 0x004dc090); fx_pop(s); }
    { const uint32_t a[4] = {pool + 0x8894, 4, 6, 0x004dc090}; const rx_addr s = fx_push(4, a, 0x00881ad2); rx_fx_vector_ctor_iterator(pool + 0x8894, 4, 6, 0x004dc090); fx_pop(s); }
    { const uint32_t a[4] = {pool + 0x88ac, 4, 6, 0x004dc090}; const rx_addr s = fx_push(4, a, 0x00881ae7); rx_fx_vector_ctor_iterator(pool + 0x88ac, 4, 6, 0x004dc090); fx_pop(s); }
    fx_set_reg(FX_EDI, 0);
    SU32(0, 0x00a89bc0, pool);
    SU32(pool, 0x883c, 0); SU32(pool, 0x8830, 0); SU32(pool, 0x8840, 0); SU32(pool, 0x8848, 0); SU32(pool, 0x8834, 0); SU32(pool, 0x8838, 0);
    SU32(E, 0x10, 1); SU32(E, 0x14, 2);
    const rx_addr name = E + 0x18;
    rx_addr thr = ebx, wake = ebp;
    for (uint32_t i = 0; i < 2; i++) {
        static const uint32_t fmt[3] = {0x009772c4, 0x009772cc, 0x009772d4};   /* "Wake%d", "Kill%d", "Sleep%d" */
        static const rx_addr ret_spf[3] = {0x00881b34, 0x00881b6d, 0x00881b9f};
        static const rx_addr ret_ev[3] = {0x00881b4f, 0x00881b81, 0x00881bb3};
        static const rx_addr ret_err[3] = {0x00881b5c, 0x00881b8e, 0x00881bc0};
        static const uint32_t slot[3] = {0, 0x30, 0x18};                       /* from the wake event's slot: +0 wake, +0x30 kill, +0x18 sleep */
        fx_set_reg(FX_EDI, i);
        SU32(wake, 0x48, 0);
        for (int k = 0; k < 3; k++) {
            if (k == 1 || k == 2) fx_set_reg(FX_EDI, i);
            {   /* sprintf(name, fmt, i): the arguments live below ESP during the call */
                SU32(E, -4u, i); SU32(E, -8u, fmt[k]); SU32(E, -0xcu, name);
#ifdef RX_ORACLE
                { const uint32_t sa[3] = {name, fmt[k], i};   /* msvcr90's own frames leave residue the core's CRT does not (known-differences 1) */
                  rx_set_sp(E); rx_call_original(U32(0, 0x00916204), RX_CDECL, 0, 3, sa); }
#else
                rx_set_sp(E - 0xcu);
                crt_sprintf(name, fmt[k], E - 4u);
                rx_set_sp(E);
#endif
                SU32(E, -0x10u, ret_spf[k]);
            }
            const uint32_t manual = k == 2 ? 1u : 0u;
            const uint32_t a[4] = {0, manual, manual, name};
            const uint32_t h = os_call(IAT_CREATE_EVT, ret_ev[k], 4, a);
            SU32(wake, slot[k], h);
            if (h == 0) os_call(IAT_LAST_ERR, ret_err[k], 0, 0);
        }
        {   /* CreateThread(0, 0, JobPool_WorkerThread, i, CREATE_SUSPENDED, &id) */
            const uint32_t a[6] = {0, 0, 0x00881900, i, 4, thr + 4};
            const uint32_t h = os_call(IAT_CREATE_THR, 0x00881bd6, 6, a);
            const uint32_t mask = 1u << (U32(E, 0x10 + 4 * i) & 31);
            SU32(thr, 0, h);
            { const uint32_t b[2] = {h, mask}; os_call(IAT_AFFINITY, 0x00881beb, 2, b); }
            { const uint32_t b[2] = {U32(thr, 0), 0}; os_call(IAT_PRIORITY, 0x00881bf6, 2, b); }
            { const uint32_t b[1] = {U32(thr, 0)}; os_call(IAT_RESUME, 0x00881bff, 1, b); }
        }
        thr += 8; wake += 4;
    }
    { const uint32_t a[1] = {pool}; os_call(IAT_ENTER_CS, 0x00881c16, 1, a); }
    SU32(pool, 0x8844, 0x3f);
    const uint32_t mask = U32(pool, 0x8844);
    uint32_t n = 0;
    if (mask & 1) n = 1;
    if (mask & 2) n++;
    SU32(pool, 0x8848, n);
    { const uint32_t a[1] = {pool}; os_call(IAT_LEAVE_CS, 0x00881c43, 1, a); }
    SU32(s1, -4u, 0x00881c54u);               /* the call of __security_check_cookie 008f9efe (no further frame: phys_security_check_cookie) */
    SU32(s1, -4u, 0x00881c54u);
    fx_restore(sv);
    rx_set_sp(sp0);
    return pool;
}
RX_FUNCTION(0x00881a70, rx_jobpool_construct)
