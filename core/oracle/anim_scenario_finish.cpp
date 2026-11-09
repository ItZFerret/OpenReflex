// anim_scenario_finish.cpp -- oracle adapters for core/game/anim_scenario_finish.c (r: 0 EAX, 1 ECX, 2 EDX, 3 EBX, 6 ESI, 7 EDI):
// the post-race presentation scenarios RaceFinishScenario / FMXFinishScenario / FMXResultsScenario and their podium helpers.
// The worlds reach them only at Post: a natural finish (REFLEX_TEST_LAPS-style short races) or the forced WindDown of the probe hook
// below (RX_FINISH_PROBE). Group rx_anim.
//   REFLEX_PORT=rx_anim,... REFLEX_PORT_VERIFY=1 RX_FINISH_PROBE=1 [RX_FINISH_FRAME=<n>] [RX_FINISH_STATE=3]
// The hook (Engine::Update 0084ed20, outside every verified call), at frame n (default 1300) on the local human's activity:
//  RX_FINISH_PROBE=1: calls the ORIGINAL Activity SetState (005586c0) with RX_FINISH_STATE (default 3, WindDown), exactly as the stop
//    audit's forced WindDown does in the product, and lets the world run on. (The race worlds' original UI render stops at the
//    next frame on a WindDown / Post state set this way: 005c3440 <- DevConsole::vf19 <- UIContainer::vf12 <- Renderer; the Freestyle
//    world reaches Post by its own time limit, so it needs no probe.)
//  RX_FINISH_PROBE=2: on a snapshot of the whole guest memory and machine state (restored afterwards; the run continues untouched):
//    the ORIGINAL SetState(4) (the Post enter: records, standings), then the activity's ORIGINAL Post update (its vf29: the Post
//    sub-states, the podium setup 0074ada0, the finish scenario's start / steps / end) RX_FINISH_STEPS times (default 2400) at
//    1/60 s, stopping when the Post sub-state reaches 2. Every port below called inside is verified against the original on the same
//    state, as for a call from the game. RX_FINISH_KIND=<k> runs the case with the activity's kind +0x238 = k.
//  RX_FINISH_PROBE=3 (at a frame in Post sub-state 2, after the finish scenario): on a snapshot, the activity's ORIGINAL vf58 (the
//    results page's done: the results scenario started, sub-state 3), then its Post update until the sub-state leaves 3.
//  RX_FINISH_PROBE=4 (in Post, the finish scenario set up): on a snapshot, the finish scenario's ORIGINAL vf13 at steps 7 .. 12, vf1(7)
//    and vf1(9) with the clip recorder playing: the debug-menu / finish-replay paths a Freestyle event does not take by itself.
#include "anim_oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "../game/anim_scenario_finish.h"
}
using namespace reflex_port;

