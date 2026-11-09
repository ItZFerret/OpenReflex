// career_heat_probe.cpp -- the oracle's probe for the w-heat ports no reference world calls (development only): MXSeriesEventHeat's
// slots, MXUIAAALoadingPage::vf4, the activity area's re-creation (Game_StartAAACreation, its job, the world's activities suspended /
// re-created / restarted, the players spawned, MXGame::Update's states 9 / 10), MXPlayer's destruction and the asset destructors of a
// package unload.
//
// A frame hook (Engine::Update 0084ed20, outside every verified call) prepares a Heat series in the world's original series manager
// with the original's own calls (LoadSeries 005b0ad0 for series RX_HEAT_SERIES (41), SetSkipPractice, CommitAndInitializeSeries
// 005b4160: the event, its 24 players from the world's Player List and the series' AI riders), then calls the ORIGINAL addresses, each
// case on a snapshot of the whole guest memory and machine state that is restored afterwards. With the ports installed, Verify runs
// the core function and then the original on the same state and compares memory and the declared registers exactly as for a call
// from the game. The world's race activity (the local player's) is the event's race (+0x14).
//   REFLEX_PORT=rx_career,rx_build8,rx_HeatProbeHook REFLEX_PORT_VERIFY=1 RX_HEAT_PROBE=<letters> [RX_HEAT_FRAME=<n> (600)]
//   letters: s the slots, p the posts (vf13 in every phase and placing), a the activity area, d MXPlayer's destruction, u the asset
//   destructors, g MXGame::Update's states 9 / 10, i print the prepared state only.
#include "oracle.h"
extern "C" {
#include "../game/build_tables.h"
#include "../game/build_vehicle.h"
#include "../game/build_activity.h"
}
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
uint32_t bits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }

constexpr uint32_t MANAGER = 0x009e2188, EVENT = MANAGER + 0x194;

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
uint32_t thiscall(uint32_t fn, uint32_t self, int n = 0, const uint32_t* args = nullptr) {
    rx_regs in = regs(); in.r[1] = self; return call(fn, in, n, args);
}
uint32_t stdcall(uint32_t fn, int n, const uint32_t* args) { rx_regs in = regs(); return call(fn, in, n, args); }

// a C string in the guest
std::string text(uint32_t a) { std::string s; for (uint32_t i = 0; i < 128 && mapped(a + i, 1); i++) { char c = (char)rx_rd8(a + i); if (!c) break; s += c; } return s; }

uint32_t local_player() {
    const uint32_t g = rd(0x00a946b8);
    for (uint32_t s = 0; g && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (mapped(p, 0x12a0) && rd(p + 4) != 0 && rd(p + 0x10) == 0 && rd(p + 0x94) && rd(p + 0x1ac)) return p;
    }
    return 0;
}

// ---- the table helpers (the originals) ----
// 005b6840 (EDX = manager, ECX = selector) -> the series' player table
uint32_t player_table() { rx_regs in = regs(); in.r[2] = MANAGER; in.r[1] = 2; return call(0x005b6840, in); }
uint32_t cell(uint32_t table, uint32_t name, uint32_t row) { return rd(rx_tbl_cell_word(table, rx_tbl_column_index(table, name), row)); }
uint32_t rows(uint32_t table) { return rx_tbl_row_count(table); }
uint32_t scratch() { return (g_m->r[4] - 0x9000) & ~15u; }
void set_int(uint32_t table, uint32_t name, uint32_t row, uint32_t value) { wr(scratch(), value); rx_tbl_set_cell_int(table, name, row, scratch()); }
void set_float(uint32_t table, uint32_t name, uint32_t row, float value) { wr(scratch(), bits(value)); rx_tbl_set_cell_float(table, name, row, scratch()); }
std::string row_name(uint32_t table, uint32_t row) {
    const uint32_t str = rx_tbl_cell_string(rd(rd(table + 0x10) + 4 * rx_tbl_column_index(table, 0x00936f00)), row);
    return text(rd(str + 0x18) < 0x10 ? str + 4 : rd(str + 4));
}

