// partylast_probe.cpp -- TEST FIXTURES (oracle only, never in the product) for the three party modes no installed level can run:
// EggHunt (activity type 0xf, "Greed"), Graffiti (0xb) and Capture (0x10) (core/game/race_egghunt.c, race_graffiti.c, race_capture.c).
// Every installed level lacks the scene entities these activities look up when they start (MXActivity*::vf2), so the ORIGINAL stops
// there in every boot (EggHunt 0057492d: no "GreedModel"; Capture 005765f1: no "RedFlag"; Graffiti 00564151: no start entity, a
// division by zero). This file fabricates the minimal entities in the emulated scene right before the world starts its activities
// (a hook on 007668e0, outside every verified call), so the original and the core run the same mode side by side under Verify.
// The fixtures are NOT game content: they borrow existing entities of the level (renamed / cloned) and are marked as such.
//   REFLEX_PORT=rx_party,rx_PartyLastFixture,rx_PartyLastDriverFixture[,rx_PartyLastProbeHook] RX_PL_FIXTURE=1
//   RX_PL_DUMP=1      the scene's subscene buckets and the world's activities at the world start (stderr)
//   RX_PL_PROBE=1     (rx_PartyLastProbeHook, Engine::Update 0084ed20, outside every verified call) once the activity has run
//                     RX_PL_PROBE_FRAMES InProgress frames (default 120): the cases below, each on a snapshot of the whole guest memory
//                     and machine state, restored afterwards. Each case calls the ORIGINAL function, so with the ports installed Verify
//                     runs the core function and then the original on the same state, as for a call from the game.
//   RX_PL_AUDIO=1     keep the audio thread round trips in the cases (by default each case runs with the audio system's shutdown flag
//                     [0x009b7430] set, see audio_quiet: the sound player / the audio logic's flush then signal at once, so the cued
//                     calls (vf15 / vf16 / the round's places / the cue handlers) are verifiable)
//   RX_PL_TEARDOWN=1  also the teardown cases (vf3 + the destructor): the world faults some frames after them (state the snapshot does
//                     not hold), so such a run ends soon after the probe (REFLEX_AFTER)
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace reflex_port;

namespace {
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
std::string rstr(uint32_t a, uint32_t max = 0x104) {
    std::string s;
    for (uint32_t i = 0; i < max && mapped(a + i, 1); i++) { const char c = char(rx_rd8(a + i)); if (!c) break; s += c; }
    return s;
}
void wstr(uint32_t a, const char* s) { for (uint32_t i = 0;; i++) { rx_wr8(a + i, uint8_t(s[i])); if (!s[i]) break; } }

uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
float rf(uint32_t a) { const uint32_t b = rd(a); float f; std::memcpy(&f, &b, 4); return f; }
void wf(uint32_t a, float f) { wr(a, fb(f)); }

void dump_scene(uint32_t scene) {
    for (uint32_t b = 0; b < 0x18; b++) {
        const uint32_t rec = scene + 0x38 + 0x110 * b;
        const uint32_t n = rd(rec + 0x108), use = rd(rec + 0x10c);
        if (!use && !n) continue;
        std::fprintf(stderr, "[pl] bucket %2u use %u count %4u '%s'\n", b, use, n, rstr(rec).c_str());
        if (std::getenv("RX_PL_DUMP") && std::atoi(std::getenv("RX_PL_DUMP")) > 1)
            for (uint32_t i = 0; i < n && i < 400; i++) {
                const uint32_t e = rd(rd(rec + 0x104) + 4 * i);
                std::fprintf(stderr, "[pl]    %3u %08x vt %08x model %08x '%s'\n", i, e, rd(e), rd(e + 0x100), rstr(rd(e + 0x18), 64).c_str());
            }
    }
}
uint32_t halloc(uint32_t size) { return rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, &size); }   /* ReflexAlignedAlloc */
uint32_t guest_string(const char* s) {
    const uint32_t n = uint32_t(std::strlen(s)) + 1, a = halloc((n + 15) & ~15u);
    if (a) wstr(a, s);
    return a;
}
/* the bucket (+0x38 + 0x110 * b) of a subscene by name, or 0 */
uint32_t bucket_named(uint32_t scene, const std::string& name) {
    for (uint32_t b = 0; b < 0x18; b++) {
        const uint32_t rec = scene + 0x38 + 0x110 * b;
        if (rd(rec + 0x10c) && rstr(rec) == name) return rec;
    }
    return 0;
}
uint32_t entity_named(uint32_t rec, const char* name) {
    for (uint32_t i = 0; i < rd(rec + 0x108); i++) {
        const uint32_t e = rd(rd(rec + 0x104) + 4 * i);
        if (rstr(rd(e + 0x18), 64) == name) return e;
    }
    return 0;
}

/* TEST FIXTURE, EggHunt: the level's golden egg (an EggHuntEggEntity of the MiniGameGreed subscene, a ModelEntity with the egg model)
 * renamed "GreedModel", the entity MXActivityEggHunt::vf2 clones its ten eggs' model from (no shipped level has one) */
