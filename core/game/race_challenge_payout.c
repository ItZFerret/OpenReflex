/* race_challenge_payout.c -- the Free Ride challenges' prize and the challenges series' completion (w-career, 2026-10-26).
 *
 * In the original, PostChallengePopup:Start (Shared.ENG PostChallengePopup.lua) pays a medal when the page appears:
 *   - the reward level: App.GetChallengeAwardLevel(activity type) = 0073ec10 (below, the level's MXChallengesSpecEntity);
 *   - the place: Gold 1, Silver 2, Bronze 3 (a "Failed" challenge pays nothing and shows no CreditReward);
 *   - the SeriesPoints row {SeriesPointId = reward level, Place = place}: Reward = the prize, BonusReward = the bonus (0 and 0 without
 *     exactly one row);
 *   - the bonus is dropped when the medal was held before: the PlayerChallenges row of the location (EventTypeLocationId of
 *     MapEventTypeId(type) 004e7780, EventLocationId, TrackId: 004ea0a0) has a count > 1 for that medal (the award 0054e670 already
 *     counted this one at the Post enter);
 *   - MXTableHelper:DepositIntoAccount(exclusive device, prize + bonus) = 004e8350 (career_shop.c: PlayerRider AccountBalance and
 *     TotalEarnings, the UI AccountBalance);
 *   - CheckForFreeRideSeriesChallengeCompletion(UI SeriesId, EventLocationId) (the global function of the same script): the
 *     challenges series' PlayerSeriesStatus row is created on the first medal ({SeriesId, BestPlaceFinish 1000, Gold / Silver /
 *     BronzeMedals 0, SeriesOwner "Tour", SeriesStatus 4}); afterwards, unless BestPlaceFinish is already 0, BestPlaceFinish becomes 0
 *     (the series won) once every challenge location of the level (EventTypeLocations with EventLocationId, EventType.IsChallenge 1)
 *     has a PlayerChallenges row with Gold > 0 (the Lua stops at the first location without one).
 * EXIT then saves (App.RequestSaveGame): in the product the session's final settlement exports the profile when the ride closes.
 * The script is Lua, not machine code: this is its transliteration on the core's tables (the same Rdb owners the Lua's queries reach
 * through SWIG), called by the product's owner of the page (api/challenge_session.inc). 0073ec10 is the one native function. */
#include "race_challenge_payout.h"
#include "build_activity_util.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "build_activity_freeride_run.h"
#include "career_shop.h"
#include "race_challenge.h"
#include "race_activity_ui.h"
#include "crt.h"
#include "anim.h"
#include "fx.h"
#include <string.h>

/* guest strings of the tables and columns (the exe's own literals) */
#define S_SERIES_POINTS        0x0094d8d4u   /* "SeriesPoints" */
#define S_SERIES_POINT_ID      0x0094d830u   /* "SeriesPointId" */
#define S_PLACE                0x0094d8e4u   /* "Place" */
#define S_REWARD               0x0094d8ecu   /* "Reward" */
#define S_BONUS_REWARD         0x0094d8f4u   /* "BonusReward" */
#define S_PLAYER_CHALLENGES    0x0093e7e0u   /* "PlayerChallenges" */
#define S_EVENT_TYPE_LOCATION  0x0093e228u   /* "EventTypeLocationId" */
#define S_GOLD                 0x0093dde4u   /* "Gold" */
#define S_SILVER               0x0093ddecu   /* "Silver" */
#define S_BRONZE               0x0093ddf4u   /* "Bronze" */
#define S_PLAYER_SERIES_STATUS 0x0094d340u   /* "PlayerSeriesStatus" */
#define S_SERIES_ID            0x0094c990u   /* "SeriesId" */
#define S_BEST_PLACE_FINISH    0x0094d36cu   /* "BestPlaceFinish" */
#define S_GOLD_MEDALS          0x0094d37cu   /* "GoldMedals" */
#define S_SILVER_MEDALS        0x0094d388u   /* "SilverMedals" */
#define S_BRONZE_MEDALS        0x0094d398u   /* "BronzeMedals" */
#define S_SERIES_OWNER         0x0094c9f4u   /* "SeriesOwner" */
#define S_SERIES_STATUS        0x0094d3a8u   /* "SeriesStatus" */
#define S_TOUR                 0x009463dcu   /* "Tour" */
#define S_EVENT_TYPE_LOCATIONS 0x00936e7cu   /* "EventTypeLocations" */
#define S_EVENT_LOCATION_ID    0x00936e20u   /* "EventLocationId" */
#define S_EVENT_TYPE_ID        0x00936e90u   /* "EventTypeId" */
#define S_EVENT_TYPE           0x00935e08u   /* "EventType" */

