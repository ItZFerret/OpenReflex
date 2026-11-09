// race_wave_probe.cpp -- the oracle's probe for the functions of the 2026-10-26 "w-race" wave that the five reference worlds do not
// reach (development only). A frame hook (Engine::Update 0084ed20, outside every verified call) runs the selected CASES; each case
// builds its arguments in the game heap / on the live objects and calls the ORIGINAL address, so with the ports installed Verify
// runs the core function and then the original on the same state and compares memory and the declared registers exactly as for a
// call from the game. Every case runs on a snapshot of the whole guest memory and machine state, restored afterwards (the run
// continues on the untouched world).
//   REFLEX_PORT=rx_race,...,rx_WaveProbeHook REFLEX_PORT_VERIFY=1 RX_WV_PROBE=<case>[,<case>...] [RX_WV_FRAME=<n>] <world run>
// No game logic here: only argument construction and the snapshot.
#include "oracle.h"
extern "C" {
#include "../game/build_tables.h"
#include "../game/build_vehicle.h"
}
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
using rx_oracle::f32;

namespace {
reflex_vm::Machine* g_m = nullptr;
uint32_t g_frame = 0, g_rng = 0x2545f491u;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
// a game-heap block (ReflexAlignedAlloc 0078fbe0), zeroed
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


// ---- logging stubs: fake objects whose virtuals are natives that append {id, this, arg0, arg1} to a log in guest memory ---------------
// (the log is part of the compared memory: the core run and the original run must call the same virtuals with the same arguments)
uint32_t g_log = 0;                    // the guest address of the log: +0 count, then 16-byte records
uint32_t g_stub_next = 0x00400800;     // stub addresses: the PE header page, never executed by the game
std::vector<uint32_t> g_stubs;
uint32_t make_stub(uint32_t id, int nargs, uint32_t ret = 0) {
    const uint32_t a = g_stub_next; g_stub_next += 4;
    g_m->natives[a] = [id, nargs, ret](reflex_vm::Machine& mm) -> uint32_t {
        const uint32_t n = uint32_t(mm.mem.read(g_log, 4));
        const uint32_t rec = g_log + 16 + 16 * n;
        mm.mem.write(rec, 4, id); mm.mem.write(rec + 4, 4, mm.r[1]);
        mm.mem.write(rec + 8, 4, nargs > 0 ? uint32_t(mm.arg(0)) : 0); mm.mem.write(rec + 12, 4, nargs > 1 ? uint32_t(mm.arg(1)) : 0);
        mm.mem.write(g_log, 4, n + 1);
        mm.r[0] = ret;
        return mm.ret(4u * uint32_t(nargs));
    };
    g_stubs.push_back(a);
    return a;
}
void stubs_begin() { g_log = halloc(0x1000); }
void stubs_end() { for (uint32_t a : g_stubs) g_m->natives.erase(a); g_stubs.clear(); g_stub_next = 0x00400800; }
// an object of the given size with a vtable whose listed slots (byte offset, stack arguments, return value) are stubs
uint32_t fake_object(uint32_t size, std::initializer_list<std::array<uint32_t, 3>> slots) {
    const uint32_t obj = halloc(size), vt = halloc(0x100);
    wr(obj, vt);
    for (const auto& sl : slots) wr(vt + sl[0], make_stub(sl[0], int(sl[1]), sl[2]));
    return obj;
}

// the local human player's slot and activity
uint32_t find_player() {
    const uint32_t g = rd(0x00a946b8);
    if (!g) return 0;
    for (uint32_t s = 0; s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) == 0 || rd(p + 0x10) != 0) continue;   /* a local human */
        return p;
    }
    return 0;
}
uint32_t find_activity() { const uint32_t p = find_player(); return p ? rd(p + 0x94) : 0; }

// ---- cases -------------------------------------------------------------------------------------------------------------------
// 00585a40 std::set<int>::find (EDI = set; out iterator, key*; RET 8): trees of 0..9 keys, keys below / inside / above / absent
void case_set_find() {
    for (uint32_t n = 0; n <= 9; n++) {
        const uint32_t set = halloc(0x40), head = halloc(0x20), proxy = halloc(0x10);
        wr(set, proxy); wr(set + 0x18, head); wr(set + 0x1c, n);
        wr(head + 0x11, 1);
        std::vector<int32_t> keys;
        for (uint32_t i = 0; i < n; i++) keys.push_back(int32_t(i * 3 + 1));
        std::vector<uint32_t> nodes;
        for (uint32_t i = 0; i < n; i++) { const uint32_t nd = halloc(0x20); wr(nd + 0xc, uint32_t(keys[i])); nodes.push_back(nd); }
        // a degenerate chain (right-leaning) is a valid binary search tree; use a balanced shape for n >= 3
        std::function<uint32_t(int, int, uint32_t)> build = [&](int lo, int hi, uint32_t parent) -> uint32_t {
            if (lo > hi) return head;
            const int mid = (lo + hi) / 2;
            const uint32_t nd = nodes[mid];
            wr(nd + 4, parent);
            wr(nd, build(lo, mid - 1, nd));
            wr(nd + 8, build(mid + 1, hi, nd));
            return nd;
        };
        const uint32_t root = n ? build(0, int(n) - 1, head) : head;
        wr(head + 4, root); wr(head, n ? nodes.front() : head); wr(head + 8, n ? nodes.back() : head);
        for (int32_t k = -2; k <= int32_t(3 * n + 3); k++) {
            const uint32_t out = halloc(0x10), key = halloc(0x10);
            wr(key, uint32_t(k));
            rx_regs in = regs(); in.r[7] = set;
            const uint32_t a[2] = {out, key};
            call(0x00585a40, in, 2, a);
        }
    }
}
// 00559da0 (ECX = the activity): the live activity, and with each driver's player flagged non-local in turn
void case_has_local_human() {
    const uint32_t act = find_activity();
    if (!act) { std::fprintf(stderr, "[wv] no activity\n"); return; }
    rx_regs in = regs(); in.r[1] = act;
    call(0x00559da0, in);
    const int n = int(rd(act + 0x26c));
    for (int i = 0; i < n; i++) {   // all drivers non-local except driver i
        const uint32_t d = rd(rd(act + 0x268) + 4 * i);
        wr(rd(d + 0x74) + 0x10, 0);
        for (int j = 0; j < n; j++) if (j != i) wr(rd(rd(rd(act + 0x268) + 4 * j) + 0x74) + 0x10, 1);
        call(0x00559da0, in);
    }
    for (int j = 0; j < n; j++) wr(rd(rd(rd(act + 0x268) + 4 * j) + 0x74) + 0x10, 1);
    call(0x00559da0, in);
    wr(act + 0x26c, 0);
    call(0x00559da0, in);
}
// 004fead0 / 004f3320 / 004faae0: the vector helpers
void case_vector_helpers() {
    for (uint32_t n : {0u, 1u, 3u, 8u, 100u, 1000u}) {
        const uint32_t a[1] = {n};
        const rx_regs o = call(0x004fead0, regs(), 1, a);
        if (o.r[0]) { const uint32_t f[1] = {o.r[0]}; call(0x004f3320, regs(), 1, f); }
    }
    for (int32_t n : {0, 1, 2, 5, 17, -3}) {
        const uint32_t src = halloc(0x100), dst = halloc(0x100);
        for (uint32_t i = 0; i < 0x40; i++) wr(src + 4 * i, 0x1000u + i);
        rx_regs in = regs();
        in.r[7] = src; in.r[0] = src + uint32_t(n * 4); in.r[1] = dst + 0x80;
        call(0x004faae0, in);
    }
}
// 007556d0 the motocard post (the session manager 0xda6ca0): a few payloads
void case_post_motocard() {
    const uint32_t mgr = rd(0x00da6ca0);   // the record manager the game keeps in [0xda6ca0]
    if (!mgr || !mapped(mgr + 0x40, 0x20)) { std::fprintf(stderr, "[wv] no session manager\n"); return; }
    for (uint32_t t = 0; t < 3; t++) {
        const uint32_t a[4] = {mgr, 0x11110000u + t, 0x22220000u + t, 0x33330000u + t};
        call(0x007556d0, regs(), 4, a);
    }
}
// Fn_00756700 the level config's load progress: counts 0..2 (the array +0x618 holds two floats before the count at +0x620; 0: 0 / 0), floats incl. specials
void case_load_progress() {
    for (int32_t n : {0, 1, 2}) {
        const uint32_t cfg = halloc(0x2200);
        wr(cfg + 0x620, uint32_t(n));
        for (uint32_t i = 0; i < 0x100 && i < uint32_t(n); i++) wr(cfg + 0x618 + 4 * i, bits(float(rnd() & 0xffff) / 65536.0f));
        wr(cfg + 0x20a0, bits(float(rnd() & 0xffff) / 65536.0f));
        rx_regs in = regs(); in.r[2] = cfg;
        call(0x00756700, in);
    }
    const uint32_t cfg = halloc(0x2200);
    wr(cfg + 0x620, 2); wr(cfg + 0x618, 0x7f800000u); wr(cfg + 0x61c, 0xff800000u);   // +inf + -inf = NaN
    wr(cfg + 0x20a0, bits(0.5f));
    rx_regs in = regs(); in.r[2] = cfg;
    call(0x00756700, in);
}


