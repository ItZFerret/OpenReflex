// World group: the physics world's generic dynamic objects (0x0076xxxx-0x0077xxxx): collision between them, integration, the
// solver dispatch around the vehicle simulations. See native/game/notes/world.md.
#include "../port.h"
#include "world_util.h"

using namespace world;
using reflex_port::OUT_EAX;
using reflex_port::OUT_XMM_ALL;

namespace {

constexpr uint32_t kAbortInvalidParam = 0x00916224;   // IAT: the CRT's invalid-parameter abort (vector bounds checks)
constexpr uint32_t kObjSize = 0xe0;                   // generic dynamic object (world +0x1960 / +0x1964 vector)

inline X cmpneqps(const X& a, const X& b) { X r; for (int i = 0; i < 4; i++) r.u[i] = !(a.f(i) == b.f(i)) ? 0xffffffffu : 0u; return r; }

// Number of objects in world's vector at +0x1960 (begin) / +0x1964 (end): (end - begin) / 0xe0, signed as the exe computes it.
inline uint32_t obj_count(const Guest& g, uint32_t world) {
    return uint32_t(int32_t(g.u32(world + 0x1964) - g.u32(world + 0x1960)) / int32_t(kObjSize));
}
inline void bounds_check(Guest& g, uint32_t i, uint32_t n) { if (i >= n) g.call(g.u32(kAbortInvalidParam)); }

// The broad-phase result cursor (world +0x196c): [c] position, [c+0x30] -> array of indices with the count at +0x400.
inline uint32_t next_pair(Guest& g, uint32_t cursor) {
    uint32_t arr = g.u32(cursor + 0x30);
    int32_t pos = g.i32(cursor);
    if (pos < g.i32(arr + 0x400)) { pos++; g.u32(cursor, uint32_t(pos)); return g.u32(arr + 4 * uint32_t(pos) - 4); }
    return 0xffffffffu;
}

// |v|^2 summed as the exe does after mulps v, v: (x + z) + y (shufps 0x66 pattern) or (x + y) + z (shufps 0x99).
inline float dot3_xzy(const X& sq) { return x86::add(x86::add(sq.f(0), sq.f(2)), sq.f(1)); }
inline float dot3_xyz(const X& sq) { return x86::add(x86::add(sq.f(0), sq.f(1)), sq.f(2)); }
// "all of lanes 0..2 set" as the exe tests a compare mask: (m1 + m2 + m0) == -3 in 32-bit arithmetic.
inline bool all3(const X& mask) { return uint32_t(mask.u[1] + mask.u[2] + mask.u[0]) == 0xfffffffdu; }

}  // namespace

// 0076bf60: collisions between the world's generic dynamic objects (stdcall, arg 0 = world, ret 4). For each object A that is
// not disabled (shape +0x14 bit 3): its bounds from shape vfunc 3, a broad-phase query (00882d90, cursor world +0x196c), and for
// every candidate B != A (unless both shapes have +0x10 bit 6): the narrow phase 0085ddd0 into a contact set on the stack, its
// impulse 0085dcf0, the response on B 0076e460, then A's accumulators (+0xb0 summed linear, +0xc0 angular) and B's / A's +0xa0
// velocity exchange (clamped by B's material +0x20 -> +0xc0), and a contact event (up to 100 at world +0, 64 bytes each,
// count +0x1900). Afterwards A's +0xb0 is averaged over the contacts (+1 when A was already moving faster than the threshold
// 0xd61670) and +0xc0 normalized (NaN pattern 0x92dc30 when its length is 0 or inf).
REFLEX_PORT_FN(0x0076bf60, DynObj_CollideAll, "world", 0) {
    Guest g{m};
    const uint32_t world = m.arg(0);
    Frame f(m, 0x7a4, 3);
    const uint32_t sp = f.esp;
    const uint32_t kBox = sp + 0x60, kQuery = sp + 0xa0, kPosA = sp + 0x50, kRotA = sp + 0x40, kPosB = sp + 0xb0, kRotB = sp + 0x80;
    const uint32_t kContacts = sp + 0xf0, kImpulse = sp + 0xc0;
    const X half = X::splat(g.u32(0x009322cc));
    auto clear_ids = [&] { for (uint32_t k = 0; k < 30; k++) { g.u32(sp + 0x6a0 + 4 * k, 0xffffffffu); g.u32(sp + 0x718 + 4 * k, 0xffffffffu); } };
    clear_ids();
    if (obj_count(g, world) != 0) {
        uint32_t i = 0, off = 0;
        do {
            bounds_check(g, i, obj_count(g, world));
            const uint32_t A = g.u32(world + 0x1960) + off;
            const uint32_t shapeA = g.u32(A + 0x2c);
            if (!((g.u32(shapeA + 0x14) >> 3) & 1)) {
                st(g, kRotA, ld(g, A + 0x40));
                st(g, kPosA, ld(g, A + 0x50));
                m.r[3] = world; m.r[6] = i; m.r[7] = A;   // the original's registers at the calls
                f.call(g.u32(g.u32(shapeA) + 0xc), {kPosA, kRotA, kBox, kQuery}, shapeA);
                const uint32_t cursor = g.u32(world + 0x196c);
                m.r[6] = g.u32(cursor + 0x30);            // 00882d90: ESI = the broad-phase grid
                f.call(0x00882d90, {kBox}, kQuery);
                g.u32(cursor, 0);
                const X va = ld(g, A + 0xc0);
                const float speed = x86::sqrt(dot3_xzy(mulps(va, va)));
                int32_t contacts = all3(cmpltps(ld(g, 0x00d61670), X::fl(speed, speed, speed, speed))) ? 1 : 0;
                for (uint32_t idx = next_pair(g, g.u32(world + 0x196c)); idx != 0xffffffffu; idx = next_pair(g, g.u32(world + 0x196c))) {
                    bounds_check(g, idx, obj_count(g, world));
                    const uint32_t B = g.u32(world + 0x1960) + idx * kObjSize;
                    if (B == A) continue;
                    const uint32_t shapeB = g.u32(B + 0x2c);
                    if (((g.u32(shapeA + 0x10) >> 6) & 1) && ((g.u32(shapeB + 0x10) >> 6) & 1)) continue;
                    st(g, kRotB, ld(g, B + 0x40));
                    st(g, kPosB, ld(g, B + 0x50));
                    g.u32(sp + 0x790, 0);
                    g.u32(kContacts, 0);
                    clear_ids();
                    m.r[3] = B; m.r[6] = 0;
                    f.call(0x0085ddd0, {shapeA, kPosA, kRotA, shapeB, kPosB, kRotB, 0}, kContacts);
                    if (g.u32(sp + 0x790) == 0) continue;
                    const X prevA = ld(g, A + 0x80), prevB = ld(g, B + 0x80);
                    m.r[6] = kContacts;
                    f.call(0x0085dcf0, {kImpulse});
                    f.call(0x0076e460, {B, kImpulse});
                    st(g, A + 0xb0, addps(ld(g, A + 0xb0), ld(g, kImpulse)));
                    contacts++;
                    const X scale = X::splat(g.u32(0x009cc8a8));
                    st(g, A + 0xc0, addps(subps(X::splat(0), ld(g, kImpulse + 0x10)), ld(g, A + 0xc0)));
                    const int32_t n = g.i32(kContacts);
                    X lo = ld(g, 0x00d61670), hi = lo;
                    for (int32_t k = 0; k < n; k++) {
                        const uint32_t p = sp + 0x110 + 0x30 * uint32_t(k);
                        const X d = mulps(ld(g, p + 0x10), ld(g, p));
                        hi = maxps(hi, d); lo = minps(lo, d);
                    }
                    X push = mulps(mulps(addps(lo, hi), scale), half);
                    const float plen = x86::sqrt(dot3_xzy(mulps(push, push)));
                    const X plen4 = X::fl(plen, plen, plen, plen);
                    const X limit = ld(g, g.u32(B + 0x20) + 0xc0);
                    if (all3(cmpltps(limit, plen4))) push = mulps(divps(limit, plen4), push);
                    st(g, B + 0xa0, addps(push, ld(g, B + 0xa0)));
                    st(g, A + 0xa0, subps(ld(g, A + 0xa0), push));
                    const X dA = subps(ld(g, A + 0x80), prevA), dB = subps(ld(g, B + 0x80), prevB);
                    const float movedA = x86::sqrt(dot3_xzy(mulps(dA, dA)));
                    const float movedB = x86::sqrt(dot3_xzy(mulps(dB, dB)));
                    const uint32_t events = g.u32(world + 0x1900);
                    if (events < 100) {
                        g.u32(world + 0x1900, events + 1);
                        const uint32_t ev = world + events * 64;
                        g.u32(ev, g.u32(A + 0x34));
                        g.u32(ev + 4, g.u32(B + 0x34));
                        g.u32(ev + 0x2c, 0);
                        g.u8(ev + 0x20, 0);
                        g.u32(ev + 0x30, g.u32(sp + 0x6a0));
                        g.u32(ev + 0x34, g.u32(sp + 0x718));
                        g.u32(ev + 0x10, g.u32(sp + 0x100));
                        g.u32(ev + 0x14, g.u32(sp + 0x104));
                        g.u32(ev + 0x18, g.u32(sp + 0x108));
                        g.f32(ev + 0x24, movedA);
                        g.f32(ev + 0x28, movedB);
                    }
                }
                if (contacts > 0) {
                    const float c = float(contacts);
                    st(g, A + 0xb0, divps(ld(g, A + 0xb0), X::fl(c, c, c, c)));
                    const X w = ld(g, A + 0xc0);
                    const float len2 = dot3_xyz(mulps(w, w));
                    const X l2 = X::fl(len2, len2, len2, len2);
                    const X len = sqrtps(l2);
                    X r = divps(w, len);
                    const X finite = cmpneqps(l2, ld(g, 0x0092dc20));
                    r = andps(andps(r, cmpneqps(len, X::splat(0))), finite);
                    st(g, A + 0xc0, orps(r, andnps(finite, ld(g, 0x0092dc30))));
                }
            }
            i++; off += kObjSize;
        } while (i < obj_count(g, world));
    }
    f.restore_saved();
    return m.ret(4);
}

namespace {
// The contact set the narrow phases fill on the caller's stack (count at +0, records of 0x30 bytes at +0x10, the feature ids at
// +0x5b0 / +0x628 (30 each), the "touching" flag at +0x6a0).
inline void reset_contact_set(Guest& g, uint32_t set) {
    g.u32(set + 0x6a0, 0); g.u32(set, 0);
    for (uint32_t k = 0; k < 30; k++) { g.u32(set + 0x5b0 + 4 * k, 0xffffffffu); g.u32(set + 0x628 + 4 * k, 0xffffffffu); }
}
inline void clear_contact_ids(Guest& g, uint32_t set) {
    for (uint32_t k = 0; k < 30; k++) { g.u32(set + 0x5b0 + 4 * k, 0xffffffffu); g.u32(set + 0x628 + 4 * k, 0xffffffffu); }
}
// A+0xb0 /= n; A+0xc0 normalized (0x92dc30 NaNs where its squared length is inf, 0 where the length is 0).
inline void average_and_normalize(Guest& g, uint32_t A, int32_t n) {
    const float c = float(n);
    st(g, A + 0xb0, divps(ld(g, A + 0xb0), X::fl(c, c, c, c)));
    const X w = ld(g, A + 0xc0);
    const float len2 = dot3_xyz(mulps(w, w));
    const X l2 = X::fl(len2, len2, len2, len2);
    const X len = sqrtps(l2);
    X r = divps(w, len);
    const X finite = cmpneqps(l2, ld(g, 0x0092dc20));
    r = andps(andps(r, cmpneqps(len, X::splat(0))), finite);
    st(g, A + 0xc0, orps(r, andnps(finite, ld(g, 0x0092dc30))));
}
inline X splat_f(float v) { return X::fl(v, v, v, v); }
// The debug ring of the last ten contacts (0xd613a0, 0x30 bytes each; index 0xd61390, high-water mark 0xd61394).
inline void debug_ring_push(Guest& g, const X& a, const X& b, const X& c) {
    uint32_t i = g.u32(0x00d61390);
    const uint32_t slot = 0x00d613a0 + i * 0x30;
    g.u32(0x00d61390, i + 1);
    st(g, slot, a); st(g, slot + 0x10, b); st(g, slot + 0x20, c);
    i = g.u32(0x00d61390);
    if (int32_t(i) > g.i32(0x00d61394)) g.u32(0x00d61394, i);
    if (int32_t(i) >= 10) g.u32(0x00d61390, 0);
}
}  // namespace

