/* fx_tree.c -- the std::map<std::string, int> red-black tree code the stunt scorer's trick maps (+0x324 / +0x344) use (msvcp's
 * inlined tree instantiated by the game): insert with a hint 006cd4a0 and the helpers it calls (the node constructor 005bac00,
 * link + rebalance 005ba570, the unique insert 005ba210, the rotations 00851f50 / 005bb590, the iterator steps 00593dc0 /
 * 0063bf50, the iterator compare 004fd0c0, the size 00608390). notes/fx.md.
 *
 * Map: +0 the container proxy, +0x18 the head node, +0x1c the size. Node (0x30 bytes): +0 left, +4 parent, +8 right, +0xc the key
 * (a std::string, 0x1c bytes), +0x28 the int, +0x2c the colour (0 red, 1 black), +0x2d the nil flag. The head's left / right are
 * the first / last node, its parent the root. Iterator: {container proxy, node}. Checked iterators report through
 * _invalid_parameter_noinfo (never in practice); the code goes on as the original does after it returns. A map of 0x7fffffe
 * entries throws std::length_error in the original (005ba570): unreachable, a fatal here. The SEH record of 005bac00 (fs:[0] is
 * linked in and restored) is written to the frame; the TEB's list head itself is restored at the end.
 * Registers as the original: ESI / EDI inputs of the iterator leaves, EDI = the map of 005ba570. */
#include <stdio.h>
#include <stdlib.h>
#include "fx.h"
#include "fx_util.h"
#include "build_alloc.h"

#define FX_FS0 rx_teb()    /* the TEB's exception list head (fs:[0]) */

static int nil(rx_addr n) { return U8(n, 0x2d) != 0; }
static void invalid(void) { crt_invalid_parameter_noinfo(); }
static int str_less(rx_addr a, rx_addr b, rx_addr ret) {
    const uint32_t args[2] = {a, b};
    const rx_addr s = fx_push(2, args, ret);
    const int r = (crt_str_less(a, b) & 0xff) != 0;
    fx_pop(s);
    return r;
}

/* ---- 00608390 (ECX = map; EAX): the size. Frame: push ebp; mov ebp, esp; push ecx. ---- */
uint32_t rx_fx_map_size(rx_addr map) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4u, fx_reg(FX_EBP)); SU32(sp0, -8u, map);
    return U32(map, 0x1c);
}
RX_FUNCTION(0x00608390, rx_fx_map_size)

/* ---- 00593dc0 TreeIterator_Increment (ESI = iterator): the in-order successor. Frame: push edi. ---- */
void rx_fx_tree_next(rx_addr it) {
    SU32(rx_sp(), -4u, fx_reg(FX_EDI));
    if (U32(it, 0) == 0) invalid();
    const rx_addr node = U32(it, 4);
    if (nil(node)) { invalid(); return; }
    rx_addr c = U32(node, 8);
    if (!nil(c)) {
        rx_addr n = U32(c, 0);
        while (!nil(n)) { c = n; n = U32(c, 0); }
        SU32(it, 4, c);
        return;
    }
    rx_addr p = U32(node, 4);
    if (!nil(p)) {
        while (U32(it, 4) == U32(p, 8)) {
            SU32(it, 4, p);
            p = U32(p, 4);
            if (nil(p)) break;
        }
    }
    SU32(it, 4, p);
}
RX_FUNCTION(0x00593dc0, rx_fx_tree_next)

