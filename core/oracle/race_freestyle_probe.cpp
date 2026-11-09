// race_freestyle_probe.cpp -- the oracle's frame hook for the Freestyle event (core/game/race_activity_freestyle.c,
// race_freestyle_driver.c, race_freestyle_rules.c, race_activity_champ_shared.c, anim_scenario_fmx.c, build_activity_freestyle.c).
// Installed with REFLEX_PORT=...,rx_FreestyleProbeHook (Engine::Update 0084ed20, outside every verified call).
//   RX_FS_INFO=<n>       every n frames: the local activity's class, state and the Freestyle fields (stderr)
//   RX_FS_RULES=<frame>  at that frame the rules popup is accepted (+0xff068 = 0: all that App_PreActivityRulesPageDone 00742a00 does);
//                        the world runs use REFLEX_POKE for the same word instead (fv.sh / wrun.sh)
//   RX_FS_DUMP=f:a:n     diagnostics: n guest words at a, at frame f
//   RX_FS_PROBE=<case,..> RX_FS_FRAME=<n>: the CASES below at frame n. Each case builds its arguments on the live Freestyle activity
//                        (or small game-heap blocks) and calls the ORIGINAL address, so with the ports installed Verify runs the core
//                        function and then the original on the same state and compares them, as for a call from the game. Every case
//                        runs on a snapshot of the whole guest memory and machine state, restored afterwards.
// No game logic here: only argument construction and the snapshot.
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>
using namespace reflex_port;
using rx_oracle::bits;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
void wrf(uint32_t a, float v) { wr(a, bits(v)); }
float rf(uint32_t a) { return rx_oracle::f32(rd(a)); }

uint32_t find_activity() {
    const uint32_t g = rd(0x00a946b8);
    if (!g) return 0;
    for (uint32_t s = 0; s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) == 0 || rd(p + 0x10) != 0) continue;   /* a local human */
        const uint32_t a = rd(p + 0x94);
        if (a && mapped(a, 0x13e610)) return a;
    }
    return 0;
}

rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
// the original at fn with these registers and stack arguments; EAX of the result
uint32_t call(uint32_t fn, rx_regs in, std::initializer_list<uint32_t> args = {}) {
    std::vector<uint32_t> a(args);
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, int(a.size()), a.empty() ? nullptr : a.data());
    return out.r[0];
}
uint32_t thiscall(uint32_t fn, uint32_t self, std::initializer_list<uint32_t> args = {}) { rx_regs in = regs(); in.r[1] = self; return call(fn, in, args); }
// a zeroed game-heap block (ReflexAlignedAlloc 0078fbe0)
uint32_t halloc(uint32_t n) {
    const uint32_t a[1] = {n};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
}
uint32_t hstring(const char* s) {
    const uint32_t n = uint32_t(std::strlen(s)) + 1, p = halloc((n + 3) & ~3u);
    for (uint32_t i = 0; i < n; i++) rx_wr8(p + i, uint8_t(s[i]));
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

void run_case(reflex_vm::Machine& m, const std::string& name, const std::function<void()>& fn) {
    uint32_t regs_[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs_, m.r, sizeof regs_); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[freestyle] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    std::fprintf(stderr, "[freestyle] case %s\n", name.c_str());
    fn();
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs_, sizeof regs_); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}

// the live objects of a case
struct Live { uint32_t A, d, p, w, sc_intro; };
Live live() {
    Live L{};
    L.A = find_activity();
    L.d = rd(rd(L.A + 0x268));
    L.p = rd(L.d + 0x74);
    L.w = rd(L.A + 0x234);
    L.sc_intro = L.A + 0xff080;
    return L;
}
// a fake driver with its own player block (the score fields the comparators read)
uint32_t fake_driver(float score, float c74, uint32_t k1088, uint32_t k1dc) {
    const uint32_t d = halloc(0x200), p = halloc(0x1300);
    wr(d + 0x74, p);
    wrf(p + 0xcec, score);
    for (uint32_t o = 0xc74; o <= 0xc80; o += 4) wrf(p + o, c74);
    wr(p + 0x1088, k1088);
    wr(p + 0x1dc, k1dc);
    return d;
}

