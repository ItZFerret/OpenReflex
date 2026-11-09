// race_stuntaudio_probe.cpp -- the oracle's probe for ActivityAudioLogic::vf6 005818c0, the handler a "PhysicsStuntBegin" delegate reaches
// (core/game/race_activity_audio.c rx_audio_driver_cheer; the delegate's member-function pointer is the vcall thunk 00592b60 -> slot 0x18).
// No reference world raises a stunt, so vf6's adapter has never been called (0 calls in all five). A frame hook (group "thunk", Engine::Update
// 0084ed20, outside every verified call) calls the ORIGINAL 005818c0 on live players at one frame, so with the port installed Verify runs the
// core function and then the original on the same state and compares memory and the registers exactly as for a call from the game.
//   REFLEX_PORT=rx_race,rx_StuntAudioProbeHook REFLEX_PORT_VERIFY=1 RX_STUNTAUDIO_PROBE=1 [RX_STUNTAUDIO_FRAME=<n>] [RX_STUNTAUDIO_CALLS=<n>]
// The call shape is the original's: ECX = the listener (vf6 never reads it -- the disassembly reads only [esp+4], the player), three pushed
// arguments {player, stunt descriptor, type} and RET 0xc. Branches: the local human (player +4 != 0, +0x10 == 0: the crowd cheer + the game
// LCG draw + by chance the reaction), an AI player (+4 == 0: the early return) and a null player. Repeating the human call covers both sides
// of the chance test; every call is a per-call Verify of the port.
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

/* one call of the original handler as the delegate makes it: ECX = listener (unused), stack {player, descriptor, type}; RET 0xc */
void call_vf6(uint32_t listener, uint32_t player) {
    const uint32_t a[3] = {player, 0, 0};
    rx_regs in = regs();
    in.r[1] = listener;
    rx_regs out = regs();
    rx_call_original_ex(0x005818c0, &in, &out, 3, a);
}

void probe() {
    const uint32_t g = rd(0x00a946b8);
    if (!g) { std::fprintf(stderr, "[stuntaudio] no game object\n"); return; }
    const uint32_t nslots = uint8_t(rx_rd8(g + 0x1541c));
    uint32_t human = 0, ai = 0;
    for (uint32_t s = 0; s < nslots && s < 16; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        if (rd(p + 4) != 0 && rd(p + 0x10) == 0) { if (!human) human = p; }
        else if (rd(p + 4) == 0 && rd(p + 0x10) != 0) { if (!ai) ai = p; }
    }
    const uint32_t calls = std::getenv("RX_STUNTAUDIO_CALLS") ? uint32_t(std::atoi(std::getenv("RX_STUNTAUDIO_CALLS"))) : 1;
    std::fprintf(stderr, "[stuntaudio] game %08x slots %u human %08x ai %08x calls %u crowd emitters %d excitement %08x rand %08x count %08x\n",
                 g, nslots, human, ai, calls, int(rd(0x00c02474)), rd(0x00c023b8), rd(0x009cb860), rd(0x009cb864));
    if (human) {
        for (uint32_t i = 0; i < (calls ? calls : 1); i++) {
            call_vf6(0, human);
            std::fprintf(stderr, "[stuntaudio] vf6 human %08x #%u -> rand %08x count %08x excitement %08x state %08x\n", human, i + 1,
                         rd(0x009cb860), rd(0x009cb864), rd(0x00c023b8), rd(0x00c02400));
        }
    }
    if (ai) { call_vf6(0, ai); std::fprintf(stderr, "[stuntaudio] vf6 ai %08x -> rand %08x\n", ai, rd(0x009cb860)); }
    call_vf6(0, 0);   /* the null guard (vf6's first test) */
    std::fprintf(stderr, "[stuntaudio] frame %u done\n", g_frame);
}
}  // namespace

RX_PORT(0x0084ed20, StuntAudioProbeHook, "thunk", 0) {
    if (!std::getenv("RX_STUNTAUDIO_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_STUNTAUDIO_FRAME") ? uint32_t(std::atoi(std::getenv("RX_STUNTAUDIO_FRAME"))) : 1000;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    probe();
    return reflex_vm::RUN_ORIGINAL;
}
