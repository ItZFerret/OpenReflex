/* Complete0055e790..0055e818,137 bytes. No scenario creation/skip or
 * host override: every original permission read and lazy manager is retained. */
#include "activity_results_permission.h"
#include "build_activity_util.h"
#include "fx.h"
uint32_t rx_activity_results_scenario_allowed(rx_addr activity) {
 if(U32(activity,0xfe8)!=0)return 0;
 if(U32(activity,0x238)==2)return 0;
 if(U32(U32(activity,0x234),0x20018)!=0)return 0;
 rx_addr manager;
 ACT_CALLR(manager,0,rx_fx_session_manager());
 const rx_addr session=U32(manager,0x10);
 if(U32(session,0x1c4c)!=0&&U32(session,0x1b0)!=1)return 0;
 if(U32(0,0xd60ff0)==0)return 0;
 ACT_CALLR(manager,0,rx_fx_series_manager());
 if(U32(manager,0x190)!=2){
  ACT_CALLR(manager,0,rx_fx_series_manager());
  if(U32(manager,0x190)!=1)return 1;
 }
 ACT_CALLR(manager,0,rx_fx_series_manager());
 const rx_addr event=U32(manager,0x18c);
 uint32_t phase=U32(event,0xc);
 if(phase==2)phase=U32(event,0x10);
 return phase==5||phase==7;
}
RX_FUNCTION(0x0055e790,rx_activity_results_scenario_allowed)
