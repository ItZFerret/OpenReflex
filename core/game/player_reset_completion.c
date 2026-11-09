/* Complete00550860, camera-directed end-pose placement; no original-code fallback. */
#include "player_reset_completion.h"
#include "fx_util.h"
#include "bike_sim.h"
#include "build_vehicle.h"
#include <stdlib.h>
static rx_addr camera_matrix(rx_addr camera,rx_addr ret){
 typedef rx_addr(*fn)(rx_addr);rx_addr target=U32(U32(camera,0),0x38);
 fn f=(fn)rx_fn(target);if(!f)abort();
 fx_set_reg(FX_ECX,camera);fx_set_reg(FX_EAX,U32(camera,0));fx_set_reg(FX_EDX,target);
 rx_addr saved=fx_push(0,0,ret);rx_addr result=f(camera);fx_pop(saved);return result;
}
#ifdef RX_RESET_COMPLETION_TEST
static unsigned calls;
unsigned rx_reset_completion_calls(void){return calls;}
#endif
void rx_player_reset_completion(rx_addr p){
#ifdef RX_RESET_COMPLETION_TEST
 ++calls;
#endif
 fx_frame frame;fx_frame_enter(&frame,0x58,16,FX_ESI,FX_EDI);const rx_addr b=frame.E;
 fx_set_reg(FX_ESI,p);
 rx_addr m=camera_matrix(U32(p,0x220),0x55087a);
 for(unsigned i=0;i<3;++i)SU32(b+0x20,4*i,U32(m,0x30+4*i));
 m=camera_matrix(U32(p,0x220),0x5508a8);
 for(unsigned i=0;i<3;++i)SU32(b+0x10,4*i,U32(m,0x20+4*i));
 rx_addr vehicle=U32(p,0x1ac),sim=U32(vehicle,0x234);
 fx_set_reg(FX_EDI,sim);rx_addr saved=fx_push(0,0,0x5508da);bike_end_pose_override(sim);fx_pop(saved);
 vehicle=U32(p,0x1ac);fx_set_reg(FX_ESI,vehicle);sim=U32(vehicle,0x234);
 typedef void(*place_fn)(rx_addr,rx_addr,rx_addr,rx_addr,rx_addr,rx_addr);
 rx_addr target=U32(U32(sim,0),0x50);place_fn place=(place_fn)rx_fn(target);if(!place)abort();
 const uint32_t args[5]={b+0x20,b+0x10,b+0x40,b+0x30,b+0x50};
 fx_set_reg(FX_ECX,sim);fx_set_reg(FX_EDX,target);fx_set_reg(FX_EAX,b+0x20);
 saved=fx_push(5,args,0x550906);place(sim,args[0],args[1],args[2],args[3],args[4]);fx_pop(saved);
 const uint32_t a[3]={vehicle,b+0x40,b+0x30};
 fx_set_reg(FX_EAX,b+0x30);fx_set_reg(FX_ECX,b+0x50);
 saved=fx_push(3,a,0x55091a);rx_vehicle_place(vehicle,b+0x40,b+0x30,b+0x50);fx_pop(saved);
 fx_frame_leave(&frame);
}
RX_FUNCTION(0x00550860,rx_player_reset_completion)