ANIM_PORT(0x0074c590, Podium_RidersSetVisible, "rx_anim", 0) { rx_podium_riders_set_visible(m.r[7], uint8_t(m.arg(0)), m.r[6]); return m.ret(4); }
ANIM_PORT(0x0074c620, Podium_VehiclesSetVisible, "rx_anim", 0) { rx_podium_vehicles_set_visible(m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
ANIM_PORT(0x0074a1e0, RaceFinishScenario_vf0, "rx_anim", 0) { rx_race_finish_vf0(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074a340, RaceFinishScenario_vf1, "rx_anim", 0) { rx_race_finish_vf1(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074a540, RaceFinishScenario_vf6, "rx_anim", 0) { rx_race_finish_vf6(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074dd40, FmxFinish_DebugStandings, "rx_anim", 0) { rx_fmx_finish_debug_standings(m.r[0], m.r[7]); return m.ret(0); }
ANIM_PORT(0x0074df30, FmxFinishScenario_vf0, "rx_anim", 0) { rx_fmx_finish_vf0(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074e050, FmxFinishScenario_vf1, "rx_anim", 0) { rx_fmx_finish_vf1(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074de60, FmxFinishScenario_vf3, "rx_anim", OUT_EAX) { m.r[0] = rx_fmx_finish_vf3(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074e140, FmxFinishScenario_vf6, "rx_anim", 0) { rx_fmx_finish_vf6(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074e2a0, FmxFinishScenario_vf13, "rx_anim", OUT_EAX) { m.r[0] = rx_fmx_finish_vf13(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074fc70, FmxResults_HideCards, "rx_anim", 0) { rx_fmx_results_hide_cards(m.r[0]); return m.ret(0); }
ANIM_PORT(0x0074fae0, FmxResultsScenario_vf0, "rx_anim", 0) { rx_fmx_results_vf0(m.r[1]); return m.ret(0); }

namespace {
uint32_t g_finish_frame = 0;

uint32_t local_activity(reflex_vm::Machine& m) {
    const uint32_t g = rx_rd32(0x00a946b8);
    if (!g) return 0;
    for (uint32_t s = 0; s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!m.mem.mapped(p) || !m.mem.mapped(p + 0x12a0 - 1)) continue;
        if (rx_rd32(p + 4) == 0 || rx_rd32(p + 0x10) != 0) continue;   /* a local human */
        const uint32_t a = rx_rd32(p + 0x94);
        if (a && m.mem.mapped(a)) return a;
    }
    return 0;
}

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

/* the Post sub-state field of the activity class (MXActivityRace / the stub +0x13e640, the Freestyle classes +0x13e5f8) */
uint32_t post_field(uint32_t act) {
    const uint32_t vt = rx_rd32(act);
    return (vt == 0x009441cc || vt == 0x009442cc) ? 0x13e5f8u : 0x13e640u;
}

void post_case(reflex_vm::Machine& m, uint32_t act) {
    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[finish] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    if (const char* k = std::getenv("RX_FINISH_KIND")) rx_wr32(act + 0x238, uint32_t(std::atoi(k)));
    /* RX_FINISH_SERIES=<event kind>: the series manager (0x009e2188) reports a series (+0x190 = 1) whose current event (+0x18c, a
     * stand-in record far below the stack pointer: only its +0xc kind is read) has that kind: the finish selection's series branch
     * (0074a070: 3 / 4 / 6 / 7 the podium, 5 the winner) */
    if (const char* k = std::getenv("RX_FINISH_SERIES")) {
        const uint32_t ev = 0x0f000000u;   /* an unmapped page below the heaps, mapped for the case (Pages::restore unmaps it) */
        if (!m.mem.mapped(ev)) { static const uint8_t zero[4096] = {}; m.mem.map(ev, zero, sizeof zero); }
        for (uint32_t o = 0; o < 0x20; o += 4) rx_wr32(ev + o, 0);
        rx_wr32(ev + 0xc, uint32_t(std::atoi(k)));
        rx_wr32(0x009e2318, 1); rx_wr32(0x009e2314, ev);
    }
    /* RX_FINISH_CLIP=<slot>: that recorded clip slot (0x00d323c8 + 0x35d0 slot, +0 the type) marked as the finish clip (type 9), so the
     * finish replay step 7 plays it */
    if (const char* c = std::getenv("RX_FINISH_CLIP")) {
        std::fprintf(stderr, "[finish] clip slot types:");
        int first = -1;
        for (uint32_t k = 0; k < 12; k++) {
            const uint32_t t = rx_rd32(0x00d323c8u + 0x35d0u * k);
            std::fprintf(stderr, " %u", t);
            if (t != 0 && first < 0) first = int(k);
        }
        std::fprintf(stderr, "\n");
        const int pick = std::strcmp(c, "auto") == 0 ? first : std::atoi(c);
        if (pick < 0) { std::fprintf(stderr, "[finish] no recorded clip\n"); pages.restore(); host_restore(); return; }
        const uint32_t slot = 0x00d323c8u + 0x35d0u * uint32_t(pick);
        std::fprintf(stderr, "[finish] clip slot %d type %u -> 9\n", pick, rx_rd32(slot));
        rx_wr32(slot, 9);
    }
    const uint32_t post = post_field(act);
    const uint32_t steps = std::getenv("RX_FINISH_STEPS") ? uint32_t(std::atoi(std::getenv("RX_FINISH_STEPS"))) : 2400;
    rx_regs in{}; rx_regs out{};
    if (std::atoi(std::getenv("RX_FINISH_PROBE")) == 4) {
        /* direct calls of the finish scenario's virtuals the game reaches only from the debug menu or a finish replay (Freestyle has
         * none): vf13 (the previous step) at steps 7 .. 12, vf1(7) (0074c590 with 1) and vf1(9) with a replay playing (0074c590 with
         * 0, the replay cameras, 006aa990). The ORIGINAL virtuals are called, so the ports are verified at their calls. */
        const uint32_t sc = (post == 0x13e5f8u) ? act + 0x114250 : act + 0x114260;
        const uint32_t vt = rx_rd32(sc);
        for (uint32_t st = 7; st <= 12; st++) {
            rx_wr32(sc + 0x151b4, st);
            std::memset(&in, 0, sizeof in); in.r[1] = sc;
            rx_call_original_ex(rx_rd32(vt + 13 * 4), &in, &out, 0, nullptr);
            std::fprintf(stderr, "[finish] vf13 at step %u -> %u (eax %u)\n", st, rx_rd32(sc + 0x151b4), out.r[0]);
        }
        const uint32_t seven = 7, nine = 9;
        std::memset(&in, 0, sizeof in); in.r[1] = sc;
        rx_call_original_ex(rx_rd32(vt + 1 * 4), &in, &out, 1, &seven);
        std::fprintf(stderr, "[finish] vf1(7): clip state %u\n", rx_rd32(0x00a942b4));
        rx_wr32(0x00a942b4, 2);
        std::memset(&in, 0, sizeof in); in.r[1] = sc;
        rx_call_original_ex(rx_rd32(vt + 1 * 4), &in, &out, 1, &nine);
        std::fprintf(stderr, "[finish] vf1(9) with a replay playing: clip state %u\n", rx_rd32(0x00a942b4));
        pages.restore();
        host_restore();
        std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
        m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
        return;
    }
    if (std::atoi(std::getenv("RX_FINISH_PROBE")) == 3) {
        /* the results scenario: the activity's vf58 (the results page's done: the replay stopped, the results scenario started,
         * Post sub-state 3), then the Post update until the sub-state leaves 3 */
        in.r[1] = act;
        rx_call_original_ex(rx_rd32(rx_rd32(act) + 58 * 4), &in, &out, 0, nullptr);
        const uint32_t results = (post == 0x13e5f8u) ? act + 0x129420 : act + 0x129430;
        std::fprintf(stderr, "[finish] results: post %u step %u started %u\n", rx_rd32(act + post), rx_rd32(results + 0x151b4),
                     rx_rd32(results + 0x151b0));
        const float rdt = 1.0f / 60.0f; uint32_t rdtw; std::memcpy(&rdtw, &rdt, 4);
        uint32_t j = 0, last = 0xffffffffu;
        for (; j < steps && rx_rd32(act + post) == 3; j++) {
            std::memset(&in, 0, sizeof in); in.r[1] = act;
            rx_call_original_ex(rx_rd32(rx_rd32(act) + 29 * 4), &in, &out, 1, &rdtw);
            if (rx_rd32(results + 0x151b4) != last) {
                last = rx_rd32(results + 0x151b4);
                std::fprintf(stderr, "[finish] results update %u: post %u step %u\n", j, rx_rd32(act + post), last);
            }
        }
        std::fprintf(stderr, "[finish] results done after %u updates: post %u\n", j, rx_rd32(act + post));
        pages.restore();
        host_restore();
        std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
        m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
        return;
    }
    /* the Post enter without the state events: SetState's "<state> Exited" / "Entered" go to the UI's subscribers, whose handlers stop
     * these worlds (005c3440 <- DevConsole::vf19 <- UIComponent::vf12: a null name); Activity::vf36 (the state's Enter: vf41 Post)
     * with the old state, then +0x21c = 4, which is all SetState does besides the exit vf30 and the two events */
    const uint32_t enter[2] = {4, rx_rd32(act + 0x21c)};
    in.r[1] = act;
    rx_call_original_ex(rx_rd32(rx_rd32(act) + 36 * 4), &in, &out, 2, enter);
    rx_wr32(act + 0x21c, 4);
    std::fprintf(stderr, "[finish] case: act %08x vt %08x kind %u state %u post %u\n", act, rx_rd32(act), rx_rd32(act + 0x238),
                 rx_rd32(act + 0x21c), rx_rd32(act + post));
    const float dt = 1.0f / 60.0f; uint32_t dtw; std::memcpy(&dtw, &dt, 4);
    const uint32_t vf29 = rx_rd32(rx_rd32(act) + 29 * 4);
    /* RX_FINISH_PROBE=5: the whole world update (MXWorld::vf5 007674b0: the drivers / activity, the players, the replay cameras)
     * instead of the activity's Post update alone, so the view camera is what the frame leaves in it (RX_FINISH_CAMLOG) */
    const bool whole = std::atoi(std::getenv("RX_FINISH_PROBE")) == 5;
    const uint32_t finish = (post == 0x13e5f8u) ? act + 0x114250 : act + 0x114260;
    uint32_t last_step = 0xffffffffu, i = 0;
    for (; i < steps; i++) {
        std::memset(&in, 0, sizeof in);
        if (whole) { in.r[1] = rx_rd32(act + 0x234); rx_call_original_ex(0x007674b0, &in, &out, 1, &dtw); }
        else { in.r[1] = act; rx_call_original_ex(vf29, &in, &out, 1, &dtw); }
        const uint32_t step = rx_rd32(finish + 0x151b4);
        if (std::getenv("RX_FINISH_CAMLOG") && i % 30 == 0) {   /* the scenario's animated camera (world + 0x218f0, Camera +0x90) */
            const uint32_t cam = rx_rd32(act + 0x234) + 0x218f0 + 0x90;
            auto f = [](uint32_t a) { uint32_t v = rx_rd32(a); float x; std::memcpy(&x, &v, 4); return x; };
            const uint32_t scene = rx_rd32(rx_rd32(act + 0x234) + 0x20138), view = scene ? rx_rd32(scene + 0x19b8) : 0, vc = view ? rx_rd32(view) : 0;
            std::fprintf(stderr, "[finish] cam %u: posed %u pos %.1f %.1f %.1f fwd %.2f %.2f %.2f fov %.3f | view %.1f %.1f %.1f fwd %.2f %.2f %.2f | controller %08x clip %u\n", i,
                         rx_rd32(act + 0x234 + 0x218f0 + 0x2b0) & 0xff, f(cam + 0x10), f(cam + 0x14), f(cam + 0x18), f(cam + 0x40), f(cam + 0x44), f(cam + 0x48), f(cam + 0x50),
                         vc ? f(vc + 0x10) : 0.f, vc ? f(vc + 0x14) : 0.f, vc ? f(vc + 0x18) : 0.f, vc ? f(vc + 0x40) : 0.f, vc ? f(vc + 0x44) : 0.f, vc ? f(vc + 0x48) : 0.f,
                         rx_rd32(rx_rd32(act + 0x234) + 0x21de0), rx_rd32(0x00a942b4));
            { const uint32_t w = rx_rd32(act + 0x234), ctl = rx_rd32(w + 0x21de0), pl = ctl ? rx_rd32(ctl + 0x190) : 0;
              std::fprintf(stderr, "[finish] cam %u replay: controller player %08x flag21c %u directors %08x tv %u current %08x timer %08x\n", i, pl,
                           pl ? (rx_rd32(pl + 0x21c) & 0xff) : 0u, rx_rd32(w + 0x21dd8), rx_rd32(w + 0x21de4) & 0xff, rx_rd32(w + 0x21dfc), rx_rd32(w + 0x21e00)); }
        }
        if (step != last_step) {
            std::fprintf(stderr, "[finish] update %u: post %u scenario step %u started %u clip state %u\n", i, rx_rd32(act + post), step,
                         rx_rd32(finish + 0x151b0), rx_rd32(0x00a942b4));
            last_step = step;
        }
        if (rx_rd32(act + post) >= 2) break;
    }
    std::fprintf(stderr, "[finish] case done after %u updates: post %u\n", i, rx_rd32(act + post));
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

// The forced finish (see the file header). Runs the ORIGINAL 005586c0 (thiscall activity, stack state; RET 4) once.
RX_PORT(0x0084ed20, FinishProbeHook, "thunk", 0) {
    if (!std::getenv("RX_FINISH_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_FINISH_FRAME") ? uint32_t(std::atoi(std::getenv("RX_FINISH_FRAME"))) : 1300;
    static const uint32_t state = std::getenv("RX_FINISH_STATE") ? uint32_t(std::atoi(std::getenv("RX_FINISH_STATE"))) : 3;
    const uint32_t frame = ++g_finish_frame;
    const uint32_t act = local_activity(m);
    if (std::getenv("RX_FINISH_LOG") && act && frame % 30 == 0)
        std::fprintf(stderr, "[finish] frame %u act %08x state %u post %u/%u scenario step %u started %u\n", frame, act,
                     rx_rd32(act + 0x21c), rx_rd32(act + 0x13e640), rx_rd32(act + 0x13e5f8), rx_rd32(act + 0x114260 + 0x151b4),
                     rx_rd32(act + 0x114260 + 0x151b0));
    if (frame != at) return reflex_vm::RUN_ORIGINAL;
    if (!act) { std::fprintf(stderr, "[finish] no activity at frame %u\n", frame); return reflex_vm::RUN_ORIGINAL; }
    if (std::atoi(std::getenv("RX_FINISH_PROBE")) >= 2) { post_case(m, act); return reflex_vm::RUN_ORIGINAL; }
    std::fprintf(stderr, "[finish] frame %u: activity %08x (vt %08x, state %u) -> SetState(%u)\n", frame, act, rx_rd32(act),
                 rx_rd32(act + 0x21c), state);
    rx_regs in{}; rx_regs out{};
    in.r[1] = act;
    rx_call_original_ex(0x005586c0, &in, &out, 1, &state);
    return reflex_vm::RUN_ORIGINAL;
}
