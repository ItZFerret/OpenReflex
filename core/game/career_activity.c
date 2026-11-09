#include "career_activity.h"
#include "build_activity_util.h"
#include "build_ui_props.h"
#include <string.h>
/* The event's authored type when active; otherwise the owned UI property.
 * RX_ORACLE uses the original UI accessor as a reference-only boundary. */
uint32_t rx_career_current_event_type(rx_addr manager){
 ACT_FRAME(F,0x88,2);uint32_t result;const rx_addr event=U32(manager,0x18c);
 if(event)result=U32(event,0x1c);
 else{
  const rx_addr buffer=F+8;crt_memset(buffer,0,0x80);
#ifdef RX_ORACLE
  rx_regs in,out;memset(&in,0,sizeof in);in.r[0]=0x935e08;in.r[7]=buffer;in.r[6]=0x80;
  const uint32_t arg=U32(0xa93fcc,0);rx_call_original_ex(0x5dc060,&in,&out,1,&arg);
#else
  (void)rx_ui_prop_get_string(0x935e08,buffer,0x80);
#endif
  ACT_CALLR(result,1,rx_event_type_of(buffer));
 }
 ACT_END(F);return result;
}
RX_FUNCTION(0x005b6a30,rx_career_current_event_type)

/* Actual authored SeriesId ranges, including negative/out-of-range fallback.
 * These are presence categories, not an event-format or vehicle-class map. */
uint32_t rx_career_series_presence(rx_addr manager){
 const int32_t series=I32(manager,0x20);if(series<0)return 0;
 static const struct{int32_t end;uint32_t value;}ranges[]={
  {3,0},{6,1},{10,4},{14,5},{22,6},{25,10},{28,11},{33,12},
  {37,7},{41,8},{49,9},{53,13},{57,14},{65,15},{68,2},
  {71,16},{74,17},{80,18},{84,3},{89,19}
 };
 for(unsigned i=0;i<sizeof ranges/sizeof*ranges;i++)if(series<ranges[i].end)return ranges[i].value;
 return 0;
}
RX_FUNCTION(0x005b74d0,rx_career_series_presence)
