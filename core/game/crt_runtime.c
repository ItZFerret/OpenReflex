/* crt_runtime.c -- msvcr90's heap entry points (malloc / free / _aligned_* / operator new / delete), qsort, rand, ldiv, the
 * type_info name cache and __RTDynamicCast, as the game calls them.
 *
 * Heap. msvcr90 runs in system-heap mode (__active_heap 0x785bd680 == 1): malloc is HeapAlloc(_crtheap 0x785bc160, 0,
 * size ? size : 1) and free is HeapFree(_crtheap, 0, p), its kernel32 imports 0x78521170 / 0x7852116c. The oracle build makes
 * those calls (the emulator's heap is host state, so the allocations must be the original's); the product allocates from the
 * core's arena heap (src/rx_heap.h). The failure paths (_callnewh, errno ENOMEM, std::bad_alloc) and the other heap modes run
 * the original (crt_unported). */
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>
#include "crt_util.h"
#include "../src/rx_heap.h"

/* _invalid_parameter_noinfo (through the exe's IAT slot 0x916224): the original reports and terminates (Watson); the product stops
 * with the same meaning. */
void crt_invalid_parameter_noinfo(void) {
#ifdef RX_ORACLE
    rx_call_original(U32(0, 0x00916224), RX_CDECL, 0, 0, 0);
#else
    fprintf(stderr, "rxcore: _invalid_parameter_noinfo: a checked index / iterator failed (the original terminates here)\n");
    abort();
#endif
}

/* ---- heap ------------------------------------------------------------------------------------------------------------------ */
static uint32_t crt_heap_alloc(uint32_t size) {
#ifdef RX_ORACLE
    const uint32_t a[3] = {U32(0, 0x785bc160), 0, size};
    return rx_call_original(U32(0, 0x78521170), RX_STDCALL, 0, 3, a);                  /* HeapAlloc */
#else
    return rx_heap_alloc(size, 8);
#endif
}
static void crt_heap_free(rx_addr p) {
#ifdef RX_ORACLE
    const uint32_t a[3] = {U32(0, 0x785bc160), 0, p};
    rx_call_original(U32(0, 0x7852116c), RX_STDCALL, 0, 3, a);                          /* HeapFree */
#else
    rx_heap_free(p);
#endif
}

/* 785839df malloc */
rx_addr crt_malloc(uint32_t size) {
    if (size <= 0xffffffe0u && U32(0, 0x785bc160) != 0 && U32(0, 0x785bd680) == 1) {
        const rx_addr p = crt_heap_alloc(size ? size : 1);
        if (p) return p;
        CRT_WHY("HeapAlloc failed");   /* the original retries through _callnewh / sets errno: allocating again there */
    }
    const uint32_t a[1] = {size};
    return crt_unported(0x785839df, 1, a);
}
RX_FUNCTION(0x785839df, crt_malloc)
/* 785837ee free */
void crt_free(rx_addr p) {
    if (p == 0) return;
    if (U32(0, 0x785bd680) != 1) { const uint32_t a[1] = {p}; crt_unported(0x785837ee, 1, a); return; }
    crt_heap_free(p);   /* (a failed HeapFree would set errno: the emulator's HeapFree always succeeds) */
}
RX_FUNCTION(0x785837ee, crt_free)
/* 78583b39 operator new: malloc until it succeeds (_callnewh / bad_alloc in between) */
rx_addr crt_operator_new(uint32_t size) { return crt_malloc(size); }
RX_FUNCTION(0x78583b39, crt_operator_new)
/* 78583ba3 operator delete: free */
void crt_operator_delete(rx_addr p) { crt_free(p); }
RX_FUNCTION(0x78583ba3, crt_operator_delete)
/* 78583bd3 _aligned_offset_malloc(size, align, offset): a malloc of size + gap + (align - 1) + 4, the result aligned so that
 * result + offset is a multiple of align, the malloc'd pointer stored in the dword below the result rounded down to 4. */