/* 0073ec10 App.GetChallengeAwardLevel's native (EAX = the activity type) -> EAX: the reward level of a challenge activity type
 * (0x15 .. 0x1e) from the level's MXChallengesSpecEntity (scene entity type 0x1019, the first one): its int at +0x2a0 + 12 * (type -
 * 0x15) when it is neither 0 nor -1, else 10; -1 for another type. The scene is the current world's (+0x20138): the alternate world
 * (game + 0x5a54) when the engine's alternate context (+0x20) is active (+0x34 == 1), else the normal one (game + 0x20c8) when it is
 * active. Frame: push ecx (the collect array, one slot, at entry - 8); push esi. */
int32_t rx_challenge_award_level(uint32_t activity_type) {
    ACT_FRAME(F, 0, 2);
    const uint32_t index = activity_type - 0x15;
    if (index > 9) {
        ACT_END(F);
        return -1;
    }
    const rx_addr context = U32(0, 0x00a95844);
    const rx_addr game = U32(0, 0x00a946b8);
    rx_addr world = 0;
    if (U32(U32(context, 0x20), 0x34) == 1) world = U32(game, 0x5a54);
    else if (U32(U32(context, 0x1c), 0x34) == 1) world = U32(game, 0x20c8);
    const rx_addr found = F + 4;
    SU32(found, 0, 0);
    int32_t count;
    ACT_CALLR(count, 4, (int32_t)rx_scene_manager_collect_typed(U32(world, 0x20138), 0x1019, found, 1, 0));
    for (int32_t i = 0; i < count; i++) {
        const int32_t level = I32(U32(found, 4u * (uint32_t)i), 0x2a0 + 12 * index);
        if (level != 0 && level != -1) {
            ACT_END(F);
            return level;
        }
    }
    ACT_END(F);
    return 10;
}
RX_FUNCTION(0x0073ec10, rx_challenge_award_level)

/* ---- the table reads and writes of the script ---------------------------------------------------------------------------------- */

static rx_addr table_of(rx_addr database, rx_addr name) {
    rx_addr t;
    ACT_CALLR(t, 1, rx_tbl_find(database, name));
    if (!t) act_unreached(0x004e8350, "challenge payout: authored table missing");
    return t;
}

static int32_t cell_int(rx_addr t, rx_addr name, uint32_t row) {
    return (int32_t)act_cell(t, name, row);
}

/* table:searchUnique_int(column, key): the row of the column's unique index, -1 without one (00418ec0) */
static int32_t unique_row(rx_addr t, rx_addr name, int32_t key) {
    uint32_t c;
    ACT_CALLR(c, 1, rx_tbl_column_index(t, name));
    if (c >= ((U32(t, 0x14) - U32(t, 0x10)) >> 2)) crt_invalid_parameter_noinfo();
    rx_addr ref;
    ACT_CALLR(ref, 1, rx_tbl_row(U32(U32(t, 0x10), 4 * c), key));
    return U32(ref, 8) && I32(ref, 0) != -1 ? I32(ref, 0) : -1;
}

/* row:setCell_int (Table_SetCellInt 004f6e60: the revision counted on a change) */
static void set_cell_int(rx_addr t, rx_addr name, uint32_t row, int32_t value) {
    RX_FRAME_ENTER(F, 4);
    SU32(F, 0, (uint32_t)value);
    ACT_CALL(2, rx_tbl_set_cell_int(t, name, row, F));
    RX_FRAME_LEAVE(F, 4);
}

