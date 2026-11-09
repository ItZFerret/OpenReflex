/* race_activity_teardown.c -- the race activity's teardown: MXActivityRace::vf3 00564c10 (the drivers leave, the race's stunt
 * listeners leave) -> MXActivityRaceStub::vf3 00566c80 (the track block, the clip recorder, the lap / timed-section listeners, the
 * podium) -> Activity::vf3 00557790 (build_activity_teardown.c). Reached when the career leaves a race for the next one ("Continue
 * to Moto Two"). Each function is a hand transliteration of the original (address in its comment). */
#include "race_activity_teardown.h"
#include "rider_cap.h"
#include "build_activity_teardown.h"
#include "build_activity.h"
#include "race_activity_misc.h"
#include "race_extern.h"
#include "anim_scenario_misc.h"
#include "build_driver_dtor.h"
#include "race_driver.h"
#include "race.h"
#include "loop_platform.h"
#include "build_alloc.h"
#include "waypoint_activity.h"
#include "waypoint_audio.h"

/* 0053a670 (EAX = the activity's channel; stdcall driver; RET 4): the ghost driver's "Ghost Session Completed" handler leaves; owned
 * and defined by core/game/waypoint_driver_events.c (no header declares it) */
void rx_waypoint_driver_ghost_unsubscribe(rx_addr channel, rx_addr driver);

/* 005882b0 (ECX = name; stdcall activity, method; RET 8): a void(MXActivityRace*, MXPlayer*, StuntDescriptor const*) stunt handler
 * (proxy 0x9459c4) leaves the stunts channel [0xa94304] */
void rx_race_unlisten_stunt(rx_addr name, rx_addr act, rx_addr method) {
    act_unlisten(1, U32(0, 0x00a94304), name, 0x009459c4, 0x009cbf38, act, method);
}
RX_FUNCTION(0x005882b0, rx_race_unlisten_stunt)
/* 00588310 (stdcall activity; RET 4): "PhysicsStuntBegin" -> 00565710 (proxy 0x9459e4) leaves the stunts channel */
void rx_race_unlisten_physics_stunt(rx_addr act) {
    act_unlisten(1, U32(0, 0x00a94304), 0x009607a0, 0x009459e4, 0x009ce768, act, 0x00565710);
}
RX_FUNCTION(0x00588310, rx_race_unlisten_physics_stunt)
/* 00588a80 (EAX = the activity's channel; stdcall activity, method; RET 8): "Lap Completed" (proxy 0x945a04) leaves */
void rx_race_unlisten_lap_completed(rx_addr channel, rx_addr act, rx_addr method) {
    act_unlisten(1, channel, 0x0093eefc, 0x00945a04, 0x009cda64, act, method);
}
RX_FUNCTION(0x00588a80, rx_race_unlisten_lap_completed)
/* 00588ae0 (stdcall activity; RET 4): the timed-section trigger handler (the trigger registry [0xa89bc8], the name [0x9c617c]
 * "EnteredTimedSection") -> 00569550 (proxy 0x945a24) leaves */
void rx_race_unlisten_timed_section(rx_addr act) {
    act_unlisten(1, U32(0, 0x00a89bc8), U32(0, 0x009c617c), 0x00945a24, 0x009cbc38, act, 0x00569550);
}
RX_FUNCTION(0x00588ae0, rx_race_unlisten_timed_section)

/* MXActivityRaceStub::vf3 00566c80 (thiscall): the track block (+0x890) reset (00538090); the clip recorder released when it records
 * this activity ([0xa942b8] / [0xa942b4] = 0); the "Lap Completed" handler (00567660 for kind 5, else 005676e0) leaves (00588a80); a
 * kind-8 activity's timed-section handler leaves (00588ae0); the world's podium (+0x234 -> +0x20150) released and cleared (0074b180);
 * then a jump to Activity::vf3. Frame: push esi (popped before the jump). */
