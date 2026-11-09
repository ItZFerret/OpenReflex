/* phys_coll_shape.c -- collision shape functions of the RavenPhysics collision system (native/game/notes/collision.md).
 *
 * Quaternions are vec4 (x, y, z, w) in game memory. The exe multiplies them with the SIMD form l * r =
 *     (z + y) + (l.w * r + x)   x = l.x * r.wzyx * (1,-1,1,-1), y = l.y * r.zwxy * (1,1,-1,-1), z = r.yxwz * l.z * (-1,1,1,-1)
 * (operand order kept: x86 returns the first NaN operand). Rotating a vector by q is q * v * conj(q), conj via (-1,-1,-1,1).
 * Shape objects: +0x20 the shape's vec4 (half extents / radius vector / (radius, height)); a composite shape (this = composite):
 * children pointers vector at +0x2c (begin) / +0x30 (end), child rotations (vec4) vector +0x44 / +0x48, child offsets (vec4)
 * vector +0x5c / +0x60.
 * The exe's std::vector element accesses are bounds checked (_invalid_parameter_noinfo when out of range); the indices used here
 * are always in range and the checks (and the composite's prefetch loop, which only runs them) are left out. */
#include "phys_coll_shape.h"
#include "phys_coll_quat.h"
#include "composite_child_observed.h"
#include "fx_util.h"
#include <stdlib.h>

/* A virtual bounds call. */
void phys_shape_bounds(rx_addr fn, rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max) {
    phys_shape_bounds_fn f = RX_FN(fn, phys_shape_bounds_fn);
    if (f) { f(self, q, pos, out_min, out_max); return; }
    const uint32_t args[4] = {q, pos, out_min, out_max};
    rx_call_original(fn, RX_THISCALL, self, 4, args);
}

/* A virtual bounds call at the original's stack addresses (the unported fallback pushes its arguments itself). */
void phys_shape_bounds_call(rx_addr fn, rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max) {
    phys_shape_bounds_fn f = RX_FN(fn, phys_shape_bounds_fn);
    if (!f) { phys_shape_bounds(fn, self, q, pos, out_min, out_max); return; }
    const rx_addr sp = rx_sp();
    SU32(sp - 16, 0, q); SU32(sp - 12, 0, pos); SU32(sp - 8, 0, out_min); SU32(sp - 4, 0, out_max);
    rx_set_sp(sp - 20);
    f(self, q, pos, out_min, out_max);
    rx_set_sp(sp);
}

/* The world box of a rotated shape from its four (absolute) axis vectors a[i]: each rotated, r_i = q * (a_i * (q * conj)) (q read
 * and conjugated per vector, as the original), bound = max(max(|r0|, |r1|), max(|r2|, |r3|)); max = pos + bound stored first,
 * then min = pos - bound with pos read again. */
static void rotated_bounds(const rx_v4 a[4], rx_addr q_p, rx_addr pos_p, rx_addr out_min, rx_addr out_max) {
    const rx_v4 q = v4_ld(q_p);
    rx_v4 r[4];
    for (int i = 0; i < 4; i++) r[i] = coll_quat_mul(q, coll_quat_mul(a[i], v4_mul(q, COLL_QCONJ)));
    const rx_v4 bound = v4_max(v4_max(coll_v4_abs_max(r[0]), coll_v4_abs_max(r[1])), v4_max(coll_v4_abs_max(r[2]), coll_v4_abs_max(r[3])));
    v4_st(out_max, v4_add(v4_ld(pos_p), bound));
    v4_st(out_min, v4_sub(v4_ld(pos_p), bound));
}

/* 008455f0 CollisionShapeBox::vf2 (thiscall: q*, pos*, out min*, out max*; RET 0x10): the world AABB of the box with half extents
 * h = this+0x20: axis vectors |h * s| for the sign patterns (1,1,1,1), (-1,1,1,1), (1,-1,1,1), (1,1,-1,-1), -1 being the runtime
 * global 0x9cd9e8. */
