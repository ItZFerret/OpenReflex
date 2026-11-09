// wave_hidden_probe.cpp -- the oracle's probe for the w-hidden wave (2026-10-26): the original functions the core called through the
// register-context form (WORLD_CTX) and that the five reference worlds reach rarely or never (development only; see core/plan/QUEUE.md
// "w-hidden"). A frame hook (Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL addresses on the live game objects
// (or on fixtures the probe builds in dead stack), each case on a snapshot of the whole guest memory and machine state that is restored
// afterwards, so with the ports installed Verify runs the core function and then the original on the same state and compares memory and
// the declared registers, exactly as for a call from the game.
//   REFLEX_PORT=rx_world,rx_HiddenProbeHook REFLEX_PORT_VERIFY=1 RX_HID_PROBE=<cases,comma separated> [RX_HID_FRAME=<n>]
// Cases:
//   lock      00769f40 (the kinematic axis lock of a vector) over the lock words
//   list      00768650 (next active entry of a handler list) and 00417070 (keyed object address) on fixture lists
//   insert    00774c30 (vector<BodyRecord> insert-at-iterator) -> 00775800 -> 00775b10 / 004db640 / 00774ce0 / 00776d50 / 004dbf20 / 00777400
//             / 00777a60 / 00778020 / 00430030 on fixture vectors (empty, grown, shifted at the front / middle / end), and the leaves
//             called directly
//   lean      0061db10 (scripted-mode lean), 00621850 (ragdoll blend), 006217a0 (ragdoll update) on a live rider animation
//   wreckkick 006514e0 (the wreck camera kick) on the live third person camera
//   quickcam  00646e80 (the quick cameras' mode) on the live quick cameras
//   keytrack  00476340 (the vehicle key-track block) on a fixture clip: translation and rotation tracks of every shape
#include "oracle.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && a && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
float fl(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
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

struct World {
    uint32_t game = 0, player = 0, vehicle = 0, scratch = 0;
};

World find_world(reflex_vm::Machine& m) {
    World w;
    w.game = rd(0x00a946b8);
    w.scratch = (m.r[4] - 0x8000) & ~0xfu;   // dead stack: the probe's scratch area
    if (!w.game) return w;
    for (uint32_t s = 0; s < 16; s++) {
        const uint32_t p = w.game + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) == 0 || rd(p + 0x10) != 0) continue;   // a local human
        w.player = p;
        w.vehicle = rd(p + 0x1ac);
        if (w.vehicle) break;
    }
    return w;
}

rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call_ex(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
void say(const char* what) { std::fprintf(stderr, "[hid] %s\n", what); }

uint32_t physics_world(const World& w) {
    for (uint32_t off : {0x5a54u, 0x20c8u}) {
        const uint32_t level = rd(w.game + off);
        if (!mapped(level + 0x20134, 4)) continue;
        const uint32_t world = rd(level + 0x20134);
        if (mapped(world, 0xcce0)) return world;
    }
    return 0;
}

uint32_t g_lcg = 12345;
uint32_t rnd() { g_lcg = g_lcg * 1664525u + 1013904223u; return g_lcg >> 8; }

// ---- 00769f40 ----------------------------------------------------------------------------------------------------------------------------
void case_lock(World& w) {
    say("lock");
    const uint32_t body = w.scratch + 0x100, vec = w.scratch + 0x200;
    for (uint32_t lock : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 0x10u, 0x100u, 0xff00u, 0xfffffff8u, 0xffffffffu}) {
        wr(body + 0xb4, lock);
        wf(vec, 1.5f); wf(vec + 4, 2.5f); wf(vec + 8, 3.5f); wr(vec + 0xc, 0x7fc00001u);
        rx_regs in = regs(); in.r[1] = vec; in.r[2] = body;
        call_ex(0x00769f40, in);
    }
}