/* ---- 0063bf50 (ESI = iterator): the in-order predecessor (the head steps to the last node). Frame: push edi. ---- */
void rx_fx_tree_prev(rx_addr it) {
    SU32(rx_sp(), -4u, fx_reg(FX_EDI));
    if (U32(it, 0) == 0) invalid();
    const rx_addr node = U32(it, 4);
    if (nil(node)) {
        const rx_addr r = U32(node, 8);
        SU32(it, 4, r);
        if (nil(r)) invalid();
        return;
    }
    const rx_addr l = U32(node, 0);
    if (!nil(l)) {
        rx_addr c = l, n = U32(c, 8);
        while (!nil(n)) { c = n; n = U32(c, 8); }
        SU32(it, 4, c);
        return;
    }
    rx_addr p = U32(node, 4);
    if (!nil(p)) {
        while (U32(it, 4) == U32(p, 0)) {
            SU32(it, 4, p);
            p = U32(p, 4);
            if (nil(p)) break;
        }
    }
    if (nil(U32(it, 4))) { invalid(); return; }
    SU32(it, 4, p);
}
RX_FUNCTION(0x0063bf50, rx_fx_tree_prev)

/* ---- 004fd0c0 (EDI = a, ESI = b; AL): the iterators are equal (same container, same node); EAX keeps the node's upper bytes. ---- */
uint32_t rx_fx_tree_iter_eq(rx_addr a, rx_addr b) {
    if (U32(a, 0) == 0 || U32(a, 0) != U32(b, 0)) invalid();
    const uint32_t eq = U32(a, 4) == U32(b, 4);
    return (U32(a, 4) & 0xffffff00u) | eq;
}
RX_FUNCTION(0x004fd0c0, rx_fx_tree_iter_eq)

/* ---- 00851f50 _Lrotate (ECX = node w; stdcall map; RET 4) / 005bb590 _Rrotate: the right (left) child becomes the parent. ---- */
void rx_fx_tree_lrotate(rx_addr w, rx_addr map) {
    const rx_addr p = U32(w, 8);
    SU32(w, 8, U32(p, 0));
    if (!nil(U32(p, 0))) SU32(U32(p, 0), 4, w);
    SU32(p, 4, U32(w, 4));
    const rx_addr head = U32(map, 0x18);
    if (w == U32(head, 4)) { SU32(head, 4, p); }
    else {
        const rx_addr par = U32(w, 4);
        if (w == U32(par, 0)) SU32(par, 0, p); else SU32(par, 8, p);
    }
    SU32(p, 0, w);
    SU32(w, 4, p);
}
RX_FUNCTION(0x00851f50, rx_fx_tree_lrotate)
void rx_fx_tree_rrotate(rx_addr w, rx_addr map) {
    const rx_addr p = U32(w, 0);
    SU32(w, 0, U32(p, 8));
    if (!nil(U32(p, 8))) SU32(U32(p, 8), 4, w);
    SU32(p, 4, U32(w, 4));
    const rx_addr head = U32(map, 0x18);
    if (w == U32(head, 4)) { SU32(head, 4, p); }
    else {
        const rx_addr par = U32(w, 4);
        if (w == U32(par, 8)) SU32(par, 8, p); else SU32(par, 0, p);
    }
    SU32(p, 8, w);
    SU32(w, 4, p);
}
RX_FUNCTION(0x005bb590, rx_fx_tree_rrotate)

/* ---- 005bac00 (stdcall left, parent, right, value*, colour; RET 0x14; EAX = the node): a node: ReflexAlignedAlloc(0x30), the
 * links, the key copy-constructed from the value's string, the int copied, the colour, nil 0. Frame: push ebp; the SEH record
 * (-1, handler 0x901e70, fs:[0]); push ecx, ebx, esi, edi; the cookie. ---- */
