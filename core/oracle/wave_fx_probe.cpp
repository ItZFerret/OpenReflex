// wave_fx_probe.cpp -- the oracle's probe for the w-fx wave (2026-10-26): the switched call sites and the ports of the effects / animation / loop /
// bike / scene / CRT range that none of the five reference worlds reaches (development only; see core/plan/QUEUE.md "w-fx"). A frame hook
// (Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL addresses of the callers on the live game objects, each case on a
// snapshot of the whole guest memory and machine state that is restored afterwards, so with the ports installed Verify runs the core function
// and then the original on the same state and compares memory and the declared registers, exactly as for a call from the game. The nested
// originals' calls of other ports are verified the same way.
//   REFLEX_PORT=<addresses / groups>,rx_WaveFxProbeHook REFLEX_PORT_VERIFY=1 RX_WFX_PROBE=<cases,comma separated> [RX_WFX_FRAME=<n>]
// Cases (each sets only the state the original functions read, then puts everything back by the snapshot):
//   series   the series manager's first-use guard 0x9e275c cleared, then 00413d20 (MXSeriesManager_Construct 005b2370 and its atexit);
//            005b6a30 with an authored event and with none; Activity_UiKind 00556280 for an authored series kind
//   audio    the activity's InProgress (005594a0, the pause branch forced), Pre (00558ce0) and InProgress-enter (005593e0): 00805120 /
//            00805000 / 00805200 are the oracle's originals (build_boundary_audio.h)
//   wipe     Activity_ShowPageWipe 0055e990 (the fade-out of the intro scenario)
//   wpdebug  the waypoint track debug traversal 005399e0 on the activity's tracks
//   tuning   0042fed0 / 00790540 (the tuning log of the XML watch)
//   reader   the guest reader reset 00827e00 on a scratch reader
//   bike     the position filter 0042d7f0 on the vehicle's filters; the replay record re-applied (004331d0) on the live simulation
//   cam      the camera class 006467c0
//   anim     Scene_RegisterEntityName 007d2500, LocSoundList_StopOwner 0080c880, CharacterAnim_CapturePose 008351c0
//   job      the job pool's stall recovery 00881d80 with a queued job
//   static   00402b10 with its guard cleared (00402be0)
//   spectate 0054de80 with the flag set (the guard head)
//   pkg      the package request queue: 0041b4b0 on the loader thread (queued), then the drain 0041b410 (0041bd10, 0041b4b0 on the main thread)
#include "oracle.h"
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
    uint32_t game = 0, player = 0, activity = 0, vehicle = 0, sim = 0, session = 0, scratch = 0;
};

World find_world(reflex_vm::Machine& m) {
    World w;
    w.game = rd(0x00a946b8);
    w.session = rd(0x00a93fcc);
    w.scratch = (m.r[4] - 0x8000) & ~0xfu;   // dead stack: the probe's scratch area
    if (!w.game) return w;
    for (uint32_t s = 0; s < 16; s++) {
        const uint32_t p = w.game + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) == 0 || rd(p + 0x10) != 0) continue;   // a local human
        w.player = p;
        w.activity = rd(p + 0x94);
        w.vehicle = rd(p + 0x1ac);
        w.sim = w.vehicle ? rd(w.vehicle + 0x234) : 0;
        if (w.activity && w.vehicle) break;
    }
    return w;
}

rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
void call_ex(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
}
uint32_t call(uint32_t fn, int conv, uint32_t ecx, int n, const uint32_t* args) {
    return rx_call_original(fn, static_cast<enum rx_conv>(conv), ecx, n, args);
}

void say(const char* what) { std::fprintf(stderr, "[wfx] %s\n", what); }

// ---- the cases -----------------------------------------------------------------------------------------------------------------------
void case_series(World& w) {
    say("series");
    (void)w;
    for (uint32_t a = 0x009e2188; a < 0x009e275c; a += 4) wr(a, 0);          // the not yet constructed manager (its tables are built on zeroed memory)
    wr(0x009e275c, rd(0x009e275c) & ~1u);
    if (std::getenv("RX_WFX_DIRECT")) call(0x005b2370, RX_CDECL, 0, 0, nullptr);
    else call(0x00413d20, RX_CDECL, 0, 0, nullptr);                // MXSeriesManager_Construct 005b2370 + the atexit (rx_fx 00413d20)
}

