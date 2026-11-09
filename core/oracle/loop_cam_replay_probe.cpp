// loop_cam_replay_probe.cpp -- the oracle's probe for the replay cameras (core/game/loop_cam_replay.c). The reference worlds reach the
// director cameras only at Post / the replay reel (and the tower cameras are the only class most of them hold), so a frame hook
// (Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL virtuals of every director of the world's list on a snapshot of the
// whole guest memory and machine state (restored afterwards: the run goes on untouched). With the ports installed the machine runs the core
// function and then the original on the same state for each call (Verify), exactly as for a call from the game.
//   REFLEX_PORT=rx_MXZipLineCamera_vf19,...,rx_ReplayCamProbeHook REFLEX_PORT_VERIFY=1 RX_RCAM_PROBE=1 [RX_RCAM_FRAME=<n>]
// Per director: vf28 (pose valid, the springs cleared), vf37 (the director's value) with the controller away from every area and, for a
// camera with an ActivationArea, at the centroid of its first triangle (value 0) and of its second, vf19 three times at 1/60 s with the
// controller moving, vf39 (the debug draw); once vf38 (two updates of one second). A MXAnimatedReplayCamera is in no level's scene: with
// RX_RCAM_ANIMATED=1 one is made by the original (ReflexAlignedAlloc 0x450 + 00655200), given the world's first MXCameraPath (+0x21de8) and
// an ActivationArea of two triangles, and gets the same calls plus the set-time function 00655b40 and the scalar deleting destructor
// (vf0(1)), which frees it (on the snapshot). RX_RCAM_BASE=1: a bare MXReplayCamera (no level holds one): its constructor, vf28, vf37 and
// destructor.
#include "oracle.h"
#include <array>
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
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
float rdf(uint32_t a) { const uint32_t v = rd(a); float f; std::memcpy(&f, &v, 4); return f; }
void wrf(uint32_t a, float f) { uint32_t v; std::memcpy(&v, &f, 4); wr(a, v); }
uint32_t fbits(float f) { uint32_t v; std::memcpy(&v, &f, 4); return v; }

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

uint32_t find_player() {
    const uint32_t g = rd(0x00a946b8);
    for (uint32_t s = 0; g && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (mapped(p, 0x12a0) && rd(p + 4) != 0 && rd(p + 0x10) == 0) return p;
    }
    return 0;
}

float call_f(uint32_t fn, uint32_t self, int n, const uint32_t* args) {
    rx_regs in; std::memset(&in, 0, sizeof in); in.r[1] = self;
    rx_regs out; std::memset(&out, 0, sizeof out);
    rx_call_original_ex(fn, &in, &out, n, args);
    return out.st0;
}

/* the calls on one director */
void director_calls(uint32_t d, uint32_t veh, bool first, uint32_t* count) {
    const uint32_t vt = rd(d);
    const uint32_t vf28 = rd(vt + 0x70), vf37 = rd(vt + 0x94), vf19 = rd(vt + 0x4c), vf39 = rd(vt + 0x9c), vf38 = rd(vt + 0x98);
    const uint32_t lo = vt == 0x0095acfc ? 0x3e4 : 0x3e0;
    const uint32_t data = rd(d + lo), tris = rd(d + lo + 4);
    const uint32_t zero = 0;
    call_f(vf28, d, 1, &zero); count[0]++;
    /* the controller (vehicle) positions: far away, then the centroid of the first / second triangle of the area */
    std::vector<std::array<float, 3>> spots;
    spots.push_back({rdf(veh + 0xc0), rdf(veh + 0xc4), rdf(veh + 0xc8)});
    for (uint32_t t = 0; t < 2 && data && t * 3 + 2 < tris; t++) {
        std::array<float, 3> c{0, 0, 0};
        for (uint32_t v = 0; v < 3; v++) for (uint32_t k = 0; k < 3; k++) c[k] += rdf(data + 16 * (t * 3 + v) + 4 * k);
        for (uint32_t k = 0; k < 3; k++) c[k] /= 3.0f;
        spots.push_back(c);
    }
    for (size_t s = 0; s < spots.size(); s++) {
        wrf(veh + 0xc0, spots[s][0]); wrf(veh + 0xc4, spots[s][1]); wrf(veh + 0xc8, spots[s][2]);
        const float v = call_f(vf37, d, 0, nullptr); count[1]++;
        if (s > 0 && v != 0.0f) std::fprintf(stderr, "[rcam] director %08x (%08x): value %g at the centroid of triangle %zu (expected 0)\n", d, vt, v, s - 1);
        for (int step = 0; step < 3; step++) {
            wrf(veh + 0xc0, spots[s][0] + 1.5f * float(step));
            const uint32_t dt = fbits(1.0f / 60.0f);
            call_f(vf19, d, 1, &dt); count[2]++;
        }
    }
    call_f(vf39, d, 0, nullptr); count[3]++;
    if (first) { call_f(vf38, d, 0, nullptr); count[4]++; }
}

