/* build_activity_dtors.h -- the remaining activity destructors / area-release virtuals (build_activity_dtors.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_activity_sdtor(rx_addr act, uint32_t flags);              /* 00556640 Activity::sdtor (thiscall; RET 4) -> this */
rx_addr rx_race_stub_sdtor(rx_addr act, uint32_t flags);             /* 005659c0 MXActivityRaceStub::sdtor (thiscall; RET 4) -> this */
void    rx_freeride_activity_destroy(rx_addr act);                   /* 0055fee0 (ECX = activity) */
rx_addr rx_freeride_activity_sdtor(rx_addr act, uint32_t flags);     /* 0055fec0 MXActivityFreeRide::sdtor (thiscall; RET 4) -> this */
void    rx_freeride_activity_vf3(rx_addr act);                       /* 00560080 MXActivityFreeRide::vf3 (thiscall) */
void    rx_waypoint_unlisten_network_object(rx_addr channel, rx_addr act);   /* 0058a420 (EAX = channel; stdcall act; RET 4) */
void    rx_waypoint_activity_vf3(rx_addr act);                       /* 00578b30 MXActivityWaypointRace::vf3 (thiscall) */
int32_t rx_activity_standings_compare(rx_addr act, rx_addr a, rx_addr b, uint32_t c);   /* 00555630 Activity::vf43 (thiscall; RET 0xc) -> 0 */
#ifdef __cplusplus
}
#endif
