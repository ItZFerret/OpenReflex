/* Authentic career progress metadata owners, handwritten from complete original spans.
 * Isolated source checkpoint: no native activation and no original equivalence claimed yet.
 * Table allocation/index/setter algorithms stay in the canonical Rdb owner. */
#include "career_progress.h"
#include "build_activity_util.h"
#include "build_series_init.h"
#include "build_mxgame.h"
#include "loop_player.h"
#include "fx_util.h"
#include "records_leave_highlights.h"
#include "career_sort_dependencies.h"
static rx_addr column(rx_addr t,rx_addr name){uint32_t c;ACT_CALLR(c,1,rx_tbl_column_index(t,name));if(c>=((U32(t,0x14)-U32(t,0x10))>>2))crt_invalid_parameter_noinfo();return U32(U32(t,0x10),4*c);}
static uint32_t word(rx_addr t,rx_addr name,uint32_t row){uint32_t c;rx_addr a;ACT_CALLR(c,1,rx_tbl_column_index(t,name));ACT_CALLR(a,1,rx_tbl_cell_word(t,c,row));return U32(a,0);}
static rx_addr text(rx_addr t,rx_addr name,uint32_t row){rx_addr a;ACT_CALLR(a,1,rx_tbl_cell_string(column(t,name),row));return rx_tbl_str_cstr(a);}
static rx_addr table(rx_addr db,rx_addr name){rx_addr t;ACT_CALLR(t,1,rx_tbl_find(db,name));return t;}
static rx_addr keyed(rx_addr t,rx_addr name,uint32_t key){rx_addr ref;ACT_CALLR(ref,1,rx_tbl_row(column(t,name),(int32_t)key));return ref;}
static void guard_manager(void){if(!(U32(0,0x9e275c)&1)){SU32(0,0x9e275c,U32(0,0x9e275c)|1);ACT_CALL(0,(void)rx_series_manager_construct());}}
static uint32_t enum_from_names(rx_addr values,uint32_t count,rx_addr s){for(uint32_t i=0;i<count;i++)if(crt_stricmp(s,U32(values,4*i))==0)return i;return count;}
uint32_t rx_career_series_format(rx_addr s){return enum_from_names(0x9d2380,10,s);}
RX_FUNCTION(0x005985a0,rx_career_series_format)
uint32_t rx_career_series_ai_format(rx_addr s){return enum_from_names(0x9d23a8,3,s);}
RX_FUNCTION(0x005985e0,rx_career_series_ai_format)
/* 004dce90 is an activity enum, distinct from EventTypeId and its 009d2220 names. */
uint32_t rx_career_activity_type_from_name(rx_addr s){return enum_from_names(0x9ccd40,0x3e,s);}
RX_FUNCTION(0x004dce90,rx_career_activity_type_from_name)
uint32_t rx_career_event_activity_type(uint32_t type){
 ACT_FRAME(F,0,4);const rx_addr t=table(RX_TBL_DEFAULT_DB,0x935e08);SU32(F,0x14,type);
 const rx_addr ref=keyed(t,0x936e90,type);uint32_t result=0x3e;
 if(U32(ref,8)&&I32(ref,0)!=-1)result=rx_career_activity_type_from_name(text(U32(ref,8),0x93e52c,U32(ref,0)));
 ACT_END(F);return result;
}
RX_FUNCTION(0x004e76c0,rx_career_event_activity_type)
/* Criteria are copied before their local Variant/path storage is destroyed. */
static void push_int(rx_addr v,rx_addr c,rx_addr name,uint32_t op,uint32_t value){ACT_CALL(1,rx_tbl_crit_init_int(c,name,op,value));ACT_CALL(1,rx_tbl_crit_push(v,c));ACT_CALL(0,rx_tbl_crit_destroy(c));}
static void push_string(rx_addr v,rx_addr c,rx_addr name,rx_addr value){ACT_CALL(2,rx_tbl_crit_init_string(c,name,0,value));ACT_CALL(1,rx_tbl_crit_push(v,c));ACT_CALL(0,rx_tbl_crit_destroy(c));}
/* 005b0690: resolve Series/EventNumber -> EventLocationId/TrackId, then the joined ETL and filename. */
void rx_career_progress_resolve_event(rx_addr p){
 ACT_FRAME_EBP(F,8,0xe4,3);const rx_addr v=F+0x14,c=F+0x40,ref=F+0x2c;
 ACT_CALL(0,rx_tbl_crit_vec_init(v));const rx_addr locations=table(RX_TBL_DEFAULT_DB,0x94d7d8),etl=table(RX_TBL_DEFAULT_DB,0x936e7c);
 (void)table(RX_TBL_DEFAULT_DB,0x93e7c4);(void)table(RX_TBL_DEFAULT_DB,0x93e86c);
 push_int(v,c,0x94c990,0,U32(p,0x20));push_int(v,c,0x94d7f0,0,U32(p,0x4c));
 SU32(ref,0,0xffffffffu);SU32(ref,8,0);ACT_CALL(3,rx_tbl_find_first(locations,v,ref));
 SU32(p,0x58,word(U32(ref,8),0x936e20,U32(ref,0)));SU32(p,0x5c,word(U32(ref,8),0x936e40,U32(ref,0)));
 ACT_CALL(0,rx_tbl_crit_vec_clear(v));
 push_int(v,c,0x936e40,0,U32(p,0x5c));push_int(v,c,0x936e20,0,U32(p,0x58));push_int(v,c,0x936e90,0,U32(p,0x50));
 SU32(ref,0,0xffffffffu);SU32(ref,8,0);ACT_CALL(3,rx_tbl_find_first(etl,v,ref));
 SU32(p,0x54,word(U32(ref,8),0x93e228,U32(ref,0)));
 rx_addr parent;ACT_CALLR(parent,0,rx_tbl_column_parent_ref(column(U32(ref,8),0x936e20),U32(ref,0)));
 SU32(p,0x60,text(U32(parent,8),0x93e7d4,U32(parent,0)));
 ACT_CALL(0,rx_tbl_crit_vec_destroy(v));ACT_END(F);
}
RX_FUNCTION(0x005b0690,rx_career_progress_resolve_event)
void rx_career_progress_load(rx_addr p,uint32_t controller,uint32_t id,uint32_t owner){
 ACT_FRAME_EBP(F,8,0x10,2);ACT_CALL(0,rx_series_progress_reset(p));SU32(p,0x20,id);SU32(p,8,owner);SU32(p,0x1c,controller);
 const rx_addr series=table(RX_TBL_DEFAULT_DB,0x93e86c),rules=table(RX_TBL_DEFAULT_DB,0x94d7fc);
 const rx_addr sr=keyed(series,0x94c990,id),rr=keyed(rules,0x94c990,id);
 SU32(p,0x50,word(U32(sr,8),0x936e90,U32(sr,0)));uint32_t a;ACT_CALLR(a,1,rx_career_event_activity_type(U32(p,0x50)));SU32(p,0x14,a);
 SU32(p,0x10,rx_career_series_format(text(U32(sr,8),0x94d810,U32(sr,0))));
 SU32(p,0x18,rx_career_series_ai_format(text(U32(sr,8),0x94d820,U32(sr,0))));
 SU32(p,0x70,0xffffffffu);SU32(p,0x6c,1);
 if(U32(rr,8)&&I32(rr,0)!=-1){SU32(p,0x70,word(U32(rr,8),0x94be78,U32(rr,0)));SU32(p,0x6c,word(U32(rr,8),0x93dd58,U32(rr,0)));}
 SU32(p,0x44,word(U32(sr,8),0x94d830,U32(sr,0)));SU32(p,0x68,word(U32(sr,8),0x936cd4,U32(sr,0)));SU32(p,0x48,word(U32(sr,8),0x94d840,U32(sr,0)));
 SU32(p,0x4c,1);SU32(p,0x64,0xffffffffu);ACT_CALL(1,rx_career_progress_resolve_event(p));guard_manager();
 /* Original uses the singleton's controller for the Player DB lookup, even when p is supplied. */
 const rx_addr statuses=table(RX_TBL_PLAYER_DB(U32(0,0x9e21a4)),0x94d340),st=keyed(statuses,0x94c990,U32(p,0x20));
 if(U32(st,8)&&I32(st,0)!=-1){SU32(p,0x24,word(U32(st,8),0x94d36c,U32(st,0)));SU32(p,0x28,word(U32(st,8),0x94d37c,U32(st,0)));SU32(p,0x2c,word(U32(st,8),0x94d388,U32(st,0)));SU32(p,0x30,word(U32(st,8),0x94d398,U32(st,0)));}
 const rx_addr settings=U32(0,0xd92efc);const int32_t primary=I32(settings,0x16c);uint32_t difficulty;
 if(I32(0,0x9c4ee0)==primary&&primary!=-1)difficulty=U32(settings,0x34+0x48*(uint32_t)primary);
 else if(primary!=-1)difficulty=U32(settings,0x34+0x48*(uint32_t)primary);else difficulty=U32(settings,0x154);
 SU32(p,0x38,difficulty);SU32(p,0,1);ACT_END(F);
}
RX_FUNCTION(0x005b0ad0,rx_career_progress_load)
void rx_career_progress_commit(rx_addr p){
 ACT_FRAME_EBP(F,8,0xec,3);guard_manager();const rx_addr t=table(RX_TBL_PLAYER_DB(U32(0,0x9e21a4)),0x94d340),ref=keyed(t,0x94c990,U32(p,0x20));
 static const uint32_t names[]={0x94d36c,0x94d37c,0x94d388,0x94d398,0x92d688,0x94d850};
 static const uint32_t offsets[]={0x24,0x28,0x2c,0x30,0x34,0x4c};
 if(U32(ref,8)&&I32(ref,0)!=-1){
  for(uint32_t i=0;i<6;i++)ACT_CALL(2,rx_tbl_set_cell_int(U32(ref,8),names[i],U32(ref,0),p+offsets[i]));
  const rx_addr str=F+0x18;rx_tbl_str_init(str);const rx_addr owner=U32(0x9d2374,U32(p,8)*4);
  ACT_CALL(1,rx_tbl_str_assign_n(str,owner,crt_strlen(owner)));
  const rx_addr owner_col=column(U32(ref,8),0x94c9f4);
  /*005b11cd ->005be890: ownerstring setter ALWAYS bumps table revision,
   *even when existing SeriesOwner text is unchanged. Integer setters differ.*/
  SU32(U32(ref,8),0x68,U32(U32(ref,8),0x68)+1u);
  ACT_CALL(2,rx_tbl_set_string(owner_col,U32(ref,0),str));rx_tbl_str_tidy(str);
  /* 005b11e3..123c bumps revision on changed status before setting the int cell. */
  const rx_addr status_table=U32(ref,8);const uint32_t row=U32(ref,0),status=U32(p,0xc);SU32(F,0x34,status);
  if(status!=word(status_table,0x94d3a8,row))SU32(status_table,0x68,U32(status_table,0x68)+1);
  ACT_CALL(3,rx_tbl_set_int(column(status_table,0x94d3a8),row,F+0x34));
  ACT_CALL(2,rx_tbl_set_cell_int(U32(ref,8),0x936e48,U32(ref,0),p+0x38));
 }else{
  const rx_addr v=F+0x18,c=F+0x38;ACT_CALL(0,rx_tbl_crit_vec_init(v));push_int(v,c,0x94c990,0,U32(p,0x20));
  for(uint32_t i=0;i<6;i++)push_int(v,c,names[i],0,U32(p,offsets[i]));
  push_string(v,c,0x94c9f4,U32(0x9d2374,U32(p,8)*4));push_int(v,c,0x94d3a8,0,U32(p,0xc));push_int(v,c,0x936e48,0,U32(p,0x38));
  ACT_CALL(2,(void)rx_tbl_add_row(t,F+4,v));ACT_CALL(0,rx_tbl_crit_vec_destroy(v));
 }
 ACT_END(F);
}
RX_FUNCTION(0x005b0ed0,rx_career_progress_commit)
/* 005a0ae0: choose the authored class from the caller's VehicleTypeId criteria.
 * The ordering/temporary sorted set is the canonical Rdb sort closure, not a host replacement.
 * Empty input still invokes SELECT; original004ed35e assigns an empty set, not all rows.
 * Root's canonical Rdb extension owns that currently-unported path. */
