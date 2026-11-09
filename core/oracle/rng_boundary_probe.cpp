// rng_boundary_probe.cpp -- the product's side of the audio boundary that draws the game's LCG (game/engine_startline.c: the engine
// emitters' start line), checked against the ORIGINAL emitters in the emulator (agent w-rng; development only).
//
//   REFLEX_PORT=rx_RngbUpdate,rx_RngbEnable,rx_RngbPreExited,rx_RngbAudioCommand RX_RNGB=verify   per call of VehicleEngineAudio_Update 00685610, 00681880 (EnableStartLineState) and
//                                            006818e0 (Pre Exited): the emitter(s) imported into the core's start-line state, the
//                                            core's function run first (the generator's words saved and restored), then the original;
//                                            compared: g_GameRandSeed / Count after both, and per emitter the state as far as the start
//                                            line (2 / 4 / other), +0x2b8, +0x2e0, +0x2e4, +0x2e8 and a car's +0x2d8. One summary line
//                                            per function at the end ("[rngb] <fn> calls N mismatches M first: ...") and every
//                                            mismatch (the first 20) on stderr.
//   REFLEX_PORT=rx_RngbUpdate,rx_RngbEnable,rx_RngbPreExited,rx_RngbAudioCommand RX_RNGB=replace  the product's way: the emitters are imported once (at the first hooked call), then the
//                                            core's start line REPLACES the original's: 00685610 runs only the core's tick (the
//                                            original's emitter is no longer updated: nothing but the sound reads it), 00681880 /
//                                            006818e0 only the core's; "DisableStartLineState" (Scenario_AudioCommand 0067ecf0) also
//                                            the core's (the original's then runs too). With rx_rngprobe in the same run, the per-frame
//                                            generator states (RX_RNG_PROBE) of this run against a run without rx_rngbound are the
//                                            whole-frame check (core/tools/rng_cmp.py --abs).
//   REFLEX_PORT=rx_RngbUpdate,rx_RngbEnable,rx_RngbPreExited,rx_RngbAudioCommand RX_RNGB=skip     the product before w-rng: the three functions do nothing (the audio boundary dropped
//                                            them): the A/B shows the skipped draws (the control of the replace run).
// The four hooks are in group "thunk" (never verified by the port framework: they compare themselves); select them by name.
// The bike's rev end (the wrapper +0x33c's state 7 at +0x474) is read from the guest emitter at each call in both modes (the FMOD
// stand-ins never finish an event, so it stays 0 in the oracle).
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <string>
extern "C" {
#include "../game/engine_startline.h"
}
using namespace reflex_port;