void phys_box_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max) {
    const rx_v4 ext = v4_ld(self + 0x20);
    const float one = 1.0f /* 0x931f24 */, minus_one = F32(0, 0x9cd9e8);
    const rx_v4 signs[4] = {v4(one, one, one, one), v4(minus_one, one, one, one), v4(one, minus_one, one, one), v4(one, one, minus_one, minus_one)};
    rx_v4 a[4];
    for (int i = 0; i < 4; i++) a[i] = v4_xyz(v4_mul(signs[i], ext));      /* & 0x92db80: w cleared */
    rotated_bounds(a, q, pos, out_min, out_max);
}
RX_FUNCTION(0x008455f0, phys_box_bounds)

/* 00846700 CollisionShapeDynamicCylinder::vf2 (as the box's): radius r = this+0x20, height h = this+0x24; axis vectors
 * (r, h*0.5, r, r), (-0.0 - r, h*0.5, r, r), (r, h*-0.5, r, r), (r, h*0.5, -0.0 - r, -0.0 - r), absolute.
 * Frame: push ebp; and esp, -16; sub esp, 0xcc; push esi -> S. The original leaves the partial products of its second quaternion
 * product q * p_i (p_i = a_i * (q * conj)) there, and later code reads them uninitialized: in CompositeVsHFTerrain 00873010's
 * child loop, CylinderVsHFTerrain 0086f2a0's never-written normal / point w lanes (its frame +0x1c / +0x2c) are this frame's
 * +0x6c / +0x7c, copied into the contact records (core/notes/phys.md "The w lanes of CompositeVsHFTerrain"). So the port writes
 * the frame as the original leaves it (disassembly 00846700..00846ba1): +0x0c h; per vector s1 = p.wzyx, s2 = p.zwxy, X = q.x * s1,
 * Y = q.y * s2 at +0x90 / +0x70 / +0x50 (X0, s2_0, Y0), +0x40 / +0x30 / +0xb0 (vector 1), +0xa0 / +0x60 / +0x80 (vector 2),
 * +0xc0 / +0x20 / +0x10 (vector 3). Not modelled: +0x00 (the caller's ESI) and the caller's EBP; +0x04 / +0x08 are not written. */
void phys_dyncyl_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max) {
    const rx_addr S = ((rx_sp() - 4) & ~15u) - 0xcc - 4;
    const float r = F32(self, 0x20), h = F32(self, 0x24);
    const float half_h = phys_mul(h, 0.5f /* 0x9322cc */), neg_half_h = phys_mul(h, -0.5f /* 0x9774b0 */);
    const float neg_r = phys_sub(-0.0f /* 0x9774a8 */, r);
    const rx_v4 v[4] = {v4(r, half_h, r, r), v4(neg_r, half_h, r, r), v4(r, neg_half_h, r, r), v4(r, half_h, neg_r, neg_r)};
    rx_v4 a[4];
    for (int i = 0; i < 4; i++) a[i] = v4_xyz(v[i]);
    {
        static const uint32_t x_at[4] = {0x90, 0x40, 0xa0, 0xc0}, s2_at[4] = {0x70, 0x30, 0x60, 0x20}, y_at[4] = {0x50, 0xb0, 0x80, 0x10};
        const rx_v4 qv = v4_ld(q);
        SF32(S, 0x0c, h);
        for (int i = 0; i < 4; i++) {
            const rx_v4 p = coll_quat_mul(a[i], v4_mul(qv, COLL_QCONJ));
            const rx_v4 s1 = v4_perm(p, 3, 2, 1, 0), s2 = v4_perm(s1, 1, 0, 3, 2);
            v4_st(S + x_at[i], v4_mul(v4_lane(qv, 0), s1));
            v4_st(S + s2_at[i], s2);
            v4_st(S + y_at[i], v4_mul(v4_lane(qv, 1), s2));
        }
    }
    rotated_bounds(a, q, pos, out_min, out_max);
}
RX_FUNCTION(0x00846700, phys_dyncyl_bounds)

