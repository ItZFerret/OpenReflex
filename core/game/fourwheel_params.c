/* Dedicated copies of the four-wheel parameter walk. Original instruction
 * streams are identical modulo code address and root type. Guest stack locals
 * retain original spills and uninitialized NEXT; do not sanitize invalid data. */
#include "fourwheel_params.h"
#include "build_vehicle_util.h"
#include "crt.h"
#include "phys_crt.h"
#ifdef RX_ORACLE
extern void rx_fourwheel_params_reg(unsigned,uint32_t);
extern uint32_t rx_fourwheel_params_getreg(unsigned);
#define REG(i,v) rx_fourwheel_params_reg(i,v)
#define GET(i) rx_fourwheel_params_getreg(i)
#else
#define REG(i,v) ((void)0)
#define GET(i) 0u
#endif
/* Actual original dynamic callees in oracle; typed registered native methods
 * in product. Unknown methods use normal missing-function dispatch. Preserve
 * consumed argument writes: only repair the synthetic return slot afterward. */
static uint32_t call_target(rx_addr b,rx_addr ra,rx_addr self,rx_addr fn,
                             unsigned n,uint32_t a,uint32_t d,uint32_t e){
    uint32_t args[3]={a,d,e},r;
    REG(1,self);
#ifdef RX_ORACLE
    veh_call_sp(b,ra,(int)n,a,d,e);rx_set_sp(b);
    r=rx_call_original(fn,RX_THISCALL,self,(int)n,args);
    SU32(b-4-4*n,0,ra);
#else
    void* f=RX_FN_INTS(fn, (n < 3 ? (int)n : 3) + 1);
    if(f){veh_call_sp(b,ra,(int)n,a,d,e);
        switch(n){case 0:r=((uint32_t(*)(rx_addr))f)(self);break;
        case 1:r=((uint32_t(*)(rx_addr,uint32_t))f)(self,a);break;
        case 2:r=((uint32_t(*)(rx_addr,uint32_t,uint32_t))f)(self,a,d);break;
        default:r=((uint32_t(*)(rx_addr,uint32_t,uint32_t,uint32_t))f)(self,a,d,e);break;}}
    else{rx_set_sp(b);r=rx_call_original(fn,RX_THISCALL,self,(int)n,args);}
#endif
    rx_set_sp(b);return r;
}
static uint32_t call_virtual(rx_addr b,rx_addr ra,rx_addr self,uint32_t slot,
                             unsigned n,uint32_t a,uint32_t d,uint32_t e){
    return call_target(b,ra,self,U32(U32(self,0),slot),n,a,d,e);
}
static int32_t string_import(rx_addr b,rx_addr ra,uint32_t slot,
                             rx_addr dst,rx_addr src,uint32_t n){
    unsigned argc=slot==0x9162bc?2:3;int32_t r;
#ifdef RX_ORACLE
    uint32_t args[3]={dst,src,n};veh_call_sp(b,ra,(int)argc,dst,src,n);rx_set_sp(b);
    r=(int32_t)rx_call_original(U32(0,slot),RX_CDECL,0,(int)argc,args);
    SU32(b-4-4*argc,0,ra);
#else
    veh_call_sp(b,ra,(int)argc,dst,src,n);
    r=argc==2?crt_stricmp(dst,src):(int32_t)crt_strncpy(dst,src,n);
#endif
    rx_set_sp(b);return r;
}
/* The original inlined two-byte strcmp also leaves ESI/EAX pointers and CL.
 * These caller registers are saved by later virtual/CRT callees on the guest
 * stack, so retain them even when the public output mask does not list them. */
