/* Exact string hash ctor/insert candidate. Original allocator virtuals remain
 * typed calls; no StdAllocator-only shortcut or source-default table. */
#include "guest_string_hash.h"
#include "build_alloc.h"
#include "build_asset.h"
#include "world_misc.h"
#include "build_vehicle_util.h"
#include "phys_vec.h"
#include "crt.h"
#ifdef RX_ORACLE
extern void rx_guest_hash_reg(unsigned,uint32_t);
extern uint32_t rx_guest_hash_getreg(unsigned);
extern void rx_guest_hash_xmm(unsigned,uint32_t,unsigned);
#define REG(i,v) rx_guest_hash_reg(i,v)
#define GET(i) rx_guest_hash_getreg(i)
#define XMM(i,v,clear) rx_guest_hash_xmm(i,v,clear)
#else
#define REG(i,v) ((void)0)
#define GET(i) 0u
#define XMM(i,v,clear) ((void)0)
#endif
static rx_addr aligned_at(rx_addr b,rx_addr ra,uint32_t size){
    rx_addr p;veh_call_sp(b,ra,1,size,0,0);
#ifdef RX_ORACLE
    rx_set_sp(b);p=rx_call_original(0x78fbe0,RX_CDECL,0,1,&size);SU32(b-8,0,ra);
#else
    p=rx_reflex_aligned_alloc(size);
#endif
    rx_set_sp(b);return p;
}
static void memset_at(rx_addr b,rx_addr ra,rx_addr p,uint32_t size){
    uint32_t args[3]={p,0,size};veh_call_sp(b,ra,3,p,0,size);
#ifdef RX_ORACLE
    rx_set_sp(b);rx_call_original(0x8fa06a,RX_CDECL,0,3,args);SU32(b-16,0,ra);
#else
    crt_memset(p,0,size);
#endif
    rx_set_sp(b);
}
static rx_addr allocator_alloc_at(rx_addr b,rx_addr ra,rx_addr al,uint32_t size,int target_edx){
    const rx_addr vt=U32(al,0),fn=U32(vt,4);REG(1,al);
    if(target_edx){REG(2,fn);}else{REG(2,vt);REG(0,fn);}
    rx_addr p;
#ifdef RX_ORACLE
    veh_call_sp(b,ra,1,size,0,0);rx_set_sp(b);p=rx_call_original(fn,RX_THISCALL,al,1,&size);SU32(b-8,0,ra);
#else
    rx_addr(*f)(rx_addr,uint32_t)=RX_FN(fn,rx_addr(*)(rx_addr,uint32_t));
    if(f){veh_call_sp(b,ra,1,size,0,0);p=f(al,size);}else{rx_set_sp(b);p=rx_call_original(fn,RX_THISCALL,al,1,&size);}
#endif
    rx_set_sp(b);return p;
}
static void allocator_free_at(rx_addr b,rx_addr ra,rx_addr al,rx_addr p){
    const rx_addr vt=U32(al,0),fn=U32(vt,8);REG(1,al);REG(0,p);REG(2,fn);
#ifdef RX_ORACLE
    veh_call_sp(b,ra,1,p,0,0);rx_set_sp(b);rx_call_original(fn,RX_THISCALL,al,1,&p);SU32(b-8,0,ra);
#else
    uint32_t(*f)(rx_addr,rx_addr)=RX_FN(fn,uint32_t(*)(rx_addr,rx_addr));
    if(f){veh_call_sp(b,ra,1,p,0,0);f(al,p);}else{rx_set_sp(b);rx_call_original(fn,RX_THISCALL,al,1,&p);}
#endif
    rx_set_sp(b);
}
/* Existing rx_strtab_rehash has matching valid standard-allocator semantics,
 * but its host helper does not mirror this frame and restricts allocators.
 * Private exact variant retains original allocator virtuals and consumed
 * argument-slot writes without replacing the accepted public0084df50 owner. */
