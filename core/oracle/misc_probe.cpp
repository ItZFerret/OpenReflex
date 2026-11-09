// misc_probe.cpp -- the oracle's probe for the miscport ports that no reference world reaches (development only).
// A frame hook (group "thunk", Engine::Update 0084ed20, outside every verified call; the same device as race_net_probe.cpp) that, at a chosen
// frame of a Verify run, calls original function addresses with live game state or with objects it builds on the game heap; every call goes
// through the installed port, so Verify runs the core function and then the original on the same state and compares memory and the declared
// registers exactly as for a call from the game.
//   REFLEX_PORT=<ports>,rx_MiscProbeHook REFLEX_PORT_VERIFY=1 RX_MISC_PROBE=<groups> [RX_MISC_PROBE_FRAME=<n>] [RX_MISC_PROBE_LATE=<n>] [RX_MISC_PROBE_SEED=<n>]
// groups: m state-name map insert (00585e90 / 005862f0 / 005864d0), l scene light registry removal (007e0190 and helpers),
//         p player table clean-up (00553a50 / 00553c60 / 004de700) and the leave of a live driver (0055c1d0; destructive: LATE frame)
// No game logic here: only argument construction. Virtual calls of fake objects go to stubs (natives on unused padding addresses of the exe)
// that both runs see; the stubs read the objects' memory, so they are deterministic.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
extern "C" {
#include "../game/race_activity_misc.h"
}
using namespace reflex_port;
using rx_oracle::bits;
using rx_oracle::f32;

namespace {
uint32_t g_frame = 0, g_rng = 0x2545f491u;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
reflex_vm::Machine* g_m = nullptr;
struct Saved {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint32_t flags; size_t fpu;
    explicit Saved(reflex_vm::Machine& m) : flags(m.flags), fpu(m.fpu.size()) { std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); }
    void restore(reflex_vm::Machine& m) const { std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu.resize(fpu); }
};
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
uint32_t halloc(uint32_t n) {
    const uint32_t a[1] = {n};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
}

// ---- m: the state-name map (std::map<int, std::string>) -------------------------------------------------------------------
// A map object as the game builds it: +0 the container proxy (a cell holding the map), +0x18 the head node (links to itself, black, isnil), +0x1c the size.
// std::string values are built by the guest's own constructor through the pair {key, string}.
uint32_t make_map() {
    const uint32_t map = halloc(0x20), proxy = halloc(4), head = halloc(0x30);
    wr(proxy, map); wr(map, proxy); wr(map + 0x18, head); wr(map + 0x1c, 0);
    wr(head, head); wr(head + 4, head); wr(head + 8, head);
    rx_wr8(head + 0x2c, 1); rx_wr8(head + 0x2d, 1);
    return map;
}
// the pair {key, std::string} of a key: the string "s<key>" (inline buffer, 0x1c bytes: +4 buffer, +0x14 size, +0x18 capacity)
uint32_t make_pair(uint32_t key) {
    const uint32_t p = halloc(0x20);
    wr(p, key);
    const char* base = "state";
    for (uint32_t i = 0; i < 6; i++) rx_wr8(p + 4 + i, uint8_t(base[i]));
    wr(p + 4 + 0x10, 5); wr(p + 4 + 0x14, 0xf);
    return p;
}
uint32_t nth_node(uint32_t map, int n) {                  // in order: 0 = first; n >= size = the head (end)
    uint32_t head = rd(map + 0x18), x = rd(head);         // leftmost
    for (int i = 0; i < n && x != head; i++) {
        if (rx_rd8(rd(x + 8) + 0x2d) == 0) { x = rd(x + 8); while (rx_rd8(rd(x) + 0x2d) == 0) x = rd(x); }
        else { uint32_t p = rd(x + 4); while (p != head && x == rd(p + 8)) { x = p; p = rd(p + 4); } x = p; }
    }
    return x;
}
void probe_map() {
    for (int t = 0; t < 40; t++) {
        const uint32_t map = make_map();
        const int count = 1 + rnd() % 14;
        for (int i = 0; i < count; i++) {
            const uint32_t key = (rnd() % 3 == 0 && rd(map + 0x1c)) ? rd(nth_node(map, rnd() % rd(map + 0x1c)) + 0xc) : rnd() % 40;   // some duplicates
            const uint32_t val = make_pair(key), out = halloc(0x10);
            const uint32_t size = rd(map + 0x1c);
            const uint32_t hint = nth_node(map, size ? int(rnd() % (size + 1)) : 0);
            const uint32_t a[5] = {map, out, val, rd(map), hint};
            call(0x00585e90, regs(), 5, a);
            if (i % 3 == 0) {                                 // insert(value) and _Insert directly too
                const uint32_t v2 = make_pair(rnd() % 40), o2 = halloc(0x10);
                const uint32_t b[3] = {map, o2, v2};
                call(0x005862f0, regs(), 3, b);
            }
        }
    }
}