// ---- 00768650 / 00417070 -----------------------------------------------------------------------------------------------------------------
void case_list(World& w) {
    say("list");
    const uint32_t list = w.scratch + 0x100, arr = w.scratch + 0x200;
    struct Cfg { uint32_t n; uint32_t e[6]; uint32_t cursor; };
    const Cfg cfgs[] = {
        {0, {0}, 0}, {3, {1, 2, 3}, 3}, {3, {0, 0, 0}, 0}, {4, {0, 0, 0x111, 0x222}, 0}, {4, {0, 0, 0x111, 0x222}, 2},
        {4, {0, 0, 0x111, 0}, 3}, {1, {0x333}, 0}, {3, {5, 6, 7}, 5}, {5, {0, 0, 0, 0, 0x444}, 1}, {2, {0, 0}, 1},
    };
    for (const Cfg& c : cfgs) {
        wr(list + 0xc, arr); wr(list + 0x10, arr + 4 * c.n); wr(list + 0x14, arr + 4 * c.n + 8); wr(list + 0x18, c.cursor);
        for (uint32_t i = 0; i < c.n; i++) wr(arr + 4 * i, c.e[i]);
        rx_regs in = regs(); in.r[6] = list; in.r[1] = 0x1234; in.r[2] = 0x5678;
        call_ex(0x00768650, in);
    }
    // the live handler list (world + 0xcc00) too
    if (w.game) {
        const uint32_t world = physics_world(w);
        if (world) { rx_regs in = regs(); in.r[6] = world + 0xcc00; call_ex(0x00768650, in); }
    }
    const uint32_t hdr = w.scratch + 0x300, recs = w.scratch + 0x400;
    wr(hdr + 0xc, recs); wr(hdr + 0x10, recs + 3 * 0xe0);
    for (uint32_t idx = 0; idx < 3; idx++) { rx_regs in = regs(); in.r[6] = hdr; in.r[7] = idx; call_ex(0x00417070, in); }
    if (w.game) {
        const uint32_t world = physics_world(w);
        if (world && rd(world + 0x19a4) > rd(world + 0x19a0)) {
            rx_regs in = regs(); in.r[6] = world + 0x1994; in.r[7] = 0; call_ex(0x00417070, in);
        }
    }
}

// ---- the BodyRecord vector -----------------------------------------------------------------------------------------------------------------
uint32_t heap_alloc(uint32_t count) {
    rx_regs in = regs(); in.r[0] = count;
    return call_ex(0x004db640, in).r[0];
}
void fill_record(uint32_t rec, uint32_t seed) { for (uint32_t k = 0; k < 0x50; k += 4) wr(rec + k, 0x01010101u * (seed & 0xff) + k * 0x10001u + seed); }
// a vector at `vec` with n records in a heap buffer of cap records (cap 0: no buffer)
void make_vec(uint32_t vec, uint32_t proxy, uint32_t n, uint32_t cap) {
    for (uint32_t k = 0; k < 0x20; k += 4) wr(vec + k, 0);
    wr(vec, proxy);
    wr(proxy, vec);
    uint32_t buf = cap ? heap_alloc(cap) : 0;
    for (uint32_t i = 0; i < cap; i++) fill_record(buf + 0x50 * i, 0x40 + i);
    wr(vec + 0xc, buf); wr(vec + 0x10, buf + 0x50 * n); wr(vec + 0x14, buf + 0x50 * cap);
}

