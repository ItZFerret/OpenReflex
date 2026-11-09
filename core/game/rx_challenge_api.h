#pragma once
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The Free Ride challenges' end-of-challenge page (game/race_challenge_state.c), additive to RX_SESSION_API_VERSION 2 (no existing
 * struct changed).
 *
 * In the original the PostChallengePopup (Shared.ENG PostChallengePopup.lyr / .lua) is shown by the UI manager when the challenge
 * activity raises "Show Activity Results" in its Post state (MXUIPostEventOverlay_OnShowActivityResults 005f42e0): it reads the
 * activity's medal flags (MXActivityChallenge +0xff074 gold, +0xff078 silver, +0xff07c bronze, set at the Post enter by the class's
 * vf64 and awarded to the profile by 0054e670) into the UI variable ChallengeMedalStatus ("Gold", "Silver", "Bronze", "Failed")
 * and the value (vf62) into ChallengePlayerResult. RESTART runs App.RestartActivity (MXGame_RestartActivity 00759d00), EXIT the
 * switch back to the level's Free Ride. This API hands the host the same state; it computes nothing the game did not. */
#define RX_CHALLENGE_API_VERSION 1u

/* Copied snapshot, 88 bytes; no guest addresses. valid = 1 when the local player's activity is a challenge (MXActivityChallenge or
 * a subclass). kind is A+0xff070 (0 BigAir, 1 HillClimb, 3 Pathfinder, 4 StepUp, 6 Trailblazer, 8 TargetPractice), state A+0x21c
 * (0 Pre, 1 Countdown, 2 InProgress, 3 WindDown, 4 Post, 5 Inactive), area A+0x220 (the track).
 * gold / silver / bronze are the activity's medal flags (A+0xff074 / 078 / 07c); medal is what the popup shows
 * (ChallengeMedalStatus): 0 Failed, 1 Bronze, 2 Silver, 3 Gold. The flags are set at the Post enter, so they are meaningful from
 * state 4 on. value is the class's vf62 (HillClimb: the best height in feet, Trailblazer: the time in seconds, TargetPractice: the
 * hits; value_valid = 0 when the class has none in the core). criteria[0..2] are the Bronze / Silver / Gold criteria of the level's
 * MXChallengesSpecEntity for the kind and area (0057b160; 10.0 without the entity), the numbers PreChallengePopup lists
 * (App.GetChallengeAwardCriteria). In Pre (state 0: PreChallengePopup's "Your record") and from state 4 on, record_* is the profile's PlayerChallenges row of the
 * location (before the attempt, resp. after the award; 004ea6c0): the medal counts and the best value (record_valid = 0: no stored value). */
typedef struct rx_challenge_state {
    uint32_t size, version, valid;
    int32_t kind, state, area;
    int32_t gold, silver, bronze, medal;
    int32_t value_valid;
    float value;
    float criteria[3];
    int32_t record_valid;
    float record;
    int32_t record_counts[3];   /* Bronze, Silver, Gold */
    int32_t reserved[2];
} rx_challenge_state;

/* Caller sets size (sizeof) and version (1). Invalid arguments return 1 and leave the output alone. Session owner thread, between
 * advances. */
RX_API int rx_session_challenge(struct rx_session* session, rx_challenge_state* out);

/* RESTART of the popup: App.RestartActivity -> MXGame_RestartActivity 00759d00: every activity of the event world with drivers is
 * restarted (its vf16: the challenge's players back at the start, Activity::vf16 00557b40, the medal flags and the value cleared by
 * vf18 as the state machine re-enters Pre). The world and the session keep running. 0 ok, 1 bad session, 2 the local player has no
 * challenge activity. Session owner thread, between advances. */
