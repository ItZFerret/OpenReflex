/* crt_frames.c -- the stack frames msvcr90 / msvcp90 leave below a call, for core callers that keep the original's stack.
 *
 * Some game code copies uninitialized locals into heap records: words of its frame that only earlier, deeper calls wrote (the
 * vehicle parts' node bounds, 00541690: the two w lanes its movaps stores, construction.md 15.2 (a)). The original's values there
 * are what msvcp90's std::string members, msvcr90's _stricmp / memcpy_s / malloc / free and the game's own lookups pushed: saved
 * registers, arguments, return addresses, SEH records. The functions below do the CRT call's work AND write every stack word the
 * original's code path writes, at the original's addresses:
 *   e    = the call's return slot (the caller's ESP right after its call instruction; the caller writes [e] and its arguments),
 *   r    = the caller's EBX / EBP / ESI / EDI at the call (what the callee's frames save).
 * Only the paths the game takes are reproduced (the error paths, _Xlen / _Xran, invalid arguments, the locale-aware paths run the
 * plain ports, without frames). In the oracle the heap calls run the original HeapAlloc / HeapFree at the original's ESP; the
 * product has no TEB (fs:[0] reads 0) and its own heap: the frame words are the same but for those two. notes/crt.md "Frames". */
#include "crt.h"
#include "crt_util.h"

#define CRT_TEB rx_teb()            /* fs:[0], the SEH chain head (the world runtime's TEB) */
#define CP_COOKIE 0x78506998u          /* msvcp90's __security_cookie */
#define CR_COOKIE 0x785ba2a8u          /* msvcr90's __security_cookie */
#define CR_HEAP 0x785bc160u            /* _crtheap */
#define CR_LOCALE_CHANGED 0x785bc218u  /* __locale_changed */

static rx_addr s_ptr(rx_addr s) { return U32(s, 0x18) >= 0x10 ? U32(s, 4) : s + 4; }

/* ---- msvcr90 ------------------------------------------------------------------------------------------------------------- */

/* memcpy 7855aac0 for n < 256 (the dword / byte paths): push ebp, edi, esi. (The SSE2 block path is crt_string.c's.) */
static void fr_memcpy(rx_addr e, crt_regs4 r, rx_addr d, rx_addr s, uint32_t n) {
    SU32(e, -4, r.ebp); SU32(e, -8, r.edi); SU32(e, -0xc, r.esi);
    if (n >= 0x100) { CRT_WITH_REGS(r.ebx, r.ebp, r.esi, r.edi, { const rx_addr sp = rx_sp(); rx_set_sp(e + 0x10); crt_memcpy(d, s, n); rx_set_sp(sp); }); return; }
    for (uint32_t i = 0; i < n; i++) SU8(d, i, U8(s, i));
}
/* memcpy_s 7855664b (dst, size, src, count) on its success path (count != 0, dst / src set, size >= count): push ebp, esi, edi;
 * ESI = count, EDI = 0; push count, src, dst; call memcpy (return 78556695) */
static void fr_memcpy_s(rx_addr e, crt_regs4 r, rx_addr d, uint32_t size, rx_addr s, uint32_t n) {
    SU32(e, -4, r.ebp); SU32(e, -8, r.esi); SU32(e, -0xc, r.edi);
    if (n == 0) return;
    if (d == 0 || s == 0 || size < n) { crt_memcpy_s(d, size, s, n); return; }   /* (the error paths: not taken by the game) */
    SU32(e, -0x10, n); SU32(e, -0x14, s); SU32(e, -0x18, d); SU32(e, -0x1c, 0x78556695u);
    const crt_regs4 m = {r.ebx, e - 4, n, 0};
    fr_memcpy(e - 0x1c, m, d, s, n);
}
/* HeapAlloc / HeapFree (kernel32, stdcall heap, flags, x) called from malloc / free: the three arguments and the return address
 * at ret_slot + 4.. / ret_slot (the oracle's original call pushes the same arguments and a 0 return slot: rewritten after) */
static rx_addr heap_call(rx_addr ret_slot, uint32_t ret, int is_free, uint32_t x) {
    const rx_addr sp = rx_sp();
    rx_addr p;
    rx_set_sp(ret_slot + 0x10);
    p = is_free ? (crt_free(x), 0) : crt_malloc(x);
    rx_set_sp(sp);
    SU32(ret_slot, 4, U32(0, CR_HEAP)); SU32(ret_slot, 8, 0); SU32(ret_slot, 0xc, x); SU32(ret_slot, 0, ret);
    return p;
}
/* malloc 785839df (size), the _crtheap / __active_heap 1 path: push ebp, esi; push ebx, edi (EBX = &HeapAlloc); push size (1 for
 * 0), 0, _crtheap; call HeapAlloc (return 78583a58) */