/* 00845480 CollisionShapeCircle::vf2 (thiscall: q*, pos*, out min*, out max*; RET 0x10): min = pos - radius vector (this+0x20),
 * max = pos + radius vector (position and radius read again for the max). */
void phys_circle_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max) {
    (void)q;
    v4_st(out_min, v4_sub(v4_ld(pos), v4_ld(self + 0x20)));
    v4_st(out_max, v4_add(v4_ld(pos), v4_ld(self + 0x20)));
}
RX_FUNCTION(0x00845480, phys_circle_bounds)

/* 00845010 CollisionShapeTriangles::vf3 (also CollisionShapeStaticRays vf2 / vf3; thiscall: q*, pos*, out min*, out max*; RET 0x10):
 * min = pos - extent (this+0x30), max = pos + extent (position read again for the min). */
void phys_triangles_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max) {
    (void)q;
    v4_st(out_max, v4_add(v4_ld(pos), v4_ld(self + 0x30)));
    v4_st(out_min, v4_sub(v4_ld(pos), v4_ld(self + 0x30)));
}
RX_FUNCTION(0x00845010, phys_triangles_bounds)

/* 00845a70 CollisionShapeBox::vf3: a tail call of the object's vtable slot 2 (vf2, the bounds) with the same arguments. */
void phys_box_vf3(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max) {
    phys_shape_bounds(U32(U32(self, 0), 8), self, q, pos, out_min, out_max);
}
RX_FUNCTION(0x00845a70, phys_box_vf3)

/* 00848650 CompositeChildRotation (EDI composite, ECX child index; stack q*, out*; RET 8): out = q * child rotation (+0x44 array). */
static void child_rotation_kernel(rx_addr comp,uint32_t index,rx_addr q,rx_addr out,rx_v4*live){
 fx_frame f;
 if(live){
  fx_frame_enter(&f,8,16,FX_EBX,FX_ESI);
  fx_set_reg(FX_EAX,(uint32_t)((int32_t)(U32(comp,0x48)-U32(comp,0x44))>>4));
  fx_set_reg(FX_EBX,out);fx_set_reg(FX_ESI,index);
  if(index>=fx_reg(FX_EAX)){
   SU32(f.E-4,0,0x848673);rx_set_sp(f.E-4);crt_invalid_parameter_noinfo();abort();
  }
  fx_set_reg(FX_ECX,q);fx_set_reg(FX_ESI,(index<<4)+U32(comp,0x44));
 }
 const rx_v4 result=coll_quat_mul_signs_first_observed(v4_ld(q),v4_ld(U32(comp,0x44)+index*16),live);
 /* No guest scalar spills added to ordinaryC callers. Original popsESI
  * before storingresult, thenEBX/EBP. No dependency can intervene here. */
 if(live)fx_set_reg(FX_ESI,U32(f.E,0));
 v4_st(out,result);
 if(live){fx_set_reg(FX_EBX,U32(f.E,4));fx_set_reg(FX_EBP,U32(f.ebp,0));rx_set_sp(f.sp0);}
}
void phys_composite_child_rotation(rx_addr comp,uint32_t index,rx_addr q,rx_addr out){child_rotation_kernel(comp,index,q,out,0);}
void phys_composite_child_rotation_observed(rx_addr comp,uint32_t index,rx_addr q,rx_addr out,rx_v4*live){if(!live)abort();child_rotation_kernel(comp,index,q,out,live);}
RX_FUNCTION(0x00848650, phys_composite_child_rotation)

/* 008486f0 CompositeChildPosition (EDI composite, ECX child index; stack q*, pos*, out*; RET 0xc): the child's world position
 * q * offset * conj(q) + pos, offset = child offset (+0x5c array) with w cleared. */
