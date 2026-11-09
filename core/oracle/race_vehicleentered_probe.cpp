// race_vehicleentered_probe.cpp -- the oracle's probe for the activity's "VehicleEntered" handler 0055e8b0
// (core/game/race_activity.c rx_activity_vehicle_entered). No reference world raises the event for a named trigger, so the
// adapter is never called there (0 calls). A frame hook (Engine::Update 0084ed20, outside every verified call) calls the
// ORIGINAL 0055e8b0 on a live vehicle / activity / trigger at one frame, so with the port installed Verify runs the core
// function and then the original on the same state and compares memory exactly as for a call from the game.
//   REFLEX_PORT=rx_race,rx_VehicleEnteredProbeHook REFLEX_PORT_VERIFY=1 RX_VE_PROBE=1 [RX_VE_FRAME=<n>]
// The calls (each one a per-call Verify) cover the handler's branches:
//   plain trigger name   -> the race's vf59 (00569f40): neither "HoleShot" nor "FinalStretch"
//   "SCR_Probe"          -> the scripted-sequence branch (0055e100: no such sequence, the load fails)
//   "SCR_Probe", AI, kind 5 -> the inactive + non-local guard (return without a sequence)
//   "HoleShot"           -> the holeshot (00569eb0: the session event, the records, the driver flag, the "Holeshot" event)
//   "FinalStretch"       -> the last-lap check (00559fd0 / the driver's laps)
// The trigger is a scratch page (the handler reads only its name at +0xc0); the fields the probe sets on the activity are put
// back after each call.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }

/* a field of live game data the probe sets and puts back */
struct Poke {
    uint32_t a, old;
    Poke(uint32_t addr, uint32_t v) : a(addr), old(rd(addr)) { wr(addr, v); }
    ~Poke() { wr(a, old); }
};

/* a free guest page for the probe's fake trigger (the handler reads the name at +0xc0) */
uint32_t scratch_page() {
    static uint32_t page = 0;
    if (page) return page;
    for (uint32_t a = 0x50000000u; a < 0x6ff00000u; a += 0x1000u) {
        if (g_m->mem.mapped(a) || g_m->mem.mapped(a + 0xffc)) continue;
        static uint8_t zero[0x1000] = {};
        g_m->mem.map(a, zero, sizeof zero);
        page = a;
        break;
    }
    return page;
}

/* the trigger name at +0xc0 of the fake trigger (0xc0 + 0x40 bytes are inside the page) */
void set_trigger(uint32_t trig, const char* name) {
    if (!trig) return;
    for (uint32_t i = 0; i < 0x40; i++) rx_wr8(trig + 0xc0 + i, 0);
    for (uint32_t i = 0; name[i] && i < 0x3f; i++) rx_wr8(trig + 0xc0 + i, uint8_t(name[i]));
}

/* one call of the original handler as the delegate proxy 00554fc0 makes it: ECX = activity, stack {vehicle, trigger}, RET 8 */
uint32_t call_handler(uint32_t act, uint32_t veh, uint32_t trig) {
    const uint32_t a[2] = {veh, trig};
    rx_regs in = regs();
    in.r[1] = act;
    rx_regs out = regs();
    rx_call_original_ex(0x0055e8b0, &in, &out, 2, a);
    return out.r[0];
}

struct PlayerPick { uint32_t human, ai; };
PlayerPick find_players() {
    PlayerPick r = {0, 0};
    const uint32_t g = rd(0x00a946b8);
    if (!g) return r;
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    for (uint32_t s = 0; s < nslots && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (!rd(p + 0x94) || !rd(p + 0x1ac)) continue;   /* activity + vehicle */
        if (rd(p + 4) != 0 && rd(p + 0x10) == 0) { if (!r.human) r.human = p; }
        else if (rd(p + 4) == 0) { if (!r.ai) r.ai = p; }
    }
    return r;
}

