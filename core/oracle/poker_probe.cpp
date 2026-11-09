// poker_probe.cpp -- the oracle's probe for the PokerRace's cards (core/game/race_poker.c). The reference worlds never complete a lap
// with the cards in play (the riders do not get round in the boot's frames, and the original's AI riders stop it: no behaviours, 00510290
// reads the null fix-progress behaviour), so a frame hook (Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL
// functions on the live poker race once the countdown runs (the flop is drawn): with the ports installed Verify runs the core function
// and then the original on the same state and compares memory as for a call from the game. Every case runs on a snapshot of the whole
// guest memory and machine state, restored afterwards.
//   REFLEX_PORT=rx_party,rx_PokerProbeHook REFLEX_PORT_VERIFY=1 RX_POKER_PROBE=1 (riders 4: the places 0 .. 3)
//   RX_POKER_PROBE=post: the race's end and Post (below, poker_post)
// Cases: the "Lap Completed" handler 00571d60 for every place (the standings +0x2d0 permuted), laps 0 .. 5, with and without an offered
// choice; the pick 00513630; the hand evaluation 005723c0 for every kind of hand; the race's end vf34 005719f0 (the standings sorted
// with vf43); vf43 itself; WindDown vf27; vf42; 00571d40; RaceDriver vf16 / vf17; the driver's vf3;
// RuleOfftrack's vf5 / 0051ae90 / 0051af60 / vf4 on the local driver's rule.
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;

namespace {
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
float rf(uint32_t a) { const uint32_t b = rd(a); float f; std::memcpy(&f, &b, 4); return f; }

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

uint32_t find_activity() {
    const uint32_t g = rd(0x00a946b8);
    for (uint32_t s = 0; g && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0) || rd(p + 4) == 0 || rd(p + 0x10) != 0) continue;
        const uint32_t a = rd(p + 0x94);
        if (a && mapped(a, 4) && rd(a) == 0x00944b74) return a;
    }
    return 0;
}
uint32_t card(uint32_t rank, uint32_t suit) { return rank * 4 + suit; }

int g_cases = 0;
template <class F> void run_case(reflex_vm::Machine& m, Pages& pages, F&& body) {
    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[poker] ERROR missing reversible host state\n"); return; }
    body();
    g_cases++;
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
void call_this(uint32_t fn, uint32_t ecx, int nargs, const uint32_t* args) {
    rx_regs in; std::memset(&in, 0, sizeof in); in.r[1] = ecx;
    rx_regs out; std::memset(&out, 0, sizeof out);
    rx_call_original_ex(fn, &in, &out, nargs, args);
}
void call_regs(uint32_t fn, int reg, uint32_t v, int nargs = 0, const uint32_t* args = nullptr) {
    rx_regs in; std::memset(&in, 0, sizeof in); in.r[reg] = v;
    rx_regs out; std::memset(&out, 0, sizeof out);
    rx_call_original_ex(fn, &in, &out, nargs, args);
}
/* the driver d at place p of the standings (+0x2d0, the activity's +0x26c drivers) */
void put_at(uint32_t act, uint32_t d, uint32_t p) {
    const uint32_t n = rd(act + 0x26c);
    for (uint32_t i = 0; i < n; i++) if (rd(act + 0x2d0 + 4 * i) == d && i != p) {
        const uint32_t o = rd(act + 0x2d0 + 4 * p);
        wr(act + 0x2d0 + 4 * p, d); wr(act + 0x2d0 + 4 * i, o);
        return;
    }
}
void set_hand(uint32_t act, uint32_t d, const uint32_t* hole, const uint32_t* comm) {
    for (int k = 0; k < 3; k++) { wr(d + 0x1134 + 4 * k, hole[k]); if (hole[k] < 52) wr(act + 0x13e668 + 0xc * hole[k], 0); }
    for (int k = 0; k < 4; k++) { wr(act + 0x13e650 + 4 * k, comm[k]); if (comm[k] < 52) wr(act + 0x13e668 + 0xc * comm[k], 0); }
}
}  // namespace

