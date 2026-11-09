/* crt_cpp.c -- msvcp90.dll's std::basic_string<char> (VC9 xstring, _SECURE_SCL checked iterators) members the game calls,
 * and char_traits<char>'s helpers. All are thiscall on the string object (ECX):
 *   +0x04 _Bx: the 16-byte buffer, or the heap pointer when _Myres >= 16;  +0x14 _Mysize;  +0x18 _Myres.
 * Memory comes from msvcr90's operator new / delete (allocator<char>), copies through memcpy_s / memmove_s, searches through
 * memchr / memcmp / strlen (msvcp90's imports of msvcr90: the crt_string.c ports). The exceptional paths (_Xran
 * out_of_range, _Xlen length_error, the checked iterators' _invalid_parameter_noinfo, aliasing in replace) run the original
 * from the entry (crt_unported), decided before any store. Iterators are {_Mycont, _Myptr} (8 bytes, returned through the
 * hidden result pointer). */
#include "crt.h"
#include "crt_util.h"

#define NPOS 0xffffffffu
static rx_addr myptr(rx_addr s) { return U32(s, 0x18) >= 0x10 ? U32(s, 4) : s + 4; }
static uint32_t mysize(rx_addr s) { return U32(s, 0x14); }

/* char_traits<char>::_Copy_s 78484f13 / _Move_s 78484f59 / find 78484f37 */
static void tr_copy_s(rx_addr d, uint32_t dsize, rx_addr s, uint32_t n) { crt_memcpy_s(d, dsize, s, n); }
static void tr_move_s(rx_addr d, uint32_t dsize, rx_addr s, uint32_t n) { crt_memmove_s(d, dsize, s, n); }
static rx_addr tr_find(rx_addr p, uint32_t n, uint8_t c) { return crt_memchr(p, (int8_t)c, n); }

/* 7848588a _Eos(n): _Mysize = n, terminator */
void crt_str_eos(rx_addr s, uint32_t n) { SU32(s, 0x14, n); SU8(myptr(s), n, 0); }
RX_FUNCTION(0x7848588a, crt_str_eos)
/* 784858b0 _Inside(ptr) */
int crt_str_inside(rx_addr s, rx_addr p) {
    if (p == 0 || p < myptr(s) || myptr(s) + mysize(s) <= p) return 0;
    return 1;
}
RX_FUNCTION(0x784858b0, crt_str_inside)
/* 78485de4 _Tidy(built, newsize) */
void crt_str_tidy(rx_addr s, int built, uint32_t newsize) {
    if (built && U32(s, 0x18) >= 0x10) {
        const rx_addr p = U32(s, 4);
        if (newsize > 0) tr_copy_s(s + 4, 0x10, p, newsize);
        crt_operator_delete(p);
    }
    SU32(s, 0x18, 15);
    crt_str_eos(s, newsize);
}
RX_FUNCTION(0x78485de4, crt_str_tidy)
/* 78485912 allocator<char>::allocate(n) -> _Allocate (784851ae): operator new(n) (n * 1 cannot overflow) */
static rx_addr str_allocate(uint32_t n) { return crt_operator_new(n); }
rx_addr crt_allocator_allocate(rx_addr self, uint32_t n) { (void)self; return str_allocate(n); }
RX_FUNCTION(0x78485912, crt_allocator_allocate)
/* 78484f13 char_traits<char>::_Copy_s(dst, dstsize, src, count) (cdecl): memcpy_s, dst */
rx_addr crt_traits_copy_s(rx_addr d, uint32_t dsize, rx_addr s, uint32_t n) { tr_copy_s(d, dsize, s, n); return d; }
RX_FUNCTION(0x78484f13, crt_traits_copy_s)
/* 78485ffd _Copy(newsize, oldlen): the new capacity newsize | 15, or 1.5 x the old when that is more */
void crt_str_copy(rx_addr s, uint32_t newsize, uint32_t oldlen) {
    (void)U32(0, 0x78506998);   /* its __EH_prolog3 frame reads msvcp90's __security_cookie (the page becomes resident) */
    uint32_t res = newsize | 15;
    const uint32_t old = U32(s, 0x18);
    if (res > 0xfffffffeu) res = newsize;
    else if (res / 3 < old / 2 && old <= 0xfffffffeu - old / 2) res = old + old / 2;
    const rx_addr p = str_allocate(res + 1);
    if (oldlen > 0) tr_copy_s(p, res + 1, myptr(s), oldlen);
    crt_str_tidy(s, 1, 0);
    SU32(s, 4, p);
    SU32(s, 0x18, res);
    crt_str_eos(s, oldlen);
}
RX_FUNCTION(0x78485ffd, crt_str_copy)
/* 784860e1 _Grow(newsize, trim) (newsize <= max_size checked by the callers) */
int crt_str_grow(rx_addr s, uint32_t newsize, int trim) {
    if (newsize > 0xfffffffeu) { const uint32_t a[2] = {newsize, (uint32_t)trim}; return (int)crt_unported_this(0x784860e1, s, 2, a); }
    if (U32(s, 0x18) < newsize) crt_str_copy(s, newsize, mysize(s));
    else if (trim && newsize < 16) crt_str_tidy(s, 1, newsize < mysize(s) ? newsize : mysize(s));
    else if (newsize == 0) crt_str_eos(s, 0);
    return newsize > 0;
}
RX_FUNCTION(0x784860e1, crt_str_grow)