namespace {
constexpr uint32_t UPDATE = 0x00685610, ENABLE = 0x00681880, PRE_EXITED = 0x006818e0, AUDIO_CMD = 0x0067ecf0;
struct Stat { uint64_t calls = 0, mism = 0; std::string first; };
Stat g_stat[3];
const char* const k_name[3] = {"00685610 VehicleEngineAudio_Update", "00681880 EnableStartLineState", "006818e0 PreExited"};
bool g_imported = false;
int g_shown = 0;

uint32_t rd(reflex_vm::Machine& m, uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; }
float rf(reflex_vm::Machine& m, uint32_t a) { uint32_t v = rd(m, a); float f; std::memcpy(&f, &v, 4); return f; }
uint32_t fb(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
uint32_t manager(reflex_vm::Machine& m) { return rd(m, 0x00d92f00); }
uint32_t emitter_of(reflex_vm::Machine& m, int k) { return manager(m) + 0x520 + uint32_t(k) * 0x1b10; }
int32_t map_state(uint32_t s) { return (s == 2 || s == 4) ? int32_t(s) : RX_SL_OTHER; }

void import(reflex_vm::Machine& m, int k) {
    rx_sl_emitter* e = rx_engine_start_line_slot(k);
    const uint32_t a = emitter_of(m, k);
    const uint32_t serial = e->serial;
    std::memset(e, 0, sizeof *e);
    e->serial = serial;
    e->vehicle = rd(m, a + 0x10); e->size = int32_t(rd(m, a + 0xc)); e->state = map_state(rd(m, a + 0x2ac));
    e->start_line = rd(m, a + 0x2b8); e->timer = rf(m, a + 0x2e0); e->visits = int32_t(rd(m, a + 0x2e4)); e->seq = int32_t(rd(m, a + 0x2e8));
    e->target = rf(m, a + 0x2d8); e->rev_rpm = rf(m, a + 0x2dc);
    e->ramp.value = rf(m, a + 0x2bc); e->ramp.prev = rf(m, a + 0x2c0); e->ramp.up = rf(m, a + 0x2c4); e->ramp.hold = rf(m, a + 0x2c8);
    e->ramp.down = rf(m, a + 0x2cc); e->ramp.time = rf(m, a + 0x2d0); e->ramp.phase = rd(m, a + 0x2d4);
    e->finished = rd(m, a + 0x474) == 7;
}
void import_all(reflex_vm::Machine& m) { for (int k = 0; k < RX_SL_SLOTS; k++) import(m, k); }
// replace mode: the manager's slot binds (00680b10 / 00680d10, the original's: the emitter's vehicle, +0xc, +0x2b8 = 0, the size's first
// state) taken over as the product's vehicle_audio register / unregister do: a slot whose guest vehicle changed is imported again
void sync_binds(reflex_vm::Machine& m) {
    if (!g_imported) { import_all(m); g_imported = true; return; }
    for (int k = 0; k < RX_SL_SLOTS; k++) if (rd(m, emitter_of(m, k) + 0x10) != rx_engine_start_line_slot(k)->vehicle) import(m, k);
}
rx_sl_input input_of(reflex_vm::Machine& m, uint32_t em) {
    rx_sl_input in; std::memset(&in, 0, sizeof in);
    const uint32_t snap = rd(m, em + 0x14);
    if (snap) { in.rpm = rf(m, snap + 0x224); in.underwater = rd(m, snap + 0x258); in.drowned = rd(m, snap + 0x25c); in.wrecked = rd(m, snap + 0x260); }
    return in;
}
// the emitter's words against the core's state after both ran; "" = equal
std::string diff_slot(reflex_vm::Machine& m, int k) {
    const rx_sl_emitter* e = rx_engine_start_line_slot(k);
    const uint32_t a = emitter_of(m, k);
    char b[256]; std::string out;
    auto add = [&](const char* what, uint32_t orig, uint32_t core) { if (orig != core) { std::snprintf(b, sizeof b, " slot %d %s orig %08x core %08x;", k, what, orig, core); out += b; } };
    add("state", uint32_t(map_state(rd(m, a + 0x2ac))), uint32_t(e->state));
    add("+2b8", rd(m, a + 0x2b8), e->start_line);
    if (e->state == RX_SL_START || e->state == RX_SL_REV) {
        add("+2e0", rd(m, a + 0x2e0), fb(e->timer)); add("+2e4", rd(m, a + 0x2e4), uint32_t(e->visits)); add("+2e8", rd(m, a + 0x2e8), uint32_t(e->seq));
        if (e->size >= 6 && e->size <= 9) { add("+2d8", rd(m, a + 0x2d8), fb(e->target)); add("+2d4", rd(m, a + 0x2d4), e->ramp.phase); }
    }
    return out;
}
void record(int which, const std::string& d) {
    Stat& s = g_stat[which];
    s.calls++;
    if (!d.empty()) {
        s.mism++;
        if (s.first.empty()) s.first = d;
        if (g_shown++ < 20) std::fprintf(stderr, "[rngb] %s call %llu:%s\n", k_name[which], (unsigned long long)s.calls, d.c_str());
    }
}
struct Rng { uint32_t seed, count; };
Rng rng_get(reflex_vm::Machine& m) { return {rd(m, 0x009cb860), rd(m, 0x009cb864)}; }
void rng_set(reflex_vm::Machine& m, Rng r) { m.mem.write(0x009cb860, 4, r.seed); m.mem.write(0x009cb864, 4, r.count); }
std::string rng_diff(Rng orig, Rng core) {
    if (orig.seed == core.seed && orig.count == core.count) return "";
    char b[128]; std::snprintf(b, sizeof b, " rng orig %08x/%u core %08x/%u;", orig.seed, orig.count, core.seed, core.count); return b;
}
// runs the original at `at` (its native taken out for the call)
uint32_t run_original(reflex_vm::Machine& m, uint32_t at, const uint32_t* args, int n, int64_t eax) {
    auto it = m.natives.find(at);
    reflex_vm::Native keep = it->second;
    m.natives.erase(it);
    const uint32_t r = m.call_function(at, args, n, -1, eax);
    m.natives[at] = keep;
    return r;
}
bool mode(const char* want) { const char* v = std::getenv("RX_RNGB"); return v && std::strcmp(v, want) == 0; }
struct Report { ~Report() { for (int i = 0; i < 3; i++) if (g_stat[i].calls) std::fprintf(stderr, "[rngb] %s calls %llu mismatches %llu%s%s\n", k_name[i],
    (unsigned long long)g_stat[i].calls, (unsigned long long)g_stat[i].mism, g_stat[i].first.empty() ? "" : " first:", g_stat[i].first.c_str()); } } g_report;
}  // namespace

// VehicleEngineAudio_Update 00685610 (stdcall emitter, dt; RET 8)
RX_PORT(UPDATE, RngbUpdate, "thunk", 0) {
    if (!std::getenv("RX_RNGB")) return reflex_vm::RUN_ORIGINAL;
    const uint32_t em = m.arg(0), dtb = m.arg(1);
    float dt; std::memcpy(&dt, &dtb, 4);
    const int k = int((em - manager(m) - 0x520) / 0x1b10);
    if (k < 0 || k >= RX_SL_SLOTS) return reflex_vm::RUN_ORIGINAL;
    if (mode("skip")) return m.ret(8);
    if (mode("replace")) {
        sync_binds(m);
        rx_engine_start_line_slot(k)->finished = rd(m, em + 0x474) == 7;
        const rx_sl_input in = input_of(m, em);
        rx_engine_start_line_update(k, &in, dt);
        return m.ret(8);
    }
    import(m, k);
    const rx_sl_input in = input_of(m, em);
    const Rng r0 = rng_get(m);
    rx_engine_start_line_update(k, &in, dt);
    const Rng rc = rng_get(m);
    rng_set(m, r0);
    const uint32_t args[2] = {em, dtb};
    run_original(m, UPDATE, args, 2, -1);
    record(0, rng_diff(rng_get(m), rc) + diff_slot(m, k));
    return m.ret(8);
}

// 00681880 (EAX = the manager): EnableStartLineState
RX_PORT(ENABLE, RngbEnable, "thunk", 0) {
    if (!std::getenv("RX_RNGB")) return reflex_vm::RUN_ORIGINAL;
    if (mode("skip")) return m.ret(0);
    if (mode("replace")) {
        sync_binds(m);
        rx_engine_start_line_enable();
        return m.ret(0);
    }
    const uint32_t mgr = m.r[0];
    import_all(m);
    const Rng r0 = rng_get(m);
    rx_engine_start_line_enable();
    const Rng rc = rng_get(m);
    rng_set(m, r0);
    run_original(m, ENABLE, nullptr, 0, mgr);
    std::string d = rng_diff(rng_get(m), rc);
    for (int k = 0; k < RX_SL_SLOTS; k++) d += diff_slot(m, k);
    record(1, d);
    return m.ret(0);
}

// 006818e0 (EAX = the manager; stdcall to_first; RET 4): Pre Exited
RX_PORT(PRE_EXITED, RngbPreExited, "thunk", 0) {
    if (!std::getenv("RX_RNGB")) return reflex_vm::RUN_ORIGINAL;
    const uint32_t arg = m.arg(0);
    if (mode("skip")) return m.ret(4);
    if (mode("replace")) {
        sync_binds(m);
        rx_engine_start_line_pre_exited(arg);
        return m.ret(4);
    }
    const uint32_t mgr = m.r[0];
    import_all(m);
    const Rng r0 = rng_get(m);
    rx_engine_start_line_pre_exited(arg);
    const Rng rc = rng_get(m);
    rng_set(m, r0);
    run_original(m, PRE_EXITED, &arg, 1, mgr);
    std::string d = rng_diff(rng_get(m), rc);
    for (int k = 0; k < RX_SL_SLOTS; k++) d += diff_slot(m, k);
    record(2, d);
    return m.ret(4);
}

// Scenario_AudioCommand 0067ecf0 (ESI = the command): the replace mode's "DisableStartLineState" (the original runs after it)
RX_PORT(AUDIO_CMD, RngbAudioCommand, "thunk", 0) {
    if (!mode("replace")) return reflex_vm::RUN_ORIGINAL;
    char cmd[32] = {};
    for (uint32_t i = 0; i < 31; i++) { const uint8_t c = m.mem.mapped(m.r[6] + i) ? uint8_t(m.mem.read(m.r[6] + i, 1)) : 0; if (!c) break; cmd[i] = char(c); }
    if (strcasecmp(cmd, "DisableStartLineState") == 0) { sync_binds(m); rx_engine_start_line_disable(); }
    return reflex_vm::RUN_ORIGINAL;
}