void rx_race_stub_vf3(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(0, rx_track_block_reset(act + 0x890));
    if (act != 0 && act == U32(0, 0x00a942b8)) {
        SU32(0, 0x00a942b4, 0);
        SU32(0, 0x00a942b8, 0);
    }
    {
        const rx_addr method = U32(act, 0x238) == 5 ? 0x00567660 : 0x005676e0;
        SU32(F, -4, method); SU32(F, -8, act);
        RACE_CORE_CALL(2, rx_race_unlisten_lap_completed(U32(act, 0x290), act, method));
    }
    if (U32(act, 0x238) == 8) {
        SU32(F, -4, act);
        RACE_CORE_CALL(1, rx_race_unlisten_timed_section(act));
    }
    if (U32(act, 0x234) != 0) {
        SU32(F, -4, 1);
        RACE_CORE_CALL(1, rx_podium_release(U32(act, 0x234) + 0x20150, 1));
    }
    RACE_FRAME_END(F);
    rx_activity_vf3(act);                                        /* mov ecx, esi; pop esi; jmp 00557790 */
}
RX_FUNCTION(0x00566c80, rx_race_stub_vf3)

/* MXActivityRace::vf3 00564c10 (thiscall): every driver of the two lists (+0x268 count +0x26c, +0x27c count +0x280) leaves the
 * activity (Activity_OnPlayerLeft 0055c1d0, the list's first driver each time: the call removes it); the race's stunt listeners
 * leave ("StuntBegin" 00565770, "StuntFailed" 005657d0: 005882b0; "PhysicsStuntBegin": 00588310); then a jump to
 * MXActivityRaceStub::vf3. Frame: push esi (popped before the jump). */
void rx_race_vf3(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    while (I32(act, 0x26c) > 0) {
        const rx_addr drv = U32(U32(act, 0x268), 0);
        SU32(F, -4, drv); SU32(F, -8, act);
        RACE_CORE_CALL(2, rx_activity_on_player_left(act, drv));
    }
    while (I32(act, 0x280) > 0) {
        const rx_addr drv = U32(U32(act, 0x27c), 0);
        SU32(F, -4, drv); SU32(F, -8, act);
        RACE_CORE_CALL(2, rx_activity_on_player_left(act, drv));
    }
    SU32(F, -4, 0x00565770); SU32(F, -8, act);
    RACE_CORE_CALL(2, rx_race_unlisten_stunt(0x0095ba28 /* "StuntBegin" */, act, 0x00565770));
    SU32(F, -4, 0x005657d0); SU32(F, -8, act);
    RACE_CORE_CALL(2, rx_race_unlisten_stunt(0x00960764 /* "StuntFailed" */, act, 0x005657d0));
    SU32(F, -4, act);
    RACE_CORE_CALL(1, rx_race_unlisten_physics_stunt(act));
    RACE_FRAME_END(F);
    rx_race_stub_vf3(act);                                       /* mov ecx, esi; pop esi; jmp 00566c80 */
}
RX_FUNCTION(0x00564c10, rx_race_vf3)

/* ======================================================================================================================== */
/* RaceDriver::vf2 (the driver's release, from Activity_OnPlayerLeft)                                                         */

/* 0053a980 (EAX = the session registry [0xda6ca0]; stdcall driver; RET 4): the driver's "NetworkActivityEvent" handler (proxy
 * 0x940330, 00512990) leaves. Frame: sub esp, 0x14; push esi. */
void rx_race_driver_unlisten_network(rx_addr registry, rx_addr drv) {
    act_unlisten(0, registry, 0x0093eed8, 0x00940330, 0x009cdd3c, drv, 0x00512990);
}
RX_FUNCTION(0x0053a980, rx_race_driver_unlisten_network)

/* 007006e0 (EAX = the network player state): its sixteen dwords +0xe8 .. +0x124 cleared */
void rx_net_player_state_clear(rx_addr st) {
    static const uint8_t k[16] = {0xe8, 0xec, 0xf0, 0xf8, 0xf4, 0xfc, 0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24};
    for (int i = 0; i < 16; i++) SU32(st, (i < 6 ? 0u : 0x100u) + k[i], 0);
}
RX_FUNCTION(0x007006e0, rx_net_player_state_clear)

