// RavenPhysics mechanism (mechanism group): preparation of the articulated-body factorization (0085cb70) and the per-type matrix
// dispatchers it uses. Node (constraint) objects: +10 child count, +14 child node indices, +30 / +34 linear / angular body,
// +38 .. +60 matrix objects (+44 the node's 6x6 / 4x4 / ... articulated inertia, +48 its inverse, +4c / +50 / +54 / +58 / +5c /
// +60 the coupling matrices); virtual slots 8 / 9 return the child's contribution to its parent.
#include "mechanism_kernels.h"

using namespace mech;
using namespace x86;
using reflex_port::OUT_ECX;

namespace {
constexpr uint32_t kNodes = 0xca0;

uint32_t node_at(Guest& g, uint32_t M, int32_t i) { return g.u32(M + kNodes + 4 * i); }

// Inverse of a square matrix object by its row count (3, 4, 5 or 6): out = inverse(mat). Shared by 0085cb70's three sites.
void invert_by_rows(Guest& g, uint32_t mat, uint32_t out) {
    switch (g.u32(mat + 4)) {
        case 3: callr(g, 0x00859250, {mat}, In().ECX(out)); break;
        case 4: callr(g, 0x00859340, {}, In().ESI(out).EDI(mat)); break;
        case 5: callr(g, 0x00859520, {out, mat}, In()); break;
        case 6: callr(g, 0x008597e0, {out}, In().ECX(mat)); break;
        default: break;
    }
}

// The diagonal of a node's articulated inertia from its bodies: rows +20/+30/+40 = mass * (00d615c0 / 00d616a0 / 00d61680),
// rows +e0/+f0/+100 = inertia * (same globals), after clearing the off-diagonal blocks +50 / +90 (slot 1).
void fill_inertia(Guest& g, uint32_t node) {
    uint32_t a = g.u32(node + 0x30), K = g.u32(node + 0x44);
    V4 mass = ld(g, a + 0x20);
    uint32_t b = g.u32(node + 0x34);
    V4 inertia = ld(g, b + 0x20);
    vcall(g, K + 0x50, 1);
    vcall(g, K + 0x90, 1);
    st(g, K + 0x20, mul(mass, ld(g, 0x00d615c0)));
    st(g, K + 0x30, mul(mass, ld(g, 0x00d616a0)));
    st(g, K + 0x40, mul(mass, ld(g, 0x00d61680)));
    st(g, K + 0xe0, mul(inertia, ld(g, 0x00d615c0)));
    st(g, K + 0xf0, mul(inertia, ld(g, 0x00d616a0)));
    st(g, K + 0x100, mul(inertia, ld(g, 0x00d61680)));
}

// K(node) -= child.slot8() for every child (stored order; count re-read).
void subtract_children(Guest& g, uint32_t M, uint32_t node) {
    for (int32_t k = 0; k < g.i32(node + 0x10); k++) {
        uint32_t child = node_at(g, M, g.i32(node + 0x14 + 4 * k));
        uint32_t K = g.u32(node + 0x44), vt = g.u32(K);
        uint32_t c = vcall(g, child, 8);
        g.call(g.u32(vt + 0x10), {K, c}, K);
    }
}
}

// 0085cb70 (arg = M): preparation. Descending non-root nodes: inertia diagonal, then slot 2 of +3c into +4c, clear +54, copy
// +38 into +5c, slot 2 of +5c into +60; the root's inertia diagonal. Then descending non-root nodes: subtract the children's
// contributions, invert K into +48, +50 = transpose(+4c), 00855740 (+48, +4c, +50), +54 -= node.slot9(), invert +54 into +58,
// 00855740 (+58, +5c, +60); finally the root: children, invert K into +48.
REFLEX_PORT_FN(0x0085cb70, Mechanism_Prepare, "mechanism", 0) {
    Guest g{m}; uint32_t M = m.arg(0);
    for (int32_t i = g.i32(M + 0x1c) - 1; i > 0; i--) {
        uint32_t node = node_at(g, M, i);
        fill_inertia(g, node);
        vcall(g, g.u32(node + 0x3c), 2, {g.u32(node + 0x4c)});
        vcall(g, g.u32(node + 0x54), 1);
        vcall(g, g.u32(node + 0x5c), 3, {g.u32(node + 0x38)});
        vcall(g, g.u32(node + 0x5c), 2, {g.u32(node + 0x60)});
    }
    fill_inertia(g, node_at(g, M, 0));
    for (int32_t i = g.i32(M + 0x1c) - 1; i > 0; i--) {
        uint32_t node = node_at(g, M, i);
        subtract_children(g, M, node);
        invert_by_rows(g, g.u32(node + 0x44), g.u32(node + 0x48));
        vcall(g, g.u32(node + 0x4c), 2, {g.u32(node + 0x50)});
        callr(g, 0x00855740, {}, In().EAX(g.u32(node + 0x48)).ECX(g.u32(node + 0x4c)).EDI(g.u32(node + 0x50)));
        uint32_t S = g.u32(node + 0x54), vt = g.u32(S);
        uint32_t c = vcall(g, node, 9);
        g.call(g.u32(vt + 0x10), {S, c}, S);
        invert_by_rows(g, g.u32(node + 0x54), g.u32(node + 0x58));
        callr(g, 0x00855740, {}, In().EAX(g.u32(node + 0x58)).ECX(g.u32(node + 0x5c)).EDI(g.u32(node + 0x60)));
    }
    uint32_t root = node_at(g, M, 0);
    subtract_children(g, M, root);
    invert_by_rows(g, g.u32(root + 0x44), g.u32(root + 0x48));
    return m.ret(4);
}

