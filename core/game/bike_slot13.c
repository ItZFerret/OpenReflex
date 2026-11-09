/* bike_slot13.c -- slot 13 of the motocross simulation, the parts after the contact queries (native/game/notes/bike.md). */
#include "bike_slot13.h"
#include "bike_contact.h"
#include "phys_mech.h"
#include "phys_coll_terrain.h"
#include "phys_quat.h"
#include "phys_contact.h"
#include "rx_vcall.h"
#include "fx_util.h"
#include "wheel_triangle_observed.h"
#include "body_probe_observed.h"
#include <stdlib.h>

#define G_AXIS_X      0x00d615c0
#define G_W_SOURCE    0x00d61660   /* its w lane becomes the w lane of the wheel spin used for the rolling step */

/* The six filtered contact-point coordinates S+8e40.. (pointers to {value, time, factor}, inline 0042d6b0 with a direct
 * division: factor = min(time, dt) / time): returns the new value. */
static float roll_filter(rx_addr S, int i, float target, float dt) {
    rx_addr F = U32(S, 0x8e40 + 4u * (uint32_t)i);
    float T = F32(F, 4), x = T > dt ? dt : T;
    float k = phys_div(x, T), old = F32(F, 0);
    float v = phys_add(phys_mul(phys_sub(target, old), k), old);
    SF32(F, 8, k); SF32(F, 0, v);
    return v;
}
static void roll_node_update(rx_addr node) {                  /* the wheel node's slot 6 (vtable +0x18), thiscall */
    rx_vcall(node, 0x18, 0, NULL);
}
/* One wheel rolling on the ground: the spin about the axle from the contact point's speed along the rolling direction
 * (vel . rolling axis * X / radius), integrated into the wheel orientation: q = normalize(q0 * spin * dt / 2 + q0). */
static void roll_wheel(rx_addr S, rx_v4 vel, rx_addr axis, rx_addr radius, rx_addr spin, rx_addr q_prev, rx_addr q_out, float dt) {
    const float c = bike_dot3(vel, rx_ld4(axis));
    rx_v4 w = rx_ld4(spin);
    w.v[0] = v4_div(v4_mul(v4_splat(c), rx_ld4(G_AXIS_X)), rx_ld4(radius)).v[0];
    w.v[3] = rx_ld4(G_W_SOURCE).v[3];
    const rx_v4 q0 = rx_ld4(q_prev);
    rx_st4(q_out, v4_add(v4_mul(bike_quat_mul(q0, w), v4_splat(phys_mul(dt, 0.5f))), q0));
    rx_st4(q_out, bike_qnormalize(rx_ld4(q_out)));
    (void)S;
}

/* 00445800 BikeWheels_UpdateRollingOrientation (ESI = S, stack dt; ret 4): the six contact-point filters over the rider body
 * position S+230 and the rear wheel position S+2d0, their velocities; while a wheel touches (S+89e0 / S+8890) with load
 * (S+89a0 / S+8850) and the bike is not resting, its orientation S+870 / S+910 rolls with the ground speed (front: velocity
 * of S+230 along S+8a00, radius S+88f0, spin S+880, previous S+8f0; rear: S+2d0 along S+88b0, S+87a0, S+920, S+990), then
 * the wheel node updates; resting or in a pose override of mode 2 both follow the chassis (S+6eb0 / S+8b30, S+8b20). */
void bike_wheels_update_rolling_orientation(rx_addr S, float dt) {
    float p0[3], p1[3];
    for (int i = 0; i < 3; i++) p0[i] = F32(U32(S, 0x8e40 + 4u * (uint32_t)i), 0);
    {
        const rx_v4 a = rx_ld4(S + 0x230);
        float n0 = roll_filter(S, 0, a.v[0], dt), n1 = roll_filter(S, 1, a.v[1], dt), n2 = roll_filter(S, 2, a.v[2], dt);
        p0[0] = phys_div(phys_sub(n0, p0[0]), dt); p0[1] = phys_div(phys_sub(n1, p0[1]), dt); p0[2] = phys_div(phys_sub(n2, p0[2]), dt);
    }
    for (int i = 0; i < 3; i++) p1[i] = F32(U32(S, 0x8e4c + 4u * (uint32_t)i), 0);
    {
        const rx_v4 b = rx_ld4(S + 0x2d0);
        float n0 = roll_filter(S, 3, b.v[0], dt), n1 = roll_filter(S, 4, b.v[1], dt), n2 = roll_filter(S, 5, b.v[2], dt);
        p1[0] = phys_div(phys_sub(n0, p1[0]), dt); p1[1] = phys_div(phys_sub(n1, p1[1]), dt); p1[2] = phys_div(phys_sub(n2, p1[2]), dt);
    }
    if (U32(S, 0x90) != 0 || (U32(S, 0x2de0) != 0 && U32(S, 0x2de4) == 2)) {
        const rx_v4 q = rx_ld4(S + 0x730);
        rx_st4(S + 0x870, bike_quat_mul(bike_quat_mul(q, rx_ld4(S + 0x6eb0)), rx_ld4(S + 0x8b30)));
        rx_st4(S + 0x910, bike_quat_mul(q, rx_ld4(S + 0x8b20)));
        roll_node_update(U32(S, 0xd78));
        roll_node_update(U32(S, 0xd7c));
        return;
    }
    if (U32(S, 0x89e0) != 0 && U32(S, 0x89a0) != 0) {
        roll_wheel(S, v4(p0[0], p0[1], p0[2], 0.0f), S + 0x8a00, S + 0x88f0, S + 0x880, S + 0x8f0, S + 0x870, dt);
        roll_node_update(U32(S, 0xd78));
    }
    if (U32(S, 0x8890) != 0 && U32(S, 0x8850) != 0) {
        roll_wheel(S, v4(p1[0], p1[1], p1[2], 0.0f), S + 0x88b0, S + 0x87a0, S + 0x920, S + 0x990, S + 0x910, dt);
        roll_node_update(U32(S, 0xd7c));
    }
}
RX_FUNCTION(0x00445800, bike_wheels_update_rolling_orientation)

