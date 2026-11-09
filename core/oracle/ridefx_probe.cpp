// ridefx_probe.cpp -- the oracle probe for 0069a170 (the YRingParticleEmitter's spawn builder, core/game/fx_emit.c) and the
// caller path 0069ad60. No reference world or boot reaches the Y-ring builder: the only live Y-ring emitter is the world's
// PhysicsObjectSplashParticles (MXWorld + 0x21dc4), whose bind (vtable slot 6) is the empty 00696470, so its state (+0x14 asset
// holder / +0x18 index) is never filled; the PlaceableYRingParticleEffect class (0x102c) is absent from all 57 level scenes
// (builds/fxconstruct/logs/level_fx_scan.txt). The probe therefore builds its call from the real thing the function reads: a live
// vehicle effect record's controller emitter state S (holder +0x14, type index +0x18, the 0x1f0 asset record T), the manager the
// emitter was bound to (controller +4), the controller's matrix (+0xf0) and the world's ParticleEntity. It varies only fields the
// function reads (the asset record's flags / ranges / channels, S's velocity source, acceleration offset, range bounds), restoring
// them after each call. Every call goes through the original address, so with the port installed Verify runs the core function and
// then the original on the same state and compares memory and registers.
//   REFLEX_PORT=rx_fx,-YRingParticleEmitter_vf0,rx_RideFxProbeHook REFLEX_PORT_VERIFY=1 RX_RIDEFX_PROBE=<groups>
//   [RX_RIDEFX_PROBE_FRAME=<n>]   groups: b the direct builder cases (default), a the 0069ad60 caller path.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
float gf(uint32_t a) { return f32(rd(a)); }
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
// a field the probe sets and restores (the asset record / a live emitter state is shared game data)
struct Poke { uint32_t a, old; Poke(uint32_t addr, uint32_t v) : a(addr), old(rd(addr)) { wr(addr, v); } ~Poke() { wr(a, old); } };
// a manager's particle slot: the spawn increments +0xb0 and writes one 0x80 record at (old counter) * 0x80 + +0x14
struct MgrSave {
    uint32_t mgr, cnt, slot, idx; uint8_t rec[0x80];
    MgrSave(uint32_t m) : mgr(m), cnt(rd(m + 0xb0)), idx(rd(m + 0xb0)) {
        const uint32_t cap = rd(m + 0x1c);
        slot = (idx < cap ? idx : 0) * 0x80u + rd(m + 0x14);
        for (uint32_t i = 0; i < 0x80; i++) rec[i] = mapped(slot + i) ? rx_rd8(slot + i) : 0;
    }
    ~MgrSave() { if (mapped(slot, 0x80)) for (uint32_t i = 0; i < 0x80; i++) rx_wr8(slot + i, rec[i]); if (mapped(mgr + 0xb0)) wr(mgr + 0xb0, cnt); }
};
struct Live {
    uint32_t S, mgr, mat, T, H, D, vel_src, acc_off; int ok;
};
// a live vehicle effect record's controller emitter state: S (+0x14 holder -> +0x44 type list), the manager (controller +4), the
// controller's matrix (+0xf0) and the record's velocity / offset blocks
Live find_live() {
    Live L; std::memset(&L, 0, sizeof L);
    const char* dg = std::getenv("RX_RIDEFX_DIAG"); const bool diag = dg && *dg;
    for (int k = 0; k < 12 && !L.ok; k++) {
        const uint32_t set = 0x00c11e90u + 0x17c30u * (uint32_t)k;
        const uint32_t veh = rd(set), ent = rd(set + 0x220), n = rd(set + 0x224);
        if (diag && (veh || ent || n)) std::fprintf(stderr, "[ridefxprobe] set %d: veh %08x ent %08x n %u\n", k, veh, ent, n);
        if (!veh || !ent || n == 0 || n > 0x60) continue;
        for (uint32_t i = 0; i < n && !L.ok; i++) {
            const uint32_t rec = set + 0x230 + 0x3f0u * i;
            if (!mapped(rec, 0x70)) break;
            const uint32_t ctl = rd(rec + 0x60);
            if (!ctl) continue;
            const uint32_t S = ctl + 0x10;
            if (!mapped(S, 0xc8)) continue;
            const uint32_t vt = rd(S);
            const uint32_t H = rd(S + 0x14);
            const uint32_t D = H ? rd(H + 0x44) : 0;
            if (diag && i == 0) std::fprintf(stderr, "[ridefxprobe] rec %u: ctl %08x S %08x vt %08x H %08x D %08x types %08x\n", i, ctl, S, vt, H, D, D ? rd(D + 0x10) : 0);
            if (vt < 0x401000u || !H || !D) continue;
            const uint32_t idx = rd(S + 0x18);
            if (!mapped(D, 0x14)) continue;
            const uint32_t types = rd(D + 0x10);
            if (diag && i == 0) std::fprintf(stderr, "[ridefxprobe]   idx %u types %08x mgr %08x mat %08x\n", idx, types, rd(ctl + 4), rd(ctl + 0xf0));
            if (!types || idx > 0x10) continue;
            const uint32_t T = types + idx * 0x1f0u;
            if (!mapped(T, 0x1f0)) continue;
            if (rd(T) == 0) continue;
            const uint32_t mgr = rd(ctl + 4), mat = rd(ctl + 0xf0);
            if (!mgr || !mat || !rd(mgr + 0x70)) continue;
            L.S = S; L.H = H; L.D = D; L.mgr = mgr; L.mat = mat;
            L.T = T;
            L.vel_src = rec + 0x50;                  // the effect's velocity (3 floats)
            L.acc_off = rec + 0xd0;                  // the effect's offset (3 floats)
            L.ok = 1;
        }
    }
    return L;
}
void spawn_once(const Live& L, uint32_t S, uint32_t mat, uint32_t index) {
    const MgrSave ms(L.mgr);
    rx_regs in = regs(); in.r[1] = mat;              // the callers' ECX: the matrix
    const uint32_t a[4] = {S, L.mgr, mat, index};
    call(0x0069a170, in, 4, a);
}
// the range / source fields a case wants, and the asset branch flags
struct Fields {
    uint32_t c4, c8;                                 // lifetime base / range
    uint32_t col, atlas, align, mirror, nan_angle;
    uint32_t index;                                  // atlas index
    uint32_t mat;                                    // 0: the live matrix, 1: identity, 2: rotated
    uint32_t far_pos, in_range, no_src, no_off;
};
uint32_t g_identity, g_rotated;
void make_matrices() {
    g_identity = halloc(0x40); g_rotated = halloc(0x40);
    for (uint32_t i = 0; i < 0x40; i += 4) { wr(g_identity + i, 0); wr(g_rotated + i, 0); }
    wr(g_identity + 0x00, 0x3f800000u); wr(g_identity + 0x14, 0x3f800000u);
    wr(g_identity + 0x28, 0x3f800000u); wr(g_identity + 0x3c, 0x3f800000u);
    const float c = 0.70710677f, s = 0.70710677f;
    wr(g_rotated + 0x00, bits(c));  wr(g_rotated + 0x08, bits(-s));
    wr(g_rotated + 0x14, 0x3f800000u);
    wr(g_rotated + 0x20, bits(s));  wr(g_rotated + 0x28, bits(c));
    wr(g_rotated + 0x30, bits(100.0f)); wr(g_rotated + 0x34, bits(200.0f)); wr(g_rotated + 0x38, bits(300.0f));
    wr(g_rotated + 0x3c, 0x3f800000u);
}
void probe_spawn() {
    const Live L = find_live();
    if (!L.ok) { std::fprintf(stderr, "[ridefxprobe] no live vehicle effect emitter state\n"); return; }
    std::fprintf(stderr, "[ridefxprobe] S %08x holder %08x T %08x mgr %08x mat %08x (idx %u)\n", L.S, L.H, L.T, L.mgr, L.mat, rd(L.S + 0x18));
    const uint32_t busy = rd(L.H + 0x4c);
    float px = 0.0f, py = 0.0f, pz = 0.0f; int have_player = 0;
    {
        const uint32_t c = rd(0x00a95844), scene = c ? rd(c + 0x1c + 4 * rd(c + 4)) : 0;
        if (scene && rd(scene + 0x19d0) > 0) {
            const uint32_t pl = rd(rd(scene + 0x19b8));
            if (pl && mapped(pl + 0x10, 0xc)) { px = gf(pl + 0x10); py = gf(pl + 0x14); pz = gf(pl + 0x18); have_player = 1; }
        }
    }
    static const Fields cases[] = {
        /* c4            c8    col atlas align mirror nan idx mat far in no_src no_off */
        {bits(10.0f), bits(5.0f), 0, 0, 0, 0, 0,  0, 0, 0, 1, 0, 0},   // plain spawn
        {bits(10.0f), bits(5.0f), 1, 1, 0, 0, 0,  5, 0, 0, 1, 0, 0},   // colour + atlas 5
        {bits(10.0f), bits(5.0f), 1, 1, 0, 0, 0, 16, 0, 0, 1, 0, 0},   // index 16: atlas -1
        {bits(10.0f), bits(5.0f), 1, 1, 1, 0, 0,  3, 1, 0, 1, 0, 0},   // align + identity matrix
        {bits(10.0f), bits(5.0f), 1, 1, 1, 1, 0,  3, 2, 0, 1, 0, 0},   // align + mirror + rotated matrix
        {bits(10.0f), bits(5.0f), 0, 0, 0, 0, 1,  0, 0, 0, 1, 0, 0},   // NaN start angle
        {bits(10.0f), bits(5.0f), 0, 0, 0, 0, 2,  0, 0, 0, 1, 0, 0},   // infinite start angle
        {bits(10.0f), bits(5.0f), 0, 0, 0, 0, 0,  0, 0, 1, 0, 0, 0},   // out of range
        {bits(10.0f), bits(5.0f), 0, 0, 0, 0, 0,  0, 0, 0, 1, 1, 1},   // no source / no offset
        {0,           0,          0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0},   // lifetime below 1/60
    };
    for (const Fields& c : cases) {
        Poke life0(L.T + 0xc4, c.c4), life1(L.T + 0xc8, c.c8);
        Poke col(L.T + 0x158, c.col), atlas(L.T + 0x161, c.atlas);
        Poke align(L.T + 0xc0, c.align), mirror(L.T + 0x128, c.mirror);
        Poke ang(L.T + 0xec, c.nan_angle == 1 ? 0x7fc00000u : c.nan_angle == 2 ? 0x7f800000u : rd(L.T + 0xec));
        Poke pos0(L.T + 0x20, c.far_pos ? bits(1.0e7f) : have_player ? bits(px) : rd(L.T + 0x20));
        Poke pos1(L.T + 0x24, c.far_pos ? bits(1.0e7f) : have_player ? bits(py) : rd(L.T + 0x24));
        Poke pos2(L.T + 0x28, c.far_pos ? bits(1.0e7f) : have_player ? bits(pz) : rd(L.T + 0x28));
        Poke bound(L.S + 0xa4, c.in_range ? bits(1.0e9f) : bits(1.0f));
        Poke flags(L.S + 0xb4, c.in_range ? 1u : 0u);
        Poke vs(L.S + 0x90, c.no_src ? 0 : L.vel_src), ao(L.S + 0xa0, c.no_off ? 0 : L.acc_off);
        spawn_once(L, L.S, c.mat == 0 ? L.mat : c.mat == 1 ? g_identity : g_rotated, c.index);
        wr(L.H + 0x4c, busy);                        // the early exits leave the holder busy
    }
}
void probe_caller() {
    const Live L = find_live();
    if (!L.ok) { std::fprintf(stderr, "[ridefxprobe] no live vehicle effect emitter state\n"); return; }
    const uint32_t S2 = halloc(0x100);
    for (uint32_t i = 0; i < 0xc8; i += 4) wr(S2 + i, rd(L.S + i));
    const uint32_t busy = rd(L.H + 0x4c);
    for (int interp = 0; interp <= 1; interp++) {
        Poke life0(L.T + 0xc4, bits(10.0f)), life1(L.T + 0xc8, bits(5.0f));
        Poke rate(L.T + 0x104, bits(2.0f)), range(L.T + 0x108, 0);
        Poke frac(S2 + 0x98, bits(2.0f)), active(S2 + 0x1c, 1), pending(S2 + 0x20, 1);
        Poke vs(S2 + 0x90, L.vel_src), ao(S2 + 0xa0, L.acc_off);
        Poke bound(S2 + 0xa4, bits(1.0e9f)), flags(S2 + 0xb4, 1);
        const MgrSave ms(L.mgr);
        for (uint32_t k = 0; k < 0x40; k += 4) wr(S2 + 0x30 + k, rd(L.mat + k));
        rx_regs in = regs(); in.r[1] = S2; in.r[2] = rd(0x00a946b8);   // EDX = the game object (0075c580's argument)
        const uint32_t a[5] = {bits(0.016666668f), L.mgr, L.mat, (uint32_t)interp, 4};
        call(0x0069ad60, in, 5, a);
        wr(L.H + 0x4c, busy);
    }
}
}  // namespace

RX_PORT(0x0084ed20, RideFxProbeHook, "thunk", 0) {
    static const char* groups = std::getenv("RX_RIDEFX_PROBE");
    if (!groups || !*groups) groups = "b";
    static const uint32_t at = std::getenv("RX_RIDEFX_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_RIDEFX_PROBE_FRAME"))) : 200;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    g_m = &m;
    make_matrices();
    if (std::strchr(groups, 'b')) probe_spawn();
    if (std::strchr(groups, 'a')) probe_caller();
    saved.restore(m);
    std::fprintf(stderr, "[ridefxprobe] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