void case_career(World& w) {
    say("career");
    const uint32_t mgr = 0x009e2188, ev = w.scratch;
    wr(ev + 0x1c, 3);
    wr(mgr + 0x18c, ev);
    { uint32_t a[1] = {mgr}; call(0x005b6a30, RX_STDCALL, 0, 1, a); }   // the authored event's type
    wr(mgr + 0x18c, 0);
    { uint32_t a[1] = {mgr}; call(0x005b6a30, RX_STDCALL, 0, 1, a); }   // the UI property "EventType"
    if (w.activity) {
        wr(w.activity + 0x23c, 0x1f);                                // a series event: the manager's type decides
        rx_regs in = regs(); in.r[0] = w.activity;
        call_ex(0x00556280, in);
    }
}

void case_audio(World& w) {
    say("audio");
    if (!w.activity) return;
    const uint32_t au = rd(w.activity + 0x234);
    if (au) {
        wr(au + 0x20018, 1);
        wr(au + 0x20014, 0);
        wr(w.scratch + 0x100, 1);
        wr(au + 0x20138, w.scratch + 0x100);
    }
    if (w.session) wr(w.session + 0x284, 0);
    { rx_regs in = regs(); in.r[1] = w.activity; uint32_t a[1] = {0x3c888889u}; call_ex(0x005594a0, in, 1, a); }   // Activity InProgress (vf28, dt)
    { rx_regs in = regs(); in.r[1] = w.activity; uint32_t a[1] = {0x3c888889u}; call_ex(0x00558ce0, in, 1, a); }   // Activity Pre (dt)
    { rx_regs in = regs(); in.r[1] = w.activity; uint32_t a[1] = {0}; call_ex(0x005593e0, in, 1, a); }                       // InProgress enter (old state)
    wr(w.activity + 0xbb0, 1);
    wr(w.activity + 0xb8c, 1);
    { rx_regs in = regs(); in.r[1] = w.activity; uint32_t a[1] = {0x3c888889u}; call_ex(0x005609d0, in, 1, a); }   // FreeRide InProgress (the debug traversal)
}

void case_wipe(World& w) {
    say("wipe");
    if (!w.activity) return;
    rx_regs in = regs(); in.r[1] = 0x0095b7c4;                                  // ECX = the page name
    uint32_t a[1] = {w.activity};
    call_ex(0x0055e990, in, 1, a);
}

void case_wpdebug(World& w) {
    say("wpdebug");
    if (!w.activity) return;
    rx_regs in = regs();
    in.r[1] = w.activity + 0x890;
    call_ex(0x005399e0, in);
    const uint32_t gs = rd(w.activity + 0x234) + 0x21e10;
    if (gs) { in.r[1] = gs; call_ex(0x005399e0, in); }
    for (uint32_t kind : {6u, 7u, 8u}) {                                          // the type-specific traversals
        wr(w.activity + 0x890 + 8, kind);
        in.r[1] = w.activity + 0x890;
        call_ex(0x005399e0, in);
    }
}

void case_tuning(World& w) {
    say("tuning");
    (void)w;
    uint32_t sink = call(0x0042fed0, RX_CDECL, 0, 0, nullptr);
    uint32_t a[4] = {sink, 10, 0, 0};
    call(0x00790540, RX_STDCALL, 0, 4, a);
}

void case_reader(World& w) {
    say("reader");
    const uint32_t r = w.scratch + 0x400;
    for (uint32_t k = 0; k < 0x80; k += 4) wr(r + k, 0);
    rx_regs in = regs(); in.r[6] = r;
    call_ex(0x00827e00, in);
}

