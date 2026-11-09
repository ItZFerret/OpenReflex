/* build_tables_rdb.c -- the Rdb core of unit 12a (build_tables.h): tables, columns, cells and their indexes, transliterated from the
 * original (0x004dc000..0x00508000). The game instantiates the MSVC 9 containers many times over (one copy of std::_Tree per node
 * type, of std::vector per element type); their code is the same template, so each template is written once here over a node
 * descriptor and every instantiation's address is named where it is used. Order of allocations, links, colors, sizes and values as
 * the original; the allocator lock and the cell critical section (0x00ae1c44) are not taken (one thread: they end as they began). */
#include "build_tables.h"
#include "build_alloc.h"
#include "build_vehicle.h"
#include "crt.h"
#include "build_tables_sort.h"
static int row_less_key(rx_addr set,uint32_t left,uint32_t right);
static void sort_columns_destroy(rx_addr vector);
static rx_addr sort_columns_copy(rx_addr vector);
#include <stdio.h>
#include <stdlib.h>

/* frames at the original's stack addresses (stack temporaries whose addresses reach the heap: the proxies of std::set / std::vector
 * locals): F = the original's ESP after its prologue, from the entry ESP rx_sp() (the return address slot) */
#define TF_PLAIN(F, sub, pushes) const rx_addr F##_sp = rx_sp(); const rx_addr F = F##_sp - (sub) - 4u * (pushes); rx_set_sp(F)
#define TF_EBP(F, align, sub, pushes) const rx_addr F##_sp = rx_sp(); \
    const rx_addr F = ((F##_sp - 4) & ~((rx_addr)(align) - 1)) - (sub) - 4u * (pushes); rx_set_sp(F)
#define TF_END(F) rx_set_sp(F##_sp)
/* a call of a core callee with n stack arguments: its entry ESP as the original's call (the arguments and the return address below
 * the current stack pointer, which must be the caller's ESP at the pushes) */
#define TF_CALL(nargs, call) do { const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 4u * (nargs) - 4u); call; rx_set_sp(sp_); } while (0)

static void unreached(uint32_t addr, const char* what) {
    fprintf(stderr, "rxcore: build_tables: %08x %s: path not in the reference boot (not ported)\n", addr, what);
    abort();
}

/* the game's _aligned_malloc(n, 16) / _aligned_free sites (004f4a30, 004f4f40, 004f52e0, 004fead0, 004feee0, 004f5290, 00506040 ..
 * 00506180, 004f3320 ...: FUN_004dc0f0 first, then the CRT under the lock) */
static rx_addr game_alloc(uint32_t n) { rx_tbl_alloc_init(); return rx_crt_aligned_malloc(n, 0x10); }
static void    game_free(rx_addr p) { rx_tbl_alloc_init(); rx_crt_aligned_free(p); }
/* StdMemoryAllocator::vf1 004eef20 (rider unit's rx_std_allocator_alloc: _aligned_malloc(n, 16)), after the callers' FUN_004dc0f0 */
static rx_addr rx_std_alloc(uint32_t n) { rx_tbl_alloc_init(); return rx_crt_aligned_malloc(n, 0x10); }

/* 004dc0f0: the static allocator table at 0x00d92d58 (eight {0, 1, 0, 0, 0, -1} records, then the 8 x 0x18 objects at 0x00d92e18
 * built by 0041b310) under the guard bit 0x00d92ed8; its destructor (0x00911fc0) goes to atexit: the core never exits. */
void rx_tbl_alloc_init(void) {
    if (U32(0, 0x00d92ed8) & 1) return;
    SU32(0, 0x00d92ed8, U32(0, 0x00d92ed8) | 1);
    for (uint32_t k = 0; k < 8; k++) {
        rx_addr r = 0x00d92d58 + 0x18 * k;
        SU32(r, 0, 0); SU32(r, 4, 1); SU32(r, 8, 0); SU32(r, 0xc, 0); SU32(r, 0x10, 0); SU32(r, 0x14, 0xffffffffu);
    }
    /* `vector_constructor_iterator'(0x00d92e18, 0x18, 8, 0041b310): 0041b310 (ECX = object) */
    for (uint32_t k = 0; k < 8; k++) {
        rx_addr o = 0x00d92e18 + 0x18 * k;   /* 0041b310: +0 0, +4 0, +8 1, +0xc -1 (the allocator records' second form) */
        SU32(o, 0, 0); SU32(o, 4, 0); SU32(o, 8, 0); SU32(o, 0xc, 0); SU32(o, 0x10, 0); SU32(o, 0x14, 0);
    }
}

/* ======================================================================================================================== */
/* the game's std::basic_string<char> (allocator: _aligned_malloc 16)                                                        */

rx_addr rx_tbl_str_cstr(rx_addr s) { return U32(s, 0x18) >= 0x10 ? U32(s, 4) : s + 4; }
void rx_tbl_str_init(rx_addr s) { SU32(s, 0x18, 0xf); SU32(s, 0x14, 0); SU8(s, 4, 0); }

/* StdString_Copy 004f48e0 + its tail 004f498c (_Copy(newsize, oldlen)) */
static void str_copy(rx_addr s, uint32_t newsize, uint32_t oldlen) {
    uint32_t newres = newsize | 0xf;
    if (newres > 0xfffffffeu) newres = newsize;
    else {
        uint32_t cap = U32(s, 0x18), half = cap >> 1;
        if (half > newres / 3 && cap <= 0xfffffffeu - half) newres = half + cap;
    }
    rx_addr p = game_alloc(newres + 1);          /* 004f5290 */
    if (oldlen) crt_memcpy(p, rx_tbl_str_cstr(s), oldlen);   /* memcpy_s(p, newres + 1, old, oldlen) */
    if (U32(s, 0x18) >= 0x10) game_free(U32(s, 4));          /* 004f3320 */
    SU8(s, 4, 0);
    SU32(s, 4, p);
    SU32(s, 0x18, newres);
    SU32(s, 0x14, oldlen);
    SU8(newres >= 0x10 ? p : s + 4, oldlen, 0);
}
/* 004f2df0 _Grow(newsize) (EAX = s, EDI = newsize): 1 when there is something to copy */
static int str_grow(rx_addr s, uint32_t newsize) {
    if (newsize > 0xfffffffeu) unreached(0x004f2df8, "string _Xlen");
    if (U32(s, 0x18) < newsize) { str_copy(s, newsize, U32(s, 0x14)); return newsize != 0; }
    if (newsize == 0) { SU32(s, 0x14, 0); SU8(rx_tbl_str_cstr(s), 0, 0); return 0; }
    return 1;
}
/* 004f1860 assign(right, pos, n) */
void rx_tbl_str_assign_sub(rx_addr s, rx_addr r, uint32_t pos, uint32_t n) {
    if (U32(r, 0x14) < pos) unreached(0x004f1873, "string _Xran");
    uint32_t num = U32(r, 0x14) - pos;
    if (n < num) num = n;
    if (s == r) unreached(0x004f188b, "string self-assign");   /* erase(pos + num), erase(0, pos) (FUN_004f2d70) */
    if (str_grow(s, num)) {
        crt_memcpy(rx_tbl_str_cstr(s), rx_tbl_str_cstr(r) + pos, num);   /* memcpy_s(buf, cap, src + pos, num) */
        SU32(s, 0x14, num);
        SU8(rx_tbl_str_cstr(s), num, 0);
    }
}
/* 004f2ce0 assign(ptr, n) */
void rx_tbl_str_assign_n(rx_addr s, rx_addr p, uint32_t n) {
    if (p) {
        rx_addr b = rx_tbl_str_cstr(s);
        if (p >= b && b + U32(s, 0x14) > p) { rx_tbl_str_assign_sub(s, s, p - b, n); return; }
    }
    if (str_grow(s, n)) {
        crt_memcpy(rx_tbl_str_cstr(s), p, n);
        SU32(s, 0x14, n);
        SU8(rx_tbl_str_cstr(s), n, 0);
    }
}
/* the string destructor inlined everywhere (e.g. 004eede5): the heap buffer freed, then cap 0xf, size 0, buf[0] 0 */
void rx_tbl_str_tidy(rx_addr s) {
    if (U32(s, 0x18) >= 0x10) game_free(U32(s, 4));
    SU32(s, 0x18, 0xf); SU32(s, 0x14, 0); SU8(s, 4, 0);
}
/* 004dc440 (ESI = s; right; RET 4) / 00507f70: the copy constructor (empty, then assign(right, 0, npos)) */
static void str_copy_construct(rx_addr s, rx_addr r) {
    SU32(s, 0x14, 0); SU32(s, 0x18, 0xf); SU8(s, 4, 0);
    rx_tbl_str_assign_sub(s, r, 0, 0xffffffffu);
}

/* ======================================================================================================================== */
/* std::_Tree (MSVC 9) over game memory                                                                                       */

enum { K_INT, K_PCHAR, K_STR, K_ROW };
typedef struct {
    uint32_t nsize, color, isnil;   /* node size and the two flag bytes */
    int      kind;                  /* key type: int (signed <), const char* (_stricmp), std::string (_stricmp of c_str), RowRef (row <) */
} tdesc;
/* the instantiations */
static const tdesc T_NAME   = {0x18, 0x14, 0x15, K_PCHAR};   /* map<const char*, int | Table*>: 004f4e00 head, 004f4a80 node, 004f3740 _Insert */
static const tdesc T_ROWSET = {0x1c, 0x18, 0x19, K_ROW};     /* set<RowRef>: 004f4ea0 head, 004f3e60 node, 004f2090 _Insert, 004f38d0 erase */
static const tdesc T_INTIDX = {0x3c, 0x38, 0x39, K_INT};     /* map<int, set<RowRef>>: 00505140 head, 00504ee0 node, 00502e30 _Insert */
static const tdesc T_INTUNI = {0x20, 0x1c, 0x1d, K_INT};     /* map<int, RowRef>: 00505310 head, 005053b0 node, 005035c0 _Insert */
static const tdesc T_STRIDX = {0x54, 0x50, 0x51, K_STR};     /* map<string, set<RowRef>>: 00504db0 head, 00506370 node, 00502ac0 _Insert */
static const tdesc T_STRUNI = {0x38, 0x34, 0x35, K_STR};     /* map<string, RowRef>: 00504ba0 head, 00506310 node, 005026e0 _Insert */

#define LEFT(n)   U32(n, 0)
#define PARENT(n) U32(n, 4)
#define RIGHT(n)  U32(n, 8)
static inline int isnil(const tdesc* T, rx_addr n) { return U8(n, T->isnil) != 0; }
static inline uint8_t color(const tdesc* T, rx_addr n) { return U8(n, T->color); }
static inline void setcolor(const tdesc* T, rx_addr n, uint8_t c) { SU8(n, T->color, c); }
#define HEAD(m) U32(m, 0x18)
#define ROOT(T, m) PARENT(HEAD(m))

/* a key: an int / row value or a game C string */
typedef struct { uint32_t u; rx_addr s; } tkey;
static tkey node_key(const tdesc* T, rx_addr n) {
    tkey k = {0, 0};
    switch (T->kind) {
    case K_INT: k.u = U32(n, 0xc); break;
    case K_ROW: k.u=U32(n,0xc); k.s=U32(n,0x10); break;
    case K_PCHAR: k.s = U32(n, 0xc); break;
    default: k.s = rx_tbl_str_cstr(n + 0xc); break;
    }
    return k;
}
static int key_less(const tdesc* T, tkey a, tkey b) {
    switch (T->kind) {
    case K_INT: return (int32_t)a.u < (int32_t)b.u;
    case K_ROW: return row_less_key(a.s,a.u,b.u);            /* 004ec2c0 with no sort column: cmp / sbb on the row indices */
    default: return crt_stricmp(a.s, b.s) < 0;
    }
}

/* the head node (_Buyheadnode): links 0, color black, isnil 0, then the constructor makes it the nil sentinel */
static rx_addr tree_buy_head(const tdesc* T) {
    rx_addr h = game_alloc(T->nsize);
    if (h) { SU32(h, 0, 0); SU32(h, 4, 0); SU32(h, 8, 0); }
    SU8(h, T->color, 1); SU8(h, T->isnil, 0);
    return h;
}
/* a container's constructor (004ec4f0's column map, 004fcf20 / 004fc9a0 / 004fb940 / 004fbe40, 004dc5c0's set): the proxy, the
 * head linked to itself, isnil 1, size 0 */
static void tree_construct(const tdesc* T, rx_addr m) {
    rx_addr px = game_alloc(4);               /* 004f4a30 */
    if (px) SU32(px, 0, m);
    SU32(m, 0, px);
    rx_addr h = tree_buy_head(T);
    SU32(m, 0x18, h);
    SU8(h, T->isnil, 1);
    SU32(h, 4, h); SU32(h, 0, h); SU32(h, 8, h);
    SU32(m, 0x1c, 0);
}

static rx_addr tree_min(const tdesc* T, rx_addr n) { while (!isnil(T, LEFT(n))) n = LEFT(n); return n; }      /* 00418920 */
static rx_addr tree_max(const tdesc* T, rx_addr n) { while (!isnil(T, RIGHT(n))) n = RIGHT(n); return n; }    /* 004f3df0 */
/* ++it (00417df0 / 00503440 / 00503750 ...) */
static rx_addr tree_next(const tdesc* T, rx_addr n) {
    if (isnil(T, n)) return n;
    if (!isnil(T, RIGHT(n))) return tree_min(T, RIGHT(n));
    rx_addr p;
    while (!isnil(T, p = PARENT(n)) && n == RIGHT(p)) n = p;
    return p;
}
/* --it (006909c0 / 00505280 / 00505450 ...) */
static rx_addr tree_prev(const tdesc* T, rx_addr n) {
    if (isnil(T, n)) return RIGHT(n);
    if (!isnil(T, LEFT(n))) return tree_max(T, LEFT(n));
    rx_addr p;
    while (!isnil(T, p = PARENT(n)) && n == LEFT(p)) n = p;
    return isnil(T, n) ? n : p;
}
static void lrotate(const tdesc* T, rx_addr m, rx_addr w) {       /* 00690540 / 004ffe80 / 004f3da0 ... (_Lrotate) */
    rx_addr p = RIGHT(w);
    SU32(w, 8, LEFT(p));
    if (!isnil(T, LEFT(p))) SU32(LEFT(p), 4, w);
    SU32(p, 4, PARENT(w));
    if (w == ROOT(T, m)) SU32(HEAD(m), 4, p);
    else if (w == LEFT(PARENT(w))) SU32(PARENT(w), 0, p);
    else SU32(PARENT(w), 8, p);
    SU32(p, 0, w);
    SU32(w, 4, p);
}
static void rrotate(const tdesc* T, rx_addr m, rx_addr w) {       /* 00705020 / 00500480 / 004f3e10 ... (_Rrotate) */
    rx_addr p = LEFT(w);
    SU32(w, 0, RIGHT(p));
    if (!isnil(T, RIGHT(p))) SU32(RIGHT(p), 4, w);
    SU32(p, 4, PARENT(w));
    if (w == ROOT(T, m)) SU32(HEAD(m), 4, p);
    else if (w == RIGHT(PARENT(w))) SU32(PARENT(w), 8, p);
    else SU32(PARENT(w), 0, p);
    SU32(p, 8, w);
    SU32(w, 4, p);
}

/* the value constructor of a new node (_Buynode: links, the value, color red, isnil 0) */
typedef void (*tvalue_fn)(rx_addr node, const void* ctx);
static rx_addr buy_node(const tdesc* T, rx_addr head, rx_addr parent, tvalue_fn val, const void* ctx) {
    rx_addr n = game_alloc(T->nsize);
    if (n) {
        SU32(n, 0, head); SU32(n, 4, parent); SU32(n, 8, head);
        val(n, ctx);
        SU8(n, T->color, 0); SU8(n, T->isnil, 0);
    }
    return n;
}
/* _Insert(addleft, where, val) (004f3740 / 004f2090 / 00502e30 / 005035c0 / 00502ac0 / 005026e0): the node linked and the tree
 * rebalanced; returns the node */
static rx_addr tree_insert_at(const tdesc* T, rx_addr m, int addleft, rx_addr where, tvalue_fn val, const void* ctx) {
    if (U32(m, 0x1c) >= 0x05d1745cu) unreached(0x00502e41, "tree _Xlen");
    rx_addr head = HEAD(m);
    rx_addr nn = buy_node(T, head, where, val, ctx);
    SU32(m, 0x1c, U32(m, 0x1c) + 1);
    if (where == head) { SU32(head, 4, nn); SU32(head, 0, nn); SU32(head, 8, nn); }
    else if (addleft) { SU32(where, 0, nn); if (where == LEFT(head)) SU32(head, 0, nn); }
    else { SU32(where, 8, nn); if (where == RIGHT(head)) SU32(head, 8, nn); }
    for (rx_addr p = nn; color(T, PARENT(p)) == 0;) {
        rx_addr pp = PARENT(p), gp = PARENT(pp);
        if (pp == LEFT(gp)) {
            rx_addr w = RIGHT(gp);
            if (color(T, w) == 0) { setcolor(T, pp, 1); setcolor(T, w, 1); setcolor(T, gp, 0); p = gp; }
            else {
                if (p == RIGHT(pp)) { p = pp; lrotate(T, m, p); }
                setcolor(T, PARENT(p), 1); setcolor(T, PARENT(PARENT(p)), 0); rrotate(T, m, PARENT(PARENT(p)));
            }
        } else {
            rx_addr w = LEFT(gp);
            if (color(T, w) == 0) { setcolor(T, pp, 1); setcolor(T, w, 1); setcolor(T, gp, 0); p = gp; }
            else {
                if (p == LEFT(pp)) { p = pp; rrotate(T, m, p); }
                setcolor(T, PARENT(p), 1); setcolor(T, PARENT(PARENT(p)), 0); lrotate(T, m, PARENT(PARENT(p)));
            }
        }
    }
    setcolor(T, ROOT(T, m), 1);
    return nn;
}
/* lower_bound (00417da0 / 00502870 / 00502c50 / the inline searches of 004fc440 / 004fc5f0 / 00419ea0 / 00763270) */
static rx_addr tree_lower_bound(const tdesc* T, rx_addr m, tkey k) {
    rx_addr head = HEAD(m), n = PARENT(head), res = head;
    while (!isnil(T, n)) {
        if (key_less(T, node_key(T, n), k)) n = RIGHT(n);
        else { res = n; n = LEFT(n); }
    }
    return res;
}
/* find (00419ea0 int / 00763270 int unique / 004fc3a0 / 004fbda0 string): the lower bound unless it is the end or k < its key */
static rx_addr tree_find(const tdesc* T, rx_addr m, tkey k) {
    rx_addr lb = tree_lower_bound(T, m, k);
    if (lb == HEAD(m) || key_less(T, k, node_key(T, lb))) return HEAD(m);
    return lb;
}
/* insert(val) for unique keys (004f06c0 set<RowRef>): returns the node, *inserted */
static rx_addr tree_insert_unique(const tdesc* T, rx_addr m, tkey k, tvalue_fn val, const void* ctx, int* inserted) {
    rx_addr head = HEAD(m), try_ = PARENT(head), where = head;
    int addleft = 1;
    while (!isnil(T, try_)) {
        where = try_;
        addleft = key_less(T, k, node_key(T, try_));
        try_ = addleft ? LEFT(try_) : RIGHT(try_);
    }
    rx_addr it = where;
    if (addleft) {
        if (it == LEFT(head)) { *inserted = 1; return tree_insert_at(T, m, 1, where, val, ctx); }
        it = tree_prev(T, it);
    }
    if (key_less(T, node_key(T, it), k)) { *inserted = 1; return tree_insert_at(T, m, addleft, where, val, ctx); }
    *inserted = 0;
    return it;
}
/* insert(hint, val) (004f1c00 / 004ffcb0 / 004fff20 / 004ff460 / 004ff8a0): the hinted insert of a key not in the tree */
static rx_addr tree_insert_hint(const tdesc* T, rx_addr m, rx_addr where, tkey k, tvalue_fn val, const void* ctx) {
    rx_addr head = HEAD(m);
    if (U32(m, 0x1c) == 0) return tree_insert_at(T, m, 1, head, val, ctx);
    if (where == LEFT(head)) {
        if (key_less(T, k, node_key(T, where))) return tree_insert_at(T, m, 1, where, val, ctx);
    } else if (where == head) {
        if (key_less(T, node_key(T, RIGHT(head)), k)) return tree_insert_at(T, m, 0, RIGHT(head), val, ctx);
    } else if (key_less(T, k, node_key(T, where))) {
        rx_addr next = tree_prev(T, where);
        if (key_less(T, node_key(T, next), k)) {
            if (isnil(T, RIGHT(next))) return tree_insert_at(T, m, 0, next, val, ctx);
            return tree_insert_at(T, m, 1, where, val, ctx);
        }
    }
    unreached(0x004f1d6b, "hinted insert falling back to insert(val)");
    return 0;
}

typedef void (*tdtor_fn)(rx_addr node);
/* erase(it) (004f38d0 / 004fc6b0 / 004fcb60 / 004fba90 / 004fc050): returns the next node */
static rx_addr tree_erase(const tdesc* T, rx_addr m, rx_addr erased, tdtor_fn dtor) {
    if (isnil(T, erased)) unreached(0x004f38e1, "erase(end)");
    rx_addr head = HEAD(m);
    rx_addr next = tree_next(T, erased);
    rx_addr fix, fixparent, p = erased;
    if (isnil(T, LEFT(p))) fix = RIGHT(p);
    else if (isnil(T, RIGHT(p))) fix = LEFT(p);
    else { p = next; fix = RIGHT(p); }
    if (p == erased) {
        fixparent = PARENT(erased);
        if (!isnil(T, fix)) SU32(fix, 4, fixparent);
        if (ROOT(T, m) == erased) SU32(head, 4, fix);
        else if (LEFT(fixparent) == erased) SU32(fixparent, 0, fix);
        else SU32(fixparent, 8, fix);
        if (LEFT(head) == erased) SU32(head, 0, isnil(T, fix) ? fixparent : tree_min(T, fix));
        if (RIGHT(head) == erased) SU32(head, 8, isnil(T, fix) ? fixparent : tree_max(T, fix));
    } else {
        SU32(LEFT(erased), 4, p);
        SU32(p, 0, LEFT(erased));
        if (p == RIGHT(erased)) fixparent = p;
        else {
            fixparent = PARENT(p);
            if (!isnil(T, fix)) SU32(fix, 4, fixparent);
            SU32(fixparent, 0, fix);
            SU32(p, 8, RIGHT(erased));
            SU32(RIGHT(erased), 4, p);
        }
        if (ROOT(T, m) == erased) SU32(head, 4, p);
        else if (LEFT(PARENT(erased)) == erased) SU32(PARENT(erased), 0, p);
        else SU32(PARENT(erased), 8, p);
        SU32(p, 4, PARENT(erased));
        uint8_t c = color(T, p); setcolor(T, p, color(T, erased)); setcolor(T, erased, c);
    }
    if (color(T, erased) == 1) {
        for (; fix != ROOT(T, m) && color(T, fix) == 1; fix = fixparent, fixparent = PARENT(fix)) {
            if (fix == LEFT(fixparent)) {
                p = RIGHT(fixparent);
                if (color(T, p) == 0) { setcolor(T, p, 1); setcolor(T, fixparent, 0); lrotate(T, m, fixparent); p = RIGHT(fixparent); }
                if (isnil(T, p)) fix = fixparent;
                else if (color(T, LEFT(p)) == 1 && color(T, RIGHT(p)) == 1) { setcolor(T, p, 0); fix = fixparent; }
                else {
                    if (color(T, RIGHT(p)) == 1) { setcolor(T, LEFT(p), 1); setcolor(T, p, 0); rrotate(T, m, p); p = RIGHT(fixparent); }
                    setcolor(T, p, color(T, fixparent)); setcolor(T, fixparent, 1); setcolor(T, RIGHT(p), 1);
                    lrotate(T, m, fixparent);
                    break;
                }
            } else {
                p = LEFT(fixparent);
                if (color(T, p) == 0) { setcolor(T, p, 1); setcolor(T, fixparent, 0); rrotate(T, m, fixparent); p = LEFT(fixparent); }
                if (isnil(T, p)) fix = fixparent;
                else if (color(T, RIGHT(p)) == 1 && color(T, LEFT(p)) == 1) { setcolor(T, p, 0); fix = fixparent; }
                else {
                    if (color(T, LEFT(p)) == 1) { setcolor(T, RIGHT(p), 1); setcolor(T, p, 0); lrotate(T, m, p); p = LEFT(fixparent); }
                    setcolor(T, p, color(T, fixparent)); setcolor(T, fixparent, 1); setcolor(T, LEFT(p), 1);
                    rrotate(T, m, fixparent);
                    break;
                }
            }
        }
        setcolor(T, fix, 1);
    }
    if (dtor) dtor(erased);
    game_free(erased);
    if (U32(m, 0x1c)) SU32(m, 0x1c, U32(m, 0x1c) - 1);
    return next;
}
/* _Erase(root) (004f2020 / 005003a0 ...): the subtree freed, right first */
static void tree_erase_subtree(const tdesc* T, rx_addr n, tdtor_fn dtor) {
    for (rx_addr p = n; !isnil(T, p); n = p) {
        tree_erase_subtree(T, RIGHT(p), dtor);
        p = LEFT(p);
        if (dtor) dtor(n);
        game_free(n);
    }
}
/* clear(): _Erase(root), the head relinked, size 0 (004ebe60's first half, 004f91e0) */
static void tree_clear(const tdesc* T, rx_addr m, tdtor_fn dtor) {
    tree_erase_subtree(T, ROOT(T, m), dtor);
    rx_addr h = HEAD(m);
    SU32(h, 4, h); SU32(m, 0x1c, 0); SU32(h, 0, h); SU32(h, 8, h);
}
/* erase(first, last) of the whole tree + the head freed (004f2220 / 004f1eb0 with begin..end): the container's destructor body */
static void tree_destroy(const tdesc* T, rx_addr m, tdtor_fn dtor) {
    tree_clear(T, m, dtor);            /* erase(begin(), end()) = clear() (004f1eb0's first path) */
    game_free(HEAD(m));
    SU32(m, 0x1c, 0); SU32(m, 0x18, 0);
}

/* ======================================================================================================================== */
/* std::set<RowRef> (0x28: the tree, +0x20 the sort column, +0x24 the table)                                                 */

typedef struct { uint32_t row; rx_addr set, table; } rowref;
static void v_rowref(rx_addr n, const void* c) { const rowref* r = (const rowref*)c; SU32(n, 0xc, r->row); SU32(n, 0x10, r->set); SU32(n, 0x14, r->table); }

/* 004f06c0 set::insert(val) (EAX = set; out, &val; RET 8) */
static void rowset_insert(rx_addr set, rowref r) {
    int ins; tkey k = {r.row, r.set};
    tree_insert_unique(&T_ROWSET, set, k, v_rowref, &r, &ins);
}
/*004f06c0 exact sorted-all call boundary: EAX=&RowRef; stack set,out.
 * The generic unique-tree algorithm preserves comparisons/allocation order.
 * RowRef.set is the caller's actual guest set address, never a host pointer. */
static void rowset_insert_result(rx_addr set,rx_addr value,rx_addr out) {
    TF_PLAIN(I,0xc,4);
    const rowref r={U32(value,0),U32(value,4),U32(value,8)};
    int inserted=0;const tkey key={r.row,r.set};
    const rx_addr node=tree_insert_unique(&T_ROWSET,set,key,v_rowref,&r,&inserted);
    SU32(out,0,U32(set,0));SU32(out,4,node);SU8(out,8,(uint8_t)inserted);
    TF_END(I);
}
/* 004dc5c0 (ESI = set, EDI = table; row; RET 4): a set holding one row */
static void rowset_construct_one(rx_addr set, rx_addr table, uint32_t row) {
    tree_construct(&T_ROWSET, set);
    SU32(set, 0x1c, 0); SU32(set, 0x20, 0); SU32(set, 0x24, table);
    rowref r = {row, set, table};
    rowset_insert(set, r);
}
/* 004dc630 (EAX = set, ECX = row): insert {row, set, table} */
static void rowset_add(rx_addr set, uint32_t row) { rowref r = {row, set, U32(set, 0x24)}; rowset_insert(set, r); }
/* 004f0800 (ECX = &key, ESI = set): erase(key) = erase(equal_range(key)) (004f3c60 + 004faa00 + 004f1eb0) */
static void rowset_erase_key(rx_addr set, uint32_t row) {
    tkey k = {row, set};
    rx_addr first = tree_lower_bound(&T_ROWSET, set, k), last = first;
    while (last != HEAD(set) && !key_less(&T_ROWSET, k, node_key(&T_ROWSET, last))) last = tree_next(&T_ROWSET, last);
    if (first == LEFT(HEAD(set)) && last == HEAD(set)) { tree_clear(&T_ROWSET, set, 0); return; }   /* 004f1eb0's clear() path */
    while (first != last) first = tree_erase(&T_ROWSET, set, first, 0);
}
/* 004f0670 (EAX = set): the destructor (004f2220 then the proxy freed) */
static void rowset_destroy(rx_addr set) {
    tree_destroy(&T_ROWSET, set, 0);
    game_free(U32(set, 0));
}
/* 004ebe60 (EDI = this; right; RET 4) set::operator=: table copied, cleared, right's rows inserted with this as their set (004ebee0) */
static void rowset_assign(rx_addr self, rx_addr right) {
    SU32(self, 0x24, U32(right, 0x24));
    if(U32(self,0x20)){sort_columns_destroy(U32(self,0x20));SU32(self,0x20,0);}
    if(U32(right,0x20))SU32(self,0x20,sort_columns_copy(U32(right,0x20)));
    tree_clear(&T_ROWSET, self, 0);
    for (rx_addr n = LEFT(HEAD(right)); n != HEAD(right); n = tree_next(&T_ROWSET, n)) {
        rowref r = {U32(n, 0xc), self, U32(self, 0x24)};
        rowset_insert(self, r);
    }
}

/* index node destructors (the pair's value / key) */
/* ~RowSet in the index nodes (005003a0 / 004fcb60 / 004fc050 ...): a sorted set's extra (004ebe10), the table word cleared, then the
 * tree and the proxy (the cleared word stays in the freed node) */
static void rowset_destroy_member(rx_addr set) {
    if(U32(set,0x20)){sort_columns_destroy(U32(set,0x20));SU32(set,0x20,0);}
    SU32(set, 0x24, 0);
    rowset_destroy(set);
}
static void d_intidx(rx_addr n) { rowset_destroy_member(n + 0x10); }
static void d_struni(rx_addr n) { rx_tbl_str_tidy(n + 0xc); }
static void d_stridx(rx_addr n) { rowset_destroy_member(n + 0x28); rx_tbl_str_tidy(n + 0xc); }

/* the pair values of new index nodes: {key, an empty set} (the default of operator[]: 004fc440 builds a temporary set, copies the
 * pair (004ebce0 / 004fe460) into the node (00504ee0 / 00506370) and destroys the temporaries; the node's set gets its own proxy and
 * head) */
typedef struct { uint32_t key; rx_addr skey; } idxval;
static void v_intidx(rx_addr n, const void* c) {
    const idxval* v = (const idxval*)c;
    SU32(n, 0xc, v->key);
    tree_construct(&T_ROWSET, n + 0x10);
    SU32(n + 0x10, 0x1c, 0); SU32(n + 0x10, 0x20, 0); SU32(n + 0x10, 0x24, 0);
}
static void v_stridx(rx_addr n, const void* c) {
    const idxval* v = (const idxval*)c;
    str_copy_construct(n + 0xc, v->skey);
    tree_construct(&T_ROWSET, n + 0x28);
    SU32(n + 0x28, 0x1c, 0); SU32(n + 0x28, 0x20, 0); SU32(n + 0x28, 0x24, 0);
}
static void v_intuni(rx_addr n, const void* c) {
    const idxval* v = (const idxval*)c;
    SU32(n, 0xc, v->key); SU32(n, 0x10, 0xffffffffu); SU32(n, 0x14, 0); SU32(n, 0x18, 0);   /* {key, RowRef()} = {-1, 0, 0} */
}
static void v_struni(rx_addr n, const void* c) {
    const idxval* v = (const idxval*)c;
    str_copy_construct(n + 0xc, v->skey);
    SU32(n, 0x28, 0xffffffffu); SU32(n, 0x2c, 0); SU32(n, 0x30, 0);
}
/* operator[](key) of an index (004fc440 int / 004fbea0 string searchable, 004fc5f0 int / 004fb980 string unique): the node (inserted
 * with the lower bound as hint). A new key is inserted the MSVC way: the default value is built in temporaries first (for the
 * searchable indexes an empty set S1 and the pair {key, copy of S1}; for string keys the pair's key string), the node copy-
 * constructed from the pair, then the temporaries destroyed: their blocks come and go in the original's order (heap layout).
 * slots = the stack slots the caller has below its current stack pointer at the call (pushed arguments + the return address): the
 * temporaries sit at the original's frame (004fc440: sub 0x60 + 4 pushes, S1 +0x1c, pair +0x44; 004fbea0: sub 0x78 + 4, S1 +0x1c,
 * pair +0x44, its set +0x60), so the stack addresses their proxies hold are the original's. */
static void rowset_insert_raw(rx_addr set, rowref r);
/* 004ebce0 (EDI = the new set; stdcall the source set; RET 4) -> EAX = the new set: std::set<RowRef>'s copy constructor: the proxy and
 * head (004f4a30 / 004f4ea0), no rows, the table copied, the sort columns (+0x20) copied (the vector assign 004f0940), then each of
 * the source's rows inserted in order (004f06c0) with the new set as the reference's set. */
static void rowset_copy_construct(rx_addr set, rx_addr src) {
    tree_construct(&T_ROWSET, set);
    SU32(set, 0x1c, 0); SU32(set, 0x20, 0); SU32(set, 0x24, U32(src, 0x24));
    if(U32(src,0x20))SU32(set,0x20,sort_columns_copy(U32(src,0x20)));
    for (rx_addr n = LEFT(HEAD(src)); n != HEAD(src); n = tree_next(&T_ROWSET, n)) {      /* 00417df0 */
        rowref r = {U32(n, 0xc), set, U32(set, 0x24)};
        rowset_insert_raw(set, r);
    }
}
rx_addr rx_tbl_rowset_copy_construct(rx_addr set, rx_addr src) {
    TF_PLAIN(I, 0x24, 3);
    rowset_copy_construct(set, src);
    TF_END(I);
    return set;
}
RX_FUNCTION(0x004ebce0, rx_tbl_rowset_copy_construct)
static rx_addr idx_index_at(const tdesc* T, rx_addr m, tkey k, tvalue_fn val, const idxval* v, uint32_t slots) {
    rx_addr lb = tree_lower_bound(T, m, k);
    if (lb != HEAD(m) && !key_less(T, k, node_key(T, lb))) return lb;
    rx_addr n;
    const rx_addr sp0 = rx_sp(), entry = sp0 - 4u * slots;
    if (T == &T_INTIDX || T == &T_STRIDX) {
        const int str = T == &T_STRIDX;
        const rx_addr L = entry - (str ? 0x78 : 0x60) - 0x10, s1 = L + 0x1c, pair = L + 0x44, pset = L + (str ? 0x60 : 0x48);
        rx_set_sp(L);
        tree_construct(&T_ROWSET, s1);                                 /* the empty set */
        SU32(s1, 0x1c, 0); SU32(s1, 0x20, 0); SU32(s1, 0x24, 0);
        if (str) { rx_tbl_str_init(pair); rx_tbl_str_assign_sub(pair, v->skey, 0, 0xffffffffu); }   /* 004f1860 */
        else SU32(pair, 0, v->key);
        rowset_copy_construct(pset, s1);                               /* 004ebce0 */
        n = tree_insert_hint(T, m, lb, k, val, v);                     /* 004ffcb0 / 004ff8a0: the node built from the pair */
        SU32(pset, 0x24, 0);                                           /* the pair destroyed (004fe460 for strings: set, then key) */
        rowset_destroy(pset);                                          /* 004f0670 */
        if (str) rx_tbl_str_tidy(pair);
        SU32(s1, 0x24, 0);                                             /* S1 destroyed */
        tree_destroy(&T_ROWSET, s1, 0);                                /* 004f2220 */
        game_free(U32(s1, 0));
    } else if (T == &T_STRUNI) {                                       /* the pair's key string, then the node, then the key freed */
        const rx_addr L = entry - 0x44 - 0x14, pair = L + 0x30;
        rx_set_sp(L);
        rx_tbl_str_init(pair); rx_tbl_str_assign_sub(pair, v->skey, 0, 0xffffffffu);
        n = tree_insert_hint(T, m, lb, k, val, v);                     /* 004ff460 */
        rx_tbl_str_tidy(pair);                                         /* 004f3320 */
    } else n = tree_insert_hint(T, m, lb, k, val, v);                  /* 004fff20 */
    rx_set_sp(sp0);
    return n;
}
static rx_addr idx_index(const tdesc* T, rx_addr m, tkey k, tvalue_fn val, const idxval* v) { return idx_index_at(T, m, k, val, v, 3); }

/* ======================================================================================================================== */
/* std::vector over game memory (layout: +0 proxy, +0xc begin, +0x10 end, +0x14 end of capacity; the game's allocator)          */

static uint32_t vsize(rx_addr v, uint32_t e) { return (U32(v, 0x10) - U32(v, 0xc)) / e; }
static uint32_t vcap(rx_addr v, uint32_t e) { return U32(v, 0xc) ? (U32(v, 0x14) - U32(v, 0xc)) / e : 0; }
/* the growth of _Insert_n(where, 1) (00501a30 / 00501bf0 / 00501e00 / 004f2e50): capacity + capacity / 2, at least size + 1 */
static uint32_t grow_cap(uint32_t cap, uint32_t size, uint32_t maxn) {
    if (maxn - size < 1) unreached(0x00501e87, "vector _Xlen");
    uint32_t half = cap >> 1, n = (maxn - half < cap) ? 0 : cap + half;
    return n < size + 1 ? size + 1 : n;
}
/* push_back of a 4-byte element (004faf10 int, 004fb080 float (fld / fstp: the bits of the files' floats), 004f0430 pointer) */
static void vec_push4(rx_addr v, uint32_t x) {
    uint32_t size = vsize(v, 4), cap = vcap(v, 4);
    if (size < cap) { SU32(U32(v, 0x10), 0, x); SU32(v, 0x10, U32(v, 0x10) + 4); return; }
    uint32_t n = grow_cap(cap, size, 0x3fffffffu);
    rx_addr p = game_alloc(4 * n);                                    /* 004fead0 */
    rx_addr b = U32(v, 0xc);
    if (size) crt_memmove(p, b, 4 * size);                             /* _Umove (memmove_s) */
    SU32(p, 4 * size, x);
    if (b) game_free(b);                                               /* 004f3320 */
    SU32(v, 0x14, p + 4 * n); SU32(v, 0x10, p + 4 * (size + 1)); SU32(v, 0xc, p);
}
/* reserve(n) of a 4-byte vector (004faf70 int / float, 004f0340 pointer) */
static void vec_reserve4(rx_addr v, uint32_t n) {
    if (n > 0x3fffffffu) unreached(0x004fafa8, "vector _Xlen");
    if (vcap(v, 4) >= n) return;
    rx_addr p = game_alloc(4 * n);
    rx_addr b = U32(v, 0xc);
    uint32_t size = vsize(v, 4);
    if (size) crt_memmove(p, b, 4 * size);
    if (b) game_free(b);
    SU32(v, 0x14, p + 4 * n); SU32(v, 0x10, p + 4 * size); SU32(v, 0xc, p);
}
/* the std::string vector (0x1c elements): push_back (004fb2d0 -> 00507cc0 / 004fed30 -> 00501e00) and reserve (004fb180) */
static void vec_str_relocate(rx_addr v, rx_addr p, uint32_t size) {   /* _Umove (00507600 / 005082f0) + the old range destroyed (00507c50) */
    rx_addr b = U32(v, 0xc);
    for (uint32_t i = 0; i < size; i++) str_copy_construct(p + 0x1c * i, b + 0x1c * i);
    if (b) {
        for (uint32_t i = 0; i < size; i++) rx_tbl_str_tidy(b + 0x1c * i);
        game_free(b);
    }
}
static void vec_str_push(rx_addr v, rx_addr s) {
    uint32_t size = vsize(v, 0x1c), cap = vcap(v, 0x1c);
    if (size < cap) { str_copy_construct(U32(v, 0x10), s); SU32(v, 0x10, U32(v, 0x10) + 0x1c); return; }
    uint32_t n = grow_cap(cap, size, 0x09249249u);
    rx_addr p = game_alloc(0x1c * n);                                  /* 004feee0 */
    rx_addr b = U32(v, 0xc);
    for (uint32_t i = 0; i < size; i++) str_copy_construct(p + 0x1c * i, b + 0x1c * i);   /* old [first, where) */
    str_copy_construct(p + 0x1c * size, s);                           /* 004feeb0: the new one */
    if (b) {
        for (uint32_t i = 0; i < size; i++) rx_tbl_str_tidy(b + 0x1c * i);
        game_free(b);
    }
    SU32(v, 0x14, p + 0x1c * n); SU32(v, 0x10, p + 0x1c * (size + 1)); SU32(v, 0xc, p);
}
static void vec_str_reserve(rx_addr v, uint32_t n) {
    if (n > 0x09249249u) unreached(0x004fb1b8, "vector _Xlen");
    if (vcap(v, 0x1c) >= n) return;
    uint32_t size = vsize(v, 0x1c);
    rx_addr p = game_alloc(0x1c * n);
    vec_str_relocate(v, p, size);
    SU32(v, 0x14, p + 0x1c * n); SU32(v, 0x10, p + 0x1c * size); SU32(v, 0xc, p);
}

void rx_tbl_vector_reserve_ptr(rx_addr vec, uint32_t n) {           /* 004f0340 (the column pointer vector at table + 4) */
    if (n > 0x3fffffffu) unreached(0x004f0378, "vector _Xlen");
    if (vcap(vec, 4) >= n) return;
    rx_addr p = game_alloc(4 * n);                                    /* 004fead0 */
    rx_addr b = U32(vec, 0xc);
    uint32_t size = vsize(vec, 4);
    if (size) crt_memmove(p, b, 4 * size);                            /* 00818960 */
    if (b) game_free(b);
    SU32(vec, 0x14, p + 4 * n); SU32(vec, 0x10, p + 4 * size); SU32(vec, 0xc, p);
}
RX_FUNCTION(0x004f0340, rx_tbl_vector_reserve_ptr)
void rx_tbl_vector_push_ptr(rx_addr vec, rx_addr valp) { vec_push4(vec, U32(valp, 0)); }   /* 004f0430 (-> 004f1900 / 004f2e50) */

/* ======================================================================================================================== */
/* the name maps (std::map<const char*, int | Table*, _stricmp>)                                                             */

typedef struct { rx_addr key; } nameval;
static void v_name(rx_addr n, const void* c) { SU32(n, 0xc, ((const nameval*)c)->key); SU32(n, 0x10, 0); }
/* 004f0540 operator[](key) (EDI = map; &key; RET 4): &node->value */
rx_addr rx_tbl_name_map_index(rx_addr map, rx_addr keyp) {
    tkey k = {0, U32(keyp, 0)};
    rx_addr lb = tree_lower_bound(&T_NAME, map, k);
    if (lb == HEAD(map) || key_less(&T_NAME, k, node_key(&T_NAME, lb))) {
        nameval v = {k.s};
        lb = tree_insert_hint(&T_NAME, map, lb, k, v_name, &v);     /* 004f1c00 */
    }
    return lb + 0x10;
}

/* ======================================================================================================================== */
/* Rdb::Table                                                                                                                */

static void table_construct_common(rx_addr t) {
    SU32(t, 0, 0x0093eb6c);
    rx_addr px = game_alloc(4);
    if (px) SU32(px, 0, t + 4);
    SU32(t, 4, px);
    SU32(t, 0x10, 0); SU32(t, 0x14, 0); SU32(t, 0x18, 0);
    tree_construct(&T_NAME, t + 0x1c);
}
rx_addr rx_tbl_table_construct(rx_addr t) {                          /* 004ec4f0 */
    table_construct_common(t);
    SU32(t, 0x68, 0); SU32(t, 0x70, 0); SU32(t, 0x6c, 1);
    return t;
}
rx_addr rx_tbl_table_construct_named(rx_addr t, rx_addr name) {      /* 004ec560 */
    table_construct_common(t);
    SU32(t, 0x6c, 1); SU32(t, 0x70, 0);
    if (crt_stricmp(t + 0x3c, name) != 0) { crt_strncpy(t + 0x3c, name, 0x29); SU8(t, 0x65, 0); }
    return t;
}

uint32_t rx_tbl_column_rows(rx_addr col) {                           /* 004eb830 */
    switch (U32(col, 0x2c)) {
    case 1: case 2: return vsize(U32(col, 0x40), 4);
    case 4: return vsize(U32(col, 0x40), 0x1c);
    case 0: return vsize(U32(col, 0x40), 0x10);
    case 5: return vsize(U32(col, 0x40), 0x18);
    case 6: return vsize(U32(col, 0x40), 8);
    default: return 0;
    }
}
uint32_t rx_tbl_row_count(rx_addr t) {                               /* 004021d0 */
    if (U32(t, 0x14) == U32(t, 0x10)) return 0;
    return rx_tbl_column_rows(U32(U32(t, 0x10), 0));
}
void rx_tbl_column_reserve(rx_addr col, uint32_t n) {               /* 004eb5c0 */
    switch (U32(col, 0x2c)) {
    case 1: case 2: vec_reserve4(U32(col, 0x40), n); break;          /* 004faf70 */
    case 4: vec_str_reserve(U32(col, 0x40), n); break;               /* 004fb180 */
    case 3: break;
    case 6: if (vcap(U32(col, 0x40), 8) < n) unreached(0x004fb7c8, "reserve of an 8-byte column with rows"); break;   /* 004fb770 */
    default: unreached(0x004eb5d4, "column type 0 / 5");
    }
}

/* the int / float / string default appended to a column (004f76b0 / 004f7830 / 004f79b0), with the index entries of the new row */
static void append_int(rx_addr col, rx_addr defp) {                  /* 004f76b0 (EAX = column; &default) */
    TF_PLAIN(F, 0x38, 4);                                              /* the temporary set at +0x20 */
    rx_addr v = U32(col, 0x40);
    uint32_t row = vsize(v, 4);
    int32_t key = I32(defp, 0);
    vec_push4(v, (uint32_t)key);                                      /* 004faf10 */
    rx_addr idx = U32(col, 0x44), uni = U32(col, 0x48);
    if (!idx && !uni) { TF_END(F); return; }
    tkey k = {(uint32_t)key, 0};
    if (!uni) {
        rx_addr it = tree_find(&T_INTIDX, idx, k);                    /* 00419ea0 */
        if (it == HEAD(idx)) {
            /* a temporary set {row} (004dc5c0), map[key] = it (004fc440 + 004ebe60), the temporary destroyed (004f2220 + free) */
            rowset_construct_one(F + 0x20, U32(col, 0x20), row);
            idxval iv = {(uint32_t)key, 0};
            rx_addr n = idx_index_at(&T_INTIDX, idx, k, v_intidx, &iv, 4);
            rowset_assign(n + 0x10, F + 0x20);
            SU32(F + 0x20, 0x24, 0);
            rowset_destroy(F + 0x20);
        } else rowset_add(it + 0x10, row);                            /* 004dc630 */
    } else {
        rx_addr it = tree_find(&T_INTUNI, uni, k);                    /* 00763270 */
        if (it == HEAD(uni)) {
            idxval iv = {(uint32_t)key, 0};
            rx_addr n = idx_index_at(&T_INTUNI, uni, k, v_intuni, &iv, 3);   /* 004fc5f0 */
            SU32(n, 0x10, row); SU32(n, 0x14, 0); SU32(n, 0x18, U32(col, 0x20));
        }
    }
    TF_END(F);
}
static void append_float(rx_addr col, rx_addr defp) {                /* 004f7830 */
    vec_push4(U32(col, 0x40), U32(defp, 0));                          /* 004fb080 */
    if (U32(col, 0x44) || U32(col, 0x48)) unreached(0x004f786a, "float column index");
}
static void append_string(rx_addr col, rx_addr def) {                /* 004f79b0 (EAX = column; the default std::string*) */
    TF_PLAIN(F, 0x38, 4);                                              /* the temporary set at +0x20 */
    rx_addr v = U32(col, 0x40);
    uint32_t row = vsize(v, 0x1c);
    vec_str_push(v, def);                                             /* 004fb2d0 */
    rx_addr idx = U32(col, 0x44), uni = U32(col, 0x48);
    if (!idx && !uni) { TF_END(F); return; }
    tkey k = {0, rx_tbl_str_cstr(def)};
    if (!uni) {
        rx_addr it = tree_find(&T_STRIDX, idx, k);                    /* 004fc3a0 */
        if (it == HEAD(idx)) {
            rowset_construct_one(F + 0x20, U32(col, 0x20), row);
            idxval iv = {0, def};
            rx_addr n = idx_index_at(&T_STRIDX, idx, k, v_stridx, &iv, 4);   /* 004fbea0 */
            rowset_assign(n + 0x28, F + 0x20);
            SU32(F + 0x20, 0x24, 0);
            rowset_destroy(F + 0x20);
        } else rowset_add(it + 0x28, row);
    } else {
        rx_addr it = tree_find(&T_STRUNI, uni, k);                    /* 004fbda0 */
        if (it == HEAD(uni)) {
            idxval iv = {0, def};
            rx_addr n = idx_index_at(&T_STRUNI, uni, k, v_struni, &iv, 3);   /* 004fb980 */
            SU32(n, 0x28, row); SU32(n, 0x2c, 0); SU32(n, 0x30, U32(col, 0x20));
        }
    }
    TF_END(F);
}
void rx_tbl_column_append_default(rx_addr col) {                     /* 004eb4e0 (push ecx; the callee's argument pushed) */
    TF_PLAIN(F, 0, 1);
    switch (U32(col, 0x2c)) {
    case 1: TF_CALL(1, append_int(col, col + 0x38)); break;
    case 2: TF_CALL(1, append_float(col, col + 0x38)); break;
    case 4: TF_CALL(1, append_string(col, U32(col, 0x38))); break;
    case 3: break;
    default: unreached(0x004eb4f0, "column type 0 / 5 / 6");
    }
    TF_END(F);
}

/* the column's cell vector and indexes (004f8cd0 int, 004f8df0 float, 004f8f20 string; EDI = column; flags; RET 4) */
static void column_storage(rx_addr col, uint32_t type, uint32_t flags, const tdesc* uni_t, const tdesc* idx_t) {
    SU32(col, 0x2c, type);
    rx_addr v = game_alloc(0x18);
    if (v) {
        rx_addr px = game_alloc(4);                                   /* 004f4a30 */
        if (px) SU32(px, 0, v);
        SU32(v, 0, px);
        SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
    }
    SU32(col, 0x40, v);
    SU32(col, 0x44, 0); SU32(col, 0x48, 0);
    if (flags & 1) {
        rx_addr m = rx_std_alloc(0x20);                               /* StdMemoryAllocator::vf1 004eef20 */
        if (m) tree_construct((flags & 2) ? uni_t : idx_t, m);
        SU32(col, (flags & 2) ? 0x48 : 0x44, m);
    }
    SU32(col, 0x4c, 0);
}

/* the Variant at column + 0x30 {+0 type, +8 value}: 004eedc0 destroys (string: the heap string freed), 004eee60 copies */
static void variant_destroy(rx_addr v) {                             /* 004eedc0 (EDI = variant) */
    uint32_t t = U32(v, 0);
    if (t == 4) {
        rx_addr s = U32(v, 8);
        rx_tbl_str_tidy(s);
        rx_tbl_alloc_init(); rx_reflex_aligned_free(s);               /* StdMemoryAllocator::vf2 00775540 */
    } else if (t == 5) unreached(0x004eedcd, "variant type 5");
}
static void variant_copy(rx_addr dst, rx_addr src) {                 /* 004eee60 (EDI = src; dst; RET 4) */
    uint32_t t = U32(src, 0);
    SU32(dst, 0, t);
    switch (t) {
    case 1: SU32(dst, 8, U32(src, 8)); break;
    case 2: SU32(dst, 8, U32(src, 8)); break;                         /* fld / fstp */
    case 4: {
        rx_addr s = rx_std_alloc(0x1c);
        if (s) str_copy_construct(s, U32(src, 8));
        SU32(dst, 8, s);
        break;
    }
    case 6: SU32(dst, 8, U32(src, 8)); SU32(dst, 0xc, U32(src, 0xc)); break;
    case 0: case 3: break;
    default: unreached(0x004eeebe, "variant type");
    }
}

/* the column constructors 004f69d0 / 004f6ae0 / 004f6f90 */
static rx_addr column_new(rx_addr table, uint32_t type, uint32_t flags, rx_addr parent, rx_addr pcol, uint32_t* idx_out) {
    *idx_out = (U32(table, 0x14) - U32(table, 0x10)) >> 2;
    rx_addr c = game_alloc(0x50);                                     /* 00506180 */
    if (c) {
        SU32(c, 0x30, 1); SU32(c, 0x44, 0); SU32(c, 0x48, 0);
        if (type == 1) column_storage(c, 1, flags, &T_INTUNI, &T_INTIDX);
        else if (type == 2) column_storage(c, 2, flags, &T_INTUNI, &T_INTIDX);   /* 004f8df0: the int index types (never used) */
        else column_storage(c, 4, flags, &T_STRUNI, &T_STRIDX);
        SU32(c, 0x20, table); SU32(c, 0x24, parent); SU32(c, 0x28, 0xffffffffu);
        if (parent) SU32(c, 0x28, rx_tbl_column_index(parent, pcol));
    }
    RX_FRAME_ENTER(F, 4);
    SU32(F, 0, c);
    rx_tbl_vector_push_ptr(table + 4, F);                             /* 004f0430 */
    RX_FRAME_LEAVE(F, 4);
    return c;
}
static void column_finish(rx_addr table, rx_addr c, rx_addr name, uint32_t idx) {
    crt_strncpy(c, name, 0x1f); SU8(c, 0x1f, 0);
    uint32_t rows = rx_tbl_row_count(table);                          /* 004021d0 */
    rx_tbl_column_reserve(c, rows);                                   /* 004eb5c0 */
    for (uint32_t r = rows; r; r--) rx_tbl_column_append_default(c);
    RX_FRAME_ENTER(F, 4);
    SU32(F, 0, c);
    SU32(rx_tbl_name_map_index(table + 0x1c, F), 0, idx);             /* 004f0540: map[name] = index */
    RX_FRAME_LEAVE(F, 4);
}
void rx_tbl_add_int_column(rx_addr table, rx_addr name, rx_addr def, uint32_t flags, rx_addr parent, rx_addr pcol, uint32_t a7) {
    uint32_t idx; rx_addr c = column_new(table, 1, flags, parent, pcol, &idx);
    RX_FRAME_ENTER(F, 0x10);                                          /* the local Variant {1, *def} */
    SU32(F, 0, 1); SU32(F, 8, U32(def, 0));
    variant_destroy(c + 0x30); variant_copy(c + 0x30, F); variant_destroy(F);
    RX_FRAME_LEAVE(F, 0x10);
    SU32(c, 0x4c, a7);
    column_finish(table, c, name, idx);
}
RX_FUNCTION(0x004f69d0, rx_tbl_add_int_column)
void rx_tbl_add_string_column(rx_addr table, rx_addr name, rx_addr def, uint32_t flags, rx_addr parent, rx_addr pcol) {
    uint32_t idx; rx_addr c = column_new(table, 4, flags, parent, pcol, &idx);
    RX_FRAME_ENTER(F, 0x10);                                          /* 004dc3c0: the local Variant {4, new std::string(def)} */
    SU32(F, 0, 4);
    rx_addr s = game_alloc(0x1c);
    if (s) { SU32(s, 0x18, 0xf); SU32(s, 0x14, 0); SU8(s, 4, 0); rx_tbl_str_assign_sub(s, def, 0, 0xffffffffu); }
    SU32(F, 8, s);
    variant_destroy(c + 0x30); variant_copy(c + 0x30, F); variant_destroy(F);
    RX_FRAME_LEAVE(F, 0x10);
    SU32(c, 0x4c, 0);
    column_finish(table, c, name, idx);
}
RX_FUNCTION(0x004f6ae0, rx_tbl_add_string_column)
void rx_tbl_add_float_column(rx_addr table, rx_addr name, rx_addr def, uint32_t flags, rx_addr parent, rx_addr pcol) {
    uint32_t idx; rx_addr c = column_new(table, 2, flags, parent, pcol, &idx);
    RX_FRAME_ENTER(F, 0x10);
    SU32(F, 0, 2); SU32(F, 8, U32(def, 0));
    variant_destroy(c + 0x30); variant_copy(c + 0x30, F); variant_destroy(F);
    RX_FRAME_LEAVE(F, 0x10);
    SU32(c, 0x4c, 0);
    column_finish(table, c, name, idx);
}
RX_FUNCTION(0x004f6f90, rx_tbl_add_float_column)
/* 004f7180 (name, &default (8 bytes); RET 8): an 8-byte (type 6) column of the "Network Sessions List" 0x00ae0460 (the table is
 * built into the function); no index */
void rx_tbl_add_int64_column(rx_addr table, rx_addr name, rx_addr def) {
    uint32_t idx = (U32(table, 0x14) - U32(table, 0x10)) >> 2;
    rx_addr c = game_alloc(0x50);
    if (c) {
        SU32(c, 0x30, 1); SU32(c, 0x44, 0); SU32(c, 0x48, 0);
        column_storage(c, 6, 0, &T_INTUNI, &T_INTIDX);                /* 004f9040 (flags 0) */
        SU32(c, 0x20, table); SU32(c, 0x24, 0); SU32(c, 0x28, 0xffffffffu);
    }
    RX_FRAME_ENTER(F, 0x14);
    SU32(F, 0x10, c);
    rx_tbl_vector_push_ptr(table + 4, F + 0x10);
    SU32(F, 0, 6); SU32(F, 8, U32(def, 0)); SU32(F, 0xc, U32(def, 4));   /* the local Variant {6, *def} */
    variant_destroy(c + 0x30); variant_copy(c + 0x30, F); variant_destroy(F);
    RX_FRAME_LEAVE(F, 0x14);
    SU32(c, 0x4c, 0);
    crt_strncpy(c, name, 0x1f); SU8(c, 0x1f, 0);
    uint32_t rows = rx_tbl_row_count(table);
    rx_tbl_column_reserve(c, rows);
    if (rows) unreached(0x004f725a, "an 8-byte column added to a table with rows");
    RX_FRAME_ENTER(G, 4);
    SU32(G, 0, c);
    SU32(rx_tbl_name_map_index(table + 0x1c, G), 0, idx);
    RX_FRAME_LEAVE(G, 4);
}

/* ---- cells ---- */
static void rx_tbl_set_int_body(rx_addr F, rx_addr col, uint32_t row, rx_addr valp);
static void rx_tbl_set_string_body(rx_addr F, rx_addr col, uint32_t row, rx_addr str);
void rx_tbl_set_int(rx_addr col, uint32_t row, rx_addr valp) {       /* 004f5330 */
    TF_EBP(F, 8, 0x3c, 3);
    rx_tbl_set_int_body(F, col, row, valp);
    TF_END(F);
}
static void rx_tbl_set_int_body(rx_addr F, rx_addr col, uint32_t row, rx_addr valp) {
    rx_addr idx = U32(col, 0x44), uni = U32(col, 0x48), v = U32(col, 0x40);
    if (row >= vsize(v, 4)) unreached(0x004f536d, "cell index");
    rx_addr cell = U32(v, 0xc) + 4 * row;
    if (!idx && !uni) { SU32(cell, 0, U32(valp, 0)); return; }
    if (!uni) {
        tkey ok = {U32(cell, 0), 0};
        rx_addr it = tree_find(&T_INTIDX, idx, ok);                   /* 00419ea0 */
        if (it == HEAD(idx)) unreached(0x004f53ec, "searchable index without the old value");
        rx_addr set = it + 0x10;
        rowset_erase_key(set, row);                                   /* 004f0800 */
        if (U32(set, 0x1c) == 0) tree_erase(&T_INTIDX, idx, it, d_intidx);   /* 004fcb60 */
        SU32(cell, 0, U32(valp, 0));
        tkey nk = {U32(valp, 0), 0};
        it = tree_find(&T_INTIDX, idx, nk);
        if (it == HEAD(idx)) {                                        /* the temporary set at the original's frame (+0x20) */
            rowset_construct_one(F + 0x20, U32(col, 0x20), row);
            idxval iv = {nk.u, 0};
            rx_addr n = idx_index_at(&T_INTIDX, idx, nk, v_intidx, &iv, 4);   /* 004fc440 */
            rowset_assign(n + 0x10, F + 0x20);
            SU32(F + 0x20, 0x24, 0);
            rowset_destroy(F + 0x20);
        } else rowset_add(it + 0x10, row);
        return;
    }
    tkey ok = {U32(cell, 0), 0};
    rx_addr it = tree_find(&T_INTUNI, uni, ok);                       /* 00763270 */
    tree_erase(&T_INTUNI, uni, it, 0);                                /* 004fc6b0 (the old value's entry, whichever row it names) */
    SU32(cell, 0, U32(valp, 0));
    tkey nk = {U32(valp, 0), 0};
    it = tree_find(&T_INTUNI, uni, nk);
    if (it == HEAD(uni)) {
        idxval iv = {nk.u, 0};
        rx_addr n = idx_index(&T_INTUNI, uni, nk, v_intuni, &iv);   /* 004fc5f0 */
        SU32(n, 0x10, row); SU32(n, 0x14, 0); SU32(n, 0x18, U32(col, 0x20));
    }
}
RX_FUNCTION(0x004f5330, rx_tbl_set_int)
void rx_tbl_set_float(rx_addr col, uint32_t row, rx_addr valp) {     /* 004f5650 */
    if (U32(col, 0x44) || U32(col, 0x48)) unreached(0x004f56b2, "float column index");
    rx_addr v = U32(col, 0x40);
    if (row >= vsize(v, 4)) unreached(0x004f568d, "cell index");
    SU32(U32(v, 0xc), 4 * row, U32(valp, 0));                         /* fld / fstp */
}
RX_FUNCTION(0x004f5650, rx_tbl_set_float)
void rx_tbl_set_string(rx_addr col, uint32_t row, rx_addr str) {     /* 004f5970 */
    TF_EBP(F, 8, 0x44, 3);
    rx_tbl_set_string_body(F, col, row, str);
    TF_END(F);
}
static void rx_tbl_set_string_body(rx_addr F, rx_addr col, uint32_t row, rx_addr str) {
    rx_addr idx = U32(col, 0x44), uni = U32(col, 0x48), v = U32(col, 0x40);
    if (row >= vsize(v, 0x1c)) unreached(0x004f59bc, "cell index");
    rx_addr cell = U32(v, 0xc) + 0x1c * row;                          /* 00419f30 */
    if (!idx && !uni) { rx_tbl_str_assign_sub(cell, str, 0, 0xffffffffu); return; }
    if (!uni) {
        tkey ok = {0, rx_tbl_str_cstr(cell)};
        rx_addr it = tree_find(&T_STRIDX, idx, ok);                   /* 004fc3a0 */
        if (it == HEAD(idx)) unreached(0x004f5a35, "searchable index without the old value");
        rx_addr set = it + 0x28;
        rowset_erase_key(set, row);
        if (U32(set, 0x1c) == 0) tree_erase(&T_STRIDX, idx, it, d_stridx);   /* 004fc050 */
        rx_tbl_str_assign_sub(cell, str, 0, 0xffffffffu);
        tkey nk = {0, rx_tbl_str_cstr(str)};
        it = tree_find(&T_STRIDX, idx, nk);
        if (it == HEAD(idx)) {                                        /* the temporary set at the original's frame (+0x28) */
            rowset_construct_one(F + 0x28, U32(col, 0x20), row);
            idxval iv = {0, str};
            rx_addr n = idx_index_at(&T_STRIDX, idx, nk, v_stridx, &iv, 4);   /* 004fbea0 */
            rowset_assign(n + 0x28, F + 0x28);
            SU32(F + 0x28, 0x24, 0);
            rowset_destroy(F + 0x28);
        } else rowset_add(it + 0x28, row);
        return;
    }
    tkey ok = {0, rx_tbl_str_cstr(cell)};
    rx_addr it = tree_find(&T_STRUNI, uni, ok);                       /* 004fbda0 */
    tree_erase(&T_STRUNI, uni, it, d_struni);                         /* 004fba90 */
    rx_tbl_str_assign_sub(cell, str, 0, 0xffffffffu);
    tkey nk = {0, rx_tbl_str_cstr(str)};
    it = tree_find(&T_STRUNI, uni, nk);
    if (it == HEAD(uni)) {
        idxval iv = {0, str};
        rx_addr n = idx_index(&T_STRUNI, uni, nk, v_struni, &iv);   /* 004fb980 */
        SU32(n, 0x28, row); SU32(n, 0x2c, 0); SU32(n, 0x30, U32(col, 0x20));
    }
}

/* Table_SetCellInt 004f6e60 (EAX = column name, ESI = table; row, &value; RET 8): the cell by column name; the table's change
 * counter +0x68 counts a changed value */
void rx_tbl_set_cell_int(rx_addr table, rx_addr name, uint32_t row, rx_addr valp) {
    TF_PLAIN(K,0,3);
    uint32_t c = rx_tbl_column_index(table, name);                    /* 00402160 */
    if (U32(valp, 0) != U32(rx_tbl_cell_word(table, c, row), 0)) SU32(table, 0x68, U32(table, 0x68) + 1);   /* 0041aaf0 */
    if (c >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) unreached(0x004f6e93, "column index");
    TF_CALL(3,rx_tbl_set_int(U32(U32(table, 0x10), 4 * c), row, valp)); /*004f5330*/
    TF_END(K);
}
RX_FUNCTION(0x004f6e60, rx_tbl_set_cell_int)

/* ---- a row erased (004ece50: every column's cell 004eb550, then +0x68) ---- */
/* 004ec180 (set, row; RET 8): the rows after the erased one renumbered (the keys stay ordered) */
static void rowset_renumber(rx_addr set, uint32_t row) {
    for (rx_addr n = LEFT(HEAD(set)); n != HEAD(set); n = tree_next(&T_ROWSET, n))   /* 00417df0 */
        if (U32(n, 0xc) > row) SU32(n, 0xc, U32(n, 0xc) - 1);
}
/* 004f8140 (column, row; RET 8): an int / float cell erased (memmove_s of the rest), then the index: a searchable index loses the
 * row from every key's set (the rest renumbered, empty keys erased), a unique index renumbers its rows and erases the row's key */
static void column_erase_int(rx_addr col, uint32_t row) {
    rx_addr v = U32(col, 0x40);
    uint32_t i = 0;
    for (rx_addr it = U32(v, 0xc); it != U32(v, 0x10); it += 4, i++) {
        if (i != row) continue;
        uint32_t n = (U32(v, 0x10) - (it + 4)) >> 2;
        if ((int32_t)n > 0) crt_memmove(it, it + 4, 4 * n);
        SU32(v, 0x10, U32(v, 0x10) - 4);
        break;
    }
    rx_addr idx = U32(col, 0x44), uni = U32(col, 0x48);
    if (!idx && !uni) return;
    if (!uni) {
        for (rx_addr n = LEFT(HEAD(idx)); n != HEAD(idx);) {
            rowset_erase_key(n + 0x10, row);                          /* 004f3c60 + 004faa00 + 004f1eb0 */
            rowset_renumber(n + 0x10, row);                           /* 004ec180 */
            if (U32(n, 0x2c) == 0) n = tree_erase(&T_INTIDX, idx, n, d_intidx);   /* 004fcb60 */
            else n = tree_next(&T_INTIDX, n);                         /* 00503440 */
        }
        return;
    }
    for (rx_addr n = LEFT(HEAD(uni)); n != HEAD(uni);) {
        if (U32(n, 0x10) > row) { SU32(n, 0x10, U32(n, 0x10) - 1); n = tree_next(&T_INTUNI, n); }   /* 00503750 */
        else if (U32(n, 0x10) == row) n = tree_erase(&T_INTUNI, uni, n, 0);                           /* 004fc6b0 */
        else n = tree_next(&T_INTUNI, n);
    }
}
/* 004f8430 (column, row; RET 8): a string cell erased (004fb380: the later strings assigned down 00507a50, the last destroyed
 * 00507c50), then the index as for ints */
static void column_erase_string(rx_addr col, uint32_t row) {
    rx_addr v = U32(col, 0x40);
    uint32_t i = 0;
    for (rx_addr it = U32(v, 0xc); it != U32(v, 0x10); it += 0x1c, i++) {
        if (i != row) continue;
        for (rx_addr s = it + 0x1c; s != U32(v, 0x10); s += 0x1c) rx_tbl_str_assign_sub(s - 0x1c, s, 0, 0xffffffffu);   /* 004f1860 */
        rx_tbl_str_tidy(U32(v, 0x10) - 0x1c);
        SU32(v, 0x10, U32(v, 0x10) - 0x1c);
        break;
    }
    rx_addr idx = U32(col, 0x44), uni = U32(col, 0x48);
    if (!idx && !uni) return;
    if (!uni) {
        for (rx_addr n = LEFT(HEAD(idx)); n != HEAD(idx);) {
            rowset_erase_key(n + 0x28, row);
            rowset_renumber(n + 0x28, row);
            if (U32(n, 0x44) == 0) n = tree_erase(&T_STRIDX, idx, n, d_stridx);   /* 004fc050 */
            else n = tree_next(&T_STRIDX, n);                         /* 005037c0 */
        }
        return;
    }
    for (rx_addr n = LEFT(HEAD(uni)); n != HEAD(uni);) {
        if (U32(n, 0x28) > row) { SU32(n, 0x28, U32(n, 0x28) - 1); n = tree_next(&T_STRUNI, n); }   /* 00503830 */
        else if (U32(n, 0x28) == row) n = tree_erase(&T_STRUNI, uni, n, d_struni);                    /* 004fba90 */
        else n = tree_next(&T_STRUNI, n);
    }
}
/* Shared004eb550 dispatch, with no table revision of its own. Both original
 *004ece50 and004ecf00 compose these exact same column erase kernels. */
static void column_erase_row(rx_addr col,uint32_t row) {
    switch(U32(col,0x2c)) {
    case 1: case 2: column_erase_int(col,row); break;
    case 4: column_erase_string(col,row); break;
    case 3: break;
    default: unreached(0x004eb559,"row erase of a column of type 0 / 5 / 6");
    }
}
void rx_tbl_erase_row(rx_addr table, uint32_t row) {                 /*004ece50*/
    for(rx_addr p=U32(table,0x10);p!=U32(table,0x14);p+=4)
        column_erase_row(U32(p,0),row);
    SU32(table,0x68,U32(table,0x68)+1);
}
RX_FUNCTION(0x004ece50, rx_tbl_erase_row)

/* ---- rows cleared ---- */
static void column_clear_int(rx_addr col) {                          /* 004f91e0 (EDI = column) */
    rx_addr v = U32(col, 0x40);
    SU32(v, 0x10, U32(v, 0xc));                                       /* 004fb0e0: erase(begin, end) */
    if (U32(col, 0x44)) tree_clear(&T_INTIDX, U32(col, 0x44), d_intidx);   /* 005003a0 */
    if (U32(col, 0x48)) tree_clear(&T_INTUNI, U32(col, 0x48), 0);          /* 005000f0 */
}
/* 004f9260 (EDI = column): 004fb3f0 destroys every string in [begin,end),
 * retains the vector allocation, then clears both original string index trees. */
static void column_clear_string(rx_addr col) {
    rx_addr v = U32(col, 0x40);
    for (rx_addr p = U32(v, 0xc); p != U32(v, 0x10); p += 0x1c) rx_tbl_str_tidy(p);
    SU32(v, 0x10, U32(v, 0xc));
    if (U32(col, 0x44)) tree_clear(&T_STRIDX, U32(col, 0x44), d_stridx);   /* 004ffac0 */
    if (U32(col, 0x48)) tree_clear(&T_STRUNI, U32(col, 0x48), d_struni);   /* 004ff680 */
}
void rx_tbl_table_clear_rows(rx_addr t) {                            /* 004ecdb0 */
    uint32_t n = (U32(t, 0x14) - U32(t, 0x10)) >> 2;
    for (uint32_t i = 0; i < n; i++) {
        rx_addr c = U32(U32(t, 0x10), 4 * i);
        switch (U32(c, 0x2c)) {
        case 1: case 2: column_clear_int(c); break;  /* original dispatches ints and floats to 004f91e0 */
        case 3: break;
        case 4: column_clear_string(c); break;
        default: unreached(0x004ecdfd, "row clear of a column of type 0 / 5 / 6");
        }
    }
    SU32(t, 0x68, U32(t, 0x68) + 1);
}
RX_FUNCTION(0x004ecdb0, rx_tbl_table_clear_rows)
void rx_tbl_database_clear_rows(rx_addr db) {                        /* 004eed40 */
    for (int32_t i = 0; i < I32(db, 0x1e8); i++) unreached(0x004eed52, "Database_ClearRows of a loaded database");
}
RX_FUNCTION(0x004eed40, rx_tbl_database_clear_rows)

/* ======================================================================================================================== */
/* Rdb::TableCollection                                                                                                      */

static void critical_section_init(rx_addr cs) {   /* InitializeCriticalSection as the reference boot leaves it: {0, -1, 0, 0, 0, 0} */
    SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 0);
}
rx_addr rx_tbl_collection_construct(rx_addr db) {                    /* 004ee8d0 */
    SU32(db, 0, 0x0093eb64);
    for (uint32_t o = 4; o < 0x18; o += 4) SU32(db, o, 0);
    rx_addr px = game_alloc(4);
    if (px) SU32(px, 0, db + 0x18);
    SU32(db, 0x18, px);
    rx_addr h = tree_buy_head(&T_NAME);                               /* 004f4e00 */
    SU32(db, 0x30, h);
    SU8(h, 0x15, 1); SU32(h, 4, h); SU32(h, 0, h); SU32(h, 8, h);
    SU32(db, 0x34, 0);
    for (uint32_t o = 0x1c8; o < 0x1e0; o += 4) SU32(db, o, 0);
    SU32(db, 0x1e0, 0); SU32(db, 0x1e8, 0);
    for (uint32_t o = 0x38; o < 0x1c8; o += 4) SU32(db, o, 0);
    SU32(db, 0x1ec, 1); SU32(db, 0x1f0, 1);
    rx_addr a = rx_reflex_aligned_alloc(rx_mul_sat(10, 4));
    SU32(db, 4, a); SU32(db, 8, 0); SU32(db, 0xc, 10); SU32(db, 0x10, 10); SU32(db, 0x14, 10);
    critical_section_init(db + 0x1c8);
    return db;
}

/* ======================================================================================================================== */
/* column / table destruction                                                                                                */

/* a map object's destructor (00500160 / 004ffed0 / 004ff850 ...: erase all + the head freed (005033d0 ...), then the proxy) */
static void map_destroy(const tdesc* T, rx_addr m, tdtor_fn dtor) {
    tree_destroy(T, m, dtor);
    game_free(U32(m, 0));
}
/* 004eb7d0 (EAX = column): the cells and indexes by type (004fa640 int / float, 004fa6c0 string) */
static void column_destroy_storage(rx_addr c) {
    uint32_t t = U32(c, 0x2c);
    if (t != 1 && t != 2 && t != 4) { if (t == 3) return; unreached(0x004eb7e2, "destroy of a column of type 0 / 5 / 6"); }
    rx_addr v = U32(c, 0x40);
    if (v) {
        if (t == 4) {                                                 /* 00507820 */
            if (U32(v, 0xc)) {
                for (rx_addr e = U32(v, 0xc); e != U32(v, 0x10); e += 0x1c) rx_tbl_str_tidy(e);
                game_free(U32(v, 0xc));
            }
        } else if (U32(v, 0xc)) game_free(U32(v, 0xc));                /* 005077c0 */
        SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
        game_free(U32(v, 0));
        rx_tbl_alloc_init(); rx_reflex_aligned_free(v);               /* StdMemoryAllocator::vf2 */
        SU32(c, 0x40, 0);
    }
    if (U32(c, 0x44)) {
        if (t == 4) map_destroy(&T_STRIDX, U32(c, 0x44), d_stridx);   /* 004ff850 */
        else map_destroy(&T_INTIDX, U32(c, 0x44), d_intidx);          /* 00500160 */
        rx_tbl_alloc_init(); rx_reflex_aligned_free(U32(c, 0x44));
        SU32(c, 0x44, 0);
    }
    if (U32(c, 0x48)) {
        if (t == 4) map_destroy(&T_STRUNI, U32(c, 0x48), d_struni);
        else map_destroy(&T_INTUNI, U32(c, 0x48), 0);                 /* 004ffed0 */
        rx_tbl_alloc_init(); rx_reflex_aligned_free(U32(c, 0x48));
        SU32(c, 0x48, 0);
    }
}
/* 004ec600 (table; RET 4): every column destroyed (004eb7d0, its default, the block), the column vector emptied (007cf450) and the
 * column map cleared (004f1a80) */
void rx_tbl_table_destroy_columns(rx_addr t) {
    for (rx_addr p = U32(t, 0x10); p != U32(t, 0x14); p += 4) {
        rx_addr c = U32(p, 0);
        if (!c) continue;
        column_destroy_storage(c);
        variant_destroy(c + 0x30);
        game_free(c);
    }
    SU32(t, 0x14, U32(t, 0x10));
    tree_clear(&T_NAME, t + 0x1c, 0);
}
/* a stack table's destructor (004e6b2a..): vtable Rdb::Table, 004ec600, the column map (004f1af0 + its proxy), the column vector
 * (005077c0) */
void rx_tbl_table_destroy(rx_addr t) {
    SU32(t, 0, 0x0093eb6c);
    rx_tbl_table_destroy_columns(t);
    tree_destroy(&T_NAME, t + 0x1c, 0);
    game_free(U32(t, 0x1c));
    if (U32(t, 0x10)) game_free(U32(t, 0x10));
    SU32(t, 0x10, 0); SU32(t, 0x14, 0); SU32(t, 0x18, 0);
    game_free(U32(t, 4));
}
rx_addr rx_tbl_table_copy(rx_addr t, rx_addr src);

/* ======================================================================================================================== */
/* table copies (Database_RestorePlayerDefaults, the startup tables)                                                         */

/* a vector of n copies of a value (004fe990 int, 004feb20 float, 004fec80 string with ""): the 0x18 block's begin / end / capacity */
static void vec_fill_construct(rx_addr v, uint32_t n, uint32_t esize) {
    SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
    if (!n) return;
    rx_addr p = game_alloc(esize * n);                                /* 004fead0 / 004feee0 */
    SU32(v, 0xc, p); SU32(v, 0x10, p); SU32(v, 0x14, p + esize * n);
    for (uint32_t i = 0; i < n; i++) {
        if (esize == 4) SU32(p, 4 * i, 0);                            /* 006c8230 / 004fec60 */
        else rx_tbl_str_init(p + 0x1c * i);                           /* 004feeb0: copies of an empty string */
    }
    SU32(v, 0x10, p + esize * n);
}
/* the index rebuild of a copied int column (005066a0 (column; RET 4)): both trees cleared, then every row entered: the searchable
 * index gets each row in its value's set, the unique index each value's first row */
static void column_rebuild_int(rx_addr col) {
    if (U32(col, 0x44)) tree_clear(&T_INTIDX, U32(col, 0x44), d_intidx);     /* 005003a0 */
    if (U32(col, 0x48)) tree_clear(&T_INTUNI, U32(col, 0x48), 0);            /* 005000f0 */
    rx_addr v = U32(col, 0x40);
    uint32_t n = vsize(v, 4);
    for (uint32_t row = 0; row < n; row++) {
        tkey k = {U32(U32(v, 0xc), 4 * row), 0};
        if (!U32(col, 0x48)) {
            rx_addr idx = U32(col, 0x44);
            rx_addr it = tree_find(&T_INTIDX, idx, k);
            if (it == HEAD(idx)) {
                RX_FRAME_ENTER(F, 0x30);
                rowset_construct_one(F, U32(col, 0x20), row);
                idxval iv = {k.u, 0};
                rx_addr nd = idx_index(&T_INTIDX, idx, k, v_intidx, &iv);
                rowset_assign(nd + 0x10, F);
                rowset_destroy(F);
                RX_FRAME_LEAVE(F, 0x30);
            } else rowset_add(it + 0x10, row);
        } else {
            rx_addr uni = U32(col, 0x48);
            if (tree_find(&T_INTUNI, uni, k) == HEAD(uni)) {
                idxval iv = {k.u, 0};
                rx_addr nd = idx_index(&T_INTUNI, uni, k, v_intuni, &iv);
                SU32(nd, 0x10, row); SU32(nd, 0x14, 0); SU32(nd, 0x18, U32(col, 0x20));
            }
        }
    }
}
/* an empty index of the copy (004f9874.. / 004f9941..: _aligned_malloc(0x20) + the map constructor) */
static rx_addr new_index(const tdesc* T) {
    rx_addr m = game_alloc(0x20);
    if (m) tree_construct(T, m);
    return m;
}
/* 004eb720 (ESI = source column, EDI = copy): the cells and the indexes by type (004f9750 int, 004f9a20 float, 004f9cf0 string),
 * then the parent table / column of the source */
static void column_copy_storage(rx_addr dst, rx_addr src) {
    uint32_t type = U32(src, 0x2c);
    if (type != 1 && type != 2 && type != 4) unreached(0x004eb730, "copy of a column of type 0 / 5 / 6");
    SU32(dst, 0x2c, type);
    rx_addr v = game_alloc(0x18);
    uint32_t esize = type == 4 ? 0x1c : 4;
    if (v) {
        uint32_t n = vsize(U32(src, 0x40), esize);
        rx_addr px = game_alloc(4);
        if (px) SU32(px, 0, v);
        SU32(v, 0, px);
        vec_fill_construct(v, n, esize);
    }
    SU32(dst, 0x40, v);
    SU32(dst, 0x4c, U32(src, 0x4c));
    rx_addr sv = U32(src, 0x40);
    uint32_t n = vsize(sv, esize);
    for (uint32_t i = 0; i < n; i++) {
        if (type == 4) rx_tbl_str_assign_sub(U32(v, 0xc) + 0x1c * i, U32(sv, 0xc) + 0x1c * i, 0, 0xffffffffu);
        else SU32(U32(v, 0xc), 4 * i, U32(U32(sv, 0xc), 4 * i));      /* int; float: fld / fstp */
    }
    if (U32(src, 0x44) || U32(src, 0x48)) {
        if (type == 2) unreached(0x004f9b38, "copy of a float index");
        int uni = U32(src, 0x48) != 0;
        if (U32(dst, uni ? 0x48 : 0x44)) unreached(0x004f987b, "copy over an existing index");
        SU32(dst, uni ? 0x48 : 0x44, new_index(type == 4 ? (uni ? &T_STRUNI : &T_STRIDX) : (uni ? &T_INTUNI : &T_INTIDX)));
        if (type == 1) column_rebuild_int(dst);
        else {                                                        /* 00506b80: the string trees cleared, then the rows */
            if (U32(dst, 0x44)) tree_clear(&T_STRIDX, U32(dst, 0x44), d_stridx);   /* 004ffac0 */
            if (U32(dst, 0x48)) tree_clear(&T_STRUNI, U32(dst, 0x48), d_struni);   /* 004ff680 */
            if (vsize(U32(dst, 0x40), 0x1c)) unreached(0x00506c0e, "index rebuild of a string column with rows");
        }
    }
    SU32(dst, 0x24, U32(src, 0x24));
    SU32(dst, 0x28, U32(src, 0x28));
}
/* 004dc6b0 (copy, table, source; RET 0xc): the column copy constructor */
static void column_copy_construct(rx_addr c, rx_addr table, rx_addr src) {
    SU32(c, 0x30, 1);
    crt_strncpy(c, src, 0x1f);
    SU8(c, 0x1f, 0);
    SU32(c, 0x20, table);
    variant_destroy(c + 0x30);
    variant_copy(c + 0x30, src + 0x30);
    SU32(c, 0x44, 0); SU32(c, 0x48, 0);
    column_copy_storage(c, src);                                      /* 004eb720 */
}
/* 004ec770 (copy, source; RET 8): an unnamed copy of a table: version and load count, every column copied, the column map rebuilt */
rx_addr rx_tbl_table_copy(rx_addr t, rx_addr src) {
    crt_strncpy(t + 0x3c, 0x00935d6c, 0x29); SU8(t, 0x65, 0);
    SU32(t, 0x6c, U32(src, 0x6c));
    SU32(t, 0x68, U32(src, 0x68));
    SU32(t, 0x70, 0);
    rx_tbl_table_destroy_columns(t);                                  /* 004ec600 */
    uint32_t n = (U32(src, 0x14) - U32(src, 0x10)) >> 2;
    rx_tbl_vector_reserve_ptr(t + 4, n);                              /* 004f0340 */
    for (uint32_t i = 0; i < n; i++) {
        rx_addr c = game_alloc(0x50);
        if (c) column_copy_construct(c, t, U32(U32(src, 0x10), 4 * i));
        vec_push4(t + 4, c);                                          /* the inline push_back (reserved) */
    }
    /* 004ec8d0: the column map rebuilt: map[column name] = index */
    tree_clear(&T_NAME, t + 0x1c, 0);
    RX_FRAME_ENTER(F, 4);
    for (uint32_t i = 0; i < n; i++) {
        SU32(F, 0, U32(U32(t, 0x10), 4 * i));
        SU32(rx_tbl_name_map_index(t + 0x1c, F), 0, i);
    }
    RX_FRAME_LEAVE(F, 4);
    return t;
}
RX_FUNCTION(0x004ec770, rx_tbl_table_copy)
/* Database_RestorePlayerDefaults 004eea10: every "Player*" table of the default database 0x00ae0910 copied into the database (its
 * parent links re-pointed to the database's own copies where it has them), named, versioned and listed */
void rx_tbl_restore_player_defaults(rx_addr db) {
    rx_tbl_database_clear_rows(db);
    const rx_addr player = 0x0093e440;                                /* "Player" */
    uint32_t plen = 6;
    for (int32_t i = 0; i < I32(RX_TBL_DEFAULT_DB, 0x1e8); i++) {
        rx_addr src = U32(RX_TBL_DEFAULT_DB, 0x38 + 4 * (uint32_t)i);
        rx_addr name = src + 0x3c;
        if (crt_strncmp(player, name, plen) != 0) continue;
        if (rx_tbl_find(db, name)) unreached(0x004eea93, "restore over an existing table");
        rx_addr p = game_alloc(0x74);
        rx_addr t = p ? rx_tbl_table_construct(p) : 0;
        rx_tbl_table_copy(t, src);                                    /* 004ec770 */
        uint32_t nc = (U32(t, 0x14) - U32(t, 0x10)) >> 2;
        for (uint32_t k = 0; k < nc; k++) {
            rx_addr c = U32(U32(t, 0x10), 4 * k);
            if (!U32(c, 0x24)) continue;
            rx_addr own = rx_tbl_find(db, U32(c, 0x24) + 0x3c);
            if (own) SU32(c, 0x24, own);
        }
        if (crt_stricmp(t + 0x3c, name) != 0) { crt_strncpy(t + 0x3c, name, 0x29); SU8(t, 0x65, 0); }
        /* Table::vf1 0x007e3be0: ret */
        SU32(t, 0x6c, U32(db, 0x1ec));
        SU32(db, 0x38 + 4 * U32(db, 0x1e8), t);
        SU32(db, 0x1e8, U32(db, 0x1e8) + 1);
        RX_FRAME_ENTER(F, 4);
        SU32(F, 0, t + 0x3c);
        SU32(rx_tbl_name_map_index(db + 0x18, F), 0, t);
        RX_FRAME_LEAVE(F, 4);
    }
}
RX_FUNCTION(0x004eea10, rx_tbl_restore_player_defaults)

/* ======================================================================================================================== */
/* rows added from a criteria vector (the row builder of the game's table helpers)                                           */
/* A criterion (0xb0): +0 the column name (0x7f chars + NUL), +0x80 the column index (-1 until resolved), +0x84 an operator (0 for
 * add-row), +0x88 the value (a Variant {+0 type, +8 value}), +0x98 a std::vector<int> (a dotted name's column path: empty here).
 * A criteria vector: proxy +0, begin +0xc, end +0x10, capacity +0x14 (ReflexAlignedAlloc blocks). */

void rx_tbl_crit_init_int(rx_addr c, rx_addr name, uint32_t op, uint32_t value) {   /* 00419a70 (ESI = c, EAX = op, ECX = &value; name) */
    SU32(c, 0x80, 0xffffffffu); SU32(c, 0x84, op); SU32(c, 0x88, 1); SU32(c, 0x90, value);
    rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, c + 0x98);
    SU32(c, 0x98, px); SU32(c, 0xa4, 0); SU32(c, 0xa8, 0); SU32(c, 0xac, 0);
    crt_strncpy(c, name, 0x7f); SU8(c, 0x7f, 0);
}
/* the criterion's destructor (inline in its users and in 0041ac70): the path vector's block, its proxy, the value */
void rx_tbl_crit_destroy(rx_addr c) {
    if (U32(c, 0xa4)) rx_reflex_aligned_free(U32(c, 0xa4));
    rx_addr px = U32(c, 0x98);
    SU32(c, 0xa4, 0); SU32(c, 0xa8, 0); SU32(c, 0xac, 0);
    rx_reflex_aligned_free(px);
    uint32_t t = U32(c, 0x88);
    if (t == 4) { rx_addr s = U32(c, 0x90); rx_tbl_str_tidy(s); game_free(s); }
    else if (t == 5) unreached(0x0041acc4, "criterion value type 5");
}
/* 004187a0 (copy, source; RET 8): the copy constructor */
static void crit_copy(rx_addr d, rx_addr s) {
    for (uint32_t k = 0; k < 0x80; k += 4) SU32(d, k, U32(s, k));
    SU32(d, 0x80, U32(s, 0x80)); SU32(d, 0x84, U32(s, 0x84));
    variant_copy(d + 0x88, s + 0x88);
    /* 00418800: the path vector copied */
    rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, d + 0x98);
    SU32(d, 0x98, px);
    uint32_t n = vsize(s + 0x98, 4);
    SU32(d, 0xa4, 0); SU32(d, 0xa8, 0); SU32(d, 0xac, 0);
    if (n) {
        rx_addr p = rx_reflex_aligned_alloc(4 * n);                   /* 0042bf30 */
        SU32(d, 0xa4, p); SU32(d, 0xa8, p); SU32(d, 0xac, p + 4 * n);
        crt_memmove(p, U32(s, 0xa4), 4 * n);
        SU32(d, 0xa8, p + 4 * n);
    }
}
static void crit_destroy_range(rx_addr first, rx_addr last) {        /* 0041ac70 */
    for (rx_addr c = first; c != last; c += 0xb0) rx_tbl_crit_destroy(c);
}
void rx_tbl_crit_vec_init(rx_addr v) {                               /* the callers' inline constructor */
    rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, v);
    SU32(v, 0, px); SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
}
void rx_tbl_crit_vec_clear(rx_addr v) {                              /* 004f2c40 erase(begin, end) */
    crit_destroy_range(U32(v, 0xc), U32(v, 0x10));
    SU32(v, 0x10, U32(v, 0xc));
}
void rx_tbl_crit_vec_destroy(rx_addr v) {                            /* the callers' inline destructor */
    if (U32(v, 0xc)) { crit_destroy_range(U32(v, 0xc), U32(v, 0x10)); rx_reflex_aligned_free(U32(v, 0xc)); }
    rx_addr px = U32(v, 0);
    SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
    rx_reflex_aligned_free(px);
}
/* 004175e0 (EDI = vector; &criterion; RET 4) push_back; full: 00417b60 -> 00418470 (_Insert_n at the end) */
void rx_tbl_crit_push(rx_addr v, rx_addr c) {
    uint32_t size = vsize(v, 0xb0), cap = vcap(v, 0xb0);
    if (size < cap) { crit_copy(U32(v, 0x10), c); SU32(v, 0x10, U32(v, 0x10) + 0xb0); return; }   /* 0041abd0 */
    uint32_t n = grow_cap(cap, size, 0x01745d17u);
    rx_addr p = rx_reflex_aligned_alloc(rx_mul_sat(n, 0xb0));        /* 00419cd0 */
    rx_addr b = U32(v, 0xc);
    for (uint32_t i = 0; i < size; i++) crit_copy(p + 0xb0 * i, b + 0xb0 * i);   /* 00419c70 */
    crit_copy(p + 0xb0 * size, c);                                    /* 00417c20 */
    if (b) { crit_destroy_range(b, U32(v, 0x10)); rx_reflex_aligned_free(b); }
    SU32(v, 0x14, p + 0xb0 * n); SU32(v, 0x10, p + 0xb0 * (size + 1)); SU32(v, 0xc, p);
}
/* 004f1640 (ECX = vector; n; RET 4) reserve */
static void crit_reserve(rx_addr v, uint32_t n) {
    if (n > 0x01745d17u) unreached(0x004f1678, "vector _Xlen");
    if (vcap(v, 0xb0) >= n) return;
    rx_addr p = rx_reflex_aligned_alloc(rx_mul_sat(n, 0xb0));        /* 00419cd0 */
    rx_addr b = U32(v, 0xc);
    uint32_t size = vsize(v, 0xb0);
    for (uint32_t i = 0; i < size; i++) crit_copy(p + 0xb0 * i, b + 0xb0 * i);   /* 0041b090 */
    if (b) unreached(0x004f171b, "reserve of a non-empty criteria vector");
    SU32(v, 0x14, p + 0xb0 * n); SU32(v, 0x10, p + 0xb0 * size); SU32(v, 0xc, p);
}
/* 004ed530 (ECX = dst; table, &source; RET 8): the source criteria with their column indices resolved by name (Table_ColumnIndex);
 * criteria of unknown columns are dropped */