uint32_t rx_career_vehicle_class_for_types(rx_addr excluded){
 ACT_FRAME(F,0x130,4);
 const rx_addr t=table(RX_TBL_DEFAULT_DB,0x936cc0),criteria=F+0x48,sort=F+0x30,item=F+0x90,set=F+0x68;
 const int32_t count=(int32_t)(U32(excluded,0x10)-U32(excluded,0xc))>>2;
 SU32(F,0x10,(uint32_t)count);SU32(F,0x2c,t);ACT_CALL(0,rx_tbl_crit_vec_init(criteria));
 for(int32_t i=0;i<count;i++){
  if((uint32_t)i>=((U32(excluded,0x10)-U32(excluded,0xc))>>2))crt_invalid_parameter_noinfo();
  push_int(criteria,item,0x936ce4,0,U32(U32(excluded,0xc),(uint32_t)i*4));
 }
 ACT_CALL(0,(void)rx_list_init(sort));
 SU32(item,0x80,0xffffffffu);SU32(item,0x84,0);ACT_CALL(0,(void)rx_list_init(item+0x88));
 crt_strncpy(item,0x936cd4,0x7f);SU8(item,0x7f,0);
 ACT_CALL(1,rx_tbl_sort_criterion_push(sort,item));
 if(U32(item,0x94))ACT_CALL(1,rx_reflex_aligned_free(U32(item,0x94)));
 SU32(item,0x94,0);SU32(item,0x98,0);SU32(item,0x9c,0);ACT_CALL(1,rx_reflex_aligned_free(U32(item,0x88)));
 ACT_CALL(1,rx_tbl_rowset_init(set,t));
 rx_addr columns;ACT_CALLR(columns,1,rx_reflex_aligned_alloc(0x18));
 if(columns)ACT_CALL(0,(void)rx_list_init(columns));SU32(set,0x20,columns);
 ACT_CALL(2,rx_tbl_resolve_sort_columns(columns,U32(set,0x24),sort));
 ACT_CALL(1,rx_tbl_select(t,set,criteria));
 int32_t best=-1,best_count=0,best_size=10000,current=-10,current_count=0,current_size=10000;
 SU32(F,0x14,(uint32_t)best);SU32(F,0x18,0);SU32(F,0x1c,10000);SU32(F,0x20,(uint32_t)current);SU32(F,0x24,0);SU32(F,0x28,10000);
 rx_addr node=U32(U32(set,0x18),0),proxy=U32(set,0);SU32(F,0x60,proxy);SU32(F,0x64,node);
 for(;;){
  if(!proxy||proxy!=U32(set,0))crt_invalid_parameter_noinfo();
  if(node==U32(set,0x18))break;
  if(node==U32(U32(proxy,0),0x18))crt_invalid_parameter_noinfo();
  const uint32_t row=U32(node,0xc);const rx_addr col=column(t,0x936cd4),cells=U32(col,0x40);
  if(row>=((U32(cells,0x10)-U32(cells,0xc))>>2))crt_invalid_parameter_noinfo();
  const int32_t cls=I32(U32(cells,0xc),row*4);SU32(F,0x10,(uint32_t)cls);
  if(cls!=current){
   if(current!=best&&current_count>=best_count&&current_size<best_size){best=current;best_count=current_count;best_size=current_size;SU32(F,0x14,(uint32_t)best);SU32(F,0x18,(uint32_t)best_count);SU32(F,0x1c,(uint32_t)best_size);}
   current=cls;current_count=0;SU32(F,0x20,(uint32_t)current);SU32(F,0x24,0);
   rx_addr all;ACT_CALLR(all,1,rx_tbl_rows(column(t,0x936cd4),current));current_size=I32(all,0x1c);SU32(F,0x28,(uint32_t)current_size);
  }
  ++current_count;SU32(F,0x24,(uint32_t)current_count);ACT_CALLR(node,0,rx_tbl_rowset_next(node));SU32(F,0x64,node);
 }
 if(current!=best&&current_count>=best_count&&current_size<best_size){best=current;best_count=current_count;SU32(F,0x14,(uint32_t)best);SU32(F,0x18,(uint32_t)best_count);}
 ACT_CALL(0,rx_tbl_rowset_destroy(set));ACT_CALL(0,rx_tbl_sort_criteria_destroy(sort));ACT_CALL(0,rx_tbl_crit_vec_destroy(criteria));
 ACT_END(F);return(uint32_t)best;
}
RX_FUNCTION(0x005a0ae0,rx_career_vehicle_class_for_types)
void rx_career_progress_set_vehicle(rx_addr p,uint32_t excluded_type){
 ACT_FRAME(F,0x34,4);const uint32_t mode=U32(p,0x18);
 if(mode==0)SU32(p,0x64,U32(p,0x68));
 else if(mode==1||mode==2){
  const rx_addr v=F+0x28;ACT_CALL(0,(void)rx_list_init(v));
  if(mode==2){SU32(F,0x4c,excluded_type);ACT_CALL(1,rx_tbl_vector_push_ptr(v,F+0x4c));}
  else{
   const rx_addr t=table(RX_TBL_DEFAULT_DB,0x936cc0);rx_addr set;ACT_CALLR(set,1,rx_tbl_rows(column(t,0x936cd4),I32(p,0x68)));
   ACT_CALL(1,rx_vecp_reserve_ecx(v,U32(set,0x1c)));
   rx_addr node=U32(U32(set,0x18),0),proxy=U32(set,0);
   for(;;){if(!proxy||proxy!=U32(set,0))crt_invalid_parameter_noinfo();if(node==U32(set,0x18))break;if(node==U32(U32(proxy,0),0x18))crt_invalid_parameter_noinfo();
    const uint32_t type=word(t,0x936ce4,U32(node,0xc));SU32(F,0x14,type);if(type!=excluded_type)ACT_CALL(1,rx_tbl_vector_push_ptr(v,F+0x14));
    ACT_CALLR(node,0,rx_tbl_rowset_next(node));
   }
  }
  uint32_t resolved;ACT_CALLR(resolved,1,rx_career_vehicle_class_for_types(v));SU32(p,0x64,resolved);
  if(U32(v,0xc))ACT_CALL(1,rx_reflex_aligned_free(U32(v,0xc)));
  SU32(v,0xc,0);SU32(v,0x10,0);SU32(v,0x14,0);ACT_CALL(1,rx_reflex_aligned_free(U32(v,0)));
 }else SU32(p,0x64,0xffffffffu);
 ACT_END(F);
}
RX_FUNCTION(0x005b1270,rx_career_progress_set_vehicle)