void case_insert(World& w) {
    say("insert");
    const uint32_t vec = w.scratch + 0x100, proxy = w.scratch + 0x180, out = w.scratch + 0x1a0, value = w.scratch + 0x200;
    fill_record(value, 0xa5);
    int count = 0;
    for (uint32_t n : {0u, 1u, 2u, 3u, 5u}) for (uint32_t extra : {0u, 1u, 3u}) {
        const uint32_t cap = (n == 0 && extra == 0) ? 0 : n + extra;
        for (uint32_t where = 0; where <= n; where++) {
            make_vec(vec, proxy, n, cap);
            wr(out, 0xdeadbeef); wr(out + 4, 0xdeadbeef);
            const uint32_t begin = rd(vec + 0xc);
            rx_regs in = regs(); in.r[6] = vec;
            uint32_t a[4] = {out, value, proxy, begin + 0x50 * where};
            call_ex(0x00774c30, in, 4, a);
            ++count;
        }
    }
    // a value that lives inside the vector (it moves while the vector shifts / grows)
    for (uint32_t extra : {0u, 2u}) for (uint32_t where : {0u, 1u, 2u}) {
        make_vec(vec, proxy, 3, 3 + extra);
        const uint32_t begin = rd(vec + 0xc);
        rx_regs in = regs(); in.r[6] = vec;
        uint32_t a[4] = {out, begin + 0x50, proxy, begin + 0x50 * where};
        call_ex(0x00774c30, in, 4, a);
        ++count;
    }
    // 00775800 directly (ECX = vector; stack value, container, where) and 00775b10 (EAX = iterator; stack out, offset)
    for (uint32_t n : {0u, 2u, 4u}) for (uint32_t extra : {0u, 2u}) for (uint32_t where : {0u, 1u, n}) {
        if (where > n) continue;
        make_vec(vec, proxy, n, n + extra);
        const uint32_t begin = rd(vec + 0xc);
        rx_regs in = regs(); in.r[1] = vec;
        uint32_t a[3] = {value, proxy, begin + 0x50 * where};
        call_ex(0x00775800, in, 3, a);
        ++count;
    }
    {
        make_vec(vec, proxy, 4, 6);
        const uint32_t iter = w.scratch + 0x1c0;
        for (int32_t off : {0, 1, 3, 4}) {
            wr(iter, proxy); wr(iter + 4, rd(vec + 0xc));
            rx_regs in = regs(); in.r[0] = iter; in.r[2] = 0x7777;
            uint32_t a[2] = {out, uint32_t(off)};
            call_ex(0x00775b10, in, 2, a);
            ++count;
        }
    }
    // the leaves
    {
        make_vec(vec, proxy, 4, 6);
        const uint32_t b = rd(vec + 0xc), dst = w.scratch + 0x1000;
        for (uint32_t k = 0; k < 0x400; k += 4) wr(dst + k, 0x11111111u * (k & 0xf));
        { rx_regs in = regs(); in.r[0] = dst; in.r[1] = value; call_ex(0x00430030, in); }
        { rx_regs in = regs(); in.r[2] = value; in.r[6] = 2; in.r[7] = dst; call_ex(0x00774ce0, in); }
        { rx_regs in = regs(); in.r[6] = 0; in.r[2] = value; in.r[7] = dst; call_ex(0x00774ce0, in); }
        { rx_regs in = regs(); in.r[2] = dst + 0x200; uint32_t a[2] = {b, b + 0x50 * 3}; call_ex(0x00776d50, in, 2, a); }
        { rx_regs in = regs(); in.r[2] = dst + 0x200; uint32_t a[2] = {b, b}; call_ex(0x00776d50, in, 2, a); }
        { uint32_t a[3] = {b, b + 0x50 * 2, dst + 0x300}; call_ex(0x004dbf20, regs(), 3, a); }
        { uint32_t a[3] = {b, b + 0x50 * 2, 0}; (void)a; }   // (a null destination would write through the CRT's null check: not called)
        { rx_regs in = regs(); in.r[1] = value; in.r[2] = dst + 0x100 + 0x50 * 3; uint32_t a[1] = {dst + 0x100}; call_ex(0x00777400, in, 1, a); }
        { rx_regs in = regs(); in.r[1] = value; in.r[2] = dst + 0x100; uint32_t a[1] = {dst + 0x100}; call_ex(0x00777400, in, 1, a); }
        { rx_regs in = regs(); in.r[1] = b + 0x50 * 3; in.r[2] = b; uint32_t a[1] = {dst + 0x300}; call_ex(0x00778020, in, 1, a); }
        { rx_regs in = regs(); in.r[1] = b; in.r[2] = b; uint32_t a[1] = {dst + 0x300}; call_ex(0x00778020, in, 1, a); }
        { rx_regs in = regs(); in.r[0] = b + 0x50 * 3; in.r[7] = dst + 0x300; uint32_t a[4] = {b, 0, 0, 0}; call_ex(0x00777a60, in, 4, a); }
        { rx_regs in = regs(); in.r[0] = b; in.r[7] = dst + 0x300; uint32_t a[4] = {b, 0, 0, 0}; call_ex(0x00777a60, in, 4, a); }
        for (uint32_t c : {1u, 7u, 0u}) { rx_regs in = regs(); in.r[0] = c; call_ex(0x004db640, in); }
    }
    std::fprintf(stderr, "[hid] insert: %d calls\n", count);
}

// ---- the rider animation ---------------------------------------------------------------------------------------------------------------
uint32_t pick_ra() {
    for (uint32_t i = 0; i < 12; i++) {
        const uint32_t ra = rd(0x00a9408cu + 4u * i);
        if (ra && mapped(ra, 0x5bc00) && rd(ra + 0xaf1cu) && rd(ra + 0x598u) && rd(ra + 0x58cu) && rd(ra + 0x3d0) && rd(ra + 0x3f4) && rd(ra + 0x400)) return ra;
    }
    return 0;
}