/* 784859ec erase(off, count) */
static void str_erase(rx_addr s, uint32_t off, uint32_t count) {
    if (mysize(s) - off < count) count = mysize(s) - off;
    if (count > 0) {
        const uint32_t nm = mysize(s) - off - count;
        tr_move_s(myptr(s) + off, U32(s, 0x18) - off, myptr(s) + off + count, nm);
        crt_str_eos(s, mysize(s) - count);
    }
}
rx_addr crt_str_erase(rx_addr s, uint32_t off, uint32_t count) {
    if (mysize(s) < off) { const uint32_t a[2] = {off, count}; return crt_unported_this(0x784859ec, s, 2, a); }   /* _Xran */
    str_erase(s, off, count);
    return s;
}
RX_FUNCTION(0x784859ec, crt_str_erase)
/* 78486316 assign(right, roff, count) */
static void str_assign_sub(rx_addr s, rx_addr right, uint32_t roff, uint32_t count) {
    uint32_t num = mysize(right) - roff;
    if (count < num) num = count;
    if (s == right) { str_erase(s, roff + num, NPOS); str_erase(s, 0, roff); return; }
    if (crt_str_grow(s, num, 0)) {
        tr_copy_s(myptr(s), U32(s, 0x18), myptr(right) + roff, num);
        crt_str_eos(s, num);
    }
}
rx_addr crt_str_assign_sub(rx_addr s, rx_addr right, uint32_t roff, uint32_t count) {
    if (mysize(right) < roff) { const uint32_t a[3] = {right, roff, count}; return crt_unported_this(0x78486316, s, 3, a); }
    str_assign_sub(s, right, roff, count);
    return s;
}
RX_FUNCTION(0x78486316, crt_str_assign_sub)
/* 784863af assign(ptr, count) */
rx_addr crt_str_assign_n(rx_addr s, rx_addr p, uint32_t count) {
    if (crt_str_inside(s, p)) {                                          /* a piece of itself */
        if (mysize(s) < p - myptr(s)) { const uint32_t a[2] = {p, count}; return crt_unported_this(0x784863af, s, 2, a); }
        str_assign_sub(s, s, p - myptr(s), count);
        return s;
    }
    if (count > 0xfffffffeu) { const uint32_t a[2] = {p, count}; return crt_unported_this(0x784863af, s, 2, a); }   /* _Xlen */
    if (crt_str_grow(s, count, 0)) {
        tr_copy_s(myptr(s), U32(s, 0x18), p, count);
        crt_str_eos(s, count);
    }
    return s;
}
RX_FUNCTION(0x784863af, crt_str_assign_n)
rx_addr crt_str_assign(rx_addr s, rx_addr p) { return crt_str_assign_n(s, p, crt_strlen(p)); }          /* 78486428 */
RX_FUNCTION(0x78486428, crt_str_assign)
rx_addr crt_str_operator_assign(rx_addr s, rx_addr p) { return crt_str_assign(s, p); }                   /* 784872a2 */
RX_FUNCTION(0x784872a2, crt_str_operator_assign)

