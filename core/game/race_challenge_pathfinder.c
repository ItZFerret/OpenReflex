/* race_challenge_pathfinder.c -- the two challenge classes no shipped level stages: MXActivityPathfinderChallenge (0x18, vtable
 * 0x009453fc) and MXActivityStepUpChallenge (0x19, vtable 0x00945514). The factory builds them for EventTypeActivation "Pathfinder" /
 * "StepUp", but no Free Ride level has a "<level>ChallengePathfinder<A..>" / "<level>ChallengeStepUp" subscene (checked: FR_Alpine,
 * FR_Gibson, FR_Cheyenne), so the game never reaches them with its own data. Hand transliterations of the disassembly.
 *
 * Pathfinder (two riders: the leader and the follower): the waypoint gates of the activity's group (type 0x1043, sorted by 005784d0)
 * at +0xff3b8 (a vector: data, size +0xff3bc, capacity +0xff3c0, grow +0xff3c4, the limit +0xff3c8), their number +0xff3b4; the gap
 * between the two riders +0xff3d0 and its largest value +0xff3cc (the value of vf64, lower is better); the AI line .tdf07 of kind 0xe
 * ("pf") in the activity's track block +0x890; a PathfinderDriver (vtable 0x009400f4) per player.
 * StepUp (a ladder of bars): the bar entity (type 0x106f) +0xff3c4, the two upright entities (0x1070) +0xff3c8 / +0xff3cc, the three
 * start entities (0x1024 / 0x1025 / 0x1026: the levels' starts +0x340 / +0x370 / +0x3a0, vf61), the level +0xff3f0 (0 .. 2) with
 * its start +0xff3f4, the attempts +0xff3fc and the clears +0xff3f8 of the level, the medals won per level +0xff3b0 / +0xff3b4 /
 * +0xff3b8 (vf64), the bar height above the start +0xff3bc (the criteria), the jump state +0xff3e0 (0 waiting, 1 airborne, 2 landed
 * with the 3 s clock +0xff3e4), the bar-passed flag +0xff3e8, the bar's height +0xff3c0, the starts' loaded flag +0xff3ec. The limits
 * of the levels: attempts [0x009c522c] / [0x009c5234] / [0x009c523c], clears [0x009c5230] / [0x009c5238] / [0x009c5240]. */
#include "race_challenge_activity.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_finish.h"
#include "race.h"
#include "race_misc.h"
#include "build_activity_int.h"
#include "build_activity_pre.h"
#include "build_activity_freeride_run.h"
#include "build_activity_teardown.h"
#include "build_vehicle.h"
#include "build_series_init.h"
#include "career_activity.h"
#include "waypoint_activity.h"
#include "waypoint_driver.h"
#include "waypoint_in_progress.h"
#include "world_misc.h"
#include "world_board_handlers.h"
#include "phys_terrain.h"
#include "phys_crt.h"
#include "loop_player.h"
#include "anim.h"
#include "fx.h"
#include "crt.h"
#include "crt_atexit.h"
#include "race_champ.h"
#include "race_beh2.h"
#include <string.h>
#include "rider_cap.h"

#define VT_PATHFINDER 0x009453fcu
#define VT_STEP_UP    0x00945514u

static uint32_t fcopy(uint32_t bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0) bits |= 0x00400000u;
    return bits;
}
static float fcopy_f(float f) { return rx_bf(fcopy(rx_fb(f))); }
static void vcall0(rx_addr obj, uint32_t slot) { (void)act_vcall(obj, slot, 0, 0); }
/* a behaviour / rule bound to its driver (vf1) */
static void bind(rx_addr o, rx_addr d) { (void)act_vcall(o, 4, 1, &d); }

/* ======================================================================================================================== */
/* MXActivityPathfinderChallenge                                                                                             */

/* 0058a640 (EAX = the activity's channel; stdcall A; RET 4): its "Lap Completed" listener (proxy vtable 0x00945cc4, signature
 * 0x009cda64, the method 005de600). Frame: push ebx; push esi (push edi only around the name compare). */
void rx_pathfinder_listen(rx_addr channel, rx_addr A) {
    act_subscribe(0x0058a640, 1, 1, 2, channel, 0x0093eefc /* "Lap Completed" */, 0x00945cc4, 0x009cda64, A, 0x005de600);
}
RX_FUNCTION(0x0058a640, rx_pathfinder_listen)

/* 0058a700 (EAX = the activity's channel; stdcall A; RET 4): the listener of 0058a640 leaves (the aligned key frame) */
void rx_pathfinder_unlisten(rx_addr channel, rx_addr A) {
    act_unlisten(1, channel, 0x0093eefc /* "Lap Completed" */, 0x00945cc4, 0x009cda64, A, 0x005de600);
}
RX_FUNCTION(0x0058a700, rx_pathfinder_unlisten)

/* 0058a760 (ECX = driver) -> a BehaviorPathfinderRoamToWaypointGate (0x1c0 bytes from the driver's block, vtable 0x00940140) appended
 * to the behaviours (+0xe8, count +0xee0, at most 0x12; 0 when full) */
rx_addr rx_pathfinder_roam_behavior_get(rx_addr d) {
    if (U32(d, 0xee0) >= 0x12) return 0;
    const rx_addr o = U32(d, 0x3e4);
    SU32(d, 0x3e4, o + 0x1c0);
    rx_addr r = 0;
    if (o != 0) {
        SU32(o, 0, 0x00940140);
        r = o;
    }
    SU32(d, 0xe8 + 4 * U32(d, 0xee0), r);
    r = U32(d, 0xe8 + 4 * U32(d, 0xee0));
    SU32(d, 0xee0, U32(d, 0xee0) + 1);
    return r;
}
RX_FUNCTION(0x0058a760, rx_pathfinder_roam_behavior_get)

/* 0057b7f0 MXActivityPathfinderChallenge::sdtor: the gates' storage freed, the vtable, the body (0057aab0), freed with flag 1 */
rx_addr rx_pathfinder_sdtor(rx_addr A, uint32_t flags) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    const rx_addr storage = U32(A, 0xff3b8);
    SU32(A, 0, VT_PATHFINDER);
    ACT_CALL(1, rx_reflex_aligned_free(storage));
    ACT_CALL(0, rx_challenge_destroy(A));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(A));
    ACT_END(L);
    return A;
}
RX_FUNCTION(0x0057b7f0, rx_pathfinder_sdtor)

/* 0057b830 MXActivityPathfinderChallenge::vf5 (the activity area; ECX = A): Activity::vf5, the AI line of kind 0xe ("pf") for the area
 * into the track block +0x890 (00536ef0), "Activity Basic Essentials Loaded" (the activity) on the game's channel [0x00a93f74].
 * Frame: push ebp; mov ebp, esp; and esp, -8; push ecx (the payload); push ebx / esi / edi. */
