/* music_player.h -- the MusicPlayer's game state on the product (music_player.c; w-sfx). Product only (the oracle runs the originals). */
#pragma once
#include "rx_game.h"
#include "rx_sound.h"
void    rx_music_reset(void);
#ifndef RX_ORACLE
void    rx_music_tracks(rx_addr list);        /* 00677440 (the music entity + 0x90): the playlist shuffled (the game's LCG) */
void    rx_music_scores(rx_addr list);        /* 006770d0: the level's score list */
void    rx_music_jukebox(uint32_t kind);      /* 00677700 */
void    rx_music_start_score(uint32_t kind);  /* 00677750 MusicPlayer_StartScenarioMusic (one draw) */
void    rx_music_fade_out(void);              /* 00677600 MusicPlayer_FadeOut */
void    rx_music_stop(void);                  /* 00677510 */
int32_t rx_music_mode(void);
void    rx_music_tick(float dt);              /* 00676ae0 / 00676d20: the delay, the track / score played */
void    rx_music_fill(rx_sound_music* out);
#endif
