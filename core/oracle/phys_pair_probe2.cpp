// phys_pair_probe2.cpp -- the oracle's probe for the pair generators ported last (phys_pair_circle.c, phys_pair_rays.c,
// phys_pair_query.c; development only, the same device as phys_pair_probe.cpp). The five reference worlds never generate these
// shape pairs: the circle (0x400) pairs need the four-wheel vehicles' body circles against a sphere / a wheel cylinder, the ray
// lists (0x20) a sphere or the terrain, and no class of the executable builds the 0x100 query shape at all.
//
// A frame hook (group "thunk", Engine::Update 0084ed20, outside every verified call) allocates the shapes and a contact set on
// the game heap and calls the original generator directly (registers and stack as the dispatcher's case passes them) and the
// original dispatcher 0085ddd0 (both orientations: the k_pairs rows and their swapped flags); every call goes through the
// installed port, so Verify runs the core function and then the original on the same state and compares memory and the
// declared registers.
//   REFLEX_PORT=rx_phys,rx_PhysPairProbe2Hook REFLEX_PORT_VERIFY=1 RX_PAIR_PROBE2=CSRHQ \
//     [RX_PAIR_PROBE2_FRAME=<n>] [RX_PAIR_PROBE2_SEED=<n>] <reflex_boot> <archive> <crt> <save> <frames> <log>
// Groups: C cylinder / circle (0086f3d0), S sphere / circle (008650c0), R sphere / ray list (00864460), H ray list / height
// field (00872c90, on the live terrain), Q the query shape against a sphere / box / cylinder / ray list (00864dd0, 0086b360,
// 0086f150, 00872970). The query object's vtable points at two code tails of the executable that only set EAX and return
// (0x008f81e3 `mov eax, 0xc9000007; ret 0x18` for the ray query, 0x00814aa6 `mov eax, 1; ret 0x14` / 0x006dcdc0
// `xor eax, eax; ret 0x14` for the sphere query): the generators then read their result buffers as the stale stack words
// the probe fills with random floats first. Radii and positions are random (with 0, -0, inf, NaN lanes), the contact set
// starts at 0..2 or 29..32 records.
// No game logic here: only argument construction.
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
uint32_t g_frame = 0, g_rng = 0x6a09e667u;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
float rfl(float lo, float hi) { return lo + (hi - lo) * (float(rnd() & 0xffffff) / 16777216.0f); }