void case_lean(World& w) {
    say("lean");
    const uint32_t ra = pick_ra();
    if (!ra) { say("lean: no rider animation"); return; }
    const uint32_t cell = w.scratch + 0x100;
    const float nan_dt = std::nanf("");
    // A NaN that reaches FloatListeners_Set goes through the emulator's x87 (fld / fstp), which stores a payload-less NaN where the
    // hardware (and the core) keep the payload: those fixtures are behind RX_HID_NAN=1 (documented artifact, like RX_REMOTE_X87NAN).
    const float nan = std::getenv("RX_HID_NAN") ? std::nanf("") : 0.75f;
    const float dts[] = {0.016f, 0.1f, 0.0f, nan_dt, -0.5f};
    int count = 0;
    auto lean = [&](float dt) { if (std::getenv("RX_HID_TRACE")) std::fprintf(stderr, "[hid] lean #%d dt %g d %g mode %u crash %u out %08x\n", count, dt, fl(rd(ra + 0x5b534)), rd(ra + 0x5bb50), rd(ra + 0x5b2e0), rd(ra + 0x3d0)); rx_regs in = regs(); in.r[0] = ra; in.r[7] = ra; uint32_t a[1] = {fb(dt)}; call_ex(0x0061db10, in, 1, a); ++count; };
    // scripted mode with a small steer input: the first entry (the crash state not yet set), then the steady state
    for (float dt : dts) for (uint32_t prev594 : {1u, 2u}) for (int unset = 0; unset < 4; unset++) {
        wf(ra + 0x5b534, 0.05f); wr(ra + 0x5bb50, 7); wr(ra + 0x5b2e0, 0); wr(ra + 0x5b594, prev594); wr(ra + 0x5b590, 5);
        const float flt_max = 3.4028234663852886e+38f;
        wf(ra + 0x5b700, (unset & 1) ? flt_max : 2.0f); wf(ra + 0x5b6fc, (unset & 2) ? flt_max : 3.0f); wf(ra + 0x5b6f8, (unset == 3) ? nan : 4.0f);
        wf(ra + 0x328, 2.0f); wf(ra + 0x32c, 0.5f); wf(ra + 0x330, 0.3f); wf(ra + 0x320, -1.0f); wf(ra + 0x324, 1.0f);
        wf(cell, 0.25f); wr(ra + 0x3d0, cell); wf(ra + 0x3c8, 0.0f); wf(ra + 0x3cc, 1.0f);
        lean(dt);
        lean(dt);   // the second time the crash state is set already
    }
    struct Rate { float a, b, v, lo, hi; };
    const Rate rates[] = {{2.0f, 0.5f, 0.3f, -1.0f, 1.0f}, {0.5f, 2.0f, 5.0f, -1.0f, 1.0f}, {1.0f, 1.0f, -5.0f, -1.0f, 1.0f}, {0.0f, std::getenv("RX_HID_NAN") ? 0.0f : -1.0f, 1.0f, -1.0f, 1.0f},
                          {nan, 1.0f, 1.0f, -1.0f, 1.0f}, {1.0f, nan, 1.0f, -1.0f, 1.0f}, {1.0f, 2.0f, nan, -1.0f, 1.0f}, {1.0f, 2.0f, 0.5f, 1.0f, -1.0f}};
    for (const Rate& r : rates) for (int out_kind = 0; out_kind < 3; out_kind++) {
        wf(ra + 0x5b534, 0.05f); wr(ra + 0x5bb50, 7); wr(ra + 0x5b2e0, 1);
        wf(ra + 0x328, r.a); wf(ra + 0x32c, r.b); wf(ra + 0x330, r.v); wf(ra + 0x320, r.lo); wf(ra + 0x324, r.hi);
        wf(cell, out_kind == 1 ? 0.95f : 0.25f); wr(ra + 0x3d0, out_kind == 2 ? 0 : cell); wf(ra + 0x3c8, 0.0f); wf(ra + 0x3cc, 1.0f);
        lean(0.1f);
    }
    // the crash state winding down (outside the scripted mode or with a large steer input)
    struct Down { float d; uint32_t mode; uint32_t crash; float cur; int null_out; };
    const Down downs[] = {{0.5f, 7, 1, 0.5f, 0}, {0.5f, 3, 1, 0.5f, 0}, {0.5f, 3, 1, 0.1f, 0}, {0.5f, 3, 1, 0.1f, 1}, {0.05f, 3, 1, 0.1f, 0}, {0.5f, 7, 0, 0.5f, 0},
                          {0.5f, 7, 1, 0.1f, 0}, {0.5f, 3, 1, 5.0f, 0}, {nan_dt, 7, 1, 0.1f, 0}, {nan_dt, 3, 1, 0.0f, 0}, {0.5f, 3, 0, 0.0f, 0}, {0.5f, 3, 1, nan, 0}, {0.1f, 3, 1, 0.3f, 0}};
    for (const Down& d : downs) for (float dt : {0.05f, 0.016f, 0.2f}) {
        wf(ra + 0x5b534, d.d); wr(ra + 0x5bb50, d.mode); wr(ra + 0x5b2e0, d.crash);
        wf(cell, d.cur); wr(ra + 0x3d0, d.null_out ? 0 : cell); wf(ra + 0x3c8, 0.0f); wf(ra + 0x3cc, 1.0f);
        lean(dt);
    }
    std::fprintf(stderr, "[hid] lean: %d calls\n", count);
}