static void child_position_kernel(rx_addr comp,uint32_t index,rx_addr q,rx_addr pos,rx_addr out,rx_v4*live){
 fx_frame f;
 if(live){
  fx_frame_enter(&f,8,16,FX_EBX,FX_ESI);
  fx_set_reg(FX_EAX,(uint32_t)((int32_t)(U32(comp,0x60)-U32(comp,0x5c))>>4));
  fx_set_reg(FX_EBX,out);fx_set_reg(FX_ESI,index);
  if(index>=fx_reg(FX_EAX)){
   SU32(f.E-4,0,0x848713);rx_set_sp(f.E-4);crt_invalid_parameter_noinfo();abort();
  }
  fx_set_reg(FX_ECX,q);fx_set_reg(FX_ESI,(index<<4)+U32(comp,0x5c));fx_set_reg(FX_EDX,pos);
 }
 const rx_v4 qv=v4_ld(q),offset=v4_xyz(v4_ld(U32(comp,0x5c)+index*16));
 const rx_v4 rotated=coll_quat_mul(qv,coll_quat_mul(offset,v4_mul(COLL_QCONJ,qv)));
 const rx_v4 translation=v4_ld(pos); /* actual finalXMM0 load8487e4 */
 const rx_v4 result=v4_add(rotated,translation);
 if(live){*live=translation;fx_set_reg(FX_ESI,U32(f.E,0));}
 v4_st(out,result);
 if(live){fx_set_reg(FX_EBX,U32(f.E,4));fx_set_reg(FX_EBP,U32(f.ebp,0));rx_set_sp(f.sp0);}
}
void phys_composite_child_position(rx_addr comp,uint32_t index,rx_addr q,rx_addr pos,rx_addr out){child_position_kernel(comp,index,q,pos,out,0);}
void phys_composite_child_position_observed(rx_addr comp,uint32_t index,rx_addr q,rx_addr pos,rx_addr out,rx_v4*live){if(!live)abort();child_position_kernel(comp,index,q,pos,out,live);}
RX_FUNCTION(0x008486f0, phys_composite_child_position)

/* 00848340 CollisionShapeComposite::vf3 (thiscall: q*, pos*, out min*, out max*; RET 0x10): the union of the children's world
 * bounds: each child's vf3 (vtable slot 3) at its world rotation q * rotation_i and position q * offset_i * conj(q) + pos, over
 * (FLT_MAX, -FLT_MAX); nothing is written when there are no children. The original's frame (push ebp; and esp, -16; sub esp, 0x84;
 * push ebx / esi / edi) holds the child's arguments at +0x40 position, +0x50 min, +0x60 rotation, +0x70 max (from the ESP after
 * the pushes), where the child's own frame starts. */
void phys_composite_bounds(rx_addr self, rx_addr q_p, rx_addr pos_p, rx_addr out_min, rx_addr out_max) {
    const uint32_t count = (uint32_t)(I32(self, 0x30) - I32(self, 0x2c)) >> 2;
    if (count == 0) return;
    v4_st(out_min, v4_splat(rx_bf(0x7f7fffff)) /* FLT_MAX 0x977494 */);
    v4_st(out_max, v4_splat(rx_bf(0xff7fffff)) /* -FLT_MAX 0x977550 */);
    const rx_addr entry_sp = rx_sp();
    const rx_addr F = ((entry_sp - 4) & ~15u) - 0x84 - 12;
    rx_set_sp(F);
    const rx_addr pos = F + 0x40, mn = F + 0x50, rot = F + 0x60, mx = F + 0x70;
    for (uint32_t i = 0; i < count; i++) {
        const rx_v4 q = v4_ld(q_p);
        const rx_v4 offset = v4_xyz(v4_ld(U32(self, 0x5c) + i * 16));
        v4_st(pos, v4_add(coll_quat_mul(q, coll_quat_mul(offset, v4_mul(q, COLL_QCONJ))), v4_ld(pos_p)));
        v4_st(rot, coll_quat_mul(v4_ld(q_p), v4_ld(U32(self, 0x44) + i * 16)));
        const rx_addr child = U32(U32(self, 0x2c), i * 4);
        phys_shape_bounds(U32(U32(child, 0), 0xc), child, rot, pos, mn, mx);
        v4_st(out_min, v4_min(v4_ld(out_min), v4_ld(mn)));
        v4_st(out_max, v4_max(v4_ld(out_max), v4_ld(mx)));
    }
    rx_set_sp(entry_sp);
}
RX_FUNCTION(0x00848340, phys_composite_bounds)

