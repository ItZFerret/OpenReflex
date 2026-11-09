/* Isolated career source candidate. Not registered in the released build until original comparison passes. */
#include "career_manager.h"
#include "build_activity_util.h"

static rx_addr column(rx_addr t,rx_addr name){
 uint32_t i;ACT_CALLR(i,1,rx_tbl_column_index(t,name));
 if(i>=((U32(t,0x14)-U32(t,0x10))>>2))crt_invalid_parameter_noinfo();
 return U32(U32(t,0x10),4*i);
}
static uint32_t word(rx_addr t,rx_addr name,uint32_t row){
 uint32_t i;rx_addr p;ACT_CALLR(i,1,rx_tbl_column_index(t,name));
 ACT_CALLR(p,1,rx_tbl_cell_word(t,i,row));return U32(p,0);
}
static void write_word(rx_addr t,rx_addr name,uint32_t row,rx_addr value){
 uint32_t i;rx_addr p;ACT_CALLR(i,1,rx_tbl_column_index(t,name));
 ACT_CALLR(p,1,rx_tbl_cell_word(t,i,row));
 if(U32(value,0)!=U32(p,0))SU32(t,0x68,U32(t,0x68)+1);
 if(i>=((U32(t,0x14)-U32(t,0x10))>>2))crt_invalid_parameter_noinfo();
 ACT_CALL(3,rx_tbl_set_int(U32(U32(t,0x10),4*i),row,value));
}
static rx_addr name_of(rx_addr table,uint32_t row){
 rx_addr c=column(table,0x936f00),s;ACT_CALLR(s,1,rx_tbl_cell_string(c,row));return rx_tbl_str_cstr(s);
}
static void validate_iterator(rx_addr set,rx_addr proxy,rx_addr node){
 if(!proxy||proxy!=U32(set,0))crt_invalid_parameter_noinfo();
 if(node!=U32(set,0x18)&&node==U32(U32(proxy,0),0x18))crt_invalid_parameter_noinfo();
}

/* 005b6840: ECX key, EDX manager. Key1 is the transient standings table. */
rx_addr rx_career_player_table(rx_addr manager,uint32_t key){
 ACT_FRAME_EBP(F,8,0x10c,3);
 rx_addr result,name=0x94d340;const uint32_t controller=U32(manager,0x1c);
 if(key==1){result=manager+0x560;ACT_END(F);return result;}
 if(key){
  const rx_addr buffer=F+0x10;SU8(buffer,0,0);crt_memset(buffer+1,0,0xff);
  SU32(F-20,0,U32(0x9d23b4,4*key));SU32(F-16,0,U32(0x9d2374,4*U32(manager,0x190)));
  rx_set_sp(F-12);ACT_CALL(3,rx_format_buffer(buffer,0x100,0x94d9d8,F-20));rx_set_sp(F);
  name=buffer;
 }
 ACT_CALLR(result,1,rx_tbl_find(RX_TBL_PLAYER_DB(controller),name));
 ACT_END(F);return result;
}
RX_FUNCTION(0x005b6840,rx_career_player_table)

/* 005b3890: EDX manager; from,to on stack. Source state index remains unchanged. */
void rx_career_request_player_states(rx_addr manager,uint32_t from,uint32_t to){
 ACT_FRAME(F,0x14,4);rx_addr table,set;
 ACT_CALLR(table,0,rx_career_player_table(manager,2));
 SU32(F,0x28,from);ACT_CALLR(set,1,rx_tbl_rows(column(table,0x94d650),(int32_t)from));
 rx_addr node=LEFT_OF(set),proxy=U32(set,0);
 SU32(F,0x14,set);SU32(F,0x18,proxy);SU32(F,0x1c,node);
 for(;;){validate_iterator(set,proxy,node);if(node==U32(set,0x18))break;
  SU32(F,0x28,to);write_word(table,0x94d670,U32(node,0xc),F+0x28);
  ACT_CALLR(node,0,rx_tbl_rowset_next(node));SU32(F,0x1c,node);
 }
 ACT_END(F);
}
RX_FUNCTION(0x005b3890,rx_career_request_player_states)

