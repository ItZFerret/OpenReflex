// stunt_events_probe.cpp -- the oracle's probe for the player's stunt-channel listeners (core/game/loop_player.c) and the rider's
// wind-up requests (core/game/anim_rider_play.c): StuntHoldBegin 0054d1a0, StuntHoldEnd 0054d1e0, StuntLanded 0054d310 (-> 00621fe0),
// StuntTakeoff 0054d360 (-> 006223e0). The five reference worlds drive keys, never the right stick, so no reference run raises a
// stunt event. A frame hook (Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL listener addresses on the
// live local player with every combination of the conditions they test (who == the player or not, the rider's trick state
// +0xaf10, the trick record's +0x64 flag, the two layer blends +0x598 / +0x58c at 1.0 or not); with the ports installed Verify
// runs the core function and then the original on the same state and compares memory and registers. Each case runs on a
// snapshot of the whole guest memory and machine state, restored afterwards.
//   REFLEX_PORT=0054d1a0,0054d1e0,0054d310,0054d360,00621fe0,006223e0,rx_StuntEventsProbeHook REFLEX_PORT_VERIFY=1 RX_SE_PROBE=1
//   [RX_SE_FRAME=<n>]
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
    for (uint32_t s = 0; s < 12; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) && rd(p + 0x1ac) && rd(rd(p + 0x1ac) + 0x1a0)) return p;
    }
    return 0;
}

struct Case { uint32_t fn; bool other; uint32_t af10; bool flag64; int blend; };

void run_case(reflex_vm::Machine& m, uint32_t P, uint32_t trick, const Case& c) {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[se] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    const uint32_t ra = rd(rd(P + 0x1ac) + 0x1a0);
    wr(ra + 0xaf10, c.af10);
    wr(trick + 0x64, c.flag64 ? 1u : 0u);
    /* the two layer blends: pointers to floats (scratch words below the stack pointer) */
    const uint32_t sp = rx_sp(), f0 = sp - 0x400, f1 = sp - 0x404;
    wr(f0, 0x3f800000u); wr(f1, c.blend == 0 ? 0x3f800000u : 0x3f000000u);
    if (c.blend == 2) wr(f0, 0x3f000000u);
    if (c.blend == 3) { wr(ra + 0x598, 0); } else { wr(ra + 0x598, f0); }
    wr(ra + 0x58c, c.blend == 4 ? 0 : f1);
    rx_regs in = regs(); in.r[1] = P;
    const uint32_t a[2] = {c.other ? 0u : P, trick};
    rx_regs out = regs();
    if (c.fn == 0x00621fe0 || c.fn == 0x006223e0) {   /* the wind-up requests: ESI = the rider, EDI = the player, ECX = the trick */
        in.r[1] = trick; in.r[6] = ra; in.r[7] = P;
        rx_call_original_ex(c.fn, &in, &out, 0, nullptr);
    } else {
        rx_call_original_ex(c.fn, &in, &out, 2, a);
    }
    std::fprintf(stderr, "[se] %08x other %d af10 %u flag64 %d blend %d -> af10 %u 5b2d4 %u 5b2d0 %u af30 %u af34 %u clip %08x\n", c.fn,
                 c.other, c.af10, c.flag64, c.blend, rd(ra + 0xaf10), rd(ra + 0x5b2d4), rd(ra + 0x5b2d0), rd(ra + 0xaf30),
                 rd(ra + 0xaf34), rd(ra + 0xaf14));
    pages.restore();
    host_restore();
    std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, StuntEventsProbeHook, "thunk", 0) {
    if (!std::getenv("RX_SE_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_SE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_SE_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    const uint32_t P = find_player();
    if (!P) { std::fprintf(stderr, "[se] no local player at frame %u\n", g_frame); return reflex_vm::RUN_ORIGINAL; }
    const char* names[] = {"9OClockNac_L_Air", "9OClockNac_L_Ground"};
    for (int air = 1; air >= 0; air--) {
        StackStr nm(names[air ? 0 : 1]);
        const uint32_t args[2] = {nm.at, uint32_t(air)};
        const uint32_t trick = [&] { rx_regs in0 = regs(), out = regs(); rx_call_original_ex(0x006c8570, &in0, &out, 2, args); return out.r[0]; }();
        std::fprintf(stderr, "[se] player %08x rider %08x trick '%s' record %08x\n", P, rd(rd(P + 0x1ac) + 0x1a0), names[air ? 0 : 1], trick);
        if (!trick) continue;
        static const uint32_t fns[] = {0x0054d1a0, 0x0054d1e0, 0x0054d310, 0x0054d360, 0x00621fe0, 0x006223e0};
        for (uint32_t fn : fns)
            for (int other = 0; other < 2; other++)
                for (uint32_t af10 : {0u, 1u, 3u})
                    for (int flag64 = 0; flag64 < 2; flag64++)
                        for (int blend = 0; blend < 5; blend++)
                            run_case(m, P, trick, Case{fn, other != 0, af10, flag64 != 0, blend});
    }
    std::fprintf(stderr, "[se] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