void rx_pathfinder_vf5(rx_addr A) {
    ACT_FRAME_EBP(L, 8, 4, 3);
    ACT_CALL(0, rx_activity_vf5(A));
    ACT_CALL(3, (void)rx_track_load(U32(A, 0x234) + 0x20030, A + 0x890, 0xe, U32(A, 0x220)));
    SU32(L, 0xc, A);
    act_raise(U32(0, 0x00a93f74), 0x00936038 /* "Activity Basic Essentials Loaded" */, 0x009cbd24, L + 0xc, 4);
    ACT_END(L);
}
RX_FUNCTION(0x0057b830, rx_pathfinder_vf5)

/* 0057b8a0 MXActivityPathfinderChallenge::vf2 (ECX = A; arg; RET 4): MXActivityChallenge::vf2, vf61, the challenge track (0057b1a0),
 * the "Lap Completed" listener (0058a640), (a championship intro (+0x23c == 0x1f) asks the series manager its event type: not this
 * class), the gates of the group (007d26d0, type 0x1043) sorted by their index (qsort, 005784d0), their number +0xff3b4; vf67 (the
 * start from the line); +0x224 = 0; outside mode 0 "Activity Init" on the game's channel (the payload in the argument slot).
 * Frame: push esi; push edi. */
void rx_pathfinder_vf2(rx_addr A, rx_addr arg) {
    ACT_FRAME(L, 0, 2);
    ACT_CALL(1, rx_challenge_vf2(A, arg));
    vcall0(A, 0xf4);
    ACT_CALL(1, rx_challenge_load_track(A));
    ACT_CALL(1, rx_pathfinder_listen(U32(A, 0x290), A));
    if (U32(A, 0x23c) == 0x1f) {
        if ((U8(0, 0x009e275c) & 1) == 0) {
            SU32(0, 0x009e275c, U32(0, 0x009e275c) | 1);
            ACT_CALL(0, (void)rx_series_manager_construct());
            RX_ATEXIT(0x00911da0);
        }
        ACT_CALL(1, (void)rx_career_current_event_type(0x009e2188));
    }
    ACT_CALL(3, rx_waypoint_scene_collect_typed(A + 0xff3b8, U32(U32(A, 0x234), 0x20138), 0x1043, A + 0x118));
    SU32(A, 0xff3b4, U32(A, 0xff3bc));
    ACT_CALL(4, crt_qsort(U32(A, 0xff3b8), U32(A, 0xff3bc), 4, 0x005784d0));
    vcall0(A, 0x10c);
    SU32(A, 0x224, 0);
    if (U32(A, 0x238) != 0) {
        SU32(L, 0xc, A);
        act_raise(U32(0, 0x00a93f74), 0x00935ea4 /* "Activity Init" */, 0x009cbd24, L + 0xc, 4);
    }
    ACT_END(L);
}
RX_FUNCTION(0x0057b8a0, rx_pathfinder_vf2)

/* 0057b9a0 MXActivityPathfinderChallenge::vf4 (ECX = A): the event types of the activity's channel: "ChallengeAward", "Lap Completed",
 * the two fixed-name lap events, "Show Arrow", "Hide Arrow", "Waypoint Advanced". Frame: push esi. */
void rx_pathfinder_vf4(rx_addr A) {
    ACT_FRAME(L, 0, 1);
    act_event_type(0x00592940, U32(A, 0x290), 0);
    act_event_type(0x00592040, U32(A, 0x290), 0x0093eefc /* "Lap Completed" */);
    act_event_type(0x00592160, U32(A, 0x290), 0);
    act_event_type(0x00592280, U32(A, 0x290), 0);
    act_event_type(0x00590f60, U32(A, 0x290), 0x0093ef30 /* "Show Arrow" */);
    act_event_type(0x00590f60, U32(A, 0x290), 0x0093eef0 /* "Hide Arrow" */);
    act_event_type(0x00592040, U32(A, 0x290), 0x0093eff0 /* "Waypoint Advanced" */);
    ACT_END(L);
}
RX_FUNCTION(0x0057b9a0, rx_pathfinder_vf4)

/* 0057ba10 MXActivityPathfinderChallenge::vf67 (ECX = A): the start from the AI line (block +0x890: the first point +0x8a0, the
 * direction +0x8b0, the node +0x8c8 / +0x8cc): the starts +0x340 (the leader) and +0x370 (the follower, 15 m behind along the
 * direction), +0x880 = 2 */
void rx_pathfinder_vf67(rx_addr A) {
    SU32(A, 0x364, U32(A, 0x8cc));
    SU32(A, 0x360, U32(A, 0x8c8));
    SU32(A, 0x340, fcopy(U32(A, 0x8a0)));
    SU32(A, 0x344, fcopy(U32(A, 0x8a4)));
    SU32(A, 0x348, fcopy(U32(A, 0x8a8)));
    SU32(A, 0x350, fcopy(U32(A, 0x8b0)));
    SU32(A, 0x354, fcopy(U32(A, 0x8b4)));
    SU32(A, 0x358, fcopy(U32(A, 0x8b8)));
    SU32(A, 0x390, U32(A, 0x8c8));
    SU32(A, 0x36c, 0);
    SU32(A, 0x394, U32(A, 0x8cc));
    SU32(A, 0x370, fcopy(U32(A, 0x340)));
    SU32(A, 0x374, fcopy(U32(A, 0x344)));
    SU32(A, 0x378, fcopy(U32(A, 0x348)));
    SU32(A, 0x380, fcopy(U32(A, 0x350)));
    SU32(A, 0x384, fcopy(U32(A, 0x354)));
    SU32(A, 0x388, fcopy(U32(A, 0x358)));
    SU32(A, 0x39c, 0);
    const float back = -15.0f;   /* 0x9d78d8 */
    const float dx = F32(A, 0x350) * back;
    const float dy = F32(A, 0x354) * back;
    const float dz = F32(A, 0x358) * back;
    const float x = F32(A, 0x370) + dx;
    SF32(A, 0x374, F32(A, 0x374) + dy);
    SF32(A, 0x370, x);
    SF32(A, 0x378, F32(A, 0x378) + dz);
    SU32(A, 0x880, 2);
}
RX_FUNCTION(0x0057ba10, rx_pathfinder_vf67)

/* 0057bb60 MXActivityPathfinderChallenge::vf3 (ECX = A): every driver and every spectator leaves (0055c1d0), the gates emptied, the
 * AI line released (00538090, EAX = block +0x890), the "Lap Completed" listener leaves (0058a700), then MXActivityChallenge::vf3 */
