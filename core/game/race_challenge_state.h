/* race_challenge_state.h -- the end-of-challenge page's inputs (race_challenge_state.c). */
#pragma once
#include "rx_game.h"
#include "rx_challenge_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The challenge activity's page state (the API snapshot); 1 when `activity` is a challenge, else 0 (out untouched). */
int rx_challenge_state_read(rx_addr activity, rx_challenge_state* out);
#ifdef __cplusplus
}
#endif
