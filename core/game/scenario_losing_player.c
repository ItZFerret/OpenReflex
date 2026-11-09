/* Complete0074b6e0..0074b745: the nth locally controlled human outside the
 * first three finish places. No fabricated player or podium result. */
#include "scenario_losing_player.h"
#include "rider_cap.h"
rx_addr rx_scenario_cast_losing_human(rx_addr cast,uint32_t index) {
 const rx_addr activity=U32(cast,0x40);
 if(!activity)return 0;
 const int32_t count=I32(activity,0x26c);
 uint32_t eligible=0;
 for(int32_t place=3;place<count;place++){
  const rx_addr driver=rx_stand(activity, (uint32_t)place);
  const rx_addr player=U32(driver,0x74);
  if(U32(player,4)!=0&&U32(player,0x10)==0){
   if(eligible==index)return U32(rx_stand(activity, (uint32_t)place),0x74);
   eligible++;
  }
 }
 return 0;
}
RX_FUNCTION(0x0074b6e0,rx_scenario_cast_losing_human)
