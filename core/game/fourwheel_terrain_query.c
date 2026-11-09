/* Handwritten complete 527B terrain-query helpers. Three proven layouts;
 * no registration or native fallback until the private chain is validated. */
#include "fourwheel_terrain_query.h"
#include "phys_vec.h"
#include "fx_util.h"
#include "loop_jobs.h"
typedef struct{rx_addr entry,triangles;unsigned count;} QueryLayout;
static rx_v4 product(rx_v4 a,rx_v4 b){
 rx_v4 shuffle=v4_shuf(b,b,0x1b);
 rx_v4 x=v4_mul(v4_mul(v4_lane(a,0),shuffle),rx_ld4(0x93a1f0));
 shuffle=v4_shuf(shuffle,shuffle,0xb1);
 rx_v4 y=v4_mul(v4_mul(v4_lane(a,1),shuffle),rx_ld4(0x93a200));
 shuffle=v4_shuf(shuffle,shuffle,0x1b);
 rx_v4 z=v4_mul(v4_mul(shuffle,v4_lane(a,2)),rx_ld4(0x93a210));
 rx_v4 w=v4_mul(v4_lane(a,3),b);
 return v4_add(v4_add(z,y),v4_add(w,x));
}
static void dependency(rx_addr target,rx_addr entry,rx_addr expected,rx_addr ra){
 SU32(entry,0,ra);rx_set_sp(entry);rx_fourwheel_query_dependency(target,expected);
}
static void query(rx_addr wheel,rx_addr s,const QueryLayout*l){
 fx_frame frame;fx_frame_enter(&frame,0x68,16,FX_EBX,FX_EDI);rx_addr b=frame.E;
 SU32(b,0x6c,U32(0,0x9b45e4)^(b+8));
 fx_set_reg(FX_EAX,s);fx_set_reg(FX_EDI,wheel+0x10);
 uint32_t stride=160u*U32(wheel,0);fx_set_reg(FX_ECX,s+stride);
 rx_st4(b+0x50,rx_ld4(s+0x730));
 rx_st4(b+0x10,v4_splat(phys_mul(F32(wheel,0x10),F32(0,0x9779e4))));
 rx_v4 q=rx_ld4(s+stride+0x730),offset=v4_and(rx_ld4(wheel+0x20),rx_ld4(0x92db80));
 rx_v4 rotated=product(q,product(offset,v4_mul(q,rx_ld4(0x93a220))));
 rx_st4(b+0x30,q);rx_v4 position=v4_add(rotated,rx_ld4(s+stride+0xf0));
 rx_st4(b+0x40,position);rx_st4(b+0x20,v4_add(v4_splat(F32(0,0x92e12c)),position));
 fx_set_reg(FX_ECX,1);
 if(!(U8(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}
 /* Four arguments already pushed for TerrainTriangles while the intervening
  * current-world lookup executes at that exact pending-argument depth. */
 SU32(b-4,0,b+0x10);SU32(b-8,0,200);fx_set_reg(FX_EBX,s+l->count);
 const rx_addr tri=rx_job_scratch(l->triangles,200*48);   /* the class's one triangle buffer: a parallel simulation job's own copy */
 SU32(b-12,0,fx_reg(FX_EBX));SU32(b-16,0,tri);fx_set_reg(FX_EDX,0x9e30d4);
 dependency(0x402dc0,b-20,b-16,l->entry+0x190);
 SU32(b-20,0,fx_reg(FX_EAX));fx_set_reg(FX_ECX,b+0x20);
 dependency(0x7c6160,b-24,b,l->entry+0x19a);
 fx_set_reg(FX_EAX,U32(fx_reg(FX_EBX),0));
 rx_job_scratch_used(l->triangles,fx_reg(FX_EAX)*48u);
 if(!fx_reg(FX_EAX)){
  SU32(wheel,0xfc,fx_reg(FX_EAX));fx_set_reg(FX_EDI,frame.in.edi);fx_set_reg(FX_EBX,frame.in.ebx);
  fx_set_reg(FX_ECX,U32(b,0x6c)^(b+8));
  dependency(0x8f9efe,b+4,b+8,l->entry+0x1b3);fx_frame_leave(&frame);return;
 }
 SU32(b-4,0,fx_reg(FX_EAX));SU32(b-8,0,tri);SU32(b-12,0,fx_reg(FX_EDI));
 fx_set_reg(FX_EAX,b+0x40);SU32(b-16,0,fx_reg(FX_EAX));
 fx_set_reg(FX_ECX,b+0x30);SU32(b-20,0,fx_reg(FX_ECX));
 fx_set_reg(FX_EDI,b+0x10);fx_set_reg(FX_ECX,b+0x20);
 dependency(0x438260,b-24,b,l->entry+0x1d7);SU32(wheel,0xfc,fx_reg(FX_EAX));
 if(fx_reg(FX_EAX)){
  fx_set_reg(FX_EDX,b+0x50);SU32(b-4,0,fx_reg(FX_EDX));
  fx_set_reg(FX_EAX,b+0x30);SU32(b-8,0,fx_reg(FX_EAX));
  fx_set_reg(FX_ECX,b+0x20);SU32(b-12,0,fx_reg(FX_ECX));SU32(b-16,0,fx_reg(FX_ESI));
  fx_set_reg(FX_ECX,b+0x40);fx_set_reg(FX_EDX,fx_reg(FX_EDI));
  dependency(0x42db20,b-20,b,l->entry+0x1fc);
 }
 fx_set_reg(FX_ECX,U32(b,0x6c));fx_set_reg(FX_EDI,frame.in.edi);fx_set_reg(FX_EBX,frame.in.ebx);
 fx_set_reg(FX_ECX,fx_reg(FX_ECX)^(b+8));
 dependency(0x8f9efe,b+4,b+8,l->entry+0x209);fx_frame_leave(&frame);
}
void rx_utv_wheel_terrain_query_exact(rx_addr w,rx_addr s){static const QueryLayout l={0x4d3140,0xad3fb0,0x8070};query(w,s,&l);}
void rx_buggy_wheel_terrain_query_exact(rx_addr w,rx_addr s){static const QueryLayout l={0x4b8990,0xad7550,0x8060};query(w,s,&l);}
void rx_truck_wheel_terrain_query_exact(rx_addr w,rx_addr s){static const QueryLayout l={0x454ff0,0xad3230,0x8070};query(w,s,&l);}