void case_bike(World& w) {
    say("bike");
    if (!w.vehicle || !w.sim) return;
    {   // 0042d7f0: ESI = the position filters (V + 0xa0), EDX = the target, EAX = the output, xmm4 = dt
        const uint32_t target = w.scratch + 0x200, out = w.scratch + 0x220;
        wr(target, 0x41200000); wr(target + 4, 0x41a00000); wr(target + 8, 0xc1200000);
        rx_regs in = regs(); in.r[6] = w.vehicle + 0xa0; in.r[2] = target; in.r[0] = out;
        in.xmm[4].v[0] = rx_oracle::f32(0x3c888889u);
        call_ex(0x0042d7f0, in);
    }
    {   // 004331d0: EDI = a replay record (the init values), stack S
        const uint32_t rec = w.scratch + 0x1000;
        rx_regs init = regs(); init.r[1] = rec;
        call_ex(0x0042e560, init);                                              // rx_veh_body_rec_init
        rx_regs in = regs(); in.r[7] = rec;
        uint32_t a[1] = {w.sim};
        call_ex(0x004331d0, in, 1, a);
    }
}

void case_cam(World& w) {
    say("cam");
    if (!w.vehicle) return;
    const uint32_t cam = w.scratch + 0x300, ctl = w.scratch + 0x500;
    for (uint32_t k = 0; k < 0x400; k += 4) wr(cam + k, 0);
    wr(cam + 0x344, ctl);
    wr(ctl + 0x1ac, w.vehicle);
    for (uint32_t cls : {3u, 5u}) {
        const uint32_t old = rd(w.vehicle + 0x194);
        wr(w.vehicle + 0x194, cls);
        for (uint32_t kind : {8u, 0u}) {
            const uint32_t old2 = rd(w.vehicle + 0x170);
            wr(w.vehicle + 0x170, kind);
            rx_regs in = regs(); in.r[0] = cam;
            call_ex(0x006467c0, in);
            wr(w.vehicle + 0x170, old2);
        }
        wr(w.vehicle + 0x194, old);
    }
}

void case_anim(World& w) {
    say("anim");
    const uint32_t ctx = rd(0x00a95844);
    const uint32_t scene = ctx ? rd(ctx + 0x20) : 0;
    if (scene) {                                                                // a fake entity whose name pointer (+0x18) points at a string
        const uint32_t ent = w.scratch + 0x800, str = w.scratch + 0x900;
        const char* nm = "WfxProbeEntity";
        for (uint32_t k = 0; k <= std::strlen(nm); k++) rx_wr8(str + k, uint8_t(nm[k]));
        wr(ent + 0x18, str);
        rx_regs in = regs(); in.r[0] = scene;
        uint32_t a[1] = {ent};
        call_ex(0x007d2500, in, 1, a);                                           // Scene_RegisterEntityName: a new name inserted
        call_ex(0x007d2500, in, 1, a);                                           // and found the second time
    }
    if (rd(0x00a957d4)) {
        uint32_t a[1] = {0x009419c4u};
        call(0x0080c880, RX_STDCALL, 0, 1, a);                                   // LocSoundList_StopOwner (an owner name)
        uint32_t b[1] = {0};
        call(0x0080c880, RX_STDCALL, 0, 1, b);                                   // every record
    }
    if (w.vehicle) {                                                            // the rider's character animation: entity + 0x148
        const uint32_t rider = rd(w.vehicle + 0x1a0);
        if (rider && mapped(rider + 0x148, 0x100)) {
            uint32_t a[1] = {rider + 0x148};
            call(0x008351c0, RX_STDCALL, 0, 1, a);                               // CharacterAnim_CapturePose
        }
    }
}

void case_job(World& w) {
    say("job");
    const uint32_t pool = 0x00d94ae0;
    const uint32_t slot = pool + 0x8030;
    for (uint32_t k = 0; k < 0x200; k += 97) {                                 // queued jobs: a function that returns at once (Activity nop 0057b2f0)
        wr(slot + 4 * k, 0x0057b2f0);
        wr(pool + 0x8830, rd(pool + 0x8830) + 1);
    }
    (void)w;
    uint32_t a[1] = {pool};
    call(0x00881d80, RX_STDCALL, 0, 1, a);
}

void case_static(World& w) {
    say("static");
    (void)w;
    call(0x00402be0, RX_CDECL, 0, 0, nullptr);                                  // (00402b10's _atexit needs the live CRT heap's exit table: not under the emulator)
}

void case_spectate(World& w) {
    say("spectate");
    if (!w.player) return;
    uint32_t a[2] = {w.player, 1};
    call(0x0054de80, RX_STDCALL, 0, 2, a);                                      // (the guard-clear branch needs _atexit: not under the emulator)
    uint32_t b[2] = {w.player, 0};
    call(0x0054de80, RX_STDCALL, 0, 2, b);
}