// a C string on the game heap
uint32_t hstr(const char* t) {
    const uint32_t n = uint32_t(std::strlen(t)) + 1;
    const uint32_t p = halloc((n + 3) & ~3u);
    for (uint32_t i = 0; i < n; i++) rx_wr8(p + i, uint8_t(t[i]));
    return p;
}
// 005dc0c0 UIProperty_GetInt (EAX = name; ui, &out; RET 8) over a fake UI manager (the property map at +0x134) and the live one
void case_ui_property() {
    const uint32_t buckets_n[3] = {1, 3, 7};
    for (uint32_t bn : buckets_n) {
        const uint32_t ui = halloc(0x200), buckets = halloc(4 * bn);
        wr(ui + 0x134, bn); wr(ui + 0x134 + 0xc, buckets);
        struct E { const char* name; uint32_t type; uint32_t value; const char* text; };
        const E es[] = {
            {"Alpha", 1, 42, ""}, {"beta", 2, 0, "123abc"}, {"Gamma", 3, bits(7.9f), ""}, {"delta", 3, bits(-2.5f), ""},
            {"Epsilon", 3, bits(3.0e9f), ""}, {"Zeta", 3, 0x7fc00000u, ""}, {"Eta", 4, 99, ""}, {"Theta", 2, 0, "-77"},
            {"Iota", 1, 0xfffffff0u, ""}, {"Kappa", 2, 0, ""},
        };
        for (const E& e : es) {
            const uint32_t name = hstr(e.name), rec = halloc(0x30), node = halloc(0x10);
            wr(rec, e.value); wr(rec + 0x24, e.type);
            for (uint32_t i = 0; e.text[i]; i++) rx_wr8(rec + 4 + i, uint8_t(e.text[i]));
            rx_regs hi = regs(); hi.r[0] = name;
            const uint32_t h = call(0x005c3400, hi).r[0];
            wr(node, name); wr(node + 4, rec); wr(node + 8, h);
            const uint32_t b = buckets + 4 * (h % bn);
            wr(node + 0xc, rd(b)); wr(b, node);
        }
        const char* q[] = {"Alpha", "ALPHA", "Beta", "gamma", "DELTA", "epsilon", "ZETA", "eta", "theta", "IOTA", "kappa", "missing", "", "Alph"};
        for (const char* n : q) {
            const uint32_t out = halloc(0x10);
            wr(out, 0xdeadbeefu);
            rx_regs in = regs(); in.r[0] = hstr(n);
            const uint32_t a[2] = {ui, out};
            call(0x005dc0c0, in, 2, a);
        }
    }
    if (const uint32_t ui = rd(0x00a93fcc)) {   // the live UI manager's store
        const char* q[] = {"InCareerMode", "WaitForUI", "EventLocationId", "PlayerMode", "EventType", "AIDifficulty", "EventRiders", "nonexistent"};
        for (const char* n : q) {
            const uint32_t out = halloc(0x10);
            wr(out, 0xdeadbeefu);
            rx_regs in = regs(); in.r[0] = hstr(n);
            const uint32_t a[2] = {ui, out};
            call(0x005dc0c0, in, 2, a);
        }
    } else std::fprintf(stderr, "[wv] no live UI manager\n");
}
// 005e0270 the language lookup over a fake table: ids, the unknown id (the default entry), no table, no data
void case_ui_language() {
    const uint32_t bn = 5;
    const uint32_t holder = halloc(0x10), table = halloc(0x80), data = halloc(0x200), buckets = halloc(4 * bn), entries = halloc(0x1c * 6);
    wr(holder, table); wr(table + 0x44, data);
    wr(data + 0x14, entries); wr(data + 0x18, data + 0x100);
    wr(data + 0x100, bn); wr(data + 0x100 + 0xc, buckets);
    const char* ids[] = {"STR_WINS", "STR_LOSES", "STR_ONLINE", "STR_X", "STR_Y"};
    for (uint32_t i = 0; i < 5; i++) {
        const uint32_t key = call(0x005e1190, regs(), 1, (const uint32_t[]){hstr(ids[i])}).r[0];
        rx_regs hi = regs(); hi.r[1] = key;
        const uint32_t h = call(0x005c3340, hi).r[0];
        const uint32_t node = halloc(0x10);
        wr(node, key); wr(node + 4, i); wr(node + 8, h);
        const uint32_t b = buckets + 4 * (h % bn);
        wr(node + 0xc, rd(b)); wr(b, node);
        wr(entries + 0x1c * i + 0x10, 0x1000u + i);
    }
    for (const char* n : {"STR_WINS", "STR_LOSES", "STR_ONLINE", "STR_X", "STR_Y", "STR_UNKNOWN", ""}) {
        rx_regs in = regs(); in.r[0] = hstr(n); in.r[7] = holder;
        call(0x005e0270, in);
    }
    wr(table + 0x44, 0);
    { rx_regs in = regs(); in.r[0] = hstr("STR_WINS"); in.r[7] = holder; call(0x005e0270, in); }
    wr(holder, 0);
    { rx_regs in = regs(); in.r[0] = hstr("STR_WINS"); in.r[7] = holder; call(0x005e0270, in); }
    for (int32_t v : {0, 1, -1, 0x12345678, int32_t(0x80000000u), 77}) {
        rx_regs hi = regs(); hi.r[1] = uint32_t(v);
        call(0x005c3340, hi);
        const uint32_t a[1] = {uint32_t(v ^ 1)};
        call(0x005c3370, hi, 1, a);
    }
}
// 0054d590 a free ride discovery recorded in the profile (the live player; its profile number forced to 0 / 1). The table
// "PlayerCollectables" of the profile database gets a row for the session's EventLocationId (appended with the core's row kernel)
// so the flag update itself runs.
void case_collectable() {
    const uint32_t p = find_player();
    if (!p) { std::fprintf(stderr, "[wv] no player\n"); return; }
    uint32_t loc = 0xffffffffu;
    {
        const uint32_t out = halloc(0x10);
        wr(out, 0xffffffffu);
        rx_regs in = regs(); in.r[0] = hstr("EventLocationId");
        const uint32_t a[2] = {rd(0x00a93fcc), out};
        call(0x005dc0c0, in, 2, a);
        loc = rd(out);
    }
    std::fprintf(stderr, "[wv] collectable: EventLocationId %d\n", int(loc));
    for (uint32_t num : {0xffffffffu, 0u, 1u}) {
        wr(p + 0x1f0, num);
        if (num != 0xffffffffu) {
            const uint32_t table = rx_tbl_find(0x00ae10f0u + num * 0x1f4u, 0x009416c0);
            std::fprintf(stderr, "[wv] collectable: profile %u table %08x\n", num, table);
            if (table && loc != 0xffffffffu) {
                const uint32_t nc = (rd(table + 0x14) - rd(table + 0x10)) / 4;
                const uint32_t vals = halloc(4 * (nc ? nc : 1));
                bool ok = true;
                for (uint32_t c = 0; c < nc; c++) {
                    const uint32_t col = rd(rd(table + 0x10) + 4 * c);
                    const uint32_t type = rd(col + 0x2c);
                    const uint32_t cell = halloc(0x20);
                    if (type != 1 && type != 2) { ok = false; break; }
                    char name[0x21] = {};
                    for (int i = 0; i < 0x20; i++) name[i] = char(rx_rd8(col + i));
                    if (std::strcmp(name, "EventLocationId") == 0) wr(cell, loc);
                    wr(vals + 4 * c, cell);
                }
                if (ok) rx_tbl_append_values_row(table, vals);
                else std::fprintf(stderr, "[wv] collectable: unsupported column type\n");
            }
        }
        for (uint32_t idx : {0u, 3u, 31u, 37u}) {
            const uint32_t a[2] = {p, idx};
            call(0x0054d590, regs(), 2, a);
        }
    }
}

