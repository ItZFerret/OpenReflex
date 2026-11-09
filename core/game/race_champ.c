/* race_champ.c -- the championship race activities (group rx_race; header race_champ.h).
 *
 * The career's sub events are activities of their own classes: MXActivityRaceChampIntro (type 0x1f), ...Practice (0x22 0x26 0x2a 0x2e
 * 0x32 0x36), ...Heat (0x23 0x27 0x2b 0x33 0x37), ...MainEvent (0x24 0x25 0x28 0x2c 0x2f 0x34 0x35 0x38), ...LCQ (0x29 0x2d 0x39) and
 * the waypoint races' ...ChampIntro (0x21) / ...Practice (0x30) / ...MainEvent (0x31). The race classes are MXActivityRaceStub with a few
 * virtuals replaced (the lap pacing of the series' event type, the music intensity counter 0x9c3edc of the main event); the waypoint
 * classes are MXActivityWaypointRace with a few replaced. This file also holds the base-class virtuals those vtables use and the
 * race classes do not: Activity::vf16 (the restart), vf20 (the camera placement of a player), vf45 (the local player's takeover),
 * the slot 14 placement swap, and the waypoint race's vf3 / vf16 / vf20.
 *
 * Each function is a hand transliteration of the original (its address in the comment). Frames follow the original's prologue
 * (RACE_FRAME_*): callees that keep stack addresses, and the vehicle placement that reads uninitialized lanes of its argument vectors,
 * see the original's ESP. Virtual dispatch goes through the vtable slot (act_vcall / RX_FN), as the other activity code does. */
#include "race_champ.h"
#include "race_extern.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "build_activity_freeride_run.h"
#include "build_activity_pre.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_activity_stub.h"
#include "race_activity_teardown.h"
#include "race_driver.h"
#include "race_misc.h"
#include "career_activity.h"
#include "build_series_init.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "build_alloc.h"
#include "crt_atexit.h"
#include "crt.h"
#include "anim_scenario_misc.h"
#include "loop_world.h"
#include "loop_player.h"
#include "world_misc.h"
#include "world_rider.h"
#include "waypoint_activity.h"
#include "waypoint_in_progress.h"
#include "build_activity_dtors.h"
#include "rider_cap.h"

void rx_waypoint_activity_in_progress(rx_addr activity, float dt);   /* 005798a0 (waypoint_in_progress.c; MXActivityWaypointRace::vf28) */

/* the activity-manager channel [0x00a93f74], and the stunt-event registry [0x00a94304] */
#define CHAMP_MANAGER_CHANNEL U32(0, 0x00a93f74)
#define CHAMP_STUNT_REGISTRY  U32(0, 0x00a94304)
/* the music intensity counter the stunt handlers of the main event write (read by the audio) */
#define CHAMP_INTENSITY       0x009c3edc

/* a thiscall to a virtual of `obj` (slot = byte offset in the vtable) with no arguments, through the registry or the original */
static uint32_t champ_vcall0(rx_addr obj, uint32_t slot) {
    return act_vcall(obj, slot, 0, NULL);
}

/* ======================================================================================================================== */
/* the destructors                                                                                                          */

/* the championship race classes' scalar deleting destructor (thiscall; flags; RET 4) -> this: the class's vtable, the track block
 * (+0x890, 0x330 bytes) cleared, MXActivityRaceStub's destructor (005659e0), the memory freed when flags & 1. Frame: push esi. */
static rx_addr champ_race_sdtor(rx_addr act, uint32_t flags, rx_addr vtable) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(act, 0, vtable);
    RACE_CORE_CALL(3, (void)crt_memset(act + 0x890, 0, 0x330));
    RACE_CORE_CALL(0, rx_race_stub_destroy(act));
    if (flags & 1) RACE_CORE_CALL(1, rx_reflex_aligned_free(act));
    RACE_FRAME_END(F);
    return act;
}
rx_addr rx_champ_heat_sdtor(rx_addr act, uint32_t flags) { return champ_race_sdtor(act, flags, 0x00943854); }
RX_FUNCTION(0x0057dfb0, rx_champ_heat_sdtor)
rx_addr rx_champ_intro_sdtor(rx_addr act, uint32_t flags) { return champ_race_sdtor(act, flags, 0x00943654); }
RX_FUNCTION(0x0057e050, rx_champ_intro_sdtor)
rx_addr rx_champ_lcq_sdtor(rx_addr act, uint32_t flags) { return champ_race_sdtor(act, flags, 0x00943a54); }
RX_FUNCTION(0x0057e1b0, rx_champ_lcq_sdtor)
rx_addr rx_champ_main_sdtor(rx_addr act, uint32_t flags) { return champ_race_sdtor(act, flags, 0x00943954); }
RX_FUNCTION(0x0057e310, rx_champ_main_sdtor)
rx_addr rx_champ_practice_sdtor(rx_addr act, uint32_t flags) { return champ_race_sdtor(act, flags, 0x00943754); }
RX_FUNCTION(0x0057e820, rx_champ_practice_sdtor)

/* the waypoint championship classes' destructor: vtable, +0x890 cleared, optionally (practice / main event) the block at +0x13ec74
 * freed (a free of 0 or of the first pointer; the memset's three arguments stay on the stack and are popped with it: add esp, 0x10),
 * MXActivityWaypointRace's destructor (00578470), the memory freed when flags & 1. Frame: push esi. */
static rx_addr champ_wp_sdtor(rx_addr act, uint32_t flags, rx_addr vtable, int frees_block) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(act, 0, vtable);
    RACE_CORE_CALL(3, (void)crt_memset(act + 0x890, 0, 0x330));
    if (frees_block) RACE_CORE_CALL(1, rx_reflex_aligned_free(U32(act, 0x13ec74)));
    RACE_CORE_CALL(0, rx_waypoint_activity_destroy(act));
    if (flags & 1) RACE_CORE_CALL(1, rx_reflex_aligned_free(act));
    RACE_FRAME_END(F);
    return act;
}
rx_addr rx_champ_wp_intro_sdtor(rx_addr act, uint32_t flags) { return champ_wp_sdtor(act, flags, 0x00943f14, 0); }
RX_FUNCTION(0x0057e880, rx_champ_wp_intro_sdtor)
rx_addr rx_champ_wp_main_sdtor(rx_addr act, uint32_t flags) { return champ_wp_sdtor(act, flags, 0x00943e14, 1); }
RX_FUNCTION(0x0057e960, rx_champ_wp_main_sdtor)
rx_addr rx_champ_wp_practice_sdtor(rx_addr act, uint32_t flags) { return champ_wp_sdtor(act, flags, 0x00943d14, 1); }
RX_FUNCTION(0x0057eaa0, rx_champ_wp_practice_sdtor)

/* 0057ea60 / 0057e920 (ESI = activity, ECX = type, EAX = world; no stack arguments of its own besides the pushed activity -> EAX): the
 * waypoint race's constructor (00578260; the pushed ESI is its argument), then the class's vtable (0x943d14 practice / 0x943e14 main
 * event), the five fields behind the waypoint class (+0x13ec74 .. +0x13ec84) cleared and the lap count flag +0x2c8 = 1. Frame: push ecx. */