static rx_addr fr_malloc(rx_addr e, crt_regs4 r, uint32_t size) {
    if (size > 0xffffffe0u || U32(0, CR_HEAP) == 0 || U32(0, 0x785bd680) != 1) { CRT_WHY("malloc frame path"); return crt_malloc(size); }
    SU32(e, -4, r.ebp); SU32(e, -8, r.esi); SU32(e, -0xc, r.ebx); SU32(e, -0x10, r.edi);
    return heap_call(e - 0x20, 0x78583a58u, 0, size ? size : 1);
}
/* operator new 78583b39 (size): push ebp; sub esp, 0xc; push size; call malloc (return 78583b58) */
static rx_addr fr_operator_new(rx_addr e, crt_regs4 r, uint32_t size) {
    SU32(e, -4, r.ebp);
    SU32(e, -0x14, size); SU32(e, -0x18, 0x78583b58u);
    const crt_regs4 m = {r.ebx, e - 4, r.esi, r.edi};
    return fr_malloc(e - 0x18, m, size);
}
/* free 785837ee (p) through operator delete 78583ba3 (push ebp / pop ebp, jmp free; the same return slot): push 0x18, 785ad230;
 * call __SEH_prolog4 (return 785837fa: the SEH record {fs:[0], _except_handler4 7858cbde} at EBP - 0x10, the scope table xored
 * with msvcr90's cookie, the cookie ^ EBP, the saved EBX ESI EDI); __active_heap 1: push p, 0, _crtheap; call HeapFree (return
 * 785838bb); call __SEH_epilog4 (return 785838da: fs:[0] restored, the return address pushed at the old EBP slot). */
static void fr_operator_delete(rx_addr e, crt_regs4 r, rx_addr p) {
    SU32(e, -4, r.ebp);                                          /* operator delete's push ebp */
    if (p == 0 || U32(0, 0x785bd680) != 1) { CRT_WHY("free frame path"); crt_free(p); return; }
    SU32(e, -4, 0x18); SU32(e, -8, 0x785ad230u); SU32(e, -0xc, 0x785837fau);
    const rx_addr P = e - 0xc, ebp = e - 4;                      /* the prolog's entry ESP; EBP after it */
    SU32(P, -4, 0x7858cbdeu); SU32(P, -8, U32(CRT_TEB, 0));      /* push _except_handler4; push fs:[0] */
    SU32(e, -4, r.ebp);                                          /* mov [esp + 0x10], ebp (the 0x18 slot) */
    SU32(P, -0x24, r.ebx); SU32(P, -0x28, r.esi); SU32(P, -0x2c, r.edi);   /* sub esp, 0x18; push ebx, esi, edi */
    const uint32_t ck = U32(0, CR_COOKIE);
    SU32(ebp, -4, U32(ebp, -4) ^ ck);                            /* xor [ebp - 4] (the scope table), cookie */
    SU32(P, -0x30, ck ^ ebp);                                    /* push cookie ^ ebp */
    SU32(ebp, -0x18, P - 0x30);                                  /* mov [ebp - 0x18], esp */
    SU32(P, -0x34, U32(ebp, -8));                                /* push [ebp - 8] (the return address 785837fa) */
    const uint32_t xs = U32(ebp, -4);
    SU32(ebp, -4, 0xfffffffeu); SU32(ebp, -8, xs);               /* the try level -2; the xored scope table */
    SU32(CRT_TEB, 0, ebp - 0x10);                                /* fs:[0] = the record; ret (ESP = P - 0x30) */
    (void)heap_call(P - 0x40, 0x785838bbu, 1, p);                /* push p (ESI), 0, _crtheap; call HeapFree */
    SU32(P, -0x34, 0x785838dau);                                 /* call __SEH_epilog4 */
    SU32(CRT_TEB, 0, U32(ebp, -0x10));
    SU32(e, -4, 0x785838dau);                                    /* mov esp, ebp; pop ebp; push ecx; ret */
}
/* _aligned_free 78583ccf (p): push ebp; mov ebp, esp; p set: push [(p & ~3) - 4]; call free (return 78583ce6) -- free's frame as
 * above (operator delete is the same entry: its own push ebp is overwritten by free's push 0x18) */
