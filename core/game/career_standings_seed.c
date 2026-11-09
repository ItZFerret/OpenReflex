/* Complete semantic clear/seed owners. Isolated candidate; original scratch,
 * allocator and incidental register equivalence not yet proved. No missing
 * player defaults, synthetic ordering or original gameplay fallback. */
#include "career_standings_seed.h"
#include "career_manager.h"
#include "build_activity_util.h"

static void seed_int(rx_addr criteria,rx_addr item,rx_addr name,uint32_t value){
 ACT_CALL(1,rx_tbl_crit_init_int(item,name,0,value));
 ACT_CALL(1,rx_tbl_crit_push(criteria,item));
 ACT_CALL(0,rx_tbl_crit_destroy(item));
}
static uint32_t seed_word(rx_addr table,rx_addr name,uint32_t row){
 uint32_t col;rx_addr cell;
 ACT_CALLR(col,1,rx_tbl_column_index(table,name));
 ACT_CALLR(cell,1,rx_tbl_cell_word(table,col,row));return U32(cell,0);
}
void rx_career_clear_standings(rx_addr manager,int32_t series){
 ACT_FRAME_EBP(F,8,0xd4,3);
 const rx_addr table=manager+0x560;
 if(table){
  const rx_addr criteria=F+0x10,item=F+0x28;
  ACT_CALL(0,rx_tbl_crit_vec_init(criteria));
  rx_addr column;uint32_t index;ACT_CALLR(index,1,rx_tbl_column_index(table,0x94c990));
  if(index>=((U32(table,0x14)-U32(table,0x10))>>2))crt_invalid_parameter_noinfo();
  column=U32(U32(table,0x10),4*index);
  rx_addr rows;ACT_CALLR(rows,1,rx_tbl_rows(column,series));
  if(U32(rows,0x1c)){
   seed_int(criteria,item,0x94c990,(uint32_t)series);
   /* Canonical complete004ecf00 owns query temporaries, column/index erasure
    *and the single final revision bump. No per-row revision suppression. */
   ACT_CALL(2,(void)rx_tbl_delete_rows(table,criteria));
  }
  ACT_CALL(0,rx_tbl_crit_vec_destroy(criteria));
 }
 ACT_END(F);
}
RX_FUNCTION(0x005b6370,rx_career_clear_standings)

void rx_career_seed_standings(rx_addr manager,int32_t series){
 ACT_FRAME_EBP(F,8,0x10c,3);
 ACT_CALL(2,rx_career_clear_standings(manager,series));
 const rx_addr target=manager+0x560;
 if(target){
  const rx_addr criteria=F+0x18,item=F+0x50,out=F+0x108;
  ACT_CALL(0,rx_tbl_crit_vec_init(criteria));
  rx_addr players;ACT_CALLR(players,0,rx_career_player_table(manager,2));
  int32_t count;ACT_CALLR(count,0,rx_tbl_row_count(players));
  for(uint32_t row=0;(int32_t)row<count;row++){
   seed_int(criteria,item,0x94c990,(uint32_t)series);
   static const rx_addr scores[]={0x94d6b8,0x94c150,0x94c144,0x94c138,0x94d910};
   for(unsigned i=0;i<5;i++)seed_int(criteria,item,scores[i],0);
   seed_int(criteria,item,0x93e170,seed_word(players,0x93e170,row));
   uint32_t column;rx_addr text;
   ACT_CALLR(column,1,rx_tbl_column_index(players,0x936f00));
   if(column>=((U32(players,0x14)-U32(players,0x10))>>2))crt_invalid_parameter_noinfo();
   ACT_CALLR(text,1,rx_tbl_cell_string(U32(U32(players,0x10),4*column),row));
   const rx_addr name=rx_tbl_str_cstr(text);
   ACT_CALL(2,rx_tbl_crit_init_string(item,0x936f00,0,name));
   ACT_CALL(1,rx_tbl_crit_push(criteria,item));ACT_CALL(0,rx_tbl_crit_destroy(item));
   seed_int(criteria,item,0x936ef4,seed_word(players,0x936ef4,row));
   ACT_CALL(2,(void)rx_tbl_add_row(target,out,criteria));
   ACT_CALL(0,rx_tbl_crit_vec_clear(criteria));
  }
  ACT_CALL(0,rx_tbl_crit_vec_destroy(criteria));
 }
 ACT_END(F);
}
RX_FUNCTION(0x005b4d30,rx_career_seed_standings)