// ---- the player's vehicle / rider release (build_activity_clear.c) ----
const uint32_t kGame = 0x00a946b8;
uint32_t game() { return rd(kGame); }
uint32_t veh_list() { return game() + 0x154b0; }
uint32_t rider_mgr() { return game() + 0x15504; }
// a first spec part with packages, and a first rider entry gear part
uint32_t first_part() {
    const uint32_t L = veh_list();
    for (int i = 0; i < int(rd(L + 0x2c)); i++) {
        const uint32_t S = rd(rd(L + 0x28) + 4 * i);
        for (uint32_t k = 0; S && k < 0x11; k++) if (rd(S + 0xc + 4 * k)) return rd(S + 0xc + 4 * k);
    }
    return 0;
}
uint32_t first_gear() {
    const uint32_t G = rider_mgr();
    for (int i = 0; i < int(rd(G + 0x18)); i++) {
        const uint32_t E = rd(rd(G + 0x14) + 4 * i);
        for (uint32_t k = 0; E && k < 8; k++) if (rd(E + 0x1c + 4 * k)) return rd(E + 0x1c + 4 * k);
    }
    return 0;
}
// 0054a500 the whole release of the live player's vehicle and rider
void case_clear_all() {
    const uint32_t P = find_player();
    if (!P) { std::fprintf(stderr, "[wv] no player\n"); return; }
    std::fprintf(stderr, "[wv] clear_all: player %08x vehicle %08x rider %08x entry %08x spec-state %d act %08x\n", P, rd(P + 0x1ac), rd(P + 0x1b4),
                 rd(P + 0x1d8), int(rd(P + 0x1d4)), rd(P + 0x94));
    const uint32_t a[1] = {P};
    call(0x0054a500, regs(), 1, a);
}
// the same with the release step by step on the live objects (each original on its own snapshot-free chain; the snapshot is the case's)
void case_clear_steps() {
    const uint32_t P = find_player();
    if (!P) { std::fprintf(stderr, "[wv] no player\n"); return; }
    const uint32_t V = rd(P + 0x1ac), L = veh_list(), G = rider_mgr();
    std::fprintf(stderr, "[wv] clear_steps: player %08x vehicle %08x list %08x rider mgr %08x\n", P, V, L, G);
    // small records
    { rx_regs in = regs(); in.r[2] = V; call(0x00408fe0, in); }
    { rx_regs in = regs(); in.r[2] = V; call(0x0047c380, in); }
    { rx_regs in = regs(); in.r[2] = V; call(0x00691d30, in); }
    for (uint32_t m : {0x00c11bd0u, 0x00c11d80u}) { rx_regs in = regs(); in.r[7] = m; call(0x0069a0c0, in); }
    { const uint32_t a[1] = {P}; call(0x00550e10, regs(), 1, a); }
    // the vehicle audio slot and the damage record
    { rx_regs in = regs(); in.r[1] = rd(0x00d92f00); in.r[2] = V; const uint32_t a[1] = {rd(0x00d92f00)}; call(0x00680d10, in, 1, a); }
    { rx_regs in = regs(); in.r[2] = rd(0x0047bb40) ? 0 : 0; (void)in; }
    // the pointer vector helper on a synthetic vector
    for (int t = 0; t < 7; t++) {
        const uint32_t vec = halloc(0x10), data = halloc(0x40), valp = halloc(8);
        const uint32_t n = (t == 6) ? 0 : 4;
        for (uint32_t i = 0; i < n; i++) wr(data + 4 * i, 0x100u * (i + 1));
        wr(vec, data); wr(vec + 4, n);
        wr(valp, t < 4 ? 0x100u * (uint32_t(t) + 1) : 0x999u);
        rx_regs in = regs(); in.r[1] = vec; in.r[2] = valp;
        call(0x00544c80, in);
    }
    // the vehicle's spec parts
    if (const uint32_t part = first_part()) {
        const uint32_t loader = rd(0x00d92ef0);
        for (uint32_t hr : {0u, 1u}) { rx_regs in = regs(); in.r[1] = part; const uint32_t a[2] = {loader, hr}; call(0x00540450, in, 2, a); }
        { rx_regs in = regs(); in.r[1] = part; const uint32_t a[1] = {loader}; call(0x00540300, in, 1, a); }
        for (uint32_t force_hr : {0u, 1u}) {   // the high-resolution flag forced so the "HR_" name paths run
            wr(part + 0x38, force_hr);
            { rx_regs in = regs(); in.r[1] = part; const uint32_t a[2] = {loader, 1}; call(0x00540450, in, 2, a); }
            { rx_regs in = regs(); in.r[1] = part; const uint32_t a[1] = {loader}; call(0x00540300, in, 1, a); }
        }
        { rx_regs in = regs(); in.r[0] = part; call(0x0053f9f0, in); }
    } else std::fprintf(stderr, "[wv] no vehicle part\n");
    // the rider's gear
    if (const uint32_t gear = first_gear()) {
        const uint32_t loader = rd(0x00d92ef4);
        for (uint32_t force_hr : {0u, 1u}) {
            wr(gear + 0x38, force_hr);
            { rx_regs in = regs(); in.r[1] = gear; const uint32_t a[2] = {loader, 1}; call(0x0053d170, in, 2, a); }
            { rx_regs in = regs(); in.r[1] = gear; const uint32_t a[1] = {loader}; call(0x0053d020, in, 1, a); }
        }
        { rx_regs in = regs(); in.r[0] = gear; call(0x0053ca30, in); }
    } else std::fprintf(stderr, "[wv] no gear part\n");
}
// the spec / vehicle / rider releases, each on the live objects
void case_clear_spec() {
    const uint32_t P = find_player();
    if (!P) return;
    const uint32_t L = veh_list();
    rx_regs in = regs(); in.r[0] = rd(P + 0x1b0);
    const uint32_t a[1] = {L};
    std::fprintf(stderr, "[wv] clear_spec: vehicle id %u specs %d\n", rd(P + 0x1b0), int(rd(L + 0x2c)));
    call(0x00540dd0, in, 1, a);
}
void case_clear_vehicle() {
    const uint32_t P = find_player();
    if (!P) return;
    rx_regs in = regs(); in.r[0] = rd(P + 0x1ac); in.r[7] = veh_list();
    call(0x005408e0, in);
}
void case_clear_rider_release() {
    const uint32_t P = find_player();
    if (!P) return;
    const uint32_t G = rider_mgr();
    std::fprintf(stderr, "[wv] clear_rider: rider %08x rider id %u entries %d\n", rd(P + 0x1b4), rd(P + 0x1b8), int(rd(G + 0x18)));
    { rx_regs in = regs(); in.r[0] = G; const uint32_t a[1] = {0x4242}; call(0x0053ece0, in, 1, a); }
    rx_regs in = regs(); in.r[0] = G; const uint32_t a[1] = {rd(P + 0x1b8)};
    call(0x0053ece0, in, 1, a);
}
void case_clear_rider_entry() {
    const uint32_t G = rider_mgr();
    if (!rd(G + 0x18)) return;
    rx_regs in = regs(); in.r[7] = G; const uint32_t a[1] = {rd(rd(G + 0x14))};
    call(0x0053ed20, in, 1, a);
}
void case_clear_rider_remove() {
    const uint32_t P = find_player();
    if (!P || !rd(P + 0x1b4)) return;
    rx_regs in = regs(); in.r[0] = rd(P + 0x1b4); in.r[7] = rider_mgr();
    call(0x0053ebc0, in);
}
void case_clear_player_objects() {
    const uint32_t P = find_player();
    if (!P) return;
    rx_regs in = regs(); in.r[6] = P;
    call(0x0054bda0, in);
}


