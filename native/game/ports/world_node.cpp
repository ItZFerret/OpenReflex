// World group, "node" share: the node hierarchy evaluator (0x007dxxxx).
#include "world_node_util.h"

using namespace world;
using namespace world_node;
using namespace reflex_port;

namespace {

// Node records (16 bytes, in the node set's +8 array): +0 node set, +0xb matrix index, +0xc flags (bit 0 world valid, bit 1
// visited), +0xd first child, +0xe next sibling, +0xf parent (0xff: none). Node set: +4 traversal cursor, +8 records, +0xc
// count, +0x18 local matrices, +0x1c world matrices (64 bytes each), +0x30, +0x34 dirty flag.
inline uint32_t rec_at(const Guest& g, uint32_t set, uint32_t idx) { return g.u32(set + 8) + idx * 16; }

// World = Local x World(parent) of record rec, through form a (first) or b.
inline MatOut eval_rec(Guest& g, uint32_t rec, bool form_b) {
    const uint32_t set = g.u32(rec);
    const uint32_t par = rec_at(g, set, g.u8(rec + 0xf));
    const uint32_t pw = g.u32(g.u32(par) + 0x1c) + uint32_t(g.u8(par + 0xb)) * 64;
    const uint32_t off = uint32_t(g.u8(rec + 0xb)) * 64;
    X L[4], R[4];
    ld4(g, pw, R);
    ld4(g, g.u32(set + 0x18) + off, L);
    MatOut o = form_b ? mat_mul_b(L, R) : mat_mul_a(L, R);
    const uint32_t w = g.u32(set + 0x1c) + off;
    for (int i = 0; i < 4; i++) st(g, w + 16 * i, o.x[1 + i]);
    return o;
}

// The inverse of a rigid world matrix P (rows): the transposed rotation (w lanes 0xd61660 lane 0) and the translation
// -T . row i summed in double precision ((y + x) + z), w lane 0xd61690 lane 3.
void rigid_inverse(const Guest& g, const X P[4], X inv[4]) {
    const uint32_t k0 = g.u32(0xd61660);
    const X nt = subps(X{}, P[3]);
    uint32_t d[3];
    for (int i = 0; i < 3; i++) {
        double s = dmul(f2d(P[i].u[1]), f2d(nt.u[1]));
        s = dadd(s, dmul(f2d(P[i].u[0]), f2d(nt.u[0])));
        s = dadd(s, dmul(f2d(P[i].u[2]), f2d(nt.u[2])));
        d[i] = d2f(s);
    }
    inv[0] = X{{P[0].u[0], P[1].u[0], P[2].u[0], k0}}; inv[1] = X{{P[0].u[1], P[1].u[1], P[2].u[1], k0}};
    inv[2] = X{{P[0].u[2], P[1].u[2], P[2].u[2], k0}}; inv[3] = X{{d[0], d[1], d[2], g.u32(0xd61690 + 12)}};
}

}  // namespace

