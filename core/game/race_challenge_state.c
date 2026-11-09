/* race_challenge_state.c -- what the end-of-challenge page (PostChallengePopup) shows, read from the challenge activity.
 *
 * Not a transliteration: it reads the fields the original's UI reads, in the order it reads them, through the core's ports of
 * the callees (nothing is recomputed):
 *   - MXUIPostEventOverlay_OnShowActivityResults 005f42e0: ChallengeMedalStatus is "Gold" when A+0xff074 is set, else "Silver"
 *     (+0xff078), else "Bronze" (+0xff07c), else "Failed"; the value is the activity's vf62 (+0xf8);
 *   - PreChallengePopup:Populate / App_GetChallengeAwardCriteria_Lua 00736630 -> 0073eb60: the medal criteria of the level's
 *     MXChallengesSpecEntity (0057b160);
 *   - PostChallengePopup:Start: the PlayerChallenges row of the location (the UI properties EventLocationId / TrackId and the
 *     activity's event type through 004e7780 / 004ea0a0, then 004ea6c0), the Bronze / Silver / Gold counts and the stored value
 *     Param, exactly as 0054e670 reads them before it writes the award.
 * The record is read in Pre (PreChallengePopup:Populate lists "Your record": the row before the attempt, a pure read) and from the Post
 * state on (the award is written at the Post enter); in between it is left unread. */
#include "race_challenge_state.h"
#include "race_challenge.h"
#include "race_challenge_activity.h"
#include "race_activity_ui.h"
#include "build_activity_util.h"
#include "build_activity_freeride_run.h"
#include <string.h>

#define STR_EVENT_LOCATION_ID 0x00936e20u   /* "EventLocationId" */
#define STR_TRACK_ID          0x00936e40u   /* "TrackId" */

/* the classes' vtables (race_challenge_activity.c) */
static int challenge_vtable(uint32_t vtable) {
    switch (vtable) {
    case 0x009450ccu: case 0x009451dcu: case 0x009452ecu: case 0x009453fcu: case 0x00945514u: case 0x00945624u: case 0x00945734u: return 1;
    default: return 0;
    }
}

/* the activity's vf62 (+0xf8): the value of the challenge; 0 when the class's is not in the core (cut content) */
static int challenge_value(rx_addr activity, float* value) {
    float (*fn)(rx_addr) = RX_FN(U32(U32(activity, 0), 0xf8), float (*)(rx_addr));
    if (!fn) return 0;
    float v;
    ACT_CALLR(v, 0, fn(activity));
    *value = v;
    return 1;
}

/* the player's PlayerChallenges row of the location (0054e670's first half) */
static void challenge_record(rx_addr activity, rx_challenge_state* out) {
    const rx_addr player = U32(U32(U32(activity, 0x268), 0), 0x74);   /* the first driver's player (FIRST_PLAYER) */
    if (!player || I32(player, 0x1f0) < 0) return;
    ACT_FRAME(F, 0x20, 0);
    const rx_addr location = F, track = F + 4, bronze = F + 8, silver = F + 0xc, gold = F + 0x10, param = F + 0x14;
    SU32(location, 0, 0xffffffffu);
    ACT_CALL(2, (void)rx_ui_session_get_int(STR_EVENT_LOCATION_ID, location));
    SU32(track, 0, 0);
    ACT_CALL(2, (void)rx_ui_session_get_int(STR_TRACK_ID, track));
    uint32_t type_id;
    ACT_CALLR(type_id, 1, rx_event_type_id_of(U32(activity, 0xff070) + 0x15));
    int32_t etl;
    ACT_CALLR(etl, 3, rx_challenge_event_location(type_id, U32(location, 0), U32(track, 0)));
    SU32(param, 0, 0xbf800000u);   /* -1.0f: no stored value */
    SU32(bronze, 0, 0); SU32(silver, 0, 0); SU32(gold, 0, 0);
    ACT_CALL(5, rx_challenge_profile_read(U32(player, 0x1f0), (uint32_t)etl, bronze, silver, gold, param));
    out->record_counts[0] = I32(bronze, 0);
    out->record_counts[1] = I32(silver, 0);
    out->record_counts[2] = I32(gold, 0);
    out->record = F32(param, 0);
    out->record_valid = F32(param, 0) != -1.0f;
    ACT_END(F);
}

int rx_challenge_state_read(rx_addr activity, rx_challenge_state* out) {
    if (!activity || !challenge_vtable(U32(activity, 0))) return 0;
    out->valid = 1;
    out->kind = I32(activity, 0xff070);
    out->state = I32(activity, 0x21c);
    out->area = I32(activity, 0x220);
    out->gold = U32(activity, 0xff074) != 0;
    out->silver = U32(activity, 0xff078) != 0;
    out->bronze = U32(activity, 0xff07c) != 0;
    out->medal = out->gold ? 3 : out->silver ? 2 : out->bronze ? 1 : 0;   /* 005f42e0 */
    float value = 0.0f;
    out->value_valid = challenge_value(activity, &value);
    out->value = value;
    for (uint32_t medal = 0; medal < 3; medal++) {
        float criterion;
        ACT_CALLR(criterion, 0, rx_challenge_medal_value(activity, medal));
        out->criteria[medal] = criterion;
    }
    if (out->state == 4 || out->state == 0) challenge_record(activity, out);
    return 1;
}