/* RaceDriver::vf2 0050ef80 (thiscall): a ghost driver's player (+0x74 -> +0x10 == 2) drops its ghost handler (0053a670 on the
 * activity's channel); under the engine's profiler scope (race_profile_begin: stat 3 when the stats system 0xda6d08 is on, else
 * 0xda6cf0; 00883d80 / 00883d90 unless 0x9df580) a networked driver's (+0xef0, flag bit 0 of +0x20, a valid reference 00884950)
 * player state (+0x1d4 -> [0]) is cleared (007006e0); the driver's "NetworkActivityEvent" handler leaves (0053a980); BaseDriver::vf2.
 * The session block's static initialiser (006de1f0 under 0xdcf304) is checked twice. Frame: push ebp; and esp, -8; push ecx /
 * ebx / esi / edi. */
void rx_race_driver_vf2(rx_addr drv) {
    RACE_FRAME_EBP(F, 8, 4, 3);
    const rx_addr p = U32(drv, 0x74);
    if (U32(p, 0x10) == 2) {
        SU32(F, -4, drv);
        RACE_CORE_CALL(1, rx_waypoint_driver_ghost_unsubscribe(U32(U32(p, 0x94), 0x290), drv));
    }
    race_static_dcf304();
    const rx_addr scope = race_profile_begin();
    const rx_addr np = U32(drv, 0xef0);
    if (np != 0 && (U8(np, 0x20) & 1) != 0) {
        uint32_t valid;
        RACE_CORE_CALL(0, valid = rx_net_ref_valid(np) & 0xff);
        if (valid != 0 && U32(U32(U32(drv, 0xef0), 0x1d4), 0) != 0) RACE_CORE_CALL(0, rx_net_player_state_clear(U32(U32(U32(drv, 0xef0), 0x1d4), 0)));
    }
    race_static_dcf304();
    SU32(F, -4, drv);
    RACE_CORE_CALL(1, rx_race_driver_unlisten_network(U32(0, 0x00da6ca0), drv));
    RACE_CORE_CALL(0, rx_base_driver_vf2(drv));
    race_profile_end(scope);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0050ef80, rx_race_driver_vf2)

/* ======================================================================================================================== */
/* the driver's destruction (BaseDriver::vf2's behaviour release, the driver's sdtor in Activity_OnPlayerLeft)               */

/* Behavior::vf0 0052aa00 (thiscall; flags; RET 4) -> this: the shared scalar deleting destructor of 26 behaviour classes: the
 * vtable reset to Behavior's (0x93f7f0), freed when bit 0 is set. Frame: push esi. */
rx_addr rx_behavior_sdtor(rx_addr b, uint32_t flags) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(b, 0, 0x0093f7f0);
    if (flags & 1) { SU32(F, -4, b); RACE_CORE_CALL(1, rx_reflex_aligned_free(b)); }
    RACE_FRAME_END(F);
    return b;
}
RX_FUNCTION(0x0052aa00, rx_behavior_sdtor)

/* 0051ff30 (ESI = the tracked entry): under the lock 0xae1c80 the entry (index +0x38, -1 = none) leaves the tracked table
 * 0x9e3f68 (count [0xa89d04]): the last entry takes its slot (and its index), the count drops, the entry's index = -1 */
void rx_tracked_entry_remove(rx_addr e) {
    SU32(rx_sp(), -4, 0x00ae1c80);
    loop_enter_cs(0x00ae1c80);
    const int32_t idx = I32(e, 0x38);
    if (idx >= 0) {
        const uint32_t n = U32(0, 0x00a89d04);
        SU32(rx_rc_tab(0x009e3f68, 4, (uint32_t)idx), 0, U32(rx_rc_tab(0x009e3f68, 4, n - 1), 0));
        const int32_t i = I32(e, 0x38);
        SU32(U32(rx_rc_tab(0x009e3f68, 4, (uint32_t)i), 0), 0x38, (uint32_t)i);
        SU32(0, 0x00a89d04, n - 1);
        SU32(e, 0x38, 0xffffffffu);
    }
    loop_leave_cs(0x00ae1c80);
}
RX_FUNCTION(0x0051ff30, rx_tracked_entry_remove)

/* BehaviorAirLeveling::sdtor 0051f3b0 (thiscall; flags; RET 4) -> this: its tracked entry (+0x60) removed (0051ff30), the vtable
 * back to Behavior's, freed when bit 0 is set. Frame: push esi / edi. */
