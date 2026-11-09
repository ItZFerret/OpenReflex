// Shape-pair contact generation with composites (0x0086xxxx / 0x0087xxxx): the composite's children paired with the other
// shape through the pair dispatcher FUN_0085ddd0 (another group's), the contacts each pair adds tagged with the child index.
// Contact manifold M (ECX of the dispatcher): +0 contact count, +0x5b0 / +0x628 per-contact child index of shape A / B.
#include "collision_shapes.h"

using namespace collision;
using reflex_port::OUT_EAX;

namespace {

constexpr uint32_t kPairDispatch = 0x85ddd0;   // thiscall M; (shapeA, rotA*, posA*, shapeB, rotB*, posB*, swap): RET 0x1c

// The contacts added since `from` get the child index (ids M+0x5b0 when swapped, else M+0x628, for the composite's side).
inline uint32_t tag_new_contacts(Guest& g, uint32_t mf, uint32_t ids, uint32_t from, uint32_t child) {
    for (int32_t k = int32_t(from); k < g.i32(mf); k++) g.u32(ids + uint32_t(k) * 4, child);
    return g.u32(mf);
}

// The overlap of two AABBs (cmpleps on x, y, z, the lane masks summed): minB <= maxA and minA <= maxB.
inline bool aabb_overlap(Guest& g, uint32_t minA, uint32_t maxA, uint32_t minB, uint32_t maxB) {
    const X a = cmpleps(ld(g, minB), ld(g, maxA));
    const uint32_t s = a.u[1] + a.u[2] + a.u[0];
    if (s != 0xfffffffdu) return false;
    const X b = cmpleps(ld(g, minA), ld(g, maxB));
    return b.u[1] + b.u[2] + b.u[0] == s;
}

// A shape's world bounds: its vf2 (thiscall rotation*, position*, min*, max*).
inline void shape_bounds(Guest& g, uint32_t shape, uint32_t rot, uint32_t pos, uint32_t mn, uint32_t mx) {
    g.call(g.u32(g.u32(shape) + 8), {rot, pos, mn, mx}, shape);
}

}  // namespace

// 008732d0 (thiscall M; composite A*, rotA*, posA*, composite B*, rotB*, posB*, swap; RET 0x1c): every child pair of two
// composites whose world bounds overlap goes through the dispatcher; their new contacts are tagged with both child indices.
REFLEX_PORT_FN(0x008732d0, CollisionPair_CompositeVsComposite, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t mf = m.r[1], A = m.arg(0), qA = m.arg(1), pA = m.arg(2), B = m.arg(3), qB = m.arg(4), pB = m.arg(5);
    const uint32_t swap = m.arg(6), edi0 = m.r[7];
    const bool sw = (swap & 0xff) != 0;
    const uint32_t idsA = mf + (sw ? 0x628 : 0x5b0), idsB = mf + (sw ? 0x5b0 : 0x628);
    uint32_t countA = g.u32(mf), countB = countA;
    uint32_t eax = idsB;
    Frame f(g, 0xd4, 3);
    const uint32_t rotA = f.at(0x80), posA = f.at(0x50), maxA = f.at(0x90), minA = f.at(0xa0);
    const uint32_t rotB = f.at(0x70), posB = f.at(0x60), minB = f.at(0xb0), maxB = f.at(0xc0);
    if (int32_t((g.u32(A + 0x30) - g.u32(A + 0x2c)) & ~3u) > 0) {
        for (uint32_t i = 0; int32_t(i) < (int32_t(g.u32(A + 0x30) - g.u32(A + 0x2c)) >> 2); i++) {
            vector_check(g, A + 0x2c, i, 2);
            const uint32_t childA = g.u32(g.u32(A + 0x2c) + i * 4);
            g.m.r[7] = A;
            g.call(0x848650, {qA, rotA}, i);
            g.call(0x8486f0, {qA, pA, posA}, i);
            shape_bounds(g, childA, rotA, posA, minA, maxA);
            if (int32_t((g.u32(B + 0x30) - g.u32(B + 0x2c)) & ~3u) > 0) {
                for (uint32_t j = 0; int32_t(j) < (int32_t(g.u32(B + 0x30) - g.u32(B + 0x2c)) >> 2); j++) {
                    vector_check(g, B + 0x2c, j, 2);
                    const uint32_t childB = g.u32(g.u32(B + 0x2c) + j * 4);
                    g.m.r[7] = B;
                    g.call(0x848650, {qB, rotB}, j);
                    g.call(0x8486f0, {qB, pB, posB}, j);
                    shape_bounds(g, childB, rotB, posB, minB, maxB);
                    if (!aabb_overlap(g, minA, maxA, minB, maxB)) continue;
                    g.m.r[7] = mf;
                    g.call(kPairDispatch, {childA, rotA, posA, childB, rotB, posB, swap}, mf);
                    countB = tag_new_contacts(g, mf, idsB, countB, j);
                }
            }
            countA = tag_new_contacts(g, mf, idsA, countA, i);
            eax = countA;
        }
    }
    f.end();
    m.r[0] = eax; m.r[7] = edi0;
    return m.ret(0x1c);
}

