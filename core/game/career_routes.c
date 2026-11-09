/* Isolated native route/loading source. No registrations or UI activation.
 * Canonical tables, formatting and owned game transitions are reused once. */
#include "career_routes.h"
#include "build_activity_util.h"
#include "race_misc.h"
static void need(int ok,uint32_t pc){if(!ok)act_unreached(pc,"unclosed career route dependency");}
void rx_career_build_route(rx_addr out,uint32_t capacity,uint32_t kind,uint32_t activity_mode,uint32_t player_mode,rx_addr prefix,const rx_career_route_dependencies*d){
 ACT_FRAME_EBP(F,1,0x1c,2);
 /*005ce910 is this original table access, called before special overrides.*/
 rx_addr activity=U32(0x9d3c00,4*kind);
 if(kind==0x3d||activity_mode==4)activity=U32(0x9d3c00,4*8);
 else if(activity_mode==8)activity=0x92d690;
 if(player_mode==2||player_mode==3){
  need(d&&d->get_int,0x5dc0c0);uint32_t spectator=d->get_int(0x9416d8,0);
  uint32_t real_world=d->get_int(0x94c9bc,0);
  if(spectator&&real_world){SU32(F,0,0x9421d8);rx_format_buffer(out,capacity,0x9419e4,F);ACT_END(F);return;}
 }
 if(player_mode==2||player_mode==3)player_mode=1;
 SU32(F,0,prefix);SU32(F,4,activity);SU32(F,8,U32(0x9d3d90,4*player_mode));
 rx_format_buffer(out,capacity,0x946350,F);ACT_END(F);
}
uint32_t rx_career_compare_route(rx_addr ui,rx_addr route,const rx_career_route_dependencies*d){
 if(!ui){need(d&&d->compare_owned_route,0x5a4350);return d->compare_owned_route(route);}
 if(U32(ui,0x2ac)){
  if(crt_stricmp(ui+0x2b0,route)==0)return U32(ui,0x2ac);
  /*Original rereads flag after comparator; release can touch package metadata.*/
  if(U32(ui,0x2ac)){need(d&&d->release_route,0x5dbda0);d->release_route(ui+0xf4,ui+0x2b0,ui);SU32(ui,0x2ac,0);}
 }
 crt_strncpy(ui+0x2b0,route,0x3f);SU8(ui,0x2ef,0);
 need(d&&d->acquire_route,0x5dbcb0);
 if(d->acquire_route(ui+0xf4,ui+0x2b0,ui,1))SU32(ui,0x2ac,1);
 return U32(ui,0x2ac);
}
void rx_career_prepare_gameplay_routes(rx_addr ui,const rx_career_route_dependencies*d){
 ACT_FRAME(F,0x54,4);uint32_t player_mode=rx_game_player_mode();
 for(uint32_t row=0;row<rx_tbl_row_count(ACT_PLAYER_LIST);row++){
  if(act_cell(ACT_PLAYER_LIST,0x93de90,row)!=1)continue;
  if(act_cell(ACT_PLAYER_LIST,0x936ef4,row)!=0)continue;
  uint32_t kind=act_cell(ACT_PLAYER_LIST,0x93df2c,row);
  uint32_t area=act_cell(ACT_PLAYER_LIST,0x93df44,row);
  if(kind==0x3d){kind=act_cell(ACT_PLAYER_LIST,0x93dd90,row);area=act_cell(ACT_PLAYER_LIST,0x93dda0,row);}
  int32_t activity_row=rx_activity_find_row(ACT_ACTIVITY_TBL,kind,(int32_t)area);
  if(activity_row!=-1){
   uint32_t mode=act_cell(ACT_ACTIVITY_TBL,0x936e30,(uint32_t)activity_row);
   const rx_addr route=F+0x14;crt_memset(route,0,64);
   rx_career_build_route(route,64,kind,mode,player_mode,0x94d198,d);
   (void)rx_career_compare_route(ui,route,d);
  }
 }
 ACT_END(F);
}
void rx_career_request_level(rx_addr ui,uint32_t prepare_gameplay,const rx_career_route_dependencies*d){
 ACT_FRAME(F,0x110,3);const rx_addr location=F+0x10;crt_memset(location,0,256);
 need(d&&d->get_string,0x5dc060);d->get_string(ui,0x93e7c4,location,256);
 if(prepare_gameplay)rx_career_prepare_gameplay_routes(ui,d);
 /*Current public event loader opens assets BEFORE rx_start_event. Career uses
  * the equivalent host preparation here, without its exhibition setup/reset.*/
 need(d->prepare_level!=0,0x75c640);if(!d->prepare_level(location))act_unreached(0x75c640,"career level preparation failed; no loading success");
 rx_player_list_free_unreferenced();
 (void)rx_game_resume_from_ui(U32(0xa946b8,0),0);
 rx_game_request_level(location);
 need(d->session_metadata!=0,0x59cd40);d->session_metadata(ui);
 ACT_END(F);
}