/* constructors / destructor */
rx_addr crt_str_ctor(rx_addr s) { SU32(s, 0x18, 15); crt_str_eos(s, 0); return s; }                       /* 78485e6a */
RX_FUNCTION(0x78485e6a, crt_str_ctor)
rx_addr crt_str_ctor_cstr(rx_addr s, rx_addr p) { crt_str_ctor(s); crt_str_assign(s, p); return s; }       /* 78487135 */
RX_FUNCTION(0x78487135, crt_str_ctor_cstr)
rx_addr crt_str_ctor_copy(rx_addr s, rx_addr right) { crt_str_ctor(s); str_assign_sub(s, right, 0, NPOS); return s; }   /* 78487272 */
RX_FUNCTION(0x78487272, crt_str_ctor_copy)
void crt_str_dtor(rx_addr s) { crt_str_tidy(s, 1, 0); }                                                     /* 78485ea4 */
RX_FUNCTION(0x78485ea4, crt_str_dtor)

/* 784857ce compare(off, n0, ptr, count): memcmp of the common length, then the lengths */
int32_t crt_str_compare_sub(rx_addr s, uint32_t off, uint32_t n0, rx_addr p, uint32_t count) {
    if (mysize(s) < off) { const uint32_t a[4] = {off, n0, p, count}; return (int32_t)crt_unported_this(0x784857ce, s, 4, a); }
    if (mysize(s) - off < n0) n0 = mysize(s) - off;
    const int32_t ans = crt_memcmp(myptr(s) + off, p, n0 < count ? n0 : count);
    if (ans != 0) return ans;
    return n0 < count ? -1 : n0 == count ? 0 : 1;
}
RX_FUNCTION(0x784857ce, crt_str_compare_sub)
int32_t crt_str_compare_cstr(rx_addr s, rx_addr p) { return crt_str_compare_sub(s, 0, mysize(s), p, crt_strlen(p)); }   /* 78485d8d */
RX_FUNCTION(0x78485d8d, crt_str_compare_cstr)
int32_t crt_str_compare(rx_addr s, rx_addr right) { return crt_str_compare_sub(s, 0, mysize(s), myptr(right), mysize(right)); }   /* 78485d0b */
RX_FUNCTION(0x78485d0b, crt_str_compare)
/* 784958f7 operator==(const string&, const char*) (cdecl): 1 / 0 in EAX */
uint32_t crt_str_eq_cstr(rx_addr s, rx_addr p) { return crt_str_compare_cstr(s, p) == 0 ? 1u : 0u; }
RX_FUNCTION(0x784958f7, crt_str_eq_cstr)
/* 78495979 operator<(const string&, const string&) (cdecl): AL = compare < 0, EAX's upper bytes those of compare's result */
uint32_t crt_str_less(rx_addr l, rx_addr r) {
    const uint32_t c = (uint32_t)crt_str_compare(l, r);
    return (c & 0xffffff00u) | ((int32_t)c < 0 ? 1u : 0u);
}
RX_FUNCTION(0x78495979, crt_str_less)

