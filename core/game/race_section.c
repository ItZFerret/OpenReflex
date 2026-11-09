/* race_section.c -- MXActivitySectionRace (the Section Race, activity type 0xc; vtable 0x00944a74; 0x13e680 bytes), hand-transliterated
 * from the original (the address of each function is in its comment).
 *
 * The class derives from MXActivityRaceStub (rx_race_stub_construct 00565830 builds the base, the audio logic is a
 * SectionRaceActivityAudioLogic). The scene "<level>SectionRace[<track>]" holds SectionRaceZone entities (type 0x1042): each zone is a
 * trigger volume that raises "VehicleEnteredSection"; the zones whose StartTrigger flag (+0x280) is set are the sections that count.
 *
 * Fields added by the class (the base class ends at +0x13e650):
 *   +0x13e650  number of sections (the zones with StartTrigger set)
 *   +0x13e654  float[sections]  the best time of each section (FLT_MAX = none yet; the HUD's "best section" values)
 *   +0x13e658  driver*[sections] the driver that holds each best time (0 = none; the HUD's section leaders)
 *   +0x13e65c  the SectionRaceZone array {data +0x13e65c, count +0x13e660, capacity +0x13e664, growth +0x13e668}, +0x13e66c = INT_MAX
 *   +0x13e670  the network object (NetSectionRace) of an online session; 0 offline
 * The SectionRaceDriver (race_section_driver.c) adds +0x1134 the sections won, +0x1138 the section count, +0x113c the section it is
 * in (-1 none), +0x1140 its running time in each section, +0x1144 its best time in each section, +0x1168 its leader count.
 *
 * Online (a Quazal session: [0x00da6cb0] + 0x1c4c set and the mode +0x1b0 not 1): the NetSectionRace object link, the network
 * handlers 0056af70 / 0056af90 and the results of a client are the session's. A Section Race run offline never reaches them; each such
 * branch is marked "ONLINE" and stops with act_unreached (no network session exists in the product). */
#include "race_section.h"
#include "race_champ.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_activity_stub.h"
#include "race_activity_teardown.h"
#include "race_activity_ui.h"
#include "race_extern.h"
#include "race.h"
#include "waypoint_activity.h"
#include "waypoint_in_progress.h"
#include "anim_scenario_race_intro.h"
#include "world_wreck.h"
#include "../src/rx_os.h"
#include <string.h>
#include "rider_cap.h"

#define FLT_MAX_BITS 0x7f7fffffu   /* [0x00977494] */

/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static float x87_load_store(float v) {
    uint32_t b = rx_fb(v);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) b |= 0x400000u;
    return rx_bf(b);
}

/* the session's static initialiser (006de1f0 under bit 0 of 0x00dcf304) has run at startup */
static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}

/* a networked session: +0x1c4c set and the mode +0x1b0 not 1 */
static int session_networked(void) {
    const rx_addr s = U32(0, 0x00da6cb0);
    return U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1;
}

/* a call at the stack depth `base` (the ESP the original has before it pushes the arguments), restoring the frame afterwards */
#define SEC_CALL(F, base, nargs, call) do { rx_set_sp((base) - 4u * (nargs) - 4u); call; rx_set_sp(F); } while (0)

/* ======================================================================================================================== */
/* construction / destruction                                                                                                */

/* 00582e80 (ESI = logic): SectionRaceActivityAudioLogic (0xa4 bytes): ActivityAudioLogic -> RaceActivityAudioLogic (as 005825a0), the
 * last vtable 0x00942d9c. Frame: push ebx. */
rx_addr rx_section_audio_logic_construct(rx_addr logic) {
    ACT_FRAME(F, 0, 1);
    SU32(logic, 0, 0x009429fc);   /* ActivityAudioLogic::vftable */
    SU32(logic, 0x94, 0);
    SU8(logic, 4, 0);
    SU32(logic, 0x50, 0);
    SU32(logic, 0x54, 0);
    SU32(logic, 0x8c, 0);
    SU32(logic, 0x90, 0);
    ACT_CALL(0, rx_audio_logic_vf32(logic));
    SU32(logic, 0, 0x00942cfc);   /* RaceActivityAudioLogic::vftable */
    ACT_CALL(0, rx_audio_logic_vf32(logic));
    SU32(logic, 0x98, 0);
    SU32(logic, 0, 0x00942d9c);   /* SectionRaceActivityAudioLogic::vftable */
    ACT_END(F);
    return logic;
}
RX_FUNCTION(0x00582e80, rx_section_audio_logic_construct)