rx_addr rx_fx_tree_node_new(rx_addr left, rx_addr parent, rx_addr right, rx_addr value, uint32_t colour) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    const rx_addr ebp = sp0 - 4;
    SU32(ebp, 0, sv.ebp);
    fx_set_reg(FX_EBP, ebp);
    SU32(sp0, -8u, 0xffffffffu); SU32(sp0, -0xcu, 0x00901e70u);
    const uint32_t seh_prev = U32(FX_FS0, 0);
    SU32(sp0, -0x10u, seh_prev);
    SU32(sp0, -0x14u, fx_reg(FX_ECX));
    SU32(sp0, -0x18u, sv.ebx); SU32(sp0, -0x1cu, sv.esi); SU32(sp0, -0x20u, sv.edi);
    SU32(sp0, -0x24u, U32(0, 0x009b45e4) ^ ebp);
    SU32(FX_FS0, 0, ebp - 0xc);
    SU32(ebp, -0x10u, sp0 - 0x24);
    rx_set_sp(sp0 - 0x24);
    fx_set_reg(FX_EBX, value);
    rx_addr n;
    { const uint32_t a[1] = {0x30}; const rx_addr s = fx_push(1, a, 0x005bac33); n = rx_reflex_aligned_alloc(0x30); fx_pop(s); }
    SU32(ebp, 0x14, n);
    fx_set_reg(FX_ESI, n);
    SU32(ebp, -4u, 0);
    if (n != 0) {
        SU32(n, 0, left); SU32(n, 4, parent); SU32(n, 8, right);
        fx_set_reg(FX_EDI, n + 0xc);
        { const uint32_t a[1] = {value}; const rx_addr s = fx_push(1, a, 0x005bac63); crt_str_ctor_copy(n + 0xc, value); fx_pop(s); }
        SU32(n + 0xc, 0x1c, U32(value, 0x1c));
        SU8(n, 0x2c, (uint8_t)colour);
        SU8(n, 0x2d, 0);
    }
    SU32(ebp, -4u, 0xffffffffu);
    SU32(FX_FS0, 0, seh_prev);
    fx_restore(sv);
    rx_set_sp(sp0);
    return n;
}
RX_FUNCTION(0x005bac00, rx_fx_tree_node_new)

/* ---- 005ba570 _Insert (EDI = map, EAX = where; stdcall out, addleft, value*; RET 0xc; EAX = out): a node for the value linked
 * as where's left (addleft) / right child (the head when the map is empty), then the red-black fix-up; out = {[map], node}.
 * Frame: sub esp, 0x44; push ebx, ebp, esi: E = sp0 - 0x50. ---- */
