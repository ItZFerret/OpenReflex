/* Complete local-data semantics of004e8350/004e88a0/004e8920/004e96e0/004e9820.
 * One canonical Rdb owner supplies criteria, rows, defaults, indexes and revisions.
 * Native functional candidate; full original CPU/allocator-scratch parity pending.
 * PurchaseStoreAsset grants ownership, it does NOT charge money or test shop price.
 * UI AccountBalance publication uses the same native UI-only boundary as construction.
 */
#include "career_shop.h"
#ifndef RX_ORACLE
#include "career_properties.h"
#endif
#include "build_tables.h"
#include "build_vehicle.h"
#include "build_activity_util.h"
#include "build_activity_freeride_run.h"
#include "records_leave_highlights.h"
#include "race_extern.h"
#include "fx.h"
#include "crt.h"

enum {
 N_RIDER=0x93e53c,N_BALANCE=0x93e5b4,N_EARNINGS=0x93e5c4,
 N_PURCHASED=0x93e64c,N_ASSET=0x93e63c,N_DLC_ASSETS=0x93e664,N_DLC=0x93e674,N_PLAYER_DLC=0x93e67c,
 N_MODEL=0x93e180,N_MODEL_ID=0x93e170,N_MODEL_PARTS=0x93e688,N_PART=0x93e69c,N_PART_ID=0x93e29c,
 N_PLAYER_VEHICLE=0x93e568,N_PLAYER_ID=0x93e578,N_PLAYER_PARTS=0x93e6a8,N_PLAYER_TUNE=0x93e6bc,
 N_TYPE=0x936ce4,N_MODEL_TYPE=0x93e588,N_FAVORITE=0x93e5a8,N_DEFAULT=0x93e6d0,
 N_PART_TYPE=0x93e508,N_ATTRIBUTE=0x93e6d8,N_ENGINE=0x93e6e8,N_SUSPENSION=0x93e720,N_TIRES=0x93e72c,
 N_TUNE1=0x93e6f0,N_TUNE2=0x93e708,N_ENGINE1=0x93dcfc,N_ENGINE2=0x93dcf4,
 N_SUSP1=0x93dce8,N_SUSP2=0x93dcdc,N_TIRE1=0x93e734,N_TIRE2=0x93e73c,
 N_UNLOCKED=0x93e744,N_PRODUCT=0x936ed4,N_COUNT=0x93e75c,N_REQUIREMENT=0x93e7b0,
 N_DLC_PRODUCTS=0x93e768,N_NEW=0x93e77c,N_PRODUCTS=0x93e790
};
typedef struct {rx_addr name;uint32_t value;} shop_field;
static rx_addr table(rx_addr db,rx_addr name){rx_addr t;RACE_CORE_CALL(1,t=rx_tbl_find(db,name));if(!t)crt_invalid_parameter_noinfo();return t;}
static rx_addr player_db(int32_t c){if(c<0||c>3)crt_invalid_parameter_noinfo();return RX_TBL_PLAYER_DB(c);}
static uint32_t column(rx_addr t,rx_addr n){uint32_t c;RACE_CORE_CALL(1,c=rx_tbl_column_index(t,n));if(c>=((U32(t,0x14)-U32(t,0x10))>>2))crt_invalid_parameter_noinfo();return c;}
static uint32_t get(rx_addr t,rx_addr n,uint32_t row){const uint32_t c=column(t,n);rx_addr p;RACE_CORE_CALL(1,p=rx_tbl_cell_word(t,c,row));return U32(p,0);}
static int32_t by(rx_addr t,rx_addr name,int32_t id){RACE_FRAME_PLAIN(F,4,0);SU32(F,0,(uint32_t)id);rx_addr r;RACE_CORE_CALL(1,r=rx_fx_table_row_by(t,name,F));const int32_t out=U32(r,8)&&U32(r,0)!=0xffffffffu?I32(r,0):-1;RACE_FRAME_END(F);return out;}
static void set(rx_addr t,rx_addr name,uint32_t row,uint32_t val){RACE_FRAME_PLAIN(F,4,0);SU32(F,0,val);RACE_CORE_CALL(2,rx_tbl_set_cell_int(t,name,row,F));RACE_FRAME_END(F);}
static void criteria(rx_addr v,rx_addr c,const shop_field*f,uint32_t n){RACE_CORE_CALL(0,rx_tbl_crit_vec_init(v));for(uint32_t i=0;i<n;++i){RACE_CORE_CALL(1,rx_tbl_crit_init_int(c,f[i].name,0,f[i].value));RACE_CORE_CALL(1,rx_tbl_crit_push(v,c));RACE_CORE_CALL(0,rx_tbl_crit_destroy(c));}}
static void append(rx_addr t,const shop_field*f,uint32_t n){RACE_FRAME_PLAIN(F,0x110,0);const rx_addr v=F+0x20,c=F+0x40;criteria(v,c,f,n);RACE_CORE_CALL(2,rx_tbl_add_row(t,F,v));RACE_CORE_CALL(0,rx_tbl_crit_vec_destroy(v));RACE_FRAME_END(F);}
static int32_t first(rx_addr t,const shop_field*f,uint32_t n){RACE_FRAME_PLAIN(F,0x110,0);const rx_addr v=F+0x20,c=F+0x40;criteria(v,c,f,n);RACE_CORE_CALL(3,rx_tbl_find_first(t,v,F));const int32_t row=U32(F,8)&&U32(F,0)!=0xffffffffu?I32(F,0):-1;RACE_CORE_CALL(0,rx_tbl_crit_vec_destroy(v));RACE_FRAME_END(F);return row;}
static void select_rows(rx_addr t,rx_addr out,const shop_field*f,uint32_t n){RACE_FRAME_PLAIN(F,0x100,0);criteria(F,F+0x20,f,n);RACE_CORE_CALL(1,rx_tbl_rowset_init(out,0));RACE_CORE_CALL(1,rx_tbl_select(t,out,F));RACE_CORE_CALL(0,rx_tbl_crit_vec_destroy(F));RACE_FRAME_END(F);}
static void dirty_profile(int32_t controller){const rx_addr mgr=U32(0,0xd92ee0);const rx_addr p=mgr?U32(mgr,4+4u*(uint32_t)controller):0;if(p&&U32(p,4)&&U32(p,0x2c)&&U32(p,8))SU32(p,0x14,1);}
static void add_dlc(int32_t controller,rx_addr mapping,rx_addr key,int32_t id){const int32_t row=by(mapping,key,id);if(row<0)return;const uint32_t dlc=get(mapping,N_DLC,(uint32_t)row);const rx_addr p=table(player_db(controller),N_PLAYER_DLC);const shop_field f[]={{N_DLC,dlc}};if(first(p,f,1)<0){append(p,f,1);dirty_profile(controller);}}