static void crit_resolve(rx_addr dst, rx_addr table, rx_addr src) {
    if (U32(dst, 0x10) != U32(dst, 0xc)) {                            /* 004f2c40: erase(begin, end) */
        crit_destroy_range(U32(dst, 0xc), U32(dst, 0x10));
        SU32(dst, 0x10, U32(dst, 0xc));
    }
    crit_reserve(dst, vsize(src, 0xb0));
    TF_PLAIN(L, 0x8e4, 4);
    const rx_addr F = L + 0x38;                                        /* the criterion copy */
    for (rx_addr s = U32(src, 0xc); s != U32(src, 0x10); s += 0xb0) {
        crit_copy(F, s);
        if (I32(F, 0x80) < 0) {
            rx_addr dot = crt_strchr(F, '.');
            if (!dot) SU32(F, 0x80, rx_tbl_column_index(table, F));    /* 00402160 */
            else {                                                     /* "Column.ParentColumn": the column, then the parent table's column as the path */
                const rx_addr N = L + 0xe8;                            /* the parent column's name */
                crt_strncpy(N, dot + 1, 0x3ff); SU8(N, 0x3ff, 0);
                SU8(dot, 0, 0);
                uint32_t col = rx_tbl_column_index(table, F);
                SU32(F, 0x80, col);
                if (crt_strchr(N, '.')) unreached(0x004ed6f3, "a criterion through two parent tables");
                if (col >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) unreached(0x004ed813, "column index");
                rx_addr parent = U32(U32(U32(table, 0x10), 4 * col), 0x24);
                vec_push4(F + 0x98, rx_tbl_column_index(parent, N));   /* 004f04e0 */
            }
        }
        if (I32(F, 0x80) >= 0) rx_tbl_crit_push(dst, F);
        rx_tbl_crit_destroy(F);
    }
    TF_END(L);
}
/* 004f6c60 (EAX = op, ECX = &C string; criterion, name; RET 8): a criterion with a string value (004dc3c0: Variant {4, a heap copy}) */
void rx_tbl_crit_init_string(rx_addr c, rx_addr name, uint32_t op, rx_addr value) {
    SU32(c, 0x84, op); SU32(c, 0x80, 0xffffffffu);
    TF_PLAIN(L, 0x1c, 4);
    const rx_addr T = L + 0x10;                                        /* the temporary std::string */
    rx_tbl_str_init(T);
    rx_tbl_str_assign_n(T, value, crt_strlen(value));                 /* 004f2ce0 */
    SU32(c, 0x88, 4);
    rx_addr sv = rx_std_alloc(0x1c);
    if (sv) { rx_tbl_str_init(sv); rx_tbl_str_assign_sub(sv, T, 0, 0xffffffffu); }   /* 004f1860 */
    SU32(c, 0x90, sv);
    rx_tbl_str_tidy(T);                                                /* 004f3320 */
    TF_END(L);
    rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, c + 0x98);
    SU32(c, 0x98, px); SU32(c, 0xa4, 0); SU32(c, 0xa8, 0); SU32(c, 0xac, 0);
    crt_strncpy(c, name, 0x7f); SU8(c, 0x7f, 0);
}

