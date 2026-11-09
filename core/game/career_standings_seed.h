#pragma once
#include "rx_game.h"
/*005b6370: original stack(manager,series),RET8. Removes only this SeriesId. */
void rx_career_clear_standings(rx_addr manager,int32_t series);
/*005b4d30: original ECX=manager,stack series,RET4. Clear, then seed every
 * actual selector2 player row in its existing physical row order. */
void rx_career_seed_standings(rx_addr manager,int32_t series);
