/* race_challenge_payout.h -- the Free Ride challenges' prize and the challenges series' completion (race_challenge_payout.c). */
#pragma once
#include "rx_game.h"
#include "rx_challenge_api.h"

int32_t rx_challenge_award_level(uint32_t activity_type);   /* 0073ec10 App.GetChallengeAwardLevel's native (EAX = the activity type) */
/* PostChallengePopup:Start's payout for the local player's challenge in Post: fills out (medal, level, prize, bonus, total, the balance
 * after the deposit, the series status). Returns 0 when the player has no profile. */
int rx_challenge_payout_apply(rx_addr player, int32_t series_id, rx_challenge_payout* out);
