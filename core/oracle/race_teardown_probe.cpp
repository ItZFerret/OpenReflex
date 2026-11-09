// race_teardown_probe.cpp -- the oracle's probe for the race activity's teardown MXActivityRace::vf3 00564c10 and its chain
// (core/game/race_activity_teardown.c, build_activity_teardown.c, build_activity_audio_teardown.c). No reference world tears an
// activity down, so their adapters are never called there (0 calls). A frame hook (Engine::Update 0084ed20, outside every
// verified call) calls the ORIGINAL 00564c10 on the live race activity at one frame, so with the ports installed Verify runs the
// core function and then the original on the same state and compares memory exactly as for a call from the game; the nested
// originals' calls of the other ports are verified the same way.
//   REFLEX_PORT=rx_race,rx_build11,rx_TeardownProbeHook REFLEX_PORT_VERIFY=1 RX_TD_PROBE=1 [RX_TD_FRAME=<n>] [RX_TD_KINDS=k,k,...]
// Each case runs on a snapshot of the whole guest memory and machine state, restored afterwards (the run continues on the
// untouched world). RX_TD_KINDS: the activity kinds (+0x238) to run the teardown with (default: the world's own); "-" = own.
// MXRecords_Leave is skipped (the records object nulled for the case, see teardown_case) unless RX_TD_RECORDS is set.
// RX_TD_NETMGR=1 makes the session block listen to the activity (006deff0's branch).
// RX_TD_CLIP=1 also makes the activity the clip recorder's ([0xa942b8]) for the stub's release branch.
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

uint32_t find_activity() {
    const uint32_t g = rd(0x00a946b8);
    if (!g) return 0;
    for (uint32_t s = 0; s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) == 0 || rd(p + 0x10) != 0) continue;   /* a local human */
        const uint32_t a = rd(p + 0x94);
        if (a && mapped(a, 0x1100)) return a;
    }
    return 0;
}

void teardown_case(reflex_vm::Machine& m, uint32_t act, int kind, bool clip) {
    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[teardown] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    if (kind >= 0) wr(act + 0x238, uint32_t(kind));
    if (clip) { wr(0x00a942b8, act); wr(0x00a942b4, 1); }
    /* MXRecords_Leave (00764560, reached from Activity_OnPlayerLeft for the local human) runs the records owner 007647f0 (its oracle
     * calls are closed since w-actarea, records_leave_highlights.c): without RX_TD_RECORDS the records object [0xa946c0] is null
     * for the case, so Activity_OnPlayerLeft skips that one call (its own guard). */
    if (!std::getenv("RX_TD_RECORDS")) wr(0x00a946c0, 0);
    /* RX_TD_NETMGR=1: the session block 0xda6ca0 listens to this activity (+4 = the activity's channel, +8 = the activity), so
     * 006deff0 takes its unsubscription branch (007074d0 x3 + the inlined one; the keys match no listener: nothing is removed) */
    if (std::getenv("RX_TD_NETMGR")) { wr(0x00da6ca0 + 4, rd(act + 0x290)); wr(0x00da6ca0 + 8, act); }
    const uint32_t ch = rd(act + 0x290), audio = rd(act + 0xfd8);
    std::fprintf(stderr, "[teardown] case kind %d clip %d: act %08x vt %08x drivers %d/%d channel %08x audio %08x (vt %08x cues %08x) world %08x\n",
                 int(rd(act + 0x238)), clip ? 1 : 0, act, rd(act), int(rd(act + 0x26c)), int(rd(act + 0x280)), ch, audio,
                 audio ? rd(audio) : 0, audio ? rd(audio + 0x8c) : 0, rd(act + 0x234));
    rx_regs in; std::memset(&in, 0, sizeof in); in.r[1] = act;
    rx_regs out; std::memset(&out, 0, sizeof out);
    rx_call_original_ex(rd(rd(act) + 0xc), &in, &out, 0, nullptr);   /* the activity's vf3 (00564c10 for MXActivityRace, 0056a520 for the section race) */
    /* RX_TD_DTOR=1: then the area update's next steps for an ended activity (007669f0): Activity::vf6(0) 00557a10 (the area's subscene
     * unloaded, the racing line released) and the deleting destructor vf0(1) (MXActivityRace::sdtor 00564af0) */
    if (std::getenv("RX_TD_DTOR")) {
        const uint32_t zero = 0, one = 1;
        std::memset(&in, 0, sizeof in); in.r[1] = act;
        rx_call_original_ex(rd(act) == 0x00944884 ? rd(rd(act) + 0x18) : 0x00557a10, &in, &out, 1, &zero);   /* (w-snake: MXActivitySnake's own vf6 0056b2d0) */
        std::memset(&in, 0, sizeof in); in.r[1] = act;
        rx_call_original_ex(rd(rd(act)), &in, &out, 1, &one);
        std::fprintf(stderr, "[teardown] case kind %d: vf6 / vf0 done\n", kind);
    }
    std::fprintf(stderr, "[teardown] case kind %d done: registry size %u\n", kind, rd(0x00dcbeb4 + 0x1c));
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}

