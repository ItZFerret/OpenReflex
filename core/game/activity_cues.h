/* activity_cues.h -- the level's activity audio cues for the product's ActivityAudioLogic (activity_cues.c; w-sfx). Product only. */
#pragma once
#include "rx_game.h"
struct fmt_entity;
void rx_activity_cues_reset(void);
#ifndef RX_ORACLE
void rx_activity_cues_capture(rx_addr scene, const struct fmt_entity* rs);   /* an ActivityAudioManagerEntity's RSData (types 0x1089 .. 0x1093) */
void rx_activity_cues_bind(rx_addr logic);                                    /* ActivityAudioLogic::vf29: the current world's scene's cues */
int  rx_activity_cues_present(rx_addr logic, rx_addr name);                   /* the cue map holds the name (vf3's test) */
void rx_activity_cues_play(rx_addr logic, rx_addr name, uint32_t a, uint32_t b);   /* 005811a0's cues of the name */
void rx_activity_cues_restart(rx_addr logic);                                 /* vf32: the played flags cleared */
#endif