rx_addr crt_aligned_offset_malloc(uint32_t size, uint32_t align, uint32_t offset) {
    if ((align & (align - 1)) != 0 || (offset != 0 && offset >= size)) {          /* EINVAL + _invalid_parameter */
        const uint32_t a[3] = {size, align, offset}; return crt_unported(0x78583bd3, 3, a);
    }
    if (align <= 4) align = 4;
    const uint32_t mask = align - 1, gap = (0u - offset) & 3;
    const rx_addr p = crt_malloc(gap + size + mask + 4);
    if (!p) return 0;
    const rx_addr r = ((p + gap + mask + offset + 4) & ~mask) - offset;
    SU32(r - gap, -4, p);
    return r;
}
RX_FUNCTION(0x78583bd3, crt_aligned_offset_malloc)
rx_addr crt_aligned_malloc(uint32_t size, uint32_t align) { return crt_aligned_offset_malloc(size, align, 0); }   /* 78583cee */
RX_FUNCTION(0x78583cee, crt_aligned_malloc)
void crt_aligned_free(rx_addr p) { if (p) crt_free(U32(p & ~3u, -4)); }                                       /* 78583ccf */
RX_FUNCTION(0x78583ccf, crt_aligned_free)

/* ---- qsort 78594640 (VC9 qsort.c: median of three, insertion by selection below 8 elements, explicit stack) ---------------
 * The comparator is game code: the core's port when one is registered at its address, else the original. Element order for
 * equal keys therefore follows the original exactly. */
typedef int32_t (*crt_cmp_fn)(rx_addr, rx_addr);
static int32_t crt_cmp(rx_addr comp, rx_addr a, rx_addr b) {
    const crt_cmp_fn f = RX_FN(comp, crt_cmp_fn);
    if (f) return f(a, b);
    const uint32_t args[2] = {a, b};
    return (int32_t)rx_call_original(comp, RX_CDECL, 0, 2, args);
}
static void crt_swap(rx_addr a, rx_addr b, uint32_t width) {                     /* 785948a0 (and its inline copies) */
    if (a == b) return;
    while (width--) { const uint8_t t = U8(a, 0); SU8(a, 0, U8(b, 0)); SU8(b, 0, t); a++; b++; }
}
static void crt_shortsort(rx_addr lo, rx_addr hi, uint32_t width, rx_addr comp) {   /* 785945b0 */
    while (hi > lo) {
        rx_addr max = lo;
        for (rx_addr p = lo + width; p <= hi; p += width) if (crt_cmp(comp, p, max) > 0) max = p;
        crt_swap(max, hi, width);
        hi -= width;
    }
}
/* *last_byte: the partition loop's inline swap (7859479e) keeps each byte in the high byte of qsort's first argument slot
 * ([ebp+0xb]): the last such byte (the adapter stores it there), -1 when that swap never ran. */
void crt_qsort_ex(rx_addr base, uint32_t num, uint32_t width, rx_addr comp, int32_t* last_byte) {
    *last_byte = -1;
    if ((base == 0 && num != 0) || width == 0 || comp == 0) {                       /* EINVAL + _invalid_parameter */
        const uint32_t a[4] = {base, num, width, comp}; crt_unported(0x78594640, 4, a); return;
    }
    if (num < 2) return;
    rx_addr lostk[30], histk[30]; int stkptr = 0;
    rx_addr lo = base, hi = base + width * (num - 1);
    for (;;) {
        const uint32_t size = (hi - lo) / width + 1;
        if (size <= 8) crt_shortsort(lo, hi, width, comp);
        else {
            rx_addr mid = lo + (size / 2) * width;
            if (crt_cmp(comp, lo, mid) > 0) crt_swap(lo, mid, width);
            if (crt_cmp(comp, lo, hi) > 0) crt_swap(lo, hi, width);
            if (crt_cmp(comp, mid, hi) > 0) crt_swap(mid, hi, width);
            rx_addr loguy = lo, higuy = hi;
            for (;;) {
                if (mid > loguy) { do loguy += width; while (loguy < mid && crt_cmp(comp, loguy, mid) <= 0); }
                if (mid <= loguy) { do loguy += width; while (loguy <= hi && crt_cmp(comp, loguy, mid) <= 0); }
                do higuy -= width; while (higuy > mid && crt_cmp(comp, higuy, mid) > 0);
                if (higuy < loguy) break;
                if (loguy != higuy && width) *last_byte = U8(loguy, width - 1);
                crt_swap(loguy, higuy, width);
                if (mid == higuy) mid = loguy;
            }
            higuy += width;
            if (mid < higuy) { do higuy -= width; while (higuy > mid && crt_cmp(comp, higuy, mid) == 0); }
            if (mid >= higuy) { do higuy -= width; while (higuy > lo && crt_cmp(comp, higuy, mid) == 0); }
            if ((int32_t)(higuy - lo) >= (int32_t)(hi - loguy)) {
                if (lo < higuy) { lostk[stkptr] = lo; histk[stkptr] = higuy; ++stkptr; }
                if (loguy < hi) { lo = loguy; continue; }
            } else {
                if (loguy < hi) { lostk[stkptr] = loguy; histk[stkptr] = hi; ++stkptr; }
                if (lo < higuy) { hi = higuy; continue; }
            }
        }
        if (--stkptr < 0) return;
        lo = lostk[stkptr]; hi = histk[stkptr];
    }
}
void crt_qsort(rx_addr base, uint32_t num, uint32_t width, rx_addr comp) { int32_t b; crt_qsort_ex(base, num, width, comp, &b); }
RX_FUNCTION(0x78594640, crt_qsort_ex)

