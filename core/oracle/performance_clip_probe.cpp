// performance_clip_probe.cpp -- the oracle's probe for the PerformanceAnimator additive clip (core/game/anim_animators.c,
// rx_performance_additive_clip, 0083e930). Its callers run only while an ATV rider performs a trick (the ATV rider performances carry
// additive blocks, the MX ones none), and the five reference worlds are MX. The probe runs in an ATV world (garage 0,0,-1,8,2,4):
// at the first take-off it starts the aerial trick 9OClockNac_L_Air through the game's own 006c98c0 (as trickland_probe.cpp), which
// makes the rider's performance animator (rider +0xaf14) current, then calls the ORIGINAL 0083e930 on the live animator's clip
// children (EAX = the clip animator; stack character, pose, weight) with several weights on a pool pose, under Verify (the core
// function and then the original on the same state, memory and registers compared). Each call runs on a snapshot of the whole guest
// memory and machine state, restored afterwards.
//   REFLEX_PORT=0083e930,rx_PerformanceClipProbeHook REFLEX_PORT_VERIFY=1 RX_PC_PROBE=1 REFLEX_HOLD_W=1 [RX_PC_ALL=1: every child]
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
using namespace reflex_port;

namespace {
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }

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
struct StackStr {
    uint32_t sp, at;
    StackStr(const char* s, uint32_t n = 0x40) : sp(rx_sp()), at(sp - n) {
        rx_set_sp(at);
        uint32_t i = 0;
        for (; s[i] && i + 1 < n; i++) rx_wr8(at + i, uint8_t(s[i]));
        rx_wr8(at + i, 0);
    }
    ~StackStr() { rx_set_sp(sp); }
};
uint32_t find_player() {
    const uint32_t g = rd(0x00a946b8);
    if (!g) return 0;
    for (uint32_t i = 0; i < 12; i++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * i;
        if (mapped(p, 0x12a0) && rd(p + 4) && rd(p + 0x10) == 0 && rd(p + 0x1ac) && rd(p + 0x1b4)) return p;
    }
    return 0;
}
uint32_t bits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
}  // namespace

RX_PORT(0x0084ed20, PerformanceClipProbeHook, "thunk", 0) {
    if (!std::getenv("RX_PC_PROBE")) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    static bool done = false, prev_air = false;
    if (done) return reflex_vm::RUN_ORIGINAL;
    const uint32_t P = find_player();
    if (!P) return reflex_vm::RUN_ORIGINAL;
    const uint32_t K = P + 0xb58, V = rd(P + 0x1ac), R = rd(V + 0x1a0);
    const bool air = rd(K + 0x88) != 0;
    const bool takeoff = air && !prev_air && rd(K + 8) == 0 && rd(V + 0x280) == 0;
    prev_air = air;
    if (!takeoff) return reflex_vm::RUN_ORIGINAL;
    {   // the aerial trick record and the trick start, as the product ride's gesture would
        StackStr nm("9OClockNac_L_Air");
        const uint32_t a[2] = {nm.at, 1};
        rx_regs in = regs(), out = regs();
        rx_call_original_ex(0x006c8570, &in, &out, 2, a);
        const uint32_t trick = out.r[0];
        if (!trick) { std::fprintf(stderr, "[pc] no trick record\n"); return reflex_vm::RUN_ORIGINAL; }
        in = regs(); in.r[6] = K; in.r[7] = trick;
        rx_call_original_ex(0x006c98c0, &in, &out, 0, nullptr);
    }
    const uint32_t clip = rd(R + 0xaf14), ch = rd(R + 0x144);
    std::fprintf(stderr, "[pc] trick started: rider %08x class %u af10 %u performance animator %08x character %08x bones %u\n", R, rd(V + 0x194),
                 rd(R + 0xaf10), clip, ch, rd(ch + 0x234));
    done = true;
    if (!clip) return reflex_vm::RUN_ORIGINAL;
    const uint32_t count = rd(clip + 0x8c), perf = rd(clip + 0x70), blocks = rd(perf + 0x18), children = rd(clip + 0x74);
    std::fprintf(stderr, "[pc] %u clip blocks\n", count);
    static const float weights[] = {0.0f, 0.37f, 0.5f, 1.0f, -0.2f, 1.5f, 1e-9f};
    int cases = 0;
    for (uint32_t k = 0; k < count; k++) {
        const uint32_t block = blocks + 0x70 * k, animator = rd(children + 4 * k);
        std::fprintf(stderr, "[pc] block %u: start %.3f end %.3f additive %u layered %u animator %08x (vt %08x)\n", k, rx_oracle::f32(rd(block + 0x40)),
                     rx_oracle::f32(rd(block + 0x54)), rd(block + 0x64), rd(block + 0x68), animator, rd(animator));
        if (!animator || (!rd(block + 0x64) && !std::getenv("RX_PC_ALL"))) continue;
        for (int rep = 0; rep < 8; rep++) for (float w : weights) {
            uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
            std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
            auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
            auto host_restore = snapshot_host_state(m);
            if (!host_restore) { std::fprintf(stderr, "[pc] ERROR missing reversible host state\n"); return reflex_vm::RUN_ORIGINAL; }
            Pages pages(m.mem);
            // a pose from the pool (0082e2d0: EDI = the character), filled with a pseudo-random valid pose
            rx_regs in = regs(), out = regs();
            in.r[7] = ch;
            rx_call_original_ex(0x0082e2d0, &in, &out, 0, nullptr);
            const uint32_t pose = out.r[0];
            uint32_t seed = 0x1234567u + k * 31u + uint32_t(cases) * 7919u;
            const uint32_t nb = rd(ch + 0x234);
            for (uint32_t b = 0; b < nb; b++) {
                float q[4];
                float len2 = 0;
                for (int j = 0; j < 4; j++) { seed = seed * 1664525u + 1013904223u; q[j] = float(int32_t(seed >> 8) % 2001 - 1000) / 1000.0f; len2 += q[j] * q[j]; }
                const float inv = len2 > 0 ? 1.0f / std::sqrt(len2) : 1.0f;
                for (int j = 0; j < 4; j++) wr(pose + 0x30 * b + 4 * j, bits(q[j] * inv));
                for (int j = 0; j < 3; j++) { seed = seed * 1664525u + 1013904223u; wr(pose + 0x30 * b + 0x10 + 4 * j, bits(float(int32_t(seed >> 8) % 2001 - 1000) / 100.0f)); }
                rx_wr8(pose + 0x30 * b + 0x20, 1); rx_wr8(pose + 0x30 * b + 0x21, 1);
            }
            in = regs(); in.r[0] = animator;
            const uint32_t args[3] = {ch, pose, bits(w)};
            rx_call_original_ex(0x0083e930, &in, nullptr, 3, args);
            cases++;
            pages.restore();
            host_restore();
            std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
            m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
        }
    }
    std::fprintf(stderr, "[pc] %d calls of 0083e930\n", cases);
    return reflex_vm::RUN_ORIGINAL;
}
