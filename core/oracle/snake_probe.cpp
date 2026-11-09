// snake_probe.cpp -- the oracle's probe for the Snake party mode's cuts (core/game/race_snake_driver.c, race_snake.c). In the reference
// Snake world the computer riders are cut off by their energy only (none rides into a trail in the frames run), so a frame hook
// (Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL functions on the live activity once its first round has
// run RX_SNAKE_PROBE_FRAMES InProgress frames (default 240): with the ports installed Verify runs the core function and then the original
// on the same state and compares memory as for a call from the game. Every case runs on a snapshot of the whole guest memory and machine
// state, restored afterwards.
//   REFLEX_PORT=rx_party,rx_SnakeProbeHook REFLEX_PORT_VERIFY=1 RX_SNAKE_PROBE=1
// Cases: SnakeDriver::vf7 with the driver's vehicle on another driver's trail (each pair: the hit, the kill, "SnakeTrailHit", 005149b0),
// on its own old trail, with the kill clock running, wrecked (+0x280) away from every trail, with no energy, off the boundary (+0xf20),
// with a new trail frame id (0069e360), as a spectator (+0x1e4); RuleOutOfSnakeBounds vf5 leaving and coming back (SnakeDriver::vf9);
// vf9 with another rule; SnakeDriver vf16 (kind 0) / vf17; the activity's vf39 / vf27 with spectators (+0xfe8), vf42 and vf44 in team
// mode (+0xff0b4), vf42 with one rider left, vf6; the visible meshes' destructors (w-partyresults, at the end).
#include "oracle.h"
extern "C" {
#include "../game/race_snake_trail.h"
}
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
void wf(uint32_t a, float f) { wr(a, fb(f)); }

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
        if (!mapped(p, 0x12a0) || rd(p + 4) == 0) continue;
        const uint32_t a = rd(p + 0x94);
        if (a && mapped(a, 4) && rd(a) == 0x00944884) return a;
    }
    return 0;
}
int g_cases = 0;
template <class F> void run_case(reflex_vm::Machine& m, Pages& pages, const char* what, F&& body) {
    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[snake] ERROR missing reversible host state\n"); return; }
    body();
    g_cases++;
    if (std::getenv("RX_SNAKE_PROBE_LOG")) std::fprintf(stderr, "[snake] case %d %s\n", g_cases, what);
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
/* a point of a driver's trail: the n-th newest segment's position (the trail accessor of race_snake_trail.c, read only) */
bool trail_point(uint32_t driver, uint32_t back, float* p) {
    static float seg[4096 * RX_SNAKE_TRAIL_SEG_FLOATS];
    const uint32_t n = rx_snake_trail_segments(rd(driver + 0xf14), seg, 4096);
    if (n <= back || n > 4096) return false;
    const float* s = seg + RX_SNAKE_TRAIL_SEG_FLOATS * (n - 1 - back);   /* the walk's last record is the newest */
    p[0] = s[0]; p[1] = s[1] + 0.5f * s[10]; p[2] = s[2];
    return true;
}
void put_vehicle(uint32_t driver, const float* p) {
    const uint32_t v = rd(rd(driver + 0x74) + 0x1ac);
    wf(v + 0xc0, p[0]); wf(v + 0xc4, p[1]); wf(v + 0xc8, p[2]);
}
}  // namespace

