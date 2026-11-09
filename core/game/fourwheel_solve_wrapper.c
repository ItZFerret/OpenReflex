/* Exact67B vf12 wrappers, no registration/activation. */
#include "fourwheel_solve_wrapper.h"
#include "fourwheel_pair_solve.h"
#include "fourwheel_active_solve.h"
#include "fx_util.h"
#include <stdlib.h>
static void solve(rx_addr s,float dt,int buggy){
 fx_frame frame;fx_frame_enter(&frame,0xc,16,FX_EDI);rx_addr b=frame.E;fx_set_reg(FX_EDI,s);
 rx_addr entry=b-8;
 int active=U32(s,buggy?0x86f0:0x8700)!=0;
 if(active){
  /* Genuine FLD/FSTP roundtrip from caller stackdt, only on active branch. */
  fx_x87_copy(b-4,frame.ebp+8);SU32(entry,0,buggy?0x4b7551:0x4d1d01);rx_set_sp(entry);
  float rounded=F32(b-4,0);if(buggy)rx_buggy_active_solve_draft(s,rounded);else rx_utv_truck_active_solve_draft(s,rounded);
 }else{
  SU32(b-4,0,s);SU32(entry,0,buggy?0x4b755e:0x4d1d0e);rx_set_sp(entry);
  if(buggy)rx_buggy_pair_angular_solve(s);else rx_utv_truck_pair_angular_solve(s);
 }
 /* Typed leaves return at their actual entry; originalRET4 is modeled here.
  * Never normalize unexpected SP drift or claim an unavailable helper returns. */
 if(rx_sp()!=entry)abort();rx_set_sp(b);
 if(!active)rx_st4(s+(buggy?0x86e0:0x86f0),rx_ld4(s+0x730));
 fx_frame_leave(&frame);(void)dt;
}
void rx_utv_truck_solve_vf12_draft(rx_addr s,float dt){solve(s,dt,0);}
void rx_buggy_solve_vf12_draft(rx_addr s,float dt){solve(s,dt,1);}