void rx_career_shop_add_credits(int32_t controller,int32_t amount){
 RACE_FRAME_EBP(F,8,4,3);const rx_addr t=table(player_db(controller),N_RIDER);
 const uint32_t balance=get(t,N_BALANCE,0)+(uint32_t)amount;set(t,N_BALANCE,0,balance);
#ifdef RX_ORACLE
 {const uint32_t args[2]={N_BALANCE,balance};(void)act_ui_call(0x5dbfb0,RX_THISCALL,U32(0,0xa93fcc),2,args);}
#else
 /*Original004e83df publishes this same computedbalance before earnings.*/
 rx_owned_property_set_int(N_BALANCE,balance);
#endif
 set(t,N_EARNINGS,0,get(t,N_EARNINGS,0)+(uint32_t)amount);RACE_FRAME_END(F);
}
RX_FUNCTION(0x004e8350,rx_career_shop_add_credits)
uint32_t rx_career_shop_is_purchased(int32_t controller,int32_t asset){RACE_FRAME_PLAIN(F,0,2);const uint32_t yes=by(table(player_db(controller),N_PURCHASED),N_ASSET,asset)>=0;RACE_FRAME_END(F);return yes;}
RX_FUNCTION(0x004e88a0,rx_career_shop_is_purchased)
uint32_t rx_career_shop_is_locked(rx_addr helper,int32_t controller,int32_t product){
 RACE_FRAME_PLAIN(F,4,4);(void)helper;uint32_t locked=0;
 if(controller==-1){uint32_t present[4];for(uint32_t c=0;c<4;++c){RACE_CORE_CALL(1,present[c]=rx_player_controller_present(c));}for(uint32_t c=0;c<4;++c)if(present[c]){uint32_t r;RACE_CORE_CALL(2,r=rx_career_shop_is_locked(helper,(int32_t)c,product));locked|=r;}}
 else {const rx_addr t=table(player_db(controller),N_UNLOCKED);const int32_t row=by(t,N_PRODUCT,product);if(row<0)locked=1;else{const int32_t count=(int32_t)get(t,N_COUNT,(uint32_t)row);const rx_addr products=table(RX_TBL_DEFAULT_DB,N_PRODUCTS);const int32_t pr=by(products,N_PRODUCT,product);if(pr<0)crt_invalid_parameter_noinfo();locked=count<(int32_t)get(products,N_REQUIREMENT,(uint32_t)pr);}}
 RACE_FRAME_END(F);return locked;
}
RX_FUNCTION(0x004e96e0,rx_career_shop_is_locked)
void rx_career_shop_unlock(rx_addr helper,int32_t controller,int32_t product,int32_t mark_new){
 RACE_FRAME_EBP(F,8,0x10c,3);uint32_t locked;RACE_CORE_CALL(2,locked=rx_career_shop_is_locked(helper,controller,product));
 if(locked&&controller>=0){const rx_addr t=table(player_db(controller),N_UNLOCKED);const int32_t row=by(t,N_PRODUCT,product);if(row<0){const shop_field f[]={{N_PRODUCT,(uint32_t)product},{N_COUNT,1}};append(t,f,2);add_dlc(controller,table(RX_TBL_DEFAULT_DB,N_DLC_PRODUCTS),N_PRODUCT,product);if(mark_new){const shop_field n[]={{N_PRODUCT,(uint32_t)product}};append(table(player_db(controller),N_NEW),n,1);}}
 else set(t,N_COUNT,(uint32_t)row,get(t,N_COUNT,(uint32_t)row)+1u);}
 RACE_FRAME_END(F);
}
RX_FUNCTION(0x004e9820,rx_career_shop_unlock)