rx_addr rx_behavior_air_leveling_sdtor(rx_addr b, uint32_t flags) {
    RACE_FRAME_PLAIN(F, 0, 2);
    SU32(b, 0, 0x0093f9f0);
    RACE_CORE_CALL(0, rx_tracked_entry_remove(b + 0x60));
    SU32(b, 0, 0x0093f7f0);
    if (flags & 1) { SU32(F, -4, b); RACE_CORE_CALL(1, rx_reflex_aligned_free(b)); }
    RACE_FRAME_END(F);
    return b;
}
RX_FUNCTION(0x0051f3b0, rx_behavior_air_leveling_sdtor)

/* 0053b8b0 (EAX = the activity's channel; stdcall behaviour; RET 4): "VehicleWrecked" -> 005234f0 (proxy 0x940430) leaves */
void rx_fix_forward_unlisten_wrecked(rx_addr channel, rx_addr b) {
    act_unlisten(1, channel, 0x0093ee38, 0x00940430, 0x009cdb7c, b, 0x005234f0);
}
RX_FUNCTION(0x0053b8b0, rx_fix_forward_unlisten_wrecked)

/* BehaviorFixForwardProgress::vf2 00523460 (thiscall): its "VehicleWrecked" handler leaves its driver's (+8 -> +0x74 -> +0x94)
 * activity channel (0053b8b0; the behaviour pushed as its argument) */
void rx_fix_forward_vf2(rx_addr b) {
    SU32(rx_sp(), -4, b);
    RACE_CORE_CALL(1, rx_fix_forward_unlisten_wrecked(U32(U32(U32(U32(b, 8), 0x74), 0x94), 0x290), b));
}
RX_FUNCTION(0x00523460, rx_fix_forward_vf2)

/* RaceDriver::sdtor 00513530 (thiscall; flags; RET 4) -> this: the vtable RaceDriver's, NetworkDriver's destructor (0050e2f0), freed
 * when bit 0 is set. Frame: push esi. */
rx_addr rx_race_driver_sdtor(rx_addr d, uint32_t flags) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(d, 0, 0x0093f8dc);
    RACE_CORE_CALL(0, rx_network_driver_dtor(d));
    if (flags & 1) { SU32(F, -4, d); RACE_CORE_CALL(1, rx_reflex_aligned_free(d)); }
    RACE_FRAME_END(F);
    return d;
}
RX_FUNCTION(0x00513530, rx_race_driver_sdtor)

/* ======================================================================================================================== */
/* the race activity's destruction (vf0 from the area update 007669f0, after vf3 / vf6)                                     */

/* 005659e0 (ECX = activity): MXActivityRaceStub's destructor: vtable 0x943454; the audio logic (+0xfd8) destroyed (00580d30) and
 * freed; the three scenarios (+0xff090 / +0x114260 / +0x129430) back to BaseScenario's vtable 0x96cea4; the Activity's destructor
 * (005566b0). Frame: push ecx / esi / edi. */
void rx_race_stub_destroy(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 3);
    const rx_addr audio = U32(act, 0xfd8);
    SU32(act, 0, 0x00943454);
    if (audio != 0) {
        RACE_CORE_CALL(1, rx_activity_audio_destroy(audio));
        RACE_CORE_CALL(1, rx_reflex_aligned_free(audio));
    }
    SU32(act, 0x129430, 0x0096cea4);
    SU32(act, 0x114260, 0x0096cea4);
    SU32(act, 0xff090, 0x0096cea4);
    RACE_CORE_CALL(0, rx_waypoint_activity_base_destroy(act));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005659e0, rx_race_stub_destroy)

/* 00564af0 MXActivityRace::sdtor (thiscall; flags; RET 4) -> this: vtable 0x943554, 005659e0, freed when flags & 1. Frame: push esi. */
rx_addr rx_race_sdtor(rx_addr act, uint32_t flags) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(act, 0, 0x00943554);
    RACE_CORE_CALL(0, rx_race_stub_destroy(act));
    if (flags & 1) RACE_CORE_CALL(1, rx_reflex_aligned_free(act));
    RACE_FRAME_END(F);
    return act;
}
RX_FUNCTION(0x00564af0, rx_race_sdtor)