void fixture_egghunt(uint32_t act, uint32_t scene) {
    const uint32_t rec = bucket_named(scene, rstr(act + 0x118));
    if (!rec) { std::fprintf(stderr, "[pl] fixture: EggHunt: no subscene '%s' in the scene\n", rstr(act + 0x118).c_str()); return; }
    if (entity_named(rec, "GreedModel")) return;
    uint32_t e = entity_named(rec, "EggHuntEggEntity_Golden");
    for (uint32_t i = 0; !e && i < rd(rec + 0x108); i++) {
        const uint32_t c = rd(rd(rec + 0x104) + 4 * i);
        if (rd(c) == 0x00938d4c) e = c;   /* any EggHuntEggEntity */
    }
    if (!e) { std::fprintf(stderr, "[pl] fixture: EggHunt: no egg entity to stand in for GreedModel\n"); return; }
    wr(e + 0x18, guest_string("GreedModel"));
    /* the round's duration: the activity row's Duration (Activity::vf1); no EggHunt row exists, so 0 ends every round at its first
       InProgress frame: 60 s for the test (RX_PL_DURATION) */
    if (!(rf(act + 0x2bc) > 0.0f)) wf(act + 0x2bc, std::getenv("RX_PL_DURATION") ? float(std::atof(std::getenv("RX_PL_DURATION"))) : 60.0f);
    std::fprintf(stderr, "[pl] fixture: EggHunt: entity %08x (model %08x) of '%s' renamed GreedModel (TEST FIXTURE)\n", e, rd(e + 0x100), rstr(act + 0x118).c_str());
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
int g_cases = 0;
template <class F> void run_case(reflex_vm::Machine& m, Pages& pages, const char* what, F&& body) {
    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[pl] ERROR missing reversible host state\n"); return; }
    if (!std::getenv("RX_PL_AUDIO")) wr(0x009b7430, 1);   /* audio_quiet (below): every case (RX_PL_AUDIO=1 keeps the audio round trips) */
    body();
    g_cases++;
    if (std::getenv("RX_PL_PROBE_LOG")) std::fprintf(stderr, "[pl] case %d %s\n", g_cases, what);
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
uint32_t call_ex(uint32_t fn, uint32_t eax, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi, int nargs, const uint32_t* args) {
    rx_regs in; std::memset(&in, 0, sizeof in); in.r[0] = eax; in.r[1] = ecx; in.r[2] = edx; in.r[6] = esi; in.r[7] = edi;
    rx_regs out; std::memset(&out, 0, sizeof out);
    rx_call_original_ex(fn, &in, &out, nargs, args);
    return out.r[0];
}
uint32_t call_this(uint32_t fn, uint32_t ecx, int nargs = 0, const uint32_t* args = nullptr) { return call_ex(fn, 0, ecx, 0, 0, 0, nargs, args); }
uint32_t world_activity(uint32_t vt) {
    const uint32_t g = rd(0x00a946b8);
    if (!g) return 0;
    const uint32_t w = rd(g + 0x5a54);
    if (!w || !mapped(w + 0x21da4, 8)) return 0;
    for (uint32_t i = 0; i < rd(w + 0x21da8) && i < 16; i++) { const uint32_t a = rd(rd(w + 0x21da4) + 4 * i); if (a && rd(a) == vt) return a; }
    return 0;
}
void put_vehicle(uint32_t driver, float x, float y, float z) {
    const uint32_t v = rd(rd(driver + 0x74) + 0x1ac);
    wf(v + 0xc0, x); wf(v + 0xc4, y); wf(v + 0xc8, z);
}

/* TEST FIXTURE, the synchronous audio round trips. The sound player 0067ead0 (a cue's play 005812f0, the players' sounds) and
 * ActivityAudioLogic::vf33 005810e0 (the logic's flush, which every activity restart vf16 / round start vf15 reaches) post a command
 * to the audio thread and wait for it (Audio_PostCommand, then WaitForSingleObject on a private event): a thread switch no verified call
 * may make, so these calls were unverifiable. audio_quiet sets the audio system's shutdown flag [0x009b7430]: the original's own path
 * for an audio system going down signals the private event at once (00805810 kind 1 / 005810e0: SetEvent instead of the post), so
 * the wait returns without the audio thread while everything else runs as before (both runs see the flag; restored with the case).
 * The cue map (+0x8c of an audio logic, a std::multimap<std::string, ActivityAudioCue*>: node +0xc the key, +0x28 the cue, +0x2d
 * isnil; the head +0x18): cues_add inserts a cue under a name (the original's multimap insert 00586270, EAX = the {key, cue} value;
 * the cue {+4 played 0, +8 "" (no sound name: 005812f0 plays nothing), +0xc no modifier, +0x10 handle -1}) so the cue handlers of
 * the cues the level lacks are subscribed (vf3) and run. */
void audio_quiet() { wr(0x009b7430, 1); }
void cues_add(uint32_t logic, const char* name) {
    const uint32_t map = rd(logic + 0x8c);
    if (!map) return;
    const uint32_t val = halloc(0x20), cue = halloc(0x20), res = halloc(16), cname = guest_string(name);
    for (uint32_t k = 0; k < 0x20; k += 4) { wr(val + k, 0); wr(cue + k, 0); }
    rx_call_original(rd(0x009161ac), RX_THISCALL, val, 1, &cname);   /* std::string(const char*) */
    wr(cue + 8, guest_string("")); wr(cue + 0x10, 0xffffffffu);
    wr(val + 0x1c, cue);
    const uint32_t a[2] = {map, res};
    call_ex(0x00586270, val, 0, 0, 0, 0, 2, a);
}

/* EggHunt (MXActivityEggHunt 0x944e24): the collections, the respawn, the round's end, WindDown, the standings, a new match, the
 * winner's message (the original never sets a winner: forced here), the drivers, the eggs, the audio logic, the teardown */
void probe_egghunt(reflex_vm::Machine& m, uint32_t act) {
    Pages pages(m.mem);
    const uint32_t n = rd(act + 0x26c);
    auto drv = [&](uint32_t i) { return rd(rd(act + 0x268) + 4 * i); };
    auto egg = [&](uint32_t k) { return act + 0xff0d0 + 0x1a0 * k; };
    const uint32_t dt = fb(1.0f / 60.0f);
    std::fprintf(stderr, "[pl] EggHunt %08x drivers %u state %u eggs' zone %u\n", act, n, rd(act + 0x21c), rd(act + 0xff0a8));
    for (uint32_t i = 0; i < n; i++)
        for (uint32_t k : {0u, 3u, 9u}) {
            const uint32_t d = drv(i), e = egg(k);
            run_case(m, pages, "collect: on the egg", [&] { put_vehicle(d, rf(e + 0x60), rf(e + 0x64), rf(e + 0x68)); const uint32_t a[1] = {rd(d + 0x74)}; call_ex(0x00574660, 0, 0, 0, e, d, 1, a); });
            run_case(m, pages, "collect: 3 ft off", [&] { put_vehicle(d, rf(e + 0x60) + 3.0f, rf(e + 0x64), rf(e + 0x68)); const uint32_t a[1] = {rd(d + 0x74)}; call_ex(0x00574660, 0, 0, 0, e, d, 1, a); });
            run_case(m, pages, "collect: collected already", [&] { wr(e + 0x190, 1); put_vehicle(d, rf(e + 0x60), rf(e + 0x64), rf(e + 0x68)); const uint32_t a[1] = {rd(d + 0x74)}; call_ex(0x00574660, 0, 0, 0, e, d, 1, a); });
            run_case(m, pages, "vf28: a rider on an egg", [&] { put_vehicle(d, rf(e + 0x60), rf(e + 0x64), rf(e + 0x68)); call_this(0x00575b60, act, 1, &dt); });
        }
    run_case(m, pages, "collect: no vehicle", [&] { const uint32_t d = drv(0), p = rd(d + 0x74); const uint32_t keep = rd(p + 0x1ac); wr(p + 0x1ac, 0); const uint32_t a[1] = {p}; call_ex(0x00574660, 0, 0, 0, egg(0), d, 1, a); wr(p + 0x1ac, keep); });
    run_case(m, pages, "vf28: the respawn due", [&] { wr(act + 0xff0c0, 1); wf(act + 0xff0c4, 0.001f); call_this(0x00575b60, act, 1, &dt); });
    run_case(m, pages, "vf28: the respawn running", [&] { wr(act + 0xff0c0, 1); wf(act + 0xff0c4, 12.0f); call_this(0x00575b60, act, 1, &dt); });
    run_case(m, pages, "spawn eggs: the last zone", [&] { wr(act + 0xff0a8, 7); call_ex(0x00575440, 0, 0, 0, 0, act, 0, nullptr); });
    run_case(m, pages, "vf42: the clock out", [&] { wf(act + 0x294, rf(act + 0x2bc) + 0.5f); call_this(0x00575eb0, act); });
    run_case(m, pages, "vf42: the clock running", [&] { wf(act + 0x294, 0.0f); call_this(0x00575eb0, act); });
    run_case(m, pages, "vf27: the timer running", [&] { wf(act + 0x100120, 2.0f); call_this(0x00575e00, act, 1, &dt); });
    run_case(m, pages, "vf27: the timer out", [&] { wf(act + 0x100120, 0.001f); call_this(0x00575e00, act, 1, &dt); });
    run_case(m, pages, "vf27: the match over", [&] { wr(act + 0x100118, 1); call_this(0x00575e00, act, 1, &dt); });
    for (uint32_t i = 0; i + 1 < n; i++)
        for (uint32_t tie : {0u, 1u})
            for (int eggs : {0, 2}) run_case(m, pages, "vf43", [&] { wr(drv(i) + 0xf1c, uint32_t(eggs)); const uint32_t a[3] = {drv(i), drv(i + 1), tie}; call_this(0x005758a0, act, 3, a); });
    run_case(m, pages, "vf16: a new match", [&] { wr(act + 0x100124, 1); for (uint32_t i = 0; i < n; i++) wr(drv(i) + 0xf14, 5); call_this(0x00574d40, act); });
    run_case(m, pages, "vf16", [&] { wr(act + 0x100124, 0); call_this(0x00574d40, act); });
    run_case(m, pages, "vf18", [&] { wr(act + 0x10011c, drv(0)); call_this(0x005757b0, act); });
    run_case(m, pages, "vf25", [&] { call_this(0x00575af0, act, 1, &dt); });
    for (uint32_t over : {0u, 1u})
        run_case(m, pages, "vf40: a winner (forced)", [&] { wr(act + 0x10011c, drv(0)); wr(act + 0x100118, over); wr(act + 0x238, 1); const uint32_t a[1] = {2}; call_this(0x005758e0, act, 1, a); });
    run_case(m, pages, "vf40: a tie", [&] { wr(act + 0x238, 1); const uint32_t a[1] = {2}; call_this(0x005758e0, act, 1, a); });
    run_case(m, pages, "vf40: mode 0", [&] { wr(act + 0x238, 0); const uint32_t a[1] = {2}; call_this(0x005758e0, act, 1, a); });
    for (uint32_t fr : {0u, 1u})
        run_case(m, pages, "vf47", [&] { const uint32_t w = rd(act + 0x234); wr(w + 0x22144, fr); const uint32_t a[1] = {drv(0)}; call_this(0x00575ed0, act, 1, a); });
    run_case(m, pages, "vf44", [&] { const uint32_t a[1] = {rd(drv(0) + 0x74)}; call_this(0x00575840, act, 1, a); });
    run_case(m, pages, "vf4", [&] { call_this(0x00574cd0, act); });
    run_case(m, pages, "vf15", [&] { call_this(0x00574d20, act); });
    for (uint32_t i = 0; i < n; i++) {
        run_case(m, pages, "driver vf3", [&] { call_this(0x00516030, drv(i)); });
        run_case(m, pages, "driver vf3, a new match", [&] { wr(act + 0x100124, 1); call_this(0x00516030, drv(i)); });
        run_case(m, pages, "driver vf7", [&] { call_this(0x005160b0, drv(i), 1, &dt); });
    }
    for (uint32_t k : {0u, 5u}) {
        run_case(m, pages, "egg vf19", [&] { call_this(0x00576070, egg(k), 1, &dt); });
        run_case(m, pages, "egg vf19, a big angle", [&] { wf(egg(k) + 0x140, -1234.5f); call_this(0x00576070, egg(k), 1, &dt); });
        run_case(m, pages, "egg vf28", [&] { const uint32_t p = egg(k) + 0x60; call_this(0x00574630, egg(k), 1, &p); });
        run_case(m, pages, "egg vf16", [&] { const uint32_t mtx = egg(k) + 0x30; call_this(0x00576040, egg(k), 1, &mtx); });
    }
    run_case(m, pages, "egg constructor (heap block)", [&] { const uint32_t b = halloc(0x1a0); if (b) call_this(0x0055ec30, b); });
    const uint32_t logic = rd(act + 0xfd8);
    if (logic) {
        run_case(m, pages, "audio vf3", [&] { call_this(0x005848f0, logic); });
        run_case(m, pages, "audio vf4", [&] { call_this(0x00584b70, logic); });
        run_case(m, pages, "audio EggCollected", [&] { const uint32_t a[2] = {act, drv(0)}; call_this(0x00584e10, logic, 2, a); });
        run_case(m, pages, "audio EggSpawned", [&] { const uint32_t a[1] = {act}; call_this(0x00584e30, logic, 1, a); });
    }
    if (logic) {
        /* the cued paths with the fixture cue map and the audio round trips signalled at once (audio_quiet / cues_add above) */
        static const char* names[] = {"Round Over", "Match Over", "EggCollected", "EggSpawned", "Player Wins"};
        auto cued = [&] { call_this(0x00584b70, logic); audio_quiet(); for (const char* nm : names) cues_add(logic, nm); call_this(0x005848f0, logic); };
        run_case(m, pages, "cues: audio vf3 / vf4 with every cue", [&] { cued(); call_this(0x00584b70, logic); });
        run_case(m, pages, "cues: vf15 (Round Started)", [&] { cued(); call_this(0x00574d20, act); });
        run_case(m, pages, "cues: vf16", [&] { cued(); wr(act + 0x100124, 0); call_this(0x00574d40, act); });
        run_case(m, pages, "cues: vf16, a new match", [&] { cued(); wr(act + 0x100124, 1); call_this(0x00574d40, act); });
        run_case(m, pages, "cues: the round's places", [&] { cued(); call_this(0x00574dd0, 0, 1, &act); });
        /* (a winner forced as in the vf40 cases: the original never sets one, and with a "Player Wins" cue its handler 00583fb0 reads the
         * winner's +0x74: a level whose EggHunt cue map held "Player Wins" would fault there in the original) */
        run_case(m, pages, "cues: vf42 / vf27 the clock out", [&] { cued(); wr(act + 0x10011c, drv(0)); wf(act + 0x294, rf(act + 0x2bc) + 0.5f); call_this(0x00575eb0, act); wf(act + 0x100120, 0.001f); call_this(0x00575e00, act, 1, &dt); });
        run_case(m, pages, "cues: the handlers", [&] {
            cued();
            { const uint32_t a[4] = {act, 2, 1, 3}; call_this(0x00584df0, logic, 4, a); }
            { const uint32_t a[1] = {act}; call_this(0x00583a30, logic, 1, a); call_this(0x00583a50, logic, 1, a); call_this(0x00584e30, logic, 1, a); }
            { const uint32_t a[2] = {act, drv(0)}; call_this(0x00584e10, logic, 2, a); }
            { const uint32_t a[1] = {act}; call_ex(0x0058a110, 0, drv(0), act + 0x10, rd(act + 0x290), 0, 1, a); } });
        for (uint32_t i = 0; i < n; i++)
            run_case(m, pages, "cues: collect", [&] { cued(); const uint32_t d = drv(i), e = egg(i % 10); put_vehicle(d, rf(e + 0x60), rf(e + 0x64), rf(e + 0x68)); const uint32_t a[1] = {rd(d + 0x74)}; call_ex(0x00574660, 0, 0, 0, e, d, 1, a); });
        run_case(m, pages, "cues: spawn eggs", [&] { cued(); call_ex(0x00575440, 0, 0, 0, 0, act, 0, nullptr); });
    }
    run_case(m, pages, "player wins", [&] { const uint32_t a[1] = {act}; call_ex(0x0058a110, 0, drv(0), act + 0x10, rd(act + 0x290), 0, 1, a); });
    /* the teardown: vf3 (the drivers leave, the eggs released, the listeners leave), then the scalar deleting destructor */
    if (std::getenv("RX_PL_TEARDOWN")) run_case(m, pages, "teardown: vf3 + sdtor", [&] { wr(0x00a946c0, 0); call_this(0x00574b90, act); const uint32_t one = 1; call_this(0x00574840, act, 1, &one); });
    run_case(m, pages, "teardown: driver sdtor", [&] { const uint32_t one = 1; call_this(0x00515ff0, drv(n - 1), 1, &one); });
    std::fprintf(stderr, "[pl] EggHunt: %d cases done (state restored)\n", g_cases);
}
/* a free subscene bucket of the scene made the area's subscene: its name, an entity array (halloc), in use; 0 when none is free */
uint32_t bucket_make(uint32_t scene, const std::string& name, uint32_t capacity) {
    for (uint32_t b = 0; b < 0x18; b++) {
        const uint32_t rec = scene + 0x38 + 0x110 * b;
        if (rd(rec + 0x10c) || rd(rec + 0x108)) continue;
        wstr(rec, name.c_str());
        wr(rec + 0x104, halloc(4 * capacity));
        wr(rec + 0x108, 0);
        wr(rec + 0x10c, 1);
        return rec;
    }
    return 0;
}
/* a clone (0x1a0 bytes) of a level ModelEntity under a new name, at x, y, z with the identity rotation, appended to the bucket */
uint32_t clone_entity(uint32_t rec, uint32_t src, const char* name, float x, float y, float z) {
    const uint32_t e = halloc(0x1a0);
    if (!e) return 0;
    for (uint32_t k = 0; k < 0x1a0; k += 4) wr(e + k, rd(src + k));
    wr(e + 0x18, guest_string(name));
    for (uint32_t i = 0; i < 16; i++) wf(e + 0x30 + 4 * i, (i % 5) == 0 ? 1.0f : 0.0f);
    wf(e + 0x60, x); wf(e + 0x64, y); wf(e + 0x68, z);
    const uint32_t n = rd(rec + 0x108);
    wr(rd(rec + 0x104) + 4 * n, e);
    wr(rec + 0x108, n + 1);
    return e;
}
/* the first plain ModelEntity (vtable 0x9370d4) with a model in the level's Base subscene */
uint32_t base_prop(uint32_t scene) {
    const uint32_t rec = scene + 0x38;
    for (uint32_t i = 0; i < rd(rec + 0x108); i++) {
        const uint32_t e = rd(rd(rec + 0x104) + 4 * i);
        if (rd(e) == 0x009370d4 && rd(e + 0x100)) return e;
    }
    return 0;
}
/* TEST FIXTURE, Capture: no level has a MiniGameCTF subscene. The area's subscene is made (a free bucket) with six clones of a level
 * prop (a plain ModelEntity with its model): RedFlag / BlueFlag (the flags' model), RedSpawn / BlueSpawn and RedBase / BlueBase 40 ft
 * either side of the prop (RX_PL_CTF_SPREAD), the spawns 10 ft nearer the middle */
void fixture_capture(uint32_t act, uint32_t scene) {
    const std::string area = rstr(act + 0x118);
    uint32_t rec = bucket_named(scene, area);
    if (rec && entity_named(rec, "RedFlag")) return;
    const uint32_t prop = base_prop(scene);
    if (!prop) { std::fprintf(stderr, "[pl] fixture: Capture: no level prop to clone\n"); return; }
    if (!rec) rec = bucket_make(scene, area, 8);
    if (!rec) { std::fprintf(stderr, "[pl] fixture: Capture: no free subscene bucket\n"); return; }
    /* the anchor: the first local player's vehicle (already in the event world), else the prop */
    float x = rf(prop + 0x60), y = rf(prop + 0x64), z = rf(prop + 0x68);
    { const uint32_t g = rd(0x00a946b8), v = g ? rd(g + 0x7460 + 0x1ac) : 0;
      if (v && mapped(v + 0xc0, 12)) { x = rf(v + 0xc0); y = rf(v + 0xc4); z = rf(v + 0xc8); } }
    const float s = std::getenv("RX_PL_CTF_SPREAD") ? float(std::atof(std::getenv("RX_PL_CTF_SPREAD"))) : 40.0f;
    clone_entity(rec, prop, "RedFlag", x - s, y, z);
    clone_entity(rec, prop, "BlueFlag", x + s, y, z);
    clone_entity(rec, prop, "RedSpawn", x - s + 10.0f, y, z + 6.0f);
    clone_entity(rec, prop, "BlueSpawn", x + s - 10.0f, y, z + 6.0f);
    clone_entity(rec, prop, "RedBase", x - s, y, z);
    clone_entity(rec, prop, "BlueBase", x + s, y, z);
    std::fprintf(stderr, "[pl] fixture: Capture: subscene '%s' made with six clones of %08x '%s' (model %08x) at %.1f %.1f %.1f +- %.0f (TEST FIXTURE)\n",
                 area.c_str(), prop, rstr(rd(prop + 0x18), 64).c_str(), rd(prop + 0x100), x, y, z, s);
}
}  // namespace

/* Capture (MXActivityCapture 0x944fd4): the grabs, the carries, the drops, the scores, the returns, the crashes, the match's end, the
 * starts, the drivers, the rule, the teardown */
uint32_t capture_rule(uint32_t d) {
    for (uint32_t k = 0; k < rd(d + 0xee4) && k < 18; k++) { const uint32_t r = rd(d + 0xa0 + 4 * k); if (r && rd(r) == 0x0093ffc0) return r; }
    return 0;
}
void probe_capture(reflex_vm::Machine& m, uint32_t act) {
    Pages pages(m.mem);
    const uint32_t n = rd(act + 0x26c);
    auto drv = [&](uint32_t i) { return rd(rd(act + 0x268) + 4 * i); };
    const uint32_t dt = fb(1.0f / 60.0f);
    const uint32_t red = act + 0xff070, blue = act + 0xff200;
    std::fprintf(stderr, "[pl] Capture %08x drivers %u state %u scores %u / %u\n", act, n, rd(act + 0x21c), rd(act + 0xff424), rd(act + 0xff428));
    auto at = [&](uint32_t d, float x, float y, float z) { put_vehicle(d, x, y, z); };
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t d = drv(i), r = capture_rule(d), team = rd(d + 0xf14);
        if (!r) { std::fprintf(stderr, "[pl] driver %08x has no RuleCapture\n", d); continue; }
        const uint32_t enemy = team ? red : blue, own = team ? blue : red;
        const uint32_t ebase = act + (team ? 0xff3d0 : 0xff410), obase = act + (team ? 0xff410 : 0xff3d0);
        run_case(m, pages, "rule: a grab", [&] { wf(r + 0x10, 5.0f); at(d, rf(enemy + 0x60), rf(enemy + 0x64), rf(enemy + 0x68)); call_this(0x0051cab0, r, 1, &dt); });
        run_case(m, pages, "rule: the grab clock running", [&] { wf(r + 0x10, 2.0f); at(d, rf(enemy + 0x60), rf(enemy + 0x64), rf(enemy + 0x68)); call_this(0x0051cab0, r, 1, &dt); });
        run_case(m, pages, "rule: far from everything", [&] { wf(r + 0x10, 5.0f); at(d, rf(ebase) + 900.0f, rf(ebase + 4), rf(ebase + 8)); call_this(0x0051cab0, r, 1, &dt); });
        run_case(m, pages, "rule: at its own base, the clock out", [&] { wf(r + 0x10, 5.0f); wf(r + 0x18, 4.999f); at(d, rf(obase), rf(obase + 4), rf(obase + 8)); call_this(0x0051cab0, r, 1, &dt); });
        run_case(m, pages, "rule: near the enemy base, an enemy close", [&] {
            wf(r + 0x10, 5.0f); at(d, rf(ebase) + 20.0f, rf(ebase + 4), rf(ebase + 8));
            for (uint32_t j = 0; j < n; j++) if (rd(drv(j) + 0xf14) != team) at(drv(j), rf(ebase) + 22.0f, rf(ebase + 4), rf(ebase + 8));
            call_this(0x0051cab0, r, 1, &dt); });
        run_case(m, pages, "rule: carrying", [&] { wr(r + 0xc, 1); rx_wr8(d + 0xf1c, 1); call_this(0x0051cab0, r, 1, &dt); });
        run_case(m, pages, "rule: carrying, wrecked", [&] { wr(r + 0xc, 1); rx_wr8(d + 0xf1c, 1); wr(act + (team ? 0xff398 : 0xff39c), d); wr(rd(rd(d + 0x74) + 0x1ac) + 0x280, 1); call_this(0x0051cab0, r, 1, &dt); });
        run_case(m, pages, "rule: carrying home, its flag home: a score", [&] { wr(r + 0xc, 1); rx_wr8(d + 0xf1c, 1); rx_wr8(act + (team ? 0xff431 : 0xff430), 1); at(d, rf(obase), rf(obase + 4), rf(obase + 8)); call_this(0x0051cab0, r, 1, &dt); });
        run_case(m, pages, "rule: carrying home, its flag away", [&] { wr(r + 0xc, 1); rx_wr8(d + 0xf1c, 1); rx_wr8(act + (team ? 0xff431 : 0xff430), 0); at(d, rf(obase), rf(obase + 4), rf(obase + 8)); call_this(0x0051cab0, r, 1, &dt); });
        run_case(m, pages, "rule: its own flag dropped, returned", [&] {
            wf(r + 0x10, 5.0f); const uint32_t ox = rf(own + 0x60) + 100.0f > 0 ? 0 : 0; (void)ox;
            uint32_t carrier = 0; for (uint32_t j = 0; j < n; j++) if (rd(drv(j) + 0xf14) != team) carrier = drv(j);
            if (carrier) wr(act + (team ? 0xff39c : 0xff398), carrier);
            rx_wr8(act + (team ? 0xff431 : 0xff430), 0);
            at(d, rf(own + 0x60), rf(own + 0x64), rf(own + 0x68)); call_this(0x0051cab0, r, 1, &dt); });
        run_case(m, pages, "rule vf1", [&] { const uint32_t a[1] = {d}; call_this(0x0051ca80, r, 1, a); });
        run_case(m, pages, "rule lookup", [&] { const uint32_t a[1] = {d}; call_ex(0x0053b290, 0, 0, 0, 0, 0, 1, a); });
        run_case(m, pages, "driver vf3", [&] { call_this(0x00516200, d); });
        run_case(m, pages, "driver vf7", [&] { call_this(0x005162a0, d, 1, &dt); });
        run_case(m, pages, "driver vf7, the grab clock below 5", [&] { wf(r + 0x10, 1.0f); call_this(0x005162a0, d, 1, &dt); });
        run_case(m, pages, "driver vf9 (the rule)", [&] { call_this(0x005164d0, d, 1, &r); });
        run_case(m, pages, "driver vf9 (another rule)", [&] { const uint32_t o = rd(d + 0xa0); call_this(0x005164d0, d, 1, &o); });
        run_case(m, pages, "driver human rules", [&] { const uint32_t a[2] = {d, dt}; call_ex(0x00516490, 0, 0, 0, 0, 0, 2, a); });
        if (rd(rd(d + 0x74) + 0x10) == 1) run_case(m, pages, "driver AI", [&] { call_ex(0x00516380, 0, 0, 0, d, 0, 1, &dt); });
    }
    for (uint32_t t = 0; t < 3; t++) {
        run_case(m, pages, "score", [&] { call_ex(0x00577c30, 0, 0, 0, act, 0, 1, &t); });
        run_case(m, pages, "base position", [&] { const uint32_t out = act + 0x101488; call_ex(0x00577ad0, out, 0, t, 0, 0, 1, &act); });
        run_case(m, pages, "flag home", [&] { call_ex(0x00577b80, t, 0, 0, 0, 0, 1, &act); });
    }
    for (int s = 0; s < 3; s++)
        run_case(m, pages, "vf42", [&] { wr(act + 0xff424, s == 1 ? 30 : 3); wr(act + 0xff428, s == 2 ? 31 : 4); call_this(0x00577280, act); });
    for (uint32_t i = 0; i + 1 < n; i++) for (uint32_t tie : {0u, 1u}) run_case(m, pages, "vf43", [&] { const uint32_t a[3] = {drv(i), drv(i + 1), tie}; call_this(0x00577240, act, 3, a); });
    for (uint32_t sp : {0u, 1u}) run_case(m, pages, "vf38", [&] { wr(act + 0xfe8, sp); const uint32_t a[1] = {1}; call_this(0x005772b0, act, 1, a); });
    run_case(m, pages, "vf19", [&] { const uint32_t o = act + 0x101488, a[4] = {rd(drv(0) + 0x74), o, o + 0x10, o + 0x20}; call_this(0x00577d20, act, 4, a); });
    run_case(m, pages, "vf27", [&] { call_this(0x00577b60, act, 1, &dt); });
    run_case(m, pages, "vf16", [&] { call_this(0x00577a60, act); });
    run_case(m, pages, "vf18", [&] { wr(act + 0xff398, drv(0)); call_this(0x00577010, act); });
    run_case(m, pages, "vf15", [&] { call_this(0x00577a00, act); });
    run_case(m, pages, "vf4", [&] { call_this(0x00576ab0, act); });
    run_case(m, pages, "place starts", [&] { call_this(0x00576b60, 0, 1, &act); });
    for (uint32_t fr : {0u, 1u}) run_case(m, pages, "vf47", [&] { wr(rd(act + 0x234) + 0x22144, fr); const uint32_t a[1] = {drv(0)}; call_this(0x00577200, act, 1, a); });
    run_case(m, pages, "vf44", [&] { const uint32_t a[1] = {rd(drv(0) + 0x74)}; call_this(0x005770f0, act, 1, a); });
    run_case(m, pages, "rule create", [&] { call_this(0x0058a2f0, drv(0)); });
    run_case(m, pages, "smoke constructor (heap block)", [&] { const uint32_t b = halloc(0x2b0); if (b) call_this(0x005764e0, b); });
    for (uint32_t ty : {0x10u, 0xbu}) run_case(m, pages, "results columns", [&] { const uint32_t a[2] = {halloc(32), ty}; call_ex(0x00614890, halloc(32), drv(0), 0, halloc(32), 0, 2, a); });
    if (const uint32_t logic = rd(act + 0xfd8)) {   /* the restart / round start paths with the audio round trips signalled at once (audio_quiet) */
        run_case(m, pages, "audio quiet: vf15", [&] { audio_quiet(); call_this(0x00577a00, act); });
        run_case(m, pages, "audio quiet: vf16", [&] { audio_quiet(); call_this(0x00577a60, act); });
        for (uint32_t sp : {0u, 1u}) run_case(m, pages, "audio quiet: vf38", [&] { audio_quiet(); wr(act + 0xfe8, sp); const uint32_t a[1] = {1}; call_this(0x005772b0, act, 1, a); });
    }
    if (std::getenv("RX_PL_TEARDOWN")) run_case(m, pages, "teardown: vf3 + sdtor", [&] { wr(0x00a946c0, 0); call_this(0x00576f40, act); const uint32_t one = 1; call_this(0x005764c0, act, 1, &one); });
    run_case(m, pages, "teardown: driver sdtor", [&] { const uint32_t one = 1; call_this(0x005161d0, drv(n - 1), 1, &one); });
    std::fprintf(stderr, "[pl] Capture: %d cases done (state restored)\n", g_cases);
}

