// race_champ_cases.cpp -- the oracle's probe for the championship activity virtuals no world reaches (development only).
// The arcade stimulus of race_champ_probe.cpp builds a championship activity and runs it, which reaches the construction, the
// per-frame virtuals and the stunt handlers; this probe reaches the rest (destructors, teardown, restart, the camera placements, the
// player takeover, the event-listener templates, the intro classes whose scenarios the arcade event cannot run, the waypoint classes'
// constructors). A frame hook (Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL addresses on the live
// activity at one frame, each case on a snapshot of the whole guest memory and machine state, restored afterwards, so with the ports
// installed Verify runs the core function and then the original on the same state and compares memory and the declared registers
// exactly as for a call from the game.
//   REFLEX_PORT=<the championship ports>,rx_ChampCasesHook REFLEX_PORT_VERIFY=1 RX_CASES=race|wp [RX_CASES_FRAME=<n>] [RX_CASES_ONLY=<name>]
// The class of a case is put on the live activity for the call (its vtable and, for the intro, its type): that is the state the
// class's own constructor leaves. No game logic here.
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }

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

// the live championship activity of the world: the local human's player's activity (+0x94)
uint32_t find_activity() {
    const uint32_t g = rd(0x00a946b8);
    if (!g) return 0;
    for (uint32_t s = 0; s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) == 0 || rd(p + 0x10) != 0) continue;
        const uint32_t a = rd(p + 0x94);
        if (a && mapped(a, 0x1100)) return a;
    }
    return 0;
}

constexpr uint32_t kIntro = 0x00943654, kPractice = 0x00943754, kHeat = 0x00943854, kMain = 0x00943954, kLcq = 0x00943a54;
constexpr uint32_t kWpIntro = 0x00943f14, kWpPractice = 0x00943d14, kWpMain = 0x00943e14;

// A case: the original address, the class it is called as (vtable; type; 0 = leave), the registers and stack arguments (built from
// the live objects by the lambda), and a note.
struct Ctx {
    uint32_t act, driver0, driver1, player0, player1, channel;
};
struct Case {
    const char* name;
    uint32_t addr;
    uint32_t vtable, type;
    int nargs;
    uint32_t (*arg)(const Ctx&, int);   // stack argument i
    uint32_t (*reg)(const Ctx&, int);   // register r (0 EAX, 1 ECX, 6 ESI, 7 EDI)
    uint32_t mode;                      // activity mode to run in (0 = leave)
    void (*setup)(const Ctx&) = nullptr;   // state the branch needs (put back with the snapshot)
};
constexpr uint32_t F033 = 0x3d072b02;   /* 0.033f */
void set_last_lap_near(const Ctx& c) {
    wr(c.driver0 + 0xf1c, rd(c.act + 0xff070) - 1);
    const uint32_t node = rd(c.driver0 + 0x90);
    for (uint32_t k = 0; k < 3; k++) wr(node + 4 * k, rd(c.driver0 + 0x80 + 4 * k));   /* the driver on the node: the plane distance is +0x4f4 */
    wr(node + 0x4f4, 0x41f00000);   /* 30 */
    wr(c.act + 0xff078, 0x40000000);   /* the intensity timer 2.0 */
}
void set_last_lap_far(const Ctx& c) {
    set_last_lap_near(c);
    wr(rd(c.driver0 + 0x90) + 0x4f4, 0x43fa0000);   /* 500 */
}
void set_results_timer(const Ctx& c) { wr(c.act + 0xff064, 0xbf800000); }   /* -1.0 */
void set_online(const Ctx&) { const uint32_t s = rd(0x00da6cb0); wr(s + 0x1c4c, 1); wr(s + 0x1b0, 0); }
void set_clip(const Ctx& c) { wr(0x00a942b8, c.act); wr(0x00a942b4, 1); }
void set_gate(const Ctx& c) { wr(c.driver0 + 0x10d4, 0); }
uint32_t a_none(const Ctx&, int) { return 0; }
uint32_t r_act(const Ctx& c, int r) { return r == 1 ? c.act : 0; }
uint32_t a_flags1(const Ctx&, int) { return 1; }
uint32_t a_flags0(const Ctx&, int) { return 0; }
uint32_t a_arg0(const Ctx&, int) { return 0x009426b4; }
uint32_t a_dt(const Ctx&, int) { return F033; }
uint32_t a_dt_big(const Ctx&, int) { return 0x41200000; }
uint32_t a_player(const Ctx& c, int) { return c.player0; }
uint32_t a_drivers(const Ctx& c, int i) { return i == 0 ? c.driver0 : c.driver1; }
uint32_t a_driver_place1(const Ctx& c, int i) { return i == 0 ? c.driver1 : 1; }
uint32_t a_driver_place9(const Ctx& c, int i) { return i == 0 ? c.driver0 : 9; }
uint32_t a_player_desc(const Ctx& c, int i) { return i == 0 ? c.player0 : (i == 1 ? c.player0 + 0x100 : 0); }
uint32_t r_name_begin(const Ctx&, int r) { return r == 1 ? 0x0095ba28 : 0; }
uint32_t r_name_failed(const Ctx&, int r) { return r == 1 ? 0x00960764 : 0; }
uint32_t a_self_method_begin(const Ctx& c, int i) { return i == 0 ? c.act : 0x0057e7c0; }
uint32_t a_self_method_failed(const Ctx& c, int i) { return i == 0 ? c.act : 0x0057e7f0; }
uint32_t r_channel(const Ctx& c, int r) { return r == 0 ? c.channel : 0; }
uint32_t a_act(const Ctx& c, int) { return c.act; }
uint32_t r_player_eax(const Ctx& c, int r) { return r == 0 ? c.player0 : 0; }