// ---- l: the scene's light registry (007e0190) ------------------------------------------------------------------------------
// The live registry (scene manager + 0x1a18, its critical sections and event grid are the game's); its lists are replaced by the probe's for each trial and
// restored. The fake entity E: vtable EV {+0x38 the matrix (E + 0x100), +0x5c the event id (E + 0x310), +0x64 the volume (E + 0x300)}; the volume V: vtable VV
// {+4 the type (V + 0x10), +0x18 the bounds (vec at V + 0x20, V + 0x30, the radius V + 0x40)}. The stubs sit on padding bytes of the exe.
const uint32_t kStubGetMat = 0x0046c060, kStubGetId = 0x0046c068, kStubGetVol = 0x0046c070, kStubGetType = 0x0046c078, kStubBounds = 0x0046c080;
void install_light_stubs() {
    g_m->natives[kStubGetMat] = [](reflex_vm::Machine& mm) -> uint32_t { mm.r[0] = mm.r[1] + 0x100; return mm.ret(0); };
    g_m->natives[kStubGetId] = [](reflex_vm::Machine& mm) -> uint32_t { mm.r[0] = rd(mm.r[1] + 0x310); return mm.ret(0); };
    g_m->natives[kStubGetVol] = [](reflex_vm::Machine& mm) -> uint32_t { mm.r[0] = rd(mm.r[1] + 0x300); return mm.ret(0); };
    g_m->natives[kStubGetType] = [](reflex_vm::Machine& mm) -> uint32_t { mm.r[0] = rd(mm.r[1] + 0x10); return mm.ret(0); };
    g_m->natives[kStubBounds] = [](reflex_vm::Machine& mm) -> uint32_t {
        const uint32_t v = mm.r[1];
        for (uint32_t i = 0; i < 4; i++) wr(mm.arg(0) + 4 * i, rd(v + 0x20 + 4 * i));
        for (uint32_t i = 0; i < 3; i++) wr(mm.arg(1) + 4 * i, rd(v + 0x30 + 4 * i));
        wr(mm.arg(2), rd(v + 0x40));
        return mm.ret(0xc);
    };
}
void remove_light_stubs() { for (uint32_t a : {kStubGetMat, kStubGetId, kStubGetVol, kStubGetType, kStubBounds}) g_m->natives.erase(a); }
uint32_t fbits(float v) { return bits(v); }
float rfl(float lo, float hi) { return lo + (hi - lo) * (float(rnd() & 0xffffff) / 16777216.0f); }
void probe_lights() {
    const uint32_t G = rd(0x00a946b8);
    uint32_t player = 0;
    for (uint32_t i = 0; i < 12 && !player; i++) { const uint32_t p = G + 0x7460 + 0x12a0 * i; if (rd(p + 0x94) != 0) player = p; }
    if (!player) { std::fprintf(stderr, "[miscprobe] no player with an activity\n"); return; }
    const uint32_t scene = rd(rd(rd(player + 0x94) + 0x234) + 0x20138), L = scene + 0x1a18;
    // the live fields the removal touches
    const uint32_t offs[] = {0x1000, 0x1004, 0x1014, 0x1018, 0x103c, 0x1040, 0x10b4, 0x10b8, 0x10bc, 0x10dc, 0x10e0, 0x10e4, 0x10fc, 0x1100, 0x1108, 0x110c};
    uint32_t keep[sizeof offs / sizeof offs[0]];
    for (size_t i = 0; i < sizeof offs / sizeof offs[0]; i++) keep[i] = rd(L + offs[i]);
    install_light_stubs();
    const uint32_t EV = halloc(0x100), VV = halloc(0x100), octree = halloc(0x100);
    wr(EV + 0x38, kStubGetMat); wr(EV + 0x5c, kStubGetId); wr(EV + 0x64, kStubGetVol);
    wr(VV + 4, kStubGetType); wr(VV + 0x18, kStubBounds);
    for (int t = 0; t < 400; t++) {
        const uint32_t type = t % 4, with_id = (t / 4) & 1, present = (t / 8) % 3 != 0, grid_extra = (t / 24) % 3;
        const uint32_t E = halloc(0x400), V = halloc(0x100);
        wr(E, EV); wr(V, VV); wr(E + 0x300, V); wr(V + 0x10, type); wr(E + 0x310, with_id ? 0x1000 + (rnd() & 0xff) : 0);
        for (uint32_t i = 0; i < 16; i++) wr(E + 0x100 + 4 * i, fbits(rfl(-2.0f, 2.0f)));
        for (uint32_t i = 0; i < 4; i++) wr(V + 0x20 + 4 * i, fbits(rfl(-30.0f, 30.0f)));
        for (uint32_t i = 0; i < 3; i++) wr(V + 0x30 + 4 * i, fbits(rfl(-30.0f, 30.0f)));
        wr(V + 0x40, fbits(rfl(0.5f, 12.0f)));
        const uint32_t other = halloc(0x40);
        // the pointer list +0x103c: e among others (or absent)
        const uint32_t pl = halloc(0x40), pn = 1 + rnd() % 3;
        for (uint32_t i = 0; i < pn; i++) wr(pl + 4 * i, other + 4 * i);
        if (present) wr(pl + 4 * (rnd() % pn), E);
        wr(L + 0x103c, pl); wr(L + 0x1040, pn); wr(L + 0x10b8, 5); wr(L + 0x10e0, 6);
        switch (type) {
        case 0: {                                          // 8-byte items {entity, ?}; the octree is the probe's (no items)
            const uint32_t items = halloc(0x80), n = 1 + rnd() % 4;
            for (uint32_t i = 0; i < n; i++) { wr(items + 8 * i, other + 4 * i); wr(items + 8 * i + 4, rnd()); }
            if (present) wr(items + 8 * (rnd() % n), E);
            wr(L + 0x10fc, items); wr(L + 0x1108, n); wr(L + 0x1100, octree);
            break; }
        case 3: { const uint32_t arr = halloc(0x40), n = 1 + rnd() % 3; for (uint32_t i = 0; i < n; i++) wr(arr + 4 * i, other + 4 * i);
            if (present) wr(arr + 4 * (rnd() % n), E);
            wr(L + 0x1014, arr); wr(L + 0x1018, n); break; }
        case 2: { const uint32_t arr = halloc(0x40), n = 1 + rnd() % 3; for (uint32_t i = 0; i < n; i++) wr(arr + 4 * i, other + 4 * i);
            if (present) wr(arr + 4 * (rnd() % n), E);
            wr(L + 0x1000, arr); wr(L + 0x1004, n); break; }
        default: {                                         // 0x20-byte records (key +0x10 = the entity) and 0x40-byte records (+0x10 the entity, or +0x30 the id)
            const uint32_t a20 = halloc(0x100), a40 = halloc(0x200), n20 = 1 + rnd() % 3, n40 = 1 + rnd() % 3;
            for (uint32_t i = 0; i < n20; i++) { for (uint32_t o = 0; o < 0x20; o += 4) wr(a20 + 0x20 * i + o, rnd()); wr(a20 + 0x20 * i + 0x10, other + 4 * i); wr(a20 + 0x20 * i + 0x14, fbits(rfl(0.5f, 9.0f))); wr(a20 + 0x20 * i, fbits(rfl(-20.0f, 20.0f))); wr(a20 + 0x20 * i + 4, fbits(rfl(-20.0f, 20.0f))); wr(a20 + 0x20 * i + 8, fbits(rfl(-20.0f, 20.0f))); }
            for (uint32_t i = 0; i < n40; i++) { for (uint32_t o = 0; o < 0x40; o += 4) wr(a40 + 0x40 * i + o, rnd()); wr(a40 + 0x40 * i + 0x10, with_id ? 0 : other + 4 * i); wr(a40 + 0x40 * i + 0x30, with_id ? rnd() : 0); wr(a40 + 0x40 * i + 0x14, fbits(rfl(0.5f, 9.0f))); wr(a40 + 0x40 * i, fbits(rfl(-20.0f, 20.0f))); wr(a40 + 0x40 * i + 4, fbits(rfl(-20.0f, 20.0f))); wr(a40 + 0x40 * i + 8, fbits(rfl(-20.0f, 20.0f))); }
            if (present) {
                const uint32_t i20 = rnd() % n20, i40 = rnd() % n40;
                wr(a20 + 0x20 * i20 + 0x10, E);
                if (with_id) wr(a40 + 0x40 * i40 + 0x30, rd(E + 0x310)); else wr(a40 + 0x40 * i40 + 0x10, E);
            }
            wr(L + 0x10b4, a20); wr(L + 0x10b8, n20); wr(L + 0x10dc, a40); wr(L + 0x10e0, n40);
            break; }
        }
        (void)grid_extra;
        rx_regs in = regs(); in.r[0] = L;
        const uint32_t a[1] = {E};
        call(0x007e0190, in, 1, a);
    }

    // the helpers of the removal called directly: the cell lists C / D (007da9c0 / 007dab70), the grid removals (007da290 / 007da350, EDI = the live grid), the
    // pointer array search (007f9db0) and the 0x40-byte record removal (007f9fd0)
    for (int t = 0; t < 300; t++) {
        const uint32_t cell = halloc(0x80), data = halloc(0x400);
        const bool c_list = t & 1;
        const uint32_t cnt = c_list ? 0x44 : 0x20, tail = c_list ? 0x4c : 0x28, dat = c_list ? 0x5c : 0x60;
        const uint32_t n = rnd() % 9, tl = (n ? n - 1 : 0) + rnd() % 4;
        for (uint32_t i = 0; i < 16; i++) { for (uint32_t o = 4; o < 0x20; o += 4) wr(data + 32 * i + o, (o == 4 || o == 8) ? fbits(rfl(-5.0f, 5.0f)) : rnd()); wr(data + 32 * i, 1 + rnd() % 3); }
        wr(cell + cnt, n); wr(cell + tail, tl); wr(cell + dat, data);
        const uint32_t ev = halloc(0x40); wr(ev, 1 + rnd() % 3);
        const uint32_t a[2] = {cell, ev};
        call(c_list ? 0x007da9c0 : 0x007dab70, regs(), 2, a);
    }
    const uint32_t grid = rd(L + 0x10f0);
    for (int t = 0; t < 120; t++) {
        const uint32_t pos = halloc(0x20);
        for (uint32_t i = 0; i < 3; i++) wr(pos + 4 * i, fbits(rfl(-300.0f, 300.0f)));
        wr(pos + 0xc, 0);
        rx_regs in = regs(); in.r[7] = grid;
        const uint32_t a[3] = {1 + rnd() % 5, fbits(rfl(0.5f, 40.0f)), pos};
        call((t & 1) ? 0x007da290 : 0x007da350, in, 3, a);
    }
    for (int t = 0; t < 120; t++) {
        const uint32_t arr = halloc(0x20), data = halloc(0x40), key = halloc(8), n = rnd() % 5;
        for (uint32_t i = 0; i < n; i++) wr(data + 4 * i, 0x100 + (rnd() % 4));
        wr(arr, data); wr(arr + 4, (t % 7 == 0) ? uint32_t(-int(rnd() % 3)) : n); wr(key, 0x100 + (rnd() % 4));
        rx_regs in = regs(); in.r[1] = arr; const uint32_t a[1] = {key};
        call(0x007f9db0, in, 1, a);
        const uint32_t arr40 = halloc(0x20), d40 = halloc(0x400), n40 = 1 + rnd() % 5;
        for (uint32_t i = 0; i < n40 * 16; i++) wr(d40 + 4 * i, (i % 16 == 1 || i % 16 == 2 || i % 16 == 5 || i % 16 == 6) ? fbits(rfl(-9.0f, 9.0f)) : rnd());
        wr(arr40, d40); wr(arr40 + 4, n40);
        rx_regs in2 = regs(); in2.r[1] = rnd() % n40; in2.r[2] = arr40;
        call(0x007f9fd0, in2);
    }
    for (size_t i = 0; i < sizeof offs / sizeof offs[0]; i++) wr(L + offs[i], keep[i]);
    remove_light_stubs();
}

