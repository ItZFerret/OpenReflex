/* sound_state.h -- the sound managers' inputs besides the engine emitters (sound_state.c, include/rx_sound.h; w-sfx). Product only:
 * the oracle build gets empty hooks. */
#pragma once
#include "rx_game.h"
#include "rx_sound.h"
void rx_sound_state_reset(void);
/* World_PostListeners (00769ab0, product): the world's contact events (world + 0, count + 0x1900) as the AudioCollisionManager gets them. */
void rx_sound_record_contacts(rx_addr events);
/* 006745d0 (product): a crowd reaction (1 negative, 2 positive, 3 positive start / end). */
void rx_sound_crowd_reaction(uint32_t kind);
/* 005812f0 (product): an activity audio cue played: its "<SoundAsset>/<EventName>" (guest string) and its sound index. */
void rx_sound_cue_played(rx_addr name, int32_t index);
void rx_sound_cue_text(const char* path, int32_t index);   /* the same from a host string (activity_cues.c) */
/* 006817c0 (product): a rider's sound slot started (the vehicle, the slot) */
void rx_sound_rider_sound(rx_addr vehicle, uint32_t slot);
/* the crowd unit's frame update runs in the product (its momentum block is the game's) */
void rx_sound_crowd_set_running(uint32_t on);
uint32_t rx_sound_crowd_running(void);
/* The query: game = the session's game block (0: the rings and globals only). 0 = no world (riders empty). */
int rx_sound_fill(rx_addr game, rx_sound_state* out);
/* object getters the queries met that are not core functions (diagnostics) */
uint32_t rx_sound_misses(void);
/* The player's selected camera is the first-person camera with a valid pose (0068bfb0 / the camera sound effect's active test); 1 / 0. */
uint32_t rx_sound_first_person_view(rx_addr player);
