// race_activation_probe.cpp -- the oracle's probe for core/game/race_activation.c (development only). The reference Free Ride world
// (FreeRide_FR_Alpine) is an arcade Free Ride: the UI property InCareerMode is not set, so MXActivityFreeRide::vf54 is 0 and the
// UI manager's "StartActivity" handlers stop at their first test; and the random rider seldom reaches an activation area. A frame hook
// (Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL functions on the live world: with the ports installed
// Verify runs the core function, then the original on the same state, and compares memory and the declared registers as for a call
// from the game. Each case runs on a snapshot of the whole guest memory and machine state, restored afterwards.
//   REFLEX_PORT=rx_race,rx_build11,rx_ActivationProbeHook REFLEX_PORT_VERIFY=1 RX_ACT_PROBE=<case>[,<case>...] [RX_ACT_FRAME=<n>]
// Cases:
//   info          the player, its vehicle, the activity, the level's StartActivity triggers (type name, activity, track, enabled)
//   enter_arcade  OnTriggerEnter / OnTriggerExit for every trigger without InCareerMode (vf54 = 0)
//   enter_career  the same with InCareerMode = 1 (the UI manager's own setter): the area marked, EventTypeActivation / TrackId set
//   classes       InCareerMode; the first trigger's spec class index set to each of 0 .. 10 (allowed / the required class / none)
//   names         InCareerMode; copies of a trigger with another type name, an empty activity, an unknown activity, and the exits
//   discovery     InCareerMode; 00585520 on the activity's tracker, 0054d640 for the first collectables (the profile row created),
//                 then two found (0054d590 sets their bits) and 00585520 again (the found ones and the DISC_ markers near them)
//   vf37          InCareerMode; MXActivityFreeRide::vf37 (the Pre enter in discovery mode)
//   callees       00596540 for every challenge name, 004109d0 / 00410830 for each type and track, 004e78b0, 00542ff0, 004199d0
// No game logic here: only argument construction and the snapshot.
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using namespace reflex_port;

namespace {
reflex_vm::Machine* g_m = nullptr;
uint32_t g_frame = 0;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
rx_regs tcall(uint32_t fn, uint32_t self, int n = 0, const uint32_t* args = nullptr) { rx_regs in = regs(); in.r[1] = self; return call(fn, in, n, args); }
std::string str(uint32_t a, uint32_t n = 64) {
    std::string s;
    for (uint32_t i = 0; i < n && mapped(a + i, 1); i++) { const char c = char(rx_rd8(a + i)); if (!c) break; s += c; }
    return s;
}
void put(uint32_t a, const char* s, uint32_t n) { for (uint32_t i = 0; i < n; i++) rx_wr8(a + i, 0); for (uint32_t i = 0; s[i] && i + 1 < n; i++) rx_wr8(a + i, uint8_t(s[i])); }
uint32_t halloc(uint32_t n) {
    const uint32_t a[1] = {n};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
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

uint32_t player() {
    const uint32_t g = rd(0x00a946b8);
    if (!g) return 0;
    for (uint32_t s = 0; s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) == 0 || rd(p + 0x10) != 0) continue;
        return p;
    }
    return 0;
}
uint32_t vehicle() { return rd(player() + 0x1ac); }
uint32_t activity() { return rd(player() + 0x94); }
uint32_t ui() { return rd(0x00a93fcc); }
std::vector<uint32_t> triggers() {
    std::vector<uint32_t> t;
    const uint32_t A = activity();
    for (uint32_t k = 0; k < 0x18; k++) { const uint32_t e = rd(A + 0xff190 + 4 * k); if (e) t.push_back(e); }
    return t;
}
// UIProperty_SetInt 005dbfb0 (ECX = ui; key, value) / GetInt 005dc0c0 (EAX = key; ui, &out) / GetString 005dc060 (EAX key, EDI buf, ESI size; ui)
void ui_set_int(uint32_t key, uint32_t v) { const uint32_t a[2] = {key, v}; tcall(0x005dbfb0, ui(), 2, a); }
int32_t ui_get_int(uint32_t key) {
    const uint32_t out = halloc(4);
    wr(out, 0xdeadbeefu);
    rx_regs in = regs(); in.r[0] = key;
    const uint32_t a[2] = {ui(), out};
    call(0x005dc0c0, in, 2, a);
    return int32_t(rd(out));
}
std::string ui_get_string(uint32_t key) {
    const uint32_t out = halloc(0x40);
    rx_regs in = regs(); in.r[0] = key; in.r[7] = out; in.r[6] = 0x40;
    const uint32_t a[1] = {ui()};
    call(0x005dc060, in, 1, a);
    return str(out);
}
void career_mode() { ui_set_int(0x00941f6c /* "InCareerMode" */, 1); }
void enter(uint32_t veh, uint32_t trig) { const uint32_t a[2] = {veh, trig}; tcall(0x005a3b60, ui(), 2, a); }
void leave(uint32_t veh, uint32_t trig) { const uint32_t a[2] = {veh, trig}; tcall(0x005a3d00, ui(), 2, a); }
void report(const char* what) {
    const uint32_t p = player();
    std::fprintf(stderr, "[act] %-28s area %u \"%s\" track %d | ui EventTypeActivation \"%s\" TrackId %d RequiredClass %d\n", what,
                 rd(p + 0xa0), str(p + 0xa4, 32).c_str(), int32_t(rd(p + 0x1a8)), ui_get_string(0x0094c320).c_str(),
                 ui_get_int(0x00936e40), ui_get_int(0x0094d0e4));
}

