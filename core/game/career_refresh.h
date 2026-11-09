#pragma once
#include "rx_game.h"
/*00541040 must resolve both authored and dynamically built vehicle identities.
 * A made-spec-only implementation is insufficient; there is no default model. */
typedef uint32_t (*rx_career_vehicle_model_resolver)(rx_addr vehicle_list,uint32_t vehicle);
void rx_career_refresh_player_models(rx_addr manager,rx_career_vehicle_model_resolver resolve);
void rx_career_refresh_standing_models(rx_addr manager,int32_t series);
void rx_career_zero_standing_scores(rx_addr manager,int32_t series);
void rx_career_refresh_medals(rx_addr manager,int32_t series);
/*005b4210 ESI manager, stack advance, RET4. True only on the first refresh.
 * ui_int is the actual owned UI-property writer, not an ignored callback. */
uint32_t rx_career_refresh(rx_addr manager,uint32_t advance,
 rx_career_vehicle_model_resolver resolve,void (*ui_int)(rx_addr,uint32_t));