/* a MXAnimatedReplayCamera made on the snapshot */
void animated_calls(uint32_t veh, uint32_t* count) {
    const uint32_t w = rd(rd(0x00a946b8) + 0x5a54);
    const uint32_t path = rd(w + 0x21de8);
    rx_regs in; std::memset(&in, 0, sizeof in);
    rx_regs out; std::memset(&out, 0, sizeof out);
    const uint32_t size = 0x450;
    rx_call_original_ex(0x0078fbe0, &in, &out, 1, &size);
    const uint32_t cam = out.r[0];
    std::memset(&in, 0, sizeof in); in.r[0] = cam;
    rx_call_original_ex(0x00655200, &in, &out, 0, nullptr);   /* the constructor (a port: Verify) */
    std::fprintf(stderr, "[rcam] animated camera %08x made (path %08x)\n", cam, path);
    wr(cam + 0x3c0, path);
    /* the ActivationArea: two triangles around the controller and 20 units to the side */
    const uint32_t block = [&]{ std::memset(&in, 0, sizeof in); const uint32_t sz = 0x100; rx_call_original_ex(0x0078fbe0, &in, &out, 1, &sz); return out.r[0]; }();
    const float px = rdf(veh + 0xc0), py = rdf(veh + 0xc4), pz = rdf(veh + 0xc8);
    const float tri[6][3] = {{px - 5, py, pz - 5}, {px, py, pz + 6}, {px + 5, py, pz - 5}, {px + 20, py, pz}, {px + 25, py, pz + 9}, {px + 30, py, pz}};   /* the level's winding */
    for (int v = 0; v < 6; v++) for (int k = 0; k < 3; k++) wrf(block + 16 * v + 4 * k, tri[v][k]);
    wr(cam + 0x3e4, block); wr(cam + 0x3e8, 6); wr(cam + 0x3ec, 0x18); wr(cam + 0x3f0, 0xc); wr(cam + 0x3f4, 0x7fffffff); wr(cam + 0x3e0, 2);
    director_calls(cam, veh, true, count);
    /* the set-time function: inside and past the path (ESI = camera; stack t; ret 4) */
    const float len = path ? rdf(path + 0x120) : 0.0f;
    const float times[3] = {0.0f, len * 0.5f, len + 1.0f};
    for (float t : times) {
        std::memset(&in, 0, sizeof in); in.r[6] = cam;
        const uint32_t tb = fbits(t);
        rx_call_original_ex(0x00655b40, &in, &out, 1, &tb); count[5]++;
    }
    /* the deleting destructor (flags 1): the camera's own path is the world's, deleted with it on the snapshot */
    std::memset(&in, 0, sizeof in); in.r[1] = cam;
    const uint32_t flags = 1;
    rx_call_original_ex(rd(rd(cam)), &in, &out, 1, &flags); count[6]++;
}

/* a bare MXReplayCamera (no level holds one): the base constructor 006485e0 (ESI = the block; the camera entity base takes the caller's pushed
 * word), vf28 and vf37 of the base, then the scalar deleting destructor 006486c0 (vf0(1)) */
void base_calls(uint32_t* count) {
    rx_regs in; std::memset(&in, 0, sizeof in);
    rx_regs out; std::memset(&out, 0, sizeof out);
    const uint32_t size = 0x3c0;
    rx_call_original_ex(0x0078fbe0, &in, &out, 1, &size);
    const uint32_t cam = out.r[0];
    std::memset(&in, 0, sizeof in); in.r[6] = cam;
    rx_call_original_ex(0x006485e0, &in, &out, 0, nullptr);
    const uint32_t vt = rd(cam), zero = 0;
    call_f(rd(vt + 0x70), cam, 1, &zero);
    const float v = call_f(rd(vt + 0x94), cam, 0, nullptr);
    std::fprintf(stderr, "[rcam] base camera %08x (vtable %08x): vf37 %g\n", cam, vt, v);
    std::memset(&in, 0, sizeof in); in.r[1] = cam;
    const uint32_t flags = 1;
    rx_call_original_ex(rd(vt), &in, &out, 1, &flags);
    count[6]++;
}

void probe_case(reflex_vm::Machine& m) {
    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[rcam] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    const uint32_t g = rd(0x00a946b8), w = rd(g + 0x5a54), player = find_player(), veh = player ? rd(player + 0x1ac) : 0;
    uint32_t count[7] = {0, 0, 0, 0, 0, 0, 0};
    std::fprintf(stderr, "[rcam] world %08x player %08x vehicle %08x directors %08x\n", w, player, veh, rd(w + 0x21dd8));
    if (w && veh) {
        wr(w + 0x21de0, veh);                       /* the replay camera controller: the player's vehicle */
        bool first = true;
        int n = 0;
        uint32_t seen_class[4] = {0, 0, 0, 0};
        std::vector<uint32_t> victims;
        for (uint32_t d = rd(w + 0x21dd8); d && n < 256; d = rd(d + 0x310), n++) {
            const uint32_t vt = rd(d);
            const int ci = vt == 0x0095ab04 ? 0 : vt == 0x0095abac ? 1 : vt == 0x0095ac54 ? 2 : 3;
            if (seen_class[ci] >= 3) continue;     /* three of each class are enough: the rest run the same code */
            seen_class[ci]++;
            director_calls(d, veh, first, count);
            first = false;
            victims.push_back(d);
        }
        /* the scalar deleting destructors (flags 1) of the directors just called, last: they free the cameras (on the snapshot) */
        for (uint32_t d : victims) {
            rx_regs in; std::memset(&in, 0, sizeof in); in.r[1] = d;
            rx_regs out; std::memset(&out, 0, sizeof out);
            const uint32_t flags = 1;
            rx_call_original_ex(rd(rd(d)), &in, &out, 1, &flags); count[6]++;
        }
        if (std::getenv("RX_RCAM_ANIMATED")) animated_calls(veh, count);
        if (std::getenv("RX_RCAM_BASE")) base_calls(count);
    }
    std::fprintf(stderr, "[rcam] calls: vf28 %u vf37 %u vf19 %u vf39 %u vf38 %u set_time %u dtor %u\n", count[0], count[1], count[2], count[3], count[4], count[5], count[6]);
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, ReplayCamProbeHook, "thunk", 0) {
    if (!std::getenv("RX_RCAM_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_RCAM_FRAME") ? uint32_t(std::atoi(std::getenv("RX_RCAM_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    probe_case(m);
    return reflex_vm::RUN_ORIGINAL;
}
