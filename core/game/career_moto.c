/* Original Moto 005aeeb0/5aefc0/5af1e0/5af2c0. SOURCE ONLY, unproved.
 * Carries real ScoreOne/ScoreTwo between the two motos. No synthetic finish. */
#include "career_moto.h"
#include "build_activity_util.h"
#include "rider_cap.h"
static rx_addr mgr(const rx_career_single_dependencies*d){if(!d||!d->manager)act_unreached(0x413d20,"Moto manager missing");return d->manager();}
static rx_addr players(const rx_career_single_dependencies*d){rx_addr m=mgr(d);if(!d->player_table)act_unreached(0x5b6840,"Moto players missing");return d->player_table(m,2);}
static void str(const rx_career_single_dependencies*d,rx_addr key,rx_addr value){if(!d->ui_string)act_unreached(0x5dbf50,"Moto presentation missing");d->ui_string(key,value);}
static void integer(const rx_career_single_dependencies*d,rx_addr key,uint32_t value){if(!d->ui_int)act_unreached(0x5dbfb0,"Moto presentation missing");d->ui_int(key,value);}
uint32_t rx_career_moto_player_state(uint32_t p){return p==1?4:p==6?2:p==7?3:0;}
void rx_career_moto_gate_result(rx_addr e,uint32_t value,const rx_career_single_dependencies*d){
 ACT_FRAME_EBP(F,8,0x24,3);
 if(value){SU32(e,0x10,1);rx_career_event_setup_activity(e,4,1,3,U32(e,0xb0),d);ACT_END(F);return;}
 if(!U32(e,4)){
  rx_addr t=players(d),rows=rx_tbl_rows(act_column(t,0x936ef4),0);
  for(rx_addr n=LEFT_OF(rows);n!=U32(rows,0x18);n=rx_tbl_rowset_next(n)){
   SU32(F,8,0x7f7fffff);rx_tbl_set_cell_float(t,0x94d6cc,U32(n,0xc),F+8);
  }
 }
 SU32(e,0x28,1);
 if(!d->apply_player_states||!d->request_player_states)act_unreached(0x5b39a0,"Moto state dependencies missing");
 ACT_CALL(0,d->apply_player_states(mgr(d)));
 uint32_t to=0,next=8;if(!U32(e,4)){to=2;next=6;}else if(U32(e,4)==1){to=3;next=7;}
 d->request_player_states(mgr(d),4,to);SU32(e,0x10,next);rx_career_event_gate(e,d);ACT_END(F);
}
void rx_career_moto_advance(rx_addr e,const rx_career_single_dependencies*d){
 (void)players(d);uint32_t phase=U32(e,0x10);
 if(phase==1){if(!U32(e,4))return;str(d,0x94c99c,0x94c128);rx_career_moto_gate_result(e,0,d);return;}
 if(phase!=6&&phase!=7)return;
 rx_career_moto_holeshot(e,1,d);
 rx_career_event_setup_activity(e,phase==6?2:3,1,U32(e,0xb4),U32(e,0xb0),d);
}
void rx_career_moto_post_event(rx_addr e,const rx_career_single_dependencies*d){
 ACT_FRAME_EBP(F,8,0x74,3);
 rx_addr t=players(d),a=U32(e,0x14);
 if(!U32(e,4)&&U32(e,0xc)==1){
  rx_addr rows=rx_tbl_rows(act_column(t,0x94d650),4);
  const int32_t count=I32(a,0x26c);
  for(int32_t i=0;i<count;i++){
   rx_addr driver=rx_stand(a, (uint32_t)i);int32_t row=rx_career_event_driver_row(driver,rows,0);if(row<0)continue;
   rx_tbl_set_cell_float(t,0x94d6cc,(uint32_t)row,driver+0x10c4);
   SU32(F,8,2);rx_tbl_set_cell_int(t,0x94d670,(uint32_t)row,F+8);
  }
  SU32(e,0x28,1);SU32(e,0x10,6);rx_career_event_gate(e,d);ACT_END(F);return;
 }
 uint32_t phase=U32(e,0xc);if(phase!=6&&phase!=7){ACT_END(F);return;}
 rx_addr rows=rx_tbl_rows(act_column(t,0x94d650),(int32_t)rx_career_moto_player_state(phase));
 const int32_t count=I32(a,0x26c);
 for(int32_t i=0;i<count;i++){
  int32_t row=rx_career_event_driver_row(rx_stand(a, (uint32_t)i),rows,0);if(row<0)continue;
  uint32_t points=i==0?25:i==1?22:i==2?20:i==3?18:(uint32_t)(20-i);
  SU32(F,0xc,points);ACT_CALL(2,rx_tbl_set_cell_int(t,phase==6?0x94d78c:0x94d798,(uint32_t)row,F+0xc));
  SF32(F,0x24,rx_subs(F32(0,0x92e134),(float)(int32_t)points));uint32_t c=rx_tbl_column_index(t,0x94d6cc);if(!(F32(rx_tbl_cell_word(t,c,(uint32_t)row),0)==F32(F,0x24)))SU32(t,0x68,U32(t,0x68)+1);ACT_CALL(3,rx_tbl_set_float(U32(U32(t,0x10),4*c),(uint32_t)row,F+0x24));
 }
 if(phase==6){
  if(!d->request_player_states)act_unreached(0x5b3890,"Moto state dependency missing");
  d->request_player_states(mgr(d),2,3);SU32(e,0x10,7);
  str(d,0x94c99c,0x94d7b4);integer(d,0x94d9e4,0);SU32(e,0x28,1);
 }else{
  /*Reread state index after all writes, retaining index iteration ordering.*/
  rows=rx_tbl_rows(act_column(t,0x94d650),(int32_t)rx_career_moto_player_state(U32(e,0xc)));
  for(rx_addr n=LEFT_OF(rows);n!=U32(rows,0x18);n=rx_tbl_rowset_next(n)){
   uint32_t row=U32(n,0xc);uint32_t sum=act_cell(t,0x94d78c,row)+act_cell(t,0x94d798,row);
   SF32(F,0x18,rx_subs(F32(0,0x9d78dc),(float)(int32_t)sum));uint32_t c=rx_tbl_column_index(t,0x94d6cc);if(!(F32(rx_tbl_cell_word(t,c,row),0)==F32(F,0x18)))SU32(t,0x68,U32(t,0x68)+1);ACT_CALL(3,rx_tbl_set_float(U32(U32(t,0x10),4*c),row,F+0x18));
  }
  str(d,0x94c99c,0x94c118);
  if(!d->refresh_standings||!d->standings)act_unreached(0x5b43c0,"Moto standings missing");
  d->refresh_standings(mgr(d));(void)mgr(d);
  d->standings(mgr(d),-3,(U32(0,0x9e2318)!=1)-2,1);
  (void)mgr(d);
  integer(d,0x94ca38,U32(0,0x9e2310)==0);
  (void)mgr(d);
  integer(d,0x94d9e4,1);
  SU32(e,0x30,1);
 }
 if(!d->ui_goto_hub)act_unreached(0x5db500,"Moto post presentation missing");
 str(d,0x94cc74,0x94d7a4);d->ui_goto_hub();
 ACT_END(F);
}
