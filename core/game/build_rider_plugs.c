/* build_rider_plugs.c -- construction unit 7 (riders): RiderAnimation's plugs into its animation tree (0061ad70: the
 * AttachPlugsTreeWalker 0061ae00 visits every animator and binds it by class and name: the procedural animator's controllers, the
 * IK animator's solvers from the RiderPhysics document (006afd50 / 006b01b0 / 006afdc0), the expression animator's bone constraints,
 * the switch animators' branch tables (0061afc0: the "Tree" and "Rider" switches), the blend weights / performance times / animation
 * percents into the rider's listener floats (0061bfa0 / 0061c000 / 0061c060 / 0061bf00 / 0061b9d0)), the std::map<std::string, ..>
 * helpers they use. Hand transliterations of the disassembly. */
#include "build_rider.h"
#include "build_rider_util.h"
#include "crt.h"
#include "anim.h"
#include "world_node.h"

/* ---------------------------------------------------------------------------------------------------------------------------
 * std::map<std::string, V> (MSVC 9 _Tree; node 0x30: left, parent, right, key (basic_string at +0xc), value +0x28, color +0x2c, isnil
 * +0x2d; head at map + 0x18, size + 0x1c). Keys are game std::string objects (crt_cpp.c's msvcp90 ports). */

/* a game strcpy (the original's inline byte loop) */
static void crt_strcpy_g(rx_addr d, rx_addr s) { for (uint32_t k = 0;; k++) { uint8_t c = U8(s, k); SU8(d, k, c); if (!c) break; } }
static int isnil(rx_addr n) { return U8(n, 0x2d) != 0; }
static int less_kn(rx_addr key, rx_addr node) { return (crt_str_less(key, node + 0xc) & 0xff) != 0; }
static int less_nk(rx_addr node, rx_addr key) { return (crt_str_less(node + 0xc, key) & 0xff) != 0; }
/* 00851f50 _Lrotate / 005bb590 _Rrotate (ESI / ECX = node, stack the map) */
static void lrotate(rx_addr map, rx_addr w) {
    rx_addr p = U32(w, 8);
    SU32(w, 8, U32(p, 0));
    if (!isnil(U32(p, 0))) SU32(U32(p, 0), 4, w);
    SU32(p, 4, U32(w, 4));
    rx_addr head = U32(map, 0x18);
    if (w == U32(head, 4)) SU32(head, 4, p);
    else if (w == U32(U32(w, 4), 0)) SU32(U32(w, 4), 0, p);
    else SU32(U32(w, 4), 8, p);
    SU32(p, 0, w); SU32(w, 4, p);
}
static void rrotate(rx_addr map, rx_addr w) {
    rx_addr p = U32(w, 0);
    SU32(w, 0, U32(p, 8));
    if (!isnil(U32(p, 8))) SU32(U32(p, 8), 4, w);
    SU32(p, 4, U32(w, 4));
    rx_addr head = U32(map, 0x18);
    if (w == U32(head, 4)) SU32(head, 4, p);
    else if (w == U32(U32(w, 4), 8)) SU32(U32(w, 4), 8, p);
    else SU32(U32(w, 4), 0, p);
    SU32(p, 8, w); SU32(w, 4, p);
}
/* 0063bd10 / 00586700 _Insert(addleft, where, pair) (EDI = map, EAX = where): the node (0063bea0 / 00851b80: ReflexAlignedAlloc(0x30),
 * the key copy-constructed, the value copied (vsize bytes)), linked, the red-black fix-up; returns the node */