rx_addr rx_fx_tree_insert_at(rx_addr map, rx_addr where, rx_addr out, uint32_t addleft, rx_addr value) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    const rx_addr a0 = sp0 - 0x44;
    fx_push_regs(a0, 3, (int[]){FX_EBX, FX_EBP, FX_ESI});
    const rx_addr E = a0 - 12;
    rx_set_sp(E);
    fx_set_reg(FX_ESI, where);
    if (U32(map, 0x1c) >= 0x7fffffeu) { fprintf(stderr, "rxcore: map/set<T> too long (005ba570)\n"); abort(); }
    rx_addr head = U32(map, 0x18);
    rx_addr node;
    { const uint32_t a[5] = {head, where, head, value, 0}; const rx_addr s = fx_push(5, a, 0x005ba5da); node = rx_fx_tree_node_new(head, where, head, value, 0); fx_pop(s); }
    fx_set_reg(FX_EBP, node); fx_set_reg(FX_EBX, 1);
    SU32(map, 0x1c, U32(map, 0x1c) + 1);
    if (where == U32(map, 0x18)) {
        SU32(where, 4, node);
        SU32(U32(map, 0x18), 0, node);
        SU32(U32(map, 0x18), 8, node);
    } else if ((addleft & 0xff) != 0) {
        SU32(where, 0, node);
        if (where == U32(U32(map, 0x18), 0)) SU32(U32(map, 0x18), 0, node);
    } else {
        SU32(where, 8, node);
        if (where == U32(U32(map, 0x18), 8)) SU32(U32(map, 0x18), 8, node);
    }
    rx_addr cur = node;
    while (U8(U32(cur, 4), 0x2c) == 0) {
        rx_addr par = U32(cur, 4), gp = U32(par, 4);
        if (par == U32(gp, 0)) {
            rx_addr unc = U32(gp, 8);
            if (U8(unc, 0x2c) == 0) {
                SU8(par, 0x2c, 1); SU8(unc, 0x2c, 1);
                SU8(U32(U32(cur, 4), 4), 0x2c, 0);
                cur = U32(U32(cur, 4), 4);
            } else {
                if (cur == U32(par, 8)) {
                    cur = par;
                    const uint32_t a[1] = {map}; const rx_addr s = fx_push(1, a, 0x005ba665); rx_fx_tree_lrotate(cur, map); fx_pop(s);
                }
                SU8(U32(cur, 4), 0x2c, 1);
                SU8(U32(U32(cur, 4), 4), 0x2c, 0);
                const rx_addr g = U32(U32(cur, 4), 4);
                const uint32_t a[1] = {map}; const rx_addr s = fx_push(1, a, 0x005ba681); rx_fx_tree_rrotate(g, map); fx_pop(s);
            }
        } else {
            rx_addr unc = U32(gp, 0);
            if (U8(unc, 0x2c) == 0) {
                SU8(par, 0x2c, 1); SU8(unc, 0x2c, 1);
                SU8(U32(U32(cur, 4), 4), 0x2c, 0);
                cur = U32(U32(cur, 4), 4);
            } else {
                if (cur == U32(par, 0)) {
                    cur = par;
                    const uint32_t a[1] = {map}; const rx_addr s = fx_push(1, a, 0x005ba6ad); rx_fx_tree_rrotate(cur, map); fx_pop(s);
                }
                SU8(U32(cur, 4), 0x2c, 1);
                SU8(U32(U32(cur, 4), 4), 0x2c, 0);
                const rx_addr g = U32(U32(cur, 4), 4);
                const uint32_t a[1] = {map}; const rx_addr s = fx_push(1, a, 0x005ba6c9); rx_fx_tree_lrotate(g, map); fx_pop(s);
            }
        }
    }
    SU8(U32(U32(map, 0x18), 4), 0x2c, 1);
    SU32(out, 4, node);
    SU32(out, 0, U32(map, 0));
    fx_restore(sv);
    rx_set_sp(sp0);
    return out;
}
RX_FUNCTION(0x005ba570, rx_fx_tree_insert_at)

/* ---- 005ba210 (stdcall map, out {iterator, inserted}, value*; RET 0xc; EAX = out): insert unique: descend from the root with
 * the value's key (op< against each node's key), then link it at the leaf unless an equal key exists: out = {container, node,
 * inserted}. Frame: sub esp, 0x10; push ebx, ebp, esi, edi: E = sp0 - 0x20 (+0x10 / +0x14 the iterator copy, +0x1c the first node,
 * +0x18 the insert's result, +0x28 the descent's last compare (over the out argument's low byte)). ---- */