// 0047bbe0 / 00479dd0: a damage manager entry of the live vehicle with fake parts (logging stubs)
void case_damage() {
    const uint32_t P = find_player();
    if (!P) return;
    const uint32_t V = rd(P + 0x1ac);
    rx_regs g = regs();
    const uint32_t D = call(0x0047bb40, g).r[0];
    std::fprintf(stderr, "[wv] damage: manager %08x vehicle %08x\n", D, V);
    if (!D || !V) return;
    stubs_begin();
    const uint32_t handle = halloc(0x80);
    wr(handle + 0x60, 0xabc12345u);
    const uint32_t engine = rd(0x00a95844);
    const uint32_t fake_db = fake_object(0x20, {{0x58, 2, 0}});
    wr(engine + 8, fake_db);
    for (uint32_t variant = 0; variant < 3; variant++) {
        const uint32_t slot = variant, E = D + 0x10 + 0x410 * slot;
        wr(D + 0x40c + 0x410 * slot, V);   // the entry's vehicle
        const uint32_t count = variant == 0 ? 0 : (variant == 1 ? 3 : 2);
        wr(E, count);
        for (uint32_t i = 0; i < count; i++) {
            const uint32_t part = i == 1 && variant == 1 ? 0 : fake_object(0x40, {{0x00, 1, 0}, {0x24, 0, 0}, {0x5c, 0, handle}});
            wr(E + 0x2c + 4 * i, part);
            wr(E + 0x370 + 4 * i, 0);          // no node: the deactivate step is skipped
            rx_wr8(E + 0x3d0 + i, 0);
            const uint32_t sub = fake_object(0x10, {{0x04, 0, 0}});
            wr(E + 0x4c + 8 * i, rd(sub));     // the sub-object: {vtable, data}
        }
        const uint32_t keep = rd(D + 0x40c + 0x410 * slot);
        (void)keep;
    }
    // the manager call finds the entry of the vehicle (the first match) ...
    { rx_regs in = regs(); in.r[2] = D; in.r[6] = V; call(0x0047bbe0, in); }
    // ... the entry release on each of the prepared entries ...
    for (uint32_t slot = 0; slot < 3; slot++) { rx_regs in = regs(); in.r[1] = D + 0x10 + 0x410 * slot; call(0x00479dd0, in); }
    // ... and a vehicle without an entry
    { rx_regs in = regs(); in.r[2] = D; in.r[6] = 0x12345678; call(0x0047bbe0, in); }
    stubs_end();
}
// 0054bda0 with fake current / start-gate / extra objects
void case_player_objects_fake() {
    const uint32_t P = find_player();
    if (!P) return;
    stubs_begin();
    wr(P + 0x9a8, fake_object(0x40, {{0x74, 0, 0}}));
    wr(P + 0x220, fake_object(0x40, {{0x00, 1, 0}, {0x24, 0, 0}}));
    wr(P + 0x998, fake_object(0x40, {{0x00, 1, 0}, {0x24, 0, 0}}));
    wr(P + 0x99c, fake_object(0x40, {{0x00, 1, 0}, {0x24, 0, 0}}));
    rx_regs in = regs(); in.r[6] = P;
    call(0x0054bda0, in);
    stubs_end();
}
// the variants of 0054a500: no vehicle (the pending spec), a pending rider entry, a global table entry of the vehicle
void case_clear_variants() {
    const uint32_t P = find_player();
    if (!P) return;
    const uint32_t V = rd(P + 0x1ac);
    const uint32_t a[1] = {P};
    wr(0x00ad73b4 + 0x20 * 3, V);                               // the global vehicle table entry
    call(0x0054a500, regs(), 1, a);
}
void case_clear_variants2() {   // no vehicle, the spec pending (+0x1d4), the rider entry pending (+0x1d8 with the rider id)
    const uint32_t P = find_player();
    if (!P) return;
    wr(P + 0x1ac, 0); wr(P + 0x1b4, 0); wr(P + 0x1d8, 1); wr(P + 0x1d4, 1);
    const uint32_t a[1] = {P};
    call(0x0054a500, regs(), 1, a);
}
// the table-driven tails of the spec / rider releases: the Player List emptied first so the ids are unreferenced
void erase_player_rows() {
    const uint32_t table = 0x00ae0e08;
    for (int r = int(rx_tbl_row_count(table)) - 1; r >= 0; r--) {
        const uint32_t a[2] = {table, uint32_t(r)};
        call(0x004ece50, regs(), 2, a);
    }
}
void case_clear_spec2() {
    const uint32_t P = find_player();
    if (!P) return;
    erase_player_rows();
    rx_regs in = regs(); in.r[0] = rd(P + 0x1b0);
    const uint32_t a[1] = {veh_list()};
    call(0x00540dd0, in, 1, a);
    rx_regs in2 = regs(); in2.r[0] = 0x4242;   // an unknown id
    call(0x00540dd0, in2, 1, a);
}
void case_clear_rider2() {
    const uint32_t P = find_player();
    if (!P) return;
    const uint32_t G = rider_mgr();
    erase_player_rows();
    { rx_regs in = regs(); in.r[7] = G; const uint32_t a[1] = {rd(rd(G + 0x14))}; call(0x0053ed20, in, 1, a); }
    { rx_regs in = regs(); in.r[7] = G; const uint32_t a[1] = {0}; call(0x0053ed20, in, 1, a); }
    { rx_regs in = regs(); in.r[0] = G; const uint32_t a[1] = {0x4242}; call(0x0053ece0, in, 1, a); }
}
void case_unknown_vehicle() {   // the searches that find nothing
    const uint32_t V = 0x12345678;
    { rx_regs in = regs(); in.r[2] = V; call(0x00408fe0, in); }
    { rx_regs in = regs(); in.r[2] = V; call(0x0047c380, in); }
    { rx_regs in = regs(); in.r[2] = V; call(0x00691d30, in); }
    { rx_regs in = regs(); in.r[1] = rd(0x00d92f00); in.r[2] = V; const uint32_t a[1] = {rd(0x00d92f00)}; call(0x00680d10, in, 1, a); }
}


