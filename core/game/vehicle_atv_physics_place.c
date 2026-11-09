/*0048f9e0 ATV four-wheel terrain placement: source-matched stack/rows kernels. */
#include "vehicle_atv_physics.h"
#include "bike_math.h"
#include "loop_util.h"
#include "loop_vehicle.h"
#include "world_veh.h"
#include "phys_terrain.h"
#include "world_wreck.h"
/* x87 `fld m32; fstp m32`: a NaN keeps its payload, an SNaN comes out quiet (loop_cam.h cam_x87_bits) */
static uint32_t as20_x87(uint32_t b) {
    return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) ? (b | 0x00400000u) : b;
}
static float as20_sqrt_x87(rx_addr slot) { return phys_sqrt(rx_bf(as20_x87(U32(slot, 0)))); }   /* SqrtF of a stack slot (fld / fstp) */

/* 00402dc0 (EDX = the function-local static 0x009e30d4, first use initializes it): the current terrain */
static rx_addr as20_terrain(rx_addr ret) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    const rx_addr s = loop_push(0, 0, ret); const rx_addr r = rx_world_find_by_controller(0x009e30d4); loop_pop(s);
    return r;
}
/* 007c5aa0 TerrainHeightAt (stdcall terrain, point in / out; RET 8) */
static uint32_t as20_height_at(rx_addr terrain, rx_addr point, rx_addr ret) {
    const uint32_t a[2] = {terrain, point};
    uint32_t edx; const rx_addr s = loop_push(2, a, ret); const uint32_t r = phys_terrain_height_at(terrain, point, &edx); loop_pop(s);
    return r;
}
/* 00401630 Vec3_Cross (ECX = c, EAX = a; cdecl out) */
static void as20_cross(rx_addr out, rx_addr c, rx_addr a, rx_addr ret) {
    const uint32_t x[1] = {out};
    const rx_addr s = loop_push(1, x, ret); rx_world_vec3_cross(out, c, a); loop_pop(s);
}

/* The orientation quaternion of the rows X (m00 = x0, X.y / X.z in memory at xy / xz), Y, Z (the larger of the four diagonal
 * combinations, as the code inlines it; the SqrtF arguments are the stack slots the candidates were stored in): out x y z w */
typedef struct { rx_addr x1, x2, y0, y2, z0, z1; } as20_rows;   /* the addresses of the off-diagonal entries read */
static void as20_rows_to_quat(float x0, float m11, float m22, as20_rows r, rx_addr S, uint32_t sw, uint32_t sx, uint32_t sy, uint32_t sz,
                            rx_addr q) {
    const float t0 = phys_add(x0, 1.0f), t1 = phys_sub(1.0f, x0), k = 0.25f /* 0x92ff84 */;
    const float qx = phys_mul(phys_sub(phys_sub(t0, m11), m22), k);
    const float qy = phys_mul(phys_sub(phys_add(t1, m11), m22), k);
    const float qw = phys_mul(phys_add(phys_add(t0, m11), m22), k);
    const float qz = phys_mul(phys_add(phys_sub(t1, m11), m22), k);
    SF32(S, sw, qw); SF32(S, sx, qx); SF32(S, sy, qy); SF32(S, sz, qz);
    const float X1 = F32(r.x1, 0), X2 = F32(r.x2, 0), Y0 = F32(r.y0, 0), Y2 = F32(r.y2, 0), Z0 = F32(r.z0, 0), Z1 = F32(r.z1, 0);
    float m = qx > qy ? qx : qy;
    if (!(m > qz)) m = qz;
    if (!(m > qw)) m = qw;
    if (m == qw) {
        const float s = as20_sqrt_x87(S + sw), d = phys_div(k, s);
        SF32(q, 0xc, s);
        SF32(q, 0, phys_mul(phys_sub(Y2, Z1), d));
        SF32(q, 4, phys_mul(phys_sub(Z0, X2), d));
        SF32(q, 8, phys_mul(phys_sub(X1, Y0), d));
        return;
    }
    float m2 = qy > qz ? qy : qz;
    if (!(m2 > qx)) m2 = qx;
    if (m2 == qx) {
        const float s = as20_sqrt_x87(S + sx), d = phys_div(k, s);
        SF32(q, 0, s);
        SF32(q, 0xc, phys_mul(phys_sub(Y2, Z1), d));
        SF32(q, 4, phys_mul(phys_add(X1, Y0), d));
        SF32(q, 8, phys_mul(phys_add(Z0, X2), d));
    } else if (qy > qz) {
        const float s = as20_sqrt_x87(S + sy), d = phys_div(k, s);
        SF32(q, 4, s);
        SF32(q, 0xc, phys_mul(phys_sub(Z0, X2), d));
        SF32(q, 0, phys_mul(phys_add(X1, Y0), d));
        SF32(q, 8, phys_mul(phys_add(Z1, Y2), d));
    } else {
        const float s = as20_sqrt_x87(S + sz), d = phys_div(k, s);
        SF32(q, 8, s);
        SF32(q, 0xc, phys_mul(phys_sub(X1, Y0), d));
        SF32(q, 0, phys_mul(phys_add(Z0, X2), d));
        SF32(q, 4, phys_mul(phys_add(Z1, Y2), d));
    }
}

