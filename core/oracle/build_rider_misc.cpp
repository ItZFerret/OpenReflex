// build_rider_misc.cpp -- oracle adapter for core/game/build_rider_misc.c (group rx_build_rider).
#include "oracle.h"
extern "C" {
#include "../game/build_rider_misc.h"
}
using namespace reflex_port;

// 0063b360 (stdcall vector; RET 4)
RX_PORT(0x0063b360, RiderVec8MarkAll, "rx_build_rider", 0) { rx_rider_vec8_mark_all(m.arg(0)); return m.ret(4); }
// 0063b3f0 (stdcall vector; RET 4)
RX_PORT(0x0063b3f0, RiderVec8ClearAll, "rx_build_rider", 0) { rx_rider_vec8_clear_all(m.arg(0)); return m.ret(4); }