// pending parts / gear (+0x50 / +0x3c slots) sharing a part with the main slot, an unknown id first (the search mismatch)
void case_clear_pending() {
    const uint32_t P = find_player();
    if (!P) return;
    const uint32_t L = veh_list(), G = rider_mgr();
    const uint32_t S = rd(rd(L + 0x28));
    if (S) {
        for (uint32_t k = 0; k < 0x11; k++) if (rd(S + 0xc + 4 * k)) { const uint32_t part = rd(S + 0xc + 4 * k); wr(S + 0x50 + 4 * k, part); wr(part + 0x44, 2); break; }
        rx_regs in = regs(); in.r[0] = 0x4242;
        const uint32_t a[1] = {L};
        call(0x00540dd0, in, 1, a);
        rx_regs in2 = regs(); in2.r[0] = rd(S + 4);
        call(0x00540dd0, in2, 1, a);
    }
    const uint32_t E = rd(G + 0x18) ? rd(rd(G + 0x14)) : 0;
    if (E) {
        uint32_t gear = 0;
        for (uint32_t k = 0; k < 8 && !gear; k++) gear = rd(E + 0x1c + 4 * k);
        if (gear) { wr(E + 0x3c, gear); wr(gear + 0x48, 2); }
        rx_regs in = regs(); in.r[7] = G;
        const uint32_t a[1] = {E};
        call(0x0053ed20, in, 1, a);
    }
    { rx_regs in = regs(); in.r[0] = G; const uint32_t a[1] = {0x4242}; call(0x0053ece0, in, 1, a); }
}
// the free ride's reset hook of 0054a500 (kind 8, state 2, no session 0x20018), a rider that is not in the made list / has no entry
void case_clear_freeride() {
    const uint32_t P = find_player();
    if (!P) return;
    const uint32_t act = rd(P + 0x94);
    wr(act + 0x23c, 8); wr(act + 0x21c, 2); wr(rd(act + 0x234) + 0x20018, 0);
    const uint32_t a[1] = {P};
    call(0x0054a500, regs(), 1, a);
}
void case_clear_rider3() {   // a rider without an entry / not in the made list: a fake rider object (stub virtuals)
    const uint32_t G = rider_mgr();
    stubs_begin();
    const uint32_t rider = fake_object(0x5b600, {{0x00, 1, 0}, {0x24, 0, 0}});
    wr(rider + 0x10, 0);                       // no scene
    wr(rider + 0x5b51c, 0x4242);               // an unknown rider id
    const uint32_t entry = halloc(0x40);       // the +0x18 write of a missing entry goes to a real record instead
    (void)entry;
    // the scene removal needs a scene: use the live one
    const uint32_t scene = rd(rd(rd(0x00a946b8) + 0x5a54) + 0x20138);
    wr(rider + 0x10, scene);
    rx_regs in = regs(); in.r[0] = rider; in.r[7] = G;
    // the original writes +0x18 of entry 0 when the id is absent: keep it harmless by giving the id an entry
    const uint32_t E = halloc(0x40); wr(E + 4, 0x4242);
    const uint32_t arr = rd(G + 0x14), n = rd(G + 0x18);
    wr(arr + 4 * n, E); wr(G + 0x18, n + 1);
    call(0x0053ebc0, in);
    stubs_end();
}


// a report of the virtual targets the release path dispatches to (the vehicle, rider and camera entities of the live player)
void case_vt_report() {
    const uint32_t P = find_player();
    if (!P) return;
    struct O { const char* name; uint32_t obj; std::initializer_list<uint32_t> slots; };
    const O objs[] = {
        {"vehicle", rd(P + 0x1ac), {0x00, 0x24, 0xc4}}, {"rider", rd(P + 0x1b4), {0x00, 0x24}},
        {"cam0", rd(P + 0x974), {0x00, 0x24}}, {"cam1", rd(P + 0x978), {0x00, 0x24}}, {"cam2", rd(P + 0x97c), {0x00, 0x24}},
        {"cam3", rd(P + 0x980), {0x00, 0x24}}, {"cam4", rd(P + 0x984), {0x00, 0x24}},
        {"quick", rd(P + 0x998), {0x00, 0x24}}, {"gate", rd(P + 0x99c), {0x00, 0x24}}, {"current", rd(P + 0x9a8), {0x74}}, {"x220", rd(P + 0x220), {0x00, 0x24}},
    };
    for (const O& o : objs) {
        if (!o.obj) { std::fprintf(stderr, "[wv] vt %s: null\n", o.name); continue; }
        const uint32_t vt = rd(o.obj);
        std::fprintf(stderr, "[wv] vt %s obj %08x vtable %08x:", o.name, o.obj, vt);
        for (uint32_t sl : o.slots) std::fprintf(stderr, " +%x=%08x", sl, rd(vt + sl));
        std::fprintf(stderr, "\n");
    }
}