static rx_addr champ_wp_construct(rx_addr act, uint32_t type, rx_addr world, rx_addr vtable) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, (void)rx_waypoint_activity_construct(act, type, world));
    SU32(act, 0, vtable);
    for (uint32_t off = 0x13ec74; off <= 0x13ec84; off += 4) SU32(act, off, 0);
    SU32(act, 0x2c8, 1);
    RACE_FRAME_END(F);
    return act;
}
rx_addr rx_champ_wp_practice_construct(rx_addr act, uint32_t type, rx_addr world) { return champ_wp_construct(act, type, world, 0x00943d14); }
RX_FUNCTION(0x0057ea60, rx_champ_wp_practice_construct)
rx_addr rx_champ_wp_main_construct(rx_addr act, uint32_t type, rx_addr world) { return champ_wp_construct(act, type, world, 0x00943e14); }
RX_FUNCTION(0x0057e920, rx_champ_wp_main_construct)

/* ======================================================================================================================== */
/* the event-handler templates and the main event's stunt handlers                                                           */

/* 0058a9a0 (ECX = the event's name; stdcall self, method; RET 8): the member function `method` of `self` (an
 * EventHandlerProxy<void (MXActivityRaceChampMainEvent::*)(MXPlayer*, StuntDescriptor const*)>, vtable 0x945d04) joins the handlers of
 * the stunt registry's event of that name (the template pushes ebx / esi before it asks 00593220). */
void rx_champ_stunt_listen(rx_addr name, rx_addr self, rx_addr method) {
    act_subscribe(0x0058a9a0, 8, 2, 2, CHAMP_STUNT_REGISTRY, name, 0x00945d04, 0x009cbf38, self, method);
}
RX_FUNCTION(0x0058a9a0, rx_champ_stunt_listen)

/* 0058aa60 (stdcall self; RET 4): the same for "PhysicsStuntBegin" (0x9607a0) and 0057e790 (proxy 0x945d24, void (...)(MXPlayer*,
 * StuntDescriptor const*, PhysicsStuntType)) */
void rx_champ_physics_stunt_listen(rx_addr self) {
    act_subscribe(0x0058aa60, 0, 1, 2, CHAMP_STUNT_REGISTRY, 0x009607a0, 0x00945d24, 0x009ce768, self, 0x0057e790);
}
RX_FUNCTION(0x0058aa60, rx_champ_physics_stunt_listen)

/* 0058ab20 (ECX = the event's name; stdcall self, method; RET 8): the handler of 0058a9a0 leaves again (the key {proxy 0x945d04, 0,
 * typeid name, self, method}, the event found, the listener that accepts the key removed) */
void rx_champ_stunt_unlisten(rx_addr name, rx_addr self, rx_addr method) {
    act_unlisten(1, CHAMP_STUNT_REGISTRY, name, 0x00945d04, 0x009cbf38, self, method);
}
RX_FUNCTION(0x0058ab20, rx_champ_stunt_unlisten)

/* 0058ab80 (stdcall self; RET 4): the handler of 0058aa60 leaves */
void rx_champ_physics_stunt_unlisten(rx_addr self) {
    act_unlisten(1, CHAMP_STUNT_REGISTRY, 0x009607a0, 0x00945d24, 0x009ce768, self, 0x0057e790);
}
RX_FUNCTION(0x0058ab80, rx_champ_physics_stunt_unlisten)

/* 0058a940 (EAX = the activity's channel; stdcall act; RET 4): "Lap Completed" -> 005676e0 (proxy 0x945a04) leaves; 0058a420: the
 * waypoint race's, -> 0057a050 (proxy 0x945ca4) */
void rx_champ_unlisten_lap(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x0093eefc, 0x00945a04, 0x009cda64, act, 0x005676e0);
}
RX_FUNCTION(0x0058a940, rx_champ_unlisten_lap)

/* 0057e7c0 (ECX = activity; the player; RET 8): "StuntBegin" in the main event: for the local-less case (player +0x10 == 0) the
 * music intensity goes to the full level (+0xff07c) and the intensity timer (+0xff078) to 4 seconds */
void rx_champ_main_stunt_begin(rx_addr act, rx_addr player, rx_addr desc) {
    (void)desc;
    if (U32(player, 0x10) != 0) return;
    const uint32_t level = U32(act, 0xff07c);
    SU32(act, 0xff080, level);
    SU32(0, CHAMP_INTENSITY, level);
    SF32(act, 0xff078, F32(0, 0x009322c0));   /* 4.0 */
}
RX_FUNCTION(0x0057e7c0, rx_champ_main_stunt_begin)

/* 0057e7f0 (ECX = activity; the player; RET 8): "StuntFailed": the intensity drops to 5 and the timer to 0 */
void rx_champ_main_stunt_failed(rx_addr act, rx_addr player, rx_addr desc) {
    (void)desc;
    if (U32(player, 0x10) != 0) return;
    SU32(act, 0xff080, 5);
    SU32(0, CHAMP_INTENSITY, 5);
    SF32(act, 0xff078, 0.0f);
}
RX_FUNCTION(0x0057e7f0, rx_champ_main_stunt_failed)

/* 0057e790 (ECX = activity; the player; RET 0xc): "PhysicsStuntBegin": as StuntBegin */
void rx_champ_main_physics_stunt_begin(rx_addr act, rx_addr player, rx_addr desc, uint32_t t) {
    (void)desc; (void)t;
    if (U32(player, 0x10) != 0) return;
    const uint32_t level = U32(act, 0xff07c);
    SF32(act, 0xff078, F32(0, 0x009322c0));   /* 4.0 */
    SU32(act, 0xff080, level);
    SU32(0, CHAMP_INTENSITY, level);
}
RX_FUNCTION(0x0057e790, rx_champ_main_physics_stunt_begin)

/* ======================================================================================================================== */
/* vf2 / vf3 / vf5                                                                                                          */

/* 0057e090 MXActivityRaceChampIntro::vf2 (thiscall; RET 4): MXActivityRaceStub::vf2 with the caller's argument, the grid from the gate
 * (005666d0). Frame: push esi. */
void rx_champ_intro_vf2(rx_addr act, rx_addr arg) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_race_stub_vf2(act, arg));
    RACE_CORE_CALL(1, rx_race_grid_from_gate(act));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e090, rx_champ_intro_vf2)

/* 0057e1f0 MXActivityRaceChampHeat::vf2 (also the practice's and the LCQ's; thiscall; RET 4; the argument is not used): as the intro's
 * with the overlay named at 0x9426b4 ("SeriesOverlay") */
void rx_champ_heat_vf2(rx_addr act, rx_addr arg) {
    (void)arg;
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_race_stub_vf2(act, 0x009426b4));
    RACE_CORE_CALL(1, rx_race_grid_from_gate(act));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e1f0, rx_champ_heat_vf2)

/* 0057e350 MXActivityRaceChampMainEvent::vf2 (thiscall; RET 4; the argument is not used): the stub's vf2 with the series overlay, the
 * main event's stunt listeners ("StuntBegin" -> 0057e7c0, "StuntFailed" -> 0057e7f0, "PhysicsStuntBegin" -> 0057e790), the grid. */
