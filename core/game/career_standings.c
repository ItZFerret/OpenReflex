/* 005b43c0 results-to-standings port (reached from the Heat main event's post, career_heat.c vf13; verified per call in the career boot). */
#include "career_manager.h"
#include "build_tables_sort.h"
#include "build_activity_util.h"
static rx_addr standings_name(rx_addr table,uint32_t row){return rx_tbl_str_cstr(rx_tbl_cell_string(act_column(table,0x936f00),row));}
void rx_career_update_standings(rx_addr manager){
 ACT_FRAME_EBP(F,8,0x124,3);const rx_addr standings=manager+0x560;
 if(standings){
  rx_addr players;ACT_CALLR(players,0,rx_career_player_table(manager,2));
  const rx_addr target=rx_tbl_rows(act_column(standings,0x94c990),-3);
  const rx_addr criteria=F+0x44,criterion=F+0x8c,sorted=F+0x5c;
  rx_tbl_crit_vec_init(criteria);rx_tbl_sort_criterion_init(criterion,0x94d6cc,0);
  rx_tbl_sort_criterion_push(criteria,criterion);rx_tbl_sort_criterion_destroy(criterion);
  uint32_t count;ACT_CALLR(count,0,rx_tbl_row_count(players));
  ACT_CALL(3,rx_tbl_rowset_sorted_all(sorted,players,count,criteria));
  uint32_t position=0;
  for(rx_addr node=LEFT_OF(sorted);node!=U32(sorted,0x18);node=rx_tbl_rowset_next(node)){
   const rx_addr name=standings_name(players,U32(node,0xc));rx_addr match=LEFT_OF(target);
   while(match!=U32(target,0x18)&&crt_stricmp(standings_name(standings,U32(match,0xc)),name)!=0)match=rx_tbl_rowset_next(match);
   if(match==U32(target,0x18))continue; /* unmatched names do not consume a finishing place */
   const uint32_t row=U32(match,0xc);const rx_addr event=U32(manager,0x18c);
   static const rx_addr names[]={0x94d6b8,0x94c150,0x94c144,0x94c138,0x94d910};
   uint32_t values[5];for(unsigned j=0;j<5;j++)values[j]=act_cell(standings,names[j],row);
   values[0]=position<12?U32(event,0xb8+4*position):0;
   if(position<3)values[1+position]=1;
   rx_addr holeshot=act_vcall(event,0x44,0,0);                      /* vf17 (005b4995) */
   if(holeshot){
    rx_set_sp(F-4);                                                    /* push edx: stricmp's second argument (005b49a7) */
    holeshot=act_vcall(event,0x44,0,0);
    rx_set_sp(F);
    if(crt_stricmp(holeshot+0x14,standings_name(standings,row))==0)values[4]=1;
   }
   /* the write-back (005b4a0c..005b4c20): each value in its own frame slot (Points +0x14, Firsts +0x28, Seconds +0x24, Thirds +0x20,
    * Holeshots +0x18), the inline Table_SetCellInt: a changed value counts in the table's revision (+0x68), then 004f5330 at depth 3 */
   static const uint8_t slots[5]={0x14,0x28,0x24,0x20,0x18};
   for(unsigned j=0;j<5;j++){
    if((int32_t)values[j]<1)values[j]=0;
    SU32(F,slots[j],values[j]);
   }
   for(unsigned j=0;j<5;j++){
    const uint32_t col=rx_tbl_column_index(standings,names[j]);
    if(U32(F,slots[j])!=U32(rx_tbl_cell_word(standings,col,row),0))SU32(standings,0x68,U32(standings,0x68)+1);
    if(col>=((U32(standings,0x14)-U32(standings,0x10))>>2))crt_invalid_parameter_noinfo();
    ACT_CALL(3,rx_tbl_set_int(U32(U32(standings,0x10),4*col),row,F+slots[j]));
   }
   position++;
  }
  rx_tbl_rowset_destroy(sorted);rx_tbl_sort_criteria_destroy(criteria);
 }
 ACT_END(F);
}
RX_FUNCTION(0x005b43c0,rx_career_update_standings)
