// world_contact_probe.cpp -- the oracle probe for the two world-body contact impulses of a kinematic body that no reference
// world and no crash drive reaches (the 12-rider product ride reaches them at race frame 1231 / 1244; the five verify worlds and
// REFLEX_DRIVE 1/2/3/4/5/7 never call 0076b070 or 0076cd00 at all):
//   0076ee60 -- per contact of a kinematic body against a static object (Kinematic_CollideStatic's contact impulse);
//   0076ea00 -- per contact of a kinematic body against a keyed body (Kinematic_CollideKeyed's contact impulse).
//
// A frame hook (group "thunk", Engine::Update 0084ed20, outside every verified call) builds the bodies, a material/shape record,
// and a world block with the kinematic records these functions touch (velocity world +0x1d10 + 0x50 k, angular velocity
// world +0x5b90 + 0x50 k, the kinematic list, the contact-handler list) on the game heap, and calls the original through the
// installed port. Verify runs the core function and then the original on the same state and compares memory and the declared
// registers exactly as for a call from the game.
//   REFLEX_PORT=rx_world,rx_ContactImpulseProbeHook REFLEX_PORT_VERIFY=1 RX_CONTACT_PROBE=sk \
//     [RX_CONTACT_PROBE_FRAME=<n>] [RX_CONTACT_PROBE_SEED=<n>] <reflex_boot> <archive> <crt> <save> <frames> <log>
// Groups: s 0076ee60 (128 cases, 1..4 contacts each; material pairs, ordinary and extreme but finite masses / inertias, identity
// and rotated poses, approaching / separating / resting velocities, tangential speeds on both sides of the friction threshold);
// k 0076ea00 (96 cases: inactive O's early return, K integrating, the speed gate, the wake-by-flag path, Kinematic_Activate on an
// inactive K, and the activation-refused one-body path with a full 200-body kinematic list).
// No game logic here: only argument construction; the machine registers are restored after the probe.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

using namespace reflex_port;
using rx_oracle::bits;