/* 0085c530 Mechanism_Rotate / 0085c500 Mechanism_Translate (phys): EDX = mechanism S+d0, ESI = &quaternion / &offset. */
static void s13_mech_call(rx_addr fn, rx_addr S, rx_v4 v) {
    const rx_addr sp = rx_sp(), F = (sp - 0x20) & ~15u;
    rx_st4(F, v);
    rx_set_sp(F);
    if (fn == 0x0085c530) phys_mech_rotate(S + 0xd0, F);                /* Mechanism_Rotate */
    else phys_mech_translate(S + 0xd0, F);                              /* 0085c500 Mechanism_Translate */
    rx_set_sp(sp);
}

/* 00446f80 (the resting pin, EDI = S; ret 0): while resting (S+90) the bike is turned so that the line between its wheel
 * anchors (front S+8970, rear S+8820 in the root frame, around S+230 / S+2d0) points along the saved contact points
 * S+8b00 -> S+8b10 (rotation about their cross product by the arccosine of their dot product, as a half-angle quaternion),
 * then translated so that the rear anchor lies on S+8b00. */
void bike_rest_pin(rx_addr S) {
    if (U32(S, 0x90) == 0) return;
    rx_v4 q = rx_ld4(S + 0x730);
    const rx_v4 front = v4_add(bike_rotate(q, rx_ld4(S + 0x8970)), rx_ld4(S + 0x230));
    const rx_v4 rear = v4_add(bike_rotate(q, rx_ld4(S + 0x8820)), rx_ld4(S + 0x2d0));
    const rx_v4 a = bike_normalize(v4_sub(front, rear));
    const rx_v4 b = bike_normalize(v4_sub(rx_ld4(S + 0x8b10), rx_ld4(S + 0x8b00)));
    const float c = bike_dot3(b, a);
    const rx_v4 axis = bike_normalize(bike_cross(a, b));
    float ang = bike_asin_approx(bike_fabs(c), c);
    ang = c >= 0.0f ? ang : phys_sub(-0.0f, ang);
    const float h = phys_mul((float)(1.5707963705062866 - (double)ang), 0.5f);     /* half of arccos(c) */
    const rx_v4 sv = v4_mul(v4_splat(bike_cos_poly2(phys_add(h, 1.5707963705062866f /* 00977508 */))), axis);
    const float w = phys_sub(-0.0f, bike_cos_poly2(h));
    s13_mech_call(0x0085c530, S, bike_qnormalize(v4(sv.v[0], sv.v[1], sv.v[2], w)));   /* Mechanism_Rotate */
    q = rx_ld4(S + 0x730);
    const rx_v4 rear2 = v4_add(bike_rotate(q, rx_ld4(S + 0x8820)), rx_ld4(S + 0x2d0));
    s13_mech_call(0x0085c500, S, v4_sub(rx_ld4(S + 0x8b00), rear2));                   /* Mechanism_Translate */
}
RX_FUNCTION(0x00446f80, bike_rest_pin)

/* Test-only boundary counter; no mathematical or ordinary-path change. */
#ifdef RX_BODY_COMPARE
extern void rx_body_c_enter(unsigned);
static rx_v4 body_counted_dependency(enum rx_body_probe_role role,rx_addr target,rx_addr expected,rx_v4 x0){rx_body_c_enter((unsigned)role);return rx_body_probe_dependency(role,target,expected,x0);}
#define rx_body_probe_dependency body_counted_dependency
#endif
/* One canonical BODY_PROBE kernel, optionally retaining actual original frames
 * and already-computed comparison masks/GPR. No observer repeats physics. */
static rx_v4 body_product(rx_v4 a,rx_v4 q){
 rx_v4 r=v4_shuf(q,q,0x1b);
 rx_v4 x=v4_mul(v4_mul(v4_lane(a,0),r),rx_ld4(0x93a1f0));r=v4_shuf(r,r,0xb1);
 rx_v4 y=v4_mul(v4_mul(v4_lane(a,1),r),rx_ld4(0x93a200));r=v4_shuf(r,r,0x1b);
 rx_v4 z=v4_mul(v4_mul(r,v4_lane(a,2)),rx_ld4(0x93a210));
 rx_v4 w=v4_mul(v4_lane(a,3),q);return v4_add(v4_add(z,y),v4_add(w,x));
}
/* Original signed multiply/high-shift divide80. Expose its real intermediate
 * quotient registers; do not replace unsigned caller comparisons with clamps. */
