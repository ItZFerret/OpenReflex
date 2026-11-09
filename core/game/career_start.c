/* Isolated source-only manager start closure. No registrations/activation yet.
 * Semantic owner, not a claim of incidental register/private-stack equivalence. */
#include "career_start.h"
#include "career_manager.h"
#include "career_progress.h"
#include "career_single.h"
#include "build_activity_util.h"
#include "race_misc.h"
static void need(int ok,uint32_t pc){if(!ok)act_unreached(pc,"unclosed career start dependency");}
static uint32_t event(const rx_career_start_dependencies*d,rx_addr e,uint32_t slot,uint32_t a){need(d&&d->event_call,0x5b6950);return d->event_call(e,slot,a);}
static void ui(const rx_career_start_dependencies*d,rx_addr k,uint32_t v){need(d&&d->ui_int,0x5dbfb0);d->ui_int(k,v);}
static void text(const rx_career_start_dependencies*d,rx_addr k,rx_addr v){need(d&&d->ui_string,0x5dbf50);d->ui_string(k,v);}
static void word(rx_addr table,rx_addr name,uint32_t row,uint32_t value,rx_addr scratch){
 uint32_t old=act_cell(table,name,row);if(old!=value)SU32(table,0x68,U32(table,0x68)+1);
 SU32(scratch,0,value);rx_tbl_set_int(act_column(table,name),row,scratch);
}
rx_addr rx_career_event_for_format(rx_addr m,uint32_t format){
 switch(format){case 0:return m+0x288;case 1:return m+0x194;case 3:return m+0x380;
 case 2:case 4:case 5:case 6:case 7:case 8:case 9:return m+0x46c;default:return 0;}
}
void rx_career_select_event(rx_addr m,uint32_t kind,uint32_t format,const rx_career_start_dependencies*d){
 const rx_addr next=rx_career_event_for_format(m,format);rx_addr current=U32(m,0x18c);
 if(current){if(current==next){event(d,current,5,0);return;}
  event(d,current,8,U32(m,0x180));event(d,U32(m,0x18c),6,0);SU32(m,0x18c,0);}
 SU32(m,0x18c,next);event(d,next,15,kind);event(d,U32(m,0x18c),16,format);
 text(d,0x93e86c,U32(0x9d2220,4*U32(m,0x14)));
 text(d,0x935e08,U32(0x9d2220,4*U32(U32(m,0x18c),0x1c)));
 event(d,U32(m,0x18c),4,0);event(d,U32(m,0x18c),7,U32(m,0x180));
}
void rx_career_initialize_series(rx_addr m,const rx_career_start_dependencies*d){
 ACT_FRAME_EBP(F,16,0x94,3);
 rx_career_select_event(m,U32(m,0x14),U32(m,0x10),d);ui(d,0x935df8,U32(m,0x68));
 need(d->clear_nonhuman_players!=0,0x59e7a0);d->clear_nonhuman_players();
 need(d->setup_ai_riders!=0,0x5b2750);d->setup_ai_riders(m);
 need(d->populate_players!=0,0x5b2a10);d->populate_players(m);
 need(d->seed_standings!=0,0x5b4d30);d->seed_standings(m,(int32_t)U32(m,0x20));
 d->seed_standings(m,(U32(m,0x190)!=1)-2);d->seed_standings(m,-3);
 if(U32(m,0))rx_career_progress_commit(m);
 const rx_addr points=F+0x40;crt_memset(points,0,48);
 rx_addr t=rx_tbl_find(0xae0910,0x94d8d4);
 rx_addr rows=rx_tbl_rows(act_column(t,0x94d830),(int32_t)U32(m,0x44));
 for(rx_addr n=LEFT_OF(rows);n!=U32(rows,0x18);n=rx_tbl_rowset_next(n)){
  uint32_t r=U32(n,0xc);int32_t place=(int32_t)act_cell(t,0x94d8e4,r);
  SU32(points,4u*((uint32_t)place-1),act_cell(t,0x94d6b8,r));
 }
 /*005ab230: count is rowset size; second stack argument is points[2],
  * not an invented multiplier. Signed count comparison follows the original. */
 const rx_addr e=U32(m,0x18c);int32_t count=(int32_t)U32(rows,0x1c);if(count>11)count=12;
 for(int32_t i=0;i<12;i++)SU32(e,0xb8+4*i,i<count?U32(points,4*i):0);
 SU32(e,0xe8,U32(points,8));SU32(e,0xb4,U32(m,0x6c));
 text(d,0x936e58,0x94cd34);SU32(m,0x188,(int32_t)U32(m,0x40)<=(int32_t)U32(m,0x4c));
 text(d,0x93e86c,U32(0x9d2220,4*U32(m,0x14)));ui(d,0x94d6a4,U32(m,0x188));
 text(d,0x935e08,U32(0x9d2220,4*U32(U32(m,0x18c),0x1c)));
 ui(d,0x94bfcc,U32(m,0x70));ui(d,0x936e48,U32(m,0x38));SU32(m,0x184,1);ACT_END(F);
}
void rx_career_setup_event_players(rx_addr m,const rx_career_start_dependencies*d){
 ACT_FRAME_EBP(F,8,0x17c,3);event(d,U32(m,0x18c),23,0);rx_tbl_table_clear_rows(ACT_ACTIVITY_TBL);
 need(d&&d->ui_get_int,0x5dc0c0);uint32_t track=d->ui_get_int(0x936e40,0);
 for(uint32_t row=0;row<rx_tbl_row_count(ACT_PLAYER_LIST);row++){
  if(!act_cell(ACT_PLAYER_LIST,0x936ef4,row))continue;
  word(ACT_PLAYER_LIST,0x93def8,row,0,F+0x10);
  need(d->reset_player_requests!=0,0x4de2f0);d->reset_player_requests(row);
 }
 uint32_t kind=0x3d;if(event(d,U32(m,0x18c),21,0)){
  kind=event(d,U32(m,0x18c),20,0);rx_activity_table_add_row(ACT_ACTIVITY_TBL,kind,(uint32_t)(int32_t)(int8_t)track,1);}
 text(d,0x935e08,U32(0x9d2220,4*U32(m,0x14)));text(d,0x936e30,0x92d6d4);ui(d,0x9416d8,0);
 text(d,0x936e58,U32(0x9d2368,4*U32(U32(m,0x18c),0xb0)));ui(d,0x936e6c,U32(U32(m,0x18c),0xb4));
 uint32_t state=event(d,U32(m,0x18c),1,U32(U32(m,0x18c),0xc));if(!state)state=4;
 rx_addr t=rx_career_player_table(m,2),rows=rx_tbl_rows(act_column(t,0x94d670),(int32_t)state);
 for(rx_addr n=LEFT_OF(rows);n!=U32(rows,0x18);n=rx_tbl_rowset_next(n)){
  uint32_t r=U32(n,0xc),type=act_cell(t,0x936ef4,r);rx_addr s=rx_tbl_cell_string(act_column(t,0x936f00),r);
  uint32_t player=rx_career_find_player_row(rx_tbl_str_cstr(s),type);
  word(ACT_PLAYER_LIST,0x93def8,player,1,F+0x10);word(ACT_PLAYER_LIST,0x93df00,player,UINT32_MAX,F+0x10);
  rx_player_list_request_activity(player,kind,track);
 }
 SU32(m,0x184,0);need(d->manager_refresh!=0,0x5b4210);d->manager_refresh(m,0);
 rx_game_setup_activity_area(U32(0xa946b8,0));ACT_END(F);
}
void rx_career_start_event(rx_addr m,const rx_career_start_dependencies*d){
 ACT_FRAME_EBP(F,8,0x4c,3);ui(d,0x936e40,U32(m,0x5c));text(d,0x93e7c4,U32(m,0x60));ui(d,0x936e20,U32(m,0x58));
 rx_career_setup_event_players(m,d);uint32_t same=0;
 if(U32(m,0x18c)){
  uint32_t mode=rx_game_player_mode();const rx_addr route=F+0x10;crt_memset(route,0,64);
  need(d->build_route!=0,0x5e3000);d->build_route(route,64,U32(U32(m,0x18c),0x1c),U32(U32(m,0x18c),0x20),mode,0x94d198);
  need(d->compare_update_route!=0,0x5a4350);same=d->compare_update_route(route);
 }
 need(d->request_configured_level!=0,0x59bd60);d->request_configured_level(U32(0xa93fcc,0),same==0);
 ui(d,0x94d884,0);ACT_END(F);
}