static void rehash_exact(rx_addr t,uint32_t n){
    const rx_addr e=rx_sp(),b=e-12;
    const uint32_t oldbx=GET(3),oldbp=GET(5),oldsi=GET(6),olddi=GET(7);
    SU32(e-4,0,oldbx);SU32(e-8,0,oldbp);SU32(e-12,0,oldsi);REG(5,t);REG(6,4*n);
    rx_addr al=U32(t,0x18);REG(0,U32(al,0));
    rx_addr buckets=allocator_alloc_at(b,0x84df6a,al,4*n,1);REG(3,buckets);
    memset_at(b,0x84df75,buckets,4*n);REG(0,0);SU32(e+4,0,0);
    if(I32(t,0)>0){
        SU32(b-4,0,olddi);rx_set_sp(b-4);
        for(int32_t i=0;i<I32(t,0);i++){
            rx_addr node=U32(U32(t,0xc),4*(uint32_t)i);
            while(node){
                REG(6,node);const uint32_t bi=U32(node,8)%n;REG(2,bi);REG(7,node+0xc);
                rx_addr next=U32(node,0xc);REG(1,next);REG(0,U32(buckets,4*bi));
                SU32(node,0xc,U32(buckets,4*bi));SU32(buckets,4*bi,node);node=next;
            }
            REG(0,(uint32_t)i+1);SU32(e+4,0,(uint32_t)i+1);
        }
        REG(7,olddi);rx_set_sp(b);
    }
    allocator_free_at(b,0x84dfc9,U32(t,0x18),U32(t,0xc));REG(0,n);
    SU32(t,0xc,buckets);SU32(t,0,n);REG(6,oldsi);REG(5,oldbp);REG(3,oldbx);rx_set_sp(e);
}
static void rehash_at(rx_addr b,rx_addr t,uint32_t n){
    veh_call_sp(b,0x843520,2,t,n,0);rehash_exact(t,n);
    /* Original rehash overwrites its consumed table argument with loop index.
     * Never rewrite args here after return. Private exact body is shared by
     * oracle/product so growth frame behavior is actually compared. */
    rx_set_sp(b);
}
static uint32_t hash_at(rx_addr b,rx_addr key){
    veh_call_sp(b,0x843530,0,0,0,0);REG(2,key);uint32_t h;
#ifdef RX_ORACLE
    /* 005c3380 takes the key in EDX: passed explicitly (an observing adapter is not always on the call path) */
    rx_set_sp(b);{rx_regs in={0},out={0};in.r[2]=key;rx_call_original_ex(0x5c3380,&in,&out,0,0);h=out.r[0];}SU32(b-4,0,0x843530);
#else
    /* Reuse registered semantic API. Only its nonempty-string saved ESI slot
     * needs mirroring; no allocator or callback is hidden in this pure hash. */
    if(U8(key,0))SU32(b-8,0,GET(6));h=rx_world_elf_hash(key);
#endif
    rx_set_sp(b);return h;
}
rx_addr rx_guest_string_hash_construct(rx_addr t,rx_addr al,uint32_t buckets,uint32_t load,uint32_t owned){
    const rx_addr e=rx_sp();
    if(al){SU32(t,0x18,al);SU8(t,0x1c,(uint8_t)owned);}
    else{al=aligned_at(e,0x82ab37,4);if(al)SU32(al,0,0x9753b0);SU32(t,0x18,al);SU8(t,0x1c,1);}
    XMM(0,load,1);REG(1,al);SU32(t,0,buckets);SU32(t,4,load);SU32(t,0x10,0);SU32(t,0x14,0xffffffffu);SU32(t,8,0);
    REG(2,U32(U32(al,0),4));REG(0,4*buckets);
    rx_addr p=allocator_alloc_at(e,0x82ab83,al,4*buckets,1);
    REG(1,4*U32(t,0));SU32(t,0xc,p);memset_at(e,0x82ab95,p,4*U32(t,0));REG(0,t);rx_set_sp(e);return t;
}
RX_FUNCTION(0x0082ab20,rx_guest_string_hash_construct)
/* The inlined strcmp compares two bytes/iteration and writes only DL, leaving
 * upper24 EDX intact. Collision/update liveEDX must retain those bits. */
