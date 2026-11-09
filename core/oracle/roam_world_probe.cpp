// roam_world_probe.cpp -- the oracle's probe for BehaviorRoamWorld (core/game/race_beh2_roam_world.c): the update vf4 0052fcf0 and its
// helpers. In the Free Ride with AI riders (FreeRide_AI_Cheyenne) the AI riders find no target area (the areas' vehicle / type flags
// never match), so only the states 4 .. 7 run; this probe makes every area of the track suit the vehicle (area +0x18 + 4 * k bit 0,
// +0x118 all type bits and 0x2000) on a snapshot, then drives a live AI rider's behaviour through the machine with the ORIGINAL vf4
// at its address (the driver clock stepping, the vehicle moved along its heading), starting in each state; with the ports installed
// Verify compares the core function with the original on every call.
//   REFLEX_PORT=<ports>,rx_RoamWorldProbeHook REFLEX_PORT_VERIFY=1 RX_RWP=1 [RX_RWP_FRAME=<n>]
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
using rx_oracle::bits;
using rx_oracle::f32;

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

RX_PORT(0x0084ed20, RoamWorldProbeHook, "thunk", 0) {
    if (!std::getenv("RX_RWP")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_RWP_FRAME") ? uint32_t(std::atoi(std::getenv("RX_RWP_FRAME"))) : 1500;
    static uint32_t frame = 0;
    if (++frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    std::vector<uint32_t> found;
    for (uint32_t pg = 0x2000; pg < (1u << 20) && found.size() < 8; pg++) {
        if (!m.mem.mapped(pg << 12)) continue;
        for (uint32_t o = 0; o < 4096 && found.size() < 8; o += 4) {
            const uint32_t a = (pg << 12) + o;
            if (rx_rd32(a) == 0x0093fa68u && mapped(a + 0x110) && rd(a + 4) == 1 && rd(a + 8) != 0 && mapped(rd(a + 8) + 0x74)) found.push_back(a);
        }
    }
    std::fprintf(stderr, "[rwp] %zu enabled BehaviorRoamWorld object(s)\n", found.size());
    int cases = 0;
    static uint64_t seen[8];
    const uint32_t variants = std::getenv("RX_RWP_VARIANTS") ? uint32_t(std::atoi(std::getenv("RX_RWP_VARIANTS"))) : 12;
    for (size_t bi = 0; bi < found.size() && bi < 2; bi++) {
        const uint32_t beh = found[bi], drv = rd(beh + 8), player = rd(drv + 0x74), veh = rd(player + 0x1ac), track = rd(drv + 0x10);
        std::fprintf(stderr, "[rwp] behaviour %08x driver %08x vehicle %08x track %08x areas %d state %u area %08x\n", beh, drv, veh, track,
                     int32_t(rd(track + 0x1d8)), rd(beh + 0xc), rd(beh + 0x10));
        for (uint32_t state = 0; state <= 7; state++)
        for (uint32_t v = 0; v < variants; v++) {
            uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
            std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
            auto fl = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
            auto host_restore = snapshot_host_state(m);
            if (!host_restore) { std::fprintf(stderr, "[rwp] ERROR missing reversible host state\n"); return reflex_vm::RUN_ORIGINAL; }
            Pages pages(m.mem);
            // every area suits the vehicle: flag bit 0 of its per-class word, every type bit, the 0x2000 bit, a long length budget
            const uint32_t k = rd(veh + 0x198) + 4 * rd(veh + 0x194);
            const int32_t n = int32_t(rd(track + 0x1d8));
            for (int32_t i = 0; i < n; i++) {
                const uint32_t area = rd(rd(track + 0x130) + 4 * i);
                wr(area + 0x18 + 4 * k, rd(area + 0x18 + 4 * k) | 1u | ((v & 1) ? 0x2u : 0u));
                wr(area + 0x118, rd(area + 0x118) | 0x3ff20u | ((v & 2) ? 0x2000u : 0u));
                if (v & 4) wr(area + 0x120, bits(500.0f));
            }
            // the target activity type of the behaviour's area: the name of type (v % 21) written into the area's name buffer (+0x108)
            {
                const uint32_t area = rd(beh + 0x10), name = rd(0x009cd858 + 4 * (v % 21));
                if (area && name && mapped(area + 0x108, 32)) { for (uint32_t i = 0; i < 31; i++) { const uint8_t c = rx_rd8(name + i); rx_wr8(area + 0x108 + i, c); if (!c) break; } }
            }
            // every third variant: the vehicle on the first node of an area, heading along it (so the candidates qualify)
            if (v % 3 == 0 && n > 0) {
                uint32_t node = 0;
                for (int32_t t = 0; t < n && !node; t++) { const uint32_t a = rd(rd(track + 0x130) + 4 * ((int32_t(v) / 3 + t) % n)); if (a) node = rd(a); }
                if (node) {
                    wr(veh + 0xc0, rd(node)); wr(veh + 0xc4, rd(node + 4)); wr(veh + 0xc8, rd(node + 8));
                    wr(veh + 0x130, bits(rx_oracle::f32(rd(node + 0x50)) * 20.0f)); wr(veh + 0x134, bits(rx_oracle::f32(rd(node + 0x54)) * 20.0f));
                    wr(veh + 0x138, bits(rx_oracle::f32(rd(node + 0x58)) * 20.0f));
                }
            }
            // the state, the targets cleared / chosen, the probes' switches
            wr(beh + 0xc, state);
            if (state <= 3) {   // approaching / following: a target area and its first node
                int32_t pick = int32_t(v % uint32_t(n > 0 ? n : 1));
                uint32_t tgt = 0;
                for (int32_t t = 0; t < n && !tgt; t++) { const uint32_t a = rd(rd(track + 0x130) + 4 * ((pick + t) % n)); if (a && rd(a)) tgt = a; }
                wr(beh + 0x30, tgt); wr(beh + 0x34, tgt ? rd(tgt) : 0);
            } else if (v & 8) { wr(beh + 0x30, 0); wr(beh + 0x34, 0); }
            wr(beh + 0x84, (v >> 4) & 1); wr(beh + 0x88, (v >> 5) & 1);
            wr(beh + 0x6c, bits(rx_oracle::f32(rd(drv + 0x130)) + ((v & 1) ? 3.0f : -1.0f)));
            float clock = rx_oracle::f32(rd(drv + 0x130));
            for (int step = 0; step < 40; step++) {
                clock += 0.37f;
                wr(drv + 0x130, bits(clock));
                if (step % 5 == 3 && v % 3 != 0) {   // move the vehicle: along its heading and a little aside
                    wr(veh + 0xc0, bits(rx_oracle::f32(rd(veh + 0xc0)) + 4.0f * ((int(v) % 3) - 1)));
                    wr(veh + 0xc8, bits(rx_oracle::f32(rd(veh + 0xc8)) + 6.0f));
                }
                if (step == 20 && v % 3 != 0) { wr(veh + 0x130, bits(0.0f)); wr(veh + 0x138, bits(0.0f)); }
                rx_regs in = regs(), out = regs(); in.r[1] = beh;
                const uint32_t dt = bits(0.0166667f);
                rx_call_original_ex(0x0052fcf0, &in, &out, 1, &dt);
                cases++;
                const uint32_t st = rd(beh + 0xc); if (st < 8) seen[st]++;
                // the helpers directly (each is a ported address of its own: Verify compares each call)
                if (step % 3 == 0) {
                    const uint32_t tmp = rx_sp() - 0x200;
                    rx_regs hin = regs(), hout = regs();
                    const uint32_t a2[2] = {beh, tmp};
                    rx_call_original_ex(0x0052f0e0, &hin, &hout, 2, a2);                                   // choose an area
                    hin = regs(); hin.r[7] = tmp; const uint32_t a1[1] = {beh};
                    rx_call_original_ex(0x00530590, &hin, &hout, 1, a1);                                   // the nearest suitable area
                    hin = regs(); hin.r[6] = beh; rx_call_original_ex(0x0052fca0, &hin, &hout, 0, nullptr);  // pick a target
                    if (rd(beh + 0x30) && rd(beh + 0x34)) {
                        hin = regs(); hin.r[1] = beh; rx_call_original_ex(0x0052f4a0, &hin, &hout, 0, nullptr);  // is it still good
                        hin = regs(); hin.r[6] = beh; rx_call_original_ex(0x0052f990, &hin, &hout, 0, nullptr);  // arrive
                        hin = regs(); rx_call_original_ex(0x0052f510, &hin, &hout, 1, a1);                       // follow
                    }
                    hin = regs(); rx_call_original_ex(0x00530740, &hin, &hout, 1, a1);                         // at the journey's end
                    if (step == 0) {
                        for (int32_t i = 0; i < n; i++) {   // 00538a60 on each area's first segment and the vehicle position
                            const uint32_t a0 = rd(rd(track + 0x130) + 4 * i), n0 = a0 ? rd(a0) : 0, n1 = n0 ? rd(n0 + 0x64) : 0;
                            if (!n0 || !n1) continue;
                            hin = regs(); hin.r[0] = n1; hin.r[1] = n0; hin.r[6] = veh + 0xc0; const uint32_t ao[1] = {tmp + 0x40};
                            rx_call_original_ex(0x00538a60, &hin, &hout, 1, ao);
                        }
                        for (int32_t i = 0; i < n; i++) {
                            hin = regs(); hin.r[0] = beh; hin.r[6] = rd(rd(track + 0x130) + 4 * i);
                            rx_call_original_ex(0x0052efa0, &hin, &hout, 0, nullptr);
                        }
                        for (uint32_t t = 0; t <= 0x14; t++) for (uint32_t c = 0; c < 8; c++) for (uint32_t kk = 0; kk < 2; kk++) {
                            hin = regs(); hin.r[1] = t; hin.r[0] = c; const uint32_t a[1] = {kk};
                            rx_call_original_ex(0x00530dc0, &hin, &hout, 1, a);
                        }
                    }
                }
            }
            pages.restore();
            host_restore();
            std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
            m.flags = fl; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
        }
    }
    std::fprintf(stderr, "[rwp] %d vf4 calls; states after a call:", cases);
    for (int i = 0; i < 8; i++) std::fprintf(stderr, " s%d=%llu", i, (unsigned long long)seen[i]);
    std::fprintf(stderr, "\n");
    return reflex_vm::RUN_ORIGINAL;
}