void case_blend(World& w) {
    say("blend");
    const uint32_t ra = pick_ra();
    if (!ra) { say("blend: no rider animation"); return; }
    (void)w;
    const float nan = std::getenv("RX_HID_NAN") ? std::nanf("") : 0.75f;   // see case_lean
    int count = 0;
    // the ragdoll blend (00621850)
    for (uint32_t state : {0u, 1u, 2u, 7u}) for (uint32_t freeze : {0u, 1u}) for (int v = 0; v < 6; v++) {
        wr(ra + 0x5bab8, state);
        wf(ra + 0x3ac, v == 1 ? 0.0f : 0.5f); wf(ra + 0x3b0, v == 2 ? 0.2f : 1.0f); wf(ra + 0x3b4, 0.1f); wf(ra + 0x3a4, v == 5 ? 5.0f : -1.0f); wf(ra + 0x3a8, v == 4 ? 0.01f : (v == 5 ? 9.0f : 1.0f));
        wf(ra + 0x2fc, v == 3 ? nan : (v == 4 ? 0.1f : 0.5f)); wf(ra + 0x300, 0.25f); wf(ra + 0x304, 0.6f); wf(ra + 0x2f4, v == 5 ? 5.0f : 0.0f); wf(ra + 0x2f8, v == 5 ? 9.0f : (v == 4 ? 0.01f : 1.0f));
        wf(ra + 0x354, 0.5f); wf(ra + 0x358, v == 2 ? 0.1f : 2.0f); wf(ra + 0x35c, 0.9f); wf(ra + 0x34c, 0.0f); wf(ra + 0x350, 1.0f);
        wf(ra + 0x3ec, 0.0f); wf(ra + 0x3f0, v == 1 ? 0.1f : 2.0f); wf(ra + 0x3f8, -3.0f); wf(ra + 0x3fc, v == 2 ? -4.0f : 3.0f);
        rx_regs in = regs(); in.r[0] = ra; uint32_t a[1] = {freeze};
        call_ex(0x00621850, in, 1, a);
        ++count;
    }
    std::fprintf(stderr, "[hid] blend: %d calls\n", count);
}

void case_ragdoll(World& w) {
    say("ragdoll");
    const uint32_t ra = pick_ra();
    if (!ra) { say("ragdoll: no rider animation"); return; }
    const float nan = std::nanf("");
    int count = 0;
    // the ragdoll update (006217a0). With a pose override (+0xaf10 != 0) it runs the crash animation update 00620a60 as well; only
    // the override 4 is safe on a live rider that is not crashing (the others read crash-state objects this rider does not have): with
    // both blend outputs at exactly 1.0 it clears the override, else it stays (it is verified in its own right where the worlds reach it).
    const uint32_t out58c = rd(ra + 0x58c), out598 = rd(ra + 0x598);
    const uint32_t cell = w.scratch + 0x100;
    struct Out { float a, b; };
    const Out outs[] = {{0.9f, 0.2f}, {1.0f, 1.0f}, {1.0f, 0.5f}, {-5.0f, -5.0f}, {5.0f, 5.0f}, {0.0f, 0.0f}};
    for (float dt : {0.016f, 0.3f, nan, -0.2f}) for (uint32_t override_flag : {0u, 4u}) for (const Out& o : outs) for (uint32_t nulls = 0; nulls < 4; nulls++) {
        wr(ra + 0xaf10, override_flag);
        wf(ra + 0x584, 0.0f); wf(ra + 0x588, 1.0f); wf(ra + 0x590, 0.0f); wf(ra + 0x594, 1.0f);
        wf(out58c, o.a); wf(out598, o.b);
        wr(ra + 0x58c, (nulls & 1) ? 0 : out58c); wr(ra + 0x598, (nulls & 2) ? 0 : out598);
        (void)cell;
        rx_regs in = regs(); in.r[6] = ra; in.r[3] = ra; uint32_t a[1] = {fb(dt)};
        call_ex(0x006217a0, in, 1, a);
        ++count;
    }
    std::fprintf(stderr, "[hid] ragdoll: %d calls\n", count);
}

// ---- the wreck camera kick ---------------------------------------------------------------------------------------------------------------
uint32_t pick_camera(const World& w, bool third) {
    if (!w.player) return 0;
    for (uint32_t k = 0; k < 6; k++) {
        const uint32_t c = rd(w.player + 0x974 + 4 * k);
        if (!c || !mapped(c, 0x740) || !mapped(rd(c), 0x100)) continue;
        if (third && rd(rd(c) + 0x88) == 0x00653300u) return c;
        if (!third && k >= 3) return c;
    }
    return 0;
}