// The Heat series in the original manager, from the world's own players (no new rider to spawn: the world runtime cannot load
// packages): LoadSeries (ESI = progress; controller, id, owner), SetSkipPractice, the tour mode (+0x190, as CommitAndInitialize sets
// it), the format's event selected (005b6950: EAX = format; manager, event type: vf8 / vf6 of the old one, vf4, vf7, vf15, vf16),
// PopulatePlayers (005b2a10, EDX = manager: one series row per Player List row, the human first), the standings seeded (005b4d30:
// the series, the tour and the working -3), then the other heat's twelve riders as series rows only (PlayerState 5, not in the
// Player List, so no case spawns them).
bool prepare_series(uint32_t series) {
    rx_regs in = regs(); in.r[6] = MANAGER;
    const uint32_t a[3] = { 0, series, 1 };
    call(0x005b0ad0, in, 3, a);
    wr(MANAGER + 0x74, 1);
    wr(MANAGER + 0x190, rd(MANAGER + 8));
    {   rx_regs e = regs(); e.r[0] = rd(MANAGER + 0x10); const uint32_t b[2] = { MANAGER, rd(MANAGER + 0x14) }; call(0x005b6950, e, 2, b); }
    {   rx_regs e = regs(); e.r[2] = MANAGER; call(0x005b2a10, e); }
    for (const int32_t selector : { int32_t(rd(MANAGER + 0x20)), int32_t(rd(MANAGER + 0x190) != 1) - 2, -3 }) {
        rx_regs e = regs(); e.r[1] = MANAGER; e.r[2] = MANAGER; const uint32_t b[1] = { uint32_t(selector) }; call(0x005b4d30, e, 1, b);
    }
    const uint32_t t = player_table();
    const uint32_t v = (g_m->r[4] - 0x8000) & ~15u, c = v + 0x40, out = v + 0x20, name = v + 0x100;
    for (uint32_t k = 0; t && k < 12; k++) {
        char text[32]; std::snprintf(text, sizeof text, "Other Heat Rider %u", k + 1);
        for (uint32_t i = 0; i <= std::strlen(text); i++) rx_wr8(name + i, uint8_t(text[i]));
        rx_tbl_crit_vec_init(v);
        auto ci = [&](uint32_t col, uint32_t value) { rx_tbl_crit_init_int(c, col, 0, value); rx_tbl_crit_push(v, c); rx_tbl_crit_destroy(c); };
        ci(0x0094d6c0, 200 + k); ci(0x0094d650, 5); ci(0x0094d670, 0);
        rx_tbl_crit_init_int(c, 0x0094d6cc, 0, 0); wr(c + 0x88, 2); wr(c + 0x90, bits(float(12 + k))); rx_tbl_crit_push(v, c); rx_tbl_crit_destroy(c);
        ci(0x00936ef4, 1);
        rx_tbl_crit_init_string(c, 0x00936f00, 0, name); rx_tbl_crit_push(v, c); rx_tbl_crit_destroy(c);
        ci(0x0093e170, 1); ci(0x0094d78c, 0); ci(0x0094d798, 0);
        rx_tbl_add_row(t, out, v);
        rx_tbl_crit_vec_destroy(v);
    }
    return rd(MANAGER + 0x18c) == EVENT && rd(EVENT) == 0x0094e28c;
}