void crt_aligned_free_at(rx_addr e, crt_regs4 r, rx_addr p) {
    SU32(e, -4, r.ebp);
    if (p == 0) return;
    const rx_addr blk = U32(p & ~3u, -4);
    SU32(e, -8, blk); SU32(e, -0xc, 0x78583ce6u);
    const crt_regs4 m = {r.ebx, e - 4, r.esi, r.edi};
    fr_operator_delete(e - 0xc, m, blk);
}
/* _stricmp 78578d50 (a, b), the "C" locale path: push ebp, esi; pop both; jmp __ascii_stricmp 78578c38: push ebp, esi, edi */
int32_t crt_stricmp_at(rx_addr e, crt_regs4 r, rx_addr a, rx_addr b) {
    SU32(e, -4, r.ebp); SU32(e, -8, r.esi);
    if (U32(0, CR_LOCALE_CHANGED) != 0 || a == 0 || b == 0) return crt_stricmp(a, b);
    SU32(e, -0xc, r.edi);
    return crt_stricmp(a, b);
}

/* ---- msvcp90 std::basic_string<char> -------------------------------------------------------------------------------------- */

/* _Eos 7848588a (n; RET 4): push ebp */
static void fr_eos(rx_addr e, uint32_t ebp, rx_addr s, uint32_t n) { SU32(e, -4, ebp); crt_str_eos(s, n); }
/* 784859a4 char_traits _Copy_s wrapper (dst, dstsize, src, count): push ebp, ecx, [ebp - 4], count, src, dstsize, dst; call
 * 78485215 (return 784859be): push ebp, count, src, dstsize, dst; call _Copy_s 78484f13 (return 7848522b): push ebp, count, src,
 * dstsize, dst; call memcpy_s (return 78484f2a) */
static void fr_copy_s(rx_addr e, crt_regs4 r, uint32_t ecx, rx_addr d, uint32_t size, rx_addr s, uint32_t n) {
    SU32(e, -4, r.ebp); SU32(e, -8, ecx); SU32(e, -0xc, ecx);
    SU32(e, -0x10, n); SU32(e, -0x14, s); SU32(e, -0x18, size); SU32(e, -0x1c, d); SU32(e, -0x20, 0x784859beu);
    const rx_addr e2 = e - 0x20;
    SU32(e2, -4, e - 4); SU32(e2, -8, n); SU32(e2, -0xc, s); SU32(e2, -0x10, size); SU32(e2, -0x14, d); SU32(e2, -0x18, 0x7848522bu);
    const rx_addr e3 = e2 - 0x18;
    SU32(e3, -4, e2 - 4); SU32(e3, -8, n); SU32(e3, -0xc, s); SU32(e3, -0x10, size); SU32(e3, -0x14, d); SU32(e3, -0x18, 0x78484f2au);
    const crt_regs4 m = {r.ebx, e3 - 4, r.esi, r.edi};
    fr_memcpy_s(e3 - 0x18, m, d, size, s, n);
}
/* _Tidy 78485de4 (built, newsize; RET 8) */
static void fr_tidy(rx_addr e, crt_regs4 r, rx_addr s, int built, uint32_t newsize) {
    SU32(e, -4, r.ebp); SU32(e, -8, r.esi);
    const rx_addr ebp = e - 4;
    if (built && U32(s, 0x18) >= 0x10) {
        const rx_addr p = U32(s, 4);
        SU32(e, -0xc, r.edi);                                    /* push edi; EDI = the heap pointer */
        if (newsize > 0) {
            SU32(e, -0x10, newsize); SU32(e, -0x14, p); SU32(e, -0x18, 0x10); SU32(e, -0x1c, s + 4); SU32(e, -0x20, 0x78485e10u);
            const crt_regs4 m = {r.ebx, ebp, s, p};
            fr_copy_s(e - 0x20, m, s, s + 4, 0x10, p, newsize);         /* (ECX = this) */
        }
        SU32(e, -0x10, p); SU32(e, -0x14, 0x78485e19u);          /* push edi; call operator delete (thunk 784b94bc: jmp) */
        const crt_regs4 m = {r.ebx, ebp, s, p};
        fr_operator_delete(e - 0x14, m, p);
    }
    SU32(e, -0xc, newsize); SU32(e, -0x10, 0x78485e2cu);         /* push newsize; call _Eos */
    SU32(s, 0x18, 15);
    fr_eos(e - 0x10, ebp, s, newsize);
}
/* _Copy 78485ffd (newsize, oldlen; RET 8): __EH_prolog3 784b9567 (0xc locals, handler 784ba453), the new capacity, allocate
 * 78485912 -> _Allocate 784851ae -> operator new, the old contents copied, _Tidy(1, 0), _Eos(oldlen), __EH_epilog3 784b961b */
