/* Shared event vf24/25/26, real sorted-Rdb and driver identity owners. */
#include "career_moto.h"
#include "build_activity_util.h"
#include "build_tables_sort.h"
static rx_addr manager(const rx_career_single_dependencies*d){if(!d||!d->manager)act_unreached(0x413d20,"Moto standings manager");return d->manager();}
void rx_career_moto_reset(rx_addr e,const rx_career_single_dependencies*d){rx_career_event_base_reset(e,d);SU32(e,0xc,1);SU32(e,0x10,1);}
int32_t rx_career_event_standing_rank(rx_addr e,rx_addr name,uint32_t type,const rx_career_single_dependencies*d){
 ACT_FRAME_EBP(F,8,0x64,3);
 rx_addr criteria=F+0x14,set=F+0x38;rx_tbl_crit_vec_init(criteria);
 act_vcall(e,0x58,1,&criteria);
 rx_addr m=manager(d);ACT_CALL(2,rx_tbl_rowset_sorted_init(set,m+0x560,criteria));
 ACT_CALL(2,rx_career_event_select_standings(set,-3,d));int32_t result=-1,rank=0;
 for(rx_addr n=LEFT_OF(set);n!=U32(set,0x18);n=rx_tbl_rowset_next(n),rank++){
  rx_addr t=U32(n,0x14);uint32_t row=U32(n,0xc);
  rx_addr text=rx_tbl_str_cstr(rx_tbl_cell_string(act_column(t,0x936f00),row));
  if(act_cell(t,0x936ef4,row)==type&&!crt_stricmp(name,text)){result=rank;break;}
 }
 rx_tbl_rowset_destroy(set);rx_tbl_sort_criteria_destroy(criteria);ACT_END(F);return result;
}
void rx_career_event_sort_drivers(rx_addr activity,const rx_career_single_dependencies*d){
 ACT_FRAME_EBP(F,8,0x114,3);
 int32_t count=I32(activity,0x26c);rx_addr saved=F+0x50;
 crt_memcpy(saved,activity+0x2d0,(uint32_t)count*4);crt_memset(activity+0x2d0,0,(uint32_t)count*4);
 rx_addr m=manager(d);if(!d->player_table)act_unreached(0x5b6840,"Moto driver sort players");rx_addr t=d->player_table(m,2);
 rx_addr criteria=F+0x10,criterion=F+0x80,set=F+0x28;
 rx_tbl_crit_vec_init(criteria);rx_tbl_sort_criterion_init(criterion,0x94d6cc,0);
 rx_tbl_sort_criterion_push(criteria,criterion);rx_tbl_sort_criterion_destroy(criterion);
 ACT_CALL(3,rx_tbl_rowset_sorted_all(set,t,rx_tbl_row_count(t),criteria));
 for(int32_t i=0;i<count;i++){
  SU32(F,0xc,0xffffffff);rx_addr driver=U32(saved,4u*(uint32_t)i);
  (void)rx_career_event_driver_row(driver,set,F+0xc);int32_t ordinal=I32(F,0xc);
  if(ordinal>=0&&ordinal<count)SU32(activity,0x2d0+4u*(uint32_t)ordinal,driver);
 }
 rx_tbl_rowset_destroy(set);rx_tbl_sort_criteria_destroy(criteria);ACT_END(F);
}
void rx_career_event_set_standing_points(rx_addr name,int32_t points,int32_t series,const rx_career_single_dependencies*d){
 ACT_FRAME_EBP(F,8,0x44,3);
 rx_addr set=F+0x28;rx_tbl_rowset_init(set,0);ACT_CALL(2,rx_career_event_select_standings(set,series,d));
 for(rx_addr n=LEFT_OF(set);n!=U32(set,0x18);n=rx_tbl_rowset_next(n)){
  rx_addr t=U32(n,0x14);uint32_t row=U32(n,0xc);
  rx_addr text=rx_tbl_str_cstr(rx_tbl_cell_string(act_column(t,0x936f00),row));
  if(crt_stricmp(name,text))continue;
  const rx_addr value=F_sp+8;SU32(value,0,(uint32_t)points);uint32_t c=rx_tbl_column_index(t,0x94d6b8);if(U32(rx_tbl_cell_word(t,c,row),0)!=(uint32_t)points)SU32(t,0x68,U32(t,0x68)+1);ACT_CALL(3,rx_tbl_set_int(U32(U32(t,0x10),4*c),row,value));break;
 }
 rx_tbl_rowset_destroy(set);ACT_END(F);
}