static rx_addr insert_at(rx_addr map, int addleft, rx_addr where, rx_addr pair, uint32_t vsize) {
    if (U32(map, 0x1c) >= 0x7fffffeu) rr_fatal("map/set<T> too long", map);
    rx_addr head = U32(map, 0x18);
    rx_addr n = rr_alloc(0x30);
    if (n) {
        SU32(n, 0, head); SU32(n, 4, where); SU32(n, 8, head);
        crt_str_ctor_copy(n + 0xc, pair);
        if (vsize == 2) SU16(n + 0xc, 0x1c, U16(pair, 0x1c)); else SU32(n + 0xc, 0x1c, U32(pair, 0x1c));
        SU8(n, 0x2c, 0); SU8(n, 0x2d, 0);
    }
    SU32(map, 0x1c, U32(map, 0x1c) + 1);
    if (where == head) { SU32(head, 4, n); SU32(U32(map, 0x18), 0, n); SU32(U32(map, 0x18), 8, n); }
    else if (addleft) { SU32(where, 0, n); if (where == U32(U32(map, 0x18), 0)) SU32(U32(map, 0x18), 0, n); }
    else { SU32(where, 8, n); if (where == U32(U32(map, 0x18), 8)) SU32(U32(map, 0x18), 8, n); }
    for (rx_addr p = n; U8(U32(p, 4), 0x2c) == 0;) {
        rx_addr par = U32(p, 4), gp = U32(par, 4);
        if (par == U32(gp, 0)) {
            rx_addr u = U32(gp, 8);
            if (U8(u, 0x2c) == 0) { SU8(par, 0x2c, 1); SU8(u, 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); p = U32(U32(p, 4), 4); }
            else {
                if (p == U32(par, 8)) { p = par; lrotate(map, p); }
                SU8(U32(p, 4), 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); rrotate(map, U32(U32(p, 4), 4));
            }
        } else {
            rx_addr u = U32(gp, 0);
            if (U8(u, 0x2c) == 0) { SU8(par, 0x2c, 1); SU8(u, 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); p = U32(U32(p, 4), 4); }
            else {
                if (p == U32(par, 0)) { p = par; rrotate(map, p); }
                SU8(U32(p, 4), 0x2c, 1); SU8(U32(U32(p, 4), 4), 0x2c, 0); lrotate(map, U32(U32(p, 4), 4));
            }
        }
    }
    SU8(U32(U32(map, 0x18), 4), 0x2c, 1);
    return n;
}
static rx_addr tree_next(rx_addr it) {   /* TreeIterator_Increment 00593dc0 */
    if (!isnil(U32(it, 8))) { it = U32(it, 8); while (!isnil(U32(it, 0))) it = U32(it, 0); return it; }
    rx_addr p;
    while (!isnil(p = U32(it, 4)) && it == U32(p, 8)) it = p;
    return p;
}
static rx_addr tree_prev(rx_addr it) {   /* 0063bf50 */
    if (isnil(it)) return U32(it, 8);
    if (!isnil(U32(it, 0))) { it = U32(it, 0); while (!isnil(U32(it, 8))) it = U32(it, 8); return it; }
    rx_addr p;
    while (!isnil(p = U32(it, 4)) && it == U32(p, 0)) it = p;
    return isnil(it) ? it : p;
}
/* 0063bbf0 insert(pair) (unique) */
static rx_addr insert_unique(rx_addr map, rx_addr pair, uint32_t vsize) {
    rx_addr head = U32(map, 0x18), t = U32(head, 4), where = head;
    int left = 1;
    while (!isnil(t)) { where = t; left = less_kn(pair, t); t = left ? U32(t, 0) : U32(t, 8); }
    rx_addr it = where;
    if (left) {
        if (it == U32(head, 0)) return insert_at(map, 1, where, pair, vsize);
        it = tree_prev(it);
    }
    if (less_nk(it, pair)) return insert_at(map, left, where, pair, vsize);
    return it;
}
/* 0063b9b0 / 00850f30 insert(hint, pair) */
static rx_addr insert_hint(rx_addr map, rx_addr hint, rx_addr pair, uint32_t vsize) {
    rx_addr head = U32(map, 0x18);
    if (U32(map, 0x1c) == 0) return insert_at(map, 1, head, pair, vsize);
    if (hint == U32(head, 0)) {
        if (less_kn(pair, hint)) return insert_at(map, 1, hint, pair, vsize);
    } else if (hint == head) {
        if (less_nk(U32(head, 8), pair)) return insert_at(map, 0, U32(head, 8), pair, vsize);
    } else if (less_kn(pair, hint)) {
        rx_addr prev = tree_prev(hint);
        if (less_nk(prev, pair)) {
            if (isnil(U32(prev, 8))) return insert_at(map, 0, prev, pair, vsize);
            return insert_at(map, 1, hint, pair, vsize);
        }
    } else if (less_nk(hint, pair)) {
        rx_addr next = tree_next(hint);
        if (next == head || less_kn(pair, next)) {
            if (isnil(U32(hint, 8))) return insert_at(map, 0, hint, pair, vsize);
            return insert_at(map, 1, next, pair, vsize);
        }
    }
    return insert_unique(map, pair, vsize);
}
/* 0063b6e0 (vsize 2) / 00850cd0 (vsize 4) operator[](key) (EDI = map, s0 &key; RET 4): lower bound (008519a0); when absent a pair
 * (a copy of the key, value 0) inserted at the hint (and destroyed); the value's address */