// 0080b490 AudioEvent_ApplyVolume (EAX = ESI = event) and the 0x24 (invalid handle) branch: 0080a8e0 / 0080bde0 / 0080dcd0; 0080ae10 stop now
void case_audio_event() {
    auto make_event = [&](uint32_t handle, bool channel, float vol, uint32_t state, uint32_t flags) {
        const uint32_t ev = halloc(0x200);
        wr(ev + 0x118, handle); wr(ev + 0x11c, bits(vol)); wr(ev + 0x14, 77); wr(ev + 0x138, state); wr(ev + 0x13c, flags);
        for (const char* c = "TestEvent"; *c; c++) rx_wr8(ev + 0x18 + uint32_t(c - "TestEvent"), uint8_t(*c));
        if (channel) {
            const uint32_t ch = halloc(0x54);
            wr(ch, bits(0.5f)); wr(ch + 4, bits(0.75f)); wr(ch + 0x18, bits(0.9f));
            wr(ev + 0x1a0, ch);
        }
        return ev;
    };
    for (bool channel : {false, true}) {
        for (float v : {0.0f, 1.0f, 0.37f, f32(0x7fc00000u), f32(0x7f800000u)}) {
            rx_regs in = regs();
            in.r[0] = make_event(0xfb000010u, channel, v, 3, 0); in.r[6] = in.r[0];
            call(0x0080b490, in);
        }
    }
    { rx_regs in = regs(); in.r[0] = make_event(0, true, 0.5f, 3, 0); in.r[6] = in.r[0]; call(0x0080b490, in); }   // no instance
    // the invalid handle result: the stand-in reports 0x24
    g_m->natives[0x00900342] = [](reflex_vm::Machine& mm) -> uint32_t { mm.r[0] = 0x24; return mm.ret(8); };
    const uint32_t list = halloc(0x40);
    for (uint32_t state : {0u, 1u, 3u}) {
        const uint32_t ev = make_event(0xfb000020u, true, 0.8f, state, 5), other = halloc(0x10);
        wr(list, other); wr(list + 4, ev); wr(0x00d67d04, list); wr(0x00d67d08, 2);
        wr(ev + 0x144, 0x1f);
        rx_regs in = regs(); in.r[0] = ev; in.r[6] = ev;
        call(0x0080b490, in);
    }
    { const uint32_t ev = make_event(0xfb000030u, false, 0.8f, 3, 0); wr(ev + 0x144, 0x1e); rx_regs in = regs(); in.r[0] = ev; in.r[6] = ev; call(0x0080b490, in); }
    g_m->natives.erase(0x00900342);
    // 0080ae10: stop now (ESI = event)
    for (uint32_t state : {0u, 1u, 2u, 3u, 5u}) for (uint32_t flags : {0u, 8u}) for (uint32_t handle : {0u, 0xfb000040u}) {
        rx_regs in = regs(); in.r[6] = make_event(handle, false, 1.0f, state, flags); call(0x0080ae10, in);
    }
}


// ---- the online session code (race_net_session.c): the Quazal leaves replaced by logging natives at their original addresses ----------
std::vector<uint32_t> g_native_addrs;
// a native at `addr`: appends {id, ECX, arg0, arg1} to the log (arg0 dereferenced when deref0), returns `ret` in EAX (and `edx` in EDX), pops `pop` bytes
void native_at(uint32_t addr, uint32_t id, uint32_t pop, uint32_t ret, bool deref0 = false, uint32_t edx = 0, bool log_ecx = true, uint32_t nargs = 99) {
    g_m->natives[addr] = [id, pop, ret, deref0, edx, log_ecx, nargs](reflex_vm::Machine& mm) -> uint32_t {
        const uint32_t n = uint32_t(mm.mem.read(g_log, 4)), rec = g_log + 16 + 16 * n;
        const uint32_t na = nargs == 99 ? pop / 4 : nargs;
        const uint32_t a0 = na >= 1 ? uint32_t(mm.arg(0)) : 0, a1 = na >= 2 ? uint32_t(mm.arg(1)) : 0;
        mm.mem.write(rec, 4, id); mm.mem.write(rec + 4, 4, log_ecx ? mm.r[1] : 0);
        mm.mem.write(rec + 8, 4, deref0 && mm.mem.mapped(a0) ? uint32_t(mm.mem.read(a0, 4)) : a0); mm.mem.write(rec + 12, 4, a1);
        mm.mem.write(g_log, 4, n + 1);
        mm.r[0] = ret; mm.r[2] = edx;
        return mm.ret(pop);
    };
    g_native_addrs.push_back(addr);
}
void natives_end() { for (uint32_t a : g_native_addrs) g_m->natives.erase(a); g_native_addrs.clear(); }