/* 00848800 CompositeSetChildPosition (ESI composite, ECX world pos*, EDX body pos*; stack index, body q*; RET 8): the child's
 * offset (+0x5c array) = conj(q) * ((world - body) with w cleared) * q. */
void phys_composite_set_child_position(rx_addr comp, rx_addr world_pos, rx_addr body_pos, uint32_t index, rx_addr q_p) {
    const rx_v4 q = v4_ld(q_p);
    const rx_v4 delta = v4_xyz(v4_sub(v4_ld(world_pos), v4_ld(body_pos)));
    const rx_v4 r = coll_quat_mul(v4_mul(COLL_QCONJ, q), coll_quat_mul(delta, q));
    v4_st(U32(comp, 0x5c) + index * 16, r);
}
RX_FUNCTION(0x00848800, phys_composite_set_child_position)

/* 00848910 CompositeSetChildRotation (ESI composite, EDX child index, ECX world rot*; stack body q*; RET 4): the child's rotation
 * (+0x44 array) = (q * conj) * world rotation. */
void phys_composite_set_child_rotation(rx_addr comp, rx_addr world_rot, uint32_t index, rx_addr q_p) {
    const rx_v4 r = coll_quat_mul_signs_first(v4_mul(v4_ld(q_p), COLL_QCONJ), v4_ld(world_rot));
    v4_st(U32(comp, 0x44) + index * 16, r);
}
RX_FUNCTION(0x00848910, phys_composite_set_child_rotation)

/* 00845a80 CollisionShapeBox_Corners (thiscall: out[8] vec4*; RET 4): the eight local corners; with the half extents h (this+0x20,
 * read again for each) and the sign patterns A = (-1,1,1,1), B = (1,-1,1,1), C = (1,1,-1,-1): h, C*h, B*h, (B*h)*C, A*h,
 * (A*h)*C, (A*h)*B, ((A*h)*B)*C (-1 the runtime global 0x9cd9e8). */
void phys_box_corners(rx_addr self, rx_addr out) {
    const float one = 1.0f /* 0x931f24 */, m1 = F32(0, 0x9cd9e8);
    const rx_v4 A = v4(m1, one, one, one), B = v4(one, m1, one, one), C = v4(one, one, m1, m1);
    v4_st(out, v4_ld(self + 0x20));
    v4_st(out + 0x10, v4_mul(C, v4_ld(self + 0x20)));
    v4_st(out + 0x20, v4_mul(B, v4_ld(self + 0x20)));
    v4_st(out + 0x30, v4_mul(v4_mul(B, v4_ld(self + 0x20)), C));
    v4_st(out + 0x40, v4_mul(A, v4_ld(self + 0x20)));
    v4_st(out + 0x50, v4_mul(v4_mul(A, v4_ld(self + 0x20)), C));
    v4_st(out + 0x60, v4_mul(v4_mul(A, v4_ld(self + 0x20)), B));
    v4_st(out + 0x70, v4_mul(v4_mul(v4_mul(A, v4_ld(self + 0x20)), B), C));
}
RX_FUNCTION(0x00845a80, phys_box_corners)

/* The frame of the two contains-point tests (push ebp; and esp, -16; sub esp, 0x10): each lane-mask vector is stored whole at [esp] to
 * sum its three low lanes, so the frame keeps the last mask tested, its w lane included (a compare of the dead w lanes, or the splat's
 * fourth copy). Later code reads that stack uninitialized: in a composite pair's child loop the mask 00845f20 leaves under a
 * BoxVsCylinder's 00845b50 becomes, through 00846f10's never-written frame words, the record normal w of the CylinderVsCylinder
 * 0086bd80 after it (core/notes/phys.md "The contains-point masks"). The saved EBP below the return address is not modelled. */
