/* Product typed composition. Existing manager ABI keeps shared owner once. */
#include "career_moto.h"
#include "build_activity_util.h"
extern const rx_career_single_dependencies* rx_career_event_dependencies(void);
uint32_t rx_career_moto_event_call(rx_addr e,uint32_t slot,uint32_t arg,const rx_career_single_dependencies*d){
 if(U32(e,0)!=0x94e2fc)act_unreached(0x5b6950,"not original Moto event");
 switch(slot){
 case 0:case 20:return rx_career_event_current_activity_type(e);
 case 1:return rx_career_moto_player_state(arg);
 case 2:rx_career_moto_holeshot(e,arg,d);break;
 case 3:return rx_career_event_scalar_delete(e,arg);
 case 4:rx_career_event_initialize(e);break;
 case 5:rx_career_moto_reset(e,d);break;
 case 6:rx_career_event_uninitialize(e,d);break;
 case 7:case 8:rx_career_moto_bind(e,arg,slot==7,d);break;
 case 9:rx_career_event_gate(e,d);break;
 case 10:rx_career_moto_gate_result(e,arg,d);break;
 case 11:rx_career_event_post_result(e,d);break;
 case 12:rx_career_moto_advance(e,d);break;
 case 13:rx_career_moto_post_event(e,d);break;
 case 14:rx_career_single_set_holeshot_player(e,arg);break;
 case 15:SU32(e,0x1c,arg);break; /*0088cd60 does NOT adjust duration*/
 case 16:SU32(e,8,arg);break; /*008ba4a0 does NOT adjust activity mode*/
 case 17:return U32(e,0xec);
 case 18:{int32_t a=(int32_t)rx_career_event_rider_limit(U32(e,0x1c),d),b=(int32_t)rx_career_class_rider_limit(-1);return(uint32_t)(a<=b?a:b);}
 case 19:return 1; /*actual00794f70*/
 case 21:return 0;
 case 22:rx_career_event_result_sort_columns(arg);break;
 case 23:rx_career_event_prepare(e,d);break;
 default:act_unreached(0x5b6950,"Moto slot requires its real typed arity");
 }
 return 0;
}
static uint32_t state(rx_addr e,uint32_t p){(void)e;return rx_career_moto_player_state(p);}
static void reset(rx_addr e){rx_career_moto_reset(e,rx_career_event_dependencies());}
static void bind_event(rx_addr e,rx_addr c){rx_career_moto_bind(e,c,1,rx_career_event_dependencies());}
static void unbind_event(rx_addr e,rx_addr c){rx_career_moto_bind(e,c,0,rx_career_event_dependencies());}
static void gate(rx_addr e,uint32_t v){rx_career_moto_gate_result(e,v,rx_career_event_dependencies());}
static void advance(rx_addr e){rx_career_moto_advance(e,rx_career_event_dependencies());}
static void post(rx_addr e){rx_career_moto_post_event(e,rx_career_event_dependencies());}
static void holeshot(rx_addr e,uint32_t v){rx_career_moto_holeshot(e,v,rx_career_event_dependencies());}
static int32_t rank(rx_addr e,rx_addr name,uint32_t type){return rx_career_event_standing_rank(e,name,type,rx_career_event_dependencies());}
static void drivers(rx_addr e,rx_addr a){(void)e;rx_career_event_sort_drivers(a,rx_career_event_dependencies());}
static void points(rx_addr e,rx_addr name,int32_t value,int32_t series){(void)e;rx_career_event_set_standing_points(name,value,series,rx_career_event_dependencies());}
RX_FUNCTION(0x005aeeb0,state)
RX_FUNCTION(0x005ac8f0,reset)
RX_FUNCTION(0x005aeee0,bind_event)
RX_FUNCTION(0x005aef50,unbind_event)
RX_FUNCTION(0x005aefc0,gate)
RX_FUNCTION(0x005af1e0,advance)
RX_FUNCTION(0x005af2c0,post)
RX_FUNCTION(0x005afb20,holeshot)
RX_FUNCTION(0x005ac050,rank)
RX_FUNCTION(0x005ac3d0,drivers)
RX_FUNCTION(0x005ac6c0,points)
static void set_activity_type(rx_addr e,uint32_t v){SU32(e,0x1c,v);}
static void set_format(rx_addr e,uint32_t v){SU32(e,8,v);}
RX_FUNCTION(0x0088cd60,set_activity_type)
RX_FUNCTION(0x008ba4a0,set_format)
