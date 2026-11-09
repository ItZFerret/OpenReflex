/* Exact guest XMLReader reset proposal. No stream conversion, parser fallback,
 * host FILE reinterpretation or omitted virtual cleanup. */
#include "guest_reader_reset.h"
#include "build_alloc.h"
#include "build_vehicle_util.h"
#include "race_misc.h"
#include "../src/rx_os.h"
#ifdef RX_ORACLE
extern void rx_reader_reset_reg(unsigned index,uint32_t value);
#define REG(i,v) rx_reader_reset_reg((i),(v))
#else
#define REG(i,v) ((void)0)
#endif
static void system1(rx_addr b,uint32_t ra,uint32_t slot,rx_addr p){
    veh_call_sp(b,ra,1,p,0,0);rx_set_sp(b);
    rx_os_import(slot,1,&p);
    SU32(b-8,0,ra);rx_set_sp(b);
}
static void crt_free_at(rx_addr b,uint32_t ra,rx_addr p){
    veh_call_sp(b,ra,1,p,0,0);rx_set_sp(b);rx_crt_aligned_free(p);
    SU32(b-8,0,ra);rx_set_sp(b);
}
/* Exact0078fc50 local wrapper needed for final table object release. */
static void free_wrapper(rx_addr p){
    const rx_addr entry=rx_sp(),b=entry-4;
    if(U8(0,0xda67d8)){
        system1(b,0x78fc67,0x91600c,0xd61704);
        if(U8(0,0xda67d8))system1(b,0x78fc77,0x91600c,0xd61704);
    }
    REG(0,p);crt_free_at(b,0x78fc82,p);
    if(U8(0,0xda67d8)){
        system1(b,0x78fc9b,0x9160f8,0xd61704);
        if(U8(0,0xda67d8))system1(b,0x78fcab,0x9160f8,0xd61704);
    }
    rx_set_sp(entry);
}
/* Original dynamic call, not original reset replay. The product uses registered
 * callees; unknown target is reported by normal project dispatch, never zeroed.
 * External/oracle calls enter at caller ESP; native C calls at callee ESP. */
static void vcall(rx_addr b,uint32_t ra,rx_addr fn,rx_addr self,int n,uint32_t arg){
    REG(1,self);
#ifdef RX_ORACLE
    veh_call_sp(b,ra,n,arg,0,0);rx_set_sp(b);
    rx_call_original(fn,RX_THISCALL,self,n,n?&arg:0);
    /* Callees may overwrite consumed argument slots (StdAllocator::vf2 lock1
     * stores its lock address there before tail LeaveCriticalSection). Preserve
     * those writes; only the original call return slot replaces the sentinel. */
    SU32(b-4u-4u*(uint32_t)n,0,ra);rx_set_sp(b);
#else
    void* f=rx_fn(fn);
    if(f){veh_call_sp(b,ra,n,arg,0,0);if(n)((void(*)(rx_addr,uint32_t))f)(self,arg);else((void(*)(rx_addr))f)(self);rx_set_sp(b);}
    else{rx_set_sp(b);rx_call_original(fn,RX_THISCALL,self,n,n?&arg:0);rx_set_sp(b);}
#endif
}
/*007fa1e0 private exact-frame variant of accepted rx_name_hash_free (race_misc.h).
 * Existing helper always reserves3savedregs; original reserves2when count<=0.
 * No duplicate registration/replacement of the accepted entry. */
static void hash_free_exact(rx_addr t){
    const rx_addr entry=rx_sp();rx_addr b=entry-8;
    if(I32(t,0)>0){
        b-=4;
        for(int32_t i=0;i<I32(t,0);i++){
            const rx_addr buckets=U32(t,0xc);REG(0,buckets);
            rx_addr node=U32(buckets,4u*(uint32_t)i);
            while(node){
                const rx_addr al=U32(t,0x18),vt=U32(al,0),p=node;
                REG(1,al);REG(2,vt);REG(0,p);node=U32(node,0xc);
                const rx_addr fn=U32(vt,8);REG(0,fn);
                vcall(b,0x7fa210,fn,al,1,p);
            }
        }
        b+=4;
    }
    const rx_addr al=U32(t,0x18),vt=U32(al,0),buckets=U32(t,0xc),fn=U32(vt,8);
    REG(1,al);REG(2,vt);REG(0,buckets);REG(2,fn);
    vcall(b,0x7fa228,fn,al,1,buckets);
    if(U8(t,0x1c)&&U32(t,0x18)){
        const rx_addr owned=U32(t,0x18),owned_vt=U32(owned,0),destroy=U32(owned_vt,0);
        REG(0,owned_vt);REG(2,destroy);
        vcall(b,0x7fa23f,destroy,owned,1,1);
    }
    rx_set_sp(entry);
}
void rx_guest_reader_reset(rx_addr r){
    const rx_addr entry=rx_sp(),b4=entry-4,b8=entry-8,b12=entry-12;
    const rx_addr allocator=U32(r,0x45c);REG(1,allocator);
    if(allocator){
        const rx_addr vt=U32(allocator,0),fn=U32(vt,0xc);REG(0,vt);REG(2,fn);
        vcall(b4,0x827e14,fn,allocator,0,0);
    }else{
        const uint32_t offsets[5]={8,0xc,0x14,0x1c,0x24};
        const uint32_t enter_ra[5]={0x827e30,0x827e66,0x827e95,0x827ec4,0x827ef3};
        const uint32_t free_ra[5]={0x827e3a,0x827e69,0x827e98,0x827ec7,0x827ef6};
        const uint32_t leave_ra[5]={0x827e50,0x827e7f,0x827eae,0x827edd,0x827f0d};
        for(unsigned i=0;i<5;i++){
            const rx_addr p=U32(r,offsets[i]);
            if(U8(0,0xda67d8))system1(i==0?b8:b12,enter_ra[i],0x91600c,0xd61704);
            crt_free_at(b12,free_ra[i],p);
            if(U8(0,0xda67d8))system1(i==4?b8:b12,leave_ra[i],0x9160f8,0xd61704);
        }
        const rx_addr table=U32(r,0x10);
        if(table){
            REG(0,table);veh_call_sp(b8,0x827f1b,0,0,0,0);hash_free_exact(table);rx_set_sp(b8);
            veh_call_sp(b8,0x827f21,1,table,0,0);free_wrapper(table);rx_set_sp(b8);
        }
    }
    const rx_addr embedded=r+0x340;REG(1,embedded);
    const uint32_t zero[]={0x10,4,8,0xc,0x14,0x18,0x1c,0x20,0x24,0x30,0x34,0x28,0x2c,0x38,0x3c};
    for(unsigned i=0;i<sizeof zero/sizeof zero[0];i++)SU32(r,zero[i],0);
    if(U32(r,0x33c)==embedded){
        const rx_addr vt=U32(embedded,0),fn=U32(vt,4);REG(0,vt);REG(2,fn);
        vcall(b4,0x827f67,fn,embedded,0,0);
    }
    SU32(r,0x33c,0);rx_set_sp(entry);
}
RX_FUNCTION(0x00827e00,rx_guest_reader_reset)
