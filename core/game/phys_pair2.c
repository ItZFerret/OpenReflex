/* phys_pair2.c -- more shape-pair contact generators (phys_contact.c dispatches; contact set M as there: count +0, 0x30-byte
 * records (point +0x10, normal +0x20, depth +0x30) from +0x10, at most 30, touching flag +0x6a0). */
#include "phys_pair2.h"
#include "collision_live.h"

/* all of lanes 0..2 set in a compare mask (the original sums the three mask words to -3) */
static int all3(rx_v4 m) { return v4_lb(m, 1) + v4_lb(m, 2) + v4_lb(m, 0) == 0xfffffffdu; }
static rx_v4 v4_xyz_mask(rx_v4 v) { return v4_and(v, v4_ld(0x0092db80)); }
/* q (xyz(v)) q^-1 and q^-1 (xyz(v)) q as the exe's two products */
static rx_v4 rotate(rx_v4 q, rx_v4 v) { return phys_quat_mul(q, phys_quat_mul(v4_xyz(v), phys_quat_conj(q))); }
static rx_v4 unrotate(rx_v4 q, rx_v4 v) { return phys_quat_mul(phys_quat_conj(q), phys_quat_mul(v4_xyz(v), q)); }
static void contact_append(rx_addr M, rx_v4 point, rx_v4 normal, rx_v4 depth) {
    int32_t k = I32(M, 0);
    if (k >= 0x1d) k = 0x1d;
    const rx_addr rec = M + 0x30u * (uint32_t)k;
    v4_st(rec + 0x10, point); v4_st(rec + 0x20, normal); v4_st(rec + 0x30, depth);
    SU32(M, 0, (uint32_t)k + 1);
    SU32(M, 0x6a0, 1);
}

/* the two crossings of the circle y^2 + z^2 = r^2 (x = 0) with the plane n . p = d, solved for the coordinate with the larger
 * normal component: returns 0 when the discriminant is not positive on x, y, z (per lane of the radius vec4) */
static int circle_plane_crossings(rx_v4 n, rx_v4 d, rx_v4 r2, rx_v4 out_y[2], rx_v4 out_z[2]) {
    const rx_v4 one = v4_ld(0x00d61650), two = v4_splat(2.0f), four = v4_splat(4.0f), m2 = v4_sub(v4_zero(), two);
    const rx_v4 nz = v4_splat(n.v[2]), ny = v4_splat(n.v[1]);
    const rx_v4 nz2 = v4_mul(nz, nz), ny2 = v4_mul(ny, ny);
    const int along_z = all3(v4_cmplt(nz2, ny2));          /* solve for z when |n.z| < |n.y| */
    const rx_v4 c_main = along_z ? nz : ny, c_other = along_z ? ny : nz;
    const rx_v4 c_main2 = along_z ? nz2 : ny2, c_other2 = along_z ? ny2 : nz2;
    const rx_v4 k = v4_add(v4_div(c_main2, c_other2), one);
    const rx_v4 b = v4_div(v4_mul(v4_mul(m2, c_main), d), c_other2);
    const rx_v4 q = v4_mul(v4_sub(v4_div(v4_mul(d, d), c_other2), r2), v4_mul(four, k));
    const rx_v4 disc = v4_sub(v4_mul(b, b), q);
    if (!all3(v4_cmplt(v4_ld(0x00d61670), disc))) return 0;
    const rx_v4 nb = v4_sub(v4_zero(), b), two_k = v4_mul(two, k), s = v4_sqrt(disc);
    const rx_v4 m1 = v4_div(v4_add(s, nb), two_k), m2r = v4_div(v4_sub(nb, s), two_k);
    const rx_v4 o1 = v4_div(v4_sub(d, v4_mul(m1, c_main)), c_other), o2 = v4_div(v4_sub(d, v4_mul(m2r, c_main)), c_other);
    if (along_z) { out_z[0] = m1; out_y[0] = o1; out_z[1] = m2r; out_y[1] = o2; }
    else { out_y[0] = m1; out_z[0] = o1; out_y[1] = m2r; out_z[1] = o2; }
    return 1;
}

/* ---- 008716b0 Triangles_CircleContact (ECX = circle shape rotation*, EDX = triangle mesh T (+0x20 count, +0x24 triangles);
 * stack: M, mesh rotation*, mesh position*, circle shape C (+0x20 radius vec4), circle position*, swap flag; RET 0x18): the
 * circle (radius r in its local y-z plane) against every triangle: the triangle in the circle's frame, the two points where the
 * circle crosses the triangle's plane, kept when inside all three edges (a ring of the last 8 points and normals). One contact:
 * the mean point (world space), the mean normal normalized (world space, negated with the flag), depth zero (0x00d61670).
 * Frame: push ebp; and esp, -16; sub esp, 0x3d4; push ebx / esi / edi (rings +0x2d0 / +0x350). Returns the EDX left. */