void rx_pathfinder_vf3(rx_addr A) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    while (I32(A, 0x26c) > 0) ACT_CALL(2, rx_activity_on_player_left(A, U32(U32(A, 0x268), 0)));
    while (I32(A, 0x280) > 0) ACT_CALL(2, rx_activity_on_player_left(A, U32(U32(A, 0x27c), 0)));
    SU32(A, 0xff3bc, 0);
    ACT_CALL(0, rx_track_block_reset(A + 0x890));
    ACT_CALL(1, rx_pathfinder_unlisten(U32(A, 0x290), A));
    ACT_END(L);
    rx_challenge_vf3(A);
}
RX_FUNCTION(0x0057bb60, rx_pathfinder_vf3)

/* 0057bbe0 MXActivityPathfinderChallenge::vf18 (ECX = A): the largest gap 0, the medals cleared, Activity::vf18 */
void rx_pathfinder_vf18(rx_addr A) {
    SF32(A, 0xff3cc, 0.0f);
    SU32(A, 0xff074, 0);
    SU32(A, 0xff078, 0);
    SU32(A, 0xff07c, 0);
    rx_activity_vf18(A);
}
RX_FUNCTION(0x0057bbe0, rx_pathfinder_vf18)

/* 0057bc10 MXActivityPathfinderChallenge::vf16 (ECX = A): vf18, then MXActivityChallenge::vf16 */
void rx_pathfinder_vf16(rx_addr A) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    vcall0(A, 0x48);
    ACT_END(L);
    rx_challenge_vf16(A);
}
RX_FUNCTION(0x0057bc10, rx_pathfinder_vf16)

/* 0057bc30 MXActivityPathfinderChallenge::vf23 (ECX = A; dt; RET 4): MXActivityChallenge::vf23, then vf65 */
void rx_pathfinder_vf23(rx_addr A, float dt) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    ACT_CALL(1, rx_challenge_vf23(A, fcopy_f(dt)));
    vcall0(A, 0x104);
    ACT_END(L);
}
RX_FUNCTION(0x0057bc30, rx_pathfinder_vf23)

/* 0057bc50 MXActivityPathfinderChallenge::vf47 (ECX = A; driver; RET 4): the driver's rules and behaviours, each bound to it: the
 * boundary rule, the free ride's rule in a level whose name holds "WORLD" (0078fcf0), avoid wreck, suck racing, air leveling, fix
 * forward, brake turn, the racing lines, the roam to the next waypoint gate (0058a760), avoid objects, avoid flip back. Frame: push esi;
 * push edi. */
void rx_pathfinder_vf47(rx_addr A, rx_addr d) {
    ACT_FRAME(L, 0, 2);
    rx_addr o;
    ACT_CALLR(o, 0, rx_challenge_rule_get(d));
    bind(o, d);
    rx_addr found;
    ACT_CALLR(found, 0, rx_str_find_nocase(U32(A, 0x234) + 0x20030, 0x0096dd28 /* "WORLD" */));
    if (found != 0) {
        ACT_CALLR(o, 0, rx_waypoint_get_free_ride_rule(d));
        bind(o, d);
    }
    ACT_CALLR(o, 0, rx_waypoint_get_avoid_wreck(d));
    bind(o, d);
    ACT_CALLR(o, 0, rx_waypoint_get_suck_racing(d));
    bind(o, d);
    ACT_CALLR(o, 0, rx_waypoint_get_air_leveling(d));
    bind(o, d);
    ACT_CALLR(o, 0, rx_waypoint_get_fix_forward(d));
    bind(o, d);
    ACT_CALLR(o, 0, rx_waypoint_get_brake_turn(d));
    bind(o, d);
    ACT_CALLR(o, 0, rx_waypoint_get_racing_lines(d));
    bind(o, d);
    ACT_CALLR(o, 0, rx_pathfinder_roam_behavior_get(d));
    bind(o, d);
    ACT_CALLR(o, 0, rx_waypoint_get_avoid_objects(d));
    bind(o, d);
    ACT_CALLR(o, 0, rx_waypoint_get_avoid_flip_back(d));
    bind(o, d);
    ACT_END(L);
}
RX_FUNCTION(0x0057bc50, rx_pathfinder_vf47)

/* the arrow events of the capture classes: an event (name, signature 0x009cda44) per driver with the driver as the payload in the
 * argument slot. Frame: push ebx; push esi (the call); push ebp; push edi (the loop). */
static void capture_arrows(rx_addr A, rx_addr name) {
    const rx_addr slot = rx_sp() + 4 + 8;   /* the entry ESP + 4, from the frame after push ebx / esi */
    if (I32(A, 0x26c) <= 0) return;
    ACT_FRAME(L2, 0, 2);   /* push ebp; push edi */
    for (int32_t i = 0; i < I32(A, 0x26c); i++) {
        SU32(slot, 0, U32(U32(A, 0x268), 4u * (uint32_t)i));
        act_raise(U32(A, 0x290), name, 0x009cda44, slot, 4);
    }
    ACT_END(L2);
}

/* 0057bd30 MXActivityCapture::vf37 (ECX = A; old state; RET 4): Activity::vf37, then "Hide Arrow" for each driver */
void rx_capture_vf37(rx_addr A, uint32_t state) {
    (void)state;
    ACT_FRAME(L, 0, 2);   /* push ebx; push esi */
    ACT_CALL(1, rx_activity_vf37(A));
    capture_arrows(A, 0x0093eef0 /* "Hide Arrow" */);
    ACT_END(L);
}
RX_FUNCTION(0x0057bd30, rx_capture_vf37)

/* 005775c0 MXActivityCapture::vf39 (ECX = A; old state; RET 4): Activity::vf39, then "Show Arrow" for each driver */
void rx_capture_vf39(rx_addr A, uint32_t state) {
    (void)state;
    ACT_FRAME(L, 0, 2);   /* push ebx; push esi */
    ACT_CALL(1, rx_activity_in_progress_enter(A));
    capture_arrows(A, 0x0093ef30 /* "Show Arrow" */);
    ACT_END(L);
}
RX_FUNCTION(0x005775c0, rx_capture_vf39)

/* 0057bda0 MXActivityPathfinderChallenge::vf38 (Countdown enter; ECX = A; old state; RET 4): the countdown 3.0 s of 3 steps,
 * Activity::vf38; the countdown clock +0x29c from it and its steps +0x2a0, +0x2cc = 0; in the modes 0, 2 and 3 the clock 0 */