using Case = std::function<void()>;
std::map<std::string, Case>& cases() {
    static std::map<std::string, Case> c = {
        {"stunts", [] {
            Live L = live();
            for (uint32_t kind : {0u, 1u}) {
                wr(L.p + 0x10, kind);
                thiscall(0x00562220, L.A, {L.p, 0});
                thiscall(0x00562250, L.A, {L.p, 0});
                thiscall(0x005621f0, L.A, {L.p, 0, 3});
            }
            wr(L.p + 0x10, 0);
        }},
        {"compare", [] {
            Live L = live();
            const uint32_t a = fake_driver(10.0f, 1.0f, 3, 5), b = fake_driver(20.0f, 2.0f, 3, 4), e = fake_driver(10.0f, 1.0f, 7, 5);
            for (uint32_t state : {2u, 4u})
                for (uint32_t tie : {0u, 1u}) {
                    wr(L.A + 0x21c, state);
                    for (auto pr : {std::pair<uint32_t, uint32_t>{a, b}, {b, a}, {a, a}, {a, e}, {e, a}, {L.d, a}, {L.d, L.d}})
                        thiscall(0x00562af0, L.A, {pr.first, pr.second, tie});
                }
            const uint32_t n = fake_driver(0.0f, 0.0f, 0, 0);
            wr(rd(n + 0x74) + 0xcec, 0x7fc00000u);   /* a NaN score: unordered */
            wr(L.A + 0x21c, 4);
            thiscall(0x00562af0, L.A, {n, a, 1});
        }},
        {"place", [] {   /* vf14: InProgress only */
            Live L = live();
            for (uint32_t state : {1u, 2u}) {
                wr(L.A + 0x21c, state);
                thiscall(0x005626e0, L.A, {L.d, 0});
            }
            wr(L.A + 0x21c, 2);
            thiscall(0x005626e0, L.A, {L.d, 5});
        }},
        {"handover", [] {   /* vf45 and Activity::vf45 */
            Live L = live();
            for (uint32_t area : {0u, 1u}) {
                wr(L.A + 0xfe8, area);
                thiscall(0x00563640, L.A, {L.d, L.d});
            }
            thiscall(0x00557570, L.A, {L.d, L.d});
        }},
        {"small", [] {
            Live L = live();
            thiscall(0x005620d0, L.A, {0});                       /* vf49 */
            thiscall(0x00563380, L.A);                            /* vf61 */
            wr(0x00a942b4, 1); wr(0x00a959a8, 9);
            thiscall(0x00563380, L.A);
            thiscall(0x0055ec00, L.A);                            /* vf48 */
            thiscall(0x0055ec10, L.A);                            /* vf9 */
            thiscall(0x00563ce0, L.A, {1});                       /* vf53 (1): nothing */
            for (uint32_t mode : {0u, 1u, 2u, 3u, 4u})
                for (uint32_t cur : {0u, 1u, 2u, 0xffffffffu}) {
                    wr(L.A + 0x238, mode); wr(0x009c47bc, cur);
                    thiscall(0x005620e0, L.A);                    /* vf52 */
                    thiscall(0x0057e760, L.A);                    /* the championship main event's vf52 */
                    thiscall(0x0057ea30, L.A);                    /* the championships' shared vf52 */
                }
            for (uint32_t mode : {0u, 1u, 2u, 3u}) { wr(L.A + 0x238, mode); thiscall(0x00561dc0, L.A, {1}); }   /* vf38 */
        }},
        {"states", [] {   /* vf8 / the Post sub-states */
            Live L = live();
            for (uint32_t state : {0u, 1u, 4u})
                for (uint32_t sub : {0u, 1u, 2u, 3u}) {
                    wr(L.A + 0x21c, state); wr(L.A + 0x13e5f8, sub);
                    thiscall(0x00563d00, L.A);
                }
        }},
        {"post", [] {
            Live L = live();
            wr(L.A + 0x21c, 4);
            if (!rd(L.w + 0x20190)) wr(L.w + 0x20190, L.A);       /* the level cast's activity (the scenarios read it) */
            for (uint32_t sub : {2u, 0u, 1u, 3u}) {
                wr(L.A + 0x13e5f8, sub);
                wr(L.w + 0x20190, L.A);
                rx_regs in = regs(); in.r[0] = L.A;
                call(0x00563b10, in, {bits(0.016f)});
            }
            { rx_regs in = regs(); in.r[1] = L.A; in.r[0] = 2; call(0x00563aa0, in); }
            { rx_regs in = regs(); in.r[1] = L.A; in.r[0] = 1; call(0x00563aa0, in); }
            wrf(L.A + 0xff064, 0.0f);
            thiscall(0x00561f10, L.A, {bits(0.016f)});              /* vf29 */
            thiscall(0x00563330, L.A, {bits(0.016f)});
            thiscall(0x0057c010, L.A, {bits(0.016f)});
            wr(L.w + 0x20190, L.A);
            thiscall(0x00563c50, L.A);                            /* vf58 */
        }},
        {"enter", [] {   /* the state enters on the live activity */
            Live L = live();
            thiscall(0x00562f70, L.A, {1});                       /* vf39 */
            for (uint32_t mode : {1u, 2u}) { wr(L.A + 0x238, mode); thiscall(0x005633a0, L.A, {2}); }   /* vf40 */
            wr(L.A + 0x238, 1);
            thiscall(0x00561ef0, L.A, {3});                       /* vf41 */
            wr(L.A + 0xff070, 1);
            thiscall(0x00563090, L.A, {3});
            thiscall(0x00562d40, L.A, {0});                       /* vf37 */
            thiscall(0x00562bd0, L.A);                            /* vf42 */
            wrf(L.A + 0x294, rf(L.A + 0x2bc));
            thiscall(0x00562bd0, L.A);
        }},
        {"restart", [] {   /* Activity::vf16 (restart) and vf20 (back to the start pose) */
            Live L = live();
            thiscall(0x0055d120, L.A, {L.p});
            thiscall(0x00557b40, L.A);
        }},
        {"ghost", [] {   /* mode 3: the ghost run's events */
            Live L = live();
            wr(L.A + 0x238, 3);
            wr(L.p + 0x10, 2);
            thiscall(0x00562170, L.A, {0, 1, bits(12.7f)});        /* vf46 */
            thiscall(0x00562170, L.A, {0, 0, bits(3.0f)});
            wr(L.p + 0x10, 0);
            thiscall(0x00561f80, L.A, {L.A, L.d});                 /* "Ghost Session Completed" */
            thiscall(0x0050d1c0, L.d);                            /* driver vf6 */
            wr(L.A + 0x238, 1); wr(L.p + 0x10, 1);
            thiscall(0x0050d1c0, L.d);
        }},
        {"start", [] {   /* the start paths of the other modes */
            Live L = live();
            thiscall(0x00561a70, L.A);                            /* mode 0's line */
            for (uint32_t gate : {0u, 1u}) { rx_wr8(L.A + 0x8c4, uint8_t(gate)); rx_regs in = regs(); in.r[7] = L.A; call(0x00562800, in); }
            thiscall(0x00562440, L.A);                            /* vf5 */
            thiscall(0x005626c0, L.A);                            /* vf18 */
            wr(L.A + 0x238, 0); thiscall(0x00563740, L.A, {L.d});   /* vf47 (once: the driver block has room for one more set) */
        }},
        {"restartvf2", [] {   /* vf2 again on the live activity, modes 0 / 2 / 3 */
            Live L = live();
            for (uint32_t mode : {0u, 2u, 3u}) { wr(L.A + 0x238, mode); thiscall(0x00561990, L.A, {0x009426b4}); }
        }},
        {"strfind", [] {
            const uint32_t hay[4] = {hstring("FMX_BuenaVista"), hstring("TheWorld_Level"), hstring(""), hstring("worldWORLDw")};
            const uint32_t nee[3] = {0x0096dd28, hstring("vista"), hstring("")};
            for (uint32_t h : hay)
                for (uint32_t n : nee) { rx_regs in = regs(); in.r[0] = h; in.r[7] = n; call(0x0078fcf0, in); }
        }},
        {"driver", [] {
            Live L = live();
            thiscall(0x0050c9c0, L.d);                            /* vf1 */
            thiscall(0x0050cbd0, L.d);                            /* vf3 */
            thiscall(0x0050bee0, L.d);                            /* vf4 */
            thiscall(0x005189f0, L.d);
            wr(L.d + 0x24, L.d); wr(L.d + 0xf30, 7); wr(L.d + 0xf44, 3);
            thiscall(0x0050dba0, L.d);                            /* vf12 */
            thiscall(0x0050dba0, L.d);
            for (uint32_t kind : {0u, 2u}) { wr(L.p + 0x10, kind); thiscall(0x0050dbc0, L.d); }   /* vf13 */
            wr(L.p + 0x10, 0);
            thiscall(0x0050dc70, L.d);                            /* vf14 */
            thiscall(0x0050dbf0, L.d, {0, 0});                    /* vf16 */
            thiscall(0x0050dda0, L.d);                            /* vf17 */
            thiscall(0x0050dd20, L.d);                            /* vf18 */
            thiscall(0x0050d1c0, L.d);                            /* vf6 */
            const uint32_t rule = rd(L.d + 0xa0);
            thiscall(0x0050db40, L.d, {rule});                    /* vf9 with the crowd rule */
            wrf(L.p + 0x116c, 0.0f);
            for (uint32_t i = 0; i < rd(L.d + 0xee4); i++) thiscall(0x0050db40, L.d, {rd(L.d + 0xa0 + 4 * i)});
            thiscall(0x0050cae0, L.d);                            /* vf2 */
        }},
        {"driverframe", [] {   /* the driver's frame in the states it acts in, mode 3 */
            Live L = live();
            for (uint32_t state : {2u, 3u}) {
                wr(L.A + 0x21c, state);
                thiscall(0x0050cf00, L.d, {bits(0.016f)});
                rx_regs in = regs(); in.r[7] = L.d;
                call(0x0050d610, in, {bits(0.016f)});
                wr(L.A + 0x238, 3); wrf(L.d + 0xf48, 1.0f); wrf(L.d + 0xf4c, 2.0f);
                call(0x0050d610, in, {bits(0.016f)});
                wr(L.A + 0x238, 1);
            }
        }},
        {"rules", [] {
            Live L = live();
            /* a crowd region of two triangles around the vehicle and one away from it */
            const uint32_t region = halloc(0x200), tris = halloc(0x30 * 3);
            const float vx = rf(rd(L.p + 0x1ac) + 0xc0), vz = rf(rd(L.p + 0x1ac) + 0xc8);
            const float t[3][6] = {{vx + 100, vz + 100, vx + 101, vz + 100, vx + 100, vz + 101},
                                   {vx - 5, vz - 5, vx + 5, vz - 5, vx, vz + 5},
                                   {vx - 5, vz - 5, vx, vz + 5, vx + 5, vz - 5}};
            for (int i = 0; i < 3; i++) for (int k = 0; k < 6; k++) wrf(tris + 0x30 * uint32_t(i) + 4 * uint32_t(k), t[i][k]);
            wr(region + 0x1cc, tris);
            for (uint32_t n : {0u, 1u, 2u, 3u}) {
                wr(region + 0x1c8, n);
                rx_regs in = regs(); in.r[0] = region; in.r[1] = rd(L.p + 0x1ac) + 0xc0;
                call(0x006a6e90, in);
            }
            const uint32_t rule = rd(L.d + 0xa0);
            thiscall(0x00519970, rule, {0});
            thiscall(0x00519970, rule, {1});
            thiscall(0x0051bc50, rule, {L.d});
            const uint32_t arena = rd(L.w + 0x21df0);
            std::fprintf(stderr, "[freestyle] rules: arena %08x region %08x\n", arena, arena ? rd(arena + 0x9c) : 0);
            if (arena) {
                const uint32_t old = rd(arena + 0x9c);
                wr(region + 0x1c8, 2);
                wr(arena + 0x9c, region);
                thiscall(0x0051bc70, rule, {bits(0.016f)});        /* inside: the rule fires (driver vf9) */
                wr(region + 0x1c8, 1);
                thiscall(0x0051bc70, rule, {bits(0.016f)});        /* outside */
                wr(arena + 0x9c, old);
            }
            for (uint32_t i = 0; i < rd(L.d + 0xee0); i++) {
                const uint32_t b = rd(L.d + 0xe8 + 4 * i), vt = rd(b);
                if (vt == 0x009401f8) { thiscall(0x00530fd0, b, {0}); thiscall(0x00530fd0, b, {1}); }
                if (vt == 0x00940220) { thiscall(0x005223c0, b, {0}); thiscall(0x005223c0, b, {1}); }
                if (vt == 0x0093fb6c) { thiscall(0x0052b620, b); wr(b + 0x38, 1); thiscall(0x0052b640, b); thiscall(0x0052b640, b); }
            }
        }},
        {"audio", [] {
            Live L = live();
            const uint32_t logic = rd(L.A + 0xfd8);
            thiscall(0x00581ba0, logic, {L.A, rd(L.p + 0x1ac)});
            thiscall(0x00582180, logic, {L.A, 3});
            thiscall(0x00582260, logic);
        }},
        {"intro", [] {   /* the FMX intro scenario's steps */
            Live L = live();
            const uint32_t sc = L.sc_intro;
            thiscall(0x0074dab0, sc);                             /* vf0 */
            for (uint32_t step : {1u, 4u, 2u}) thiscall(0x0074dbc0, sc, {step});   /* vf1 */
            for (uint32_t step : {1u, 4u, 5u, 0u}) { wr(sc + 0x151b4, step); wr(sc + 0x151c0, 0); thiscall(0x0074d700, sc); }   /* vf3 */
            wr(sc + 0x151b4, 4); wr(sc + 0x151c0, 5); thiscall(0x0074d700, sc);
            thiscall(0x0074dcf0, sc);                             /* vf6 */
            for (uint32_t k = 0; k < 6; k++) { rx_regs in = regs(); in.r[1] = L.p; call(0x0074da40, in, {k}); }
            for (uint32_t step : {0u, 1u, 4u}) { rx_regs in = regs(); in.r[1] = step; in.r[2] = L.p; call(0x0074d7a0, in); }
            { rx_regs in = regs(); in.r[6] = L.w + 0x20150; call(0x0074c5e0, in); }
        }},
        {"champ", [] {   /* the championship classes' functions on the live activity (the stub's layout) */
            Live L = live();
            thiscall(0x0057eaf0, L.A);                            /* shared vf15 / vf17 / vf37 / vf38 */
            thiscall(0x0057e270, L.A);
            thiscall(0x0057e900, L.A, {0});
            thiscall(0x0057dcf0, L.A, {1});
            thiscall(0x0057db40, L.A);                            /* vf5s */
            thiscall(0x0057ded0, L.A);
            thiscall(0x0057dbe0, L.A, {bits(0.016f)});            /* vf25 */
            thiscall(0x0057dd00, L.A, {bits(0.016f)});            /* vf28 */
            wr(L.A, 0x009443cc);
            thiscall(0x0057daf0, L.A, {0x009426b4});              /* the intro's vf2 */
            wr(L.A, 0x009445cc);
            thiscall(0x0057dc30, L.A, {0});                       /* the main event's vf2 */
        }},
        {"champcam", [] {   /* the championships' intro camera (WindDown / Post exit) */
            Live L = live();
            rx_regs in = regs(); in.r[0] = L.w;
            const uint32_t path = call(0x00767760, in);
            std::fprintf(stderr, "[freestyle] champcam: intro camera path %08x\n", path);
            if (path) {
                thiscall(0x0057dd40, L.A, {3});
                thiscall(0x0057ddd0, L.A, {4});
                thiscall(0x0057df10, L.A, {4});
            }
        }},
        {"camparts", [] {   /* the championship intro camera's pieces on a fake path (the FMX arenas have no "Freestyle_Intro_Cam") */
            Live L = live();
            const uint32_t w = halloc(0x22000), tmpl = halloc(0x200), tmpl2 = halloc(0x200);
            const char* nm = "Freestyle_Intro_Cam";
            for (uint32_t i = 0; i <= std::strlen(nm); i++) rx_wr8(tmpl2 + 0x9c + i, i < std::strlen(nm) ? uint8_t(nm[i]) : 0);
            rx_wr8(tmpl + 0x9c, 'X');
            wr(tmpl + 0x90, tmpl2);
            wr(w + 0x21de8, tmpl);
            for (uint32_t o = 0x10; o < 0x124; o += 4) { if (o != 0x90 && o != 0x94 && o != 0x98 && (o < 0x9c || o >= 0xb0)) wr(tmpl2 + o, 0x3f800000u + o); }
            rx_regs in = regs(); in.r[0] = w;
            const uint32_t path = call(0x00767760, in);
            wr(w + 0x21de8, 0);
            call(0x00767760, in);
            const uint32_t cam = halloc(0x4b0);
            in = regs(); in.r[6] = cam;
            call(0x0063d3e0, in, {path});
            wr(cam + 0x310 + 0x98, w);                            /* linked in the fake world's path list */
            wr(w + 0x21de8, cam + 0x310); wr(w + 0x21dec, cam + 0x310); wr(cam + 0x310 + 0x90, 0); wr(cam + 0x310 + 0x94, 0);
            thiscall(0x0063d3b0, cam, {0});
            (void)L;
        }},
        {"factory", [] {   /* the activity factory's Freestyle types */
            Live L = live();
            const uint32_t mgr = L.w + 0x21da0;
            for (uint32_t type : {5u, 0x14u, 0x20u, 0x3au, 0x3bu, 0x3cu}) {
                if (int32_t(rd(mgr + 8)) >= int32_t(rd(mgr + 0xc))) { std::fprintf(stderr, "[freestyle] factory: array full\n"); break; }
                rx_regs in = regs();
                const uint32_t a = call(0x0055ef80, in, {mgr, type});
                std::fprintf(stderr, "[freestyle] factory type %u -> %08x vt %08x\n", type, a, rd(a));
            }
        }},
        {"teardown", [] {   /* the end: vf3 and the deleting destructor */
            Live L = live();
            if (!std::getenv("RX_FS_RECORDS")) wr(0x00a946c0, 0);   /* MXRecords_Leave's oracle seam (as race_teardown_probe.cpp) */
            thiscall(0x00561d30, L.A);
            thiscall(0x00561960, L.A, {1});
        }},
        {"champtear", [] {   /* the championship classes' end and destructors */
            Live L = live();
            if (!std::getenv("RX_FS_RECORDS")) wr(0x00a946c0, 0);
            thiscall(0x0057dc90, L.A);
            const uint32_t dt = std::getenv("RX_FS_CHAMP_DTOR") ? uint32_t(std::strtoul(std::getenv("RX_FS_CHAMP_DTOR"), nullptr, 16)) : 0x0057dab0;
            thiscall(dt, L.A, {0});
        }},
    };
    return c;
}
}  // namespace

