#include "career_property_bridge.h"
#ifndef RX_ORACLE
#include "build_activity_util.h"
#include "race_misc.h"
uint32_t rx_career_property_int(rx_addr key,uint32_t initial){ACT_FRAME(F,4,0);SU32(F,0,initial);(void)rx_owned_property_get_int(key,F,1);uint32_t v=U32(F,0);ACT_END(F);return v;}
void rx_career_property_string(rx_addr ui,rx_addr key,rx_addr out,uint32_t cap){(void)ui;(void)rx_owned_property_get_string(key,out,cap);}
void rx_career_offline_session_metadata(rx_addr ui){
 (void)ui;uint32_t mode=rx_game_player_mode();
 if(mode>1||U32(0xda6cb0,0)!=0)act_unreached(0x59cd40,"career presence requires offline stand-in, online session unclosed");
 /*0059cd40 only sends presence keys through PCSession/NetworkSession vf12;
  * both actual online effects are gated by+1c4c. The current product has no
  * PCSession. No PlayerDB/activity/earned progress mutation is omitted here.*/
}
#endif