rx_addr rx_fx_tree_insert_unique(rx_addr map, rx_addr out, rx_addr value) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    const rx_addr a0 = sp0 - 0x10;
    fx_push_regs(a0, 4, (int[]){FX_EBX, FX_EBP, FX_ESI, FX_EDI});
    const rx_addr E = a0 - 16;
    rx_set_sp(E);
    fx_set_reg(FX_EBX, map); fx_set_reg(FX_EBP, out);
    rx_addr where = U32(map, 0x18);
    rx_addr cur = U32(where, 4);
    fx_set_reg(FX_EDI, where); fx_set_reg(FX_ESI, cur);
    uint8_t al = 1;
    SU8(E, 0x28, 1);
    while (!nil(cur)) {
        al = (uint8_t)str_less(value, cur + 0xc, 0x005ba242u);
        SU8(E, 0x28, al);
        where = cur;
        cur = al ? U32(cur, 0) : U32(cur, 8);
        fx_set_reg(FX_EDI, where); fx_set_reg(FX_ESI, cur);
    }
    const rx_addr proxy = U32(map, 0);
    fx_set_reg(FX_ESI, proxy);
    SU32(E, 0x14, where); SU32(E, 0x10, proxy);
    if (al != 0) {
        const rx_addr first = U32(U32(map, 0x18), 0);
        SU32(E, 0x1c, first);
        if (proxy == 0) invalid();
        if (where == first) {
            fx_set_reg(FX_EDI, map);
            rx_addr r;
            { const uint32_t a[3] = {E + 0x18, 1, value}; const rx_addr s = fx_push(3, a, 0x005ba29a); r = rx_fx_tree_insert_at(map, where, E + 0x18, 1, value); fx_pop(s); }
            SU32(out, 0, U32(r, 0)); SU32(out, 4, U32(r, 4)); SU8(out, 8, 1);
            fx_restore(sv);
            rx_set_sp(sp0);
            return out;
        }
        fx_set_reg(FX_ESI, E + 0x10);
        { const rx_addr s = fx_push(0, 0, 0x005ba2be); rx_fx_tree_prev(E + 0x10); fx_pop(s); }
        fx_set_reg(FX_ESI, U32(E, 0x10));
    }
    if (str_less(U32(E, 0x14) + 0xc, value, 0x005ba2d5)) {
        fx_set_reg(FX_EDI, map);
        rx_addr r;
        { const uint32_t a[3] = {E + 0x18, U32(E, 0x28), value}; const rx_addr s = fx_push(3, a, 0x005ba2f4); r = rx_fx_tree_insert_at(map, where, E + 0x18, U32(E, 0x28), value); fx_pop(s); }
        SU32(out, 4, U32(r, 4)); SU32(out, 0, U32(r, 0)); SU8(out, 8, 1);
    } else {
        SU32(out, 0, U32(E, 0x10)); SU32(out, 4, U32(E, 0x14)); SU8(out, 8, 0);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
    return out;
}
RX_FUNCTION(0x005ba210, rx_fx_tree_insert_unique)

/* the insert call of 005ba570 at the original's stack (arguments out, addleft, value; EDI = map; EAX = where) */
static void insert_call(rx_addr map, rx_addr out, rx_addr value, rx_addr where, uint32_t left, rx_addr ret) {
    fx_set_reg(FX_EDI, map);
    const uint32_t a[3] = {out, left, value};
    const rx_addr s = fx_push(3, a, ret);
    rx_fx_tree_insert_at(map, where, out, left, value);
    fx_pop(s);
}
#define INSERT_AT_ insert_call

/* ---- 006cd4a0 (stdcall map, out, value*, hint proxy, hint node; RET 0x14; EAX = out): insert the value with a hint (the map's
 * iterator before which it should go): empty map: link at the head; hint = begin: before it when the value is below its key; hint
 * = end: after the last node when above its key; else between the hint's predecessor and the hint (or the hint and its
 * successor); a hint that does not fit falls back to the unique insert (005ba210); out = {container, node}. Frame: sub esp,
 * 0x18; push ebx, ebp, esi, edi: E = sp0 - 0x28 (+0x14 / +0x18 and +0x1c / +0x20 two iterators). ---- */
