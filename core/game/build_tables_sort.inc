/* Included by the one canonical Rdb implementation: uses the same trees, allocator,
 * column readers and parent links. Candidate only; original comparison is pending. */
void rx_tbl_sort_criterion_init(rx_addr c,rx_addr name,uint32_t direction){
 SU32(c,0x80,0xffffffffu);SU32(c,0x84,direction);
 rx_tbl_crit_vec_init(c+0x88);crt_strncpy(c,name,0x7f);SU8(c,0x7f,0);
}
RX_FUNCTION(0x004dc500,rx_tbl_sort_criterion_init)
void rx_tbl_sort_criterion_destroy(rx_addr c){
 if(U32(c,0x94))rx_reflex_aligned_free(U32(c,0x94));
 SU32(c,0x94,0);SU32(c,0x98,0);SU32(c,0x9c,0);rx_reflex_aligned_free(U32(c,0x88));
}
RX_FUNCTION(0x004e0a00,rx_tbl_sort_criterion_destroy)
static void sort_copy(rx_addr d,rx_addr s){
 for(unsigned i=0;i<0x88;i+=4)SU32(d,i,U32(s,i));
 rx_tbl_crit_vec_init(d+0x88);
 const uint32_t n=vsize(s+0x88,4);
 if(n){const rx_addr p=rx_reflex_aligned_alloc(4*n);SU32(d,0x94,p);SU32(d,0x98,p);SU32(d,0x9c,p+4*n);
  crt_memmove(p,U32(s,0x94),4*n);SU32(d,0x98,p+4*n);}
}
static void sort_destroy_range(rx_addr begin,rx_addr end){for(rx_addr c=begin;c!=end;c+=0xa0)rx_tbl_sort_criterion_destroy(c);}
static void sort_reserve(rx_addr v,uint32_t n){
 if(n>0x01999999u)unreached(0x004f0aa0,"sort vector length");
 if(vcap(v,0xa0)>=n)return;
 const rx_addr p=rx_reflex_aligned_alloc(rx_mul_sat(n,0xa0)),old=U32(v,0xc);const uint32_t size=vsize(v,0xa0);
 for(uint32_t i=0;i<size;i++)sort_copy(p+0xa0*i,old+0xa0*i);
 if(old){sort_destroy_range(old,U32(v,0x10));rx_reflex_aligned_free(old);}
 SU32(v,0xc,p);SU32(v,0x10,p+size*0xa0);SU32(v,0x14,p+n*0xa0);
}
void rx_tbl_sort_criterion_push(rx_addr v,rx_addr c){
 const uint32_t size=vsize(v,0xa0),cap=vcap(v,0xa0);
 if(size<cap){sort_copy(U32(v,0x10),c);SU32(v,0x10,U32(v,0x10)+0xa0);return;}
 const uint32_t n=grow_cap(cap,size,0x01999999u);const rx_addr p=rx_reflex_aligned_alloc(rx_mul_sat(n,0xa0)),old=U32(v,0xc);
 for(uint32_t i=0;i<size;i++)sort_copy(p+i*0xa0,old+i*0xa0);
 sort_copy(p+size*0xa0,c);
 if(old){sort_destroy_range(old,U32(v,0x10));rx_reflex_aligned_free(old);}
 SU32(v,0xc,p);SU32(v,0x10,p+(size+1)*0xa0);SU32(v,0x14,p+n*0xa0);
}
RX_FUNCTION(0x004f0be0,rx_tbl_sort_criterion_push)
void rx_tbl_sort_criteria_destroy(rx_addr v){
 if(U32(v,0xc)){sort_destroy_range(U32(v,0xc),U32(v,0x10));rx_reflex_aligned_free(U32(v,0xc));}
 SU32(v,0xc,0);SU32(v,0x10,0);SU32(v,0x14,0);rx_reflex_aligned_free(U32(v,0));
}
static void sort_columns_destroy(rx_addr v){rx_tbl_sort_criteria_destroy(v);rx_reflex_aligned_free(v);}
static rx_addr sort_columns_copy(rx_addr src){
 const rx_addr v=rx_reflex_aligned_alloc(0x18);rx_tbl_crit_vec_init(v);
 const uint32_t n=vsize(src,0xa0);sort_reserve(v,n);
 for(rx_addr s=U32(src,0xc);s!=U32(src,0x10);s+=0xa0){sort_copy(U32(v,0x10),s);SU32(v,0x10,U32(v,0x10)+0xa0);}
 return v;
}
void rx_tbl_resolve_sort_columns(rx_addr columns,rx_addr table,rx_addr criteria){
 TF_PLAIN(F,0x8d4,4);
 sort_destroy_range(U32(columns,0xc),U32(columns,0x10));SU32(columns,0x10,U32(columns,0xc));sort_reserve(columns,vsize(criteria,0xa0));
 const rx_addr c=F+0x38,remaining=F+0xd8,segment=F+0x4d8;
 for(rx_addr source=U32(criteria,0xc);source!=U32(criteria,0x10);source+=0xa0){
  sort_copy(c,source);
  if(I32(c,0x80)<0){
   rx_addr dot=crt_strchr(c,'.');
   if(!dot)SU32(c,0x80,rx_tbl_column_index(table,c));
   else{
    crt_strncpy(remaining,dot+1,0x3ff);SU8(remaining,0x3ff,0);SU8(dot,0,0);
    rx_addr t=table;uint32_t col=rx_tbl_column_index(t,c);SU32(c,0x80,col);
    while((dot=crt_strchr(remaining,'.'))!=0){
     const uint32_t length=dot-remaining;crt_strncpy(segment,remaining,length);SU8(segment,length,0);
     if(col>=((U32(t,0x14)-U32(t,0x10))>>2))crt_invalid_parameter_noinfo();
     t=U32(U32(U32(t,0x10),col*4),0x24);col=rx_tbl_column_index(t,segment);vec_push4(c+0x88,col);
     crt_strncpy(remaining,dot+1,0x3ff);SU8(remaining,0x3ff,0);
    }
    if(col>=((U32(t,0x14)-U32(t,0x10))>>2))crt_invalid_parameter_noinfo();
    const rx_addr parent=U32(U32(U32(t,0x10),col*4),0x24);vec_push4(c+0x88,rx_tbl_column_index(parent,remaining));
   }
  }
  if(I32(c,0x80)>=0)rx_tbl_sort_criterion_push(columns,c);
  rx_tbl_sort_criterion_destroy(c);
 }
 TF_END(F);
}
RX_FUNCTION(0x004ed9b0,rx_tbl_resolve_sort_columns)
static int sort_cell_compare(rx_addr column,uint32_t direction,uint32_t a,uint32_t b){
 if(a>=rx_tbl_column_rows(column)||b>=rx_tbl_column_rows(column))crt_invalid_parameter_noinfo();
 const rx_addr data=U32(U32(column,0x40),0xc);int greater=0,equal=0;
 switch(U32(column,0x2c)){
 case 1:greater=I32(data,4*a)>I32(data,4*b);equal=U32(data,4*a)==U32(data,4*b);break;
 case 2:greater=F32(data,4*a)>F32(data,4*b);equal=F32(data,4*a)==F32(data,4*b);break;
 case 4:{int value=crt_stricmp(rx_tbl_str_cstr(data+0x1c*a),rx_tbl_str_cstr(data+0x1c*b));greater=value>0;equal=value==0;break;}
 default:unreached(0x004ebac0,"sort comparison for unported column type");
 }
 if(equal||direction>1)return 0;
 return direction?(greater?1:-1):(greater?-1:1);
}
int rx_tbl_row_less(rx_addr set,uint32_t left,uint32_t right){
 const rx_addr columns=U32(set,0x20);
 if(columns)for(rx_addr c=U32(columns,0xc);c!=U32(columns,0x10);c+=0xa0){
  rx_addr lt=U32(set,0x24),rt=lt;uint32_t l=left,r=right,col=U32(c,0x80);
  for(rx_addr p=U32(c,0x94);p!=U32(c,0x98);p+=4){
   if(col>=((U32(lt,0x14)-U32(lt,0x10))>>2)||col>=((U32(rt,0x14)-U32(rt,0x10))>>2))crt_invalid_parameter_noinfo();
   rx_addr ref=parent_ref(U32(U32(lt,0x10),4*col),l);l=U32(ref,0);lt=U32(ref,8);
   ref=parent_ref(U32(U32(rt,0x10),4*col),r);r=U32(ref,0);rt=U32(ref,8);
   if(!lt||!rt||l==0xffffffffu||r==0xffffffffu)break;col=U32(p,0);
  }
  if(lt&&rt&&l!=0xffffffffu&&r!=0xffffffffu){
   if(col>=((U32(lt,0x14)-U32(lt,0x10))>>2))crt_invalid_parameter_noinfo();
   /* 004ec474 pushes left; 004ec475 loads right into EAX: preserve this reversal. */
   const int cmp=sort_cell_compare(U32(U32(lt,0x10),4*col),U32(c,0x84),r,l);
   if(cmp)return cmp<0;
  }
 }
 return left<right;
}
/* Original comparator receives RowRefs; typed caller passes their row words. */
static int row_less_key(rx_addr set,uint32_t left,uint32_t right){return set?rx_tbl_row_less(set,left,right):left<right;}
/*004ec0f0: separate frame from both constructors. It allocates columns,
 * publishes set+20 before resolving, then calls004ed9b0 at its actual depth. */