// 0076d3c0: the generic dynamic objects against the terrain (stdcall, arg 0 = world, ret 4; nothing when world +0xcc9c, the
// terrain, is null). Per object whose shape lacks +0x14 bit 5: 00667e40 (EDX = contact set at [esp+0x40], second set at
// [esp+0x6f0], outputs [esp+0x1c] / [esp+0xc]); an event for the second set (type 1); for the first set the contacts deeper
// than 0xd61670 get the response 0076f160 (accumulating +0xb0 / +0xc0 as in 0076bf60), then the push-out along the contact
// normals (min + max of depth * normal over the penetrating contacts, times -0.8, clamped to material +0xc0) moves +0x40 and
// +0xa0, and an event (type 0) records it.
REFLEX_PORT_FN(0x0076d3c0, DynObj_CollideTerrain, "world", 0) {
    Guest g{m};
    const uint32_t world = m.arg(0);
    Frame f(m, 0xda4, 3);
    const uint32_t sp = f.esp, set1 = sp + 0x40, set2 = sp + 0x6f0, kOut1c = sp + 0x1c, kOutC = sp + 0xc;
    if (g.u32(world + 0xcc9c) != 0) {
        clear_contact_ids(g, set1); clear_contact_ids(g, set2);
        if (obj_count(g, world) != 0) {
            uint32_t i = 0, off = 0;
            do {
                bounds_check(g, i, obj_count(g, world));
                const uint32_t A = g.u32(world + 0x1960) + off;
                int32_t cnt = 0;
                reset_contact_set(g, set1); reset_contact_set(g, set2);
                const uint32_t shape = g.u32(A + 0x2c);
                g.u32(kOut1c, 0); g.u32(kOutC, 0);
                if (!((g.u32(shape + 0x14) >> 5) & 1)) {
                    m.r[3] = world; m.r[6] = 0; m.r[7] = A; m.r[2] = set1;
                    f.call(0x00667e40, {g.u32(g.u32(world + 0xcc9c) + 0x31ec), A + 0x40, A + 0x50, kOut1c, set2, kOutC}, shape);   // ECX = shape, EDX = set1
                }
                if (g.u32(set2 + 0x6a0) != 0) {
                    const uint32_t events = g.u32(world + 0x1900);
                    if (events < 100) {
                        g.u32(world + 0x1900, events + 1);
                        const uint32_t ev = world + events * 64;
                        g.u32(ev, g.u32(A + 0x34));
                        g.u32(ev + 4, g.u32(world + 0xcc9c));
                        g.u32(ev + 0x2c, g.u32(kOutC));
                        g.u8(ev + 0x20, 1);
                        g.u32(ev + 0x30, g.u32(set2 + 0x5b0));
                        g.u32(ev + 0x34, g.u32(set2 + 0x628));
                        g.u32(ev + 0x10, g.u32(set2 + 0x10)); g.u32(ev + 0x14, g.u32(set2 + 0x14)); g.u32(ev + 0x18, g.u32(set2 + 0x18));
                        const X v = ld(g, A + 0x60);
                        g.f32(ev + 0x24, x86::sqrt(dot3_xzy(mulps(v, v))));
                        const X v2 = ld(g, A + 0x60);
                        g.f32(ev + 0x28, x86::sqrt(dot3_xzy(mulps(v2, v2))));
                    }
                }
                if (g.u32(set1 + 0x6a0) != 0) {
                    const int32_t n = g.i32(set1);
                    X thr = ld(g, 0x00d61670);
                    const X prev = ld(g, A + 0x80);
                    if (n > 0) {
                        for (int32_t k = 0; k < n; k++) {
                            const uint32_t p = set1 + 0x10 + 0x30 * uint32_t(k);
                            const X pt = ld(g, p), nrm = ld(g, p + 0x10), third = ld(g, p + 0x20);
                            debug_ring_push(g, pt, nrm, third);
                            const X d = mulps(subps(pt, ld(g, A + 0x40)), nrm);
                            if (all3(cmpltps(thr, splat_f(dot3_xyz(d))))) {
                                m.r[3] = world; m.r[6] = p; m.r[7] = A;
                                f.call(0x0076f160, {p});
                                st(g, A + 0xb0, addps(ld(g, p), ld(g, A + 0xb0)));
                                cnt++;
                                st(g, A + 0xc0, addps(subps(X::splat(0), ld(g, p + 0x10)), ld(g, A + 0xc0)));
                                thr = ld(g, 0x00d61670);
                            }
                        }
                        if (cnt > 0) { average_and_normalize(g, A, cnt); thr = ld(g, 0x00d61670); }
                    }
                    const X scale = X::splat(g.u32(0x009cc8a8));
                    X hi = thr, lo = thr;
                    if (n > 0) {
                        const X pos = ld(g, A + 0x40);
                        for (int32_t k = 0; k < n; k++) {
                            const uint32_t p = set1 + 0x10 + 0x30 * uint32_t(k);
                            const X nrm = ld(g, p + 0x10);
                            const X d = mulps(subps(ld(g, p), pos), nrm);
                            if (all3(cmpltps(thr, splat_f(dot3_xyz(d))))) {
                                const X depth = mulps(ld(g, p + 0x20), nrm);
                                lo = minps(lo, depth); hi = maxps(hi, depth);
                            }
                        }
                    }
                    X push = mulps(addps(lo, hi), subps(X::splat(0), scale));
                    const float plen = x86::sqrt(dot3_xzy(mulps(push, push)));
                    const X plen4 = splat_f(plen);
                    const X limit = ld(g, g.u32(A + 0x20) + 0xc0);
                    if (all3(cmpltps(limit, plen4))) push = mulps(divps(limit, plen4), push);
                    st(g, A + 0x40, addps(ld(g, A + 0x40), push));
                    st(g, A + 0xa0, addps(ld(g, A + 0xa0), push));
                    const X dm = subps(ld(g, A + 0x80), prev);
                    const float moved = x86::sqrt(dot3_xzy(mulps(dm, dm)));
                    const uint32_t events = g.u32(world + 0x1900);
                    if (events < 100) {
                        g.u32(world + 0x1900, events + 1);
                        const uint32_t ev = world + events * 64;
                        g.u32(ev, g.u32(A + 0x34));
                        g.u32(ev + 4, g.u32(world + 0xcc9c));
                        g.u32(ev + 0x2c, g.u32(kOut1c));
                        g.u8(ev + 0x20, 0);
                        g.u32(ev + 0x30, g.u32(set1 + 0x5b0));
                        g.u32(ev + 0x34, g.u32(set1 + 0x628));
                        g.u32(ev + 0x10, g.u32(set1 + 0x10)); g.u32(ev + 0x14, g.u32(set1 + 0x14)); g.u32(ev + 0x18, g.u32(set1 + 0x18));
                        g.f32(ev + 0x24, moved); g.f32(ev + 0x28, moved);
                    }
                }
                i++; off += kObjSize;
            } while (i < obj_count(g, world));
        }
    }
    f.restore_saved();
    return m.ret(4);
}

// 0076b4a0: the generic dynamic objects against the static collision objects (stdcall, arg 0 = world, ret 4). Per object A:
// bounds (shape vfunc 3), the static broad phase 00883020 (cursor world +0x19cc; ECX = query, EDX = box, ESI = grid), A's
// accumulators +0xb0 / +0xc0 reset to 0xd61670, then per enabled static S (world +0x19c0 vector, 64 bytes, byte +0x38): the
// narrow phase 0085ddd0, impulse 0085dcf0, S's response 0076efc0, A's accumulators, and the push-out (min + max of depth *
// normal times -scale, clamped to |0.5|; the clamp ratio becomes the scale of every later push in this call, as in the
// original), moving +0x40 and +0xa0, plus a contact event (type 0, other = S +0x30).
REFLEX_PORT_FN(0x0076b4a0, DynObj_CollideStatic, "world", 0) {
    Guest g{m};
    const uint32_t world = m.arg(0);
    Frame f(m, 0x7a4, 3);
    const uint32_t sp = f.esp;
    const uint32_t kPos = sp + 0x50, kRot = sp + 0x60, kBox = sp + 0x80, kQuery = sp + 0xa0, kPosS = sp + 0x70, kRotS = sp + 0xb0;
    const uint32_t kContacts = sp + 0xf0, kImpulse = sp + 0xc0;
    const X limit = X::splat(g.u32(0x009322cc));
    X scale = X::splat(g.u32(0x00931f24));
    clear_contact_ids(g, kContacts);
    if (obj_count(g, world) != 0) {
        uint32_t i = 0, off = 0;
        do {
            bounds_check(g, i, obj_count(g, world));
            const uint32_t A = g.u32(world + 0x1960) + off;
            st(g, kRot, ld(g, A + 0x40));
            st(g, kPos, ld(g, A + 0x50));
            const uint32_t shapeA = g.u32(A + 0x2c);
            m.r[3] = world; m.r[6] = i; m.r[7] = A;
            f.call(g.u32(g.u32(shapeA) + 0xc), {kPos, kRot, kBox, kQuery}, shapeA);
            const uint32_t cursor = g.u32(world + 0x19cc);
            m.r[6] = g.u32(cursor + 0x30); m.r[2] = kBox;
            f.call(0x00883020, {kQuery}, kQuery);
            g.u32(cursor, 0);
            st(g, A + 0xb0, ld(g, 0x00d61670));
            st(g, A + 0xc0, ld(g, 0x00d61670));
            int32_t cnt = 0;
            for (uint32_t idx = next_pair(g, g.u32(world + 0x19cc)); idx != 0xffffffffu; idx = next_pair(g, g.u32(world + 0x19cc))) {
                const uint32_t ns = uint32_t(int32_t(g.u32(world + 0x19c4) - g.u32(world + 0x19c0)) >> 6);
                bounds_check(g, idx, ns);
                const uint32_t S = g.u32(world + 0x19c0) + idx * 64;
                if (g.u8(S + 0x38) == 0) continue;
                st(g, kRotS, ld(g, S));
                st(g, kPosS, ld(g, S + 0x10));
                reset_contact_set(g, kContacts);
                m.r[3] = world; m.r[6] = S; m.r[7] = A; m.r[2] = kPos;
                f.call(0x0085ddd0, {g.u32(A + 0x2c), kPos, kRot, g.u32(S + 0x28), kPosS, kRotS, 0}, kContacts, g.u32(A + 0x2c));
                if (g.u32(kContacts + 0x6a0) == 0) continue;
                const X prevA = ld(g, A + 0x80);
                m.r[6] = kContacts;
                f.call(0x0085dcf0, {kImpulse}, kImpulse);
                m.r[2] = kImpulse;
                f.call(0x0076efc0, {kImpulse}, S);
                st(g, A + 0xb0, addps(ld(g, kImpulse), ld(g, A + 0xb0)));
                cnt++;
                st(g, A + 0xc0, addps(subps(X::splat(0), ld(g, kImpulse + 0x10)), ld(g, A + 0xc0)));
                const int32_t n = g.i32(kContacts);
                const X thr = ld(g, 0x00d61670);
                X hi = thr, lo = thr;
                if (n > 0) {
                    const X pos = ld(g, A + 0x40);
                    for (int32_t k = 0; k < n; k++) {
                        const uint32_t p = kContacts + 0x10 + 0x30 * uint32_t(k);
                        const X nrm = ld(g, p + 0x10);
                        const X depth = mulps(ld(g, p + 0x20), nrm);
                        const X d = mulps(subps(ld(g, p), pos), nrm);
                        if (all3(cmpltps(thr, splat_f(dot3_xyz(d))))) { lo = minps(lo, depth); hi = maxps(hi, depth); }
                    }
                }
                X push = mulps(subps(X::splat(0), scale), addps(lo, hi));
                const X len = splat_f(x86::sqrt(dot3_xzy(mulps(push, push))));
                if (all3(cmpltps(limit, len))) { const X ratio = divps(limit, len); push = mulps(push, ratio); scale = ratio; }
                st(g, A + 0x40, addps(ld(g, A + 0x40), push));
                st(g, A + 0xa0, addps(ld(g, A + 0xa0), push));
                const X dm = subps(ld(g, A + 0x80), prevA);
                const float moved = x86::sqrt(dot3_xzy(mulps(dm, dm)));
                const uint32_t events = g.u32(world + 0x1900);
                if (events < 100) {
                    g.u32(world + 0x1900, events + 1);
                    const uint32_t ev = world + events * 64;
                    g.u32(ev, g.u32(A + 0x34));
                    g.u32(ev + 4, g.u32(S + 0x30));
                    g.u32(ev + 0x2c, 0);
                    g.u8(ev + 0x20, 0);
                    g.u32(ev + 0x30, g.u32(kContacts + 0x5b0));
                    g.u32(ev + 0x34, g.u32(kContacts + 0x628));
                    g.u32(ev + 0x10, g.u32(kContacts + 0x10)); g.u32(ev + 0x14, g.u32(kContacts + 0x14)); g.u32(ev + 0x18, g.u32(kContacts + 0x18));
                    g.f32(ev + 0x24, moved); g.u32(ev + 0x28, 0);
                }
            }
            if (cnt > 0) average_and_normalize(g, A, cnt);
            i++; off += kObjSize;
        } while (i < obj_count(g, world));
    }
    f.restore_saved();
    return m.ret(4);
}