/* The SeriesPoints row {SeriesPointId = level, Place = place}: its Reward and BonusReward; 0 and 0 unless exactly one row matches. */
static void series_points(int32_t level, int32_t place, int32_t* reward, int32_t* bonus) {
    const rx_addr t = table_of(RX_TBL_DEFAULT_DB, S_SERIES_POINTS);
    int32_t found = -1;
    uint32_t matches = 0;
    for (uint32_t row = 0; row < rx_tbl_row_count(t); row++) {
        if (cell_int(t, S_SERIES_POINT_ID, row) != level || cell_int(t, S_PLACE, row) != place) continue;
        found = (int32_t)row;
        matches++;
    }
    *reward = 0;
    *bonus = 0;
    if (matches != 1) return;
    *reward = cell_int(t, S_REWARD, (uint32_t)found);
    *bonus = cell_int(t, S_BONUS_REWARD, (uint32_t)found);
}

/* EventType.IsChallenge of an EventTypeId (0 when the type has no row) */
static int event_type_is_challenge(int32_t event_type_id) {
    const rx_addr types = table_of(RX_TBL_DEFAULT_DB, S_EVENT_TYPE);
    const int32_t row = unique_row(types, S_EVENT_TYPE_ID, event_type_id);
    if (row < 0) return 0;
    RX_FRAME_ENTER(F, 16);
    for (uint32_t i = 0; i < 12; i++) SU8(F, i, (uint8_t)"IsChallenge"[i]);
    const int yes = cell_int(types, F, (uint32_t)row) != 0;
    RX_FRAME_LEAVE(F, 16);
    return yes;
}

/* CheckForFreeRideSeriesChallengeCompletion(seriesId, eventLocationId) of PostChallengePopup.lua. */
static void series_completion(uint32_t profile, int32_t series_id, int32_t location, rx_challenge_payout* out) {
    const rx_addr statuses = table_of(RX_TBL_PLAYER_DB(profile), S_PLAYER_SERIES_STATUS);
    const rx_addr challenges = table_of(RX_TBL_PLAYER_DB(profile), S_PLAYER_CHALLENGES);
    const int32_t status_row = unique_row(statuses, S_SERIES_ID, series_id);
    if (status_row < 0) {
        /* the first medal of the series: its status row */
        RX_FRAME_ENTER(F, 0x120);
        const rx_addr vector = F + 0x18;
        const rx_addr criterion = F + 0x38;
        static const struct { rx_addr name; int32_t value; } fields[] = {
            { S_SERIES_ID, 0 }, { S_BEST_PLACE_FINISH, 1000 }, { S_GOLD_MEDALS, 0 }, { S_SILVER_MEDALS, 0 }, { S_BRONZE_MEDALS, 0 },
        };
        ACT_CALL(0, rx_tbl_crit_vec_init(vector));
        for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++) {
            const int32_t value = fields[i].name == S_SERIES_ID ? series_id : fields[i].value;
            ACT_CALL(1, rx_tbl_crit_init_int(criterion, fields[i].name, 0, (uint32_t)value));
            ACT_CALL(1, rx_tbl_crit_push(vector, criterion));
            ACT_CALL(0, rx_tbl_crit_destroy(criterion));
        }
        ACT_CALL(2, rx_tbl_crit_init_string(criterion, S_SERIES_OWNER, 0, S_TOUR));
        ACT_CALL(1, rx_tbl_crit_push(vector, criterion));
        ACT_CALL(0, rx_tbl_crit_destroy(criterion));
        ACT_CALL(1, rx_tbl_crit_init_int(criterion, S_SERIES_STATUS, 0, 4));
        ACT_CALL(1, rx_tbl_crit_push(vector, criterion));
        ACT_CALL(0, rx_tbl_crit_destroy(criterion));
        ACT_CALL(2, (void)rx_tbl_add_row(statuses, F + 4, vector));
        ACT_CALL(0, rx_tbl_crit_vec_destroy(vector));
        RX_FRAME_LEAVE(F, 0x120);
        out->series_row_created = 1;
        out->series_best_place = 1000;
        return;
    }
    out->series_best_place = cell_int(statuses, S_BEST_PLACE_FINISH, (uint32_t)status_row);
    if (out->series_best_place == 0) return;   /* "Skipping completion check, already have gold!" */
    const rx_addr locations = table_of(RX_TBL_DEFAULT_DB, S_EVENT_TYPE_LOCATIONS);
    uint32_t challenge_rows = 0;
    uint32_t golds = 0;
    int stopped = 0;
    for (uint32_t row = 0; row < rx_tbl_row_count(locations); row++) {
        if (cell_int(locations, S_EVENT_LOCATION_ID, row) != location) continue;
        if (!event_type_is_challenge(cell_int(locations, S_EVENT_TYPE_ID, row))) continue;
        challenge_rows++;
        if (stopped) continue;
        const int32_t etl = cell_int(locations, S_EVENT_TYPE_LOCATION, row);
        const int32_t record = unique_row(challenges, S_EVENT_TYPE_LOCATION, etl);
        if (record >= 0 && cell_int(challenges, S_GOLD, (uint32_t)record) > 0) golds++;
        else stopped = 1;   /* "Aborting! No gold!" */
    }
    if (golds == challenge_rows) {
        set_cell_int(statuses, S_BEST_PLACE_FINISH, (uint32_t)status_row, 0);
        out->series_best_place = 0;
        out->series_completed = 1;
    }
}

