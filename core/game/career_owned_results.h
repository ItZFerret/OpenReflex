#pragma once
#include "rx_game.h"
#ifndef RX_ORACLE
/* Bind only the owning session's actual series manager; clear before teardown.
 * No result notification is manufactured by bind or dispatch. */
void rx_career_owned_results_bind(rx_addr manager);
void rx_career_owned_results_clear(void);
/* The original typed "Show Activity Results" after the canonical dispatch 004020c0 (every activity class raises it there). */
void rx_career_owned_results_dispatch(rx_addr channel, rx_addr name, rx_addr payload, uint32_t bytes);
#endif
