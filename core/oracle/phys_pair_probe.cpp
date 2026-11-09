// phys_pair_probe.cpp -- the oracle's probe for pair generators that no reference world or crash drive reaches (development
// only; the same device as phys_coll_probe.cpp). Today: 00861a60 CollisionPair_SphereVsSphere (the 4-rider clean product ride
// reaches it at race frame 432; the five reference worlds and the crash drive never generate a sphere / sphere contact).
//
// A frame hook (group "thunk", Engine::Update 0084ed20, outside every verified call) allocates two sphere shapes and a contact
// set on the game heap and calls the original dispatcher 0085ddd0 / the original generator 00861a60; the call goes through the
// installed port, so Verify runs the core function and then the original on the same state and compares memory and the declared
// registers exactly as for a call from the game.
//   REFLEX_PORT=rx_phys,rx_PhysPairProbeHook REFLEX_PORT_VERIFY=1 RX_PAIR_PROBE=1 \
//     [RX_PAIR_PROBE_FRAME=<n>] [RX_PAIR_PROBE_SEED=<n>] <reflex_boot> <archive> <crt> <save> <frames> <log>
// Groups: d direct 00861a60 calls (registers and stack as the dispatcher's case passes them), m through 0085ddd0 (the case table
// entry), both with random radii (all four lanes), positions, depths and swap flags, plus the count-clamp and no-overlap cases;
// c direct 0086f2a0 (cylinder vs height-field terrain) on the live terrain of the terrain registry (0x009e30d4), the cylinder
// placed on / above / below the surface with random radii, heights, rotations, flags and start counts; t the same through the
// 0085ddd0 case table (the (8, 0x200) row).
// No game logic here: only argument construction; the state is restored after the probe.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
extern "C" {
#include "../game/world_veh.h"
}

using namespace reflex_port;
using rx_oracle::bits;