// 0076b9b0: the generic dynamic objects against the keyed / kinematic objects (stdcall, arg 0 = world, ret 4): world +0x19a0
// vector (0xe0 records like the generic ones; enabled by byte +0xb0), broad phase cursor +0x19ac. Per contact pair: the narrow
// phase 0085ddd0, per contact 0076e090 (ECX = contact; first a "still moving" check of A's +0x60 speed against K's +0xc0 that
// clears K's +0xb4 axis mask and +0xd0 flag); K with +0x84 bit 5 is pushed: the impulse (min + max of depth * normal, * 0.8 *
// 0.5) goes through 0084fe00 (ECX = world +0x9a00 + 32 * K+0x80), clamped by K's material, is taken off A's +0xa0, its locked
// axes (mask +0xb4) zeroed, and added to the kinematic position world +0x1d00 + 0x50 * K+0x80 (copied to K +0x40). An event
// records both displacements.
REFLEX_PORT_FN(0x0076b9b0, DynObj_CollideKinematic, "world", 0) {
    Guest g{m};
    const uint32_t world = m.arg(0);
    Frame f(m, 0x784, 3);
    const uint32_t sp = f.esp;
    const uint32_t kPos = sp + 0x60, kRot = sp + 0x70, kBox = sp + 0x90, kQuery = sp + 0xa0, kPosK = sp + 0xc0, kRotK = sp + 0x80;
    const uint32_t kSet = sp + 0xd0, kPush = sp + 0x40;
    const X half = X::splat(g.u32(0x009322cc));
    // this contact set: count +0, records +0x10, ids +0x5b0 / +0x628, flag +0x6a0 (same layout at sp + 0xd0)
    clear_contact_ids(g, kSet);
    if (obj_count(g, world) != 0) {
        uint32_t i = 0, off = 0;
        do {
            bounds_check(g, i, obj_count(g, world));
            const uint32_t A = g.u32(world + 0x1960) + off;
            st(g, kRot, ld(g, A + 0x40));
            st(g, kPos, ld(g, A + 0x50));
            const uint32_t shapeA = g.u32(A + 0x2c);
            m.r[3] = world; m.r[6] = i; m.r[7] = A;
            f.call(g.u32(g.u32(shapeA) + 0xc), {kPos, kRot, kBox, kQuery}, shapeA);
            const uint32_t cursor = g.u32(world + 0x19ac);
            m.r[6] = g.u32(cursor + 0x30); m.r[2] = kBox;
            f.call(0x00883020, {kQuery}, kQuery);
            g.u32(cursor, 0);
            for (uint32_t idx = next_pair(g, g.u32(world + 0x19ac)); idx != 0xffffffffu; idx = next_pair(g, g.u32(world + 0x19ac))) {
                const uint32_t nk = uint32_t(int32_t(g.u32(world + 0x19a4) - g.u32(world + 0x19a0)) / int32_t(kObjSize));
                bounds_check(g, idx, nk);
                const uint32_t K = g.u32(world + 0x19a0) + idx * kObjSize;
                if (g.u8(K + 0xb0) == 0) continue;
                st(g, kRotK, ld(g, K + 0x40));
                st(g, kPosK, ld(g, K + 0x50));
                reset_contact_set(g, kSet);
                m.r[3] = world; m.r[6] = K; m.r[7] = A; m.r[2] = kPos;
                f.call(0x0085ddd0, {g.u32(A + 0x2c), kPos, kRot, g.u32(K + 0x2c), kPosK, kRotK, 0}, kSet, g.u32(A + 0x2c));
                if (g.u32(kSet + 0x6a0) == 0) continue;
                const X prevA = ld(g, A + 0x80);
                X x2 = ld(g, 0x00d61670);
                X prevK = x2;
                if (g.i32(K + 0x80) >= 0) prevK = ld(g, world + 0x1d10 + g.u32(K + 0x80) * 0x50);
                int32_t n = g.i32(kSet);
                if (n > 0) {
                    for (int32_t k = 0; k < n; k = k + 1, n = g.i32(kSet)) {
                        const uint32_t p = kSet + 0x10 + 0x30 * uint32_t(k);
                        if (g.u8(K + 0xd0) != 0) {
                            const X v = ld(g, A + 0x60);
                            if (all3(cmpltps(ld(g, K + 0xc0), splat_f(x86::sqrt(dot3_xzy(mulps(v, v))))))) { g.u32(K + 0xb4, 0); g.u8(K + 0xd0, 0); }
                        }
                        m.r[3] = world; m.r[6] = K; m.r[7] = A;
                        f.call(0x0076e090, {world, K}, p);
                    }
                    x2 = ld(g, 0x00d61670);
                }
                if (g.u8(K + 0x84) & 0x20) {
                    const X scale = X::splat(g.u32(0x009cc8a8));
                    X lo = x2, hi = x2;
                    for (int32_t k = 0; k < n; k++) {
                        const uint32_t p = kSet + 0x10 + 0x30 * uint32_t(k);
                        const X d = mulps(ld(g, p + 0x20), ld(g, p + 0x10));
                        hi = maxps(hi, d); lo = minps(lo, d);
                    }
                    const uint32_t body = world + (g.u32(K + 0x80) + 0x4d0) * 32;
                    st(g, kPush, mulps(mulps(addps(lo, hi), scale), half));
                    m.r[2] = kPush;
                    f.call(0x0084fe00, {kPush}, body);
                    X push = ld(g, kPush);
                    const X len = splat_f(x86::sqrt(dot3_xzy(mulps(push, push))));
                    const X limit = ld(g, g.u32(K + 0x20) + 0xc0);
                    if (all3(cmpltps(limit, len))) push = mulps(divps(limit, len), push);
                    st(g, A + 0xa0, subps(ld(g, A + 0xa0), push));
                    const uint32_t lock = g.u32(K + 0xb4);
                    if (lock != 0) {
                        const X z = ld(g, 0x00d61670);
                        if (lock & 1) push.u[0] = z.u[0];
                        if (lock & 2) push.u[1] = z.u[1];
                        if (lock & 4) { push.u[2] = z.u[2]; push.u[3] = z.u[2]; }
                    }
                    const uint32_t kpos = world + 0x1d00 + g.u32(K + 0x80) * 0x50;
                    st(g, kpos, addps(ld(g, kpos), push));
                    st(g, K + 0x40, ld(g, world + 0x1d00 + g.u32(K + 0x80) * 0x50));
                    x2 = ld(g, 0x00d61670);
                }
                const X dA = subps(ld(g, A + 0x80), prevA);
                if (g.i32(K + 0x80) >= 0) x2 = ld(g, world + 0x1d10 + g.u32(K + 0x80) * 0x50);
                const X dK = subps(x2, prevK);
                const float movedA = x86::sqrt(dot3_xzy(mulps(dA, dA)));
                const float movedK = x86::sqrt(dot3_xzy(mulps(dK, dK)));
                const uint32_t events = g.u32(world + 0x1900);
                if (events < 100) {
                    g.u32(world + 0x1900, events + 1);
                    const uint32_t ev = world + events * 64;
                    g.u32(ev, g.u32(A + 0x34));
                    g.u32(ev + 4, g.u32(K + 0x34));
                    g.u32(ev + 0x2c, 0);
                    g.u8(ev + 0x20, 0);
                    g.u32(ev + 0x30, g.u32(kSet + 0x5b0));
                    g.u32(ev + 0x34, g.u32(kSet + 0x628));
                    g.u32(ev + 0x10, g.u32(kSet + 0x10)); g.u32(ev + 0x14, g.u32(kSet + 0x14)); g.u32(ev + 0x18, g.u32(kSet + 0x18));
                    g.f32(ev + 0x24, movedA); g.f32(ev + 0x28, movedK);
                }
            }
            i++; off += kObjSize;
        } while (i < obj_count(g, world));
    }
    f.restore_saved();
    return m.ret(4);
}

#ifdef WORLD_HAND_READABLE   // readable port; the literal one in world_phys_lit4.cpp (exact ECX / EDX for generated callers) is used
// 007729f0: what a shape at a position touches in the physics world (ECX = int types[10] out, EDX = shape; stdcall world, pos,
// results (10 contacts of 0x30 bytes: point, normal, depth), ids[10]; ret 0x10; EAX = count). The results start as
// (1e10, 0xd61670, -1e10). Order: water (00402dc0 on the list at 0x9e30d4, 007c5950, then the depth test against the shape's
// radius 008454c0: type 0x10, id water +0xe4), static objects (world +0x19c0, type 1, id S +0x30), keyed objects (+0x19a0, type
// 2, id +0x34), generic dynamic objects (+0x1960, type 4, id +0x34), the terrain (+0xcc9c, 00667e40, type 8, id the terrain);
// at most 10 contacts, stopping early once 10 are found. Rotation of the shape: the identity at 0xd61640.
REFLEX_PORT_FN(0x007729f0, World_QueryShapeContacts, "world", OUT_EAX) {
    Guest g{m};
    const uint32_t types = m.r[1], shape = m.r[2];
    const uint32_t world = m.arg(0), pos = m.arg(1), results = m.arg(2), ids = m.arg(3);
    Frame f(m, 0x724, 3);
    const uint32_t sp = f.esp, kBox = sp + 0x50, kQuery = sp + 0x60, kSet = sp + 0x70, kA = sp + 0x30, kB = sp + 0x40;
    constexpr uint32_t kIdentity = 0x00d61640;
    {
        const X hi = X::splat(g.u32(0x00977b88)), lo = X::splat(g.u32(0x00977d60));
        for (uint32_t k = 0; k < 10; k++) {
            const uint32_t r = results + 0x30 * k;
            st(g, r, hi); st(g, r + 0x10, ld(g, 0x00d61670)); st(g, r + 0x20, lo);
        }
    }
    clear_contact_ids(g, kSet);
    uint32_t count = 0;
    m.r[3] = world; m.r[6] = pos; m.r[7] = 0;
    f.call(g.u32(g.u32(shape) + 8), {kIdentity, pos, kBox, kQuery}, shape);
    if (!(g.u8(0x009e30e0) & 1)) { g.u32(0x009e30e0, g.u32(0x009e30e0) | 1); g.u32(0x009e30dc, 0); }
    m.r[2] = 0x009e30d4;
    const uint32_t water = f.call(0x00402dc0);
    g.u32(kA, g.u32(pos)); g.u32(kA + 4, g.u32(pos + 4)); g.u32(kA + 8, g.u32(pos + 8));
    if (water != 0 && f.call(0x007c5950, {kA}, water) != 0) {
        const X d = mulps(ld(g, pos), ld(g, 0x00d616a0));
        const float dot = dot3_xyz(d);
        f.call(0x008454c0, {}, -1, shape);
        const float r = xr(g, 0).f(0);
        const float depth = x86::sub(dot, r);
        if (!(g.f32(kA + 4) < depth)) {
            g.u32(types, 0x10);
            g.u32(ids, g.u32(water + 0xe4));
            count = 1;
        }
    }
    // copies the contact set's records into the results (at most 10 in all)
    auto take = [&](uint32_t type, uint32_t id) {
        const int32_t n = g.i32(kSet);
        if (g.u32(kSet + 0x6a0) == 0 || n <= 0) return;
        for (int32_t k = 0; k < n; k++) {
            if (count >= 10) continue;
            const uint32_t rec = kSet + 0x10 + 0x30 * uint32_t(k), out = results + 0x30 * count;
            g.u32(types + 4 * count, type);
            g.u32(ids + 4 * count, id);
            st(g, out, ld(g, rec)); st(g, out + 0x10, ld(g, rec + 0x10)); st(g, out + 0x20, ld(g, rec + 0x20));
            count++;
        }
    };
    auto query = [&](uint32_t cursor_off) {
        const uint32_t cursor = g.u32(world + cursor_off);
        m.r[3] = world; m.r[6] = g.u32(cursor + 0x30); m.r[7] = count; m.r[2] = kBox;
        f.call(0x00883020, {kQuery});
        g.u32(cursor, 0);
    };
    // static objects
    query(0x19cc);
    for (uint32_t idx = next_pair(g, g.u32(world + 0x19cc)); idx != 0xffffffffu; idx = next_pair(g, g.u32(world + 0x19cc))) {
        bounds_check(g, idx, uint32_t(int32_t(g.u32(world + 0x19c4) - g.u32(world + 0x19c0)) >> 6));
        const uint32_t S = g.u32(world + 0x19c0) + idx * 64;
        if (g.u8(S + 0x38) == 0) continue;
        st(g, kA, ld(g, S)); st(g, kB, ld(g, S + 0x10));
        reset_contact_set(g, kSet);
        m.r[3] = world; m.r[6] = S; m.r[7] = count; m.r[2] = pos;
        f.call(0x0085ddd0, {shape, kIdentity, pos, g.u32(S + 0x28), kB, kA, 0}, kSet, shape);
        take(1, g.u32(S + 0x30));
    }
    if (count != 10) {
        query(0x19ac);
        for (uint32_t idx = next_pair(g, g.u32(world + 0x19ac)); idx != 0xffffffffu; idx = next_pair(g, g.u32(world + 0x19ac))) {
            bounds_check(g, idx, uint32_t(int32_t(g.u32(world + 0x19a4) - g.u32(world + 0x19a0)) / int32_t(kObjSize)));
            const uint32_t K = g.u32(world + 0x19a0) + idx * kObjSize;
            if (g.u8(K + 0xb0) == 0) continue;
            st(g, kB, ld(g, K + 0x40)); st(g, kA, ld(g, K + 0x50));
            reset_contact_set(g, kSet);
            m.r[3] = world; m.r[6] = 0; m.r[7] = count; m.r[2] = pos;
            f.call(0x0085ddd0, {shape, kIdentity, pos, g.u32(K + 0x2c), kA, kB, 0}, kSet, shape);
            take(2, g.u32(K + 0x34));
        }
        if (count != 10) {
            query(0x196c);
            for (uint32_t idx = next_pair(g, g.u32(world + 0x196c)); idx != 0xffffffffu; idx = next_pair(g, g.u32(world + 0x196c))) {
                bounds_check(g, idx, obj_count(g, world));
                const uint32_t G = g.u32(world + 0x1960) + idx * kObjSize;
                st(g, kB, ld(g, G + 0x40)); st(g, kA, ld(g, G + 0x50));
                reset_contact_set(g, kSet);
                m.r[3] = world; m.r[6] = G; m.r[7] = count; m.r[2] = shape;
                f.call(0x0085ddd0, {shape, kIdentity, pos, g.u32(G + 0x2c), kA, kB, 0}, kSet, g.u32(G + 0x2c));
                take(4, g.u32(G + 0x34));
            }
            if (count != 10) {
                const uint32_t T = g.u32(world + 0xcc9c);
                if (T != 0 && g.u32(T + 0x31ec) != 0) {
                    reset_contact_set(g, kSet);
                    m.r[3] = T; m.r[7] = count; m.r[2] = kSet;
                    f.call(0x00667e40, {g.u32(T + 0x31ec), pos, kIdentity, sp + 0x10, 0, sp + 0x14}, shape);
                    take(8, g.u32(world + 0xcc9c));
                }
            }
        }
    }
    f.restore_saved();
    return m.ret(0x10, count);
}
#endif

