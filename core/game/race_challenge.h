/* race_challenge.h -- the profile's challenge awards (race_challenge.c): the Free Ride jump / discovery medals written into the
 * player's "PlayerChallenges" table, and the challenge-award event they raise. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* value arguments that are floats in the original travel as their IEEE bits (x87 fld / fstp pairs quiet a signalling NaN) */
void     rx_challenge_award(rx_addr player, uint32_t kind, uint32_t medal, uint32_t value_bits);           /* 0054e670 (stdcall; RET 0x10) */
int32_t  rx_challenge_event_location(uint32_t type_id, uint32_t location, uint32_t track);                  /* 004ea0a0 (stdcall; RET 0xc) */
void     rx_challenge_profile_read(uint32_t profile, uint32_t etl, rx_addr bronze, rx_addr silver,
                                   rx_addr gold, rx_addr param);                                           /* 004ea6c0 (EAX = profile; stdcall; RET 0x14) */
void     rx_challenge_profile_write(uint32_t profile, uint32_t etl, uint32_t bronze, uint32_t silver,
                                    uint32_t gold, uint32_t param_bits);                                   /* 004ea3a0 (EAX = profile; stdcall; RET 0x14) */
void     rx_challenge_career_progress(rx_addr tracking, uint32_t profile);                                 /* 006d8390 (EDI = tracking, ESI = profile) */
void     rx_challenge_award_event(uint32_t kind, uint32_t medal, rx_addr events, rx_addr player);          /* 00554d40 (ECX = kind, EDX = medal, ESI = events; stdcall player; RET 4) */
int32_t  rx_challenge_compare_lower_better(rx_addr activity, uint32_t new_bits, uint32_t old_bits);               /* 0055ec70 (activity vf63 of most challenges: -1 when the new value is lower; RET 8) */
int32_t  rx_challenge_compare_higher_better(rx_addr activity, uint32_t new_bits, uint32_t old_bits);              /* 0055ed20 (hill climb / target practice vf63: -1 when the new value is higher; RET 8) */
#ifdef __cplusplus
}
#endif
