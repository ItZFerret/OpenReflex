#ifndef RX_ORACLE
/* Hand-written initial guest stream loader pieces. No RX_FUNCTION registration: the complete, verified 008252c0 is
 * build_xml_stream.c (rx_xml_reader_load_stream, used here for readers with their own allocator); this product-only
 * default-allocator body remains for its bridged compressed path (ZLibCompression lifecycle not closed). */
#include "guest_reader_reset.h"
#include "guest_reader_load.h"
#include "build_xml_stream.h"
#include "guest_string_hash.h"
#include "native_bridge.h"
#include "fx_util.h"
#include "build_vehicle_util.h"
#include "build_alloc.h"
#include "../src/rx_os.h"
#ifdef RX_ORACLE
extern void rx_guest_load_reg(unsigned,uint32_t);
extern uint32_t rx_guest_load_getreg(unsigned);
#define REG(i,v) rx_guest_load_reg(i,v)
#define GET(i) rx_guest_load_getreg(i)
#else
#define REG(i,v) fx_set_reg(i,v)
#define GET(i) fx_reg(i)
#endif
static uint32_t virtual_at(rx_addr b,rx_addr ra,rx_addr self,rx_addr fn,
                           unsigned n,uint32_t a,uint32_t d){
    uint32_t args[2]={a,d},r;REG(1,self);
#ifdef RX_ORACLE
    veh_call_sp(b,ra,(int)n,a,d,0);rx_set_sp(b);
    r=rx_call_original(fn,RX_THISCALL,self,(int)n,args);
    SU32(b-4-4*n,0,ra);
#else
    veh_call_sp(b,ra,(int)n,a,d,0);
    rx_native_compressed_dependency(fn,b);r=fx_reg(FX_EAX);
#endif
    rx_set_sp(b);return r;
}
static void system_at(rx_addr b,rx_addr ra,uint32_t slot){
    const uint32_t cs=0xd61704;veh_call_sp(b,ra,1,cs,0,0);
    rx_set_sp(b);rx_os_import(slot,1,&cs);SU32(b-8,0,ra);rx_set_sp(b);
}
static void aligned_free_at(rx_addr b,rx_addr p){
    veh_call_sp(b,0x80fbc2,1,p,0,0);rx_set_sp(b);rx_crt_aligned_free(p);
    SU32(b-8,0,0x80fbc2);rx_set_sp(b);
}
void rx_guest_compressed_construct(rx_addr s){
    REG(1,0);SU32(s,0,0x974b48);
    const uint32_t fields[]={4,8,0x14,0x18,0x1c,0x20,0x24,0x28,0x2c};
    for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);i++)SU32(s,fields[i],0);
    /* +c chunk slot and +10 owns flag are NOT initialized by this ctor. */
}
void rx_guest_compressed_close(rx_addr s){
    const rx_addr entry=rx_sp();rx_addr b=entry-8;
    const uint32_t ebx=GET(3),esi=GET(6),ebp=GET(5),edi=GET(7);
    SU32(entry-4,0,ebx);SU32(entry-8,0,esi);REG(6,s);REG(3,0);rx_set_sp(b);
    if(U32(s,4)){
        const rx_addr z=U32(s,8),vt=U32(z,0),fn=U32(vt,0x10);
        REG(1,z);REG(0,vt);REG(2,fn);
        /* Actual ZLibCompression::vf4, NOT a host inflateEnd/free surrogate.
         * Product missing target remains reported by normal dynamic dispatch. */
        virtual_at(b,0x80fb95,z,fn,0,0,0);
    }
    if(U32(s,0x10)&&I32(s,0x14)>0){
        SU32(b-4,0,ebp);SU32(b-8,0,edi);b-=8;rx_set_sp(b);
        rx_addr slot=s+0xc;REG(5,slot);
        for(int32_t i=0;i<I32(s,0x14);i++){
            rx_addr chunk=U32(slot,0);REG(7,chunk);
            if(U8(0,0xda67d8))system_at(b,0x80fbbb,0x91600c);
            aligned_free_at(b,chunk);
            if(U8(0,0xda67d8))system_at(b,0x80fbd9,0x9160f8);
            REG(3,(uint32_t)i+1);slot+=4;REG(5,slot);
        }
        b+=8;REG(7,edi);REG(3,0);REG(5,ebp);rx_set_sp(b);
    }
    const uint32_t fields[]={4,8,0x14,0x18,0x1c,0x24,0x28,0x20};
    for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);i++)SU32(s,fields[i],0);
    /* Original retains chunk slot+c, owns+10 and last-read+2c after close. */
    REG(6,esi);REG(3,ebx);rx_set_sp(entry);
}
uint32_t rx_guest_reader_load_header_at(rx_addr r,rx_addr stream,rx_addr b){
    REG(3,r);REG(6,r);REG(7,stream);
    veh_call_sp(b,0x8252d9,0,0,0,0);   /* call 00827e00 (ESI = the reader; no stack arguments) */
    rx_guest_reader_reset(r);
    rx_set_sp(b);const rx_addr vt=U32(stream,0),fn=U32(vt,8);REG(0,vt);REG(2,fn);
    return virtual_at(b,0x8252e9,stream,fn,2,b+0x40,0x20);
}