/* ======================================================================================================================== */
/* row queries: the rows of a table matching a criteria vector, as a std::set<RowRef> (004eddc0), and the delete (004ecf00)   */

/* 004dc570 (ESI = set; table; RET 4): an empty set of the table */
static void rowset_construct(rx_addr set, rx_addr table) {
    tree_construct(&T_ROWSET, set);
    SU32(set, 0x24, table); SU32(set, 0x1c, 0); SU32(set, 0x20, 0);
}
/* the temporary set's destructor in the query (004ebe10 for a sorted set, then 004f2220, the proxy freed) */
static void rowset_destroy_tmp(rx_addr set) {
    if(U32(set,0x20)){sort_columns_destroy(U32(set,0x20));SU32(set,0x20,0);}
    SU32(set, 0x24, 0);
    tree_destroy(&T_ROWSET, set, 0);                                   /* 004f2220 */
    game_free(U32(set, 0));
}
/* 004f06c0 set::insert(val) without the container's sort check (the key's set decides: rows by index) */
static void rowset_insert_raw(rx_addr set, rowref r) {
    int ins; tkey k = {r.row, r.set};
    tree_insert_unique(&T_ROWSET, set, k, v_rowref, &r, &ins);
}
/* 004f3ba0 (ECX = tree; node, parent; RET 8) _Copy: the subtree copied in preorder (node, left, right) */
static rx_addr rowset_copy_nodes(rx_addr dst, rx_addr n, rx_addr parent) {
    rx_addr head = HEAD(dst), res = head;
    if (isnil(&T_ROWSET, n)) return res;
    rowref r = {U32(n, 0xc), U32(n, 0x10), U32(n, 0x14)};
    rx_addr c = game_alloc(T_ROWSET.nsize);                            /* 004f3e60 */
    if (c) { SU32(c, 0, head); SU32(c, 4, parent); SU32(c, 8, head); v_rowref(c, &r); SU8(c, T_ROWSET.color, color(&T_ROWSET, n)); SU8(c, T_ROWSET.isnil, 0); }
    res = c;
    SU32(c, 0, rowset_copy_nodes(dst, LEFT(n), c));
    SU32(c, 8, rowset_copy_nodes(dst, RIGHT(n), c));
    return res;
}
/* 004ebf80 (EDI = set; other; RET 4): set = set & other (rows of both; the kept nodes' values as they are) */
static void rowset_intersect(rx_addr set, rx_addr other) {
    if (U32(set, 0x1c) == 0) return;
    if (U32(other, 0x1c) == 0) { tree_clear(&T_ROWSET, set, 0); return; } /* 004ebf9a..c8: retain sort/table, clear only nodes */
    TF_PLAIN(L, 0x34, 3);
    const rx_addr T = L + 0x20;                                        /* 004f0630: proxy, head, size (no sort column / table) */
    rx_addr px = game_alloc(4);
    if (px) SU32(px, 0, T);
    SU32(T, 0, px);
    rx_addr h = tree_buy_head(&T_ROWSET);
    SU32(T, 0x18, h); SU8(h, T_ROWSET.isnil, 1); SU32(h, 4, h); SU32(h, 0, h); SU32(h, 8, h);
    SU32(T, 0x1c, 0);
    for (rx_addr n = LEFT(HEAD(set)); n != HEAD(set); n = tree_next(&T_ROWSET, n)) {
        tkey k = {U32(n, 0xc), other}; /* 004dc660 builds the search RowRef with OTHER comparator context */
        if (tree_find(&T_ROWSET, other, k) != HEAD(other)) {          /* 004dc660 -> 004f0860 */
            rowref r = {U32(n, 0xc), U32(n, 0x10), U32(n, 0x14)};
            rowset_insert_raw(T, r);                                   /* 004f06c0 */
        }
    }
    tree_clear(&T_ROWSET, set, 0);                                     /* 004f1eb0 erase(begin, end) */
    rx_addr th = HEAD(T), sh = HEAD(set);                              /* 004f1f90: _Copy of T */
    SU32(sh, 4, rowset_copy_nodes(set, PARENT(th), sh));
    SU32(set, 0x1c, U32(T, 0x1c));
    rx_addr root = PARENT(sh);
    if (isnil(&T_ROWSET, root)) { SU32(sh, 0, sh); SU32(sh, 8, sh); }
    else { SU32(sh, 0, tree_min(&T_ROWSET, root)); SU32(sh, 8, tree_max(&T_ROWSET, root)); }
    tree_destroy(&T_ROWSET, T, 0);                                     /* 004f2220 */
    game_free(U32(T, 0));
    TF_END(L);
}
/* 004eb9f0 (EAX = &value, ECX = row; column, op; RET 8): cell op value (004f6740 ints: == != < <= > >=; 004f6850 strings: ==
 * != through _stricmp 004dc330 / 004dc370) */