/* TEST FIXTURE, Graffiti: no level has a MiniGameGraffiti subscene nor a StartEntityFreestyle / StartEntityFreeRide in the subscenes a
 * Graffiti event loads. The area's subscene is made (a free bucket) with two FreeRideStartEntity stand-ins (a level prop's first 0xa0
 * bytes with the class's vtable 0x937ccc: only its matrix (vf14) and type (vf22, 0x1014) are read) 20 ft apart at the first local
 * player's vehicle; the round's duration 60 s (RX_PL_DURATION; the row's Duration, 0 without a row). No capture point (type 0x1040: a
 * scene entity built from RSData) is made. */
/* the original's XMLBuilder (as 005daae0 builds the UIManager's RSData): the reader the scene loader puts in [0x00a9583c] */
struct Rs {
    uint32_t b = 0, ent = 0;
    uint32_t node(uint32_t parent, uint32_t tag) { const uint32_t a[2] = {parent, tag}; return call_this(0x00877360, b, 2, a); }   /* vf81 */
    void attr(uint32_t n, uint32_t name, uint32_t value) { const uint32_t a[3] = {n, name, value}; call_this(0x00877500, b, 3, a); } /* vf94 */
    uint32_t field(uint32_t name) { const uint32_t r = node(ent, 0x00937014 /* "RSData" */); attr(r, 0x009362a4 /* "name" */, name); return r; }
    void str(uint32_t name, const char* v) { const uint32_t t = node(field(name), 0x0094efc8 /* "String" */); const uint32_t a[2] = {t, guest_string(v)}; call_this(0x008795a0, b, 2, a); }
    void asset(uint32_t name, const char* type, const char* v) { const uint32_t t = node(field(name), guest_string("Asset")); attr(t, 0x009362a4 /* "name" */, guest_string(v)); attr(t, guest_string("type"), guest_string(type)); }
    void boolean(uint32_t name, uint32_t v) { const uint32_t t = node(field(name), 0x0094fc84 /* "Bool" */); const uint32_t vp = halloc(16); wr(vp, v); const uint32_t a[3] = {t, vp, 1}; call_this(0x008798a0, b, 3, a); }
    void matrix(float x, float y, float z) {
        const uint32_t t = node(field(0x0094fc70 /* "Matrix" */), 0x0094fc70), mp = halloc(64);
        for (uint32_t i = 0; i < 16; i++) wf(mp + 4 * i, (i % 5) == 0 ? 1.0f : 0.0f);
        wf(mp + 0x30, x); wf(mp + 0x34, y); wf(mp + 0x38, z);
        const uint32_t a[3] = {t, mp, 1}; call_this(0x0087a040, b, 3, a);
    }
    Rs() { b = halloc(0x2ac0); call_this(0x00874300, b); ent = node(0, 0x0094fc5c /* "Entity" */); }
    void bind() { const uint32_t a[1] = {ent}; wr(0x00a9583c, b); call_this(0x00877be0, b, 1, a); }   /* vf3: the current node */
};
/* a GraffitiCapturePoint as the scene loader builds one (004046a0 case 0x1040: 00401f90(0xcb0), 004075c0; then vf5 on its RSData) */
std::string g_point_model;   /* the points' ModelAsset: a level prop's model (its handle's name +4) */
uint32_t make_point(const char* name, const char* trigger, int counted, float x, float y, float z) {
    const uint32_t sz = 0xcb0, p = rx_call_original(0x00401f90, RX_CDECL, 0, 1, &sz);
    if (!p) return 0;
    call_ex(0x004075c0, 0, 0, 0, 0, p, 0, nullptr);
    Rs rs;
    rs.str(0x0093a3bc /* "Name" */, name);
    rs.asset(guest_string("ModelAsset"), "model", g_point_model.c_str());
    (void)rs.field(guest_string("MaterialList"));   /* an empty list (ModelInstance_Init's 0080ed50 walks the field's children) */
    rs.matrix(x, y, z);
    if (trigger) rs.str(0x00935eb4 /* "LinkedTriggerID" */, trigger);
    if (counted >= 0) rs.boolean(0x00935ec4 /* "CountTowardCapPointTotal" */, uint32_t(counted));
    const uint32_t keep = rd(0x00a9583c);
    rs.bind();
    call_this(0x00407780, p, 1, &rs.b);   /* vf5 */
    wr(0x00a9583c, keep);
    return p;
}
void fixture_graffiti(uint32_t act, uint32_t scene) {
    const std::string area = rstr(act + 0x118);
    uint32_t rec = bucket_named(scene, area);
    if (rec && entity_named(rec, "GraffitiStartA")) return;
    const uint32_t prop = base_prop(scene);
    if (!prop) { std::fprintf(stderr, "[pl] fixture: Graffiti: no level prop to clone\n"); return; }
    if (!rec) rec = bucket_make(scene, area, 16);
    if (!rec) { std::fprintf(stderr, "[pl] fixture: Graffiti: no free subscene bucket\n"); return; }
    float x = rf(prop + 0x60), y = rf(prop + 0x64), z = rf(prop + 0x68);
    { const uint32_t g = rd(0x00a946b8), v = g ? rd(g + 0x7460 + 0x1ac) : 0;
      if (v && mapped(v + 0xc0, 12)) { x = rf(v + 0xc0); y = rf(v + 0xc4); z = rf(v + 0xc8); } }
    for (int k = 0; k < 2; k++) {
        const uint32_t e = clone_entity(rec, prop, k ? "GraffitiStartB" : "GraffitiStartA", x + (k ? 10.0f : -10.0f), y, z);
        if (e) wr(e, 0x00937ccc);   /* FreeRideStartEntity */
    }
    if (!(rf(act + 0x2bc) > 0.0f)) wf(act + 0x2bc, std::getenv("RX_PL_DURATION") ? float(std::atof(std::getenv("RX_PL_DURATION"))) : 60.0f);
    std::fprintf(stderr, "[pl] fixture: Graffiti: subscene '%s' made with two FreeRideStartEntity stand-ins at %.1f %.1f %.1f (TEST FIXTURE)\n", area.c_str(), x, y, z);
    if (std::getenv("RX_PL_NO_POINTS")) return;
    /* the capture points (GraffitiCapturePointRSData, type 0x1040): the level's first two MXTriggers (vtable 0x9376cc, any subscene; or,
     * as on FR_Alpine whose Graffiti subscenes hold none, two built by the original's constructor 00409800 and named) also listed in the
     * area's subscene (the point's LinkedTriggerID is looked up there: 00408400), and three points built as the scene
     * loader builds them on a fabricated RSData: A linked to the first trigger and counted, B linked to the second (no
     * CountTowardCapPointTotal field: counted by default), C linked to no trigger and not counted */
    std::vector<uint32_t> trig;
    for (uint32_t b = 0; b < 0x18 && trig.size() < 2; b++) {
        const uint32_t r = scene + 0x38 + 0x110 * b;
        if (!rd(r + 0x10c) || r == rec) continue;
        for (uint32_t i = 0; i < rd(r + 0x108) && trig.size() < 2; i++) { const uint32_t e = rd(rd(r + 0x104) + 4 * i); if (rd(e) == 0x009376cc) trig.push_back(e); }
    }
    while (trig.size() < 2) {   /* no level trigger in the subscenes a Graffiti event loads: an MXTrigger built by its constructor 00409800 */
        const uint32_t t = halloc(0x280);
        if (!t) break;
        rx_call_original(0x00409800, RX_STDCALL, 0, 1, &t);
        trig.push_back(t);
    }
    for (size_t k = 0; k < trig.size(); k++)   /* a trigger without a TriggerID (+0xc0) gets one */
        if (rstr(trig[k] + 0xc0, 64).empty()) wstr(trig[k] + 0xc0, k ? "GraffitiTriggerB" : "GraffitiTriggerA");
    auto add = [&](uint32_t e) { const uint32_t n = rd(rec + 0x108); wr(rd(rec + 0x104) + 4 * n, e); wr(rec + 0x108, n + 1); };
    for (uint32_t t : trig) add(t);
    g_point_model = rstr(rd(prop + 0x100) + 4, 64);
    const std::string t0 = trig.size() > 0 ? rstr(trig[0] + 0xc0, 64) : "NoTrigger", t1 = trig.size() > 1 ? rstr(trig[1] + 0xc0, 64) : t0;
    const uint32_t pa = make_point("GraffitiPointA", t0.c_str(), 1, x - 15.0f, y, z);
    const uint32_t pb = make_point("GraffitiPointB", t1.c_str(), -1, x + 15.0f, y, z);
    const uint32_t pc = make_point("GraffitiPointC", "NoSuchTrigger", 0, x, y, z + 15.0f);
    for (uint32_t pt : {pa, pb, pc}) if (pt) add(pt);
    std::fprintf(stderr, "[pl] fixture: Graffiti: %zu triggers ('%s', '%s') listed in the area, points %08x %08x %08x (counted %u %u %u, model '%s') (TEST FIXTURE)\n",
                 trig.size(), t0.c_str(), t1.c_str(), pa, pb, pc, pa ? rd(pa + 0xb44) : 0, pb ? rd(pb + 0xb44) : 0, pc ? rd(pc + 0xb44) : 0, g_point_model.c_str());
}