/* 0056a210 (EDI = the allocated activity; stdcall type, world; RET 8): the activity factory's case 0xc. The race stub (00565830) with
 * a new SectionRaceActivityAudioLogic, then the class's fields: the zone array (4 entries) and the section arrays empty.
 * Frame: push ecx / ebp / esi. */
rx_addr rx_section_race_construct(rx_addr act, uint32_t type, rx_addr world) {
    ACT_FRAME(F, 0, 3);
    rx_addr logic;
    ACT_CALLR(logic, 1, rx_reflex_aligned_alloc(0xa4));
    rx_addr audio = 0;
    if (logic != 0) ACT_CALLR(audio, 0, rx_section_audio_logic_construct(logic));
    ACT_CALL(2, rx_race_stub_construct(act, type, world, audio));
    SU32(act, 0, 0x00944a74);              /* MXActivitySectionRace::vftable */
    SU32(act, 0x13e65c, 0);
    SU32(act, 0x13e660, 0);
    SU32(act, 0x13e664, 0);
    SU32(act, 0x13e668, 0);
    SU32(act, 0x13e66c, 0);
    SU32(act, 0x2c8, 0);
    rx_addr zones;
    ACT_CALLR(zones, 1, rx_reflex_aligned_alloc(4 * 4));   /* 4 zone pointers (the allocation size is 4 * 4, no overflow) */
    SU32(act, 0x13e664, 4);
    SU32(act, 0x13e668, 4);
    SU32(act, 0x13e65c, zones);
    SU32(act, 0x13e660, 0);
    SU32(act, 0x13e66c, 0x7fffffff);
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x0056a210, rx_section_race_construct)

/* 0056a2c0 slot 0 (thiscall; flags; RET 4) -> this: the zone array freed, the race stub's destructor (005659e0), freed when flags & 1.
 * Frame: push esi. */
rx_addr rx_section_race_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    SU32(act, 0, 0x00944a74);
    ACT_CALL(1, rx_reflex_aligned_free(U32(act, 0x13e65c)));
    ACT_CALL(0, rx_race_stub_destroy(act));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(act));
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x0056a2c0, rx_section_race_sdtor)

/* ======================================================================================================================== */
/* the session's network listeners (templates 00588b80 / 00588c40 and their mirrors 00588d00 / 00588d60)                       */

/* 00588b80 (EAX = the session channel [0xda6ca0]; stdcall activity; RET 4): the activity listens to "NetworkObjectRemoved" with
 * 0056af70 (proxy 0x00945a44: void(NetObjectType, DuplicatedObject*)). Frame: push ebx / esi. */
void rx_section_race_net_removed_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00588b80, 1, 1, 2, channel, 0x0093ee98 /* "NetworkObjectRemoved" */, 0x00945a44, 0x009cdc7c, act, 0x0056af70));
}
RX_FUNCTION(0x00588b80, rx_section_race_net_removed_listen)

/* 00588c40 (EAX = the session channel; stdcall activity; RET 4): "NetworkSectionRaceActivity" with 0056af90 (proxy 0x00945a64:
 * void(DOHandle, int, float)) */
void rx_section_race_net_activity_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00588c40, 1, 1, 2, channel, 0x009420fc /* "NetworkSectionRaceActivity" */, 0x00945a64, 0x009cffcc, act, 0x0056af90));
}
RX_FUNCTION(0x00588c40, rx_section_race_net_activity_listen)

/* 00588d00 (EAX = the session channel; stdcall activity; RET 4): the listener of 00588b80 leaves.
 * Frame: push ebp; and esp, -8; sub esp, 0x14; push esi. */
void rx_section_race_net_removed_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x0093ee98, 0x00945a44, 0x009cdc7c, act, 0x0056af70);
}
RX_FUNCTION(0x00588d00, rx_section_race_net_removed_unlisten)

