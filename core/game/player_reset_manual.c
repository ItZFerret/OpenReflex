/*00550400 manual reset: saved spot, activity point, then vehicle fallback. */
#include "player_reset_manual.h"
#include "race.h"
#include "race_extern.h"
#include "phys_vec.h"
static void manual_axes(rx_addr F,int saved) {
 float r[8]={0};
 if(saved) {
  r[3]=F32(F,0x10);r[7]=F32(F,0x14);r[1]=F32(F,0x18);r[4]=F32(F,0x1c);r[2]=F32(0,0x92e330);
  r[0]=r[7];r[1]=phys_mul(r[1],r[2]);r[0]=phys_mul(r[0],r[2]);r[5]=phys_mul(r[3],r[2]);r[2]=phys_mul(r[1],r[7]);r[6]=phys_mul(r[4],r[5]);r[7]=phys_mul(r[3],r[5]);SF32(F,0xc,r[7]);
  r[7]=phys_mul(r[4],r[1]);r[5]=phys_sub(phys_mul(r[3],r[0]),r[7]);r[7]=F32(0,0x931f24);SF32(F,0x30,r[5]);
  r[5]=phys_add(phys_mul(r[1],F32(F,0x18)),F32(F,0xc));r[4]=phys_mul(r[4],r[0]);r[0]=phys_add(phys_mul(r[0],F32(F,0x14)),F32(F,0xc));r[3]=phys_mul(r[3],r[1]);r[1]=F32(0,0x931f24);r[7]=phys_sub(r[7],r[5]);r[5]=phys_add(r[2],r[6]);r[4]=phys_add(r[4],r[3]);r[2]=phys_sub(r[2],r[6]);r[1]=phys_sub(r[1],r[0]);
  SF32(F,0x38,r[5]);SF32(F,0x20,r[4]);SF32(F,0x24,r[2]);SF32(F,0x28,r[1]);
 } else {
  r[5]=F32(F,0x10);r[6]=F32(F,0x14);r[1]=F32(F,0x18);r[7]=F32(F,0x1c);r[3]=F32(0,0x92e330);
  r[0]=phys_mul(r[6],r[3]);r[1]=phys_mul(r[1],r[3]);r[2]=phys_mul(r[5],r[3]);r[4]=r[2];r[2]=phys_mul(r[2],r[5]);SF32(F,0xc,r[2]);r[3]=phys_mul(r[1],r[6]);r[2]=phys_mul(r[0],r[5]);r[6]=phys_mul(r[1],r[7]);r[2]=phys_sub(r[2],r[6]);r[4]=phys_mul(r[4],r[7]);SF32(F,0x30,r[2]);r[2]=F32(0,0x931f24);
  r[6]=phys_add(phys_mul(r[1],F32(F,0x18)),F32(F,0xc));r[7]=phys_sub(r[2],r[6]);r[6]=phys_add(r[3],r[4]);SF32(F,0x38,r[6]);r[6]=r[0];r[0]=phys_mul(r[0],F32(F,0x14));r[6]=phys_mul(r[6],F32(F,0x1c));r[0]=phys_add(r[0],F32(F,0xc));r[1]=phys_mul(r[1],r[5]);r[6]=phys_add(r[6],r[1]);r[3]=phys_sub(r[3],r[4]);r[2]=phys_sub(r[2],r[0]);SF32(F,0x20,r[6]);SF32(F,0x24,r[3]);SF32(F,0x28,r[2]);
 }
 SF32(F,0x34,r[7]);
}
void rx_player_reset_manual(rx_addr p) {
 RACE_FRAME_EBP(F,16,0x50,0);
 if(!(F32(p,0x116c)>F32(0,0x92e12c))) {
  int make_axes=1,saved=U32(p,0x1130)!=0&&U32(p,0x1138)!=0;
  if(saved){rx_st4(F+0x40,rx_ld4(p+0x1140));rx_st4(F+0x10,rx_ld4(p+0x1150));}
  else {
   rx_addr a=U32(p,0x94);
   if(race_thiscall(race_vfn(a,19),a,4,p,F+0x40,F+0x20,F+0x30))make_axes=0;
   else {rx_addr v=U32(p,0x1ac);rx_st4(F+0x40,rx_ld4(v+0x200));rx_st4(F+0x10,rx_ld4(v+0x210));}
  }
  if(make_axes)manual_axes(F,saved);
  RACE_CORE_CALL(1,rx_player_reset_to(p,F+0x40,F+0x20));SU32(p,0x1170,1);
 }
 RACE_FRAME_END(F);
}
RX_FUNCTION(0x00550400,rx_player_reset_manual)

/*00550a20: out-of-bounds reset reason7. Same dependencies as manual reset;
 * unlike manual input it always asks the activity before using vehicle pose. */
void rx_player_reset_out_of_bounds(rx_addr p) {
 RACE_FRAME_EBP(F,16,0x50,0);
 if(!(F32(p,0x116c)>F32(0,0x92e12c))) {
  rx_addr a=U32(p,0x94);
  if(!race_thiscall(race_vfn(a,19),a,4,p,F+0x40,F+0x20,F+0x30)) {
   rx_addr v=U32(p,0x1ac);rx_st4(F+0x40,rx_ld4(v+0x200));rx_st4(F+0x10,rx_ld4(v+0x210));
   float r[8]={0};r[1]=F32(F,0x10);r[6]=F32(F,0x14);r[2]=F32(F,0x18);r[7]=F32(F,0x1c);r[3]=F32(0,0x92e330);
   r[0]=r[6];r[2]=phys_mul(r[2],r[3]);r[0]=phys_mul(r[0],r[3]);r[4]=phys_mul(r[1],r[3]);r[3]=phys_mul(r[2],r[6]);r[5]=r[4];r[6]=phys_mul(r[1],r[4]);SF32(F,0xc,r[6]);
   r[4]=phys_mul(r[1],r[0]);r[6]=phys_mul(r[2],r[7]);r[4]=phys_sub(r[4],r[6]);r[5]=phys_mul(r[5],r[7]);r[6]=phys_add(phys_mul(r[2],F32(F,0x18)),F32(F,0xc));SF32(F,0x30,r[4]);r[4]=F32(0,0x931f24);
   r[1]=phys_mul(r[1],r[2]);r[2]=r[0];r[0]=phys_mul(r[0],F32(F,0x14));r[2]=phys_mul(r[2],F32(F,0x1c));r[0]=phys_add(r[0],F32(F,0xc));r[7]=phys_sub(r[4],r[6]);r[6]=phys_add(r[3],r[5]);r[1]=phys_add(r[1],r[2]);r[3]=phys_sub(r[3],r[5]);r[4]=phys_sub(r[4],r[0]);
   SF32(F,0x34,r[7]);SF32(F,0x38,r[6]);SF32(F,0x20,r[1]);SF32(F,0x24,r[3]);SF32(F,0x28,r[4]);
  }
  RACE_CORE_CALL(1,rx_player_reset_to(p,F+0x40,F+0x20));SU32(p,0x1170,7);
 }
 RACE_FRAME_END(F);
}
RX_FUNCTION(0x00550a20,rx_player_reset_out_of_bounds)
