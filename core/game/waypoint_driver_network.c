/*Actual offline semantics of inherited NetworkDriver leaves. No remote payload
 * support is inferred: any genuine network object is explicitly rejected.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "race_driver.h"
#include "loop_player.h"
void rx_waypoint_network_vf14(rx_addr driver){ACT_FRAME_EBP(F,8,4,3);race_static_dcf304();if(U32(0,0xda6d08)||U32(driver,0xef0))act_unreached(0x50ec30,"NetworkDriver publish/ghost reset requires real network ownership");if(!U8(0,0x9df580)){ACT_CALL(0,rx_lock_enter(0xda6cf0));ACT_CALL(0,rx_lock_leave(0xda6cf0));}ACT_END(F);}
RX_FUNCTION(0x0050ec30,rx_waypoint_network_vf14)
void rx_waypoint_network_vf15(rx_addr driver){SU32(driver,0xefc,1);}
RX_FUNCTION(0x0050e500,rx_waypoint_network_vf15)
void rx_waypoint_driver_vf17(rx_addr driver){
 rx_addr network_player=U32(U32(driver,0x74),0xb2c);
 if(network_player&&((U8(network_player,0x128)&8)||!U8(network_player,0x138))){if(!U32(driver,0x70)){SU32(driver,0xf10,0);SU32(driver,0x10b0,U32(0,0x977494));}SU32(driver,0x10d4,0);}
 rx_waypoint_network_vf14(driver); /*original tail JMP uses same guest entrySP*/
}
RX_FUNCTION(0x00518870,rx_waypoint_driver_vf17)
