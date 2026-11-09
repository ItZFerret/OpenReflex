// phys_coll_probe.cpp -- the oracle's probe for the collision-shape scale virtuals (00848af0 composite / 00843fc0 sphere) that no
// reference world reaches (development only; core/oracle/phys_coll2.cpp holds the adapters).
//
// Why: 00667e40 Grid_ReportOverlaps calls the terrain layer's collision object's vf4 (slot +0x10) with the item's radius; the five
// reference worlds' drive never enters a terrain layer whose object is a composite (the port is 0 calls there, dispatchfix
// 2026-10-20). The probe builds the shapes on the game heap and calls the original address through the installed port, so Verify
// runs the core function and then the original on the same state and compares memory and the declared registers exactly as for a
// call from the game.
//   REFLEX_PORT=rx_phys,rx_PhysCollProbeHook REFLEX_PORT_VERIFY=1 RX_PHYSCOLL_PROBE=1 \
//     [RX_PHYSCOLL_PROBE_FRAME=<n>] [RX_PHYSCOLL_PROBE_SEED=<n>] <reflex_boot> <archive> <crt> <save> <frames> <log>
// Groups: c the composite's vf4 with stub children (0..4 children, random offsets, random scales, x87-bit scales), s the
// composite with real sphere children (the nested vf4), p the sphere's own vf4.
// No game logic here: only argument construction. The children's vf4 in group c is a native stub both runs execute.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

using namespace reflex_port;
using rx_oracle::bits;
using rx_oracle::f32;