uint32_t phys_pair_triangles_circle(rx_addr QC, rx_addr T, rx_addr M, rx_addr QT, rx_addr PT, rx_addr C, rx_addr PC,
                                    uint32_t flag) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x3d4 - 12;
    rx_set_sp(L);
    const rx_addr ring_p = L + 0x2d0, ring_n = L + 0x350;
    const rx_v4 qc = v4_ld(QC), qt = v4_ld(QT), pt = v4_ld(PT), pc = v4_ld(PC), zero = v4_ld(0x00d61670);
    const rx_v4 radius = v4_ld(C + 0x20), r2 = v4_mul(radius, radius);
    uint32_t edx = T;
    int32_t hits = 0, slot = 0;
    if (I32(T, 0x20) <= 0) goto done;
    for (int32_t i = 0; i < I32(T, 0x20); i++) {
        const rx_addr tri = U32(T, 0x24) + 0x30u * (uint32_t)i;
        rx_v4 l[3];
        for (int j = 0; j < 3; j++) {
            const rx_v4 w = v4_sub(v4_add(rotate(qt, v4_ld(tri + 0x10u * (uint32_t)j)), pt), pc);
            l[j] = unrotate(qc, v4_xyz_mask(w));
        }
        const rx_v4 ea = v4_sub(l[1], l[0]), eb = v4_sub(l[2], l[0]), ec = v4_sub(l[2], l[1]);
        const rx_v4 n = phys_normalize3(phys_cross_ba(eb, ea));
        const rx_v4 d = v4_splat(phys_hsum3(v4_mul(l[0], n)));
        rx_v4 ys[2], zs[2];
        if (!circle_plane_crossings(n, d, r2, ys, zs)) continue;
        for (int c = 0; c < 2; c++) {
            const rx_v4 p = v4(zero.v[0], ys[c].v[0], zs[c].v[0], zs[c].v[0]);
            const float c0 = phys_hsum3(v4_mul(phys_cross_ba(v4_sub(p, l[0]), ea), n));
            const float c1 = phys_hsum3(v4_mul(phys_cross_ba(v4_sub(p, l[1]), ec), n));
            const float c2 = phys_hsum3(v4_mul(phys_cross_ba(v4_sub(p, l[2]), v4_sub(v4_zero(), eb)), n));
            if (!all3(v4_cmple(zero, v4(c0, c1, c2, c2)))) continue;
            v4_st(ring_p + 0x10u * (uint32_t)slot, p);
            v4_st(ring_n + 0x10u * (uint32_t)slot, n);
            slot = (slot + 1) & 7;
            hits++;
        }
    }
    edx = U32(T, 0x24) + 0x30u * (uint32_t)I32(T, 0x20);
    if (hits > 0) {
        const int32_t n = hits < 8 ? hits : 8;
        rx_v4 psum = zero, nsum = zero;
        for (int32_t k = 0; k < n; k++) {
            psum = v4_add(v4_ld(ring_p + 0x10u * (uint32_t)k), psum);
            nsum = v4_add(v4_ld(ring_n + 0x10u * (uint32_t)k), nsum);
        }
        const rx_v4 mean = v4_xyz(v4_div(psum, v4_splat((float)n)));
        const rx_v4 point = v4_add(phys_quat_mul(qc, phys_quat_mul(mean, phys_quat_conj(qc))), pc);
        rx_v4 normal = rotate(qc, v4_xyz_mask(phys_normalize3(nsum)));
        if ((flag & 0xff) != 0) normal = v4_sub(v4_zero(), normal);
        contact_append(M, point, normal, v4_ld(0x00d61670));
        edx = M;
    }
done:
    rx_set_sp(sp0);
    return edx;
}
RX_FUNCTION(0x008716b0, phys_pair_triangles_circle)

/* the exe's cross product b x a without the w mask (lane 3 = a.w b.w - a.w b.w) */
static rx_v4 cross_raw(rx_v4 a, rx_v4 b) {
    return v4_sub(v4_mul(v4_perm(a, 2, 0, 1, 3), v4_perm(b, 1, 2, 0, 3)), v4_mul(v4_perm(a, 1, 2, 0, 3), v4_perm(b, 2, 0, 1, 3)));
}
static int all3_le(rx_v4 a, rx_v4 b) { return all3(v4_cmple(a, b)); }
static int all3_lt(rx_v4 a, rx_v4 b) { return all3(v4_cmplt(a, b)); }

/* 0085ee70: one six-ray semantic kernel, with an optional exact private frame.
 * No nested gameplay operation exists. Private stores can be published at their
 * semantic stages because the observed domain excludes source/output/scratch
 * overlap; no call can observe intermediate GPRs before the final cookie. */