// RX_POKER_PROBE=post (w-partyresults): the race's end and Post, which no offline poker world reaches (the one human never finishes
// four laps in a boot, AI riders fault): RX_POKER_POST_AFTER (120) frames into the race (state 2) the ORIGINAL SetState 005586c0 puts
// the activity into WindDown (3) as its end check vf42 does when a human finished, once; from there the game runs its own WindDown and
// Post (vf27 / vf29 / the race stub's Post 00569a90, "Show Activity Results", with REFLEX_EVENT_UI the results page) under Verify. At
// the first Post frame, on snapshots: vf29 with the Post clock +0xff064 at 0 (the results shown again), 0.5 (not), a NaN (skipped), and
// with the class's vf57 answer as is.
static uint32_t poker_post(reflex_vm::Machine& m) {
    static uint32_t racing = 0, phase = 0;
    g_m = &m;
    const uint32_t act = find_activity();
    if (!act) return reflex_vm::RUN_ORIGINAL;
    const uint32_t st = rd(act + 0x21c);
    if (phase == 0 && st == 2 && ++racing >= uint32_t(std::getenv("RX_POKER_POST_AFTER") ? std::atoi(std::getenv("RX_POKER_POST_AFTER")) : 120)) {
        phase = 1;
        std::fprintf(stderr, "[poker] activity %08x racing %u frames -> SetState(3)\n", act, racing);
        rx_regs in{}, out{}; const uint32_t a[2] = {act, 3};   /* 005586c0 (stdcall activity, state; RET 8) */
        rx_call_original_ex(0x005586c0, &in, &out, 2, a);
        return reflex_vm::RUN_ORIGINAL;
    }
    if (phase == 1 && st == 4) {
        phase = 2;
        std::fprintf(stderr, "[poker] Post entered: clock %g\n", double(rf(act + 0xff064)));
        Pages pages(m.mem);
        const uint32_t dt = fb(1.0f / 60.0f);
        for (uint32_t clock : {fb(0.0f), fb(0.5f), 0x7fc00000u, fb(-2.0f)})
            run_case(m, pages, [&] { wr(act + 0xff064, clock); call_this(0x00571ac0, act, 1, &dt); });
        /* PokerRaceDriver::vf2 00513570 (a jump to RaceDriver::vf2), which no teardown of the poker race calls (the probe's teardown boot:
           RX_TD_PROBE with vf3 / sdtor excluded), and vf1 00513560 */
        const uint32_t d0 = rd(rd(act + 0x268));
        if (d0) { run_case(m, pages, [&] { call_this(0x00513570, d0, 0, nullptr); }); run_case(m, pages, [&] { call_this(0x00513560, d0, 0, nullptr); }); }
        std::fprintf(stderr, "[poker] %d Post cases done (state restored)\n", g_cases);
    }
    return reflex_vm::RUN_ORIGINAL;
}

