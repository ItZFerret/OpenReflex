/* Exact caller sequencing candidate; no existing product function replacement.
 * Original Database::vf27 is thiscall +FOUR stack arguments, not a name lookup
 * shortcut. The accepted parameter wrappers execute at their real callee ESP.
 * Product dependencies retain their existing implementations/policies; see README
 * for registry initialization and CRT exit-registration acceptance boundaries. */
#include "crt_atexit.h"
#include "fourwheel_param_load.h"
#include "fourwheel_params.h"
#include "build_vehicle.h"
#include "build_vehicle_util.h"
#include "fx_util.h"
#include "phys_crt.h"

#ifdef RX_ORACLE
extern void rx_param_load_set_xmm0(uint64_t low);
#endif

static void mirror(rx_addr caller,uint32_t ra,unsigned n,const uint32_t* args){
    const rx_addr entry=caller-4u*(n+1u);
    SU32(entry,0,ra);
    for(unsigned i=0;i<n;i++)SU32(entry,4+4*i,args[i]);
    rx_set_sp(entry);
}
static rx_addr database_handle(rx_addr caller,uint32_t ra,rx_addr database,
                               rx_addr target,rx_addr name){
    const uint32_t args[4]={name,0x6c6d7862,0,1};
    rx_addr result;
    fx_set_reg(FX_ECX,database);
    mirror(caller,ra,4,args);
#ifdef RX_ORACLE
    rx_set_sp(caller);
    result=rx_call_original(target,RX_THISCALL,database,4,args);
    SU32(caller-20,0,ra); /* preserve actual callee-written argument words */
#else
    void* fn=rx_fn(target);
    if(fn)result=((rx_addr(*)(rx_addr,rx_addr,uint32_t,uint32_t,uint32_t))fn)(database,name,0x6c6d7862,0,1);
    else{rx_set_sp(caller);result=rx_call_original(target,RX_THISCALL,database,4,args);}
#endif
    fx_set_reg(FX_EAX,result);rx_set_sp(caller);return result;
}
static void apply_parameters(rx_addr caller,uint32_t ra,rx_addr p,rx_addr reader,
                              rx_addr target){
    const uint32_t args[2]={p,reader};
    mirror(caller,ra,2,args);
#ifdef RX_ORACLE
    rx_set_sp(caller);
    (void)rx_call_original(target,RX_STDCALL,0,2,args);
    SU32(caller-12,0,ra);
#else
    /* Closed accepted targets, with exact native two-integer C prototypes. */
    if(target==0x4cb3f0)(void)rx_utv_params_apply(p,reader);
    else if(target==0x4b0bc0)(void)rx_buggy_params_apply(p,reader);
    else (void)rx_truck2wd_params_apply(p,reader);
#endif
    rx_set_sp(caller);
}
static void initialize_registry(rx_addr caller,uint32_t ra){
    mirror(caller,ra,0,0);
#ifdef RX_ORACLE
    rx_set_sp(caller);
    (void)rx_call_original(0x790400,RX_CDECL,0,0,0);
    SU32(caller-4,0,ra);
#else
    /* Existing registered initializer dependency, not a new abbreviated body. */
    rx_veh_tuning_registry_init();
#endif
    rx_set_sp(caller);
}
static void register_exit(rx_addr caller,uint32_t ra){
    const uint32_t handler=0x911ed0;
    mirror(caller,ra,1,&handler);rx_set_sp(caller);
    /* Original CRT in oracle; established product exit policy via registered
     * 008f9fbf in product. Do not silently omit the caller's registration call. */
    RX_ATEXIT(handler);
    SU32(caller-8,0,ra);rx_set_sp(caller);
}
static void load_finish(rx_addr caller,rx_addr handle,uint32_t init_ra,uint32_t exit_ra){
    /* Test guard before clearing loading flag, matching original instruction
     * order. Do not replace this with0042fed0: guard is set BEFORE init here. */
    const uint32_t initialized=U8(0,0xd72ad4)&1u;
    SU32(handle,0x4c,0);
    if(!initialized){
        SU32(0,0xd72ad4,U32(0,0xd72ad4)|1u);
        initialize_registry(caller,init_ra);
        register_exit(caller,exit_ra);
    }
}
static void short_load(rx_addr p,rx_addr code,rx_addr asset_name,rx_addr apply_target){
    const rx_addr entry=rx_sp(),ebp=entry-4,aligned=ebp&~7u,caller=aligned-8;
    const fx_saved saved=fx_save();
    SU32(ebp,0,saved.ebp);fx_set_reg(FX_EBP,ebp);
    SU32(aligned-4,0,fx_reg(FX_ECX)); /*pushECX*/
    const rx_addr world=U32(0,0xa95844),database=U32(world,8),vt=U32(database,0),target=U32(vt,0x6c);
    fx_set_reg(FX_EAX,target);fx_set_reg(FX_ECX,database);fx_set_reg(FX_EDX,vt);
    SU32(caller,0,saved.esi); /*pushESI after database target loads*/
    rx_set_sp(caller);
    const rx_addr handle=database_handle(caller,code+0x25,database,target,asset_name);
    fx_set_reg(FX_ESI,handle);
    rx_addr asset=U32(handle,0x44);fx_set_reg(FX_EAX,asset);
    SU32(handle,0x4c,1);
    const rx_addr reader=asset?asset-0x460:0;
    fx_set_reg(FX_EAX,reader);fx_set_reg(FX_ECX,p);
    apply_parameters(caller,code+0x48,p,reader,apply_target);
    load_finish(caller,handle,code+0x64,code+0x6e);
    SU32(0,0xd92cd4,0x4391a0);
    fx_restore(saved);rx_set_sp(entry);
}
void rx_param_load_utv_exact(rx_addr p){short_load(p,0x4cb360,0x93c728,0x4cb3f0);}
void rx_param_load_buggy_exact(rx_addr p){short_load(p,0x4b0b30,0x93c5bc,0x4b0bc0);}

