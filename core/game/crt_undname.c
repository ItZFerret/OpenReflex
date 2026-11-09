/* crt_undname.c -- the first name of a type (unit 10 addition to the CRT layer, core/notes/crt.md): type_info's
 * _Name_base_internal 7857c2b8 and the part of msvcr90's undecorator (__unDName 78582b5e) the executable's type descriptors use.
 *
 * _Name_base_internal (this = the type descriptor, the __type_info_node list): under _lock(0xe), __unDNameHelper 7857c280 (malloc /
 * free, flags 0x2800 = 32-bit decode, type only) on the decorated name after its '.', the trailing blanks cut, a list node
 * (malloc(8) {name, next}) and the copy (malloc(length + 1), strcpy_s) linked at the list's head, the copy cached at this + 4, the
 * undecorator's string freed.
 * __unDName: _lock(5); the HeapManager static (0x785bc0e8: alloc, free, head block, current block, bytes left) reset; an UnDecorator
 * on its frame ({+0 -1, +0x2c -1}: the name and argument replicators; statics 0x785bc0fc this, 0x785bc100 this + 0x2c, 0x785bc108 /
 * 0x785bc10c the name cursor / start, 0x785bc110 / 0x785bc114 the output and its size, 0x785bc118 the flags, 0x785bc11c the
 * parameter callback, 0x785bc120 a byte); the decode (the nodes of the result in 0x1008-byte heap blocks taken from their end), the
 * output malloc(round8(length + 1)) with runs of blanks collapsed; the heap's blocks freed.
 * The grammar decoded here is the one of the executable's 114 non-class descriptors (scan in core/notes/construction.md 17): `P6A`
 * function pointers returning void with arguments of the basic types (C D E F G H I J K M N), `_N` bool / `_J` / `_K` 64-bit /
 * `_W` wchar_t, pointers and references `PA` / `PB` / `AA` / `AB`, `V` / `U` / `W4` class / struct / enum names (scoped, with name
 * back-references 0-9), argument back-references 0-9, `X` (void list) and the plain basic types (".H", ".M"). Anything else aborts
 * with the descriptor's name. The decoder's scratch nodes (the 0x1008-byte blocks, freed before the return) are not reproduced word
 * for word: only their allocation and release (known-differences.md entry 2). */
#include "crt.h"
#include "crt_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef RX_ORACLE
static uint32_t os1(uint32_t slot, uint32_t a0) { return rx_call_original(U32(0, slot), RX_STDCALL, 0, 1, &a0); }
#else
#include "../src/rx_os.h"
static uint32_t os1(uint32_t slot, uint32_t a0) { return rx_os_call(slot, 1, &a0); }
#endif
/* _lock / _unlock (78542efa / 78542e16) of an already created lock (_locktable 0x785b70c0); the lock is created by the CRT's
 * startup or the first _mtinitlocknum (78542e32): the reference boot has both 5 and 0xe created before the first type name */
static void lock(uint32_t n) {
    const rx_addr cs = U32(0x785b70c0u + 8u * n, 0);
    if (!cs) { fprintf(stderr, "rxcore: crt_undname: lock %u not created\n", n); abort(); }
    os1(0x78521048, cs);
}
static void unlock(uint32_t n) { os1(0x78521044, U32(0x785b70c0u + 8u * n, 0)); }
/* 78542e32 _mtinitlocknum(n): a lock created on first use: _malloc_crt(0x18) (785833ed), under _lock(10) the critical section
 * (__crtInitCritSecAndSpinCount 7858d583, spin count 4000: the object as the reference boot's kernel32 leaves it {0, -1, 0, 0, 0,
 * 4000}) stored in _locktable[n], or the block freed when another thread was first. The CRT heap is set up (__crtheap) here. */
static void mtinitlocknum(uint32_t n) {
    const rx_addr slot = 0x785b70c0u + 8u * n;
    if (U32(slot, 0)) return;
    if (!U32(0, 0x785bc160)) { fprintf(stderr, "rxcore: crt_undname: _mtinitlocknum before the CRT heap\n"); abort(); }
    const rx_addr cs = crt_malloc(0x18);
    if (!cs) { fprintf(stderr, "rxcore: crt_undname: _malloc_crt failed\n"); abort(); }
    lock(10);
    if (!U32(slot, 0)) {
        SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 4000);
        SU32(slot, 0, cs);
    } else crt_free(cs);
    unlock(10);
}