namespace {
uint32_t g_frame = 0, g_rng = 0x9e3779b9u;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
reflex_vm::Machine* g_m = nullptr;
struct Saved {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint32_t flags; size_t fpu;
    explicit Saved(reflex_vm::Machine& m) : flags(m.flags), fpu(m.fpu.size()) { std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); }
    void restore(reflex_vm::Machine& m) const { std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu.resize(fpu); }
};
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
uint32_t halloc(uint32_t n) {
    const uint32_t a[1] = {n};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
}
float rfl(float lo, float hi) { return lo + (hi - lo) * (float(rnd() & 0xffffff) / 16777216.0f); }
// a scale that also covers the x87 argument path: ordinary floats and the special bit patterns (0, -0, inf, sNaN / qNaN)
float scale_at(int k) {
    switch (k % 8) {
    case 0: return 1.0f;
    case 1: return 2.5f;
    case 2: return -1.0f;
    case 3: return 0.0f;
    case 4: return bits(0x80000000u);
    case 5: return bits(0x7f800000u);
    case 6: return bits(0x7f800001u);   /* sNaN */
    default: return bits(0xffc00000u);  /* qNaN */
    }
}
// The child stub: a shape's vf4 takes (this, float) and returns the same EAX family; the probe only needs it deterministic.
const uint32_t kStub = 0x00848bc8;      /* int3 padding after 00848af0's RET 4 (never executed by the game) */
void install_stub() {
    g_m->natives[kStub] = [](reflex_vm::Machine& mm) -> uint32_t { mm.r[0] = mm.r[1]; return mm.ret(4); };
}
// A shape composite as the game's constructor leaves it: +0x2c children (n * 4), +0x30 end, +0x5c offsets (n * 16), +0x60 end,
// +0x74 the saved offsets (n * 16), +0x78 end, vtable 0x00976774.
uint32_t make_composite(uint32_t n, int kind) {
    const uint32_t self = halloc(0x80);
    const uint32_t kids = halloc(n ? 4 * n : 4);
    const uint32_t offs = halloc(n ? 16 * n : 16);
    const uint32_t save = halloc(n ? 16 * n : 16);
    wr(self, 0x00976774u);
    wr(self + 0x2c, kids); wr(self + 0x30, kids + 4 * n);
    wr(self + 0x5c, offs); wr(self + 0x60, offs + 16 * n);
    wr(self + 0x74, save); wr(self + 0x78, save + 16 * n);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t child;
        if (kind == 0) {                                  /* a fake shape whose vf4 is the stub */
            child = halloc(0x40);
            const uint32_t vt = halloc(0x20);
            wr(vt + 0x10, kStub);
            wr(child, vt);
        } else {                                          /* a real sphere: vtable 0x00976674, radius vector +0x20 */
            child = halloc(0x40);
            wr(child, 0x00976674u);
            for (uint32_t j = 0; j < 4; j++) wr(child + 0x20 + 4 * j, bits(rfl(0.5f, 3.0f)));
        }
        wr(kids + 4 * i, child);
        for (uint32_t j = 0; j < 4; j++) wr(offs + 16 * i + 4 * j, bits(rfl(-40.0f, 40.0f)));
        for (uint32_t j = 0; j < 4; j++) wr(save + 16 * i + 4 * j, bits(rfl(-40.0f, 40.0f)));
    }
    return self;
}
// 00848af0 CollisionShapeComposite::vf4 (thiscall, stack scale; RET 4)
void probe_composite(int kind) {
    for (int t = 0; t < 48; t++) {
        const uint32_t n = uint32_t(t / 12) % 5;           /* 0..4 children, each count with all scale kinds */
        const uint32_t self = make_composite(n, kind);
        const float s = scale_at(t);
        const uint32_t a[1] = {bits(s)};
        rx_regs in = regs(); in.r[1] = self; in.r[2] = rd(self);
        call(0x00848af0, in, 1, a);
    }
}
// 00843fc0 CollisionShapeSphere::vf4 (thiscall, stack scale; RET 4)
void probe_sphere() {
    for (int t = 0; t < 16; t++) {
        const uint32_t self = halloc(0x40);
        wr(self, 0x00976674u);
        for (uint32_t j = 0; j < 4; j++) wr(self + 0x20 + 4 * j, bits(rfl(0.25f, 4.0f)));
        for (uint32_t j = 0; j < 4; j++) wr(self + 0x30 + 4 * j, bits(rfl(-4.0f, 4.0f)));
        const uint32_t a[1] = {bits(scale_at(t))};
        rx_regs in = regs(); in.r[1] = self;
        call(0x00843fc0, in, 1, a);
    }
}
// 00844050 / 00844080 CollisionShapeDynamicCylinder::vf4 / vf5 (thiscall; RET 4 / RET): the scale saves and scales radius /
// height, the restore copies the saved values back through the x87 (an sNaN comes out quiet).
void probe_dyncyl() {
    for (int t = 0; t < 24; t++) {
        const uint32_t self = halloc(0x40);
        wr(self, 0x00976704u);
        wr(self + 0x20, bits(rfl(0.25f, 4.0f)));
        wr(self + 0x24, bits(rfl(-4.0f, 4.0f)));
        wr(self + 0x28, bits(rfl(-4.0f, 4.0f)));
        wr(self + 0x2c, (t % 8 == 6) ? 0x7f800001u : bits(rfl(-4.0f, 4.0f)));   /* an sNaN in the saved height */
        const uint32_t a[1] = {bits(scale_at(t))};
        rx_regs in = regs(); in.r[1] = self;
        call(0x00844050, in, 1, a);
        rx_regs in2 = regs(); in2.r[1] = self;
        call(0x00844080, in2);
    }
}
}  // namespace

RX_PORT(0x0084ed20, PhysCollProbeHook, "thunk", 0) {
    static const char* groups = std::getenv("RX_PHYSCOLL_PROBE");
    if (!groups || !*groups) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_PHYSCOLL_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_PHYSCOLL_PROBE_FRAME"))) : 5;
    static bool seeded = false;
    if (!seeded) { seeded = true; if (const char* s = std::getenv("RX_PHYSCOLL_PROBE_SEED")) g_rng ^= uint32_t(std::atoi(s)) * 2654435761u; }
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    g_m = &m;
    install_stub();
    if (std::strchr(groups, 'c')) probe_composite(0);
    if (std::strchr(groups, 's')) probe_composite(1);
    if (std::strchr(groups, 'p')) probe_sphere();
    if (std::strchr(groups, 'd')) probe_dyncyl();
    g_m->natives.erase(kStub);
    saved.restore(m);
    std::fprintf(stderr, "[physcollprobe] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