namespace {
uint32_t g_frame = 0, g_rng = 0x9e3779b9u;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
void wf(uint32_t a, float f) { wr(a, bits(f)); }
float rfl(float lo, float hi) { return lo + (hi - lo) * (float(rnd() & 0xffffff) / 16777216.0f); }
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
// a unit-length direction from two random angles
void dir(float* d) {
    const float z = rfl(-1.0f, 1.0f), a = rfl(0.0f, 6.2831853f), r = std::sqrt(1.0f - z * z);
    d[0] = r * std::cos(a); d[1] = z; d[2] = r * std::sin(a);
}
// a unit quaternion (identity every fifth case)
void quat(uint32_t obj, int t) {
    float q[4];
    if (t % 5 == 0) { q[0] = 0; q[1] = 0; q[2] = 0; q[3] = 1.0f; }
    else {
        float n = 0.0f;
        for (int j = 0; j < 4; j++) { q[j] = rfl(-1.0f, 1.0f); n += q[j] * q[j]; }
        n = std::sqrt(n);
        for (int j = 0; j < 4; j++) q[j] /= n;
    }
    for (int j = 0; j < 4; j++) wf(obj + 4 * j, q[j]);
}
// a rigid body record: inverse mass +0, inverse inertia +0x10..+0x18, material +0x24, position +0x40, pose +0x50
void body(uint32_t obj, int t, float px, float py, float pz, uint32_t material) {
    static const float masses[8] = {500.0f, 120.0f, 60.0f, 12.0f, 2.0f, 0.5f, 1.0f, 2000.0f};
    static const float inertia[8] = {1000.0f, 220.0f, 60.0f, 5.0f, 0.25f, 1.0f, 3000.0f, 40.0f};
    const float i0 = inertia[(t / 8) % 8];
    wf(obj + 0x00, 1.0f / masses[t % 8]);
    wf(obj + 0x10, 1.0f / i0);
    wf(obj + 0x14, 1.0f / (i0 * 1.5f));
    wf(obj + 0x18, 1.0f / (i0 * 2.0f));
    wr(obj + 0x24, material);
    wf(obj + 0x40, px); wf(obj + 0x44, py); wf(obj + 0x48, pz); wf(obj + 0x4c, 0.0f);
    quat(obj + 0x50, t);
}
// 0076ee60: a kinematic body O (index k) against a static object, 1..4 contacts
void static_case(uint32_t W, int t) {
    const uint32_t k = uint32_t(t & 3);
    const uint32_t O = halloc(0x90), S = halloc(0x40);
    const float px = rfl(-100.0f, 100.0f), py = rfl(0.0f, 60.0f), pz = rfl(-100.0f, 100.0f);
    body(O, t, px, py, pz, uint32_t(t % 11));
    wr(S + 0x20, uint32_t((t / 11) % 11));
    wr(O + 0x80, k);
    const uint32_t vel = W + 0x1d10 + 0x50 * k, ang = W + 0x5b90 + 0x50 * k;
    const float speed = (t % 7 == 0) ? rfl(8.0f, 20.0f) : rfl(0.0f, 2.0f);
    for (int j = 0; j < 4; j++) { wf(vel + 4 * j, rfl(-speed, speed)); wf(ang + 4 * j, rfl(-4.0f, 4.0f)); }
    if (t % 6 == 0) { wf(vel + 4, 0.0f); wf(vel + 8, 0.0f); }   /* resting bodies: the tangential branches */
    const int n = 1 + (t % 4);
    for (int ci = 0; ci < n; ci++) {
        const uint32_t C = halloc(0x30);
        float d[3];
        dir(d);
        if (t % 9 == 1) { d[0] = 0; d[1] = 1; d[2] = 0; }        /* flat ground normal */
        const float reach = rfl(0.1f, 1.5f);
        wf(C + 0x00, px + d[0] * reach); wf(C + 0x04, py + d[1] * reach); wf(C + 0x08, pz + d[2] * reach);
        wf(C + 0x0c, 0.0f);
        wf(C + 0x10, d[0]); wf(C + 0x14, d[1]); wf(C + 0x18, d[2]); wf(C + 0x1c, 0.0f);
        const uint32_t args[2] = {W, C};
        rx_regs in = regs(), out = regs();
        in.r[1] = O; in.r[2] = S;
        rx_call_original_ex(0x0076ee60, &in, &out, 2, args);
    }
}
// the world state 0076ea00 / Kinematic_Activate read: the kinematic list (empty, or full for the refused-activation case) and the
// contact-handler list (+0xcc00) with room in place, so no allocation happens
void world_init(uint32_t W, uint32_t count) {
    wr(W + 0x19d4, count);
    wr(W + 0x19d8, 0);
    wr(W + 0xcc00, W + 0xce00);   /* begin */
    wr(W + 0xcc04, W + 0xce00);   /* end */
    wr(W + 0xcc08, W + 0xce40);   /* capacity end */
    wr(W + 0xcc18, 0);            /* the handler picker's cursor */
}
void keyed_case(uint32_t W, uint32_t WF, uint32_t M, int t) {
    const uint32_t O = halloc(0x90), K = halloc(0x90), C = halloc(0x30);
    const float px = rfl(-50.0f, 50.0f), py = rfl(0.0f, 40.0f), pz = rfl(-50.0f, 50.0f);
    body(O, t, px, py, pz, uint32_t(t % 11));          /* O: material id t % 11 */
    body(K, t + 1, px + rfl(-2.0f, 2.0f), py + rfl(-1.0f, 1.0f), pz + rfl(-2.0f, 2.0f), uint32_t((t / 11) % 11));
    wr(O + 0x80, 0);                                   /* O's kinematic index */
    wr(O + 0x84, 1);
    wr(K + 0x20, M);                                   /* K's material / shape record (thresholds +0xb0) */
    wr(K + 0x80, 1);                                   /* K's kinematic index (an inactive K for the activate case) */
    wr(K + 0x84, 0);
    wr(M + 0x80, 0x3f800000u);                         /* the activation's body +0xa0 (a lane's value does not matter) */
    for (int j = 0; j < 4; j++) wr(M + 0xb0 + 4 * j, 0);   /* thresholds below every speed (the gate passes) */
    wr(M + 0xbc, 0x7f7fffffu);                         /* the fourth lane is not tested */
    const uint32_t vO = W + 0x1d10, aO = W + 0x5b90, vK = W + 0x1d50, aK = W + 0x5be0;
    for (int j = 0; j < 4; j++) {
        wf(vO + 4 * j, rfl(-6.0f, 6.0f)); wf(aO + 4 * j, rfl(-4.0f, 4.0f));
        wf(vK + 4 * j, rfl(-6.0f, 6.0f)); wf(aK + 4 * j, rfl(-4.0f, 4.0f));
    }
    wf(vO + 4, 3.0f); wf(vO + 8, 0.0f);                /* the speed gate compares |vO|^2 with the thresholds */
    float d[3];
    dir(d);
    wf(C + 0x00, px); wf(C + 0x04, py); wf(C + 0x08, pz); wf(C + 0x0c, 0.0f);
    wf(C + 0x10, d[0]); wf(C + 0x14, d[1]); wf(C + 0x18, d[2]); wf(C + 0x1c, 0.0f);
    uint32_t world = W;
    switch (t % 6) {
    case 0: wr(O + 0x80, 0xffffffffu); break;          /* inactive O: the early return */
    case 1: wr(K + 0x84, 2); break;                    /* K integrates: the pair impulse branch */
    case 2: for (int j = 0; j < 3; j++) wr(M + 0xb0 + 4 * j, 0x7f7fffffu); break;   /* the speed gate fails */
    case 3: wr(K + 0x84, 0x20); break;                 /* woken by the state bit */
    case 4: wr(K + 0x80, 0xffffffffu); break;          /* Kinematic_Activate on an inactive K */
    default: wr(K + 0x80, 0xffffffffu); world = WF; break;   /* a full kinematic list: activation refused */
    }
    const uint32_t args[2] = {world, O};
    rx_regs in = regs(), out = regs();
    in.r[1] = C; in.r[2] = K;
    rx_call_original_ex(0x0076ea00, &in, &out, 2, args);
}
}  // namespace