void probe() {
    const PlayerPick pp = find_players();
    const uint32_t human = pp.human, ai = pp.ai;
    const uint32_t hp = human ? rd(human + 0x1ac) : 0, ha = human ? rd(human + 0x94) : 0;
    const uint32_t ap = ai ? rd(ai + 0x1ac) : 0, aa = ai ? rd(ai + 0x94) : 0;
    const uint32_t trig = scratch_page();
    std::fprintf(stderr, "[veentered] human %08x act %08x veh %08x kind %u state %d | ai %08x act %08x veh %08x | trig %08x\n", human, ha, hp,
                 ha ? rd(ha + 0x23c) : 0, ha ? int(rd(ha + 0x21c)) : -1, ai, aa, ap, trig);
    if (!trig || !ha || !hp) { std::fprintf(stderr, "[veentered] no activity / vehicle / scratch\n"); return; }

    /* 1. an ordinary trigger name: the vf59 path, both name tests fail */
    set_trigger(trig, "StartZone");
    std::fprintf(stderr, "[veentered] plain %08x\n", call_handler(ha, hp, trig));

    /* 2. a scripted-sequence trigger on an active activity: 0055e100 for a sequence that does not exist */
    set_trigger(trig, "SCR_Probe");
    std::fprintf(stderr, "[veentered] scr-active %08x\n", call_handler(ha, hp, trig));

    /* 2b. a trigger that names one of the activity's own sequences: the restart branch of 0055e100 */
    {
        char seq[0x104] = {};
        for (int i = 0; i < 12; i++) {
            g_m->mem.read_block(ha + 0xfe330 + 0x104 * uint32_t(i), seq, sizeof seq);
            if (seq[0]) break;
        }
        if (seq[0]) {
            char name[0x80];
            std::snprintf(name, sizeof name, "SCR_%s", seq);
            set_trigger(trig, name);
            std::fprintf(stderr, "[veentered] scr-match \"%s\" %08x\n", name, call_handler(ha, hp, trig));
        } else {
            std::fprintf(stderr, "[veentered] scr-match: no sequence name in the activity\n");
        }
    }

    /* 3. the same on the AI's activity while it is inactive (kind 5): the non-local guard, no sequence */
    if (aa && ap) {
        Poke kind(aa + 0x23c, 5);
        set_trigger(trig, "SCR_Probe");
        std::fprintf(stderr, "[veentered] scr-inactive-ai %08x\n", call_handler(aa, ap, trig));
    }
    /* 3b. the same on a local human: the guard does not fire (kind 5 still starts the sequence) */
    {
        Poke kind(ha + 0x23c, 5);
        set_trigger(trig, "SCR_Probe");
        std::fprintf(stderr, "[veentered] scr-inactive-human %08x\n", call_handler(ha, hp, trig));
    }
    /* 3c. the guard itself: inactive activity and a non-local player (+4 == 0): return, no sequence */
    {
        Poke kind(ha + 0x23c, 5);
        Poke local(human + 4, 0);
        set_trigger(trig, "SCR_Probe");
        std::fprintf(stderr, "[veentered] scr-guard %08x\n", call_handler(ha, hp, trig));
    }

    /* 4. the holeshot trigger (once: the activity's +0x13e644 cleared) */
    {
        Poke done(ha + 0x13e644, 0);
        set_trigger(trig, "HoleShot");
        std::fprintf(stderr, "[veentered] holeshot %08x -> done %u\n", call_handler(ha, hp, trig), rd(ha + 0x13e644));
    }

    /* 5. the final-stretch trigger */
    {
        Poke done(ha + 0x13e648, 0);
        set_trigger(trig, "FinalStretch");
        std::fprintf(stderr, "[veentered] finalstretch %08x -> done %u laps %u player %08x\n", call_handler(ha, hp, trig),
                     rd(ha + 0x13e648), rd(ha + 0xff070), rd(hp + 0x190));
    }
    std::fprintf(stderr, "[veentered] frame %u done\n", g_frame);
}
}  // namespace

RX_PORT(0x0084ed20, VehicleEnteredProbeHook, "thunk", 0) {
    if (!std::getenv("RX_VE_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_VE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_VE_FRAME"))) : 700;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    probe();
    return reflex_vm::RUN_ORIGINAL;
}
