/* Complete 005b20b0 semantic handport. No credits or unlocks inferred from finishing an exhibition.
 * Source checkpoint only: original scratch/volatile-register parity remains to be measured.
 * Canonical shop leaves are separately owned; no original-call/default reward fallback. */
#include "career_rewards.h"
#include "career_properties.h"
#include "career_progress.h"
#include "highlight_progress.h"
#include "career_shop.h"
#include "build_activity_util.h"
static rx_addr reward_column(rx_addr t,rx_addr name){uint32_t c;ACT_CALLR(c,1,rx_tbl_column_index(t,name));if(c>=((U32(t,0x14)-U32(t,0x10))>>2))crt_invalid_parameter_noinfo();return U32(U32(t,0x10),4*c);}
static uint32_t reward_word(rx_addr t,rx_addr name,uint32_t row){uint32_t c;ACT_CALLR(c,1,rx_tbl_column_index(t,name));rx_addr p;ACT_CALLR(p,1,rx_tbl_cell_word(t,c,row));return U32(p,0);}
/* The source keeps iterator/proxy validation: the shop callback can mutate player tables,
 * while these nodes belong to immutable authored SeriesRewards* tables. */
static rx_addr reward_node(rx_addr proxy,rx_addr node,rx_addr expected_proxy,rx_addr end){
 if(!proxy||proxy!=expected_proxy)crt_invalid_parameter_noinfo();
 if(node!=end&&node==U32(U32(proxy,0),0x18))crt_invalid_parameter_noinfo();
 return node;
}
void rx_career_rewards_apply(rx_addr r,uint32_t controller){
 ACT_FRAME(F,0x14,4);
 if(U32(r,0xfc)||U32(r,0x100)||U32(r,0x104)){
  if(U32(r,0x104)){
   rx_addr t;ACT_CALLR(t,1,rx_tbl_find(RX_TBL_DEFAULT_DB,0x94d898));
   const uint32_t series=U32(U32(r,0),0x20);SU32(F,0x14,series);
   rx_addr set;ACT_CALLR(set,1,rx_tbl_rows(reward_column(t,0x94c990),(int32_t)series));
   rx_addr node=U32(U32(set,0x18),0),proxy=U32(set,0);
   SU32(F,0x14,set);SU32(F,0x18,proxy);SU32(F,0x1c,node);
   for(;;){const rx_addr end=U32(set,0x18);reward_node(proxy,node,U32(set,0),end);if(node==end)break;
    const uint32_t product=reward_word(t,0x936ed4,U32(node,0xc));uint32_t locked;
    ACT_CALLR(locked,2,rx_career_shop_is_locked(U32(0,0xd72a34),(int32_t)controller,(int32_t)product));
    if(locked)ACT_CALL(3,rx_career_shop_unlock(U32(0,0xd72a34),(int32_t)controller,(int32_t)product,1));
    ACT_CALLR(node,0,rx_tbl_rowset_next(node));SU32(F,0x1c,node);
   }
   /* The standalone product is intentionally unconditionally unlocked, unlike each table row. */
   if(I32(r,0xec)!=-1)ACT_CALL(3,rx_career_shop_unlock(U32(0,0xd72a34),(int32_t)controller,I32(r,0xec),1));
  }
  if(U32(r,0x100)){
   rx_addr t;ACT_CALLR(t,1,rx_tbl_find(RX_TBL_DEFAULT_DB,0x94d8bc));
   const uint32_t series=U32(U32(r,0),0x20);SU32(F,0x14,series);
   rx_addr set;ACT_CALLR(set,1,rx_tbl_rows(reward_column(t,0x94c990),(int32_t)series));
   rx_addr node=U32(U32(set,0x18),0),proxy=U32(set,0);
   SU32(F,0x14,set);SU32(F,0x18,proxy);SU32(F,0x1c,node);
   for(;;){const rx_addr end=U32(set,0x18);reward_node(proxy,node,U32(set,0),end);if(node==end)break;
    const rx_addr c=reward_column(t,0x93e63c),v=U32(c,0x40);const uint32_t row=U32(node,0xc);
    if(row>=((U32(v,0x10)-U32(v,0xc))>>2))crt_invalid_parameter_noinfo();
    const uint32_t asset=U32(U32(v,0xc),row*4);uint32_t purchased;
    ACT_CALLR(purchased,1,rx_career_shop_is_purchased((int32_t)controller,(int32_t)asset));
    if(!purchased)ACT_CALL(2,rx_career_shop_purchase(U32(0,0xd72a34),(int32_t)controller,(int32_t)asset));
    ACT_CALLR(node,0,rx_tbl_rowset_next(node));SU32(F,0x1c,node);
   }
  }
  if(U32(r,0xfc)){const uint32_t amount=U32(r,0xf8)+U32(r,0xf4);ACT_CALL(1,rx_career_shop_add_credits((int32_t)controller,(int32_t)amount));}
 }
 ACT_END(F);
}
RX_FUNCTION(0x005b20b0,rx_career_rewards_apply)
/* 006d80e0 (EDI tracking, ESI controller, stack series/place, RET8).
 * Exact authored switch indices and short-circuit aggregate checks; no reward thresholds inferred. */
