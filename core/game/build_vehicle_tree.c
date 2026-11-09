/* build_vehicle_tree.c -- see build_vehicle_tree.h. */
#include "build_vehicle_tree.h"
#include "build_alloc.h"
#include <stdio.h>
#include <stdlib.h>

#define RED   0
#define BLACK 1
static void tree_fatal(const char* what, rx_addr a) { fprintf(stderr, "rxcore: std::map: %s (%08x)\n", what, a); abort(); }
static uint32_t colour(const vt_tree* t, rx_addr n) { return U8(n, t->col); }
static void set_colour(const vt_tree* t, rx_addr n, uint32_t c) { SU8(n, t->col, c); }

/* 00690540 / 0041bb70 (ECX = node; s0 = tree; RET 4) */
static void rotate_left(const vt_tree* t, rx_addr m, rx_addr x) {
    rx_addr y = U32(x, 8);
    SU32(x, 8, U32(y, 0));
    if (!vt_nil(t, U32(y, 0))) SU32(U32(y, 0), 4, x);
    SU32(y, 4, U32(x, 4));
    rx_addr head = U32(m, 0x18);
    if (x == U32(head, 4)) SU32(head, 4, y);
    else if (x == U32(U32(x, 4), 0)) SU32(U32(x, 4), 0, y);
    else SU32(U32(x, 4), 8, y);
    SU32(y, 0, x);
    SU32(x, 4, y);
}
/* 00705020 / 0041bbe0 */
static void rotate_right(const vt_tree* t, rx_addr m, rx_addr x) {
    rx_addr y = U32(x, 0);
    SU32(x, 0, U32(y, 8));
    if (!vt_nil(t, U32(y, 8))) SU32(U32(y, 8), 4, x);
    SU32(y, 4, U32(x, 4));
    rx_addr head = U32(m, 0x18);
    if (x == U32(head, 4)) SU32(head, 4, y);
    else if (x == U32(U32(x, 4), 8)) SU32(U32(x, 4), 8, y);
    else SU32(U32(x, 4), 0, y);
    SU32(y, 8, x);
    SU32(x, 4, y);
}

/* --it (006909c0 / 00544b90): from the head, the rightmost; from a node, its predecessor */
rx_addr vt_prev(const vt_tree* t, rx_addr n) {
    if (vt_nil(t, n)) {
        n = U32(n, 8);
        if (vt_nil(t, n)) tree_fatal("--begin() (_invalid_parameter_noinfo)", n);
        return n;
    }
    if (!vt_nil(t, U32(n, 0))) {
        n = U32(n, 0);
        while (!vt_nil(t, U32(n, 8))) n = U32(n, 8);
        return n;
    }
    rx_addr p = U32(n, 4);
    while (!vt_nil(t, p) && n == U32(p, 0)) { n = p; p = U32(p, 4); }
    if (vt_nil(t, n)) tree_fatal("--begin() (_invalid_parameter_noinfo)", n);
    return p;
}
/* ++it (00690280 / 0041bc30) */
rx_addr vt_next(const vt_tree* t, rx_addr n) {
    if (vt_nil(t, n)) { tree_fatal("++end() (_invalid_parameter_noinfo)", n); return n; }
    if (!vt_nil(t, U32(n, 8))) {
        n = U32(n, 8);
        while (!vt_nil(t, U32(n, 0))) n = U32(n, 0);
        return n;
    }
    rx_addr p = U32(n, 4);
    while (!vt_nil(t, p) && n == U32(p, 8)) { n = p; p = U32(p, 4); }
    return p;
}

/* _Insert (00705de0 / 00544840: EDI = tree, EAX = where; s0 out, s1 add_left, s2 &value): the node (_Buynode(head, where, head,
 * value)) linked under `where`, the size counted, the tree rebalanced, the root black. With a frame context (vt_insert_at_fr) the
 * stack words of the uint map's 00705de0 too: sub esp, 0x44; push ebx, ebp, esi; push head, where, head; call _Buynode (return
 * 00705e47); each rotation push edi (the tree); call (00690540 / 00705020: returns 00705ed5 / 00705ef1, 00705f1d / 00705f39). */
