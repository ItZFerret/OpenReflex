#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_wake_append_center(rx_addr array,uint32_t x,uint32_t z); /*007f9510 EAX=array,stack x/z RET8*/
void rx_wake_append_float(rx_addr array,uint32_t bits); /*007f9550 EAX=array,XMM0=value RET0*/
void rx_wake_add_quad(rx_addr wake,rx_addr c,rx_addr d,rx_addr a,rx_addr b,rx_addr velocity,uint32_t strength,float dt); /*007ec280 EDI=wake,ECX=c,EAX=d,stack a/b/velocity/strength/dt RET20*/
void rx_wake_add_splat(rx_addr wake,rx_addr point,uint32_t strength,float size); /*007ecd80 ESI=wake,XMM0=size,stackpoint/strength RET8*/
void rx_wake_add_guarded_splat(rx_addr set,rx_addr point,uint32_t strength,float size); /*007efed0 EAX/ECX/XMM0,stackstrength RET4*/
#ifdef __cplusplus
}
#endif