void rx_career_series_highlights(rx_addr tracking,uint32_t controller,int32_t series,int32_t place){
 ACT_FRAME(F,4,2);
 if(place<3){uint32_t index=0xffffffffu;
  switch(series){case 4:index=0;break;case 0x42:index=1;break;case 0x51:index=2;break;case 8:index=3;break;case 0xc:index=4;break;case 0x13:index=5;break;case 0x23:index=6;break;case 0x27:index=7;break;case 0x2e:index=8;break;case 0x17:index=9;break;case 0x1a:index=10;break;case 0x1f:index=11;break;case 0x33:index=12;break;case 0x37:index=13;break;case 0x3e:index=14;break;case 0x45:index=15;break;case 0x48:index=16;break;case 0x4d:index=17;break;default:break;}
  if(index!=0xffffffffu)ACT_CALL(3,(void)rx_highlight_add_progress(tracking,controller,index));
 }
 uint32_t all=1;
 for(uint32_t index=0;index<18;index++){uint32_t unlocked;ACT_CALLR(unlocked,3,rx_highlight_is_unlocked(tracking,controller,index));if(!unlocked){all=0;break;}}
 if(all)ACT_CALL(3,(void)rx_highlight_add_progress(tracking,controller,0x12));
 if(place==0){
  switch(series){case 0x14:case 0x2f:case 0x3f:case 0x4e:ACT_CALL(3,(void)rx_highlight_add_progress(tracking,controller,0x13));break;default:break;}
  if(series<0x54||series>0x58)ACT_CALL(3,(void)rx_highlight_add_progress(tracking,controller,0x14));
 }
 ACT_END(F);
}
RX_FUNCTION(0x006d80e0,rx_career_series_highlights)
static void visible_reward(rx_addr target,rx_addr name,uint32_t value,rx_addr F){
 const rx_addr v=F+0x20,c=F+0x50;ACT_CALL(0,rx_tbl_crit_vec_init(v));
 ACT_CALL(1,rx_tbl_crit_init_int(c,name,0,value));ACT_CALL(1,rx_tbl_crit_push(v,c));ACT_CALL(0,rx_tbl_crit_destroy(c));
 ACT_CALL(2,(void)rx_tbl_add_row(target,F+0x104,v));ACT_CALL(0,rx_tbl_crit_vec_destroy(v));
}
static uint32_t checked_reward_word(rx_addr t,rx_addr name,uint32_t row){const rx_addr col=reward_column(t,name),v=U32(col,0x40);if(row>=((U32(v,0x10)-U32(v,0xc))>>2))crt_invalid_parameter_noinfo();return U32(U32(v,0xc),row*4);}
/* 005b1530: EAX zero-based place, stack(rewards,controller), RET8. */
void rx_career_rewards_calculate(rx_addr r,uint32_t controller,int32_t place){
 ACT_FRAME(F,0x104,4);SU32(r,0xf0,(uint32_t)place);SU32(r,0xfc,0);SU32(r,0x100,0);SU32(r,0x104,0);
 ACT_CALL(1,rx_tbl_table_clear_rows(r+4));ACT_CALL(1,rx_tbl_table_clear_rows(r+0x78));SU32(r,0xec,0xffffffffu);
 const uint32_t improved=I32(r,0xf0)<I32(U32(r,0),0x24)&&I32(r,0xf0)<3;
 SU32(F,0x4c,improved);
 if(improved){
  SU32(r,0x104,1);rx_addr t;ACT_CALLR(t,1,rx_tbl_find(RX_TBL_DEFAULT_DB,0x94d898));
  rx_addr set;ACT_CALLR(set,1,rx_tbl_rows(reward_column(t,0x94c990),I32(U32(r,0),0x20)));
  rx_addr node=U32(U32(set,0x18),0),proxy=U32(set,0);SU32(F,0x14,set);SU32(F,0x38,proxy);SU32(F,0x3c,node);
  for(;;){const rx_addr end=U32(set,0x18);reward_node(proxy,node,U32(set,0),end);if(node==end)break;
   const uint32_t row=U32(node,0xc),product=reward_word(t,0x936ed4,row),hidden=reward_word(t,0x94d8ac,row);
   if(!hidden){uint32_t locked;ACT_CALLR(locked,2,rx_career_shop_is_locked(U32(0,0xd72a34),(int32_t)controller,(int32_t)product));if(locked)visible_reward(r+4,0x936ed4,product,F);}
   ACT_CALLR(node,0,rx_tbl_rowset_next(node));SU32(F,0x3c,node);
  }
  if(I32(U32(r,0),0x54)!=-1){uint32_t activity;ACT_CALLR(activity,1,rx_career_event_activity_type(U32(U32(r,0),0x50)));
   if(activity==8||activity==0||activity==1||activity==0x11||activity==0x13||activity==5||activity==6){
    rx_addr etl;ACT_CALLR(etl,1,rx_tbl_find(RX_TBL_DEFAULT_DB,0x936e7c));rx_addr ref;ACT_CALLR(ref,1,rx_tbl_row(reward_column(etl,0x93e228),I32(U32(r,0),0x54)));
    if(U32(ref,8)&&I32(ref,0)!=-1){const uint32_t product=reward_word(U32(ref,8),0x936ed4,U32(ref,0));uint32_t locked;
     ACT_CALLR(locked,2,rx_career_shop_is_locked(U32(0,0xd72a34),(int32_t)controller,(int32_t)product));
     if(locked){SU32(r,0xec,product);visible_reward(r+4,0x936ed4,product,F);}
    }
   }
  }
  /* Highlight controller comes from progress, unlike the purchase/unlock presentation controller argument. */
  ACT_CALL(2,rx_career_series_highlights(U32(0,0xd92edc),U32(U32(r,0),0x1c),I32(U32(r,0),0x20),I32(r,0xf0)));
  SU32(r,0x100,1);ACT_CALLR(t,1,rx_tbl_find(RX_TBL_DEFAULT_DB,0x94d8bc));
  ACT_CALLR(set,1,rx_tbl_rows(reward_column(t,0x94c990),I32(U32(r,0),0x20)));
  node=U32(U32(set,0x18),0);proxy=U32(set,0);SU32(F,0x14,set);SU32(F,0x38,proxy);SU32(F,0x3c,node);
  for(;;){const rx_addr end=U32(set,0x18);reward_node(proxy,node,U32(set,0),end);if(node==end)break;
   const uint32_t row=U32(node,0xc),asset=checked_reward_word(t,0x93e63c,row),hidden=checked_reward_word(t,0x94d8ac,row);
   if(!hidden){uint32_t purchased;ACT_CALLR(purchased,1,rx_career_shop_is_purchased((int32_t)controller,(int32_t)asset));if(!purchased)visible_reward(r+0x78,0x93e63c,asset,F);}
   ACT_CALLR(node,0,rx_tbl_rowset_next(node));SU32(F,0x3c,node);
  }
 }
 /* Base prize is looked up even when this is not a personal-best medal result. Bonus is gated by improved. */
 SU32(r,0xf4,0);SU32(r,0xf8,0);rx_addr points;ACT_CALLR(points,1,rx_tbl_find(RX_TBL_DEFAULT_DB,0x94d8d4));
 const rx_addr v=F+0x20,c=F+0x50,ref=F+0x40;ACT_CALL(0,rx_tbl_crit_vec_init(v));
 ACT_CALL(1,rx_tbl_crit_init_int(c,0x94d830,0,U32(U32(r,0),0x44)));ACT_CALL(1,rx_tbl_crit_push(v,c));ACT_CALL(0,rx_tbl_crit_destroy(c));
 ACT_CALL(1,rx_tbl_crit_init_int(c,0x94d8e4,0,U32(r,0xf0)+1u));ACT_CALL(1,rx_tbl_crit_push(v,c));ACT_CALL(0,rx_tbl_crit_destroy(c));
 SU32(ref,0,0xffffffffu);SU32(ref,8,0);ACT_CALL(3,rx_tbl_find_first(points,v,ref));
 if(U32(ref,8)&&I32(ref,0)!=-1){SU32(r,0xf4,reward_word(U32(ref,8),0x94d8ec,U32(ref,0)));if(improved)SU32(r,0xf8,U32(r,0xf8)+reward_word(U32(ref,8),0x94d8f4,U32(ref,0)));}
 ACT_CALL(0,rx_tbl_crit_vec_destroy(v));if((int32_t)(U32(r,0xf8)+U32(r,0xf4))>0)SU32(r,0xfc,1);
 const uint32_t has=U32(r,0xfc)||U32(r,0x100)||U32(r,0x104);
 /* Existing explicit UI boundary only. Authoritative gameplay state is the actual block flags/tables above. */
#ifdef RX_ORACLE
 {const uint32_t args[]={0x94d884,has};(void)act_ui_call(0x005dbfb0,RX_THISCALL,U32(0,0xa93fcc),2,args);}
#else
 rx_owned_property_set_int(0x94d884,has);
#endif
 ACT_END(F);
}
RX_FUNCTION(0x005b1530,rx_career_rewards_calculate)
