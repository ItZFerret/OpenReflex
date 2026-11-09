// race_challenge_activity_probe.cpp -- the oracle's probe for the challenge activities' paths the reference challenge worlds do not
// reach in a plain ride (development only): the restart, the respawn, the medals at the Post enter, the WindDown, the hand-over, the
// TrailBlazer finish trigger, the TargetPractice hits, the teardown, BigAir (the HillClimb activity with BigAir's vtable), and
// Pathfinder / StepUp (activities built by the original constructors on the live world: no level stages them).
// A frame hook (Engine::Update 0084ed20, outside every verified call) runs the selected CASES on the live challenge world; each case
// builds its arguments and calls the ORIGINAL address, so with the ports installed Verify runs the core function, then the original
// on the same state, and compares memory and the declared registers as for a call from the game. Each case runs on a snapshot of the
// whole guest memory and machine state, restored afterwards.
//   REFLEX_PORT=<ports>,rx_ChallengeActivityProbeHook REFLEX_PORT_VERIFY=1 RX_CA_PROBE=<case>[,<case>...] [RX_CA_FRAME=<n>] <world run>
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
using rx_oracle::bits;

namespace {
reflex_vm::Machine* g_m = nullptr;
uint32_t g_frame = 0;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
void wrf(uint32_t a, float f) { wr(a, bits(f)); }
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
// thiscall
rx_regs tcall(uint32_t fn, uint32_t self, int n = 0, const uint32_t* args = nullptr) { rx_regs in = regs(); in.r[1] = self; return call(fn, in, n, args); }
uint32_t vfn(uint32_t obj, uint32_t slot) { return rd(rd(obj) + 4 * slot); }
uint32_t halloc(uint32_t n) {
    const uint32_t a[1] = {n};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
}
uint32_t hstr(const char* t) {
    const uint32_t n = uint32_t(std::strlen(t)) + 1;
    const uint32_t p = halloc((n + 3) & ~3u);
    for (uint32_t i = 0; i < n; i++) rx_wr8(p + i, uint8_t(t[i]));
    return p;
}
void copy(uint32_t dst, uint32_t src, uint32_t n) { for (uint32_t i = 0; i < n; i += 4) wr(dst + i, rd(src + i)); }

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

// the local human player and its activity
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
uint32_t activity() { return rd(player() + 0x94); }
uint32_t driver0(uint32_t A) { return rd(rd(A + 0x268)); }
const float kDt = 1.0f / 60.0f;

// ---- the shipped classes ----------------------------------------------------------------------------------------------------------
void case_info() {
    const uint32_t A = activity();
    std::fprintf(stderr, "[ca] player %08x activity %08x vt %08x kind %u state %u mode %u spec %08x drivers %d block %u\n", player(), A,
                 rd(A), rd(A + 0xff070), rd(A + 0x21c), rd(A + 0x238), rd(rd(A + 0x234) + 0x22140), int(rd(A + 0x26c)), rd(A + 0xff3a0));
}
// the restart (vf16) and the respawn at the start (Activity::vf20)
void case_restart() { const uint32_t A = activity(); tcall(vfn(A, 16), A); }
void case_post_medals();
// App.RestartActivity (MXGame_RestartActivity 00759d00: the activity manager's restart pass, the viewports, ResumeFromUI)
void case_game_restart() { rx_call_original(0x00759d00, RX_CDECL, 0, 0, nullptr); }
// the restart from the Post state with medals won (the end page's RESTART): the state set to Post, the Post enter with every medal, the Post
// update, then App.RestartActivity
void case_post_restart() { wr(activity() + 0x21c, 4); case_post_medals(); case_game_restart(); }
void case_respawn() { const uint32_t A = activity(); const uint32_t a[1] = {player()}; tcall(0x0055d120, A, 1, a); }
// the Post enter with every medal won (the class's value set beyond the gold criterion), then the Post update and exit
void case_post_medals() {
    const uint32_t A = activity();
    switch (rd(A + 0xff070)) {
    case 1: wrf(A + 0xff3b0, 100000.0f); break;          // HillClimb: the height climbed
    case 6: wrf(A + 0xff3b0, 0.001f); break;             // Trailblazer: the time
    case 8: wr(A + 0xff434, 1000); break;                // TargetPractice: the hits
    default: break;
    }
    const uint32_t st[1] = {2};
    tcall(vfn(A, 41), A, 1, st);
    wrf(A + 0xff064, 0.0f);
    const uint32_t dt[1] = {bits(kDt)};
    tcall(vfn(A, 29), A, 1, dt);
    tcall(vfn(A, 35), A, 1, dt);
    tcall(0x00559a80, A);   // Activity::vf58
}
// the Post enter without medals (the value far from every criterion)
void case_post_none() {
    const uint32_t A = activity();
    if (rd(A + 0xff070) == 6) wrf(A + 0xff3b0, 1.0e9f); else { wrf(A + 0xff3b0, -1.0e9f); wr(A + 0xff434, 0); }
    const uint32_t st[1] = {2};
    tcall(vfn(A, 41), A, 1, st);
}
// the WindDown enter and update, the Countdown enter, the InProgress exit, the class's small slots
void case_states() {
    const uint32_t A = activity();
    const uint32_t st[1] = {2}, dt[1] = {bits(kDt)};
    tcall(vfn(A, 40), A, 1, st);
    tcall(vfn(A, 27), A, 1, dt);
    tcall(vfn(A, 38), A, 1, st);
    tcall(vfn(A, 33), A, 1, st);
    tcall(vfn(A, 52), A);
    const uint32_t c[3] = {driver0(A), driver0(A), 1};
    tcall(vfn(A, 43), A, 3, c);
    const uint32_t q[4] = {player(), halloc(16), halloc(16), halloc(16)};
    tcall(vfn(A, 19), A, 4, q);
    tcall(vfn(A, 62), A);
    tcall(vfn(A, 18), A);
}
// the finish checks: the clock at the duration; HillClimb's best at the gold criterion; TargetPractice with every target hit
void case_finish() {
    const uint32_t A = activity();
    const uint32_t saved_mode = rd(A + 0x238);
    wr(A + 0x238, 1);
    wrf(A + 0x294, rd(A + 0x2bc) ? rx_oracle::f32(rd(A + 0x2bc)) + 1.0f : 1.0e6f);
    tcall(vfn(A, 42), A);
    wrf(A + 0x294, 0.0f);
    wrf(A + 0xff3b0, 100000.0f);
    if (rd(A + 0xff070) == 8) { wr(A + 0xff434, 64); wr(A + 0xff3b0, 3); }
    tcall(vfn(A, 42), A);
    wr(A + 0x238, saved_mode);
}
// the hand-over of the control (Activity::vf45: the first driver to itself, offline)
void case_handover() {
    const uint32_t A = activity();
    const uint32_t a[2] = {driver0(A), driver0(A)};
    tcall(0x00557570, A, 2, a);
}
// TrailBlazer's "VehicleEntered" handler: a trigger named "TrailblazerFinish" and one with another name
void case_tb_finish() {
    const uint32_t A = activity();
    for (const char* name : {"SomeOtherTrigger", "TrailblazerFinish"}) {
        const uint32_t trig = halloc(0x200);
        const uint32_t s = hstr(name);
        for (uint32_t i = 0; i < 0x40; i++) rx_wr8(trig + 0xc0 + i, rx_rd8(s + i));
        const uint32_t a[2] = {rd(player() + 0x1ac), trig};
        tcall(0x0057d3f0, A, 2, a);
    }
}
// TargetPractice's hits: every target within the collision radius, then within the head radius; the finish check after
void case_tp_hits() {
    const uint32_t A = activity();
    for (int k = 0; k < 2; k++) {
        if (k == 0) { wrf(0x009ca324, 1.0e12f); } else { wrf(0x009ca324, 0.0f); wrf(0x009ca328, 1.0e12f); }
        for (uint32_t i = 0; i < rd(A + 0xff3b0); i++) wr(rd(A + 0xff3b4 + 4 * i) + 0x140, 0);
        rx_regs in = regs(); in.r[7] = A;
        const uint32_t a[1] = {player()};
        call(0x0057d880, in, 1, a);
        tcall(vfn(A, 42), A);
    }
}
// the teardown: the players leave and the activity's destructor (vf3, then the scalar deleting destructor)
void case_teardown() {
    const uint32_t A = activity();
    wr(0x00a946c0, 0);   // the records object off (MXRecords_Leave's oracle seam is unclosed: as race_teardown_probe.cpp does)
    tcall(vfn(A, 3), A);
    const uint32_t f[1] = {1};
    tcall(vfn(A, 0), A, 1, f);
}
// the destructor alone (flags 0: the body, the block kept)
void case_dtor() { const uint32_t A = activity(); const uint32_t f[1] = {0}; tcall(vfn(A, 0), A, 1, f); }
// the driver: its reset (vf3), the end-of-run vf4 and its destructor
void case_driver() {
    const uint32_t A = activity(), d = driver0(A);
    tcall(vfn(d, 3), d);
    tcall(vfn(d, 4), d);
    const uint32_t f[1] = {0};
    tcall(vfn(d, 0), d, 1, f);
}
// the audio logic: the cue handlers and its slots, its unsubscription and destructor
void case_audio() {
    const uint32_t A = activity(), l = rd(A + 0xfd8);
    const uint32_t award[5] = {A, driver0(A), 1, 0, 0};
    tcall(0x00585150, l, 5, award);
    const uint32_t found[4] = {player(), 1, 3, halloc(16)};
    tcall(0x005851a0, l, 4, found);
    const uint32_t hit[3] = {player(), 1, 2};
    tcall(0x005851d0, l, 3, hit);
    const uint32_t wreck[2] = {A, rd(player() + 0x1ac)};
    tcall(0x00581ba0, l, 2, wreck);
    const uint32_t two[2] = {A, 2};
    tcall(0x00582050, l, 2, two);
    tcall(0x00585010, l);
    const uint32_t f[1] = {1};
    tcall(0x00584e90, l, 1, f);
}

// ---- BigAir: the live activity with BigAir's vtable (the same layout as HillClimb) ----------------------------------------------------
void case_big_air() {
    const uint32_t A = activity();
    wr(A, 0x009452ec);
    wr(A + 0xff070, 0);
    tcall(0x0057b5e0, A);                                   // vf18
    const uint32_t dt[1] = {bits(kDt)};
    const uint32_t c = player() + 0xb58;
    wr(c + 0x68, 1);
    tcall(0x0057b550, A, 1, dt);                            // vf28: state 0 -> 1 (airborne)
    wr(c + 0x68, 0); wr(c + 0x1c, 0);
    wr(player() + 0xbc0, 0); wr(player() + 0xb74, 0);
    tcall(0x0057b550, A, 1, dt);                            // vf28: state 1, landed
    wrf(player() + 0xc84, 20.0f); wrf(player() + 0xc88, 20.0f); wrf(player() + 0xc8c, 20.0f);
    const uint32_t st[1] = {2};
    tcall(0x0057b050, A, 1, st);                            // vf41: vf64 and the medals (value 0.0: vf62 005189f0)
    wrf(player() + 0xc84, 1.0f);
    tcall(0x0057b050, A, 1, st);
    const uint32_t f[1] = {0};
    tcall(0x0055eca0, A, 1, f);                             // the destructor
}

// ---- Pathfinder / StepUp: built by the original constructors on the live world ------------------------------------------------------
// a new activity of the type through its constructor (ESI = block, EAX = world, ECX = type) with the live activity's Activity part
uint32_t make_activity(uint32_t ctor, uint32_t type, uint32_t size) {
    const uint32_t A = activity();
    const uint32_t N = halloc(size);
    rx_regs in = regs(); in.r[6] = N; in.r[0] = rd(A + 0x234); in.r[1] = type;
    call(ctor, in);
    // the Activity part (everything below the challenge's +0xff070) taken from the live activity, the vtable kept: the state machine,
    // the drivers, the channel, the scenarios and the starts work as the live activity's
    const uint32_t vt = rd(N);
    copy(N + 4, A + 4, 0xff06c);
    wr(N, vt);
    return N;
}
void case_pathfinder() {
    const uint32_t A = activity();
    const uint32_t N = make_activity(0x0057b740, 0x18, 0xff3e0);
    std::fprintf(stderr, "[ca] pathfinder %08x\n", N);
    tcall(0x0057b9a0, N);                                   // vf4 (the event types)
    // (vf5 0057b830 is not probed: without a "pf" AI line asset the original opens the loose file, which the emulator does not model)
    tcall(0x0057ba10, N);                                   // vf67 (the starts from the AI line)
    tcall(0x0057bbe0, N);                                   // vf18
    const uint32_t dt[1] = {bits(kDt)}, st[1] = {2};
    tcall(0x0057be20, N, 1, dt);                            // vf25
    tcall(0x0057bda0, N, 1, st);                            // vf38
    wr(N + 0x238, 0); tcall(0x0057bda0, N, 1, st); wr(N + 0x238, rd(A + 0x238));
    const uint32_t m[3] = {N + 0xff07c, N + 0xff078, N + 0xff074};
    wrf(N + 0xff3cc, 0.5f);
    tcall(0x0057c320, N, 3, m);                             // vf64
    tcall(0x0057bd30, N, 1, st);                            // MXActivityCapture::vf37
    tcall(0x005775c0, N, 1, st);                            // MXActivityCapture::vf39
    {   // the follower: a second local player slot (+0x10 == 1) with the human's vehicle, for the gap (vf28) and the update (vf23)
        const uint32_t base = rd(0x00a946b8) + 0x7460;
        const uint32_t k = 15, other = base + 0x12a0 * k;
        rx_wr8(base + 0xdfbc, 2);
        wr(base + 0xdf8c + 4, k);
        wr(other + 0x10, 1);
        wr(other + 0x1ac, rd(player() + 0x1ac));
        wr(N + 0xb8c, 1);
        tcall(0x0057be50, N, 1, dt);                        // vf28
        wr(N + 0xb8c, 0);
        tcall(0x0057bc30, N, 1, dt);                        // vf23
    }
    tcall(0x0057c050, N, 1, st);                            // vf41 (the records)
    tcall(0x0057bc10, N);                                   // vf16 (the restart)
    const uint32_t p[1] = {player()};
    wr(driver0(A) + 0x10d4, 0xffffffffu);
    tcall(0x0057c230, N, 1, p);                             // vf20 without a gate
    const uint32_t q[1] = {player()};
    tcall(0x0057c1d0, N, 1, q);                             // vf44 (a PathfinderDriver)
    rx_regs in = regs(); in.r[0] = rd(N + 0x290);
    const uint32_t a[1] = {N};
    call(0x0058a640, in, 1, a);                             // the "Lap Completed" listener
    call(0x0058a700, in, 1, a);                             // and its removal
    wr(N + 0x26c, 0); wr(N + 0x280, 0);
    tcall(0x0057bb60, N);                                   // vf3 (no drivers left to leave)
    rx_regs s = regs(); s.r[0] = rd(A + 0x234) + 0x20030; s.r[7] = hstr("alpine");
    call(0x0078fcf0, s);
    s.r[7] = hstr("WORLD"); call(0x0078fcf0, s);
}
// an entity of the live world: the first entity of the type in the activity's group (007d2820), else the first of type 0x1014 (007d2920)
uint32_t find_entity(uint32_t A, uint32_t type) {
    const uint32_t scene = rd(rd(A + 0x234) + 0x20138), out = halloc(0x40), flag = halloc(4);
    const uint32_t a[6] = {scene, type, out, 1, A + 0x118, 0};
    if (call(0x007d2820, regs(), 6, a).r[0] != 0) return rd(out);
    rx_regs in = regs(); in.r[0] = scene;
    const uint32_t b[4] = {0x1014, out, 0x10, flag};
    if (call(0x007d2920, in, 4, b).r[0] != 0) return rd(out);
    return 0;
}
// Pathfinder's start (vf2: MXActivityChallenge::vf2, then the class's own part) on an activity with the live Activity part
void case_starts() {
    const uint32_t A = activity();
    const uint32_t a[1] = {0};
    const uint32_t P = make_activity(0x0057b740, 0x18, 0xff3e0);
    tcall(0x0057b8a0, P, 1, a);
    (void)A;
    // (StepUp's vf2 0057cd30 is not probed: it places the rider at the level's bar (0x106f) and no level has one; the original faults)
}
// the destructors on activities that own their Activity part (constructed only: the cases above share the live activity's)
void case_new_dtors() {
    const uint32_t A = activity();
    for (const auto& c : {std::array<uint32_t, 4>{0x0057b740u, 0x18u, 0xff3e0u, 0x0057b7f0u}, std::array<uint32_t, 4>{0x0057c3e0u, 0x19u, 0xff400u, 0x0055ecd0u}}) {
        const uint32_t N = halloc(c[2]);
        rx_regs in = regs(); in.r[6] = N; in.r[0] = rd(A + 0x234); in.r[1] = c[1];
        call(c[0], in);
        const uint32_t f[1] = {1};
        tcall(c[3], N, 1, f);
    }
}
void case_step_up() {
    const uint32_t A = activity();
    const uint32_t N = make_activity(0x0057c3e0, 0x19, 0xff400);
    // the bar and the uprights: the challenge's start entity stands in (only their vf17 positions are read)
    const uint32_t e = find_entity(A, rd(A + 0xff070) == 1 ? 0x101c : rd(A + 0xff070) == 6 ? 0x1021 : 0x1020);
    std::fprintf(stderr, "[ca] step up %08x (stand-in entity %08x)\n", N, e);
    if (e == 0) return;
    wr(N + 0xff3c4, e); wr(N + 0xff3c8, e); wr(N + 0xff3cc, e);
    tcall(0x0057cf50, N);                                   // vf61 (the starts; HillClimb's world has no StepUp starts)
    wr(N + 0xff3ec, 0);
    copy(N + 0x340, A + 0x340, 0x20); copy(N + 0x370, A + 0x340, 0x20); copy(N + 0x3a0, A + 0x340, 0x20);
    for (uint32_t level = 0; level < 3; level++) { rx_regs in = regs(); in.r[0] = N; in.r[1] = level; call(0x0057c640, in); }
    { rx_regs in = regs(); in.r[0] = N; in.r[1] = 0; call(0x0057c640, in); }
    rx_regs ed = regs(); ed.r[7] = N;
    const uint32_t zero[1] = {0}, one[1] = {1};
    call(0x0057cbb0, ed, 1, zero);                          // the rider placed at the level's start
    call(0x0057cbb0, ed, 1, one);
    { rx_regs in = regs(); in.r[6] = N; const uint32_t p[1] = {player()}; call(0x0057cdf0, in, 1, p); }   // past the bar
    { rx_regs in = regs(); in.r[7] = N; in.r[6] = rd(player() + 0x1ac) + 0xc0; call(0x0057ce90, in); }   // between the uprights
    for (int k = 0; k < 4; k++) { rx_regs in = regs(); in.r[0] = N; call(0x0057c6d0, in); }              // failed attempts
    for (int k = 0; k < 8; k++) { rx_regs in = regs(); in.r[0] = N; call(0x0057c760, in); }              // clears up the levels
    const uint32_t dt[1] = {bits(kDt)};
    wr(N + 0xff3e8, 0); wr(N + 0x21c, 2);
    for (uint32_t jump = 0; jump < 3; jump++) { wr(N + 0xff3e0, jump); wrf(N + 0xff3e4, 0.001f); tcall(0x0057c810, N, 1, dt); }   // vf28
    const uint32_t m[3] = {N + 0xff07c, N + 0xff078, N + 0xff074};
    tcall(0x0057cd00, N, 3, m);                             // vf64
    tcall(0x0057c9d0, N);                                   // vf65 (the debug text)
    tcall(0x0057cb20, N);                                   // vf18
}

using CaseFn = void (*)();
const std::map<std::string, CaseFn>& cases() {
    static const std::map<std::string, CaseFn> c = {
        {"info", case_info}, {"restart", case_restart}, {"game_restart", case_game_restart}, {"post_restart", case_post_restart}, {"respawn", case_respawn}, {"post_medals", case_post_medals},
        {"post_none", case_post_none}, {"states", case_states}, {"finish", case_finish}, {"handover", case_handover},
        {"tb_finish", case_tb_finish}, {"tp_hits", case_tp_hits}, {"teardown", case_teardown}, {"dtor", case_dtor},
        {"driver", case_driver}, {"audio", case_audio}, {"big_air", case_big_air}, {"pathfinder", case_pathfinder},
        {"step_up", case_step_up}, {"new_dtors", case_new_dtors}, {"starts", case_starts},
    };
    return c;
}

void run_case(reflex_vm::Machine& m, const std::string& name, CaseFn fn) {
    uint32_t regs_[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs_, m.r, sizeof regs_); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[ca] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    std::fprintf(stderr, "[ca] case %s\n", name.c_str());
    fn();
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs_, sizeof regs_); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, ChallengeActivityProbeHook, "thunk", 0) {
    const char* sel = std::getenv("RX_CA_PROBE");
    if (!sel || !*sel) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_CA_FRAME") ? uint32_t(std::atoi(std::getenv("RX_CA_FRAME"))) : 600;
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
        if (it == cases().end()) { std::fprintf(stderr, "[ca] unknown case %s\n", t.c_str()); continue; }
        run_case(m, t, it->second);
    }
    std::fprintf(stderr, "[ca] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
