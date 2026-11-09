#pragma once
#include "career_refresh.h"
/*005b40e0: original ESI=manager, RET0. Semantic direct-call composition;
 * this is not a guessed stack-ABI adapter for the register-input original. */
void rx_career_begin_race(rx_addr manager,
    rx_career_vehicle_model_resolver vehicle_model,
    void (*ui_int)(rx_addr name,uint32_t value),
    uint32_t (*event_call)(rx_addr event,uint32_t slot,uint32_t argument));
#ifndef RX_ORACLE
/*Installed product composition only; rejects an unbound session.*/
void rx_career_composition_begin_race(rx_addr manager);
#endif