void case_pkg(World& w) {
    say("pkg");
    const uint32_t db = rd(rd(0x00a95844) + 8), Q = rd(0x00d72a30);
    const uint32_t name = w.scratch + 0x600, dbname = w.scratch + 0x700;
    const char* n = std::getenv("RX_WFX_PKG") ? std::getenv("RX_WFX_PKG") : "National_NAT_Concord", * d = n;
    for (uint32_t k = 0; k < 0x60; k++) { rx_wr8(name + k, k < std::strlen(n) + 1 ? uint8_t(n[k]) : 0); rx_wr8(dbname + k, k < std::strlen(d) + 1 ? uint8_t(d[k]) : 0); }
    {   // find a database that is loaded already (Database vf7), so that the main-thread request does not load anything
        static const char* const names[] = {"National_NAT_Concord", "Concord", "NAT_Concord", "Common", "FrontEnd", "Vehicles", "Level", "Global", "Shared", "RiderAssets", "UI"};
        const uint32_t probe_name = w.scratch + 0xa00;
        for (const char* cand : names) {
            for (uint32_t k = 0; k < 0x40; k++) rx_wr8(probe_name + k, k < std::strlen(cand) + 1 ? uint8_t(cand[k]) : 0);
            uint32_t a[1] = {probe_name};
            const uint32_t loaded = rx_call_original(rd(rd(db) + 0x1c), RX_THISCALL, db, 1, a);
            std::fprintf(stderr, "[wfx] database \"%s\" loaded %u\n", cand, loaded);
            if (loaded && !std::getenv("RX_WFX_PKG")) {
                for (uint32_t k = 0; k < 0x60; k++) { rx_wr8(name + k, k < std::strlen(cand) + 1 ? uint8_t(cand[k]) : 0); rx_wr8(dbname + k, k < std::strlen(cand) + 1 ? uint8_t(cand[k]) : 0); }
                // the database's first package name (Database::vf8's table: map db + 0x24 -> entry; names at entry + 0x148, 0xa0 bytes each)
                const uint32_t key = w.scratch + 0xb00, out = w.scratch + 0xb10;
                wr(key, probe_name); wr(out, 0);
                rx_regs in = regs(); in.r[0] = db + 0x24; in.r[7] = key;
                uint32_t b[1] = {out};
                call_ex(0x007fa250, in, 1, b);
                const uint32_t entry = rd(out);
                std::fprintf(stderr, "[wfx] database entry %08x packages %u\n", entry, entry ? rd(entry + 0x14c) : 0);
                if (entry && rd(entry + 0x14c) != 0) {
                    const uint32_t pkg = rd(entry + 0x148);
                    for (uint32_t k = 0; k < 0x60; k++) rx_wr8(dbname + k, rx_rd8(pkg + k));
                }
                break;
            }
        }
    }
    const uint32_t loader = rd(db + 0x100);
    wr(db + 0x100, 0x1234);                                                      // not the main thread: the request is made now
    { rx_regs in = regs(); in.r[1] = name; in.r[2] = dbname; uint32_t a[1] = {Q}; call_ex(0x0041b4b0, in, 1, a); }
    // the queue branch: make the main thread the loader
    const uint32_t me = rx_call_original(rd(0x0091607c), RX_STDCALL, 0, 0, nullptr);
    wr(db + 0x100, me);
    { rx_regs in = regs(); in.r[1] = name; in.r[2] = dbname; uint32_t a[1] = {Q}; call_ex(0x0041b4b0, in, 1, a); }
    { rx_regs in = regs(); in.r[1] = name; in.r[2] = dbname; uint32_t a[1] = {Q}; call_ex(0x0041b4b0, in, 1, a); }
    wr(db + 0x100, 0x1234);
    { uint32_t a[1] = {Q}; call(0x0041b410, RX_STDCALL, 0, 1, a); }             // the drain: 0041bd10 and 0041b4b0 (main thread)
    wr(db + 0x100, loader);
}