RX_PORT(0x0084ed20, PokerProbeHook, "thunk", 0) {
    if (!std::getenv("RX_POKER_PROBE")) return reflex_vm::RUN_ORIGINAL;
    if (std::strcmp(std::getenv("RX_POKER_PROBE"), "post") == 0) return poker_post(m);
    static bool done = false;
    if (done) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    const uint32_t act = find_activity();
    if (!act || rd(act + 0x21c) != 1 || rd(act + 0x13e650) == 0xffffffffu) return reflex_vm::RUN_ORIGINAL;   /* the countdown, the flop drawn */
    done = true;
    const uint32_t n = rd(act + 0x26c);
    std::fprintf(stderr, "[poker] activity %08x drivers %u flop %d %d %d\n", act, n, int(rd(act + 0x13e650)), int(rd(act + 0x13e654)), int(rd(act + 0x13e658)));
    Pages pages(m.mem);
    const uint32_t d0 = rd(rd(act + 0x268));
    /* the "Lap Completed" handler: every place, laps 0 .. 5, with / without an offered choice */
    for (uint32_t p = 0; p < n && p < 4; p++)
        for (uint32_t lap = 0; lap <= 5; lap++)
            for (int offered = 0; offered < 2; offered++) {
                if (offered && (lap == 0 || lap == 3 || lap == 5)) continue;
                run_case(m, pages, [&] {
                    put_at(act, d0, p);
                    if (offered) { wr(d0 + 0x1140, card(5, 1)); wr(d0 + 0x1144, card(6, 2)); wr(d0 + 0x1148, card(7, 3)); wr(d0 + 0x114c, 0xffffffffu); }
                    const uint32_t a[4] = {d0, lap, 0, 0};
                    call_this(0x00571d60, act, 4, a);
                });
            }
    run_case(m, pages, [&] { wr(act + 0x13e65c, card(2, 2)); const uint32_t a[4] = {d0, 3, 0, 0}; call_this(0x00571d60, act, 4, a); });   /* the fourth card drawn already */
    /* the pick: no choice; a choice with no card button pressed, at each lap count */
    run_case(m, pages, [&] { call_regs(0x00513630, 6, d0); });
    for (uint32_t laps : {1u, 2u, 3u, 4u})
        run_case(m, pages, [&] { wr(d0 + 0xf1c, laps); wr(d0 + 0x1140, card(5, 1)); wr(d0 + 0x1144, card(6, 2)); call_regs(0x00513630, 6, d0); });
    /* the hands */
    struct H { uint32_t hole[3], comm[4]; const char* name; };
    const H hands[] = {
        {{card(12, 0), card(11, 0), card(0, 1)}, {card(10, 0), card(9, 0), card(8, 0), card(1, 2)}, "royal flush"},
        {{card(7, 1), card(6, 1), card(11, 3)}, {card(5, 1), card(4, 1), card(3, 1), card(0, 0)}, "straight flush"},
        {{card(5, 0), card(5, 1), card(12, 3)}, {card(5, 2), card(5, 3), card(0, 0), card(1, 1)}, "four of a kind"},
        {{card(0, 3), card(11, 0), card(11, 1)}, {card(11, 2), card(11, 3), card(3, 0), card(4, 1)}, "four of a kind (high)"},
        {{card(11, 0), card(11, 1), card(2, 3)}, {card(11, 2), card(2, 0), card(7, 1), card(0, 3)}, "full house"},
        {{card(1, 0), card(1, 1), card(7, 3)}, {card(7, 2), card(7, 0), card(0, 1), card(3, 3)}, "full house (pair low)"},
        {{card(0, 2), card(5, 2), card(9, 3)}, {card(7, 2), card(11, 2), card(2, 2), card(1, 0)}, "flush"},
        {{card(3, 0), card(4, 1), card(11, 3)}, {card(5, 2), card(6, 3), card(7, 0), card(0, 1)}, "straight"},
        {{card(10, 0), card(10, 1), card(1, 3)}, {card(10, 2), card(6, 3), card(3, 0), card(0, 1)}, "three of a kind"},
        {{card(9, 0), card(9, 1), card(2, 3)}, {card(2, 2), card(7, 3), card(11, 0), card(0, 1)}, "two pair"},
        {{card(8, 0), card(8, 1), card(1, 3)}, {card(4, 2), card(6, 3), card(11, 0), card(0, 1)}, "a pair"},
        {{card(12, 0), card(7, 1), card(1, 3)}, {card(4, 2), card(6, 3), card(11, 0), card(0, 1)}, "high card"},
        {{card(12, 0), card(0, 1), card(1, 3)}, {card(2, 2), card(3, 3), card(9, 0), card(6, 1)}, "the wheel (no straight here)"},
        {{card(4, 0), card(4, 1), 0xffffffffu}, {card(4, 2), card(4, 3), card(9, 0), card(6, 1)}, "no third card"},
    };
    for (const H& h : hands)
        run_case(m, pages, [&] { set_hand(act, d0, h.hole, h.comm); const uint32_t a[2] = {act, d0}; rx_regs in{}, out{}; rx_call_original_ex(0x005723c0, &in, &out, 2, a); });
    /* the race's end: every driver's hand evaluated, the standings sorted (vf43 on the hands when all finished the four laps) */
    for (int all4 = 0; all4 < 2; all4++)
        run_case(m, pages, [&] {
            for (uint32_t i = 0; i < n; i++) {
                const uint32_t d = rd(rd(act + 0x268) + 4 * i);
                const H& h = hands[(i * 3 + 2) % 12];
                set_hand(act, d, h.hole, h.comm);
                if (all4) wr(d + 0xf1c, 4);
                if (i == 1) { wr(d + 0x1140, card(12, 2)); wr(d + 0x1144, card(12, 3)); }
            }
            const uint32_t a[1] = {0};
            call_this(0x005719f0, act, 1, a);
        });
    /* the standings' order (vf43: both finished the four laps -> the hands; else the race stub's order) */
    if (n >= 2) {
        const uint32_t d1 = rd(rd(act + 0x268) + 4);
        for (int c = 0; c < 6; c++)
            run_case(m, pages, [&] {
                set_hand(act, d0, hands[c].hole, hands[c].comm); const uint32_t a[2] = {act, d0}; rx_regs in{}, out{}; rx_call_original_ex(0x005723c0, &in, &out, 2, a);
                set_hand(act, d1, hands[c + 5].hole, hands[c + 5].comm); const uint32_t b[2] = {act, d1}; rx_call_original_ex(0x005723c0, &in, &out, 2, b);
                if (c == 3) { for (uint32_t k = 0x1150; k <= 0x1164; k += 4) wr(d1 + k, rd(d0 + k)); }   /* a tie */
                wr(d0 + 0xf1c, c == 4 ? 3 : 4); wr(d1 + 0xf1c, 4);
                const uint32_t x[3] = {c == 5 ? d1 : d0, c == 5 ? d0 : d1, uint32_t(c & 1)};
                call_this(0x00571b60, act, 3, x);
            });
    }
    /* WindDown (vf27: the pending picks, then Activity::vf27) */
    {
        const uint32_t dt = fb(1.0f / 60.0f);
        run_case(m, pages, [&] { wr(d0 + 0x1140, card(9, 1)); wr(d0 + 0x1144, card(9, 2)); call_this(0x00571980, act, 1, &dt); });
        /* (no Post case: vf29 at the countdown faults in the ORIGINAL, 006aa8e4 under 00571b14: no results context yet) */
    }
    /* the race's end check (vf42): nobody finished; the local human finished (+0x70 set through the class's vf61) */
    run_case(m, pages, [&] { call_this(0x005715f0, act, 0, nullptr); });
    run_case(m, pages, [&] { wr(act + 0x2a4, fb(-1.0f)); call_this(0x005715f0, act, 0, nullptr); });
    /* a removed network object (kind 7 unlinks the NetPokerRace object, others nothing); RaceDriver vf16 / vf17; the driver's vf3 */
    for (uint32_t kind : {7u, 3u}) run_case(m, pages, [&] { wr(act + 0x13e8d0, 0x12345678); const uint32_t a[2] = {kind, 0}; call_this(0x00571d40, act, 2, a); });
    run_case(m, pages, [&] { const uint32_t a[2] = {0, 0}; call_this(0x00512bc0, d0, 2, a); });   /* (kind 1 needs a live network object: 00884950 faults on 0) */
    run_case(m, pages, [&] { call_this(0x00512c30, d0, 0, nullptr); });
    run_case(m, pages, [&] { wr(d0 + 0x1134, card(3, 3)); call_this(0x00513580, d0, 0, nullptr); });
    /* RuleOfftrack (the local driver's: its rules +0xa0, count +0xee4) */
    uint32_t rule = 0;
    for (uint32_t k = 0; k < rd(d0 + 0xee4) && k < 18; k++) if (rd(rd(d0 + 0xa0 + 4 * k)) == 0x0093fa90) rule = rd(d0 + 0xa0 + 4 * k);
    if (rule) {
        const uint32_t dt = fb(1.0f / 60.0f);
        run_case(m, pages, [&] { call_this(0x0051abc0, rule, 1, &dt); });                                              /* on the track */
        run_case(m, pages, [&] { wr(rule + 0x50, 0); call_this(0x0051abc0, rule, 1, &dt); });                          /* off: found again */
        run_case(m, pages, [&] { const uint32_t v = rd(rd(d0 + 0x74) + 0x1ac); wr(v + 0xc4, fb(rf(v + 0xc4) + 400.0f)); call_this(0x0051abc0, rule, 1, &dt); });   /* lost (far above) */
        run_case(m, pages, [&] { const uint32_t v = rd(rd(d0 + 0x74) + 0x1ac); wr(rule + 0x50, 0); wr(v + 0xc4, fb(rf(v + 0xc4) + 400.0f)); call_this(0x0051abc0, rule, 1, &dt); });   /* still off, clock */
        run_case(m, pages, [&] { call_regs(0x0051ae90, 2, rule); });
        run_case(m, pages, [&] { wr(rule + 0x58, 1); call_regs(0x0051ae90, 2, rule); });
        run_case(m, pages, [&] { call_regs(0x0051af60, 0, rule); });
        const uint32_t on0 = 0, on1 = 1;
        run_case(m, pages, [&] { call_this(0x0051aba0, rule, 1, &on0); });
        run_case(m, pages, [&] { call_this(0x0051aba0, rule, 1, &on1); });
    } else std::fprintf(stderr, "[poker] no RuleOfftrack on the local driver\n");
    std::fprintf(stderr, "[poker] %d cases done (state restored)\n", g_cases);
    return reflex_vm::RUN_ORIGINAL;
}
