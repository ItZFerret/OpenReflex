/* race_finish_highlights.c -- the race finish's highlight progress (agent "finishhl", 2026-10-29): 00569930 MXActivityRace_FinishHighlights
 * and 006d8530 MXHighlightTracking_OnRaceFinished, with the body-panel checks 0047bed0 / 0047bf00 (and 0047baf0 under the first) that
 * the four-wheeler highlights read. Hand transliterations of the disassembly. The highlight progress itself is 006d7980
 * (rx_highlight_add_progress, highlight_progress.c: PlayerHighlight.GoalProgress / UnlockedOrder in the profile).
 *
 * Highlights added here (index passed to 006d7980; the table row is index + 1):
 *   0x2c  ACHIEVEMENT_MISC_09  a win (place 0) at UI property AIDifficulty == 0x78 (120, "All Time"), activity type != 8
 *   0x26  ACHIEVEMENT_MISC_03  more than 2 unique tricks (player +0x1074 .. +0x1084), activity type != 7
 *   0x29  a car (vehicle type 3 / 5 / 7) that finished with every body panel gone
 *   0x28  a car (vehicle type 3 / 5 / 7) that finished with no panel lost
 * Not here: 006d8420 (highlight start, 0x17 .. 0x1d) is online-only (MXGame_PlayerMode == 2) and stays the product boundary in
 * race_finish.c. */
#include "race.h"
#include "rider_cap.h"
#include "race_extern.h"
#include "race_finish.h"
#include "race_activity_ui.h"
#include "highlight_progress.h"
#include "world_veh.h"

#define STR_AI_DIFFICULTY   0x00936e48u   /* "AIDifficulty" */
#define STR_PUBLIC_ACCESS   0x0094bfacu   /* "PublicAccess" */
#define HIGHLIGHT_TRACKING  0x00d92edcu   /* the tracking object's global */

/* 006d7980 AddProgress (stdcall tracking, controller, index; RET 0xc). The oracle's rx_highlight_add_progress (oracle_highlight.cpp:
 * highlight_progress.c needs a Steam provider there) calls the original, whose call pushes its own arguments; the product runs the
 * port at the original's depth. */
#ifdef RX_ORACLE
#define ADD_PROGRESS(t, c, i) ((void)rx_highlight_add_progress((t), (c), (i)))
#else
#define ADD_PROGRESS(t, c, i) RACE_CORE_CALL(3, (void)rx_highlight_add_progress((t), (c), (i)))
#endif

/* ---- the body-panel checks ------------------------------------------------------------------------------------------------- */

/* 0047baf0 (ECX = a damage behaviour of the manager 0xdb1410; plain ret): 1 when every part is gone: for each of the +0 parts the
 * state (+0xc + 4i) is at least 3, or the vehicle (+0x3fc) is type 3 and the part's model node (+0x370 + 4i) is 0 (never fitted). */
uint32_t rx_damage_panels_all_gone(rx_addr beh) {
    const uint32_t count = U32(beh, 0);
    if (count == 0) return 1;
    const uint32_t type = U32(U32(beh, 0x3fc), 0x194);
    for (uint32_t i = 0; i < count; i++) {
        const int absent = type == 3 && U32(beh, 0x370 + 4 * i) == 0;
        if (I32(beh, 0xc + 4 * i) < 3 && !absent) return 0;
    }
    return 1;
}
RX_FUNCTION(0x0047baf0, rx_damage_panels_all_gone)

/* 0047bed0 (ESI = the damage manager, EDI = the vehicle; plain ret): the behaviour of this vehicle (the first of the 12 whose
 * +0x3fc matches) is asked 0047baf0; a vehicle with no behaviour answers 0. */
uint32_t rx_damage_vehicle_all_gone(rx_addr mgr, rx_addr veh) {
    for (uint32_t i = 0; i < rx_dmg_n(); i++) {   /* (w-rc-core) + the side entries made */
        const rx_addr beh = rx_dmg_entry(mgr, i);
        if (U32(beh, 0x3fc) == veh) return rx_damage_panels_all_gone(beh);
    }
    return 0;
}
RX_FUNCTION(0x0047bed0, rx_damage_vehicle_all_gone)

/* 0047bf00 (EAX = the damage manager, EDI = the vehicle; plain ret): 1 when the vehicle's behaviour has no part past state 2
 * (nothing broken off); 0 when one is, and 0 for a vehicle with no behaviour. */