RX_PORT(0x0084ed20, ContactImpulseProbeHook, "thunk", 0) {
    static const char* groups = std::getenv("RX_CONTACT_PROBE");
    if (!groups || !*groups) return reflex_vm::RUN_ORIGINAL;
    const uint32_t at = std::getenv("RX_CONTACT_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_CONTACT_PROBE_FRAME"))) : 10;
    static bool seeded = false;
    if (!seeded) { seeded = true; if (const char* s = std::getenv("RX_CONTACT_PROBE_SEED")) g_rng ^= uint32_t(std::atoi(s)) * 2654435761u; }
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    if (std::strchr(groups, 's')) {
        const uint32_t WS = halloc(0x6100);   /* the kinematic velocity arrays for k < 4 */
        for (int t = 0; t < 128; t++) static_case(WS, t);
    }
    if (std::strchr(groups, 'k')) {
        const uint32_t M = halloc(0xc0);
        uint32_t W = halloc(0x10000), WF = halloc(0x10000);
        world_init(WF, 200);                  /* the full kinematic list: Kinematic_Activate refuses */
        wf(WF + 0x1d10, 0.0f); wf(WF + 0x1d14, 3.0f); wf(WF + 0x1d18, 0.0f); wf(WF + 0x1d1c, 0.0f);
        for (int t = 0; t < 96; t++) {
            if (t % 6 == 4) { W = halloc(0x10000); world_init(W, 0); }   /* the activation case: an empty list */
            keyed_case(W, WF, M, t);
        }
    }
    saved.restore(m);
    std::fprintf(stderr, "[contactprobe] frame %u: groups '%s' done\n", g_frame, groups);
    return reflex_vm::RUN_ORIGINAL;
}