static void contains_frame(rx_v4 mask) { v4_st(((rx_sp() - 4) & ~15u) - 0x10, mask); }

/* 00845f20 CollisionShapeBox_ContainsPoint (ECX point*, stack box*; RET 4): 1 when p <= h and -h <= p on x, y and z (h = box+0x20;
 * cmpleps with the three lane masks summed as -3), else 0. */
uint32_t phys_box_contains_point(rx_addr point, rx_addr box) {
    const rx_v4 h = v4_ld(box + 0x20), p = v4_ld(point);
    const rx_v4 a = v4_cmple(p, h);
    contains_frame(a);
    const uint32_t sum = v4_lb(a, 2) + v4_lb(a, 1) + v4_lb(a, 0);
    if (sum != 0xfffffffdu) return 0;
    const rx_v4 b = v4_cmple(v4_sub(v4_zero(), h), p);
    contains_frame(b);
    return v4_lb(b, 2) + v4_lb(b, 1) + v4_lb(b, 0) == sum;
}
RX_FUNCTION(0x00845f20, phys_box_contains_point)

/* 00846bb0 CollisionShapeCylinder_ContainsPoint (ECX point*, stack cylinder*; RET 4): 1 when x^2 + z^2 < r^2 and -h/2 < y < h/2
 * (radius +0x20, height +0x24; cmpltps, the lane masks summed as -3), else 0. */
uint32_t phys_cylinder_contains_point(rx_addr point, rx_addr cyl) {
    const rx_v4 p = v4_ld(point);
    const float r = F32(cyl, 0x20), half_h = phys_mul(F32(cyl, 0x24), 0.5f /* 0x9322cc */);
    const rx_v4 d2 = v4_add(v4_mul(v4_lane(p, 2), v4_lane(p, 2)), v4_mul(v4_lane(p, 0), v4_lane(p, 0)));
    const rx_v4 a = v4_cmplt(d2, v4_splat(phys_mul(r, r)));
    contains_frame(a);
    const uint32_t sum = v4_lb(a, 2) + v4_lb(a, 1) + v4_lb(a, 0);
    if (sum != 0xfffffffdu) return 0;
    const rx_v4 hh = v4_splat(half_h), y = v4_lane(p, 1);
    const rx_v4 b = v4_cmplt(y, hh);
    contains_frame(b);
    if (v4_lb(b, 2) + v4_lb(b, 1) + v4_lb(b, 0) != sum) return 0;
    const rx_v4 c = v4_cmplt(v4_sub(v4_zero(), hh), y);
    contains_frame(c);
    return v4_lb(c, 2) + v4_lb(c, 1) + v4_lb(c, 0) == 0xfffffffdu;
}
RX_FUNCTION(0x00846bb0, phys_cylinder_contains_point)

/* 00843fc0 CollisionShapeSphere::vf4 (thiscall, stack scale; RET 4): saves the radius vector (+0x20 to +0x30), then scales it. */
void phys_sphere_scale(rx_addr self, float scale) {
    v4_st(self + 0x30, v4_ld(self + 0x20));
    v4_st(self + 0x20, v4_mul(v4_splat(scale), v4_ld(self + 0x20)));
}
RX_FUNCTION(0x00843fc0, phys_sphere_scale)

/* 00843ff0 CollisionShapeSphere::vf5 (thiscall): restores the radius vector (+0x30 to +0x20). */
void phys_sphere_restore(rx_addr self) { v4_st(self + 0x20, v4_ld(self + 0x30)); }
RX_FUNCTION(0x00843ff0, phys_sphere_restore)

/* 00844050 CollisionShapeDynamicCylinder::vf4 (thiscall, stack scale; RET 4): saves radius (+0x20) / height (+0x24) to the
 * saved slots +0x28 / +0x2c, then scales both (movss / mulss; EAX untouched). */