RX_API int rx_session_challenge_restart(struct rx_session* session);
/* RESTART of the pause menu, any event (PauseMenu:OnRestartEventPopupResult: App.RestartActivity, then SwitchToHud): the same
 * MXGame_RestartActivity as above for the local player's activity (an arcade event, a Waypoint race, a career event: PauseMenu.lua offers
 * RESTART in every mode but the Free Ride and the online games). The activity starts again from its Pre state in the running world;
 * the career keeps its event, phase, heat, moto, gate and standings, and records nothing: the results are written by the activity's
 * Post of the attempt that is finished. 0 ok, 1 bad session, 2 the local player has no activity, 3 not allowed now (the session's
 * records are final, or the career event's Continue was submitted: its results were taken). Session owner thread, between advances. */
RX_API int rx_session_restart_activity(struct rx_session* session);
/* The page's prize (w-career): PostChallengePopup:Start pays a medal as the page appears (PostChallengePopup.lua): the SeriesPoints row
 * of the level's reward level (App.GetChallengeAwardLevel 0073ec10) and the place (Gold 1, Silver 2, Bronze 3) gives the PRIZE
 * (Reward) and the BONUS (BonusReward, dropped when the profile held the medal before), MXTableHelper:DepositIntoAccount (004e8350)
 * pays PRIZE + BONUS into PlayerRider AccountBalance / TotalEarnings, and CheckForFreeRideSeriesChallengeCompletion updates the
 * challenges series' PlayerSeriesStatus (created on the first medal; BestPlaceFinish 0 = the series won, once every challenge of
 * the level has a gold). A Failed challenge pays nothing (paid 0, medal 0) and shows no CreditReward.
 * series_id is the UI SeriesId the Race & Ride challenges page sets (the Free Ride challenges series of the level: 0, 1, 2).
 * Call when the PostChallengePopup is shown, with the challenge in Post (state 4). The prize is paid once per Post: a repeated call
 * returns the same copy with repeat 1 and pays nothing; RESTART (rx_session_challenge_restart) arms the next Post. The profile is
 * saved when the session's ride is settled (EXIT: App.RequestSaveGame in the original). */
#define RX_CHALLENGE_PAYOUT_VERSION 1u
typedef struct rx_challenge_payout {
    uint32_t size, version;
    uint32_t paid;                   /* the deposit was made (for this Post) */
    uint32_t repeat;                 /* this call found the Post already paid */
    int32_t medal;                   /* 0 Failed, 1 Bronze, 2 Silver, 3 Gold */
    int32_t reward_level;            /* SeriesPointId (0073ec10) */
    int32_t event_type_location_id;
    int32_t prize, bonus, total;     /* credits */
    int32_t account_balance;         /* PlayerRider AccountBalance after the deposit */
    int32_t series_id;
    int32_t series_best_place;       /* PlayerSeriesStatus BestPlaceFinish after the check (-1: not checked) */
    uint32_t series_row_created, series_completed;
    int32_t reserved[2];
} rx_challenge_payout;
/* 0 ok; 1 bad arguments / session; 2 the local player has no challenge activity in Post. Session owner thread, between advances. */
RX_API int rx_session_challenge_payout(struct rx_session* session, int32_t series_id, rx_challenge_payout* out);

/* PreChallengePopup (w-polish; additive, RX_CHALLENGE_API_VERSION unchanged): the original's popup is shown while the challenge is in
 * Pre (vf2 raises "ShowPreChallengePopup"), sets the UI variable WaitForUI to 1 at Start ("activities are not allowed to start") and to
 * 0 at End; the Pre update (Activity::vf25 00558ce0) is the only reader and starts the countdown only when it reads 0. The host that
 * draws the popup holds the property here: hold = 1 keeps the challenge in Pre (it may be set before the event load: the property is
 * then 1 from the first frame), hold = 0 releases it. Exhibition sessions; the hold is cleared when the next session is created.
 * 0 ok, 1 bad session or a career session (whose own properties decide). */
RX_API int rx_session_wait_for_ui(struct rx_session* session, int32_t hold);
#ifdef __cplusplus
}
#endif