static uint32_t body_descriptor_count_difference(uint32_t difference,int observed,unsigned output_reg){
 int64_t product=(int64_t)(int32_t)difference*(int64_t)0x66666667;
 int32_t high=(int32_t)(product>>32),shifted=high>>5;
 uint32_t count=(uint32_t)shifted+((uint32_t)shifted>>31);
 if(observed){fx_set_reg(FX_ECX,difference);fx_set_reg(FX_EAX,(uint32_t)product);fx_set_reg(FX_EDX,(uint32_t)shifted);fx_set_reg(output_reg,count);}
 return count;
}
static uint32_t body_descriptor_count(rx_addr s,int observed,unsigned output_reg){
 uint32_t end=U32(s,0x1310),begin=U32(s,0x130c);
 return body_descriptor_count_difference(end-begin,observed,output_reg);
}
static rx_v4 body_bounds(rx_addr ra,rx_addr b,int observed,rx_v4 x0){
 if(!observed){crt_invalid_parameter_noinfo();return x0;}
 SU32(b-4,0,ra);rx_set_sp(b-4);return rx_body_probe_dependency(RX_BP_BOUNDS,U32(0,0x916224),b,x0);
}
static rx_addr body_current_world(int observed){
 if(!(U8(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}
 uint32_t i=0;if(observed)fx_set_reg(FX_EAX,0);
 if(U32(0,0x9e30dc)){
  rx_addr scene=U32(0,0xa95844);if(observed)fx_set_reg(FX_EDX,scene);
  do{
   rx_addr world=U32(0,0x9e30d4+4*i);if(observed)fx_set_reg(FX_ECX,world);
   if(world){uint32_t identity=U32(scene,0x1c+4*U32(scene,4));if(observed)fx_set_reg(FX_EBX,identity);
    if(identity==U32(world,0x44a8)){if(observed)fx_set_reg(FX_EAX,world);return world;}}
   ++i;if(observed)fx_set_reg(FX_EAX,i);
  }while(i<U32(0,0x9e30dc));
 }
 if(observed)fx_set_reg(FX_EAX,0);return 0;
}
static void body_probe_kernel(rx_addr s,int observed){
 const rx_addr entry=rx_sp(),b=((entry-4)&~15u)-0x730;fx_frame frame;
 if(observed){
  fx_frame_enter(&frame,0x728,16,FX_EBX,FX_EDI);
  SU32(b+0x72c,0,U32(0,0x9b45e4)^(b+8));
  fx_set_reg(FX_EAX,U32(s,0));fx_set_reg(FX_EDX,U32(U32(s,0),0x80));fx_set_reg(FX_ECX,s);
  SU32(b-4,0,0x435b28);rx_set_sp(b-4);(void)rx_body_probe_dependency(RX_BP_PROBES,fx_reg(FX_EDX),b,v4_zero());
 }else{rx_set_sp(b);rx_vcall(s,0x80,0,NULL);}
 const rx_v4 position=rx_ld4(s+0xf0),root=rx_ld4(s+0x730);rx_v4 live_x0=position;
 rx_st4(b+0x60,position);rx_st4(b+0x50,root);
 if(observed){fx_set_reg(FX_EDX,0);fx_set_reg(FX_ECX,0);}
 for(int32_t i=0;i<I32(s,0x1318);++i){
  SU32(s,0x1af0+4u*(uint32_t)i,0);SU32(s,0x1b54+4u*(uint32_t)i,0);
  if(observed){fx_set_reg(FX_ECX,(uint32_t)i+1);fx_set_reg(FX_EAX,s+0x1b54+4u*((uint32_t)i+1));}
 }
 if(observed)SU32(b+0x10,0,0);
 if(I32(s,0x1318)>0&&observed){SU32(b+0xc,0,s+0x1bb8);fx_set_reg(FX_EAX,s+0x1bb8);fx_set_reg(FX_EDI,s+0x1320);}
 for(int32_t i=0;i<I32(s,0x1318);++i){
  const rx_addr p=s+0x1320+16u*(uint32_t)i;
  /*435b95..435c70: same products, actual runtime tables and operand order. */
  rx_v4 local=v4_and(rx_ld4(p+0x190),rx_ld4(0x92db80));
  rx_v4 inner=body_product(local,v4_mul(root,rx_ld4(0x93a220)));
  rx_v4 world=v4_add(body_product(root,inner),position);rx_st4(p,world);rx_st4(b+0x20,rx_ld4(p));
  rx_addr terrain=body_current_world(observed);
  if(observed){fx_set_reg(FX_ECX,b+0x30);SU32(b-4,0,b+0x20);SU32(b-8,0,terrain);SU32(b-12,0,0x435cd8);rx_set_sp(b-12);(void)rx_body_probe_dependency(RX_BP_TERRAIN,0x7c5e80,b,world);}
  else PHYS_CALL_SP(8,(void)phys_terrain_height_normal(b+0x30,terrain,b+0x20));
  rx_st4(p+0x320,rx_ld4(b+0x20));rx_st4(p+0x640,rx_ld4(b+0x30));
  rx_v4 correction=v4_sub(rx_ld4(p+0x320),rx_ld4(p));rx_st4(p+0x4b0,correction);
  live_x0=v4_lane(correction,1);rx_v4 mask=v4_cmplt(rx_ld4(0xd61670),live_x0);uint32_t sum=v4_lb(mask,2)+v4_lb(mask,1)+v4_lb(mask,0);
  if(observed){rx_st4(b+0x40,mask);fx_set_reg(FX_EDX,v4_lb(mask,1));fx_set_reg(FX_EAX,U32(b+0xc,0));}
  if(sum==0xfffffffdu){
   SU32(s,0x1bb8+4u*(uint32_t)i,U32(s,0x1bb8+4u*(uint32_t)i)|1);SU32(s,0x1af0+4u*(uint32_t)i,1);
   rx_v4 normal=rx_ld4(p+0x640),terms=v4_mul(rx_ld4(p+0x4b0),normal);
   float dot=phys_add(phys_add(terms.v[0],terms.v[1]),terms.v[2]);live_x0=terms;live_x0.v[0]=dot;rx_st4(p+0x4b0,v4_mul(v4_splat(dot),normal));
  }
  if(observed){fx_set_reg(FX_ECX,(uint32_t)i+1);fx_set_reg(FX_EAX,s+0x1bb8+4u*((uint32_t)i+1));fx_set_reg(FX_EDI,p+0x10);SU32(b+0x10,0,(uint32_t)i+1);SU32(b+0xc,0,fx_reg(FX_EAX));}
 }
 if(observed){fx_set_reg(FX_EBX,0);fx_set_reg(FX_EAX,0);}
 SU32(s,0x20f8,0);
 for(unsigned j=0;j<30;++j){SU32(b,0x620+4*j,0xffffffffu);SU32(b,0x698+4*j,0xffffffffu);}
 if(observed){fx_set_reg(FX_ECX,0xffffffffu);fx_set_reg(FX_EAX,30);}
 uint32_t count=body_descriptor_count(s,observed,FX_ECX);
 if(count&&observed){fx_set_reg(FX_EAX,0);SU32(b+0xc,0,0);}
 for(uint32_t descriptor=0;descriptor<count;){
  SU32(b+0x710,0,0);SU32(b+0x70,0,0);
  for(unsigned j=0;j<30;++j){SU32(b,0x620+4*j,0xffffffffu);SU32(b,0x698+4*j,0xffffffffu);}
  if(observed){fx_set_reg(FX_ECX,0xffffffffu);fx_set_reg(FX_EAX,30);}
  uint32_t offset=descriptor*0x50u; /* Observed loads occur at the real post-bounds sites below. */
  uint32_t n=body_descriptor_count(s,observed,FX_EAX);if(descriptor>=n)live_x0=body_bounds(0x435e3e,b,observed,live_x0);
  rx_addr record_base=U32(s,0x130c);if(observed)offset=U32(b+0xc,0);
  rx_addr record=record_base+offset;if(observed){fx_set_reg(FX_EAX,record);SU32(b+0x10,0,record);}
  n=body_descriptor_count(s,observed,FX_ECX);if(descriptor>=n)live_x0=body_bounds(0x435e73,b,observed,live_x0);
  rx_addr shape_base=U32(s,0x130c);if(observed)offset=U32(b+0xc,0);
  rx_addr shape_record=shape_base+offset;if(observed)fx_set_reg(FX_EDI,shape_record);
  n=body_descriptor_count(s,observed,FX_EAX);if(descriptor>=n)live_x0=body_bounds(0x435ea4,b,observed,live_x0);
  /* Real pending-push register values, two record reads intentionally retained. */
  if(observed){record=U32(b+0x10,0);offset=U32(b+0xc,0);}
  rx_addr shape=U32(U32(U32(s,0x130c)+offset,0x44),4);
  if(observed){
   SU32(b-4,0,0);SU32(b-8,0,b+0x60);SU32(b-12,0,b+0x50);SU32(b-16,0,s+0x20d0);SU32(b-20,0,record);SU32(b-24,0,shape_record+0x10);SU32(b-28,0,shape);
   fx_set_reg(FX_EAX,U32(U32(s,0x130c)+offset,0x44));fx_set_reg(FX_EDX,offset);fx_set_reg(FX_EDI,shape_record+0x10);fx_set_reg(FX_ECX,b+0x70);
   SU32(b-32,0,0x435ede);rx_set_sp(b-32);live_x0=rx_body_probe_dependency(RX_BP_COLLISION,0x85ddd0,b,live_x0);
  }else PHYS_CALL_SP(28,phys_contact_dispatch(b+0x70,shape,shape_record+0x10,record,s+0x20d0,b+0x50,b+0x60,0));
  int32_t contacts=I32(b+0x70,0);if(observed)fx_set_reg(FX_ECX,(uint32_t)contacts);
  if(contacts>0&&observed){SU32(b+0x10,0,0);fx_set_reg(FX_EDI,b+0xa0);}
  for(int32_t j=0;j<contacts;){
   int32_t current=I32(s,0x20f8);if(observed)fx_set_reg(FX_EAX,(uint32_t)current);
   if(current<12){
    rx_addr contact=observed?fx_reg(FX_EDI)-0x20:b+0x80+0x30u*(uint32_t)j;
    rx_v4 normal=rx_ld4(contact+0x10),depth=rx_ld4(contact+0x20),point=rx_ld4(contact);
    live_x0=v4_mul(depth,normal);rx_st4(s+0x21c0+16u*(uint32_t)current,live_x0);
    if(observed)fx_set_reg(FX_EAX,0x21c0+16u*(uint32_t)current);
    current=I32(s,0x20f8);rx_st4(s+0x2100+16u*(uint32_t)current,point);if(observed)fx_set_reg(FX_EDX,0x2100+16u*(uint32_t)current);
    current=I32(s,0x20f8);rx_st4(s+0x2280+16u*(uint32_t)current,normal);if(observed)fx_set_reg(FX_EAX,0x2280+16u*(uint32_t)current);
    n=body_descriptor_count(s,observed,FX_ECX);if(descriptor>=n)live_x0=body_bounds(0x435f75,b,observed,live_x0);
    rx_addr surface_base=U32(s,0x130c);uint32_t surface_offset=observed?U32(b+0xc,0):offset;
    uint32_t surface=U32(surface_base+surface_offset,0x40);current=I32(s,0x20f8);SU32(s,0x2340+4u*(uint32_t)current,surface);SU32(s,0x20f8,(uint32_t)current+1);
    if(observed){fx_set_reg(FX_EDX,surface);fx_set_reg(FX_ECX,U32(b+0xc,0));fx_set_reg(FX_EAX,(uint32_t)current);fx_set_reg(FX_ECX,U32(b+0x70,0));}
    contacts=I32(b+0x70,0);
   }
   if(observed){uint32_t next=U32(b+0x10,0)+1;fx_set_reg(FX_EAX,next);fx_set_reg(FX_EDI,fx_reg(FX_EDI)+0x30);SU32(b+0x10,0,next);j=(int32_t)next;}else ++j;
  }
  uint32_t count_end=U32(s,0x1310),count_begin=U32(s,0x130c);
  if(observed)SU32(b+0xc,0,U32(b+0xc,0)+0x50);
  count=body_descriptor_count_difference(count_end-count_begin,observed,FX_EAX);++descriptor;if(observed)fx_set_reg(FX_EBX,descriptor);
 }
 /*436004 loop deliberately re-reads real counts; neither count is clamped. */
 if(observed)SU32(b+0xc,0,0);
 rx_v4 limit=v4_splat(F32(0,0x935aa8));
 for(int32_t c=0;c<I32(s,0x20f8);++c){
  if(observed){SU32(b+0x10,0,s+0x2100+16u*(uint32_t)c);fx_set_reg(FX_ECX,0);}
  if(I32(s,0x1318)>0&&observed){fx_set_reg(FX_EDX,s+0x1b54);fx_set_reg(FX_EAX,s+0x1320);}
  for(int32_t k=0;k<I32(s,0x1318);++k){
   rx_addr point=s+0x2100+16u*(uint32_t)c,probe=s+0x1320+16u*(uint32_t)k;
   rx_v4 d=v4_sub(rx_ld4(point),rx_ld4(probe)),terms=v4_mul(d,d);
   rx_v4 length=v4_sqrt(v4_splat(phys_add(phys_add(terms.v[0],terms.v[2]),terms.v[1]))),mask=v4_cmplt(length,limit);
   live_x0=mask;uint32_t sum=v4_lb(mask,2)+v4_lb(mask,1)+v4_lb(mask,0);
   if(observed){fx_set_reg(FX_EDI,point);rx_st4(b+0x40,mask);fx_set_reg(FX_EDI,v4_lb(mask,1));fx_set_reg(FX_EBX,sum);}
   if(sum==0xfffffffdu)SU32(s,0x1b54+4u*(uint32_t)k,1);
   if(observed){fx_set_reg(FX_ECX,(uint32_t)k+1);fx_set_reg(FX_EAX,probe+0x10);fx_set_reg(FX_EDX,s+0x1b54+4u*((uint32_t)k+1));}
  }
  if(observed){fx_set_reg(FX_EAX,(uint32_t)c+1);SU32(b+0x10,0,s+0x2100+16u*((uint32_t)c+1));SU32(b+0xc,0,(uint32_t)c+1);}
 }
 if(observed){
  fx_set_reg(FX_ECX,U32(b+0x72c,0)^(b+8));fx_set_reg(FX_EDI,frame.in.edi);fx_set_reg(FX_EBX,frame.in.ebx);
  SU32(b+4,0,0x4360a2);rx_set_sp(b+4);(void)rx_body_probe_dependency(RX_BP_COOKIE,0x8f9efe,b+8,live_x0);fx_frame_leave(&frame);
 }else rx_set_sp(entry);
}
void bike_body_probe_contacts(rx_addr s){body_probe_kernel(s,0);}
void bike_body_probe_contacts_observed(rx_addr s){body_probe_kernel(s,1);}
RX_FUNCTION(0x00435b00,bike_body_probe_contacts)

/* One wheel's object contacts (inline in 00444b90): the dynamic-object descriptors whose boxes (+20 min, +30 max, grown by the
 * wheel extent W+10) contain the wheel body position are tested with 0085ddd0 (contacts into S+80d0); the contact points /
 * normals (and the terrain contact W+50 / W+40 when W+fc) are averaged into the wheel record by 0042db20. F = the original
 * frame (locals: normal sum F+30, point sum F+50, root orientation F+60, wheel body position F+70 / orientation F+80). */
static void s13_wheel_objects(rx_addr S, rx_addr W, rx_addr surface_at, rx_addr F) {
    const rx_addr B = S + 0xa0u * U32(W, 0);
    rx_st4(F + 0x80, rx_ld4(B + 0x730));
    rx_st4(F + 0x60, rx_ld4(S + 0x730));
    rx_st4(F + 0x70, rx_ld4(B + 0xf0));
    rx_st4(S + 0x8e30, rx_ld4(W + 0x10));
    int32_t count = 0;
    rx_st4(F + 0x50, rx_ld4(0x00d61670)); rx_st4(F + 0x30, rx_ld4(0x00d61670));
    bike_wheel_query_terrain_contact(W, S);
    rx_v4 sum_n = rx_ld4(F + 0x30);
    if (U32(W, 0xfc) != 0) { rx_st4(F + 0x50, rx_ld4(W + 0x50)); sum_n = rx_ld4(W + 0x40); count = 1; }
    SU32(surface_at, 0, 0);
    for (uint32_t i = 0; i < (uint32_t)((I32(S, 0x1310) - I32(S, 0x130c)) / 0x50); i++) {
        const rx_addr desc = U32(S, 0x130c) + 0x50 * i;
        const rx_v4 ext = rx_ld4(W + 0x10), pos = rx_ld4(F + 0x70);
        const rx_v4 lo = v4_sub(rx_ld4(desc + 0x20), ext), hi = v4_add(rx_ld4(desc + 0x30), ext);
        if (!bike_all3(v4_cmplt(pos, hi)) || !bike_all3(v4_cmplt(lo, pos))) continue;
        SU32(S, 0x8770, 0); SU32(S, 0x80d0, 0);
        for (int j = 0; j < 30; j++) { SU32(S, 0x8680 + 4 * j, 0xffffffffu); SU32(S, 0x86f8 + 4 * j, 0xffffffffu); }
        const rx_addr obj = U32(desc, 0x44);
        PHYS_CALL_SP(28, phys_contact_dispatch(S + 0x80d0, U32(obj, 4), desc + 0x10, desc, S + 0x8e10, F + 0x80, F + 0x70, 0));   /* Collision_ContactDispatch: ret 0x1c */
        const int32_t n = I32(S, 0x80d0);
        if (n > 0) {
            count += n;
            rx_v4 sum_p = rx_ld4(F + 0x50);
            for (int32_t j = 0; j < n; j++) {
                const rx_addr e = S + 0x80e0 + 0x30u * (uint32_t)j;
                sum_p = v4_add(rx_ld4(e), sum_p);
                sum_n = v4_add(rx_ld4(e + 0x10), sum_n);
            }
            rx_st4(F + 0x50, sum_p);
            SU32(surface_at, 0, U32(U32(S, 0x130c) + 0x50 * i, 0x40));
        }
    }
    if (count > 0) {
        SU32(W, 0xfc, 1);
        rx_st4(F + 0x50, v4_div(rx_ld4(F + 0x50), v4_splat((float)count)));
        rx_st4(F + 0x30, bike_normalize(sum_n));
        PHYS_CALL_SP(16, phys_probe_frame_setup(F + 0x70, F + 0x30, W, F + 0x50, F + 0x80, F + 0x60));   /* the contact record: ret 0x10 */
    }
}

/* 00444b90 RavenSimulationMotocross::vf13 (ECX = S, stack dt; ret 4): slot 13, after the pose integration: the wheels'
 * terrain and object contacts (front W = S+88e0, rear W = S+8790), the contact flags (S+8890 / 89e0 now, S+8894 / 89e4
 * since the last reset, both off -> airborne S+8d98, S+8d9c = just took off), the body probes 00435b00, the position
 * correction 004456d0, the rolling wheels 00445800; then the planar constraint (S+5c: the root kept within +-1.15 of the
 * plane of S+70 / S+80 along its z, flags S+60 / S+64, all bodies translated) and the resting pin 00446f80. */
void bike_slot13(rx_addr S, float dt) {
    const rx_addr sp = rx_sp(), A = (sp - 4) & ~15u, F = A - 0xb0;
    rx_set_sp(F);                                                         /* the original's ESP inside its frame */
    s13_wheel_objects(S, S + 0x88e0, S + 0x8780, F);
    s13_wheel_objects(S, S + 0x8790, S + 0x8784, F);
    const uint32_t rear = U32(S, 0x888c), front = U32(S, 0x89dc);
    SU32(S, 0x8894, U32(S, 0x8894) | rear);
    SU32(S, 0x89e4, U32(S, 0x89e4) | front);
    SU32(S, 0x8890, rear);
    SU32(S, 0x89e0, front);
    if (rear == 0) { SU32(S, 0x8850, 0); SU32(S, 0x8854, 0); }
    if (front == 0) { SU32(S, 0x89a0, 0); SU32(S, 0x89a4, 0); }
    if (front == 0 && rear == 0) {
        uint32_t was = U32(S, 0x8d98);
        SU32(S, 0x8d98, 1); SU32(S, 0x8d9c, was == 0);
    } else { SU32(S, 0x8d98, 0); SU32(S, 0x8d9c, 0); }
    bike_body_probe_contacts(S);
    bike_wheel_apply_contact_correction(S);
    bike_wheels_update_rolling_orientation(S, dt);
    SU32(S, 0x60, 0); SU32(S, 0x64, 0);
    if (U32(S, 0x5c) != 0) {
        const rx_v4 Fq = rx_ld4(S + 0x70), C = rx_ld4(S + 0x80), pos = rx_ld4(S + 0xf0);
        rx_v4 p = bike_rotate_inv(Fq, v4_sub(pos, C));
        p.v[0] = rx_ld4(0x00d61670).v[0];
        const rx_v4 lim = v4_splat(1.2000000476837158f /* 00930064 */), margin = v4_splat(0.05000000074505806f /* 009d3bfc */);
        if (bike_all3(v4_cmplt(lim, v4_lane(p, 2)))) {
            SU32(S, 0x60, 1);
            p = v4_shuf(p, v4_sub(lim, margin), 0xa4);
        } else {
            const rx_v4 nlim = v4_splat(-1.2000000476837158f /* 00977888 */);
            if (bike_all3(v4_cmplt(v4_lane(p, 2), nlim))) { SU32(S, 0x64, 1); p = v4_shuf(p, v4_add(margin, nlim), 0xa4); }
        }
        const rx_v4 t = v4_sub(v4_add(bike_rotate(Fq, p), C), pos);
        for (int32_t i = 0; i < I32(S, 0xec); i++) {
            rx_addr P = S + 0xf0 + 0xa0u * (uint32_t)i;
            rx_st4(P, v4_add(t, rx_ld4(P)));
        }
    }
    if (U32(S, 0x2374) == 0) bike_rest_pin(S);
    rx_set_sp(sp);
}
RX_FUNCTION(0x00444b90, bike_slot13)

/* 00438260 Wheel_TriangleContactTest (thiscall-like: ECX = &out point, EDI = &out normal; stack &body q, &body position p,
 * &wheel extent e, triangles (3 vec4 each), count; ret 0x14; EAX = hit). The wheel is the disc of radius e in its body's
 * local y-z plane: for each triangle whose box (grown by 1.05 e) contains p, the vertices go to the body frame (l0, l1, l2);
 * the circle y^2 + z^2 = e^2 (x = G.x) is intersected with the triangle's plane n . c = n . l0 (solved for z through y or for
 * y through z, whichever axis the normal favours); each intersection inside the triangle (n . (edge x (c - corner)) >= 0
 * for the three edges) goes into a ring of the last 8 hits. The result is the average hit and the normalized average normal
 * back in the world. *out point is the original's scratch too: with no hit it keeps the last candidate it held.
 * Returns 1 on a hit; *hits gets the hit count. */
/* The same canonical dot product, with optional observation of ADDSS's
 * retained upper product lanes. Each multiplication/reduction executes once. */
static float wheel_dot_observed(rx_v4 a, rx_v4 b, rx_v4* packed) {
    rx_v4 product = v4_mul(a, b);
    const float value = bike_hsum_012(product);
    if (packed) { *packed = product; packed->v[0] = value; }
    return value;
}
static uint32_t wheel_triangle_contact_kernel(rx_addr out_point, rx_addr out_normal, rx_addr qa, rx_addr pa, rx_addr ea,
                                     rx_addr tris, int32_t count, int32_t* hits_out, int observed) {
    fx_frame frame;
    if (observed) {
        const rx_addr entry = rx_sp(), bottom = ((entry - 4) & ~15u) - 0x260;
        const rx_addr addresses[] = {out_point, out_normal, qa, pa, ea, tris};
        const uint64_t sizes[] = {16, 16, 16, 16, 16, count > 0 ? 48ull*(uint32_t)count : 0};
        for (unsigned i=0; i<6; ++i) {
            const uint64_t end = (uint64_t)addresses[i] + sizes[i];
            if (end > 0x100000000ull || (sizes[i] && addresses[i] < entry && end > bottom)) abort();
        }
        fx_frame_enter(&frame, 0x258, 16, FX_EBX, FX_ESI);
        SU32(frame.E, 0x25c, U32(0x9b45e4, 0) ^ (frame.E + 8));
        fx_set_reg(FX_EAX, (uint32_t)count); fx_set_reg(FX_EBX, 0); fx_set_reg(FX_EDX, 0); fx_set_reg(FX_ESI, ea);
    }
    const rx_v4 G = rx_ld4(0x00d61670), N = rx_ld4(0x00d61650), zero = v4_splat(0.0f);
    const rx_v4 two = v4_splat(2.0f), four = v4_splat(4.0f /* 009322c0 */);
    const rx_v4 ext = v4_mul(v4_splat(1.0499999523162842f /* 009774c8 */), rx_ld4(ea));
    if (observed) {
        rx_st4(frame.E + 0x20, two); rx_st4(frame.E + 0x40, four); rx_st4(frame.E + 0x140, ext);
        if (count > 0) { SU32(frame.E, 0x10, tris + 0x20); SU32(frame.E, 0xc, (uint32_t)count); fx_set_reg(FX_ESI, tris + 0x20); }
    }
    rx_v4 ring_p[8], ring_n[8];
    int32_t hits = 0, slot = 0;
    for (int32_t t = 0; t < count; t++) {
        const rx_addr T = tris + 0x30u * (uint32_t)t;
        const rx_v4 v0 = rx_ld4(T), v1 = rx_ld4(T + 0x10), v2 = rx_ld4(T + 0x20), p = rx_ld4(pa);
        const rx_v4 lower = v4_cmple(v4_sub(v4_min(v0, v4_min(v1, v2)), ext), p);
        if (observed) {
            rx_st4(frame.E + 0x70, lower); rx_st4(frame.E + 0x30, v2);
            fx_set_reg(FX_EAX, rx_fb(lower.v[1]));
            fx_set_reg(FX_ESI, rx_fb(lower.v[2]) + rx_fb(lower.v[1]) + rx_fb(lower.v[0]));
        }
        if (!bike_all3(lower)) goto next_triangle;
        const rx_v4 upper = v4_cmple(p, v4_add(v4_max(v0, v4_max(v1, v2)), ext));
        if (observed) {
            rx_st4(frame.E + 0x70, upper); fx_set_reg(FX_EAX, rx_fb(upper.v[1]));
            fx_set_reg(FX_ESI, rx_fb(upper.v[2]) + rx_fb(upper.v[1]) + rx_fb(upper.v[0]));
        }
        if (!bike_all3(upper)) goto next_triangle;
        const rx_v4 q = rx_ld4(qa);
        const rx_v4 l0 = bike_rotate_inv(q, v4_sub(v0, p)), l1 = bike_rotate_inv(q, v4_sub(v1, rx_ld4(pa)));
        const rx_v4 l2 = bike_rotate_inv(q, v4_sub(v2, rx_ld4(pa)));
        const rx_v4 e01 = v4_sub(l1, l0), e02 = v4_sub(l2, l0), e12 = v4_sub(l2, l1);
        if (observed) {
            rx_st4(frame.E + 0x120, e02); rx_st4(frame.E + 0x130, e12);
            rx_st4(frame.E + 0x30, v4_shuf(e01, e01, 0xc9));
        }
        const rx_v4 n = bike_normalize(bike_cross(e01, e02));
        const rx_v4 nz = v4_lane(n, 2), nz2 = v4_mul(nz, nz), ny = v4_lane(n, 1), ny2 = v4_mul(ny, ny);
        const rx_v4 d = v4_splat(bike_dot3(n, l0));
        const rx_v4 E = rx_ld4(ea);
        rx_v4 y1, z1, y2, z2;
        const rx_v4 choose_z = v4_cmplt(nz2, ny2);
        if (observed) {
            rx_st4(frame.E + 0x70, zero); rx_st4(frame.E + 0xa0, nz); rx_st4(frame.E + 0xb0, nz2);
            rx_st4(frame.E + 0x50, choose_z); rx_st4(frame.E + 0xd0, ny2); rx_st4(frame.E + 0x90, d);
        }
        if (bike_all3(choose_z)) {       /* solve for z: y = (d - nz z) / ny */
            const rx_v4 a = v4_add(v4_div(nz2, ny2), N);
            const rx_v4 b = v4_div(v4_mul(v4_mul(v4_sub(zero, two), nz), d), ny2);
            const rx_v4 c = v4_mul(v4_sub(v4_div(v4_mul(d, d), ny2), v4_mul(E, E)), v4_mul(a, four));
            const rx_v4 disc = v4_sub(v4_mul(b, b), c);
            const rx_v4 real_roots = v4_cmplt(G, disc);
            if (observed) { rx_st4(frame.E + 0x80, a); rx_st4(frame.E + 0x60, c); rx_st4(frame.E + 0x50, real_roots); }
            if (!bike_all3(real_roots)) goto next_triangle;
            const rx_v4 mb = v4_sub(zero, b), sq = v4_sqrt(disc), den = v4_mul(a, two);
            z1 = v4_div(v4_add(sq, mb), den);
            z2 = v4_div(v4_sub(mb, sq), den);
            y1 = v4_div(v4_sub(d, v4_mul(nz, z1)), ny);
            y2 = v4_div(v4_sub(d, v4_mul(nz, z2)), ny);
            if (observed) {
                rx_st4(frame.E + 0x50, sq); rx_st4(frame.E + 0x60, den);
                rx_st4(frame.E + 0x100, z1); rx_st4(frame.E + 0x110, z2); rx_st4(frame.E + 0xf0, y2);
            }
        } else {                                    /* solve for y: z = (d - ny y) / nz */
            const rx_v4 a = v4_add(v4_div(ny2, nz2), N);
            const rx_v4 b = v4_div(v4_mul(v4_mul(v4_sub(zero, two), ny), d), nz2);
            const rx_v4 c = v4_mul(v4_sub(v4_div(v4_mul(d, d), nz2), v4_mul(E, E)), v4_mul(a, four));
            const rx_v4 disc = v4_sub(v4_mul(b, b), c);
            const rx_v4 real_roots = v4_cmplt(G, disc);
            if (observed) { rx_st4(frame.E + 0x80, a); rx_st4(frame.E + 0x50, real_roots); }
            if (!bike_all3(real_roots)) goto next_triangle;
            const rx_v4 mb = v4_sub(zero, b), sq = v4_sqrt(disc), den = v4_mul(a, two);
            y1 = v4_div(v4_add(sq, mb), den);
            z1 = v4_div(v4_sub(d, v4_mul(ny, y1)), nz);
            y2 = v4_div(v4_sub(mb, sq), den);
            z2 = v4_div(v4_sub(d, v4_mul(ny, y2)), nz);
            if (observed) {
                rx_st4(frame.E + 0x60, den); rx_st4(frame.E + 0x50, sq); rx_st4(frame.E + 0xe0, mb);
                rx_st4(frame.E + 0x100, z1); rx_st4(frame.E + 0xf0, y2); rx_st4(frame.E + 0x110, z2);
            }
        }
        const rx_v4 e20 = v4_sub(zero, e02);
        for (int k = 0; k < 2; k++) {
            rx_st4(out_point, G);
            const rx_v4 g = rx_ld4(out_point);
            const rx_v4 cy = k == 0 ? y1 : y2, cz = k == 0 ? z1 : z2;
            const rx_v4 c = v4_shuf(v4_movss(v4_shuf(cy, g, 0xe1), g), cz, 0x04);   /* (G.x, y, z, z) */
            rx_st4(out_point, c);
            rx_v4 packed0, packed1;
            const float t0 = wheel_dot_observed(bike_cross(e01, v4_sub(c, l0)), n, observed ? &packed0 : NULL);
            const float t1 = wheel_dot_observed(bike_cross(e12, v4_sub(c, l1)), n, observed ? &packed1 : NULL);
            const float t2 = bike_dot3(bike_cross(e20, v4_sub(c, l2)), n);
            const rx_v4 inside = v4_cmple(G, v4(t0, t1, t2, t2));
            if (observed) {
                if (k == 0) {
                    rx_st4(frame.E + 0x50, c); rx_st4(frame.E + 0x60, packed0);
                    rx_st4(frame.E + 0xc0, v4_shuf(e12, e12, 0xc9));
                    rx_st4(frame.E + 0xe0, packed1); rx_st4(frame.E + 0x70, e20);
                } else rx_st4(frame.E + 0xc0, packed1);
                rx_st4(frame.E + 0x60, inside);
            }
            if (bike_all3(inside)) {
                ring_p[slot] = c; ring_n[slot] = n;
                if (observed) { rx_st4(frame.E + 0x150 + 16u*(uint32_t)slot, c); rx_st4(frame.E + 0x1d0 + 16u*(uint32_t)slot, n); }
                slot = (slot + 1) & 7;
                hits++;
            }
        }
next_triangle:
        if (observed) {
            const rx_addr cursor = tris + 0x20 + 0x30u * (uint32_t)(t + 1);
            SU32(frame.E, 0x10, cursor); SU32(frame.E, 0xc, (uint32_t)(count - t - 1)); fx_set_reg(FX_ESI, cursor);
        }
    }
    if (hits_out) *hits_out = hits;
    if (hits <= 0) {
        if (observed) {
            fx_set_reg(FX_EAX, 0);
            const uint32_t cookie = U32(frame.E, 0x25c) ^ (frame.E + 8);
            if (cookie != U32(0x9b45e4, 0)) abort();
            fx_set_reg(FX_ECX, cookie); SU32(frame.E, 4, 0x438ec5);
            fx_frame_leave(&frame);
        }
        return 0;
    }
    const int32_t m = hits >= 8 ? 8 : hits;
    rx_st4(out_point, G); rx_st4(out_normal, G);
    for (int32_t k = 0; k < m; k++) {
        rx_st4(out_point, v4_add(ring_p[k], rx_ld4(out_point)));
        rx_st4(out_normal, v4_add(ring_n[k], rx_ld4(out_normal)));
    }
    rx_st4(out_point, v4_div(rx_ld4(out_point), v4_splat((float)m)));
    rx_st4(out_normal, bike_normalize(rx_ld4(out_normal)));
    const rx_v4 q = rx_ld4(qa);
    rx_st4(out_point, v4_add(bike_rotate(q, rx_ld4(out_point)), rx_ld4(pa)));
    rx_st4(out_normal, bike_rotate(q, rx_ld4(out_normal)));
    if (observed) {
        fx_set_reg(FX_EAX, 1); fx_set_reg(FX_EDX, qa);
        const uint32_t cookie = U32(frame.E, 0x25c) ^ (frame.E + 8);
        if (cookie != U32(0x9b45e4, 0)) abort();
        fx_set_reg(FX_ECX, cookie); SU32(frame.E, 4, 0x438ec5); fx_frame_leave(&frame);
    }
    return 1;
}
uint32_t bike_wheel_triangle_contact(rx_addr op, rx_addr on, rx_addr q, rx_addr p, rx_addr e, rx_addr tris, int32_t count, int32_t* hits) {
    return wheel_triangle_contact_kernel(op, on, q, p, e, tris, count, hits, 0);
}
uint32_t bike_wheel_triangle_contact_observed(rx_addr op, rx_addr on, rx_addr q, rx_addr p, rx_addr e, rx_addr tris, int32_t count, int32_t* hits) {
    return wheel_triangle_contact_kernel(op, on, q, p, e, tris, count, hits, 1);
}
static uint32_t s13_triangle_test(rx_addr op, rx_addr on, rx_addr q, rx_addr p, rx_addr e, rx_addr t, int32_t n) {
    return bike_wheel_triangle_contact(op, on, q, p, e, t, n, NULL);
}
RX_FUNCTION(0x00438260, s13_triangle_test)
