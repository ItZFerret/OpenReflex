// career_wave_probe.cpp -- the oracle's probe for the w-career ports no reference world calls (development only):
//   a  0073ec10 App.GetChallengeAwardLevel's native for the activity types 0x14 .. 0x1f (the level's MXChallengesSpecEntity, 0x1019);
//   t  the Outrun timed-section chain: MXActivityRace's "EnteredTimedSection" handler 00569550 -> 0054f4b0 (the player's section clock)
//      -> 00554df0 ("TimedSectionEntered"), on the world's TimedRaceSection entities (type 0x107b) or, without one, on section records
//      the probe writes into dead stack (the chain reads only +0x280 / +0x284 / +0x288 of the section);
//   s  TimedRaceSection's scalar deleting destructor 00413960 (flags 0) on each of the world's sections.
// (race_finish.c's changed 00569a40 is covered by raceend_probe.cpp: RX_RACEEND_PROBE=1 on National_NAT_Concord.)
// A frame hook (Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL addresses at one frame, each case on a snapshot
// of the whole guest memory and machine state that is restored afterwards, so with the ports installed Verify runs the core function
// and then the original on the same state and compares memory and the declared registers exactly as for a call from the game.
//   REFLEX_PORT=rx_race,rx_CareerWaveProbeHook REFLEX_PORT_VERIFY=1 RX_CW_PROBE=<letters> [RX_CW_FRAME=<n> (600)]
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <cstring>
#include <vector>
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
uint32_t bits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }

// the whole guest memory and the machine state, restored at the end of a case
struct Snapshot {
    reflex_vm::Machine& m;
    struct Page { uint32_t address; std::array<uint8_t, 4096> bytes; };
    std::vector<Page> saved;
    std::vector<uint8_t> was;
    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    decltype(m.flags) flags; decltype(m.fpu) fpu; decltype(m.fpu_cw) cw; decltype(m.fpu_sw) sw; decltype(m.mxcsr) mxcsr; bool mmx_active;
    std::function<void()> host;
    explicit Snapshot(reflex_vm::Machine& mm) : m(mm), was(1u << 20, 0), flags(mm.flags), fpu(mm.fpu), cw(mm.fpu_cw), sw(mm.fpu_sw),
                                               mxcsr(mm.mxcsr), mmx_active(mm.mmx_active) {
        std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
        host = snapshot_host_state(m);
        for (uint32_t i = 0; i < (1u << 20); i++) if (m.mem.mapped(i << 12)) {
            was[i] = 1; saved.push_back(Page{}); auto& p = saved.back(); p.address = i << 12;
            std::memcpy(p.bytes.data(), m.mem.page(p.address), 4096);
        }
    }
    bool ok() const { return bool(host); }
    ~Snapshot() {
        for (auto& p : saved) std::memcpy(m.mem.page(p.address), p.bytes.data(), 4096);
        for (uint32_t i = 0; i < (1u << 20); i++) if (!was[i] && m.mem.mapped(i << 12)) m.mem.unmap(i << 12, 4096);
        if (host) host();
        std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
        m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
    }
};

rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
uint32_t call(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out.r[0];
}

uint32_t local_player() {
    const uint32_t g = rd(0x00a946b8);
    for (uint32_t s = 0; g && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (mapped(p, 0x12a0) && rd(p + 4) != 0 && rd(p + 0x10) == 0 && rd(p + 0x94) && rd(p + 0x1ac)) return p;
    }
    return 0;
}

// the current world's scene (0073ec10's selection: the alternate world when its context is active, else the normal one)
uint32_t scene() {
    const uint32_t context = rd(0x00a95844), game = rd(0x00a946b8);
    uint32_t world = 0;
    if (rd(rd(context + 0x20) + 0x34) == 1) world = rd(game + 0x5a54);
    else if (rd(rd(context + 0x1c) + 0x34) == 1) world = rd(game + 0x20c8);
    return world ? rd(world + 0x20138) : 0;
}

// the scene's entities of a type (007d2920: EAX = scene; stdcall type, out, capacity, overflow; RET 0x10), out in dead stack
std::vector<uint32_t> entities(uint32_t type, uint32_t cap) {
    std::vector<uint32_t> list;
    const uint32_t s = scene();
    if (!s) return list;
    const uint32_t out = (g_m->r[4] - 0x4000) & ~15u;
    for (uint32_t i = 0; i < cap; i++) wr(out + 4 * i, 0);
    rx_regs in = regs(); in.r[0] = s;
    const uint32_t args[4] = { type, out, cap, 0 };
    const uint32_t n = call(0x007d2920, in, 4, args);
    for (uint32_t i = 0; i < n && i < cap; i++) list.push_back(rd(out + 4 * i));
    return list;
}

void probe_award_level(reflex_vm::Machine& m) {
    const auto specs = entities(0x1019, 8);
    std::fprintf(stderr, "[careerwave] award level: %zu spec entities\n", specs.size());
    for (uint32_t type = 0x14; type <= 0x1f; type++) {
        Snapshot snap(m);
        rx_regs in = regs(); in.r[0] = type;
        const uint32_t level = call(0x0073ec10, in);
        std::fprintf(stderr, "[careerwave] 0073ec10 type %02x -> %d\n", type, int(level));
    }
    // a spec entity with the field 0 / -1 / 7 for the HillClimb type (0x16): the default 10 and the authored level
    if (!specs.empty()) {
        const uint32_t field = specs[0] + 0x2a0 + 12 * (0x16 - 0x15);
        for (const uint32_t v : { 0u, 0xffffffffu, 7u }) {
            Snapshot snap(m);
            wr(field, v);
            rx_regs in = regs(); in.r[0] = 0x16;
            std::fprintf(stderr, "[careerwave] 0073ec10 HillClimb field %08x -> %d\n", v, int(call(0x0073ec10, in)));
        }
    }
}

