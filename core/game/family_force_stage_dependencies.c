/* Complete finite family vf11 composition proposal. Unregistered until every
 * revised dependency and the composed caller pass their own gates. */
#include "fourwheel_force_stage.h"
#include "fourwheel_axle_torque.h"
#include "fourwheel_contact_forces.h"
#include "utv_force_gears.h"
#include "utv_engine.h"
#include "buggy_engine.h"
#include "truck_engine.h"
#include "fourwheel_suspension.h"
#include "tyre_prefix.h"
#include "buggy_tyre.h"
#include "truck_tyre.h"
#include "fx_util.h"
#include <stdio.h>
#include <stdlib.h>

enum Family { UTV, BUGGY, TRUCK };
enum Role { POWER, COAST, ENGINE, AXLE, SUSPENSION, CONTACT, TYRE };
typedef struct { rx_addr target,returned; unsigned family,role,pop; } Route;
/* Each actual caller-written return address is explicit. Shared truck/UTV
 * axle/contact targets retain their two distinct caller frames. */
static const Route routes[]={
 {0x4d3350,0x4cdae0,UTV,POWER,4},{0x4d35c0,0x4cdaf2,UTV,COAST,4},
 {0x4d3820,0x4cdb00,UTV,ENGINE,8},{0x456060,0x4cdb06,UTV,AXLE,8},
 {0x4d0180,0x4cdb0b,UTV,SUSPENSION,4},{0x452a90,0x4cdb10,UTV,CONTACT,4},
 {0x4d50e0,0x4cdb23,UTV,TYRE,12},{0x4d50e0,0x4cdb36,UTV,TYRE,12},
 {0x4d50e0,0x4cdb49,UTV,TYRE,12},{0x4d50e0,0x4cdb5c,UTV,TYRE,12},
 {0x4b8ba0,0x4b32e0,BUGGY,POWER,4},{0x4b8e10,0x4b32f2,BUGGY,COAST,4},
 {0x4b9070,0x4b3300,BUGGY,ENGINE,8},{0x4b9840,0x4b3306,BUGGY,AXLE,8},
 {0x4b59d0,0x4b330b,BUGGY,SUSPENSION,4},{0x4b54c0,0x4b3310,BUGGY,CONTACT,4},
 {0x4bad20,0x4b3323,BUGGY,TYRE,12},{0x4bad20,0x4b3336,BUGGY,TYRE,12},
 {0x4bad20,0x4b3349,BUGGY,TYRE,12},{0x4bad20,0x4b335c,BUGGY,TYRE,12},
 {0x455200,0x450e60,TRUCK,POWER,4},{0x455470,0x450e72,TRUCK,COAST,4},
 {0x4556d0,0x450e80,TRUCK,ENGINE,8},{0x456060,0x450e86,TRUCK,AXLE,8},
 {0x452fa0,0x450e8b,TRUCK,SUSPENSION,4},{0x452a90,0x450e90,TRUCK,CONTACT,4},
 {0x457070,0x450ea3,TRUCK,TYRE,12},{0x457070,0x450eb6,TRUCK,TYRE,12},
 {0x457070,0x450ec9,TRUCK,TYRE,12},{0x457070,0x450edc,TRUCK,TYRE,12}
};
static void require(int ok,rx_addr target,rx_addr entry,rx_addr expected,const char*why){
 if(!ok){fprintf(stderr,"Family force-stage contract target%08x entry%08x expected%08x actual%08x %s\n",target,entry,expected,rx_sp(),why);abort();}
}
void rx_fourwheel_force_stage_dependency(rx_addr target,rx_addr expected,int first,float timer){
 const rx_addr entry=rx_sp(),ra=U32(entry,0);const Route*r=NULL;
 (void)first;(void)timer; /* Typed owners consume their explicit scalar inputs. */
 for(unsigned i=0;i<sizeof routes/sizeof routes[0];++i)if(routes[i].target==target&&routes[i].returned==ra){
  require(r==NULL,target,entry,expected,"duplicate finite route");r=&routes[i];
 }
 require(r!=NULL,target,entry,expected,"unsupported caller/target pair");
 require(expected==entry+r->pop,target,entry,expected,"actual argument cleanup");
 const rx_addr self=fx_reg(FX_EDI);
 switch(r->role){
 case POWER:
  if(r->family==UTV)rx_utv_select_power_gear(self);
  else if(r->family==BUGGY)rx_buggy_select_power_gear(self);
  else rx_truck_select_power_gear(self);break;
 case COAST:
  if(r->family==UTV)rx_utv_select_coast_gear(self);
  else if(r->family==BUGGY)rx_buggy_select_coast_gear(self);
  else rx_truck_select_coast_gear(self);break;
 case ENGINE:
  if(r->family==UTV)rx_utv_engine_step_exact(self,F32(entry,4));
  else if(r->family==BUGGY)rx_buggy_engine_step_exact(self,F32(entry,4));
  else rx_truck_engine_step_exact(self,F32(entry,4));break;
 case AXLE:
  if(r->family==BUGGY)rx_buggy_apply_axle_torques(U32(entry,4));
  else rx_fourwheel_apply_axle_torques(U32(entry,4));break;
 case SUSPENSION:
  if(r->family==UTV)rx_utv_suspension_step_exact(self);
  else if(r->family==BUGGY)rx_buggy_suspension_step_exact(self);
  else rx_truck_suspension_step_exact(self);break;
 case CONTACT:
  if(r->family==BUGGY)rx_buggy_apply_contact_forces(self);
  else rx_fourwheel_apply_contact_forces(self);break;
 case TYRE:
  if(r->family==UTV)rx_utv_tyre_step_draft(self,U32(entry,4),F32(entry,8));
  else if(r->family==BUGGY)rx_buggy_tyre_step_exact(self,U32(entry,4),F32(entry,8));
  else rx_truck_tyre_step_exact(self,U32(entry,4),F32(entry,8));break;
 default:require(0,target,entry,expected,"unknown role");
 }
 require(rx_sp()==entry,target,entry,expected,"typed callee changed entered SP");
 rx_set_sp(expected);
}
