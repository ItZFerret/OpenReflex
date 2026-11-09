/* Complete Vvf29 semantic parent: UTV004c1620, Buggy004a38e0,
 * TrophyTruck0046b6d0. Original calls stay explicit through the finite
 * dependency boundary until its separately verified native owners land. */
#include "fourwheel_vehicle_update.h"
#include "fx_util.h"
#include "bike_math.h"
#include "world_veh_util.h"

typedef struct {
    rx_addr simulation, wheel_update;
    uint32_t wheel_delta;
    rx_addr returns[22];
} vehicle_layout;
static const vehicle_layout layouts[3]={
 {0x1540,0x4c7ed0,0,{0x4c164a,0x4c1667,0x4c167f,0x4c1691,0x4c1787,0x4c179b,0x4c1802,0x4c1827,0x4c1d43,0x4c1d4e,0x4c1d9c,0x4c1dd1,0x4c1ded,0x4c1dfa,0,0x4c1e6a,0x4c1ea9,0x4c1ee8,0x4c1f27,0x4c1f74,0x4c1f81,0x4c1f92}},
 {0x15c0,0x4ad6b0,0x10,{0x4a390a,0x4a3927,0x4a393f,0x4a3951,0x4a3a47,0x4a3a5b,0x4a3ac2,0x4a3ae7,0x4a4003,0x4a400e,0x4a405c,0x4a4091,0x4a40ad,0x4a40ba,0x4a40c8,0x4a4138,0x4a4177,0x4a41b6,0x4a41f5,0x4a4240,0x4a424d,0x4a425e}},
 {0x15d0,0x4744f0,0,{0x46b6fa,0x46b717,0x46b72f,0x46b741,0x46b837,0x46b84b,0x46b8b2,0x46b8d7,0x46bdf3,0x46bdfe,0x46be4c,0x46be81,0x46be9d,0x46beaa,0,0x46bf1a,0x46bf59,0x46bf98,0x46bfd7,0x46c03b,0x46c048,0x46c059}}
};
static rx_v4 invoke(unsigned family,unsigned site,rx_addr target,
                    unsigned count,const uint32_t *args,unsigned cleanup,rx_v4 x0){
    const rx_addr sp=fx_push((int)count,args,layouts[family].returns[site]);
    const rx_addr expected=rx_sp()+4+cleanup;
    rx_v4 out=rx_fourwheel_update_dependency(family,site,target,expected,x0);
    fx_pop(sp);return out;
}
static rx_v4 call0(unsigned family,unsigned site,rx_addr target,rx_v4 x0){
    return invoke(family,site,target,0,0,0,x0);
}
static rx_v4 call1(unsigned family,unsigned site,rx_addr target,uint32_t arg,unsigned cleanup,rx_v4 x0){
    return invoke(family,site,target,1,&arg,cleanup,x0);
}
static float length3(unsigned family,unsigned site,rx_v4 vector,int yfirst){
    float x=phys_mul(vector.v[0],vector.v[0]),y=phys_mul(vector.v[1],vector.v[1]);
    float value=phys_add(yfirst?phys_add(y,x):phys_add(x,y),phys_mul(vector.v[2],vector.v[2]));
    return call1(family,site,0x42c230,rx_fb(value),0,v4_zero()).v[0];
}
/* The original uses scalar SSE double products, rounding the four quaternion
 * terms to float before the outer double products. Do not fold zero products. */