static rx_addr insert_at(const vt_tree* t, rx_addr m, rx_addr where, uint32_t add_left, rx_addr val, const vt_frame* f) {
    if (U32(m, 0x1c) >= t->max_size) tree_fatal("map/set<T> too long (length_error)", m);
    rx_addr head = U32(m, 0x18);
    rx_addr z;
    if (f) {
        SU32(f->e, -0x48, f->ebx); SU32(f->e, -0x4c, f->ebp); SU32(f->e, -0x50, f->esi);
        SU32(f->e, -0x54, head); SU32(f->e, -0x58, where); SU32(f->e, -0x5c, head); SU32(f->e, -0x60, 0x00705e47u);
        z = f->make(f->e - 0x60, f, head, where, val);
    } else z = t->make(head, where, val);
    SU32(m, 0x1c, U32(m, 0x1c) + 1);
    if (where == head) {
        SU32(head, 4, z); SU32(head, 0, z); SU32(head, 8, z);
    } else if (add_left) {
        SU32(where, 0, z);
        if (where == U32(head, 0)) SU32(head, 0, z);
    } else {
        SU32(where, 8, z);
        if (where == U32(head, 8)) SU32(head, 8, z);
    }
#define ROT_FRAME(ret) do { if (f) { SU32(f->e, -0x54, f->edi); SU32(f->e, -0x58, (ret)); } } while (0)
    rx_addr x = z;
    while (colour(t, U32(x, 4)) == RED) {
        rx_addr p = U32(x, 4), g = U32(p, 4);
        if (p == U32(g, 0)) {
            rx_addr y = U32(g, 8);
            if (colour(t, y) == RED) {
                set_colour(t, p, BLACK); set_colour(t, y, BLACK); set_colour(t, g, RED);
                x = g;
            } else {
                if (x == U32(p, 8)) { x = p; ROT_FRAME(0x00705ed5u); rotate_left(t, m, x); }
                set_colour(t, U32(x, 4), BLACK);
                set_colour(t, U32(U32(x, 4), 4), RED);
                ROT_FRAME(0x00705ef1u);
                rotate_right(t, m, U32(U32(x, 4), 4));
            }
        } else {
            rx_addr y = U32(g, 0);
            if (colour(t, y) == RED) {
                set_colour(t, p, BLACK); set_colour(t, y, BLACK); set_colour(t, g, RED);
                x = g;
            } else {
                if (x == U32(p, 0)) { x = p; ROT_FRAME(0x00705f1du); rotate_right(t, m, x); }
                set_colour(t, U32(x, 4), BLACK);
                set_colour(t, U32(U32(x, 4), 4), RED);
                ROT_FRAME(0x00705f39u);
                rotate_left(t, m, U32(U32(x, 4), 4));
            }
        }
    }
#undef ROT_FRAME
    set_colour(t, U32(U32(m, 0x18), 4), BLACK);
    return z;
}
rx_addr vt_insert_at(const vt_tree* t, rx_addr m, rx_addr where, uint32_t add_left, rx_addr val) { return insert_at(t, m, where, add_left, val, 0); }
rx_addr vt_insert_at_fr(const vt_tree* t, rx_addr m, rx_addr where, uint32_t add_left, rx_addr val, const vt_frame* f) {
    return insert_at(t, m, where, add_left, val, f);
}

/* insert(value) (00774f60 / 005446d0): the descent by less(value, node), then the predecessor test; an equal key returns the node */
rx_addr vt_insert_unique(const vt_tree* t, rx_addr m, rx_addr val) {
    rx_addr head = U32(m, 0x18), y = head, x = U32(head, 4);
    uint32_t left = 1;
    while (!vt_nil(t, x)) {
        y = x;
        left = (uint32_t)t->less(val, x + 0xc);
        x = left ? U32(x, 0) : U32(x, 8);
    }
    rx_addr p = y;
    if (left) {
        if (y == U32(head, 0)) return vt_insert_at(t, m, y, 1, val);
        p = vt_prev(t, y);
    }
    if (t->less(p + 0xc, val)) return vt_insert_at(t, m, y, left, val);
    return p;
}

/* insert(hint, value) (00773e80 / 00544480; s0 tree, s1 out, s2 &value, s3 / s4 the hint) */
rx_addr vt_insert_hint(const vt_tree* t, rx_addr m, rx_addr val, rx_addr where) {
    rx_addr head = U32(m, 0x18);
    if (U32(m, 0x1c) == 0) return vt_insert_at(t, m, head, 1, val);
    if (where == U32(head, 0)) {
        if (t->less(val, where + 0xc)) return vt_insert_at(t, m, where, 1, val);
    } else if (where == head) {
        if (t->less(U32(head, 8) + 0xc, val)) return vt_insert_at(t, m, U32(head, 8), 0, val);
    } else if (t->less(val, where + 0xc)) {
        rx_addr p = vt_prev(t, where);
        if (t->less(p + 0xc, val)) {
            if (vt_nil(t, U32(p, 8))) return vt_insert_at(t, m, p, 0, val);
            return vt_insert_at(t, m, where, 1, val);
        }
    } else if (t->less(where + 0xc, val)) {
        rx_addr n = vt_next(t, where);
        if (n == head || t->less(val, n + 0xc)) {
            if (vt_nil(t, U32(where, 8))) return vt_insert_at(t, m, where, 0, val);
            return vt_insert_at(t, m, n, 1, val);
        }
    }
    return vt_insert_unique(t, m, val);
}