static void fr_copy(rx_addr e, crt_regs4 r, rx_addr s, uint32_t newsize, uint32_t oldlen) {
    /* push 0xc; mov eax, 784ba453; call __EH_prolog3 (return 78486009) */
    SU32(e, -4, 0xc); SU32(e, -8, 0x78486009u);
    const rx_addr P = e - 8, F = e - 4;                          /* the prolog's entry ESP; EBP after it */
    SU32(P, -4, 0x784ba453u); SU32(P, -8, U32(CRT_TEB, 0));      /* push eax (the handler); push fs:[0] */
    rx_addr sp = P - 8 - 0xc;                                    /* sub esp, [esp + 0xc] (0xc) */
    SU32(sp, -4, r.ebx); SU32(sp, -8, r.esi); SU32(sp, -0xc, r.edi); sp -= 0xc;
    SU32(F, 0, r.ebp);                                           /* mov [eax], ebp (the 0xc slot) */
    SU32(sp, -4, U32(0, CP_COOKIE) ^ F); sp -= 4;                /* push cookie ^ ebp */
    SU32(F, -0x10, sp);                                          /* mov [ebp - 0x10], esp */
    SU32(sp, -4, U32(F, -4)); sp -= 4;                           /* push [ebp - 4] (the return address) */
    SU32(F, -4, 0xffffffffu);
    SU32(CRT_TEB, 0, F - 0xc);                                   /* fs:[0] = the record */
    sp += 4;                                                     /* ret */
    /* the body: EDI = s, [ebp - 0x18] = s; ESI = the new capacity */
    SU32(F, -0x18, s);
    uint32_t res = newsize | 15;
    const uint32_t old = U32(s, 0x18);
    if (res > 0xfffffffeu) res = newsize;
    else {
        SU32(sp, -4, 3);                                         /* push 3; pop ebx */
        SU32(F, -0x14, old); SU32(F, -0x14, old >> 1);
        if (res / 3 < (old >> 1)) {
            SU32(sp, -4, 0xfffffffeu);                           /* push -2; pop eax */
            if (old <= 0xfffffffeu - (old >> 1)) res = (old >> 1) + old;
        }
    }
    const uint32_t ebx_body = res > 0xfffffffeu ? r.ebx : 3;
    SU32(F, -4, 0);                                              /* and [ebp - 4], 0 */
    /* push res + 1; call allocate (return 78486052): push ebp, 0, count; call _Allocate (return 78485921): push ebp; sub esp, 0xc;
     * push count; call operator new (thunk 784b9504: jmp; return 784851c5) */
    SU32(sp, -4, res + 1); SU32(sp, -8, 0x78486052u);
    const rx_addr A = sp - 8;
    SU32(A, -4, F); SU32(A, -8, 0); SU32(A, -0xc, res + 1); SU32(A, -0x10, 0x78485921u);
    const rx_addr A2 = A - 0x10;
    SU32(A2, -4, A - 4);
    SU32(A2, -0x14, res + 1); SU32(A2, -0x18, 0x784851c5u);
    const crt_regs4 m = {ebx_body, A2 - 4, res, s};
    const rx_addr p = fr_operator_new(A2 - 0x18, m, res + 1);
    /* the old contents: push oldlen, old pointer, res + 1, p; call 784859a4 (return 784860a1) */
    if (oldlen > 0) {
        SU32(sp, -4, oldlen); SU32(sp, -8, s_ptr(s)); SU32(sp, -0xc, res + 1); SU32(sp, -0x10, p); SU32(sp, -0x14, 0x784860a1u);
        const crt_regs4 c = {p, F, res, s};
        fr_copy_s(sp - 0x14, c, 0, p, res + 1, s_ptr(s), oldlen);   /* (ECX: whatever 7848607f leaves) */
    }
    /* push 0, 1; call _Tidy (return 784860af) */
    SU32(sp, -4, 0); SU32(sp, -8, 1); SU32(sp, -0xc, 0x784860afu);
    const crt_regs4 t = {p, F, res, s};
    fr_tidy(sp - 0xc, t, s, 1, 0);
    /* push oldlen; [edi + 4] = p; [edi + 0x18] = res; call _Eos (return 784860bf) */
    SU32(sp, -4, oldlen); SU32(sp, -8, 0x784860bfu);
    SU32(s, 4, p); SU32(s, 0x18, res);
    fr_eos(sp - 8, F, s, oldlen);
    /* call __EH_epilog3 (return 784860c4): fs:[0] = [ebp - 0xc]; ... mov esp, ebp; pop ebp; push ecx (the return address) */
    SU32(sp, -4, 0x784860c4u);
    SU32(CRT_TEB, 0, U32(F, -0xc));
    SU32(F, 0, 0x784860c4u);
}
/* _Grow 784860e1 (newsize, trim = 0; RET 8) -> nonzero when newsize > 0 */
static int fr_grow(rx_addr e, crt_regs4 r, rx_addr s, uint32_t newsize) {
    SU32(e, -4, r.ebp); SU32(e, -8, r.esi); SU32(e, -0xc, r.edi);
    const rx_addr ebp = e - 4;
    if (U32(s, 0x18) < newsize) {
        SU32(e, -0x10, U32(s, 0x14)); SU32(e, -0x14, newsize); SU32(e, -0x18, 0x78486107u);
        const crt_regs4 m = {r.ebx, ebp, s, newsize};
        fr_copy(e - 0x18, m, s, newsize, U32(s, 0x14));
    } else if (newsize == 0) {
        SU32(e, -0x10, 0); SU32(e, -0x14, 0x78486135u);
        fr_eos(e - 0x14, ebp, s, 0);
    }
    return newsize > 0;
}
/* assign(ptr, count) 784863af for a ptr outside the string: _Inside (return 784863c1), _Grow (return 784863ef), the copy (784859a4,
 * return 7848640e), _Eos (return 7848641b) */
