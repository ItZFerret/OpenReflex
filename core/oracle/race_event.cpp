// race_event.cpp -- oracle adapters for core/game/race_event.c.
#include "oracle.h"
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

RX_PORT(0x0053a470, Event_DriverInt, "rx_race", 0) { rx_event_driver_int(m.r[6], m.r[2], m.arg(0), m.r[1]); return m.ret(4); }
RX_PORT(0x0053a6d0, Event_ActivityDriver, "rx_race", 0) { rx_event_activity_driver(m.r[6], m.r[2], m.arg(0), m.r[1]); return m.ret(4); }
RX_PORT(0x005875e0, Event_ActivityInt, "rx_race", 0) { rx_event_activity_int(m.r[6], m.r[2], m.arg(0), m.r[1]); return m.ret(4); }
RX_PORT(0x0053a9e0, Event_Driver3Int, "rx_race", 0) {
    rx_event_driver_3int(m.r[6], m.arg(0), m.arg(1), m.r[1], m.r[2], m.arg(2));
    return m.ret(0xc);
}
// 0053ac80 "Player Eliminated" (ESI = manager, ECX = driver, EDX = next driver, arg 0 = activity; RET 4).
RX_PORT(0x0053ac80, Event_PlayerEliminated, "rx_race", 0) { rx_event_player_eliminated(m.r[6], m.arg(0), m.r[1], m.r[2]); return m.ret(4); }
RX_PORT(0x0053a770, Event_Driver, "rx_race", 0) { rx_event_driver(m.arg(0), m.r[7], m.r[0]); return m.ret(4); }
RX_PORT(0x0058d7d0, Event_Player2Int, "rx_race", 0) { rx_event_player_2int(m.r[6], m.arg(0), m.arg(1), m.r[1], m.r[2]); return m.ret(8); }
RX_PORT(0x00587d00, Event_PlayerWins, "rx_race", 0) { rx_event_player_wins(m.r[6], m.arg(0), m.r[1], m.r[2], m.arg(1)); return m.ret(8); }

// 0053aa40 (ESI = manager, arg 0 = driver, xmm0 = time; RET 4).
RX_PORT(0x0053aa40, Event_BestLap, "rx_race", 0) { rx_event_best_lap(m.r[6], m.arg(0), rx_oracle::f32(uint32_t(m.xmm[0].lo))); return m.ret(4); }
