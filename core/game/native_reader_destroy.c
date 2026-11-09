/* Exact original00824fe0 +0080f8a0 ownership/order/frames. No registration. */
#include "guest_reader_destroy.h"
#include "fx_util.h"
#include "build_vehicle_util.h"
/* Dependency owner: product native_bridge.c; oracle guest_reader_destroy.cpp. */
static void call0(rx_addr B,rx_addr ra,rx_addr fn){
    veh_call_sp(B,ra,0,0,0,0);rx_guest_destroy_dependency(fn,B);rx_set_sp(B);
}
static void call1(rx_addr B,rx_addr ra,rx_addr fn,uint32_t arg,int callee_pops){
    veh_call_sp(B,ra,1,arg,0,0);rx_guest_destroy_dependency(fn,callee_pops?B:B-4);rx_set_sp(B);
}
static void lock_at(rx_addr B,rx_addr ra,rx_addr iat){call1(B,ra,U32(0,iat),0xd61704,1);}
void rx_guest_compressed_destroy_exact(rx_addr s){
    const rx_addr entry=rx_sp();
    SU32(s,0,0x974b48);
    if(U32(s,4)){
        const uint32_t saved_bp=fx_reg(FX_EBP);
        fx_set_reg(FX_ECX,U32(s,8));fx_set_reg(FX_EAX,U32(fx_reg(FX_ECX),0));
        fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0x10));
        SU32(entry-4,0,saved_bp);
        call0(entry-4,0x80f8b7,fx_reg(FX_EDX));
        fx_set_reg(FX_EBP,0);
        if(I32(s,0x14)>0){
            const uint32_t saved_bx=fx_reg(FX_EBX),saved_si=fx_reg(FX_ESI);
            SU32(entry-8,0,saved_bx);SU32(entry-12,0,saved_si);
            fx_set_reg(FX_EBX,s+0xc);
            do{
                fx_set_reg(FX_ESI,U32(fx_reg(FX_EBX),0));
                if(U8(0,0xda67d8))lock_at(entry-12,0x80f8d9,0x91600c);
                call1(entry-12,0x80f8e0,U32(0,0x916350),fx_reg(FX_ESI),0);
                if(U8(0,0xda67d8))lock_at(entry-12,0x80f8f7,0x9160f8);
                fx_set_reg(FX_EBP,fx_reg(FX_EBP)+1);fx_set_reg(FX_EBX,fx_reg(FX_EBX)+4);
            }while((int32_t)fx_reg(FX_EBP)<I32(s,0x14));
            fx_set_reg(FX_ESI,saved_si);fx_set_reg(FX_EBX,saved_bx);
        }
        fx_set_reg(FX_EBP,saved_bp);
    }
    SU32(s,0,0x94dab4);rx_set_sp(entry);
}
void rx_guest_reader_destroy_exact(rx_addr r){
    const rx_addr entry=rx_sp();const uint32_t saved_di=fx_reg(FX_EDI);
    SU32(0,0xa95950,U32(0,0xa95950)-1);SU32(entry-4,0,saved_di);SU32(r,0,0x9753dc);
    if(!U32(r,0x45c)){
        const uint32_t saved_bx=fx_reg(FX_EBX),saved_bp=fx_reg(FX_EBP);
        fx_set_reg(FX_EDI,U32(r,8));SU32(entry-8,0,saved_bx);SU32(entry-12,0,saved_bp);
        fx_set_reg(FX_EBP,U32(0,0x91600c));
        if(U8(0,0xda67d8))call1(entry-12,0x825015,fx_reg(FX_EBP),0xd61704,1);
        fx_set_reg(FX_EBX,U32(0,0x916350));
        call1(entry-12,0x82501e,fx_reg(FX_EBX),fx_reg(FX_EDI),0);
        if(U8(0,0xda67d8))lock_at(entry-12,0x825035,0x9160f8);
        const uint32_t offsets[]={0xc,0x14,0x1c,0x24};
        const uint32_t enters[]={0x825048,0x825075,0x8250a2,0x8250cf};
        const uint32_t frees[]={0x82504b,0x825078,0x8250a5,0x8250d2};
        const uint32_t leaves[]={0x825062,0x82508f,0x8250bc,0x8250eb};
        for(unsigned i=0;i<4;i++){
            fx_set_reg(FX_EDI,U32(r,offsets[i]));
            if(U8(0,0xda67d8))call1(entry-12,enters[i],fx_reg(FX_EBP),0xd61704,1);
            call1(entry-12,frees[i],fx_reg(FX_EBX),fx_reg(FX_EDI),0);
            /* Original pops EBP/EBX BEFORE the final conditional Leave. */
            if(i==3){fx_set_reg(FX_EBP,saved_bp);fx_set_reg(FX_EBX,saved_bx);}
            if(U8(0,0xda67d8))lock_at(i==3?entry-4:entry-12,leaves[i],0x9160f8);
        }
        fx_set_reg(FX_EDI,U32(r,0x10));
        if(fx_reg(FX_EDI)){
            fx_set_reg(FX_EAX,fx_reg(FX_EDI));call0(entry-4,0x8250f9,0x7fa1e0);
            call1(entry-4,0x8250ff,0x78fc50,fx_reg(FX_EDI),0);
        }
    }else{
        fx_set_reg(FX_EAX,U32(r,0x10));
        if(fx_reg(FX_EAX))call0(entry-4,0x825110,0x7fa1e0);
    }
    fx_set_reg(FX_EAX,U32(r,0x344));SU32(r,0x340,0x974c78);
    if(fx_reg(FX_EAX)&&U32(r,0x348)){
        const uint32_t file=fx_reg(FX_EAX);SU8(r,0x358,0);
        call1(entry-4,0x82513b,U32(0,0x9162c4),file,0);
    }
    SU32(r,0x340,0x94dab4);fx_set_reg(FX_EAX,U32(r,0x2fc)-1);SU32(r,0x2f8,0x975368);
    if(!fx_reg(FX_EAX)){
        /* BOUNDARY (not reached): compression mode 1 (deflate, a writer): the core's only ZCompression initialiser (rx_guest_zcompression_init, 00829d10) sets mode 2 (inflate), so mode 1 never exists in the product and the original's reference world executes 0 of these bytes. The inflate side (mode 2) is native (zlib_inflate.c, native_bridge.c). */
        fx_set_reg(FX_ECX,r+0x300);call1(entry-4,0x82517c,0x8ed680,fx_reg(FX_ECX),0);
    }else{
        fx_set_reg(FX_EAX,fx_reg(FX_EAX)-1);
        if(!fx_reg(FX_EAX)){
            fx_set_reg(FX_EAX,r+0x300);call1(entry-4,0x82516e,0x8ec710,fx_reg(FX_EAX),0);
        }
    }
    fx_set_reg(FX_EDI,r+0x2c8);SU32(r,0x2f8,0x975334);
    veh_call_sp(entry-4,0x825194,0,0,0,0);rx_guest_compressed_destroy_exact(r+0x2c8);rx_set_sp(entry-4);
    SU32(r,0,0x9751ec);fx_set_reg(FX_EDI,saved_di);rx_set_sp(entry);
}