namespace {
// The water body of the current player (00402dc0's search, inlined in places): the first non-null entry of the list (count at
// +8) whose +0x44a8 matches the active player's id ([0xa95844]: [[+4] * 4 + 0x1c]). Returns 0 if none; *ebx gets the last id
// compared, as the inlined copies leave it in EBX.
inline uint32_t find_player_water(Guest& g, uint32_t list, uint32_t& ebx) {
    for (uint32_t c = 0; c < g.u32(list + 8); c++) {
        const uint32_t e = g.u32(list + 4 * c);
        if (e == 0) continue;
        const uint32_t pl = g.u32(0x00a95844);
        ebx = g.u32(pl + g.u32(pl + 4) * 4 + 0x1c);
        if (ebx == g.u32(e + 0x44a8)) return e;
    }
    return 0;
}
}  // namespace

#ifdef WORLD_HAND_DEADPATH   // readable port; its contact paths never run in the test worlds: the literal one (world_phys_lit3.cpp) is used
// 0076c510: the generic dynamic objects in water (ESI = world, no stack arguments, plain ret). Objects with byte +0xd4: the
// narrow phase against the water shape world +0xccb0 (at the origin 0xd61670, rotation 0xd61640), the buoyancy per contact
// 0076dec0 (ECX = world), the push (min + max of depth * normal times 0.8, clamped to the material's +0xc0) added to +0xa0, and
// an event (other = the player's water body +0xe4). EBX is a function-wide register in the original that the per-contact loop
// and the water search leave nonzero; it then initializes the next object's contact set (count and flag) and is the narrow
// phase's last argument: reproduced.
REFLEX_PORT_FN(0x0076c510, DynObj_CollideWater, "world", 0) {
    Guest g{m};
    const uint32_t world = m.r[6];
    Frame f(m, 0x738, 2);
    const uint32_t sp = f.esp, kRot = sp + 0x30, kPos = sp + 0x50, kWaterRot = sp + 0x40, kWaterPos = sp + 0x70, kSet = sp + 0x80;
    st(g, kWaterPos, ld(g, 0x00d61670));
    st(g, kWaterRot, ld(g, 0x00d61640));
    uint32_t ebx = 0;
    clear_contact_ids(g, kSet);
    if (obj_count(g, world) != 0) {
        uint32_t i = 0, off = 0;
        do {
            bounds_check(g, i, obj_count(g, world));
            const uint32_t A = g.u32(world + 0x1960) + off;
            if (g.u8(A + 0xd4) != 0) {
                st(g, kRot, ld(g, A + 0x40));
                st(g, kPos, ld(g, A + 0x50));
                g.u32(kSet + 0x6a0, ebx); g.u32(kSet, ebx);
                clear_contact_ids(g, kSet);
                m.r[3] = ebx; m.r[6] = world; m.r[7] = A; m.r[2] = kWaterPos;
                f.call(0x0085ddd0, {world + 0xccb0, kWaterRot, kWaterPos, g.u32(A + 0x2c), kPos, kRot, ebx}, kSet, kWaterRot);
                if (g.u32(kSet + 0x6a0) != ebx) {
                    int32_t n = g.i32(kSet);
                    const X prevA = ld(g, A + 0x80);
                    if (n > 0) {
                        uint32_t p = kSet + 0x10;
                        do {
                            m.r[3] = ebx; m.r[6] = world; m.r[7] = A;
                            f.call(0x0076dec0, {p}, world, p);
                            n = g.i32(kSet);
                            p += 0x30; ebx++;
                        } while (int32_t(ebx) < n);
                    }
                    X hi = ld(g, 0x00d61670), lo = hi;
                    for (int32_t k = 0; k < n; k++) {
                        const uint32_t p = kSet + 0x10 + 0x30 * uint32_t(k);
                        const X d = mulps(ld(g, p + 0x20), ld(g, p + 0x10));
                        hi = maxps(hi, d); lo = minps(lo, d);
                    }
                    X push = mulps(addps(lo, hi), X::splat(g.u32(0x009cc8a8)));
                    const X len = splat_f(x86::sqrt(dot3_xzy(mulps(push, push))));
                    const X limit = ld(g, g.u32(A + 0x20) + 0xc0);
                    if (all3(cmpltps(limit, len))) push = mulps(divps(limit, len), push);
                    st(g, A + 0xa0, addps(push, ld(g, A + 0xa0)));
                    const X dm = subps(ld(g, A + 0x80), prevA);
                    const float moved = x86::sqrt(dot3_xzy(mulps(dm, dm)));
                    const uint32_t events = g.u32(world + 0x1900);
                    if (events < 100) {
                        g.u32(world + 0x1900, events + 1);
                        const uint32_t ev = world + events * 64;
                        g.u32(ev, g.u32(A + 0x34));
                        const uint32_t water = find_player_water(g, g.u32(world + 0xccd0), ebx);
                        g.u32(ev + 4, g.u32(water + 0xe4));
                        g.u32(ev + 0x2c, 0);
                        g.u8(ev + 0x20, 0);
                        g.u32(ev + 0x30, g.u32(kSet + 0x5b0));
                        g.u32(ev + 0x34, g.u32(kSet + 0x628));
                        g.u32(ev + 0x10, g.u32(kSet + 0x10)); g.u32(ev + 0x14, g.u32(kSet + 0x14)); g.u32(ev + 0x18, g.u32(kSet + 0x18));
                        g.f32(ev + 0x24, moved); g.u32(ev + 0x28, 0);
                    }
                }
            }
            i++; off += kObjSize;
        } while (i < obj_count(g, world));
    }
    f.restore_saved();
    return m.ret(0);
}
#endif

#ifdef WORLD_HAND_DEADPATH   // readable port; its contact paths never run in the test worlds: the literal one (world_phys_lit3.cpp) is used
// 0076d9a0: the kinematic bodies against the terrain (ESI = world, plain ret). Bodies: world +0x19d8[k] for k < +0x19d4, with
// their pose at world +0x1d00 + 0x50 k (position +0x10); enabled by byte +0x84 bit 0. As 0076d3c0: 00667e40 into two contact
// sets, an event for the second (type 1, speed = |pose position|), and for the first the per-contact response 0076f2f0
// (ECX = body), the push-out (min + max of depth * normal times -0.8, clamped) added to the pose +0 (copied to body +0x40) and
// an event (type 0).
REFLEX_PORT_FN(0x0076d9a0, Kinematic_CollideTerrain, "world", 0) {
    Guest g{m};
    const uint32_t world = m.r[6];
    Frame f(m, 0xdb8, 2);
    const uint32_t sp = f.esp, set1 = sp + 0x50, set2 = sp + 0x700, kOut28 = sp + 0x28, kOutC = sp + 0xc;
    if (g.u32(world + 0xcc9c) != 0) {
        clear_contact_ids(g, set1); clear_contact_ids(g, set2);
        for (int32_t k = 0; k < g.i32(world + 0x19d4); k++) {
            const uint32_t O = g.u32(world + 0x19d8 + 4 * uint32_t(k));
            const uint32_t kp = world + 0x1d10 + 0x50 * uint32_t(k);
            if (!(g.u8(O + 0x84) & 1)) continue;
            reset_contact_set(g, set1); reset_contact_set(g, set2);
            const uint32_t shape = g.u32(O + 0x2c);
            g.u32(kOut28, 0); g.u32(kOutC, 0);
            if (!((g.u32(shape + 0x14) >> 5) & 1)) {
                m.r[3] = 0; m.r[6] = world; m.r[7] = O; m.r[2] = set1;
                f.call(0x00667e40, {g.u32(g.u32(world + 0xcc9c) + 0x31ec), O + 0x40, O + 0x50, kOut28, set2, kOutC}, shape);
            }
            if (g.u32(set2 + 0x6a0) != 0) {
                const X v = ld(g, kp);
                const float speed = x86::sqrt(dot3_xzy(mulps(v, v)));
                const uint32_t events = g.u32(world + 0x1900);
                if (events < 100) {
                    g.u32(world + 0x1900, events + 1);
                    const uint32_t ev = world + events * 64;
                    g.u32(ev, g.u32(O + 0x34));
                    g.u32(ev + 4, g.u32(world + 0xcc9c));
                    g.u32(ev + 0x2c, g.u32(kOutC));
                    g.u8(ev + 0x20, 1);
                    g.u32(ev + 0x30, g.u32(set2 + 0x5b0));
                    g.u32(ev + 0x34, g.u32(set2 + 0x628));
                    g.u32(ev + 0x10, g.u32(set2 + 0x10)); g.u32(ev + 0x14, g.u32(set2 + 0x14)); g.u32(ev + 0x18, g.u32(set2 + 0x18));
                    g.f32(ev + 0x24, speed); g.f32(ev + 0x28, speed);
                }
            }
            if (g.u32(set1 + 0x6a0) != 0) {
                const X prev = ld(g, kp);
                int32_t n = g.i32(set1);
                if (n > 0) {
                    uint32_t c = 0, p = set1 + 0x10;
                    do {
                        m.r[3] = c; m.r[6] = world; m.r[7] = O;
                        f.call(0x0076f2f0, {world, p}, O);
                        n = g.i32(set1);
                        p += 0x30; c++;
                    } while (int32_t(c) < n);
                }
                const X scale = X::splat(g.u32(0x009cc8a8));
                X lo = ld(g, 0x00d61670), hi = lo;
                for (int32_t j = 0; j < n; j++) {
                    const uint32_t p = set1 + 0x10 + 0x30 * uint32_t(j);
                    const X d = mulps(ld(g, p + 0x20), ld(g, p + 0x10));
                    hi = maxps(hi, d); lo = minps(lo, d);
                }
                X push = mulps(addps(lo, hi), subps(X::splat(0), scale));
                const X len = splat_f(x86::sqrt(dot3_xzy(mulps(push, push))));
                const X limit = ld(g, g.u32(O + 0x20) + 0xc0);
                if (all3(cmpltps(limit, len))) push = mulps(divps(limit, len), push);
                st(g, kp - 0x10, addps(ld(g, kp - 0x10), push));
                st(g, O + 0x40, ld(g, kp - 0x10));
                const X dm = subps(ld(g, kp), prev);
                const float moved = x86::sqrt(dot3_xzy(mulps(dm, dm)));
                const uint32_t events = g.u32(world + 0x1900);
                if (events < 100) {
                    g.u32(world + 0x1900, events + 1);
                    const uint32_t ev = world + events * 64;
                    g.u32(ev, g.u32(O + 0x34));
                    g.u32(ev + 4, g.u32(world + 0xcc9c));
                    g.u32(ev + 0x2c, g.u32(kOut28));
                    g.u8(ev + 0x20, 0);
                    g.u32(ev + 0x30, g.u32(set1 + 0x5b0));
                    g.u32(ev + 0x34, g.u32(set1 + 0x628));
                    g.u32(ev + 0x10, g.u32(set1 + 0x10)); g.u32(ev + 0x14, g.u32(set1 + 0x14)); g.u32(ev + 0x18, g.u32(set1 + 0x18));
                    g.f32(ev + 0x24, moved); g.f32(ev + 0x28, moved);
                }
            }
        }
    }
    f.restore_saved();
    return m.ret(0);
}
#endif

