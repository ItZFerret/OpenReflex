/* Host persistence boundary over existing Rdb owners. No disk/platform/UI calls. */
#include "player_snapshot.h"
#include "rx_player_snapshot_types.h"
#include "build_tables.h"
#include "build_alloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
_Static_assert(sizeof(rx_player_snapshot_header)==32,"snapshot header ABI");
_Static_assert(sizeof(rx_player_snapshot_table)==64,"snapshot table ABI");
_Static_assert(sizeof(rx_player_snapshot_column)==36,"snapshot column ABI");
_Static_assert(sizeof(rx_player_snapshot_cell)==12,"snapshot cell ABI");
#define MAX_TABLES 100u
#define MAX_COLS 1024u
#define MAX_ROWS 65536u
#define MAX_CELLS 1000000u
#define MAX_TEXT (1024u*1024u)
typedef struct {rx_addr table;uint32_t rows,nc;} native_table;
typedef struct {rx_player_snapshot_header h;unsigned char* blob;rx_player_snapshot_table* tables;rx_player_snapshot_column* columns;rx_player_snapshot_cell* cells;unsigned char* text;} snapshot;
static int fail(char* e,size_t n,const char* s){if(e&&n)snprintf(e,n,"%s",s);return -1;}
static int name_read(rx_addr p,char* out,size_t n){for(size_t i=0;i<n;i++){out[i]=(char)U8(p,(uint32_t)i);if(!out[i])return 1;}return 0;}
static int prefix(rx_addr p){static const char s[]="Player";for(uint32_t i=0;i<6;i++)if(U8(p,i)!=(uint8_t)s[i])return 0;return 1;}
static uint32_t col_count(rx_addr t){return(U32(t,0x14)-U32(t,0x10))/4u;}
static rx_addr column(rx_addr t,uint32_t i){return U32(U32(t,0x10),4u*i);}
static int native_schema(native_table* tables,uint32_t* nt,uint32_t* nc,char* e,size_t n){
 if(U32(0,0x785bc218u)!=0)return fail(e,n,"Player snapshot requires canonical C locale");
 *nt=*nc=0;rx_addr db=RX_TBL_PLAYER_DB(0);uint32_t count=U32(db,0x1e8);if(count>MAX_TABLES)return fail(e,n,"Player database table bound exceeded");
 for(uint32_t i=0;i<count;i++){
  rx_addr t=U32(db,0x38+4*i);if(!t||!prefix(t+0x3c))continue;
  uint32_t cols=col_count(t),rows=rx_tbl_row_count(t);if(!cols||cols>MAX_COLS||rows>MAX_ROWS)return fail(e,n,"Player table dimensions unsupported");
  char name[48];if(!name_read(t+0x3c,name,sizeof name))return fail(e,n,"Unterminated native table name");
  for(uint32_t j=0;j<cols;j++){
   rx_addr c=column(t,j);uint32_t type=U32(c,0x2c);char cn[32];
   if(!name_read(c,cn,sizeof cn)||(type!=1&&type!=2&&type!=4))return fail(e,n,"Unsupported Player column schema");
   if(rx_tbl_column_rows(c)!=rows)return fail(e,n,"Player column row counts disagree");
  }
  tables[*nt]=(native_table){t,rows,cols};++*nt;*nc+=cols;
 }
 if(!*nt)return fail(e,n,"No Player tables available");return 0;
}
static int layout(snapshot* s,void* blob,uint32_t bytes){
 uint64_t fixed=sizeof(s->h)+(uint64_t)s->h.tables*sizeof(*s->tables)+(uint64_t)s->h.columns*sizeof(*s->columns)+(uint64_t)s->h.cells*sizeof(*s->cells);
 if(fixed+s->h.text_bytes!=bytes||bytes>RX_PLAYER_SNAPSHOT_MAX_BYTES)return 0;
 s->blob=blob;s->tables=(void*)(s->blob+sizeof(s->h));s->columns=(void*)(s->tables+s->h.tables);s->cells=(void*)(s->columns+s->h.columns);s->text=(void*)(s->cells+s->h.cells);return 1;
}
static rx_addr text_address(rx_addr str){return U32(str,0x18)<16?str+4:U32(str,4);}
int rx_player_snapshot_export(void* output,uint32_t capacity,uint32_t* required,char* e,size_t n){
 native_table nt[MAX_TABLES];uint32_t tc,cc;if(!required||(!output&&capacity))return fail(e,n,"Invalid snapshot export output");
 if(native_schema(nt,&tc,&cc,e,n))return -1;
 uint64_t cells=0,text=0;
 for(uint32_t t=0;t<tc;t++){
  cells+=(uint64_t)nt[t].rows*nt[t].nc;if(cells>MAX_CELLS)return fail(e,n,"Player cell bound exceeded");
  for(uint32_t c=0;c<nt[t].nc;c++)if(U32(column(nt[t].table,c),0x2c)==4){
   rx_addr vec=U32(column(nt[t].table,c),0x40),begin=U32(vec,0xc);
   for(uint32_t r=0;r<nt[t].rows;r++){uint32_t len=U32(begin+28u*r,0x14);if(len>MAX_TEXT)return fail(e,n,"Player text bound exceeded");text+=len;}
  }
 }
 uint64_t total=sizeof(rx_player_snapshot_header)+(uint64_t)tc*sizeof(rx_player_snapshot_table)+(uint64_t)cc*sizeof(rx_player_snapshot_column)+cells*sizeof(rx_player_snapshot_cell)+text;
 if(total>RX_PLAYER_SNAPSHOT_MAX_BYTES)return fail(e,n,"Player snapshot byte bound exceeded");
 *required=(uint32_t)total;if(!output&&capacity==0)return 0;if(capacity<total)return fail(e,n,"Player snapshot buffer too small");
 /* Construct in host memory first: no partial public output on validation/allocation failure. */
 unsigned char* b=output;memset(b,0,(size_t)total);
 snapshot s={0};s.h=(rx_player_snapshot_header){RX_PLAYER_SNAPSHOT_MAGIC,1,(uint32_t)total,tc,cc,(uint32_t)cells,(uint32_t)text,0};if(!layout(&s,b,(uint32_t)total)){return fail(e,n,"Player export layout error");}memcpy(b,&s.h,sizeof s.h);
 uint32_t ci=0,ri=0,ti=0;
 for(uint32_t t=0;t<tc;t++){
  rx_player_snapshot_table* st=s.tables+t;name_read(nt[t].table+0x3c,st->name,sizeof st->name);st->rows=nt[t].rows;st->column_first=ci;st->column_count=nt[t].nc;st->cell_first=ri;
  for(uint32_t c=0;c<nt[t].nc;c++){rx_addr col=column(nt[t].table,c);name_read(col,s.columns[ci+c].name,32);s.columns[ci+c].type=U32(col,0x2c);}
  for(uint32_t r=0;r<nt[t].rows;r++)for(uint32_t c=0;c<nt[t].nc;c++){
   rx_addr col=column(nt[t].table,c),begin=U32(U32(col,0x40),0xc);rx_player_snapshot_cell* v=s.cells+ri++;
   if(U32(col,0x2c)==4){rx_addr str=begin+28u*r;v->text_offset=ti;v->text_length=U32(str,0x14);rx_addr p=text_address(str);for(uint32_t j=0;j<v->text_length;j++)s.text[ti++]=U8(p,j);}
   else v->bits=U32(begin,4*r);
  }
  ci+=nt[t].nc;
 }
 if(e&&n)*e=0;return 0;
}
typedef struct {const snapshot* s;const rx_player_snapshot_cell* value;uint32_t type;} unique_value;
static int compare_unique(const void* av,const void* bv){
 const unique_value* a=av;const unique_value* b=bv;
 if(a->type!=4)return a->value->bits<b->value->bits?-1:a->value->bits>b->value->bits;
 /* Canonical string indexes use crt_stricmp: ASCII C-locale case folding,
  * stopping at the first NUL, not std::string length or host locale. The
  * schema preflight verifies __locale_changed==0 before this comparison. */
 for(uint32_t i=0;;i++){
  uint32_t x=i<a->value->text_length?a->s->text[a->value->text_offset+i]:0;
  uint32_t y=i<b->value->text_length?b->s->text[b->value->text_offset+i]:0;
  if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
  if(!x||x!=y)return(int)x-(int)y;
 }
}
static int validate(snapshot* s,native_table* native,uint32_t tc,uint32_t cc,char* e,size_t n){
 if(s->h.tables!=tc||s->h.columns!=cc)return fail(e,n,"Player snapshot schema dimensions differ");
 uint32_t ci=0,cell=0,text=0;
 for(uint32_t t=0;t<tc;t++){
  rx_player_snapshot_table* st=s->tables+t;char name[48];name_read(native[t].table+0x3c,name,sizeof name);
  if(!memchr(st->name,0,sizeof st->name)||strcmp(st->name,name)||st->rows>MAX_ROWS||st->column_first!=ci||st->column_count!=native[t].nc||st->cell_first!=cell)return fail(e,n,"Player table schema mismatch");
  uint64_t end=(uint64_t)cell+(uint64_t)st->rows*st->column_count;if(end>s->h.cells)return fail(e,n,"Player cell range invalid");
  for(uint32_t c=0;c<st->column_count;c++){
   rx_addr col=column(native[t].table,c);rx_player_snapshot_column* sc=s->columns+ci+c;char cn[32];name_read(col,cn,sizeof cn);
   if(!memchr(sc->name,0,32)||strcmp(sc->name,cn)||sc->type!=U32(col,0x2c))return fail(e,n,"Player column schema mismatch");
   if(sc->type==2&&(U32(col,0x44)||U32(col,0x48)))return fail(e,n,"Indexed Player float unsupported");
  }
  for(uint32_t r=0;r<st->rows;r++)for(uint32_t c=0;c<st->column_count;c++){
   rx_player_snapshot_cell* v=s->cells+cell+r*st->column_count+c;uint32_t type=s->columns[ci+c].type;
   if(type==4){if(v->bits||v->text_offset!=text||v->text_length>MAX_TEXT||(uint64_t)text+v->text_length>s->h.text_bytes)return fail(e,n,"Player text range invalid");text+=v->text_length;}
   else if(v->text_offset||v->text_length)return fail(e,n,"Numeric Player cell contains text");
  }
  /* Host-only sorting checks uniqueness before any guest mutation. */
  for(uint32_t c=0;c<st->column_count;c++)if(U32(column(native[t].table,c),0x48)&&st->rows){
   unique_value* keys=malloc((size_t)st->rows*sizeof(*keys));if(!keys)return fail(e,n,"Player uniqueness validation allocation failed");
   for(uint32_t r=0;r<st->rows;r++)keys[r]=(unique_value){s,s->cells+cell+r*st->column_count+c,s->columns[ci+c].type};
   qsort(keys,st->rows,sizeof(*keys),compare_unique);int duplicate=0;
   for(uint32_t r=1;r<st->rows;r++)if(!compare_unique(keys+r-1,keys+r)){duplicate=1;break;}
   free(keys);if(duplicate)return fail(e,n,"Duplicate Player unique key");
  }
  ci+=st->column_count;cell=(uint32_t)end;
 }
 if(cell!=s->h.cells||text!=s->h.text_bytes)return fail(e,n,"Unreferenced Player snapshot payload");return 0;
}
int rx_player_snapshot_import(const void* input,uint32_t bytes,char* e,size_t n){
 if(!input||bytes<sizeof(rx_player_snapshot_header)||bytes>RX_PLAYER_SNAPSHOT_MAX_BYTES)return fail(e,n,"Invalid Player snapshot size");
 snapshot s={0};memcpy(&s.h,input,sizeof s.h);
 if(s.h.magic!=RX_PLAYER_SNAPSHOT_MAGIC||s.h.version!=1||s.h.total_bytes!=bytes||s.h.reserved||s.h.tables>MAX_TABLES||s.h.columns>MAX_TABLES*MAX_COLS||s.h.cells>MAX_CELLS)return fail(e,n,"Invalid Player snapshot header");
 /* Own an aligned immutable host copy; no caller aliasing during mutation. */
 unsigned char* copy=(unsigned char*)input; /* aligned immutable public-worker-owned copy */
 if(!layout(&s,copy,bytes)){return fail(e,n,"Invalid Player snapshot layout");}
 native_table nt[MAX_TABLES];uint32_t tc,cc;
 if(native_schema(nt,&tc,&cc,e,n)||validate(&s,nt,tc,cc,e,n)){return -1;}
 /* All rejection checks precede the first guest write. Once application starts,
  * native allocation/fault is fatal to the session, not a rollback promise. */
 uint32_t longest=0;for(uint32_t i=0;i<s.h.cells;i++)if(s.cells[i].text_length>longest)longest=s.cells[i].text_length;
 RX_FRAME_ENTER(F,0x20);
 uint32_t maxcols=0;for(uint32_t t=0;t<tc;t++)if(nt[t].nc>maxcols)maxcols=nt[t].nc;
 /* Per-column values and actual strings are guest temporaries consumed only by
  * original append kernels. They never become published table storage. */
 rx_tbl_alloc_init();
 rx_addr values=rx_crt_aligned_malloc(4u*maxcols,16),storage=rx_crt_aligned_malloc(32u*maxcols,16);
 rx_addr text_buffer=longest?rx_crt_aligned_malloc(longest,16):0;
 if(!values||!storage||(longest&&!text_buffer))abort();
 for(uint32_t t=0;t<tc;t++){
  rx_player_snapshot_table* st=s.tables+t;rx_addr table=nt[t].table;rx_tbl_table_clear_rows(table);
  for(uint32_t r=0;r<st->rows;r++){
   for(uint32_t c=0;c<st->column_count;c++){
    rx_addr value=storage+32u*c;SU32(values,4*c,value);
    rx_player_snapshot_cell* v=s.cells+st->cell_first+r*st->column_count+c;
    if(s.columns[st->column_first+c].type==4){
     rx_tbl_str_init(value);
     for(uint32_t j=0;j<v->text_length;j++)SU8(text_buffer,j,s.text[v->text_offset+j]);
     rx_tbl_str_assign_n(value,v->text_length?text_buffer:F,v->text_length);
    }else SU32(value,0,v->bits);
   }
   rx_tbl_append_values_row(table,values);
   for(uint32_t c=0;c<st->column_count;c++)if(s.columns[st->column_first+c].type==4)rx_tbl_str_tidy(storage+32u*c);
  }
  if(!rx_tbl_snapshot_unique_valid(table))abort();
 }
 if(text_buffer)rx_crt_aligned_free(text_buffer);rx_crt_aligned_free(storage);rx_crt_aligned_free(values);
 RX_FRAME_LEAVE(F,0x20);if(e&&n)*e=0;return 0;
}