void case_info() {
    const uint32_t p = player(), v = vehicle(), A = activity();
    const uint32_t spec = rd(rd(A + 0x234) + 0x22140);
    std::fprintf(stderr, "[act] player %08x (veh->player %08x) vehicle %08x activity %08x vt %08x state %u controller %d spec %08x ui %08x\n", p,
                 rd(v + 0x190), v, A, rd(A), rd(A + 0x21c), int32_t(rd(p + 0x1f0)), spec, ui());
    std::fprintf(stderr, "[act] InCareerMode %d EventLocationId %d EventType \"%s\" vf54 %u\n", ui_get_int(0x00941f6c), ui_get_int(0x00936e20),
                 ui_get_string(0x00935e08).c_str(), tcall(rd(rd(A) + 0xd8), A).r[0]);
    for (const uint32_t t : triggers())
        std::fprintf(stderr, "[act] trigger %08x \"%s\" activity \"%s\" track %d enabled %u pos %.1f %.1f %.1f\n", t, str(t + 0xc0).c_str(),
                     str(t + 0x108).c_str(), int32_t(rd(t + 0x104)), uint32_t(rx_rd8(t + 0x210)), rx_oracle::f32(rd(t + 0x60)),
                     rx_oracle::f32(rd(t + 0x64)), rx_oracle::f32(rd(t + 0x68)));
}
void case_enter_arcade() {
    for (const uint32_t t : triggers()) { enter(vehicle(), t); report("arcade enter"); leave(vehicle(), t); report("arcade exit"); }
}
void case_enter_career() {
    career_mode();
    for (const uint32_t t : triggers()) { enter(vehicle(), t); report(("career enter " + str(t + 0x108)).c_str()); leave(vehicle(), t); report("career exit"); }
}
void case_classes() {
    career_mode();
    const auto t = triggers();
    if (t.empty()) return;
    const uint32_t A = activity(), spec = rd(rd(A + 0x234) + 0x22140);
    for (const uint32_t trig : t) {
        // the activity's type (00596540) and the spec slot of {type, track}
        const uint32_t a[1] = {trig + 0x108};
        const uint32_t type = rx_call_original(0x00596540, RX_CDECL, 0, 1, a);
        const uint32_t slot = spec + 4 * (rd(trig + 0x104) + 3 * (type + 0x4e));
        const uint32_t was = rd(slot);
        for (uint32_t index = 0; index <= 10; index++) {
            wr(slot, index);
            enter(vehicle(), trig);
            char what[64]; std::snprintf(what, sizeof what, "class index %u (type %u)", index, type);
            report(what);
            leave(vehicle(), trig);
        }
        wr(slot, 0xffffffffu);
        enter(vehicle(), trig);
        report("class index -1");
        wr(slot, was);
    }
}
void case_names() {
    career_mode();
    const auto t = triggers();
    if (t.empty()) return;
    const uint32_t copy = halloc(0x280);
    for (uint32_t i = 0; i < 0x280; i += 4) wr(copy + i, rd(t[0] + i));
    put(copy + 0xc0, "ResetPlayer", 0x40);
    enter(vehicle(), copy); report("another type");
    leave(vehicle(), copy); report("another type exit");
    put(copy + 0xc0, "StartActivity", 0x40);
    put(copy + 0x108, "", 0x40);
    enter(vehicle(), copy); report("empty activity");
    leave(vehicle(), copy); report("empty activity exit");
    put(copy + 0x108, "NoSuchChallenge", 0x40);
    enter(vehicle(), copy); report("unknown activity");
    put(copy + 0x108, "BigAir", 0x40);
    enter(vehicle(), copy); report("BigAir");
    leave(vehicle(), copy); report("exit");
}
void case_discovery() {
    career_mode();
    const uint32_t A = activity(), p = player();
    const uint32_t a2[2] = {A + 0xff080, A};
    rx_call_original(0x00585520, RX_STDCALL, 0, 2, a2);
    std::fprintf(stderr, "[act] discovery setup: %u collectables, %u found\n", rd(A + 0xff080), rd(A + 0xff080 + 0x84));
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t a[2] = {p, i};
        std::fprintf(stderr, "[act] found(%u) = %u\n", i, rx_call_original(0x0054d640, RX_STDCALL, 0, 2, a));
    }
    for (uint32_t i = 0; i < 3; i += 2) { const uint32_t a[2] = {p, i}; rx_call_original(0x0054d590, RX_STDCALL, 0, 2, a); }
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t a[2] = {p, i};
        std::fprintf(stderr, "[act] found(%u) = %u (after 0054d590 0 / 2)\n", i, rx_call_original(0x0054d640, RX_STDCALL, 0, 2, a));
    }
    rx_call_original(0x00585520, RX_STDCALL, 0, 2, a2);
    std::fprintf(stderr, "[act] discovery setup again: %u collectables, %u found\n", rd(A + 0xff080), rd(A + 0xff080 + 0x84));
}
void case_vf37() {
    career_mode();
    const uint32_t A = activity();
    const uint32_t st[1] = {1};
    tcall(0x005603c0, A, 1, st);
    std::fprintf(stderr, "[act] vf37: discovery flag %u collectables %u found %u\n", rd(A + 0xff180), rd(A + 0xff080), rd(A + 0xff080 + 0x84));
}