#include "rays6_observed.h"
#include "fx_util.h"
#include "phys_crt.h"
#include <stdlib.h>
typedef struct rays6_frame {fx_frame saved;rx_addr B;int active;} rays6_frame;
static void r6v(rays6_frame*f,uint32_t o,rx_v4 v){if(f->active)v4_st(f->B+o,v);}
static void r6u(rays6_frame*f,uint32_t o,uint32_t v){if(f->active)SU32(f->B,o,v);}
static rx_v4 r6neq(rx_v4 a,rx_v4 b){return v4_mask(a.v[0]!=b.v[0],a.v[1]!=b.v[1],a.v[2]!=b.v[2],a.v[3]!=b.v[3]);}
static rx_v4 r6c9(rx_v4 v){return v4_perm(v,1,2,0,3);}
static rx_v4 r6d2(rx_v4 v){return v4_perm(v,2,0,1,3);}
static rx_v4 r6sum012(rx_v4 p){return v4_splat(phys_add(phys_add(p.v[0],p.v[1]),p.v[2]));}
static rx_v4 r6sum021(rx_v4 p){return v4_splat(phys_add(phys_add(p.v[0],p.v[2]),p.v[1]));}
static uint32_t r6mask(rays6_frame*f,rx_v4 mask){r6v(f,0x290,mask);return (uint32_t)all3(mask);}
static int r6overlap(rx_addr a,uint32_t n,rx_addr b,uint32_t m){return (uint64_t)a+n>b&&(uint64_t)b+m>a;}
static void r6enter(rays6_frame*f,int exact,rx_addr ends,rx_addr a,rx_addr b,rx_addr c,rx_addr starts,rx_addr hits,rx_addr points,rx_addr normals){
 f->active=exact;if(!exact)return;rx_addr e=rx_sp(),B=((e-4)&~15u)-0x5c0;
 rx_addr in[]={ends,a,b,c,starts},out[]={hits,points,normals};uint32_t size[]={24,96,96};
 if(e<0x5d0||fx_reg(FX_ECX)!=ends||fx_reg(FX_EDX)!=a||U32(e,4)!=b||U32(e,8)!=c||U32(e,12)!=starts||U32(e,16)!=hits||U32(e,20)!=points||U32(e,24)!=normals)abort();
 for(unsigned i=0;i<5;i++)if((in[i]&15u)||(uint64_t)in[i]+96>0x100000000ull||r6overlap(in[i],96,B,e+28-B))abort();
 for(unsigned i=0;i<3;i++){
  if((out[i]&(i?15u:3u))||(uint64_t)out[i]+size[i]>0x100000000ull||r6overlap(out[i],size[i],B,e+28-B))abort();
  for(unsigned j=0;j<5;j++)if(r6overlap(out[i],size[i],in[j],96))abort();
  for(unsigned j=0;j<i;j++)if(r6overlap(out[i],size[i],out[j],size[j]))abort();
  /* Constants are writable guest storage; output may not replace them mid-kernel. */
  const rx_addr constants[]={0x9777c4,0x977494,0x92dc20,0x92dc30,0x92de10,0xd61650,0xd61670,0x9b45e4};
  for(unsigned j=0;j<8;j++)if(r6overlap(out[i],size[i],constants[j],16))abort();
 }
 fx_frame_enter(&f->saved,0x5b4,16,FX_EBX,FX_ESI,FX_EDI);f->B=f->saved.E;
 if(f->B!=B)abort();r6u(f,0x5bc,U32(0,0x9b45e4)^(B+12));
}
static uint32_t r6leave(rays6_frame*f,uint32_t answer,uint32_t edx,int late){
 if(!f->active)return answer;rx_addr B=f->B;if(rx_sp()!=B)abort();
 fx_set_reg(FX_EAX,answer);fx_set_reg(FX_EDX,edx);
 fx_set_reg(FX_ECX,U32(B,0x5bc)^(B+12));
 fx_set_reg(FX_EDI,f->saved.in.edi);fx_set_reg(FX_ESI,f->saved.in.esi);fx_set_reg(FX_EBX,f->saved.in.ebx);
 rx_set_sp(B+12);SU32(B,8,late?0x860e83:0x85f9c2);rx_set_sp(B+8);
 phys_security_check_cookie(fx_reg(FX_ECX));rx_set_sp(B+12);fx_frame_leave(&f->saved);return answer;
}
static uint32_t rays6_kernel(rx_addr ends,rx_addr a,rx_addr b,rx_addr c,rx_addr starts,rx_addr hits,rx_addr points,rx_addr normals,uint32_t*left_edx,int exact){
 rays6_frame frame;r6enter(&frame,exact,ends,a,b,c,starts,hits,points,normals);rays6_frame*f=&frame;
 const rx_v4 epsilon=v4_splat(F32(0,0x9777c4));rx_v4 start[6];
 for(unsigned i=0;i<6;i++)start[i]=v4_ld(starts+16*i);
 r6u(f,0x58,points);r6u(f,0xbc,normals);r6v(f,0xf0,epsilon);
 const rx_v4 big=v4_splat(F32(0,0x977494));
 for(unsigned i=0;i<6;i++)v4_st(points+16*i,big);
 r6u(f,0x24,ends);r6v(f,0x240,big);
 for(unsigned i=0;i<6;i++)SU32(hits,4*i,0);
 rx_v4 d[6],square[6],length[6],direction[6],e1[6],e2[6],s[6],p[6],det[6],u[6],inverse[6],q[6],v[6],t[6];uint32_t det_ok[6],u_ok[6],v_ok[6];
 for(unsigned i=0;i<6;i++){d[i]=v4_sub(v4_ld(ends+16*i),start[i]);r6v(f,0x4f0+16*i,d[i]);square[i]=v4_mul(d[i],d[i]);length[i]=v4_sqrt(r6sum021(square[i]));r6v(f,0x550+16*i,length[i]);}
 r6v(f,0x40,square[5]);
 const rx_v4 infinity=v4_ld(0x92dc20),invalid=v4_ld(0x92dc30),register_zero=v4_zero();
 for(unsigned i=0;i<6;i++){
  rx_v4 norm2=r6sum012(square[i]),n=v4_sqrt(norm2),finite=r6neq(norm2,infinity),nonzero=r6neq(register_zero,n);
  if(i<5)r6v(f,0x2a0,finite);
  direction[i]=v4_or(v4_and(v4_and(v4_div(d[i],n),nonzero),finite),v4_andn(finite,invalid));
  /* The normalization scratch is overwritten with original triangle zero below. */
  /*85f0e1/85f281 rewrite only endpoints0/5. Normalized1..4
   * stay in XMM1..4; private500..530 deliberately keep raw directions. */
  if(i==0||i==5)r6v(f,0x4f0+16*i,direction[i]);
 }
 rx_v4 first[6];for(unsigned i=0;i<6;i++)first[i]=v4_ld(a+16*i);
 const uint32_t first_slot[]={0x2a0,0x40,0x290,0xd0,0xc0,0xe0};
 for(unsigned i=0;i<6;i++){r6v(f,first_slot[i],first[i]);e1[i]=v4_sub(v4_ld(b+16*i),first[i]);r6v(f,0x2b0+16*i,e1[i]);}
 for(unsigned i=0;i<6;i++){e2[i]=v4_sub(v4_ld(c+16*i),first[i]);if(i>=2)r6v(f,0x490+16*i,e2[i]);}
 const uint32_t start_slot[]={0x200,0x180,0x190,0x1b0,0x1d0,0x1f0};
 for(unsigned i=0;i<6;i++){r6v(f,start_slot[i],start[i]);s[i]=v4_sub(start[i],first[i]);r6v(f,0x310+16*i,s[i]);}
 const uint32_t e2c[]={0xc0,0x250,0x1a0,0x1c0,0x1e0,0x2a0},e2d[]={0x280,0x230,0x210,0x170,0x220,0x270};
 const rx_v4 negative=v4_ld(0x92de10);
 for(unsigned i=0;i<6;i++){
  r6v(f,e2c[i],r6c9(e2[i]));r6v(f,e2d[i],r6d2(e2[i]));
  if(i<2)r6v(f,0x490+16*i,e2[i]);
  p[i]=v4_mul(cross_raw(direction[i],e2[i]),negative);r6v(f,0x370+16*i,p[i]);
 }
 for(unsigned i=0;i<6;i++){
  /* Original ray0 multiplies p by edge, rays1..5 edge by p. */
  det[i]=r6sum012(i?v4_mul(e1[i],p[i]):v4_mul(p[i],e1[i]));r6v(f,0x430+16*i,det[i]);
 }
 for(unsigned i=0;i<6;i++){u[i]=r6sum012(i?v4_mul(s[i],p[i]):v4_mul(p[i],s[i]));r6v(f,0x3d0+16*i,u[i]);}
 uint32_t any=0;for(unsigned i=0;i<6;i++){det_ok[i]=r6mask(f,v4_cmple(epsilon,det[i]));r6u(f,0x74+4*i,det_ok[i]);any|=det_ok[i];}
 if(!any){*left_edx=0;return r6leave(f,0,0,0);}
 const rx_v4 one=v4_ld(0xd61650);
 for(unsigned i=0;i<6;i++){inverse[i]=v4_div(one,det[i]);r6v(f,0x370+16*i,inverse[i]);}
 for(unsigned i=0;i<6;i++){u[i]=i?v4_mul(u[i],inverse[i]):v4_mul(inverse[i],u[i]);r6v(f,0x3d0+16*i,u[i]);}
 uint32_t lower[6],upper[6];
 for(unsigned i=0;i<6;i++){lower[i]=r6mask(f,v4_cmple(v4_ld(0xd61670),u[i]));if(i>=3)r6u(f,0x68+4*(i-3),lower[i]);}
 for(unsigned i=0;i<6;i++){upper[i]=r6mask(f,v4_cmple(u[i],one));r6u(f,0xc+4*i,upper[i]);}
 any=0;for(unsigned i=0;i<6;i++){u_ok[i]=lower[i]&upper[i];if(i>=3)r6u(f,0x68+4*(i-3),u_ok[i]);any|=u_ok[i];}
 if(!any){*left_edx=u_ok[0];return r6leave(f,0,u_ok[0],0);}
 const uint32_t e1c[]={0x260,0xd0,0x140,0x150,0x110,0xf0},e1d[]={0xe0,0x130,0x160,0x100,0x120,0x40};
 for(unsigned i=0;i<6;i++){
  r6v(f,e1c[i],r6c9(e1[i]));r6v(f,e1d[i],r6d2(e1[i]));
  rx_v4 first_product=v4_mul(r6d2(s[i]),r6c9(e1[i]));
  /* Last first product occupies290; earlier products use40 but that is
   * subsequently the last edge's d2 and must not be restored afterward. */
  if(i>0&&i<5)r6v(f,0x40,first_product);
  if(i==5)r6v(f,0x290,first_product);
  q[i]=v4_mul(cross_raw(s[i],e1[i]),negative);r6v(f,0x430+16*i,q[i]);
 }
 for(unsigned i=0;i<6;i++){v[i]=r6sum012(i?v4_mul(q[i],direction[i]):v4_mul(direction[i],q[i]));r6v(f,0x310+16*i,v[i]);}
 for(unsigned i=0;i<6;i++){t[i]=r6sum012(i?v4_mul(q[i],e2[i]):v4_mul(e2[i],q[i]));r6v(f,0x2b0+16*i,t[i]);}
 /*860226..8602b9: V0 normalizes in XMM6 only. Its raw dot remains
  * in B310; only V1..5 overwrite their private dot cells. */
 for(unsigned i=0;i<6;i++){v[i]=v4_mul(v[i],inverse[i]);if(i)r6v(f,0x310+16*i,v[i]);}
 for(unsigned i=0;i<6;i++)r6v(f,0x490+16*i,v4_add(v[i],u[i]));
 const rx_v4 zero=v4_ld(0xd61670);
 for(unsigned i=0;i<6;i++){lower[i]=r6mask(f,v4_cmple(zero,v[i]));r6u(f,0xc+4*i,lower[i]);}
 for(unsigned i=0;i<6;i++){upper[i]=r6mask(f,v4_cmple(v4_add(v[i],u[i]),v4_ld(0xd61650)));r6u(f,0x28+4*i,upper[i]);}
 any=0;for(unsigned i=0;i<6;i++){v_ok[i]=lower[i]&upper[i];r6u(f,0xc+4*i,v_ok[i]);any|=v_ok[i];}
 if(!any){*left_edx=u_ok[0];return r6leave(f,0,u_ok[0],0);}
 for(unsigned i=0;i<6;i++){t[i]=v4_mul(t[i],inverse[i]);r6v(f,0x2b0+16*i,t[i]);}
 uint32_t in_length[6],forward[6],finite_t[6],result[6];
 for(unsigned i=0;i<6;i++){in_length[i]=r6mask(f,v4_cmplt(t[i],length[i]));r6u(f,0x8c+4*i,in_length[i]);}
 for(unsigned i=0;i<6;i++){forward[i]=r6mask(f,v4_cmplt(zero,t[i]));r6u(f,0xa4+4*i,forward[i]);}
 for(unsigned i=0;i<6;i++){finite_t[i]=r6mask(f,v4_cmplt(t[i],big));r6u(f,0x28+4*i,finite_t[i]);}
 any=0;for(unsigned i=0;i<6;i++){result[i]=u_ok[i]&det_ok[i]&finite_t[i]&forward[i]&in_length[i]&v_ok[i];SU32(hits,4*i,result[i]);if(i==4)r6u(f,0x24,result[i]);any|=result[i];}
 *left_edx=result[0];if(!any)return r6leave(f,0,result[0],1);
 rx_v4 normal[6];for(unsigned i=0;i<6;i++){
  normal[i]=v4_mul(cross_raw(e1[i],e2[i]),negative);if(i>0&&i<5)r6v(f,0x550+16*i,normal[i]);
 }
 for(unsigned i=0;i<6;i++)v4_st(points+16*i,v4_add(i?v4_mul(t[i],direction[i]):v4_mul(direction[i],t[i]),start[i]));
 for(unsigned i=0;i<6;i++)v4_st(normals+16*i,v4_div(normal[i],v4_sqrt(r6sum012(v4_mul(normal[i],normal[i])))));
 return r6leave(f,1,result[0],1);
}
uint32_t phys_rays6_triangles(rx_addr ends,rx_addr a,rx_addr b,rx_addr c,rx_addr starts,rx_addr hits,rx_addr points,rx_addr normals,uint32_t*edx){return rays6_kernel(ends,a,b,c,starts,hits,points,normals,edx,0);}
uint32_t phys_rays6_triangles_observed(rx_addr ends,rx_addr a,rx_addr b,rx_addr c,rx_addr starts,rx_addr hits,rx_addr points,rx_addr normals){uint32_t edx;return rays6_kernel(ends,a,b,c,starts,hits,points,normals,&edx,1);}
RX_FUNCTION(0x0085ee70,phys_rays6_triangles)

