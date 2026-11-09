/* race_stunt.h -- MXActivityRace's stunt event handlers (the stunts channel 0x00a94304). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_race_physics_stunt_begin(rx_addr act, rx_addr player, rx_addr desc, uint32_t type);   /* 00565710 "PhysicsStuntBegin" */
void rx_race_stunt_begin(rx_addr act, rx_addr player, rx_addr desc);                         /* 00565770 "StuntBegin" */
void rx_race_stunt_failed(rx_addr act, rx_addr player, rx_addr desc);                        /* 005657d0 "StuntFailed" */
#ifdef __cplusplus
}
#endif