// 007db890 Node_EvaluateWorld (thiscall rec, arg 0 = also re-evaluate the subtree below rec's next sibling; RET 4). When rec's
// world matrix is invalid: marks the set dirty (+0x34 = 1), collects rec and its invalid ancestors, evaluates them top-down
// (the root: world = local), setting bit 0. With arg 0 and a next sibling: clears bit 1 of every record, starts the set's
// cursor (+4) at the sibling and evaluates every record 007deaa0 (EDI = set) yields; then cursor and +0x30 = 0.
REFLEX_PORT_FN(0x007db890, Node_EvaluateWorld, "world", OUT_XMM_ALL) {
    Guest g{m};
    Frame f(m, 0x144, 3);
    const uint32_t rec = m.r[1], arg = m.arg(0);
    bool evaluated = false;
    MatOut last;
    bool have = false;
    if (!(g.u8(rec + 0xc) & 1)) {
        const uint32_t set = g.u32(rec);
        g.u32(set + 0x34, 1);
        evaluated = true;
        uint32_t chain[80];
        int n = 0;
        if (!(g.u8(rec + 0xc) & 1)) {
            chain[n++] = rec;
            uint32_t cur = rec;
            while (g.u8(cur + 0xf) != 0xff) {
                uint32_t p = rec_at(g, g.u32(cur), g.u8(cur + 0xf));
                if (g.u8(p + 0xc) & 1) break;
                chain[n++] = p;
                cur = p;
            }
        }
        int k = n - 1;
        uint32_t top = chain[k];
        if (g.u8(top + 0xf) != 0xff) {
            last = eval_rec(g, top, false); have = true;
        } else {
            const uint32_t s = g.u32(top), off = uint32_t(g.u8(top + 0xb)) * 64;
            const uint32_t l = g.u32(s + 0x18) + off, w = g.u32(s + 0x1c) + off;
            X v;
            for (int i = 0; i < 4; i++) { v = ld(g, l + 16 * i); st(g, w + 16 * i, v); }
            xw(g, 0, v);
        }
        g.u8(top + 0xc, g.u8(top + 0xc) | 1);
        for (k--; k >= 0; k--) {
            last = eval_rec(g, chain[k], true); have = true;
            g.u8(chain[k] + 0xc, g.u8(chain[k] + 0xc) | 1);
        }
        if (have) put_xmm(g, last);
    }
    if (arg != 0) {
        const uint8_t sib = g.u8(rec + 0xe);
        if (sib != 0xff) {
            (void)evaluated;   // only selects the prefetch loop
            const uint32_t set = g.u32(rec);
            const uint32_t start = rec_at(g, set, sib);
            for (int32_t i = 0; i < g.i32(set + 0xc); i++) {
                const uint32_t a = g.u32(set + 8) + uint32_t(i) * 16 + 0xc;
                g.u8(a, g.u8(a) & 0xfd);
            }
            g.u32(set + 4, start);
            for (;;) {
                m.r[7] = g.u32(rec);
                const uint32_t r = f.call(0x007deaa0);
                if (!r) break;
                put_xmm(g, eval_rec(g, r, true));
                g.u8(r + 0xc, g.u8(r + 0xc) | 1);
            }
            const uint32_t s = g.u32(rec);
            g.u32(s + 4, 0); g.u32(s + 0x30, 0);
            m.r[0] = 0;
        }
    }
    f.restore_saved();
    return m.ret(4);
}

namespace {
// 007db7e0 with its register results (EAX / ECX / EDX as the original leaves them; ECX = record at entry).
struct R3 { uint32_t eax, ecx, edx; };
inline uint32_t lo8(uint32_t r, uint8_t b) { return (r & ~0xffu) | b; }
// sp = ESP at entry (the return address); cbx / csi / cdi = the caller's EBX / ESI / EDI, which the original pushes: the
// dead stack is left as the original leaves it (later code reads uninitialized stack there).
void node_invalidate(Guest& g, R3& r, uint32_t sp, uint32_t cbx, uint32_t csi, uint32_t cdi) {
    const uint32_t ebx = r.ecx;
    g.u32(sp - 4, cbx);
    uint8_t al = g.u8(ebx + 0xc);
    r.eax = lo8(r.eax, al);
    if (!(al & 1)) return;
    al &= 0xfe; r.eax = lo8(r.eax, al); g.u8(ebx + 0xc, al);
    r.eax = g.u32(ebx); g.u32(r.eax + 0x34, 1);
    r.eax = ebx + 0xe;
    g.u32(sp - 8, csi);
    uint32_t esi = ebx;
    if (g.u8(ebx + 0xe) != 0xff) {
        g.u32(sp - 0xc, cdi);
        for (;;) {
            r.ecx = g.u8(r.eax); r.edx = g.u32(esi); r.ecx = (r.ecx << 4) + g.u32(r.edx + 8);
            esi = r.ecx;
            al = g.u8(esi + 0xd); r.eax = lo8(r.eax, al);
            uint32_t edi = esi;
            while (al != 0xff) {
                r.ecx = g.u32(edi); r.eax = (uint32_t(al) << 4) + g.u32(r.ecx + 8);
                edi = r.eax; r.ecx = edi;
                g.u32(sp - 0x10, 0x007db83a);   // the recursive call's return address
                node_invalidate(g, r, sp - 0x10, ebx, esi, edi);
                al = g.u8(edi + 0xd); r.eax = lo8(r.eax, al);
            }
            al = g.u8(esi + 0xc); r.eax = lo8(r.eax, al);
            if (!(al & 1)) break;
            al &= 0xfe; r.eax = lo8(r.eax, al); g.u8(esi + 0xc, al);
            r.eax = esi + 0xe;
            if (g.u8(esi + 0xe) == 0xff) break;
        }
    }
    r.edx = lo8(r.edx, 0xfb);
    g.u8(ebx + 0xc, g.u8(ebx + 0xc) & 0xfb);
    uint8_t cl = g.u8(ebx + 0xf); r.ecx = lo8(r.ecx, cl);
    r.eax = ebx;
    while (cl != 0xff) {
        r.eax = g.u32(r.eax);
        r.ecx = (uint32_t(cl) << 4) + g.u32(r.eax + 8);
        r.eax = r.ecx;
        g.u8(r.eax + 0xc, g.u8(r.eax + 0xc) & 0xfb);
        cl = g.u8(r.eax + 0xf); r.ecx = lo8(r.ecx, cl);
    }
}
}  // namespace