uint32_t rx_damage_vehicle_none_lost(rx_addr mgr, rx_addr veh) {
    for (uint32_t i = 0; i < rx_dmg_n(); i++) {   /* (w-rc-core) + the side entries made */
        const rx_addr beh = rx_dmg_entry(mgr, i);
        if (U32(beh, 0x3fc) != veh) continue;
        const uint32_t count = U32(beh, 0);
        for (uint32_t j = 0; j < count; j++)
            if (I32(beh, 0xc + 4 * j) > 2) return 0;
        return 1;
    }
    return 0;
}
RX_FUNCTION(0x0047bf00, rx_damage_vehicle_none_lost)

/* ---- the highlight progress ------------------------------------------------------------------------------------------------ */

/* 006d8530 (stdcall tracking, place; ESI = the player, EDI = its controller (+0x1f0); RET 8): a win (place 0) of a player whose
 * activity (+0x94) is absent or not type 8, with UI property AIDifficulty == 0x78: highlight 0x2c. Then for a player with an
 * activity: "PublicAccess" is read into the same slot (the answer is not used), and when the activity is not type 7 and the five
 * trick counters (+0x1074 .. +0x1084) add up to more than 2: highlight 0x26. The original reuses its first argument slot as the
 * UI properties' output (the tracking pointer lives in EBX). Frame: push ebx. */
void rx_highlight_on_race_finished(rx_addr tracking, uint32_t place, rx_addr player, uint32_t controller) {
    RACE_FRAME_PLAIN(F, 0, 1);
    const rx_addr slot = F_sp + 4;   /* the first argument slot, past the return address */
    if (player != 0 && place == 0) {
        const rx_addr act = U32(player, 0x94);
        if (act == 0 || U32(act, 0x238) != 8) {
            SU32(slot, 0, 0);
            RACE_CORE_CALL(2, (void)rx_ui_session_get_int(STR_AI_DIFFICULTY, slot));
            if (U32(slot, 0) == 0x78) ADD_PROGRESS(tracking, controller, 0x2c);
        }
    }
    if (U32(player, 0x94) != 0) {
        RACE_CORE_CALL(2, (void)rx_ui_session_get_int(STR_PUBLIC_ACCESS, slot));
        const int32_t tricks = I32(player, 0x1084) + I32(player, 0x1080) + I32(player, 0x107c) + I32(player, 0x1078) + I32(player, 0x1074);
        if (U32(U32(player, 0x94), 0x238) != 7 && tricks >= 3)
            ADD_PROGRESS(tracking, controller, 0x26);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x006d8530, rx_highlight_on_race_finished)

/* 00569930 MXActivityRace_FinishHighlights (stdcall activity; RET 4): for each driver (+0x268, count +0x26c) whose player (+0x74) is
 * a local human (+4 set, +0x10 clear, controller +0x1f0 >= 0) and which is a racer (+0x70 set): 006d8530 with the driver's place
 * (+0x78); then for a player with a vehicle (+0x1ac) of type 3 / 5 / 7 (+0x194): every panel gone (0047bed0) adds highlight 0x29,
 * else nothing lost (0047bf00) adds 0x28. The damage manager singleton (0047bb40) is asked before the type test and before each
 * check, as the original does. Frame: push ebp; and esp, -8; push ecx (the loop counter); push ebx / esi / edi. */
void rx_race_finish_highlights(rx_addr act) {
    RACE_FRAME_EBP(F, 8, 4, 3);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4 * (uint32_t)i);
        const rx_addr p = U32(d, 0x74);
        if (U32(p, 4) == 0 || U32(p, 0x10) != 0 || I32(p, 0x1f0) < 0 || U32(d, 0x70) == 0) continue;
        RACE_CORE_CALL(2, rx_highlight_on_race_finished(U32(0, HIGHLIGHT_TRACKING), U32(d, 0x78), p, U32(p, 0x1f0)));
        const rx_addr veh = U32(U32(d, 0x74), 0x1ac);
        if (veh == 0) continue;
        RACE_CORE_CALL(0, (void)rx_world_damage_manager_instance());
        const uint32_t type = U32(veh, 0x194);
        if (type != 3 && type != 5 && type != 7) continue;
        rx_addr mgr;
        uint32_t index = 0x29;
        RACE_CORE_CALL(0, mgr = rx_world_damage_manager_instance());
        if (!rx_damage_vehicle_all_gone(mgr, U32(U32(d, 0x74), 0x1ac))) {
            RACE_CORE_CALL(0, mgr = rx_world_damage_manager_instance());
            if (!rx_damage_vehicle_none_lost(mgr, U32(U32(d, 0x74), 0x1ac))) continue;
            index = 0x28;
        }
        ADD_PROGRESS(U32(0, HIGHLIGHT_TRACKING), U32(U32(d, 0x74), 0x1f0), index);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00569930, rx_race_finish_highlights)