namespace {
// A float argument passed on through the x87 stack (fld dword / fstp dword): bit-exact except that a signalling NaN comes out quiet.
inline uint32_t x87_pass(uint32_t b) { return x86::isnan_bits(b) ? (b | 0x400000u) : b; }
inline void vcall_each_listener(Guest& g, Frame& f, uint32_t sp, uint32_t world, uint32_t slot, uint32_t dt_bits,
                                uint32_t ebx, int reg_index) {
    // world +0x1914: listener pointers, count +0x1918 (re-read every iteration); vfunc `slot` (thiscall, float dt)
    for (int32_t i = 0; i < g.i32(world + 0x1918); i++) {
        const uint32_t l = g.u32(g.u32(world + 0x1914) + 4 * uint32_t(i));
        g.m.r[3] = ebx; g.m.r[uint32_t(reg_index)] = uint32_t(i);
        f.call_at(sp, g.u32(g.u32(l) + slot), {x87_pass(dt_bits)}, l);
    }
}
}  // namespace

// 007699e0: the physics pre-listeners (EDI = world, float dt on the stack, ret 4): listener vfunc 0 with dt, the contact event
// count world +0x1900 reset, 200 contact-sound timers (world +0xb300, 40 x 5 floats 0x20 apart) advanced by dt while below 1,
// then 00768750 (EAX = world +0xcc00, stack world).
REFLEX_PORT_FN(0x007699e0, World_PreListeners, "world", 0) {
    Guest g{m};
    const uint32_t world = m.r[7], dt = m.arg(0);
    Frame f = Frame::flat(m, 2, 0);
    vcall_each_listener(g, f, f.esp, world, 0, dt, m.r[3], 6);
    const float fdt = x86::flt(dt), one = g.f32(0x00931f24);
    g.u32(world + 0x1900, 0);
    for (uint32_t j = 0; j < 40; j++)
        for (uint32_t k = 0; k < 5; k++) {
            const uint32_t a = world + 0xb300 + 0xa0 * j + 0x20 * k;
            const float t = g.f32(a);
            if (one > t) g.f32(a, x86::add(t, fdt));
        }
    m.r[6] = uint32_t(g.i32(world + 0x1918) > 0 ? g.i32(world + 0x1918) : 0); m.r[7] = world; m.r[1] = 0;   // ESI = listener count, ECX = 0
    f.call(0x00768750, {world}, -1, world + 0xcc00);
    f.restore_saved();
    return m.ret(4);
}

// 00769ab0: the physics post-listeners (ESI = world, float dt, ret 4): listener vfunc 1 with dt; when contact events were
// recorded (world +0x1900), every contact handler (world +0x1934 vector) gets vfunc 0 (world); then the kinematic bodies'
// pose +0x20 vectors (world +0x1d20, 0x50 apart, +0x19d4 of them) are cleared to 0xd61670.
REFLEX_PORT_FN(0x00769ab0, World_PostListeners, "world", 0) {
    Guest g{m};
    const uint32_t world = m.r[6], dt = m.arg(0);
    Frame f(m, 8, 2);
    vcall_each_listener(g, f, f.esp, world, 4, dt, m.r[3], 7);
    if (g.u32(world + 0x1900) != 0) {
        auto n = [&] { return uint32_t(int32_t(g.u32(world + 0x1938) - g.u32(world + 0x1934)) >> 2); };
        for (uint32_t i = 0; i < n(); i++) {
            if (i >= n()) g.call(g.u32(kAbortInvalidParam));
            const uint32_t h = g.u32(g.u32(world + 0x1934) + 4 * i);
            m.r[3] = g.u32(kAbortInvalidParam); m.r[7] = i; m.r[6] = world;
            f.call(g.u32(g.u32(h)), {world}, h);
        }
    }
    for (int32_t k = 0; k < g.i32(world + 0x19d4); k++) st(g, world + 0x1d20 + 0x50 * uint32_t(k), ld(g, 0x00d61670));
    f.restore_saved();
    return m.ret(4);
}

// 0076a010: the solver dispatch (EAX = world, float dt, ret 4): collision preparation and the object collisions (0076ac40,
// 0076b070, 0076b4a0, 0076b9b0, 0076bf60, 0076c510, 0076cd00, 0076c880, 0076d3c0, 0076d9a0), slot 1 of every simulation
// (world +0x1940, count +0x1944) with dt, 0076a1e0, then four quarter steps of the generic integration 0076a360 (ECX = world,
// xmm0 = dt / 4; the quartered dt is written back into the argument slot as the original does) with 0076ac40 between them.
// The worker-thread split (flag 0xa946c4) is not ported: with the flag set the original runs (it is clear in every world).
REFLEX_PORT_FN(0x0076a010, World_SolverDispatch, "world", 0) {
    Guest g{m};
    if (g.u32(0x00a946c4) != 0) return reflex_vm::RUN_ORIGINAL;
    const uint32_t world = m.r[0], dt = m.arg(0), dt_slot = m.r[4] + 4;
    Frame f = Frame::flat(m, 4, 0);
    m.r[6] = world;
    f.call(0x0076ac40, {world});
    m.r[7] = world;
    f.call(0x0076b070);
    f.call(0x0076b4a0, {world});
    f.call(0x0076b9b0, {world});
    f.call(0x0076bf60, {world});
    f.call(0x0076c510);
    f.call(0x0076cd00);
    f.call(0x0076c880);
    f.call(0x0076d3c0, {world});
    f.call(0x0076d9a0);
    for (int32_t i = 0; i < g.i32(world + 0x1944); i++) {
        const uint32_t sim = g.u32(g.u32(world + 0x1940) + 4 * uint32_t(i));
        m.r[7] = uint32_t(i);
        f.call(g.u32(g.u32(sim) + 4), {x87_pass(dt)}, sim);
    }
    f.call(0x0076a1e0);
    const float quarter = x86::mul(x86::flt(dt), g.f32(0x0092ff84));
    g.f32(dt_slot, quarter);
    xw(g, 0, X{{x86::bits(quarter), 0, 0, 0}});   // movss xmm0, m32 / mulss: upper lanes zero
    f.call(0x0076a360, {}, world);
    for (int k = 3; k > 0; k--) {
        m.r[7] = uint32_t(k);
        f.call(0x0076ac40, {world});
        xw(g, 0, X{{g.u32(dt_slot), 0, 0, 0}});
        f.call(0x0076a360, {}, world);
    }
    f.restore_saved();
    return m.ret(4);
}

// 00769b80 World_PhysicsUpdate (EAX = world, float dt, ret 4): pre-listeners, the simulation clock (0042ef70, cdecl), the
// solver dispatch, post-listeners.
REFLEX_PORT_FN(0x00769b80, World_PhysicsUpdate, "world", 0) {
    const uint32_t world = m.r[0], dt = m.arg(0);
    Frame f(m, 8, 2);
    m.r[6] = world; m.r[7] = world;
    f.call(0x007699e0, {x87_pass(dt)});
    f.call(0x0042ef70, {x87_pass(dt)});
    f.call(0x0076a010, {x87_pass(dt)}, -1, world);
    f.call(0x00769ab0, {x87_pass(dt)});
    f.restore_saved();
    return m.ret(4);
}

namespace {
// The normalize-or-NaN pattern of the exe: v / |v| (|v|^2 summed (x + y) + z), with 0x92dc30 where |v|^2 is inf and 0 where
// |v| is 0.
inline X normalize_xyz(const Guest& g, const X& v) {
    const float len2 = dot3_xyz(mulps(v, v));
    const X l2 = splat_f(len2), len = sqrtps(l2);
    X r = divps(v, len);
    const X finite = cmpneqps(l2, ld(g, 0x0092dc20));
    r = andps(andps(r, cmpneqps(len, X::splat(0))), finite);
    return orps(r, andnps(finite, ld(g, 0x0092dc30)));
}
}  // namespace

// 0076ac40: the kinematic bodies in water (stdcall, arg 0 = world, ret 4). Per body k (enabled by +0x84 bit 0) with its water
// state at world +0x9a00 + 0x20 k (+0 touching, +0x10 push direction, +0x1900 a float, +0x1910 the impulse): the narrow phase
// against the water shape world +0xccb0, the impulse 0085dcf0 (a vertical-speed window -3 < v.impulse < 3 zeroes +0x1900), the
// per-contact buoyancy 0076dd50 (ECX = body), the push (min + max of depth * normal times 0.8, clamped, with the locked axes of
// +0xb4 zeroed) added to the pose and copied to body +0x40, its direction, and an event (other = the player's water +0xe4).
REFLEX_PORT_FN(0x0076ac40, Kinematic_CollideWater, "world", 0) {
    Guest g{m};
    const uint32_t world = m.arg(0);
    Frame f(m, 0x784, 3);
    const uint32_t sp = f.esp, kRot = sp + 0x50, kPos = sp + 0x90, kWaterRot = sp + 0x60, kWaterPos = sp + 0x80, kSet = sp + 0xd0;
    const uint32_t kImpulse = sp + 0xa0;
    st(g, kWaterPos, ld(g, 0x00d61670));
    st(g, kWaterRot, ld(g, 0x00d61640));
    clear_contact_ids(g, kSet);
    for (int32_t k = 0; k < g.i32(world + 0x19d4); k++) {
        const uint32_t O = g.u32(world + 0x19d8 + 4 * uint32_t(k));
        const uint32_t kp = world + 0x1d10 + 0x50 * uint32_t(k), wb = world + 0x9a00 + 0x20 * uint32_t(k);
        st(g, kRot, ld(g, O + 0x40));
        st(g, kPos, ld(g, O + 0x50));
        g.u32(wb, 0);
        if (!(g.u8(O + 0x84) & 1)) continue;
        reset_contact_set(g, kSet);
        m.r[3] = world; m.r[6] = 0; m.r[7] = O; m.r[2] = kWaterPos;
        f.call(0x0085ddd0, {world + 0xccb0, kWaterRot, kWaterPos, g.u32(O + 0x2c), kPos, kRot, 0}, kSet, kWaterRot);
        if (g.u32(kSet + 0x6a0) == 0) continue;
        const X prev = ld(g, kp);
        m.r[6] = kSet; m.r[2] = kp;
        f.call(0x0085dcf0, {kImpulse}, -1, kImpulse);
        const X imp = ld(g, kImpulse + 0x10);
        const float v = dot3_xyz(mulps(ld(g, kp), imp));
        if (v > g.f32(0x009cc380) && g.f32(0x0092fe48) > v) g.u32(wb + 0x1900, 0);
        st(g, wb + 0x1910, imp);
        int32_t n = g.i32(kSet);
        if (n > 0) {
            uint32_t c = 0, p = kSet + 0x10;
            do {
                m.r[3] = p; m.r[6] = c; m.r[7] = O;
                f.call(0x0076dd50, {world, p}, O);
                n = g.i32(kSet);
                c++; p += 0x30;
            } while (int32_t(c) < n);
        }
        const X z = ld(g, 0x00d61670);
        X hi = z, lo = z;
        for (int32_t j = 0; j < n; j++) {
            const uint32_t p = kSet + 0x10 + 0x30 * uint32_t(j);
            const X d = mulps(ld(g, p + 0x20), ld(g, p + 0x10));
            hi = maxps(hi, d); lo = minps(lo, d);
        }
        X push = mulps(addps(lo, hi), X::splat(g.u32(0x009cc8a8)));
        const X len = splat_f(x86::sqrt(dot3_xzy(mulps(push, push))));
        const X limit = ld(g, g.u32(O + 0x20) + 0xc0);
        if (all3(cmpltps(limit, len))) push = mulps(divps(limit, len), push);
        const uint32_t lock = g.u32(O + 0xb4);
        if (lock != 0) {
            if (lock & 1) push.u[0] = z.u[0];
            if (lock & 2) push.u[1] = z.u[1];
            if (lock & 4) { push.u[2] = z.u[2]; push.u[3] = z.u[2]; }
        }
        st(g, kp - 0x10, addps(ld(g, kp - 0x10), push));
        st(g, O + 0x40, ld(g, kp - 0x10));
        g.u32(wb, 1);
        st(g, wb + 0x10, normalize_xyz(g, push));
        const X dm = subps(ld(g, kp), prev);
        const float moved = x86::sqrt(dot3_xzy(mulps(dm, dm)));
        const uint32_t events = g.u32(world + 0x1900);
        if (events < 100) {
            g.u32(world + 0x1900, events + 1);
            const uint32_t ev = world + events * 64;
            g.u32(ev, g.u32(O + 0x34));
            uint32_t unused = 0;
            const uint32_t water = find_player_water(g, g.u32(world + 0xccd0), unused);
            g.u32(ev + 4, g.u32(water + 0xe4));
            g.u32(ev + 0x2c, 0);
            g.u8(ev + 0x20, 0);
            g.u32(ev + 0x30, g.u32(kSet + 0x5b0));
            g.u32(ev + 0x34, g.u32(kSet + 0x628));
            g.u32(ev + 0x10, g.u32(kSet + 0x10)); g.u32(ev + 0x14, g.u32(kSet + 0x14)); g.u32(ev + 0x18, g.u32(kSet + 0x18));
            g.f32(ev + 0x24, moved); g.u32(ev + 0x28, 0);
        }
    }
    f.restore_saved();
    return m.ret(4);
}