/* ---- rand 78594bed: the seed in the per-thread data (+0x14); ldiv 7859442e ---------------------------------------------- */
int32_t crt_rand(void) {
    const rx_addr ptd = crt_ptd();
    if (!ptd) return (int32_t)crt_unported(0x78594bed, 0, NULL);                  /* _getptd creates it (or _amsg_exit) */
    const uint32_t seed = U32(ptd, 0x14) * 0x343fdu + 0x269ec3u;
    SU32(ptd, 0x14, seed);
    return (int32_t)((seed >> 16) & 0x7fff);
}
RX_FUNCTION(0x78594bed, crt_rand)
/* ldiv(n, d): {quot, rem} (EAX, EDX) as idiv */
crt_ldiv_t crt_ldiv(int32_t n, int32_t d) { crt_ldiv_t r; r.quot = n / d; r.rem = n % d; return r; }
RX_FUNCTION(0x7859442e, crt_ldiv)

/* ---- type_info::_name_internal_method 7857bfaf (thiscall, the __type_info_node list) -> _Name_base_internal 7857c2b8:
 * the demangled name cached at this + 4. The first call per type demangles (__unDName and the demangler's internals under
 * _lock(0xe), malloc of the copy and of the list node): that path runs the original. */
rx_addr crt_type_info_name(rx_addr self, rx_addr node) {
    const rx_addr name = U32(self, 4);
    if (name) return name;
    /* the first name: _Name_base_internal (crt_undname.c), entered as 7857bfaf calls it (push ebp; push node; push this; call) from
     * 7857bfaf's entry ESP e (crt_frame_e: the adapter's entry, or a core caller's ESP before its push of node and the call) */
    const rx_addr sp = rx_sp();
    rx_addr e = crt_frame_e(1);
    if (!e) e = sp - 8;
    rx_set_sp(e - 4 - 8 - 4);
    const rx_addr r = crt_type_name_base(self, node);
    rx_set_sp(sp);
    return r;
}
RX_FUNCTION(0x7857bfaf, crt_type_info_name)

/* ---- __RTDynamicCast 7857e090 (VC9 rtti.cpp) --------------------------------------------------------------------------------
 * Complete object locator at vtable[-1]: +4 offset, +8 cdOffset, +0x10 class hierarchy descriptor (+4 attributes: 1 multiple,
 * 2 virtual inheritance; +8 number of bases; +0xc base class array). Base class descriptor: +0 type descriptor (name at +8),
 * +4 number of contained bases, +8 PMD {mdisp, pdisp, vdisp}, +0x14 attributes (1 not visible, 2 ambiguous, 0x40 has its own
 * hierarchy descriptor at +0x18). */