// one Outrun handler call: 00569550 (thiscall activity; vehicle, section; RET 8)
void entered(uint32_t act, uint32_t vehicle, uint32_t section) {
    rx_regs in = regs(); in.r[1] = act;
    const uint32_t args[2] = { vehicle, section };
    call(0x00569550, in, 2, args);
}

void probe_timed_sections(reflex_vm::Machine& m) {
    const uint32_t p = local_player();
    if (!p) { std::fprintf(stderr, "[careerwave] no local player\n"); return; }
    const uint32_t act = rd(p + 0x94), vehicle = rd(p + 0x1ac);
    if (rd(act) != 0x00943554) { std::fprintf(stderr, "[careerwave] the activity is not an MXActivityRace (vtable %08x): no Outrun handler\n", rd(act)); return; }
    auto sections = entities(0x107b, 64);
    std::fprintf(stderr, "[careerwave] timed sections: %zu in the world; activity %08x mode %u drivers %d\n", sections.size(), act,
                 rd(act + 0x238), int(rd(act + 0x26c)));
    // section records in dead stack when the world has none (start flag, time, number)
    const uint32_t fake = (m.r[4] - 0x8000) & ~15u;
    const uint32_t records[4][3] = { { 1, bits(30.0f), 1 }, { 1, bits(20.5f), 2 }, { 0, bits(99.0f), 3 }, { 1, bits(10.0f), 2 } };
    std::vector<uint32_t> synthetic;
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t s = fake + 0x300 * i;
        wr(s + 0x280, records[i][0]); wr(s + 0x284, records[i][1]); wr(s + 0x288, records[i][2]);
        synthetic.push_back(s);
    }
    for (int pass = 0; pass < 2; pass++) {
        const auto& list = pass == 0 ? sections : synthetic;
        if (list.empty()) continue;
        Snapshot snap(m);
        if (pass == 1) {   // the records live in dead stack below ESP: rewrite them inside the snapshot (the snapshot restores them after)
            for (uint32_t i = 0; i < 4; i++) { wr(synthetic[i] + 0x280, records[i][0]); wr(synthetic[i] + 0x284, records[i][1]); wr(synthetic[i] + 0x288, records[i][2]); }
        }
        wr(act + 0x238, 8);                      // an Outrun race
        wr(p + 0x11ac, 0xffffffffu); wr(p + 0x11b0, 0xffffffffu); wr(p + 0x11b4, 0);
        for (const uint32_t s : list) {
            entered(act, vehicle, s);
            std::fprintf(stderr, "[careerwave] %s section %08x (start %u time %08x number %d): player section %d previous %d time %08x\n",
                         pass == 0 ? "world" : "record", s, rd(s + 0x280), rd(s + 0x284), int(rd(s + 0x288)), int(rd(p + 0x11ac)),
                         int(rd(p + 0x11b0)), rd(p + 0x11b4));
        }
        entered(act, 0x12345678u, list[0]);       // a vehicle no driver rides
        // the player's clock directly: 0054f4b0 (ECX = player, EAX = section) for a section with a lower number
        rx_regs in = regs(); in.r[1] = p; in.r[0] = list[0];
        call(0x0054f4b0, in);
        // the event alone: 00554df0 (ESI = channel, ECX = driver, EDX = number; stdcall activity)
        rx_regs ev = regs(); ev.r[6] = rd(act + 0x290); ev.r[1] = rd(rd(act + 0x268)); ev.r[2] = 3;
        const uint32_t a[1] = { act };
        call(0x00554df0, ev, 1, a);
    }
    {   // not an Outrun race: the handler returns at once
        Snapshot snap(m);
        if (!synthetic.empty()) { wr(synthetic[0] + 0x280, 1); wr(synthetic[0] + 0x288, 1); }
        entered(act, vehicle, synthetic[0]);
    }
}

void probe_section_dtor(reflex_vm::Machine& m) {
    const auto sections = entities(0x107b, 64);
    std::fprintf(stderr, "[careerwave] section destructors: %zu sections\n", sections.size());
    for (size_t i = 0; i < sections.size() && i < 8; i++) {
        Snapshot snap(m);
        rx_regs in = regs(); in.r[1] = sections[i];
        const uint32_t flags[1] = { 0 };
        const uint32_t r = call(0x00413960, in, 1, flags);
        std::fprintf(stderr, "[careerwave] 00413960 %08x -> %08x\n", sections[i], r);
    }
}
}  // namespace

RX_PORT(0x0084ed20, CareerWaveProbeHook, "thunk", 0) {
    static const char* groups = std::getenv("RX_CW_PROBE");
    if (!groups || !*groups) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_CW_FRAME") ? uint32_t(std::atoi(std::getenv("RX_CW_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    {
        Snapshot outer(m);
        if (!outer.ok()) { std::fprintf(stderr, "[careerwave] ERROR missing reversible host state\n"); return reflex_vm::RUN_ORIGINAL; }
        if (std::strchr(groups, 'a')) probe_award_level(m);
        if (std::strchr(groups, 't')) probe_timed_sections(m);
        if (std::strchr(groups, 's')) probe_section_dtor(m);
    }
    std::fprintf(stderr, "[careerwave] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