rx_addr rx_rider_strmap_index(rx_addr map, rx_addr key, uint32_t vsize) {
    rx_addr best = rx_world_string_map_lower_bound(map, key);
    rx_addr head = U32(map, 0x18);
    if (best == head || less_kn(key, best)) {
        RX_FRAME_ENTER(F, 0x28);
        crt_str_ctor_copy(F + 8, key);
        if (vsize == 2) SU16(F + 8, 0x1c, 0); else SU32(F + 8, 0x1c, 0);
        best = insert_hint(map, best, F + 8, vsize);
        crt_str_dtor(F + 8);
        RX_FRAME_LEAVE(F, 0x28);
    }
    return best + 0x28;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * listener floats */

/* 0063c250 (EDI = vector of 8-byte listeners, s0 &listener; RET 4): push_back */
static void listener_push(rx_addr v, rx_addr el) {
    rx_addr b = U32(v, 0xc), e = U32(v, 0x10);
    uint32_t cap = b ? (U32(v, 0x14) - b) >> 3 : 0;
    if (((e - b) >> 3) < cap) { SU32(e, 0, U32(el, 0)); SU32(e, 4, U32(el, 4)); SU32(v, 0x10, e + 8); return; }
    rx_rider_vec8_insert_n(v, 1, el, e);
}
/* {target, 1} into the listeners at `v` */
static void listen(rx_addr v, rx_addr target) {
    RX_FRAME_ENTER(F, 8);
    SU32(F, 0, target); SU32(F, 4, 1);
    listener_push(v, F);
    RX_FRAME_LEAVE(F, 8);
}
/* an animator's parameter address by name (vf11 at +0x2c: AL true and *out), 0 when absent */
static rx_addr param(rx_addr a, rx_addr name) {
    RX_FRAME_ENTER(F, 4);
    SU32(F, 0, 0);
    uint32_t args[2] = {name, F};
    uint32_t ok = rr_vcall(a, 0x2c, 2, args) & 0xff;
    rx_addr p = ok ? U32(F, 0) : 0;
    RX_FRAME_LEAVE(F, 4);
    return ok ? (p ? p : 0xffffffffu) : 0;
}
static rx_addr pval(rx_addr p) { return p == 0xffffffffu ? 0 : p; }
static rx_addr aname(rx_addr a) { return rr_vcall(a, 4, 0, NULL); }
static int has(rx_addr a, rx_addr s) { return crt_strstr(aname(a), s) != 0; }

/* 0061bfa0 / 0061c000 / 0061c060 (ECX = blend animator, s0 ra; RET 4): its BlendPercent drives the listener float at ra + 0x308 /
 * 0x334 / 0x360 */
static void bind_blend(rx_addr a, rx_addr ra, uint32_t off) {
    rx_addr p = param(a, 0x009583c0);
    if (p) listen(ra + off, pval(p));
}
/* 0061bf00 (ESI = performance animator, s0 ra; RET 4): PerformanceTime into ra + 0x46c and + 0x48c */
static void bind_performance(rx_addr a, rx_addr ra) {
    rx_addr p = param(a, 0x00958404);
    if (p) listen(ra + 0x46c, pval(p));
    p = param(a, 0x00958404);
    if (p) listen(ra + 0x48c, pval(p));
}
/* 0061b9d0 (ESI = motion animator, s0 ra; RET 4): the motion's AnimationPercent / TimeScale by its name */
static void bind_motion(rx_addr a, rx_addr ra) {
    static const struct { uint32_t name, off; } direct[] = {
        {0x00958508, 0x418}, {0x00958510, 0x430}, {0x00958518, 0x448}, {0x00958520, 0x460}, {0x00958528, 0x3c4},
        {0x009581ac, 0x3d0}, {0x00958534, 0x3dc}, {0x0095853c, 0x3b8}, {0x00958544, 0x3e8}, {0x00958548, 0x3f4}, {0x0095854c, 0x400},
    };
    for (unsigned k = 0; k < sizeof direct / sizeof direct[0]; k++)
        if (has(a, direct[k].name)) { rx_addr p = param(a, 0x009583d0); if (p) SU32(ra, direct[k].off, pval(p)); return; }
    static const struct { uint32_t name, lst, ts; } timed[] = {
        {0x009584f0, 0x4c0, 0x4cc}, {0x009408a0, 0x538, 0x544}, {0x009584bc, 0x4e8, 0x4f4}, {0x009584c4, 0x510, 0x51c},
    };
    for (unsigned k = 0; k < sizeof timed / sizeof timed[0]; k++)
        if (has(a, timed[k].name)) {
            rx_addr p = param(a, 0x009583d0);
            if (p) listen(ra + timed[k].lst + 0x10, pval(p));   /* 0063c150 */
            p = param(a, 0x00958550);
            if (p) SU32(ra, timed[k].ts, pval(p));
            return;
        }
    rx_addr p = param(a, 0x009583d0);
    if (p) listen(ra + 0x378 + 0x14, pval(p));   /* 0063c110 */
}

/* the switch's branch name copied into a 40-byte slot record (name, u16 branch index at +0x20, float at +0x24) and counted */
static void slot_record(rx_addr ra, uint32_t base, uint32_t cnt_off, uint32_t idx, rx_addr branch, int with_time, rx_addr time_src) {
    uint32_t n = U32(ra, cnt_off);
    rx_addr r = ra + base + 0x28 * n;
    SU16(r, 0x20, (uint16_t)idx);
    if (with_time) SU32(r, 0x24, rr_x87_copy(U32(time_src, 0x94)));
    crt_strcpy_g(r, aname(branch));
    SU32(ra, cnt_off, n + 1);
}

/* 0061afc0 (ECX = switch animator, s0 ra; RET 4): the "Tree" switch (the wreck / air performances, the flail and recover motions, the
 * light / medium / heavy wreck slots) or the "Rider" switch (idles, hops, the ground performance, walk / reverse / whip / stop) */
void rx_rider_bind_switch(rx_addr sw, rx_addr ra) {
    int32_t n = (int32_t)((U32(sw, 0xc8) - U32(sw, 0xc4)) >> 2);
    if (crt_strstr(aname(sw), 0x009583f8)) {   /* "Tree" */
        SU32(ra, 0x59c, sw);
        rx_addr p;
        if ((p = param(sw, 0x00958394))) SU32(ra, 0x568, pval(p));
        if ((p = param(sw, 0x009583a4))) SU32(ra, 0x574, pval(p));
        if ((p = param(sw, 0x009583b4))) SU32(ra, 0x564, pval(p));
        if ((p = param(sw, 0x009583c0))) SU32(ra, 0x58c, pval(p));
        for (int32_t i = 0; i < n; i++) {
            rx_addr b = U32(U32(sw, 0xc4), 4u * (uint32_t)i);
            uint32_t t = U32(b, 4);
            static const uint8_t kind[10] = {0, 1, 2, 4, 4, 4, 4, 4, 4, 3};
            if (t > 9) continue;
            switch (kind[t]) {
            case 0: SU16(ra, 0x5b4, (uint16_t)i); break;
            case 2: SU16(ra, 0x5ac, (uint16_t)i); break;
            case 1:
                if (has(b, 0x009584bc)) SU16(ra, 0x5ae, (uint16_t)i);
                else if (has(b, 0x009584c4)) SU16(ra, 0x5b0, (uint16_t)i);
                else if (has(b, 0x009584d0)) SU16(ra, 0x5b2, (uint16_t)i);
                break;
            case 3:
                if (has(b, 0x00958400)) {   /* "Air": the performance's name -> branch index (map +0xaf7c), PerformanceTime (map +0xafbc) */
                    RX_FRAME_ENTER(F, 0x20);
                    crt_str_ctor_cstr(F, b + 0x9c);
                    SU16(rx_rider_strmap_index(ra + 0xaf7c, F, 2), 0, (uint16_t)i);
                    /* the PerformanceTime parameter's address (the out slot is not preset: a performance animator always has it) */
                    rx_addr pt = pval(param(b, 0x00958404));
                    SU32(rx_rider_strmap_index(ra + 0xafbc, F, 4), 0, pt);
                    uint32_t cnt = U32(ra, 0xba24);
                    SU16(ra, 0xb01c + 0x28 * cnt, (uint16_t)i);
                    crt_strcpy_g(ra + 0xaffc + 0x28 * cnt, aname(b));
                    SU32(ra, 0xba24, cnt + 1);
                    crt_str_dtor(F);
                    RX_FRAME_LEAVE(F, 0x20);
                } else if (has(b, 0x00958414)) {   /* "Wreck": the light / medium / heavy slot by its direction */
                    static const struct { uint32_t name, slot; } w[] = {
                        {0x0095841c, 3}, {0x00958428, 2}, {0x00958434, 1}, {0x00958440, 0}, {0x00958450, 7}, {0x00958460, 6},
                        {0x0095846c, 5}, {0x00958478, 4}, {0x00958488, 0xb}, {0x00958494, 0xa}, {0x009584a0, 9}, {0x009584ac, 8},
                    };
                    uint32_t slot = 0xffffffffu;
                    for (unsigned k = 0; k < sizeof w / sizeof w[0]; k++) if (has(b, w[k].name)) { slot = w[k].slot; break; }
                    if (slot == 0xffffffffu) rr_fatal("wreck branch of no direction (the original reads a stale slot)", b);
                    rx_addr r = ra + 0x28 * slot;
                    SU16(r, 0xc5bc, (uint16_t)i);
                    SU32(r, 0xc5c0, rr_x87_copy(U32(b, 0x94)));
                    crt_strcpy_g(r + 0xc59c, aname(b));
                    SU32(ra, 0xc81c, U32(ra, 0xc81c) + 1);
                }
                break;
            }
        }
        return;
    }
    if (!crt_strstr(aname(sw), 0x009404d0)) return;   /* "Rider" */
    rx_addr p;
    if ((p = param(sw, 0x00958394))) SU32(ra, 0x580, pval(p));
    if ((p = param(sw, 0x009583b4))) SU32(ra, 0x57c, pval(p));
    if ((p = param(sw, 0x009583c0))) SU32(ra, 0x598, pval(p));
    for (int32_t i = 0; i < n; i++) {
        rx_addr b = U32(U32(sw, 0xc4), 4u * (uint32_t)i);
        uint32_t t = U32(b, 4);
        static const uint8_t kind[10] = {0, 1, 2, 4, 4, 4, 4, 4, 4, 3};
        if (t > 9) continue;
        switch (kind[t]) {
        case 3:   /* performance */
            if (U32(ra, 0x5bab8) == 1 && has(b, 0x009584dc)) {   /* ATV idle */
                if (has(b, 0x0093e6d0)) SU32(ra, 0x5b2f0, U32(ra, 0xc594));
                slot_record(ra, 0xc454, 0xc594, (uint32_t)i, b, 1, b);
            } else if (has(b, 0x009584e4)) {   /* "Hop" */
                if (has(b, 0x00940824)) {
                    SU16(ra, 0xc8ec, (uint16_t)i); SU32(ra, 0xc8f0, rr_x87_copy(U32(b, 0x94)));
                    crt_strcpy_g(ra + 0xc8cc, aname(b)); SU32(ra, 0xc924, U32(ra, 0xc924) + 1);
                } else if (has(b, 0x0094081c)) {
                    SU16(ra, 0xc914, (uint16_t)i); SU32(ra, 0xc918, rr_x87_copy(U32(b, 0x94)));
                    crt_strcpy_g(ra + 0xc8f4, aname(b)); SU32(ra, 0xc924, U32(ra, 0xc924) + 1);
                }
            } else if (has(b, 0x009584e8)) {   /* "Ground": map +0xaf9c and the ground slots +0xba28 */
                RX_FRAME_ENTER(F, 0x20);
                crt_str_ctor_cstr(F, b + 0x9c);
                SU16(rx_rider_strmap_index(ra + 0xaf9c, F, 2), 0, (uint16_t)i);
                uint32_t cnt = U32(ra, 0xc450);
                SU16(ra, 0xba48 + 0x28 * cnt, (uint16_t)i);
                crt_strcpy_g(ra + 0xba28 + 0x28 * cnt, aname(b));
                SU32(ra, 0xc450, cnt + 1);
                crt_str_dtor(F);
                RX_FRAME_LEAVE(F, 0x20);
            }
            break;
        case 1:   /* motion */
            if (has(b, 0x009408a0)) {   /* "Reverse" */
                if (has(b, 0x00940824)) SU16(ra, 0x5a2, (uint16_t)i);
                else if (has(b, 0x0094081c)) SU16(ra, 0x5a4, (uint16_t)i);
                else { SU16(ra, 0x5a2, (uint16_t)i); SU16(ra, 0x5a4, (uint16_t)i); }
            } else if (has(b, 0x009584f0)) SU16(ra, 0x5a8, (uint16_t)i);
            else if (has(b, 0x009584f8)) SU16(ra, 0x5aa, (uint16_t)i);
            break;
        case 2:   /* additive */
            if (has(b, 0x009584dc)) {
                if (has(b, 0x0093e6d0)) SU32(ra, 0x5b2f0, U32(ra, 0xc594));
                uint32_t cnt = U32(ra, 0xc594);
                rx_addr r = ra + 0xc454 + 0x28 * cnt;
                SU16(r, 0x20, (uint16_t)i);
                SU32(r, 0x24, rr_x87_copy(U32(U32(b, 0x2c), 0x94)));
                crt_strcpy_g(r, aname(b));
                SU32(ra, 0xc594, cnt + 1);
            } else if (has(b, 0x00958500)) SU16(ra, 0x5a6, (uint16_t)i);
            break;
        case 0:   /* blend */
            if (U32(ra, 0x5bab8) == 1 && has(b, 0x00958500)) SU16(ra, 0x5a6, (uint16_t)i);
            else SU16(ra, 0x5a0, (uint16_t)i);
            break;
        }
    }
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the IK animator's solvers (RiderPhysics "<physics>" document) */

/* 00460450 (ESI = name, EDI = vehicle): the vehicle's hand / foot target (+0x1c0 / +0x1d0 / +0x1e0 / +0x1f0 for NDE_LFT_Hand /
 * NDE_RHT_Hand / NDE_LFT_Foot / NDE_RHT_Foot), else 0 */
rx_addr rx_vehicle_ik_target(rx_addr veh, rx_addr name) {
    if (!crt_strcmp(name, 0x0093ba78)) return veh + 0x1c0;
    if (!crt_strcmp(name, 0x0093bab8)) return veh + 0x1d0;
    if (!crt_strcmp(name, 0x0093baf8)) return veh + 0x1e0;
    if (!crt_strcmp(name, 0x0093bb38)) return veh + 0x1f0;
    return 0;
}
static void solver_init(rx_addr s) {   /* 006afba0 (ECX) */
    SF32(s, 0x70, 0.25f);
    SF32(s, 0x74, 1.0f);
    SU32(s, 8, 1); SU32(s, 4, 0); SU32(s, 0xc, 3); SU32(s, 0x10, 0); SU32(s, 0x64, 0);
    SF32(s, 0x40, 0);
    SF32(s, 0x78, 0.5f);
    SF32(s, 0, 0);
    SU32(s, 0x68, 0); SU32(s, 0x6c, 0); SU32(s, 0x60, 0);
}
/* the active flag change: blend time 0.25, the blend restarted (0063 inline of 006afdc0 / 006b01b0) */
static void solver_set_active(rx_addr s, uint32_t active) {
    if (active != U32(s, 8)) {
        float t = 0.25f, b = F32(s, 0x40);
        SF32(s, 0x70, t);
        SF32(s, 0x40, !(b >= t) ? t - b : 0.0f);   /* comiss b, 0.25; jb (unordered too) */
    }
    SU32(s, 8, active);
}
/* 006afdc0 (ECX = solver, EDX = node; s0 reader, s1 default iterations, s2 hierarchy, s3 target; RET 0x10) */
void rx_ik_solver_load(rx_addr s, rx_addr node, rx_addr r, uint32_t def_iter, rx_addr hier, rx_addr target) {
    RX_FRAME_ENTER(F, 0xb0);
    uint32_t a[3];
    #define X(off, n0, n1, n2) (a[0] = (n0), a[1] = (n1), a[2] = (n2), rr_vcall(r, (off), 3, a))
    #define XC(name) (a[0] = node, a[1] = (name), rr_vcall(r, 0x1c, 2, a))
    rx_addr e = XC(0x0095eddc);
    X(0xd0, e, F + 0xc, 1);   /* NumberOfBones */
    int32_t nbones = (int32_t)U32(F, 0xc);
    if ((e = XC(0x0095edec))) { X(0xd0, e, F + 8, 1); SU32(s, 0xc, U32(F, 8)); } else SU32(s, 0xc, def_iter);
    uint32_t act = 1;
    if ((e = XC(0x0095edf8))) { X(0xe4, e, F + 8, 1); act = U32(F, 8); }
    solver_set_active(s, act);
    float tt = 0.25f;
    if ((e = XC(0x0095ee08))) { X(0xcc, e, F + 8, 1); tt = F32(F, 8); }
    SF32(s, 0x70, tt);
    if (1e-6f /* [0x9777c4] */ > tt) SF32(s, 0x70, 1e-7f /* [0x977324] */);
    float v = 1.0f;
    if ((e = XC(0x0095ee18))) { X(0xcc, e, F + 8, 1); v = F32(F, 8); }
    SF32(s, 0x74, v);
    v = 0.5f;
    if ((e = XC(0x00935e70))) { X(0xcc, e, F + 8, 1); v = F32(F, 8); }
    SF32(s, 0x78, v);
    v = 0.001f;   /* [0x97743c] */
    if ((e = XC(0x0095ee28))) { X(0xcc, e, F + 8, 1); v = F32(F, 8); }
    SF32(s, 0x50, v); SF32(s, 0x54, v); SF32(s, 0x58, v); SF32(s, 0x5c, v);
    e = XC(0x0095ee38);
    X(0xd8, e, F + 0x28, 0x40);   /* SolutionBoneName */
    rx_addr rec = rx_hierarchy_find_node(hier, F + 0x28);
    SU32(s, 0x64, rec ? (uint32_t)((int32_t)(rec - U32(hier, 8)) >> 4) : 0xffffffffu);
    SU32(s, 0x10, target);
    SF32(s, 0x30, 0); SF32(s, 0x34, 0); SF32(s, 0x38, 0);
    SU32(s, 0x68, rr_alloc(rx_mul_sat((uint32_t)nbones, 4)));
    SU32(s, 0x6c, rr_alloc(rx_mul_sat((uint32_t)nbones, 4)));
    SU32(s, 0x60, 0);
    for (int32_t k = 1; k <= nbones; k++) {
        char buf[0x40];
        snprintf(buf, sizeof buf, "Bone%d", k);   /* 0078fd60 "Bone%d" into the 0x40 frame buffer */
        for (uint32_t j = 0;; j++) { SU8(F + 0x28, j, (uint8_t)buf[j]); if (!buf[j]) break; }
        e = XC(F + 0x28);
        X(0xd8, e, F + 0x28, 0x40);
        rx_addr br = rx_hierarchy_find_node(hier, F + 0x28);
        snprintf(buf, sizeof buf, "Weight%d", k);
        for (uint32_t j = 0;; j++) { SU8(F + 0x68, j, (uint8_t)buf[j]); if (!buf[j]) break; }
        e = XC(F + 0x68);
        float w = 1.0f;
        if (e) {
            X(0xcc, e, F + 0x14, 1);
            float x = F32(F, 0x14);
            if (1.0f > x) w = (x > 0.0f) ? x : 0.0f;   /* clamp to [0, 1] (comiss 1, x; jbe / comiss x, 0; jbe) */
        }
        uint32_t idx = br ? (uint32_t)((int32_t)(br - U32(hier, 8)) >> 4) : 0xffffffffu;
        SU32(U32(s, 0x68), 4 * U32(s, 0x60), idx);
        SF32(U32(s, 0x6c), 4 * U32(s, 0x60), w);
        SU32(s, 0x60, U32(s, 0x60) + 1);
    }
    #undef X
    #undef XC
    RX_FRAME_LEAVE(F, 0xb0);
}
/* 006b01b0 (stdcall ik, reader; RET 8): the "InverseKinematics" root: NumberOfSolvers, DefaultIterations (3), ActiveAtLoad (1), the
 * solvers (0x80 each, a counted new[] of FUN_00401f90) and each from "Solver<k>" with its vehicle target */
void rx_ik_animator_load(rx_addr ik, rx_addr r) {
    SU32(ik, 0x60, 0);
    if (U32(ik, 0x64)) rr_fatal("IK animator reload (006b0420) not ported", ik);
    RX_FRAME_ENTER(F, 0xb0);
    uint32_t a[3];
    rx_addr root = rr_vcall(r, 4, 0, NULL);
    a[0] = root; a[1] = 0x0095ee60;
    rx_addr top = rr_vcall(r, 0x1c, 2, a);
    if (top) {
        a[0] = top; a[1] = 0x0095ee74;
        rx_addr e = rr_vcall(r, 0x1c, 2, a);
        uint32_t ns = 0;
        if (e) { a[0] = e; a[1] = F + 0x14; a[2] = 1; rr_vcall(r, 0xd0, 3, a); ns = U32(F, 0x14); }
        SU32(ik, 0x60, ns);
        a[0] = top; a[1] = 0x0095ee84;
        e = rr_vcall(r, 0x1c, 2, a);
        uint32_t di = 3;
        if (e) { a[0] = e; a[1] = F + 0x14; a[2] = 1; rr_vcall(r, 0xd0, 3, a); di = U32(F, 0x14); }
        SU32(ik, 0x68, di);
        a[0] = top; a[1] = 0x0095edf8;
        e = rr_vcall(r, 0x1c, 2, a);
        uint32_t act = 1;
        if (e) { a[0] = e; a[1] = F + 0x14; a[2] = 1; rr_vcall(r, 0xe4, 3, a); act = U32(F, 0x14); }
        SU32(ik, 0x70, act);
        uint32_t n = U32(ik, 0x60);
        uint32_t sz = rx_mul_sat(n, 0x80);
        sz = sz + 0x10 < sz ? 0xffffffffu : sz + 0x10;
        rx_addr blk = rx_reflex_aligned_alloc(sz);   /* FUN_00401f90 */
        rx_addr sv = 0;
        if (blk) { SU32(blk, 0, n); sv = blk + 0x10; rx_rider_vector_ctor(sv, 0x80, (int32_t)n, solver_init); }
        SU32(ik, 0x64, sv);
        rx_addr hier = U32(U32(ik, 0x54), 0x240);
        for (uint32_t k = 0; k < U32(ik, 0x60); k++) {
            char buf[0x40]; snprintf(buf, sizeof buf, "Solver%d", k + 1);
            for (uint32_t j = 0;; j++) { SU8(F + 0x28, j, (uint8_t)buf[j]); if (!buf[j]) break; }
            a[0] = top; a[1] = F + 0x28;
            rx_addr sn = rr_vcall(r, 0x1c, 2, a);
            a[0] = sn; a[1] = 0x0095eea4;
            rx_addr tb = rr_vcall(r, 0x1c, 2, a);
            a[0] = tb; a[1] = F + 0x68; a[2] = 0x40;
            rr_vcall(r, 0xd8, 3, a);
            rx_addr target = rx_vehicle_ik_target(U32(ik, 0x58), F + 0x68);
            rx_addr s = U32(ik, 0x64) + 0x80 * k;
            rx_ik_solver_load(s, sn, r, U32(ik, 0x68), hier, target);
            uint32_t on = (U32(s, 8) && U32(ik, 0x70)) ? 1 : 0;
            solver_set_active(s, on);
        }
    }
    RX_FRAME_LEAVE(F, 0xb0);
}
/* 006afd50 (ESI = ik animator, EAX = vehicle, ECX = character): the physics document (BXML "<ik + 0x74>") */
void rx_ik_animator_attach(rx_addr ik, rx_addr veh, rx_addr ch) {
    SU32(ik, 0x58, veh);
    SU32(ik, 0x54, ch);
    uint32_t a[4] = {U32(ik, 0x74), 0x6c6d7862, 0, 1};
    rx_addr h = rr_vcall(rr_database(), 0x6c, 4, a);
    SU32(h, 0x4c, 1);
    rx_ik_animator_load(ik, U32(h, 0x44) ? U32(h, 0x44) - 0x460 : 0);
    SU32(h, 0x4c, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the walker */

/* AttachPlugsTreeWalker vf0 0061ae00 (this = walker {vtable 0x00958930, +4 vehicle, +8 ra, +0xc / +0x10 / +0x14 / +0x18 the found
 * animators}; animator) */
uint32_t rx_attach_plugs_walker_enter(rx_addr w, rx_addr a) {
    uint32_t t = U32(a, 4);
    rx_addr ra = U32(w, 8);
    switch (t) {
    case 0xa:   /* procedural: the look / lean / limb controllers */
        SU32(a, 0x54, ra + 0x5b590);
        SU32(a, 0x58, ra + 0x5b6e4);
        SU32(a, 0x64, ra + 0x5b940);
        if (U32(U32(w, 4), 0x194) != 1) { SU32(a, 0x5c, ra + 0x5b760); SU32(a, 0x60, ra + 0x5b850); }
        break;
    case 7: SU32(w, 0xc, a); break;
    case 6: SU32(w, 0x10, a); break;
    case 5: SU32(w, 0x14, a); rx_ik_animator_attach(a, U32(w, 4), U32(ra, 0x144)); break;
    case 8:
        SU32(w, 0x18, a);
        if (U8(a, 0x5c)) rr_fatal("expression bone constraints (006acb30) not ported", a);
        break;
    case 3: rx_rider_bind_switch(a, ra); break;
    case 0:
        if (has(a, 0x009583e4)) bind_blend(a, ra, 0x308);
        else if (has(a, 0x009583e8)) bind_blend(a, ra, 0x334);
        else if (has(a, 0x009583ec) || has(a, 0x009583f0) || has(a, 0x009583f4)) bind_blend(a, ra, 0x360);
        break;
    case 9: bind_performance(a, ra); break;
    case 1: bind_motion(a, ra); break;
    default: break;
    }
    return 0;
}
RX_FUNCTION(0x0061ae00, rx_attach_plugs_walker_enter)

/* 0061ad70 (ECX = ra, EAX = vehicle): the walk over the tree; the found animators kept (+0x5b8 / +0x5bc / +0x5c4 / +0x5c0), the IK
 * animator's +0x5c byte cleared, its +0x28 / +0x2c = (the expression animator, ra + 0xaefc) */
void rx_rider_attach_plugs(rx_addr veh, rx_addr ra) {
    RX_FRAME_ENTER(F, 0x20);
    SU32(F, 4, veh);
    SU32(F, 0, 0x00958930);
    SU32(F, 8, ra);
    SU32(F, 0x14, 0); SU32(F, 0x10, 0); SU32(F, 0xc, 0); SU32(F, 0x18, 0);
    uint32_t wa = F;
    rr_vcall(U32(U32(ra, 0x144), 0x224), 0x28, 1, &wa);
    SU32(ra, 0x5b8, U32(F, 0x10));
    SU32(ra, 0x5bc, U32(F, 0x14));
    SU32(ra, 0x5c4, U32(F, 0x18));
    SU32(ra, 0x5c0, U32(F, 0xc));
    SU8(U32(F, 0x18), 0x5c, 0);
    rx_addr ik = U32(ra, 0x5bc);
    SU32(ik + 0x28, 4, ra + 0xaefc);
    SU32(ik + 0x28, 0, U32(F, 0xc));
    RX_FRAME_LEAVE(F, 0x20);
}

/*0061ab30: machine switch bindings. Unlike the MX/ATV walker these switches
 * bind both sets of transition parameters, with motion percentages driving
 * the rider's same listener vector at+38c. */
void rx_rider_machine_bind_switch(rx_addr ra,rx_addr sw){
    static const struct {uint32_t name,offset;} fields[]={
        {0x00958394,0x568},{0x009583a4,0x574},{0x009583b4,0x564},{0x009583c0,0x58c},
        {0x00958394,0x580},{0x009583b4,0x57c},{0x009583c0,0x598}};
    int32_t count=(int32_t)(U32(sw,0xc8)-U32(sw,0xc4))>>2;
    for(unsigned i=0;i<7;i++){rx_addr p=param(sw,fields[i].name);if(p)SU32(ra,fields[i].offset,pval(p));}
    for(int32_t i=0;i<count;i++){
        if((uint32_t)i>=((U32(sw,0xc8)-U32(sw,0xc4))>>2))rr_fatal("machine animator branch range",sw);
        rx_addr branch=U32(U32(sw,0xc4),4u*(uint32_t)i);
        if(U32(branch,4)!=1)continue;
        if(has(branch,0x009408a0))SU16(ra,0x5b6,(uint16_t)i);
        else{SU16(ra,0x5a0,(uint16_t)i);SU16(ra,0x5ac,(uint16_t)i);}
        rx_addr p=param(branch,0x009583d0);
        if(p)listen(ra+0x38c,pval(p));
    }
}
RX_FUNCTION(0x0061ab30,rx_rider_machine_bind_switch)

/*0061aaa0: AttachPlugsToAnimationTree_Machines walker, vtable958924.
 * Expression controllers are disabled in all four authored machine trees;
 * retain the existing explicit unsupported flag-on constraint branch. */
uint32_t rx_rider_machine_walker_enter(rx_addr w,rx_addr animator){
    rx_addr ra=U32(w,8);
    switch(U32(animator,4)){
    case 10:SU32(animator,0x54,ra+0x5b590);break;
    case 7:SU32(w,0xc,animator);break;
    case 6:SU32(w,0x10,animator);break;
    case 5:SU32(w,0x14,animator);rx_ik_animator_attach(animator,U32(w,4),U32(ra,0x144));break;
    case 8:SU32(w,0x18,animator);if(U8(animator,0x5c))rr_fatal("expression bone constraints (006acb30) not ported",animator);break;
    case 3:rx_rider_machine_bind_switch(ra,animator);break;
    default:break;
    }
    return 0;
}
RX_FUNCTION(0x0061aaa0,rx_rider_machine_walker_enter)

/*0061aa10: original SUB ESP,1c/PUSH EBX,ESI places the walker at entry-1c.
 * It retains the actual model's switch/IK/expression objects and connects
 * their vehicle targets; no proxy animation tree is introduced. */
void rx_rider_truck_setup(rx_addr veh,rx_addr ra){
    RX_FRAME_ENTER(F,0x24);rx_addr w=F+8;
    SU32(w,0,0x00958924);SU32(w,4,veh);SU32(w,8,ra);
    for(unsigned off=0xc;off<=0x18;off+=4)SU32(w,off,0);
    uint32_t arg=w;rr_vcall(U32(U32(ra,0x144),0x224),0x28,1,&arg);
    SU32(ra,0x5b8,U32(w,0x10));SU32(ra,0x5bc,U32(w,0x14));
    SU32(ra,0x5c4,U32(w,0x18));SU32(ra,0x5c0,U32(w,0xc));
    SU8(U32(w,0x18),0x5c,0);
    rx_addr ik=U32(ra,0x5bc);SU32(ik,0x2c,ra+0xaefc);SU32(ik,0x28,U32(w,0xc));
    RX_FRAME_LEAVE(F,0x24);
}
RX_FUNCTION(0x0061aa10,rx_rider_truck_setup)