void phys_dyncyl_scale(rx_addr self, float scale) {
    const float r = F32(self, 0x20), h = F32(self, 0x24);
    SF32(self, 0x28, r);
    SF32(self, 0x2c, h);
    SF32(self, 0x20, phys_mul(r, scale));
    SF32(self, 0x24, phys_mul(h, scale));
}
RX_FUNCTION(0x00844050, phys_dyncyl_scale)

/* 00844080 CollisionShapeDynamicCylinder::vf5 (thiscall): restores radius / height from +0x28 / +0x2c through the x87
 * (fld m32 / fstp m32: a signalling NaN comes back quiet). */
void phys_dyncyl_restore(rx_addr self) {
    uint32_t r = U32(self, 0x28), h = U32(self, 0x2c);
    if ((r & 0x7f800000u) == 0x7f800000u && (r & 0x7fffffu)) r |= 0x400000u;
    if ((h & 0x7f800000u) == 0x7f800000u && (h & 0x7fffffu)) h |= 0x400000u;
    SU32(self, 0x20, r);
    SU32(self, 0x24, h);
}
RX_FUNCTION(0x00844080, phys_dyncyl_restore)

/* 008454d0 CollisionShape_ScaleToRadius (EDX shape, ECX direction out*, stack v* in / out; RET 4): v = r * v / (|v| + 1e-6) with
 * r = the shape's vector +0x20 (|v| = sqrt((x^2 + z^2) + y^2)), stored back to v; the direction v / |v| goes to *out, quiet NaN
 * where |v| is infinite or zero. */
void phys_shape_scale_to_radius(rx_addr shape, rx_addr dir_out, rx_addr vp) {
    const rx_v4 sq = v4_mul(v4_ld(vp), v4_ld(vp));
    const float length = phys_sqrt(phys_add(phys_add(sq.v[0], sq.v[2]), sq.v[1]));
    const rx_v4 s = v4_mul(v4_div(v4_ld(shape + 0x20), v4_splat(phys_add(length, 1e-6f /* 0x9777c4 */))), v4_ld(vp));
    const rx_v4 s2 = v4_mul(s, s);
    const float len2 = phys_add(phys_add(s2.v[0], s2.v[1]), s2.v[2]);
    const float len = phys_sqrt(len2);
    v4_st(vp, s);
    const rx_v4 dir = v4_div(s, v4_splat(len));
    const rx_v4 finite = v4_xor(v4_cmpeq(v4_splat(len2), v4_splat(rx_bf(0x7f800000))), v4_bits(~0u, ~0u, ~0u, ~0u));   /* len2 != +inf */
    const rx_v4 nonzero = v4_xor(v4_cmpeq(v4_splat(len), v4_zero()), v4_bits(~0u, ~0u, ~0u, ~0u));                      /* len != 0 */
    const rx_v4 qnan = v4_splat(rx_bf(0x7fc00000)) /* 0x92dc30 */;
    v4_st(dir_out, v4_or(v4_and(v4_and(dir, nonzero), finite), v4_andn(finite, qnan)));
}
RX_FUNCTION(0x008454d0, phys_shape_scale_to_radius)

/* 008439f0 XenonCore::vf2 (thiscall): returns this+0x34 (a field read, in EAX). */
uint32_t phys_xenon_core_vf2(rx_addr self) { return U32(self, 0x34); }
RX_FUNCTION(0x008439f0, phys_xenon_core_vf2)

/* 008454c0 (EAX shape): the shape's vec4 at +0x20 (radius / half extents) in xmm0. */
rx_v4 phys_shape_vec20(rx_addr shape) { return v4_ld(shape + 0x20); }
RX_FUNCTION(0x008454c0, phys_shape_vec20)

/* 008448e0 CollisionShapeHFTerrain::vf2 (= its vf3; thiscall: q*, pos*, out min*, out max*; RET 0x10): the height field has no finite
 * box: max = (FLT_MAX x4, 0x977494) stored first, then min = (-FLT_MAX x4, 0x977550). w-vtunported2 (a composite child's bounds). */