/* the capture points (GraffitiCapturePoint 0x938b9c) of the fixture: the trigger events, the stunts, the captures, the records, the tick,
 * the events through their channels (the proxies), the destructors. Each case calls the original functions; Verify runs the core's. */
void probe_graffiti_points(reflex_vm::Machine& m, Pages& pages, uint32_t act) {
    const uint32_t n = rd(act + 0x26c), np = rd(act + 0xff074);
    auto drv = [&](uint32_t i) { return rd(rd(act + 0x268) + 4 * i); };
    auto pl = [&](uint32_t i) { return rd(drv(i) + 0x74); };
    auto veh = [&](uint32_t i) { return rd(pl(i) + 0x1ac); };
    const uint32_t dt = fb(1.0f / 60.0f);
    const uint32_t scene = rd(rd(act + 0x234) + 0x20138), rec = bucket_named(scene, rstr(act + 0x118));
    auto trigger_of = [&](uint32_t p) -> uint32_t {
        for (uint32_t i = 0; rec && i < rd(rec + 0x108); i++) { const uint32_t e = rd(rd(rec + 0x104) + 4 * i); if (rd(e) == 0x009376cc && rstr(e + 0xc0, 64) == rstr(p + 0xb00, 64)) return e; }
        return 0;
    };
    uint32_t other = 0;   /* a trigger that is not the point's */
    for (uint32_t i = 0; rec && i < rd(rec + 0x108) && !other; i++) { const uint32_t e = rd(rd(rec + 0x104) + 4 * i); if (rd(e) == 0x009376cc) other = e; }
    std::fprintf(stderr, "[pl] Graffiti points %u\n", np);
    auto enter = [&](uint32_t p, uint32_t i, uint32_t t) { const uint32_t a[2] = {veh(i), t}; call_this(0x00407af0, p, 2, a); };
    auto exitv = [&](uint32_t p, uint32_t i, uint32_t t) { const uint32_t a[2] = {veh(i), t}; call_this(0x00407c00, p, 2, a); };
    auto score = [&](uint32_t p, uint32_t i, float sc) { const uint32_t a[2] = {pl(i), fb(sc)}; call_this(0x00408180, p, 2, a); };
    auto begins = [&](uint32_t p, uint32_t i) { const uint32_t a[3] = {pl(i), 3, 7}; call_this(0x004081c0, p, 3, a); };
    auto tick = [&](uint32_t p) { call_this(0x00407ec0, p, 1, &dt); };
    if (!other) { std::fprintf(stderr, "[pl] Graffiti points: no MXTrigger in the area: the trigger cases are skipped\n"); }
    for (uint32_t k = 0; k < np && other; k++) {
        const uint32_t p = rd(rd(act + 0xff070) + 4 * k), t = trigger_of(p), wrong = other && other != t ? other : 0;
        run_case(m, pages, "point vf19 (no riders)", [&] { tick(p); });
        for (uint32_t i = 0; i < n; i++) {
            run_case(m, pages, "point entered", [&] { enter(p, i, t ? t : other); tick(p); });
            if (wrong) run_case(m, pages, "point entered another trigger", [&] { enter(p, i, wrong); });
            run_case(m, pages, "point entered twice, exited, ticked", [&] { enter(p, i, t ? t : other); enter(p, i, t ? t : other); tick(p); exitv(p, i, t ? t : other); tick(p); });
            run_case(m, pages, "point entered in a stunt", [&] { wr(rd(veh(i) + 0x1a0) + 0xaf10, 1); enter(p, i, t ? t : other); tick(p); });
            run_case(m, pages, "point capture: a stunt and its score", [&] { enter(p, i, t ? t : other); begins(p, i); tick(p); score(p, i, 1500.0f); tick(p); });
            run_case(m, pages, "point capture taken over, a lower score, a higher one", [&] {
                const uint32_t j = (i + 1) % n;
                enter(p, i, t ? t : other); score(p, i, 1200.5f);
                enter(p, j, t ? t : other); score(p, j, 900.0f);
                enter(p, j, t ? t : other); score(p, j, 2400.0f);
                exitv(p, i, t ? t : other); exitv(p, j, t ? t : other); tick(p); });
            run_case(m, pages, "point score without a record", [&] { score(p, i, 5000.0f); });
            run_case(m, pages, "point stunt failed / kicked out", [&] {
                enter(p, i, t ? t : other); begins(p, i); { const uint32_t a[1] = {pl(i)}; call_this(0x004081e0, p, 1, a); }
                enter(p, i, t ? t : other); { const uint32_t a[2] = {pl(i), 0}; call_this(0x00408200, p, 2, a); } tick(p); });
            run_case(m, pages, "point capture by a vehicle (00408110)", [&] { const uint32_t a[2] = {veh(i), 777}; call_ex(0x00408110, p, 0, 0, 0, 0, 2, a); });
            run_case(m, pages, "point holder set / released (00407970)", [&] { call_ex(0x00407970, veh(i), 0, 0, p, 0, 0, nullptr); call_ex(0x00407970, 0, 0, 0, p, 0, 0, nullptr); });
            run_case(m, pages, "point entered, the driver networked (an invalid reference)", [&] { const uint32_t r = halloc(64); for (uint32_t q = 0; q < 64; q += 4) wr(r + q, 0); wr(drv(i) + 0xef0, r); enter(p, i, t ? t : other); tick(p); });
        }
        run_case(m, pages, "point records full: grown (0041a530)", [&] { wr(p + 0xb50, 1); for (uint32_t i = 0; i < n; i++) enter(p, i, t ? t : other); tick(p); });
        run_case(m, pages, "point records grown directly", [&] { enter(p, 0, t ? t : other); const uint32_t c = 40; call_ex(0x0041a530, 0, 0, 0, p + 0xb48, 0, 1, &c); });
        run_case(m, pages, "point capture, a NaN / negative / huge score", [&] {
            enter(p, 0, t ? t : other); score(p, 0, -50.0f); enter(p, 0, t ? t : other); score(p, 0, 3.0e9f);
            enter(p, 0, t ? t : other); { const uint32_t a[2] = {pl(0), 0x7fc00000u}; call_this(0x00408180, p, 2, a); } });
        run_case(m, pages, "point Activity Init", [&] { enter(p, 0, t ? t : other); score(p, 0, 100.0f); call_this(0x00407930, p, 1, &act); });
        run_case(m, pages, "point capture (00408090) / offer (00407f10) directly", [&] {
            enter(p, 0, t ? t : other); const uint32_t r = rd(p + 0xb48);
            { const uint32_t a[2] = {r, 10}; call_ex(0x00407f10, p, 0, 0, 0, 0, 2, a); }
            { const uint32_t a[2] = {r, 5}; call_ex(0x00407f10, p, 0, 0, 0, 0, 2, a); }
            { const uint32_t a[2] = {r, 20}; call_ex(0x00408090, p, 0, 0, 0, 0, 2, a); } });
        run_case(m, pages, "point records: lookup, removal, prune", [&] {
            for (uint32_t i = 0; i < n; i++) enter(p, i, t ? t : other);
            { const uint32_t a[2] = {p, veh(n - 1)}; call_ex(0x00407a30, 0, 0, 0, 0, 0, 2, a); }
            { const uint32_t a[2] = {p, 0x1234}; call_ex(0x00407a30, 0, 0, 0, 0, 0, 2, a); }
            { const uint32_t a[1] = {veh(0)}; call_ex(0x00407e00, p, 0, 0, 0, 0, 1, a); }
            for (uint32_t i = 0; i < n; i++) exitv(p, i, t ? t : other);
            call_ex(0x00407e60, p, 0, 0, 0, 0, 0, nullptr); });
        run_case(m, pages, "point add rider (00407a70) / record state (00407d10)", [&] {
            call_ex(0x00407a70, veh(0), 0, 0, 0, 0, 1, &p);
            if (rd(p + 0xb4c)) { call_ex(0x00407d10, 0, p, 0, rd(p + 0xb48), 0, 0, nullptr); wr(rd(p + 0xb48) + 4, 1); call_ex(0x00407d10, 0, p, 0, rd(p + 0xb48), 0, 0, nullptr); } });
        run_case(m, pages, "driver captured (0050e0a0)", [&] { const uint32_t a[1] = {250}; call_ex(0x0050e0a0, drv(0), 0, 0, 0, 0, 1, a); });
        /* the events through their channels (00591080-style dispatch 004020c0: EAX = channel, EDI = name; stdcall payload, size): the
         * listeners' proxies call the point's handlers (0041a0d0 / 006c57d0 / 006c5960 / 006c55a0) */
        run_case(m, pages, "events through the channels", [&] {
            const uint32_t pay = halloc(32);
            wr(pay, veh(0)); wr(pay + 4, t ? t : other); call_ex(0x004020c0, rd(0x00a89bc8), 0, 0, 0, 0x00935e84 /* VehicleEntered */, 2, std::array<uint32_t, 2>{pay, 8}.data());
            wr(pay, pl(0)); wr(pay + 4, 3); wr(pay + 8, 7); call_ex(0x004020c0, rd(0x00a94318), 0, 0, 0, 0x0092fd00 /* StuntBegins */, 2, std::array<uint32_t, 2>{pay, 12}.data());
            wr(pay, pl(0)); wf(pay + 4, 1999.0f); call_ex(0x004020c0, rd(0x00a94318), 0, 0, 0, 0x0092fc60 /* StuntFinalScore */, 2, std::array<uint32_t, 2>{pay, 8}.data());
            wr(pay, veh(1 % n)); wr(pay + 4, t ? t : other); call_ex(0x004020c0, rd(0x00a89bc8), 0, 0, 0, 0x00935e84, 2, std::array<uint32_t, 2>{pay, 8}.data());
            wr(pay, veh(1 % n)); wr(pay + 4, t ? t : other); call_ex(0x004020c0, rd(0x00a89bc8), 0, 0, 0, 0x00935e94 /* VehicleExited */, 2, std::array<uint32_t, 2>{pay, 8}.data());
            wr(pay, pl(1 % n)); call_ex(0x004020c0, rd(0x00a94318), 0, 0, 0, 0x0092fd20 /* StuntFailed */, 2, std::array<uint32_t, 2>{pay, 4}.data());
            wr(pay, act); call_ex(0x004020c0, rd(0x00a93f74), 0, 0, 0, 0x00935ea4 /* Activity Init */, 2, std::array<uint32_t, 2>{pay, 4}.data()); });
        run_case(m, pages, "point destructor (00407670, flags 1)", [&] { const uint32_t one = 1; call_this(0x00407670, p, 1, &one); });
        run_case(m, pages, "point destructor (IRenderable 0041b170, flags 0)", [&] { const uint32_t zero = 0; call_this(0x0041b170, p + 0x90, 1, &zero); });
    }
    /* a point built in place (the factory's constructor and vf5 on a fabricated RSData without fields, then one with them) */
    run_case(m, pages, "point constructor + vf5 (no fields)", [&] { make_point("GraffitiPointD", nullptr, -1, 0.0f, 0.0f, 0.0f); });
    run_case(m, pages, "point constructor + vf5 (all fields)", [&] { make_point("GraffitiPointE", "NoSuchTrigger", 1, 1.0f, 2.0f, 3.0f); });
}