RX_PORT(0x0084ed20, SnakeProbeHook, "thunk", 0) {
    if (!std::getenv("RX_SNAKE_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static bool done = false;
    static int inprogress = 0;
    if (done) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    const uint32_t act = find_activity();
    static int calls = 0;
    if (std::getenv("RX_SNAKE_PROBE_LOG") && calls++ % 100 == 0) std::fprintf(stderr, "[snake] frame hook: activity %08x state %u\n", act, act ? rd(act + 0x21c) : 0);
    if (!act || rd(act + 0x21c) != 2) return reflex_vm::RUN_ORIGINAL;
    const int want = std::getenv("RX_SNAKE_PROBE_FRAMES") ? std::atoi(std::getenv("RX_SNAKE_PROBE_FRAMES")) : 240;
    if (++inprogress < want) return reflex_vm::RUN_ORIGINAL;
    done = true;
    const uint32_t n = rd(act + 0x26c);
    std::fprintf(stderr, "[snake] activity %08x drivers %u round %u\n", act, n, rd(act + 0xff09c));
    Pages pages(m.mem);
    const uint32_t dt = fb(1.0f / 60.0f);
    auto drv = [&](uint32_t i) { return rd(rd(act + 0x268) + 4 * i); };
    /* the cuts by a trail: each rider on every other rider's trail and on its own (the oldest segment; its own needs 6 or more,
       the newest are its head) */
    for (uint32_t a = 0; a < n; a++)
        for (uint32_t b = 0; b < n; b++) {
            const uint32_t da = drv(a), db = drv(b);
            float p[3];
            const uint32_t segs = rx_snake_trail_segments(rd(db + 0xf14), nullptr, 0);
            if (segs < (a == b ? 6u : 2u)) { std::fprintf(stderr, "[snake] driver %u: %u trail segments, no case for driver %u\n", b, segs, a); continue; }
            const uint32_t back = segs - 1;
            if (!trail_point(db, back, p)) { std::fprintf(stderr, "[snake] driver %u: no trail point %u back (%u segments, trail %08x)\n", b, back, rx_snake_trail_segments(rd(db + 0xf14), nullptr, 0), rd(db + 0xf14)); continue; }
            run_case(m, pages, "trail hit", [&] { wr(da + 0xf1c, 0); put_vehicle(da, p); call_this(0x00513f30, da, 1, &dt); });
            run_case(m, pages, "trail hit, kill clock running", [&] { wr(da + 0xf1c, 0); wf(da + 0xf50, 0.25f); put_vehicle(da, p); call_this(0x00513f30, da, 1, &dt); });
        }
    const uint32_t d0 = drv(0);
    run_case(m, pages, "wrecked, no trail", [&] { wr(d0 + 0xf1c, 0); const uint32_t v = rd(rd(d0 + 0x74) + 0x1ac); wf(v + 0xc4, rf(v + 0xc4) + 300.0f); wr(v + 0x280, 1); call_this(0x00513f30, d0, 1, &dt); });
    run_case(m, pages, "no energy", [&] { wr(d0 + 0xf1c, 0); wf(d0 + 0xf34, 0.0f); call_this(0x00513f30, d0, 1, &dt); });
    run_case(m, pages, "off the boundary", [&] { wr(d0 + 0xf1c, 0); wr(d0 + 0xf20, 1); call_this(0x00513f30, d0, 1, &dt); });
    run_case(m, pages, "a new trail frame id", [&] { wr(rd(d0 + 0xf14) + 0x100, rd(0x009cd974) ^ 1); call_this(0x00513f30, d0, 1, &dt); });
    run_case(m, pages, "a spectator", [&] { wr(rd(d0 + 0x74) + 0x1e4, 1); call_this(0x00513f30, d0, 1, &dt); });
    /* the boundary rule (kind 9) of the first driver: out, then back in */
    uint32_t rule = 0;
    for (uint32_t k = 0; k < rd(d0 + 0xee4) && k < 18; k++) if (rd(rd(d0 + 0xa0 + 4 * k)) == 0x0093fe00) rule = rd(d0 + 0xa0 + 4 * k);
    if (rule) {
        run_case(m, pages, "leaves the boundary", [&] { const uint32_t v = rd(rd(d0 + 0x74) + 0x1ac); wf(v + 0xc0, rf(v + 0xc0) + 5000.0f); call_this(0x0051bb70, rule, 1, &dt); });
        run_case(m, pages, "comes back", [&] { wr(rule + 0x50, 0); call_this(0x0051bb70, rule, 1, &dt); });
        run_case(m, pages, "comes back cut off", [&] { wr(rule + 0x50, 0); wr(d0 + 0xf1c, 1); call_this(0x0051bb70, rule, 1, &dt); });
        run_case(m, pages, "vf9 with the rule, outside", [&] { wr(rule + 0x50, 0); wr(d0 + 0xf1c, 0); call_this(0x005148f0, d0, 1, &rule); });
        run_case(m, pages, "vf9 with the rule, inside", [&] { call_this(0x005148f0, d0, 1, &rule); });
    } else std::fprintf(stderr, "[snake] no RuleOutOfSnakeBounds on the first driver\n");
    {   const uint32_t other = rd(d0 + 0xa0);   /* the first rule (another kind) */
        if (other && other != rule) run_case(m, pages, "vf9 with another rule", [&] { call_this(0x005148f0, d0, 1, &other); }); }
    run_case(m, pages, "driver vf16 kind 0", [&] { const uint32_t a[2] = {0, 0}; call_this(0x00514c70, d0, 2, a); });
    run_case(m, pages, "driver vf17", [&] { call_this(0x00514cc0, d0, 0, nullptr); });
    /* the activity */
    run_case(m, pages, "vf39 with spectators", [&] { wr(act + 0xfe8, 1); const uint32_t a[1] = {1}; call_this(0x0056c550, act, 1, a); });
    run_case(m, pages, "vf27 with spectators, the timer out", [&] { wr(act + 0xfe8, 1); wf(act + 0xff0ac, 0.0f); call_this(0x0056c700, act, 1, &dt); });
    run_case(m, pages, "vf27, the match over", [&] { wr(act + 0xff0a4, 1); call_this(0x0056c700, act, 1, &dt); });
    run_case(m, pages, "vf42 one rider left", [&] { for (uint32_t i = 1; i < n; i++) wr(drv(i) + 0xf1c, 1); wr(d0 + 0xf1c, 0); call_this(0x0056cae0, act, 0, nullptr); });
    run_case(m, pages, "vf42 one rider left, its last round", [&] { for (uint32_t i = 1; i < n; i++) wr(drv(i) + 0xf1c, 1); wr(d0 + 0xf1c, 0); wr(d0 + 0xf24, rd(act + 0xff0a0) - 1); call_this(0x0056cae0, act, 0, nullptr); });
    run_case(m, pages, "vf42 nobody left", [&] { for (uint32_t i = 0; i < n; i++) wr(drv(i) + 0xf1c, 1); call_this(0x0056cae0, act, 0, nullptr); });
    for (int t = 0; t < 3; t++)
        run_case(m, pages, "vf42 teams", [&] {
            wr(act + 0xff0b4, 1);
            for (uint32_t i = 0; i < n; i++) { wr(drv(i) + 0xf28, i & 1); wr(drv(i) + 0xf1c, (i & 1) == uint32_t(t & 1) ? 1 : 0); }
            if (t == 2) { wr(act + 0xff0b8, rd(act + 0xff0a0)); }
            call_this(0x0056cae0, act, 0, nullptr);
        });
    run_case(m, pages, "vf44 teams", [&] { wr(act + 0xff0b4, 1); const uint32_t a[1] = {rd(d0 + 0x74)}; call_this(0x0056bda0, act, 1, a); });
    run_case(m, pages, "vf44", [&] { const uint32_t a[1] = {rd(d0 + 0x74)}; call_this(0x0056bda0, act, 1, a); });
    /* w-partyresults: the visible meshes' scalar deleting destructors, which no teardown calls (a trail's mesh lives inside the trail
       object, +0x10, freed by 0069e460 through its body 006a0db0; the wall inside the activity, +0xff100, by the activity's destructors
       through its body 0057f020): XenonSnakeVisibleMesh 006a0c50 / its IRenderable adjustor 006a2770 and XenonSnakeWallVisibleMesh
       0057ef90 / 00592b70, in place with flags 0, and with flags 1 on a heap copy of the object (ReflexAlignedAlloc 0078fbe0) */
    {
        auto heap_copy = [&](uint32_t obj, uint32_t size) {
            const uint32_t c = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, &size);
            for (uint32_t k = 0; c && k < size; k += 4) wr(c + k, rd(obj + k));
            return c;
        };
        const uint32_t z = 0, one = 1;
        const uint32_t mesh = rd(d0 + 0xf14) ? rd(d0 + 0xf14) + 0x10 : 0, wall = act + 0xff100;
        if (mesh) {
            run_case(m, pages, "trail mesh sdtor 0", [&] { call_this(0x006a0c50, mesh, 1, &z); });
            run_case(m, pages, "trail mesh IRenderable sdtor 0", [&] { call_this(0x006a2770, mesh + 0x90, 1, &z); });
            run_case(m, pages, "trail mesh sdtor 1 (heap copy)", [&] { const uint32_t c = heap_copy(mesh, 0xf0); if (c) call_this(0x006a0c50, c, 1, &one); });
            run_case(m, pages, "trail mesh IRenderable sdtor 1 (heap copy)", [&] { const uint32_t c = heap_copy(mesh, 0xf0); if (c) call_this(0x006a2770, c + 0x90, 1, &one); });
        } else std::fprintf(stderr, "[snake] the first driver has no trail\n");
        run_case(m, pages, "wall sdtor 0", [&] { call_this(0x0057ef90, wall, 1, &z); });
        run_case(m, pages, "wall IRenderable sdtor 0", [&] { call_this(0x00592b70, wall + 0x90, 1, &z); });
        run_case(m, pages, "wall sdtor 1 (heap copy)", [&] { const uint32_t c = heap_copy(wall, 0x100); if (c) call_this(0x0057ef90, c, 1, &one); });
        run_case(m, pages, "wall IRenderable sdtor 1 (heap copy)", [&] { const uint32_t c = heap_copy(wall, 0x100); if (c) call_this(0x00592b70, c + 0x90, 1, &one); });
    }
    /* w-partyresults: the party modes' results-table fillers with made-up values on a driver (REFLEX_PORT with rx_results): a column
       list {0xa, own column, 9 (none)} and an empty criteria vector as 00613dd0 builds them, in heap blocks; EggHunt's 006151a0 (no
       level holds its scene's GreedModel, so no EggHunt runs), Tag's 00614dc0 / Snake's 00614f00 / Infection's 00615070 with counts that
       need the thousands separator (005dc3c0 / 007900c0), -1 and 0 */
    {
        auto alloc = [&](uint32_t size) { return rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, &size); };
        struct V { int32_t a, b; } values[] = {{-1, -1}, {0, 0}, {7, 12}, {1234, 1234567}, {1000000, 99}};
        const uint32_t own[4] = {0xb, 0xc, 0xd, 0xe}, fn[4] = {0x00614dc0, 0x00614f00, 0x00615070, 0x006151a0};
        const uint32_t fa[4] = {0xf1c, 0xf24, 0xf18, 0xf14}, fb2[4] = {0xf18, 0xf2c, 0, 0xf18};
        for (int mode = 0; mode < 4; mode++)
            for (const V& v : values)
                run_case(m, pages, "party results filler", [&] {
                    const uint32_t ints = alloc(12), cols = alloc(0x18), vec = alloc(0x18), px = alloc(4);
                    if (!ints || !cols || !vec || !px) return;
                    wr(ints, 0xa); wr(ints + 4, own[mode]); wr(ints + 8, 9);
                    for (uint32_t k = 0; k < 0x18; k += 4) { wr(cols + k, 0); wr(vec + k, 0); }
                    wr(cols + 0xc, ints); wr(cols + 0x10, ints + 12); wr(cols + 0x14, ints + 12);
                    wr(px, vec); wr(vec, px);
                    wr(d0 + fa[mode], uint32_t(v.a));
                    if (fb2[mode]) wr(d0 + fb2[mode], mode == 0 ? fb(float(v.b) / 100.0f) : uint32_t(v.b));
                    if (mode == 1 || mode == 3) { const uint32_t a[2] = {vec, cols}; call_this(fn[mode], d0, 2, a); }
                    else { const uint32_t a[3] = {vec, cols, d0}; rx_regs in{}, out{}; rx_call_original_ex(fn[mode], &in, &out, 3, a); }
                });
    }
    std::fprintf(stderr, "[snake] %d cases done (state restored)\n", g_cases);
    return reflex_vm::RUN_ORIGINAL;
}