#include "fourwheel_xml_packet.h"
/* Exact already-written compressed-init frame, explicitly typed by bridge. */
static uint32_t dependency_at(rx_addr b,rx_addr ra,rx_addr fn,unsigned n,uint32_t a,uint32_t d){
    veh_call_sp(b,ra,(int)n,a,d,0);
    rx_native_compressed_dependency(fn,b);uint32_t r=fx_reg(FX_EAX);
    rx_set_sp(b);return r;
}
/* Native semantic composition: explicit typed hash arguments carry LTCG
 * inputs; fx observers retain dependency-call register context. No claim of
 * original incidental CPU equivalence in this native regression. */
static rx_addr native_hash_construct_at(rx_addr b,rx_addr ra,rx_addr t,rx_addr al,uint32_t buckets,uint32_t owns){
 veh_call_sp(b,ra,2,0x3f400000,owns,0);
 rx_addr out=rx_guest_string_hash_construct(t,al,buckets,0x3f400000,owns);rx_set_sp(b);return out;
}
static uint32_t native_hash_insert_at(rx_addr b,rx_addr ra,rx_addr t,rx_addr key,rx_addr value){
 veh_call_sp(b,ra,2,key,value,0);uint32_t out=rx_guest_string_hash_insert(t,key,value);rx_set_sp(b);return out;
}
static rx_addr allocate_at(rx_addr b,rx_addr ra,rx_addr fn,uint32_t size){
    uint32_t r;
#ifdef RX_ORACLE
    veh_call_sp(b,ra,1,size,0,0);rx_set_sp(b);
    r=rx_call_original(fn,RX_CDECL,0,1,&size);SU32(b-8,0,ra);
#else
    veh_call_sp(b,ra,1,size,0,0);
    r=fn==0x401f90?rx_tuning_buffer_alloc(size):rx_reflex_aligned_alloc(size);
#endif
    rx_set_sp(b);return r;
}
/* Exact private007105c0 cdecl free, no save frame or double-lock wrapper. */
static void free_at(rx_addr b,rx_addr ra,rx_addr p){
    const rx_addr entry=b-8;veh_call_sp(b,ra,1,p,0,0);
    if(U8(0,0xda67d8))system_at(entry,0x7105d4,0x91600c);
    REG(0,p);veh_call_sp(entry,0x7105df,1,p,0,0);rx_set_sp(entry);
    rx_crt_aligned_free(p);SU32(entry-8,0,0x7105df);
    if(U8(0,0xda67d8))system_at(entry,0x7105f6,0x9160f8);
    rx_set_sp(b);
}
static uint32_t read_at(rx_addr b,rx_addr ra,rx_addr s,rx_addr dst,uint32_t size,int target_in_eax){
    rx_addr vt=U32(s,0),fn=U32(vt,8);
    if(target_in_eax){REG(0,fn);REG(2,dst);}else{REG(0,vt);REG(2,fn);}
    return virtual_at(b,ra,s,fn,2,dst,size);
}
static void close_wrapper_at(rx_addr b,rx_addr ra,rx_addr r,int target_in_eax){
    rx_addr s=r+0x2c8,vt=U32(s,0),fn=U32(vt,4);
    if(target_in_eax){REG(2,vt);REG(0,fn);}else{REG(0,vt);REG(2,fn);}
    if(fn==0x80fb80){veh_call_sp(b,ra,0,0,0,0);rx_guest_compressed_close(s);rx_set_sp(b);}
    else virtual_at(b,ra,s,fn,0,0,0);
}
static rx_addr custom_alloc_at(rx_addr b,rx_addr ra,rx_addr r,uint32_t size,int target_in_eax){
    rx_addr al=U32(r,0x45c),vt=U32(al,0),fn=U32(vt,4);
    if(target_in_eax){REG(0,fn);REG(2,vt);}else{REG(0,vt);REG(2,fn);}
    /* vf1 has one word, unlike stream read's two. */
#ifdef RX_ORACLE
    veh_call_sp(b,ra,1,size,0,0);rx_set_sp(b);REG(1,al);
    uint32_t p=rx_call_original(fn,RX_THISCALL,al,1,&size);SU32(b-8,0,ra);rx_set_sp(b);return p;
#else
    rx_addr(*f)(rx_addr,uint32_t)=RX_FN(fn,rx_addr(*)(rx_addr,uint32_t));
    if(f){veh_call_sp(b,ra,1,size,0,0);rx_addr p=f(al,size);rx_set_sp(b);return p;}
    rx_set_sp(b);return rx_call_original(fn,RX_THISCALL,al,1,&size);
#endif
}
static void custom_free_at(rx_addr b,rx_addr ra,rx_addr r,rx_addr p,int eax_target){
    rx_addr al=U32(r,0x45c),vt=U32(al,0),fn=U32(vt,8);
    REG(1,al);if(eax_target){REG(0,fn);REG(2,p);}else{REG(2,fn);REG(0,p);}
#ifdef RX_ORACLE
    veh_call_sp(b,ra,1,p,0,0);rx_set_sp(b);rx_call_original(fn,RX_THISCALL,al,1,&p);SU32(b-8,0,ra);
#else
    void(*f)(rx_addr,uint32_t)=RX_FN(fn,void(*)(rx_addr,uint32_t));
    if(f){veh_call_sp(b,ra,1,p,0,0);f(al,p);}else{rx_set_sp(b);rx_call_original(fn,RX_THISCALL,al,1,&p);}
#endif
    rx_set_sp(b);
}
static void inline_node_free(rx_addr b,rx_addr enter,rx_addr freed,rx_addr leave,rx_addr p,int first){
    REG(6,p);if(U8(0,0xda67d8))system_at(b,enter,0x91600c);
    if(first)REG(7,U32(0,0x916350));
    veh_call_sp(b,freed,1,p,0,0);rx_set_sp(b);rx_crt_aligned_free(p);SU32(b-8,0,freed);
    if(U8(0,0xda67d8))system_at(b,leave,0x9160f8);rx_set_sp(b);
}
/* Failure cleanup for the default-allocator modern branch. Positions/order are
 * stage-specific below; no blanket reset is substituted for original failures. */
