#pragma once
#include "rx_game.h"
void rx_replay_start(int32_t type); /*006aa8d0*/
void rx_replay_begin_clip(int32_t slot); /*006aaa30*/
void rx_replay_setup_clip(rx_addr clip); /*006a9f30, source authored; review required*/
void rx_replay_next_clip(void); /*006ab730*/
void rx_replay_fade_in(void); /*006ab6d0, original Oracle effect / required product host*/
void rx_replay_fade_out(void); /*006aaab0, original Oracle effect / required product host*/
void rx_replay_play_event(rx_addr event, rx_addr clip);
void rx_replay_playback_tick(rx_addr clip, float dt);
/* Product-owned presentation boundary must acknowledge actual viewport state;
 * missing provider is a hard failure. Original00757660 isn't approximated. */
void rx_replay_require_viewport(rx_addr game, int32_t count);
