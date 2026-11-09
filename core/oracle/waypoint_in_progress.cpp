// waypoint_in_progress.cpp -- oracle adapter for core/game/waypoint_in_progress.c (group rx_race).
#include "oracle.h"
extern "C" {
#include "../game/waypoint_in_progress.h"
}
using namespace reflex_port;

// 005399e0 (thiscall, ECX = the waypoint track; plain ret): the track's debug traversal (it writes only the track's +0x324 flag and its own frame).
RX_PORT(0x005399e0, WaypointTrackDebug, "rx_race", 0) { rx_waypoint_track_debug(m.r[1]); return m.ret(0); }