static int cell_matches(rx_addr value, uint32_t row, rx_addr col, uint32_t op) {
    switch (U32(col, 0x2c)) {
    case 1: {
        if (row >= rx_tbl_column_rows(col)) unreached(0x004fa8b0, "cell index");
        int32_t a = I32(U32(U32(col, 0x40), 0xc), 4 * row), b = I32(value, 8);   /* 004fa890 */
        switch (op) {
        case 0: return a == b;
        case 1: return a != b;
        case 2: return a < b;
        case 3: return a <= b;
        case 4: return a > b;
        case 5: return a >= b;
        default: return 0;
        }
    }
    case 4: {
        rx_addr cell = rx_tbl_cell_string(col, row), v = U32(value, 8);  /* 0041a480 */
        if (op == 0) return crt_stricmp(rx_tbl_str_cstr(v), rx_tbl_str_cstr(cell)) == 0;   /* 004dc330 */
        unreached(0x004f6850, "a string criterion other than ==");
        return 0;
    }
    default: unreached(0x004eba13, "a criterion on a column of type 0 / 2 / 5 / 6"); return 0;
    }
}
/* 004eb920 (ESI = column, EAX = row): the parent table's row the cell names (the parent column's unique index) */
static rx_addr parent_ref(rx_addr col, uint32_t row) {
    rx_addr pt = U32(col, 0x24);
    uint32_t pc = U32(col, 0x28);
    if (pc >= (U32(pt, 0x14) - U32(pt, 0x10)) >> 2) unreached(0x004eb947, "parent column index");
    if (U32(col, 0x2c) != 1) unreached(0x004eb966, "a parent link of a non-int column");
    if (row >= rx_tbl_column_rows(col)) unreached(0x004fa8b0, "cell index");
    return rx_tbl_row(U32(U32(pt, 0x10), 4 * pc), I32(U32(U32(col, 0x40), 0xc), 4 * row));   /* 004fa890, 00418ec0 */
}
/* Public typed entry, one existing parent-link algorithm. */
rx_addr rx_tbl_column_parent_ref(rx_addr column,uint32_t row){return parent_ref(column,row);}
RX_FUNCTION(0x004eb920,rx_tbl_column_parent_ref)