void rx_pathfinder_vf38(rx_addr A, uint32_t state) {
    (void)state;
    ACT_FRAME(L, 0, 1);   /* push esi */
    SF32(A, 0x2b0, 3.0f);   /* 0x92fe48 */
    SU32(A, 0x2b8, 3);
    ACT_CALL(1, rx_activity_countdown_enter(A));
    SU32(A, 0x29c, fcopy(U32(A, 0x2b0)));
    SU32(A, 0x2a0, U32(A, 0x2b8));
    SU32(A, 0x2cc, 0);
    const uint32_t mode = U32(A, 0x238);
    if (mode == 0 || mode == 2 || mode == 3) SF32(A, 0x29c, 0.0f);
    ACT_END(L);
}
RX_FUNCTION(0x0057bda0, rx_pathfinder_vf38)

/* 0057be20 MXActivityPathfinderChallenge::vf25 (Pre update; ECX = A; dt; RET 4): Activity::vf25 unless +0x224 is set */
void rx_pathfinder_vf25(rx_addr A, float dt) {
    if (U32(A, 0x224) != 0) return;
    ACT_CALL(1, rx_activity_pre(A, fcopy_f(dt)));
}
RX_FUNCTION(0x0057be20, rx_pathfinder_vf25)

/* the first local player (+0x10 == want) of the game's player slots (players at [0xa946b8] + 0x7460, 0x12a0 bytes each; the slots
 * +0xdf8c, -1 terminated at the first; the count byte +0xdfbc), or 0 */
static rx_addr first_player_of(uint32_t want) {
    const rx_addr base = U32(0, 0x00a946b8) + 0x7460;
    if (I32(base, 0xdf8c) == -1) return 0;
    const uint32_t n = U8(base, 0xdfbc);
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr p = rx_seat_active(base, i);
        if (U32(p, 0x10) == want) return p;
    }
    return 0;
}

/* 0057be50 MXActivityPathfinderChallenge::vf28 (InProgress; ECX = A; dt; RET 4): Activity::vf28; the gap between the local rider
 * (+0x10 == 0) and the other one (+0x10 == 1): their vehicles' distance (+0xc0, SqrtF) into +0xff3d0, its largest value +0xff3cc;
 * while running (+0x21c == 2) in the modes 1 / 2 the AI line's debug traversal (005399e0) when it has one (+0xb8c).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x14; push ebx / esi / edi; the activity kept at +0x1c. */
void rx_pathfinder_vf28(rx_addr A, float dt) {
    ACT_FRAME_EBP(L, 16, 0x14, 3);
    SU32(L, 0x1c, A);
    ACT_CALL(1, rx_activity_in_progress(A, fcopy_f(dt)));
    SU32(L, 0x18, 0);
    const rx_addr leader = first_player_of(0);
    const rx_addr follower = first_player_of(1);
    const rx_addr v2 = U32(follower, 0x1ac);   /* (no second rider: the original reads address 0x1ac) */
    const rx_addr v1 = U32(leader, 0x1ac);
    const float dz = F32(v1, 0xc8) - F32(v2, 0xc8);
    const float dy = F32(v1, 0xc4) - F32(v2, 0xc4);
    const float dx = F32(v1, 0xc0) - F32(v2, 0xc0);
    float d2 = dz * dz + dy * dy;
    d2 = d2 + dx * dx;
    float gap;
    {
        const rx_addr sp = rx_sp();
        SF32(sp, -4, d2);
        rx_set_sp(sp - 8);
        gap = phys_sqrtf(d2);
        rx_set_sp(sp);
    }
    SF32(A, 0xff3d0, gap);
    const float best = F32(A, 0xff3cc);
    SF32(A, 0xff3cc, best > gap ? best : gap);
    if (U32(A, 0x21c) == 2) {
        const uint32_t mode = U32(A, 0x238);
        if ((mode == 1 || mode == 2) && U32(A, 0xb8c) != 0) ACT_CALL(0, rx_waypoint_track_debug(A + 0x890));
    }
    ACT_END(L);
}
RX_FUNCTION(0x0057be50, rx_pathfinder_vf28)

/* 0057c050 MXActivityPathfinderChallenge::vf41 (Post enter; ECX = A; old state; RET 4): MXActivityChallenge::vf41, then the records of
 * each local human finisher (+0x2d0, count +0x300; player +4 set, +0x10 clear) in its records slot (+0x1f0 of the records [0xa946c0]):
 * TotalEvents + 1, the placing, TotalLaps + the gate count (+0xff3b0), the total time (driver +0x134), the score (player +0xcec),
 * the best lap (driver +0x10b0). The records refs {row, 0, table} live on the frame; the finisher cursor in the argument slot.
 * Frame: sub esp, 0x28; push ebp / esi; (the loop) push ebx / edi. */
void rx_pathfinder_vf41(rx_addr A, uint32_t state) {
    const rx_addr entry = rx_sp();
    const rx_addr S = entry - 0x28;
    rx_set_sp(S - 8);   /* push ebp; push esi */
    ACT_CALL(1, rx_challenge_vf41(A, state));
    SU32(S, 0, 0);
    if (I32(A, 0x300) <= 0) { rx_set_sp(entry); return; }
    SU32(entry, 4, A + 0x2d0);   /* the cursor (the argument's slot) */
    const rx_addr L = S - 16;     /* push ebx; push edi */
    rx_set_sp(L);
    for (int32_t i = 0; i < I32(A, 0x300); i++, SU32(entry, 4, U32(entry, 4) + 4)) {
        SU32(L, 0x10, (uint32_t)i);
        const rx_addr d = U32(U32(entry, 4), 0);
        const rx_addr p = U32(d, 0x74);
        if (U32(p, 4) == 0 || U32(p, 0x10) != 0) continue;
        const uint32_t slot = U32(p, 0x1f0);
        if (slot != 0xffffffffu) {
            const rx_addr e = U32(0, 0x00a946c0) + 0x18 * slot;
            if (U32(e, 4) != 0xffffffffu) {
                SU32(L, 0x14, U32(e, 4)); SU32(L, 0x18, 0); SU32(L, 0x1c, U32(e, 0));
                ACT_CALL(3, rx_records_add_int(L + 0x14, 1, 0x0096d99c /* "TotalEvents" */));
            }
        }
        ACT_CALL(1, (void)rx_records_placing(U32(0, 0x00a946c0), slot, i + 1));
        if (slot != 0xffffffffu) {
            const rx_addr e = U32(0, 0x00a946c0) + 0x18 * slot;
            if (U32(e, 4) != 0xffffffffu) {
                SU32(L, 0x20, U32(e, 4)); SU32(L, 0x24, 0); SU32(L, 0x28, U32(e, 0));
                ACT_CALL(3, rx_records_add_int(L + 0x20, (int32_t)U32(A, 0xff3b0), 0x0096d990 /* "TotalLaps" */));
            }
            const rx_addr e2 = U32(0, 0x00a946c0) + 0x18 * slot;
            if (U32(e2, 4) != 0xffffffffu) {
                SU32(L, 0x2c, U32(e2, 4)); SU32(L, 0x30, 0); SU32(L, 0x34, U32(e2, 0));
                ACT_CALL(2, rx_records_add_total_time(L + 0x2c, rx_bf(fcopy(U32(d, 0x134)))));
            }
        }
        ACT_CALL(1, (void)rx_records_score(U32(0, 0x00a946c0), slot, rx_bf(fcopy(U32(U32(d, 0x74), 0xcec)))));
        ACT_CALL(1, (void)rx_records_best_lap(U32(0, 0x00a946c0), slot, rx_bf(fcopy(U32(d, 0x10b0)))));
        i = (int32_t)U32(L, 0x10);
    }
    rx_set_sp(entry);
}
RX_FUNCTION(0x0057c050, rx_pathfinder_vf41)

