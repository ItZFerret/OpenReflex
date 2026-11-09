#pragma once
#include "build_activity_util.h"
void rx_replay_rider_reset(rx_addr rider); /* 0061d270 EAX */
void rx_replay_restore_wreck(rx_addr player, uint32_t state); /* 0054edc0 EAX/EDX */
void rx_replay_select_camera(rx_addr driver, rx_addr world); /* 007676f0 EAX/ESI */
void rx_replay_restore_model_states(rx_addr player, rx_addr registry, rx_addr values); /*0047be80 EAX/EDX/ESI*/
void rx_replay_world_section_a(rx_addr entity, uint32_t state); /*006c0c80 ESI*/
void rx_replay_world_section_b(rx_addr entity, uint32_t state); /*006c1110 ESI*/