// 007db7e0 Node_Invalidate (ECX = record): when its world matrix is valid, clears bit 0 of it, marks the set dirty (+0x34 = 1)
// and invalidates the subtree (+0xe first child, +0xd next sibling; siblings recursively, the child chain iteratively while
// valid); then clears bit 2 of the record and of every ancestor (+0xf).
REFLEX_PORT_FN(0x007db7e0, Node_Invalidate, "world", OUT_EAX | OUT_ECX | OUT_EDX) {
    Guest g{m};
    R3 r{m.r[0], m.r[1], m.r[2]};
    node_invalidate(g, r, m.r[4], m.r[3], m.r[6], m.r[7]);
    m.r[0] = r.eax; m.r[1] = r.ecx; m.r[2] = r.edx;
    return m.ret(0);
}

// 007dc890 Node_SetWorld (EDI = record, ESI = 16-byte aligned world matrix; cdecl, no arguments): sets the local matrix so the
// record's world matrix becomes [ESI]: without parent local = W; else (parent evaluated with Node_EvaluateWorld(parent, 0))
// local = W x inverse(parent world), the inverse rigid: transposed rotation (w lanes from 0xd61660 lane 0) and translation
// -T . row i in double precision ((y + x) + z) (w lane from 0xd61690 lane 3). Then Node_Invalidate(record).
REFLEX_PORT_FN(0x007dc890, Node_SetWorld, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    Frame f(m, 0xb0, 0);
    const uint32_t rec = m.r[7], src = m.r[6];
    X W[4];
    ld4(g, src, W);
    if (rec == 0) {   // unreachable in practice (reads record 0)
        const uint32_t off = uint32_t(g.u8(0xb)) << 6, set = g.u32(0);
        const uint32_t a = off + g.u32(set + 0x18);
        X R[4]; ld4(g, a, R);
        MatOut o = mat_mul_b(W, R);
        for (int i = 0; i < 4; i++) st(g, a + 16 * i, o.x[1 + i]);
        put_xmm(g, o);
        m.r[0] = a; m.r[2] = g.u32(set + 0x18);
        f.call(0x007db7e0, {}, rec);
        return m.ret(0);
    }
    const uint8_t par = g.u8(rec + 0xf);
    if (par == 0xff) {
        const uint32_t a = (uint32_t(g.u8(rec + 0xb)) << 6) + g.u32(g.u32(rec) + 0x18);
        for (int i = 0; i < 4; i++) st(g, a + 16 * i, ld(g, src + 16 * i));
        xw(g, 0, ld(g, src + 0x30));
        m.r[0] = a;
        f.call(0x007db7e0, {}, rec);
        return m.ret(0);
    }
    f.call(0x007db890, {0}, rec_at(g, g.u32(rec), par));
    const uint32_t p = rec_at(g, g.u32(rec), g.u8(rec + 0xf));
    const uint32_t pw = (uint32_t(g.u8(p + 0xb)) << 6) + g.u32(g.u32(p) + 0x1c);
    X P[4]; ld4(g, pw, P);
    X inv[4];
    rigid_inverse(g, P, inv);
    const uint32_t d[3] = {inv[3].u[0], inv[3].u[1], inv[3].u[2]};
    // the original re-reads W after the parent evaluation (ESI rows): same addresses, loaded again
    ld4(g, src, W);
    MatOut o = mat_mul_a(W, inv);
    const uint32_t a = (uint32_t(g.u8(rec + 0xb)) << 6) + g.u32(g.u32(rec) + 0x18);
    for (int i = 0; i < 4; i++) st(g, a + 16 * i, o.x[1 + i]);
    put_xmm(g, o);
    m.r[0] = a; m.r[2] = d[2];
    f.call(0x007db7e0, {}, rec);
    return m.ret(0);
}