/* 00588d60 (EAX = the session channel; stdcall activity; RET 4): the listener of 00588c40 leaves */
void rx_section_race_net_activity_unlisten(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x009420fc, 0x00945a64, 0x009cffcc, act, 0x0056af90);
}
RX_FUNCTION(0x00588d60, rx_section_race_net_activity_unlisten)

/* 0056af70 (thiscall activity; kind, object; RET 8): the network handler of "NetworkObjectRemoved": a removed NetSectionRace object
 * (kind 6) unlinks from the activity. Reached only through the proxy of 00588b80 (a networked session raises the event). */
void rx_section_net_object_removed(rx_addr act, uint32_t kind, rx_addr object) {
    (void)object;
    if (kind == 6) SU32(act, 0x13e670, 0);
}
RX_FUNCTION(0x0056af70, rx_section_net_object_removed)

/* ======================================================================================================================== */
/* the activity's virtuals                                                                                                   */

/* 0056a300 slot 2 (thiscall; arg; RET 4): the race stub's start, the two network listeners, the session's NetSectionRace link
 * (ONLINE), the zones of the scene (type 0x1042, group = the activity's subscene name at +0x118) whose StartTrigger flag is set
 * counted as the sections, the best-time arrays (FLT_MAX / no leader), the start grid (005666d0) and one lap.
 * Frame: push ebx / edi (esi is pushed at 0056a395 and popped at 0056a4cf). */
void rx_section_race_vf2(rx_addr act, rx_addr arg) {
    ACT_FRAME(F, 0, 2);
    const rx_addr F3 = F - 4;   /* the push esi */
    ACT_CALL(1, rx_race_stub_vf2(act, arg));
    session_static();
    ACT_CALL(1, rx_section_race_net_removed_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_section_race_net_activity_listen(U32(0, 0x00da6ca0), act));
    session_static();
    rx_set_sp(F3);
    if (session_networked()) act_unreached(0x0056a3a8, "ONLINE: the networked section race (NetSectionRace link)");
    const rx_addr world = U32(act, 0x234);
    SU32(act, 0x13e650, 0);
    SEC_CALL(F3, F3, 3, rx_waypoint_scene_collect_typed(act + 0x13e65c, U32(world, 0x20138), 0x1042, act + 0x118));
    const int32_t zones = I32(act, 0x13e660);
    for (int32_t i = 0; i < zones; i++) {
        const rx_addr zone = U32(U32(act, 0x13e65c), 4u * (uint32_t)i);
        if (U32(zone, 0x280) != 0) SU32(act, 0x13e650, U32(act, 0x13e650) + 1);
    }
    /* the two arrays of 4 bytes per section (the allocation size is -1 on overflow, as the compiler's new[]) */
    uint64_t bytes = (uint64_t)U32(act, 0x13e650) * 4;
    rx_addr times, leaders;
    ACT_CALLR(times, 1, rx_reflex_aligned_alloc(bytes >> 32 ? 0xffffffffu : (uint32_t)bytes));
    SU32(act, 0x13e654, times);
    bytes = (uint64_t)U32(act, 0x13e650) * 4;
    ACT_CALLR(leaders, 1, rx_reflex_aligned_alloc(bytes >> 32 ? 0xffffffffu : (uint32_t)bytes));
    SU32(act, 0x13e658, leaders);
    rx_set_sp(F);   /* pop esi */
    for (int32_t i = 0; i < I32(act, 0x13e650); i++) {
        SU32(U32(act, 0x13e654), 4u * (uint32_t)i, FLT_MAX_BITS);
        SU32(U32(act, 0x13e658), 4u * (uint32_t)i, 0);
    }
    ACT_CALL(1, rx_race_grid_from_gate(act));
    SU32(act, 0xff070, 1);
    ACT_END(F);
}
RX_FUNCTION(0x0056a300, rx_section_race_vf2)

/* 0056a520 slot 3 (thiscall): the teardown. The network link (ONLINE) is skipped offline; the two network listeners leave, every
 * driver of the two lists leaves the activity, the section arrays are freed and the zone count cleared; a jump to
 * MXActivityRaceStub::vf3. Frame: sub esp, 0x24; push ebx / edi. */