static int compare_names(rx_addr a,rx_addr b){
    REG(0,a);REG(6,b);
    for(;;){
        uint8_t x=U8(a,0),y=U8(b,0);REG(1,(GET(1)&~255u)|x);
        if(x!=y){int r=x<y?-1:1;REG(0,(uint32_t)r);return r;}
        if(!x){REG(0,0);return 0;}
        x=U8(a,1);y=U8(b,1);REG(1,(GET(1)&~255u)|x);
        if(x!=y){int r=x<y?-1:1;REG(0,(uint32_t)r);return r;}
        a+=2;b+=2;REG(0,a);REG(6,b);
        if(!x){REG(0,0);return 0;}
    }
}
/* Exact private 0048bd20 closure, not another registry entry. Unlike the older
 * helper, it passes the float array after saved EBX and pops EBX before fld. */
static void read_curve(rx_addr r,rx_addr node,rx_addr c){
    const rx_addr entry=rx_sp(),b=entry-0x54,buf=entry-0x50;uint32_t a;
    const uint32_t saved_ebx=GET(3);SU32(b,0,saved_ebx);REG(3,node);REG(0,U32(r,0));REG(2,U32(U32(r,0),0xe8));
    a=call_virtual(b,0x48bd3a,r,0xe8,2,node,0x93b484,0);
    REG(2,U32(r,0));REG(0,U32(U32(r,0),0x134));
    call_virtual(b,0x48bd4a,r,0x134,3,a,c,1);
    REG(2,U32(r,0));REG(0,U32(U32(r,0),0xe8));
    a=call_virtual(b,0x48bd5c,r,0xe8,2,node,0x93b490,0);
    REG(2,U32(r,0));REG(2,U32(U32(r,0),0x130));
    call_virtual(b,0x48bd6f,r,0x130,3,a,c+0x54,1);
    REG(0,U32(r,0));REG(2,U32(U32(r,0),0xe8));
    a=call_virtual(b,0x48bd81,r,0xe8,2,node,0x93b498,0);
    REG(2,U32(U32(r,0),0x130));
    call_virtual(b,0x48bd94,r,0x130,3,a,c+0x58,1);
    REG(0,U32(U32(r,0),0xcc));REG(2,buf);
    call_virtual(b,0x48bdab,r,0xcc,3,node,buf,2*U32(c,0));
    REG(2,0);REG(3,saved_ebx);rx_set_sp(buf);
    if(I32(c,0)>0){REG(0,buf);REG(1,c+0x2c);}
    for(int32_t i=0;i<I32(c,0);i++){
        SU32(c,4+4*(uint32_t)i,veh_x87_copy(U32(buf,8*(uint32_t)i)));
        SU32(c,0x2c+4*(uint32_t)i,veh_x87_copy(U32(buf,8*(uint32_t)i+4)));
        REG(0,buf+8*(uint32_t)(i+1));REG(1,c+0x2c+4*(uint32_t)(i+1));REG(2,(uint32_t)(i+1));
    }
    rx_set_sp(entry);
}
/* All offsets below are relative to 004cb3f0; identical relative return sites
 * were checked in buggy and truck2WD. The vtable is intentionally reloaded at
 * the same boundaries; caller-provided virtuals remain real ABI callees. */