void rx_param_load_truck_exact(rx_addr p,uint32_t engine_class){
    const rx_addr entry=rx_sp(),ebp=entry-4,aligned=ebp&~7u,E=aligned-0x34,caller=E-4;
    const fx_saved saved=fx_save();
    SU32(ebp,0,saved.ebp);fx_set_reg(FX_EBP,ebp);
    veh_cookie(E,0x30);
    /* Exact stack-built names; E+0 remains deliberately uninitialized. */
    SU32(E,0x1c,U32(0,0x93ba34));SU32(E,0x20,U32(0,0x93ba38));
    SU32(E,0x24,U32(0,0x93ba3c));SU32(E,0x28,U32(0,0x93ba40));SU8(E,0x2c,U8(0,0x93ba44));
    SU32(E,4,U32(0,0x93ba48));SU32(E,8,U32(0,0x93ba4c));
    SU32(caller,0,saved.esi);
    SU32(E,0xc,U32(0,0x93ba50));SU32(E,0x10,U32(0,0x93ba54));
    SU32(E,0x14,U32(0,0x93ba58));SU8(E,0x18,U8(0,0x93ba5c));
#ifdef RX_ORACLE
    rx_param_load_set_xmm0(rx_rd64(0x93ba50)); /*lastMOVQ zeros upper64*/
#endif
    fx_set_reg(FX_ECX,U8(0,0x93ba5c));
    fx_set_reg(FX_EDI,p);
    /* Original invalid-class path reads E+0 as a pointer. Do not invent a
     * clean reject or a valid name for it; valid fixtures use only classes7/8. */
    const rx_addr name=engine_class==7?E+0x1c:engine_class==8?E+4:U32(E,0);
    const rx_addr world=U32(0,0xa95844),database=U32(world,8),vt=U32(database,0),target=U32(vt,0x6c);
    fx_set_reg(FX_EDX,vt);fx_set_reg(FX_EAX,target);rx_set_sp(caller);
    const rx_addr handle=database_handle(caller,0x44f732,database,target,name);
    fx_set_reg(FX_ESI,handle);
    const rx_addr asset=U32(handle,0x44);fx_set_reg(FX_EAX,asset);SU32(handle,0x4c,1);
    const rx_addr reader=asset?asset-0x460:0;fx_set_reg(FX_EAX,reader);
    apply_parameters(caller,0x44f752,p,reader,0x44f7a0);
    load_finish(caller,handle,0x44f76e,0x44f778);
    const uint32_t cookie=U32(E,0x30)^E;
    fx_set_reg(FX_ESI,saved.esi);fx_set_reg(FX_ECX,cookie);rx_set_sp(E);
    SU32(0,0xd92cd4,0x4391a0);
    mirror(E,0x44f791,0,0);phys_security_check_cookie(cookie);
    fx_restore(saved);rx_set_sp(entry);
}