void rx_section_race_vf3(rx_addr act) {
    ACT_FRAME(F, 0x24, 2);
    session_static();
    if (session_networked()) act_unreached(0x0056a561, "ONLINE: the networked section race teardown (the selection iterator loop)");
    session_static();
    ACT_CALL(1, rx_section_race_net_removed_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_section_race_net_activity_unlisten(U32(0, 0x00da6ca0), act));
    SU32(act, 0x13e670, 0);
    while (I32(act, 0x26c) > 0) {
        const rx_addr driver = U32(U32(act, 0x268), 0);
        ACT_CALL(2, rx_activity_on_player_left(act, driver));
    }
    while (I32(act, 0x280) > 0) {
        const rx_addr driver = U32(U32(act, 0x27c), 0);
        ACT_CALL(2, rx_activity_on_player_left(act, driver));
    }
    if (U32(act, 0x13e654) != 0) {
        ACT_CALL(1, rx_reflex_aligned_free(U32(act, 0x13e654)));
        SU32(act, 0x13e654, 0);
    }
    if (U32(act, 0x13e658) != 0) {
        ACT_CALL(1, rx_reflex_aligned_free(U32(act, 0x13e658)));
        SU32(act, 0x13e658, 0);
    }
    SU32(act, 0x13e660, 0);
    ACT_END(F);
    rx_race_stub_vf3(act);   /* mov ecx, edi; pop edi; pop ebx; add esp, 0x24; jmp 00566c80 */
}
RX_FUNCTION(0x0056a520, rx_section_race_vf3)

/* 0056a680 slot 4 (ECX = activity): the race's event types, plus "SectionWinner" and "SectionLost" (FUN_005924c0: void(SectionRaceDriver*,
 * int), the type 0x9cdad8) on the activity's channel. Frame: push esi. */
void rx_section_race_vf4(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_race_stub_vf4(act));
    act_event_type(0x005924c0, U32(act, 0x290), 0x0093ef5c /* "SectionWinner" */);
    act_event_type(0x005924c0, U32(act, 0x290), 0x0093ef6c /* "SectionLost" */);
    ACT_END(F);
}
RX_FUNCTION(0x0056a680, rx_section_race_vf4)




/* 00571490 slot 16 (ECX = activity; MXActivityPokerRace::vf16, shared with the section race): the class's vf18 (the restart), then a
 * jump to Activity::vf16. Frame: push esi. */
void rx_poker_race_vf16(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    act_vcall(act, 0x48, 0, 0);
    ACT_END(F);
    rx_activity_vf16(act);
}
RX_FUNCTION(0x00571490, rx_poker_race_vf16)

/* 0056a6b0 slot 18 (ECX = activity): every driver of the list resets (vf3), the section arrays back to FLT_MAX / no leader, then a
 * jump to MXActivityRaceStub::vf18. Frame: push esi / edi. */
void rx_section_race_vf18(rx_addr act) {
    ACT_FRAME(F, 0, 2);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        act_vcall(U32(U32(act, 0x268), 4u * (uint32_t)i), 0xc, 0, 0);
    }
    for (int32_t i = 0; i < I32(act, 0x13e650); i++) {
        SU32(U32(act, 0x13e654), 4u * (uint32_t)i, FLT_MAX_BITS);
        SU32(U32(act, 0x13e658), 4u * (uint32_t)i, 0);
    }
    ACT_END(F);
    rx_race_stub_vf18(act);
}
RX_FUNCTION(0x0056a6b0, rx_section_race_vf18)

/* 0056a750 slot 23 (thiscall; dt; RET 4): the race stub's update, then under the engine's profiler scope the link to the session's
 * NetSectionRace object while there is none (ONLINE only: nothing offline). Frame: push ebp; and esp, -8; sub esp, 0x24; push ebx /
 * esi / edi; the dt is loaded and stored through x87 (a signalling NaN is quieted). */
void rx_section_race_vf23(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 0x24, 3);
    ACT_CALL(1, rx_race_stub_update(act, x87_load_store(dt)));
    session_static();
    const rx_addr scope = race_profile_begin();
    if (U32(act, 0x13e670) == 0) {
        session_static();
        if (session_networked()) act_unreached(0x0056a826, "ONLINE: the networked section race's NetSectionRace link");
    }
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x0056a750, rx_section_race_vf23)