/* Graffiti (MXActivityGraffiti 0x94497c): the round's end, the standings, the message, the drivers, the audio logic, a capture point
 * stand-in for the activity's calls (a zeroed heap block), the teardown */
void probe_graffiti(reflex_vm::Machine& m, uint32_t act) {
    Pages pages(m.mem);
    const uint32_t n = rd(act + 0x26c);
    auto drv = [&](uint32_t i) { return rd(rd(act + 0x268) + 4 * i); };
    const uint32_t dt = fb(1.0f / 60.0f);
    std::fprintf(stderr, "[pl] Graffiti %08x drivers %u state %u points %u\n", act, n, rd(act + 0x21c), rd(act + 0xff074));
    run_case(m, pages, "vf42: the clock out", [&] { wf(act + 0x294, rf(act + 0x2bc) + 1.0f); call_this(0x005648c0, act); });
    run_case(m, pages, "vf42: the clock running", [&] { wf(act + 0x294, 0.0f); call_this(0x005648c0, act); });
    for (uint32_t i = 0; i + 1 < n; i++)
        for (uint32_t tie : {0u, 1u})
            for (int c = 0; c < 4; c++)
                run_case(m, pages, "vf43", [&] { wr(drv(i) + 0xf14, c & 1); wr(drv(i) + 0xf1c, (c & 2) ? 5 : 0); const uint32_t a[3] = {drv(i), drv(i + 1), tie}; call_this(0x005644f0, act, 3, a); });
    if (rd(act + 0x2d0)) run_case(m, pages, "vf40", [&] { const uint32_t a[1] = {2}; call_this(0x00564560, act, 1, a); });
    run_case(m, pages, "vf18", [&] { call_this(0x00564450, act); });
    run_case(m, pages, "vf15", [&] { call_this(0x00564960, act); });
    run_case(m, pages, "vf23", [&] { call_this(0x005646c0, act, 1, &dt); });
    run_case(m, pages, "vf44", [&] { const uint32_t a[1] = {rd(drv(0) + 0x74)}; call_this(0x005644c0, act, 1, a); });
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t d = drv(i);
        run_case(m, pages, "driver vf1", [&] { call_this(0x0050df80, d); });
        run_case(m, pages, "driver vf3", [&] { call_this(0x0050dfa0, d); });
        run_case(m, pages, "driver vf7", [&] { call_this(0x0050e030, d, 1, &dt); });
        run_case(m, pages, "driver vf7, the capture clock out", [&] { wf(d + 0xf24, 0.001f); wr(d + 0xf20, 1); call_this(0x0050e030, d, 1, &dt); });
        run_case(m, pages, "driver vf7, the capture clock running", [&] { wf(d + 0xf24, 2.0f); wr(d + 0xf20, 1); call_this(0x0050e030, d, 1, &dt); });
        run_case(m, pages, "driver of its vehicle", [&] { const uint32_t a[1] = {rd(rd(d + 0x74) + 0x1ac)}; call_ex(0x00564970, 0, 0, 0, 0, act, 1, a); });
    }
    run_case(m, pages, "driver of no vehicle", [&] { const uint32_t a[1] = {0x12345678}; call_ex(0x00564970, 0, 0, 0, 0, act, 1, a); });
    /* a capture point stand-in (zeroed; the activity's array holds it) for vf18 / driver vf17 / the trigger lookup / the label's skip */
    auto stand_in = [&]() {
        const uint32_t p = halloc(0x1000);
        for (uint32_t k = 0; p && k < 0x1000; k += 4) wr(p + k, 0);
        if (!p) return 0u;
        wr(p + 0xb80, act);
        const uint32_t arr = halloc(16);
        wr(arr, p); wr(act + 0xff070, arr); wr(act + 0xff074, 1); wr(act + 0xff078, 4);
        return p;
    };
    run_case(m, pages, "vf18 with a point", [&] { const uint32_t p = stand_in(); if (p) { wr(p + 0xb5c, 1); call_this(0x00564450, act); } });
    for (uint32_t i = 0; i < n; i++)
        run_case(m, pages, "driver vf17 holding a point", [&] { const uint32_t p = stand_in(); if (p) { wr(p + 0xb5c, rd(rd(drv(i) + 0x74) + 0x1ac)); call_this(0x0050e120, drv(i)); } });
    run_case(m, pages, "point trigger (no match)", [&] { const uint32_t p = stand_in(); if (p) { wstr(p + 0xb00, "NoSuchTrigger"); const uint32_t a[1] = {p}; call_ex(0x00408400, 0, 0, 0, 0, 0, 1, a); } });
    run_case(m, pages, "point label (not counted)", [&] { const uint32_t p = stand_in(); if (p) call_ex(0x00408220, 0, 0, 0, 0, p, 0, nullptr); });
    const uint32_t logic = rd(act + 0xfd8);
    if (logic) {
        run_case(m, pages, "audio vf19", [&] { const uint32_t a[2] = {0, 0}; call_this(0x00582010, logic, 2, a); });
        run_case(m, pages, "audio vf30", [&] { call_this(0x005824f0, logic); });
        run_case(m, pages, "audio vf29", [&] { call_this(0x005824f0, logic); const uint32_t a[1] = {act}; call_this(0x00582490, logic, 1, a); });
        for (int k = 0; k < 4; k++) run_case(m, pages, "audio point captured", [&] { wr(0x009cb860, 0x1234567u * uint32_t(k + 1)); const uint32_t a[1] = {drv(0)}; call_this(0x00582520, logic, 1, a); });
    }
    /* the results table's dispatcher (00614890: ESI = walker, EAX = column list, ECX = driver; stdcall vector, type): Graffiti 0xb and
     * Capture 0x10 have no filler (the jump table's default) */
    for (uint32_t ty : {0xbu, 0x10u}) run_case(m, pages, "results columns", [&] { const uint32_t a[2] = {halloc(32), ty}; call_ex(0x00614890, halloc(32), drv(0), 0, halloc(32), 0, 2, a); });
    if (const uint32_t logic = rd(act + 0xfd8)) run_case(m, pages, "audio quiet: vf15", [&] { audio_quiet(); call_this(0x00564960, act); });
    probe_graffiti_points(m, pages, act);
    if (std::getenv("RX_PL_TEARDOWN")) run_case(m, pages, "teardown: vf3 + sdtor", [&] { wr(0x00a946c0, 0); call_this(0x00564320, act); const uint32_t one = 1; call_this(0x00563e10, act, 1, &one); });
    std::fprintf(stderr, "[pl] Graffiti: %d cases done (state restored)\n", g_cases);
}