static int crt_type_eq(rx_addr a, rx_addr b) { return a == b || crt_strcmp(a + 8, b + 8) == 0; }
static rx_addr crt_find_complete_object(rx_addr inptr) {                        /* 7857dd03 */
    const rx_addr col = U32(U32(inptr, 0), -4);
    rx_addr r = inptr - U32(col, 4);
    const uint32_t cd = U32(col, 8);
    if (cd) r -= U32(inptr - cd, 0);
    return r;
}
static int32_t crt_pmd_to_offset(rx_addr obj, rx_addr pmd) {                    /* 7857dd9a */
    int32_t off = 0;
    const int32_t pdisp = I32(pmd, 4);
    if (pdisp >= 0) off = pdisp + I32(U32(obj + (uint32_t)pdisp, 0), U32(pmd, 8));
    return I32(pmd, 0) + off;
}
static rx_addr crt_find_si(rx_addr col, rx_addr src, rx_addr target) {          /* 7857dd1e FindSITargetTypeInstance */
    const rx_addr chd = U32(col, 0x10), arr = U32(chd, 0xc);
    const uint32_t n = U32(chd, 8);
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr base = U32(arr, 4 * i);
        if (!crt_type_eq(U32(base, 0), target)) continue;
        for (i++; i < n; i++) {
            const rx_addr b2 = U32(arr, 4 * i);
            if (U8(b2, 0x14) & 4) return 0;
            if (crt_type_eq(U32(b2, 0), src)) return base;
        }
        return 0;
    }
    return 0;
}
static rx_addr crt_find_mi(rx_addr obj, rx_addr col, rx_addr src, int32_t src_off, rx_addr target) {   /* 7857de28 */
    const rx_addr chd = U32(col, 0x10), arr = U32(chd, 0xc);
    const uint32_t n = U32(chd, 8);
    rx_addr ptarget = 0, psource = 0; uint32_t tidx = 0xffffffffu, tend = 0;
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr base = U32(arr, 4 * i);
        if (i - tidx > tend && crt_type_eq(U32(base, 0), target)) {
            if (psource == 0) { ptarget = base; tidx = i; tend = U32(base, 4); }
            else {
                if (U8(base, 0x14) & 3) return 0;
                if (U8(psource, 0x14) & 1) return 0;
                return base;
            }
        }
        if (crt_type_eq(U32(base, 0), src) && crt_pmd_to_offset(obj, base + 8) == src_off) {
            if (ptarget == 0) { psource = base; continue; }
            if (i - tidx <= tend) {
                if (U8(ptarget, 0x14) & 0x40) {
                    const rx_addr sub = U32(U32(U32(ptarget, 0x18), 0xc), 4 * (i - tidx));
                    return (U8(sub, 0x14) & 1) ? 0 : ptarget;
                }
                if (tidx == 0 && (U8(base, 0x14) & 1)) return 0;
                return ptarget;
            }
            if (U8(ptarget, 0x14) & 3) return 0;
            if (U8(base, 0x14) & 1) return 0;
            return ptarget;
        }
    }
    return 0;
}
rx_addr crt_rtdynamiccast(rx_addr inptr, int32_t vfdelta, rx_addr src, rx_addr target, int32_t is_ref) {
    if (inptr == 0) return 0;
    const rx_addr obj = crt_find_complete_object(inptr);
    const rx_addr col = U32(U32(inptr, 0), -4);
    const int32_t delta = (int32_t)(inptr - (uint32_t)vfdelta - obj);
    const uint32_t attr = U32(U32(col, 0x10), 4);
    rx_addr base;
    if (!(attr & 1)) base = crt_find_si(col, src, target);
    else if (!(attr & 2)) base = crt_find_mi(obj, col, src, delta, target);
    else { CRT_WHY("virtual inheritance"); goto original; }                    /* FindVITargetTypeInstance 7857df35 */
    if (base) return obj + (uint32_t)crt_pmd_to_offset(obj, base + 8);
    if (!is_ref) return 0;
    CRT_WHY("bad_cast");                                                          /* throw std::bad_cast */
original:;
    const uint32_t a[5] = {inptr, (uint32_t)vfdelta, src, target, (uint32_t)is_ref};
    return crt_unported(0x7857e090, 5, a);
}
RX_FUNCTION(0x7857e090, crt_rtdynamiccast)