static int same_key(rx_addr a,rx_addr key){
    REG(0,a);REG(1,key);
    for(;;){uint8_t x=U8(a,0),y=U8(key,0);REG(2,(GET(2)&~255u)|x);
        if(x!=y){REG(0,x<y?0xffffffffu:1);return 0;}if(!x){REG(0,0);return 1;}
        x=U8(a,1);y=U8(key,1);REG(2,(GET(2)&~255u)|x);
        if(x!=y){REG(0,x<y?0xffffffffu:1);return 0;}
        a+=2;key+=2;REG(0,a);REG(1,key);if(!x){REG(0,0);return 1;}
    }
}
uint32_t rx_guest_string_hash_insert(rx_addr t,rx_addr keyp,rx_addr valp){
    const rx_addr e=rx_sp(),b=e-20;const uint32_t oldbx=GET(3),oldbp=GET(5),oldsi=GET(6);
    float n=(float)I32(t,0),count=(float)I32(t,8);XMM(0,rx_fb(n),0);XMM(1,rx_fb(count),0);
    float ratio=phys_div(count,n);XMM(1,rx_fb(ratio),0);REG(0,t+8);SU32(e-4,0,t+8);rx_set_sp(e-8);
    if(ratio>F32(t,4)){float grown=phys_mul(n,F32(0,0x9322a4));XMM(0,rx_fb(grown),0);uint32_t size=(uint32_t)rx_cvtt(grown);REG(0,size);rehash_at(e-8,t,size);}
    REG(1,keyp);SU32(e-12,0,oldbx);rx_addr key=U32(keyp,0);REG(3,key);SU32(e-16,0,oldbp);SU32(e-20,0,oldsi);
    uint32_t h=hash_at(b,key);REG(2,0);REG(1,h);uint32_t bucket=h%U32(t,0);SU32(b,0xc,h);REG(5,bucket);REG(2,U32(t,0xc));
    rx_addr node=U32(U32(t,0xc),4*bucket);REG(6,node);
    while(node){
        if(U32(node,8)==h){if(same_key(U32(node,0),key)){
                REG(0,valp);REG(1,U32(valp,0));SU32(node,4,U32(valp,0));
                REG(6,oldsi);REG(5,oldbp);REG(3,oldbx);rx_set_sp(e);return valp;}
            REG(1,h);}
        node=U32(node,0xc);REG(6,node);
    }
    rx_addr al=U32(t,0x18);REG(1,al);REG(2,U32(al,0));REG(0,U32(U32(al,0),4));
    rx_addr fresh=allocator_alloc_at(b,0x843590,al,16,0);
    if(fresh)SU32(fresh,0xc,0);
    /* Null allocation is deliberately not a successful/no-op insert: the
     * original subsequently writes through zero. Retain the original writes. */
    REG(1,keyp);REG(2,U32(keyp,0));SU32(fresh,0,U32(keyp,0));REG(1,valp);REG(2,U32(valp,0));SU32(fresh,4,U32(valp,0));REG(1,h);SU32(fresh,8,h);
    REG(2,U32(t,0xc));REG(1,U32(U32(t,0xc),4*bucket));SU32(fresh,0xc,U32(U32(t,0xc),4*bucket));SU32(U32(t,0xc),4*bucket,fresh);
    REG(0,t+8);SU32(t,8,U32(t,8)+1);REG(6,oldsi);REG(5,oldbp);REG(3,oldbx);rx_set_sp(e);return t+8;
}
RX_FUNCTION(0x008434f0,rx_guest_string_hash_insert)
