/* Complete semantic refresh owners from original physical spans; isolated
 * candidate. No registration, original comparison or playable career claim. */
#include "career_refresh.h"
#include "career_manager.h"
#include "career_progress.h"
#include "career_rewards.h"
#include "career_single.h"
#include "build_tables_sort.h"
#include "build_activity_util.h"

static rx_addr player_name(rx_addr table,uint32_t row){
 return rx_tbl_str_cstr(rx_tbl_cell_string(act_column(table,0x936f00),row));
}
static void set_word(rx_addr table,rx_addr name,uint32_t row,uint32_t value,rx_addr scratch){
 SU32(scratch,0,value);rx_tbl_set_cell_int(table,name,row,scratch);
}
static void int_filter(rx_addr filters,rx_addr item,rx_addr name,uint32_t value){
 rx_tbl_crit_init_int(item,name,0,value);rx_tbl_crit_push(filters,item);rx_tbl_crit_destroy(item);
}
static void name_filter(rx_addr filters,rx_addr item,rx_addr name){
 rx_tbl_crit_init_string(item,0x936f00,0,name);rx_tbl_crit_push(filters,item);rx_tbl_crit_destroy(item);
}

/*005b3680 EDX manager: preserve RequestedVehicleId==-1 fallback to VehicleId. */
void rx_career_refresh_player_models(rx_addr manager,rx_career_vehicle_model_resolver resolve){
 ACT_FRAME(F,0xc,4);
 const rx_addr players=rx_career_player_table(manager,2);
 const int32_t count=(int32_t)rx_tbl_row_count(players);
 for(uint32_t row=0;(int32_t)row<count;row++){
  uint32_t type=act_cell(players,0x936ef4,row);
  uint32_t player=rx_career_find_player_row(player_name(players,row),type);
  uint32_t vehicle=act_cell(ACT_PLAYER_LIST,0x93df5c,player);
  if(vehicle==UINT32_MAX)vehicle=act_cell(ACT_PLAYER_LIST,0x936cb4,player);
  if(!resolve)act_unreached(0x541040,"unclosed career vehicle identity resolver");
  set_word(players,0x93e170,row,resolve(U32(0xa946b8,0)+0x154b0,vehicle),F+0x14);
 }
 ACT_END(F);
}

/*005b52a0 EDX manager, stack series. Match BOTH PlayerType and PlayerName. */
void rx_career_refresh_standing_models(rx_addr manager,int32_t series){
 ACT_FRAME_EBP(F,8,0x104,3);
 const rx_addr standings=manager+0x560,players=rx_career_player_table(manager,2);
 if(standings&&players){
  const rx_addr rows=rx_tbl_rows(act_column(standings,0x94c990),series);
  const rx_addr filters=F+0x1c,item=F+0x58,found=F+0x38;
  for(rx_addr n=LEFT_OF(rows);n!=U32(rows,0x18);n=rx_tbl_rowset_next(n)){
   uint32_t row=U32(n,0xc);rx_tbl_crit_vec_init(filters);
   int_filter(filters,item,0x936ef4,act_cell(standings,0x936ef4,row));
   name_filter(filters,item,player_name(standings,row));
   SU32(found,0,UINT32_MAX);SU32(found,8,0);
   rx_tbl_find_first(players,filters,found);
   uint32_t value=act_cell(U32(found,8),0x93e170,U32(found,0));
   set_word(standings,0x93e170,row,value,F+0xc);
   rx_tbl_crit_vec_destroy(filters);
  }
 }
 ACT_END(F);
}

/*005b6060 stack(manager,series),RET8: retain identities/rows, zero five scores.
 * Every changed cell increments revision; an already-zero cell does not. */