void rx_career_shop_purchase(rx_addr helper,int32_t controller,int32_t asset){
 RACE_FRAME_EBP(F,8,0x18c,3);uint32_t owned;RACE_CORE_CALL(1,owned=rx_career_shop_is_purchased(controller,asset));if(owned)goto done;
 const rx_addr db=player_db(controller);const shop_field own[]={{N_ASSET,(uint32_t)asset}};append(table(db,N_PURCHASED),own,1);
 add_dlc(controller,table(RX_TBL_DEFAULT_DB,N_DLC_ASSETS),N_ASSET,asset);
 const rx_addr models=table(RX_TBL_DEFAULT_DB,N_MODEL),parts=table(RX_TBL_DEFAULT_DB,N_MODEL_PARTS);(void)table(RX_TBL_DEFAULT_DB,N_PART);
 const shop_field model_match[]={{N_ASSET,(uint32_t)asset}};const int32_t model_row=first(models,model_match,1);if(model_row<0)goto done;
 const uint32_t model=get(models,N_MODEL_ID,(uint32_t)model_row),type=get(models,N_TYPE,(uint32_t)model_row);
 const rx_addr vehicles=table(db,N_PLAYER_VEHICLE),player_parts=table(db,N_PLAYER_PARTS),tuning=table(db,N_PLAYER_TUNE);
 uint32_t next=0,n;RACE_CORE_CALL(0,n=rx_tbl_row_count(vehicles));for(uint32_t r=0;r<n;++r){const int32_t id=(int32_t)get(vehicles,N_PLAYER_ID,r);if((int32_t)next<=id)next=(uint32_t)id+1u;}
 const shop_field favorite[]={{N_MODEL_TYPE,type},{N_FAVORITE,1}};const uint32_t first_type=first(vehicles,favorite,2)<0;
 const shop_field veh[]={{N_PLAYER_ID,next},{N_MODEL_ID,model},{N_FAVORITE,first_type}};append(vehicles,veh,3);
 const shop_field defaults[]={{N_MODEL_ID,model},{N_DEFAULT,1}};const rx_addr set_=F+0x158;select_rows(parts,set_,defaults,2);
 uint32_t tune[6]={0,0,0,0,0,0};const rx_addr head=U32(set_,0x18);
 for(rx_addr node=U32(head,0);node!=head;node=rx_tbl_rowset_next(node)){
  const uint32_t row=U32(node,0xc),part_asset=get(parts,N_ASSET,row);RACE_CORE_CALL(1,owned=rx_career_shop_is_purchased(controller,(int32_t)part_asset));if(!owned){RACE_CORE_CALL(2,rx_career_shop_purchase(helper,controller,(int32_t)part_asset));}
  const uint32_t part_id=get(parts,N_PART_ID,row);const uint32_t pc=column(parts,N_PART_ID);rx_addr ref;RACE_CORE_CALL(1,ref=rx_tbl_column_parent_ref(U32(U32(parts,0x10),4*pc),row));if(!U32(ref,8)||U32(ref,0)==0xffffffffu)crt_invalid_parameter_noinfo();
  const rx_addr pt=U32(ref,8);const uint32_t pr=U32(ref,0),tc=column(pt,N_PART_TYPE);RACE_CORE_CALL(1,ref=rx_tbl_column_parent_ref(U32(U32(pt,0x10),4*tc),pr));if(!U32(ref,8)||U32(ref,0)==0xffffffffu)crt_invalid_parameter_noinfo();
  const rx_addr type_table=U32(ref,8);const uint32_t type_row=U32(ref,0),ac=column(type_table,N_ATTRIBUTE);rx_addr str;RACE_CORE_CALL(1,str=rx_tbl_cell_string(U32(U32(type_table,0x10),4*ac),type_row));const rx_addr text=rx_tbl_str_cstr(str);
  int slot=-1;if(crt_stricmp(text,N_ENGINE)==0)slot=0;else if(crt_stricmp(text,N_SUSPENSION)==0)slot=2;else if(crt_stricmp(text,N_TIRES)==0)slot=4;
  if(slot>=0){tune[slot]=get(pt,N_TUNE1,pr);tune[slot+1]=get(pt,N_TUNE2,pr);}
  const shop_field installed[]={{N_PLAYER_ID,next},{N_PART_ID,part_id}};append(player_parts,installed,2);
 }
 const shop_field tuning_fields[]={{N_PLAYER_ID,next},{N_ENGINE1,tune[0]},{N_ENGINE2,tune[1]},{N_SUSP1,tune[2]},{N_SUSP2,tune[3]},{N_TIRE1,tune[4]},{N_TIRE2,tune[5]}};append(tuning,tuning_fields,7);
 RACE_CORE_CALL(0,rx_tbl_rowset_destroy(set_));
 done:RACE_FRAME_END(F);
}
RX_FUNCTION(0x004e8920,rx_career_shop_purchase)