static int as20_probe(rx_addr sim,rx_addr S,rx_addr terrain,rx_v4 down,rx_addr ray_ret,rx_addr h_ret,int second){
 const rx_v4 axis=rx_ld4(0x00d616a0);
 for(unsigned i=0;i<4;i++){
  rx_v4 radius=rx_ld4(sim+0x9150+0x150*i);
  rx_v4 off=i==1?v4_mul(axis,radius):v4_mul(radius,axis);
  rx_v4 start=v4_sub(rx_ld4(sim+0x230+0xa0*i),off);
  rx_st4(S+0xd0+32*i,start);rx_st4(S+0xe0+32*i,v4_sub(start,down));
 }
 const rx_addr ctx=U32(0,0x00a95844);rx_addr level=0;
 if(U32(U32(ctx,0x20),0x34)==1)level=U32(U32(0,0x00a946b8),0x5a54);
 else if(U32(U32(ctx,0x1c),0x34)==1)level=U32(U32(0,0x00a946b8),0x20c8);
 if(second){rx_addr call=loop_push(0,0,0x0049077c);level=rx_world_active_sound_table(U32(0,0x00a946b8));loop_pop(call);}
 const rx_addr world=U32(level,0x20134);
 const uint32_t args[]={world,S+0xd0,4,S+0x1c0,0};
 rx_addr sp=loop_push(5,args,ray_ret);rx_physics_world_raycast_bodies(world,S+0xd0,4,S+0x1c0,0);loop_pop(sp);
 for(unsigned i=0;i<4;i++){
  rx_addr point=S+0x150+16*i,hit=S+0x1c0+48*i;
  rx_st4(point,rx_ld4(S+0xd0+32*i));
  uint32_t found=as20_height_at(terrain,point,h_ret);
  if(found){SU32(S,0x80+4*i,1);if(U8(hit,0)&&F32(point,4)<F32(hit,0x14))rx_st4(point,rx_ld4(hit+0x10));}
  else if(U8(hit,0)){SU32(S,0x80+4*i,1);rx_st4(point,rx_ld4(hit+0x10));}
  else SU32(S,0x80+4*i,0);
 }
 return U32(S,0x80)&&U32(S,0x84)&&U32(S,0x88)&&U32(S,0x8c);
}
void rx_atv_ground_place(rx_addr sim,rx_addr pos,rx_addr dir,rx_addr out_pos,rx_addr out_fwd,rx_addr out_up){
 const rx_addr entry=rx_sp(),A=(entry-4)&~15u,B=A-0x284,S=B-12;rx_set_sp(S);
 SU32(B,0x280,U32(0,0x009b45e4)^B);
 SU32(S,0x44,out_pos);SU32(S,0x48,out_fwd);SU32(S,0x4c,out_up);
 const rx_addr terrain=as20_terrain(0x0048fa3b);SU32(S,0x40,terrain);
 rx_v4 p=rx_ld4(pos);rx_st4(S+0xc0,p);rx_st4(S+0x150,p);
 as20_height_at(terrain,S+0x150,0x0048fa60);
 if(p.v[1]<F32(S,0x154))p=rx_ld4(S+0x150);
 for(unsigned i=0;i<3;i++)SU32(out_fwd,4*i,as20_x87(U32(dir,4*i)));
 rx_st4(S+0xc0,v4_add(v4(0,3.5f,0,0),p));rx_st4(S+0x70,v4_zero());
 float x=F32(out_fwd,0),z=F32(out_fwd,8);
 float length=phys_add(phys_add(phys_mul(x,x),phys_mul(0,0)),phys_mul(z,z));
 SF32(out_fwd,4,0);SF32(S,0x10,length);
 if(length>0){float inv=phys_div(1,as20_sqrt_x87(S+0x10));SF32(out_fwd,0,phys_mul(x,inv));SF32(out_fwd,4,phys_mul(inv,0));SF32(out_fwd,8,phys_mul(z,inv));}
 as20_cross(S+0x50,0x009cb4b0,out_fwd,0x0048fb7b);
 float cx=F32(S,0x50),cy=F32(S,0x54),cz=F32(S,0x58);
 length=phys_add(phys_add(phys_mul(cy,cy),phys_mul(cx,cx)),phys_mul(cz,cz));SF32(S,0x38,length);
 if(length>0){float inv=phys_div(1,as20_sqrt_x87(S+0x38));cx=phys_mul(inv,cx);SF32(S,0x54,phys_mul(inv,cy));SF32(S,0x58,phys_mul(inv,cz));}
 as20_rows r={S+0x54,S+0x58,0x009cb4b0,0x009cb4b8,out_fwd,out_fwd+4};
 as20_rows_to_quat(cx,F32(0,0x009cb4b4),F32(out_fwd,8),r,S,0x38,0x10,0x20,0x3c,S+0x60);
 rx_st4(S+0xa0,bike_quat_mul(rx_ld4(S+0x60),bike_quat_conj(rx_ld4(0x00ad66b0))));
 loop_tc2(loop_vslot(sim,8),sim,S+0xc0,S+0xa0);
 const rx_v4 down=v4(0,20,0,0);rx_st4(S+0xb0,down);
 if(as20_probe(sim,S,terrain,down,0x00490014,0x00490055,0)){
  rx_v4 aa=v4_sub(rx_ld4(S+0x150),rx_ld4(S+0x180));
  rx_v4 bb=v4_sub(rx_ld4(S+0x160),rx_ld4(S+0x170));
  rx_v4 cross=v4_xyz(v4_sub(v4_mul(v4_perm(bb,2,0,1,3),v4_perm(aa,1,2,0,3)),v4_mul(v4_perm(bb,1,2,0,3),v4_perm(aa,2,0,1,3))));
  rx_v4 normal=bike_normalize(cross),forward=bike_rotate(rx_ld4(S+0xa0),rx_ld4(0x00d61680));
  rx_v4 projected=bike_normalize(v4_sub(forward,v4_mul(v4_splat(bike_dot3(forward,normal)),normal)));
  rx_st4(S+0x70,normal);rx_st4(S+0x90,projected);
  as20_cross(S+0x60,S+0x70,S+0x90,0x00490316);
  cx=F32(S,0x60);cy=F32(S,0x64);cz=F32(S,0x68);
  length=phys_add(phys_add(phys_mul(cx,cx),phys_mul(cy,cy)),phys_mul(cz,cz));SF32(S,0x34,length);
  if(length>0){float inv=phys_div(1,as20_sqrt_x87(S+0x34));cx=phys_mul(cx,inv);SF32(S,0x64,phys_mul(cy,inv));SF32(S,0x68,phys_mul(cz,inv));}
  as20_rows rr={S+0x64,S+0x68,S+0x70,S+0x78,S+0x90,S+0x94};
  as20_rows_to_quat(cx,F32(S,0x74),F32(S,0x98),rr,S,0x3c,0x38,0x10,0x20,S+0x50);
  rx_st4(S+0xa0,bike_quat_mul(rx_ld4(S+0x50),bike_quat_conj(rx_ld4(0x00ad66b0))));
  loop_tc2(loop_vslot(sim,8),sim,S+0xc0,S+0xa0);
  if(as20_probe(sim,S,terrain,rx_ld4(S+0xb0),0x00490788,0x004907c5,1)){
   float distance=F32(0,0x009cc37c);
   for(unsigned i=0;i<4;i++){
    rx_v4 delta=v4_sub(rx_ld4(S+0x150+16*i),rx_ld4(S+0xd0+32*i));
    float d=phys_sqrt(bike_hsum_021(v4_mul(delta,delta)));if(d<distance)distance=d;
   }
   rx_v4 final=v4_sub(rx_ld4(S+0xc0),v4_mul(v4_splat(distance),rx_ld4(0x00d616a0)));
   rx_st4(S+0xc0,final);rx_st4(out_pos,final);
   rx_v4 q=rx_ld4(S+0xa0);rx_st4(out_fwd,bike_rotate(q,rx_ld4(0x00d61680)));rx_st4(out_up,bike_rotate(q,rx_ld4(0x00d616a0)));
  }
 }
 rx_set_sp(entry);
}
RX_FUNCTION(0x0048f9e0,rx_atv_ground_place)