// the callees of the handlers, each called as the original calls it (so each one is a per-call Verify of its own port)
void case_callees() {
    const uint32_t A = activity(), spec = rd(rd(A + 0x234) + 0x22140);
    static const char* names[] = {"BigAir", "HillClimb", "Pathfinder", "StepUp", "Trailblazer", "TargetPractice", "hillclimb", "FreeRide", "", "NoSuch"};
    const uint32_t name = halloc(0x40);
    for (const char* n : names) {
        put(name, n, 0x40);
        const uint32_t a[1] = {name};
        const uint32_t type = rx_call_original(0x00596540, RX_CDECL, 0, 1, a);
        for (uint32_t track = 0; track < 3; track++) {
            rx_regs in = regs(); in.r[0] = type; in.r[1] = track;
            const uint32_t s1[1] = {spec};
            const int32_t required = int32_t(call(0x004109d0, in, 1, s1).r[0]);
            const uint32_t s2[2] = {spec, rd(player() + 0x1f0)};
            const uint32_t allowed = call(0x00410830, in, 2, s2).r[0];
            if (track == 0) std::fprintf(stderr, "[act] \"%s\" type %u: required class %d allowed %u\n", n, type, required, allowed);
        }
    }
    for (int32_t c = -1; c < 4; c++) {
        const uint32_t a[1] = {uint32_t(c)};
        const uint32_t ref = rx_call_original(0x004e78b0, RX_STDCALL, 0, 1, a);
        std::fprintf(stderr, "[act] player row of controller %d: %08x {row %d store %08x}\n", c, ref, int32_t(rd(ref)), rd(ref + 8));
    }
    const uint32_t list = rd(0x00a946b8) + 0x154b0;
    for (uint32_t i = 0; i < rd(list + 0x2c) && i < 4; i++) {
        const uint32_t r = rd(rd(list + 0x28) + 4 * i);
        rx_regs in = regs(); in.r[0] = list; in.r[7] = r ? rd(r + 4) : 0;
        std::fprintf(stderr, "[act] vehicle record of id %d: %08x\n", int32_t(in.r[7]), call(0x00542ff0, in).r[0]);
    }
    { rx_regs in = regs(); in.r[0] = list; in.r[7] = 0x7fffffff; call(0x00542ff0, in); }
    const uint32_t key = halloc(4);
    const uint32_t a1[1] = {0x00936cc0 /* "VehicleClassType" */};
    rx_regs fin = regs(); fin.r[0] = 0x00ae0910;
    const uint32_t table = call(0x00402220, fin, 1, a1).r[0];
    for (uint32_t cls : {1u, 2u, 3u, 15u, 27u, 999u}) {
        wr(key, cls);
        rx_regs in = regs(); in.r[6] = table; in.r[0] = 0x00936cd4 /* "VehicleClassId" */;
        const uint32_t a[1] = {key};
        const uint32_t set = call(0x004199d0, in, 1, a).r[0];
        std::fprintf(stderr, "[act] VehicleClassType rows of class %u: %u\n", cls, rd(set + 0x1c));
    }
}

using CaseFn = void (*)();
const std::map<std::string, CaseFn>& cases() {
    static const std::map<std::string, CaseFn> c = {
        {"info", case_info}, {"enter_arcade", case_enter_arcade}, {"enter_career", case_enter_career}, {"classes", case_classes},
        {"names", case_names}, {"discovery", case_discovery}, {"vf37", case_vf37}, {"callees", case_callees},
    };
    return c;
}

void run_case(reflex_vm::Machine& m, const std::string& name, CaseFn fn) {
    uint32_t regs_[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs_, m.r, sizeof regs_); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[act] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    std::fprintf(stderr, "[act] case %s\n", name.c_str());
    fn();
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs_, sizeof regs_); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, ActivationProbeHook, "thunk", 0) {
    const char* sel = std::getenv("RX_ACT_PROBE");
    if (!sel || !*sel) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_ACT_FRAME") ? uint32_t(std::atoi(std::getenv("RX_ACT_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    std::string s(sel);
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t e = s.find(',', pos); if (e == std::string::npos) e = s.size();
        const std::string t = s.substr(pos, e - pos);
        pos = e + 1;
        if (t.empty()) continue;
        const auto it = cases().find(t);
        if (it == cases().end()) { std::fprintf(stderr, "[act] unknown case %s\n", t.c_str()); continue; }
        run_case(m, t, it->second);
    }
    std::fprintf(stderr, "[act] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
