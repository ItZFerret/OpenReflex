// phys_crash_solver.cpp -- oracle adapter for core/game/phys_crash_solver.c (00426d00, group rx_phys).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_crash_solver.h"
}
using namespace phys_oracle;

// 00426d00 (stdcall A, dt; RET 8). The caller (00425640) keeps the registers of the tracked call for 0042a6c0, but that path is
// water-only (never reached in the reference worlds: no water on Concord); no other caller reads them.
// Verified per call in the crash world (Concord, REFLEX_DRIVE=7): 312 calls, 312 verified, 0 mismatches.
// Frame 4 of the port (settled / touching: the root body's world angular velocity, the up component and the linear damp) is
// not reachable: its condition needs (A+0x374 || A+0x88) && A+0x98 == 0 while the entry guard admits only
// (A+0x374 == 0 && A+0x88 == 0) || A+0x98 != 0, and neither slot is written between them (reviewed twice against the
// disassembly: 0x426d1e-0x426d66 the guard, 0x42901d-0x42903a the stage; the called helpers write no such field).
RX_PORT(0x00426d00, Rider_BikeSolver, "rx_phys", 0) {
    phys_crash_bike_solver(m.arg(0), rx_bf(m.arg(1)));
    return m.ret(8);
}
