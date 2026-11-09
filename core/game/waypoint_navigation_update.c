/*0052e0c0 full physical CFG through0052e832, not the truncated function-metadata span.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
extern void rx_waypoint_probe_reset(rx_addr probe,rx_addr origin);
extern void rx_waypoint_probe_advance(rx_addr probe,rx_addr direction);
extern void rx_waypoint_navigation_steer(rx_addr object);
extern uint32_t rx_waypoint_navigation_seek(rx_addr object); /*00536080*/
extern rx_addr rx_waypoint_navigation_follow(rx_addr object); /*00535b20*/
extern uint32_t rx_waypoint_navigation_follow_valid(rx_addr object); /*00536c30*/
extern uint32_t rx_waypoint_navigation_reselect(rx_addr object); /*0052e060*/
static float dot2(float x,float z,float a,float b){return rx_adds(rx_muls(z,b),rx_muls(x,a));}
static void normalize3(rx_addr p,uint32_t mode){float x=F32(p,0),y=F32(p,4),z=F32(p,8);float n=mode==4?rx_adds(rx_adds(rx_muls(z,z),rx_muls(y,y)),rx_muls(x,x)):mode==5?rx_adds(rx_adds(rx_muls(x,x),rx_muls(z,z)),rx_muls(y,y)):rx_adds(rx_adds(rx_muls(x,x),rx_muls(y,y)),rx_muls(z,z));if(n>0){float root;ACT_CALLR(root,1,rx_sqrts(rx_f80_to_f32(rx_f80_from_f32(n),0)));float inv=rx_divs(F32(0,0x931f24),root);SF32(p,0,rx_muls(inv,x));SF32(p,4,rx_muls(inv,y));SF32(p,8,rx_muls(z,inv));}}
uint32_t rx_waypoint_navigation_update(rx_addr o,float dt){
 (void)dt;ACT_FRAME_EBP(F,16,0x94,3);if(!U32(o,4)){ACT_END(F);return 0;}rx_addr v=U32(U32(U32(o,8),0x74),0x1ac),probe=o+0xf0;SU32(F,0x30,U32(v,0xc0));SU32(F,0x34,U32(v,0xc4));SU32(F,0x38,U32(v,0xc8));SU32(F,0x4c,probe);
 if(!U32(o,0x108))ACT_CALL(1,rx_waypoint_probe_advance(probe,v+0x130));uint32_t changed;
 switch(U32(o,0xc)){
 case 0:ACT_CALLR(changed,0,rx_waypoint_navigation_seek(o));if(changed)SU32(o,0xc,1);break;
 case 1:{rx_addr line;ACT_CALLR(line,0,rx_waypoint_navigation_follow(o));if(!line)SF32(o,0xe0,rx_adds(F32(U32(o,8),0x130),F32(o,0xdc)));else{ACT_CALLR(changed,0,rx_waypoint_navigation_follow_valid(o));if(changed)break;}SU32(o,0xa4,0);SU32(o,0xa8,0);SU32(o,0xc,2);break;}
 case 2:ACT_CALLR(changed,0,rx_waypoint_navigation_reselect(o));if(changed)break;if(U32(o,0x108)){if(U32(o,0x104)){SU32(o,0xc,5);ACT_CALL(0,rx_waypoint_probe_reset(o+0x130,F+0x30));ACT_CALL(0,rx_waypoint_probe_reset(o+0x170,F+0x30));}ACT_CALL(0,rx_waypoint_probe_reset(probe,F+0x30));}ACT_CALL(0,rx_waypoint_navigation_steer(o));break;
 case 3:case 4:{uint32_t side=U32(o,0xc);ACT_CALLR(changed,0,rx_waypoint_navigation_reselect(o));if(changed)break;rx_addr d=U32(o,8);if(U32(o,0x104))SF32(d,0x60,side==3?rx_subs(F32(d,0x60),F32(0,0x931f24)):rx_adds(F32(d,0x60),F32(0,0x931f24)));uint32_t args=F+0x60;act_vcall(U32(U32(d,0x74),0x1ac),0x3c,1,&args);
  SU32(F,0x20,side==3?U32(F,0x60):rx_fb(rx_subs(-0.0f,F32(F,0x60))));SU32(F,0x24,U32(F,0x64));SU32(F,0x28,side==3?U32(F,0x68):rx_fb(rx_subs(-0.0f,F32(F,0x68))));normalize3(F+0x20,side);
  if(U32(o,0x108))ACT_CALL(0,rx_waypoint_probe_reset(probe,F+0x30));rx_addr scan=o+(side==3?0x170:0x130);if(!U32(scan,0x18))ACT_CALL(1,rx_waypoint_probe_advance(scan,F+0x20));if(!U32(scan,0x18))break;if(!U32(scan,0x14)&&!U32(o,0x104))SU32(o,0xc,2);ACT_CALL(0,rx_waypoint_probe_reset(scan,F+0x30));break;}
 case 5:{ACT_CALLR(changed,0,rx_waypoint_navigation_reselect(o));if(changed)break;uint32_t args=F+0x60;act_vcall(U32(U32(U32(o,8),0x74),0x1ac),0x3c,1,&args);
  SF32(F,0x50,rx_subs(-0.0f,F32(F,0x60)));SU32(F,0x54,U32(F,0x64));SF32(F,0x58,rx_subs(-0.0f,F32(F,0x68)));SU32(F,0x20,U32(F,0x60));SU32(F,0x24,U32(F,0x64));SU32(F,0x28,U32(F,0x68));normalize3(F+0x50,5);normalize3(F+0x20,5);
  if(!U32(o,0x148))ACT_CALL(1,rx_waypoint_probe_advance(o+0x130,F+0x50));if(!U32(o,0x188))ACT_CALL(1,rx_waypoint_probe_advance(o+0x170,F+0x20));v=U32(U32(U32(o,8),0x74),0x1ac);float x=F32(v,0x130),z=F32(v,0x138),n=dot2(x,z,x,z);SF32(F,0x40,x);SF32(F,0x44,z);if(n!=0){float length;ACT_CALLR(length,1,rx_sqrts(rx_f80_to_f32(rx_f80_from_f32(n),0)));float inv=rx_divs(F32(0,0x931f24),length);x=rx_muls(inv,F32(F,0x40));z=rx_muls(F32(F,0x44),inv);}float dx=rx_subs(F32(o,0x10),F32(F,0x30)),dz=rx_subs(F32(o,0x18),F32(F,0x38));
  if((!U32(o,0x188)&&!U32(o,0x148))||dot2(dx,dz,x,z)<F32(0,0x9322cc)*-1.0f){ACT_CALL(0,rx_waypoint_navigation_steer(o));break;}
  float left=F32(0,0x977494),right=left;if(U32(o,0x144)){float xx=rx_subs(F32(o,0x150),F32(F,0x30)),yy=rx_subs(F32(o,0x154),F32(F,0x34)),zz=rx_subs(F32(o,0x158),F32(F,0x38));left=rx_adds(rx_adds(rx_muls(zz,zz),rx_muls(yy,yy)),rx_muls(xx,xx));}if(U32(o,0x184)){float xx=rx_subs(F32(o,0x190),F32(F,0x30)),yy=rx_subs(F32(o,0x194),F32(F,0x34)),zz=rx_subs(F32(o,0x198),F32(F,0x38));right=rx_adds(rx_adds(rx_muls(zz,zz),rx_muls(yy,yy)),rx_muls(xx,xx));}
  uint32_t side=0;if(!(dot2(dx,dz,F32(F,0x20),F32(F,0x28))<0)){if(left>F32(0,0x9cc37c))side=3;else if(right>F32(0,0x9cc37c))side=4;}else{if(right>F32(0,0x9cc37c))side=4;else if(left>F32(0,0x9cc37c))side=3;}
  if(side)SU32(o,0xc,side);ACT_CALL(0,rx_waypoint_probe_reset(side?probe:o+0x130,F+0x30));ACT_CALL(0,rx_waypoint_probe_reset(side==4?o+0x130:o+0x170,F+0x30));break;}
 default:break;
 }
 ACT_END(F);return 0;
}
RX_FUNCTION(0x0052e0c0,rx_waypoint_navigation_update)