/* 0056acf0 (EDX = activity): the sections' best times recomputed from the drivers' own bests: for each driver, +0x1134 = 0, then
 * for each section the driver's best time (FLT_MAX out of range) that is at least as good as the section's best and is a real time
 * (not FLT_MAX) becomes the section's best, the driver its leader and +0x1134 counts it. Frame: push edi / ebx / ebp / esi (the
 * pushes of the loop; no calls). */
void rx_section_race_leaders(rx_addr act) {
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        SU32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x1134, 0);
        for (int32_t s = 0; s < I32(act, 0x13e650); s++) {
            const rx_addr driver = U32(U32(act, 0x268), 4u * (uint32_t)i);
            const int in_range = s >= 0 && s < I32(driver, 0x1138);
            const float driver_best = in_range ? F32(U32(driver, 0x1144), 4u * (uint32_t)s) : rx_bf(FLT_MAX_BITS);
            const rx_addr section_best = U32(act, 0x13e654) + 4u * (uint32_t)s;
            if (!(F32(section_best, 0) >= driver_best)) continue;                    /* comiss; jb */
            if (!in_range) continue;
            if (F32(U32(driver, 0x1144), 4u * (uint32_t)s) == rx_bf(FLT_MAX_BITS)) continue;   /* ucomiss; jnp */
            SF32(section_best, 0, F32(U32(driver, 0x1144), 4u * (uint32_t)s));
            SU32(U32(act, 0x13e658), 4u * (uint32_t)s, U32(U32(act, 0x268), 4u * (uint32_t)i));
            const rx_addr leader = U32(U32(act, 0x268), 4u * (uint32_t)i);
            SU32(leader, 0x1134, U32(leader, 0x1134) + 1);
        }
    }
}
RX_FUNCTION(0x0056acf0, rx_section_race_leaders)

/* 0056ac70 slot 28 (thiscall; dt; RET 4): racing (InProgress). The track debug view while the track block has one (+0xb8c), the lap
 * flags (00567e30, EDI = activity), the lapping warnings (00567f30), the section leaders (0056acf0), then Activity::vf28.
 * Frame: push ebp; and esp, -8; push ecx / edi. */
void rx_section_race_vf28(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 0, 2);
    if (U32(act, 0xb8c) != 0) ACT_CALL(0, rx_waypoint_track_debug(act + 0x890));
    ACT_CALL(0, rx_race_lap_flags(act));
    ACT_CALL(1, rx_race_lapping_warnings(act));
    ACT_CALL(0, rx_section_race_leaders(act));
    ACT_CALL(1, rx_activity_in_progress(act, x87_load_store(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x0056ac70, rx_section_race_vf28)

/* 0056a730 slot 38 (thiscall; old state; RET 4): Activity::vf38 (the countdown starts), then +0x2cc = 0. Frame: push esi. */
void rx_section_race_vf38(rx_addr act, uint32_t old_state) {
    (void)old_state;
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_activity_countdown_enter(act));
    SU32(act, 0x2cc, 0);
    ACT_END(F);
}
RX_FUNCTION(0x0056a730, rx_section_race_vf38)

/* 0056a930 slot 42 (ECX = activity): the end of the time limit (the activity clock +0x294 reaches the limit +0x2bc; online a client only
 * with a host flag +0xbc0): every ranked driver's message raised as "Player Wins" (void(Activity*, BaseDriver*, const char*, int),
 * payload {the player's activity, the driver, the text, 0}) on the activity's channel; the text is UI-only (the "Player Wins" subscribers
 * are HUD components and audio cues; no gameplay code reads it: the oracle runs the original localization chain so a Verify sees the same
 * memory, the product leaves an empty string, as race_activity_stub.c's lap_message); then a recording clip candidate (type 9) while
 * the clip recorder runs; then state 3 (WindDown). The message offline: the localized "STR_YOU_PLACED" and the driver's ordinal
 * (005dc720) formatted "%s %s".
 * Frame: push ebp; and esp, -8; sub esp, 0xa34; push ebx / esi / edi; the activity at F + 0x10, the loop index F + 0x14, the ASCII ordinal
 * F + 0x18 (8), the payload F + 0x20, the text F + 0x30 (0x104), the UTF-8 name F + 0x138 (0x100), the wide ordinal F + 0x238. */
void rx_section_race_vf42(rx_addr act) {
    ACT_FRAME_EBP(F, 8, 0xa34, 3);
    SU32(F, 0x10, act);
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0 && U32(act, 0xbc0) == 0) { ACT_END(F); return; }
        act_unreached(0x0056aa29, "ONLINE: the networked section race's results");
    }
    if (!(F32(act, 0x294) >= F32(act, 0x2bc))) { ACT_END(F); return; }   /* comiss; jb: not reached (or NaN) */
    const rx_addr text = F + 0x30, payload = F + 0x20;
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        SU32(F, 0x14, (uint32_t)i);
        session_static();
        SU8(text, 0, 0);