// a zeroed game-heap block, 16-byte aligned
uint32_t halloc(uint32_t n) {
    const uint32_t a[1] = {n + 16};
    const uint32_t p = (rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a) + 15u) & ~15u;
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
}
// a value for a radius lane: ordinary values and the patterns the comparisons treat specially
float special(int k, float lo, float hi) {
    switch (k % 12) {
    case 0: return 0.0f;
    case 1: return bits(0x80000000u);           /* -0 */
    case 2: return bits(0x7f800000u);           /* +inf */
    case 3: return bits(0x7fc00001u);           /* qNaN */
    default: return rfl(lo, hi);
    }
}
void set_vec(uint32_t p, float x, float y, float z, float w) { wr(p, bits(x)); wr(p + 4, bits(y)); wr(p + 8, bits(z)); wr(p + 12, bits(w)); }
// a random unit quaternion (every third an identity)
uint32_t make_rotation(int t) {
    const uint32_t q = halloc(16);
    if (t % 3 == 0) { set_vec(q, 0, 0, 0, 1); return q; }
    float v[4];
    for (int j = 0; j < 4; j++) v[j] = rfl(-1.0f, 1.0f);
    const float n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
    for (int j = 0; j < 4; j++) v[j] = n > 0.0f ? v[j] / n : 0.0f;
    set_vec(q, v[0], v[1], v[2], v[3]);
    return q;
}
uint32_t make_position(float spread, float w) {
    const uint32_t p = halloc(16);
    set_vec(p, rfl(-spread, spread), rfl(-spread, spread), rfl(-spread, spread), w);
    return p;
}
uint32_t make_shape(uint32_t vtable, uint32_t type) {
    const uint32_t s = halloc(0x60);
    wr(s, vtable);
    wr(s + 8, type);
    return s;
}
// the contact set with 0..2 or 29..32 records already in it
uint32_t make_set(int t) {
    const uint32_t M = halloc(0x700);
    wr(M, t % 5 == 0 ? 0x1du + uint32_t(t % 4) : uint32_t(t % 3));
    return M;
}
// random floats into the stack below the current ESP (the generators' frames and their result buffers)
void fill_stack(reflex_vm::Machine& m) {
    const uint32_t esp = m.r[4];
    for (uint32_t a = esp - 0x1400; a < esp - 0x10; a += 4) wr(a, (rnd() & 7) == 0 ? rnd() : bits(rfl(-3.0f, 3.0f)));
}
// a ray list of n segments through / near the point c
uint32_t make_rays(int n, float cx, float cy, float cz, float len) {
    const uint32_t s = make_shape(0x0097662cu, 0x20);
    const uint32_t e = halloc(0x20u * uint32_t(n));
    for (int i = 0; i < n; i++) {
        const float dx = rfl(-1.0f, 1.0f), dy = rfl(-1.0f, 1.0f), dz = rfl(-1.0f, 1.0f);
        const float ox = cx + rfl(-0.6f, 0.6f), oy = cy + rfl(-0.6f, 0.6f), oz = cz + rfl(-0.6f, 0.6f);
        set_vec(e + 0x20u * uint32_t(i), ox - dx * len, oy - dy * len, oz - dz * len, rfl(0.0f, 1.0f));
        set_vec(e + 0x20u * uint32_t(i) + 0x10, ox + dx * len, oy + dy * len, oz + dz * len, rfl(0.0f, 1.0f));
    }
    wr(s + 0x20, uint32_t(n));
    wr(s + 0x24, e);
    return s;
}
// a direct call of the original generator with the dispatcher case's registers
void direct(uint32_t fn, uint32_t eax, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi, int n, const uint32_t* args) {
    rx_regs in; std::memset(&in, 0, sizeof in);
    in.r[0] = eax; in.r[1] = ecx; in.r[2] = edx; in.r[6] = esi; in.r[7] = edi;
    rx_regs out; std::memset(&out, 0, sizeof out);
    rx_call_original_ex(fn, &in, &out, n, args);
}
// the original dispatcher: thiscall M; A, qA*, pA*, B, qB*, pB*, flag
void dispatch(uint32_t M, uint32_t A, uint32_t QA, uint32_t PA, uint32_t B, uint32_t QB, uint32_t PB, uint32_t flag) {
    const uint32_t args[7] = {A, QA, PA, B, QB, PB, flag};
    rx_call_original(0x0085ddd0, RX_THISCALL, M, 7, args);
}

int g_calls = 0, g_hits = 0;
void count(uint32_t M, uint32_t before) { ++g_calls; if (rd(M) != before) ++g_hits; }

