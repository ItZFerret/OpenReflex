/*005170d0 actual waypoint control policy. No handling constants are invented.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "race.h"
#include "race_driver.h"
#include "phys_crt.h"
#include "waypoint_avoid_dispatch.h"
static rx_addr player(rx_addr d){return U32(d,0x74);}
static rx_addr vehicle(rx_addr d){return U32(player(d),0x1ac);}
static int debug(rx_addr d){return U32(player(d),0x1cc)&&U32(0,0xae1b90);}
static float abs_lane(float x){return rx_bf(rx_fb(x)&U32(0,0x977570));}
static float axis(float x){float hi=F32(0,0x931f24),lo=F32(0,0x9cd9e8);if(x>=hi)return hi;if(x<=lo)return lo;return x;}
static float unit(float x){float hi=F32(0,0x931f24);if(x>=hi)return hi;if(x<=0)return 0;return x;}
extern void rx_waypoint_driver_line_assist(rx_addr driver);
void rx_waypoint_driver_ai_update(rx_addr d,float dt){
 ACT_FRAME_EBP(F,8,0x114,3);SU32(F,0x11c,U32(0,0x9b45e4)^(F+0xc));
 SU32(d,0x60,0);SU32(d,0x64,0);SU32(d,0x68,0);SU32(d,0x3cc,0);SU32(d,0x3c4,0);SU32(d,0x3c8,0);SU32(d,0x3dc,0);SU32(d,0x3e0,0);SU32(d,0x3d4,0);SU32(d,0x3d8,0);SU32(d,0x3d0,0);SU32(d,0x3c0,U32(0,0x931f24));
 for(uint32_t i=0;i<U32(d,0xee4);i++)waypoint_rule_update(U32(d,0xa0+i*4),0.0f);
 uint32_t i=0;for(;i<U32(d,0xee0);i++){rx_addr b=U32(d,0xe8+i*4);float value=rx_f80_to_f32(rx_f80_from_f32(dt),0);uint32_t claimed=waypoint_behavior_update(b,value);SU32(F,0xc,claimed);if(debug(d))act_vcall(U32(d,0xe8+i*4),0x20,0,0);if(U32(F,0xc))break;}
 for(i++;i<U32(d,0xee0);i++)if(debug(d))act_vcall(U32(d,0xe8+i*4),0x20,0,0);
 float steer=F32(d,0x60),low=F32(0,0x9cd9e8),high=F32(0,0x931f24);if(steer>high)steer=high;else if(steer<low)steer=low;SF32(d,0x3dc,steer);
 if(act_vcall(vehicle(d),0xac,0,0)==0)SF32(d,0x3d4,F32(d,0x3dc)*F32(0,0x9322cc));
 if(act_vcall(vehicle(d),0xac,0,0)==0){float slow=F32(d,0x64);if(slow>=F32(0,0x9cc8a8)){SU32(d,0x3c0,0);SF32(d,0x3c8,slow);SF32(d,0x3c4,slow);}else if(slow>=F32(0,0x931d94))SU32(d,0x3c0,0);}
 ACT_CALL(1,rx_waypoint_driver_line_assist(d));float lean=F32(d,0x318);
 if(lean!=0){uint32_t kind=U32(vehicle(d),0x194);if(F32(0,0x931d94)>abs_lane(F32(d,0x3dc))||(kind!=2&&kind!=4)){if(F32(vehicle(d),0x150)>F32(0,0x9cd954)&&!U32(vehicle(d),0x280)){SF32(d,0x3e0,lean);SF32(d,0x3d8,lean);}}else{SU32(d,0x3e0,0);SU32(d,0x3d8,0);}}
 rx_addr al;ACT_CALLR(al,1,rx_driver_air_leveling_behavior(d));
 if(al&&F32(d,0x318)==0){if(F32(al,0x58)>F32(d,0x130)){SU32(d,0x3e0,U32(0,0x9cd9e8));SU32(d,0x3c8,0);SU32(d,0x3c4,0);SU32(d,0x3d8,U32(0,0x9cd9e8));}
  else if(F32(d,0x318)==0){uint32_t kind=U32(vehicle(d),0x194);if(kind==0||kind==1){SF32(d,0x3d8,F32(al,0x40)+F32(d,0x3d8));SF32(d,0x3dc,F32(al,0x44)+F32(d,0x3dc));if(U32(al,0x4c)){SU32(d,0x3dc,0);SU32(d,0x3d4,0);}if(act_vcall(vehicle(d),0xac,0,0)==0){float magnitude=abs_lane(F32(d,0x3dc));if(!(high>magnitude))magnitude=high;uint32_t index=U32(player(d),0x8f4)+(U32(d,0x328)*3+U32(d,0x32c))*4;float bias=F32(0,0x9e4478+index*4)*magnitude;SF32(d,0x3e0,bias+F32(d,0x3e0));SF32(d,0x3d8,F32(0,0x9e4478+index*4)*magnitude+F32(d,0x3d8));}}
   else if(U32(al,0x4c)){SU32(d,0x3dc,0);SU32(d,0x3d4,0);}}}
 rx_addr pace;ACT_CALLR(pace,1,rx_waypoint_driver_find_behavior22(d));
 if(pace&&F32(d,0x3c0)==high&&F32(d,0x3c4)==0&&F32(pace,0x18)!=high){uint32_t kind=U32(vehicle(d),0x194);if(kind==0||kind==1)SF32(d,0x3c0,F32(pace,0x18));else if(F32(vehicle(d),0x150)>F32(0,0x9cd964)){SF32(d,0x3c0,F32(pace,0x18));SU32(d,0x3c8,U32(0,0x9d3bfc));SU32(d,0x3c4,U32(0,0x9d3bfc));}}
 ACT_CALL(0,rx_race_limit_throttle_in_turns(d,d+0x3c0));rx_addr suck;ACT_CALLR(suck,1,rx_driver_suck_racing_behavior(d));if(suck&&U32(suck,0x20)){SU32(d,0x3dc,0);SU32(d,0x3d4,0);}
 float t=F32(d,0x134);if(t>F32(0,0x9322a4)&&F32(vehicle(d),0x150)>F32(0,0x9cd96c)&&F32(0,0x935890)>t){rx_addr fix;ACT_CALLR(fix,1,rx_driver_fix_progress_behavior(d));if(fix){SU32(fix,0x18,0);SU32(fix,0x54,0);SU32(fix,0x58,0);SU32(fix,0x20,0);SU32(U32(fix,8),0x3cc,0);}int32_t count=(int32_t)(12u-U32(d,0x78));SU32(F,0xc,(uint32_t)count);rx_f80 boost=rx_f80_from_int(count);if(count<0)boost=rx_f80_add(boost,rx_f80_from_f32(F32(0,0x97731c)),53,0);boost=rx_f80_mul(boost,rx_f80_from_f32(F32(0,0x9cd970)),53,0);boost=rx_f80_div(boost,rx_f80_from_int(I32(U32(player(d),0x94),0x26c)),53,0);boost=rx_f80_add(boost,rx_f80_from_f32(F32(0,0x931d94)),53,0);boost=rx_f80_mul(boost,rx_f80_from_f32(F32(d,0x3c0)),53,0);SF32(d,0x3c0,rx_f80_to_f32(boost,0));}
 if(U32(d,0x32c)==2)SU32(d,0x3d8,0);
 SF32(d,0x3dc,axis(F32(d,0x3dc)));SF32(d,0x3e0,axis(F32(d,0x3e0)));SF32(d,0x3d4,axis(F32(d,0x3d4)));SF32(d,0x3d8,axis(F32(d,0x3d8)));SF32(d,0x3c0,unit(F32(d,0x3c0)));SF32(d,0x3c4,unit(F32(d,0x3c4)));SF32(d,0x3c8,unit(F32(d,0x3c8)));
 if(U32(d,0x32c)!=1)ACT_CALL(0,rx_ai_control_smooth(d,d+0x3c0,dt));if(debug(d))ACT_CALL(0,rx_free_ride_debug_draw(d));
 if(U32(player(d),0x1cc))for(int32_t j=0;j<I32(d,0xf10);j++)ACT_CALL(0,rx_lap_time_text(F+0x10,F32(d,0xf18+(uint32_t)j*4),1));
 phys_security_check_cookie(U32(F,0x11c)^(F+0xc));ACT_END(F);
}
RX_FUNCTION(0x005170d0,rx_waypoint_driver_ai_update)