namespace {
// Quaternion normalization as 0076a360 computes it: |q|^2 = (q3^2 + q1^2) + (q2^2 + q0^2) through two shuffles, then the
// normalize-or-NaN pattern.
inline X normalize_quat(const Guest& g, const X& q) {
    const X sq = mulps(q, q);
    X x1 = shufps(sq, sq, 0xee);
    X x2 = addps(x1, sq);
    x2 = shufps(x2, x2, 0x40);
    x1 = shufps(x1, x2, 0xf0);
    x1 = addps(x1, x2);
    x1 = shufps(x1, x1, 0xaa);
    const X len = sqrtps(x1);
    X r = divps(q, len);
    const X finite = cmpneqps(x1, ld(g, 0x0092dc20));
    r = andps(andps(r, cmpneqps(len, X::splat(0))), finite);
    return orps(r, andnps(finite, ld(g, 0x0092dc30)));
}
// The keyed-object index of O in world +0x19a0 (vector header at world +0x1994: begin +0xc, end +0x10), as passed to 0042b000.
inline uint32_t keyed_index(Guest& g, uint32_t world, uint32_t O) {
    const uint32_t hdr = world + 0x1994;
    if (int32_t(g.u32(hdr + 0x10) - g.u32(hdr + 0xc)) / int32_t(kObjSize) == 0) g.call(g.u32(kAbortInvalidParam));
    return uint32_t(int32_t(O - g.u32(hdr + 0xc)) / int32_t(kObjSize));
}
}  // namespace

// 0076a360: the kinematic bodies' integration over one quarter step (ECX = world, xmm0 = dt; plain ret). Substeps
// n = clamp(1 - trunc(dt / -0.001), 1, 10) of h = dt / n. Per active body (+0x84 bit 1): buoyancy prep 0084f700 (when +0xd0),
// 00769e40 / 00769d40, then per substep the accelerations of 00769bd0 (locked axes +0xb4 zeroed) integrate the pose at
// world +0x1d00 + 0x50 k (position, velocity) and world +0x5b80 + 0x50 k (orientation, angular velocity); then the water
// state 0084fe00 (when 0xda67c8), the orientation normalized, the pose copied to +0x40 / +0x50 and sent to the render object
// (vfunc 0x78), 0042b000, and the sleep test against the material (+0x90 / +0xa0: state 0x27, else 0x23 with timer +0xa0 from
// +0x80). Timers +0xa0 (state bit 2, to 0x28) and +0x90 (bit 3, to 0x30) count down by dt. A second pass, last body first,
// resets bodies with +0x84 bit 4 (0076a8b0, pose from +0x60 / +0x70, lock mask from +0xb8, flag +0xd1).
REFLEX_PORT_FN(0x0076a360, Kinematic_Integrate, "world", 0) {
    Guest g{m};
    const uint32_t world = m.r[1];
    const X dt_in = xr(g, 0);
    Frame f(m, 0xa4, 3);
    const uint32_t sp = f.esp, kAcc = sp + 0x60, kAngAcc = sp + 0x70, kVel = sp + 0x80, kAngVel = sp + 0x90;
    const float dt = dt_in.f(0);
    const uint32_t q = uint32_t(x86::cvtt(x86::div(dt, g.f32(0x00977ba8))));
    const int32_t steps0 = int32_t(1u - q);
    const int32_t steps = steps0 < 1 ? 1 : (steps0 > 10 ? 10 : steps0);
    const float h = x86::div(dt, float(steps));
    const X dt4 = splat_f(dt), h4 = splat_f(h);
    for (int32_t k = 0; k < g.i32(world + 0x19d4); k++) {
        const uint32_t O = g.u32(world + 0x19d8 + 4 * uint32_t(k));
        const uint32_t pos = world + 0x1d00 + 0x50 * uint32_t(k), rot = world + 0x5b80 + 0x50 * uint32_t(k);
        const uint32_t wb = world + 0x9a00 + 0x20 * uint32_t(k);
        const uint32_t mat = g.u32(O + 0x20);
        if (g.u8(O + 0x84) & 2) {
            if (g.u8(O + 0xd0) != 0) { m.r[3] = rot; m.r[6] = rot; m.r[7] = O; f.call(0x0084f700, {O}, world); }
            m.r[3] = rot; m.r[6] = O; m.r[7] = O;
            f.call(0x00769e40, {uint32_t(k)}, world, uint32_t(k));
            f.call(0x00769d40, {uint32_t(k)}, world);
            for (int32_t s = steps; s > 0; s--) {
                m.r[3] = rot; m.r[6] = kAngVel; m.r[7] = O; m.r[2] = kAcc;
                f.call(0x00769bd0, {world, mat, kVel, kAngAcc}, uint32_t(k), kVel);
                X acc = ld(g, kAcc);
                const uint32_t lock = g.u32(O + 0xb4);
                if (lock != 0) {
                    const X z = ld(g, 0x00d61670);
                    if (lock & 1) { acc.u[0] = z.u[0]; st(g, kAcc, acc); }
                    if (lock & 2) { acc.u[1] = z.u[1]; st(g, kAcc, acc); }
                    if (lock & 4) { acc.u[2] = z.u[2]; acc.u[3] = z.u[2]; st(g, kAcc, acc); }
                }
                st(g, pos + 0x10, addps(mulps(acc, h4), ld(g, pos + 0x10)));
                st(g, pos, addps(mulps(ld(g, kVel), h4), ld(g, pos)));
                st(g, rot + 0x10, addps(mulps(ld(g, kAngAcc), h4), ld(g, rot + 0x10)));
                st(g, rot, addps(mulps(ld(g, kAngVel), h4), ld(g, rot)));
            }
            if (g.u32(0x00da67c8) != 0) { m.r[3] = rot; m.r[7] = O; f.call(0x0084fe00, {pos + 0x10}, wb, pos + 0x10); }
            st(g, rot, normalize_quat(g, ld(g, rot)));
            st(g, O + 0x40, ld(g, pos));
            st(g, O + 0x50, ld(g, rot));
            {
                const uint32_t ro = g.u32(O + 0x34);
                m.r[3] = rot; m.r[6] = g.u32(ro); m.r[7] = O; m.r[2] = O + 0x50;
                f.call(g.u32(g.u32(ro) + 0x78), {O + 0x40, O + 0x50}, ro, O + 0x40);
            }
            m.r[6] = world + 0x1994;
            const uint32_t idx = keyed_index(g, world, O);
            m.r[3] = rot; m.r[7] = O;
            f.call(0x0042b000, {}, idx);
            const X v = ld(g, pos + 0x10), w = ld(g, rot + 0x10);
            const float lin = dot3_xyz(mulps(v, v)), ang = dot3_xyz(mulps(w, w));
            if (all3(cmpltps(splat_f(lin), ld(g, mat + 0x90))) && all3(cmpltps(splat_f(ang), ld(g, mat + 0xa0))))
                g.u32(O + 0x84, 0x27);
            else { g.u32(O + 0x84, 0x23); st(g, O + 0xa0, ld(g, mat + 0x80)); }
        }
        if (g.u8(O + 0x84) & 4) {
            st(g, O + 0xa0, subps(ld(g, O + 0xa0), dt4));
            if (all3(cmpltps(ld(g, O + 0xa0), ld(g, 0x00d61670))) && g.u8(O + 0xb1) != 0) {
                st(g, O + 0x90, ld(g, mat + 0x70));
                g.u32(O + 0x84, 0x28);
            }
        }
        if (g.u8(O + 0x84) & 8) {
            st(g, O + 0x90, subps(ld(g, O + 0x90), dt4));
            if (all3(cmpltps(ld(g, O + 0x90), ld(g, 0x00d61670))) && g.u8(O + 0xb2) != 0) g.u32(O + 0x84, 0x30);
        }
    }
    const int32_t count = g.i32(world + 0x19d4);
    for (int32_t j = count - 1; j > -1; j--) {
        const uint32_t O = g.u32(world + 0x19d8 + 4 * uint32_t(j));
        if (!(g.u8(O + 0x84) & 0x10)) continue;
        m.r[3] = world + 0x19d8 + 4 * uint32_t(j); m.r[7] = O; m.r[2] = O;
        f.call(0x0076a8b0, {world}, world);
        st(g, O + 0x40, ld(g, O + 0x60));
        st(g, O + 0x50, ld(g, O + 0x70));
        {
            const uint32_t ro = g.u32(O + 0x34);
            m.r[6] = g.u32(ro); m.r[2] = O + 0x50;
            f.call(g.u32(g.u32(ro) + 0x78), {O + 0x40, O + 0x50}, ro, O + 0x40);
        }
        g.u32(O + 0xb4, g.u32(O + 0xb8));
        g.u8(O + 0xd0, g.u8(O + 0xd1));
        m.r[6] = world + 0x1994;
        const uint32_t idx = keyed_index(g, world, O);
        f.call(0x0042b000, {}, idx);
    }
    f.restore_saved();
    return m.ret(0);
}