/* ---- 0086fac0 Triangles_DynamicRayContact: a triangle mesh against a segment mesh ------------------------------------------ */

/* The complete triangle/segment algorithm has one numeric owner. Optional
 * observations retain real stack values at semantic boundaries; no generated
 * instruction interpreter or alternate collision evaluator is used. */
#include "triangles_segments_observed.h"
#include "triangles_segments_frames.h"
#include "triangles_segments_dependencies.h"
struct row_terms {float aa,bb,cc,wa,wb,wc,ac,bc;rx_v4 q;};
static void rows_from_quat(rx_v4 q,rx_v4 r[3],struct row_terms*t){
 const float K=F32(0,0x9d7b30),a=q.v[0],b=q.v[1],c=q.v[2],d=q.v[3];
 const float z=F32(0,0x9774a8),one=F32(0,0x931f24);
 const float aK=phys_mul(a,K),bK=phys_mul(b,K),cK=phys_mul(c,K);
 const float aa=phys_sub(z,phys_mul(a,aK)),bb=phys_sub(z,phys_mul(b,bK)),cc=phys_sub(z,phys_mul(c,cK));
 const float wa=phys_mul(d,aK),wb=phys_mul(d,bK),wc=phys_mul(d,cK);
 const float ac_raw=phys_mul(c,aK);
 const float ab=phys_sub(z,phys_mul(b,aK)),ac=phys_sub(z,ac_raw),bc=phys_sub(z,phys_mul(c,bK));
 const float r02=phys_sub(ac,wb),r12=phys_add(bc,wa),r22=phys_sub(one,phys_add(bb,aa));
 r[0]=v4(phys_sub(one,phys_add(cc,bb)),phys_add(ab,wc),r02,r02);
 r[1]=v4(phys_sub(ab,wc),phys_sub(one,phys_add(cc,aa)),r12,r12);
 r[2]=v4(phys_add(ac,wb),phys_sub(bc,wa),r22,r22);
 if(t){t->aa=aa;t->bb=bb;t->cc=cc;t->wa=wa;t->wb=wb;t->wc=wc;t->ac=ac_raw;t->bc=bc;t->q=q;}
}
/* ADDSS changes only lane0: preserve the packed product's upper lanes in
 * every source spill instead of manufacturing a splat of its scalar dot. */