RX_PORT(0x0084ed20, FreestyleProbeHook, "thunk", 0) {
    g_m = &m;
    ++g_frame;
    const uint32_t act = find_activity();
    if (!act) return reflex_vm::RUN_ORIGINAL;
    if (const char* r = std::getenv("RX_FS_RULES")) {
        if (g_frame == uint32_t(std::atoi(r))) {
            std::fprintf(stderr, "[freestyle] frame %u: rules accepted (hold %u -> 0)\n", g_frame, rd(act + 0xff068));
            wr(act + 0xff068, 0);
        }
    }
    if (const char* dmp = std::getenv("RX_FS_DUMP")) {
        unsigned f = 0, a = 0, n = 0;
        if (std::sscanf(dmp, "%u:%x:%u", &f, &a, &n) == 3 && g_frame == f)
            for (unsigned k = 0; k < n; k += 4) {
                std::fprintf(stderr, "[freestyle] dump %08x:", a + 4 * k);
                for (unsigned j = 0; j < 4 && k + j < n; j++) std::fprintf(stderr, " %08x", rd(a + 4 * (k + j)));
                std::fprintf(stderr, "\n");
            }
    }
    if (const char* i = std::getenv("RX_FS_INFO")) {
        const uint32_t n = uint32_t(std::atoi(i));
        if (n && g_frame % n == 0) {
            const uint32_t d0 = rd(rd(act + 0x268));
            std::fprintf(stderr, "[freestyle] frame %u: act %08x vt %08x type %u mode %u state %u hold %u intro %u post %u/%u timer %.3f "
                                 "elapsed %.3f dur %.3f drivers %u d0 %08x (vt %08x) score %.3f crowd %.3f/%u/%u\n",
                         g_frame, act, rd(act), rd(act + 0x23c), rd(act + 0x238), rd(act + 0x21c), rd(act + 0xff068), rd(act + 0xff074),
                         rd(act + 0x13e5f8), rd(act + 0x13e5f0), rf(act + 0xff064), rf(act + 0x294), rf(act + 0x2bc), rd(act + 0x26c), d0,
                         rd(d0), d0 ? rf(rd(d0 + 0x74) + 0xcec) : 0.0f, rf(act + 0x13e600), rd(act + 0x13e604), rd(act + 0x13e608));
        }
    }
    if (const char* sel = std::getenv("RX_FS_PROBE")) {
        const uint32_t at = std::getenv("RX_FS_FRAME") ? uint32_t(std::atoi(std::getenv("RX_FS_FRAME"))) : 1000;
        if (g_frame == at) {
            std::string s(sel);
            size_t pos = 0;
            while (pos <= s.size()) {
                size_t e = s.find(',', pos);
                if (e == std::string::npos) e = s.size();
                const std::string t = s.substr(pos, e - pos);
                pos = e + 1;
                if (t.empty()) continue;
                const auto it = cases().find(t);
                if (it == cases().end()) { std::fprintf(stderr, "[freestyle] unknown case %s\n", t.c_str()); continue; }
                run_case(m, t, it->second);
            }
            std::fprintf(stderr, "[freestyle] frame %u: cases done\n", g_frame);
        }
    }
    return reflex_vm::RUN_ORIGINAL;
}