/* 0057c1d0 MXActivityPathfinderChallenge::vf44 (ECX = A; player; RET 4) -> a PathfinderDriver: 0xf30 bytes, NetworkDriver's constructor,
 * the vtable 0x009400f4, +0xee8 = 11, +0xf24 / +0xf28 = 0 */
rx_addr rx_pathfinder_vf44(rx_addr A, rx_addr player) {
    (void)A;
    ACT_FRAME(L, 0, 1);   /* push edi */
    rx_addr d;
    ACT_CALLR(d, 1, rx_reflex_aligned_alloc(0xf30));
    if (d == 0) { ACT_END(L); return 0; }
    ACT_CALL(1, (void)rx_network_driver_construct(d, player));
    SU32(d, 0, 0x009400f4);
    SU32(d, 0xee8, 0xb);
    SU32(d, 0xf24, 0);
    SU32(d, 0xf28, 0);
    ACT_END(L);
    return d;
}
RX_FUNCTION(0x0057c1d0, rx_pathfinder_vf44)

/* 0057c230 MXActivityPathfinderChallenge::vf20 (ECX = A; player; RET 4): a driver without a gate (+0x10d4 < 0) restarts as Activity's
 * (0055d120); else the vehicle placed at its gate: the gate's pose (vf15 +0x3c: the position, then the direction at +0x30), 00460020,
 * the fade (0054b780). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp,
 * 0x64; push ebx / esi / edi; the gate pose +0x30, the position +0x10, the direction +0x20. */
void rx_pathfinder_vf20(rx_addr A, rx_addr player) {
    ACT_FRAME_EBP(L, 16, 0x64, 3);
    int32_t index;
    ACT_CALLR(index, 0, rx_world_group_rider_index(A, player));
    const int32_t gate = I32(U32(U32(A, 0x268), 4u * (uint32_t)index), 0x10d4);
    if (gate < 0) {
        ACT_CALL(1, rx_activity_vf20(A, player));
        ACT_END(L);
        return;
    }
    {
        const uint32_t out = L + 0x30;
        (void)act_vcall(U32(U32(A, 0xff3b8), 4u * (uint32_t)gate), 0x3c, 1, &out);
    }
    SU32(L, 0x10, U32(L, 0x30)); SU32(L, 0x14, U32(L, 0x34)); SU32(L, 0x18, U32(L, 0x38));
    SU32(L, 0x20, U32(L, 0x60)); SU32(L, 0x24, U32(L, 0x64)); SU32(L, 0x28, U32(L, 0x68));
    ACT_CALL(2, rx_vehicle_effect_query(U32(player, 0x1ac), L + 0x20, L + 0x10));
    ACT_CALL(0, rx_player_set_fade_out(player));
    ACT_END(L);
}
RX_FUNCTION(0x0057c230, rx_pathfinder_vf20)

/* 0057c320 MXActivityPathfinderChallenge::vf64 (ECX = A; three medal flags; RET 0xc): each flag set (never cleared) when the largest
 * gap (+0xff3cc) stayed under the medal's criterion */
void rx_pathfinder_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2) {
    const rx_addr spec = U32(U32(A, 0x234), 0x22140);
    const uint32_t k = U32(A, 0xff070), area = U32(A, 0x220);
    const float c2 = spec ? F32(spec, 4 * (area + 12 * k) + 0xa8) : 10.0f;
    const float c1 = spec ? F32(spec, 4 * (area + 12 * k) + 0x9c) : 10.0f;
    const float c0 = spec ? F32(spec, 4 * (area + 12 * (k + 3))) : 10.0f;
    if (c2 > F32(A, 0xff3cc)) SU32(m2, 0, 1);
    if (c1 > F32(A, 0xff3cc)) SU32(m1, 0, 1);
    if (c0 > F32(A, 0xff3cc)) SU32(m0, 0, 1);
}
RX_FUNCTION(0x0057c320, rx_pathfinder_vf64)

/* ======================================================================================================================== */
/* MXActivityStepUpChallenge                                                                                                 */

/* 0055ecd0 MXActivityStepUpChallenge::sdtor */
rx_addr rx_step_up_sdtor(rx_addr A, uint32_t flags) {
    ACT_FRAME(L, 0, 1);   /* push esi */
    SU32(A, 0, VT_STEP_UP);
    ACT_CALL(0, rx_challenge_destroy(A));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(A));
    ACT_END(L);
    return A;
}
RX_FUNCTION(0x0055ecd0, rx_step_up_sdtor)

/* the attempts and clears of a level (the tables 0x009c522c / 0x009c5234 / 0x009c523c and 0x009c5230 / 0x009c5238 / 0x009c5240; 1 for
 * another level) */
static int32_t level_attempts(uint32_t level) {
    if (level == 0) return I32(0, 0x009c522c);
    if (level == 1) return I32(0, 0x009c5234);
    if (level == 2) return I32(0, 0x009c523c);
    return 1;
}
static int32_t level_clears(uint32_t level) {
    if (level == 0) return I32(0, 0x009c5230);
    if (level == 1) return I32(0, 0x009c5238);
    if (level == 2) return I32(0, 0x009c5240);
    return 1;
}

/* 0057c640 (EAX = A, ECX = level): the level: its counters cleared, the bar height from the criteria (index 3 * (level + 4 kind + 12)
 * + area; 10.0 without the spec entity), its start (+0x340 / +0x370 / +0x3a0) */