/* ---- the decoder (host strings; the result is written into game memory by the caller) ---- */
typedef struct { const char* t; uint32_t i; char names[10][64]; int nn; char args[10][160]; int na; int bad; } und_t;
static void und_fail(und_t* u) { u->bad = 1; }
static void und_name(und_t* u, char* out, size_t cap) {      /* Name@Scope@...@ (or digit back-references), "Scope::Name" */
    char parts[8][64]; int np = 0;
    for (;;) {
        const char c = u->t[u->i];
        if (c == '@') { u->i++; break; }
        if (!c || np == 8) { und_fail(u); return; }
        if (c >= '0' && c <= '9') { if (c - '0' >= u->nn) { und_fail(u); return; } snprintf(parts[np++], 64, "%s", u->names[c - '0']); u->i++; continue; }
        const char* e = strchr(u->t + u->i, '@');
        if (!e || e - (u->t + u->i) >= 63) { und_fail(u); return; }
        char n[64]; memcpy(n, u->t + u->i, (size_t)(e - (u->t + u->i))); n[e - (u->t + u->i)] = 0;
        u->i = (uint32_t)(e - u->t) + 1;
        int known = 0;
        for (int k = 0; k < u->nn; k++) if (!strcmp(u->names[k], n)) known = 1;
        if (!known && u->nn < 10) snprintf(u->names[u->nn++], 64, "%s", n);
        snprintf(parts[np++], 64, "%s", n);
    }
    out[0] = 0;
    for (int k = np - 1; k >= 0; k--) { strncat(out, parts[k], cap - strlen(out) - 1); if (k) strncat(out, "::", cap - strlen(out) - 1); }
}
static void und_type(und_t* u, char* out, size_t cap, int arg) {
    static const char* basic[26] = {['C' - 'A'] = "signed char", ['D' - 'A'] = "char", ['E' - 'A'] = "unsigned char", ['F' - 'A'] = "short",
        ['G' - 'A'] = "unsigned short", ['H' - 'A'] = "int", ['I' - 'A'] = "unsigned int", ['J' - 'A'] = "long", ['K' - 'A'] = "unsigned long",
        ['M' - 'A'] = "float", ['N' - 'A'] = "double", ['X' - 'A'] = "void"};
    const uint32_t start = u->i;
    const char c = u->t[u->i++];
    if (arg && c >= '0' && c <= '9') { if (c - '0' >= u->na) { und_fail(u); return; } snprintf(out, cap, "%s", u->args[c - '0']); return; }
    char inner[160], nm[128];
    if (c >= 'A' && c <= 'Z' && basic[c - 'A']) snprintf(out, cap, "%s", basic[c - 'A']);
    else if (c == '_') {
        const char e = u->t[u->i++];
        if (e == 'N') snprintf(out, cap, "bool"); else if (e == 'J') snprintf(out, cap, "__int64");
        else if (e == 'K') snprintf(out, cap, "unsigned __int64"); else if (e == 'W') snprintf(out, cap, "wchar_t");
        else { und_fail(u); return; }
    } else if (c == 'P' || c == 'A') {
        const char q = u->t[u->i++];
        if (q != 'A' && q != 'B') { und_fail(u); return; }
        und_type(u, inner, sizeof inner, 0);
        snprintf(out, cap, "%s%s %c", inner, q == 'B' ? " const" : "", c == 'P' ? '*' : '&');
    } else if (c == 'V' || c == 'U') { und_name(u, nm, sizeof nm); snprintf(out, cap, "%s %s", c == 'V' ? "class" : "struct", nm); }
    else if (c == 'W') {
        if (u->t[u->i++] != '4') { und_fail(u); return; }
        und_name(u, nm, sizeof nm); snprintf(out, cap, "enum %s", nm);
    } else { und_fail(u); return; }
    if (arg && u->i - start > 1 && u->na < 10) snprintf(u->args[u->na++], 160, "%s", out);
}
/* the undecorated type of a decorated name (after the descriptor's '.'): 1 when decoded, the length consumed in *used */
static int und_decode(const char* m, char* out, size_t cap, uint32_t* used) {
    und_t u; memset(&u, 0, sizeof u); u.t = m;
    if (!strncmp(m, "P6A", 3)) {
        char ret[160], a[160], list[512] = "";
        u.i = 3;
        und_type(&u, ret, sizeof ret, 0);
        if (u.t[u.i] == 'X') { u.i++; snprintf(list, sizeof list, "void"); }
        else {
            int first = 1;
            while (!u.bad && u.t[u.i] && u.t[u.i] != '@') {
                und_type(&u, a, sizeof a, 1);
                if (!first) strncat(list, ",", sizeof list - strlen(list) - 1);
                strncat(list, a, sizeof list - strlen(list) - 1); first = 0;
            }
            if (u.t[u.i] != '@') u.bad = 1; else u.i++;
        }
        if (u.t[u.i] != 'Z') u.bad = 1; else u.i++;
        snprintf(out, cap, "%s (__cdecl*)(%s)", ret, list);
    } else und_type(&u, out, cap, 0);
    if (u.bad || u.t[u.i]) return 0;
    *used = u.i;
    return 1;
}