const std::vector<Case>& race_cases() {
    static const std::vector<Case> v = {
        // destructors (flags 1: the memory freed)
        {"heat_sdtor", 0x0057dfb0, kHeat, 0, 1, a_flags1, r_act, 0},
        {"intro_sdtor", 0x0057e050, kIntro, 0x1f, 1, a_flags1, r_act, 0},
        {"lcq_sdtor", 0x0057e1b0, kLcq, 0, 1, a_flags1, r_act, 0},
        {"main_sdtor", 0x0057e310, kMain, 0, 1, a_flags1, r_act, 0},
        {"practice_sdtor", 0x0057e820, kPractice, 0, 1, a_flags1, r_act, 0},
        {"heat_sdtor0", 0x0057dfb0, kHeat, 0, 1, a_flags0, r_act, 0},
        // vf2 / vf3 / vf5
        {"intro_vf2", 0x0057e090, kIntro, 0x1f, 1, a_arg0, r_act, 0},
        {"heat_vf2", 0x0057e1f0, kHeat, 0, 1, a_arg0, r_act, 0},
        {"main_vf2", 0x0057e350, kMain, 0, 1, a_arg0, r_act, 0},
        {"heat_vf3", 0x0057e210, kHeat, 0, 0, a_none, r_act, 0},
        {"main_vf3", 0x0057e390, kMain, 0, 0, a_none, r_act, 0},
        // the virtuals of the frame loop
        {"vf15", 0x0057eaf0, kHeat, 0, 0, a_none, r_act, 0},
        {"vf17", 0x0057e270, kHeat, 0, 0, a_none, r_act, 0},
        {"intro_vf25", 0x0057e1a0, kIntro, 0x1f, 1, a_dt, r_act, 0},
        {"heat_vf28", 0x0057e000, kHeat, 0, 1, a_dt, r_act, 0},
        {"lcq_vf28", 0x0057e2d0, kLcq, 0, 1, a_dt, r_act, 0},
        {"practice_vf28", 0x0057e860, kPractice, 0, 1, a_dt, r_act, 0},
        {"main_vf28", 0x0057e4f0, kMain, 0, 1, a_dt, r_act, 0},
        {"main_vf28_big", 0x0057e4f0, kMain, 0, 1, a_dt_big, r_act, 0},
        {"main_vf28_last_lap", 0x0057e4f0, kMain, 0, 1, a_dt, r_act, 0, set_last_lap_near},
        {"main_vf28_last_lap_far", 0x0057e4f0, kMain, 0, 1, a_dt, r_act, 0, set_last_lap_far},
        {"heat_vf29_results", 0x0057c010, kHeat, 0, 1, a_dt, r_act, 0, set_results_timer},
        {"main_vf29_results", 0x0057e6b0, kMain, 0, 1, a_dt, r_act, 0, set_results_timer},
        {"activity_vf45_online", 0x00557570, kHeat, 0, 2, a_drivers, r_act, 0, set_online},
        {"activity_vf16_clip", 0x00557b40, kHeat, 0, 0, a_none, r_act, 0, set_clip},
        {"heat_vf29", 0x0057c010, kHeat, 0, 1, a_dt, r_act, 0},
        {"activity_vf29_results", 0x00559a00, kHeat, 0, 1, a_dt, r_act, 0, set_results_timer},
        {"activity_vf29_results_main", 0x00559a00, kMain, 0, 1, a_dt, r_act, 0, set_results_timer},
        {"activity_vf29", 0x00559a00, kHeat, 0, 1, a_dt, r_act, 0},
        {"main_vf29", 0x0057e6b0, kMain, 0, 1, a_dt, r_act, 0},
        {"main_vf34", 0x0057e700, kMain, 0, 1, a_none, r_act, 0},
        {"vf35", 0x0057e720, kHeat, 0, 1, a_none, r_act, 0},
        {"heat_vf38", 0x0057dff0, kHeat, 0, 1, a_none, r_act, 0},
        {"main_vf38", 0x0057e4b0, kMain, 0, 1, a_none, r_act, 0},
        {"vf52", 0x0057ea30, kHeat, 0, 0, a_none, r_act, 0},
        {"main_vf52", 0x0057e760, kMain, 0, 0, a_none, r_act, 0},
        {"vf9", 0x0055eb70, kHeat, 0, 0, a_none, r_act, 0},
        // the base class
        {"activity_vf16", 0x00557b40, kHeat, 0, 0, a_none, r_act, 0},
        {"activity_vf20", 0x0055d120, kHeat, 0, 1, a_player, r_act, 0},
        {"activity_vf45", 0x00557570, kHeat, 0, 2, a_drivers, r_act, 0},
        {"race_stub_vf45", 0x00567ac0, kHeat, 0, 2, a_drivers, r_act, 0},
        {"place_swap", 0x005693d0, kHeat, 0, 2, a_driver_place1, r_act, 0},
        {"place_swap_last", 0x005693d0, kHeat, 0, 2, a_driver_place9, r_act, 0},
        {"place_swap_practice", 0x005693d0, kHeat, 0, 2, a_driver_place1, r_act, 2},
        // the event-listener templates and the main event's handlers
        {"stunt_listen_begin", 0x0058a9a0, kMain, 0, 2, a_self_method_begin, r_name_begin, 0},
        {"stunt_listen_failed", 0x0058a9a0, kMain, 0, 2, a_self_method_failed, r_name_failed, 0},
        {"stunt_unlisten_begin", 0x0058ab20, kMain, 0, 2, a_self_method_begin, r_name_begin, 0},
        {"stunt_unlisten_failed", 0x0058ab20, kMain, 0, 2, a_self_method_failed, r_name_failed, 0},
        {"physics_listen", 0x0058aa60, kMain, 0, 1, a_act, a_none, 0},
        {"physics_unlisten", 0x0058ab80, kMain, 0, 1, a_act, a_none, 0},
        {"lap_unlisten", 0x0058a940, kHeat, 0, 1, a_act, r_channel, 0},
        {"stunt_begin", 0x0057e7c0, kMain, 0, 2, a_player_desc, r_act, 0},
        {"stunt_failed", 0x0057e7f0, kMain, 0, 2, a_player_desc, r_act, 0},
        {"physics_stunt_begin", 0x0057e790, kMain, 0, 3, a_player_desc, r_act, 0},
        {"player_restart_reset", 0x0054e190, kHeat, 0, 0, a_none, r_player_eax, 0},
    };
    return v;
}
const std::vector<Case>& wp_cases() {
    static const std::vector<Case> v = {
        {"wp_intro_sdtor", 0x0057e880, kWpIntro, 0, 1, a_flags1, r_act, 0},
        {"wp_main_sdtor", 0x0057e960, kWpMain, 0, 1, a_flags1, r_act, 0},
        {"wp_practice_sdtor", 0x0057eaa0, kWpPractice, 0, 1, a_flags1, r_act, 0},
        {"wp_intro_vf2", 0x0057e8c0, kWpIntro, 0, 1, a_arg0, r_act, 0},
        {"wp_main_vf2", 0x0057e9b0, kWpMain, 0, 1, a_arg0, r_act, 0},
        {"wp_vf3", 0x00578b30, kWpMain, 0, 0, a_none, r_act, 0},
        {"wp_intro_vf3", 0x0057e8f0, kWpIntro, 0, 0, a_none, r_act, 0},
        {"wp_intro_vf5", 0x0057e8e0, kWpIntro, 0, 0, a_none, r_act, 0},
        {"wp_vf16", 0x00578cc0, kWpMain, 0, 0, a_none, r_act, 0},
        {"wp_main_vf16", 0x0057e9c0, kWpMain, 0, 0, a_none, r_act, 0},
        {"wp_practice_vf16", 0x0057eb60, kWpPractice, 0, 0, a_none, r_act, 0},
        {"wp_vf20", 0x0057a3e0, kWpMain, 0, 1, a_player, r_act, 0},
        {"wp_vf20_gate", 0x0057a3e0, kWpMain, 0, 1, a_player, r_act, 0, set_gate},
        {"wp_intro_vf25", 0x0057e910, kWpIntro, 0, 1, a_dt, r_act, 0},
        {"wp_main_vf28", 0x0057e9e0, kWpMain, 0, 1, a_dt, r_act, 0},
        {"wp_vf35", 0x0057e9f0, kWpMain, 0, 1, a_none, r_act, 0},
        {"wp_vf37", 0x0057e900, kWpMain, 0, 1, a_none, r_act, 0},
        {"wp_main_vf42", 0x0057e9d0, kWpMain, 0, 0, a_none, r_act, 0},
        {"wp_lap_unlisten", 0x0058a420, kWpMain, 0, 1, a_act, r_channel, 0},
        {"vf15", 0x0057eaf0, kWpMain, 0, 0, a_none, r_act, 0},
        {"vf17", 0x0057e270, kWpMain, 0, 0, a_none, r_act, 0},
        {"vf52", 0x0057ea30, kWpMain, 0, 0, a_none, r_act, 0},
        {"activity_vf16", 0x00557b40, kWpMain, 0, 0, a_none, r_act, 0},
        {"activity_vf20", 0x0055d120, kWpMain, 0, 1, a_player, r_act, 0},
        {"activity_vf45", 0x00557570, kWpMain, 0, 2, a_drivers, r_act, 0},
    };
    return v;
}