void rx_step_up_set_level(rx_addr A, uint32_t level) {
    SU32(A, 0xff3f0, level);
    SU32(A, 0xff3f8, 0);
    SU32(A, 0xff3fc, 0);
    const rx_addr spec = U32(U32(A, 0x234), 0x22140);
    const float h = spec ? F32(spec, 4 * (3 * (level + 4 * U32(A, 0xff070) + 12) + U32(A, 0x220))) : 10.0f;
    SF32(A, 0xff3bc, h);
    if (level == 0) SU32(A, 0xff3f4, A + 0x340);
    else if (level == 1) SU32(A, 0xff3f4, A + 0x370);
    else if (level == 2) SU32(A, 0xff3f4, A + 0x3a0);
}
RX_FUNCTION(0x0057c640, rx_step_up_set_level)

/* 0057cbb0 (EDI = A; stdcall restart; RET 4): the local rider (+0x10 == 0; else address 0 as the original reads it) placed at the
 * level's start (the simulation's placement query, 0045f600), the fade; the bar raised to the start's height + the bar height (its
 * vf17 position, +0x60 / +0x64 / +0x68); the bar-passed flag, the bar height and the jump state cleared; with `restart` the state
 * 1 (Countdown). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x48; push ebx / esi. Locals: the bar position +0x10, the
 * query's outputs +0x20 (axis a), +0x30 (position), +0x40 (axis b). */
void rx_step_up_place(rx_addr A, uint32_t restart) {
    ACT_FRAME_EBP(L, 16, 0x48, 2);
    SU32(L, 0xc, 0);
    const rx_addr player = first_player_of(0);
    const rx_addr vehicle = U32(player, 0x1ac);
    const rx_addr start = U32(A, 0xff3f4);
    {
        const uint32_t a[5] = {start, start + 0x10, L + 0x30, L + 0x20, L + 0x40};
        (void)act_vcall(U32(vehicle, 0x234), 0x50, 5, a);
    }
    ACT_CALL(3, rx_vehicle_place(vehicle, L + 0x30, L + 0x20, L + 0x40));
    ACT_CALL(0, rx_player_set_fade_out(player));
    {
        const uint32_t out = L + 0x10;
        (void)act_vcall(U32(A, 0xff3c4), 0x44, 1, &out);
    }
    SF32(L, 0x14, F32(A, 0xff3bc) + F32(U32(A, 0xff3f4), 4));
    const rx_addr bar = U32(A, 0xff3c4);
    SU32(bar, 0x60, U32(L, 0x10));
    SU32(bar, 0x64, U32(L, 0x14));
    SU32(bar, 0x68, U32(L, 0x18));
    SU32(A, 0xff3e8, 0);
    SF32(A, 0xff3c0, 0.0f);
    SU32(A, 0xff3e0, 0);
    if (restart != 0) ACT_CALL(2, rx_activity_set_state(A, 1));
    ACT_END(L);
}
RX_FUNCTION(0x0057cbb0, rx_step_up_place)

/* 0057c6d0 (EAX = A): an attempt failed: the attempts + 1; when the attempts left (limit - clears - attempts) are fewer than the clears
 * still needed (clears limit - clears) the challenge ends (state 3), else the rider is placed again (0057cbb0, restart 1).
 * Frame: push ebx / esi / edi. */
void rx_step_up_fail(rx_addr A) {
    ACT_FRAME(L, 0, 3);
    const uint32_t level = U32(A, 0xff3f0);
    SU32(A, 0xff3fc, U32(A, 0xff3fc) + 1);
    const int32_t attempts = I32(A, 0xff3fc);
    const int32_t clears = I32(A, 0xff3f8);
    const int32_t left = level_attempts(level) - clears - attempts;
    const int32_t needed = level_clears(level) - clears;
    if (needed > left) ACT_CALL(2, rx_activity_set_state(A, 3));
    else ACT_CALL(1, rx_step_up_place(A, 1));
    ACT_END(L);
}
RX_FUNCTION(0x0057c6d0, rx_step_up_fail)

/* 0057c760 (EAX = A): a clear: the clears + 1; at the level's clears limit its medal is won (+0xff3b0 / +0xff3b4 / +0xff3b8) and the
 * next level set up (0057c640; none after level 2); with the last medal the challenge ends (state 3), else the rider is placed again.
 * Frame: push ebx / esi / edi. */
void rx_step_up_clear(rx_addr A) {
    ACT_FRAME(L, 0, 3);
    const uint32_t level = U32(A, 0xff3f0);
    SU32(A, 0xff3f8, U32(A, 0xff3f8) + 1);
    if (I32(A, 0xff3f8) >= level_clears(level)) {
        if (level == 0) {
            SU32(A, 0xff3b0, 1);
            SU32(A, 0xff3f0, 1);
            rx_step_up_set_level(A, 1);
        } else if (level == 1) {
            SU32(A, 0xff3b4, 1);
            SU32(A, 0xff3f0, 2);
            rx_step_up_set_level(A, 2);
        } else if (level == 2) {
            SU32(A, 0xff3b8, 1);
        }
    }
    if (U32(A, 0xff3b8) != 0) ACT_CALL(2, rx_activity_set_state(A, 3));
    else ACT_CALL(1, rx_step_up_place(A, 1));
    ACT_END(L);
}
RX_FUNCTION(0x0057c760, rx_step_up_clear)

/* 0057cdf0 (ESI = A; stdcall player; RET 4) -> 1 when the player's vehicle is past the bar: the dot product of (vehicle - bar) with
 * (start - bar) in the ground plane (the height term weighted 0) is negative. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp,
 * 0x1c; push edi; the bar position +0x10. */
uint32_t rx_step_up_past_bar(rx_addr A, rx_addr player) {
    ACT_FRAME_EBP(L, 16, 0x1c, 1);
    {
        const uint32_t out = L + 0x10;
        (void)act_vcall(U32(A, 0xff3c4), 0x44, 1, &out);
    }
    const rx_addr v = U32(player, 0x1ac);
    const float bx = F32(L, 0x10), by = F32(L, 0x14), bz = F32(L, 0x18);
    const float dy = F32(v, 0xc4) - by;
    const float vx = F32(v, 0xc0) - bx;
    const float sx = F32(A, 0x340) - bx;
    const float vz = F32(v, 0xc8) - bz;
    const float sz = F32(A, 0x348) - bz;
    float dot = vz * sz;
    dot = dot + vx * sx;
    dot = dot + dy * 0.0f;
    ACT_END(L);
    return 0.0f > dot ? 1 : 0;
}
RX_FUNCTION(0x0057cdf0, rx_step_up_past_bar)

/* 0057ce90 (EDI = A, ESI = position) -> 1 when the position is between the two uprights (+0xff3c8 / +0xff3cc, their vf17 positions):
 * on the inner side of each in the ground plane. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x3c; push ebx; the uprights'
 * positions +0x10 / +0x20, a spill +0x38. */