/* TEST FIXTURE, Capture's computer riders: CaptureDriver's drive 00516380 reads the AI ability record (+0x18) without the null check
 * Tag's has, and the party mode's riders have none (the original faults there, as PokerRace with computer riders): each computer
 * driver without one gets a zeroed record (every speed percentage 0: the throttle x 0.5) */
void fixture_capture_ai(uint32_t act) {
    static uint32_t zero = 0;
    if (!zero) { zero = halloc(0x600); for (uint32_t k = 0; zero && k < 0x600; k += 4) wr(zero + k, 0); }
    for (uint32_t i = 0; zero && i < rd(act + 0x26c); i++) {
        const uint32_t d = rd(rd(act + 0x268) + 4 * i), p = rd(d + 0x74);
        if (rd(p + 0x10) == 1 && rd(d + 0x18) == 0) { wr(d + 0x18, zero); std::fprintf(stderr, "[pl] fixture: Capture: driver %08x gets a zeroed AI ability record (TEST FIXTURE)\n", d); }
    }
}

RX_PORT(0x0084ed20, PartyLastProbeHook, "thunk", 0) {
    g_m = &m;
    static uint32_t frame = 0;
    ++frame;
    if (std::getenv("RX_PL_FIXTURE")) { const uint32_t ca = world_activity(0x00944fd4); if (ca) fixture_capture_ai(ca); }
    if (!std::getenv("RX_PL_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static bool done = false;
    static int inprogress = 0;
    if (done) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    const uint32_t eh = world_activity(0x00944e24), ca = world_activity(0x00944fd4), gr = world_activity(0x0094497c);
    const uint32_t act = eh ? eh : ca ? ca : gr;
    if (!act || rd(act + 0x21c) != 2) return reflex_vm::RUN_ORIGINAL;
    const int want = std::getenv("RX_PL_PROBE_FRAMES") ? std::atoi(std::getenv("RX_PL_PROBE_FRAMES")) : 120;
    if (++inprogress < want) return reflex_vm::RUN_ORIGINAL;
    done = true;
    std::fprintf(stderr, "[pl] probe at hook frame %u\n", frame);
    if (eh) probe_egghunt(m, eh);
    if (ca) probe_capture(m, ca);
    if (gr) probe_graffiti(m, gr);
    return reflex_vm::RUN_ORIGINAL;
}

// 0084ea00 (the engine's frame mark: Game_EnterRealWorldAudio calls it right after the activities started, then every frame): the
// computer riders' fixture of Capture, before their first drive
RX_PORT(0x0084ea00, PartyLastDriverFixture, "thunk", 0) {
    g_m = &m;
    if (std::getenv("RX_PL_FIXTURE")) { const uint32_t ca = world_activity(0x00944fd4); if (ca) fixture_capture_ai(ca); }
    return reflex_vm::RUN_ORIGINAL;
}

// 007668e0 (stdcall world; RET 4): every activity of the world starts (vf2). The fixtures go in first.
RX_PORT(0x007668e0, PartyLastFixture, "thunk", 0) {
    g_m = &m;
    const uint32_t w = m.arg(0);
    const uint32_t scene = rd(w + 0x20138);
    if (std::getenv("RX_PL_DUMP")) {
        std::fprintf(stderr, "[pl] world %08x scene %08x activities %u\n", w, scene, rd(w + 0x21da8));
        for (uint32_t i = 0; i < rd(w + 0x21da8); i++) {
            const uint32_t a = rd(rd(w + 0x21da4) + 4 * i);
            std::fprintf(stderr, "[pl] activity %08x vt %08x type %u area '%s' drivers %u\n", a, rd(a), rd(a + 0x23c), rstr(a + 0x118).c_str(), rd(a + 0x26c));
        }
        dump_scene(scene);
    }
    if (std::getenv("RX_PL_FIXTURE"))
        for (uint32_t i = 0; i < rd(w + 0x21da8); i++) {
            const uint32_t a = rd(rd(w + 0x21da4) + 4 * i);
            if (rd(a) == 0x00944e24) fixture_egghunt(a, scene);
            if (rd(a) == 0x00944fd4) fixture_capture(a, scene);
            if (rd(a) == 0x0094497c) fixture_graffiti(a, scene);
        }
    return reflex_vm::RUN_ORIGINAL;
}