// 008656c0 (custom: ECX rotA*, EDX posA*; stack: M*, shape A*, composite B*, rotB*, posB*, swap; RET 0x18): shape A against
// every child of composite B (no bounds test); the new contacts tagged with the child index (child count read once).
REFLEX_PORT_FN(0x008656c0, CollisionPair_ShapeVsComposite, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t rotA = m.r[1], posA = m.r[2];
    const uint32_t mf = m.arg(0), shapeA = m.arg(1), B = m.arg(2), qB = m.arg(3), pB = m.arg(4), swap = m.arg(5);
    const int32_t n = int32_t(g.u32(B + 0x30) - g.u32(B + 0x2c)) >> 2;
    const uint32_t ids = mf + ((swap & 0xff) ? 0x5b0 : 0x628);
    uint32_t count = g.u32(mf), eax = uint32_t(n);
    Frame f(g, 0x54, 3);
    const uint32_t rot = f.at(0x40), pos = f.at(0x30), edi0 = m.r[7];
    m.r[7] = B;
    for (int32_t i = 0; i < n; i++) {
        vector_check(g, B + 0x2c, uint32_t(i), 2);
        const uint32_t child = g.u32(g.u32(B + 0x2c) + uint32_t(i) * 4);
        g.call(0x848650, {qB, rot}, uint32_t(i));
        g.call(0x8486f0, {qB, pB, pos}, uint32_t(i));
        g.call(kPairDispatch, {shapeA, rotA, posA, child, rot, pos, swap}, mf);
        count = tag_new_contacts(g, mf, ids, count, uint32_t(i));
        eax = count;
    }
    f.end();
    m.r[0] = eax; m.r[7] = edi0;
    return m.ret(0x18);
}

// 00872860 (as 008656c0, the child count read again per child): the eax left is the child count (the byte size when there
// are none).
REFLEX_PORT_FN(0x00872860, CollisionPair_ShapeVsCompositeB, "collision", OUT_EAX) {
    Guest g{m};
    const uint32_t rotA = m.r[1], posA = m.r[2];
    const uint32_t mf = m.arg(0), shapeA = m.arg(1), B = m.arg(2), qB = m.arg(3), pB = m.arg(4), swap = m.arg(5);
    const uint32_t ids = mf + ((swap & 0xff) ? 0x5b0 : 0x628);
    uint32_t count = g.u32(mf);
    uint32_t eax = g.u32(B + 0x30) - g.u32(B + 0x2c);
    Frame f(g, 0x54, 3);
    const uint32_t rot = f.at(0x40), pos = f.at(0x30), edi0 = m.r[7];
    m.r[7] = B;
    if (int32_t(eax & ~3u) > 0) {
        for (uint32_t i = 0;;) {
            vector_check(g, B + 0x2c, i, 2);
            const uint32_t child = g.u32(g.u32(B + 0x2c) + i * 4);
            g.call(0x848650, {qB, rot}, i);
            g.call(0x8486f0, {qB, pB, pos}, i);
            g.call(kPairDispatch, {shapeA, rotA, posA, child, rot, pos, swap}, mf);
            tag_new_contacts(g, mf, ids, count, i);
            eax = uint32_t(int32_t(g.u32(B + 0x30) - g.u32(B + 0x2c)) >> 2);
            count = g.u32(mf);
            i++;
            if (!(int32_t(i) < int32_t(eax))) break;
        }
    }
    f.end();
    m.r[0] = eax; m.r[7] = edi0;
    return m.ret(0x18);
}