// ---- p: the Player List clean-ups (00553a50 / 00553c60 with 004de700) and the leave of a live driver (0055c1d0) --------------
uint32_t tbl_rows(uint32_t t) { const uint32_t b = rd(t + 0x10), e = rd(t + 0x14); return b == e ? 0 : rd(rd(rd(b) + 0x40) + 0x10) - rd(rd(rd(b) + 0x40) + 0xc) >> 2; }
uint32_t cell_of(uint32_t t, uint32_t name, uint32_t row) {
    const uint32_t a[1] = {name};
    rx_regs in = regs(); in.r[0] = t;
    const uint32_t ci = call(0x00402160, in, 1, a).r[0];
    return rd(rd(rd(rd(t + 0x10) + 4 * ci) + 0x40) + 0xc) + 4 * row;
}
void probe_players(bool destructive) {
    const uint32_t G = rd(0x00a946b8), players = G + 0x7460, PL = 0x00ae0e08;
    const uint32_t rows = tbl_rows(PL);
    std::fprintf(stderr, "[miscprobe] Player List rows %u; Vehicle List %u; tables 0xae0550 %u, 0xae0898 %u; Rider List %u; 0xae1a28 %u, 0xae0f10 %u\n", rows, tbl_rows(0x00ae05c8),
                 tbl_rows(0x00ae0550), tbl_rows(0x00ae0898), tbl_rows(0x00ae19b0), tbl_rows(0x00ae1a28), tbl_rows(0x00ae0f10));
    uint32_t uid[16]; uint32_t n = 0;
    for (uint32_t r = 0; r < rows && n < 16; r++) uid[n++] = rd(cell_of(PL, 0x0093df0c, r));
    // 00553a50: the pending disconnects: some existing unique ids, an unknown one
    for (int t = 0; t < 6; t++) {
        const uint32_t cnt = 1 + rnd() % (n < 4 ? n : 4);
        for (uint32_t i = 0; i < cnt; i++) wr(players + 0xdfdc + 4 * i, (t == 5 && i == 0) ? 0x7777 : uid[(rnd() + i) % n]);
        wr(players + 0xe00c, t == 4 ? 0 : cnt);
        const uint32_t a[1] = {players};
        call(0x00553a50, regs(), 1, a);
    }
    // 00552c60: a slot allocated by the game's own 00552ad0 (the player constructed) released again
    for (int t = 0; t < 3; t++) {
        rx_regs in = regs(); in.r[1] = players;
        const uint32_t P = call(0x00552ad0, in).r[0];
        if (!P) break;
        rx_regs in2 = regs(); in2.r[6] = players; in2.r[0] = P;
        call(0x00552c60, in2);
    }
    if (!destructive) return;
    // 00553c60: rows added by the game's own builders (Player List 004dd460, Vehicle List 004de810, Rider List 004dea90) with the vehicle / rider owned by a
    // network client (owner 5) and their proxy rows in the four network tables (the columns' defaults appended: 004eb4e0, the cells then written)
    const uint32_t PLn = 0x00ae0e08, VL = 0x00ae05c8, RL = 0x00ae19b0;
    auto add_table_row = [&](uint32_t t) {
        for (uint32_t c = rd(t + 0x10); c < rd(t + 0x14); c += 4) { rx_regs in = regs(); in.r[0] = rd(c); call(0x004eb4e0, in); }
        return tbl_rows(t) - 1;
    };
    uint32_t ids[4]; uint32_t made = 0;
    for (uint32_t k = 0; k < 4; k++) {
        const uint32_t p = halloc(0x60), name = halloc(0x20);
        const char* nm = "probe";
        for (uint32_t i = 0; i < 6; i++) rx_wr8(name + i, uint8_t(nm[i]));
        wr(p + 0, k & 1); wr(p + 4, (k >> 1) & 1); wr(p + 8, name); wr(p + 0xc, name); wr(p + 0x10, 0);
        wr(p + 0x14, 0x3d); wr(p + 0x18, 0xff); wr(p + 0x1c, 0xffffffffu); wr(p + 0x20, 0xffffffffu); wr(p + 0x24, 0xffffffffu); wr(p + 0x28, 0xffffffffu);
        wr(p + 0x2c, 0xffffffffu); wr(p + 0x30, 0); wr(p + 0x34, 0x5150 + k); wr(p + 0x38, 0); wr(p + 0x3c, 0); wr(p + 0x40, 0);
        { const uint32_t a[1] = {p}; call(0x004dd460, regs(), 1, a); }
        const uint32_t prow = tbl_rows(PLn) - 1;
        if (k != 3) {                                         // the vehicle / rider rows (k == 3: ids left -1: no rows)
            const uint32_t av[2] = {0, 0x77 + k}; call(0x004de810, regs(), 2, av); const uint32_t vrow = tbl_rows(VL) - 1;
            const uint32_t ar[3] = {0, 0x88 + k, 0}; call(0x004dea90, regs(), 3, ar); const uint32_t rrow = tbl_rows(RL) - 1;
            const uint32_t vid = rd(cell_of(VL, 0x00936cb4, vrow)), rid = rd(cell_of(RL, 0x0093dec8, rrow));
            if (k != 2) {                                     // k == 2: owners left as the builders made them
                wr(cell_of(VL, 0x0093df84, vrow), 5); wr(cell_of(VL, 0x0093df94, vrow), 1000 + k);
                wr(cell_of(RL, 0x0093dfa4, rrow), 5); wr(cell_of(RL, 0x0093dfb0, rrow), 2000 + k);
                for (uint32_t t : {0x00ae0550u, 0x00ae0898u}) { for (int n = 0; n < 2; n++) wr(cell_of(t, 0x0093e288, add_table_row(t)), 1000 + (n ? 0 : k)); }
                for (uint32_t t : {0x00ae1a28u, 0x00ae0f10u}) { for (int n = 0; n < 2; n++) wr(cell_of(t, 0x0093e23c, add_table_row(t)), 2000 + (n ? 0 : k)); }
            }
            wr(cell_of(PLn, 0x00936cb4, prow), vid); wr(cell_of(PLn, 0x0093dec8, prow), rid);
            { const uint32_t kp = halloc(8); wr(kp, vid);
              const uint32_t ci = call(0x00402160, [&]{ rx_regs r = regs(); r.r[0] = VL; return r; }(), 1, (const uint32_t[]){0x00936cb4}).r[0];
              rx_regs in = regs(); in.r[0] = kp; const uint32_t a[1] = {rd(rd(VL + 0x10) + 4 * ci)};
              const uint32_t ref = call(0x00418ec0, in, 1, a).r[0];
              std::fprintf(stderr, "[miscprobe] k %u vehicle row %u id %u ref {%u, %08x, %08x} owner cell %u\n", k, vrow, vid, rd(ref), rd(ref + 4), rd(ref + 8), rd(cell_of(VL, 0x0093df84, vrow))); }
        }
        ids[made++] = 0x5150 + k;
    }
    for (uint32_t i = 0; i < made; i++) wr(players + 0xe010 + 4 * i, ids[i]);
    wr(players + 0xe040, made);
    const uint32_t a[1] = {players};
    call(0x00553c60, regs(), 1, a);
}

