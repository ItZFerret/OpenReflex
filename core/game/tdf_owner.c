/* Bounded ownership closure. Unregistered candidate: existing close remains
 * main-owned until verified replacement. Never casts guest FILE or host stream. */
#include "tdf_owner.h"
#include "build_alloc.h"
#include "build_vehicle_util.h"
#include "../src/rx_os.h"
#ifdef RX_ORACLE
extern void rx_tdf_owner_reg(unsigned,uint32_t);
extern uint32_t rx_tdf_owner_getreg(unsigned);
#define REG(i,v) rx_tdf_owner_reg(i,v)
#define GET(i) rx_tdf_owner_getreg(i)
#else
#define REG(i,v) ((void)0)
#define GET(i) 0u
#endif
static void system_at(rx_addr b,uint32_t ra,uint32_t slot){
 const uint32_t cs=0xd61704;veh_call_sp(b,ra,1,cs,0,0);rx_set_sp(b);
 rx_os_import(slot,1,&cs);SU32(b-8,0,ra);rx_set_sp(b);
}
static void aligned_at(rx_addr b,uint32_t ra,rx_addr p){
 veh_call_sp(b,ra,1,p,0,0);rx_set_sp(b);rx_crt_aligned_free(p);SU32(b-8,0,ra);rx_set_sp(b);
}
/* Exact private0078fc50 including savedESI and both recursive lock entries.
 * Native allocator service is single-threaded; oracle imports retain outputs. */
static void wrapper_free(rx_addr p){
 rx_addr e=rx_sp(),b=e-4;uint32_t si=GET(6);SU32(b,0,si);
 if(U8(0,0xda67d8)){REG(6,U32(0,0x91600c));system_at(b,0x78fc67,0x91600c);if(U8(0,0xda67d8))system_at(b,0x78fc77,0x91600c);}
 REG(0,p);aligned_at(b,0x78fc82,p);
 if(U8(0,0xda67d8)){REG(6,U32(0,0x9160f8));system_at(b,0x78fc9b,0x9160f8);if(U8(0,0xda67d8))system_at(b,0x78fcab,0x9160f8);}
 REG(6,si);rx_set_sp(e);
}
static void delete_free_at(rx_addr b,uint32_t ra,rx_addr p){veh_call_sp(b,ra,1,p,0,0);wrapper_free(p);rx_set_sp(b);}
void rx_owned_memory_stream_close(rx_addr s){
 rx_addr e=rx_sp(),b=e-8;uint32_t bx=GET(3),si=GET(6),di=GET(7);
 SU32(e-4,0,bx);SU32(e-8,0,si);REG(6,s);REG(3,0);
 if(U32(s,0x14)){
  SU32(b-4,0,di);b-=4;rx_addr p=U32(s,4);REG(7,p);
  if(U8(0,0xda67d8))system_at(b,0x810982,0x91600c);
  aligned_at(b,0x810989,p);b+=4;REG(7,di);
  if(U8(0,0xda67d8))system_at(b,0x8109a0,0x9160f8);
 }
 SU32(s,4,0);SU32(s,8,0);REG(6,si);REG(3,bx);rx_set_sp(e);
}
void rx_owned_memory_stream_destroy(rx_addr s){
 rx_addr e=rx_sp();uint32_t di=GET(7);SU32(s,0,0x974d1c);
 if(U32(s,0x14)){
  rx_addr b=e-4,p=U32(s,4);SU32(b,0,di);REG(7,p);
  if(U8(0,0xda67d8))system_at(b,0x8108d4,0x91600c);
  aligned_at(b,0x8108db,p);
  /* Original CMP precedes popEDI; leave call occurs at e after the pop. */
  int locked=U8(0,0xda67d8)!=0;REG(7,di);if(locked)system_at(e,0x8108f3,0x9160f8);
 }
 SU32(s,0,0x94dab4);rx_set_sp(e);
}
rx_addr rx_owned_memory_stream_delete(rx_addr s,uint32_t flags){
 rx_addr e=rx_sp(),b=e-4;uint32_t si=GET(6);SU32(b,0,si);REG(6,s);
 veh_call_sp(b,0x810898,0,0,0,0);rx_owned_memory_stream_destroy(s);rx_set_sp(b);
 if(flags&1)delete_free_at(b,0x8108a5,s);
 REG(0,s);REG(6,si);rx_set_sp(e);return s;
}
static void child_delete_at(rx_addr b,rx_addr s,rx_addr fn){
 REG(1,s);veh_call_sp(b,0x53a008,1,1,0,0);
 /* Compose the complete known genuine stream destructor in both backends;
  * an unknown virtual target retains normal project dispatch. */
 if(fn==0x810890)rx_owned_memory_stream_delete(s,1);
 else{
#ifdef RX_ORACLE
  rx_set_sp(b);uint32_t arg=1;rx_call_original(fn,RX_THISCALL,s,1,&arg);SU32(b-8,0,0x53a008);
#else
  void* f=rx_fn(fn);if(f)((rx_addr(*)(rx_addr,uint32_t))f)(s,1);else{rx_set_sp(b);uint32_t arg=1;rx_call_original(fn,RX_THISCALL,s,1,&arg);}
#endif
 }
 rx_set_sp(b);
}
rx_addr rx_tdf_owner_delete(rx_addr s,uint32_t flags){
 rx_addr e=rx_sp(),b=e-4;uint32_t si=GET(6);SU32(b,0,si);REG(6,s);rx_addr stream=U32(s,0x10);REG(1,stream);SU32(s,0,0x940298);
 if(stream){rx_addr vt=U32(stream,0),fn=U32(vt,0);REG(0,vt);REG(2,fn);child_delete_at(b,stream,fn);}
 SU32(s,0x10,0);SU32(s,0,0x93eb78);if(flags&1)delete_free_at(b,0x53a022,s);
 REG(0,s);REG(6,si);rx_set_sp(e);return s;
}

/* The original ESI-only body is registered for typed lookup/oracle coverage;
 * product callers must pass self explicitly, never generic CDECL/noargs. */
RX_FUNCTION(0x00539ff0,rx_tdf_owner_delete)
RX_FUNCTION(0x00810890,rx_owned_memory_stream_delete)
RX_FUNCTION(0x008108b0,rx_owned_memory_stream_destroy)
