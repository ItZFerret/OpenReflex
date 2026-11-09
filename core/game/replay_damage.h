#pragma once
#include "rx_game.h"
void rx_replay_damage_deactivate(rx_addr entry,uint32_t index); /*47a830*/
void rx_replay_damage_reset_rotation(rx_addr entry,uint32_t index); /*47afb0*/
void rx_replay_damage_reset_motion(rx_addr entry,uint32_t index); /*47af30*/
void rx_replay_damage_activate(rx_addr entry,uint32_t index); /*47a630*/
void rx_replay_damage_apply(rx_addr entry,uint32_t index,uint32_t state); /*479ab0*/
void rx_replay_damage_restore(rx_addr vehicle,rx_addr manager,rx_addr values); /*47bdd0*/