void phys_hfterrain_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max) {
    (void)self; (void)q; (void)pos;
    v4_st(out_max, v4_splat(F32(0, 0x00977494)));
    v4_st(out_min, v4_splat(F32(0, 0x00977550)));
}
RX_FUNCTION(0x008448e0, phys_hfterrain_bounds)

/* 00845100 CollisionShapeDynamicRays::vf2 (= its vf3; thiscall: q*, pos*, out min*, out max*; RET 0x10): the world box of the rays
 * (+0x20 count, +0x24 array of 32-byte rays: start vec4, end vec4). No ray: min = max = the vec4 at 0xd61670. Else the rotation rows
 * of q (x2 = x * 2 ... with 2 = 0x92e330, 1 = 0x931f24):
 *   r0 = (1 - (zz + yy), xy + wz, xz - wy, 0), r1 = (xy - wz, 1 - (zz + xx), yz + wx, 0), r2 = (xz + wy, yz - wx, 1 - (yy + xx), 0),
 *   P = (pos.xyz, 1); a point p maps to (p.z * r2) + ((p.y * r1) + (p.x * r0)) + P (the operand order of the original's addps).
 * The first ray sets min / max = minps / maxps(start, end); each further ray folds min(min(min, start), end), max likewise. */
static rx_v4 rays_point(rx_v4 p, rx_v4 r0, rx_v4 r1, rx_v4 r2, rx_v4 P) {
    const rx_v4 xy = v4_add(v4_mul(v4_lane(p, 1), r1), v4_mul(v4_lane(p, 0), r0));
    return v4_add(v4_add(v4_mul(v4_lane(p, 2), r2), xy), P);
}
void phys_dynrays_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max) {
    const uint32_t n = U32(self, 0x20);
    if (n == 0) {
        v4_st(out_min, v4_ld(0x00d61670));
        v4_st(out_max, v4_ld(0x00d61670));
        return;
    }
    const float two = F32(0, 0x0092e330), one = F32(0, 0x00931f24);
    const float x = F32(q, 0), y = F32(q, 4), z = F32(q, 8), w = F32(q, 0xc);
    const float x2 = phys_mul(x, two), y2 = phys_mul(y, two), z2 = phys_mul(z, two);
    const float zz = phys_mul(z, z2), wx = phys_mul(w, x2), xx = phys_mul(x, x2), wz = phys_mul(w, z2), wy = phys_mul(w, y2);
    const float xy = phys_mul(y, x2), yy = phys_mul(y, y2), yz = phys_mul(z, y2), xz = phys_mul(z, x2);
    const rx_v4 r0 = v4(phys_sub(one, phys_add(zz, yy)), phys_add(xy, wz), phys_sub(xz, wy), 0.0f);
    const rx_v4 r1 = v4(phys_sub(xy, wz), phys_sub(one, phys_add(zz, xx)), phys_add(yz, wx), 0.0f);
    const rx_v4 r2 = v4(phys_add(xz, wy), phys_sub(yz, wx), phys_sub(one, phys_add(yy, xx)), 0.0f);
    const rx_v4 P = v4(F32(pos, 0), F32(pos, 4), F32(pos, 8), one);
    const rx_addr rays = U32(self, 0x24);
    const rx_v4 s = rays_point(v4_ld(rays), r0, r1, r2, P), e = rays_point(v4_ld(rays + 0x10), r0, r1, r2, P);
    v4_st(out_min, v4_min(s, e));
    v4_st(out_max, v4_max(s, e));
    for (int32_t i = 1; i < (int32_t)n; i++) {           /* cmp ebx, 1; jle: a signed count */
        const rx_addr ray = U32(self, 0x24) + 0x20u * (uint32_t)i;
        const rx_v4 a = rays_point(v4_ld(ray), r0, r1, r2, P), b = rays_point(v4_ld(ray + 0x10), r0, r1, r2, P);
        v4_st(out_min, v4_min(v4_min(v4_ld(out_min), a), b));
        v4_st(out_max, v4_max(v4_max(v4_ld(out_max), a), b));
    }
}
RX_FUNCTION(0x00845100, phys_dynrays_bounds)