// ---- the solve (0085cf80) and its matrix-vector dispatchers

// 00857b30 (ECX = A: 6x6 of 3x3 blocks at +10/+50/+90/+d0, arg = x: Vec6, ESI = Y: Vec6): Y -= A x, through four 3x3 products
// (00854490) on stack locals: Y0 -= A00 x0 + A01 x1, Y1 -= A10 x0 + A11 x1.
REFLEX_PORT_FN(0x00857b30, Mat6_MulVecSub, "mechanism", 0) {
    Guest g{m}; uint32_t A = m.r[1], x = m.arg(0), Y = m.r[6];
    {
        Frame f(m, 0x40);
        uint32_t t1 = f.at(0x30), t2 = f.at(0x10), t3 = f.at(0x20), t4 = f.at(0x00);
        Mat3_MulVec(g, t1, x + 0x10, A + 0x10);
        Mat3_MulVec(g, t2, x + 0x20, A + 0x50);
        Mat3_MulVec(g, t3, x + 0x10, A + 0x90);
        Mat3_MulVec(g, t4, x + 0x20, A + 0xd0);
        V4 a = ld(g, t2), b = ld(g, t1);
        V4 s = add(b, a), y = ld(g, Y + 0x10);
        st(g, Y + 0x10, sub(y, s));
        a = ld(g, t3); b = ld(g, t4);
        s = add(a, b); y = ld(g, Y + 0x20);
        st(g, Y + 0x20, sub(y, s));
    }
    return m.ret(0);
}

// 00857590 (ECX = A, EDX = x, arg = Y): Y -= A x, by A's shape (rows x 6 for rows 3..5, 6 x cols for rows 6). ECX kept.
REFLEX_PORT_FN(0x00857590, Mat_MulVecSub, "mechanism", OUT_ECX) {
    Guest g{m}; uint32_t A = m.r[1], x = m.r[2], Y = m.arg(0);
    uint32_t rows = g.u32(A + 4), cols = g.u32(A + 8);
    switch (rows) {
        case 3: if (cols == 6) callr(g, 0x00857650, {A}, In().ECX(Y).EDX(x)); break;
        case 4: if (cols == 6) callr(g, 0x00857750, {A}, In().ECX(Y).EDX(x)); break;
        case 5: if (cols == 6) callr(g, 0x00857b30, {x}, In().ECX(A).ESI(Y)); break;
        case 6:
            switch (cols) {
                case 3: callr(g, 0x008578f0, {A}, In().ECX(Y).EDX(x)); break;
                case 4: callr(g, 0x00857a00, {A}, In().ECX(Y).EDX(x)); break;
                case 5: case 6: callr(g, 0x00857b30, {x}, In().ECX(A).ESI(Y)); break;
                default: break;
            }
            break;
        default: break;
    }
    m.r[1] = A; return m.ret(0);
}

// 00853900 (EAX = A, EDX = x, ESI = y): y = A x by A's shape (rows 3..6 x cols 3 / 4 / 5 / 6). ECX kept.
REFLEX_PORT_FN(0x00853900, Mat_MulVec, "mechanism", OUT_ECX) {
    Guest g{m}; uint32_t A = m.r[0], x = m.r[2], y = m.r[6], ecx = m.r[1];
    uint32_t rows = g.u32(A + 4), cols = g.u32(A + 8);
    auto leaf = [&](uint32_t f) { callr(g, f, {A}, In().ECX(y).EDX(x)); };
    switch (rows) {
        case 3:
            if (cols == 3) leaf(0x00854520); else if (cols == 4) leaf(0x008545b0);
            else if (cols == 6) callr(g, 0x00853d80, {A}, In().ECX(x).EDX(y));
            break;
        case 4:
            if (cols == 3) leaf(0x00853ca0); else if (cols == 4) leaf(0x00853bd0);
            else if (cols == 6) callr(g, 0x00853a30, {A}, In().ECX(x).EDX(y));
            break;
        case 5: if (cols == 5 || cols == 6) leaf(0x008542c0); break;
        case 6:
            if (cols == 3) leaf(0x00853fa0); else if (cols == 4) leaf(0x00853e80); else if (cols == 5 || cols == 6) leaf(0x008540a0);
            break;
        default: break;
    }
    m.r[1] = ecx; return m.ret(0);
}

