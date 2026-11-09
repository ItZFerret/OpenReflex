#pragma once
#include "rx_game.h"
void rx_race_results_done(rx_addr activity); /*00569be0 thiscall vf58*/
#ifndef RX_ORACLE
void rx_app_post_activity_results_page_done(void); /*00742aa0 tail dispatch*/
#endif