void rx_career_zero_standing_scores(rx_addr manager,int32_t series){
 ACT_FRAME_EBP(F,8,0x14,3);const rx_addr table=manager+0x560;
 if(table){
  const rx_addr rows=rx_tbl_rows(act_column(table,0x94c990),series);
  static const rx_addr names[]={0x94d6b8,0x94c150,0x94c144,0x94c138,0x94d910};
  for(rx_addr n=LEFT_OF(rows);n!=U32(rows,0x18);n=rx_tbl_rowset_next(n))
   for(unsigned j=0;j<5;j++)set_word(table,names[j],U32(n,0xc),0,F+0xc);
 }
 ACT_END(F);
}

/*005b6460 EDX manager, stack series: first HUMAN in SortValue ordering.
 * No human leaves medal fields unchanged. A missing matching standings row
 * remains an invalid query, as in the original; do not invent zero medals. */
void rx_career_refresh_medals(rx_addr manager,int32_t series){
 ACT_FRAME_EBP(F,8,0x14c,3);
 const rx_addr table=manager+0x560,players=rx_career_player_table(manager,2);
 if(table&&players){
  const rx_addr sort=F+0x38,item=F+0xa0,sorted=F+0x50,filters=F+0x20,selected=F+0x78;
  rx_tbl_crit_vec_init(sort);rx_tbl_sort_criterion_init(item,0x94d6cc,0);
  rx_tbl_sort_criterion_push(sort,item);rx_tbl_sort_criterion_destroy(item);
  rx_tbl_rowset_sorted_all(sorted,players,rx_tbl_row_count(players),sort);
  for(rx_addr n=LEFT_OF(sorted);n!=U32(sorted,0x18);n=rx_tbl_rowset_next(n)){
   uint32_t row=U32(n,0xc);if(act_cell(players,0x936ef4,row))continue;
   rx_tbl_crit_vec_init(filters);int_filter(filters,item,0x936ef4,0);
   int_filter(filters,item,0x94c990,(uint32_t)series);
   name_filter(filters,item,player_name(players,row));
   rx_tbl_rowset_init(selected,0);rx_tbl_select(table,selected,filters);
   rx_addr first=LEFT_OF(selected);if(first==U32(selected,0x18))crt_invalid_parameter_noinfo();
   row=U32(first,0xc);
   SU32(manager,0x28,act_cell(table,0x94c150,row));
   SU32(manager,0x2c,act_cell(table,0x94c144,row));
   SU32(manager,0x30,act_cell(table,0x94c138,row));
   rx_tbl_rowset_destroy(selected);rx_tbl_crit_vec_destroy(filters);break;
  }
  rx_tbl_rowset_destroy(sorted);rx_tbl_sort_criteria_destroy(sort);
 }
 ACT_END(F);
}

uint32_t rx_career_refresh(rx_addr manager,uint32_t advance,
 rx_career_vehicle_model_resolver resolve,void (*ui_int)(rx_addr,uint32_t)){
 ACT_FRAME_EBP(F,8,8,2);
 if(U32(manager,0x184)){ACT_END(F);return 0;}
 SU32(manager,0x184,1);
 rx_career_rewards_apply(manager+0x78,U32(manager,0x1c));
 rx_career_refresh_player_models(manager,resolve);
 rx_career_refresh_standing_models(manager,(int32_t)U32(manager,0x20));
 rx_career_refresh_standing_models(manager,(U32(manager,0x190)!=1)-2);
 rx_career_refresh_standing_models(manager,-3);
 if(advance)rx_career_accumulate_standings(manager,(U32(manager,0x190)!=1)-2,-3,0);
 rx_career_zero_standing_scores(manager,-3);
 if(!ui_int)act_unreached(0x5dbfb0,"unclosed career UI-property writer");
 ui_int(0x94c9dc,UINT32_MAX-2u);
 rx_career_refresh_medals(manager,(U32(manager,0x190)!=1)-2);
 if(advance){SU32(manager,0x4c,U32(manager,0x4c)+1);rx_career_progress_resolve_event(manager);}
 if(U32(manager,0))rx_career_progress_commit(manager);
 ACT_END(F);return 1;
}