/* RX_TD_RESPAWN=1: the original 00550cc0 (ESI = the local human player: the respawn of a "ResetPlayer" trigger, 0054f0b0) on a snapshot, so with
 * the port installed Verify runs the core function and then the original (no reference world drives a rider into such a trigger volume). */
uint32_t find_player() {
    const uint32_t g = rd(0x00a946b8);
    for (uint32_t s = 0; g && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (mapped(p, 0x12a0) && rd(p + 4) != 0 && rd(p + 0x10) == 0) return p;
    }
    return 0;
}
void respawn_case(reflex_vm::Machine& m, uint32_t player) {
    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[respawn] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    rx_regs in; std::memset(&in, 0, sizeof in); in.r[6] = player;
    rx_regs out; std::memset(&out, 0, sizeof out);
    std::fprintf(stderr, "[respawn] player %08x reset delay bits %08x state %u\n", player, rd(player + 0x116c), rd(player + 0x1170));
    rx_call_original_ex(0x00550cc0, &in, &out, 0, nullptr);
    std::fprintf(stderr, "[respawn] done: state %u\n", rd(player + 0x1170));
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, TeardownProbeHook, "thunk", 0) {
    if (!std::getenv("RX_TD_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_TD_FRAME") ? uint32_t(std::atoi(std::getenv("RX_TD_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    const uint32_t act = find_activity();
    if (!act) { std::fprintf(stderr, "[teardown] no activity at frame %u\n", g_frame); return reflex_vm::RUN_ORIGINAL; }
    if (std::getenv("RX_TD_RESPAWN")) {
        g_m = &m;
        if (const uint32_t pl = find_player()) respawn_case(m, pl);
        return reflex_vm::RUN_ORIGINAL;
    }
    if (rd(act) != 0x00943554 && rd(act) != 0x00944a74 && rd(act) != 0x009446cc && rd(act) != 0x00944b74 && rd(act) != 0x00944c74 && rd(act) != 0x00944884) {   /* (w-partymodes: + MXActivityTag's 0056e1a0, MXActivityPokerRace's 00571260; w-snake: MXActivityInfection's 00573080, MXActivitySnake's 0056bc00) */   /* MXActivityRace's vtable (00564c10 is its vf3 only) or MXActivitySectionRace's (0056a520); the free ride's activity is another class */
        std::fprintf(stderr, "[teardown] activity %08x is not an MXActivityRace (vtable %08x): no case\n", act, rd(act));
        return reflex_vm::RUN_ORIGINAL;
    }
    if (std::getenv("RX_TD_LIST")) {   /* diagnostics: what Activity::vf6 00557a10 / the activity's destructor would release */
        const uint32_t w = rd(act + 0x234), sc = rd(w + 0x20138);
        std::fprintf(stderr, "[teardown] list: act +0xec8 %08x +0x106c %08x (vt %08x) world +0x21df0 %08x (vt %08x) scene %08x name '%s'\n",
                     rd(act + 0xec8), rd(act + 0x106c), rd(rd(act + 0x106c)), rd(w + 0x21df0), rd(rd(w + 0x21df0)), sc,
                     mapped(act + 0x118, 64) ? (const char*)g_m->mem.page(act + 0x118) + ((act + 0x118) & 0xfff) : "?");
        for (uint32_t k = 0; k < 0x18; k++) {
            const uint32_t r = sc + 0x38 + 0x110 * k, n = rd(r + 0x108), arr = rd(r + 0x104);
            if (!n) continue;
            char nm[64] = {0}; for (int i = 0; i < 63; i++) { const uint32_t c = rd((r + i) & ~3u) >> (8 * ((r + i) & 3)) & 0xff; if (!c) break; nm[i] = char(c); }
            std::fprintf(stderr, "[teardown] list: record %u '%s' %u entities:", k, nm, n);
            std::vector<std::pair<uint32_t, int>> h;
            for (uint32_t i = 0; i < n; i++) { const uint32_t vt = rd(rd(arr + 4 * i)); bool f = false; for (auto& e : h) if (e.first == vt) { e.second++; f = true; } if (!f) h.push_back({vt, 1}); }
            for (auto& e : h) std::fprintf(stderr, " %08x(vf9 %08x vf0 %08x) x%d", e.first, rd(e.first + 0x24), rd(e.first), e.second);
            std::fprintf(stderr, "\n");
        }
    }
    std::vector<int> kinds;
    if (const char* k = std::getenv("RX_TD_KINDS")) {
        std::string s(k);
        size_t pos = 0;
        while (pos <= s.size()) {
            size_t e = s.find(',', pos); if (e == std::string::npos) e = s.size();
            const std::string t = s.substr(pos, e - pos);
            if (!t.empty()) kinds.push_back(t == "-" ? -1 : std::atoi(t.c_str()));
            pos = e + 1;
        }
    }
    if (kinds.empty()) kinds.push_back(-1);
    const bool clip = std::getenv("RX_TD_CLIP") != nullptr;
    for (int k : kinds) teardown_case(m, act, k, clip);
    std::fprintf(stderr, "[teardown] frame %u: %zu cases done\n", g_frame, kinds.size());
    return reflex_vm::RUN_ORIGINAL;
}
