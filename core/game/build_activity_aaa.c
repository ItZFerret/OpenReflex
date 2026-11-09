/* build_activity_aaa.c -- the activity area's re-creation inside a running event world ("AAA Creation"): a series event whose next
 * race needs other riders (the Heat format's heat -> LCQ -> main event: MXSeriesEvent_SetupNextActivityPlayers 005a8af0 finds riders
 * of the next phase that are not active) routes to the in-game "MXUIAAALoading Page" (UIRoutingMessage "Select Activity Area"); its
 * entry (career_aaa_loading.c, 005a8030) clears the activity table, marks the next riders, and calls Game_StartAAACreation:
 *   game state 9:  the job "AAA Creation" on the Database loader (0075a3d0 -> 00766420: the world's activities destroyed and created
 *                  again from the activity table); when it is done (+0x15b04) the activities started (007665b0), the player slots
 *                  synchronised with the Player List (0075ba20) and the new players' vehicles / riders spawned (00766e70); state 10;
 *   game state 10: when the loader is idle, the UI resumed, and once every local player has its vehicle and rider: game state 2 and the
 *                  series event's gate selection (vf9), or the "AdvanceEvent" the UI asked for (NeedSeriesAdvanceEvent).
 * MXGame::Update's two states are in build_activity_game.c; these are their callees. */
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "race_activity.h"

/* ---- Game_StartAAACreation 0075a380 (EAX = activity type, ECX = activity mode) ------------------------------------------------------
 * The type / mode are what game state 9 restarts the race with (0x3e = none: the race is not restarted, the series event sets it up).
 * The world's activities suspended (007663d0), then the job posted (Database vf14: name, function, argument, progress = 0). */
void rx_game_start_aaa_creation(uint32_t activity_type, uint32_t activity_mode) {
    ACT_FRAME(F, 0, 1);
    const rx_addr game = U32(0, 0x00a946b8);
    SU32(game, 0x18, 9);
    SU32(game, 0x15b08, activity_type);
    SU32(game, 0x15b0c, activity_mode);
    ACT_CALL(1, rx_world_activities_suspend(U32(game, 0x5a54)));         /* 007663d0 */
    SU32(game, 0x15b04, 0);
    const rx_addr database = U32(U32(0, 0x00a95844), 8);
    act_vcall(database, 0x38, 4, (const uint32_t[]){0x0096d708 /* "AAA Creation" */, 0x0075a3d0, game, 0});
    ACT_END(F);
}

/* ---- 0075a3d0 (the loader job: cdecl game; the original reads no other argument) -> 0: the world's activities re-created, the job's done flag (+0x15b04) ---- */
uint32_t rx_game_aaa_creation_job(rx_addr game) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_world_recreate_activities(U32(game, 0x5a54)));        /* 00766420 */
    SU32(game, 0x15b04, 1);
    ACT_END(F);
    return 0;
}
RX_FUNCTION(0x0075a3d0, rx_game_aaa_creation_job)

/* ---- 007663d0 (stdcall world; RET 4): every activity's vf17 (the race suspended), and vf3 for an activity with +0xec4 set ------- */
void rx_world_activities_suspend(rx_addr world) {
    ACT_FRAME(F, 0, 4);
    const int32_t count = I32(world, 0x21da8);
    for (int32_t i = 0; i < count; i++) {
        const rx_addr activity = U32(U32(world, 0x21da4), 4 * (uint32_t)i);
        act_vcall(activity, 0x44, 0, 0);
        if (U32(activity, 0xec4) != 0) act_vcall(activity, 0xc, 0, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x007663d0, rx_world_activities_suspend)

/* ---- 00766420 (stdcall world; RET 4): the world's activities (list +0x21da4, count +0x21da8), last first: vf6(0) (ended), removed
 * from the list (the last one moved into its place), the scalar deleting destructor vf0(1). Then one activity per row of the
 * activity table (0x00ae1b18): the factory (0055ef80, the world's activity manager +0x21da0, the row's ActivityType), vf1 with the
 * row record {row, 0, the table} (on the stack), vf5 (load). Frame: and esp, -8; sub esp, 0x14; push ebx / esi / edi. */
void rx_world_recreate_activities(rx_addr world) {
    ACT_FRAME_EBP(F, 8, 0x14, 3);
    for (int32_t index = I32(world, 0x21da8) - 1; index >= 0; index--) {
        const rx_addr activity = U32(U32(world, 0x21da4), 4 * (uint32_t)index);
        const uint32_t ended = 0;
        act_vcall(activity, 0x18, 1, &ended);
        const int32_t count = I32(world, 0x21da8);
        const rx_addr list = U32(world, 0x21da4);
        for (int32_t k = 0; k < count; k++) {
            if (U32(list, 4 * (uint32_t)k) != activity) continue;
            SU32(list, 4 * (uint32_t)k, U32(list, 4 * (uint32_t)(count - 1)));
            SU32(world, 0x21da8, U32(world, 0x21da8) - 1);
            break;
        }
        const uint32_t free_it = 1;
        act_vcall(activity, 0, 1, &free_it);
    }
    const rx_addr row_record = F + 0x14;
    for (uint32_t row = 0; row < rx_tbl_row_count(ACT_ACTIVITY_TBL); row++) {   /* 004eb830 (0 without columns) */
        SU32(row_record, 0, row);
        SU32(row_record, 4, 0);
        SU32(row_record, 8, ACT_ACTIVITY_TBL);
        const uint32_t type = act_cell(ACT_ACTIVITY_TBL, 0x0093dd90 /* "ActivityType" */, row);
        rx_addr activity;
        ACT_CALLR(activity, 2, rx_activity_create(world + 0x21da0, type));
        act_vcall(activity, 4, 1, &row_record);
        act_vcall(activity, 0x14, 0, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00766420, rx_world_recreate_activities)

/* ---- 007665b0 (stdcall world; RET 4): every activity started again: vf2("PostEventOverlay"), the overlay page its results use ---- */
void rx_world_activities_restart(rx_addr world) {
    ACT_FRAME(F, 0, 3);
    const int32_t count = I32(world, 0x21da8);
    for (int32_t i = 0; i < count; i++) {
        const rx_addr activity = U32(U32(world, 0x21da4), 4 * (uint32_t)i);
        act_vcall(activity, 8, 1, (const uint32_t[]){0x0096dd14 /* "PostEventOverlay" */});
    }
    ACT_END(F);
}
RX_FUNCTION(0x007665b0, rx_world_activities_restart)

/* ---- 00766e70 (stdcall world; RET 4): one spawn step (PlayerSlot_SpawnStep 00767200) for every active player slot (00552b90 kind
 * 3, while 00552ce0 counts them) ------------------------------------------------------------------------------------------------- */
void rx_world_players_spawn(rx_addr world) {
    ACT_FRAME(F, 0, 4);
    const rx_addr slots = U32(0, 0x00a946b8) + 0x7460;
    for (uint32_t i = 0;; i++) {
        uint32_t count;
        ACT_CALLR(count, 0, rx_players_count(slots, 3));
        if (i >= count) break;
        rx_addr player;
        ACT_CALLR(player, 2, rx_player_active(slots, i, 3));
        ACT_CALL(1, (void)rx_player_slot_spawn_step(player, world));
    }
    ACT_END(F);
}
RX_FUNCTION(0x00766e70, rx_world_players_spawn)