uint32_t rx_native_reader_load_pc(rx_addr r,rx_addr source){
    /* A reader with its own allocator (+0x45c): the complete loader 008252c0 (build_xml_stream.c), entered at this same ESP. */
    if(U32(r,0x45c)!=0)return rx_xml_reader_load_stream(r,source);
    const rx_addr entry=rx_sp(),aligned=(entry-4)&~7u,b=aligned-0x80;
    const uint32_t oldbp=GET(5),oldbx=GET(3),oldsi=GET(6),olddi=GET(7);
    SU32(entry-4,0,oldbp);REG(5,entry-4);SU32(b+8,0,oldbx);SU32(b+4,0,oldsi);SU32(b,0,olddi);
    uint32_t result=0;unsigned failure_stage=0;rx_set_sp(b);
    if(!rx_guest_reader_load_header_at(r,source,b))goto out;
    /* Real PC header controls compressed initialization; no retagging or eager
     * host decoding. Native fixture scope is authored default-allocator BXML. */
    if(U16(b,0x46)&1){
        REG(6,r+0x2c8);REG(0,source);REG(1,r+0x2f8);SU32(b,0x18,r+0x2c8);
        if(!dependency_at(b,0x825310,0x80f920,1,0x40000,0))goto out;
        source=r+0x2c8;
    }else SU32(b,0x18,source);REG(1,U32(r,0x45c));REG(7,0);
    if(U32(r,0x45c)){
        REG(2,U32(b,0x4c));rx_addr p=custom_alloc_at(b,0x82533b,r,U32(b,0x4c),1);SU32(r,8,p);
        REG(0,U32(b,0x48)*4);p=custom_alloc_at(b,0x825354,r,U32(b,0x48)*4,0);SU32(r,0xc,p);
    }else{
        REG(0,U32(b,0x4c));rx_addr p=allocate_at(b,0x825360,0x401f90,U32(b,0x4c));
        REG(1,U32(b,0x48));REG(2,U32(b,0x48)*4);SU32(r,8,p);
        p=allocate_at(b-4,0x825374,0x401f90,U32(b,0x48)*4);SU32(r,0xc,p);
    }
    REG(6,source);REG(2,U32(r,8));
    if(!read_at(b,0x825390,source,U32(r,8),U32(b,0x4c),1))goto strings_failed;
    SU32(U32(r,0xc),0,U32(r,8));SU32(r,4,1);REG(2,1);REG(1,0);
    for(uint32_t i=0;I32(r,4)<I32(b,0x48);i++){
        rx_addr s=U32(r,8)+i;
        if(!U8(s,0)){SU32(U32(r,0xc),4*U32(r,4),s+1);SU32(r,4,U32(r,4)+1);}
        REG(0,U32(b,0x48));REG(1,i+1);REG(2,1);
    }
    if(U32(b,0x50)){
        SU32(r,0x18,U32(b,0x50));REG(0,U32(b,0x50));REG(1,U32(r,0x45c));
        rx_addr p=U32(r,0x45c)?custom_alloc_at(b,0x82545c,r,U32(b,0x50),1):allocate_at(b,0x825463,0x401f90,U32(b,0x50));SU32(r,0x14,p);
        REG(0,U32(b,0x50));REG(2,U32(U32(source,0),8));
        if(!virtual_at(b,0x82547b,source,U32(U32(source,0),8),2,p,U32(b,0x50)))goto blob_failed;
    }
    SU32(r,0x30,U32(b,0x54));SU32(r,0x28,U32(b,0x58));REG(1,U32(r,0x45c));
    {uint32_t bytes=12*(U32(b,0x54)+3*U32(b,0x58));REG(0,bytes);
     rx_addr p=U32(r,0x45c)?custom_alloc_at(b,0x825529,r,bytes,1):allocate_at(b,0x825530,0x401f90,bytes);
     SU32(r,0x24,p);SU32(r,0x34,p);SU32(r,0x2c,p+12*U32(r,0x30));}
    SU32(b,0x14,0);REG(6,0);
    for(int32_t i=0;i<I32(b,0x54);i++){
        REG(0,b+0x28);REG(2,U32(U32(source,0),8));
        if(!virtual_at(b,0x825582,source,U32(U32(source,0),8),2,b+0x28,12))goto attributes_failed;
        SU32(b,0x1c,0);rx_addr a=U32(r,0x34)+12*(uint32_t)i;uint16_t count=U16(b,0x30),kind=U16(b,0x32);
        SU32(a,0,U32(b,0x28));SU16(a,8,count);SU16(a,0xa,kind);
        SU32(a,4,kind==1?U32(b,0x2c):kind==0?0:U32(r,0x14)+U32(b,0x2c));
        if(!kind)SU16(a,8,0);
        REG(0,(uint32_t)i+1);REG(6,12*((uint32_t)i+1));SU32(b,0x14,(uint32_t)i+1);
    }
    /* Native-endian typed arrays require no swaps, but the original still
     * walks counts and leaves these index spills before the first node read. */
    SU32(b,0x14,0);
    for(int32_t i=0;i<I32(b,0x54);i++){
        rx_addr a=U32(r,0x34)+12*(uint32_t)i;uint16_t kind=U16(a,0xa),count=U16(a,8);SU32(b,0x20,12*(uint32_t)i);
        if(kind>=3&&kind<=12){SU32(b,0x10,count);REG(6,count);}
        SU32(b,0x14,(uint32_t)i+1);REG(7,0);
    }
    SU32(b,0x14,0);REG(6,0);
    for(int32_t i=0;i<I32(b,0x58);i++){
        REG(0,U32(U32(source,0),8));REG(2,b+0x60);
        if(!virtual_at(b,0x826422,source,U32(U32(source,0),8),2,b+0x60,32))goto nodes_failed;
        rx_addr n=U32(r,0x2c)+36*(uint32_t)i;uint32_t count=U32(b,0x68);uint16_t kind=U16(b,0x6c);
        SU32(n,0,U32(U32(r,0xc),4*U32(b,0x60)));SU16(n,6,U16(b,0x6e));
        if(U8(b,0x6e)&1){SU16(n,4,kind);SU32(n,0xc,count);SU32(n,8,U32(b,0x64));}
        if(!count){SU32(n,0xc,0);SU32(n,8,0);SU16(n,4,kind==1?1:0);if(kind==1)SU32(n,8,U32(U32(r,0xc),4*U32(b,0x64)));}
        else{SU16(n,4,kind);SU32(n,0xc,count);SU32(n,8,U32(r,0x14)+U32(b,0x64));}
        SU32(n,0x18,U32(b,0x74));SU32(n,0x14,U32(r,0x2c)+36*U32(b,0x70));
        for(int32_t child=0;child<I32(n,0x18);child++)SU32(U32(n,0x14),0x10+36*(uint32_t)child,n);
        SU32(n,0x20,U32(b,0x7c));SU32(n,0x1c,U32(r,0x34)+12*U32(b,0x78));
        REG(7,I32(n,0x18)>0?U32(n,0x14):U32(b,0x60));
        SU32(b,0x14,(uint32_t)i+1);REG(6,36*((uint32_t)i+1));REG(0,(uint32_t)i+1);REG(1,U32(b,0x58));
    }
    SU32(r,0x38,U32(r,0x2c));SU32(r,0x3c,U32(r,0x2c));SU32(b,0x14,0);REG(7,0);
    for(int32_t i=0;i<I32(b,0x58);i++){
        rx_addr n=U32(r,0x2c)+36*(uint32_t)i;int32_t kind=(int16_t)U16(n,4);SU32(b,0x1c,36*(uint32_t)i);
        if(kind>=3&&kind<=12){uint32_t count=U32(n,0xc);SU32(b,0x10,count);REG(6,count);}
        SU32(b,0x14,(uint32_t)i+1);REG(7,0);
    }
    /* Exact original table constructor/insertion dependencies, not the host
     * strings_table replacement. Native registrations are still unclosed. */
    {rx_addr al=U32(r,0x45c),table;REG(1,al);
     if(al){table=custom_alloc_at(b,0x8273d7,r,0x20,0);REG(6,table);
        if(table){REG(0,al);REG(7,2*U32(r,4));table=native_hash_construct_at(b,0x8273f9,table,al,2*U32(r,4),0);}}
     else{table=allocate_at(b,0x827400,0x78fbe0,0x20);REG(6,table);
        if(table){al=allocate_at(b,0x827410,0x78fbe0,4);if(al)SU32(al,0,0x9753b0);REG(0,al);REG(7,2*U32(r,4));table=native_hash_construct_at(b,0x827437,table,al,2*U32(r,4),1);}}
     SU32(r,0x10,table);REG(6,0);SU32(b,0x14,0);}
    for(int32_t i=0;i<I32(r,4);i++){
        REG(1,U32(r,0xc));REG(7,U32(r,0x10));REG(0,b+0x14);REG(2,U32(r,0xc)+4*(uint32_t)i);
        native_hash_insert_at(b,0x827464,U32(r,0x10),U32(r,0xc)+4*(uint32_t)i,b+0x14);REG(6,(uint32_t)i+1);SU32(b,0x14,(uint32_t)i+1);
    }
    close_wrapper_at(b,0x82747f,r,0);SU32(r,0x33c,0);result=1;goto out;
strings_failed:
    /* Default branch keeps first cdecl arg pending during second free. */
    if(U32(r,0x45c)){failure_stage=0;goto custom_failed;}
    REG(1,U32(r,8));free_at(b,0x8253c5,U32(r,8));REG(2,U32(r,0xc));free_at(b-4,0x8253ce,U32(r,0xc));
    SU32(r,8,0);SU32(r,0xc,0);close_wrapper_at(b,0x8253ea,r,0);goto out;
blob_failed:
    if(U32(r,0x45c)){failure_stage=1;goto custom_failed;}
    REG(1,U32(r,8));free_at(b,0x8254c1,U32(r,8));REG(2,U32(r,0xc));free_at(b-4,0x8254ca,U32(r,0xc));
    REG(0,U32(r,0x14));free_at(b-8,0x8254d3,U32(r,0x14));SU32(r,0x14,0);SU32(r,0xc,0);SU32(r,8,0);close_wrapper_at(b,0x8254f2,r,1);goto out;
attributes_failed:
    if(U32(r,0x45c)){failure_stage=2;goto custom_failed;}
    REG(0,U32(r,8));free_at(b,0x825749,U32(r,8));REG(1,U32(r,0xc));free_at(b-4,0x825752,U32(r,0xc));
    if(U32(b,0x50)){REG(2,U32(r,0x14));free_at(b,0x825764,U32(r,0x14));}
    REG(0,U32(r,0x24));free_at(b,0x825770,U32(r,0x24));SU32(r,0x24,0);SU32(r,0x14,0);SU32(r,0xc,0);SU32(r,8,0);close_wrapper_at(b,0x8254f2,r,1);goto out;
nodes_failed:
    if(U32(r,0x45c)){failure_stage=3;REG(6,0);goto custom_failed;}
    inline_node_free(b,0x8267c0,0x8267c9,0x8267e0,U32(r,8),1);
    inline_node_free(b,0x8267f7,0x8267fa,0x826811,U32(r,0xc),0);
    if(U32(b,0x50))inline_node_free(b,0x82682f,0x826832,0x826849,U32(r,0x14),0);
    inline_node_free(b,0x826860,0x826863,0x82687a,U32(r,0x24),0);REG(6,0);
    SU32(r,8,0);SU32(r,0xc,0);SU32(r,0x14,0);SU32(r,0x24,0);close_wrapper_at(b,0x8254f2,r,1);goto out;
custom_failed:
    if(failure_stage==0){
        custom_free_at(b,0x8253a9,r,U32(r,8),0);custom_free_at(b,0x8253ba,r,U32(r,0xc),1);
        SU32(r,8,0);SU32(r,0xc,0);close_wrapper_at(b,0x8253ea,r,0);goto out;
    }
    if(failure_stage==1){
        custom_free_at(b,0x825494,r,U32(r,8),1);custom_free_at(b,0x8254a5,r,U32(r,0xc),0);custom_free_at(b,0x8254b6,r,U32(r,0x14),1);
    }else if(failure_stage==2){
        custom_free_at(b,0x8256ff,r,U32(r,8),1);custom_free_at(b,0x825710,r,U32(r,0xc),0);
        if(U32(b,0x50))custom_free_at(b,0x825727,r,U32(r,0x14),1);
        custom_free_at(b,0x825738,r,U32(r,0x24),0);SU32(r,0x24,0);
    }else{
        custom_free_at(b,0x82675f,r,U32(r,8),0);custom_free_at(b,0x826770,r,U32(r,0xc),1);
        if(U32(b,0x50))custom_free_at(b,0x826787,r,U32(r,0x14),0);
        custom_free_at(b,0x826798,r,U32(r,0x24),1);SU32(r,0x24,0);
    }
    SU32(r,0x14,0);SU32(r,0xc,0);SU32(r,8,0);close_wrapper_at(b,0x8254f2,r,1);
out:
    REG(7,olddi);REG(6,oldsi);REG(3,oldbx);REG(5,oldbp);REG(0,result);rx_set_sp(entry);return result;
}

#endif /* native-only typed composition; original oracle callers remain original */