/* FUN_004eddc0 (table, &criteria, &out set; RET 0xc): out = the rows matching every criterion, criterion by criterion: an equality
 * on a uniquely indexed column through the index (004fa960), an equality on an indexed column through its row set (004dca30),
 * otherwise a scan of every row (the first criterion) or a filter of the rows found so far; stops at an empty set. Criteria
 * must be resolved (004ed530). */
void rx_tbl_query(rx_addr table, rx_addr crits, rx_addr out) {
    rx_addr c = U32(crits, 0xc);
    if (vsize(crits, 0xb0) == 0) return;
    TF_PLAIN(L, 0xc0, 4);
    tree_clear(&T_ROWSET, out, 0);                                     /* 004f2020 + the head relinked */
    int first = 1;
    for (; c != U32(crits, 0x10); c += 0xb0) {
        uint32_t ci = U32(c, 0x80), op = U32(c, 0x84);
        int nopath = ((U32(c, 0xa8) - U32(c, 0xa4)) & 0xfffffffcu) == 0;
        if (ci >= (U32(table, 0x14) - U32(table, 0x10)) >> 2 && op == 0 && nopath) unreached(0x004edef6, "column index");
        rx_addr col = (op == 0 && nopath) ? U32(U32(table, 0x10), 4 * ci) : 0;
        if (col && U32(col, 0x48)) {                                   /* the unique index */
            rx_addr t1 = L + 0x88, t2 = L + 0x3c;
            rowset_construct(t1, 0);                                   /* 004dc570(0) */
            if (U32(out, 0x24) && first) {
                rowset_construct(t2, 0); rowset_assign(t2, out); /* 004ebce0 copy preserves out sort/table */
            } else rowset_construct(t2, table);                               /* 004dc570(table) */
            rowset_assign(t1, t2);                                     /* 004ebe60: t1 = a set of the table */
            rowset_destroy_tmp(t2);
            tree_clear(&T_ROWSET, t1, 0);                              /* 004fa960 */
            if (U32(col, 0x2c) != 4) unreached(0x004dcb41, "a unique lookup of a non-string column");
            rx_addr uni = U32(col, 0x48);                              /* 004dcb20 -> 004f64a0 (004fbda0) */
            tkey k = {0, rx_tbl_str_cstr(U32(c, 0x90))};
            rx_addr n = tree_find(&T_STRUNI, uni, k);
            /* a key the unique index does not hold (004f64fe): the lookup answers the shared entry 0x009c9ff4, whose row is -1 (e.g. the
             * EventType table's "ActivityName" of a championship activity type, which has no row): nothing is inserted */
            if (n != HEAD(uni) && U32(n, 0x30) && U32(n, 0x28) != 0xffffffffu) { rowref r = {U32(n, 0x28), t1, U32(t1, 0x24)}; rowset_insert_raw(t1, r); }
            if (first) rowset_assign(out, t1);                         /* 004ebe60 */
            else TF_CALL(1, rowset_intersect(out, t1));             /* 004ee06e */
            rowset_destroy_tmp(t1);
        } else if (col && (U32(col, 0x44) || U32(col, 0x48))) {       /* the searchable index */
            rx_addr rows;
            if (U32(col, 0x2c) == 1) rows = rx_tbl_rows(col, I32(c, 0x90)); /*004dca51 ->00418e50*/
            else if (U32(col, 0x2c) == 4) {                           /*004dca73 tail004f62e0*/
                const rx_addr index=U32(col,0x44);
                tkey key={0,rx_tbl_str_cstr(U32(c,0x90))};
                rx_addr node=tree_find(&T_STRIDX,index,key);          /*004fc3a0: existing canonical string tree*/
                const rx_addr proxy=U32(index,0);
                if(!proxy||U32(proxy,0)!=index)crt_invalid_parameter_noinfo();
                rows=node==HEAD(index)?0x00ae1c1cu:node+0x28;         /*missing key is the shared empty set*/
            } else unreached(0x004dca62, "an index lookup of unsupported cell type 0 / 2 / 5 / 6");
            if (first) {
                if (U32(out, 0x24)) {
                    /* 004ebee0 appends {row,OUT,OUT.table}; it does not replace OUT's sort columns. */
                    for (rx_addr n = LEFT(HEAD(rows)); n != HEAD(rows); n = tree_next(&T_ROWSET, n)) {
                        rowref r = {U32(n, 0xc), out, U32(out, 0x24)};
                        rowset_insert_raw(out, r);
                    }
                } else rowset_assign(out, rows);                              /* 004ebe60 */
            } else TF_CALL(1, rowset_intersect(out, rows));            /* 004ebf80 */
        } else if (first) {                                            /* scan */
            if (!U32(out, 0x24)) {
                rowset_construct(L + 0x3c, table);
                rowset_assign(out, L + 0x3c);
                rowset_destroy_tmp(L + 0x3c);
            }
            for (uint32_t r = 0; U32(table, 0x14) != U32(table, 0x10) && r < rx_tbl_column_rows(U32(U32(table, 0x10), 0)); r++) {
                uint32_t row = r; rx_addr t = table, cc = ci;
                for (rx_addr p = U32(c, 0xa4); p != U32(c, 0xa8); p += 4) {
                    rx_addr ref = parent_ref(U32(U32(t, 0x10), 4 * cc), row);
                    row = U32(ref, 0); t = U32(ref, 8); cc = U32(p, 0);
                    if (!t || row == 0xffffffffu) break;
                }
                if (!t || row == 0xffffffffu) continue;
                if (cc >= (U32(t, 0x14) - U32(t, 0x10)) >> 2) unreached(0x004ee58c, "column index");
                if (cell_matches(c + 0x88, row, U32(U32(t, 0x10), 4 * cc), op)) {
                    rowref rr = {r, out, U32(out, 0x24)};
                    rowset_insert_raw(out, rr);                        /* 004f06c0 */
                }
            }
        } else {                                                       /* filter the rows found so far */
            for (rx_addr n = LEFT(HEAD(out)); n != HEAD(out);) {
                uint32_t row = U32(n, 0xc); rx_addr t = U32(n, 0x14), cc = ci;
                for (rx_addr p = U32(c, 0xa4); p != U32(c, 0xa8); p += 4) {
                    rx_addr ref = parent_ref(U32(U32(t, 0x10), 4 * cc), row);
                    row = U32(ref, 0); t = U32(ref, 8); cc = U32(p, 0);
                }
                uint32_t key = U32(n, 0xc);
                n = tree_next(&T_ROWSET, n);                           /* 00417df0 */
                if (cc >= (U32(t, 0x14) - U32(t, 0x10)) >> 2) unreached(0x004ee807, "column index");
                if (!cell_matches(c + 0x88, row, U32(U32(t, 0x10), 4 * cc), op)) rowset_erase_key(out, key);   /* 004f3c60 + 004faa00 + 004f1eb0 */
            }
        }
        if (U32(out, 0x1c) == 0) break;
        first = 0;
    }
    TF_END(L);
}
RX_FUNCTION(0x004eddc0, rx_tbl_query)
/* FUN_004ed2f0 (ECX = table, EAX = &set; &criteria; RET 4): the criteria resolved (004ed530), then the query (004eddc0); without
 * criteria assigns an empty set of the table (004dc570, verified original disassembly). */
