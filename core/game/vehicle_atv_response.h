#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_atv_contact_response(rx_addr s, float dt); /* 0048d570 ECX=S, unused stack argument, RET4 */
void rx_atv_limit_wheel_slip(rx_addr s); /* 0048ef80 EDI=S, RET0 */
uint32_t rx_atv_all_contacts(rx_addr s); /* 0048ee30 ECX=S, RET0, EAX */
uint32_t rx_atv_all_contact_frames(rx_addr s); /* 0048ee60 ECX=S, RET0, EAX */
rx_addr rx_atv_contact_normal(rx_addr s,rx_addr out); /* 0048eed0 ECX=S, RET4, EAX=out */
#ifdef __cplusplus
}
#endif
