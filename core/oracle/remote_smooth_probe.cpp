// remote_smooth_probe.cpp (w-remote4w) -- per-call Verify of the remote simulation's smoothing step 00432680 (bike_sim.c
// bike_remote_smooth) on the live simulations of a reference world. No reference world has a remote (type 1) simulation, so no
// world calls it (Sweep B left it "not reached"); this probe is its verification. A frame hook (thunk, Engine::Update 0084ed20,
// outside every verified call) at one frame takes each slot's live simulation (MX: 4 bodies; the other classes when the world
// has them), saves it, sets the corrections the step smooths in (S+2da0 position, S+2db0 velocity, S+2dc0 incremental
// orientation, S+2dd0 angular velocity) and the three applied records' times (S+2c90 / S+2d00 / S+2d70) for each case, calls
// 00432680 (EDI = S, stack dt) through rx_call_original_ex (a registered port: native and original run on the same state and
// are compared), and puts the simulation back. No game logic: only the fields the step reads are set.
//   REFLEX_PORT=rx_bike,rx_RemoteSmoothProbeHook REFLEX_PORT_VERIFY=1 RX_REMOTE_SMOOTH_PROBE=1 [RX_REMOTE_SMOOTH_PROBE_FRAME=<n>]
#include "oracle.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
uint32_t fb(float f) { uint32_t v; std::memcpy(&v, &f, 4); return v; }
struct Saved {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint32_t flags; size_t fpu;
    explicit Saved(reflex_vm::Machine& m) : flags(m.flags), fpu(m.fpu.size()) { std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); }
    void restore(reflex_vm::Machine& m) const { std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu.resize(fpu); }
};
// the whole simulation block (the step writes the bodies S+f0.., the mechanism S+d0.., the ring S+2390.., the corrections)
const uint32_t kSimBytes = 0x2e00;
void set4(uint32_t a, float x, float y, float z, float w) { wr(a, fb(x)); wr(a + 4, fb(y)); wr(a + 8, fb(z)); wr(a + 12, fb(w)); }
void call_smooth(uint32_t S, float dt) {
    const uint32_t a[1] = {fb(dt)};
    rx_regs in; std::memset(&in, 0, sizeof in); in.r[7] = S;   // EDI
    rx_regs out; std::memset(&out, 0, sizeof out);
    rx_call_original_ex(0x00432680u, &in, &out, 1, a);
}
}  // namespace

RX_PORT(0x0084ed20, RemoteSmoothProbeHook, "thunk", 0) {
    static const char* on = std::getenv("RX_REMOTE_SMOOTH_PROBE");
    if (!on || !*on) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_REMOTE_SMOOTH_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_REMOTE_SMOOTH_PROBE_FRAME"))) : 650;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    g_m = &m;
    const uint32_t g = rd(0x00a946b8);
    const uint32_t nslots = g && mapped(g, 0x15420) ? uint8_t(rx_rd8(g + 0x1541c)) : 0;
    int calls = 0, sims = 0;
    for (uint32_t s = 0; s < nslots && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t V = rd(p + 0x1ac), S = V ? rd(V + 0x234) : 0;
        if (!S || !mapped(S, kSimBytes) || int32_t(rd(S + 0xec)) <= 0 || int32_t(rd(S + 0xec)) > 8) continue;
        std::vector<uint32_t> keep(kSimBytes / 4);
        for (uint32_t k = 0; k < kSimBytes / 4; k++) keep[k] = rd(S + 4 * k);
        ++sims;
        std::fprintf(stderr, "[remotesmooth] slot %u sim %08x vtable %08x bodies %d\n", s, S, rd(S), int(rd(S + 0xec)));
        // cases: (times t_newest, t_mid, t_oldest; the four corrections; dt). The weight k = dt / (((t_mid - t_old) + (t_new - t_mid))
        // * 1.5): k < 1 (records 1/60 s apart), k = 1 (clamped: a short span), a large span; a turn about each axis and a skew one.
        struct Case { float t0, t1, t2, dt; float dp[4], dv[4], q[4], dw[4]; };
        const float s2 = 0.70710678f;
        const Case cases[] = {
            {10.0f, 9.9833f, 9.9667f, 1.0f / 60, {0.3f, -0.2f, 0.5f, 0}, {1.0f, -2.0f, 0.5f, 0}, {0, 0.0998f, 0, 0.9950f}, {0.4f, -1.2f, 2.0f, 0}},
            {10.0f, 9.999f, 9.998f, 1.0f / 60, {-1.0f, 0.5f, 0.25f, 0}, {-3.0f, 0.2f, 1.0f, 0}, {0.1987f, 0, 0, 0.9801f}, {-2.0f, 0.5f, -0.7f, 0}},
            {10.0f, 9.7f, 9.4f, 1.0f / 30, {0.05f, 0.02f, -0.04f, 0}, {0.2f, 0.3f, -0.1f, 0}, {0, 0, 0.0499f, 0.9988f}, {3.0f, 1.0f, -1.5f, 0}},
            {10.0f, 9.9833f, 9.9667f, 1.0f / 60, {0, 0, 0, 0}, {0, 0, 0, 0}, {0.3f, 0.4f, -0.2f, std::sqrt(1.0f - 0.29f)}, {0, 0, 5.0f, 0}},
            {10.0f, 9.95f, 9.9f, 1.0f / 60, {2.0f, -1.0f, 0.0f, 0}, {0, 5.0f, 0, 0}, {0, s2 * 0.2f, s2 * 0.2f, std::sqrt(1.0f - 0.04f)}, {1.0f, 1.0f, 1.0f, 0}},
        };
        for (const Case& c : cases) {
            wr(S + 0x2c90, fb(c.t0)); wr(S + 0x2d00, fb(c.t1)); wr(S + 0x2d70, fb(c.t2));
            set4(S + 0x2da0, c.dp[0], c.dp[1], c.dp[2], c.dp[3]); set4(S + 0x2db0, c.dv[0], c.dv[1], c.dv[2], c.dv[3]);
            set4(S + 0x2dc0, c.q[0], c.q[1], c.q[2], c.q[3]); set4(S + 0x2dd0, c.dw[0], c.dw[1], c.dw[2], c.dw[3]);
            call_smooth(S, c.dt); ++calls;
            std::fprintf(stderr, "[remotesmooth] case dt %.4f: root angular %08x %08x %08x, body 1 velocity %08x %08x %08x\n", c.dt,
                         rd(S + 0x740), rd(S + 0x744), rd(S + 0x748), rd(S + 0x1a0), rd(S + 0x1a4), rd(S + 0x1a8));
            for (uint32_t k = 0; k < kSimBytes / 4; k++) wr(S + 4 * k, keep[k]);   // each case from the live state
        }
    }
    std::fprintf(stderr, "[remotesmooth] probe: %d simulations, %d calls\n", sims, calls);
    saved.restore(m);
    return reflex_vm::RUN_ORIGINAL;
}
