/* Original private frames for base enter/leave-rest, not a second rest
 * semantic owner. The canonical bike_sim functions compute decisions once;
 * explicit notifications below retain only original structural effects. */
#include "fourwheel_rest_frames.h"
#include "fourwheel_rest_callbacks.h"
#include "bike_sim.h"
#include "fx_util.h"
#include "phys_crt.h"
#include <stdlib.h>
typedef struct rest_frame {
 struct rest_frame* previous;rx_addr self,b;unsigned leave,call_index,stopped,normal_called,impulse;
} rest_frame;
static _Thread_local rest_frame* current;
void* rx_fourwheel_rest_frames_bind(void*p){void*old=current;current=(rest_frame*)p;return old;}
int rx_fourwheel_rest_frames_active(void){return current!=NULL;}
static rest_frame* active(unsigned leave){if(!current||current->leave!=leave)abort();return current;}
static unsigned actual_family(rx_addr s){uint32_t vt=U32(s,0);if(vt==0x93d24c)return 1;if(vt==0x93d43c||vt==0x93c93c)return 0;abort();}
static void unknown(rx_addr target,rx_addr caller,int argc,const uint32_t*args){
 /* Existing missing-function diagnostics at caller depth, never a guessed
  * callback value or arbitrary LTCG function cast. Product cannot run bytes. */
 rx_regs in=fx_regs(),out;for(unsigned i=0;i<8;i++)in.r[i]=fx_reg((int)i);
 rx_set_sp(caller);rx_call_original_ex(target,&in,&out,argc,args);abort();
}
uint32_t rx_fourwheel_rest_frames_vcall(rx_addr self,uint32_t slot,int argc,const uint32_t*args){
 if(!current||current->self!=self)abort();rest_frame*c=current;unsigned family=actual_family(self);rx_addr b=c->b,entry;uint32_t ra;
 if(c->leave){if(slot!=0x78||argc||c->call_index++)abort();ra=0x432373;}
 else if(c->call_index==0){if(slot!=0x74||argc)abort();++c->call_index;ra=0x43253e;}
 else if(c->call_index==1){if(slot!=0x78||argc)abort();++c->call_index;ra=0x432549;fx_set_reg(FX_EDI,fx_reg(FX_EAX));}
 else{if(c->call_index!=2||slot!=0x7c||argc!=1||!args||args[0]!=b+0x30)abort();++c->call_index;c->normal_called=1;ra=0x432636;}
 uint32_t vt=U32(self,0),target=U32(vt,slot);fx_set_reg(FX_EAX,vt);fx_set_reg(FX_EDX,target);fx_set_reg(FX_ECX,self);
 if(argc){SU32(b-4,0,args[0]);entry=b-8;}else entry=b-4;
 SU32(entry,0,ra);rx_set_sp(entry);uint32_t value;
 if(slot==0x74&&target==(family?0x4b40e0u:0x4ce8e0u))value=family?rx_buggy_rest_all_contacts(self):rx_utv_truck_rest_all_contacts(self);
 else if(slot==0x78&&target==(family?0x4b4110u:0x4ce910u))value=family?rx_buggy_rest_all_markers(self):rx_utv_truck_rest_all_markers(self);
 else if(slot==0x7c&&target==(family?0x4b4180u:0x4ce980u))value=family?rx_buggy_rest_normal(self,args[0]):rx_utv_truck_rest_normal(self,args[0]);
 else{unknown(target,b,argc,args);abort();}
 if(rx_sp()!=entry)abort();fx_set_reg(FX_EAX,value);rx_set_sp(b);return value;
}
void rx_fourwheel_rest_frames_speed(float speed,float elapsed){rest_frame*c=active(0);if(c->call_index)abort();SF32(c->b,0x1c,speed);SF32(c->b,0x18,elapsed);}
void rx_fourwheel_rest_frames_impulse(int above_limit){rest_frame*c=active(1);uint32_t mask=above_limit?UINT32_MAX:0;for(unsigned i=0;i<4;i++)SU32(c->b,0x10+4*i,mask);fx_set_reg(FX_ECX,mask);fx_set_reg(FX_EDX,mask+mask+mask);c->impulse=above_limit!=0;}
void rx_fourwheel_rest_frames_stopped(void){rest_frame*c=active(0);if(c->call_index!=2)abort();c->stopped=1;}
rx_addr rx_fourwheel_rest_frames_normal_output(void){rest_frame*c=active(0);if(c->call_index!=2)abort();return c->b+0x30;}
void rx_fourwheel_rest_frames_normal_result(int above_limit){rest_frame*c=active(0);if(!c->normal_called)abort();uint32_t mask=above_limit?UINT32_MAX:0;for(unsigned i=0;i<4;i++)SU32(c->b,0x20+4*i,mask);}
static void canonical_constants(void){
 /* These immutable game constants are literal inputs of the canonical owner;
  * changed pages require a separate semantic contract, never silent tuning. */
 if(U32(0,0x931f24)!=rx_fb(1.f)||U32(0,0x9322cc)!=rx_fb(.5f)||U32(0,0x92fe48)!=rx_fb(3.f)||U32(0,0x977770)!=rx_fb(.9962f))abort();
}
uint32_t rx_fourwheel_enter_rest_frame(rx_addr self){
 actual_family(self);canonical_constants();fx_frame f;fx_frame_enter(&f,0x44,16,FX_EBX,FX_ESI,FX_EDI);rx_addr b=f.E;
 rest_frame c={current,self,b,0,0,0,0,0};current=&c;
 SU32(b,0x4c,U32(0,0x9b45e4)^(b+12));for(unsigned i=0;i<4;i++)SU32(b,0x20+4*i,U32(0,0x977770));fx_set_reg(FX_ESI,self);fx_set_reg(FX_EBX,0);
 uint32_t result=bike_sim_enter_rest_test(self);if(c.call_index<2||c.call_index>3)abort();fx_set_reg(FX_EAX,result);
 /* Original pops EDI/ESI/EBX before its cookie call. The call's return word
  * overwrites the consumed EBX save; never remirror saved words afterward. */
 fx_set_reg(FX_EDI,f.in.edi);fx_set_reg(FX_ESI,f.in.esi);fx_set_reg(FX_EBX,f.in.ebx);rx_set_sp(b+12);
 fx_set_reg(FX_ECX,U32(b,0x4c)^(b+12));SU32(b,8,c.stopped?0x432620:0x432673);rx_set_sp(b+8);phys_security_check_cookie(fx_reg(FX_ECX));if(rx_sp()!=b+8)abort();
 current=c.previous;fx_frame_leave(&f);return result;
}
uint32_t rx_fourwheel_leave_rest_frame(rx_addr self){
 actual_family(self);canonical_constants();fx_frame f;fx_frame_enter(&f,0x1c,16,FX_ESI);rest_frame c={current,self,f.E,1,0,0,0,0};current=&c;fx_set_reg(FX_ESI,self);
 uint32_t result=bike_sim_leave_rest_test(self);if(c.call_index!=1)abort();fx_set_reg(FX_ECX,c.impulse);fx_set_reg(FX_EAX,result);current=c.previous;fx_frame_leave(&f);return result;
}