// 00700a70 (EAX = N): the local value pointers stored into the four snapshot records
void case_net_store() {
    stubs_begin();
    for (int variant = 0; variant < 3; variant++) {
        const uint32_t N = halloc(0x200);
        for (uint32_t k = 0; k < 4; k++) wr(N + 0x128 + 4 * k, halloc(0x100));
        for (uint32_t off = 0xe8; off <= 0x124; off += 4) {
            if (variant == 1 && (off & 8)) continue;                         // some pointers unset
            const uint32_t cell = halloc(8);
            wr(cell, variant == 2 && off == 0xe8 ? 0x7f800000u : (off == 0xec ? 0xff800000u : 0x3f000000u + off));   // +inf / -inf / ordinary (the emulator's x87 keeps no NaN payloads)
            wr(N + off, cell);
        }
        rx_regs in = regs(); in.r[0] = N;
        call(0x00700a70, in);
    }
    stubs_end();
}
// 00700e70 (ESI = holder): element 3 of the ring copied into N
void case_net_load() {
    stubs_begin();
    for (int variant = 0; variant < 2; variant++) {
        const uint32_t N = halloc(0x200), H = halloc(8), rec = halloc(0x100), iter = halloc(8);
        wr(H, N); wr(iter, rec);
        for (uint32_t off = 0x7c; off <= 0xbc; off += 4) wr(rec + off, variant ? 0x7f800000u : 0x3f000000u + off);
        rx_wr8(rec + 0xb4, 0x5a);
        native_at(0x00701bf0, 1, 4, iter, false, 0, false);
        rx_regs in = regs(); in.r[6] = H;
        call(0x00700e70, in);
        natives_end();
    }
    stubs_end();
}
// 006fe870 / 006fe9c0: the apply (with and without a clock hook), the flag
void case_net_apply() {
    stubs_begin();
    for (int variant = 0; variant < 3; variant++) {
        const uint32_t O = halloc(0x300), N = halloc(0x200), H = halloc(8), rec = halloc(0x100), iter = halloc(8);
        wr(H, variant == 2 ? 0 : N); wr(iter, rec); wr(O + 0x1d4, H);
        std::fprintf(stderr, "[wv] net_apply v%d: O %08x N %08x H %08x rec %08x iter %08x log %08x\n", variant, O, N, H, rec, iter, g_log);
        for (uint32_t k = 0; k < 4; k++) wr(N + 0x128 + 4 * k, halloc(0x100));
        for (uint32_t off = 0xe8; off <= 0x124; off += 4) { const uint32_t cell = halloc(8); wr(cell, 0x3f800000u + off); wr(N + off, cell); }
        for (uint32_t off = 0x7c; off <= 0xbc; off += 4) wr(rec + off, 0x40000000u + off);
        rx_wr8(rec + 0xb4, 0x33);
        // the ring element comes from the record; N's replicated fields are written from it by 00700e70
        native_at(0x00701bf0, 1, 4, iter, false, 0, false);
        native_at(0x00884ba0, 2, 8, 0);
        if (variant == 1) { native_at(0x00700ff0, 3, 0, 0x1234, false, 0x5678, false); wr(0x009df898, 0x00700ff0); } else wr(0x009df898, 0);
        if (variant != 2) {
            rx_regs in = regs(); in.r[7] = O; call(0x006fe870, in);
        } else {   // N null: the store is skipped, the load reads the (null) holder -> N null: the original faults too, so skip
        }
        rx_regs in = regs(); in.r[0] = O; in.r[1] = 0x1ff; if (variant != 2) call(0x006fe9c0, in);
        natives_end();
    }
    stubs_end();
}
// 006ff8d0: the SendEvent message
void case_net_send() {
    stubs_begin();
    const uint32_t O = halloc(0x300), buffer = halloc(0x40);
    native_at(0x008932c0, 1, 4, 0, false, 0, false);
    native_at(0x00897d40, 2, 0, 0xabcd4321u, false, 0, false);
    native_at(0x008847b0, 3, 4, buffer, true);
    native_at(0x0088c5e0, 4, 0xc, 1, true);
    native_at(0x00886b10, 5, 8, 1);
    rx_regs in = regs(); in.r[7] = O;
    const uint32_t a[3] = {0x11111111u, 0x22222222u, 0x33333333u};
    call(0x006ff8d0, in, 3, a);
    natives_end();
    stubs_end();
}
// 0088bcd0 / 0088bac0 / 00885050: the handle
void case_quazal_handle() {
    stubs_begin();
    const uint32_t H = halloc(0x10), object = halloc(0x80), station = halloc(0x20), vt = halloc(0x80);
    wr(station, vt); wr(object + 0x50, 7u << 22);
    native_at(0x0088bb50, 1, 0, 0);
    native_at(0x0088b8f0, 2, 0, 0);
    native_at(0x00885310, 3, 0, 0);
    native_at(0x00884390, 4, 0, 0x10003b00u, false, 0, false, 4);
    native_at(0x008a32a0, 5, 0, station, true, 0, false, 1);
    for (int ok = 0; ok < 2; ok++) {
        native_at(0x00700ff4, 6, 4, ok ? 1u : 0u, false, 0, true, 1);
        wr(vt + 0x3c, 0x00700ff4);
        wr(0x009dfd9c, 0x9876);
        for (uint32_t obj : {0u, object}) {
            wr(H, obj);
            { rx_regs in = regs(); in.r[1] = H; call(0x00885050, in); }
        }
    }
    for (uint32_t obj : {0u, 0x00112233u}) {
        const uint32_t h = halloc(0x10);
        rx_regs in = regs(); in.r[1] = h; const uint32_t a[1] = {obj};
        call(0x0088bcd0, in, 1, a);
    }
    for (uint32_t flag : {0u, 1u}) for (uint32_t obj : {0u, object}) {
        const uint32_t h = halloc(0x10);
        wr(h, obj); rx_wr8(h + 8, uint8_t(flag));
        rx_regs in = regs(); in.r[1] = h; call(0x0088bac0, in);
    }
    natives_end();
    stubs_end();
}
// 006f7810: the recorder with a valid session, with an invalid one, without
void case_net_record() {
    stubs_begin();
    const uint32_t session = halloc(0x200);
    native_at(0x0088bb50, 1, 0, 0);
    native_at(0x0088b8f0, 2, 0, 0);
    native_at(0x00885310, 3, 0, 0);
    native_at(0x00555540, 4, 0, 0);
    for (int variant = 0; variant < 3; variant += 1) {
        // 00703070 (EAX = H) -> AL (variant 1: valid)
        g_m->natives[0x00703070] = [variant](reflex_vm::Machine& mm) -> uint32_t { mm.r[0] = variant == 1 ? 1u : 0u; return mm.ret(0); };
        g_native_addrs.push_back(0x00703070);
        // the handle constructor's lookup (0088bb50, ECX = H) gives the handle its object
        { const uint32_t obj = variant == 2 ? 0 : session;
          g_m->natives[0x0088bb50] = [obj](reflex_vm::Machine& mm) -> uint32_t { mm.mem.write(mm.r[1], 4, obj); return mm.ret(0); };
          g_native_addrs.push_back(0x0088bb50); }
        wr(0x00d60d54, variant == 2 ? 0 : session);
        // 00884950 (ECX = object) -> AL: the real original with the object's owner: make it "valid" by owner 0
        wr(session + 0x14, 0x5150); wr(0x009dfb98, 0);
        for (uint32_t st : {0u, 3u}) { const uint32_t a[1] = {st}; call(0x006f7810, regs(), 1, a); }
    }
    natives_end();
    stubs_end();
}
// 005544c0 / 00585b20 / 00508880: the selection iterators
void case_net_selection() {
    stubs_begin();
    const uint32_t filter = fake_object(0x20, {{0x08, 0, 0}});
    for (int variant = 0; variant < 2; variant++) {
        native_at(0x0088c380, 1, 8, 0);
        native_at(0x00884650, 2, 0, halloc(0x20), false, 0, false, 3);
        native_at(0x0088afe0, 3, 4, filter);
        native_at(0x0088c150, 4, 4, 0);
        native_at(0x0088c1e0, 6, 4, 0);
        native_at(0x0088bc40, 7, 4, 0, true);
        const uint32_t it = halloc(0x40);
        // 00508800 is a port now (the holder's std::set<uint> is walked in game memory): variant 1 gets an empty set; the stats lock is off
        uint32_t holder = 0;
        if (variant) {
            holder = halloc(0x60);
            const uint32_t head = halloc(0x20), proxy = halloc(0x10);
            wr(holder + 4, proxy); wr(proxy, holder + 4); wr(holder + 4 + 0x18, head); rx_wr8(head + 0x15, 1);
            wr(head, head); wr(head + 4, head); wr(head + 8, head);
        }
        wr(it, holder);
        rx_wr8(0x009df580, 1);
        wr(0x00a94348, 0x2222);
        { rx_regs in = regs(); in.r[6] = it; call(0x005544c0, in); }
        { rx_regs in = regs(); in.r[6] = it; call(0x00585b20, in); }
        { rx_regs in = regs(); in.r[1] = it; call(0x00508880, in); }
        natives_end();
    }
    stubs_end();
}