// 0085cf80 (arg = M): the solve. Backward (descending non-root nodes, then node min(count-1, 0)): bias b (+64) cleared (slot 0),
// b -= child.C (+60) * child.z (+68) for every child (00857590); z (+68) = node +40 (slot 1), z -= +50 * b. Root: b = K^-1 b
// (+48, 00853900). Forward (ascending non-root): z = +58 z, z -= +5c * parent.b, b = +48 b, b -= +4c z, node slot 5.
REFLEX_PORT_FN(0x0085cf80, Mechanism_Solve, "mechanism", 0) {
    Guest g{m}; uint32_t M = m.arg(0);
    auto children = [&](uint32_t node) {
        for (int32_t k = 0; k < g.i32(node + 0x10); k++) {
            uint32_t child = node_at(g, M, g.i32(node + 0x14 + 4 * k));
            callr(g, 0x00857590, {g.u32(node + 0x64)}, In().ECX(g.u32(child + 0x60)).EDX(g.u32(child + 0x68)));
        }
    };
    int32_t i = g.i32(M + 0x1c) - 1;
    for (; i > 0; i--) {
        uint32_t node = node_at(g, M, i);
        vcall(g, g.u32(node + 0x64), 0);
        children(node);
        vcall(g, g.u32(node + 0x68), 1, {g.u32(node + 0x40)});
        callr(g, 0x00857590, {g.u32(node + 0x68)}, In().ECX(g.u32(node + 0x50)).EDX(g.u32(node + 0x64)));
    }
    uint32_t last = node_at(g, M, i);
    vcall(g, g.u32(last + 0x64), 0);
    children(last);
    uint32_t root = node_at(g, M, 0);
    callr(g, 0x00853900, {}, In().EAX(g.u32(root + 0x48)).EDX(g.u32(root + 0x64)).ESI(g.u32(root + 0x64)));
    for (int32_t k = 1; k < g.i32(M + 0x1c); k++) {
        uint32_t node = node_at(g, M, k);
        uint32_t parent = node_at(g, M, g.i32(node + 0xc));
        callr(g, 0x00853900, {}, In().EAX(g.u32(node + 0x58)).EDX(g.u32(node + 0x68)).ESI(g.u32(node + 0x68)));
        callr(g, 0x00857590, {g.u32(node + 0x68)}, In().ECX(g.u32(node + 0x5c)).EDX(g.u32(parent + 0x64)));
        callr(g, 0x00853900, {}, In().EAX(g.u32(node + 0x48)).EDX(g.u32(node + 0x64)).ESI(g.u32(node + 0x64)));
        callr(g, 0x00857590, {g.u32(node + 0x64)}, In().ECX(g.u32(node + 0x4c)).EDX(g.u32(node + 0x68)));
        vcall(g, node, 5);
    }
    return m.ret(4);
}

// 00853740 (ECX = B: 3x3 matrix object, EDI = A, ESI = out): out = A * B, through B's transpose (virtual slot 2) into a stack
// Matrix3x3: out row j = (dot3(Aj, T0), dot3(Aj, T1), dot3(Aj, T2), W = lane 2), stored after all rows are computed.
REFLEX_PORT_FN(0x00853740, Mat3_Mul, "mechanism", 0) {
    Guest g{m}; uint32_t B = m.r[1], A = m.r[7], out = m.r[6];
    {
        Frame f(m, 0x40);
        uint32_t T = f.at(0);
        g.u32(T + 4, 3); g.u32(T + 8, 3); g.u32(T, 0x0093ca04);
        vcall(g, B, 2, {T});
        V4 T0 = ld(g, T + 0x10), T1 = ld(g, T + 0x20), T2 = ld(g, T + 0x30);
        float d[3][3];
        for (int j = 0; j < 3; j++) {
            V4 a = ld(g, A + 0x10 + 0x10 * j);
            d[j][0] = dot3(a, T0); d[j][1] = dot3(a, T1); d[j][2] = dot3(a, T2);
        }
        for (int j = 0; j < 3; j++) st(g, out + 0x10 + 0x10 * j, V4{{d[j][0], d[j][1], d[j][2], d[j][2]}});
    }
    return m.ret(0);
}

