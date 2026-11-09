// race_stunt_probe.cpp -- the oracle's probe for MXActivityRace's stunt handlers 00565710 / 00565770 / 005657d0
// (core/game/race_stunt.c). No reference world raises a stunt, so their adapters are never called by the worlds' own frames (0 calls
// in all five). A frame hook (group "thunk", Engine::Update 0084ed20, outside every verified call) calls the ORIGINAL handlers on the
// live event activity at one frame, so with the ports installed Verify runs the core function and then the original on the same state
// and compares memory exactly as for a call from the game.
//   REFLEX_PORT=rx_race,rx_StuntHandlerProbeHook REFLEX_PORT_VERIFY=1 RX_STUNTPROBE=1 [RX_STUNTPROBE_FRAME=<n>] [RX_STUNTPROBE_CALLS=<n>]
// Call shapes: ECX = the activity, the player (and the descriptor / type) pushed; 00565710 "PhysicsStuntBegin" pops 0xc, 00565770
// "StuntBegin" / 005657d0 "StuntFailed" pop 8. Branches: a local human (player +0x10 == 0: the level / fade / 0x009c3edc writes,
// with the activity's audio logic +0x234 +0x22148 set and clear, and with no audio logic at all) and an AI player (+0x10 != 0:
// the early return). The +0x22148 / +0x234 pokes are restored after each call pair.
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
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }

/* a block on the game heap (0078fbe0, the game's own allocator), zeroed: the fixture player for the AI early return. */
uint32_t halloc(uint32_t n) {
    const uint32_t a[1] = {n};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);
    for (uint32_t i = 0; i < n; i += 4) rx_wr32(p + i, 0);
    return p;
}

/* one call as the event proxy makes it: ECX = the activity, `nargs` pushed words {player, descriptor, type}. */
void call_handler(uint32_t addr, uint32_t act, uint32_t player, int nargs) {
    const uint32_t a[3] = {player, 0, 0};
    rx_regs in = regs();
    in.r[1] = act;
    rx_regs out = regs();
    rx_call_original_ex(addr, &in, &out, nargs, a);
}

void probe() {
    const uint32_t g = rd(0x00a946b8);
    if (!g) { std::fprintf(stderr, "[stunt] no game object\n"); return; }
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    uint32_t human = 0, ai = 0;
    for (uint32_t s = 0; s < nslots && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) != 0 && rd(p + 0x10) == 0) { if (!human) human = p; }
        else if (rd(p + 4) == 0 && rd(p + 0x10) != 0) { if (!ai) ai = p; }
    }
    const uint32_t act = human ? rd(human + 0x94) : 0;
    const uint32_t audio = act ? rd(act + 0x234) : 0;
    const uint32_t mute = audio ? rd(audio + 0x22148) : 0;
    const uint32_t calls = std::getenv("RX_STUNTPROBE_CALLS") ? uint32_t(std::atoi(std::getenv("RX_STUNTPROBE_CALLS"))) : 1;
    std::fprintf(stderr, "[stunt] game %08x slots %u human %08x ai %08x act %08x audio %08x mute %08x level %08x timer %08x cur %08x flag %08x calls %u\n",
                 g, nslots, human, ai, act, audio, mute, act ? rd(act + 0xff07c) : 0, act ? rd(act + 0xff078) : 0,
                 act ? rd(act + 0xff080) : 0, rd(0x009c3edc), calls);
    if (!human || !act) { std::fprintf(stderr, "[stunt] no human player / activity\n"); return; }
    /* one handler with every branch: the human with the live audio logic (its +0x22148 as is), the human with +0x22148 set and
     * clear, the human with no audio logic (+0x234 = 0), and an AI player (the early return): the live AI player when one exists,
     * else a fixture player block with +0x10 = 1 (the only field the handlers read). */
    const uint32_t fake_ai = halloc(0x20);
    rx_wr32(fake_ai + 0x10, 1);
    auto exercise = [&](uint32_t addr, int nargs) {
        call_handler(addr, act, human, nargs);
        if (audio) {
            rx_wr32(audio + 0x22148, 1);                        /* the level kept */
            call_handler(addr, act, human, nargs);
            rx_wr32(audio + 0x22148, 0);                        /* the track muted: 0x009c3edc = 0 */
            call_handler(addr, act, human, nargs);
            rx_wr32(audio + 0x22148, mute);
            const uint32_t a0 = rd(act + 0x234);
            rx_wr32(act + 0x234, 0);                            /* no audio logic: the level as it is */
            call_handler(addr, act, human, nargs);
            rx_wr32(act + 0x234, a0);
        }
        call_handler(addr, act, ai ? ai : fake_ai, nargs);       /* the AI early return */
    };
    for (uint32_t i = 0; i < (calls ? calls : 1); i++) {
        exercise(0x00565710, 3);                                 /* "PhysicsStuntBegin" (RET 0xc) */
        std::fprintf(stderr, "[stunt] 565710 -> cur %08x flag %08x timer %08x\n", rd(act + 0xff080), rd(0x009c3edc), rd(act + 0xff078));
        exercise(0x00565770, 2);                                 /* "StuntBegin" (RET 8) */
        std::fprintf(stderr, "[stunt] 565770 -> cur %08x flag %08x timer %08x\n", rd(act + 0xff080), rd(0x009c3edc), rd(act + 0xff078));
        exercise(0x005657d0, 2);                                 /* "StuntFailed" (RET 8) */
        std::fprintf(stderr, "[stunt] 5657d0 -> cur %08x flag %08x timer %08x\n", rd(act + 0xff080), rd(0x009c3edc), rd(act + 0xff078));
    }
    std::fprintf(stderr, "[stunt] frame %u done\n", g_frame);
}
}  // namespace

RX_PORT(0x0084ed20, StuntHandlerProbeHook, "thunk", 0) {
    if (!std::getenv("RX_STUNTPROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_STUNTPROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_STUNTPROBE_FRAME"))) : 900;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    probe();
    return reflex_vm::RUN_ORIGINAL;
}