void rx_champ_main_vf2(rx_addr act, rx_addr arg) {
    (void)arg;
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_race_stub_vf2(act, 0x009426b4));
    RACE_CORE_CALL(2, rx_champ_stunt_listen(0x0095ba28 /* "StuntBegin" */, act, 0x0057e7c0));
    RACE_CORE_CALL(2, rx_champ_stunt_listen(0x00960764 /* "StuntFailed" */, act, 0x0057e7f0));
    RACE_CORE_CALL(1, rx_champ_physics_stunt_listen(act));
    RACE_CORE_CALL(1, rx_race_grid_from_gate(act));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e350, rx_champ_main_vf2)

/* the series manager's current event type behind the static guard 0x9e275c (MXSeriesManager_Construct + _atexit; 005b6a30): the base
 * type a championship type (>= 0x1f) stands in for in the activity area's name, the lap pacing and the audio. Called at the ESP of the
 * inlined original (the 005b6a30 call pushes the manager). */
uint32_t rx_series_current_type(void) {
    if ((U8(0, 0x009e275c) & 1) == 0) {
        SU32(0, 0x009e275c, U32(0, 0x009e275c) | 1);
        RACE_CORE_CALL(0, (void)rx_series_manager_construct());
        RX_ATEXIT(0x00911da0);
    }
    uint32_t kind;
    RACE_CORE_CALL(1, kind = rx_career_current_event_type(0x009e2188));
    return kind;
}

/* 0057e210 (the heat's, the intro's, the practice's and the LCQ's vf3; thiscall): every driver of the two lists leaves (Activity_OnPlayerLeft
 * takes the list's first each time), the "Lap Completed" listener leaves (0058a940), then a jump to Activity::vf3. Frame: push esi. */
static void champ_players_leave(rx_addr act) {
    while (I32(act, 0x26c) > 0) {
        RACE_CORE_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x268), 0)));
    }
    while (I32(act, 0x280) > 0) {
        RACE_CORE_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x27c), 0)));
    }
}
void rx_champ_heat_vf3(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    champ_players_leave(act);
    RACE_CORE_CALL(1, rx_champ_unlisten_lap(U32(act, 0x290), act));
    RACE_FRAME_END(F);
    rx_activity_vf3(act);                                        /* mov ecx, esi; pop esi; jmp 00557790 */
}
RX_FUNCTION(0x0057e210, rx_champ_heat_vf3)

/* 0057e390 MXActivityRaceChampMainEvent::vf3 (thiscall): the three stunt listeners leave, the drivers leave, then a jump to
 * MXActivityRaceStub::vf3 (00566c80). Frame: push esi. */
void rx_champ_main_vf3(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(2, rx_champ_stunt_unlisten(0x0095ba28, act, 0x0057e7c0));
    RACE_CORE_CALL(2, rx_champ_stunt_unlisten(0x00960764, act, 0x0057e7f0));
    RACE_CORE_CALL(1, rx_champ_physics_stunt_unlisten(act));
    champ_players_leave(act);
    RACE_FRAME_END(F);
    rx_race_stub_vf3(act);                                       /* mov ecx, esi; pop esi; jmp 00566c80 */
}
RX_FUNCTION(0x0057e390, rx_champ_main_vf3)

/* the "Activity Basic Essentials Loaded" event of the activity-manager channel with the activity as payload */
static void champ_essentials_loaded(rx_addr act) {
    const uint32_t payload[1] = {act};
    race_raise_event(CHAMP_MANAGER_CHANNEL, 0x00936038 /* "Activity Basic Essentials Loaded" */, 0x009cbd24, payload, 1);
}

/* 0057e0b0 MXActivityRaceChampHeat::vf5 (thiscall; the intro's too): Activity::vf5, the event type (the series manager's for the intro,
 * type 0x1f), the lap pacing of that type (00556720), for an active activity of an intro-or-earlier type its track name (+0xed4) and
 * racing line, "Activity Basic Essentials Loaded". Frame: push ebp; and esp, -8; push ecx / ebx / esi / edi. */
void rx_champ_heat_vf5(rx_addr act) {
    RACE_FRAME_EBP(F, 8, 0, 4);
    RACE_CORE_CALL(0, rx_activity_vf5(act));
    uint32_t kind = U32(act, 0x23c);
    if (kind == 0x1f) kind = rx_series_current_type();
    const rx_addr world = U32(act, 0x234);
    RACE_CORE_CALL(4, rx_activity_lap_pacing(world + 0x20030, act, kind, U32(act, 0x220), U32(world, 0x10)));
    const uint32_t mode = U32(act, 0x238);
    if (mode != 2 && mode != 0 && (U32(act, 0x23c) < 0x1f || U32(act, 0x23c) <= 0x21)) {
        RACE_CORE_CALL(1, rx_activity_track_name(act + 0xed4, kind, act));
        RACE_CORE_CALL(2, rx_mxworld_set_track(U32(act, 0x234), act + 0xed4, U32(act, 0xbb0) ? act + 0x890 : 0));
    }
    champ_essentials_loaded(act);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e0b0, rx_champ_heat_vf5)

/* 0057e410 MXActivityRaceChampLCQ::vf5 (thiscall; also the practice's and the main event's): as the heat's without the track name
 * and racing line. Frame: push ebp; and esp, -8; push ecx / ebx / esi / edi. */
void rx_champ_lcq_vf5(rx_addr act) {
    RACE_FRAME_EBP(F, 8, 0, 4);
    RACE_CORE_CALL(0, rx_activity_vf5(act));
    uint32_t kind = U32(act, 0x23c);
    if (kind == 0x1f) kind = rx_series_current_type();
    const rx_addr world = U32(act, 0x234);
    RACE_CORE_CALL(4, rx_activity_lap_pacing(world + 0x20030, act, kind, U32(act, 0x220), U32(world, 0x10)));
    champ_essentials_loaded(act);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e410, rx_champ_lcq_vf5)

/* ======================================================================================================================== */
/* vf15 / vf17: the players' controls                                                                                         */

/* 0057eaf0 (slot 15; thiscall): Activity::vf15, then every driver's player's two flags set (the vehicle's +0x1328 block byte +0x14e and
 * the rider's +0xa2) */
static void champ_driver_flags(rx_addr act, uint8_t value) {
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        rx_addr player = U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74);
        SU8(U32(U32(player, 0x1ac), 0x1328), 0x14e, value);
        player = U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74);
        SU8(U32(player, 0x1b4), 0xa2, value);
    }
}
void rx_champ_vf15(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(0, rx_activity_vf15(act));
    champ_driver_flags(act, 1);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057eaf0, rx_champ_vf15)