rx_addr rx_fx_map_insert_hint(rx_addr map, rx_addr out, rx_addr value, rx_addr hint_proxy, rx_addr hint_node) {
    fx_frame f; fx_frame_flat(&f, 0x18, FX_EBX, FX_EBP, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    fx_set_reg(FX_EBX, map); fx_set_reg(FX_EBP, out);
    uint32_t size;
    { fx_set_reg(FX_ECX, map); const rx_addr s = fx_push(0, 0, 0x006cd4b6); size = rx_fx_map_size(map); fx_pop(s); }
    if (size == 0) { INSERT_AT_(map, out, value, U32(map, 0x18), 1, 0x006cd4cc); fx_frame_leave(&f); return out; }
    rx_addr esi = U32(U32(map, 0x18), 0);            /* begin */
    fx_set_reg(FX_ESI, esi); fx_set_reg(FX_EDI, U32(0, 0x00916224));
    if (hint_proxy == 0 || hint_proxy != U32(map, 0)) invalid();
    if (hint_node == esi) {                           /* the hint is begin */
        fx_set_reg(FX_ESI, value);
        if (str_less(value, hint_node + 0xc, 0x006cd50e)) { INSERT_AT_(map, out, value, hint_node, 1, 0x006cd528); fx_frame_leave(&f); return out; }
        goto fallback;
    }
    esi = U32(map, 0x18);
    fx_set_reg(FX_ESI, esi);
    if (hint_proxy == 0 || hint_proxy != U32(map, 0)) invalid();
    if (hint_node == esi) {                           /* the hint is end */
        const rx_addr last = U32(U32(map, 0x18), 8);
        if (str_less(last + 0xc, value, 0x006cd560)) { INSERT_AT_(map, out, value, last, 0, 0x006cd580); fx_frame_leave(&f); return out; }
        goto fallback;
    }
    fx_set_reg(FX_EDI, value);
    if (str_less(value, hint_node + 0xc, 0x006cd59b)) {
        SU32(E, 0x14, hint_proxy); SU32(E, 0x18, hint_node);
        fx_set_reg(FX_ESI, E + 0x14);
        { const rx_addr s = fx_push(0, 0, 0x006cd5bb); rx_fx_tree_prev(E + 0x14); fx_pop(s); }
        esi = U32(E, 0x18);
        fx_set_reg(FX_ESI, esi);
        if (str_less(esi + 0xc, value, 0x006cd5ca)) {
            if (nil(U32(esi, 8))) INSERT_AT_(map, out, value, esi, 0, 0x006cd5e7);     /* prev has no right child */
            else INSERT_AT_(map, out, value, hint_node, 1, 0x006cd5ff);
            fx_frame_leave(&f);
            return out;
        }
    }
    if (!str_less(hint_node + 0xc, value, 0x006cd61a)) goto fallback;
    SU32(E, 0x14, hint_proxy); SU32(E, 0x18, hint_node);
    SU32(E, 0x20, U32(map, 0x18)); SU32(E, 0x1c, U32(map, 0));
    fx_set_reg(FX_ESI, E + 0x14);
    { const rx_addr s = fx_push(0, 0, 0x006cd64b); rx_fx_tree_next(E + 0x14); fx_pop(s); }
    fx_set_reg(FX_ESI, E + 0x1c); fx_set_reg(FX_EDI, E + 0x14);
    uint32_t eq;
    { const rx_addr s = fx_push(0, 0, 0x006cd658); eq = rx_fx_tree_iter_eq(E + 0x14, E + 0x1c) & 0xff; fx_pop(s); }
    esi = U32(E, 0x18);
    fx_set_reg(FX_ESI, esi);
    if (eq == 0 && !str_less(value, esi + 0xc, 0x006cd66f)) goto fallback;
    if (nil(U32(hint_node, 8))) INSERT_AT_(map, out, value, hint_node, 0, 0x006cd692);
    else INSERT_AT_(map, out, value, esi, 1, 0x006cd6ad);
    fx_frame_leave(&f);
    return out;
fallback:
    fx_set_reg(FX_ESI, value);
    {
        const uint32_t a[3] = {map, E + 0x1c, value};
        const rx_addr s = fx_push(3, a, 0x006cd6c9);
        const rx_addr r = rx_fx_tree_insert_unique(map, E + 0x1c, value);
        fx_pop(s);
        SU32(out, 0, U32(r, 0));
        SU32(out, 4, U32(r, 4));
    }
    fx_frame_leave(&f);
    return out;
}
RX_FUNCTION(0x006cd4a0, rx_fx_map_insert_hint)