// ---- s: the player attribute table (004e21d0 / 004e2340) -------------------------------------------------------------------
void probe_attrs() {
    for (int t = 0; t < 40; t++) {
        const uint32_t a[4] = {rnd() % 3, rnd() % 3, rnd() % 5, rnd()};
        call(0x004e2340, regs(), 4, a);
        // deletes match no row: rx_tbl_delete_rows' matching-row path (004ed07e) is not ported
        if (t % 4 == 3) { const uint32_t b[2] = {100 + rnd() % 3, rnd() % 3}; call(0x004e21d0, regs(), 2, b); }
    }
}

// ---- r: MXRecords_Leave (00764560, with 00763600) on the live record slots ---------------------------------------------------------
void probe_records() {
    const uint32_t rec = rd(0x00a946c0);
    if (!rec) { std::fprintf(stderr, "[miscprobe] no MXRecords\n"); return; }
    for (uint32_t slot = 0; slot < 4; slot++) {
        const uint32_t S = rec + 0x18 * slot;
        std::fprintf(stderr, "[miscprobe] records slot %u: table %08x row %08x stored %08x loc %08x veh %08x\n", slot, rd(S), rd(S + 4), rd(S + 8), rd(S + 0xc), rd(S + 0x10));
        const uint32_t a[2] = {rec, slot};
        call(0x00764560, regs(), 2, a);
    }
}

