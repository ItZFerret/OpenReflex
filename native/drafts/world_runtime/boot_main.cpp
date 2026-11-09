// reflex_boot: boots MXReflex.exe from an install inside the interpreter (diagnostics CLI).
//   reflex_boot <game_dir> <crt_dir> <save_dir> [frames] [log]
#include "boot.h"
#include "clock_pin.h"
#include "port.h"
#include "jit.h"
#include "runtime.h"
#include "vm.h"
#include "ambient.h"
#include "atv.h"
#include "modes.h"
#include "career.h"
#include "garage.h"
#include "tutorial.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include "vm.h"
namespace reflex_vm { extern uint32_t g_block_watch; const char* mnemonic_name(int i); }
int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: %s game_dir crt_dir save_dir [frames] [log]\n", argv[0]); return 2; }
    if (uint64_t fd = reflex_vm::fixed_clock_ticks())
        std::printf("clock: pinned %llu x 100 ns per frame (%.3f Hz), REFLEX_FIXED_CLOCK\n", (unsigned long long)fd, 1e7 / double(fd));
    reflex_vm::BootConfig c; c.game_dir = argv[1]; c.crt_dir = argv[2]; c.save_dir = argv[3]; if (argc > 5) c.log_path = argv[5];
    // a career series boot (REFLEX_EVENT_SERIES below) signs the profile in with the Steam remote storage in the save folder, as the
    // plugin's career events do (plugin_api.cpp)
    // REFLEX_TEST_DRIVE (w-testride): the garage's TEST DRIVE (garage.cpp start_test_drive) instead of the event's start, signed in the same way
    if (std::getenv("REFLEX_EVENT_SERIES") || std::getenv("REFLEX_TEST_DRIVE")) { c.remote_storage = std::string(argv[3]) + "/remote"; c.steam_persona = reflex_vm::career::persona(argv[3]); }
    uint32_t frames = argc > 4 ? uint32_t(std::atoi(argv[4])) : 10;
    if (const char* w = std::getenv("REFLEX_WORLD_TEST")) {   // run a boot-made world image in the world runtime
        std::string o = w; reflex_vm::World world;
        try {
            world.load(c.game_dir + "/MXReflex.exe", c.crt_dir + "/msvcr90.dll", o + "-world.bin", o + "-deform.bin");
            uint8_t keys[256] = {}; std::printf("loaded; whole frames %d\n", int(world.whole_frames()));
            {   // the mode worlds (Section Race, Freestyle ...) open the UI pages' videos: movies are not played (BinkOpen 0), as tutorial.cpp / boot_devices.inc
                auto& bm = world.machine();
                bm.imports["binkw32.dll!_BinkOpen@8"] = [](reflex_vm::Machine& mm) { return mm.ret(8, 0); };
                for (auto [f, n] : std::vector<std::pair<const char*, uint32_t>>{{"_BinkSetSoundOnOff@8", 8}, {"_BinkWait@4", 4}, {"_BinkNextFrame@4", 4}, {"_BinkGetFrameBuffersInfo@8", 8},
                                                                                {"_BinkShouldSkip@4", 4}, {"_BinkPause@8", 8}, {"_BinkSetVolume@12", 12}, {"_BinkClose@4", 4},
                                                                                {"_BinkDoFrame@4", 4}, {"_BinkGoto@12", 12}, {"_BinkRegisterFrameBuffers@8", 8}, {"_BinkGetRects@8", 8}}) {
                    std::string k = std::string("binkw32.dll!") + f;
                    if (!bm.imports.count(k)) { uint32_t b = n; bm.imports[k] = [b](reflex_vm::Machine& mm) { return mm.ret(b, 0); }; }
                }
            }
            if (std::getenv("REFLEX_AMBIENT_OFF")) world.set_ambient_animation(false);
            if (const char* k = std::getenv("REFLEX_AMBIENT_KEEP")) world.set_ambient_mask(uint32_t(std::strtoul(k, nullptr, 16)));   // ambient classes kept (ambient.cpp)
            if (std::getenv("REFLEX_PARTICLES")) world.set_particles(true);   // particle managers simulated (particles.cpp)
            if (std::getenv("REFLEX_JIT")) world.machine().enable_jit(true);
            if (std::getenv("REFLEX_TABLES")) {   // diagnostics: run-time tables with their row counts and columns
                auto& m = world.machine(); auto rd = [&](uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; };
                for (uint32_t t : {0xae0e08u, 0xae19b0u, 0xae05c8u, 0xae1b18u}) {
                    char name[64] = {}; m.mem.read_bytes(t + 0x3c, name, 63);
                    uint32_t b = rd(t + 0x10), e = rd(t + 0x14), rows = 0; std::string cols;
                    for (uint32_t p = b; p < e && p < b + 4 * 60; p += 4) { uint32_t c = rd(p); char cn[33] = {}; m.mem.read_bytes(c, cn, 32); cols += std::string(" ") + cn + ":" + std::to_string(rd(c + 0x2c)); if (p == b) { uint32_t hd = rd(c + 0x40); static const uint32_t st[] = {16,4,4,1,28,24,8}; uint32_t ty = rd(c + 0x2c); if (ty < 7) rows = (rd(hd + 0x10) - rd(hd + 0xc)) / st[ty]; } }
                    std::printf("static table %08x %s rows %u:%s\n", t, name, rows, cols.c_str());
                }
                for (uint32_t db : {0xae0910u, 0xae10f0u}) {
                    uint32_t count = rd(db + 0x1e8);
                    for (uint32_t i = 0; i < count && i < 108; i++) {
                        uint32_t t = rd(db + 0x38 + 4 * i); if (!t) continue;
                        char name[64] = {}; m.mem.read_bytes(t + 0x3c, name, 63);
                        uint32_t b = rd(t + 0x10), e = rd(t + 0x14), rows = 0; std::string cols;
                        for (uint32_t p = b; p < e && p < b + 4 * 40; p += 4) { uint32_t c = rd(p); char cn[33] = {}; m.mem.read_bytes(c, cn, 32); cols += std::string(" ") + cn + ":" + std::to_string(rd(c + 0x2c)); if (p == b) { uint32_t hd = rd(c + 0x40); static const uint32_t st[] = {16,4,4,1,28,24,8}; uint32_t ty = rd(c + 0x2c); if (ty < 7) rows = (rd(hd + 0x10) - rd(hd + 0xc)) / st[ty]; } }
                        if (std::getenv("REFLEX_TABLES")[0] == '*' || std::strstr(name, std::getenv("REFLEX_TABLES"))) std::printf("db %x table %08x %s rows %u:%s\n", db, t, name, rows, cols.c_str());
                    }
                }
            }
            if (std::getenv("REFLEX_SLOTS")) {   // diagnostics: rider slots and names reachable from each vehicle / rider
                auto& m = world.machine(); auto rd = [&](uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; };
                uint32_t G = rd(0xa946b8);
                for (uint32_t k = 0; k < 12; k++) {
                    uint32_t P = G + 0x7460 + k * 0x12a0, V = rd(P + 0x1ac), R = rd(P + 0x1b4); if (!V) continue;
                    std::printf("slot %u P %08x type %u V %08x R %08x vvt %08x rvt %08x:", k, P, rd(P + 0x10), V, R, rd(V), rd(R));
                    std::set<std::string> names;
                    for (uint32_t base : {V, R, P}) for (uint32_t off = 0; off < 0x1400; off += 4) {
                        uint32_t q = rd(base + off); if (q < 0x10000 || !m.mem.mapped(q)) continue;
                        for (uint32_t o2 : {0u, 4u, 8u, 0x18u}) { std::string t; for (uint32_t i = 0; i < 48; i++) { uint8_t c = uint8_t(m.mem.mapped(q + o2 + i) ? m.mem.read(q + o2 + i, 1) : 0); if (!c) break; if (c < 32 || c > 126) { t.clear(); break; } t += char(c); }
                            if (t.size() >= 6 && std::isalpha((unsigned char)t[0]) && t.find('_') != std::string::npos) names.insert(t); }
                    }
                    for (auto& n : names) std::printf(" %s", n.c_str());
                    std::printf("\n");
                }
                // offsets of P whose values differ across AI slots and look like small table ids
                for (uint32_t off = 0; off < 0x12a0; off += 4) {
                    std::set<uint32_t> vals; bool small = true; std::string row;
                    for (uint32_t k = 1; k < 12; k++) { uint32_t v = rd(G + 0x7460 + k * 0x12a0 + off); vals.insert(v); if (v > 5000) small = false; char b[16]; std::snprintf(b, sizeof b, " %u", v); row += b; }
                    if (small && vals.size() >= 8) std::printf("P+%03x:%s\n", off, row.c_str());
                }
            }
            if (std::getenv("REFLEX_GRID_DIAG")) {   // gridfix: the Player List start zones, the game rand, the grid and the player slots
                auto& m = world.machine();
                auto rd = [&](uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; };
                auto rf = [&](uint32_t a) { uint32_t u = rd(a); float f; std::memcpy(&f, &u, 4); return f; };
                auto rstr = [&](uint32_t a) { std::string s; for (uint32_t i = 0; i < 63; i++) { uint8_t c = uint8_t(m.mem.mapped(a + i) ? m.mem.read(a + i, 1) : 0); if (!c) break; s += char(c); } return s; };
                std::printf("GRID game_rand %08x game_count %u\n", rd(0x9cb860), rd(0x9cb864));
                uint32_t G = rd(0xa946b8), cnt = G ? uint32_t(m.mem.read(G + 0x1541c, 1)) : 0;
                uint32_t A = 0;
                for (uint32_t k = 0; k < cnt && k < 12; k++) { uint32_t P0 = G + 0x7460 + rd(G + 0x153ec + 4 * k) * 0x12a0; if (rd(P0 + 0x94) && rd(P0 + 0x10) == 0) { A = rd(P0 + 0x94); break; } }
                std::printf("GRID players %u activity %08x", cnt, A);
                if (A) std::printf(" state %u type %u mode %u gridcount %u", rd(A + 0x21c), rd(A + 0x23c), rd(A + 0x238), rd(A + 0x880));
                std::printf("\n");
                for (uint32_t k = 0; k < cnt && k < 12; k++) {
                    uint32_t sl = rd(G + 0x153ec + 4 * k), P = G + 0x7460 + sl * 0x12a0, d = 0, idx = 0xffffffffu;
                    for (uint32_t q = 0, n = A ? rd(A + 0x26c) : 0, list = A ? rd(A + 0x268) : 0; list && q < n && q < 32; q++) { uint32_t dd = rd(list + 4 * q); if (dd && rd(dd + 0x74) == P) { d = dd; break; } }
                    uint32_t ds = d ? rd(d + 0x144) : 0;
                    if (A && ds >= A + 0x340 && ds < A + 0x340 + 16u * 0x30) idx = (ds - (A + 0x340)) / 0x30;
                    std::printf("GRID player %u slot %u %-20s local %u kind %u startzone %d P+1ac %08x driver %08x grid %d\n", k, sl,
                                rstr(P + 0x14).c_str(), rd(P + 4), rd(P + 0x10), int32_t(rd(P + 0x1dc)), rd(P + 0x1ac), d, int32_t(idx));
                }
                if (A) for (uint32_t i = 0; i < 16; i++) {
                    uint32_t s = A + 0x340 + 0x30 * i;
                    std::printf("GRID slot %u x %08x %.4f y %.4f z %.4f users %u\n", i, rd(s), rf(s), rf(s + 4), rf(s + 8), rd(s + 0x28));
                }
            }
            static std::map<std::string, uint64_t> callers;
            if (const char* ca = std::getenv("REFLEX_CALLERS")) {   // count (this vtable, 3 return addresses) at a function's entry
                uint32_t at = uint32_t(std::strtoul(ca, nullptr, 16));
                world.machine().natives[at] = [](reflex_vm::Machine& m) -> uint32_t {
                    char b[96]; uint32_t vt = m.mem.mapped(m.r[1]) ? uint32_t(m.mem.read(m.r[1], 4)) : 0; std::snprintf(b, sizeof b, "this-vt %08x", vt); std::string k = b;
                    for (uint32_t a = m.r[4], n = 0, i = 0; i < 512 && n < 3; a += 4, i++) { uint32_t v = uint32_t(m.mem.read(a, 4)); if (v > 0x401000 && v < 0x916000) { std::snprintf(b, sizeof b, " <%08x", v); k += b; n++; } }
                    callers[k]++; return reflex_vm::RUN_ORIGINAL; };
            }
            static std::map<std::string, std::pair<uint64_t, uint64_t>> sites;   // key -> (instructions, calls)
            if (const char* ss = std::getenv("REFLEX_SITES")) {   // "call:ret,..." -> instructions spent per (site, this vtable)
                std::string list = ss;
                for (size_t at = 0; at < list.size();) {
                    size_t comma = list.find(',', at); std::string item = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
                    at = comma == std::string::npos ? list.size() : comma + 1;
                    uint32_t c = uint32_t(std::strtoul(item.c_str(), nullptr, 16)), r = uint32_t(std::strtoul(item.c_str() + item.find(':') + 1, nullptr, 16));
                    auto stack = std::make_shared<std::vector<std::pair<std::string, uint64_t>>>();
                    world.machine().natives[c] = [stack, c](reflex_vm::Machine& m) -> uint32_t {
                        char b[48]; std::snprintf(b, sizeof b, "%08x vt %08x", c, m.mem.mapped(m.r[1]) ? uint32_t(m.mem.read(m.r[1], 4)) : 0u);
                        stack->push_back({b, m.steps}); return reflex_vm::RUN_ORIGINAL; };
                    world.machine().natives[r] = [stack](reflex_vm::Machine& m) -> uint32_t {
                        if (!stack->empty()) { auto& e = sites[stack->back().first]; e.first += m.steps - stack->back().second; e.second++; stack->pop_back(); }
                        return reflex_vm::RUN_ORIGINAL; };
                }
            }
            static std::vector<uint64_t> histo;
            if (std::getenv("REFLEX_HISTO")) { histo.assign(2 * reflex_vm::M_COUNT, 0); world.machine().histogram = histo.data(); }
            if (std::getenv("REFLEX_JIT_HISTO")) { histo.assign(2 * reflex_vm::M_COUNT, 0); world.machine().jit_histo = histo.data(); }
            if (const char* ww = std::getenv("REFLEX_WORLD_WATCH")) {   // debugging: "addr:file", every interpreted write covering addr (pc)
                auto& m = world.machine(); m.watch_address = uint32_t(std::strtoul(ww, nullptr, 16)); m.trace_out = std::fopen(std::strchr(ww, ':') + 1, "w");
                m.trace_from = m.trace_to = 0; m.enable_jit(false);
            }
            static std::vector<uint32_t> coverage; uint32_t coverage_from = 0;
            static std::vector<uint32_t> dll_coverage;
            if (const char* cv = std::getenv("REFLEX_COVERAGE")) { (void)cv; coverage.assign(0x600000, 0); dll_coverage.assign(0x200000, 0); world.machine().dll_counts = dll_coverage.data(); coverage_from = std::getenv("REFLEX_COVERAGE_FROM") ? uint32_t(std::atoi(std::getenv("REFLEX_COVERAGE_FROM"))) : 0; }
            if (std::getenv("REFLEX_READSET")) { static std::vector<uint8_t> rmap(1u << 25, 0); world.machine().mem.read_map = rmap.data(); }
            if (const char* pf = std::getenv("REFLEX_PROFILE")) { world.machine().trace_every = 9973; world.machine().trace_out = std::fopen(pf, "w"); }
            auto t0 = std::chrono::steady_clock::now();
            static uint32_t gridfix_frame = 0;
            if (std::getenv("REFLEX_RAND_CALLERS")) {   // gridfix: the CRT rand's callers per world frame (the countdown entry's per-rider draws)
                auto& m = world.machine(); auto it = m.crt_exports.find("rand");
                if (it == m.crt_exports.end()) std::printf("RANDCALLERS: no rand export\n");
                else m.natives[it->second] = [](reflex_vm::Machine& m) -> uint32_t {
                    char b[160]; int n = 0; std::snprintf(b, sizeof b, "RANDCALL f %u esp %08x ret", gridfix_frame, m.r[4]);
                    for (uint32_t a = m.r[4], i = 0; i < 24 && n < 5; a += 4, i++) { uint32_t v = m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0; if (v > 0x401000 && v < 0x916000) { char t[16]; std::snprintf(t, sizeof t, " %08x", v); std::strncat(b, t, sizeof b - std::strlen(b) - 1); n++; } }
                    std::fprintf(stderr, "%s\n", b); return reflex_vm::RUN_ORIGINAL; };
            }
            for (uint32_t f = 0; f < frames; f++) {
                gridfix_frame = f;
                auto& m = world.machine(); uint32_t G = uint32_t(m.mem.read(0xa946b8, 4)), A = uint32_t(m.mem.read(G + 0x7460 + 0x94, 4));
                int race = A ? int(m.mem.read(A + 0x21c, 4)) : -1;
                keys[0x11] = race >= 2 && !std::getenv("REFLEX_NO_W") ? 0x80 : 0;   // W after the gate drop (REFLEX_NO_W: none, the pad drives alone)
                if (const char* dv = std::getenv("REFLEX_DRIVE")) {   // a deterministic random rider (native/game verification): after the drop
                    static uint32_t seed = uint32_t(std::strtoul(dv, nullptr, 10)) * 2654435761u + 1, until = 0;   // (no race: after 120 frames)
                    static uint8_t held[8] = {};
                    static const uint8_t pool[8] = {0x11, 0x1f, 0x1e, 0x20, 0xc8, 0xd0, 0xcb, 0xcd};   // W S A D, arrows (rider lean)
                    bool go = race >= 2 || (race < 0 && f >= 120);
                    if (go && f >= until) {
                        auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
                        until = f + 20 + rnd() % 70;
                        held[0] = rnd() % 10 < 8; held[1] = !held[0] && rnd() % 3 == 0;          // throttle mostly, sometimes brake
                        uint32_t st = rnd() % 3; held[2] = st == 1; held[3] = st == 2;            // steer left / right / none
                        uint32_t ln = rnd() % 5; held[4] = ln == 1; held[5] = ln == 2; held[6] = ln == 3; held[7] = ln == 4;
                    }
                    for (int i = 0; i < 8; i++) keys[pool[i]] = go && held[i] ? 0x80 : 0;
                }
                if (std::getenv("REFLEX_HOLD_W")) keys[0x11] = 0x80;   // framecmp: hold W from the first frame (the input/clock check)
                if (const char* pk = std::getenv("REFLEX_POKE")) {   // verification stimulus (fxport): "frame:addr:value[,frame:addr:value]" 32-bit writes (hex addr / value, decimal frame) at the start of that frame
                    for (const char* p = pk; *p;) {
                        char* e = nullptr; uint32_t pf = uint32_t(std::strtoul(p, &e, 10)); if (e == p || *e != ':') break;
                        uint32_t pa = uint32_t(std::strtoul(e + 1, &e, 16)); if (*e != ':') break;
                        uint32_t pv = uint32_t(std::strtoul(e + 1, &e, 16));
                        if (pf == f) world.machine().mem.write(pa, 4, pv);
                        p = *e ? e + 1 : e;
                    }
                }
                if (!coverage.empty() && f == coverage_from) {
                    m.enable_jit(false);
                    if (const char* roots = std::getenv("REFLEX_COVERAGE_ROOT")) {   // count only inside these functions' calls (inclusive)
                        for (const char* p = roots; *p;) {
                            char* e = nullptr; uint32_t at = uint32_t(std::strtoul(p, &e, 16)); if (e == p) break; p = *e ? e + 1 : e;
                            static int depth = 0;
                            m.natives[at] = [at](reflex_vm::Machine& mm) -> uint32_t {
                                if (depth > 0) return reflex_vm::RUN_ORIGINAL;   // nested: already counting
                                uint32_t esp = mm.r[4], ra = uint32_t(mm.mem.read(esp, 4));
                                mm.mem.write(esp, 4, 0); depth++; mm.pc_counts = coverage.data();
                                mm.run(at); depth--; mm.pc_counts = nullptr; mm.mem.write(esp, 4, ra);
                                return ra;
                            };
                        }
                    } else m.pc_counts = coverage.data();
                }
                if (const char* se = std::getenv("REFLEX_SEED")) {   // framecmp: the product's FRAMECMP_SEED (builds/framecmp/README.md)
                    static const uint32_t seed = uint32_t(std::strtoul(se, nullptr, 16));
                    const bool pre = !A || (m.mem.mapped(A + 0x21c) && int32_t(m.mem.read(A + 0x21c, 4)) == 0);
                    if (pre) {
                        uint32_t ptd = uint32_t(m.call_function(0x7854383f, nullptr, 0));
                        if (ptd && m.mem.mapped(ptd + 0x14)) m.mem.write(ptd + 0x14, 4, seed);
                    }
                }
                // REFLEX_MODE_UI=<frame>: the mode worlds (Section Race ...) run the game's own UI pages (modes.cpp ui_activate: the InGame HUB route
                // "Goto<Activity>SinglePlayer"); REFLEX_PRESS="frame:dik[,frame:dik]": that DirectInput key (Enter 1c = accept the rules popup) is held
                // for 3 frames from that frame
                if (const char* mu = std::getenv("REFLEX_MODE_UI")) {
                    if (f == uint32_t(std::atoi(mu))) std::printf("mode UI route: %s\n", reflex_vm::ui_activate(m).c_str());
                }
                if (const char* pr = std::getenv("REFLEX_PRESS")) {
                    for (const char* p = pr; *p;) {
                        char* e = nullptr; uint32_t pf = uint32_t(std::strtoul(p, &e, 10)); if (e == p || *e != ':') break;
                        uint32_t dik = uint32_t(std::strtoul(e + 1, &e, 16));
                        if (f >= pf && f < pf + 3) keys[dik & 255] = 0x80;
                        p = *e ? e + 1 : e;
                    }
                }
                // REFLEX_FRAME_100NS=<n> (w-physparity, 2026-10-28): the frame's clock advance in 100 ns units instead of 1/60 s (333333 = a 30 fps frame)
                static const uint32_t frame_100ns = std::getenv("REFLEX_FRAME_100NS") ? uint32_t(std::atoi(std::getenv("REFLEX_FRAME_100NS"))) : 166667u;
                world.engine_frame(frame_100ns, keys, nullptr, 0);
                // REFLEX_FRAME_TRACE=<file> (framecmp, 2026-10-20): per-frame gameplay trace for the product comparison
                // (builds/framecmp; the product's core/api/rx_session.c getters read the same guest fields). One line per record; every
                // float is its raw 32-bit word. Nothing is written except the lazy node evaluation the renderer does (same as the
                // product getters); the trace does not change the frame.
                if (const char* tr = std::getenv("REFLEX_FRAME_TRACE")) {
                    static FILE* tf = std::fopen(tr, "w");
                    if (tf) {
                        auto& m2 = world.machine();
                        auto rd = [&](uint32_t a) { return m2.mem.mapped(a) ? uint32_t(m2.mem.read(a, 4)) : 0u; };
                        auto wf = [&](uint32_t a) { std::fprintf(tf, " %08x", rd(a)); };
                        uint32_t G = rd(0xa946b8), P = 0;
                        if (G) {
                            uint32_t cnt = m2.mem.mapped(G + 0x1541c) ? uint32_t(m2.mem.read(G + 0x1541c, 1)) : 0u;
                            for (uint32_t k = 0; k < cnt && k < 8; k++) {
                                uint32_t sl = rd(G + 0x153ec + 4 * k); if (sl >= 16) continue;
                                uint32_t p = G + 0x7460 + sl * 0x12a0;
                                if (rd(p + 0x10) == 0) { P = p; break; }
                            }
                        }
                        uint32_t A = P ? rd(P + 0x94) : 0, V = P ? rd(P + 0x1ac) : 0, S = V ? rd(V + 0x234) : 0;
                        uint32_t ptd = uint32_t(m2.call_function(0x7854383f, nullptr, 0));
                        std::fprintf(tf, "O f %u st %d gt %08x edt %08x gdt %08x ldt %08x rs %08x gr %08x\n", f, A ? int(rd(A + 0x21c)) : -1,
                                     rd(0xd728d0 + 0x3c), rd(0xd728d0 + 0x18), 0u, 0u,
                                     (ptd && m2.mem.mapped(ptd + 0x14)) ? rd(ptd + 0x14) : 0u, rd(0x009cb860));
                        // race: the local driver of the activity (rx_session_race's chains)
                        uint32_t d = 0, n = A ? rd(A + 0x26c) : 0, list = A ? rd(A + 0x268) : 0;
                        for (uint32_t k = 0; list && k < n && k < 64; k++) { uint32_t dd = rd(list + 4 * k); if (dd && rd(dd + 0x74) == P) { d = dd; break; } }
                        uint32_t board = A ? rd(A + 0x2cc) : 0;
                        std::fprintf(tf, "O race %d %d %d %d %d %d %d", 0, d ? 1 : 0, A ? int(rd(A + 0x21c)) : 0,
                                     d ? int(rd(d + 0xf1c)) : 0, d ? int(rd(d + 0x78)) + 1 : 0, int(n), A ? int(rd(A + 0xff070)) : 0);
                        wf(d ? d + 0xf20 : 0); wf(d ? d + 0x10e0 : 0); wf(d ? d + 0x10c4 : 0); wf(d ? d + 0x134 : 0);
                        wf(board ? board + 0x4f4 : 0);
                        {   // lap progress: 1 - to_line / length, clamped (rx_session_race)
                            float progress = 0.0f;
                            if (d) {
                                uint32_t track = rd(d + 0x14), node = track ? rd(track) : 0;
                                float length = 0.0f, to_line = 0.0f;
                                if (node) { uint32_t v = rd(node + 0x13d * 4); std::memcpy(&length, &v, 4); }
                                { uint32_t v = rd(d + 0x10cc); std::memcpy(&to_line, &v, 4); }
                                if (length > 0.0f) {
                                    progress = 1.0f - to_line / length;
                                    if (progress < 0.0f) progress = 0.0f;
                                    if (progress > 1.0f) progress = 1.0f;
                                }
                            }
                            uint32_t pb; std::memcpy(&pb, &progress, 4);
                            std::fprintf(tf, " %08x", pb);
                        }
                        wf(d ? d + 0x116c : 0);
                        std::fprintf(tf, "\n");
                        // bike (body 0) and the vehicle's render bodies
                        std::fprintf(tf, "O bike %d %08x %d", (V && S) ? 0 : 1, rd(S ? S + 0x94 : 0), S ? int(rd(S + 0x8ac0)) : 0);
                        for (uint32_t k = 0; k < 4; k++) wf(S ? S + 0x100 + 4 * k : 0);
                        for (uint32_t k = 0; k < 8; k++) wf(S ? S + 0x1c + 4 * k : 0);
                        std::fprintf(tf, " %u\n", f);
                        uint32_t bodies = (S && rd(S) == 0x93c8a4u) ? rd(S + 0xec) : 0;
                        std::fprintf(tf, "O b0");
                        for (uint32_t k = 0; k < 16; k++) wf(S ? S + 0xf0 + 4 * k : 0);
                        for (uint32_t k = 0; k < 16; k++) wf(S ? S + 0x730 + 4 * k : 0);
                        for (uint32_t k = 0; k < 16; k++) wf(S ? S + 0x90 + 4 * k : 0);
                        for (uint32_t k = 0; k < 16; k++) wf(S ? S + 0x6d0 + 4 * k : 0);
                        std::fprintf(tf, "\n");
                        for (uint32_t b = 0; b < bodies && b < 6; b++) {
                            std::fprintf(tf, "O br %u", b);
                            for (uint32_t k = 0; k < 4; k++) wf(S + 0xf0 + 0xa0 * b + 0x90 + 4 * k);
                            for (uint32_t k = 0; k < 4; k++) wf(S + 0xf0 + 0xa0 * b + 0x6d0 + 4 * k);
                            std::fprintf(tf, "\n");
                        }
                        {   // veh: rx_session_vehicle_info's fields
                            uint32_t model = V ? rd(V + 0x1328) : 0, vvt = V ? rd(V) : 0;
                            int type = -1; uint32_t klass_bodies = 0, gear_off = 0, eng_off = 0;
                            if (vvt == 0x0093cd84u && S && rd(S) == 0x0093c8a4u) { type = 0; klass_bodies = 4; gear_off = 0x8ac0; eng_off = 0x8abc; }
                            std::fprintf(tf, "O veh %d %d %d %d %d", V ? 0 : 1, type, klass_bodies ? int(bodies) : 0,
                                         model ? int(rd(model + 0x10 + 0xc)) : 0, klass_bodies ? int(rd(S + gear_off)) + 1 : 0);
                            wf(S && klass_bodies ? S + eng_off : 0); wf(V ? V + 0x150 : 0); std::fprintf(tf, " %08x\n", V);
                        }
                        // bike model nodes: rx_session_bike_pose (evaluate the lazy world matrices first, as the renderer does)
                        {
                            uint32_t model = V ? rd(V + 0x1328) : 0, hier = model + 0x10;
                            uint32_t recs = model ? rd(hier + 8) : 0, cnt2 = model ? rd(hier + 0xc) : 0, wm = model ? rd(hier + 0x1c) : 0;
                            int nn = (recs && wm && cnt2 > 0 && cnt2 <= 64) ? int(cnt2) : 0;
                            if (nn) { uint32_t zero = 0; for (int32_t k = nn - 1; k >= 0; k--) m2.call_function(0x7db890, &zero, 1, recs + 16 * uint32_t(k)); }
                            std::fprintf(tf, "O node %d", nn);
                            for (int k = 0; k < nn * 16; k++) wf(wm + 4 * uint32_t(k));
                            std::fprintf(tf, "\n");
                        }
                        // rider bones: rx_session_rider_pose (69 bones of RiderAnimation 0x00958724)
                        {
                            uint32_t R = P ? rd(P + 0x1b4) : 0;
                            uint32_t recs = (R && rd(R) == 0x00958724u) ? rd(R + 0x188) : 0, cnt2 = R ? rd(R + 0x18c) : 0, wm = R ? rd(R + 0x19c) : 0;
                            int nn = (recs && wm && cnt2 == 69) ? 69 : 0;
                            if (nn) { uint32_t zero = 0; for (int32_t k = nn - 1; k >= 0; k--) m2.call_function(0x7db890, &zero, 1, recs + 16 * uint32_t(k)); }
                            std::fprintf(tf, "O bone %d", nn);
                            for (int k = 0; k < nn * 16; k++) wf(wm + 4 * uint32_t(k));
                            std::fprintf(tf, "\n");
                        }
                        {   // camera: rx_session_camera
                            uint32_t cam = rd(0xa7b1d8);
                            if (!cam && rd(0xd6f620) == 0x0095a2d8u) cam = 0xd6f620;
                            std::fprintf(tf, "O cam %d", cam ? 0 : 2);
                            for (uint32_t k = 0; k < 3; k++) wf(cam ? cam + 0x10 + 4 * k : 0);
                            wf(0); for (uint32_t k = 0; k < 3; k++) wf(cam ? cam + 0x20 + 4 * k : 0);
                            wf(0); for (uint32_t k = 0; k < 3; k++) wf(cam ? cam + 0x30 + 4 * k : 0);
                            wf(0); for (uint32_t k = 0; k < 3; k++) wf(cam ? cam + 0x40 + 4 * k : 0);
                            wf(0);
                            wf(cam ? cam + 0x50 : 0); wf(cam ? cam + 0x58 : 0); wf(cam ? cam + 0x5c : 0);
                            std::fprintf(tf, "\n");
                        }
                        {   // standings: rx_session_standings
                            uint32_t nonnull = 0;
                            for (uint32_t k = 0; list && k < n && k < 64; k++) if (rd(list + 4 * k)) nonnull++;
                            uint32_t shown = nonnull < 16 ? nonnull : 16;
                            std::fprintf(tf, "O stands %u\n", shown);
                            for (uint32_t k = 0, kk = 0; list && k < n && k < 64 && kk < shown; k++) {
                                uint32_t dd = rd(list + 4 * k); if (!dd) continue;
                                uint32_t dp = rd(dd + 0x74);
                                int slot = (P && dp >= G + 0x7460) ? int((dp - (G + 0x7460)) / 0x12a0) : -1;
                                std::fprintf(tf, "O stand %d %d %d %d %d", slot, dp == P ? 1 : 0, int(rd(dd + 0x78)) + 1,
                                             int(rd(dd + 0xf1c)), int(rd(dd + 0x70)));
                                wf(dd + 0x134); wf(dd + 0x10c4); wf(dd + 0x10e0); std::fprintf(tf, " %08x\n", 0u);
                                kk++;
                            }
                        }
                        {   // grid: the local driver's start slot record (A + 0x340 + 0x30 * index; driver +0x144)
                            uint32_t slot = d ? rd(d + 0x144) : 0;
                            int idx = -1;
                            if (A && slot >= A + 0x340 && slot < A + 0x340 + 16 * 0x30) idx = int((slot - (A + 0x340)) / 0x30);
                            std::fprintf(tf, "O grid %d %08x", idx, slot);
                            for (uint32_t k = 0; k < 12; k++) wf(slot ? slot + 4 * k : 0);
                            std::fprintf(tf, "\n");
                        }
                        std::fflush(tf);
                    }
                }
                // REFLEX_RACE_STATE=<file> (native/game tools/race_compare.py, notes/loading.md): the race without addresses every
                // REFLEX_RACE_STATE_EVERY frames (60) and at the end: per slot the vehicle's bodies, gear, rpm, speed and a hash of its
                // simulation / rider objects with pointer-like words left out; the race state and every driver's place, laps and times
                if (static FILE* rs = std::getenv("REFLEX_RACE_STATE") ? std::fopen(std::getenv("REFLEX_RACE_STATE"), "w") : nullptr; rs) {
                    static const uint32_t every = std::getenv("REFLEX_RACE_STATE_EVERY") ? uint32_t(std::atoi(std::getenv("REFLEX_RACE_STATE_EVERY"))) : 60;
                    if (f % every == 0 || f + 1 == frames) {
                        auto rd = [&](uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; };
                        auto rf = [&](uint32_t a) { uint32_t v = rd(a); float x; std::memcpy(&x, &v, 4); return x; };
                        auto masked = [&](uint32_t base, uint32_t n) {   // FNV over the words that are not addresses (heap, stacks, device)
                            uint64_t h = 1469598103934665603ull; if (!base) return h;
                            for (uint32_t a = base; a < base + n; a += 4) { uint32_t v = rd(a);   // (boot heap 0x10000000.., runtime heap 0x30000000.., stacks 0x7f..)
                                if (((v >= 0x10000000u && v < 0x34000000u) || (v >= 0x7f000000u && v < 0x80000000u)) && m.mem.mapped(v)) continue;
                                h = (h ^ v) * 1099511628211ull; }
                            return h; };
                        for (uint32_t k = 0; k < 12; k++) {
                            uint32_t P = G + 0x7460 + k * 0x12a0; reflex_vm::VehicleInfo v;
                            if (!G || !reflex_vm::vehicle_info(m.mem, P, int32_t(k), v)) continue;
                            std::fprintf(rs, "f %u slot %u type %d gear %d rpm %.9g speed %.9g sim %016llx rider %016llx", f, k, v.type, v.gear, v.rpm, v.speed,
                                         (unsigned long long)masked(v.sim, 0x3000), (unsigned long long)masked(rd(P + 0x1b4), 0x2000));
                            for (int i = 0; i < v.bodies && i < 6; i++) std::fprintf(rs, " b%d %.9g %.9g %.9g %.9g %.9g %.9g %.9g", i, v.render_position[i][0], v.render_position[i][1], v.render_position[i][2],
                                                                                v.render_orientation[i][0], v.render_orientation[i][1], v.render_orientation[i][2], v.render_orientation[i][3]);
                            std::fputc('\n', rs);
                        }
                        uint32_t A2 = G ? rd(G + 0x7460 + 0x94) : 0;
                        if (A2) {
                            std::fprintf(rs, "f %u race %d act %08x hold %u", f, int(rd(A2 + 0x21c)), A2, rd(A2 + 0xff068));
                            uint32_t drv = rd(A2 + 0x268), n = rd(A2 + 0x26c);
                            for (uint32_t i = 0; i < n && i < 64; i++) { uint32_t dd = rd(drv + 4 * i); if (!dd) continue; uint32_t p = rd(dd + 0x74);
                                std::fprintf(rs, " d%d place %d laps %d fin %d time %.9g best %.9g", p >= G + 0x7460 ? int((p - (G + 0x7460)) / 0x12a0) : -1, int(rd(dd + 0x78)) + 1,
                                             int(rd(dd + 0xf1c)), int(rd(dd + 0x70)), rf(dd + 0x134), rf(dd + 0x10c4)); }
                            std::fputc('\n', rs);
                        }
                        std::fflush(rs);
                    }
                }
                if (f % 120 == 0 || f + 1 == frames) {
                    uint32_t V = uint32_t(m.mem.read(G + 0x7460 + 0x1ac, 4)); float p[3] = {}; if (V) m.mem.read_bytes(V + 0x150, p, 4);
                    uint32_t cam = uint32_t(m.mem.read(0xa7b1d8, 4)); float cp[3] = {}; if (cam) m.mem.read_bytes(cam + 0x10, cp, 12);
                    uint32_t board = A ? uint32_t(m.mem.read(A + 0x2cc, 4)) : 0; float bt = 0; if (board) { uint32_t v = uint32_t(m.mem.read(board + 0x4f4, 4)); std::memcpy(&bt, &v, 4); }
                    uint32_t game = uint32_t(m.mem.read(0xd728d8, 4)), eng = 0xd728d0; float edt; { uint32_t v = uint32_t(m.mem.read(eng + 0x18, 4)); std::memcpy(&edt, &v, 4); }
                    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / double(f + 1);
                    std::printf("%.2f ms/frame %.0f MIPS ", ms, double(m.steps) / (ms * double(f + 1) * 1000.0));
                    std::printf("frame %u race %d speed %.1f cam %.1f %.1f %.1f board %.2f game %d dt %.4f ui288 %u instructions %llu\n", f, race, p[0], cp[0], cp[1], cp[2], bt,
                                int(m.mem.read(game + 0x18, 4)), edt, uint32_t(m.mem.read(uint32_t(m.mem.read(game + 0x745c, 4)) + 0x288, 4)), (unsigned long long)m.steps); std::fflush(stdout);
                }
            }
            if (world.machine().histogram) {
                uint64_t tot = 0; for (int i = 0; i < reflex_vm::M_COUNT; i++) tot += histo[i];
                std::vector<std::pair<uint64_t, int>> v; for (int i = 0; i < reflex_vm::M_COUNT; i++) v.push_back({histo[i], i}); std::sort(v.rbegin(), v.rend());
                std::printf("total %llu\n", (unsigned long long)tot);
                for (int i = 0; i < 60; i++) std::printf("%-12s %5.2f%%  generic %5.2f%%\n", reflex_vm::mnemonic_name(v[i].second), 100.0 * double(v[i].first) / double(tot), 100.0 * double(histo[reflex_vm::M_COUNT + v[i].second]) / double(tot));
            }
            if (world.machine().jit_histo) {
                double tot = double(world.machine().steps);
                std::vector<std::pair<uint64_t, int>> v; for (int i = 0; i < 2 * reflex_vm::M_COUNT; i++) v.push_back({histo[i], i}); std::sort(v.rbegin(), v.rend());
                for (int i = 0; i < 70 && v[i].first; i++) std::printf("%-12s %-8s %6.2f%%\n", reflex_vm::mnemonic_name(v[i].second % reflex_vm::M_COUNT), v[i].second < reflex_vm::M_COUNT ? "helper" : "interp", 100.0 * double(v[i].first) / tot);
                reflex_vm::jit_print_forms(80);
            }
            { std::vector<std::pair<uint64_t, std::string>> v; for (auto& [k, e] : sites) v.push_back({e.first, k + " calls " + std::to_string(e.second)}); std::sort(v.rbegin(), v.rend());
              uint64_t tot = world.machine().steps; for (size_t i = 0; i < v.size() && i < 30; i++) std::printf("%5.1f%% %12llu %s\n", 100.0 * double(v[i].first) / double(tot), (unsigned long long)v[i].first, v[i].second.c_str()); }
            { std::vector<std::pair<uint64_t, std::string>> v; for (auto& [k, n] : callers) v.push_back({n, k}); std::sort(v.rbegin(), v.rend());
              for (size_t i = 0; i < v.size() && i < 25; i++) std::printf("%8llu %s\n", (unsigned long long)v[i].first, v[i].second.c_str()); }
            { auto& m = world.machine(); uint32_t S = world.sim(); uint64_t h = 1469598103934665603ull; for (uint32_t a = S; a < S + 0x3000; a++) { h ^= uint8_t(m.mem.read(a, 1)); h *= 1099511628211ull; }
              reflex_vm::TerrainInfo ti{}; world.terrain_info(ti);
              std::printf("sim %08x state hash %016llx tracks version %u deform version %u\n", S, (unsigned long long)h, world.tracks_version(), ti.deform_version); }
            if (std::getenv("REFLEX_BODIES")) {   // diagnostics: the local vehicle (atv.cpp) and its model nodes in the frame's axes
                auto& m = world.machine(); auto rd = [&](uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; };
                uint32_t G = rd(0xa946b8), P = G + 0x7460; reflex_vm::VehicleInfo v; reflex_vm::vehicle_info(m.mem, P, 0, v);
                std::printf("vehicle %08x type %d sim %08x bodies %d model nodes %d gear %d rpm %.0f speed %.2f\n", v.vehicle, v.type, v.sim, v.bodies, v.model_nodes, v.gear, v.rpm, v.speed);
                for (int i = 0; i < v.bodies && i < 6; i++) std::printf("body %d render %.3f %.3f %.3f q %.3f %.3f %.3f %.3f\n", i, v.render_position[i][0], v.render_position[i][1], v.render_position[i][2], v.render_orientation[i][0], v.render_orientation[i][1], v.render_orientation[i][2], v.render_orientation[i][3]);
                uint32_t model = rd(v.vehicle + 0x1328), hier = model + 0x10, records = rd(hier + 8), count = rd(hier + 0xc), wm = rd(hier + 0x1c), zero = 0;
                for (int i = int(count) - 1; i >= 0 && count <= 64; i--) m.call_function(0x7db890, &zero, 1, records + 16 * uint32_t(i));
                float f0[16]; if (count && count <= 64) m.mem.read_bytes(wm, f0, 64);
                for (uint32_t i = 0; i < count && i < 64; i++) { float f[16]; m.mem.read_bytes(wm + 64 * i, f, 64); float d[3] = {f[12] - f0[12], f[13] - f0[13], f[14] - f0[14]};
                    std::printf("node %2u %.3f %.3f %.3f\n", i, d[0] * f0[0] + d[1] * f0[1] + d[2] * f0[2], d[0] * f0[4] + d[1] * f0[5] + d[2] * f0[6], d[0] * f0[8] + d[1] * f0[9] + d[2] * f0[10]); }
            }
            if (std::getenv("REFLEX_PORT")) std::printf("%s", reflex_port::report().c_str());
            if (!coverage.empty()) {   // native/game progress: original instructions executed per exe address (tools/coverage.py aggregates)
                FILE* cf = std::fopen(std::getenv("REFLEX_COVERAGE"), "wb");
                uint32_t hdr[4] = {0x56435852u, frames - coverage_from, uint32_t(world.machine().outside_exe), uint32_t(world.machine().outside_exe >> 32)};
                std::fwrite(hdr, 4, 4, cf); std::fwrite(coverage.data(), 4, coverage.size(), cf); std::fclose(cf);
                // the CRT DLLs: "<file>.dll": u32 frames, counts for 0x78400000.., then "<file>.dll.tsv": the msvcr90 exports
                std::string dn = std::string(std::getenv("REFLEX_COVERAGE")) + ".dll";
                FILE* df = std::fopen(dn.c_str(), "wb"); uint32_t fr = frames - coverage_from; std::fwrite(&fr, 4, 1, df); std::fwrite(dll_coverage.data(), 4, dll_coverage.size(), df); std::fclose(df);
                FILE* ef = std::fopen((dn + ".tsv").c_str(), "w"); for (auto& [name, a] : world.machine().crt_exports) std::fprintf(ef, "%08x\t%s\n", a, name.c_str()); std::fclose(ef);
                std::printf("coverage written: %u frames\n", frames - coverage_from);
            }
            if (std::getenv("REFLEX_MEMHASH")) {   // every resident guest page (the whole machine memory), in address order
                auto& m = world.machine(); uint64_t h = 1469598103934665603ull; uint32_t pages = 0;
                // without the stacks (main 0x7fec0000..0x7ff00000, workers 0x7fbc0000..0x7fc00000): all dead between frames, and a
                // native port need not leave the original's dead locals there (native/game verify.sh). REFLEX_MEMHASH=stack keeps them.
                const bool stacks = std::string(std::getenv("REFLEX_MEMHASH")) == "stack";
                // REFLEX_MEMHASH=nojobs (core/ verify.sh): also without the JobPool ring 0x00d94b10..0x00d9cb10 (512 job records
                // copied from the submitter's stack, partly uninitialized locals: dead data that differs whenever a native function
                // leaves different dead stack; the slot functions at 0x00d9cb10.. and the ring indices stay in).
                const bool nojobs = std::string(std::getenv("REFLEX_MEMHASH")) == "nojobs";
                for (uint64_t pg = 0; pg < (1ull << 20); pg++) { const uint8_t* p = m.mem.peek(uint32_t(pg << 12)); if (!p) continue;
                    uint32_t a = uint32_t(pg << 12);
                    if (!stacks && ((a >= 0x7fec0000u && a < 0x7ff00000u) || (a >= 0x7fbc0000u && a < 0x7fc00000u))) continue;
                    pages++;
                    for (int i = 0; i < 4096; i += 8) {
                        if (nojobs && a + uint32_t(i) >= 0x00d94b10u && a + uint32_t(i) < 0x00d9cb10u) continue;
                        uint64_t v; std::memcpy(&v, p + i, 8); h = (h ^ v) * 1099511628211ull; } }
                auto& r = m.r; std::printf("memory hash %016llx over %u pages, regs %08x %08x\n", (unsigned long long)h, pages, r[4], r[5]);
            }
            if (const char* rs = std::getenv("REFLEX_READSET")) {   // the read map (vm.h Memory::read_map), 32 MB, 1 bit per 16 bytes
                auto& m = world.machine();
                if (m.mem.read_map) { FILE* f = std::fopen(rs, "wb"); std::fwrite(m.mem.read_map, 1, 1u << 25, f); std::fclose(f); std::printf("read set written\n"); }
            }
            if (const char* md = std::getenv("REFLEX_MEMDUMP")) {   // every resident page but the stacks (native/game tools/memdiff.py)
                auto& m = world.machine(); FILE* df = std::fopen(md, "wb");
                for (uint64_t pg = 0; pg < (1ull << 20); pg++) { const uint8_t* p = m.mem.peek(uint32_t(pg << 12)); if (!p) continue;
                    uint32_t a = uint32_t(pg << 12);
                    if ((a >= 0x7fec0000u && a < 0x7ff00000u) || (a >= 0x7fbc0000u && a < 0x7fc00000u)) continue;
                    std::fwrite(&a, 4, 1, df); std::fwrite(p, 1, 4096, df); }
                std::fclose(df);
            }
            if (std::getenv("REFLEX_AMBIENT_DUMP")) {   // ambient entities (ambient.cpp): per class counts and a hash of the export
                reflex_vm::AmbientScene sc; std::vector<reflex_vm::AmbientEntity> es; std::vector<float> ms; reflex_vm::AmbientInfo ai;
                auto t1 = std::chrono::steady_clock::now();
                sc.collect(world.machine(), es, ms, ai);
                auto t2 = std::chrono::steady_clock::now();
                sc.collect(world.machine(), es, ms, ai);
                auto t3 = std::chrono::steady_clock::now();
                std::map<std::string, std::array<int, 4>> per;   // count, drawn, skinned, completed
                for (auto& e : es) { auto& c = per[std::string(reflex_vm::ambient_kind_name(e.kind)) + " " + e.model]; c[0]++; c[1] += e.flags & 1; c[2] += (e.flags >> 1) & 1; c[3] += (e.flags >> 5) & 1; }
                if (std::getenv("REFLEX_AMBIENT_DUMP")[0] == 'v') for (auto& [k, c] : per) std::printf("ambient %-70s n %3d drawn %3d skinned %3d completed %d\n", k.c_str(), c[0], c[1], c[2], c[3]);
                if (const char* of = std::getenv("REFLEX_AMBIENT_OUT")) { FILE* f = std::fopen(of, "wb"); uint32_t ne = uint32_t(es.size()), nm = uint32_t(ms.size());
                    std::fwrite(&ne, 4, 1, f); std::fwrite(es.data(), sizeof es[0], ne, f); std::fwrite(&nm, 4, 1, f); std::fwrite(ms.data(), 4, nm, f); std::fclose(f); }
                uint64_t h = 1469598103934665603ull; for (float f : ms) { uint32_t v; std::memcpy(&v, &f, 4); h = (h ^ v) * 1099511628211ull; }
                for (auto& e : es) for (float f : e.matrix) { uint32_t v; std::memcpy(&v, &f, 4); h = (h ^ v) * 1099511628211ull; }
                std::printf("ambient: %d entities, %d node matrices, %d objects, export hash %016llx, scan+collect %.2f ms, collect %.3f ms\n", ai.entities, ai.matrices, ai.objects,
                            (unsigned long long)h, std::chrono::duration<double, std::milli>(t2 - t1).count(), std::chrono::duration<double, std::milli>(t3 - t2).count());
            }
        } catch (const std::exception& e) {
            std::printf("WORLD STOPPED: %s\n", e.what());
            auto& wm = world.machine(); std::printf("  stack (return addresses):");   // diagnostics: where the world stopped
            for (uint32_t a = wm.r[4], k = 0, n = 0; k < 8192 && n < 48 && wm.mem.mapped(a); a += 4, k++) { uint32_t v = uint32_t(wm.mem.read(a, 4)); if (v > 0x401000 && v < 0x916000) { std::printf(" %08x", v); n++; } }
            std::printf("\n"); return 1;
        }
        return 0;
    }
    c.jit = std::getenv("REFLEX_BOOT_JIT") != nullptr;
    { std::string sc = std::getenv("REFLEX_BOOT_SCHED") ? std::getenv("REFLEX_BOOT_SCHED") : "";   // native/game notes/loading.md
      c.baton = sc == "baton" || sc == "baton-sliced"; c.baton_slices = sc == "baton-sliced"; }
    reflex_vm::Boot boot(c);
    // native/game ports in the boot (REFLEX_PORT=<filter>; On mode: with the default exact-slice scheduler a native that is sliced or
    // blocks inside a nested call breaks, so use REFLEX_BOOT_SCHED=baton)
    if (int n = reflex_port::install_from_env(boot.machine())) std::printf("boot: %d ports installed%s\n", n, c.baton ? "" : " (warning: exact-slice scheduling)");
    static std::vector<uint32_t> boot_coverage;   // native/game: what the boot (start + event) runs, for the loading waves
    static std::vector<uint32_t> boot_dll_coverage;   // the CRT DLLs (0x78400000..), "<file>.dll" + "<file>.dll.tsv" as the world test
    static const std::unordered_map<std::string, uint32_t>* boot_crt_exports = nullptr;
    if (std::getenv("REFLEX_COVERAGE")) {
        boot_coverage.assign(0x600000, 0); boot.machine().pc_counts = boot_coverage.data();
        boot_dll_coverage.assign(0x200000, 0); boot.machine().dll_counts = boot_dll_coverage.data(); boot_crt_exports = &boot.machine().crt_exports;
    }
    struct CoverageOut { ~CoverageOut() {
        if (boot_coverage.empty()) return;
        FILE* cf = std::fopen(std::getenv("REFLEX_COVERAGE"), "wb"); uint32_t hdr[4] = {0x56435852u, 1, 0, 0};
        std::fwrite(hdr, 4, 4, cf); std::fwrite(boot_coverage.data(), 4, boot_coverage.size(), cf); std::fclose(cf);
        std::string dn = std::string(std::getenv("REFLEX_COVERAGE")) + ".dll"; uint32_t one = 1;
        FILE* df = std::fopen(dn.c_str(), "wb"); std::fwrite(&one, 4, 1, df); std::fwrite(boot_dll_coverage.data(), 4, boot_dll_coverage.size(), df); std::fclose(df);
        FILE* ef = std::fopen((dn + ".tsv").c_str(), "w"); for (auto& [name, a] : *boot_crt_exports) std::fprintf(ef, "%08x\t%s\n", a, name.c_str()); std::fclose(ef);
        std::printf("boot coverage written\n"); } } coverage_out;
    auto t0 = std::chrono::steady_clock::now();
    auto secs = [&]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); };
    if (const char* w = std::getenv("REFLEX_WATCH")) { boot.machine().watch_address = uint32_t(std::strtoul(w, nullptr, 16)); boot.machine().trace_out = std::fopen(std::getenv("REFLEX_WATCH_OUT") ? std::getenv("REFLEX_WATCH_OUT") : "/private/tmp/claude-501/-Users-payton-Documents-Development-REFLEX/793ecc78-de06-47ae-a637-7fb110df0591/scratchpad/boot/watch.txt", "w"); boot.machine().trace_from = boot.machine().trace_to = 0; reflex_vm::g_block_watch = boot.machine().watch_address; }
    static std::map<std::string, uint64_t> boot_callers;   // REFLEX_BOOT_CALLERS: the stack's return addresses at these entries
    if (std::getenv("REFLEX_GRID_HOOK")) {   // gridfix: the scenes / tickers at the grid pass
        boot.machine().natives[0x0075f890] = [](reflex_vm::Machine& m) -> uint32_t {
            uint32_t ctx = m.mem.mapped(0xa95844) ? uint32_t(m.mem.read(0xa95844, 4)) : 0;
            std::printf("GRIDHOOK world %08x ui %08x", m.mem.mapped(0xa946b8) ? uint32_t(m.mem.read(m.mem.read(0xa946b8, 4) + 0x5a54, 4)) : 0,
                        m.mem.mapped(0xa946b8) ? uint32_t(m.mem.read(m.mem.read(0xa946b8, 4) + 0x20c8, 4)) : 0);
            { uint32_t G = m.mem.mapped(0xa946b8) ? uint32_t(m.mem.read(0xa946b8, 4)) : 0;
              uint32_t w = G ? uint32_t(m.mem.read(G + 0x5a54, 4)) : 0, u = G ? uint32_t(m.mem.read(G + 0x20c8, 4)) : 0;
              std::printf(" evscn %08x uiscn %08x", w ? uint32_t(m.mem.read(w + 0x20138, 4)) : 0, u ? uint32_t(m.mem.read(u + 0x20138, 4)) : 0); }
            for (uint32_t o = 0x1c; o < 0x28; o += 4) {
                uint32_t s = ctx && m.mem.mapped(ctx + o) ? uint32_t(m.mem.read(ctx + o, 4)) : 0;
                if (!s) { std::printf(" [-0]"); continue; }
                uint32_t act = m.mem.mapped(s + 0x34) ? uint32_t(m.mem.read(s + 0x34, 4)) : 0;
                int32_t n = m.mem.mapped(s + 0x2c20) ? int32_t(m.mem.read(s + 0x2c20, 4) - m.mem.read(s + 0x2c1c, 4)) / 4 : 0;
                std::printf(" [%08x act %u tickers %d]", s, act, n);
                if (o == 0x1c) for (int32_t i = 0; i < n; i++) {
                    uint32_t data = m.mem.read(s + 0x2c10 + 0xc, 4), t = m.mem.read(data + 4 * i, 4);
                    uint32_t vt = m.mem.mapped(t) ? uint32_t(m.mem.read(t, 4)) : 0;
                    uint32_t f19 = vt ? uint32_t(m.mem.read(vt + 0x4c, 4)) : 0;
                    std::printf("\nGRIDTICK %d %08x vt %08x vf19 %08x", int(i), t, vt, f19);
                }
            }
            std::printf("\n");
            return reflex_vm::RUN_ORIGINAL; };
    }
    if (const char* bc = std::getenv("REFLEX_BOOT_CALLERS")) {
        char buf[256]; std::snprintf(buf, sizeof buf, "%s", bc);
        for (char* tok = std::strtok(buf, ","); tok; tok = std::strtok(nullptr, ",")) {
            uint32_t at = uint32_t(std::strtoul(tok, nullptr, 16));
            boot.machine().natives[at] = [](reflex_vm::Machine& m) -> uint32_t {
                uint32_t gc = m.mem.mapped(0x9cb864) ? uint32_t(m.mem.read(0x9cb864, 4)) : 0;
                if (gc < 0x700 || gc > 0x900) return reflex_vm::RUN_ORIGINAL;   // gridfix: only the load block before the grid pass
                char b[192]; uint32_t n = 0;
                std::snprintf(b, sizeof b, "esp %08x ret", m.r[4]);
                for (uint32_t a = m.r[4], i = 0; i < 24 && n < 6; a += 4, i++) { uint32_t v = m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0; if (v > 0x401000 && v < 0x916000) { char t[16]; std::snprintf(t, sizeof t, " %08x", v); std::strncat(b, t, sizeof b - std::strlen(b) - 1); n++; } }
                boot_callers[b]++; return reflex_vm::RUN_ORIGINAL; };
        }
    }
    try {
        boot.start();
        if (const char* dm = std::getenv("REFLEX_BOOT_DEFORM"))   // w-gridpair: the deformation pass in the boot (Boot::enable_deformation)
            std::printf("boot deformation: %s\n", boot.enable_deformation(dm) ? "model loaded" : "model NOT loaded");
        if (std::getenv("REFLEX_PORT") && std::getenv("REFLEX_BOOT_PORT_REPORT")) std::printf("%s", reflex_port::report().c_str());
        std::printf("started: %.2fs, %llu instructions\n", secs(), (unsigned long long)boot.machine().steps);
        if (std::getenv("REFLEX_EVENT")) {
            std::string log; reflex_vm::Boot::EventSpec e;
            if (const char* sp = std::getenv("REFLEX_EVENT_SPEC")) {   // type,typeid,location,locid,track,class,durtype,dur,activity
                // split at every comma (fields may be empty: FreeRide has no duration type, "FreeRide,9,FR_Alpine,45,0,-1,,0,8"); a
                // spec without exactly 9 fields is refused (sscanf's %[^,] stopped at an empty field and left the default event)
                std::vector<std::string> f; std::string cur;
                for (const char* c = sp; ; c++) { if (*c == ',' || !*c) { f.push_back(cur); cur.clear(); if (!*c) break; } else cur += *c; }
                auto num = [](const std::string& x, int& out) { char* end = nullptr; long v = std::strtol(x.c_str(), &end, 10); if (x.empty() || *end) return false; out = int(v); return true; };
                reflex_vm::Boot::EventSpec x = e;
                bool ok = f.size() == 9 && !f[0].empty() && !f[2].empty() && num(f[1], x.type_id) && num(f[3], x.location_id) && num(f[4], x.track_id) &&
                          num(f[5], x.vehicle_class) && (f[7].empty() ? (x.duration = 0, true) : num(f[7], x.duration)) && num(f[8], x.activity);
                if (!ok) { std::printf("REFLEX_EVENT_SPEC: expected type,typeid,location,locid,track,class,durtype,dur,activity: %s\n", sp); return 2; }
                x.type = f[0]; x.location = f[2]; x.duration_type = f[6]; e = x;
            }
            // REFLEX_EVENT_MODES=activation,series,device,cursor: the game-mode fields the plugin sets for a Free Ride challenge (modes.h;
            // ReflexModes.Challenges): EventTypeActivation ("HillClimb", "Trailblazer", "TargetPractice"; "-" none), UI SeriesId, the
            // exclusive device, the cursor off screen (1). Fields may be empty (left as they are).
            if (const char* ms = std::getenv("REFLEX_EVENT_MODES")) {
                std::vector<std::string> f; std::string cur;
                for (const char* c = ms; ; c++) { if (*c == ',' || !*c) { f.push_back(cur); cur.clear(); if (!*c) break; } else cur += *c; }
                if (f.size() > 0 && !f[0].empty() && f[0] != "-") e.activation = f[0];
                if (f.size() > 1 && !f[1].empty()) e.series_challenge = std::atoi(f[1].c_str());
                if (f.size() > 2 && !f[2].empty()) e.exclusive_device = std::atoi(f[2].c_str());
                if (f.size() > 3 && !f[3].empty()) e.cursor_off_screen = std::atoi(f[3].c_str()) != 0;
            }
            if (const char* g = std::getenv("REFLEX_GARAGE"))   // Vehicle Select: rider_owner,rider_id,gear_set,vehicle_owner,vehicle_id,vehicle_type (-1 unset)
                std::sscanf(g, "%d,%d,%d,%d,%d,%d", &e.rider_owner, &e.rider_id, &e.rider_gear_set, &e.vehicle_owner, &e.vehicle_id, &e.vehicle_type);
            // REFLEX_EVENT_SERIES=id[,skip_practice[,outcomes_file,count]]: the event is the Tour series `id` of the Series table, started
            // as the front end starts it (career.cpp start_series_event: sign-in, LoadSeries, the practice popup's SetSkipPractice,
            // CommitAndInitializeSeries, StartEvent, the gate selection) and, with an outcomes file (career::race_outcome records, the
            // plugin's career_outcomes.bin), the series' races already run replayed (their results, the next sub event, the activity
            // area's reload, the next gate selection). The spec's other fields are the series' first event's (its location).
            if (const char* se = std::getenv("REFLEX_EVENT_SERIES")) {
                std::vector<std::string> f; std::string cur;
                for (const char* c = se; ; c++) { if (*c == ',' || !*c) { f.push_back(cur); cur.clear(); if (!*c) break; } else cur += *c; }
                e.series_id = std::atoi(f[0].c_str());
                if (f.size() > 1 && !f[1].empty()) e.series_skip_practice = std::atoi(f[1].c_str());
                if (f.size() > 3 && !reflex_vm::career::read_outcomes(f[2], std::atoi(f[3].c_str()), e.series_outcomes)) {
                    std::printf("REFLEX_EVENT_SERIES: %s does not hold %s race outcomes\n", f[2].c_str(), f[3].c_str()); return 2;
                }
            }
            if (const char* rr = std::getenv("REFLEX_RIDERS")) { e.riders = std::atoi(rr); e.ai = std::getenv("REFLEX_AI") ? std::atoi(std::getenv("REFLEX_AI")) : 1; }
            if (std::string(std::getenv("REFLEX_EVENT")) == "alpine") { e.type = "FreeRide"; e.type_id = 9; e.location = "FR_Alpine"; e.location_id = 45; e.vehicle_class = -1; e.activity = 8; e.duration_type = ""; }
            if (const char* ss = std::getenv("REFLEX_SEED_SETUP")) {   // framecmp: the event setup's rand streams (builds/framecmp)
                uint32_t seed = uint32_t(std::strtoul(ss, nullptr, 16));
                uint32_t ptd = uint32_t(boot.machine().call_function(0x7854383f, nullptr, 0));
                if (ptd && boot.machine().mem.mapped(ptd + 0x14)) boot.machine().mem.write(ptd + 0x14, 4, seed);
                boot.machine().mem.write(0x009cb860, 4, seed);   // the game's own rand (the grid's start zones)
                boot.machine().mem.write(0x009cb864, 4, 0);
                std::printf("seed setup %08x\n", seed);
            }
            // REFLEX_TEST_DRIVE=1 (w-testride): GarageHUB:OnStartPressed and the Test Drive Loading Page through the game's own code
            // (garage.cpp start_test_drive: EnableTestDrive, Game_SetupActivityArea, MXGarage_TestDriveStart, the setup until the
            // local slot spawned); REFLEX_TEST_DRIVE_END=<frame>: the pause request's end at that frame of the frames below
            // (EnableTestDrive( FALSE ), then MXGarage_TestDriveEnd, as the Test Drive Exit Page)
            bool ok = std::getenv("REFLEX_TEST_DRIVE") ? reflex_vm::garage::start_test_drive(boot, e, 6000, &log) : boot.start_event(e, frames, &log);
            std::printf("%s%s after %.2fs\n", log.c_str(), ok ? "EVENT STARTED" : "EVENT FAILED", secs());
            if (std::getenv("REFLEX_PORT")) std::printf("%s", reflex_port::report().c_str());   // the ports the boot called (native/game)
            frames = std::getenv("REFLEX_AFTER") ? uint32_t(std::atoi(std::getenv("REFLEX_AFTER"))) : 0;
            if (ok && std::getenv("REFLEX_WORLD_OUT")) {
                std::string o = std::getenv("REFLEX_WORLD_OUT"), info;
                boot.write_world(o + "-world.bin", o + "-deform.bin", o + "-deform-model.bin", &info);
                if (const char* cn = std::getenv("REFLEX_WORLD_CANON"))   // native/game tools/world_compare.py (notes/loading.md)
                    boot.write_canonical(*cn ? cn : (o + "-canon.bin").c_str(), std::getenv("REFLEX_WORLD_CANON_DEVICE") != nullptr, &info);
                std::printf("%swritten after %.2fs\n", info.c_str(), secs());
            }
        }
        // REFLEX_EVENT_UI=<frame> (w-resultsrng): the event's in-game UI routed to its HUD page as the hub does when the player joins
        // (modes.cpp ui_activate: "Goto<Activity>SinglePlayer"), so the pages that follow the race (PostEventOverlay "Show Activity
        // Results" -> GotoPostEvent -> PostEventResults:Start -> App.PopulateResultsTable) run; the boot's start_event leaves the UI where
        // the front end was
        const char* event_ui = std::getenv("REFLEX_EVENT") ? std::getenv("REFLEX_EVENT_UI") : nullptr;
        // REFLEX_TUT_SCRIPT=<file> (w-tutorials): the lesson driver's lines per frame (tutorial.h script_frame)
        const char* tut_script = std::getenv("REFLEX_EVENT") ? std::getenv("REFLEX_TUT_SCRIPT") : nullptr;
        if (tut_script) reflex_vm::tutorial::script_load(tut_script);
        for (uint32_t i = 0; i < frames; i++) {
            if (tut_script) reflex_vm::tutorial::script_frame(boot, i);
            if (event_ui && i == uint32_t(std::atoi(event_ui))) std::printf("event UI route: %s\n", reflex_vm::ui_activate(boot.machine()).c_str());
            if (const char* te = std::getenv("REFLEX_TEST_DRIVE_END"); te && i == uint32_t(std::atoi(te))) {
                std::string tl; reflex_vm::garage::end_test_drive(boot, &tl); std::printf("%s", tl.c_str());
            }
            // REFLEX_EVENT_ACCEPT=1 (w-wpfill): the rules popup accepted as a player does: while the local player's activity holds the
            // start for its rules (+0xff068 != 0, the popup up: Freestyle, Section Race, the career formats) Enter (DIK 0x1c) is held for 3
            // frames, then released for 30 (the popup's own key handling, so the UI leaves the popup page as in the game)
            if (std::getenv("REFLEX_EVENT_ACCEPT")) {
                static uint32_t held = 0, wait = 0; uint8_t keys[256] = {};
                const uint32_t G = boot.read32(0xa946b8), A = G ? boot.read32(G + 0x7460 + 0x94) : 0;
                if (held) { held--; keys[0x1c] = 0x80; if (!held) wait = 30; }
                else if (wait) wait--;
                else if (A && boot.read32(A + 0xff068) != 0 && boot.read32(A + 0x21c) == 0) { held = 3; keys[0x1c] = 0x80; std::printf("event accept: Enter at %u\n", i); }
                boot.set_keys(keys);
            }
            // REFLEX_EVENT_POKE (w-partyresults): the world test's REFLEX_POKE stimulus ("frame:addr:value,...", 32-bit writes at the start
            // of that frame) in an event boot's frames (frame = the index after the event start)
            if (const char* pk = std::getenv("REFLEX_EVENT_POKE")) {
                if (i == 0) { const uint32_t G = boot.read32(0xa946b8), A = G ? boot.read32(G + 0x7460 + 0x94) : 0;
                              std::printf("event activity %08x type %u\n", A, A ? boot.read32(A + 0x23c) : 0u); }
                for (const char* p = pk; *p;) {
                    char* e = nullptr; uint32_t pf = uint32_t(std::strtoul(p, &e, 10)); if (e == p || *e != ':') break;
                    uint32_t pa = uint32_t(std::strtoul(e + 1, &e, 16)); if (*e != ':') break;
                    uint32_t pv = uint32_t(std::strtoul(e + 1, &e, 16));
                    if (pf == i) boot.machine().mem.write(pa, 4, pv);
                    p = *e ? e + 1 : e;
                }
            }
            boot.run_frames(1);
            if (i % 50 == 0 || i + 1 == frames) {
                auto& m = boot.machine(); uint32_t game = uint32_t(m.mem.read(0xd728d8, 4));
                uint32_t G = boot.read32(0xa946b8), P = G ? G + 0x7460 : 0, A = P ? boot.read32(P + 0x94) : 0, V = P ? boot.read32(P + 0x1ac) : 0;
                { int veh = 0; for (uint32_t k = 0; k < 16; k++) if (G && boot.read32(G + 0x7460 + k * 0x12a0 + 0x1ac)) veh++; std::printf("vehicles %d  ", veh); }
                uint32_t cam = boot.read32(0xa7b1d8); float cp[3] = {}; if (cam) m.mem.read_bytes(cam + 0x10, cp, 12);
                uint32_t board = A ? boot.read32(A + 0x2cc) : 0; float bt = 0; if (board) { uint32_t v = boot.read32(board + 0x4f4); std::memcpy(&bt, &v, 4); }
                std::printf("frame %llu: %.2fs, %llu instructions, %.1f s virtual, game state %d, race state %d, cam %.1f %.1f %.1f board %.2f\n", (unsigned long long)boot.frames(), secs(), (unsigned long long)m.steps, boot.now() / 1e7,
                            game ? int(m.mem.read(game + 0x18, 4)) : -1, A ? int(boot.read32(A + 0x21c)) : -1, cp[0], cp[1], cp[2], bt);
                std::fflush(stdout);
            }
        }
        // the ports the frames after the event start called (REFLEX_EVENT_UI / race-end runs: w-resultsrng), as after start_event
        if (std::getenv("REFLEX_PORT") && std::getenv("REFLEX_EVENT") && frames) std::printf("after the frames:\n%s", reflex_port::report().c_str());
    } catch (const std::exception& e) {
        std::printf("STOPPED after %.2fs, %llu instructions, frame %llu: %s\n%s", secs(), (unsigned long long)boot.machine().steps, (unsigned long long)boot.frames(), e.what(), boot.status().c_str());
        return 1;
    }
    { auto& m = boot.machine(); uint32_t game = uint32_t(m.mem.read(0xd728d8, 4)); if (game) std::printf("game %08x: event 161f4 = %x, 161f8 = %x\n", game, uint32_t(m.mem.read(game + 0x161f4, 4)), uint32_t(m.mem.read(game + 0x161f8, 4))); }
    if (std::getenv("REFLEX_MEMHASH") && std::getenv("REFLEX_EVENT")) {   // the boot's whole memory after its frames (a career / event boot's
        // On check: REFLEX_PORT natives against none; the same exclusions as the world test's hash: the stacks, nojobs the JobPool ring)
        auto& m = boot.machine(); uint64_t h = 1469598103934665603ull; uint32_t pages = 0;
        const bool nojobs = std::string(std::getenv("REFLEX_MEMHASH")) == "nojobs";
        for (uint64_t pg = 0; pg < (1ull << 20); pg++) {
            const uint8_t* p = m.mem.peek(uint32_t(pg << 12)); if (!p) continue;
            const uint32_t a = uint32_t(pg << 12);
            if ((a >= 0x7fec0000u && a < 0x7ff00000u) || (a >= 0x7fbc0000u && a < 0x7fc00000u)) continue;
            pages++;
            for (int i = 0; i < 4096; i += 8) {
                if (nojobs && a + uint32_t(i) >= 0x00d94b10u && a + uint32_t(i) < 0x00d9cb10u) continue;
                uint64_t v; std::memcpy(&v, p + i, 8); h = (h ^ v) * 1099511628211ull;
            }
        }
        std::printf("memory hash %016llx over %u pages\n", (unsigned long long)h, pages);
        if (const char* md = std::getenv("REFLEX_MEMDUMP")) {   // every resident page but the stacks (native/game tools/memdiff.py)
            FILE* df = std::fopen(md, "wb");
            for (uint64_t pg = 0; pg < (1ull << 20); pg++) {
                const uint8_t* p = m.mem.peek(uint32_t(pg << 12)); if (!p) continue;
                const uint32_t a = uint32_t(pg << 12);
                if ((a >= 0x7fec0000u && a < 0x7ff00000u) || (a >= 0x7fbc0000u && a < 0x7fc00000u)) continue;
                std::fwrite(&a, 4, 1, df); std::fwrite(p, 1, 4096, df);
            }
            std::fclose(df);
        }
    }
    if (!boot_callers.empty()) { std::vector<std::pair<uint64_t, std::string>> v; for (auto& [k, n] : boot_callers) v.push_back({n, k}); std::sort(v.rbegin(), v.rend()); for (auto& [n, k] : v) std::printf("CALLERS %llu %s\n", (unsigned long long)n, k.c_str()); }
    std::printf("%s", boot.status().c_str());
    return 0;
}