#ifdef RX_ORACLE
        {   /* the localized message (UI only): the original's calls in the original's order */
            const rx_addr utf8 = F + 0x138, ascii = F + 0x18, wide = F + 0x238;
            uint32_t zero = 0;
            const uint32_t lang = rx_call_original(0x005dfc90, RX_CDECL, 0, 1, &zero);                       /* the language handle */
            rx_addr entry;
            ACT_CALLR(entry, 0, rx_ui_language_lookup(0x00942078 /* "STR_YOU_PLACED" */, lang));            /* 005e0270 */
            { uint32_t a[8] = {0xfde9, 0, U32(entry, 0x10), 0xffffffffu, utf8, 0x100, 0, 0}; rx_os_import(0x009160a8, 8, a); }
            const uint32_t n = U32(F, 0x14) + 1;
            SU8(F, 0x237, 0);
            { uint32_t a[3] = {wide, 8, n}; rx_call_original(0x005dc720, RX_CDECL, 0, 3, a); }               /* the wide ordinal text */
            { uint32_t a[8] = {0xfde9, 0, wide, 0xffffffffu, ascii, 8, 0, 0}; rx_os_import(0x009160a8, 8, a); }
            SU8(F, 0x1f, 0);
            rx_addr p = F + 0x2f;                                    /* the suffix is appended where the first NUL at or after F + 0x30 is */
            do { p++; } while (U8(p, 0) != 0);
            SU32(p, 0, U32(0, n == 1 ? 0x00942118 : n == 2 ? 0x0094211c : n == 3 ? 0x00942120 : 0x00942124));
            SU32(F, -8, utf8);
            SU32(F, -4, ascii);
            rx_set_sp(F - 12);
            rx_format_buffer(text, 0x104, 0x00941fd0 /* "%s %s" */, F - 8);
            rx_set_sp(F);
        }
#endif
        const rx_addr manager = U32(act, 0x290);
        const rx_addr driver = rx_stand(act, (uint32_t)i);
        SU32(payload, 0, U32(U32(driver, 0x74), 0x94));
        SU32(payload, 4, driver);
        SU32(payload, 8, text);
        SU32(payload, 0xc, 0);
        act_raise(manager, 0x00941ca8 /* "Player Wins" */, 0x009cf630, payload, 0x10);
        if (U32(0, 0x00d5a9a4) != 0) {   /* the clip recorder runs: a clip candidate around the recorded activity's clock */
            const float t = F32(U32(0, 0x00a942b8), 0x294);
            ACT_CALL(2, rx_world_clip_recorder_candidate(9, U32(U32(act, 0x268), 0), rx_subs(t, F32(0, 0x009c40f0)), rx_adds(t, F32(0, 0x009c40f4))));
        }
    }
    ACT_CALL(2, rx_activity_set_state(act, 3));
    ACT_END(F);
}
RX_FUNCTION(0x0056a930, rx_section_race_vf42)

/* 0056ae40 (EAX = driver a; stdcall driver b; RET 4) -> EAX: the order of two drivers with the same number of won sections. Each
 * driver's best section time and the section it is in (005133f0, both calls on driver a: the original asks driver a twice): when the
 * two sections agree (or one is none) the lower time first; then the driver with more leader marks (+0x1168) first.
 * Frame: sub esp, 0x10; push esi (the four locals just below the entry ESP). */
