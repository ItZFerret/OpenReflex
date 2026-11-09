/* Isolated exact getter/cleanup dependency. No vehicle vf8 activation, guest stream
 * casts, parser substitute or cleanup no-op. OS imports retain project contracts. */
#include "crt_atexit.h"
#include "tuning_registry.h"
#include "build_alloc.h"
#include "build_vehicle_util.h"
#include "../src/rx_os.h"

#ifdef RX_ORACLE
extern void rx_tuning_observe_reg(unsigned index,uint32_t value);
#define REG(i,v) rx_tuning_observe_reg((i),(v))
#else
#define REG(i,v) ((void)0)
#endif
/* External helpers reserve their own call frame; native C callees do not. */
static uint32_t system1(rx_addr b,uint32_t ra,uint32_t slot,uint32_t arg){
    veh_call_sp(b,ra,1,arg,0,0);
    rx_set_sp(b);
    uint32_t result=rx_os_import(slot,1,&arg);
    veh_call_sp(b,ra,1,arg,0,0);rx_set_sp(b);return result;
}
/* Original0078fbe0/0078fc50 wrappers, local to avoid changing existing registrations.
 * The existing CRT backend is reused; unlike bare build_alloc wrappers, these keep
 * the double-check/double-enter lock path and exact nested stack/return slots. */
static rx_addr aligned_alloc_exact(uint32_t size){
    const rx_addr entry=rx_sp(),b=entry-4;
    if(U8(0,0xda67d8)){
        system1(b,0x78fbf7,0x91600c,0xd61704);
        if(U8(0,0xda67d8))system1(b,0x78fc07,0x91600c,0xd61704);
    }
    veh_call_sp(b,0x78fc14,2,size,16,0);
    REG(0,size);rx_set_sp(b);
    const rx_addr result=rx_crt_aligned_malloc(size,16);
    veh_call_sp(b,0x78fc14,2,size,16,0);rx_set_sp(b);
    if(U8(0,0xda67d8)){
        const rx_addr leave_b=b-4; /*saved EDI*/
        system1(leave_b,0x78fc30,0x9160f8,0xd61704);
        if(U8(0,0xda67d8))system1(leave_b,0x78fc40,0x9160f8,0xd61704);
    }
    rx_set_sp(entry);return result;
}
static void aligned_free_exact(rx_addr p){
    const rx_addr entry=rx_sp(),b=entry-4;
    if(U8(0,0xda67d8)){
        system1(b,0x78fc67,0x91600c,0xd61704);
        if(U8(0,0xda67d8))system1(b,0x78fc77,0x91600c,0xd61704);
    }
    veh_call_sp(b,0x78fc82,1,p,0,0);REG(0,p);rx_set_sp(b);
    rx_crt_aligned_free(p);veh_call_sp(b,0x78fc82,1,p,0,0);rx_set_sp(b);
    if(U8(0,0xda67d8)){
        system1(b,0x78fc9b,0x9160f8,0xd61704);
        if(U8(0,0xda67d8))system1(b,0x78fcab,0x9160f8,0xd61704);
    }
    rx_set_sp(entry);
}
/*00790400 exact local initialization, including real InitializeCriticalSection
 * in oracle and the proxy allocation at original allocator frame depth. */
static void registry_init_exact(void){
    const rx_addr entry=rx_sp(),b=entry-4;
    SU32(0,0xd72adc,0);system1(b,0x790414,0x916014,0xd72ae4);
    for(uint32_t i=0;i<0x18;i+=4){SU32(0,0xd92c10+i,0);SU32(0,0xd92c40+i,0);SU32(0,0xd92c64+i,0);}
    /*push4 at0079046c remains live through following stores.*/
    SU32(b-4,0,4);
    const uint32_t zero[]={0xd92c28,0xd92c30,0xd92c34,0xd92c2c,0xd92c38,0xd92c5c,0xd92c60,0xd92c58,0xd92c80,0xd92c84,0xd92c7c};
    for(unsigned i=0;i<sizeof zero/sizeof zero[0];i++)SU32(0,zero[i],0);
    for(uint32_t i=0;i<0x18;i+=4)SU32(0,0xd92c88+i,0);
    SU32(0,0xd92ca4,0);SU32(0,0xd92ca8,0);SU32(0,0xd92ca0,0);
    veh_call_sp(b,0x7904df,1,4,0,0);
    const rx_addr proxy=aligned_alloc_exact(4);rx_set_sp(b);
    if(proxy)SU32(proxy,0,0xd92d2c);
    SU32(0,0xd92d2c,proxy);
    SU32(0,0xd92d38,0);SU32(0,0xd92d3c,0);SU32(0,0xd92d40,0);
    SU32(0,0xd72afc,0);SU32(0,0xd72b00,0);SU8(0,0xd72ad8,0);
    for(rx_addr a=0xd92cec;a<0xd92d2c;a+=4){SU32(a-0x40,0,0);SU32(a,0,0);}
    rx_set_sp(entry);
}
rx_addr rx_tuning_registry_get(void){
    const rx_addr entry=rx_sp(),b=entry-4; /*push ECX, restored on return*/
    if(!(U8(0,0xd72ad4)&1)){
        SU32(0,0xd72ad4,U32(0,0xd72ad4)|1u);
        veh_call_sp(b,0x42fee9,0,0,0,0);registry_init_exact();rx_set_sp(b);
        const uint32_t handler=0x911ed0;
        veh_call_sp(b,0x42fef3,1,handler,0,0);
        rx_set_sp(b);RX_ATEXIT(handler);
        veh_call_sp(b,0x42fef3,1,handler,0,0);rx_set_sp(b);
    }
    rx_set_sp(entry);return 0xd72ad8;
}
RX_FUNCTION(0x0042fed0,rx_tuning_registry_get)

