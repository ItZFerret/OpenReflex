// build_stl_cue_map.cpp -- oracle adapters for core/game/build_stl_cue_map.c (group rx_race: the audio cue dispatch).
#include "oracle.h"
extern "C" {
#include "../game/build_stl_cue_map.h"
}
using namespace reflex_port;

// 00705f70 (stdcall map, out, key; RET 0xc) -> EAX = out
RX_PORT(0x00705f70, CueMapEqualRange, "rx_race", OUT_EAX) { m.r[0] = rx_cue_map_equal_range(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 00709390 (EDI = &count; first {proxy, node}, last {proxy, node} on the stack; a plain RET at 007093db: the caller pops them)
// (Found by the challenge restart probe: the adapter used to return with RET 0x10, 16 bytes more than the original.)
RX_PORT(0x00709390, CueRangeCount, "rx_race", 0) { rx_cue_range_count(m.r[7], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0); }