// 007dcfb0 Node_SetWorldRotation (thiscall record, arg 0 = quaternion x y z w; RET 4): the rotation matrix of the quaternion
// (2 = [0x92e330], 1 = [0x931f24]; row 3 = ([0xa95d50], [0xa95d54], [0xa95d58], 1)) set as the world matrix with Node_SetWorld
// (ESI = the stack matrix), then the local translation row put back as it was.
REFLEX_PORT_FN(0x007dcfb0, Node_SetWorldRotation, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    using x86::mul; using x86::add; using x86::sub;
    Guest g{m};
    Frame f(m, 0x68, 2);
    const uint32_t rec = m.r[1], q = m.arg(0);
    const float x = g.f32(q), y = g.f32(q + 4), z = g.f32(q + 8), w = g.f32(q + 12), c = g.f32(0x92e330), one = g.f32(0x931f24);
    const float z2 = mul(z, c), x2 = mul(x, c), y2 = mul(y, c);
    const float zz = mul(z, z2), wx = mul(w, x2), wz = mul(w, z2), wy = mul(w, y2), xx = mul(x, x2), yy = mul(y, y2);
    const float xy = mul(y, x2), yz = mul(z, y2), xz = mul(z, x2);
    const uint32_t t0 = g.u32(0xa95d50), t1 = g.u32(0xa95d54), t2 = g.u32(0xa95d58);
    const float row[16] = {sub(one, add(zz, yy)), add(xy, wz), sub(xz, wy), 0.0f,
                           sub(xy, wz), sub(one, add(zz, xx)), add(yz, wx), 0.0f,
                           add(xz, wy), sub(yz, wx), sub(one, add(yy, xx)), 0.0f,
                           x86::flt(t0), x86::flt(t1), x86::flt(t2), one};
    const uint32_t buf = f.esp + 0x20;
    for (int i = 0; i < 16; i++) g.f32(buf + 4 * i, row[i]);
    const uint32_t off = uint32_t(g.u8(rec + 0xb)) << 6, set = g.u32(rec), loc = g.u32(set + 0x18);
    const X keep = ld(g, off + loc + 0x30);
    xw(g, 0, keep); xw(g, 1, X{{t2, 0, 0, 0}}); xw(g, 2, X::fl(add(yy, xx), 0, 0, 0)); xw(g, 3, X::fl(wx, 0, 0, 0));
    xw(g, 4, X::fl(add(xz, wy), 0, 0, 0)); xw(g, 5, X{}); xw(g, 6, X::fl(add(yz, wx), 0, 0, 0)); xw(g, 7, X::fl(xx, 0, 0, 0));
    m.r[0] = off; m.r[1] = set; m.r[2] = loc; m.r[6] = buf; m.r[7] = rec;
    f.call(0x007dc890);
    const uint32_t off2 = uint32_t(g.u8(rec + 0xb)) << 6, set2 = g.u32(rec), loc2 = g.u32(set2 + 0x18);
    st(g, off2 + loc2 + 0x30, keep);
    xw(g, 0, keep);
    m.r[0] = off2; m.r[1] = set2; m.r[2] = loc2;
    f.restore_saved();
    return m.ret(4);
}

// 007dc560 Node_WorldToLocalPoint (ESI = record, EDI = 16-byte output, arg 0 = point; RET 4; returns EDI): evaluates the
// record's world matrix (Node_EvaluateWorld(rec, 0)) and transforms the point by its rigid inverse:
// ((p.y col y + p.x col x) + p.z col z) + T'.
REFLEX_PORT_FN(0x007dc560, Node_WorldToLocalPoint, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    Frame f(m, 0xb0, 0);
    const uint32_t rec = m.r[6], out = m.r[7], pp = m.arg(0);
    f.call(0x007db890, {0}, rec);
    const uint32_t pw = (uint32_t(g.u8(rec + 0xb)) << 6) + g.u32(g.u32(rec) + 0x1c);
    X P[4]; ld4(g, pw, P);
    X inv[4];
    rigid_inverse(g, P, inv);
    const X p = ld(g, pp);
    X r = addps(mulps(splat(p, 1), inv[1]), mulps(splat(p, 0), inv[0]));
    r = addps(mulps(splat(p, 2), inv[2]), r);
    r = addps(r, inv[3]);
    st(g, out, r);
    const X nt = subps(X{}, P[3]);
    xw(g, 0, r); xw(g, 1, inv[3]); xw(g, 2, inv[2]);
    const uint64_t d3 = dbits(f2d(nt.u[2]));
    xw(g, 3, X{{uint32_t(d3), uint32_t(d3 >> 32), 0, 0}});
    xw(g, 4, P[0]); xw(g, 5, P[1]); xw(g, 6, P[2]);
    xw(g, 7, X{{P[0].u[0], P[1].u[0], P[2].u[0], P[3].u[0]}});
    m.r[0] = out; m.r[1] = inv[3].u[2]; m.r[2] = pp;
    return m.ret(4);
}

