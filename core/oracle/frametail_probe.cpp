// frametail_probe.cpp -- the engine's frame counters and the Database's pending removal lists as the ORIGINAL's frame leaves them
// (w-frametail; development only). A frame hook at Engine::Update 0084ed20 (outside every verified call) prints, at the update's
// entry, the render counter [engine + 0x54], the update counter [engine + 0x58] and the two pending lists of Database vf22 (+0x64 /
// +0x7c: handles queued for removal, drained by vf33 0084cc80 at the end of Engine::Frame 0084f260): the first RX_FT_N (default 12)
// updates, then every RX_FT_EVERY (default 100), and every update that finds a list non-empty. In a baton boot (REFLEX_EVENT=1) the
// original's Engine::Frame runs (render included): +0x54 == +0x58 at every update entry, both one more per frame; in a world test only
// Engine::Update runs (both stay at the capture's value) unless REFLEX_FRAME_TAIL=1 (runtime.cpp) adds the frame's tail.
//   REFLEX_PORT=rx_FrameTailProbeHook RX_FT_PROBE=1 [RX_FT_N=<n>] [RX_FT_EVERY=<n>]
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
using namespace reflex_port;

RX_PORT(0x0084ed20, FrameTailProbeHook, "thunk", 0) {
    if (!std::getenv("RX_FT_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t first = std::getenv("RX_FT_N") ? uint32_t(std::atoi(std::getenv("RX_FT_N"))) : 12;
    static const uint32_t every = std::getenv("RX_FT_EVERY") ? uint32_t(std::atoi(std::getenv("RX_FT_EVERY"))) : 100;
    static uint32_t n = 0;
    ++n;
    const uint32_t engine = m.arg(0), ctx = rx_rd32(0x00a95844), db = ctx ? rx_rd32(ctx + 8) : 0;
    const uint32_t p0 = db ? (rx_rd32(db + 0x74) - rx_rd32(db + 0x70)) / 4 : 0, p1 = db ? (rx_rd32(db + 0x8c) - rx_rd32(db + 0x88)) / 4 : 0;
    if (n <= first || (every && n % every == 0) || p0 || p1)
        std::fprintf(stderr, "[ft] update %u: render counter %u update counter %u pending %u / %u\n", n, rx_rd32(engine + 0x54), rx_rd32(engine + 0x58), p0, p1);
    return reflex_vm::RUN_ORIGINAL;
}