void rx_tuning_registry_queue_clear(rx_addr q){
    const rx_addr entry=rx_sp();
    while(U32(q,0)){
        const rx_addr node=U32(q,4),next=U32(node,8);REG(0,node);REG(1,next);
        if(next)SU32(q,4,next);
        REG(1,U32(q,4));SU32(U32(q,4),4,0);
        if(node){veh_call_sp(entry,0x4db6f6,1,node,0,0);aligned_free_exact(node);rx_set_sp(entry);}
        SU32(q,0,U32(q,0)-1u);
    }
    /*Original deliberately leaves stale head/tail at count0; do not normalize.*/
    rx_set_sp(entry);
}
RX_FUNCTION(0x004db6d0,rx_tuning_registry_queue_clear)

uint32_t rx_tuning_registry_cleanup(void){
    const rx_addr entry=rx_sp(),early=entry-8,b=entry-16;
    rx_addr p=U32(0,0xd92d38);REG(0,p);
    if(p){veh_call_sp(early,0x42ff13,1,p,0,0);aligned_free_exact(p);rx_set_sp(early);}
    p=U32(0,0xd92d2c);REG(0,p);
    SU32(0,0xd92d38,0);SU32(0,0xd92d3c,0);SU32(0,0xd92d40,0);
    veh_call_sp(b,0x42ff35,1,p,0,0);aligned_free_exact(p);rx_set_sp(b);
    veh_call_sp(b,0x42ff42,0,0,0,0);rx_tuning_registry_queue_clear(0xd92ca0);rx_set_sp(b);
    if(U32(0,0xd92c94))system1(b,0x42ff57,0x916010,0xd92c88);
    veh_call_sp(b,0x42ff61,0,0,0,0);rx_tuning_registry_queue_clear(0xd92c7c);rx_set_sp(b);
    if(U32(0,0xd92c70))system1(b,0x42ff70,0x916010,0xd92c64);
    veh_call_sp(b,0x42ff7a,0,0,0,0);rx_tuning_registry_queue_clear(0xd92c58);rx_set_sp(b);
    if(U32(0,0xd92c4c))system1(b,0x42ff89,0x916010,0xd92c40);
    p=U32(0,0xd92c38);REG(0,p);
    if(p){system1(b,0x42ff9b,0x916018,p);SU32(0,0xd92c38,0);}
    veh_call_sp(b,0x42ffab,0,0,0,0);rx_tuning_registry_queue_clear(0xd92c2c);rx_set_sp(b);
    p=U32(0,0xd92c28);REG(0,p);
    if(p){system1(b,0x42ffb7,0x916018,p);SU32(0,0xd92c28,0);}
    if(U32(0,0xd92c1c))system1(b,0x42ffcc,0x916010,0xd92c10);
    system1(b,0x42ffd3,0x916010,0xd72ae4);
    /*pop ESI/EBP before final conditional CloseHandle.*/
    p=U32(0,0xd72adc);REG(0,p);uint32_t result=p;
    if(p){result=system1(early,0x42ffe1,0x916018,p);SU32(0,0xd72adc,0);}
    rx_set_sp(entry);return result;
}
RX_FUNCTION(0x0042ff00,rx_tuning_registry_cleanup)
uint32_t rx_tuning_registry_exit(void){return rx_tuning_registry_cleanup();}
RX_FUNCTION(0x00911ed0,rx_tuning_registry_exit)