void dump(const char* what) {
    const uint32_t t = rd(MANAGER) ? player_table() : 0;
    std::fprintf(stderr, "[heatprobe] %s: event %08x vt %08x sub %u phase %u next %u +0x14 %08x +0x28 %u +0x30 %u table %08x rows %u\n", what,
                 rd(MANAGER + 0x18c), rd(EVENT), rd(EVENT + 4), rd(EVENT + 0xc), rd(EVENT + 0x10), rd(EVENT + 0x14), rd(EVENT + 0x28),
                 rd(EVENT + 0x30), t, t ? rows(t) : 0);
    for (uint32_t r = 0; t && r < rows(t) && r < 30; r++)
        std::fprintf(stderr, "[heatprobe]   row %2u %-20s type %u state %u requested %u sort %08x\n", r, row_name(t, r).c_str(),
                     cell(t, 0x00936ef4, r), cell(t, 0x0094d650, r), cell(t, 0x0094d670, r), cell(t, 0x0094d6cc, r));
    const uint32_t p = local_player();
    const uint32_t act = p ? rd(p + 0x94) : 0;
    if (act) {
        std::fprintf(stderr, "[heatprobe]   activity %08x vt %08x drivers %d\n", act, rd(act), int(rd(act + 0x26c)));
        for (uint32_t i = 0; i < rd(act + 0x26c) && i < 16; i++) {
            const uint32_t d = rd(act + 0x2d0 + 4 * i), pl = d ? rd(d + 0x74) : 0;
            std::fprintf(stderr, "[heatprobe]   driver %2u %08x player %08x %-20s type %u local %u\n", i, d, pl, pl ? text(pl + 0x14).c_str() : "",
                         pl ? rd(pl + 0x10) : 0, pl ? rd(pl + 4) : 0);
        }
    }
}
// The world image's COM stand-ins (the boot's D3D objects: vtables in the boot heap with host code at 0xed000000+) have no natives in
// the world runtime: their IUnknown AddRef / Release (slots 1 / 2, stdcall this) become reference-count stubs, as the runtime's
// "com_stubs" command makes them (a destroyed player's textures are released through them).
int com_stubs(reflex_vm::Machine& m) {
    int n = 0;
    auto host = [](uint32_t a) { return a >= 0xed000000u && a < 0xed100000u; };
    for (uint32_t v = 0x10000000; v < 0x10020000; v += 4) {
        if (!m.mem.mapped(v) || !m.mem.mapped(v + 8)) { v = (v | 0xfff); continue; }
        const uint32_t q = rd(v), a = rd(v + 4), r = rd(v + 8);
        if (!host(q) || a != q + 4 || r != q + 8 || (v > 0x10000000 && host(rd(v - 4)) && rd(v - 4) + 4 == q)) continue;
        for (const uint32_t t : { a, r }) if (!m.natives.count(t)) { m.natives[t] = [](reflex_vm::Machine& mm) -> uint32_t { return mm.ret(4, 1); }; n++; }
    }
    return n;
}

uint32_t world_activity() { const uint32_t p = local_player(); return p ? rd(p + 0x94) : 0; }

// PlayerState 4 for the heat's rows 0..11 (the world's players), 5 for the other heat's 12..23.
void bind_drivers() {
    const uint32_t t = player_table();
    if (!t) return;
    for (uint32_t r = 0; r < rows(t); r++) set_int(t, 0x0094d650, r, r < 12 ? 4 : 5);
}
// the activity's finishing order with the local human moved to `place` (the others keep their order)
void human_at(uint32_t place) {
    const uint32_t act = world_activity();
    const uint32_t n = rd(act + 0x26c);
    std::vector<uint32_t> d, others;
    uint32_t human = 0;
    for (uint32_t i = 0; i < n; i++) { const uint32_t x = rd(act + 0x2d0 + 4 * i); if (rd(rd(x + 0x74) + 0x10) == 0) human = x; else others.push_back(x); }
    if (place >= n) place = n ? n - 1 : 0;   /* the world's activity has only its own drivers (a reference world: one) */
    for (uint32_t i = 0, k = 0; i < n; i++) wr(act + 0x2d0 + 4 * i, i == place || k >= others.size() ? human : others[k++]);
}
void event_phase(uint32_t phase, uint32_t next, uint32_t sub = 0) {
    wr(EVENT + 0xc, phase); wr(EVENT + 0x10, next); wr(EVENT + 4, sub); wr(EVENT + 0x14, world_activity());
}

