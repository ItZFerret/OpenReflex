#pragma once
#include "rx_game.h"
/* StartPlayback006aa8d0 is intentionally absent: full pose/tick path still open. */
int32_t rx_clip_recorder_find_playback_type(int32_t type); /* 006aaee0 EDX type */
void rx_clip_recorder_clear_follow(rx_addr activity); /* 006ac3f0 ESI activity */
void rx_clip_recorder_stop_playback(void); /* 006aa990 cdecl0 */