// 0076b070: the kinematic bodies against the static objects (EDI = world, plain ret). Per enabled body (+0x84 bit 0; it is
// keyed object idx of world +0x19a0, whose broad-phase box is read from the keyed grid's +0x41c / +0x420 arrays): the static
// broad phase 00883020, then per static S (byte +0x38) the narrow phase 0085ddd0, per contact 0076ee60 (ECX = body, EDX = S),
// the push (min + max of depth * normal, times -scale, clamped; the clamp ratio becomes the scale of later pushes, as in
// 0076b4a0), through 0084fe00 when 0xda67cc == 1, added to the pose (copied to +0x40) and an event (other = S +0x30).
REFLEX_PORT_FN(0x0076b070, Kinematic_CollideStatic, "world", 0) {
    Guest g{m};
    const uint32_t world = m.r[7];
    Frame f(m, 0x788, 2);
    const uint32_t sp = f.esp, kRot = sp + 0x70, kPos = sp + 0x90, kBox = sp + 0x80, kQuery = sp + 0xa0;
    const uint32_t kRotS = sp + 0xc0, kPosS = sp + 0x60, kSet = sp + 0xd0, kPush = sp + 0x40;
    X scale = X::splat(g.u32(0x009cc8a8));
    clear_contact_ids(g, kSet);
    for (int32_t k = 0; k < g.i32(world + 0x19d4); k++) {
        const uint32_t O = g.u32(world + 0x19d8 + 4 * uint32_t(k));
        const uint32_t pose = world + 0x1d00 + 0x50 * uint32_t(k), wb = world + 0x9a00 + 0x20 * uint32_t(k);
        st(g, kRot, ld(g, O + 0x40));
        st(g, kPos, ld(g, O + 0x50));
        if (!(g.u8(O + 0x84) & 1)) continue;
        if (int32_t(g.u32(world + 0x19a4) - g.u32(world + 0x19a0)) / int32_t(kObjSize) == 0) g.call(g.u32(kAbortInvalidParam));
        const uint32_t idx = uint32_t(int32_t(O - g.u32(world + 0x19a0)) / int32_t(kObjSize));
        const uint32_t cursor = g.u32(world + 0x19cc);
        const uint32_t kgrid = g.u32(g.u32(world + 0x19ac) + 0x30);
        st(g, kBox, ld(g, g.u32(kgrid + 0x41c) + idx * 16));
        st(g, kQuery, ld(g, g.u32(kgrid + 0x420) + idx * 16));
        m.r[3] = cursor; m.r[6] = g.u32(cursor + 0x30); m.r[2] = kBox;
        f.call(0x00883020, {kQuery}, g.u32(kgrid + 0x420), idx * 16);
        g.u32(cursor, 0);
        for (uint32_t si = next_pair(g, g.u32(world + 0x19cc)); si != 0xffffffffu; si = next_pair(g, g.u32(world + 0x19cc))) {
            bounds_check(g, si, uint32_t(int32_t(g.u32(world + 0x19c4) - g.u32(world + 0x19c0)) >> 6));
            const uint32_t S = g.u32(world + 0x19c0) + si * 64;
            if (g.u8(S + 0x38) == 0) continue;
            st(g, kRotS, ld(g, S)); st(g, kPosS, ld(g, S + 0x10));
            reset_contact_set(g, kSet);
            m.r[3] = 0; m.r[6] = S; m.r[2] = kPos;
            f.call(0x0085ddd0, {g.u32(O + 0x2c), kPos, kRot, g.u32(S + 0x28), kPosS, kRotS, 0}, kSet, O);
            if (g.u32(kSet + 0x6a0) == 0) continue;
            const X prevV = ld(g, pose + 0x10);
            int32_t n = g.i32(kSet);
            if (n > 0) {
                uint32_t c = 0, p = kSet + 0x10;
                do {
                    m.r[3] = p; m.r[6] = c; m.r[2] = S;
                    f.call(0x0076ee60, {world, p}, O);
                    n = g.i32(kSet);
                    c++; p += 0x30;
                } while (int32_t(c) < n);
            }
            X lo = ld(g, 0x00d61670), hi = lo;
            for (int32_t j = 0; j < n; j++) {
                const uint32_t p = kSet + 0x10 + 0x30 * uint32_t(j);
                const X d = mulps(ld(g, p + 0x20), ld(g, p + 0x10));
                hi = maxps(hi, d); lo = minps(lo, d);
            }
            X push = mulps(addps(lo, hi), subps(X::splat(0), scale));
            const X len = splat_f(x86::sqrt(dot3_xzy(mulps(push, push))));
            const X limit = ld(g, g.u32(O + 0x20) + 0xc0);
            if (all3(cmpltps(limit, len))) { const X ratio = divps(limit, len); push = mulps(push, ratio); scale = ratio; }
            st(g, kPush, push);
            if (g.u32(0x00da67cc) == 1) {
                m.r[3] = pose; m.r[6] = O; m.r[2] = kPush;
                f.call(0x0084fe00, {kPush}, wb);
                push = ld(g, kPush);
            }
            st(g, pose, addps(ld(g, pose), push));
            st(g, O + 0x40, ld(g, pose));
            const X dm = subps(ld(g, pose + 0x10), prevV);
            const float moved = x86::sqrt(dot3_xzy(mulps(dm, dm)));
            const uint32_t events = g.u32(world + 0x1900);
            if (events < 100) {
                g.u32(world + 0x1900, events + 1);
                const uint32_t ev = world + events * 64;
                g.u32(ev, g.u32(O + 0x34));
                g.u32(ev + 4, g.u32(S + 0x30));
                g.u32(ev + 0x2c, 0);
                g.u8(ev + 0x20, 0);
                g.u32(ev + 0x30, g.u32(kSet + 0x5b0));
                g.u32(ev + 0x34, g.u32(kSet + 0x628));
                g.u32(ev + 0x10, g.u32(kSet + 0x10)); g.u32(ev + 0x14, g.u32(kSet + 0x14)); g.u32(ev + 0x18, g.u32(kSet + 0x18));
                g.f32(ev + 0x24, moved); g.u32(ev + 0x28, 0);
            }
        }
    }
    f.restore_saved();
    return m.ret(0);
}

namespace {
// The axis lock of the kinematic code: lanes of v replaced by the zero vector z's (mask bit 0: x, bit 1: y, bit 2: lanes 2 and 3
// both from z lane 2, as shufps v, z, 0xa4 leaves them).
[[maybe_unused]] inline X apply_lock(X v, uint32_t lock, const X& z) {
    if (lock == 0) return v;
    if (lock & 1) v.u[0] = z.u[0];
    if (lock & 2) v.u[1] = z.u[1];
    if (lock & 4) { v.u[2] = z.u[2]; v.u[3] = z.u[2]; }
    return v;
}
}  // namespace

#ifdef WORLD_HAND_DEADPATH   // readable port; its contact paths never run in the test worlds: the literal one (world_phys_lit3.cpp) is used
// 0076cd00: the kinematic bodies against the keyed objects (EDI = world, plain ret). Per enabled body O (it is keyed object
// myidx; box from the keyed grid +0x41c / +0x420): the keyed broad phase, then per other enabled keyed object K (byte +0xb0):
// the narrow phase, per contact 0076ea00 (ECX = contact, EDX = K), the push (min + max of depth * normal times -scale; 0084fe00
// when 0xda67d4 == 1; clamped with the ratio kept as the scale), then: both kinematic (O +0x80 >= 0, K +0x80 >= 0 and K +0x84 bit
// 0): half the push to each pose, each with its own axis lock; only O kinematic: the whole push to O's pose; only K: K's share
// from 00769f40 (and, as in the original, K's pose copied into O +0x40). An event records the two displacements as the original
// computes them (K's new pose velocity minus O's previous and minus K's previous). A non-kinematic O stops after its first pair.
REFLEX_PORT_FN(0x0076cd00, Kinematic_CollideKeyed, "world", 0) {
    Guest g{m};
    const uint32_t world = m.r[7];
    Frame f(m, 0x7a8, 2);
    const uint32_t sp = f.esp, kRot = sp + 0x90, kPos = sp + 0xb0, kBox = sp + 0xa0, kQuery = sp + 0xc0;
    const uint32_t kRotK = sp + 0xe0, kPosK = sp + 0x80, kSet = sp + 0xf0, kPush = sp + 0x40;
    X scale = X::splat(g.u32(0x009cc8a8));
    clear_contact_ids(g, kSet);
    auto nkeyed = [&] { return int32_t(g.u32(world + 0x19a4) - g.u32(world + 0x19a0)) / int32_t(kObjSize); };
    for (int32_t k = 0; k < g.i32(world + 0x19d4); k++) {
        const uint32_t O = g.u32(world + 0x19d8 + 4 * uint32_t(k));
        const uint32_t pose = world + 0x1d00 + 0x50 * uint32_t(k), wb = world + 0x9a00 + 0x20 * uint32_t(k);
        st(g, kRot, ld(g, O + 0x40));
        st(g, kPos, ld(g, O + 0x50));
        if (!(g.u8(O + 0x84) & 1)) continue;
        if (nkeyed() == 0) g.call(g.u32(kAbortInvalidParam));
        const uint32_t kcur = g.u32(world + 0x19ac);
        const uint32_t kgrid = g.u32(kcur + 0x30);
        const uint32_t oidx = uint32_t(int32_t(O - g.u32(world + 0x19a0)) / int32_t(kObjSize));
        st(g, kBox, ld(g, g.u32(kgrid + 0x41c) + oidx * 16));
        st(g, kQuery, ld(g, g.u32(kgrid + 0x420) + oidx * 16));
        m.r[3] = kcur; m.r[6] = kgrid; m.r[2] = kBox;
        f.call(0x00883020, {kQuery}, g.u32(kgrid + 0x420), oidx * 16);
        g.u32(kcur, 0);
        for (uint32_t ki = next_pair(g, g.u32(world + 0x19ac)); ki != 0xffffffffu; ki = next_pair(g, g.u32(world + 0x19ac))) {
            if (nkeyed() == 0) g.call(g.u32(kAbortInvalidParam));
            if (ki == uint32_t(int32_t(O - g.u32(world + 0x19a0)) / int32_t(kObjSize))) continue;
            bounds_check(g, ki, uint32_t(nkeyed()));
            const uint32_t K = g.u32(world + 0x19a0) + ki * kObjSize;
            if (g.u8(K + 0xb0) == 0) continue;
            st(g, kRotK, ld(g, K + 0x40)); st(g, kPosK, ld(g, K + 0x50));
            reset_contact_set(g, kSet);
            m.r[3] = O; m.r[6] = K; m.r[2] = kPos;
            f.call(0x0085ddd0, {g.u32(O + 0x2c), kPos, kRot, g.u32(K + 0x2c), kPosK, kRotK, 0}, kSet, g.u32(O + 0x2c));
            if (g.u32(kSet + 0x6a0) == 0) continue;
            const X prevO = ld(g, pose + 0x10);
            X prevK = ld(g, 0x00d61670);
            if (g.i32(K + 0x80) >= 0) prevK = ld(g, world + 0x1d10 + 0x50 * g.u32(K + 0x80));
            int32_t n = g.i32(kSet);
            if (n > 0) {
                uint32_t c = 0, p = kSet + 0x10;
                do {
                    m.r[3] = c; m.r[6] = K; m.r[2] = K;
                    f.call(0x0076ea00, {world, O}, p);
                    n = g.i32(kSet);
                    c++; p += 0x30;
                } while (int32_t(c) < n);
            }
            X lo = ld(g, 0x00d61670), hi = lo;
            for (int32_t j = 0; j < n; j++) {
                const uint32_t p = kSet + 0x10 + 0x30 * uint32_t(j);
                const X d = mulps(ld(g, p + 0x20), ld(g, p + 0x10));
                hi = maxps(hi, d); lo = minps(lo, d);
            }
            X push = mulps(addps(lo, hi), subps(X::splat(0), scale));
            st(g, kPush, push);
            if (g.u32(0x00da67d4) == 1) {
                m.r[2] = kPush;
                f.call(0x0084fe00, {kPush}, wb);
                push = ld(g, kPush);
            }
            const uint32_t mat = g.u32(O + 0x20);
            const X len = splat_f(x86::sqrt(dot3_xzy(mulps(push, push))));
            const X limit = ld(g, mat + 0xc0);
            if (all3(cmpltps(limit, len))) { const X ratio = divps(limit, len); push = mulps(push, ratio); scale = ratio; st(g, kPush, push); }
            const bool o_kin = g.i32(O + 0x80) >= 0;
            const bool k_kin = g.i32(K + 0x80) >= 0 && (g.u8(K + 0x84) & 1);
            const X z = ld(g, 0x00d61670);
            if (o_kin) {
                if (k_kin) {
                    const X half = mulps(X::splat(g.u32(0x009322cc)), push);
                    st(g, pose, addps(ld(g, pose), apply_lock(half, g.u32(O + 0xb4), z)));
                    st(g, O + 0x40, ld(g, pose));
                    const X hk = apply_lock(half, g.u32(K + 0xb4), ld(g, 0x00d61670));
                    const uint32_t kpose = world + 0x1d00 + 0x50 * g.u32(K + 0x80);
                    st(g, kpose, subps(ld(g, kpose), hk));
                    st(g, K + 0x40, ld(g, world + 0x1d00 + 0x50 * g.u32(K + 0x80)));
                } else {
                    st(g, pose, addps(ld(g, pose), apply_lock(push, g.u32(O + 0xb4), z)));
                    st(g, O + 0x40, ld(g, pose));
                }
            } else if (k_kin) {
                m.r[2] = K;
                f.call(0x00769f40, {}, kPush);
                const X share = ld(g, kPush);
                const uint32_t kpose = world + 0x1d00 + 0x50 * g.u32(K + 0x80);
                st(g, kpose, subps(ld(g, kpose), share));
                st(g, O + 0x40, ld(g, world + 0x1d00 + 0x50 * g.u32(K + 0x80)));
            }
            X a = ld(g, 0x00d61670), b = a;
            if (g.i32(K + 0x80) >= 0) { a = ld(g, world + 0x1d10 + 0x50 * g.u32(K + 0x80)); b = a; }
            const X d1 = subps(a, prevO), d2 = subps(b, prevK);
            const float moved1 = x86::sqrt(dot3_xzy(mulps(d1, d1)));
            const float moved2 = x86::sqrt(dot3_xzy(mulps(d2, d2)));
            const uint32_t events = g.u32(world + 0x1900);
            if (events < 100) {
                g.u32(world + 0x1900, events + 1);
                const uint32_t ev = world + events * 64;
                g.u32(ev, g.u32(O + 0x34));
                g.u32(ev + 4, g.u32(K + 0x34));
                g.u32(ev + 0x2c, 0);
                g.u8(ev + 0x20, 0);
                g.u32(ev + 0x30, g.u32(kSet + 0x5b0));
                g.u32(ev + 0x34, g.u32(kSet + 0x628));
                g.u32(ev + 0x10, g.u32(kSet + 0x10)); g.u32(ev + 0x14, g.u32(kSet + 0x14)); g.u32(ev + 0x18, g.u32(kSet + 0x18));
                g.f32(ev + 0x24, moved1); g.f32(ev + 0x28, moved2);
            }
            if (!o_kin) break;
        }
    }
    f.restore_saved();
    return m.ret(0);
}
#endif