void case_wreckkick(World& w) {
    say("wreckkick");
    const uint32_t cam = pick_camera(w, true);
    if (!cam) { say("wreckkick: no third person camera"); return; }
    const uint32_t v = w.scratch + 0x100;   // 16-aligned impact vector
    const float nan = std::nanf("");
    const float inf = INFINITY;
    int count = 0;
    wf(cam + 0x5fc, 1.0f); wf(cam + 0x600, 10.0f); wf(cam + 0x604, 0.75f);
    for (uint32_t k = 0; k < 8; k++) { wf(cam + 0x680 + 4 * k, 0.5f + k); }
    struct Imp { float x, y, z, w; };
    const Imp imps[] = {{3, 0, 0, 0}, {0, 0, 5, 1}, {1, 2, 2, 0}, {0.1f, 0.1f, 0.1f, 0}, {30, 0, 0, 0}, {nan, 0, 0, 0}, {0, inf, 0, 0}, {1, 1, 1, nan}, {10, 0, 0, 0}, {1, 0, 0, 0}, {-4, 4, -4, 9}};
    const uint32_t veh = rd(rd(cam + 0x344) + 0x1ac);
    const uint32_t saved194 = rd(veh + 0x194), saved170 = rd(veh + 0x170);
    const uint32_t session_addr = w.game + 0x15b24;   // the session type
    for (const Imp& i : imps) for (uint32_t cls : {saved194, 3u}) for (uint32_t k170 : {saved170, 8u, 9u}) for (uint32_t session : {0u, 1u, 2u}) {
        wf(v, i.x); wf(v + 4, i.y); wf(v + 8, i.z); wf(v + 12, i.w);
        wr(veh + 0x194, cls); wr(veh + 0x170, k170); wr(session_addr, session);
        rx_regs in = regs(); in.r[7] = cam; in.r[3] = 1; in.r[6] = v; uint32_t a[1] = {v};
        call_ex(0x006514e0, in, 1, a);
        ++count;
        if (cls != 3u) break;
    }
    std::fprintf(stderr, "[hid] wreckkick: %d calls (camera %08x)\n", count, cam);
}

void case_quickcam(World& w) {
    say("quickcam");
    const uint32_t cam = pick_camera(w, false);
    if (!cam) { say("quickcam: no quick camera"); return; }
    int count = 0;
    for (uint32_t mode : {0u, 1u, 2u, 3u, 7u, 0u, 1u, 2u}) {
        rx_regs in = regs(); in.r[0] = mode; in.r[6] = cam; in.r[7] = w.player;
        call_ex(0x00646e80, in);
        ++count;
    }
    std::fprintf(stderr, "[hid] quickcam: %d calls (camera %08x)\n", count, cam);
}

