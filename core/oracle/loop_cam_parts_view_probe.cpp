// loop_cam_parts_view_probe.cpp -- the oracle's probe for the Garage parts-view camera's update 00656070 (core/game/
// loop_cam_parts_view.c). The reference worlds never run the Garage / gate-selection camera, so a frame hook (Engine::Update
// 0084ed20) calls the ORIGINAL 00656070 on the MXGarage's own camera (MXGame [0xa946b8] + 0x15570 + 0x10, vtable 0x95a6dc) with
// the case's fields set: with the rx_loop ports installed in Verify mode the core runs first, then the original on the same state.
//   REFLEX_PORT=rx_loop,rx_PartsViewProbeHook REFLEX_PORT_VERIFY=1 RX_PV_PROBE=1 [RX_PV_FRAME=<n>]
// Stick values: the three key maps the camera reads (0xd5b258 yaw, 0xd5dbc8 pitch, 0xd690c8 zoom) get their pad-0 binding as an
// enabled pad binding without an opposite code, pad 0 marked connected (+0x2a0), and the input device's control record of that
// binding (its vf41, run original) the case's value at +0xc; the core and the original read the same records.
// Every case runs on a snapshot of the guest memory and machine state, restored afterwards (the run continues on the untouched world).
#include "oracle.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
float rf(uint32_t a) { return rx_oracle::f32(rd(a)); }
void wf(uint32_t a, float f) { wr(a, rx_oracle::bits(f)); }

struct Pages {
    reflex_vm::Memory& memory;
    struct Page { uint32_t address; std::array<uint8_t, 4096> bytes; };
    std::vector<Page> saved;
    std::vector<uint8_t> was;
    explicit Pages(reflex_vm::Memory& m) : memory(m), was(1u << 20, 0) {
        for (uint32_t i = 0; i < (1u << 20); i++) if (m.mapped(i << 12)) {
            was[i] = 1; saved.push_back(Page{}); auto& p = saved.back(); p.address = i << 12;
            std::memcpy(p.bytes.data(), m.page(p.address), 4096);
        }
    }
    void restore() {
        for (auto& p : saved) std::memcpy(memory.page(p.address), p.bytes.data(), 4096);
        for (uint32_t i = 0; i < (1u << 20); i++) if (!was[i] && memory.mapped(i << 12)) memory.unmap(i << 12, 4096);
    }
};

struct Case {
    const char* what;
    int mode;              /* +0x3e0 */
    int pad;               /* +0x450 (-1 none) */
    float yaw_in, pitch_in, zoom_in;
    float dt;
    int timers;            /* 0 done, 1 running, 2 finishing this step */
    int vectors;           /* 0 generic, 1 parallel, 2 opposite, 3 zero, 4 eye at the look-at point */
    int idle;              /* the auto-rotation: 0 off, 1 below the delay, 2 between, 3 beyond twice the delay */
    float yaw, pitch;      /* +0x3ec / +0x3f0 (orbit), the springs' positions (springs) */
    int paused;
};
}  // namespace