#ifdef WORLD_HAND_DEADPATH   // readable port; its contact paths never run in the test worlds: the literal one (world_phys_lit3.cpp) is used
// 0076c880: the rider bodies against the keyed objects (EDI = world, plain ret). Rider bodies: world +0x1980 vector, 0x80
// bytes each (count fixed at entry). Per body R: bounds (shape vfunc 3), the keyed broad phase, then per enabled keyed K
// (byte +0xb0): the narrow phase, per contact 0076e6c0 (ECX = contact, EDX = K), and for a pushable K (+0x84 bit 5) the push
// (min + max of depth * normal * 0.8 * 0.5; 0084fe00 on K's water state when 0xda67d0 == 1; clamped by K's material) added to
// K's kinematic pose (copied to K +0x40); an event (K's displacement; the rider's is written as 0).
REFLEX_PORT_FN(0x0076c880, Rider_CollideKeyed, "world", 0) {
    Guest g{m};
    const uint32_t world = m.r[7];
    Frame f(m, 0x778, 2);
    const uint32_t sp = f.esp, kPos = sp + 0x40, kRot = sp + 0x60, kBox = sp + 0x80, kQuery = sp + 0xa0;
    const uint32_t kRotK = sp + 0xb0, kPosK = sp + 0x90, kSet = sp + 0xc0, kPush = sp + 0x70;
    const X half = X::splat(g.u32(0x009322cc));
    clear_contact_ids(g, kSet);
    const uint32_t nr = uint32_t(int32_t(g.u32(world + 0x1984) - g.u32(world + 0x1980)) >> 7);
    uint32_t ebx = m.r[3], esi = m.r[6];
    for (uint32_t i = 0; i < nr; i++) {
        bounds_check(g, i, uint32_t(int32_t(g.u32(world + 0x1984) - g.u32(world + 0x1980)) >> 7));
        const uint32_t R = g.u32(world + 0x1980) + i * 0x80;
        st(g, kRot, ld(g, R + 0x40));
        st(g, kPos, ld(g, R + 0x50));
        const uint32_t shape = g.u32(R + 0x2c);
        m.r[3] = ebx; m.r[6] = esi;
        f.call(g.u32(g.u32(shape) + 0xc), {kPos, kRot, kBox, kQuery}, shape);
        const uint32_t kcur = g.u32(world + 0x19ac);
        m.r[3] = kcur; m.r[6] = g.u32(kcur + 0x30); m.r[2] = kBox;
        f.call(0x00883020, {kQuery}, -1, kQuery);
        g.u32(kcur, 0);
        ebx = kcur;
        uint32_t ki;
        for (ki = next_pair(g, g.u32(world + 0x19ac)); ki != 0xffffffffu; ki = next_pair(g, g.u32(world + 0x19ac))) {
            bounds_check(g, ki, uint32_t(int32_t(g.u32(world + 0x19a4) - g.u32(world + 0x19a0)) / int32_t(kObjSize)));
            const uint32_t K = g.u32(world + 0x19a0) + ki * kObjSize;
            if (g.u8(K + 0xb0) == 0) continue;
            st(g, kRotK, ld(g, K + 0x40)); st(g, kPosK, ld(g, K + 0x50));
            ebx = 0;
            reset_contact_set(g, kSet);
            m.r[3] = 0; m.r[6] = K; m.r[2] = g.u32(R + 0x2c);
            f.call(0x0085ddd0, {g.u32(R + 0x2c), kPos, kRot, g.u32(K + 0x2c), kPosK, kRotK, 0}, kSet, kPos);
            if (g.u32(kSet + 0x6a0) == 0) continue;
            const bool pushable = g.u8(K + 0x84) & 0x20;
            X prevK = ld(g, 0x00d61670);
            if (pushable) prevK = ld(g, world + 0x1d10 + 0x50 * g.u32(K + 0x80));
            int32_t n = g.i32(kSet);
            if (n > 0) {
                uint32_t p = kSet + 0x10;
                do {
                    m.r[3] = ebx; m.r[6] = K; m.r[2] = K;
                    f.call(0x0076e6c0, {world, R}, p);
                    n = g.i32(kSet);
                    p += 0x30; ebx++;
                } while (int32_t(ebx) < n);
            }
            if (pushable) {
                const X scale = X::splat(g.u32(0x009cc8a8));
                X lo = ld(g, 0x00d61670), hi = lo;
                for (int32_t j = 0; j < n; j++) {
                    const uint32_t p = kSet + 0x10 + 0x30 * uint32_t(j);
                    const X d = mulps(ld(g, p + 0x20), ld(g, p + 0x10));
                    hi = maxps(hi, d); lo = minps(lo, d);
                }
                X push = mulps(mulps(addps(lo, hi), scale), half);
                st(g, kPush, push);
                if (g.u32(0x00da67d0) == 1) {
                    m.r[3] = ebx; m.r[6] = K; m.r[2] = K;
                    f.call(0x0084fe00, {kPush}, world + (g.u32(K + 0x80) + 0x4d0) * 32, kPush);
                    push = ld(g, kPush);
                }
                const X len = splat_f(x86::sqrt(dot3_xzy(mulps(push, push))));
                const X limit = ld(g, g.u32(K + 0x20) + 0xc0);
                if (all3(cmpltps(limit, len))) push = mulps(divps(limit, len), push);
                const uint32_t kpose = world + 0x1d00 + 0x50 * g.u32(K + 0x80);
                st(g, kpose, addps(ld(g, kpose), push));
                st(g, K + 0x40, ld(g, world + 0x1d00 + 0x50 * g.u32(K + 0x80)));
            }
            X now = ld(g, 0x00d61670);
            if (g.u8(K + 0x84) & 0x20) now = ld(g, world + 0x1d10 + 0x50 * g.u32(K + 0x80));
            const X dk = subps(now, prevK);
            const float moved = x86::sqrt(dot3_xzy(mulps(dk, dk)));
            const uint32_t events = g.u32(world + 0x1900);
            if (events < 100) {
                g.u32(world + 0x1900, events + 1);
                const uint32_t ev = world + events * 64;
                g.u32(ev, g.u32(R + 0x34));
                g.u32(ev + 4, g.u32(K + 0x34));
                g.u32(ev + 0x2c, 0);
                g.u8(ev + 0x20, 0);
                g.u32(ev + 0x30, g.u32(kSet + 0x5b0));
                g.u32(ev + 0x34, g.u32(kSet + 0x628));
                g.u32(ev + 0x10, g.u32(kSet + 0x10)); g.u32(ev + 0x14, g.u32(kSet + 0x14)); g.u32(ev + 0x18, g.u32(kSet + 0x18));
                g.u32(ev + 0x24, 0); g.f32(ev + 0x28, moved);
            }
        }
        esi = ki;
    }
    f.restore_saved();
    return m.ret(0);
}
#endif

#ifdef WORLD_HAND_READABLE   // readable port; the literal one in world_phys_lit4.cpp (exact EAX / ECX / EDX for the vehicle callers) is used
// 00770e80: the static objects near a vehicle into its list (EDI = list (simulation +0x1300: a vector of 0x50-byte records at
// +0xc..+0x14), ECX = query box, stack world, box; ret 8). The list is cleared (004db5b0, erase(begin, end)), the static broad
// phase run, and every enabled static S whose shape has +0x10 bit 2 appended while below capacity (004db570) through
// 00773d10: S's rotation and position, its broad-phase box (static grid +0x41c / +0x420), id S +0x30 and S +0x24. The
// record's last 8 bytes are the original's uninitialized stack (it builds the record on the stack): the record is built at
// the same guest address, so they are the same bytes.
REFLEX_PORT_FN(0x00770e80, Vehicle_CollectNearbyStatics, "world", 0) {
    Guest g{m};
    const uint32_t list = m.r[7], query = m.r[1], world = m.arg(0), box = m.arg(1);
    Frame f(m, 0x78, 2);
    const uint32_t sp = f.esp, kRec = sp + 0x20;
    const uint32_t end0 = g.u32(list + 0x10);
    if (g.u32(list + 0xc) > end0) g.call(g.u32(kAbortInvalidParam));
    if (g.u32(list + 0xc) > g.u32(list + 0x10)) g.call(g.u32(kAbortInvalidParam));
    m.r[3] = world; m.r[6] = list;
    f.call(0x004db5b0, {sp + 0x18, g.u32(list), g.u32(list + 0xc), g.u32(list), end0});
    const uint32_t cursor = g.u32(world + 0x19cc);
    m.r[6] = g.u32(cursor + 0x30); m.r[2] = box;
    f.call(0x00883020, {query}, query);
    g.u32(cursor, 0);
    for (uint32_t idx = next_pair(g, g.u32(world + 0x19cc)); idx != 0xffffffffu; idx = next_pair(g, g.u32(world + 0x19cc))) {
        bounds_check(g, idx, uint32_t(int32_t(g.u32(world + 0x19c4) - g.u32(world + 0x19c0)) >> 6));
        const uint32_t S = g.u32(world + 0x19c0) + idx * 64;
        if (g.u8(S + 0x38) == 0) continue;
        if (!((g.u32(g.u32(S + 0x28) + 0x10) >> 2) & 1)) continue;
        const uint32_t sgrid = g.u32(g.u32(world + 0x19cc) + 0x30);
        st(g, kRec, ld(g, S));
        st(g, kRec + 0x10, ld(g, S + 0x10));
        st(g, kRec + 0x20, ld(g, g.u32(sgrid + 0x41c) + idx * 16));
        st(g, kRec + 0x30, ld(g, g.u32(sgrid + 0x420) + idx * 16));
        g.u32(kRec + 0x40, g.u32(S + 0x30));
        g.u32(kRec + 0x44, S + 0x24);
        m.r[6] = idx << 4; m.r[2] = g.u32(S + 0x30);
        const uint32_t cap = f.call(0x004db570, {}, list, S + 0x24);
        const uint32_t n = uint32_t(int32_t(g.u32(list + 0x10) - g.u32(list + 0xc)) / 0x50);
        if (n >= cap) continue;
        m.r[6] = cap;
        f.call(0x00773d10, {kRec}, -1, list);
    }
    f.restore_saved();
    return m.ret(8);
}
#endif

// 0076a1e0: resets lost kinematic bodies (ESI = world, plain ret). The kill height is the player's water level (00402dc0 on
// 0x9e30d4, its +0xdc; else 0x977bac) minus 0x9778ec. A body whose pose position or velocity (lanes 0..2) is NaN or +-inf
// (0xadfdd0), or whose height is below the kill height, is reset: 0076a8b0 (EDX = body, stack world) and 0076abb0 (ECX = world,
// EDX = body).
REFLEX_PORT_FN(0x0076a1e0, Kinematic_ResetLost, "world", 0) {
    Guest g{m};
    const uint32_t world = m.r[6];
    Frame f(m, 0x38, 2);
    if (!(g.u8(0x009e30e0) & 1)) { g.u32(0x009e30e0, g.u32(0x009e30e0) | 1); g.u32(0x009e30dc, 0); }
    m.r[3] = 0; m.r[2] = 0x009e30d4;
    const uint32_t water = f.call(0x00402dc0);
    float level = g.f32(0x00977bac);
    if (water != 0) level = g.f32(water + 0xdc);
    level = x86::sub(level, g.f32(0x009778ec));
    const X kill = splat_f(level);
    auto bad = [&](const X& v, const X& inf, const X& ninf) {
        const X ok = cmpeqps(v, v);
        if ((ok.u[0] & ok.u[2] & ok.u[1]) == 0) return true;
        const X hit = orps(cmpeqps(v, inf), cmpeqps(v, ninf));
        return (hit.u[2] | hit.u[1] | hit.u[0]) != 0;
    };
    for (int32_t k = 0; k < g.i32(world + 0x19d4); k++) {
        const uint32_t pose = world + 0x1d00 + 0x50 * uint32_t(k);
        const uint32_t O = g.u32(world + 0x19d8 + 4 * uint32_t(k));
        const X inf = ld(g, 0x00adfdd0), ninf = subps(X::splat(0), inf);
        bool reset = bad(ld(g, pose + 0x10), inf, ninf);
        if (!reset) {
            const X p = ld(g, pose);
            reset = bad(p, inf, ninf) || all3(cmpltps(shufps(p, p, 0x55), kill));
        }
        if (!reset) continue;
        m.r[3] = uint32_t(k); m.r[7] = O; m.r[2] = O;
        f.call(0x0076a8b0, {world}, -1);
        m.r[2] = O;
        f.call(0x0076abb0, {}, world);
    }
    f.restore_saved();
    return m.ret(0);
}