// ---- o: the podium scenario (0074ada0 binds, 0074b180 releases) on the live free-ride activity ------------------------------------------
void probe_podium() {
    const uint32_t G = rd(0x00a946b8);
    uint32_t act = 0;
    for (uint32_t i = 0; i < 12 && !act; i++) { const uint32_t p = G + 0x7460 + 0x12a0 * i; if (rd(p + 0x94) != 0) act = rd(p + 0x94); }
    if (!act) return;
    const uint32_t P = rd(act + 0x234) + 0x20150;
    for (int t = 0; t < 2; t++) {
        rx_regs in = regs(); in.r[0] = act; in.r[1] = P;
        call(0x0074ada0, in);
        for (uint32_t flag = 0; flag < 2; flag++) {
            if (flag == 0 && t == 0) continue;
            rx_regs in2 = regs(); in2.r[0] = P;
            const uint32_t a[1] = {flag};
            call(0x0074b180, in2, 1, a);
            if (flag == 0 && t == 1) { rx_regs in3 = regs(); in3.r[0] = act; in3.r[1] = P; call(0x0074ada0, in3); }
        }
    }
}

// ---- d: the free ride's discovery tracker (00585330) around a live player's vehicle --------------------------------------------------
void probe_discoveries() {
    const uint32_t G = rd(0x00a946b8);
    uint32_t player = 0;
    for (uint32_t i = 0; i < 12 && !player; i++) { const uint32_t p = G + 0x7460 + 0x12a0 * i; if (rd(p + 4) != 0 && rd(p + 0x10) == 0 && rd(p + 0x1ac) != 0 && rd(p + 0x94) != 0) player = p; }
    if (!player) { std::fprintf(stderr, "[miscprobe] no local player\n"); return; }
    const uint32_t act = rd(player + 0x94), veh = rd(player + 0x1ac);
    const uint32_t stub = 0x0046c088;
    g_m->natives[stub] = [](reflex_vm::Machine& mm) -> uint32_t { const uint32_t e = mm.r[1], out = mm.arg(0); for (uint32_t i = 0; i < 4; i++) wr(out + 4 * i, rd(e + 0x1c0 + 4 * i)); return mm.ret(4); };
    const uint32_t EV = halloc(0x100); wr(EV + 0x44, stub);
    float vpos[3];
    {   const uint32_t out = halloc(0x10); rx_regs in = regs(); in.r[1] = veh; const uint32_t a[1] = {out};
        call(rd(rd(veh) + 0x44), in, 1, a); for (int i = 0; i < 3; i++) vpos[i] = f32(rd(out + 4 * i)); }
    const float thr1 = f32(rd(0x009ca32c)), thr2 = f32(rd(0x009ca330));
    std::fprintf(stderr, "[miscprobe] vehicle at %.1f %.1f %.1f thresholds %.2f %.2f\n", vpos[0], vpos[1], vpos[2], thr1, thr2);
    for (int t = 0; t < 60; t++) {
        const uint32_t T = halloc(0x100), n = 1 + rnd() % 9;
        wr(T, n); wr(T + 0x84, rnd() % (n + 1));
        for (uint32_t i = 0; i < n; i++) {
            const uint32_t E = halloc(0x240); wr(E, EV); wr(E + 0x140, rnd() % 4 == 0); wr(E + 0x70, rnd()); rx_wr8(E + 0xa2, 1);
            const float r = std::sqrt(thr1 > 0 ? thr1 : 4.0f);
            const float ox = float(int(rnd() % 2001) - 1000) / 1000.0f * r * 1.6f, oz = float(int(rnd() % 2001) - 1000) / 1000.0f * r * 1.6f, oy = (rnd() % 3 == 0) ? 3.0f : float(int(rnd() % 100) - 50) / 50.0f;
            wr(E + 0x1c0, bits(vpos[0] + ox)); wr(E + 0x1c4, bits(vpos[1] + oy)); wr(E + 0x1c8, bits(vpos[2] + oz)); wr(E + 0x1cc, 0);
            wr(T + 4 + 4 * i, E);
        }
        rx_regs in = regs(); in.r[7] = T;
        const uint32_t a[2] = {act, player};
        call(0x00585330, in, 2, a);
    }
    g_m->natives.erase(stub);
}
}  // namespace