namespace {
uint32_t g_frame = 0, g_rng = 0x2545f491u;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
struct Saved {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint32_t flags; size_t fpu;
    explicit Saved(reflex_vm::Machine& m) : flags(m.flags), fpu(m.fpu.size()) { std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); }
    void restore(reflex_vm::Machine& m) const { std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu.resize(fpu); }
};
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
uint32_t halloc(uint32_t n) {
    const uint32_t a[1] = {n};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
}
float rfl(float lo, float hi) { return lo + (hi - lo) * (float(rnd() & 0xffffff) / 16777216.0f); }
// a radius lane covering ordinary values and the bit patterns the mask logic treats specially
float rad_at(int k, float lo, float hi) {
    switch (k % 10) {
    case 0: return 0.0f;
    case 1: return bits(0x80000000u);          /* -0 */
    case 2: return lo;
    case 3: return hi;
    case 4: return bits(0x7f800000u);          /* +inf */
    case 5: return bits(0x7f800001u);          /* sNaN */
    case 6: return bits(0xffc00000u);          /* qNaN */
    default: return rfl(lo, hi);
    }
}
// two sphere shapes (vtable 0x00976674, type word +0x08 = 2, radius vec4 +0x20) and their positions
struct Pair {
    uint32_t A, B, PA, PB, QA, QB;
};
Pair make_pair(int k) {
    Pair p;
    p.A = halloc(0x40); p.B = halloc(0x40);
    p.PA = halloc(0x10); p.PB = halloc(0x10);
    p.QA = halloc(0x10); p.QB = halloc(0x10);
    wr(p.A, 0x00976674u); wr(p.B, 0x00976674u);
    wr(p.A + 8, 2); wr(p.B + 8, 2);            /* sphere type */
    for (int j = 0; j < 4; j++) {
        wr(p.A + 0x20 + 4 * j, bits(rad_at(k + j, 0.25f, 3.0f)));
        wr(p.B + 0x20 + 4 * j, bits(rad_at(k + 3 + j, 0.25f, 3.0f)));
        wr(p.PA + 4 * j, bits(k % 4 == 0 ? rfl(-0.5f, 0.5f) : rfl(-4.0f, 4.0f)));
        /* every fourth case is a guaranteed overlap (small offset), the rest random */
        wr(p.PB + 4 * j, bits(k % 4 == 1 ? rfl(-0.3f, 0.3f) : rfl(-4.0f, 4.0f)));
        wr(p.QA + 4 * j, j == 3 ? 0x3f800000u : 0);   /* identity quaternion */
        wr(p.QB + 4 * j, j == 3 ? 0x3f800000u : 0);
    }
    return p;
}
uint32_t make_set(uint32_t count) {
    const uint32_t M = halloc(0x700);
    wr(M, count);
    return M;
}
// 00861a60 direct: ECX = position A*, EDX = sphere A, EAX = sphere A, ESI = sphere B, EDI = M; stack M, B, pB*, flag
void probe_direct(int t) {
    const Pair p = make_pair(t);
    const uint32_t M = make_set(t % 5 == 0 ? 0x1du + uint32_t(t % 4) : uint32_t(t % 3));
    const uint32_t args[4] = {M, p.B, p.PB, uint32_t(t & 1)};
    rx_regs in = regs();
    in.r[0] = p.A; in.r[1] = p.PA; in.r[2] = p.A; in.r[6] = p.B; in.r[7] = M;
    rx_regs out = regs();
    rx_call_original_ex(0x00861a60, &in, &out, 4, args);
}
// 0085ddd0 through the original dispatcher: thiscall M; stack A, qA*, pA*, B, qB*, pB*, flag
void probe_dispatch(int t) {
    const Pair p = make_pair(t);
    const uint32_t M = make_set(t % 5 == 0 ? 0x1du + uint32_t(t % 4) : uint32_t(t % 3));
    const uint32_t args[7] = {p.A, p.QA, p.PA, p.B, p.QB, p.PB, uint32_t(t & 1)};
    rx_call_original(0x0085ddd0, RX_THISCALL, M, 7, args);
}
// 0086f2a0 cylinder vs height-field terrain, on the live terrain: the terrain registry list 0x009e30d4 is what the height-field
// shape's +0x20 points at in the world (00402dc0 scans it), so a synthetic shape B with +0x20 = that list resolves the same
// terrain object the game's own contact pairs use. The cylinder is placed on the surface (00402dc0's terrain found by the
// registry, base height 007c5950 at a point inside the grid) so the 24 center -> vertex rays cross it.
struct Cyl {
    uint32_t A, B, PA, QA, M;
};
float f32at(uint32_t a) { return rx_oracle::f32(rd(a)); }
void probe_cylinder_terrain(int t) {
    static int hits = 0, calls = 0;
    const uint32_t terrain = rx_world_find_by_controller(0x009e30d4);
    if (!terrain) return;
    Cyl c;
    c.A = halloc(0x40); c.B = halloc(0x40); c.PA = halloc(0x10); c.QA = halloc(0x10);
    wr(c.A, 0x00976674u); wr(c.A + 8, 8);              /* cylinder type word */
    wr(c.B, 0x00976674u); wr(c.B + 8, 0x200);          /* height-field type word */
    wr(c.B + 0x20, 0x009e30d4u);                       /* its object list: the terrain registry */
    wr(c.A + 0x20, bits(rad_at(t, 0.5f, 2.5f)));       /* radius (+0x20) */
    wr(c.A + 0x24, bits(t % 7 == 3 ? 0.0f : rfl(1.0f, 4.0f)));   /* height (+0x24) */
    const float sx = f32at(terrain + 0x44d0), sz = f32at(terrain + 0x44d4);
    const uint32_t P = halloc(0x10);
    const uint32_t ha[1] = {P};
    float x = 0.0f, z = 0.0f, y = 0.0f;
    bool inside = false;
    for (int k = 0; k < 25 && !inside; k++) {
        x = ((float)(k % 5) - 2.0f) * sx * 0.05f; z = ((float)(k / 5) - 2.0f) * sz * 0.05f;
        wr(P, bits(x)); wr(P + 4, 0); wr(P + 8, bits(z)); wr(P + 12, 0);
        if (rx_call_original(0x007c5950, RX_THISCALL, terrain, 1, ha) != 0) { y = f32at(P + 4); inside = true; }
    }
    if (!inside) return;
    /* the center on the surface, above it, below it, and a few offsets (the ray ends sweep the surface) */
    y += ((float)(t % 6) - 2.5f) * 0.5f;
    wr(c.PA, bits(x)); wr(c.PA + 4, bits(y)); wr(c.PA + 8, bits(z)); wr(c.PA + 12, bits(0.0f));
    if (t % 3 == 0) {
        wr(c.QA, 0); wr(c.QA + 4, 0); wr(c.QA + 8, 0); wr(c.QA + 12, 0x3f800000u);      /* identity */
    } else {                                                                           /* a normalised rotation */
        float q[4];
        for (int j = 0; j < 4; j++) q[j] = rfl(-1.0f, 1.0f);
        const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        for (int j = 0; j < 4; j++) wr(c.QA + 4 * j, bits(n > 0.0f ? q[j] / n : 0.0f));
    }
    c.M = make_set(t % 5 == 0 ? 0x1du + uint32_t(t % 4) : uint32_t(t % 3));
    const uint32_t args[3] = {c.A, c.B, uint32_t(t & 1)};
    rx_regs in = regs();
    in.r[0] = c.A; in.r[1] = c.PA; in.r[2] = c.QA; in.r[6] = c.B; in.r[7] = c.M;
    rx_regs out = regs();
    rx_call_original_ex(0x0086f2a0, &in, &out, 3, args);
    ++calls;
    if (rd(c.M) != 0) ++hits;
    if (t == 63) std::fprintf(stderr, "[pairprobe] cylinder/terrain: %d calls, %d with contacts\n", calls, hits);
}
// the same pair through the original dispatcher (the k_pairs row (8, 0x200) and its register / stack mapping): thiscall M;
// stack A, qA*, pA*, B, qB*, pB*, flag
void probe_cylinder_terrain_dispatch(int t) {
    const uint32_t terrain = rx_world_find_by_controller(0x009e30d4);
    if (!terrain) return;
    const uint32_t A = halloc(0x40), B = halloc(0x40), PA = halloc(0x10), QA = halloc(0x10);
    const uint32_t Z = halloc(0x10);
    uint32_t M = 0;
    wr(A, 0x00976674u); wr(A + 8, 8);
    wr(B, 0x00976674u); wr(B + 8, 0x200);
    wr(B + 0x20, 0x009e30d4u);
    wr(A + 0x20, bits(rad_at(t, 0.5f, 2.5f)));
    wr(A + 0x24, bits(rfl(1.0f, 4.0f)));
    const float sx = f32at(terrain + 0x44d0), sz = f32at(terrain + 0x44d4);
    const uint32_t P = halloc(0x10);
    const uint32_t ha[1] = {P};
    float x = 0.0f, z = 0.0f, y = 0.0f;
    bool inside = false;
    for (int k = 0; k < 25 && !inside; k++) {
        x = ((float)(k % 5) - 2.0f) * sx * 0.05f; z = ((float)(k / 5) - 2.0f) * sz * 0.05f;
        wr(P, bits(x)); wr(P + 4, 0); wr(P + 8, bits(z)); wr(P + 12, 0);
        if (rx_call_original(0x007c5950, RX_THISCALL, terrain, 1, ha) != 0) { y = f32at(P + 4); inside = true; }
    }
    if (!inside) return;
    y += ((float)(t % 6) - 2.5f) * 0.5f;
    wr(PA, bits(x)); wr(PA + 4, bits(y)); wr(PA + 8, bits(z)); wr(PA + 12, 0);
    wr(QA, 0); wr(QA + 4, 0); wr(QA + 8, 0); wr(QA + 12, 0x3f800000u);
    wr(Z, 0x3f800000u);                       /* pB (unused by the case; the mask checks read only +0x08 / +0x14) */
    M = make_set(t % 5 == 0 ? 0x1du + uint32_t(t % 4) : uint32_t(t % 3));
    const uint32_t args[7] = {A, QA, PA, B, Z, Z, uint32_t(t & 1)};
    rx_call_original(0x0085ddd0, RX_THISCALL, M, 7, args);
}
}  // namespace

RX_PORT(0x0084ed20, PhysPairProbeHook, "thunk", 0) {
    static const char* groups = std::getenv("RX_PAIR_PROBE");
    if (!groups || !*groups) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_PAIR_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_PAIR_PROBE_FRAME"))) : 5;
    static bool seeded = false;
    if (!seeded) { seeded = true; if (const char* s = std::getenv("RX_PAIR_PROBE_SEED")) g_rng ^= uint32_t(std::atoi(s)) * 2654435761u; }
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    if (std::strchr(groups, 'd')) for (int t = 0; t < 64; t++) probe_direct(t);
    if (std::strchr(groups, 'm')) for (int t = 0; t < 64; t++) probe_dispatch(t);
    if (std::strchr(groups, 'c')) for (int t = 0; t < 64; t++) probe_cylinder_terrain(t);
    if (std::strchr(groups, 't')) for (int t = 0; t < 64; t++) probe_cylinder_terrain_dispatch(t);
    saved.restore(m);
    std::fprintf(stderr, "[pairprobe] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
