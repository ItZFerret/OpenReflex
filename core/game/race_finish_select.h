#pragma once
#include "rx_game.h"

/* Candidate only. All five original entries RET0; none starts a scenario. */
uint32_t rx_finish_select_advance(rx_addr scenario, uint32_t series_kind); /*0074a070 EAX=sc,ECX=kind*/
uint32_t rx_finish_select_next(rx_addr scenario);                        /*0074a110 ECX=sc,vf3*/
uint32_t rx_finish_select_previous(rx_addr scenario);                    /*0074f0f0 ECX=sc,vf13*/
void rx_finish_select_local_flags(rx_addr scenario);                     /*0074f1a0 EAX=sc*/
uint32_t rx_finish_select_cancel(rx_addr scenario);                       /*0074fad0 ECX=sc,vf4*/
