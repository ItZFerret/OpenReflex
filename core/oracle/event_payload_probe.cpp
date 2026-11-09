// event_payload_probe.cpp -- what an event's handler proxy receives (agent w-evfix; development only, group "thunk": select it by name).
//
//   REFLEX_PORT=rx_ScenarioPayloadProbe[,rx_<group>] RX_EVPAYLOAD=<log>
//
// 005effa0 is slot 2 of EventHandlerProxy<void (Class::*)(Activity*, char const*)> (vtable 0x9574b0, the HUD page's PostEventOverlay
// registers it for "Post Scenarios Start" / "Post Scenarios End" through 00608ae0): it unpacks the event {payload, size} into the
// member's two arguments (payload word 0, word 1). The dispatch 004020c0 hands it the raiser's payload by address, so the log line is
// the event as the handler sees it: the event record's address, the payload's address (a stack address of the raiser's frame), the
// size, the two words and the word after them (00587ea0's /GS cookie). A run of the originals and a run with the core's raisers (On)
// must log the same lines.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

static std::FILE* g_log = nullptr;
static uint32_t g_calls = 0;

RX_PORT(0x005effa0, ScenarioPayloadProbe, "thunk", 0) {
    const char* path = std::getenv("RX_EVPAYLOAD");
    if (!path) return reflex_vm::RUN_ORIGINAL;
    if (!g_log) g_log = std::fopen(path, "w");
    if (!g_log) return reflex_vm::RUN_ORIGINAL;
    const uint32_t proxy = m.r[1], ev = m.arg(0);
    const uint32_t payload = U32(ev, 0), size = U32(ev, 4);
    const bool words = size == 8 && payload >= 0x10000;
    std::fprintf(g_log, "%u proxy %08x member %08x object %08x esp %08x event %08x payload %08x size %08x w0 %08x w1 %08x w2 %08x\n", g_calls++,
                 proxy, U32(proxy, 0x10), U32(proxy, 0xc), m.r[4], ev, payload, size, words ? U32(payload, 0) : 0u, words ? U32(payload, 4) : 0u,
                 words ? U32(payload, 8) : 0u);
    std::fflush(g_log);
    return reflex_vm::RUN_ORIGINAL;
}