RX_PORT(0x0084ed20, MiscProbeHook, "thunk", 0) {
    static const char* groups = std::getenv("RX_MISC_PROBE");
    if (!groups || !*groups) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_MISC_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_MISC_PROBE_FRAME"))) : 200;
    static const uint32_t late = std::getenv("RX_MISC_PROBE_LATE") ? uint32_t(std::atoi(std::getenv("RX_MISC_PROBE_LATE"))) : 0;
    static bool seeded = false;
    if (!seeded) { seeded = true; if (const char* s = std::getenv("RX_MISC_PROBE_SEED")) g_rng ^= uint32_t(std::atoi(s)) * 2654435761u; }
    ++g_frame;
    const bool early = g_frame == at, last = late && g_frame == late;
    if (!early && !last) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    g_m = &m;
    if (early) {
        if (std::strchr(groups, 'm')) probe_map();
        if (std::strchr(groups, 'l')) probe_lights();
        if (std::strchr(groups, 'p')) probe_players(false);
        if (std::strchr(groups, 's')) probe_attrs();
        if (std::strchr(groups, 'r')) probe_records();
        if (std::strchr(groups, 'o')) probe_podium();
        if (std::strchr(groups, 'd')) probe_discoveries();
    }
    if (last && std::strchr(groups, 'p')) probe_players(true);
    saved.restore(m);
    std::fprintf(stderr, "[miscprobe] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