void rx_tbl_select(rx_addr table, rx_addr out, rx_addr crits) {
    TF_PLAIN(L, 0x44, 3);
    const rx_addr v = L + 0x10;
    rx_tbl_crit_vec_init(v);
    TF_CALL(2, crit_resolve(v, table, crits));                         /* 004ed530 */
    if(vsize(v,0xb0)==0){
        const rx_addr empty=L+0x28;TF_CALL(1,rowset_construct(empty,table));
        TF_CALL(1,rowset_assign(out,empty));rowset_destroy_tmp(empty);
    }else TF_CALL(3, rx_tbl_query(table, v, out));                           /* 004eddc0 */
    rx_tbl_crit_vec_destroy(v);
    TF_END(L);
}
/* FUN_004ed410 (table, &criteria, &out RowRef; RET 0xc): the first matching row (rows in index order), else the null reference
 * {-1, 0, 0} (0x009c9ff4) */
void rx_tbl_find_first(rx_addr table, rx_addr crits, rx_addr out) {
    TF_PLAIN(L, 0x28, 4);
    const rx_addr set = L + 0x10;
    rx_addr px = game_alloc(4);                                        /* 004f4a30 */
    if (px) SU32(px, 0, set);
    SU32(set, 0, px);
    rx_addr h = tree_buy_head(&T_ROWSET);
    SU32(set, 0x18, h); SU8(h, T_ROWSET.isnil, 1); SU32(h, 4, h); SU32(h, 0, h); SU32(h, 8, h);
    SU32(set, 0x1c, 0); SU32(set, 0x20, 0); SU32(set, 0x24, 0);
    TF_CALL(1, rx_tbl_select(table, set, crits));                      /* 004ed2f0 */
    if (U32(set, 0x1c)) {
        rx_addr n = LEFT(HEAD(set));
        SU32(out, 0, U32(n, 0xc)); SU32(out, 4, U32(n, 0x10)); SU32(out, 8, U32(n, 0x14));
    } else { SU32(out, 0, U32(0, 0x009c9ff4)); SU32(out, 4, U32(0, 0x009c9ff8)); SU32(out, 8, U32(0, 0x009c9ffc)); }
    rowset_destroy_tmp(set);
    TF_END(L);
}
RX_FUNCTION(0x004ed410, rx_tbl_find_first)
/* 004f0800 (ECX = &key {row, set, table}, ESI = set): erase(key) */
void rx_tbl_rowset_erase(rx_addr set, uint32_t row) { rowset_erase_key(set, row); }
/* 004ebce0 (EDI = set; source; RET 4): the copy constructor (rows inserted one by one with this set as their set) */
void rx_tbl_rowset_copy(rx_addr set, rx_addr src) {
    tree_construct(&T_ROWSET, set);
    SU32(set, 0x1c, 0); SU32(set, 0x20, 0); SU32(set, 0x24, U32(src, 0x24));
    if(U32(src,0x20))SU32(set,0x20,sort_columns_copy(U32(src,0x20)));
    for (rx_addr n = LEFT(HEAD(src)); n != HEAD(src); n = tree_next(&T_ROWSET, n)) {
        rowref r = {U32(n, 0xc), set, U32(set, 0x24)};
        rowset_insert_raw(set, r);                                     /* 004f06c0 */
    }
}
/* 004dc570 (ESI = set; table; RET 4): an empty set of a table (proxy, head; size, sort column 0) */
void rx_tbl_rowset_init(rx_addr set, rx_addr table) { rowset_construct(set, table); }
/* the set's destructor as the callers inline it (004ebe10 for a sorted set, the table word cleared, 004f2220, the proxy freed) */
void rx_tbl_rowset_destroy(rx_addr set) { rowset_destroy_tmp(set); }
/* ++it of a std::set<RowRef> (00417df0) */
rx_addr rx_tbl_rowset_next(rx_addr node) { return tree_next(&T_ROWSET, node); }