static void fr_assign_n(rx_addr e, crt_regs4 r, rx_addr s, rx_addr p, uint32_t n) {
    SU32(e, -4, r.ebp); SU32(e, -8, r.esi); SU32(e, -0xc, r.edi);
    const rx_addr ebp = e - 4;
    SU32(e, -0x10, p); SU32(e, -0x14, 0x784863c1u);             /* push edi (ptr); call _Inside: push ebp, esi */
    SU32(e, -0x18, ebp); SU32(e, -0x1c, s);
    const crt_regs4 m = {r.ebx, ebp, s, p};
    SU32(e, -0x10, 0); SU32(e, -0x14, n); SU32(e, -0x18, 0x784863efu);
    if (!fr_grow(e - 0x18, m, s, n)) return;
    const uint32_t res = U32(s, 0x18);
    SU32(e, -0x10, n); SU32(e, -0x14, p); SU32(e, -0x18, res); SU32(e, -0x1c, s_ptr(s)); SU32(e, -0x20, 0x7848640eu);
    fr_copy_s(e - 0x20, m, res, s_ptr(s), res, p, n);
    SU32(e, -0x10, n); SU32(e, -0x14, 0x7848641bu);
    fr_eos(e - 0x14, ebp, s, n);
}
/* basic_string(const char*) 78487135: push ebp, esi; push 0; [s + 0x18] = 15; call _Eos (return 7848714b); push ptr; call assign
 * 78486428 (return 78487155): push ebp, esi; push ptr; call strlen (no frame; return 78486438); push len, ptr; call assign(ptr, len)
 * (return 78486444). A ptr inside the new string cannot happen (it is empty). */
rx_addr crt_str_ctor_cstr_at(rx_addr e, crt_regs4 r, rx_addr s, rx_addr p) {
    SU32(e, -4, r.ebp); SU32(e, -8, r.esi);
    SU32(e, -0xc, 0); SU32(e, -0x10, 0x7848714bu);
    SU32(s, 0x18, 15);
    fr_eos(e - 0x10, e - 4, s, 0);
    SU32(e, -0xc, p); SU32(e, -0x10, 0x78487155u);
    const rx_addr E1 = e - 0x10;
    SU32(E1, -4, e - 4); SU32(E1, -8, s); SU32(E1, -0xc, p); SU32(E1, -0x10, 0x78486438u);
    const uint32_t n = crt_strlen(p);
    SU32(E1, -0xc, n); SU32(E1, -0x10, p); SU32(E1, -0x14, 0x78486444u);
    const crt_regs4 m = {r.ebx, E1 - 4, s, r.edi};
    fr_assign_n(E1 - 0x14, m, s, p, n);
    return s;
}
/* ~basic_string 78485ea4: push 0, 1; call _Tidy (return 78485ead) */
void crt_str_dtor_at(rx_addr e, crt_regs4 r, rx_addr s) {
    SU32(e, -4, 0); SU32(e, -8, 1); SU32(e, -0xc, 0x78485eadu);
    fr_tidy(e - 0xc, r, s, 1, 0);
}