/* 005b39a0: EDX manager. Zero means no request, not a request to enter state0. */
void rx_career_apply_player_states(rx_addr manager){
 ACT_FRAME(F,0xc,4);rx_addr table;uint32_t count;
 ACT_CALLR(table,0,rx_career_player_table(manager,2));ACT_CALLR(count,0,rx_tbl_row_count(table));SU32(F,0x18,count);
 for(uint32_t row=0;(int32_t)row<(int32_t)count;row++){
  const uint32_t requested=word(table,0x94d670,row);if(!requested)continue;
  SU32(F,0x14,requested);write_word(table,0x94d650,row,F+0x14);
  SU32(F,0x14,0);write_word(table,0x94d670,row,F+0x14);
 }
 ACT_END(F);
}
RX_FUNCTION(0x005b39a0,rx_career_apply_player_states)

void rx_career_clear_requested_states(rx_addr manager){
 ACT_FRAME(F,4,3);rx_addr table;uint32_t count;
 ACT_CALLR(table,0,rx_career_player_table(manager,2));ACT_CALLR(count,0,rx_tbl_row_count(table));
 if((int32_t)count>0){SU32(F,0xc,0);rx_set_sp(F-4);
  for(uint32_t row=0;(int32_t)row<(int32_t)count;row++)write_word(table,0x94d670,row,F+0xc);
 }
 ACT_END(F);
}
RX_FUNCTION(0x005b3ae0,rx_career_clear_requested_states)

/* 005b55d0: destination/source series rowsets joined by case-insensitive PlayerName.
 * All five source values are read before any destination write, including aliasing sets. */
void rx_career_accumulate_standings(rx_addr manager,int32_t destination,int32_t source,int32_t multiplier){
 ACT_FRAME_EBP(F,8,0x3c,3);const rx_addr table=manager+0x560;
 if(table){
  rx_addr dst,src;ACT_CALLR(dst,1,rx_tbl_rows(column(table,0x94c990),destination));
  ACT_CALLR(src,1,rx_tbl_rows(column(table,0x94c990),source));
  SU32(F,0x38,dst);SU32(F,0x34,src);
  rx_addr dn=LEFT_OF(dst),dp=U32(dst,0);
  for(;;){validate_iterator(dst,dp,dn);if(dn==U32(dst,0x18))break;
   SU32(F,0x10,dp);SU32(F,0x14,dn);SU32(F,0x40,U32(dst,0x18));
   const uint32_t dr=U32(dn,0xc);const rx_addr dname=name_of(table,dr);
   rx_addr sn=LEFT_OF(src),sp=U32(src,0);
   for(;;){validate_iterator(src,sp,sn);if(sn==U32(src,0x18))break;
    if(crt_stricmp(dname,name_of(table,U32(sn,0xc)))==0)break;
    ACT_CALLR(sn,0,rx_tbl_rowset_next(sn));
   }
   if(sn!=U32(src,0x18)){
    const uint32_t sr=U32(sn,0xc);
    static const rx_addr names[]={0x94d6b8,0x94c150,0x94c144,0x94c138,0x94d910};
    static const uint32_t slots[]={0x24,0x2c,0x28,0x30,0x20};
    uint32_t values[5];
    for(unsigned j=0;j<5;j++)values[j]=word(table,names[j],dr);
    for(unsigned j=0;j<5;j++){
     const uint32_t v=word(table,names[j],sr);
     values[j]=multiplier?values[j]+v*(uint32_t)multiplier:v;SU32(F,slots[j],values[j]);
    }
    for(unsigned j=0;j<5;j++)write_word(table,names[j],dr,F+slots[j]);
   }
   ACT_CALLR(dn,0,rx_tbl_rowset_next(dn));
  }
 }
 ACT_END(F);
}
RX_FUNCTION(0x005b55d0,rx_career_accumulate_standings)