// C: cylinder (type 8, radius +0x20, height +0x24) against a circle (type 0x400, radius vec4 +0x20)
void probe_cylinder_circle(reflex_vm::Machine& m, int t) {
    fill_stack(m);
    const uint32_t cyl = make_shape(0x00976704u, 8), circ = make_shape(0x00976750u, 0x400);
    wr(cyl + 0x20, bits(special(t, 0.3f, 1.5f)));
    wr(cyl + 0x24, bits(t % 9 == 4 ? 0.0f : rfl(0.4f, 3.0f)));
    const float r = rfl(0.3f, 1.6f);
    for (int j = 0; j < 4; j++) wr(circ + 0x20 + 4 * j, bits(t % 7 == 2 ? special(t + j, 0.3f, 1.6f) : r));
    const uint32_t qa = make_rotation(t), pa = make_position(0.5f, 0.0f), qc = make_rotation(t + 1), pc = make_position(1.2f, 0.0f);
    const uint32_t M = make_set(t), before = rd(M), flag = uint32_t((t >> 2) & 1);
    switch (t % 4) {
    case 0: { const uint32_t a[6] = {M, cyl, pa, circ, qc, flag}; direct(0x0086f3d0, cyl, pc, qa, circ, M, 6, a); break; }
    case 1: { const uint32_t a[6] = {M, cyl, pa, circ, qc, flag ? 0u : 1u}; direct(0x0086f3d0, pc, pc, qa, cyl, M, 6, a); break; }
    case 2: dispatch(M, cyl, qa, pa, circ, qc, pc, flag); break;
    default: dispatch(M, circ, qc, pc, cyl, qa, pa, flag); break;
    }
    count(M, before);
}
// S: sphere (type 2, radius vec4 +0x20) against a circle
void probe_sphere_circle(reflex_vm::Machine& m, int t) {
    fill_stack(m);
    const uint32_t sph = make_shape(0x00976674u, 2), circ = make_shape(0x00976750u, 0x400);
    const float rs = rfl(0.3f, 2.0f), rc = rfl(0.3f, 1.6f);
    for (int j = 0; j < 4; j++) {
        wr(sph + 0x20 + 4 * j, bits(t % 7 == 3 ? special(t + j, 0.3f, 2.0f) : rs));
        wr(circ + 0x20 + 4 * j, bits(t % 11 == 5 ? special(t + 2 * j, 0.3f, 1.6f) : rc));
    }
    const uint32_t qs = make_rotation(t), ps = make_position(1.0f, rfl(0.0f, 1.0f)), qc = make_rotation(t + 2), pc = make_position(1.0f, 1.0f);
    if (t % 6 == 5) {
        /* the sphere's center on the circle's axis (local y = z = 0): both rim points coincide (the second branch) */
        set_vec(qc, 0, 0, 0, 1);
        set_vec(ps, rx_oracle::f32(rd(pc)) + rfl(-1.0f, 1.0f), rx_oracle::f32(rd(pc + 4)), rx_oracle::f32(rd(pc + 8)), 0.0f);
    }
    const uint32_t M = make_set(t), before = rd(M), flag = uint32_t((t >> 2) & 1);
    switch (t % 4) {
    case 0: { const uint32_t a[5] = {M, sph, ps, circ, pc}; direct(0x008650c0, sph, (pc & ~0xffu) | flag, qc, circ, M, 5, a); break; }
    case 1: { const uint32_t a[5] = {M, sph, ps, circ, pc}; direct(0x008650c0, ps, flag ? 0u : 1u, qc, sph, M, 5, a); break; }
    case 2: dispatch(M, sph, qs, ps, circ, qc, pc, flag); break;
    default: dispatch(M, circ, qc, pc, sph, qs, ps, flag); break;
    }
    count(M, before);
}
// R: sphere against a ray list (type 0x20)
void probe_sphere_rays(reflex_vm::Machine& m, int t) {
    fill_stack(m);
    const uint32_t sph = make_shape(0x00976674u, 2);
    const float rs = rfl(0.3f, 2.0f);
    for (int j = 0; j < 4; j++) wr(sph + 0x20 + 4 * j, bits(t % 9 == 1 ? special(t + j, 0.3f, 2.0f) : rs));
    const uint32_t rays = make_rays(t % 13 == 6 ? 0 : 1 + t % 4, rfl(-0.5f, 0.5f), rfl(-0.5f, 0.5f), rfl(-0.5f, 0.5f), rfl(0.2f, 2.5f));
    const uint32_t qs = make_rotation(t), ps = make_position(0.8f, 0.0f), qr = make_rotation(t + 1), pr = make_position(0.8f, 1.0f);
    const uint32_t M = make_set(t), before = rd(M), flag = uint32_t((t >> 2) & 1);
    switch (t % 4) {
    case 0: { const uint32_t a[4] = {ps, rays, qr, flag}; direct(0x00864460, sph, pr, sph, M, M, 4, a); break; }
    case 1: { const uint32_t a[4] = {ps, rays, qr, flag}; direct(0x00864460, ps, pr, sph, M, M, 4, a); break; }
    case 2: dispatch(M, sph, qs, ps, rays, qr, pr, flag); break;
    default: dispatch(M, rays, qr, pr, sph, qs, ps, flag); break;
    }
    count(M, before);
}
// H: ray list against the height field, near the live terrain's surface
bool surface_point(float* x, float* y, float* z) {
    const uint32_t terrain = rx_world_find_by_controller(0x009e30d4);
    if (!terrain) return false;
    const float sx = rx_oracle::f32(rd(terrain + 0x44d0)), sz = rx_oracle::f32(rd(terrain + 0x44d4));
    const uint32_t P = halloc(16);
    const uint32_t ha[1] = {P};
    for (int k = 0; k < 25; k++) {
        *x = (float(k % 5) - 2.0f) * sx * 0.05f + rfl(-2.0f, 2.0f);
        *z = (float(k / 5) - 2.0f) * sz * 0.05f + rfl(-2.0f, 2.0f);
        set_vec(P, *x, 0, *z, 0);
        if (rx_call_original(0x007c5950, RX_THISCALL, terrain, 1, ha) != 0) { *y = rx_oracle::f32(rd(P + 4)); return true; }
    }
    return false;
}
void probe_rays_terrain(reflex_vm::Machine& m, int t) {
    float x, y, z;
    if (!surface_point(&x, &y, &z)) return;
    fill_stack(m);
    const uint32_t hf = make_shape(0x009765e4u, 0x200);
    wr(hf + 0x20, 0x009e30d4u);                                 /* its object list: the terrain registry */
    const uint32_t rays = make_rays(1 + t % 4, 0.0f, 0.0f, 0.0f, rfl(0.3f, 3.0f));
    const uint32_t qr = make_rotation(t), pr = halloc(16), qh = make_rotation(0);
    set_vec(pr, x, y + rfl(-1.0f, 1.0f), z, 1.0f);
    const uint32_t M = make_set(t), before = rd(M), flag = uint32_t((t >> 2) & 1);
    switch (t % 4) {
    case 0: { const uint32_t a[4] = {M, qr, hf, flag}; direct(0x00872c90, rays, rays, pr, hf, M, 4, a); break; }
    case 1: { const uint32_t a[4] = {M, qr, hf, flag}; direct(0x00872c90, qr, rays, pr, rays, M, 4, a); break; }
    case 2: dispatch(M, rays, qr, pr, hf, qh, pr, flag); break;
    default: dispatch(M, hf, qh, pr, rays, qr, pr, flag); break;
    }
    count(M, before);
}
// Q: the query shape (type 0x100) whose object answers with the stubs
uint32_t make_query_shape(int t) {
    const uint32_t vt = halloc(0x50), obj = halloc(0x10), s = make_shape(0x009765c0u, 0x100);
    wr(vt + 0x20, 0x008f81e3u);                                 /* ray query: nonzero EAX, RET 0x18 */
    wr(vt + 0x44, t % 5 == 4 ? 0x006dcdc0u : 0x00814aa6u);      /* sphere query: 0 / 1, RET 0x14 */
    wr(obj, vt);
    wr(s + 0x20, obj);
    return s;
}
void probe_query(reflex_vm::Machine& m, int t) {
    fill_stack(m);
    const uint32_t qs = make_query_shape(t), qq = make_rotation(0), pq = make_position(0.5f, 1.0f);
    const uint32_t M = make_set(t), before = rd(M), flag = uint32_t((t >> 2) & 1);
    const uint32_t q = make_rotation(t + 1), p = make_position(1.0f, 1.0f);
    switch (t % 8) {
    case 0: case 1: {                                           /* sphere */
        const uint32_t sph = make_shape(0x00976674u, 2);
        for (int j = 0; j < 4; j++) wr(sph + 0x20 + 4 * j, bits(special(t + j + 4, 0.3f, 2.0f)));
        if (t % 8 == 0) { const uint32_t a[3] = {M, qs, flag}; direct(0x00864dd0, sph, 0x100u, flag, sph, p, 3, a); }
        else if (t & 2) dispatch(M, sph, q, p, qs, qq, pq, flag);
        else dispatch(M, qs, qq, pq, sph, q, p, flag);
        break;
    }
    case 2: case 3: {                                           /* box (half extents +0x20) */
        const uint32_t box = make_shape(0x009766e0u, 4);
        set_vec(box + 0x20, rfl(0.2f, 1.5f), rfl(0.2f, 1.5f), rfl(0.2f, 1.5f), 0.0f);
        if (t % 8 == 2) { const uint32_t a[4] = {M, q, qs, flag}; direct(0x0086b360, box, box, p, qs, M, 4, a); }
        else if (t & 4) dispatch(M, box, q, p, qs, qq, pq, flag);
        else dispatch(M, qs, qq, pq, box, q, p, flag);
        break;
    }
    case 4: case 5: {                                           /* cylinder */
        const uint32_t cyl = make_shape(0x00976704u, 8);
        wr(cyl + 0x20, bits(rfl(0.3f, 1.5f)));
        wr(cyl + 0x24, bits(rfl(0.4f, 3.0f)));
        if (t % 8 == 4) { const uint32_t a[3] = {cyl, qs, flag}; direct(0x0086f150, cyl, p, q, qs, M, 3, a); }
        else if (t & 8) dispatch(M, cyl, q, p, qs, qq, pq, flag);
        else dispatch(M, qs, qq, pq, cyl, q, p, flag);
        break;
    }
    default: {                                                  /* ray list */
        const uint32_t rays = make_rays(1 + t % 3, 0.0f, 0.0f, 0.0f, rfl(0.3f, 2.0f));
        if (t % 8 == 6) { const uint32_t a[2] = {q, flag}; direct(0x00872970, rays, p, qs, rays, M, 2, a); }
        else if (t & 16) dispatch(M, rays, q, p, qs, qq, pq, flag);
        else dispatch(M, qs, qq, pq, rays, q, p, flag);
        break;
    }
    }
    count(M, before);
}
}  // namespace

