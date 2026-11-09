// finishui_probe.cpp -- the oracle's finish/results dump (development only): a hook on RaceFinishScenario::vf0 (0074a1e0) that,
// when the race reaches Post, prints the activity's state and the MXRecords rows the Post enter wrote. The probe only reads; it
// returns RUN_ORIGINAL (so the original scenario still runs and the world continues unchanged).
//   REFLEX_PORT=rx_FinishUIProbeHook REFLEX_FINISHUI_PROBE=1 (world test / long run)
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/rx_game.h"
#include "../game/build_vehicle.h"
}
using namespace reflex_port;

namespace {
uint32_t rd(uint32_t a) { return rx_rd32(a); }
float rf(uint32_t a) { uint32_t u = rd(a); float f; std::memcpy(&f, &u, 4); return f; }

struct col { const char* name; uint32_t addr; int is_float; };
void dump_cell(uint32_t table, uint32_t row, const col& c) {
    const uint32_t ci = rx_tbl_column_index(table, c.addr);
    if (ci == 0xffffffffu) { std::printf(" %s=absent", c.name); return; }
    const uint32_t cell = rx_tbl_cell_word(table, ci, row);
    if (c.is_float) std::printf(" %s=%08x(%.6f)", c.name, rd(cell), rf(cell));
    else std::printf(" %s=%d", c.name, int32_t(rd(cell)));
}
void dump_records() {
    const uint32_t rec = rd(0xa946c0);
    std::printf("[finishui] records object %08x\n", rec);
    if (!rec) return;
    for (uint32_t slot = 0; slot < 12; slot++) {
        const uint32_t e = rec + 0x18u * slot;
        const uint32_t table = rd(e), row = rd(e + 4);
        if (!table) continue;
        std::printf("[finishui] slot %u table %08x row %08x", slot, table, row);
        static const col cols[] = {
            {"TotalEvents", 0x0096d99c, 0}, {"TotalLaps", 0x0096d990, 0}, {"TotalPlacing", 0x0096d9a8, 0},
            {"TotalScore", 0x00957c04, 0}, {"TotalTime", 0x00957c1c, 1}, {"BestLapTime", 0x00957c28, 1},
            {"BestPlace", 0x0096da98, 0}, {"BestScore", 0x009626b0, 0},
        };
        if (row != 0xffffffffu)
            for (const col& c : cols) dump_cell(table, row, c);
        std::printf("\n");
    }
}
void dump_activity(uint32_t A) {
    std::printf("[finishui] activity %08x state %d sub %d drivers %d finishers %d laps %d clock %.6f\n",
                A, int32_t(rd(A + 0x21c)), int32_t(rd(A + 0x13e640)), int32_t(rd(A + 0x26c)),
                int32_t(rd(A + 0x300)), int32_t(rd(A + 0xff070)), rf(A + 0x294));
    for (int32_t i = 0; i < int32_t(rd(A + 0x300)); i++) {
        const uint32_t d = rd(A + 0x2d0 + 4u * uint32_t(i));
        if (!d) continue;
        const uint32_t p = rd(d + 0x74);
        std::printf("[finishui]   order %d driver %08x player %08x pad %d local %d laps %d total %08x(%.6f) bestlap %08x(%.6f) score %08x(%.6f)\n",
                    int(i), d, p, int32_t(rd(p + 0x1f0)), int32_t(rd(p + 0x10)), int32_t(rd(d + 0xf1c)),
                    rd(d + 0x1124), rf(d + 0x1124), rd(d + 0x10c4), rf(d + 0x10c4), rd(p + 0xcec), rf(p + 0xcec));
    }
}
}  // namespace

RX_PORT(0x0074a1e0, FinishUIProbeHook, "thunk", 0) {
    static const bool on = std::getenv("REFLEX_FINISHUI_PROBE") != nullptr;
    if (!on) return reflex_vm::RUN_ORIGINAL;
    static bool done = false;
    if (!done) {
        done = true;
        const uint32_t sc = m.r[1];
        const uint32_t A = rd(sc + 0x14);
        std::printf("[finishui] RaceFinishScenario::vf0 at scenario %08x activity %08x (natural finish)\n", sc, A);
        dump_activity(A);
        dump_records();
        std::fflush(stdout);
    }
    return reflex_vm::RUN_ORIGINAL;
}

// Reading probe for the Activity "VehicleEntered" handler 0055e8b0 (unported in the product): prints the arguments and the
// trigger name so the port can be written against the live objects. REFLEX_FINISHUI_TRIGGER=1.
RX_PORT(0x0055e8b0, FinishUITriggerProbe, "thunk", 0) {
    static const bool on = std::getenv("REFLEX_FINISHUI_TRIGGER") != nullptr;
    if (!on) return reflex_vm::RUN_ORIGINAL;
    const uint32_t act = m.r[1], veh = m.arg(0), trig = m.arg(1);
    const uint32_t name = trig + 0xc0;
    std::printf("[finishui] VehicleEntered act %08x veh %08x trig %08x channel %08x name '%c%c%c%c' lit '%c%c%c%c%c%c%c%c' act+4 %08x %08x state %d\n",
                act, veh, trig, rd(act + 0x290), rd(name) & 0xff, (rd(name) >> 8) & 0xff, (rd(name) >> 16) & 0xff, (rd(name) >> 24) & 0xff,
                rd(0x009419a8) & 0xff, (rd(0x009419a8) >> 8) & 0xff, (rd(0x009419a8) >> 16) & 0xff, (rd(0x009419a8) >> 24) & 0xff,
                rd(0x009419ac) & 0xff, (rd(0x009419ac) >> 8) & 0xff, (rd(0x009419ac) >> 16) & 0xff, (rd(0x009419ac) >> 24) & 0xff,
                rd(act + 4), rd(act + 8), int32_t(rd(act + 0x21c)));
    std::fflush(stdout);
    return reflex_vm::RUN_ORIGINAL;
}