/* FUN_004ecf00 (table, &criteria; RET 8): the rows matching the criteria erased; returns their number. Without criteria every row
 * goes (not in the reference boot). */
uint32_t rx_tbl_delete_rows(rx_addr table, rx_addr crits) {
    TF_PLAIN(L,0x54,4);
    const rx_addr v=L+0x20,set=L+0x38;
    SU32(L,0x14,0); /* original running mutation count and final EAX */
    rx_tbl_crit_vec_init(v);
    TF_CALL(2,crit_resolve(v,table,crits));
    if(vsize(v,0xb0)==0) {
        /*004ecf69..cfe1: clear each actual column, not Table_ClearRows (which
         *would increment table revision itself).004ed291 reads count AFTER
         *clearing: for ordinary scalar tables that produces zero. */
        for(rx_addr p=U32(table,0x10);p!=U32(table,0x14);p+=4) {
            const rx_addr c=U32(p,0);
            switch(U32(c,0x2c)) {
            case 1: case 2: column_clear_int(c); break;
            case 4: column_clear_string(c); break;
            case 3: break;
            default: unreached(0x004eb6c9,"delete-all of a column of type 0 / 5 / 6");
            }
        }
        if(U32(table,0x14)!=U32(table,0x10))
            SU32(L,0x14,rx_tbl_column_rows(U32(U32(table,0x10),0)));
    } else {
        for(;;) {
            /* Original destroys/reconstructs this query result every iteration,
             *including the terminal empty query. Do not reuse an index rowset
             *or erase saved row IDs after compaction. */
            rowset_construct(set,0);
            TF_CALL(3,rx_tbl_query(table,v,set));
            if(!U32(set,0x1c)) {rowset_destroy_tmp(set);break;}
            const rx_addr node=LEFT(HEAD(set));
            SU32(L,0x18,U32(set,0));SU32(L,0x1c,node);
            const uint32_t row=U32(node,0xc);
            for(rx_addr p=U32(table,0x10);p!=U32(table,0x14);p+=4)
                column_erase_row(U32(p,0),row);
            SU32(L,0x14,U32(L,0x14)+1);
            rowset_destroy_tmp(set);
        }
    }
    if(U32(L,0x14))SU32(table,0x68,U32(table,0x68)+1);
    rx_tbl_crit_vec_destroy(v);
    /*004ed260 pushes criteria proxy for cdecl free; that argument remains
     *on the stack at004ed272. [ESP+18] there is therefore L14, the running
     *mutation count, notL18.004ed276 removes the outstanding argument. */
    const uint32_t result=U32(L,0x14);
    TF_END(L);return result;
}
RX_FUNCTION(0x004ecf00, rx_tbl_delete_rows)