int rx_challenge_payout_apply(rx_addr player, int32_t series_id, rx_challenge_payout* out) {
    const rx_addr activity = U32(player, 0x94);
    const int32_t profile = I32(player, 0x1f0);
    if (profile < 0 || profile > 3) return 0;
    /* ChallengeMedalStatus (005f42e0): Gold when +0xff074, else Silver +0xff078, else Bronze +0xff07c, else Failed */
    int32_t place = -1000;
    int32_t medal = 0;
    if (U32(activity, 0xff074)) { place = 1; medal = 3; }
    else if (U32(activity, 0xff078)) { place = 2; medal = 2; }
    else if (U32(activity, 0xff07c)) { place = 3; medal = 1; }
    out->medal = medal;
    out->series_id = series_id;
    out->series_best_place = -1;
    if (medal == 0) return 1;   /* Failed: no prize, the CreditReward stays hidden, no completion check */

    const uint32_t type = U32(activity, 0x23c);   /* App.GetMXActivityType(UI EventType): the challenge activity's type */
    out->reward_level = rx_challenge_award_level(type);
    RX_FRAME_ENTER(F, 8);
    SU32(F, 0, 0xffffffffu);
    ACT_CALL(2, (void)rx_ui_session_get_int(S_EVENT_LOCATION_ID, F));
    SU32(F, 4, 0);
    ACT_CALL(2, (void)rx_ui_session_get_int(0x00936e40 /* "TrackId" */, F + 4));
    const int32_t location = I32(F, 0);
    const int32_t track = I32(F, 4);
    RX_FRAME_LEAVE(F, 8);
    uint32_t type_id;
    ACT_CALLR(type_id, 1, rx_event_type_id_of(type));                                         /* MapEventTypeId 004e7780 */
    int32_t etl;
    ACT_CALLR(etl, 3, rx_challenge_event_location(type_id, (uint32_t)location, (uint32_t)track));   /* 004ea0a0 */
    out->event_type_location_id = etl;

    int32_t prize = 0;
    int32_t bonus = 0;
    series_points(out->reward_level, place, &prize, &bonus);
    const rx_addr challenges = table_of(RX_TBL_PLAYER_DB((uint32_t)profile), S_PLAYER_CHALLENGES);
    const int32_t record = unique_row(challenges, S_EVENT_TYPE_LOCATION, etl);
    if (record >= 0) {
        const int32_t gold = cell_int(challenges, S_GOLD, (uint32_t)record);
        const int32_t silver = cell_int(challenges, S_SILVER, (uint32_t)record);
        const int32_t bronze = cell_int(challenges, S_BRONZE, (uint32_t)record);
        if (gold > 1 && medal == 3) bonus = 0;
        else if (silver > 1 && medal == 2) bonus = 0;
        else if (bronze > 1 && medal == 1) bonus = 0;
    }
    out->prize = prize;
    out->bonus = bonus;
    out->total = prize + bonus;
    ACT_CALL(1, rx_career_shop_add_credits(profile, out->total));                              /* DepositIntoAccount 004e8350 */
    out->paid = 1;
    out->account_balance = cell_int(table_of(RX_TBL_PLAYER_DB((uint32_t)profile), 0x0093e53c /* "PlayerRider" */), 0x0093e5b4, 0);
    series_completion((uint32_t)profile, series_id, location, out);
    return 1;
}