void probe_slots(reflex_vm::Machine& m) {
    for (uint32_t phase = 0; phase < 10; phase++) { const uint32_t a[1] = { phase }; std::fprintf(stderr, "[heatprobe] vf1(%u) -> %u\n", phase, thiscall(0x005ad840, EVENT, 1, a)); }
    for (uint32_t sub = 0; sub < 2; sub++) for (const uint32_t type : { 0x13u, 0x0u, 0x7u, rd(EVENT + 0x1c) }) {
        Snapshot snap(m);
        wr(EVENT + 4, sub); wr(EVENT + 0x1c, type);
        std::fprintf(stderr, "[heatprobe] vf18 sub %u type %x -> %u\n", sub, type, thiscall(0x005aedd0, EVENT));
    }
    {   Snapshot snap(m);
        for (const uint32_t v : { 1u, 1u, 0u, 0u, 1u }) { const uint32_t a[1] = { v }; thiscall(0x005aee00, EVENT, 1, a); }
        std::fprintf(stderr, "[heatprobe] vf2 sequence: +0xf0 %u\n", rd(EVENT + 0xf0));
    }
    {   Snapshot snap(m);
        const uint32_t a[1] = { rd(MANAGER + 0x180) };
        thiscall(0x005ad8f0, EVENT, 1, a);
        thiscall(0x005ad880, EVENT, 1, a);
        thiscall(0x005ad8f0, EVENT, 1, a);
        std::fprintf(stderr, "[heatprobe] vf8 / vf7 / vf8 done\n");
    }
    // vf10(1) and vf12 set up an activity (005a9290: the Player List rows by name, the activity area): the world test has neither
    // the series' Player List rows nor file access (CreateFileA), so they run only with RX_HEAT_SETUP=1; the career boot verifies them.
    const bool setup = std::getenv("RX_HEAT_SETUP") != nullptr;
    for (const uint32_t v : { 0u, 1u }) {
        if (v == 1 && !setup) continue;
        Snapshot snap(m);
        bind_drivers();
        const uint32_t a[1] = { v };
        thiscall(0x005ad960, EVENT, 1, a);
        std::fprintf(stderr, "[heatprobe] vf10(%u): phase %u next %u gate %u\n", v, rd(EVENT + 0xc), rd(EVENT + 0x10), rd(EVENT + 0x24));
    }
    for (uint32_t sub = 0; sub < 2 && setup; sub++) for (const uint32_t next : { 1u, 3u, 4u, 5u, 8u }) {
        Snapshot snap(m);
        bind_drivers();
        event_phase(sub ? 1 : 3, next, sub);
        thiscall(0x005adb00, EVENT);
        std::fprintf(stderr, "[heatprobe] vf12 sub %u next %u: phase %u next %u\n", sub, next, rd(EVENT + 0xc), rd(EVENT + 0x10));
    }
    {   // the proxies' clones: a proxy record in dead stack {vtable, 0, type name, object, member}
        const uint32_t p = (m.r[4] - 0x7000) & ~15u;
        for (const uint32_t fn : { 0x005bfc40u, 0x005bfce0u, 0x005bfda0u }) {
            Snapshot snap(m);
            wr(p, 0x0094eaf8); wr(p + 4, 0); wr(p + 8, 0); wr(p + 0xc, EVENT); wr(p + 0x10, 0x005c3300);
            const uint32_t c = thiscall(fn, p);
            std::fprintf(stderr, "[heatprobe] clone %08x -> %08x vt %08x\n", fn, c, rd(c));
        }
    }
}