/* 004dc710 (column, row, &value; RET 0xc): a cell set from a Variant */
static void set_cell_variant(rx_addr col, uint32_t row, rx_addr var) {
    TF_PLAIN(F, 0x40, 4);
    switch (U32(col, 0x2c)) {
    case 1: TF_CALL(3, rx_tbl_set_int(col, row, var + 8)); break;   /* 004f5330 */
    case 2: TF_CALL(3, rx_tbl_set_float(col, row, var + 8)); break; /* 004dc75e ->004f5650: actual Float criterion */
    case 4: TF_CALL(2, rx_tbl_set_string(col, row, U32(var, 8))); break;   /* 004f5970 */
    default: unreached(0x004dc754, "a cell of type 0 / 5 / 6 from a criterion");
    }
    TF_END(F);
}
/* 004ec950 (EAX = table; out, &criteria; RET 8): a row of defaults appended, the criteria's cells set; out = {row, 0, table} */
rx_addr rx_tbl_add_row(rx_addr table, rx_addr out, rx_addr crits) {
    TF_EBP(L, 8, 0x2c, 3);
    const rx_addr F = L + 0x20;                                        /* the resolved criteria */
    rx_tbl_crit_vec_init(F);
    TF_CALL(2, crit_resolve(F, table, crits));                        /* 004ed530 */
    uint32_t row = rx_tbl_row_count(table);
    uint32_t nc = (U32(table, 0x14) - U32(table, 0x10)) >> 2;
    for (uint32_t i = 0; i < nc; i++) TF_CALL(0, rx_tbl_column_append_default(U32(U32(table, 0x10), 4 * i)));
    for (rx_addr c = U32(F, 0xc); c != U32(F, 0x10); c += 0xb0)
        TF_CALL(3, set_cell_variant(U32(U32(table, 0x10), 4 * U32(c, 0x80)), row, c + 0x88));
    SU32(table, 0x68, U32(table, 0x68) + 1);
    SU32(out, 0, row); SU32(out, 8, table); SU32(out, 4, 0);
    if (U32(F, 0xc)) { crit_destroy_range(U32(F, 0xc), U32(F, 0x10)); game_free(U32(F, 0xc)); }
    rx_addr px = U32(F, 0);
    SU32(F, 0xc, 0); SU32(F, 0x10, 0); SU32(F, 0x14, 0);
    game_free(px);
    TF_END(L);
    return out;
}

/* VehicleList_AddRow 004de810 (owner, owner vehicle id; RET 8) / RiderList_AddRow 004dea90 (owner, owner rider id, gear set id;
 * RET 0xc): the next id (++[0x00ae063c] / ++[0x00ae1a24]) and the arguments as criteria, a row of "Vehicle List" 0x00ae05c8 /
 * "Rider List" 0x00ae19b0; returns the new id */
static uint32_t list_add_row(rx_addr table, rx_addr counter, const rx_addr* names, const uint32_t* vals, int n) {
    TF_PLAIN(F, n == 3 ? 0xe0 : 0xdc, 3);                              /* 004de810: sub 0xe0; 004dea90: sub 0xdc */
    const rx_addr vec = F + 0xc, crit = F + 0x28, out = F + 0xdc;     /* (the same offsets in both) */
    rx_tbl_crit_vec_init(vec);
    SU32(0, counter, U32(0, counter) + 1);
    for (int i = 0; i < n; i++) {
        rx_tbl_crit_init_int(crit, names[i], 0, i == 0 ? U32(0, counter) : vals[i - 1]);   /* 00419a70 / 004f6eb0 / 004f6f20 */
        rx_tbl_crit_push(vec, crit);
        rx_tbl_crit_destroy(crit);
    }
    TF_CALL(2, rx_tbl_add_row(table, out, vec));
    uint32_t id = U32(0, counter);
    rx_tbl_crit_vec_destroy(vec);
    TF_END(F);
    return id;
}
uint32_t rx_tbl_vehicle_list_add_row(uint32_t owner, uint32_t owner_vehicle_id) {
    static const rx_addr names[3] = {0x00936cb4, 0x0093df84, 0x0093df94};   /* VehicleId, VehicleOwner, OwnerVehicleId */
    uint32_t v[2] = {owner, owner_vehicle_id};
    return list_add_row(0x00ae05c8, 0x00ae063c, names, v, 3);
}
RX_FUNCTION(0x004de810, rx_tbl_vehicle_list_add_row)
uint32_t rx_tbl_rider_list_add_row(uint32_t owner, uint32_t owner_rider_id, uint32_t gear_set_id) {
    static const rx_addr names[4] = {0x0093dec8, 0x0093dfa4, 0x0093dfb0, 0x0093dfc0};   /* RiderId, RiderOwner, OwnerRiderId, OwnerRiderGearSetId */
    uint32_t v[3] = {owner, owner_rider_id, gear_set_id};
    return list_add_row(0x00ae19b0, 0x00ae1a24, names, v, 4);
}
RX_FUNCTION(0x004dea90, rx_tbl_rider_list_add_row)

/* Host Player-snapshot boundary: append actual values through the same original
 * append kernels used by rx_tbl_column_append_default. No default-key staging,
 * criteria clone or second index implementation. Caller validates every column
 * and unique key before the first table write. values is one guest address per
 * schema column: scalar word pointer for int/float, std::string object for text. */
void rx_tbl_append_values_row(rx_addr table,rx_addr values) {
    uint32_t nc=(U32(table,0x14)-U32(table,0x10))/4u;
    for(uint32_t i=0;i<nc;i++) {
        rx_addr col=U32(U32(table,0x10),4*i),value=U32(values,4*i);
        switch(U32(col,0x2c)) {
        case 1: append_int(col,value);break;
        case 2: append_float(col,value);break;
        case 4: append_string(col,value);break;
        default: unreached(0x004eb4f0,"unsupported snapshot row column type");
        }
    }
    SU32(table,0x68,U32(table,0x68)+1);
}
/* Read-only structural postcondition for the imported unique indexes. Uses the
 * same tree_find/comparators and original RowRef layouts as ordinary consumers. */
uint32_t rx_tbl_snapshot_unique_valid(rx_addr table) {
    uint32_t nc=(U32(table,0x14)-U32(table,0x10))/4u,rows=rx_tbl_row_count(table);
    for(uint32_t c=0;c<nc;c++) {
        rx_addr col=U32(U32(table,0x10),4*c),map=U32(col,0x48);
        if(!map)continue;
        uint32_t type=U32(col,0x2c);rx_addr begin=U32(U32(col,0x40),0xc);
        for(uint32_t r=0;r<rows;r++) {
            tkey key={0,0};rx_addr found;uint32_t offset;
            if(type==1){key.u=U32(begin,4*r);found=tree_find(&T_INTUNI,map,key);offset=0x10;}
            else if(type==4){key.s=rx_tbl_str_cstr(begin+28*r);found=tree_find(&T_STRUNI,map,key);offset=0x28;}
            else return 0;
            if(found==HEAD(map)||U32(found,offset)!=r||U32(found,offset+8)!=table)return 0;
        }
    }
    return 1;
}

#include "build_tables_sort.inc"

/* Public Rules helpers. The canonical tree and allocation kernels above are the
 * sole implementation; this successor retains the V8 prefix byte for byte. */
#include "career_rules.h"
/*004f64a0 ECX=column, EAX=std::string. Missing key is the original null RowRef,
 * not a missing-owner failure and not an insertion into the unique index. */
rx_addr rx_tbl_string_row(rx_addr column,rx_addr text){
 TF_EBP(F,8,0xc,3);
 const rx_addr tree=U32(column,0x48);
 const tkey key={0,rx_tbl_str_cstr(text)};
 const rx_addr node=tree_find(&T_STRUNI,tree,key);
 const rx_addr result=node==HEAD(tree)?0x009c9ff4u:node+0x28;
 TF_END(F);return result;
}
RX_FUNCTION(0x004f64a0,rx_tbl_string_row)
/*004f7420 ESI=criterion, EAX=std::string; name; RET4. Unlike004f6c60 this
 * copies an existing std::string, including its explicit length. */
rx_addr rx_tbl_crit_copy_string(rx_addr c,rx_addr name,rx_addr value){
 TF_PLAIN(F,0,2);
 SU32(c,0x80,0xffffffffu);SU32(c,0x84,0);SU32(c,0x88,4);
 rx_addr copy=rx_std_alloc(0x1c);
 if(copy)str_copy_construct(copy,value);
 SU32(c,0x90,copy);
 rx_addr proxy=rx_reflex_aligned_alloc(4);
 if(proxy)SU32(proxy,0,c+0x98);
 SU32(c,0x98,proxy);SU32(c,0xa4,0);SU32(c,0xa8,0);SU32(c,0xac,0);
 crt_strncpy(c,name,0x7f);SU8(c,0x7f,0);
 TF_END(F);return c;
}
RX_FUNCTION(0x004f7420,rx_tbl_crit_copy_string)