void run_case(reflex_vm::Machine& m, const Case& c, const Ctx& ctx) {
    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[cases] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    // the records object [0xa946c0] is nulled for the teardown cases: MXRecords_Leave's seam is not closed in the oracle (as
    // race_teardown_probe.cpp); the restart reads it, so it stays for the others
    if (std::strstr(c.name, "vf3") || std::strstr(c.name, "sdtor")) wr(0x00a946c0, 0);
    if (c.vtable) wr(ctx.act, c.vtable);
    if (c.type) wr(ctx.act + 0x23c, c.type);
    if (c.mode) wr(ctx.act + 0x238, c.mode);
    if (c.setup) c.setup(ctx);
    uint32_t args[4] = {0, 0, 0, 0};
    for (int i = 0; i < c.nargs; i++) args[i] = c.arg(ctx, i);
    rx_regs in; std::memset(&in, 0, sizeof in);
    in.r[0] = c.reg(ctx, 0); in.r[1] = c.reg(ctx, 1); in.r[6] = c.reg(ctx, 6); in.r[7] = c.reg(ctx, 7);
    rx_regs out; std::memset(&out, 0, sizeof out);
    rx_call_original_ex(c.addr, &in, &out, c.nargs, args);
    std::fprintf(stderr, "[cases] %-24s %08x done\n", c.name, c.addr);
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, ChampCasesHook, "thunk", 0) {
    const char* which = std::getenv("RX_CASES");
    if (!which) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_CASES_FRAME") ? uint32_t(std::atoi(std::getenv("RX_CASES_FRAME"))) : 900;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    const uint32_t act = find_activity();
    if (!act) { std::fprintf(stderr, "[cases] no activity at frame %u\n", g_frame); return reflex_vm::RUN_ORIGINAL; }
    Ctx ctx;
    ctx.act = act;
    const uint32_t d = rd(act + 0x268);
    ctx.driver0 = rd(d); ctx.driver1 = rd(d + 4);
    ctx.player0 = rd(ctx.driver0 + 0x74); ctx.player1 = rd(ctx.driver1 + 0x74);
    ctx.channel = rd(act + 0x290);
    std::fprintf(stderr, "[cases] frame %u activity %08x vt %08x type %u mode %u drivers %u/%u\n", g_frame, act, rd(act), rd(act + 0x23c), rd(act + 0x238),
                 rd(act + 0x26c), rd(act + 0x280));
    const std::string only = std::getenv("RX_CASES_ONLY") ? std::getenv("RX_CASES_ONLY") : "";
    const auto& list = std::string(which) == "wp" ? wp_cases() : race_cases();
    int n = 0;
    for (const Case& c : list) {
        if (!only.empty() && only != c.name) continue;
        run_case(m, c, ctx);
        n++;
    }
    std::fprintf(stderr, "[cases] %d cases done\n", n);
    return reflex_vm::RUN_ORIGINAL;
}
