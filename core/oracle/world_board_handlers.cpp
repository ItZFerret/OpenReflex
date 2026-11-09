// world_board_handlers.cpp -- oracle adapters for core/game/world_board_handlers.c (group rx_world).
#include "phys_oracle.h"
extern "C" {
#include "../game/world_board_handlers.h"
}
using namespace reflex_port;
using namespace phys_oracle;

// 006b7b90 (thiscall ECX = the board, stack the activity; RET 4; the original ends in a tail jump to 006b8ab0)
RX_PORT(0x006b7b90, ThirtySecondBoard_FirstHuman, "rx_world", 0) { rx_thirty_second_board_first_human(m.r[1], m.arg(0)); return m.ret(4); }
// 00538310 (ESI = out; stack track block, distance; RET 8)
RX_PORT(0x00538310, Track_Walk, "rx_world", 0) { rx_track_walk(m.r[6], m.arg(0), argf(m, 1)); return m.ret(8); }
// 0042d960 (EDX = the terrain list, EDI = scene; EAX = terrain)
RX_PORT(0x0042d960, Terrain_ForScene, "rx_world", OUT_EAX) { m.r[0] = rx_terrain_for_scene(m.r[2], m.r[7]); return m.ret(0); }