static uint32_t apply(rx_addr p,rx_addr r,rx_addr code,rx_addr type){
    const rx_addr entry=rx_sp(),aligned=(entry-4)&~15u,b=aligned-0x1050;
    const rx_addr name=b+0x48,next=b+0x848,cursor=b+0x1c;
    const uint32_t oldebx=GET(3),oldesi=GET(6),oldedi=GET(7),oldebp=GET(5);
    SU32(entry-4,0,oldebp);REG(5,entry-4);SU32(aligned-8,0,GET(1));
    veh_cookie(aligned-0x1044,0x1040);
    SU32(b+8,0,oldebx);SU32(b+4,0,oldesi);SU32(b,0,oldedi);SU32(b,0x40,p);
    rx_set_sp(b);REG(3,r);REG(2,U32(r,0));REG(0,U32(U32(r,0),4));
    uint32_t node=call_virtual(b,code+0x34,r,4,0,0,0,0),ret=0;
    if(!node)goto done;
    /* NAME root fetch: size/destination already pushed while root is called. */
    rx_addr cached=U32(r,0);REG(6,cached);SU32(b-4,0,0x800);SU32(b-8,0,name);REG(0,U32(cached,4));REG(2,cached);
    node=call_virtual(b-8,code+0x66,r,4,0,0,0,0);
    REG(2,U32(cached,0xa0));
    call_target(b,code+0x71,r,U32(cached,0xa0),3,node,name,0x800);
    cached=U32(r,0);REG(6,cached);SU32(b-4,0,0x93b470);REG(2,U32(cached,4));REG(0,cached);
    node=call_virtual(b-4,code+0x81,r,4,0,0,0,0);
    REG(0,U32(cached,0xe8));
    node=call_target(b,code+0x8c,r,U32(cached,0xe8),2,node,0x93b470,0);
    REG(2,U32(U32(r,0),0x13c));
    call_virtual(b,code+0xa0,r,0x13c,3,node,name,0x40);
    if(compare_names(name,type))goto done;
    cached=U32(r,0);REG(6,cached);SU32(b-4,0,cursor);SU32(b-8,0,0);REG(2,cached);REG(0,U32(cached,4));
    node=call_virtual(b-8,code+0xef,r,4,0,0,0,0);
    REG(2,U32(cached,0x20));
    node=call_target(b,code+0xf7,r,U32(cached,0x20),3,node,0,cursor);SU32(b,0x14,node);
    REG(7,node);
    while(node){
        REG(0,U32(r,0));REG(2,U32(U32(r,0),0xa0));
        call_virtual(b,code+0x127,r,0xa0,3,node,name,0x800);
        uint32_t matched=0;REG(0,p);REG(6,0);SU32(b,0x3c,0);
        for(uint32_t i=0;i<U32(p,0x400);i++){
            rx_addr rec=U32(p,4*i);
            REG(1,p);REG(2,i);REG(0,name);
            if(string_import(b,code+0x157,0x9162bc,name,rec,0))goto advance;
            REG(1,rec);REG(2,p);SU32(b,0x18,0);SU32(b,0x38,rec);
            const uint32_t kind=U32(rec,0x40);REG(0,kind);
            if(kind>5)goto advance;
            uint32_t k=0;rx_addr first=rec+(kind==1||kind==4?0x50:0x44),dst=first;
            if(kind==3||kind==4){REG(0,first);SU32(b,0x20,first);SU32(b,0x14,first);}
            if(kind==5){REG(0,first);SU32(b,0x44,first);SU32(b,0x20,first);}
            do{
                if((kind<3&&k==0)||(kind>=3&&k<3)){
                    if(kind==0||kind==3){REG(0,kind==0?U32(r,0):dst);REG(2,U32(U32(r,0),0xcc));
                        call_virtual(b,code+(kind==0?0x193:0x231),r,0xcc,3,node,dst,1);}
                    else if(kind==1||kind==4){REG(0,kind==1?U32(r,0):dst);REG(2,U32(U32(r,0),0xc0));
                        call_virtual(b,code+(kind==1?0x336:0x3d4),r,0xc0,3,node,dst,1);}
                    else{REG(2,node);REG(6,r);REG(7,dst);
                        veh_call_sp(b,code+(kind==2?0x49f:0x555),1,node,0,0);read_curve(r,node,dst);rx_set_sp(b);}
                }
                REG(0,U32(r,0));REG(2,U32(U32(r,0),0x28));
                const uint32_t next_ra[6]={0x1a1,0x344,0x4ae,0x23f,0x3e2,0x563};
                node=call_virtual(b,code+next_ra[kind],r,0x28,1,U32(cursor,0),0,0);
                k++;if(kind!=2)REG(7,node);if(kind==3)REG(2,k);if(kind==3||kind==4||kind==5){SU32(b,0x18,k);dst+=kind==3?4:kind==4?16:0x5c;SU32(b,kind==5?0x20:0x14,dst);}
                else SU32(b,0x14,node);
                if(!node)break;
                REG(0,kind==2?node:U32(r,0));REG(2,U32(U32(r,0),0xa0));
                const uint32_t name_ra[6]={0x1c9,0x36c,0x4d0,0x26d,0x40b,0x590};
                call_virtual(b,code+name_ra[kind],r,0xa0,3,node,next,0x800);
            }while(compare_names(name,next)==0);
            if(kind==3||kind==4||kind==5)SU32(b,0x14,node);
            /* Replication retains the compiler's register setup too. Counts
             * are actual same-name rows, including rows beyond the first 3. */
            if(kind==3){
                REG(6,first);REG(0,rec);REG(2,k);
                if(k<3){REG(0,first+4*k);REG(1,3-k);}
            }else if(kind==4){
                REG(2,k);if(k<3){REG(0,first+16*k);REG(1,3-k);REG(2,first);}
            }else if(kind==5){
                REG(1,k);if(k<3){REG(0,first+0x5c*k);REG(2,3-k);}
            }
            if(kind>=3)for(uint32_t j=k;j<3;j++){
                if(kind==3){SU32(first,4*j,veh_x87_copy(U32(first,0)));REG(0,first+4*(j+1));REG(1,3-j-1);}
                else if(kind==4){veh_copy(first+16*j,first,16);REG(0,first+16*(j+1));REG(1,3-j-1);}
                else{REG(6,first);REG(7,first+0x5c*j);REG(1,0x17);
                    veh_copy(first+0x5c*j,first,0x5c);REG(0,first+0x5c*(j+1));REG(2,3-j-1);
                    REG(6,first+0x5c);REG(7,first+0x5c*(j+1));REG(1,0);}
            }
            if(kind==5&&k<3)REG(7,node);
            /* strncpy executes even when NEXT was never written: retain the
             * original stack's bytes, including malformed/last-node behavior. */
            if(kind==3){REG(1,next);REG(2,name);}else{REG(0,next);REG(1,name);}
            string_import(b,code+(kind==2?0x521:0x621),0x916260,name,next,0x7ff);
            if(kind==2)REG(7,node);REG(6,1);SU8(name,0x7ff,0);matched=1;
advance:   REG(0,i+1);REG(2,p);SU32(b,0x3c,i+1);
        }
        if(!matched&&node){REG(0,U32(r,0));REG(2,U32(U32(r,0),0x28));
            node=call_virtual(b,code+0x660,r,0x28,1,U32(cursor,0),0,0);SU32(b,0x14,node);REG(7,node);}
    }
    REG(0,U32(r,0));REG(2,U32(U32(r,0),0x24));
    call_virtual(b,code+0x67c,r,0x24,1,U32(cursor,0),0,0);ret=1;
done:
    REG(7,oldedi);REG(6,oldesi);REG(3,oldebx);
    REG(1,U32(b,0x104c)^(aligned-0x1044));REG(0,ret);
    veh_call_sp(aligned-0x1044,code+(ret?0x692:0x4b),0,0,0,0);
    phys_security_check_cookie(U32(b,0x104c)^(aligned-0x1044));REG(5,oldebp);rx_set_sp(entry);return ret;
}
uint32_t rx_utv_params_apply(rx_addr p,rx_addr r){return apply(p,r,0x4cb3f0,0x93c734);}
RX_FUNCTION(0x004cb3f0,rx_utv_params_apply)
uint32_t rx_buggy_params_apply(rx_addr p,rx_addr r){return apply(p,r,0x4b0bc0,0x93c5cc);}
RX_FUNCTION(0x004b0bc0,rx_buggy_params_apply)
uint32_t rx_truck2wd_params_apply(rx_addr p,rx_addr r){return apply(p,r,0x44f7a0,0x93ba60);}
RX_FUNCTION(0x0044f7a0,rx_truck2wd_params_apply)