static rx_v4 dot_terms(rx_v4 a,rx_v4 b){rx_v4 p=v4_mul(a,b);p.v[0]=phys_hsum3(p);return p;}
static rx_v4 rows_apply(const rx_v4 r[3],rx_v4 p,rx_v4*terms){
 rx_v4 d[3];for(unsigned i=0;i<3;i++)d[i]=dot_terms(p,r[i]);
 if(terms)for(unsigned i=0;i<3;i++)terms[i]=d[i];
 return v4(d[0].v[0],d[1].v[0],d[2].v[0],d[2].v[0]);
}
struct pair_effects {int exact,triangle;rx_addr B;rx_tri_segment_frame frame;};
static void ef_u(struct pair_effects*e,unsigned o,uint32_t v){if(e->exact)SU32(e->B,o,v);}
static void ef_v(struct pair_effects*e,unsigned o,rx_v4 v){if(e->exact)v4_st(e->B+o,v);}
static void ef_f(struct pair_effects*e,unsigned o,float v){if(e->exact)SF32(e->B,o,v);}
static void ef_r(struct pair_effects*e,unsigned r,uint32_t v){if(e->exact)fx_set_reg(r,v);}
static void row_effects(struct pair_effects*e,const struct row_terms*t,unsigned phase,const rx_v4 r[3]){
 if(!e->exact)return;
 const int tri=e->triangle;ef_v(e,0x20,t->q);ef_f(e,0x1c,t->aa);ef_f(e,0x30,t->wa);
 ef_f(e,tri?0x10:0x14,t->wb);ef_f(e,tri?0x14:0x10,t->wc);
 ef_f(e,phase==2?(tri?0x70:0xa0):(tri?0x34:0x40),t->ac);if(phase!=2)ef_f(e,0x18,t->bc);
 if(phase==0){ef_v(e,tri?0xd0:0xe0,r[0]);ef_v(e,tri?0xf0:0xd0,r[1]);ef_v(e,tri?0xe0:0xc0,r[2]);}
 else if(phase==1){if(tri)ef_v(e,0xc0,r[2]);}
 else ef_v(e,tri?0x100:0xe0,r[0]);
}
/* Six pairs, each batch buffer remains actual guest memory. */
struct ray_batch {rx_addr L,q,v0,v1,v2,p,hits,points,normals,M;uint32_t flag;int k;struct pair_effects*e;};
static void batch_add(struct ray_batch*b,rx_v4 v0,rx_v4 v1,rx_v4 v2,rx_v4 p,rx_v4 q){
 uint32_t o=16u*(uint32_t)b->k;v4_st(b->v0+o,v0);v4_st(b->v1+o,v1);v4_st(b->v2+o,v2);v4_st(b->p+o,p);v4_st(b->q+o,q);++b->k;
 ef_r(b->e,FX_EAX,o);ef_r(b->e,FX_EDI,(uint32_t)b->k);ef_u(b->e,b->e->triangle?0x3c:0x18,(uint32_t)b->k);
}
static void batch_run(struct ray_batch*b,int valid,int full){
 struct pair_effects*e=b->e;uint32_t r,edx;rx_addr B=b->L;
 if(e->exact){
  uint32_t args[]={b->v1,b->v2,b->p,b->hits,b->points,b->normals};
  ef_r(e,FX_EAX,b->v2);ef_r(e,FX_ECX,b->q);ef_r(e,FX_EDX,b->v0);
  rx_addr ra=e->triangle?(full?0x87039f:0x870571):(full?0x87115f:0x87132e);
  (void)fx_push(6,args,ra);r=rx_tri_segment_rays_dependency(B-24);rx_set_sp(B);
 }else{rx_set_sp(B-28);r=phys_rays6_triangles(b->q,b->v0,b->v1,b->v2,b->p,b->hits,b->points,b->normals,&edx);rx_set_sp(B);}
 if(!r)return;
 if(full){ef_r(e,FX_EDI,b->hits);ef_r(e,FX_EAX,0);}else{ef_r(e,FX_EBX,0);ef_r(e,FX_EDX,0);}
 for(int i=0;i<valid;i++){
  uint32_t o=16u*(uint32_t)i;
  if(U32(b->hits,4u*(uint32_t)i)){
   rx_addr n=b->normals+o;if(b->flag&255)v4_st(n,v4_sub(v4_zero(),v4_ld(n)));
   int32_t c=I32(b->M,0);if(c>=29)c=29;rx_addr rec=b->M+48u*(uint32_t)c;
   rx_v4 point=v4_ld(b->points+o),normal=v4_ld(n);v4_st(rec+16,point);v4_st(rec+32,normal);
   rx_v4 d=v4_sub(v4_ld(b->q+o),point),sq=v4_mul(d,d);
   v4_st(rec+48,v4_splat(phys_sqrt(phys_add(phys_add(sq.v[0],sq.v[2]),sq.v[1]))));
   SU32(b->M,0,(uint32_t)c+1);SU32(b->M,0x6a0,e->exact&&full?fx_reg(FX_EBX):1);
   ef_r(e,full?FX_EDX:FX_ECX,(uint32_t)c+1);ef_r(e,full?FX_ECX:FX_EAX,rec);
  }
  if(full){ef_r(e,FX_EAX,o+16);ef_r(e,FX_EDI,b->hits+4u*((uint32_t)i+1));}
  else{ef_r(e,FX_EBX,(uint32_t)i+1);ef_r(e,FX_EDX,o+16);}
 }
}
static void batch_flush(struct ray_batch*b){
 if(b->k<=0)return;struct pair_effects*e=b->e;uint32_t o=16u*(uint32_t)b->k,last=o-16;
 if(b->k<6){
  if(e->exact){
   ef_u(e,e->triangle?0x80:0x40,b->v1+last);ef_u(e,e->triangle?0x50:0x70,b->v2+last);
   ef_r(e,FX_ECX,b->p+last);ef_r(e,FX_EDX,b->q+last);ef_r(e,FX_EDI,b->v0+last);
  }
  for(;o<96;o+=16){
   /* Actual source padding order differs from pair assembly order. */
   v4_st(b->p+o,v4_ld(b->p+last));v4_st(b->q+o,v4_ld(b->q+last));v4_st(b->v0+o,v4_ld(b->v0+last));
   v4_st(b->v1+o,v4_ld(b->v1+last));v4_st(b->v2+o,v4_ld(b->v2+last));
   ef_r(e,FX_EBX,b->v2+last);ef_r(e,FX_EAX,o+16);
  }
  ef_r(e,FX_EDI,(uint32_t)b->k);
 }
 batch_run(b,b->k,0);
}
static void matrix_compose(struct pair_effects*e,const rx_v4 rt[3],const rx_v4 rs[3],rx_v4 N[3]){
 rx_v4 d[3][3];for(unsigned i=0;i<3;i++)for(unsigned j=0;j<3;j++)d[i][j]=dot_terms(rt[i],rs[j]);
 if(e->triangle){
  ef_v(e,0x150,d[0][1]);ef_v(e,0x160,d[1][0]);ef_v(e,0x130,d[1][1]);ef_v(e,0x110,d[1][2]);ef_v(e,0x120,d[2][0]);ef_v(e,0x140,d[2][1]);
  for(unsigned i=0;i<3;i++)N[i]=v4(d[i][0].v[0],d[i][1].v[0],d[i][2].v[0],d[i][2].v[0]);
  ef_v(e,0xd0,N[0]);ef_v(e,0xf0,N[1]);ef_v(e,0xe0,N[2]);
 }else{
  ef_v(e,0x100,v4_splat(d[1][0].v[0]));ef_v(e,0x170,d[2][0]);ef_v(e,0x140,d[0][1]);ef_v(e,0x120,d[1][1]);
  for(unsigned j=0;j<3;j++)N[j]=v4(d[0][j].v[0],d[1][j].v[0],d[2][j].v[0],d[2][j].v[0]);
  ef_v(e,0x130,N[0]);ef_v(e,0xf0,N[1]);ef_v(e,0x110,N[2]);
 }
}
static void triangles_segments_kernel(rx_addr PS,rx_addr QT,rx_addr M,rx_addr T,rx_addr PT,rx_addr S,rx_addr QS,uint32_t flag,int exact,rx_v4*live){
 struct pair_effects e={0};e.exact=exact;rx_addr entry=rx_sp();int32_t nt=I32(T,0x20),ns=I32(S,0x20);e.triangle=nt>=ns;
 if(exact){rx_tri_segment_frame_enter(&e.frame,PS,QT,M,T,PT,S,QS,flag);e.B=e.frame.base;}
 else{e.B=((entry-4)&~15u)-0x490;rx_set_sp(e.B);}
 rx_addr B=e.B;const rx_v4 conj=v4_ld(0x93a220);rx_v4 rs[3],rt[3],N[3],back[3];struct row_terms terms;
 rows_from_quat(v4_mul(conj,v4_ld(QS)),rs,&terms);row_effects(&e,&terms,0,rs);
 rows_from_quat(v4_mul(v4_ld(QT),conj),rt,&terms);row_effects(&e,&terms,1,rt);
 matrix_compose(&e,rt,rs,N);
 rx_v4 offset=rows_apply(e.triangle?rt:rs,e.triangle?v4_sub(v4_ld(PS),v4_ld(PT)):v4_sub(v4_ld(PT),v4_ld(PS)),0);
 ef_v(&e,e.triangle?0x140:0xc0,offset);
 int32_t first=I32(M,0);if(first>=29)first=29;ef_u(&e,e.triangle?0x34:0x6c,(uint32_t)first);ef_u(&e,e.triangle?0x3c:0x18,0);ef_r(&e,FX_EDI,0);
 struct ray_batch b={B,B+0x200,B+0x360,B+0x300,B+0x3c0,B+0x2a0,B+0x180,B+0x420,B+0x1a0,M,flag,0,&e};
 if(e.triangle){
  ef_r(&e,FX_EAX,(uint32_t)ns);
  if(ns>0){ef_u(&e,0x38,0);ef_u(&e,0x50,(uint32_t)ns);ef_r(&e,FX_EBX,1);}
  for(int32_t s=0;s<ns;s++){
   rx_addr segment=U32(S,0x24)+32u*(uint32_t)s;ef_r(&e,FX_ECX,S);ef_r(&e,FX_EDX,U32(S,0x24));ef_r(&e,FX_EAX,segment);
   rx_v4 pd[3];rx_v4 p=v4_add(rows_apply(N,v4_ld(segment),pd),offset);ef_v(&e,0x120,pd[1]);
   rx_v4 q=v4_add(rows_apply(N,v4_ld(segment+16),0),offset),smax=v4_max(p,q),smin=v4_min(p,q);
   ef_v(&e,0x150,p);ef_v(&e,0x160,q);ef_v(&e,0x170,smax);ef_v(&e,0x100,smin);ef_r(&e,FX_EAX,(uint32_t)nt);
   if(nt>0){ef_u(&e,0x18,0);ef_u(&e,0x6c,(uint32_t)nt);}
   for(int32_t t=0;t<nt;t++){
    rx_addr tri=U32(T,0x24)+48u*(uint32_t)t;rx_v4 v0=v4_ld(tri),v1=v4_ld(tri+16),v2=v4_ld(tri+32);
    ef_r(&e,FX_ECX,T);ef_r(&e,FX_EAX,tri);ef_v(&e,0x130,v2);ef_v(&e,0x110,v1);
    rx_v4 m0=v4_cmple(v4_min(v0,v4_min(v1,v2)),smax);ef_v(&e,0xc0,m0);
    uint32_t c1=v4_lb(m0,1),sum=v4_lb(m0,2)+c1+v4_lb(m0,0);ef_r(&e,FX_ECX,c1);ef_r(&e,FX_EDX,sum);
    if(sum==0xfffffffdu){
     rx_v4 m1=v4_cmple(smin,v4_max(v0,v4_max(v1,v2)));ef_v(&e,0xc0,m1);
     ef_r(&e,FX_EAX,v4_lb(m1,1));ef_r(&e,FX_ECX,v4_lb(m1,2)+v4_lb(m1,1)+v4_lb(m1,0));
     if(all3(m1)){batch_add(&b,v0,v1,v2,p,q);if(b.k==6){batch_run(&b,6,1);b.k=0;ef_r(&e,FX_EDI,0);ef_u(&e,0x3c,0);}}
    }
    ef_u(&e,0x18,48u*((uint32_t)t+1));ef_u(&e,0x6c,(uint32_t)(nt-t-1));
   }
   ef_u(&e,0x38,32u*((uint32_t)s+1));ef_u(&e,0x50,(uint32_t)(ns-s-1));
  }
 }else{
  b.v0=B+0x2a0;b.v1=B+0x3c0;b.v2=B+0x300;b.p=B+0x360;b.hits=B+0xa8;
  ef_r(&e,FX_EAX,(uint32_t)nt);if(nt>0){ef_u(&e,0x3c,0);ef_u(&e,0x50,(uint32_t)nt);ef_r(&e,FX_EBX,1);}
  for(int32_t t=0;t<nt;t++){
   rx_addr tri=U32(T,0x24)+48u*(uint32_t)t;rx_v4 d0[3],d1[3],d2[3];
   ef_r(&e,FX_EAX,tri);ef_r(&e,FX_ECX,U32(T,0x24));ef_r(&e,FX_EDX,48u*(uint32_t)t);
   rx_v4 v0=v4_add(rows_apply(N,v4_ld(tri),d0),offset),v1=v4_add(rows_apply(N,v4_ld(tri+16),d1),offset),v2=v4_add(rows_apply(N,v4_ld(tri+32),d2),offset);
   ef_v(&e,0x170,d0[0]);ef_v(&e,0x100,d0[1]);ef_v(&e,0x120,d1[1]);ef_v(&e,0x140,d2[0]);
   rx_v4 tmax=v4_max(v0,v4_max(v1,v2)),tmin=v4_min(v0,v4_min(v1,v2));
   ef_v(&e,0x290,v0);ef_v(&e,0x260,v1);ef_v(&e,0x280,v2);ef_v(&e,0x270,tmax);ef_v(&e,0x180,tmin);ef_r(&e,FX_EAX,(uint32_t)ns);
   if(ns>0){ef_u(&e,0x68,0);ef_u(&e,0x34,(uint32_t)ns);}
   for(int32_t s=0;s<ns;s++){
    rx_addr segment=U32(S,0x24)+32u*(uint32_t)s;rx_v4 p=v4_ld(segment),q=v4_ld(segment+16);
    ef_r(&e,FX_ECX,U32(S,0x24));ef_r(&e,FX_EDX,32u*(uint32_t)s);ef_r(&e,FX_EAX,segment);ef_v(&e,0x150,p);ef_v(&e,0x160,q);
    rx_v4 m0=v4_cmple(tmin,v4_max(p,q));ef_v(&e,0xd0,m0);ef_r(&e,FX_EAX,v4_lb(m0,1));ef_r(&e,FX_ECX,v4_lb(m0,2)+v4_lb(m0,1)+v4_lb(m0,0));
    if(all3(m0)){
     rx_v4 m1=v4_cmple(v4_min(p,q),tmax);ef_v(&e,0xd0,m1);ef_r(&e,FX_EDX,v4_lb(m1,1));ef_r(&e,FX_EAX,v4_lb(m1,2)+v4_lb(m1,1)+v4_lb(m1,0));
     if(all3(m1)){batch_add(&b,v0,v1,v2,p,q);if(b.k==6){batch_run(&b,6,1);b.k=0;ef_r(&e,FX_EDI,0);ef_u(&e,0x18,0);}}
    }
    ef_u(&e,0x68,32u*((uint32_t)s+1));ef_u(&e,0x34,(uint32_t)(ns-s-1));
   }
   ef_u(&e,0x3c,48u*((uint32_t)t+1));ef_u(&e,0x50,(uint32_t)(nt-t-1));
  }
 }
 batch_flush(&b);
 /* Late quaternion and position loads are intentional: genuine nested calls
  * can write the manifold, but no early pose snapshot substitutes for them. */
 rx_addr backQ=e.triangle?QT:QS;ef_r(&e,FX_EAX,backQ);rows_from_quat(v4_ld(backQ),back,&terms);row_effects(&e,&terms,2,back);
 /*707b1/71577: the complete late back row2 replaces incoming XMM0,
  * even when there were no new contacts or no six-ray calls. */
 if(live)*live=back[2];
 int converted=0;ef_r(&e,FX_ECX,(uint32_t)first);
 for(int32_t c=first;c<I32(M,0);c++){
  rx_addr rec=M+16+48u*(uint32_t)c;rx_addr position=e.triangle?PT:PS;ef_r(&e,FX_EDX,position);
  v4_st(rec+16,rows_apply(back,v4_ld(rec+16),0));v4_st(rec,rows_apply(back,v4_ld(rec),0));rx_v4 worldPoint=v4_add(v4_ld(position),v4_ld(rec));v4_st(rec,worldPoint);if(live)*live=worldPoint;
  ef_r(&e,FX_EAX,rec+48);ef_r(&e,FX_ECX,(uint32_t)c+1);converted=1;
 }
 if(exact)rx_tri_segment_frame_leave(&e.frame,converted);else rx_set_sp(entry);
}
void phys_pair_triangles_segments(rx_addr PS,rx_addr QT,rx_addr M,rx_addr T,rx_addr PT,rx_addr S,rx_addr QS,uint32_t flag){triangles_segments_kernel(PS,QT,M,T,PT,S,QS,flag,0,0);}
void phys_pair_triangles_segments_observed(rx_addr PS,rx_addr QT,rx_addr M,rx_addr T,rx_addr PT,rx_addr S,rx_addr QS,uint32_t flag){triangles_segments_kernel(PS,QT,M,T,PT,S,QS,flag,1,0);}
void phys_pair_triangles_segments_observed_live(rx_addr PS,rx_addr QT,rx_addr M,rx_addr T,rx_addr PT,rx_addr S,rx_addr QS,uint32_t flag,rx_v4*live){
#ifdef RX_DISPATCH_LIVE_COUNTERS
 extern void rx_dispatch_live_count(unsigned,uint32_t);rx_dispatch_live_count(1,U32(rx_sp(),0));
#endif
 triangles_segments_kernel(PS,QT,M,T,PT,S,QS,flag,1,live);}
RX_FUNCTION(0x0086fac0,phys_pair_triangles_segments)
