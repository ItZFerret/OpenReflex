/* Isolated semantic lifecycle composition; no original UI/save execution in
 * product and no implicit success at presentation/checkpoint boundaries. */
#include "career_lifecycle.h"
#include "career_manager.h"
#include "career_progress.h"
#include "build_activity_util.h"
static void need(int ok,uint32_t pc){if(!ok)act_unreached(pc,"unclosed career lifecycle dependency");}
static const rx_career_start_dependencies* start(const rx_career_lifecycle_dependencies*d){need(d&&d->start,0x5b4160);return d->start;}
void rx_career_bind_manager_channel(rx_addr manager,rx_addr channel){
 ACT_FRAME(F,4,0);SU32(manager,0x180,channel);
 static const struct{uint32_t name,type;}items[]={
 {0x94d6d8,0x9d24d0},{0x94d6fc,0x9cf3f0},{0x94d740,0x9cf3f0},
 {0x94d580,0x9cf3f0},{0x94d5fc,0x9d24bc},{0x94d768,0x9cf3f0},
 {0x94d71c,0x9cf3f0},{0x94d5d0,0x9cf3f0},{0x94d5a8,0x9cf3f0},
 {0x94d91c,0x9cf3f0},{0x94d948,0x9cf3f0}};
 for(unsigned i=0;i<sizeof items/sizeof*items;i++)ACT_CALL(1,rx_event_type_register(channel,items[i].name,items[i].type));
 /*005c1b40: unaligned sub6c/push4/pushnode/call. Registry allocation is
  * canonical; its undecoration entry is original entry-0x84. */
 ACT_CALL(0,rx_event_type_register_at(channel,0x94d970,0x9d3224,0,-0x84));ACT_END(F);
}
RX_FUNCTION(0x005b2690,rx_career_bind_manager_channel)
void rx_career_end_series(rx_addr m,uint32_t save,const rx_career_lifecycle_dependencies*d){
 ACT_FRAME_EBP(F,8,4,3);const rx_career_start_dependencies*s=start(d);const rx_addr e=U32(m,0x18c);
 if(e&&U32(e,0x2c)&&(U32(m,0x190)==1||U32(m,0x190)==2)){
  uint32_t advance=0;if(!save)SU32(m,0x184,1);else if(U32(e,0x30))advance=1;
  need(d->vehicle_model&&s->ui_int&&s->event_call,0x5b4210);
  ACT_CALL(1,(void)rx_career_refresh(m,advance,d->vehicle_model,s->ui_int));
  ACT_CALL(1,(void)s->event_call(U32(m,0x18c),8,U32(m,0x180)));
  ACT_CALL(0,(void)s->event_call(U32(m,0x18c),6,0));SU32(m,0x18c,0);
  need(s->ui_string!=0,0x5dbf50);s->ui_string(0x94c99c,0x935e14);
  SU32(m,0x190,0);s->ui_string(0x94c9f4,U32(0x9d2374,0));
  if(save){need(d->request_save!=0,0x5a6910);d->request_save(U32(m,0x1c));}
  need(s->clear_nonhuman_players!=0,0x59e7a0);ACT_CALL(0,s->clear_nonhuman_players());
 }
 need(s->ui_int!=0,0x5dbfb0);s->ui_int(0x94ca38,0);s->ui_int(0x94d9e4,1);
 ACT_CALL(1,rx_tbl_table_clear_rows(m+0x560));ACT_END(F);
}
void rx_career_commit_initialize(rx_addr m,const rx_career_lifecycle_dependencies*d){
 ACT_FRAME(F,0,3);SU32(m,0x184,1);ACT_CALL(2,rx_career_end_series(m,0,d));
 SU32(m,0x190,U32(m,8));const rx_career_start_dependencies*s=start(d);
 need(s->ui_string!=0,0x5dbf50);s->ui_string(0x94c9f4,U32(0x9d2374,4*U32(m,8)));
 ACT_CALL(1,rx_career_initialize_series(m,s));ACT_END(F);
}
void rx_career_complete_event(rx_addr m,const rx_career_lifecycle_dependencies*d){
 if(!U32(m,0x188))return;
 if(U32(m,0x18c)){need(d&&d->advance_victory,0x5aa1c0);ACT_CALL(0,d->advance_victory(U32(m,0x18c)));}
 need(d&&d->ui_route,0x5db4c0);
 d->ui_route((U32(m,0x174)||U32(m,0x178)||U32(m,0x17c))?0x94d9ac:0x94d9bc);
}
void rx_career_complete_series(rx_addr m,const rx_career_lifecycle_dependencies*d){
 ACT_FRAME_EBP(F,8,4,3);const rx_career_start_dependencies*s=start(d);
 ACT_CALL(4,rx_career_accumulate_standings(m,(U32(m,0x190)!=1)-2,-3,0));
 need(d->vehicle_model&&s->ui_int,0x5b4210);ACT_CALL(1,(void)rx_career_refresh(m,0,d->vehicle_model,s->ui_int));
 ACT_CALL(4,rx_career_accumulate_standings(m,(int32_t)U32(m,0x20),(U32(m,0x190)!=1)-2,0));
 ACT_CALL(2,rx_career_zero_standing_scores(m,(U32(m,0x190)!=1)-2));
 if(U32(m,0))ACT_CALL(0,rx_career_progress_commit(m));
 need(d->request_save!=0,0x5a6910);d->request_save(U32(m,0x1c));
 ACT_CALL(2,rx_career_end_series(m,1,d));ACT_END(F);
}