static void model_offset(rx_v4 q,float out[3]){
    const double x=q.v[0],y=q.v[1],z=q.v[2],w=q.v[3];
    const double zero=0.0,positive=2.1670000553131104,negative=-2.1670000553131104;
    const float nx=phys_sub(-0.0f,q.v[0]),ny=phys_sub(-0.0f,q.v[1]),nz=phys_sub(-0.0f,q.v[2]);
    const double wx=w*zero,xx=x*zero,zz=z*zero,yy=y*zero;
    const float a=(float)(((wx-xx)-y*negative)-zz);
    const float b=(float)(((xx+wx)+yy)-z*negative);
    const float c=(float)(((yy-w*positive)+zz)-xx);
    const float d=(float)(((zz+wx)-x*positive)-yy);
    out[0]=(float)(((w*b+(double)a*nx)+(double)nz*c)-(double)ny*d);
    out[1]=(float)(((w*c+(double)ny*a)+(double)d*nx)-(double)nz*b);
    out[2]=(float)(((w*d+(double)nz*a)+(double)ny*b)-(double)c*nx);
}
static void update(rx_addr vehicle,float dt,unsigned family){
    const vehicle_layout *layout=&layouts[family];
    fx_frame frame;fx_frame_enter(&frame,0x94,16,FX_EBX,FX_ESI,FX_EDI);
    const rx_addr f=frame.E;SU32(f,0x9c,U32(0,0x9b45e4)^(frame.A-0x94u));
    fx_set_reg(FX_EBX,vehicle);fx_set_reg(FX_ESI,U32(vehicle,layout->simulation));
    call0(family,0,0x434d00,v4_zero());
    if(fx_reg(FX_EAX)){
        rx_addr sim=U32(vehicle,layout->simulation);rx_v4 value=rx_ld4(sim+0x170);
        rx_st4(f+0x60,value);fx_set_reg(FX_EAX,vehicle);call0(family,1,0x460070,value);
    }
    rx_addr sim=U32(vehicle,layout->simulation);fx_set_reg(FX_ECX,sim);
    if(!U32(sim,4)){
        fx_set_reg(FX_EDX,vehicle+0x1220);call1(family,2,0x433120,vehicle+0x1220,4,v4_zero());
    }
    sim=U32(vehicle,layout->simulation);fx_set_reg(FX_EAX,sim);fx_set_reg(FX_ESI,vehicle+0x12c0);
    call1(family,3,0x433550,sim,4,v4_zero());
    sim=U32(vehicle,layout->simulation);fx_set_reg(FX_EAX,sim);
    const rx_v4 rotation=rx_ld4(sim+0x7c0),center=v4_and(rx_ld4(sim+0x36e0),rx_ld4(0x92db80));
    const rx_v4 conjugate=v4_mul(rotation,rx_ld4(0x93a220));
    const rx_v4 position=v4_sub(rx_ld4(sim+0x180),bike_quat_mul(rotation,bike_quat_mul(center,conjugate)));
    rx_st4(f+0x60,position);
    rx_addr target=U32(U32(vehicle,0),0x90);fx_set_reg(FX_EDX,target);fx_set_reg(FX_EAX,f+0x70);fx_set_reg(FX_ECX,vehicle);
    call1(family,4,target,f+0x70,4,position);
    fx_set_reg(FX_EAX,U32(vehicle,0));target=U32(fx_reg(FX_EAX),0x94);fx_set_reg(FX_EDX,target);fx_set_reg(FX_ECX,vehicle);
    call1(family,5,target,f+0x80,4,v4_zero());
    rx_st4(vehicle+0xc0,rx_ld4(f+0x60));rx_st4(vehicle+0xe0,rx_ld4(f+0x70));rx_st4(vehicle+0x160,rx_ld4(f+0x80));
    SF32(vehicle,0x150,length3(family,6,rx_ld4(vehicle+0x160),0));
    sim=U32(vehicle,layout->simulation);fx_set_reg(FX_EAX,sim);
    if(!U32(sim,4)){
        fx_set_reg(FX_EDI,vehicle);call1(family,7,0x45d910,world_x87_pass(rx_fb(dt)),4,v4_zero());
    }
    const rx_v4 q=rx_ld4(vehicle+0xe0);float offset[3];model_offset(q,offset);
    const veh_rot rows=veh_quat_rows(q.v[0],q.v[1],q.v[2],q.v[3]);
    for(unsigned row=0;row<3;++row){
        for(unsigned col=0;col<3;++col)SF32(vehicle,0xf0+16*row+4*col,rows.m[row][col]);
        SF32(vehicle,0xfc+16*row,0.0f);
        SF32(vehicle,0x120+4*row,phys_add(F32(vehicle,0xc0+4*row),offset[row]));
    }
    SF32(vehicle,0x12c,1.0f);
    /* Original recomputes two rows with doubled operand on the left. */
    const float x2=phys_mul(q.v[0],2),y2=phys_mul(q.v[1],2),z2=phys_mul(q.v[2],2);
    const float wx=phys_mul(x2,q.v[3]),xx=phys_mul(x2,q.v[0]);
    const float yz=phys_mul(z2,q.v[1]),yy=phys_mul(y2,q.v[1]);
    SF32(vehicle,0x140,phys_sub(phys_mul(y2,q.v[0]),phys_mul(z2,q.v[3])));
    SF32(vehicle,0x144,phys_sub(1.0f,phys_add(phys_mul(z2,q.v[2]),xx)));
    SF32(vehicle,0x148,phys_add(yz,wx));
    SF32(vehicle,0x130,phys_add(phys_mul(z2,q.v[0]),phys_mul(y2,q.v[3])));
    SF32(vehicle,0x134,phys_sub(yz,wx));SF32(vehicle,0x138,phys_sub(1.0f,phys_add(yy,xx)));
    if(1.00100005f>F32(vehicle,0x150))for(unsigned i=0;i<3;++i)SU32(vehicle,0x160+4*i,world_x87_pass(U32(vehicle,0x130+4*i)));
    if(!U32(vehicle,0x280)){
        fx_set_reg(FX_EDI,U32(vehicle,0x1328)+0x10);
        call1(family,8,0x7de9e0,0x93bff4,4,v4_zero());
        const rx_addr node=fx_reg(FX_EAX);fx_set_reg(FX_ESI,node);fx_set_reg(FX_ECX,node);
        call1(family,9,0x7db890,0,4,v4_zero());
        fx_set_reg(FX_ECX,U32(node,0));const rx_addr matrix=U32(fx_reg(FX_ECX),0x1c)+((uint32_t)U8(node,0xb)<<6);fx_set_reg(FX_EAX,matrix);
        for(unsigned i=0;i<4;++i)rx_st4(vehicle+0x240+16*i,rx_ld4(matrix+16*i));
    }else{
        fx_set_reg(FX_ECX,U32(vehicle,0x1a0));
        if(fx_reg(FX_ECX)){fx_set_reg(FX_EAX,vehicle+0x240);call0(family,10,0x6243d0,v4_zero());}
    }
    const int32_t phase=I32(0,0xad3ed4),third=phase/3;
    fx_set_reg(FX_ECX,(uint32_t)phase);fx_set_reg(FX_EDX,(uint32_t)third*3u);fx_set_reg(FX_EAX,(uint32_t)phase-(uint32_t)third*3u);
    if(phase%3==0||phase%3==2){
        fx_set_reg(FX_EAX,U32(vehicle,0));target=U32(fx_reg(FX_EAX),0xf0);fx_set_reg(FX_EDX,target);fx_set_reg(FX_ECX,vehicle);
        call1(family,11,target,world_x87_pass(rx_fb(dt)),4,v4_zero());
        if(U32(vehicle,0x1a0)){
            fx_set_reg(FX_EAX,U32(vehicle,0));target=U32(fx_reg(FX_EAX),0x8c);fx_set_reg(FX_EDX,target);fx_set_reg(FX_ECX,vehicle);
            call1(family,12,target,world_x87_pass(rx_fb(dt)),4,v4_zero());
        }
    }
    const uint32_t args[]={vehicle,world_x87_pass(rx_fb(dt))};invoke(family,13,layout->wheel_update,2,args,8,v4_zero());
    if(family==1){fx_set_reg(FX_ESI,vehicle);call1(family,14,0x4ad580,args[1],4,v4_zero());}
    sim=U32(vehicle,layout->simulation);fx_set_reg(FX_EAX,sim);
    const uint32_t wheel_offsets[]={0x8150,0x83f0,0x82a0,0x8540};
    const uint32_t local_offsets[]={0x40,0x20,0x50,0x60};
    for(unsigned i=0;i<4;++i)rx_st4(f+local_offsets[i],rx_ld4(sim+wheel_offsets[i]-layout->wheel_delta));
    float lengths[4];for(unsigned i=0;i<4;++i)lengths[i]=length3(family,15+i,rx_ld4(f+local_offsets[i]),1);
    float impact=lengths[3]<lengths[1]?lengths[1]:lengths[3];
    float first=lengths[2]<lengths[0]?lengths[0]:lengths[2];
    if(impact<first)impact=first;if(impact<0)impact=0;
    float divisor=family==0?F32(0,0x9cf5ac):family==1?8000.0f:U32(vehicle,0x170)==8?10000.0f:13000.0f;
    impact=phys_div(impact,divisor);SF32(f,0x18,impact);fx_set_reg(FX_ESI,vehicle);
    call0(family,19,0x4617b0,v4(impact,0,0,0));
    call1(family,20,0x4618e0,world_x87_pass(U32(f,0x18)),4,v4_zero());
    fx_set_reg(FX_ECX,U32(f,0x9c)^(frame.A-0x94));
    fx_set_reg(FX_EDI,frame.in.edi);fx_set_reg(FX_ESI,frame.in.esi);fx_set_reg(FX_EBX,frame.in.ebx);rx_set_sp(frame.A-0x94);
    call0(family,21,0x8f9efe,v4_zero());fx_frame_leave(&frame);
}
void rx_fourwheel_vehicle_update_utv(rx_addr v,float dt){update(v,dt,0);}
void rx_fourwheel_vehicle_update_buggy(rx_addr v,float dt){update(v,dt,1);}
void rx_fourwheel_vehicle_update_truck(rx_addr v,float dt){update(v,dt,2);}
RX_FUNCTION(0x004c1620,rx_fourwheel_vehicle_update_utv)
RX_FUNCTION(0x004a38e0,rx_fourwheel_vehicle_update_buggy)
RX_FUNCTION(0x0046b6d0,rx_fourwheel_vehicle_update_truck)