RX_PORT(0x0084ed20, PartsViewProbeHook, "thunk", 0) {
    if (!std::getenv("RX_PV_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_PV_FRAME") ? uint32_t(std::atoi(std::getenv("RX_PV_FRAME"))) : 300;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const uint32_t game = rd(0x00a946b8);
    const uint32_t cam = game ? game + 0x15580 : 0;
    if (!cam || !m.mem.mapped(cam) || rd(cam) != 0x0095a6dc) {
        std::fprintf(stderr, "[partsview] ERROR no MXUIPartsViewCamera at MXGame + 0x15580 (%08x)\n", cam ? rd(cam) : 0);
        return reflex_vm::RUN_ORIGINAL;
    }
    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[partsview] ERROR no reversible host state\n"); return reflex_vm::RUN_ORIGINAL; }
    Pages pages(m.mem);
    auto restore_all = [&]() {
        pages.restore(); host_restore();
        std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
        m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
    };
    auto invoke = [&](uint32_t fn, std::function<void(rx_regs&)> set, std::initializer_list<uint32_t> args) {
        rx_regs in{}, out{};
        for (unsigned i = 0; i < 8; i++) in.r[i] = regs[i];
        set(in);
        std::vector<uint32_t> a(args);
        rx_call_original_ex(fn, &in, &out, int(a.size()), a.empty() ? nullptr : a.data());
        return out;
    };
    /* the stick: map's pad-0 binding enabled as a pad binding, its control record's value */
    auto set_input = [&](uint32_t map, float value) {
        const uint32_t e = map;                                         /* pad 0 */
        wr(e + 0x14, 1); wr(e + 0x1c, 0); wr(e + 0x28, 1); wr(e + 0x34, 1);
        if (rd(e + 0x18) == 0) wr(e + 0x18, map == 0x00d5b258 ? 3 : map == 0x00d5dbc8 ? 4 : 5);
        const uint32_t g = rd(0x00a95844);
        const uint32_t dev = invoke(rd(rd(g) + 8), [&](rx_regs& in) { in.r[1] = g; }, {}).r[0];
        wr(dev + 0x2a0, 1);                                             /* pad 0 connected (its vf41 00814f30 checks it) */
        const uint32_t rec = invoke(rd(rd(dev) + 0xa4), [&](rx_regs& in) { in.r[1] = dev; }, {1, 0, rd(e + 0x18)}).r[0];
        if (rec && rec != 0x0092e0ec && m.mem.mapped(rec + 0xc)) wf(rec + 0xc, value);
        else std::fprintf(stderr, "[partsview] note: no control record for map %08x (%08x)\n", map, rec);
    };
    auto vec = [&](uint32_t off, float x, float y, float z) { wf(cam + off, x); wf(cam + off + 4, y); wf(cam + off + 8, z); };
    unsigned cases = 0;
    auto run = [&](const Case& c) {
        wr(cam + 0x3e0, uint32_t(c.mode));
        wr(cam + 0x450, uint32_t(c.pad));
        if (c.pad >= 0) { set_input(0x00d5b258, c.yaw_in); set_input(0x00d5dbc8, c.pitch_in); set_input(0x00d690c8, c.zoom_in); }
        /* a placement: rotated, translated */
        const float p[12] = {0.8f, 0.0f, -0.6f, 0.0f, 1.0f, 0.0f, 0.6f, 0.0f, 0.8f, 12.5f, -3.0f, 40.25f};
        for (int i = 0; i < 12; i++) wf(cam + 0x310 + 4 * i, p[i]);
        /* the blends' goals and starts */
        switch (c.vectors) {
        case 0: vec(0x350, 1.0f, 2.0f, 3.0f); vec(0x360, -2.0f, 0.5f, 1.0f); vec(0x370, 0.0f, 1.5f, -4.0f); vec(0x380, 3.0f, 0.2f, -1.0f);
                vec(0x460, 0.1f, 1.0f, 0.05f); vec(0x470, -0.2f, 0.9f, 0.3f); break;
        case 1: vec(0x350, 1.0f, 2.0f, 3.0f); vec(0x360, 2.0f, 4.0f, 6.0f); vec(0x370, 0.0f, 1.0f, -4.0f); vec(0x380, 0.0f, 2.0f, -8.0f);
                vec(0x460, 0.0f, 1.0f, 0.0f); vec(0x470, 0.0f, 1.0f, 0.0f); break;
        case 2: vec(0x350, 1.0f, 2.0f, 3.0f); vec(0x360, -1.0f, -2.0f, -3.0f); vec(0x370, 0.0f, 1.0f, 0.0f); vec(0x380, 0.0f, -3.0f, 0.0f);
                vec(0x460, 0.0f, 1.0f, 0.0f); vec(0x470, 0.0f, -1.0f, 0.0f); break;
        case 3: vec(0x350, 0.0f, 0.0f, 0.0f); vec(0x360, 0.0f, 0.0f, 0.0f); vec(0x370, 0.0f, 0.0f, 0.0f); vec(0x380, 0.0f, 0.0f, 0.0f);
                vec(0x460, 0.0f, 0.0f, 0.0f); vec(0x470, 0.0f, 0.0f, 0.0f); break;
        case 4: vec(0x350, 1.0f, 2.0f, 3.0f); vec(0x360, 1.0f, 2.0f, 3.0f); vec(0x370, 1.0f, 2.0f, 3.0f); vec(0x380, 1.0f, 2.0f, 3.0f);
                vec(0x460, 0.0f, 1.0f, 0.0f); vec(0x470, 0.0f, 1.0f, 0.0f); break;
        default: vec(0x350, 0.0f, 2.0f, 0.0f); vec(0x360, 0.0f, -1.0f, 0.0f); vec(0x370, 0.0f, 1.5f, -4.0f); vec(0x380, 3.0f, 0.2f, -1.0f);
                vec(0x460, 0.1f, 1.0f, 0.05f); vec(0x470, -0.2f, 0.9f, 0.3f); break;
        }
        /* the clocks */
        for (uint32_t d : {0x3c0u, 0x3c8u, 0x480u}) {
            wf(cam + d, 0.5f);
            wf(cam + d + 4, c.timers == 0 ? 0.5f : c.timers == 1 ? 0.125f : 0.49f);
        }
        wf(cam + 0x3d4, 6.0f);
        wr(cam + 0x41c, c.idle ? 1 : 0);
        wf(cam + 0x448, 2.0f);
        wf(cam + 0x44c, c.idle == 1 ? 0.5f : c.idle == 2 ? 2.5f : c.idle == 3 ? 3.99f : 0.0f);
        if (c.mode == 0) { wf(cam + 0x3ec, c.yaw); wf(cam + 0x3f0, c.pitch); }
        else {
            wf(cam + 0x3fc, c.yaw); wf(cam + 0x400, -c.yaw); wf(cam + 0x404, 0.0f);
            wf(cam + 0x408, c.pitch); wf(cam + 0x40c, c.pitch * 0.5f); wf(cam + 0x410, 1.0f);
        }
        wf(cam + 0x490, 0.0f); wf(cam + 0x494, 0.5f);
        /* the dead stack below ESP holds finite floats (0.25): the degenerate spring case reads its frame's B + 0xb0 .. unwritten
         * (00657ab5), and a NaN there would only show the emulator's payload-less x87 NaN through the square-root argument copy */
        for (uint32_t a = (m.r[4] - 0x800) & ~3u; a < m.r[4]; a += 4) wr(a, 0x3e800000u);
        const uint32_t pause = rd(rd(0x00a95844) + 0x10) + 0xc;
        if (c.paused) wr(pause, 1);
        invoke(0x00656070, [&](rx_regs& in) { in.r[1] = cam; }, {rx_oracle::bits(c.dt)});
        cases++;
        std::fprintf(stderr, "[partsview] %s: yaw %g pitch %g zoom %g eye-off %g %g %g at %g %g %g\n", c.what, rf(cam + 0x3ec), rf(cam + 0x3f0),
                     rf(cam + 0x48c), rf(cam + 0x390), rf(cam + 0x394), rf(cam + 0x398), rf(cam + 0x3a0), rf(cam + 0x3a4), rf(cam + 0x3a8));
        restore_all();
    };
    const float dt = 1.0f / 60.0f;
    const Case list[] = {
        {"orbit, done, no pad", 0, -1, 0, 0, 0, dt, 0, 0, 0, 30.0f, -10.0f, 0},
        {"orbit, running", 0, -1, 0, 0, 0, dt, 1, 0, 0, 30.0f, -10.0f, 0},
        {"orbit, finishing", 0, -1, 0, 0, 0, dt, 2, 0, 0, 30.0f, -10.0f, 0},
        {"orbit, running parallel", 0, -1, 0, 0, 0, dt, 1, 1, 0, 30.0f, -10.0f, 0},
        {"orbit, running opposite", 0, -1, 0, 0, 0, dt, 1, 2, 0, 30.0f, -10.0f, 0},
        {"orbit, running zero", 0, -1, 0, 0, 0, dt, 1, 3, 0, 30.0f, -10.0f, 0},
        {"orbit, eye at the point", 0, -1, 0, 0, 0, dt, 0, 4, 0, 0.0f, 0.0f, 0},
        {"orbit, idle below the delay", 0, -1, 0, 0, 0, dt, 0, 0, 1, 30.0f, -10.0f, 0},
        {"orbit, idle rotating", 0, -1, 0, 0, 0, dt, 0, 0, 2, 30.0f, -10.0f, 0},
        {"orbit, idle beyond twice", 0, -1, 0, 0, 0, dt, 0, 0, 3, 30.0f, -10.0f, 0},
        {"orbit, stick", 0, 0, 0.5f, -0.3f, 0.7f, dt, 0, 0, 1, 30.0f, -10.0f, 0},
        {"orbit, pitch stick only", 0, 0, 0.0f, 0.4f, 0.0f, dt, 0, 0, 2, 30.0f, -10.0f, 0},
        {"orbit, no stick with pad", 0, 0, 0.0f, 0.0f, 0.0f, dt, 1, 0, 2, 30.0f, -10.0f, 0},
        {"orbit, yaw wrap up", 0, 0, 1.0f, 0.0f, 0.0f, 0.5f, 0, 0, 0, 179.9f, 5.0f, 0},
        {"orbit, yaw wrap down", 0, 0, -1.0f, 0.0f, 0.0f, 0.5f, 0, 0, 0, -179.9f, 5.0f, 0},
        {"orbit, pitch clamps", 0, 0, 0.0f, 1.0f, -0.5f, 2.0f, 0, 0, 0, 0.0f, 80.0f, 0},
        {"orbit, pitch clamps low", 0, 0, 0.0f, -1.0f, 0.5f, 2.0f, 0, 0, 0, 0.0f, -80.0f, 0},
        {"orbit, paused", 0, -1, 0, 0, 0, dt, 1, 0, 0, 30.0f, -10.0f, 1},
        {"springs, running", 1, -1, 0, 0, 0, dt, 1, 0, 0, 20.0f, 10.0f, 0},
        {"springs, finishing", 1, -1, 0, 0, 0, dt, 2, 0, 0, 20.0f, 10.0f, 0},
        {"springs, running parallel", 1, -1, 0, 0, 0, dt, 1, 1, 0, 20.0f, 10.0f, 0},
        {"springs, running opposite", 1, -1, 0, 0, 0, dt, 1, 2, 0, 20.0f, 10.0f, 0},
        {"springs, done, angles", 1, -1, 0, 0, 0, dt, 0, 0, 0, 20.0f, 10.0f, 0},
        {"springs, done, zero angles", 1, -1, 0, 0, 0, dt, 0, 0, 0, 0.0f, 0.0f, 0},
        {"springs, stick positive", 1, 0, 0.6f, 0.4f, 0.3f, dt, 0, 0, 0, 20.0f, 10.0f, 0},
        {"springs, stick negative", 1, 0, -0.6f, -0.4f, -0.3f, dt, 0, 0, 0, -20.0f, -10.0f, 0},
        {"springs, stick zero", 1, 0, 0.0f, 0.0f, 0.0f, dt, 0, 0, 0, 20.0f, 10.0f, 0},
        {"springs, limits", 1, -1, 0, 0, 0, dt, 0, 0, 0, 400.0f, -400.0f, 0},
        {"springs, limits the other way", 1, -1, 0, 0, 0, dt, 0, 0, 0, -400.0f, 400.0f, 0},
        {"orbit, running vertical opposite", 0, -1, 0, 0, 0, dt, 1, 5, 0, 30.0f, -10.0f, 0},
        {"springs, running vertical opposite", 1, -1, 0, 0, 0, dt, 1, 5, 0, 20.0f, 10.0f, 0},
        {"springs, eye at the point", 1, -1, 0, 0, 0, dt, 0, 4, 0, 0.0f, 0.0f, 0},
        {"springs, eye at the point, angles", 1, -1, 0, 0, 0, dt, 0, 4, 0, 20.0f, 10.0f, 0},
    };
    for (const Case& c : list) run(c);
    /* the helpers directly: the direction blend with a wide angle, the look-at matrix of a degenerate pair */
    for (int k = 0; k < 6; k++) {
        vec(0x370, 1.0f, 0.0f, 0.0f);
        const float a = 0.3f + 0.9f * float(k);
        vec(0x380, std::cos(a), std::sin(a), k == 3 ? 0.0f : 0.25f);
        vec(0x350, 0.0f, 0.0f, 1.0f);
        vec(0x360, std::cos(a), 0.25f, std::sin(a));
        /* the elapsed time before the start for k >= 4: a negative ease (the turn's other sign) */
        wf(cam + 0x3c0, 0.5f); wf(cam + 0x3c4, k >= 4 ? -0.1f * float(k - 3) : 0.2f + 0.1f * float(k));
        wf(cam + 0x3c8, 0.5f); wf(cam + 0x3cc, k >= 4 ? -0.15f : 0.3f);
        const uint32_t dst = cam + 0x3b0;   /* 12 bytes of the camera as the output: restored afterwards */
        invoke(0x00658550, [&](rx_regs& in) { in.r[0] = cam; in.r[6] = dst; }, {});
        invoke(0x00658d80, [&](rx_regs& in) { in.r[0] = cam; in.r[6] = dst; }, {});
        cases++;
        restore_all();
    }
    {
        const uint32_t dst = cam + 0x310;   /* the 0x40 bytes of the placement: restored afterwards */
        const uint32_t pt = cam + 0x350;
        vec(0x350, 1.0f, 2.0f, 3.0f);
        invoke(0x0063e160, [&](rx_regs& in) { in.r[6] = dst; in.r[0] = pt; in.r[1] = pt; }, {});
        cases++;
        restore_all();
    }
    std::fprintf(stderr, "[partsview] frame %u: %u cases on %08x\n", g_frame, cases, cam);
    return reflex_vm::RUN_ORIGINAL;
}