uint32_t rx_step_up_between_uprights(rx_addr A, rx_addr pos) {
    ACT_FRAME_EBP(L, 16, 0x3c, 1);
    {
        const uint32_t out = L + 0x10;
        (void)act_vcall(U32(A, 0xff3c8), 0x44, 1, &out);
    }
    {
        const uint32_t out = L + 0x20;
        (void)act_vcall(U32(A, 0xff3cc), 0x44, 1, &out);
    }
    const float az = F32(L, 0x28), ax = F32(L, 0x20);
    const float bx = F32(L, 0x10), bz = F32(L, 0x18);
    const float px = F32(pos, 0), pz = F32(pos, 8);
    SF32(L, 0x38, bz - az);
    const float pax = px - ax;
    const float bax = bx - ax;
    SF32(L, 0x20, ax - bx);
    const float pbz = pz - bz;
    const float paz = pz - az;
    const float abz = az - bz;
    const float pbx = px - bx;
    float s = pbz * abz;
    s = s + pbx * F32(L, 0x20);
    uint32_t r = 0;
    if (s > 0.0f) {
        float t = paz * F32(L, 0x38);
        t = t + pax * bax;
        r = t > 0.0f ? 1 : 0;
    }
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x0057ce90, rx_step_up_between_uprights)

/* 0057cd30 MXActivityStepUpChallenge::vf2 (ECX = A; arg; RET 4): MXActivityChallenge::vf2; the bar (0x106f) and the two uprights
 * (0x1070) of the group in the current world's scene (007d2820); level 0: counters cleared, the bar height (criteria medal 0), its
 * start +0x340; the rider placed (0057cbb0, no restart). Frame: push ebp; mov ebp, esp; and esp, -16; push ecx; push ebx / esi / edi. */
void rx_step_up_vf2(rx_addr A, rx_addr arg) {
    ACT_FRAME_EBP(L, 16, 4, 3);
    ACT_CALL(1, rx_challenge_vf2(A, arg));
    rx_addr world;
    ACT_CALLR(world, 0, rx_game_current_world());
    const rx_addr scene = U32(world, 0x20138);
    ACT_CALL(6, (void)rx_scene_group_query(scene, 0x106f, A + 0xff3c4, 1, A + 0x118, 0));
    ACT_CALL(6, (void)rx_scene_group_query(scene, 0x1070, A + 0xff3c8, 2, A + 0x118, 0));
    SU32(A, 0xff3f0, 0);
    SU32(A, 0xff3f8, 0);
    SU32(A, 0xff3fc, 0);
    const rx_addr spec = U32(U32(A, 0x234), 0x22140);
    SF32(A, 0xff3bc, spec ? F32(spec, 4 * (U32(A, 0x220) + 12 * (U32(A, 0xff070) + 3))) : 10.0f);
    SU32(A, 0xff3f4, A + 0x340);
    ACT_CALL(1, rx_step_up_place(A, 0));
    ACT_END(L);
}
RX_FUNCTION(0x0057cd30, rx_step_up_vf2)

/* 0057cb20 MXActivityStepUpChallenge::vf18 (ECX = A): the bar height 0, the medals cleared, Activity::vf18; level 0 again (counters,
 * the bar height of medal 0, the start +0x340) and the rider placed (no restart). Frame: push esi; push edi. */
void rx_step_up_vf18(rx_addr A) {
    ACT_FRAME(L, 0, 2);
    SF32(A, 0xff3c0, 0.0f);
    SU32(A, 0xff074, 0);
    SU32(A, 0xff078, 0);
    SU32(A, 0xff07c, 0);
    ACT_CALL(0, rx_activity_vf18(A));
    SU32(A, 0xff3f0, 0);
    SU32(A, 0xff3f8, 0);
    SU32(A, 0xff3fc, 0);
    const rx_addr spec = U32(U32(A, 0x234), 0x22140);
    SF32(A, 0xff3bc, spec ? F32(spec, 4 * (U32(A, 0x220) + 12 * (U32(A, 0xff070) + 3))) : 10.0f);
    SU32(A, 0xff3f4, A + 0x340);
    ACT_CALL(1, rx_step_up_place(A, 0));
    ACT_END(L);
}
RX_FUNCTION(0x0057cb20, rx_step_up_vf18)

/* 0057c810 MXActivityStepUpChallenge::vf28 (InProgress; ECX = A; dt; RET 4): Activity::vf28; the human driver's (0055a000 (A, 0, 0))
 * player: a vehicle resetting (+0x280) fails the attempt; until the bar is passed (+0xff3e8) passing it (0057cdf0) marks it and checks
 * the bar's height (its vf17) against the vehicle (+0xc4): over the bar or between the uprights (0057ce90) goes on, else the attempt
 * fails; then outside Countdown (+0x21c != 3) the jump: state 0 airborne (vehicle vf43) -> 1; state 1 the height under the vehicle
 * (the terrain at its position, 007c5950) into +0xff3c0, landed (vf43 false) -> state 2 with the 3 s clock; state 2 the clock runs
 * out -> a clear (0057c760). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x14; push ebx / esi / edi; the player +0xc,
 * the positions +0x10. */
void rx_step_up_vf28(rx_addr A, float dt) {
    ACT_FRAME_EBP(L, 16, 0x14, 3);
    ACT_CALL(1, rx_activity_in_progress(A, fcopy_f(dt)));
    rx_addr d;
    ACT_CALLR(d, 2, rx_activity_driver_of_kind(A, 0, 0));
    rx_addr p = U32(d, 0x74);
    SU32(L, 0xc, p);
    if (U32(U32(p, 0x1ac), 0x280) != 0) {
        ACT_CALL(0, rx_step_up_fail(A));
        ACT_END(L);
        return;
    }
    if (U32(A, 0xff3e8) == 0) {
        uint32_t past;
        ACT_CALLR(past, 1, rx_step_up_past_bar(A, p));
        if (past != 0) {
            SU32(A, 0xff3e8, 1);
            {
                const uint32_t out = L + 0x10;
                (void)act_vcall(U32(A, 0xff3c4), 0x44, 1, &out);
            }
            const rx_addr v = U32(p, 0x1ac);
            if (F32(L, 0x14) >= F32(v, 0xc4)) {
                ACT_CALL(0, rx_step_up_fail(A));
            } else {
                uint32_t between;
                ACT_CALLR(between, 0, rx_step_up_between_uprights(A, v + 0xc0));
                if (between == 0) ACT_CALL(0, rx_step_up_fail(A));
                p = U32(L, 0xc);
            }
        }
    }
    if (U32(A, 0x21c) == 3) { ACT_END(L); return; }
    const uint32_t jump = U32(A, 0xff3e0);
    if (jump == 0) {
        if (act_vcall(U32(p, 0x1ac), 0xac, 0, 0) != 0) SU32(A, 0xff3e0, 1);
    } else if (jump == 1) {
        rx_addr reg;
        ACT_CALLR(reg, 0, rx_fx_terrain_registry());
        rx_addr terrain;
        ACT_CALLR(terrain, 0, rx_terrain_for_scene(reg, U32(U32(A, 0x234), 0x20138)));
        p = U32(L, 0xc);
        {
            const uint32_t out = L + 0x10;
            (void)act_vcall(U32(p, 0x1ac), 0x44, 1, &out);
        }
        uint32_t edx = L + 0x10;
        ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, L + 0x10, &edx));
        SU32(A, 0xff3c0, U32(L, 0x14));
        if (act_vcall(U32(p, 0x1ac), 0xac, 0, 0) == 0) {
            SF32(A, 0xff3e4, 3.0f);   /* 0x92fe48 */
            SU32(A, 0xff3e0, 2);
        }
    } else if (jump == 2) {
        const float t = F32(A, 0xff3e4) - dt;
        SF32(A, 0xff3e4, t);
        if (!(0.0f < t)) ACT_CALL(0, rx_step_up_clear(A));
    }
    ACT_END(L);
}
RX_FUNCTION(0x0057c810, rx_step_up_vf28)