/* __unDName(0, name, 0, malloc, free, 0x2800) as the type descriptors reach it: the result (a malloc block) or 0 */
static rx_addr undname(rx_addr name, rx_addr ud) {
    char m[512], out[1024];
    uint32_t n = 0;
    for (; n < sizeof m - 1 && U8(name, n); n++) m[n] = (char)U8(name, n);
    m[n] = 0;
    uint32_t used = 0;
    if (!und_decode(m, out, sizeof out, &used)) {
        fprintf(stderr, "rxcore: crt_undname: decorated name '%s' outside the ported grammar\n", m);
        abort();
    }
    mtinitlocknum(5);
    lock(5);
    SU32(0, 0x785bc0e8, 0x785839df); SU32(0, 0x785bc0ec, 0x785837ee);      /* HeapManager: malloc, free */
    SU32(0, 0x785bc0f8, 0); SU32(0, 0x785bc0f0, 0); SU32(0, 0x785bc0f4, 0);
    SU32(ud, 0, 0xffffffffu); SU32(ud, 0x2c, 0xffffffffu);                 /* UnDecorator 7857ea23 */
    SU32(0, 0x785bc10c, name); SU32(0, 0x785bc108, name);
    SU32(0, 0x785bc110, 0); SU32(0, 0x785bc114, 0);
    SU32(0, 0x785bc100, ud + 0x2c); SU32(0, 0x785bc118, 0x2800); SU32(0, 0x785bc0fc, ud); SU32(0, 0x785bc11c, 0);
    SU8(0, 0x785bc120, 0);
    /* the decode: the result's nodes from one heap block (HeapManager::getMemory 7857e713: blocks of 0x1004 bytes, rounded to 8) */
    const rx_addr blk = crt_malloc(0x1008);
    if (blk) {
        SU32(blk, 0, 0);
        SU32(0, 0x785bc0f0, blk); SU32(0, 0x785bc0f4, blk);
    }
    SU32(0, 0x785bc108, name + used);
    /* the output (785828e6): malloc(round8(length + 1)), blank runs collapsed (none in these names) */
    const uint32_t len = (uint32_t)strlen(out);
    SU32(0, 0x785bc114, len + 1);
    const rx_addr r = crt_malloc((len + 1 + 7) & ~7u);
    SU32(0, 0x785bc110, r);
    if (r) { for (uint32_t i = 0; i <= len; i++) SU8(r, i, (uint8_t)out[i]); }
    /* the heap's destructor 7857e16a: the blocks freed from the head */
    for (;;) {
        const rx_addr b = U32(0, 0x785bc0f0);
        SU32(0, 0x785bc0f4, b);
        if (!b) break;
        SU32(0, 0x785bc0f0, U32(b, 0));
        crt_free(b);
    }
    unlock(5);
    return r;
}

/* 7857c2b8 _Name_base_internal (cdecl this, list): the cached name, or the first one made and listed */
rx_addr crt_type_name_base(rx_addr self, rx_addr list) {
    if (U32(self, 4)) return U32(self, 4);
    lock(0xe);
    if (!U32(self, 4)) {
        const rx_addr ud = rx_sp() - 0xe0;                                    /* the UnDecorator on __unDName's frame ([ebp - 0x74]) */
        const rx_addr r = undname(self + 9, ud);
        if (!r) { unlock(0xe); return 0; }
        uint32_t i = crt_strlen(r);
        while (i > 0 && U8(r, i - 1) == ' ') { SU8(r, i - 1, 0); i--; }
        const rx_addr node = crt_malloc(8);
        if (node) {
            const rx_addr copy = crt_malloc(i + 1);
            if (copy) {
                crt_strcpy_s(copy, i + 1, r);
                SU32(self, 4, copy);
                SU32(node, 0, copy);
                SU32(node, 4, U32(list, 4));
                SU32(list, 4, node);
            } else crt_free(node);
        }
        crt_free(r);
    }
    unlock(0xe);
    return U32(self, 4);
}