// 008953c0: the session state through the handle (the object and its station are fake; the Quazal leaves are natives)
void case_net_state() {
    stubs_begin();
    const uint32_t object = halloc(0x80), station = halloc(0x20), vt = halloc(0x80);
    wr(station, vt); wr(object + 0x50, 7u << 22);
    native_at(0x0088b8f0, 2, 0, 0);
    native_at(0x00885310, 3, 0, 0);
    native_at(0x00884390, 4, 0, 0x10003b00u, false, 0, false, 4);
    native_at(0x008a32a0, 5, 0, station, true, 0, false, 1);
    wr(0x009dfd9c, 0x9876); wr(0x009dfb98, 0);
    for (int variant = 0; variant < 4; variant++) {
        g_m->natives[0x00700ff4] = [variant](reflex_vm::Machine& mm) -> uint32_t { mm.r[0] = variant == 0 ? 0u : 1u; return mm.ret(4); };
        g_native_addrs.push_back(0x00700ff4);
        wr(vt + 0x3c, 0x00700ff4);
        const uint32_t obj = variant == 3 ? 0 : object;
        g_m->natives[0x0088bb50] = [obj](reflex_vm::Machine& mm) -> uint32_t { mm.mem.write(mm.r[1], 4, obj); return mm.ret(0); };
        g_native_addrs.push_back(0x0088bb50);
        wr(0x009df938, object);
        wr(object + 0x14, variant == 2 ? 0 : 0x5150);   // variant 2: no owner: not valid
        call(0x008953c0, regs());
    }
    natives_end();
    stubs_end();
}


// 004ebce0 std::set<RowRef>'s copy constructor over the live tables' index sets (non-empty and empty)
void case_rowset_copy() {
    int copied = 0;
    for (uint32_t table : {0x00ae0e08u, 0x00ae05c8u, 0x00ae19b0u, 0x00ae1b18u}) {
        const uint32_t nc = (rd(table + 0x14) - rd(table + 0x10)) / 4;
        for (uint32_t c = 0; c < nc && c < 12; c++) {
            const uint32_t col = rd(rd(table + 0x10) + 4 * c), idx = rd(col + 0x44);
            if (!idx || rd(col + 0x2c) != 1) continue;     // int-keyed searchable indexes (node isnil at +0x39)
            // every key of the column's searchable index: its set
            const uint32_t head = rd(idx + 0x18);
            uint32_t n = rd(head);
            for (int guard = 0; n != head && guard < 8 && n; guard++) {
                const uint32_t set = n + 0x10;
                const uint32_t dst = halloc(0x40);
                rx_regs in = regs(); in.r[7] = dst;
                const uint32_t a[1] = {set};
                call(0x004ebce0, in, 1, a);
                copied++;
                // the in-order successor
                uint32_t r = rd(n + 8);
                if (rx_rd8(r + 0x39) == 0) { while (rx_rd8(rd(r) + 0x39) == 0) r = rd(r); n = r; }
                else { uint32_t p = rd(n + 4); while (rx_rd8(p + 0x39) == 0 && n == rd(p + 8)) { n = p; p = rd(p + 4); } n = p; }
            }
        }
    }
    const uint32_t empty = halloc(0x40);   // an empty source set built by the original constructor path
    { const uint32_t dst = halloc(0x40); rx_regs in = regs(); in.r[7] = dst; const uint32_t a[1] = {empty}; (void)a; (void)in; }
    std::fprintf(stderr, "[wv] rowset_copy: %d sets copied\n", copied);
}


// 00479970 / 0047b960: a damage entry's tick (the spring 0047aff0 is a logging native)
void case_damage_update() {
    const uint32_t P = find_player();
    if (!P) return;
    const uint32_t V = rd(P + 0x1ac);
    stubs_begin();
    native_at(0x0047aff0, 9, 12, 0, false, 0, false);
    const uint32_t body_h = halloc(0x40), body = halloc(0x100);
    (void)body_h; (void)body;
    for (int variant = 0; variant < 6; variant++) {
        const uint32_t E = halloc(0x410), names = halloc(0x350 * 4), part = halloc(0x200);   // a part entity without a physics handle (a live handle would need a damage part's body)
        wr(E, 4); wr(E + 4, names);
        for (uint32_t i = 0; i < 4; i++) {
            wr(E + 0xc + 4 * i, i == 0 ? 2u : (i == 1 ? 3u : (i == 2 ? 4u : 0u)));
            wr(E + 0x2c + 4 * i, part);
            wr(E + 0x370 + 4 * i, i == 3 ? 0 : halloc(0x100));     // part nodes (zeroed fake nodes)
            const float timers[] = {-0.5f, 0.25f, -0.1f, 1.0f, -1.0f, 0.05f};
            wr(E + 0x3d8 + 4 * i, bits(timers[(variant + i) % 6]));
        }
        wr(E + 0x3fc, 0);      // (a set vehicle would reach the part's end: 0047afb0 on a real model node, not built here)
        wr(E + 0x3f8, bits(variant < 3 ? 1.9f : 0.5f)); wr(E + 0x400, variant % 3 == 2 ? 0 : 1);
        for (float dt : {0.0166667f, 0.2f}) {
            const uint32_t a[1] = {bits(dt)};
            rx_regs in = regs(); in.r[6] = E;
            call(0x00479970, in, 1, a);
        }
    }
    natives_end();
    stubs_end();
}

using CaseFn = void (*)();
const std::map<std::string, CaseFn>& cases() {
    static const std::map<std::string, CaseFn> c = {
        {"set_find", case_set_find},
        {"has_local_human", case_has_local_human},
        {"vector", case_vector_helpers},
        {"motocard", case_post_motocard},
        {"load_progress", case_load_progress},
        {"ui_property", case_ui_property},
        {"ui_language", case_ui_language},
        {"collectable", case_collectable},
        {"clear_all", case_clear_all},
        {"net_store", case_net_store},
        {"damage_update", case_damage_update},
        {"rowset_copy", case_rowset_copy},
        {"net_state", case_net_state},
        {"net_load", case_net_load},
        {"net_apply", case_net_apply},
        {"net_send", case_net_send},
        {"quazal_handle", case_quazal_handle},
        {"net_record", case_net_record},
        {"net_selection", case_net_selection},
        {"audio_event", case_audio_event},
        {"vt_report", case_vt_report},
        {"clear_pending", case_clear_pending},
        {"clear_freeride", case_clear_freeride},
        {"clear_rider3", case_clear_rider3},
        {"damage", case_damage},
        {"player_objects_fake", case_player_objects_fake},
        {"clear_variants", case_clear_variants},
        {"clear_variants2", case_clear_variants2},
        {"clear_spec2", case_clear_spec2},
        {"clear_rider2", case_clear_rider2},
        {"unknown_vehicle", case_unknown_vehicle},
        {"clear_steps", case_clear_steps},
        {"clear_spec", case_clear_spec},
        {"clear_vehicle", case_clear_vehicle},
        {"clear_rider_release", case_clear_rider_release},
        {"clear_rider_entry", case_clear_rider_entry},
        {"clear_rider_remove", case_clear_rider_remove},
        {"clear_player_objects", case_clear_player_objects},
    };
    return c;
}

void run_case(reflex_vm::Machine& m, const std::string& name, CaseFn fn) {
    uint32_t regs_[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs_, m.r, sizeof regs_); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[wv] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    std::fprintf(stderr, "[wv] case %s\n", name.c_str());
    fn();
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs_, sizeof regs_); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, WaveProbeHook, "thunk", 0) {
    const char* sel = std::getenv("RX_WV_PROBE");
    if (!sel || !*sel) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_WV_FRAME") ? uint32_t(std::atoi(std::getenv("RX_WV_FRAME"))) : 600;
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
        if (it == cases().end()) { std::fprintf(stderr, "[wv] unknown case %s\n", t.c_str()); continue; }
        run_case(m, t, it->second);
    }
    std::fprintf(stderr, "[wv] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