// ---- 00476340 ------------------------------------------------------------------------------------------------------------------------------
void case_keytrack(World& w) {
    say("keytrack");
    const uint32_t S = w.scratch;
    const uint32_t block = S + 0x40, clip = S + 0x100, ttab = S + 0x200, rtab = S + 0x400, keys = S + 0x1000;   // 16-aligned
    // translation tracks (0x14 bytes: keys*, -, linear, count): keys of 16 bytes (time, x, y, z)
    uint32_t kp = keys;
    auto tkeys = [&](uint32_t n) { const uint32_t p = kp; for (uint32_t i = 0; i < n; i++) { wf(p + 16 * i, 1.0f + i); wf(p + 16 * i + 4, float(rnd() % 100) - 50.0f); wf(p + 16 * i + 8, float(rnd() % 100) / 7.0f); wf(p + 16 * i + 12, float(rnd() % 100) / 3.0f); } kp += 16 * n + 0x40; return p; };
    struct T { uint32_t linear, n; };
    const T ts[] = {{1, 4}, {0, 4}, {1, 1}, {0, 0}, {1, 2}, {0, 1}, {1, 0}, {0, 7}};
    for (uint32_t i = 0; i < 8; i++) {
        const uint32_t t = ttab + 0x14 * i;
        kp += 0x40;   // a gap so the key before the array is readable memory
        wr(t, tkeys(ts[i].n)); wr(t + 4, 0xff); wr(t + 8, ts[i].linear); wr(t + 0xc, ts[i].n);
    }
    // rotation tracks (0x18 bytes: keys*, cached index, -, enabled, count): keys of 12 bytes (time, packed quaternion u16 + 3 x s16)
    auto rkeys = [&](uint32_t n) {
        const uint32_t p = kp;
        for (uint32_t i = 0; i < n; i++) {
            wf(p + 12 * i, 0.5f + 0.75f * i);
            wr(p + 12 * i + 4, (rnd() & 0xffff) | (rnd() << 16));
            wr(p + 12 * i + 8, (rnd() & 0xffff) | (rnd() << 16));
        }
        kp += 12 * n + 0x40; return p;
    };
    struct R { uint32_t enabled, n, cache; };
    const R rs[] = {{0, 4, 1}, {1, 0, 1}, {1, 1, 1}, {1, 2, 1}, {1, 6, 1}, {1, 12, 10}, {1, 12, 0}, {1, 12, 5}, {1, 3, 9}, {1, 40, 30}};
    for (uint32_t i = 0; i < 10; i++) {
        const uint32_t r = rtab + 0x18 * i;
        wr(r, rkeys(rs[i].n)); wr(r + 4, rs[i].cache); wr(r + 8, 0xff); wr(r + 0xc, rs[i].enabled); wr(r + 0x10, rs[i].n);
    }
    wr(clip + 0x1c, ttab); wr(clip + 0x24, rtab);
    const float times[] = {-1.0f, 0.0f, 0.5f, 1.0f, 1.5f, 2.999f, 4.0f, 5.0f, 9.5f, 20.0f, 28.0f, 100.0f, 1e30f, -0.0f, INFINITY};   // (no NaN time: KeyTrack_FindSegment's binary search never ends on one, in the original as in the core)
    int count = 0;
    for (uint32_t ti = 0; ti < 8; ti++) for (uint32_t ri = 0; ri < 10; ri++) for (float t : times) {
        for (uint32_t i = 0; i < 10; i++) wr(rtab + 0x18 * i + 4, rs[i].cache);   // the cached key indexes as authored
        wr(block, clip); wf(block + 4, t); wr(block + 8, ti % 8); wr(block + 0xc, ri);
        // the rotation record index of the first rotation is the same index as the translation's: bounded by the table (10 records)
        wr(block + 8, ti);
        for (uint32_t k = 0x10; k < 0x40; k += 4) wr(block + k, 0xdeadbeef);
        uint32_t a[1] = {block};
        call_ex(0x00476340, regs(), 1, a);
        ++count;
    }
    std::fprintf(stderr, "[hid] keytrack: %d calls\n", count);
}


// ---- the call sites of the ported functions, run in their callers ----------------------------------------------------------------------
bool skip(const char* what) { const char* e = std::getenv("RX_HID_SKIP"); return e && std::strstr(e, what); }