int32_t rx_section_compare_best(rx_addr a, rx_addr b) {
    const rx_addr E = rx_sp();
    ACT_FRAME(F, 0x10, 1);
    ACT_CALL(2, rx_section_driver_best(a, E - 0xc, E - 4));
    ACT_CALL(2, rx_section_driver_best(a, E - 0x10, E - 8));
    const int32_t index_first = I32(E, -0xc), index_second = I32(E, -0x10);
    if (index_first == index_second || index_second == -1 || index_first == -1) {
        const float value_first = F32(E, -4), value_second = F32(E, -8);
        if (value_first > value_second) { ACT_END(F); return -1; }                          /* comiss; jbe */
        if (value_second > value_first) { ACT_END(F); return 1; }
    }
    ACT_END(F);
    const int32_t mine = I32(a, 0x1168), theirs = I32(b, 0x1168);
    if (mine > theirs) return -1;
    return theirs > mine ? 1 : 0;
}
RX_FUNCTION(0x0056ae40, rx_section_compare_best)

/* 0056adf0 slot 43 (arguments driver a, driver b, tie; RET 0xc; the activity is not used) -> EAX: the standings order: more
 * sections won (+0x1134) first (-1 when b has fewer than a... a ahead), equal: 0, or with `tie` the best-time order (0056ae40). */
int32_t rx_section_race_compare(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    (void)act;
    ACT_FRAME(F, 0, 1);
    int32_t result;
    if (I32(b, 0x1134) < I32(a, 0x1134)) result = -1;
    else if (I32(a, 0x1134) < I32(b, 0x1134)) result = 1;
    else if (tie == 0) result = 0;
    else ACT_CALLR(result, 1, rx_section_compare_best(a, b));
    ACT_END(F);
    return result;
}
RX_FUNCTION(0x0056adf0, rx_section_race_compare)

/* 0056acc0 slot 44 (thiscall; player; RET 4) -> a new SectionRaceDriver (0x1170 bytes), 0 when the allocation fails. Frame: push esi. */
rx_addr rx_section_race_vf44(rx_addr act, rx_addr player) {
    (void)act;
    ACT_FRAME(F, 0, 1);
    rx_addr driver;
    ACT_CALLR(driver, 1, rx_reflex_aligned_alloc(0x1170));
    if (driver != 0) ACT_CALLR(driver, 0, rx_section_driver_construct(driver, player));
    ACT_END(F);
    return driver;
}
RX_FUNCTION(0x0056acc0, rx_section_race_vf44)


/* 0056a720 slot 47 (thiscall; driver; RET 4): a jump to MXActivityRaceStub::vf47 (the driver's rules and behaviours). */
void rx_section_race_vf47(rx_addr act, rx_addr driver) {
    rx_race_stub_vf47(act, driver);
}
RX_FUNCTION(0x0056a720, rx_section_race_vf47)

/* 0056b020 slot 52: the audio / camera globals of the section race: 0x009c3ed8 = 0.5 (0x9322cc); the target 0x009c47c0 = 1 unless 0x009c47bc
 * already is. */
void rx_section_race_vf52(void) {
    SU32(0, 0x009c3ed8, U32(0, 0x009322cc));
    if (I32(0, 0x009c47bc) != 1) SU32(0, 0x009c47c0, 1);
}
RX_FUNCTION(0x0056b020, rx_section_race_vf52)

/* 0056aed0 (EDI = the level name [world + 0x20030]; ESI = the area): the AI track asset kind the section race loads for its level and
 * area: the first of the kinds 0, 1, 9, 10, 0xc whose asset exists (00536e80), else 5 if that exists, else 0.
 * Frame: push ecx / esi. */
uint32_t rx_section_track_audio_kind(rx_addr level, uint32_t area) {
    ACT_FRAME(F, 0, 2);
    static const uint8_t kinds[5] = {0, 1, 9, 10, 0xc};
    uint32_t has;
    for (int k = 0; k < 5; k++) {
        ACT_CALLR(has, 1, rx_track_has_asset(kinds[k], level, area));
        if (has != 0) { ACT_END(F); return kinds[k]; }
    }
    ACT_CALLR(has, 1, rx_track_has_asset(5, level, area));
    ACT_END(F);
    return has != 0 ? 5 : 0;
}
RX_FUNCTION(0x0056aed0, rx_section_track_audio_kind)
