/* Complete semantic population/roster source checkpoint, not registered.
 * Existing canonical Rdb/AI/prune kernels retain their unique ownership. */
#include "career_players.h"
#include "career_manager.h"
#include "career_single.h"
#include "build_activity_util.h"
static void required(int ok,uint32_t pc){if(!ok)act_unreached(pc,"unclosed career player dependency");}
static void ci(rx_addr v,rx_addr c,rx_addr name,uint32_t value){
 rx_tbl_crit_init_int(c,name,0,value);rx_tbl_crit_push(v,c);rx_tbl_crit_destroy(c);
}
static void cf(rx_addr v,rx_addr c,rx_addr name,float value){
 /*004f73b0's numeric Variant constructor differs from int only in type2
  * and raw scalar payload; use the shared canonical criterion path allocator. */
 rx_tbl_crit_init_int(c,name,0,0);SU32(c,0x88,2);SF32(c,0x90,value);
 rx_tbl_crit_push(v,c);rx_tbl_crit_destroy(c);
}
static void cs(rx_addr v,rx_addr c,rx_addr name,rx_addr text){
 rx_tbl_crit_init_string(c,name,0,text);rx_tbl_crit_push(v,c);rx_tbl_crit_destroy(c);
}
static void validate(rx_addr set,rx_addr proxy,rx_addr node){
 if(!proxy||proxy!=U32(set,0))crt_invalid_parameter_noinfo();
 if(node!=U32(set,0x18)&&node==U32(U32(proxy,0),0x18))crt_invalid_parameter_noinfo();
}
static void vec_init(rx_addr v){rx_addr px=rx_reflex_aligned_alloc(4);if(px)SU32(px,0,v);SU32(v,0,px);SU32(v,0xc,0);SU32(v,0x10,0);SU32(v,0x14,0);}
static void vec_destroy(rx_addr v){if(U32(v,0xc))rx_reflex_aligned_free(U32(v,0xc));rx_addr px=U32(v,0);SU32(v,0xc,0);SU32(v,0x10,0);SU32(v,0x14,0);rx_reflex_aligned_free(px);}
void rx_career_series_ai_ids(rx_addr v,uint32_t series){
 ACT_FRAME(F,0x1c,3);
 rx_addr table=rx_tbl_find(0xae0910,0x94d9f8);
 rx_addr rows=rx_tbl_rows(act_column(table,0x94c990),(int32_t)series);
 if(U32(v,0x10)<U32(v,0xc))crt_invalid_parameter_noinfo();
 /*Original erases old elements without releasing capacity, then reserves exactly
  * the indexed result size before ordered AIRiderId publication. */
 SU32(v,0x10,U32(v,0xc));rx_tbl_vector_reserve_ptr(v,U32(rows,0x1c));
 rx_addr proxy=U32(rows,0),n=LEFT_OF(rows);
 for(;;){validate(rows,proxy,n);if(n==U32(rows,0x18))break;
  SU32(F,0x14,act_cell(table,0x94cf70,U32(n,0xc))); /*AIRiderId*/
  rx_tbl_vector_push_ptr(v,F+0x14);n=rx_tbl_rowset_next(n);
 }
 ACT_END(F);
}
uint32_t rx_career_player_vehicle_model(uint32_t row,const rx_career_player_dependencies*d){
 uint32_t id=act_cell(ACT_PLAYER_LIST,0x93df5c,row);if(id==UINT32_MAX)id=act_cell(ACT_PLAYER_LIST,0x936cb4,row);
 required(d&&d->vehicle_model,0x541040);return d->vehicle_model(U32(0xa946b8,0)+0x154b0,id);
}
void rx_career_populate_players(rx_addr manager,const rx_career_player_dependencies*d){
 ACT_FRAME(F,0x114,4);
 rx_addr destination=rx_career_player_table(manager,2);rx_tbl_table_clear_rows(destination);
 uint32_t next_id=0;int32_t sort=0;
 /*The two source-index traversals are distinct and ordered. Human identity is
  * generated sequentially; AI identity is the Rider List's authored OwnerRiderId. */
 for(uint32_t type=0;type<2;type++){
  rx_addr rows=rx_tbl_rows(act_column(ACT_PLAYER_LIST,0x936ef4),(int32_t)type);
  rx_addr proxy=U32(rows,0),n=LEFT_OF(rows);
  for(;;){validate(rows,proxy,n);if(n==U32(rows,0x18))break;
   uint32_t row=U32(n,0xc),player_id;
   if(!type){player_id=next_id++;}
   else {
    uint32_t rider=act_cell(ACT_PLAYER_LIST,0x93df70,row);if(rider==UINT32_MAX)rider=act_cell(ACT_PLAYER_LIST,0x93dec8,row);
    rx_addr ref=rx_tbl_row(act_column(ACT_RIDER_LIST,0x93dec8),(int32_t)rider);
    if(!U32(ref,8)||U32(ref,0)==UINT32_MAX){n=rx_tbl_rowset_next(n);continue;}
    player_id=act_cell(U32(ref,8),0x93dfb0,U32(ref,0));
    if(player_id==UINT32_MAX){n=rx_tbl_rowset_next(n);continue;}
   }
   const rx_addr v=F+0x3c,c=F+0x58,out=F+0x30;
   rx_tbl_crit_vec_init(v);
   ci(v,c,0x94d6c0,player_id);ci(v,c,0x94d650,0);ci(v,c,0x94d670,0);
   cf(v,c,0x94d6cc,(float)sort);sort=(int32_t)((uint32_t)sort+1);
   ci(v,c,0x936ef4,type);
   rx_addr name=rx_tbl_cell_string(act_column(ACT_PLAYER_LIST,type?0x936f00:0x93deb8),row);
   cs(v,c,0x936f00,rx_tbl_str_cstr(name));
   ci(v,c,0x93e170,rx_career_player_vehicle_model(row,d));ci(v,c,0x94d78c,0);ci(v,c,0x94d798,0);
   rx_tbl_add_row(destination,out,v);rx_tbl_crit_vec_destroy(v);
   n=rx_tbl_rowset_next(n);
  }
 }
 ACT_END(F);
}
void rx_career_setup_ai_riders(rx_addr m,const rx_career_player_dependencies*d){
 ACT_FRAME(F,0x18,4);int32_t desired=I32(m,0x70);
 if(desired==-1&&U32(m,0x18c)){required(d&&d->event_capacity,0x5b2750);desired=(int32_t)d->event_capacity(U32(m,0x18c));}
 const uint32_t humans=rx_career_local_player_count();
 const rx_addr ids=F+0x10;vec_init(ids);rx_career_series_ai_ids(ids,U32(m,0x20));
 int32_t fixed=(int32_t)((U32(ids,0x10)-U32(ids,0xc))>>2);
 int32_t remaining=(int32_t)((uint32_t)desired-(uint32_t)fixed-humans);if(remaining<1)remaining=0;
 for(int32_t i=0;i<(int32_t)((U32(ids,0x10)-U32(ids,0xc))>>2);i++)
  rx_ai_rider_add_of_class(U32(U32(ids,0xc),4*i),U32(m,0x64),0x3d,0,0);
 if(remaining>0)rx_ai_roster_build(U32(m,0x38),U32(0xa93fcc,0),(uint32_t)remaining,0x3d,0,U32(m,0x64));
 int32_t count=(int32_t)rx_tbl_row_count(ACT_PLAYER_LIST);
 for(int32_t row=0;row<count;row++){
  if(act_cell(ACT_PLAYER_LIST,0x93def8,(uint32_t)row)!=0)SU32(ACT_PLAYER_LIST,0x68,U32(ACT_PLAYER_LIST,0x68)+1);
  SU32(F,0x2c,0);rx_tbl_set_int(act_column(ACT_PLAYER_LIST,0x93def8),(uint32_t)row,F+0x2c);
  required(d&&d->reset_requests,0x4de2f0);d->reset_requests((uint32_t)row);
 }
 vec_destroy(ids);ACT_END(F);
}
