// race_challenge_probe.cpp -- the oracle's probe for the profile's challenge awards (core/game/race_challenge.c) and the network
// selection iterator's advance (race_net_session.c rx_net_selection_advance), which no reference world reaches (development only).
// A frame hook (Engine::Update 0084ed20, outside every verified call) runs the selected CASES; each case builds its arguments on the
// live objects and calls the ORIGINAL address, so with the ports installed Verify runs the core function and then the original on
// the same state and compares memory and the declared registers exactly as for a call from the game. Each case runs on a snapshot of
// the whole guest memory and machine state, restored afterwards (the run continues on the untouched world).
//   REFLEX_PORT=rx_race,rx_fx,rx_ChallengeProbeHook REFLEX_PORT_VERIFY=1 RX_LC_PROBE=<case>[,<case>...] [RX_LC_FRAME=<n>] <world run>
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
std::string cstr(uint32_t a, uint32_t max = 0x40) {
    std::string s;
    for (uint32_t i = 0; i < max; i++) { const uint8_t c = rx_rd8(a + i); if (!c) break; s += char(c); }
    return s;
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

// the local human player's slot
uint32_t find_player() {
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

// ---- tables ----------------------------------------------------------------------------------------------------------------------
uint32_t column_of(uint32_t table, const char* name) {
    const uint32_t i = rx_tbl_column_index(table, hstr(name));
    return i == 0xffffffffu ? 0 : rd(rd(table + 0x10) + 4 * i);
}
uint32_t cell(uint32_t table, const char* name, uint32_t row) {
    const uint32_t i = rx_tbl_column_index(table, hstr(name));
    return rd(rx_tbl_cell_word(table, i, row));
}
uint32_t table_rows(uint32_t table) { return rx_tbl_row_count(table); }
void dump_table(const char* label, uint32_t table, uint32_t max_rows = 8) {
    if (!table) { std::fprintf(stderr, "[lc] %s: no table\n", label); return; }
    const uint32_t nc = (rd(table + 0x14) - rd(table + 0x10)) / 4;
    std::fprintf(stderr, "[lc] %s: table %08x %u columns %u rows rev %u:", label, table, nc, table_rows(table), rd(table + 0x68));
    for (uint32_t c = 0; c < nc; c++) std::fprintf(stderr, " %s(%u)", cstr(rd(rd(table + 0x10) + 4 * c)).c_str(), rd(rd(rd(table + 0x10) + 4 * c) + 0x2c));
    std::fprintf(stderr, "\n");
    for (uint32_t r = 0; r < table_rows(table) && r < max_rows; r++) {
        std::fprintf(stderr, "[lc]   row %u:", r);
        for (uint32_t c = 0; c < nc; c++) {
            const uint32_t col = rd(rd(table + 0x10) + 4 * c);
            if (rd(col + 0x2c) == 4) { std::fprintf(stderr, " <str>"); continue; }
            std::fprintf(stderr, " %08x", rd(rx_tbl_cell_word(table, c, r)));
        }
        std::fprintf(stderr, "\n");
    }
}
uint32_t profile_db(uint32_t n) { return 0x00ae10f0u + n * 0x1f4u; }
uint32_t event_locations() { return rx_tbl_find(0x00ae0910, hstr("EventTypeLocations")); }
uint32_t challenges(uint32_t profile) { return rx_tbl_find(profile_db(profile), hstr("PlayerChallenges")); }

// the UI property store: UIProperty_SetInt 005dbfb0 (ECX = the UI manager; stack key, value; RET 8)
void set_prop(const char* name, uint32_t value) {
    rx_regs in = regs(); in.r[1] = rd(0x00a93fcc);
    const uint32_t a[2] = {hstr(name), value};
    call(0x005dbfb0, in, 2, a);
}
// 004e7780 (stdcall activity type; RET 4) -> the EventTypeId
uint32_t event_type_id(uint32_t type) { const uint32_t a[1] = {type}; return call(0x004e7780, regs(), 1, a).r[0]; }

// ---- cases ------------------------------------------------------------------------------------------------------------------------
void case_dump() {
    const uint32_t p = find_player();
    std::fprintf(stderr, "[lc] player %08x profile %d activity %08x vt %08x ui %08x\n", p, int(rd(p + 0x1f0)), rd(p + 0x94), rd(rd(p + 0x94)), rd(0x00a93fcc));
    for (uint32_t k = 0; k < 14; k++) std::fprintf(stderr, "[lc] kind %u -> type %u -> EventTypeId %d\n", k, k + 0x15, int(event_type_id(k + 0x15)));
    dump_table("EventTypeLocations", event_locations(), 12);
    for (uint32_t n = 0; n < 2; n++) dump_table(n ? "PlayerChallenges 1" : "PlayerChallenges 0", challenges(n), 6);
    const uint32_t t = event_locations();
    for (uint32_t r = 0; r < table_rows(t); r++)
        std::fprintf(stderr, "[lc] etl %u: type %d loc %d track %d\n", cell(t, "EventTypeLocationId", r), int(cell(t, "EventTypeId", r)), int(cell(t, "EventLocationId", r)), int(cell(t, "TrackId", r)));
}

// ---- helpers for the award cases ------------------------------------------------------------------------------------------------
// the EventTypeLocations row of an activity kind (event type 0x15 + kind): {etl, location, track}; false when the table has none
struct Loc { uint32_t etl, loc, track; };
bool location_of(uint32_t kind, int nth, Loc* out) {
    const uint32_t t = event_locations(), type = event_type_id(kind + 0x15);
    int seen = 0;
    for (uint32_t r = 0; r < table_rows(t); r++) {
        if (cell(t, "EventTypeId", r) != type) continue;
        if (seen++ < nth) continue;
        *out = {cell(t, "EventTypeLocationId", r), cell(t, "EventLocationId", r), cell(t, "TrackId", r)};
        return true;
    }
    return false;
}
// 0054e670 (stdcall player, kind, medal, value; RET 0x10)
void award(uint32_t player, uint32_t kind, uint32_t medal, uint32_t value_bits) {
    const uint32_t a[4] = {player, kind, medal, value_bits};
    call(0x0054e670, regs(), 4, a);
}
// the challenge row of the profile for an etl: its table and row index (row -1 when absent)
bool challenge_row(uint32_t profile, uint32_t etl, uint32_t* table, uint32_t* row) {
    *table = challenges(profile);
    const uint32_t ref = rx_tbl_row(column_of(*table, "EventTypeLocationId"), int32_t(etl));
    *row = rd(ref + 8) ? rd(ref) : 0xffffffffu;
    return *row != 0xffffffffu;
}
void set_cell(uint32_t table, const char* name, uint32_t row, uint32_t bits_) {
    const uint32_t v = halloc(4); wr(v, bits_);
    const uint32_t c = rx_tbl_column_index(table, hstr(name));
    const uint32_t col = rd(rd(table + 0x10) + 4 * c);
    if (rd(col + 0x2c) == 2) rx_tbl_set_float(col, row, v); else rx_tbl_set_int(col, row, v);
}
// a fake activity for the vf63 compare: the live one's event manager (+0x290), the given compare function in slot 63
uint32_t fake_activity(uint32_t real, uint32_t compare) {
    const uint32_t f = halloc(0x300), vt = halloc(0x200);
    wr(f, vt); wr(vt + 0xfc, compare); wr(f + 0x290, rd(real + 0x290)); wr(f + 0x234, rd(real + 0x234));
    return f;
}
const uint32_t kValues[] = {0x3f800000u /*1*/, 0x40a00000u /*5*/, 0x41480000u /*12.5*/, 0x3f000000u /*0.5*/, 0x42c80000u /*100*/, 0x40a00000u /*5 again: equal*/};

// kinds 7 (discovery counts) and 2 (jump lengths): each medal several times with a growing value: the first call adds the row, the
// later ones update it; career mode on and off (the first Gold counts the highlights)
void case_award_free_ride() {
    const uint32_t P = find_player();
    for (uint32_t career : {0u, 1u}) {
        set_prop("InCareerMode", career);
        for (uint32_t kind : {7u, 2u}) {
            Loc l;
            if (!location_of(kind, 0, &l)) { std::fprintf(stderr, "[lc] kind %u: no EventTypeLocations row (event type %d)\n", kind, int(event_type_id(kind + 0x15))); l = {0, 0xffffffffu, 0}; }
            set_prop("EventLocationId", l.loc); set_prop("TrackId", l.track);
            std::fprintf(stderr, "[lc] award kind %u career %u: etl %u loc %u track %u\n", kind, career, l.etl, l.loc, l.track);
            for (uint32_t round = 0; round < 3; round++)
                for (uint32_t medal : {0u, 1u, 2u, 3u, 2u, 0u, 7u})
                    for (uint32_t v : kValues) award(P, kind, medal, v);
        }
    }
}
// the kinds 1 / 6 / 8 ask the activity's vf63: a fake activity with the lower-is-better and the higher-is-better compare
void case_award_compare() {
    const uint32_t P = find_player(), real = rd(P + 0x94);
    set_prop("InCareerMode", 0);
    for (uint32_t compare : {0x0055ec70u, 0x0055ed20u}) {
        wr(P + 0x94, fake_activity(real, compare));
        for (uint32_t kind : {1u, 6u, 8u}) {
            Loc l;
            if (!location_of(kind, 0, &l)) { std::fprintf(stderr, "[lc] kind %u: no row\n", kind); continue; }
            set_prop("EventLocationId", l.loc); set_prop("TrackId", l.track);
            std::fprintf(stderr, "[lc] award_compare %08x kind %u: etl %u loc %u track %u\n", compare, kind, l.etl, l.loc, l.track);
            for (uint32_t round = 0; round < 2; round++)
                for (uint32_t medal : {0u, 1u, 2u, 3u})
                    for (uint32_t v : kValues) award(P, kind, medal, v);
        }
    }
    wr(P + 0x94, real);
}
// a negative profile number: only the event is raised (medal != 3); a location that is not in the table
void case_award_edges() {
    const uint32_t P = find_player();
    set_prop("InCareerMode", 0);
    for (uint32_t profile : {0xffffffffu, 0x80000000u}) {
        wr(P + 0x1f0, profile);
        for (uint32_t medal : {0u, 3u}) award(P, 7, medal, 0x3f800000u);
    }
    wr(P + 0x1f0, 0);
    set_prop("EventLocationId", 9999); set_prop("TrackId", 5);       // no such location: etl -1, no row
    for (uint32_t kind : {7u, 2u, 1u}) for (uint32_t medal : {0u, 1u, 2u}) award(P, kind, medal, 0x40000000u);
    set_prop("EventLocationId", 0xffffffffu); set_prop("TrackId", 0);
    award(P, 7, 1, 0x40000000u);
    // other kinds: no write (not 1 / 6 / 7 / 8); kind 10 and up would name an activity type without an EventType row, whose lookup
    // (004e7780 -> a missing key of the unique name index) is a not-yet-ported path of the table kernel (build_tables_rdb.c 004f64fe)
    for (uint32_t kind : {0u, 3u, 4u, 5u, 9u}) award(P, kind, 0, 0x40000000u);
}
// extreme stored counts (the clamp to 1 .. 0x7ffffffe) and NaN values
void case_award_clamp() {
    const uint32_t P = find_player();
    set_prop("InCareerMode", 1);
    Loc l;
    if (!location_of(7, 0, &l)) { std::fprintf(stderr, "[lc] no kind 7 location\n"); return; }
    set_prop("EventLocationId", l.loc); set_prop("TrackId", l.track);
    award(P, 7, 3, 0x40a00000u);                                      // creates the row
    uint32_t table, row;
    if (!challenge_row(0, l.etl, &table, &row)) { std::fprintf(stderr, "[lc] award_clamp: no row after the first award\n"); return; }
    const uint32_t extremes[] = {0x7ffffffeu, 0x7fffffffu, 0xffffffffu, 0x80000000u, 0, 1, 0x7ffffffdu};
    for (uint32_t e : extremes) {
        for (uint32_t medal : {0u, 1u, 2u}) {
            set_cell(table, "Bronze", row, e); set_cell(table, "Silver", row, e); set_cell(table, "Gold", row, e);
            award(P, 7, medal, 0x40a00000u);
        }
    }
    // NaN, sNaN, -NaN (RX_LC_NAN=1: the emulator's x87 stores payload-less NaNs, 0x7fffffff, where the hardware - and the core - keep the
    // payload: the cell writers 004f5650 / 004f5330 copy through fld / fstp, so those values differ in the NaN's low bits by design), +-inf, -0, 0
    std::vector<uint32_t> floats = {0x7f800000u, 0xff800000u, 0x80000000u, 0u};
    if (std::getenv("RX_LC_NAN")) for (uint32_t v : {0x7fc00000u, 0x7f800001u, 0xffc00000u}) floats.push_back(v);
    for (uint32_t v : floats) {
        set_cell(table, "Param", row, 0x40a00000u);
        award(P, 7, 1, v);
        set_cell(table, "Param", row, v);                               // a stored NaN / inf / -0
        award(P, 7, 1, 0x40a00000u);
        award(P, 7, 1, v);
    }
    set_cell(table, "Param", row, 0xbf800000u);                         // a stored -1.0 counts as "nothing stored"
    award(P, 7, 0, 0x41200000u);
}
// 004ea0a0: the location of {type, location, track}: a match, a different track, an unknown type / location, every row of the table
void case_event_location() {
    const uint32_t t = event_locations();
    const uint32_t n = table_rows(t);
    for (uint32_t r = 0; r < n; r++) {
        const uint32_t a[3] = {cell(t, "EventTypeId", r), cell(t, "EventLocationId", r), cell(t, "TrackId", r)};
        call(0x004ea0a0, regs(), 3, a);
        const uint32_t b[3] = {a[0], a[1], a[2] + 1};
        if (r % 5 == 0) call(0x004ea0a0, regs(), 3, b);
    }
    for (uint32_t x : {0u, 0xffffffffu, 77u}) { const uint32_t a[3] = {x, x, x}; call(0x004ea0a0, regs(), 3, a); }
    // two rows with the same triple: a copy of the first row appended with the core's row kernel
    if (n) {
        const uint32_t nc = (rd(t + 0x14) - rd(t + 0x10)) / 4, vals = halloc(4 * nc);
        bool ok = true;
        for (uint32_t c = 0; c < nc; c++) {
            const uint32_t col = rd(rd(t + 0x10) + 4 * c);
            if (rd(col + 0x2c) != 1 && rd(col + 0x2c) != 2) { ok = false; break; }
            const uint32_t v = halloc(4); wr(v, rd(rx_tbl_cell_word(t, c, 0)));
            if (cstr(col) == "EventTypeLocationId") wr(v, 100000);
            if (cstr(col) == "StoreProductId") wr(v, 100000);
            wr(vals + 4 * c, v);
        }
        if (ok) {
            rx_tbl_append_values_row(t, vals);
            const uint32_t a[3] = {cell(t, "EventTypeId", 0), cell(t, "EventLocationId", 0), cell(t, "TrackId", 0)};
            std::fprintf(stderr, "[lc] event_location: duplicate row appended (rows %u)\n", table_rows(t));
            call(0x004ea0a0, regs(), 3, a);
        } else std::fprintf(stderr, "[lc] event_location: unsupported column type\n");
    }
}
// 004ea6c0 / 004ea3a0 directly: reads and writes of a location with and without a row, the profile's table and the revision counter
void case_profile_rw() {
    Loc l;
    if (!location_of(7, 0, &l)) { std::fprintf(stderr, "[lc] no kind 7 location\n"); return; }
    for (uint32_t profile : {0u, 1u}) {
        for (uint32_t etl : {l.etl, 9999u, 0xffffffffu}) {
            const uint32_t out = halloc(0x20);
            for (int i = 0; i < 4; i++) wr(out + 4 * i, 0xdead0000u + i);
            rx_regs in = regs(); in.r[0] = profile;
            const uint32_t a[5] = {etl, out, out + 4, out + 8, out + 12};
            call(0x004ea6c0, in, 5, a);
            for (int pass = 0; pass < 3; pass++) {
                rx_regs w = regs(); w.r[0] = profile;
                const uint32_t b[5] = {etl, uint32_t(3 + pass), uint32_t(2), uint32_t(pass == 2 ? 1 : 0), pass == 1 && std::getenv("RX_LC_NAN") ? 0x7fc00000u : 0x40490fdbu};
                call(0x004ea3a0, w, 5, b);
                call(0x004ea6c0, in, 5, a);
            }
            // the same values again: nothing changes (the revision counter stays)
            rx_regs w = regs(); w.r[0] = profile;
            const uint32_t b[5] = {etl, 5, 2, 1, 0x40490fdbu};
            call(0x004ea3a0, w, 5, b);
            call(0x004ea3a0, w, 5, b);
        }
    }
}
// 0055ec70 / 0055ed20: new < old, new > old, equal, NaNs
void case_compare() {
    const uint32_t vs[] = {0x3f800000u, 0x40000000u, 0x7fc00000u, 0xbf800000u, 0u, 0x80000000u};
    for (uint32_t fn : {0x0055ec70u, 0x0055ed20u})
        for (uint32_t a : vs) for (uint32_t b : vs) { rx_regs in = regs(); in.r[1] = 0x1234; const uint32_t args[2] = {a, b}; call(fn, in, 2, args); }
}
// 00554d40 (ECX = kind, EDX = medal, ESI = the event manager; stdcall player): the event with every kind / medal
void case_award_event() {
    const uint32_t P = find_player(), act = rd(P + 0x94);
    for (uint32_t kind : {1u, 2u, 6u, 7u, 8u}) for (uint32_t medal : {0u, 1u, 2u}) {
        rx_regs in = regs(); in.r[1] = kind; in.r[2] = medal; in.r[6] = rd(act + 0x290);
        const uint32_t a[1] = {P};
        call(0x00554d40, in, 1, a);
    }
}
// 006d8390 (EDI = the highlight tracking, ESI = the profile): career mode off / on
void case_career() {
    for (uint32_t career : {0u, 1u}) for (uint32_t profile : {0u, 1u}) {
        set_prop("InCareerMode", career);
        rx_regs in = regs(); in.r[7] = rd(0x00d92edc); in.r[6] = profile;
        call(0x006d8390, in);
    }
}

// ---- the network selection iterator: 00508800 (+ 00508770, 0053a170) ----------------------------------------------------------------
// a std::set<uint> of the given keys inside a holder (the holder's +4 is the set, whose +0 is its proxy and +0x18 its head), balanced
struct FakeSet { uint32_t holder, head; };
FakeSet make_set(const std::vector<uint32_t>& keys) {
    const uint32_t holder = halloc(0x60), set = holder + 4, head = halloc(0x20), proxy = halloc(0x10);
    wr(set, proxy); wr(proxy, set); wr(set + 0x18, head); wr(set + 0x1c, uint32_t(keys.size()));
    rx_wr8(head + 0x15, 1);
    std::vector<uint32_t> nodes;
    for (uint32_t k : keys) { const uint32_t nd = halloc(0x20); wr(nd + 0xc, k); nodes.push_back(nd); }
    std::function<uint32_t(int, int, uint32_t)> build = [&](int lo, int hi, uint32_t parent) -> uint32_t {
        if (lo > hi) return head;
        const int mid = (lo + hi) / 2;
        const uint32_t nd = nodes[mid];
        wr(nd + 4, parent); wr(nd, build(lo, mid - 1, nd)); wr(nd + 8, build(mid + 1, hi, nd));
        return nd;
    };
    const uint32_t root = keys.empty() ? head : build(0, int(keys.size()) - 1, head);
    wr(head + 4, root); wr(head, keys.empty() ? head : nodes.front()); wr(head + 8, keys.empty() ? head : nodes.back());
    return {holder, head};
}
void case_net_advance() {
  // the stats lock: variant 0 a fake scope whose lock (+0x3c) is the allocator's critical section, 1 no stat object and locking off
  // (byte 0x9df580), 2 the fake scope with locking off
  for (int variant = 0; variant < 3; variant++) {
    const uint32_t s1 = halloc(0x20), s2 = halloc(0x20), lockobj = halloc(0x10);
    wr(lockobj, 0x00d61704); wr(s1 + 0xc, s2); wr(s2 + 8, lockobj - 0x3c);
    wr(0x009df590, variant == 1 ? 0 : s1);
    rx_wr8(0x009df580, variant == 0 ? 0 : 1);
    std::fprintf(stderr, "[lc] net_advance variant %d\n", variant);
    for (size_t n = 0; n <= 7; n++) {
        std::vector<uint32_t> keys;
        for (size_t i = 0; i < n; i++) keys.push_back(uint32_t(3 * i + 2));
        if (n == 7) keys[6] = 0xfffffff0u;                       // an unsigned key above the signed range
        const FakeSet fs = make_set(keys);
        for (uint32_t cur : {0u, 1u, 2u, 3u, 4u, 5u, 8u, 11u, 14u, 17u, 18u, 19u, 100u, 0xffffffefu, 0xffffffffu, 0xfffffffeu}) {
            const uint32_t it = halloc(0x40);
            wr(it, fs.holder); wr(it + 0x14, cur);
            wr(it + 4, 0);                                       // no earlier handle (0088bc40 would release it)
            rx_regs in = regs(); in.r[1] = it;
            call(0x00508800, in);
            // the successor alone, into a scratch word
            const uint32_t out = halloc(0x10);
            wr(out, 0xdeadbeefu);
            rx_regs in2 = regs(); in2.r[1] = it;
            const uint32_t a[2] = {out, cur};
            call(0x00508770, in2, 2, a);
            // the lower bound alone
            const uint32_t res = halloc(0x10), key = halloc(0x10);
            wr(key, cur);
            rx_regs in3 = regs(); in3.r[1] = fs.holder + 4;
            const uint32_t b[2] = {res, key};
            call(0x0053a170, in3, 2, b);
        }
    }
  }
}

using CaseFn = void (*)();
const std::map<std::string, CaseFn>& cases() {
    static const std::map<std::string, CaseFn> c = {
        {"dump", case_dump},
        {"award_free_ride", case_award_free_ride},
        {"award_compare", case_award_compare},
        {"award_edges", case_award_edges},
        {"award_clamp", case_award_clamp},
        {"event_location", case_event_location},
        {"profile_rw", case_profile_rw},
        {"compare", case_compare},
        {"award_event", case_award_event},
        {"career", case_career},
        {"net_advance", case_net_advance},
    };
    return c;
}

void run_case(reflex_vm::Machine& m, const std::string& name, CaseFn fn) {
    uint32_t regs_[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs_, m.r, sizeof regs_); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[lc] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    std::fprintf(stderr, "[lc] case %s\n", name.c_str());
    fn();
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs_, sizeof regs_); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, ChallengeProbeHook, "thunk", 0) {
    const char* sel = std::getenv("RX_LC_PROBE");
    if (!sel || !*sel) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_LC_FRAME") ? uint32_t(std::atoi(std::getenv("RX_LC_FRAME"))) : 600;
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
        if (it == cases().end()) { std::fprintf(stderr, "[lc] unknown case %s\n", t.c_str()); continue; }
        run_case(m, t, it->second);
    }
    std::fprintf(stderr, "[lc] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
