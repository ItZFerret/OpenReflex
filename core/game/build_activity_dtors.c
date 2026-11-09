/* build_activity_dtors.c -- the remaining activity destructors and area-release virtuals of the activity classes (2026-10-26,
 * agent w-stunts; the destruction chain of rx_activity_area_update 007669f0: vf3 and vf6 end a started activity, vf0(1) deletes it):
 *   Activity::sdtor                  00556640   (the base destructor 005566b0, freed when bit 0 of the flags is set)
 *   MXActivityRaceStub::sdtor        005659c0   (005659e0, rx_race_stub_destroy)
 *   MXActivityFreeRide::sdtor        0055fec0   (0055fee0, below)
 *   MXActivityFreeRide::vf3          00560080   (every driver leaves, the track block is reset, the start-slot scenario entity
 *                                                leaves the scene; then Activity::vf3)
 *   MXActivityWaypointRace::vf3      00578b30   (every driver leaves, the waypoint arrays freed, the track block reset, the
 *                                                podium released, the activity's "network object removed" handler leaves; then
 *                                                Activity::vf3; the clip recorder is cleared when it was this activity)
 *   FUN_0058a420                     0058a420   (the "NetworkObject..." listener unsubscription of the activity's channel)
 * Each is a hand transliteration at the original's stack frame. */
#include "build_activity_dtors.h"
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "race_activity_misc.h"
#include "race_activity_teardown.h"
#include "anim_scenario_misc.h"
#include "waypoint_activity.h"
#include "waypoint_audio.h"
#include "anim.h"

/* 00556640 Activity::sdtor (thiscall; flags; RET 4) -> this. Frame: push esi. */
rx_addr rx_activity_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_waypoint_activity_base_destroy(act));         /* 005566b0 */
    if (flags & 1) {
        SU32(F, -4, act);
        ACT_CALL(1, rx_reflex_aligned_free(act));                /* 0078fc50 */
    }
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x00556640, rx_activity_sdtor)

/* 005659c0 MXActivityRaceStub::sdtor (thiscall; flags; RET 4) -> this: the stub's destructor 005659e0, freed when bit 0 is set.
 * Frame: push esi. */
rx_addr rx_race_stub_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_race_stub_destroy(act));
    if (flags & 1) {
        SU32(F, -4, act);
        ACT_CALL(1, rx_reflex_aligned_free(act));
    }
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x005659c0, rx_race_stub_sdtor)

/* 0055fee0 (ECX = the free-ride activity): MXActivityFreeRide's destructor: vtable 0x94329c; the audio logic (+0xfd8) destroyed
 * (00580d30) and freed; when this activity is the world's podium owner (world +0x20190) the podium is released (0074b180 with the
 * flag 1 at world + 0x20150); the embedded scenario entity at +0x1143e0 gets the entity base vtables (+0x90 0x93708c, +0 0x95a2e4)
 * and leaves both entity registries ([0xa95824] / [0xa95828] vf1 with the entity); the scenario slot +0xff200 is reset to
 * BaseScenario's vtable 0x96cea4; then the Activity base destructor (tail jump to 005566b0). Frame: push esi; push edi. */
void rx_freeride_activity_destroy(rx_addr act) {
    ACT_FRAME(F, 0, 2);
    const rx_addr audio = U32(act, 0xfd8);
    SU32(act, 0, 0x0094329c);
    if (audio != 0) {
        SU32(F, -4, audio);
        ACT_CALL(1, rx_activity_audio_destroy(audio));           /* 00580d30 */
        SU32(F, -4, audio);
        ACT_CALL(1, rx_reflex_aligned_free(audio));
    }
    const rx_addr world = U32(act, 0x234);
    if (act == U32(world, 0x20190)) {
        SU32(F, -4, 1);
        ACT_CALL(1, rx_podium_release(world + 0x20150, 1));      /* 0074b180 (EAX = podium; stack flag) */
    }
    const rx_addr entity = act + 0x1143e0;
    SU32(entity, 0x90, 0x0093708c);
    SU32(entity, 0, 0x0095a2e4);
    for (int k = 0; k < 2; k++) {
        const rx_addr registry = U32(0, k ? 0x00a95828 : 0x00a95824);
        if (registry != 0) {
            const uint32_t arg = entity;
            SU32(F, -4, entity);
            act_vcall(registry, 4, 1, &arg);
        }
    }
    SU32(act, 0xff200, 0x0096cea4);
    ACT_END(F);
    rx_waypoint_activity_base_destroy(act);                      /* mov ecx, edi; pop edi; pop esi; jmp 005566b0 */
}
RX_FUNCTION(0x0055fee0, rx_freeride_activity_destroy)

/* 0055fec0 MXActivityFreeRide::sdtor (thiscall; flags; RET 4) -> this: 0055fee0, freed when bit 0 is set. Frame: push esi. */
rx_addr rx_freeride_activity_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_freeride_activity_destroy(act));
    if (flags & 1) {
        SU32(F, -4, act);
        ACT_CALL(1, rx_reflex_aligned_free(act));
    }
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x0055fec0, rx_freeride_activity_sdtor)

