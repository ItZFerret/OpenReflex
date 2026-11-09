// perform_stunt_probe.cpp -- the oracle's probe for BehaviorPerformStunt (core/game/race_beh2_perform_stunt.c): the update vf4 0052aac0
// and its state functions (0052ae10 .. 0052b410), the take-off test 0052ab80 and the planner 0052b0a0. In the Free Ride with AI
// riders (the world FreeRide_AI_Cheyenne: 6 riders, 5 AI) the riders only ever reach states 0 and 2 (they rarely jump); this probe
// drives a live AI rider's behaviour through the whole machine: on a snapshot it makes a ground plan with the ORIGINAL 0052aed0, then
// sets the state, mode, kind, take-off flags, the vehicle's air / vf45 words and its velocity, and calls the ORIGINAL vf4 many times
// with the driver clock stepping forward; with the ports installed Verify runs the core function and the original on each call and
// compares memory and registers.
//   REFLEX_PORT=0052aac0,0052ae10,...,rx_PerformStuntProbeHook REFLEX_PORT_VERIFY=1 RX_PSP=1 [RX_PSP_FRAME=<n>]
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
using rx_oracle::bits;

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
}  // namespace

RX_PORT(0x0084ed20, PerformStuntProbeHook, "thunk", 0) {
    if (!std::getenv("RX_PSP")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_PSP_FRAME") ? uint32_t(std::atoi(std::getenv("RX_PSP_FRAME"))) : 1500;
    static uint32_t frame = 0;
    if (++frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    // the AI riders' BehaviorPerformStunt objects (vtable 0x0093f888), enabled
    std::vector<uint32_t> found;
    for (uint32_t pg = 0x2000; pg < (1u << 20) && found.size() < 8; pg++) {
        if (!m.mem.mapped(pg << 12)) continue;
        for (uint32_t o = 0; o < 4096 && found.size() < 8; o += 4) {
            const uint32_t a = (pg << 12) + o;
            if (rx_rd32(a) == 0x0093f888u && mapped(a + 0xa8) && rd(a + 4) == 1 && rd(a + 8) != 0 && mapped(rd(a + 8) + 0x74)) found.push_back(a);
        }
    }
    std::fprintf(stderr, "[psp] %zu enabled BehaviorPerformStunt object(s)\n", found.size());
    int cases = 0;
    static uint64_t seen[8][4], planned[4];
    struct Vel { float x, y, z; };
    static const Vel vels[] = {{0, 0, 0}, {0, 30, 90}, {0, 12, 80}, {0, 30, 60}, {0, 9, 78}, {0, 26, 79}};
    for (size_t bi = 0; bi < found.size() && bi < 2; bi++) {
        const uint32_t beh = found[bi], drv = rd(beh + 8), player = rd(drv + 0x74), veh = rd(player + 0x1ac), sim = rd(veh + 0x16d4);
        std::fprintf(stderr, "[psp] behaviour %08x driver %08x player %08x vehicle %08x sim %08x state %u mode %u\n", beh, drv, player, veh, sim, rd(beh + 0xc), rd(beh + 0x10));
        for (uint32_t state = 0; state <= 5; state++)
        for (uint32_t kind = 1; kind <= 2; kind++)
        for (uint32_t air = 0; air < 2; air++)
        for (uint32_t mode = 0; mode < 4; mode++)
        for (uint32_t flags = 0; flags < 3; flags++)
        for (uint32_t vi = 0; vi < 6; vi += (mode == 0 ? 1 : 6)) {
            uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
            std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
            auto fl = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
            auto host_restore = snapshot_host_state(m);
            if (!host_restore) { std::fprintf(stderr, "[psp] ERROR missing reversible host state\n"); return reflex_vm::RUN_ORIGINAL; }
            Pages pages(m.mem);
            // a ground plan (the original 0052aed0, EAX = the behaviour) so the entries are real stunts
            { rx_regs in = regs(), out = regs(); in.r[0] = beh; rx_call_original_ex(0x0052aed0, &in, &out, 0, nullptr); }
            wr(beh + 0xc, state); wr(beh + 0x10, mode); wr(beh + 0x5c, kind);
            wr(beh + 0x44, flags == 1 ? 1u : 0u); wr(beh + 0x48, flags == 2 ? 1u : 0u);
            wr(beh + 0x50, (state + vi) % 3); wr(beh + 0x58, (state == 3 && kind == 1) ? 2u : 0xffffffffu);
            wr(beh + 0x54, bits(state == 2 ? 0.0f : 1.5f));
            if (rd(beh + 0x94) == 0 && state >= 3) { wr(beh + 0x94, 1); }
            wr(sim + 0x8d98, air); wr(sim + 0x8890, air); wr(sim + 0x89e0, air);
            wr(veh + 0x160, bits(vels[vi].x)); wr(veh + 0x164, bits(vels[vi].y)); wr(veh + 0x168, bits(vels[vi].z));
            float clock = rx_oracle::f32(rd(drv + 0x130));
            for (int step = 0; step < 24; step++) {
                clock += 0.43f;
                wr(drv + 0x130, bits(clock));
                if (step == 8) wr(sim + 0x8d98, air ^ 1u);
                if (step == 12) wr(veh + 0x280, 1);
                if (step == 14) wr(veh + 0x280, 0);
                rx_regs in = regs(), out = regs(); in.r[1] = beh;
                const uint32_t dt = bits(0.0166667f);
                rx_call_original_ex(0x0052aac0, &in, &out, 1, &dt);
                cases++;
                { const uint32_t st = rd(beh + 0xc), md = rd(beh + 0x10); if (st < 8 && md < 4) seen[st][md]++; planned[rd(beh + 0x94) & 3]++; }
            }
            pages.restore();
            host_restore();
            std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
            m.flags = fl; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
        }
    }
    std::fprintf(stderr, "[psp] %d vf4 calls; states after a call:", cases);
    for (int i = 0; i < 8; i++) for (int j = 0; j < 4; j++) if (seen[i][j]) std::fprintf(stderr, " s%d/m%d=%llu", i, j, (unsigned long long)seen[i][j]);
    std::fprintf(stderr, "; planned stunts 0..3: %llu %llu %llu %llu\n", (unsigned long long)planned[0], (unsigned long long)planned[1], (unsigned long long)planned[2], (unsigned long long)planned[3]);
    return reflex_vm::RUN_ORIGINAL;
}