// 007f8e00 PackedGrid_SampleUV: replaced 2026-10-18 by the lifted port in ports/world_lift.cpp (this hand port matched every
// verified call but left the dead stack different; a later particle update reads it uninitialized: notes/world.md).

// 007dd1b0 Node_SetWorldPosition (stdcall record, vector*; RET 8): the local translation (row 3 x y z) set so the record's
// world position becomes the vector: through the parent's Node_WorldToLocalPoint, or directly without parent. Then
// Node_Invalidate(record). (Record 0: adds the vector to row 3 of record 0's local matrix; unreachable.)
REFLEX_PORT_FN(0x007dd1b0, Node_SetWorldPosition, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    Frame f(m, 0x34, 3);
    const uint32_t rec = m.arg(0), vp = m.arg(1);
    const X v = ld(g, vp);
    xw(g, 1, v);
    const uint32_t buf = f.esp + 0x10;
    st(g, buf, v);
    uint32_t a;
    if (rec == 0) {
        const uint32_t set = g.u32(0);
        a = (uint32_t(g.u8(0xb)) << 6) + g.u32(set + 0x18);
        const uint32_t w = g.u32(a + 0x3c);
        st(g, a + 0x30, addps(ld(g, a + 0x30), v));
        g.u32(a + 0x3c, w);
        xw(g, 0, X{{w, 0, 0, 0}});
        xw(g, 2, addps(ld(g, a + 0x30), v));   // not reached in practice
    } else {
        const uint8_t par = g.u8(rec + 0xf);
        if (par != 0xff) {
            m.r[6] = rec_at(g, g.u32(rec), par); m.r[7] = f.esp + 0x20;
            const uint32_t r = f.call(0x007dc560, {buf});
            st(g, buf, ld(g, r));
            a = (uint32_t(g.u8(rec + 0xb)) << 6) + g.u32(g.u32(rec) + 0x18);
            m.r[2] = buf;
        } else {
            m.r[2] = g.u32(rec);
            a = (uint32_t(g.u8(rec + 0xb)) << 6) + g.u32(m.r[2] + 0x18);
        }
        for (int i = 0; i < 3; i++) g.u32(a + 0x30 + 4 * i, g.u32(buf + 4 * i));
        xw(g, 0, X{{g.u32(buf + 8), 0, 0, 0}});
    }
    m.r[0] = a;
    f.call(0x007db7e0, {}, rec);
    m.r[1] = g.u32(0x9b45e4);   // ECX after __security_check_cookie
    f.restore_saved();
    return m.ret(8);
}

// 007dd3a0 Node_GetWorldPosition (ESI = record, EDI = 16-byte output): row 3 of the evaluated world matrix.
REFLEX_PORT_FN(0x007dd3a0, Node_GetWorldPosition, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    Frame f(m, 0x10, 0);
    const uint32_t rec = m.r[6], out = m.r[7];
    uint32_t base, idx;
    if (rec == 0) { base = g.u32(g.u32(0) + 0x18); idx = g.u8(0xb); }   // unreachable
    else { f.call(0x007db890, {0}, rec); base = g.u32(g.u32(rec) + 0x1c); idx = g.u8(rec + 0xb); }
    const X v = ld(g, (idx << 6) + base + 0x30);
    st(g, out, v);
    xw(g, 0, v);
    m.r[0] = idx << 6; m.r[1] = g.u32(0x9b45e4); m.r[2] = base;
    return m.ret(0);
}

// 007dd320 Node_GetWorldMatrix (EDI = record, ESI = 64-byte output, unaligned): the evaluated world matrix.
REFLEX_PORT_FN(0x007dd320, Node_GetWorldMatrix, "world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM_ALL) {
    Guest g{m};
    Frame f = Frame::flat(m, 0, 0);
    const uint32_t rec = m.r[7], out = m.r[6];
    f.call(0x007db890, {0}, rec);
    const uint32_t set = g.u32(rec), a = (uint32_t(g.u8(rec + 0xb)) << 6) + g.u32(set + 0x1c);
    X v;
    for (int i = 0; i < 4; i++) { v = ld(g, a + 16 * i); st(g, out + 16 * i, v); }
    xw(g, 0, v);
    m.r[0] = a; m.r[1] = set;
    return m.ret(0);
}
