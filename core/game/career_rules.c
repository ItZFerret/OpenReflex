#include "career_rules.h"
#include "build_activity_util.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "crt.h"
static rx_addr column(rx_addr table,rx_addr name){
 uint32_t index;ACT_CALLR(index,1,rx_tbl_column_index(table,name));
 if(index>=(U32(table,0x14)-U32(table,0x10))/4)act_unreached(0x4eac43,"PlayerEvent column index");
 return U32(U32(table,0x10),index*4);
}
/*004eaaf0 EAX=controller, ECX=event C string, no stack args. */
uint32_t rx_player_event_status(uint32_t controller,rx_addr name){
 ACT_FRAME(F,0x1c,3);rx_addr table,ref;uint32_t result=0;
 ACT_CALLR(table,1,rx_tbl_find(RX_TBL_PLAYER_DB(controller),0x93e858));
 if(table){
  rx_tbl_str_init(F+0xc);ACT_CALL(1,rx_tbl_str_assign_n(F+0xc,name,crt_strlen(name)));
  rx_addr col=column(table,0x93e864);ACT_CALLR(ref,0,rx_tbl_string_row(col,F+0xc));
  rx_tbl_str_tidy(F+0xc);
  table=U32(ref,8);const uint32_t row=U32(ref,0);
  if(table&&row!=UINT32_MAX){
   uint32_t index;rx_addr cell;ACT_CALLR(index,1,rx_tbl_column_index(table,0x93de30));
   ACT_CALLR(cell,1,rx_tbl_cell_word(table,index,row));result=U32(cell,0);
  }
 }
 ACT_END(F);return result;
}
RX_FUNCTION(0x004eaaf0,rx_player_event_status)
/*004eabd0 EAX=controller, ECX=event C string; status; RET4. The original only
 * changes PlayerEvent; it does not perform a save or complete any event. */
void rx_player_event_status_set(uint32_t controller,rx_addr name,uint32_t value){
 ACT_FRAME(F,0xf8,4);rx_addr table,ref;
 SU32(F,0x10c,value);
 ACT_CALLR(table,1,rx_tbl_find(RX_TBL_PLAYER_DB(controller),0x93e858));
 if(table){
  rx_tbl_str_init(F+0x14);ACT_CALL(1,rx_tbl_str_assign_n(F+0x14,name,crt_strlen(name)));
  rx_addr col=column(table,0x93e864);ACT_CALLR(ref,0,rx_tbl_string_row(col,F+0x14));
  rx_tbl_str_tidy(F+0x14);
  const rx_addr found=U32(ref,8);const uint32_t row=U32(ref,0);
  if(found&&row!=UINT32_MAX){
   uint32_t index;rx_addr cell;ACT_CALLR(index,1,rx_tbl_column_index(found,0x93de30));
   ACT_CALLR(cell,1,rx_tbl_cell_word(found,index,row));
   if(U32(cell,0)!=value)SU32(found,0x68,U32(found,0x68)+1);
   if(index>=(U32(found,0x14)-U32(found,0x10))/4)act_unreached(0x4ead74,"PlayerEvent Status column index");
   ACT_CALL(3,rx_tbl_set_int(U32(U32(found,0x10),4*index),row,F+0x10c));
  }else{
   ACT_CALL(0,rx_tbl_crit_vec_init(F+0x14));
   rx_tbl_str_init(F+0x30);ACT_CALL(1,rx_tbl_str_assign_n(F+0x30,name,crt_strlen(name)));
   ACT_CALL(1,rx_tbl_crit_copy_string(F+0x58,0x93e864,F+0x30));
   ACT_CALL(1,rx_tbl_crit_push(F+0x14,F+0x58));ACT_CALL(0,rx_tbl_crit_destroy(F+0x58));
   rx_tbl_str_tidy(F+0x30);
   ACT_CALL(1,rx_tbl_crit_init_int(F+0x58,0x93de30,0,value));
   ACT_CALL(1,rx_tbl_crit_push(F+0x14,F+0x58));ACT_CALL(0,rx_tbl_crit_destroy(F+0x58));
   ACT_CALL(2,rx_tbl_add_row(table,F+0x4c,F+0x14));ACT_CALL(0,rx_tbl_crit_vec_destroy(F+0x14));
  }
 }
 ACT_END(F);
}
RX_FUNCTION(0x004eabd0,rx_player_event_status_set)
/*00742a00: the actual App action used by RulesPopup.End. Keep its context/world
 * selection and native zero-world behavior; the public API validates first. */