void probe_posts(reflex_vm::Machine& m) {
    const uint32_t t = player_table();
    {   Snapshot snap(m); bind_drivers(); event_phase(1, 8); thiscall(0x005adde0, EVENT);
        std::fprintf(stderr, "[heatprobe] post practice: next %u\n", rd(EVENT + 0x10)); }
    for (const uint32_t place : { 0u, 4u, 5u, 7u, 10u, 11u }) for (int lcq_rows = 0; lcq_rows < 2; lcq_rows++) {
        Snapshot snap(m);
        bind_drivers();
        if (lcq_rows) for (uint32_t r = 7; r < 13 && r < rows(t); r++) set_int(t, 0x0094d650, r, 6);   /* the other heat's LCQ riders */
        human_at(place);
        event_phase(3, 8);
        thiscall(0x005adde0, EVENT);
        std::fprintf(stderr, "[heatprobe] post heat, human %u, other LCQ %d: next %u +0x28 %u\n", place, lcq_rows, rd(EVENT + 0x10), rd(EVENT + 0x28));
    }
    // the LCQ / main event posts read every driver of the race: a reference world's activity has one, so they need RX_HEAT_RACE_POSTS=1
    // on a world with a full grid (the career boot verifies them: heat, LCQ, main event)
    if (!std::getenv("RX_HEAT_RACE_POSTS")) return;
    for (const uint32_t place : { 0u, 1u, 2u, 6u, 11u }) for (int main_human = 0; main_human < 2; main_human++) {
        Snapshot snap(m);
        bind_drivers();
        for (uint32_t r = 0; r < 12; r++) set_int(t, 0x0094d650, r, 6);
        if (main_human) set_int(t, 0x0094d650, 0, 8);
        human_at(place);
        event_phase(4, 8);
        thiscall(0x005adde0, EVENT);
        std::fprintf(stderr, "[heatprobe] post LCQ, human %u, main human %d: next %u\n", place, main_human, rd(EVENT + 0x10));
    }
    for (const uint32_t place : { 0u, 3u, 11u }) {
        Snapshot snap(m);
        bind_drivers();
        for (uint32_t r = 0; r < rows(t); r++) set_int(t, 0x0094d650, r, r < 12 ? 8 : 9);
        human_at(place);
        event_phase(5, 8);
        thiscall(0x005adde0, EVENT);
        std::fprintf(stderr, "[heatprobe] post main, human %u: +0x30 %u\n", place, rd(EVENT + 0x30));
    }
}
}  // namespace

RX_PORT(0x0084ed20, HeatProbeHook, "thunk", 0) {
    static const char* letters = std::getenv("RX_HEAT_PROBE");
    if (!letters || !*letters) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_HEAT_FRAME") ? uint32_t(std::atoi(std::getenv("RX_HEAT_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    {
        Snapshot outer(m);
        if (!outer.ok()) { std::fprintf(stderr, "[heatprobe] ERROR missing reversible host state\n"); return reflex_vm::RUN_ORIGINAL; }
        const uint32_t series = std::getenv("RX_HEAT_SERIES") ? uint32_t(std::atoi(std::getenv("RX_HEAT_SERIES"))) : 41;
        std::fprintf(stderr, "[heatprobe] COM stubs: %d\n", com_stubs(m));
        dump("before");
        const bool ok = prepare_series(series);
        dump(ok ? "prepared" : "NOT PREPARED");
        /* MXRecords_Leave (00764560, reached when the local human's player is removed: the prepared series pruned the world's Player
         * List, so a Game_SetupActivityArea of a case removes the world's players) runs the records owner 007647f0 (its oracle calls of
         * 004eb830 / 006d7980 are closed since w-actarea, records_leave_highlights.c); by default the records object [0xa946c0] is
         * null for the cases, so Activity_OnPlayerLeft skips that one call (its own guard), as race_teardown_probe.cpp does;
         * RX_HEAT_RECORDS=1 keeps it. */
        if (!std::getenv("RX_HEAT_RECORDS")) wr(0x00a946c0, 0);
        if (ok && std::strchr(letters, 's')) probe_slots(m);
        if (ok && std::strchr(letters, 'p')) probe_posts(m);
    }
    std::fprintf(stderr, "[heatprobe] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
