#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native functional owners. Incidental CPU/private scratch parity is not claimed. */
uint32_t rx_career_shop_is_locked(rx_addr helper,int32_t controller,int32_t product); /*004e96e0 ECX,RET8*/
void rx_career_shop_unlock(rx_addr helper,int32_t controller,int32_t product,int32_t mark_new); /*004e9820 ECX,RET12; increment is always1*/
uint32_t rx_career_shop_is_purchased(int32_t controller,int32_t asset); /*004e88a0 EAX,RET4*/
void rx_career_shop_purchase(rx_addr helper,int32_t controller,int32_t asset); /*004e8920 ECX,RET8*/
void rx_career_shop_add_credits(int32_t controller,int32_t amount); /*004e8350 EAX,RET4*/
#ifdef __cplusplus
}
#endif