/* 7848536a find(ptr, off, count) */
uint32_t crt_str_find(rx_addr s, rx_addr p, uint32_t off, uint32_t count) {
    if (count == 0 && off <= mysize(s)) return off;
    uint32_t nm;
    if (off < mysize(s) && count <= (nm = mysize(s) - off)) {
        const uint8_t first = U8(p, 0);
        nm -= count - 1;
        rx_addr v = myptr(s) + off, u;
        while ((u = tr_find(v, nm, first)) != 0) {
            if (crt_memcmp(u, p, count) == 0) return u - myptr(s);
            nm -= u - v + 1;
            v = u + 1;
        }
    }
    return NPOS;
}
RX_FUNCTION(0x7848536a, crt_str_find)
uint32_t crt_str_find_cstr(rx_addr s, rx_addr p, uint32_t off) { return crt_str_find(s, p, off, crt_strlen(p)); }   /* 7848540c */
RX_FUNCTION(0x7848540c, crt_str_find_cstr)
uint32_t crt_str_find_str(rx_addr s, rx_addr right, uint32_t off) { return crt_str_find(s, myptr(right), off, mysize(right)); }   /* 78485c03 */
RX_FUNCTION(0x78485c03, crt_str_find_str)
/* 78485435 find_first_of(ch, off) = find(&ch, off, 1): the first ch at or after off */
uint32_t crt_str_find_first_of_ch(rx_addr s, uint8_t ch, uint32_t off) {
    if (off < mysize(s) && 1 <= mysize(s) - off) {
        const rx_addr u = tr_find(myptr(s) + off, mysize(s) - off, ch);
        if (u) return u - myptr(s);
    }
    return NPOS;
}
RX_FUNCTION(0x78485435, crt_str_find_first_of_ch)
/* 7848d935 basic_string<wchar_t>::size */
uint32_t crt_wstr_size(rx_addr s) { return mysize(s); }
RX_FUNCTION(0x7848d935, crt_wstr_size)

/* 78486811 replace(off, n0, right, roff, count) */
rx_addr crt_str_replace_sub(rx_addr s, uint32_t off, uint32_t n0, rx_addr right, uint32_t roff, uint32_t count) {
    const uint32_t a[5] = {off, n0, right, roff, count};
    if (mysize(s) < off || mysize(right) < roff) return crt_unported_this(0x78486811, s, 5, a);   /* _Xran */
    if (mysize(s) - off < n0) n0 = mysize(s) - off;
    if (mysize(right) - roff < count) count = mysize(right) - roff;
    if (NPOS - count <= mysize(s) - n0) return crt_unported_this(0x78486811, s, 5, a);           /* _Xlen */
    if (s == right) return CRT_WHY("replace from itself"), crt_unported_this(0x78486811, s, 5, a);
    const uint32_t nm = mysize(s) - n0 - off, newsize = mysize(s) + count - n0;
    if (mysize(s) < newsize) crt_str_grow(s, newsize, 0);
    tr_move_s(myptr(s) + off + count, U32(s, 0x18) - off - count, myptr(s) + off + n0, nm);
    tr_copy_s(myptr(s) + off, U32(s, 0x18) - off, myptr(right) + roff, count);
    crt_str_eos(s, newsize);
    return s;
}
RX_FUNCTION(0x78486811, crt_str_replace_sub)
rx_addr crt_str_replace(rx_addr s, uint32_t off, uint32_t n0, rx_addr right) {                  /* 784874d8 */
    return crt_str_replace_sub(s, off, n0, right, 0, NPOS);
}
RX_FUNCTION(0x784874d8, crt_str_replace)
/* 78486aff replace(off, n0, ptr, count): a source inside the string goes through replace(off, n0, *this, ptr - _Myptr, count);
 * the tail moves before _Grow when the string shrinks, after it when it grows */