void case_sites(World& w) {
    say("sites");
    int count = 0;
    // 00773d10: BodyRecordVector push_back, in place and (the vector full) through 00774c30
    if (!skip("push")) {
        const uint32_t vec = w.scratch + 0x100, proxy = w.scratch + 0x180, value = w.scratch + 0x200;
        fill_record(value, 0x77);
        for (uint32_t n : {0u, 1u, 3u}) for (uint32_t extra : {0u, 2u}) {
            const uint32_t cap = (n == 0 && extra == 0) ? 0 : n + extra;
            make_vec(vec, proxy, n, cap);
            rx_regs in = regs(); in.r[0] = vec;
            uint32_t a[1] = {value};
            call_ex(0x00773d10, in, 1, a);
            ++count;
        }
    }
    // 00770b00: Kinematic_Activate with the list full (200): the first active kinematic body of the handler list is evicted. Keyed
    // objects (inactive: +0x80 < 0) are activated one after the other; every second one with the count forced to 200.
    if (w.game && !skip("activate")) {
        const uint32_t world = physics_world(w);
        std::vector<uint32_t> spare;
        if (world) {
            const uint32_t begin = rd(world + 0x19a0), end = rd(world + 0x19a4);
            for (uint32_t o = begin; o < end && mapped(o, 0xe0) && spare.size() < 8; o += 0xe0) if (int32_t(rd(o + 0x80)) < 0) spare.push_back(o);
        }
        if (spare.size() >= 2) {
            const uint32_t vel = w.scratch + 0x300;
            for (uint32_t k = 0; k < 16; k += 4) wr(vel + k, 0);
            for (size_t i = 0; i < spare.size(); i++) {
                if (i % 2 == 1) wr(world + 0x19d4, 200);
                rx_regs in = regs(); in.r[1] = spare[i];
                uint32_t a[2] = {world, vel};
                call_ex(0x00770b00, in, 2, a);
                ++count;
                if (i % 2 == 1) { wr(world + 0x19d4, 200); call_ex(0x00770b00, in, 2, a); ++count; }   // (already active: the early return)
            }
            std::fprintf(stderr, "[hid] sites: handler list after: [%08x %08x], count %u\n", rd(world + 0xcc0c), rd(world + 0xcc10), rd(world + 0x19d4));
        } else say("sites: fewer than two inactive keyed bodies for Kinematic_Activate");
    }
    // 0061e3d0: the locomotion update in a scripted mode / a crash (the two ported callees)
    if (const uint32_t ra = skip("loco") ? 0 : pick_ra()) {
        const uint32_t cell = w.scratch + 0x100;
        for (uint32_t mode : {7u, 3u}) for (uint32_t crash : {0u, 1u}) {
            if (mode != 7 && crash == 0) continue;   // (a plain ride in a made-up mode runs the whole steer update on a state it does not have)
            wr(ra + 0xaf10, 0); wr(ra + 0x5bb50, mode); wr(ra + 0x5b2e0, crash);
            wf(ra + 0x5b534, 0.05f); wf(cell, 0.25f); wr(ra + 0x3d0, cell);
            rx_regs in = regs(); in.r[0] = ra; uint32_t a[1] = {fb(0.016f)};
            call_ex(0x0061e3d0, in, 1, a);
            ++count;
        }
        // RiderAnimation::vf19 (0061c210) in the ragdoll mode 2: 006217a0 and the rest of the frame update
        if (std::getenv("RX_HID_VF19")) {
            wr(ra + 0xaf10, 0); wr(ra + 0xaf08, 0); wr(ra + 0xaee0, 2); wr(ra + 0x5bb5c, 1);
            rx_regs in = regs(); in.r[1] = ra; uint32_t a[1] = {fb(0.016f)};
            call_ex(0x0061c210, in, 1, a);
            ++count;
        }
    }
    // 00651000: MXThirdPersonCamera::vf33 with the wreck kick as the fallback
    if (const uint32_t cam = skip("cam") ? 0 : pick_camera(w, true)) {
        const uint32_t rider = rd(rd(cam + 0x344) + 0x1ac);
        const uint32_t imp = w.scratch + 0x400, shake = w.scratch + 0x420;
        wf(cam + 0x5fc, 1.0f); wf(cam + 0x600, 10.0f); wf(cam + 0x604, 0.75f);
        wr(rider + 0x280, 0);
        for (uint32_t k = 0; k < 16; k += 4) { wr(shake + k, 0); }
        for (float mag : {3.0f, 0.1f, 30.0f}) for (uint32_t no_kick : {0u, 1u}) {
            wf(imp, mag); wf(imp + 4, -mag); wf(imp + 8, 0.0f); wf(imp + 12, 0.0f);
            wr(cam + 0x608, 0); wr(cam + 0x618, 0); wr(cam + 0x5f8, 1); wr(cam + 0x392, 0);
            rx_regs in = regs(); in.r[1] = cam;
            uint32_t a[4] = {0, shake, imp, no_kick};
            call_ex(0x00651000, in, 4, a);
            ++count;
        }
    }
    // (004636f0 with the effect timer enabled is not driven: enabled, the vehicle's own update goes on into rider code that needs the whole
    //  effect set up, and faults the original on the made-up block; the call site there is the plain WORLD_CALL_SP of the verified function.)
    std::fprintf(stderr, "[hid] sites: %d calls\n", count);
}

struct Case { const char* name; void (*fn)(World&); };
const Case kCases[] = {
    {"lock", case_lock}, {"list", case_list}, {"insert", case_insert}, {"lean", case_lean}, {"blend", case_blend}, {"ragdoll", case_ragdoll},
    {"wreckkick", case_wreckkick}, {"quickcam", case_quickcam}, {"keytrack", case_keytrack}, {"sites", case_sites},
};

void run_case(reflex_vm::Machine& m, const Case& c, World w) {
    uint32_t regs_[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs_, m.r, sizeof regs_); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[hid] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    c.fn(w);
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs_, sizeof regs_); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, HiddenProbeHook, "thunk", 0) {
    const char* cases = std::getenv("RX_HID_PROBE");
    if (!cases) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_HID_FRAME") ? uint32_t(std::atoi(std::getenv("RX_HID_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    const World w = find_world(m);
    std::fprintf(stderr, "[hid] frame %u: game %08x player %08x vehicle %08x\n", g_frame, w.game, w.player, w.vehicle);
    const std::string list(cases);
    for (const Case& c : kCases) {
        size_t pos = 0;
        while (pos <= list.size()) {
            size_t e = list.find(',', pos); if (e == std::string::npos) e = list.size();
            if (list.compare(pos, e - pos, c.name) == 0 && e - pos == std::strlen(c.name)) { run_case(m, c, w); break; }
            pos = e + 1;
        }
    }
    return reflex_vm::RUN_ORIGINAL;
}