// 00858fe0 (ECX = A, EDX = B, arg0 = C, arg1 = D): C = B * D^T * B^T-style congruence products by C's and A's shapes (the
// constraint's contribution to its parent, slots 8 / 9), through a block-matrix local at the original's stack address (built by
// its constructor) and the shape-specific products. Only the shapes the bike uses (rows 4 / 5 / 6) run in the worlds.
REFLEX_PORT_FN(0x00858fe0, Mat_Congruence, "mechanism", 0) {
    Guest g{m};
    const uint32_t ESP0 = m.r[4], Arows = g.u32(m.r[1] + 4), B = m.r[2], C = m.arg(0), D = m.arg(1);
    const uint32_t Af = (ESP0 - 4) & ~15u, E = Af - 0x170, L = E + 0x50;   // esp after the prologue, the local at [esp+50]
    auto at = [&](uint32_t esp) { m.r[4] = esp; };
    switch (g.u32(C + 4)) {
        case 3:
            if (Arows == 3) {
                uint32_t T = E + 0x10;
                g.u32(T + 4, 3); g.u32(T + 8, 3); g.u32(T, 0x0093ca04);
                at(E); callr(g, 0x00854650, {D}, In().ECX(T).EDX(B));
                at(E); callr(g, 0x00854650, {B}, In().ECX(C).EDX(T));
            } else if (Arows == 6) {
                at(E); callr(g, 0x00853550, {}, In().EAX(L));
                at(E); callr(g, 0x008547d0, {B}, In().ECX(L).EDX(D));
                at(E); callr(g, 0x008550f0, {B}, In().EDI(L).ESI(C));
            }
            break;
        case 4:
            at(E); callr(g, 0x0042f230, {}, In().EAX(L));
            at(E); callr(g, 0x00854910, {B}, In().ECX(L).EDX(D));
            at(E); callr(g, 0x00855190, {B}, In().EDI(L).ESI(C));
            break;
        case 5:
            at(E); callr(g, 0x008535b0, {}, In().ESI(L));
            at(E); callr(g, 0x00854f00, {D}, In().ECX(B).ESI(L));
            at(E); callr(g, 0x00854f00, {B}, In().ECX(L).ESI(C));
            break;
        case 6:
            switch (Arows) {
                case 3:
                    at(E); callr(g, 0x00853710, {}, In().EAX(L));
                    at(E); callr(g, 0x00854a60, {L}, In().EDI(B).ESI(D));
                    at(E); callr(g, 0x00855030, {L, B}, In().ESI(C));
                    break;
                case 4:
                    at(E); callr(g, 0x0042f2a0, {}, In().EAX(L));
                    at(E); callr(g, 0x00854a90, {L}, In().EDI(B).ESI(D));
                    at(E); callr(g, 0x00855090, {L, B}, In().ESI(C));
                    break;
                case 5:
                    at(E); callr(g, 0x00853640, {}, In().ESI(L));
                    at(E); callr(g, 0x00854f00, {D}, In().ECX(B).ESI(L));
                    at(E); callr(g, 0x00854f00, {B}, In().ECX(L).ESI(C));
                    break;
                case 6:
                    at(E); callr(g, 0x0042f1d0, {}, In().EAX(L));
                    at(E); callr(g, 0x00854f00, {D}, In().ECX(B).ESI(L));
                    at(E); callr(g, 0x00854f00, {B}, In().ECX(L).ESI(C));
                    break;
                default: break;
            }
            break;
        default: break;
    }
    m.r[4] = ESP0;
    return m.ret(0);
}

// 00855740 (EAX = A, ECX = B, EDI = C): the coupling products of the preparation by A's shape (and B's columns for 6-row A):
// 3 rows 00857220, 4 rows 00856b40, 5 rows (and 6 x 5 / 6 x 6) B = A * C^T (00854f00) then C = B^T (B's slot 2),
// 6 x 3 00855810, 6 x 4 00855ee0.
REFLEX_PORT_FN(0x00855740, Mat_Coupling, "mechanism", 0) {
    Guest g{m}; const uint32_t A = m.r[0], Bm = m.r[1], C = m.r[7];
    auto via_product = [&]() {
        callr(g, 0x00854f00, {C}, In().ECX(A).ESI(Bm));
        uint32_t f = g.u32(g.u32(Bm) + 8);
        g.call(f, {C}, Bm);
    };
    switch (g.u32(A + 4)) {
        case 3: callr(g, 0x00857220, {C}, In().ECX(A).EDX(Bm)); break;
        case 4: callr(g, 0x00856b40, {C}, In().ECX(Bm).EDX(A)); break;
        case 5: via_product(); break;
        case 6:
            switch (g.u32(Bm + 8)) {
                case 3: callr(g, 0x00855810, {C}, In().ECX(A).EDX(Bm)); break;
                case 4: callr(g, 0x00855ee0, {C}, In().ECX(A).EDX(Bm)); break;
                case 5: case 6: via_product(); break;
                default: break;
            }
            break;
        default: break;
    }
    return m.ret(0);
}