void rx_app_pre_activity_rules_page_done(void){
 const rx_addr context=U32(0,0xa95844);const uint32_t alternate=U32(U32(context,0x20),0x34);
 ACT_FRAME(F,0,1);const rx_addr game=U32(0,0xa946b8);rx_addr world=0;
 if(alternate==1)world=U32(game,0x5a54);
 else if(U32(U32(context,0x1c),0x34)==1)world=U32(game,0x20c8);
 if(I32(world,0x21da8)>0){
  if(alternate==1)world=U32(game,0x5a54);
  else if(U32(U32(context,0x1c),0x34)==1)world=U32(game,0x20c8);
  else world=0;
  SU32(U32(U32(world,0x21da4),0),0xff068,0);
 }
 ACT_END(F);
}
RX_FUNCTION(0x00742a00,rx_app_pre_activity_rules_page_done)
#ifndef RX_ORACLE
#include "career_properties.h"
#include "career_activity.h"
#include "race_misc.h"
#include "fx.h"
static rx_owned_rules_host host;
uint32_t rx_owned_rules_available(void){return host.context!=NULL;}
void rx_owned_rules_bind(const rx_owned_rules_host*h){
 if(!h||!h->context||!h->show||h->controller>3||host.context)act_unreached(0x5a4bd0,"owned Rules binding invalid");
 host=*h;
}
void rx_owned_rules_unbind(void*context){if(host.context==context)memset(&host,0,sizeof host);}
/*005a4bd0's full branch graph, with owned property/controller services replacing
 * only the unavailable MXUIManager object. No native address registration is
 * claimed for this different, explicit semantic boundary signature. */
uint32_t rx_owned_rules_select(rx_addr activity){
 if(!host.context)act_unreached(0x5a4bd0,"owned Rules host unavailable");
 ACT_FRAME(F,0x4c,4);uint32_t show=0,kind=U32(activity,0x23c),mode=U32(activity,0x238);
 rx_addr text=0x935d6c,manager;
 if(kind>=0x1f){manager=rx_fx_series_manager();ACT_CALLR(kind,1,rx_career_current_event_type(manager));}
 if(kind==5){text=0x935dd0;show=1;}
 else if(kind==0xc){text=0x935db8;show=1;}
 else if(kind==8){SU32(F,0x10,0);if(rx_owned_property_get_int(0x941f6c,F+0x10,0)&&U32(F,0x10)){text=0x94d270;show=1;}}
 switch(mode){
 case 4:show=0;text=0x935d6c;break;
 case 5:show=1;text=0x92d6b4;break;
 case 6:show=1;text=0x92d6a8;break;
 case 7:show=1;text=0x92b894;break;
 case 8:show=1;text=0x92d690;break;
 }
 manager=rx_fx_series_manager();
 if((mode!=4&&U32(manager,0x190)==1)||U32(manager,0x190)==2){
  if(U32(manager,0x18)==1){show=1;text=0x92b87c;}
  const rx_addr event=U32(manager,0x18c);
  if(event){
   const uint32_t event_kind=U32(event,8),phase=U32(event,0xc);rx_addr selected=0;
   if(phase==1){if(event_kind==0)selected=0x94d27c;else if(event_kind==1)selected=0x94d28c;}
   else if(phase==3&&event_kind==1)selected=0x92b8ac;
   else if(phase==4&&event_kind==1)selected=0x94c124;
   else if(phase==5&&event_kind==1)selected=0x94d29c;
   else if(phase==6&&event_kind==0)selected=0x92b8b4;
   if(!selected&&event_kind==8)selected=0x92b88c;
   if(selected){show=1;text=selected;}
  }
 }
 uint32_t player_mode;ACT_CALLR(player_mode,0,rx_game_player_mode());
 if(player_mode==0||player_mode==2||player_mode==3)show=0;
 if(show){
  /* Original bounded Rule_%s buffer, 64 bytes. All selected literals fit. */
  const char prefix[]="Rule_";for(uint32_t n=0;n<5;n++)SU8(F+0x18,n,(uint8_t)prefix[n]);
  uint32_t n=0;do{if(n>=58)act_unreached(0x5a4e2f,"RulesType exceeds original buffer");SU8(F+0x1d,n,U8(text,n));}while(U8(text,n++));
  uint32_t status;ACT_CALLR(status,0,rx_player_event_status(host.controller,F+0x18));
  if(status)show=0;
 }
 if(show)rx_owned_property_set_string(0x94d2b4,text);else rx_owned_property_erase(0x94d2b4);
 ACT_END(F);return show;
}
/*005a4ec0 selects again, then sends ShowRaceRulesPopup even if the second select
 * returns false. The host rejects a missing RulesType instead of guessing it. */
void rx_owned_rules_show(rx_addr activity){
 (void)rx_owned_rules_select(activity);
 ACT_FRAME(F,64,0);
 if(!rx_owned_property_get_string(0x94d2b4,F,64)||!host.show(host.context,activity,F))
  act_unreached(0x5a4ec0,"owned Rules popup request rejected");
 ACT_END(F);
}
#endif