void case_trigger(World& w) {
    say("trigger");
    // a trigger the probe builds: the zero world-to-local matrix puts every point at the origin, inside the unit volume
    const uint32_t T = w.scratch + 0x1800;
    for (uint32_t k = 0; k < 0x300; k += 4) wr(T + k, 0);
    wr(T + 0x98, 0); wr(T + 0x9c, 4);               // occupants: empty, growth 4 (the vector grows on the first add)
    wr(T + 0xac, 0); wr(T + 0xb0, 4);               // watched physics objects: empty, growth 4
    wr(T + 0xb8, 0);                                // box
    wr(T + 0x20c, 1);                               // ticks every frame
    wr(T + 0x210, 1);                               // active
    for (uint32_t kind : {0u}) {
        wr(T + 0xbc, kind);
        wr(T + 0x20c, 1);
        say("vf19 #1");
        { rx_regs in = regs(); in.r[1] = T; uint32_t a[1] = {0x3c888889u}; call_ex(0x0040a650, in, 1, a); }   // MXTrigger::vf19: enter / watch
        say("vf19 #1 done");
        std::fprintf(stderr, "[wfx] occupants %u array %08x first %08x\n", rd(T + 0x94), rd(T + 0x90), rd(T + 0x90) ? rd(rd(T + 0x90)) : 0);
        // (the "stay" events, 0040a600 and the physics-object path 0040a900, post names no listener of this world registered: the original itself
        //  faults dispatching them, so the probe stops at the entering events)
    }
    // (the timed section's player-inside 00413b40 -> 00419a10 posts the event named at 0x9c617c, also unregistered in these worlds: not probed)
}

void case_effect(World& w) {
    say("effect");
    // a placeable effect with a sound event: enable it (00678bd0 starts the sound), disable it (stop now / queued stop)
    const uint32_t e = w.scratch + 0x2400, snd = w.scratch + 0x2800;
    for (uint32_t k = 0; k < 0x400; k += 4) { wr(e + k, 0); wr(snd + k, 0); }
    wr(e + 0x204, snd);
    const char* nm = "WfxProbeSound";
    for (uint32_t k = 0; k <= std::strlen(nm); k++) rx_wr8(snd + 0x18 + k, uint8_t(nm[k]));
    wr(snd + 0x1cc, 0x3f800000); wr(snd + 0x1d0, 0x41200000);
    for (uint32_t on : {1u, 0u}) {
        rx_regs in = regs(); in.r[1] = e;
        uint32_t a[1] = {on};
        call_ex(0x006968e0, in, 1, a);
    }
}

void case_storage(World& w) {
    say("storage");
    // PCStorage::vf7's device construction (006d4ac0, EDI = a 0x1c block); the enumeration event it posts afterwards (006d5600, "DeviceAcquired")
    // has no listener in these worlds (the original faults dispatching it), so it is not probed
    const uint32_t blk = w.scratch + 0x2c00;
    for (uint32_t k = 0; k < 0x40; k += 4) wr(blk + k, 0xdeadbeef);
    rx_regs in = regs(); in.r[7] = blk;
    call_ex(0x006d4ac0, in);
}

struct Case { const char* name; void (*fn)(World&); };
const Case kCases[] = {
    {"trigger", case_trigger}, {"effect", case_effect}, {"storage", case_storage}, {"series", case_series}, {"career", case_career}, {"audio", case_audio}, {"wipe", case_wipe}, {"wpdebug", case_wpdebug}, {"tuning", case_tuning},
    {"reader", case_reader}, {"bike", case_bike}, {"cam", case_cam}, {"anim", case_anim}, {"job", case_job}, {"static", case_static},
    {"spectate", case_spectate}, {"pkg", case_pkg},
};

void run_case(reflex_vm::Machine& m, const Case& c, World w) {
    uint32_t regs_[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs_, m.r, sizeof regs_); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[wfx] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    c.fn(w);
    pages.restore();
    host_restore();
    std::memcpy(m.r, regs_, sizeof regs_); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
    m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, WaveFxProbeHook, "thunk", 0) {
    const char* cases = std::getenv("RX_WFX_PROBE");
    if (!cases) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_WFX_FRAME") ? uint32_t(std::atoi(std::getenv("RX_WFX_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    const World w = find_world(m);
    std::fprintf(stderr, "[wfx] frame %u: game %08x player %08x activity %08x vehicle %08x sim %08x\n", g_frame, w.game, w.player, w.activity, w.vehicle, w.sim);
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