/* 0057e270 (slot 17; thiscall): Activity::vf17, then the same two flags cleared */
void rx_champ_vf17(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(0, rx_activity_vf17(act));
    champ_driver_flags(act, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e270, rx_champ_vf17)

/* ======================================================================================================================== */
/* vf25 / vf28 / vf29 / vf34 / vf35 / vf38 / vf52                                                                            */

/* 0057e1a0 MXActivityRaceChampIntro::vf25 (thiscall; dt; RET 4): MXActivityRaceStub::vf25 (the float goes through the x87 stack) */
void rx_champ_intro_vf25(rx_addr act, float dt) {
    RACE_CORE_CALL(1, rx_race_stub_vf25(act, dt));
}
RX_FUNCTION(0x0057e1a0, rx_champ_intro_vf25)

/* 0057e910 MXActivityWaypointRaceChampIntro::vf25 (thiscall; dt; RET 4) */
void rx_champ_wp_intro_vf25(rx_addr act, float dt) {
    RACE_CORE_CALL(1, rx_waypoint_activity_vf25(act, dt));
}
RX_FUNCTION(0x0057e910, rx_champ_wp_intro_vf25)

/* 0057e000 MXActivityRaceChampHeat::vf28 (thiscall; dt; RET 4): Activity::vf28; in the in-progress state the waypoint track's debug
 * traversal when there is one (+0xb8c), the gaps to the leader (00567b10), the lap flags (00567e30) and the lapping warnings (00567f30).
 * Frame: push ebp; and esp, -8; push ecx; push edi. */
void rx_champ_heat_vf28(rx_addr act, float dt) {
    RACE_FRAME_EBP(F, 8, 0, 2);
    RACE_CORE_CALL(1, rx_activity_in_progress(act, dt));
    if (U32(act, 0x21c) == 2) {
        if (U32(act, 0xb8c) != 0) RACE_CORE_CALL(0, rx_waypoint_track_debug(act + 0x890));
        RACE_CORE_CALL(1, rx_race_gaps_to_leader(act));
        RACE_CORE_CALL(0, rx_race_lap_flags(act));
        RACE_CORE_CALL(1, rx_race_lapping_warnings(act));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e000, rx_champ_heat_vf28)

/* 0057e2d0 MXActivityRaceChampLCQ::vf28 (thiscall; dt; RET 4): as the heat's up to the gaps to the leader */
void rx_champ_lcq_vf28(rx_addr act, float dt) {
    RACE_FRAME_EBP(F, 8, 0, 2);
    RACE_CORE_CALL(1, rx_activity_in_progress(act, dt));
    if (U32(act, 0x21c) == 2) {
        if (U32(act, 0xb8c) != 0) RACE_CORE_CALL(0, rx_waypoint_track_debug(act + 0x890));
        RACE_CORE_CALL(1, rx_race_gaps_to_leader(act));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e2d0, rx_champ_lcq_vf28)

/* 0057e860 MXActivityRaceChampPractice::vf28 (thiscall; dt; RET 4): Activity::vf28 */
void rx_champ_practice_vf28(rx_addr act, float dt) {
    RACE_CORE_CALL(1, rx_activity_in_progress(act, dt));
}
RX_FUNCTION(0x0057e860, rx_champ_practice_vf28)

/* 0057e9e0 MXActivityWaypointRaceChampMainEvent::vf28 (thiscall; dt; RET 4): MXActivityWaypointRace::vf28 */
void rx_champ_wp_main_vf28(rx_addr act, float dt) {
    RACE_CORE_CALL(1, rx_waypoint_activity_in_progress(act, dt));
}
RX_FUNCTION(0x0057e9e0, rx_champ_wp_main_vf28)

/* the main event's distance to the finish line of a driver on its last lap: the node's plane (+0x50 .. +0x58 the normal, +0x4f4 the
 * plane's offset) against the driver's position (+0x80 .. +0x88): offset - normal . (position - node position), summed as
 * ((z + y) + x) in the original's order */
static float champ_gap_to_line(rx_addr driver, rx_addr node) {
    const float dz = F32(driver, 0x88) - F32(node, 8);
    float acc = F32(node, 0x58) * dz;
    const float dy = F32(driver, 0x84) - F32(node, 4);
    const float dx = F32(driver, 0x80) - F32(node, 0);
    const float ay = F32(node, 0x54) * dy;
    const float ax = F32(node, 0x50) * dx;
    acc = acc + ay;
    acc = acc + ax;
    return F32(node, 0x4f4) - acc;
}

/* 0057e4f0 MXActivityRaceChampMainEvent::vf28 (thiscall; dt; RET 4): Activity::vf28; in the in-progress state the music intensity
 * (+0xff080, also the global counter 0x9c3edc) follows the timer +0xff078 (counting down by dt: intensity = 5 + (level - 5) * timer *
 * 0.25), the track debug, the gaps and lap flags, and for a driver of the local player (player +0x10 == 0) on its last lap
 * (lap +0xf1c == laps - 1) closer than 100 to the finish line the intensity 100 - (that distance).
 * Frame: push ebp; and esp, -16; sub esp, 8; push esi / edi. */
void rx_champ_main_vf28(rx_addr act, float dt) {
    RACE_FRAME_EBP(F, 16, 8, 2);
    RACE_CORE_CALL(1, rx_activity_in_progress(act, dt));
    if (U32(act, 0x21c) == 2) {
        const float timer = F32(act, 0xff078);
        if (timer >= 0.0f) {   /* comiss against 0.0: below or unordered skips */
            const int32_t span = I32(act, 0xff07c) - 5;
            const float quarter = timer * F32(0, 0x0092ff84);   /* 0.25 */
            const float next_timer = timer - dt;
            float level = (float)span * quarter;
            level = level + F32(0, 0x00935890);                 /* 5.0 */
            const int32_t intensity = rx_cvtt(level);
            SU32(act, 0xff080, (uint32_t)intensity);
            SU32(0, CHAMP_INTENSITY, (uint32_t)intensity);
            SF32(act, 0xff078, next_timer);
        }
        if (U32(act, 0xb8c) != 0) RACE_CORE_CALL(0, rx_waypoint_track_debug(act + 0x890));
        RACE_CORE_CALL(1, rx_race_gaps_to_leader(act));
        RACE_CORE_CALL(0, rx_race_lap_flags(act));
        const rx_addr drivers = U32(act, 0x268);
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            const rx_addr driver = U32(drivers, 4u * (uint32_t)i);
            const rx_addr player = U32(driver, 0x74);
            if (U32(player, 0x10) != 0) continue;
            if (I32(driver, 0xf1c) != I32(act, 0xff070) - 1) continue;
            const rx_addr node = U32(driver, 0x90);
            const float gap = champ_gap_to_line(driver, node);
            if (!(100.0 > (double)gap)) continue;   /* comisd: not above, or unordered, skips */
            const int32_t closeness = 0x64 - rx_cvtt(champ_gap_to_line(driver, node));
            SU32(act, 0xff080, (uint32_t)closeness);
            SU32(0, CHAMP_INTENSITY, (uint32_t)closeness);
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e4f0, rx_champ_main_vf28)

/* 0057c010 (the heat's and the LCQ's vf29; thiscall; dt; RET 4): when the results timer (+0xff064) is not above 0 and the class's virtual 57
 * (+0xe4) says so, "Show Activity Results" (00559a40); then the timer advances by dt. Frame: push esi. */
void rx_champ_heat_vf29(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(F, 0, 1);
    if (F32(act, 0xff064) <= 0.0f) {   /* comiss 0.0, timer; below or unordered skips */
        if (champ_vcall0(act, 0xe4) != 0) RACE_CORE_CALL(0, rx_waypoint_show_results(act));
    }
    SF32(act, 0xff064, rx_adds(dt, F32(act, 0xff064)));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057c010, rx_champ_heat_vf29)

/* 00559a00 Activity::vf29 (thiscall; dt; RET 4): the base class's Post update, used by the Free Ride (and every class that does not override
 * slot 29): the same body as the heat's 0057c010 above, with the frame push ecx, push esi. When the results timer (+0xff064) is not above 0
 * and the class's virtual 57 (+0xe4) says so, "Show Activity Results" (00559a40); then the timer advances by dt.
 * Reached by the audit's forced Free Ride WindDown -> Post (stop_audit_run STOP_AUDIT_LIFE: a Free Ride has no natural end). */
void rx_activity_vf29(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(F, 4, 1);
    if (F32(act, 0xff064) <= 0.0f) {   /* comiss 0.0, timer; below or unordered skips */
        if (champ_vcall0(act, 0xe4) != 0) RACE_CORE_CALL(0, rx_waypoint_show_results(act));
    }
    SF32(act, 0xff064, rx_adds(dt, F32(act, 0xff064)));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00559a00, rx_activity_vf29)

/* 0057e6b0 MXActivityRaceChampMainEvent::vf29 (thiscall; dt; RET 4): as the heat's, then the global intensity counter takes +0xff080 */
void rx_champ_main_vf29(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(F, 0, 1);
    if (F32(act, 0xff064) <= 0.0f) {
        if (champ_vcall0(act, 0xe4) != 0) RACE_CORE_CALL(0, rx_waypoint_show_results(act));
    }
    SF32(act, 0xff064, rx_adds(dt, F32(act, 0xff064)));
    SU32(0, CHAMP_INTENSITY, U32(act, 0xff080));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e6b0, rx_champ_main_vf29)

/* 0057e700 MXActivityRaceChampMainEvent::vf34 (thiscall; one argument, not used; RET 4): the intensity back to 5 */
void rx_champ_main_vf34(rx_addr act, uint32_t unused) {
    (void)unused;
    SU32(act, 0xff080, 5);
    SU32(0, CHAMP_INTENSITY, 5);
}
RX_FUNCTION(0x0057e700, rx_champ_main_vf34)

/* 0057e720 Activity::vf35 (thiscall; one argument, not used; RET 4): the players' controls enabled (0055a1e0), the world's replay
 * camera flag +0x21de0 cleared and ReplayCameras_Update(world, 0.033), [0xa941b8] = 0. Frame: push esi. */
void rx_champ_vf35(rx_addr act, uint32_t unused) {
    (void)unused;
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_activity_enable_controls(1, act));
    const float dt = F32(0, 0x00977818);   /* 0.033 */
    const rx_addr world = U32(act, 0x234);
    SU32(world, 0x21de0, 0);
    RACE_CORE_CALL(2, rx_replay_cameras_update(world, dt));
    SU32(0, 0x00a941b8, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e720, rx_champ_vf35)

/* 0057dff0 MXActivityRaceChampHeat::vf38 (a jump to MXActivityRaceStub::vf38 00568340; thiscall; RET 4) */
void rx_champ_heat_vf38(rx_addr act, uint32_t old) {
    rx_race_stub_vf38(act, old);
}
RX_FUNCTION(0x0057dff0, rx_champ_heat_vf38)

/* 0057e4b0 MXActivityRaceChampMainEvent::vf38 (thiscall; the old state; RET 4): MXActivityRaceStub::vf38, then the intensity timer 4
 * seconds and the intensity 100. Frame: push esi. */
void rx_champ_main_vf38(rx_addr act, uint32_t old) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_race_stub_vf38(act, old));
    SF32(act, 0xff078, F32(0, 0x009322c0));   /* 4.0 */
    SU32(act, 0xff080, 0x64);
    SU32(0, CHAMP_INTENSITY, 0x64);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e4b0, rx_champ_main_vf38)

/* 0057ea30 (slot 52 of the championship classes; thiscall): the music state (0x9c3edc = 0, 0x9c3ed8 = 0.5) and the request 1 unless
 * the music is already in 1 (0x9c47bc) */
void rx_champ_vf52(rx_addr act) {
    (void)act;
    SU32(0, CHAMP_INTENSITY, 0);
    SF32(0, 0x009c3ed8, F32(0, 0x009322cc));   /* 0.5 */
    if (U32(0, 0x009c47bc) != 1) SU32(0, 0x009c47c0, 1);
}
RX_FUNCTION(0x0057ea30, rx_champ_vf52)

/* 0057e760 MXActivityRaceChampMainEvent::vf52 (thiscall): the music state intensity 5, 1.0, request 2 */
void rx_champ_main_vf52(rx_addr act) {
    (void)act;
    SF32(0, 0x009c3ed8, F32(0, 0x00931f24));   /* 1.0 */
    SU32(0, CHAMP_INTENSITY, 5);
    if (U32(0, 0x009c47bc) != 2) SU32(0, 0x009c47c0, 2);
}
RX_FUNCTION(0x0057e760, rx_champ_main_vf52)

/* ======================================================================================================================== */
/* the waypoint championship classes                                                                                          */

/* 0057e8c0 MXActivityWaypointRaceChampIntro::vf2 (thiscall; RET 4; the argument is not used): MXActivityWaypointRace::vf2 with the
 * series overlay (0x9426b4), then the start slots (005787a0). Frame: push esi. */
void rx_champ_wp_intro_vf2(rx_addr act, rx_addr arg) {
    (void)arg;
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_waypoint_activity_vf2(act, 0x009426b4));
    RACE_CORE_CALL(0, rx_waypoint_start_slots(act));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0057e8c0, rx_champ_wp_intro_vf2)

/* 0057e9b0 MXActivityWaypointRaceChampMainEvent::vf2 (also the practice's; a jump to 00578590 with the argument replaced by 0x9426b4) */
void rx_champ_wp_main_vf2(rx_addr act, rx_addr arg) {
    (void)arg;
    SU32(rx_sp(), 4, 0x009426b4);
    rx_waypoint_activity_vf2(act, 0x009426b4);
}
RX_FUNCTION(0x0057e9b0, rx_champ_wp_main_vf2)

/* 0057e8e0 (a jump to MXActivityWaypointRace::vf5 00578500) */
void rx_champ_wp_intro_vf5(rx_addr act) {
    rx_waypoint_activity_vf5(act);
}
RX_FUNCTION(0x0057e8e0, rx_champ_wp_intro_vf5)

/* 0057e8f0 (a jump to MXActivityWaypointRace::vf3 00578b30) */
void rx_champ_wp_intro_vf3(rx_addr act) {
    rx_waypoint_activity_vf3(act);
}
RX_FUNCTION(0x0057e8f0, rx_champ_wp_intro_vf3)

/* 0057e900 (slot 37; a jump to Activity::vf37 005589c0; thiscall; the old state; RET 4) */
void rx_champ_wp_vf37(rx_addr act, uint32_t old) {
    (void)old;
    rx_activity_vf37(act);
}
RX_FUNCTION(0x0057e900, rx_champ_wp_vf37)

/* 0057e9f0 (slot 35; a jump to MXActivityFreestyleStub::vf35 = the waypoint race's 00579b20) */
void rx_champ_wp_vf35(rx_addr act, uint32_t next) {
    rx_waypoint_activity_vf35(act, next);
}
RX_FUNCTION(0x0057e9f0, rx_champ_wp_vf35)

/* 0057e9c0 (slot 16 of the main event; a jump to MXActivityWaypointRace::vf16 00578cc0) */
void rx_champ_wp_main_vf16(rx_addr act) {
    rx_waypoint_activity_vf16(act);
}
RX_FUNCTION(0x0057e9c0, rx_champ_wp_main_vf16)

/* 0057e9d0 (slot 42 of the main event; a jump to MXActivityWaypointRace::vf42 00579300) */
void rx_champ_wp_main_vf42(rx_addr act) {
    rx_waypoint_activity_vf42(act);
}
RX_FUNCTION(0x0057e9d0, rx_champ_wp_main_vf42)

/* 0057eb60 MXActivityWaypointRaceChampPractice::vf16 (thiscall): the class's virtual 18 (+0x48), then a jump to
 * MXActivityWaypointRace::vf16. Frame: push esi (popped before the jump). */
void rx_champ_wp_practice_vf16(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    (void)champ_vcall0(act, 0x48);
    RACE_FRAME_END(F);
    rx_waypoint_activity_vf16(act);
}
RX_FUNCTION(0x0057eb60, rx_champ_wp_practice_vf16)

/* 0057a3e0 MXActivityWaypointRace::vf20 (thiscall; a player; RET 4): the camera placement of a player. A driver whose gate index
 * (+0x10d4) is negative: Activity::vf20. Else the gate's virtual 15 (+0x3c) writes the gate's transform (0x40 bytes) at the frame's
 * +0x30; its first row becomes the position, its fourth the direction, and the vehicle is placed through the effect query
 * (00460020); the player fades out (0054b780). Frame: push ebp; and esp, -16; sub esp, 0x64; push ebx / esi / edi. */
void rx_waypoint_activity_vf20(rx_addr act, rx_addr player) {
    ACT_FRAME_EBP(L, 16, 0x64, 3);
    int32_t index;
    ACT_CALLR(index, 0, rx_world_group_rider_index(act, player));
    const rx_addr driver = U32(U32(act, 0x268), 4u * (uint32_t)index);
    const int32_t gate_index = I32(driver, 0x10d4);
    if (gate_index < 0) {
        ACT_CALL(1, rx_activity_vf20(act, player));
        ACT_END(L);
        return;
    }
    const rx_addr gate = U32(U32(act, 0xff078), 4u * (uint32_t)gate_index);
    const uint32_t transform_arg[1] = {L + 0x30};
    act_vcall(gate, 0x3c, 1, transform_arg);
    SU32(L, 0x10, U32(L, 0x30));
    SU32(L, 0x14, U32(L, 0x34));
    SU32(L, 0x18, U32(L, 0x38));
    SU32(L, 0x20, U32(L, 0x60));
    SU32(L, 0x24, U32(L, 0x64));
    SU32(L, 0x28, U32(L, 0x68));
    ACT_CALL(2, rx_vehicle_effect_query(U32(player, 0x1ac), L + 0x20, L + 0x10));
    ACT_CALL(0, rx_player_set_fade_out(player));
    ACT_END(L);
}
RX_FUNCTION(0x0057a3e0, rx_waypoint_activity_vf20)

/* 0055d120 Activity::vf20 (thiscall; a player; RET 4): the camera placement of a player: the player's driver (00559a50's index into
 * the player's list or the second list when player +0x908 is set); the point `position + (-direction) * k * 0.5` with k the vehicle
 * class's distance (0x93569c[vehicle +0x194]) is handed to the simulation's virtual 20 (+0x50), which writes a position and two axes
 * at +0x30 / +0x20 / +0x40, the vehicle is placed there (0045f600) and the player fades out (0054b780).
 * Frame: push ebp; and esp, -16; sub esp, 0x44; push ebx / esi / edi. */
void rx_activity_vf20(rx_addr act, rx_addr player) {
    ACT_FRAME_EBP(L, 16, 0x44, 3);
    int32_t index;
    ACT_CALLR(index, 0, rx_world_group_rider_index(act, player));
    const rx_addr driver = U32(U32(act, U32(player, 0x908) != 0 ? 0x27c : 0x268), 4u * (uint32_t)index);
    const rx_addr transform = U32(driver, 0x144);
    const float px = F32(transform, 0), py = F32(transform, 4), pz = F32(transform, 8);
    SF32(L, 0x10, px); SF32(L, 0x14, py); SF32(L, 0x18, pz);
    const rx_addr vehicle = U32(player, 0x1ac);
    const float distance = F32(0, 0x0093569c + 4 * U32(vehicle, 0x194));
    const rx_addr direction = U32(driver, 0x144) + 0x10;
    const float neg_zero = F32(0, 0x009774a8);   /* -0.0 */
    const float half = F32(0, 0x009322cc);       /* 0.5 */
    float x = rx_muls(rx_subs(neg_zero, F32(direction, 0)), distance);
    float y = rx_muls(rx_subs(neg_zero, F32(direction, 4)), distance);
    float z = rx_muls(rx_subs(neg_zero, F32(direction, 8)), distance);
    x = rx_muls(x, half);
    y = rx_muls(y, half);
    z = rx_muls(z, half);
    SF32(L, 0x10, rx_adds(x, px));
    SF32(L, 0x14, rx_adds(y, py));
    SF32(L, 0x18, rx_adds(z, pz));
    const rx_addr sim = U32(vehicle, 0x234);
    {   /* the simulation's virtual 20: (position, direction, out position, out axis, out axis), RET 0x14 */
        void (*vf20)(rx_addr, rx_addr, rx_addr, rx_addr, rx_addr, rx_addr) =
            RX_FN(race_vfn(sim, 20), void (*)(rx_addr, rx_addr, rx_addr, rx_addr, rx_addr, rx_addr));
        if (vf20) {
            ACT_CALL(5, vf20(sim, L + 0x10, direction, L + 0x30, L + 0x20, L + 0x40));
        } else {
            const uint32_t args[5] = {L + 0x10, direction, L + 0x30, L + 0x20, L + 0x40};
            rx_call_original(race_vfn(sim, 20), RX_THISCALL, sim, 5, args);
        }
    }
    ACT_CALL(3, rx_vehicle_place(vehicle, L + 0x30, L + 0x20, L + 0x40));
    ACT_CALL(0, rx_player_set_fade_out(player));
    ACT_END(L);
}
RX_FUNCTION(0x0055d120, rx_activity_vf20)

/* 00578b30 MXActivityWaypointRace::vf3: rx_waypoint_activity_vf3 (build_activity_dtors.c) */

/* 00578cc0 MXActivityWaypointRace::vf16 (thiscall): the class's virtual 18 (+0x48), Activity::vf16 (the restart), and the lap-time array
 * (+0xff098: laps * gates + 1 floats) cleared. Frame: push esi. */
void rx_waypoint_activity_vf16(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    (void)champ_vcall0(act, 0x48);
    RACE_CORE_CALL(0, rx_activity_vf16(act));
    if (U32(act, 0xff098) != 0) {
        int32_t limit = (int32_t)(U32(act, 0xff074) * U32(act, 0xff090)) + 1;
        if (limit > 0) {
            int32_t i = 0;
            do {
                SF32(U32(act, 0xff098), 4u * (uint32_t)i, 0.0f);
                limit = (int32_t)(U32(act, 0xff074) * U32(act, 0xff090)) + 1;
                i++;
            } while (i < limit);
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00578cc0, rx_waypoint_activity_vf16)

/* ======================================================================================================================== */
/* the base class                                                                                                           */

/* 0054e190 (EAX = the player; no stack arguments): the player's state reset for a restart: its camera object's (+0x99c) virtual 29
 * (+0x74) when that object has a pending flag (+0x2f4), the elimination counters (+0x11b8 .. +0x11c4) in an activity of mode 7, the
 * wreck state (0054e890), the timers and offsets (+0x1168 ...) cleared, +0x1220 = 1, and the active camera entity's virtual 36 (+0x90)
 * with 1. Frame: push ecx / esi / edi. */
void rx_player_restart_reset(rx_addr player) {
    RACE_FRAME_PLAIN(F, 0, 3);
    const rx_addr camera = U32(player, 0x99c);
    if (U32(camera, 0x2f4) != 0) (void)champ_vcall0(camera, 0x74);
    if (U32(U32(player, 0x94), 0x238) == 7) {
        SU32(player, 0x11b8, 0); SU32(player, 0x11bc, 0); SU32(player, 0x11c0, 0); SU32(player, 0x11c4, 0);
    }
    RACE_CORE_CALL(0, rx_world_rider_clear_wreck(player));
    SF32(player, 0x1168, 0.0f); SF32(player, 0x116c, 0.0f);
    SU32(player, 0x1224, 0); SU32(player, 0x1280, 0);
    SF32(player, 0x122c, 0.0f); SF32(player, 0x1230, 0.0f);
    SF32(player, 0x1250, 0.0f); SF32(player, 0x1254, 0.0f); SF32(player, 0x1258, 0.0f);
    SF32(player, 0x1260, 0.0f); SF32(player, 0x1264, 0.0f); SF32(player, 0x1268, 0.0f);
    SF32(player, 0x1270, 0.0f); SF32(player, 0x1274, 0.0f); SF32(player, 0x1278, 0.0f);
    SU32(player, 0x1220, 1);
    rx_addr entity;
    RACE_CORE_CALL(0, entity = rx_world_player_active_camera_entity(player));
    if (entity != 0) {
        const uint32_t one[1] = {1};
        act_vcall(entity, 0x90, 1, one);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0054e190, rx_player_restart_reset)

/* 00557b40 Activity::vf16 (thiscall): the restart. The audio logic's virtual 33 (+0x84), the activity's lap-counted flag +0x1068 = 1,
 * "Restart" raised on the activity's channel, the clip recorder's pending time restored (0xa942a4 from the recorded activity +0x294)
 * when it was waiting (0xa942b4 == 1), the class's virtuals 17 (+0x44) and 15 (+0x3c), every driver's player reset (+0x154 of its
 * vehicle's block cleared and 0054e190), the session's virtual 16 (+0x40), and the engine's 0x19d0 input records re-initialised
 * (+0x70 records: the dead zone / scale defaults). Frame: push ebp; and esp, -16; push ecx / ebx / esi / edi. */
void rx_activity_vf16(rx_addr act) {
    RACE_FRAME_EBP(F, 16, 0, 4);
    const rx_addr audio = U32(act, 0xfd8);
    if (audio != 0) (void)champ_vcall0(audio, 0x84);
    const rx_addr channel = U32(act, 0x290);
    SU32(act, 0x1068, 1);
    {
        const uint32_t payload[1] = {act};
        race_raise_event(channel, 0x00941ba0 /* "Restart" */, 0x009cbd24, payload, 1);
    }
    if (U32(0, 0x00a942b4) == 1) {
        const rx_addr recorded = U32(0, 0x00a942b8);
        SU32(0, 0x00a942a4, U32(recorded, 0x294));
        SU32(0, 0x00a942b4, 0);
    }
    (void)champ_vcall0(act, 0x44);
    (void)champ_vcall0(act, 0x3c);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr player = U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74);
        SF32(U32(player, 0x1ac), 0x154, 0.0f);
        RACE_CORE_CALL(0, rx_player_restart_reset(player));
    }
    race_static_dcf304();
    {   /* NetworkSession::vf16 006e3450: acts only for an online host (+0x1c4c, +0x1b0 == 1). The offline product has no session object
         * ([0x00da6cb0] = 0: every other reader of it tests the fields at address 0 and finds nothing), so there is nothing to call. */
        const rx_addr session = U32(0, 0x00da6cb0);
        if (session != 0) (void)act_vcall(session, 0x40, 0, NULL);
    }
    const rx_addr engine = U32(0, 0x00a95844);
    rx_addr table = U32(engine, 0x1c + 4 * U32(engine, 4));
    if (U32(table, 0x19d0) != 0) {
        uint32_t k = 0;
        uint32_t offset = 0x19b8;
        do {
            const rx_addr record = U32(U32(table, offset), 0x70);
            SU32(record, 0x00, 0);
            SU32(record, 0x40, 0);
            SF32(record, 0x30, 0.0f);
            SF32(record, 0x34, 0.0f);
            SF32(record, 0x38, 0.0f);
            SF32(record, 0x3c, F32(0, 0x00931f24));   /* 1.0 */
            SF32(record, 0x1c, F32(0, 0x0092fe48));   /* 3.0 */
            SU32(record, 0x14, 1);
            SF32(record, 0x20, 0.0f);
            SU32(record, 0x18, 4);
            SF32(record, 0x28, 0.0f);
            SF32(record, 0x2c, F32(0, 0x00931f24));
            table = U32(engine, 0x1c + 4 * U32(engine, 4));
            k++;
            offset += 4;
        } while (k < U32(table, 0x19d0));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00557b40, rx_activity_vf16)

/* 00557570 Activity::vf45 (thiscall; two drivers; RET 8): the local player's takeover of a driver's player slot: `from` hands its
 * controller slot (player +0x1f0) to `to`. In an online session (the session +0x1c4c set and +0x1b0 != 1) every driver of both lists is
 * detached first (player +8 = -1, +0x1f0 = -1 unless it has a controller and no flag, +0x1cc = 0) and `to` takes the slot; else the
 * single driver is reset (the vehicle's virtual 55 (+0xdc) with 2, the rider's pose override 00621490, the trick state cleared) and
 * `to` takes it. Frame: push ebx / ebp / esi / edi. */
void rx_activity_vf45(rx_addr act, rx_addr from, rx_addr to) {
    RACE_FRAME_PLAIN(F, 0, 4);
    race_static_dcf304();   /* the session's static initialiser (006de1f0, _atexit) behind bit 0 of 0xdcf304 */
    const rx_addr session = U32(0, 0x00da6cb0);
    if (U32(session, 0x1c4c) != 0 && U32(session, 0x1b0) != 1) {
        const uint32_t slot = U32(U32(from, 0x74), 0x1f0);
        for (int32_t list = 0; list < 2; list++) {
            const rx_addr base = list == 0 ? 0x268 : 0x27c;
            const rx_addr count = list == 0 ? 0x26c : 0x280;
            for (int32_t i = 0; i < I32(act, count); i++) {
                const rx_addr driver = U32(U32(act, base), 4u * (uint32_t)i);
                SU32(U32(driver, 0x74), 8, 0xffffffffu);
                const rx_addr p = U32(driver, 0x74);
                if (U32(p, 4) == 0 || U32(p, 0x10) != 0) SU32(p, 0x1f0, 0xffffffffu);
                SU32(U32(driver, 0x74), 0x1cc, 0);
            }
        }
        const rx_addr taker = U32(to, 0x74);
        SU32(taker, 8, 0);
        SU32(U32(to, 0x74), 0x1f0, slot);
        SU32(U32(to, 0x74), 0x1cc, 1);
    } else {
        const rx_addr leaver = U32(from, 0x74);
        const uint32_t slot = U32(leaver, 0x1f0);
        SU32(leaver, 8, 0xffffffffu);
        SU32(U32(from, 0x74), 0x1f0, 0xffffffffu);
        SU32(U32(from, 0x74), 0x10, 1);
        const uint32_t two[1] = {2};
        act_vcall(U32(U32(from, 0x74), 0x1ac), 0xdc, 1, two);
        SU32(U32(from, 0x74), 0x1cc, 0);
        const rx_addr rider = U32(U32(from, 0x74), 0x1b4);
        RACE_CORE_CALL(1, rx_world_rider_end_pose_override(rider));
        SU32(rider, 0x5bb60, 0);
        {
            const rx_addr vehicle = U32(U32(from, 0x74), 0x1ac);
            SF32(vehicle, 0x220, 0.0f); SF32(vehicle, 0x224, 0.0f); SF32(vehicle, 0x228, 0.0f);
            SF32(vehicle, 0x22c, 0.0f); SF32(vehicle, 0x230, 0.0f);
        }
        SU32(U32(to, 0x74), 8, 0);
        SU32(U32(to, 0x74), 0x1f0, slot);
        SU32(U32(to, 0x74), 0x1cc, 1);
        SU32(U32(U32(from, 0x74), 0x1b4), 0x5bb60, 1);
        {
            const rx_addr vehicle = U32(U32(to, 0x74), 0x1ac);
            SF32(vehicle, 0x220, 0.0f); SF32(vehicle, 0x224, 0.0f); SF32(vehicle, 0x228, 0.0f);
            SF32(vehicle, 0x22c, 0.0f); SF32(vehicle, 0x230, 0.0f);
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00557570, rx_activity_vf45)

/* 00567ac0 MXActivityRaceStub::vf45 (thiscall; two drivers; RET 8): Activity::vf45, then when both drivers' players are in state 3
 * (+0xee8) the best-lap event for `to` with its lap time (+0x10c4). Frame: push ebx / esi / edi. */
void rx_race_stub_vf45(rx_addr act, rx_addr from, rx_addr to) {
    RACE_FRAME_PLAIN(F, 0, 3);
    RACE_CORE_CALL(2, rx_activity_vf45(act, from, to));
    if (I32(from, 0xee8) == 3 && I32(to, 0xee8) == 3) {
        const float lap = F32(to, 0x10c4);
        RACE_CORE_CALL(1, rx_event_best_lap(U32(act, 0x290), to, lap));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00567ac0, rx_race_stub_vf45)

/* 0055eb70 MXActivityRaceStub::vf9 (a jump to the virtual 9 (+0x24) of the finish scenario at +0x114260, with that scenario as this) */
uint32_t rx_race_stub_vf9(rx_addr act) {
    const rx_addr scenario = act + 0x114260;
    const rx_addr fn = U32(U32(scenario, 0), 0x24);
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    if (f) return f(scenario);
    return rx_call_original(fn, RX_THISCALL, scenario, 0, NULL);
}
RX_FUNCTION(0x0055eb70, rx_race_stub_vf9)

/* 005693d0 (slot 14 of the activity classes; thiscall; a driver and a place; RET 8): in the in-progress state. In practice mode (+0x238
 * == 2) the Player List row whose PlayerName is the driver's player's name (player +0x14) requests the FreeRide activity (004dde70: type
 * 8, area 0) and the activity table gets that row (004dedc0) when it has none. Else the driver moves to `place` (clamped to the last
 * place) in the standings array (+0x2d0), swapping with the driver that held it (the places +0x78 swapped), the standings count
 * +0x300 is set and the state goes to 3 (005586c0). Frame: push ebp; and esp, -8; push ecx / ebx / esi / edi. */
void rx_champ_place_swap(rx_addr act, rx_addr driver, int32_t place) {
    ACT_FRAME_EBP(L, 8, 0, 4);
    if (U32(act, 0x21c) != 2) { ACT_END(L); return; }
    if (U32(act, 0x238) == 2) {
        uint32_t rows;
        ACT_CALLR(rows, 0, rx_tbl_row_count(ACT_PLAYER_LIST));
        if (rows == 0) { ACT_END(L); return; }
        uint32_t row = 0;
        for (;;) {
            const rx_addr column = act_column(ACT_PLAYER_LIST, 0x00936f00);
            rx_addr name;
            ACT_CALLR(name, 1, rx_tbl_cell_string(column, row));
            const rx_addr text = U32(name, 0x18) < 0x10 ? name + 4 : U32(name, 4);
            if (crt_strcmp(text, U32(driver, 0x74) + 0x14) == 0) break;
            row++;
            ACT_CALLR(rows, 0, rx_tbl_row_count(ACT_PLAYER_LIST));
            if (!(row < rows)) { ACT_END(L); return; }
        }
        ACT_CALL(2, rx_player_list_request_activity(row, 8, 0));
        int32_t found;
        ACT_CALLR(found, 1, rx_activity_find_row(ACT_ACTIVITY_TBL, 8, 0));
        if (found == -1) ACT_CALL(4, (void)rx_activity_table_add_row(ACT_ACTIVITY_TBL, 8, 0, 2));
        ACT_END(L);
        return;
    }
    const int32_t count = I32(act, 0x26c);
    int32_t target = place;
    if (target >= count - 1) target = count - 1;   /* cmovge */
    int32_t at = 0;
    if (count > 0) {
        do {
            if (driver == rx_stand(act, (uint32_t)at)) break;
            at++;
        } while (at < I32(act, 0x26c));
    }
    const rx_addr displaced = rx_stand(act, (uint32_t)target);
    rx_stand_set(act, (uint32_t)target, driver);
    rx_stand_set(act, (uint32_t)at, displaced);
    const uint32_t place_a = U32(displaced, 0x78);
    const uint32_t place_b = U32(driver, 0x78);
    SU32(displaced, 0x78, place_b);
    SU32(driver, 0x78, place_a);
    SU32(act, 0x300, U32(act, 0x26c));
    ACT_CALL(2, rx_activity_set_state(act, 3));
    ACT_END(L);
}
RX_FUNCTION(0x005693d0, rx_champ_place_swap)