RX_PORT(0x0084ed20, PhysPairProbe2Hook, "thunk", 0) {
    static const char* groups = std::getenv("RX_PAIR_PROBE2");
    if (!groups || !*groups) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_PAIR_PROBE2_FRAME") ? uint32_t(std::atoi(std::getenv("RX_PAIR_PROBE2_FRAME"))) : 5;
    static bool seeded = false;
    if (!seeded) { seeded = true; if (const char* s = std::getenv("RX_PAIR_PROBE2_SEED")) g_rng ^= uint32_t(std::atoi(s)) * 2654435761u; }
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    uint32_t r[8]; reflex_vm::Xmm xmm[8];
    std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm);
    const uint32_t flags = m.flags; const size_t fpu = m.fpu.size();
    struct { char g; void (*f)(reflex_vm::Machine&, int); const char* name; } table[] = {
        {'C', probe_cylinder_circle, "cylinder/circle"}, {'S', probe_sphere_circle, "sphere/circle"},
        {'R', probe_sphere_rays, "sphere/rays"}, {'H', probe_rays_terrain, "rays/terrain"}, {'Q', probe_query, "query"}};
    for (auto& e : table) {
        if (!std::strchr(groups, e.g)) continue;
        g_calls = g_hits = 0;
        for (int t = 0; t < 96; t++) e.f(m, t);
        std::fprintf(stderr, "[pairprobe2] %s: %d calls, %d with contacts\n", e.name, g_calls, g_hits);
    }
    std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu.resize(fpu);
    std::fprintf(stderr, "[pairprobe2] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