/* 0057cd00 MXActivityStepUpChallenge::vf64 (ECX = A; three medal flags; RET 0xc): the medals of the levels: m2 = level 2's, m1 =
 * level 1's, m0 = level 0's */
void rx_step_up_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2) {
    SU32(m2, 0, U32(A, 0xff3b8));
    SU32(m1, 0, U32(A, 0xff3b4));
    SU32(m0, 0, U32(A, 0xff3b0));
}
RX_FUNCTION(0x0057cd00, rx_step_up_vf64)

/* 0057c9d0 MXActivityStepUpChallenge::vf65 (ECX = A): the debug text of the challenge (level, bar height, attempts / clears and their
 * limits, the jump clock, the medals): every print is the empty debug sink 007e3be0, so only the frame's security cookie is written.
 * Frame: push ebp; mov ebp, esp; and esp, -64; sub esp, 0x3c (the cookie at +0x38); push esi. */
void rx_step_up_vf65(rx_addr A) {
    (void)A;
    const rx_addr entry = rx_sp();
    const rx_addr S = ((entry - 4) & ~(rx_addr)63) - 0x3c;
    SU32(S, 0x38, U32(0, 0x009b45e4) ^ S);
}
RX_FUNCTION(0x0057c9d0, rx_step_up_vf65)

/* the start entity of a type in the activity's group (else the first entity of type 0x1014 of the scene): its matrix's position
 * (+0x30) snapped to the terrain and its direction (+0x20) into the start at A + out. `pos_first`: the original's locals for the
 * first start keep the position at +0x30 and the direction at +0x40, the other two the reverse. 0 when there is none. */
static int step_up_start(rx_addr A, rx_addr L, rx_addr scene, uint32_t type, uint32_t out, int pos_first) {
    if (pos_first) SU32(L, 0x18, 0);   /* (only the first query clears the list's entry) */
    uint32_t n;
    ACT_CALLR(n, 6, rx_scene_group_query(scene, type, L + 0x18, 1, A + 0x118, 0));
    if (n == 0) {
        ACT_CALLR(n, 4, rx_scene_manager_collect_typed(scene, 0x1014, L + 0x18, 0x10, L + 0x1c));
        if (n == 0) return 0;
    }
    const rx_addr e = U32(L, 0x18);
    const uint32_t pos_at = pos_first ? 0x30 : 0x40, dir_at = pos_first ? 0x40 : 0x30;
    rx_addr m = act_vcall(e, 0x38, 0, 0);
    SU32(L, pos_at, U32(m, 0x30)); SU32(L, pos_at + 4, U32(m, 0x34)); SU32(L, pos_at + 8, U32(m, 0x38));
    m = act_vcall(e, 0x38, 0, 0);
    SU32(L, dir_at, U32(m, 0x20)); SU32(L, dir_at + 4, U32(m, 0x24)); SU32(L, dir_at + 8, U32(m, 0x28));
    (void)act_vcall(e, 0x38, 0, 0);
    SU32(L, 0x20, U32(L, pos_at)); SU32(L, 0x24, U32(L, pos_at + 4)); SU32(L, 0x28, U32(L, pos_at + 8));
    if ((U8(0, 0x009e30e0) & 1) == 0) {
        SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
        SU32(0, 0x009e30dc, 0);
    }
    {
        const rx_addr sp = rx_sp();
        SU32(sp, -4, L + 0x20);
        rx_set_sp(sp - 4);
        rx_addr terrain;
        ACT_CALLR(terrain, 0, rx_terrain_for_scene(0x009e30d4, scene));
        uint32_t edx = L + 0x20;
        rx_set_sp(sp - 8);
        (void)phys_terrain_base_height_at(terrain, L + 0x20, &edx);
        rx_set_sp(sp);
    }
    SU32(A, out, U32(L, 0x20)); SU32(A, out + 4, U32(L, 0x24)); SU32(A, out + 8, U32(L, 0x28));
    SU32(A, out + 0x10, U32(L, dir_at)); SU32(A, out + 0x14, U32(L, dir_at + 4)); SU32(A, out + 0x18, U32(L, dir_at + 8));
    return 1;
}

/* 0057cf50 MXActivityStepUpChallenge::vf61 (ECX = A): once (+0xff3ec), the three levels' starts from the start entities 0x1024 / 0x1025
 * / 0x1026 of the group (+0x340 / +0x370 / +0x3a0: the position snapped to the terrain, the direction). Frame: push ebp; mov ebp, esp;
 * and esp, -16; sub esp, 0x44; push ebx / esi / edi. Locals: the entity list +0x18 (one entry), the overflow flag +0x1c, the snapped
 * position +0x20, the matrix copies +0x30 / +0x40. */
void rx_step_up_vf61(rx_addr A) {
    ACT_FRAME_EBP(L, 16, 0x44, 3);
    if (U32(A, 0xff3ec) != 0) { ACT_END(L); return; }
    const rx_addr scene = U32(U32(A, 0x234), 0x20138);
    (void)step_up_start(A, L, scene, 0x1024, 0x340, 1);
    (void)step_up_start(A, L, scene, 0x1025, 0x370, 0);
    (void)step_up_start(A, L, scene, 0x1026, 0x3a0, 0);
    SU32(A, 0xff3ec, 1);
    ACT_END(L);
}
RX_FUNCTION(0x0057cf50, rx_step_up_vf61)