rx_addr crt_str_replace_n(rx_addr s, uint32_t off, uint32_t n0, rx_addr p, uint32_t count) {
    if (crt_str_inside(s, p)) return crt_str_replace_sub(s, off, n0, s, p - myptr(s), count);
    const uint32_t a[4] = {off, n0, p, count};
    if (mysize(s) < off) return crt_unported_this(0x78486aff, s, 4, a);                           /* _Xran */
    if (mysize(s) - off < n0) n0 = mysize(s) - off;
    if (NPOS - count <= mysize(s) - n0) return crt_unported_this(0x78486aff, s, 4, a);            /* _Xlen */
    const uint32_t nm = mysize(s) - off - n0;
    if (count < n0) tr_move_s(myptr(s) + off + count, U32(s, 0x18) - off - count, myptr(s) + off + n0, nm);
    if (count > 0 || n0 > 0) {
        const uint32_t newsize = mysize(s) + count - n0;
        if (crt_str_grow(s, newsize, 0)) {
            if (n0 < count) tr_move_s(myptr(s) + off + count, U32(s, 0x18) - off - count, myptr(s) + off + n0, nm);
            tr_copy_s(myptr(s) + off, U32(s, 0x18) - off, p, count);
            crt_str_eos(s, newsize);
        }
    }
    return s;
}
RX_FUNCTION(0x78486aff, crt_str_replace_n)
rx_addr crt_str_replace_cstr(rx_addr s, uint32_t off, uint32_t n0, rx_addr p) {                   /* 78486c52 */
    return crt_str_replace_n(s, off, n0, p, crt_strlen(p));
}
RX_FUNCTION(0x78486c52, crt_str_replace_cstr)

/* ---- checked iterators ------------------------------------------------------------------------------------------------------
 * 7848592c _String_const_iterator(ptr, cont): cont and ptr checked (ptr within [_Myptr, _Myptr + _Mysize]). */
static int iter_ok(rx_addr ptr, rx_addr cont) {
    if (cont == 0 || ptr == 0) return 0;
    return myptr(cont) <= ptr && ptr <= myptr(cont) + mysize(cont);
}
static void iter_set(rx_addr it, rx_addr ptr, rx_addr cont) { SU32(it, 0, cont); SU32(it, 4, ptr); }
/* 78485a62 begin() / 78485ab4 end(): the iterator into *ret */
rx_addr crt_str_begin(rx_addr s, rx_addr ret) { iter_set(ret, myptr(s), s); return ret; }
RX_FUNCTION(0x78485a62, crt_str_begin)
rx_addr crt_str_end(rx_addr s, rx_addr ret) { iter_set(ret, myptr(s) + mysize(s), s); return ret; }
RX_FUNCTION(0x78485ab4, crt_str_end)
/* 784858f3 _Pdif(p2, p1): 0 for a null p2, else p2 - p1 (784850c1: the containers must match, _Mycont -4 passes) */
static int pdif_ok(rx_addr c2, rx_addr p2, rx_addr c1) {
    (void)p2;
    return c2 == 0xfffffffcu || (c2 != 0 && c2 == c1);
}
static uint32_t pdif(rx_addr c2, rx_addr p2, rx_addr c1, rx_addr p1) { (void)c2; (void)c1; return p2 == 0 ? 0 : p2 - p1; }
/* 78485f0e erase(first, last) -> iterator at the erased position */
rx_addr crt_str_erase_range(rx_addr s, rx_addr ret, rx_addr fc, rx_addr fp, rx_addr lc, rx_addr lp) {
    const rx_addr b = myptr(s);
    const uint32_t a[5] = {ret, fc, fp, lc, lp};
    if (!iter_ok(b, s) || (fp != 0 && !pdif_ok(fc, fp, s)) || (lp != 0 && !pdif_ok(lc, lp, fc)))
        return CRT_WHY("checked iterator"), crt_unported_this(0x78485f0e, s, 5, a);
    const uint32_t count = pdif(fc, fp, s, b), len = pdif(lc, lp, fc, fp);
    if (mysize(s) < count) return crt_unported_this(0x78485f0e, s, 5, a);                 /* _Xran */
    str_erase(s, count, len);
    iter_set(ret, myptr(s) + count, s);                       /* within [_Myptr, _Myptr + _Mysize]: the check passes */
    return ret;
}
RX_FUNCTION(0x78485f0e, crt_str_erase_range)
/* 78485f79 clear() = erase(begin(), end()) */
void crt_str_clear(rx_addr s) {
    if (!iter_ok(myptr(s), s)) { crt_unported_this(0x78485f79, s, 0, NULL); return; }
    str_erase(s, 0, mysize(s));
}
RX_FUNCTION(0x78485f79, crt_str_clear)