/* the leftmost / rightmost node of a subtree (0041bbc0 the max) */
static rx_addr tree_min(const vt_tree* t, rx_addr n) { while (!vt_nil(t, U32(n, 0))) n = U32(n, 0); return n; }
static rx_addr tree_max(const vt_tree* t, rx_addr n) { while (!vt_nil(t, U32(n, 8))) n = U32(n, 8); return n; }

/* erase(iterator) (0041b840: s0 tree, s1 out, s2 / s3 the iterator; RET 0x10): the node unlinked (its successor taking its place when
 * it has two children), the tree rebalanced, the value destroyed (dtor), the node freed (ReflexAlignedFree), the size counted down;
 * returns the next node */
rx_addr vt_erase(const vt_tree* t, rx_addr m, rx_addr erased, void (*dtor)(rx_addr node)) {
    if (vt_nil(t, erased)) tree_fatal("erase(end()) (out_of_range)", erased);
    rx_addr head = U32(m, 0x18);
    const rx_addr next = vt_next(t, erased);
    rx_addr fix, fixparent, p = erased;
    if (vt_nil(t, U32(p, 0))) fix = U32(p, 8);
    else if (vt_nil(t, U32(p, 8))) fix = U32(p, 0);
    else { p = next; fix = U32(p, 8); }
    if (p == erased) {
        fixparent = U32(erased, 4);
        if (!vt_nil(t, fix)) SU32(fix, 4, fixparent);
        if (U32(head, 4) == erased) SU32(head, 4, fix);
        else if (U32(fixparent, 0) == erased) SU32(fixparent, 0, fix);
        else SU32(fixparent, 8, fix);
        if (U32(head, 0) == erased) SU32(head, 0, vt_nil(t, fix) ? fixparent : tree_min(t, fix));
        if (U32(head, 8) == erased) SU32(head, 8, vt_nil(t, fix) ? fixparent : tree_max(t, fix));
    } else {
        SU32(U32(erased, 0), 4, p);
        SU32(p, 0, U32(erased, 0));
        if (p == U32(erased, 8)) fixparent = p;
        else {
            fixparent = U32(p, 4);
            if (!vt_nil(t, fix)) SU32(fix, 4, fixparent);
            SU32(fixparent, 0, fix);
            SU32(p, 8, U32(erased, 8));
            SU32(U32(erased, 8), 4, p);
        }
        if (U32(head, 4) == erased) SU32(head, 4, p);
        else if (U32(U32(erased, 4), 0) == erased) SU32(U32(erased, 4), 0, p);
        else SU32(U32(erased, 4), 8, p);
        SU32(p, 4, U32(erased, 4));
        uint32_t c = colour(t, p); set_colour(t, p, colour(t, erased)); set_colour(t, erased, c);
    }
    if (colour(t, erased) == BLACK) {
        for (; fix != U32(head, 4) && colour(t, fix) == BLACK; fix = fixparent, fixparent = U32(fix, 4)) {
            if (fix == U32(fixparent, 0)) {
                p = U32(fixparent, 8);
                if (colour(t, p) == RED) { set_colour(t, p, BLACK); set_colour(t, fixparent, RED); rotate_left(t, m, fixparent); p = U32(fixparent, 8); }
                if (vt_nil(t, p)) continue;
                if (colour(t, U32(p, 0)) == BLACK && colour(t, U32(p, 8)) == BLACK) { set_colour(t, p, RED); continue; }
                if (colour(t, U32(p, 8)) == BLACK) { set_colour(t, U32(p, 0), BLACK); set_colour(t, p, RED); rotate_right(t, m, p); p = U32(fixparent, 8); }
                set_colour(t, p, colour(t, fixparent)); set_colour(t, fixparent, BLACK); set_colour(t, U32(p, 8), BLACK);
                rotate_left(t, m, fixparent);
                break;
            } else {
                p = U32(fixparent, 0);
                if (colour(t, p) == RED) { set_colour(t, p, BLACK); set_colour(t, fixparent, RED); rotate_right(t, m, fixparent); p = U32(fixparent, 0); }
                if (vt_nil(t, p)) continue;
                if (colour(t, U32(p, 8)) == BLACK && colour(t, U32(p, 0)) == BLACK) { set_colour(t, p, RED); continue; }
                if (colour(t, U32(p, 0)) == BLACK) { set_colour(t, U32(p, 8), BLACK); set_colour(t, p, RED); rotate_left(t, m, p); p = U32(fixparent, 0); }
                set_colour(t, p, colour(t, fixparent)); set_colour(t, fixparent, BLACK); set_colour(t, U32(p, 0), BLACK);
                rotate_right(t, m, fixparent);
                break;
            }
        }
        set_colour(t, fix, BLACK);
    }
    if (dtor) dtor(erased);
    rx_reflex_aligned_free(erased);
    if (U32(m, 0x1c)) SU32(m, 0x1c, U32(m, 0x1c) - 1);
    return next;
}