static void rowset_sorted_columns(rx_addr set,rx_addr criteria) {
 TF_PLAIN(C,0,3);
 const rx_addr columns=rx_reflex_aligned_alloc(0x18);
 if(columns)rx_tbl_crit_vec_init(columns);
 SU32(set,0x20,columns);
 TF_CALL(2,rx_tbl_resolve_sort_columns(columns,U32(set,0x24),criteria));
 TF_END(C);
}
void rx_tbl_rowset_sorted_init(rx_addr set,rx_addr table,rx_addr criteria){
 TF_PLAIN(S,0,1); /*004ebc80 PUSH ECX; this is not sorted_all's frame*/
 rowset_construct(set,table);
 TF_CALL(2,rowset_sorted_columns(set,criteria));
 TF_END(S);
}
void rx_tbl_rowset_sorted_all(rx_addr set,rx_addr table,uint32_t count,rx_addr criteria){
 TF_PLAIN(G,0x18,2); /*004ebbf0 SUB18/PUSH EBX/PUSH EDI*/
 rowset_construct(set,table); /*original constructs directly; no sorted_init call*/
 TF_CALL(2,rowset_sorted_columns(set,criteria));
 if(count){
  SU32(G,0xc,set);
  for(uint32_t row=0;row<count;row++){
   SU32(G,8,row);SU32(G,0x10,U32(set,0x24));
   /*004ebc50..65: actual RowRef G+8, result pair G+14, CALL2.*/
   TF_CALL(2,rowset_insert_result(set,G+8,G+0x14));
  }
 }
 TF_END(G);
}