/* every driver of the two lists (+0x268 count +0x26c, +0x27c count +0x280) leaves the activity: the list's first entry each time
 * (Activity_OnPlayerLeft 0055c1d0 removes it from the list) */
static void activity_players_leave(rx_addr F, rx_addr act) {
    while (I32(act, 0x26c) > 0) {
        const rx_addr drv = U32(U32(act, 0x268), 0);
        SU32(F, -4, drv); SU32(F, -8, act);
        ACT_CALL(2, rx_activity_on_player_left(act, drv));
    }
    while (I32(act, 0x280) > 0) {
        const rx_addr drv = U32(U32(act, 0x27c), 0);
        SU32(F, -4, drv); SU32(F, -8, act);
        ACT_CALL(2, rx_activity_on_player_left(act, drv));
    }
}

/* 00560080 MXActivityFreeRide::vf3 (thiscall): the drivers leave; the track block +0x890 is reset (00538090); a loaded
 * start-slot scenario entity (+0x114520) leaves the scene ([+0x234] + 0x20138; 007d3040 with ESI = the embedded entity +0x1143e0)
 * and is released (its vf9); then Activity::vf3 (tail jump to 00557790). Frame: push edi (push esi around the entity release). */
void rx_freeride_activity_vf3(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    activity_players_leave(F, act);
    ACT_CALL(0, rx_track_block_reset(act + 0x890));
    if (U32(act, 0x114520) != 0) {
        const rx_addr scene = U32(U32(act, 0x234), 0x20138);
        const rx_addr entity = act + 0x1143e0;
        const rx_addr s0 = rx_sp();
        rx_set_sp(s0 - 4);                                       /* push esi */
        SU32(rx_sp(), -4, scene);
        ACT_CALL(1, rx_scene_manager_remove_entity(entity, scene));
        act_vcall(entity, 0x24, 0, NULL);
        rx_set_sp(s0);
    }
    ACT_END(F);
    rx_activity_vf3(act);                                        /* mov ecx, edi; pop edi; jmp 00557790 */
}
RX_FUNCTION(0x00560080, rx_freeride_activity_vf3)

/* 0058a420 (EAX = the activity's event channel; stdcall activity; RET 4): the activity's handler 0057a050 (proxy 0x945ca4,
 * signature 0x9cda64) leaves the event 0x93eefc of the channel: the unsubscription template (act_unlisten, aligned frame) */
void rx_waypoint_unlisten_network_object(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x0093eefc, 0x00945ca4, 0x009cda64, act, 0x0057a050);
}
RX_FUNCTION(0x0058a420, rx_waypoint_unlisten_network_object)

/* 00578b30 MXActivityWaypointRace::vf3 (thiscall): the drivers leave; the route arrays are freed (+0xff09c with its count +0xff07c
 * cleared first, +0xff098 when +0xff074 is set); the track block is reset (00538090); the world's podium (world +0x20150) is
 * released with the flag 1 when there is a world (0074b180); the activity's channel handler leaves (0058a420); Activity::vf3
 * (00557790); and when this activity was the clip recorder ([0xa942b8]) the recorder and its mode ([0xa942b4]) are cleared.
 * Frame: push esi. */
void rx_waypoint_activity_vf3(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    activity_players_leave(F, act);
    const rx_addr route = U32(act, 0xff09c);
    SU32(act, 0xff07c, 0);
    if (route != 0) {
        SU32(F, -4, route);
        ACT_CALL(1, rx_reflex_aligned_free(route));
    }
    if (U32(act, 0xff074) != 0) {
        const rx_addr p = U32(act, 0xff098);
        SU32(F, -4, p);
        ACT_CALL(1, rx_reflex_aligned_free(p));
    }
    ACT_CALL(0, rx_track_block_reset(act + 0x890));
    const rx_addr world = U32(act, 0x234);
    if (world != 0) {
        SU32(F, -4, 1);
        ACT_CALL(1, rx_podium_release(world + 0x20150, 1));
    }
    SU32(F, -4, act);
    ACT_CALL(1, rx_waypoint_unlisten_network_object(U32(act, 0x290), act));
    ACT_CALL(0, rx_activity_vf3(act));
    if (act == U32(0, 0x00a942b8)) {
        SU32(0, 0x00a942b4, 0);
        SU32(0, 0x00a942b8, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00578b30, rx_waypoint_activity_vf3)

/* 00555630 Activity::vf43 (thiscall; stack a, b, c; RET 0xc) -> EAX 0: the standings comparator of the activities that do not rank
 * their drivers (the FreeRide activity's): every pair compares equal. Frame: none. */
int32_t rx_activity_standings_compare(rx_addr act, rx_addr a, rx_addr b, uint32_t c) {
    (void)act; (void)a; (void)b; (void)c;
    return 0;
}
RX_FUNCTION(0x00555630, rx_activity_standings_compare)
