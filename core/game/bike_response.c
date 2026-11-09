/* bike_response.c -- contact response of the vehicle bodies (slot 15's first call), _re/notes/bike-contact-response-recovery.md.
 *
 * 004360b0 BikeSim_ContactResponse is shared by every vehicle simulation's vf15 (bike, ATV, truck: ECX = S, plain ret). It
 * resolves the contacts found by slot 13 sequentially (Gauss-Seidel) on a working copy of the ROOT body's velocity v / angular
 * velocity w: for each approaching contact a normal impulse that cancels the approach speed, then a friction impulse along the
 * sliding direction (its angular part scaled by speed). The resulting velocity change of the root is then given to every body
 * (rigidly, through the lever arms), and the strongest contact is reported once as an impact event.
 *
 * Contacts: terrain contacts S+20f8 (count), points S+2100 + 16k (world), normals S+2280 + 16k, surface words S+2340 + 4k;
 * body probes S+1318 (count), flags S+1af0 + 4k, probe vectors S+14b0 + 16k (root-local), normals S+1960 + 16k. */
#include "bike_response.h"
#include "bike_curve.h"
#include "phys_body.h"
#include "phys_mech.h"
#include "world_veh.h"
#include "world_wreck.h"
#include "bike_response_observed.h"
#include "fx_util.h"
#include "loop_jobs.h"
#include <stdio.h>
#include <stdlib.h>

#define G_NUMERATOR   0x00d61650
#define G_ZERO        0x00d61670

/* The original entry/RET0 contract is explicit. Native dependencies use the
 * existing typed semantic owners once; oracle bindings run the genuine target
 * at this very caller frame. No guest code dispatch belongs in product. */
static rx_addr response_dependency(const rx_response_observed_frame*o,rx_addr target,rx_addr ra){
 const rx_addr caller=o->b;SU32(caller,-4,ra);rx_set_sp(caller-4);
 rx_response_dependency_exact(target,ra,caller);
 if(rx_sp()!=caller){fprintf(stderr,"shared response dependency stack drift %08x\n",target);abort();}
 return fx_reg(FX_EAX);
}
static rx_addr resp_current_world(rx_addr registry,const rx_response_observed_frame*o){
 if(o){fx_set_reg(FX_EDX,registry);return response_dependency(o,0x402dc0,0x43760f);}
 rx_addr r;PHYS_CALL_SP(0,r=rx_world_find_by_controller(registry));return r;
}
static rx_addr resp_event_owner(rx_addr game,const rx_response_observed_frame*o){
 if(o){fx_set_reg(FX_EDX,game);return response_dependency(o,0x75c580,0x437ff5);}
 rx_addr r;PHYS_CALL_SP(0,r=rx_world_active_sound_table(game));return r;
}
static rx_addr resp_handle_record(rx_addr handle,const rx_response_observed_frame*o){
 if(o){fx_set_reg(FX_EAX,handle);return response_dependency(o,0x42bd90,0x438022);}
 rx_addr r;PHYS_CALL_SP(0,r=phys_body_record(handle));return r;
}

typedef struct {
    rx_v4 q, inv_inertia, inv_mass, N, G;
    rx_v4 v, w;                  /* the root's working velocity / angular velocity */
    rx_v4 spin_scale;            /* scale of the friction's angular part: min(5, |v| * 5 / 100) */
    rx_v4 best, best_arm;        /* the strongest normal impulse so far and its root-local lever arm */
    int any;
} resp_state;

/* One scalar-free packed product used by this kernel's observable rotation
 * stages. Terms are returned from the same arithmetic that supplies gameplay
 * values; the observer never executes a second response/impulse calculation. */
typedef struct {rx_v4 xraw,yraw,z_signed,z_y,result,reverse_x,reverse_y,reverse_z,lane_z;} response_product;
static response_product response_product_value(rx_v4 a,rx_v4 q,unsigned reversed){
 response_product t;rx_v4 r=v4_shuf(q,q,0x1b);t.reverse_x=r;
 t.xraw=reversed&1?v4_mul(r,v4_lane(a,0)):v4_mul(v4_lane(a,0),r);
 r=v4_shuf(r,r,0xb1);t.reverse_y=r;
 t.yraw=reversed&2?v4_mul(r,v4_lane(a,1)):v4_mul(v4_lane(a,1),r);
 r=v4_shuf(r,r,0x1b);t.reverse_z=r;t.lane_z=v4_lane(a,2);
 rx_v4 z=v4_mul(reversed&16?v4_mul(t.lane_z,r):v4_mul(r,t.lane_z),rx_ld4(0x93a210));
 t.z_signed=z;t.z_y=v4_add(z,reversed&64?v4_mul(rx_ld4(0x93a200),t.yraw):v4_mul(t.yraw,rx_ld4(0x93a200)));
 rx_v4 w=reversed&4?v4_mul(q,v4_lane(a,3)):v4_mul(v4_lane(a,3),q);
 rx_v4 x=reversed&32?v4_mul(rx_ld4(0x93a1f0),t.xraw):v4_mul(t.xraw,rx_ld4(0x93a1f0));t.result=v4_add(t.z_y,reversed&8?v4_add(x,w):v4_add(w,x));return t;
}
static rx_v4 response_terrain_lever(resp_state*c,rx_v4 r,const rx_response_observed_frame*o){
 rx_v4 masked=v4_and(r,rx_ld4(0x92db80));
 response_product first=response_product_value(masked,c->q,0);
 rx_v4 conjugate=v4_mul(c->q,rx_ld4(0x93a220));
 response_product second=response_product_value(conjugate,first.result,0);
 if(o){rx_addr b=o->b;
  rx_st4(b+0x180,r);rx_st4(b+0xf0,masked);rx_st4(b+0xa0,first.xraw);
  rx_st4(b+0x120,first.reverse_y);rx_st4(b+0xe0,first.yraw);rx_st4(b+0x70,first.reverse_z);
  rx_st4(b+0x120,first.lane_z);rx_st4(b+0x70,first.z_y);rx_st4(b+0xa0,first.result);
  rx_st4(b+0xe0,second.xraw);rx_st4(b+0x100,second.result);
 }
 return second.result;
}
/* Probe input rotation has a different original spill schedule from terrain.
 * The returned world arm is the value consumed by the canonical contact. */
static rx_v4 response_probe_lever(resp_state*c,rx_v4 probe,const rx_response_observed_frame*o){
 rx_v4 masked=v4_and(probe,rx_ld4(0x92db80));
 rx_v4 conjugate=v4_mul(c->q,rx_ld4(0x93a220));
 response_product first=response_product_value(masked,conjugate,0);
 response_product second=response_product_value(c->q,first.result,1|2|4);
 if(o){rx_addr b=o->b;
  rx_st4(b+0xf0,probe);rx_st4(b+0x120,masked);rx_st4(b+0x100,v4_shuf(conjugate,conjugate,0x1b));
  rx_st4(b+0x100,first.reverse_y);rx_st4(b+0xe0,first.xraw);rx_st4(b+0xa0,first.yraw);
  rx_st4(b+0x100,first.reverse_z);rx_st4(b+0xb0,first.lane_z);rx_st4(b+0x100,first.z_y);rx_st4(b+0xb0,first.result);
  rx_st4(b+0x100,v4_shuf(first.result,first.result,0x1b));rx_st4(b+0xa0,second.xraw);
  rx_st4(b+0x60,v4_lane(c->q,0));rx_st4(b+0x100,second.reverse_y);
  rx_st4(b+0x50,v4_lane(c->q,1));rx_st4(b+0x20,v4_lane(c->q,2));rx_st4(b+0x40,v4_lane(c->q,3));
  rx_st4(b+0xd0,conjugate);rx_st4(b+0x1a0,second.result);
 }
 return second.result;
}
static rx_v4 response_point_velocity(resp_state*c,rx_v4 r_local,const rx_response_observed_frame*o,int terrain){
 rx_v4 a1=v4_shuf(c->w,c->w,0xc9),a2=v4_shuf(a1,a1,0xc9);
 rx_v4 b1=v4_shuf(r_local,r_local,0xd2),b2=v4_shuf(b1,b1,0xd2);
 rx_v4 cross=v4_and(v4_sub(v4_mul(b1,a1),(terrain?v4_mul(a2,b2):v4_mul(b2,a2))),rx_ld4(0x92db80));cross=v4_and(cross,rx_ld4(0x92db80));
 rx_v4 conjugate=v4_mul(c->q,rx_ld4(0x93a220));response_product inner=response_product_value(cross,conjugate,0);
 response_product outer=response_product_value(c->q,inner.result,1|2|4);
 rx_v4 point=v4_add(outer.result,c->v);
 if(o){rx_addr b=o->b;
  if(terrain)rx_st4(b+0xa0,a1);else rx_st4(b+0xb0,b1);
  rx_st4(b+(terrain?0xe0:0xa0),inner.xraw);rx_st4(b+(terrain?0xa0:0xb0),inner.yraw);
  rx_st4(b+(terrain?0xe0:0xa0),outer.xraw);rx_st4(b+(terrain?0xa0:0xb0),outer.yraw);rx_st4(b+(terrain?0x190:0x1b0),point);
 }
 return point;
}
static rx_v4 response_cross(rx_v4 a,rx_v4 b,int reverse_second){
 rx_v4 a1=v4_shuf(a,a,0xc9),a2=v4_shuf(a1,a1,0xc9),b1=v4_shuf(b,b,0xd2),b2=v4_shuf(b1,b1,0xd2);
 return v4_and(v4_sub(v4_mul(b1,a1),reverse_second?v4_mul(a2,b2):v4_mul(b2,a2)),rx_ld4(0x92db80));
}
static void observed_product_xy(const rx_response_observed_frame*o,response_product t,int terrain,int friction,unsigned step){
 if(!o)return;rx_addr b=o->b;unsigned x,y;
 if(terrain){x=friction?0xa0:0xe0;y=friction?0xb0:0xa0;}
 else{x=friction?0xd0:(step==1?0xe0:0xa0);y=friction?0xc0:0xb0;}
 rx_st4(b+x,t.xraw);rx_st4(b+y,t.yraw);
}
/* A single effective-inertia computation supplies both response and spills. */
static float response_effective_rotation(resp_state*c,rx_v4 r,rx_v4 axis,const rx_response_observed_frame*o,int terrain,int friction){
 rx_v4 cross=response_cross(r,axis,0);cross=v4_and(cross,rx_ld4(0x92db80));
 if(o&&!friction){rx_st4(o->b+(terrain?0xa0:0xb0),v4_shuf(axis,axis,0xd2));rx_st4(o->b+(terrain?0xb0:0xc0),v4_shuf(r,r,0xc9));}
 rx_v4 conjugate=v4_mul(c->q,rx_ld4(0x93a220));
 response_product t=response_product_value(cross,c->q,0);observed_product_xy(o,t,terrain,friction,0);
 if(o&&!terrain&&!friction){rx_st4(o->b+0x30,v4_shuf(c->q,c->q,0x1b));rx_st4(o->b+0xa0,t.result);}
 t=response_product_value(conjugate,t.result,0);observed_product_xy(o,t,terrain,friction,1);
 rx_v4 scaled=v4_and(v4_mul(t.result,c->inv_inertia),rx_ld4(0x92db80));
 t=response_product_value(scaled,conjugate,0);observed_product_xy(o,t,terrain,friction,2);
 t=response_product_value(c->q,t.result,(terrain&&friction)?4|8:4);observed_product_xy(o,t,terrain,friction,3);
 if(o&&!friction)rx_st4(o->b+0x120,v4_shuf(r,r,0xd2));
 return bike_dot3(response_cross(t.result,r,0),axis);
}
static rx_v4 response_normal_angular(resp_state*c,rx_v4 r_local,rx_v4 impulse,const rx_response_observed_frame*o,int terrain,int friction){
 rx_v4 conjugate=v4_mul(c->q,rx_ld4(0x93a220));
 response_product t=response_product_value(v4_and(impulse,rx_ld4(0x92db80)),c->q,0);observed_product_xy(o,t,terrain,friction,0);
 t=response_product_value(conjugate,t.result,(terrain?friction:!friction)?16:0);observed_product_xy(o,t,terrain,friction,2);
 if(o&&!(friction&&!terrain))rx_st4(o->b+(terrain&&!friction?0xa0:0xb0),v4_shuf(t.result,t.result,0xd2));
 return v4_mul(response_cross(r_local,t.result,0),c->inv_inertia);
}

/* One contact: lever arm r (world) / r_local (root frame), normal n. Returns 1 when its normal impulse is the strongest so far. */
static int resp_contact(resp_state* c, rx_v4 r, rx_v4 r_local, rx_v4 n,const rx_response_observed_frame*observer,int terrain,rx_addr S,uint32_t*best_surface,unsigned contact_index) {
    const rx_v4 zero = v4_splat(0.0f);                       /* 0092e12c */
    const rx_v4 vc = response_point_velocity(c,r_local,observer,terrain);
    const float vn = bike_dot3(vc, n);
    rx_v4 approaching=v4_cmplt(v4_splat(vn),c->G);
    if(observer){
        rx_addr b=observer->b;unsigned mask=terrain?0xf0:0x100;int y=terrain?FX_EDI:FX_ECX,z=terrain?FX_EBX:FX_EDX;
        rx_st4(b+(terrain?0x1b0:0x170),v4_splat(vn));rx_st4(b+mask,approaching);
        fx_set_reg(y,U32(b,mask+4));fx_set_reg(z,U32(b,mask+8));fx_set_reg(z,fx_reg(z)+fx_reg(y));fx_set_reg(z,fx_reg(z)+U32(b,mask));rx_st4(b+(terrain?0x170:0x180),n);
    }
    if (!bike_all3(approaching)) return 0; /* complete separating contact */

    int strongest = 0;
    {   /* normal impulse */
        float e = response_effective_rotation(c,r,n,observer,terrain,0);
        rx_v4 J = v4_div(v4_mul(v4_mul(v4_sub(zero, v4_add(zero, c->N)), v4_splat(vn)), n), v4_add(v4_splat(e), c->inv_mass));
        c->v = v4_add(v4_mul(c->inv_mass, J), c->v);
        if(observer){rx_st4(observer->b+0x70,v4_zero());rx_st4(observer->b+(terrain?0x1c0:0x190),J);rx_st4(observer->b+0x150,c->v);}
        c->w = v4_add(response_normal_angular(c,r_local,J,observer,terrain,0), c->w);
        if(observer)rx_st4(observer->b+0x140,c->w);
        rx_v4 lb = v4_sqrt(v4_splat(bike_hsum_021(v4_mul(c->best, c->best))));
        rx_v4 lj = v4_sqrt(v4_splat(bike_hsum_021(v4_mul(J, J))));
        rx_v4 best_mask=v4_cmplt(lb,lj);
        if(observer){rx_addr b=observer->b;unsigned mask=terrain?0xf0:0x100;int y=terrain?FX_EDI:FX_EAX,z=terrain?FX_EBX:FX_ECX;
         rx_st4(b+mask,best_mask);fx_set_reg(y,U32(b,mask+4));fx_set_reg(z,U32(b,mask+8));fx_set_reg(z,fx_reg(z)+fx_reg(y));fx_set_reg(z,fx_reg(z)+U32(b,mask));}
        if (bike_all3(best_mask)) {
            c->best = J;c->any = 1;strongest = 1;c->best_arm=r_local;
            if(terrain)*best_surface=U32(S,0x2340+4u*contact_index);
            else{
                if(observer){fx_set_reg(FX_EAX,1);rx_st4(observer->b+0x1d0,J);rx_st4(observer->b+0x200,r_local);SU32(observer->b,0x18,1);}
                if(!(U8(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}
                *best_surface=U32(resp_current_world(0x9e30d4,observer),0xe4);
                if(observer)fx_set_reg(FX_EDX,*best_surface);
            }
            if(observer){rx_addr b=observer->b;if(terrain){SU32(b,0x18,1);fx_set_reg(FX_EDI,*best_surface);rx_st4(b+0x1d0,J);rx_st4(b+0x200,r_local);}SU32(b,0x1c,*best_surface);}
        }

    }
    {   /* friction along the sliding direction (from the velocity before the normal impulse) */
        rx_v4 tangent=v4_sub(vc,v4_mul(v4_splat(vn),n));
        float tangent_l2=bike_dot3(tangent,tangent);rx_v4 t=bike_normalize_masked(tangent,tangent_l2);
        rx_v4 packed_vt=v4_mul(vc,t);packed_vt.v[0]=phys_add(phys_add(packed_vt.v[0],packed_vt.v[1]),packed_vt.v[2]);
        float vt=packed_vt.v[0];
        if(observer){rx_addr b=observer->b;
         if(!terrain){int finite=tangent_l2!=F32(0,0x92dc20);rx_st4(b+0xd0,v4_mask(finite,finite,finite,finite));}
         rx_st4(b+(terrain?0x1a0:0x1c0),t);rx_st4(b+(terrain?0xe0:0xb0),packed_vt);rx_st4(b+(terrain?0xa0:0xd0),v4_shuf(t,t,0xd2));}
        float e=response_effective_rotation(c,r,t,observer,terrain,1);
        rx_v4 keep = v4_splat(F32(0,0x9778f8));          /* 009778f8 */
        rx_v4 J = v4_div(v4_mul(v4_mul(v4_sub(zero, v4_add(keep, c->N)), v4_splat(vt)), t), v4_add(v4_splat(e), c->inv_mass));
        c->v = v4_add(v4_mul(J, c->inv_mass), c->v);
        if(observer)rx_st4(observer->b+0x150,c->v);
        c->w=v4_add(response_normal_angular(c,v4_mul(r_local,c->spin_scale),J,observer,terrain,1),c->w);
        if(observer)rx_st4(observer->b+0x140,c->w);
    }
    return strongest;
}

/* The isolated exact entry is additive to this one canonical gameplay kernel.
 * Its complete frame mapping is source-only until independent proof. */
static void observed_prefix(rx_addr s,rx_addr b,const resp_state*c,rx_v4 position,rx_v4 speed_mask){
 rx_v4 zero=v4_splat(F32(0,0x92e12c)),friction=v4_splat(F32(0,0x9778f8));
 /* Original physical ordering, including the conditional B130 overwrite. */
 rx_st4(b+0x210,position);rx_st4(b+0x160,c->inv_inertia);rx_st4(b+0x1e0,c->inv_mass);
 fx_set_reg(FX_EAX,s+0x110);rx_st4(b+0x150,c->v);rx_st4(b+0x90,zero);rx_st4(b+0x1f0,friction);
 rx_st4(b+0x1d0,zero);rx_st4(b+0x200,zero);rx_st4(b+0xf0,speed_mask);
 fx_set_reg(FX_ECX,U32(b,0xf8));SU32(b,0x80,fx_reg(FX_EAX));fx_set_reg(FX_EAX,U32(b,0xf4));
 fx_set_reg(FX_ECX,fx_reg(FX_ECX)+fx_reg(FX_EAX));fx_set_reg(FX_ECX,fx_reg(FX_ECX)+U32(b,0xf0));
 rx_st4(b+0x110,c->q);rx_st4(b+0x140,c->w);SU32(b,0x18,0);rx_st4(b+0x130,c->spin_scale);
}
/* 004360b0 BikeSim_ContactResponse (ECX = S). */
static void contact_response_kernel(rx_addr S,const rx_response_observed_frame*observer) {
    resp_state c;
    c.N = rx_ld4(G_NUMERATOR); c.G = rx_ld4(G_ZERO);
    const rx_v4 pos = rx_ld4(S + 0xf0);
    c.inv_inertia = rx_ld4(S + 0x760);
    c.inv_mass = v4_div(c.N, rx_ld4(S + 0x110));
    c.q = rx_ld4(S + 0x730);
    c.v = rx_ld4(S + 0x100); c.w = rx_ld4(S + 0x740);
    c.best = v4_splat(0.0f); c.best_arm = v4_splat(0.0f); c.any = 0;
    uint32_t best_surface = 0;
    {
        const rx_v4 speed = v4_sqrt(v4_splat(bike_hsum_021(v4_mul(c.v, c.v))));
        const rx_v4 speed_mask=v4_cmplt(v4_splat(F32(0,0x93005c)), speed);
        if (bike_all3(speed_mask)) c.spin_scale = v4_splat(F32(0,0x935890));
        else c.spin_scale = v4_div(v4_mul(speed, v4_splat(F32(0,0x935890))), v4_splat(F32(0,0x93005c)));
        if(observer)observed_prefix(S,observer->b,&c,pos,speed_mask);
    }
    if(observer&&I32(S,0x20f8)>0){
        rx_addr b=observer->b;fx_set_reg(FX_EDX,U32(S,0x20f8));
        rx_st4(b+0x60,v4_lane(c.q,0));rx_st4(b+0x50,v4_lane(c.q,1));rx_st4(b+0x20,v4_lane(c.q,2));rx_st4(b+0x40,v4_lane(c.q,3));rx_st4(b+0x30,v4_shuf(c.q,c.q,0x1b));
        fx_set_reg(FX_ECX,S+0x2340);fx_set_reg(FX_EAX,S+0x2280);
    }

    /* terrain contacts */
    for (int32_t k = 0, n = I32(S, 0x20f8); k < n; k++) {
        rx_v4 r = v4_sub(rx_ld4(S + 0x2100 + 16u * (uint32_t)k), pos);
        rx_v4 r_local = response_terrain_lever(&c,r,observer);
        (void)resp_contact(&c,r,r_local,rx_ld4(S+0x2280+16u*(uint32_t)k),observer,1,S,&best_surface,(unsigned)k);
        if(observer){fx_set_reg(FX_EAX,fx_reg(FX_EAX)+16);fx_set_reg(FX_ECX,fx_reg(FX_ECX)+4);fx_set_reg(FX_EDX,fx_reg(FX_EDX)-1);}
    }
    if(observer){fx_set_reg(FX_EBX,0);if(I32(S,0x1318)>0){fx_set_reg(FX_EDX,S+0x1af0);fx_set_reg(FX_EDI,S+0x1960);SU32(observer->b,0x14,fx_reg(FX_EDX));}}
    /* Body probes share the canonical contact calculation, with their actual
     * private packed spill layout instead of a second observer replay. */
    for(int32_t k=0;k<I32(S,0x1318);++k){
        if(observer)fx_set_reg(FX_EAX,U32(observer->b,0x14));
        if(U32(S,0x1af0+4u*(uint32_t)k)){
            rx_v4 probe=rx_ld4(S+0x14b0+16u*(uint32_t)k);
            rx_v4 arm=response_probe_lever(&c,probe,observer);
            (void)resp_contact(&c,arm,probe,rx_ld4(S+0x1960+16u*(uint32_t)k),observer,0,S,&best_surface,(unsigned)k);
        }
        if(observer){SU32(observer->b,0x14,U32(observer->b,0x14)+4);fx_set_reg(FX_EBX,fx_reg(FX_EBX)+1);fx_set_reg(FX_EDI,fx_reg(FX_EDI)+16);}
    }
    if (!c.any) return;
    /* Root delta: exact same clamped value supplies every body's update. */
    const rx_v4 dv=v4_sub(c.v,rx_ld4(S+0x100));rx_v4 dw=v4_sub(c.w,rx_ld4(S+0x740));
    rx_v4 dwlen=v4_sqrt(v4_splat(bike_hsum_021(v4_mul(dw,dw))));rx_v4 limit=v4_splat(F32(0,0x9322d0));rx_v4 cap=v4_cmplt(limit,dwlen);
    if(observer){rx_addr b=observer->b;rx_st4(b+0xe0,dv);fx_set_reg(FX_EAX,S+0x100);rx_st4(b+0xf0,dw);rx_st4(b+0x130,cap);fx_set_reg(FX_ECX,U32(b,0x134));fx_set_reg(FX_EDX,U32(b,0x138));fx_set_reg(FX_EDX,fx_reg(FX_EDX)+fx_reg(FX_ECX));fx_set_reg(FX_EDX,fx_reg(FX_EDX)+U32(b,0x130));}
    if(bike_all3(cap)){dw=v4_div(v4_mul(limit,dw),dwlen);if(observer)rx_st4(observer->b+0xf0,dw);}
    if(observer){fx_set_reg(FX_ECX,0);if(I32(S,0xec)>0){rx_addr b=observer->b;rx_v4 a=v4_shuf(dw,dw,0xc9);rx_st4(b+0xa0,a);rx_st4(b+0xb0,v4_shuf(a,a,0xc9));rx_st4(b+0x60,v4_lane(c.q,0));rx_st4(b+0x50,v4_lane(c.q,1));rx_st4(b+0x20,v4_lane(c.q,2));rx_st4(b+0x30,v4_shuf(c.q,c.q,0x1b));rx_st4(b+0x40,v4_lane(c.q,3));}}
    const rx_v4 cq=v4_mul(c.q,rx_ld4(0x93a220));
    for(int32_t i=0;i<I32(S,0xec);++i){
        rx_addr body=S+0xa0u*(uint32_t)i;rx_addr b=observer?observer->b:0;
        rx_v4 delta=v4_and(v4_sub(rx_ld4(body+0xf0),rx_ld4(S+0xf0)),rx_ld4(0x92db80));
        response_product a=response_product_value(delta,c.q,0);
        if(observer){rx_st4(b+0xd0,a.xraw);rx_st4(b+0xc0,a.yraw);}
        response_product arm=response_product_value(cq,a.result,16|32|64);
        if(observer){rx_st4(b+0xd0,arm.xraw);rx_st4(b+0xc0,arm.yraw);rx_st4(b+0x130,arm.z_signed);rx_st4(b+0x130,arm.z_y);}
        rx_v4 cross=response_cross(dw,arm.result,0);cross=v4_and(cross,rx_ld4(0x92db80));
        a=response_product_value(cross,cq,0);
        if(observer){rx_st4(b+0xd0,a.xraw);rx_st4(b+0xc0,a.yraw);}
        response_product world=response_product_value(c.q,a.result,1|2|4);
        if(observer){rx_st4(b+0x130,world.reverse_x);rx_st4(b+0xd0,world.xraw);rx_st4(b+0x130,world.reverse_y);rx_st4(b+0xc0,world.yraw);}
        rx_v4 linear=v4_add(v4_add(dv,rx_ld4(body+0x100)),world.result);
        if(observer)rx_st4(b+0x1f0,linear);
        a=response_product_value(v4_and(dw,rx_ld4(0x92db80)),cq,0);
        if(observer){rx_st4(b+0xd0,a.xraw);rx_st4(b+0xc0,a.yraw);}
        world=response_product_value(c.q,a.result,1|2|4);
        if(observer)rx_st4(b+0xc0,world.yraw);
        rx_v4 world_dw=v4_and(world.result,rx_ld4(0x92db80)),bodyq=rx_ld4(body+0x730);
        a=response_product_value(world_dw,bodyq,0);if(observer)rx_st4(b+0xc0,a.yraw);
        response_product local=response_product_value(v4_mul(bodyq,rx_ld4(0x93a220)),a.result,16);if(observer)rx_st4(b+0xc0,local.yraw);
        rx_v4 angular=v4_add(rx_ld4(body+0x740),local.result);
        rx_st4(body+0x100,linear);rx_st4(body+0x740,angular);
        if(observer){fx_set_reg(FX_ECX,(uint32_t)i+1);fx_set_reg(FX_EAX,body+0x1a0);}
    }
    /* Impact publication uses the true typed event/handle owners. */
    if(observer)fx_set_reg(FX_EDI,S+0x2ebc);
    if(U32(S,0x2ebc)==0||U32(S,0x2370)!=0)return;
    rx_job_ordered();   /* the world's contact-event queue: appended in the simulations' order in a parallel batch (loop_jobs.h) */
    SU32(S,0x2370,1);
    rx_addr queue=U32(resp_event_owner(U32(0,0xa946b8),observer),0x20134);
    uint32_t count=U32(queue,0x1900);
    if(observer){fx_set_reg(FX_EAX,queue);fx_set_reg(FX_ESI,count);}
    if(count>=100)return;
    SU32(queue,0x1900,count+1);
    rx_addr event=queue+(count<<6);
    if(observer){fx_set_reg(FX_ECX,count+1);fx_set_reg(FX_ESI,event);}
    rx_addr rec=resp_handle_record(U32(U32(S,0x2ebc),4),observer);
    if(observer){fx_set_reg(FX_EDX,U32(rec,0x34));fx_set_reg(FX_EAX,best_surface);}
    SU32(event,0,U32(rec,0x34));SU32(event,4,best_surface);SU32(event,0x2c,0);SU8(event,0x20,0);
    if(observer)fx_set_reg(FX_EAX,0xffffffffu);
    SU32(event,0x30,0xffffffffu);SU32(event,0x34,0xffffffffu);
    response_product a=response_product_value(v4_and(c.best_arm,rx_ld4(0x92db80)),cq,0);
    response_product world=response_product_value(c.q,a.result,16);
    rx_st4(event+0x10,v4_add(world.result,pos));
    float impulse=phys_sqrt(bike_hsum_021(v4_mul(c.best,c.best))),mass=F32(S,0x110);
    if(observer){SF32(observer->b,0x18,0);fx_set_reg(FX_ECX,U32(observer->b,0x80));SF32(observer->b,0x14,0);}
    SF32(event,0x24,mass>0.0f?phys_div(impulse,mass):impulse);SF32(event,0x28,0);

}
/* Existing callers retain one canonical gameplay implementation. */
void bike_contact_response(rx_addr S){contact_response_kernel(S,NULL);}
void bike_contact_response_observed_exact(rx_addr S){
 fx_frame f;fx_frame_enter(&f,0x214,16,FX_EBX,FX_ESI,FX_EDI);
 rx_response_observed_frame observer={f.E};fx_set_reg(FX_ESI,S);
 contact_response_kernel(S,&observer);fx_frame_leave(&f);
}
RX_FUNCTION(0x004360b0, bike_contact_response)

/* ---------------------------------------------------------------------------------------------------------------------------
 * The rest of slot 15 */

/* 00443af0 Bike_ClampAngularVelocities (stack S; ret 4): root angular velocity capped at length 20, the other bodies' at 120
 * (all four lanes rescaled: w * limit / |w|); S+80c0 = 1 when one was capped. */
static void clamp_spin(rx_addr S, rx_addr w_at, float limit) {
    rx_v4 w = rx_ld4(w_at);
    rx_v4 len = v4_sqrt(v4_splat(bike_hsum_021(v4_mul(w, w))));
    if (bike_all3(v4_cmplt(v4_splat(limit), len))) {
        rx_st4(w_at, v4_div(v4_mul(w, v4_splat(limit)), len));
        SU32(S, 0x80c0, 1);
    }
}
void bike_clamp_angular_velocities(rx_addr S) {
    clamp_spin(S, S + 0x740, 20.0f /* 00935aac */);
    for (int32_t i = 1; i < I32(S, 0xec); i++) clamp_spin(S, S + 0x740 + 0xa0u * (uint32_t)i, 120.0f /* 00977480 */);
}
RX_FUNCTION(0x00443af0, bike_clamp_angular_velocities)

/* 00447810 BikeWheels_SpinRelativeCorrection (EDI = S): each wheel's spin relative to the chassis (both in the wheel's frame)
 * is locked while that wheel brakes and the relative spin is below 10, otherwise scaled by the curve S+3d0c of its length.
 * S+a8 = 1 when both wheels are locked. Front: body 2 (orientation S+870, spin S+880), brake S+24; rear: body 3 (S+910 / 920),
 * brake S+28. Returns the number of locked wheels. */
static int wheel_spin(rx_addr S, rx_addr wheel /* body base */, rx_v4 chassis_spin_world, float brake) {
    const rx_v4 chassis = bike_rotate_inv(rx_ld4(wheel + 0x730), chassis_spin_world);
    const rx_v4 rel = v4_sub(rx_ld4(wheel + 0x740), chassis);
    const rx_v4 len = v4_sqrt(v4_splat(bike_hsum_021(v4_mul(rel, rel))));
    if (brake > 0.0f && bike_all3(v4_cmplt(len, v4_splat(10.0f /* 0092fd88 */)))) {
        rx_st4(wheel + 0x740, v4_add(v4_mul(rel, v4_splat(0.0f)), chassis));
        return 1;
    }
    float k = bike_param_curve(S + 0x3d0c, len.v[0]);
    rx_st4(wheel + 0x740, v4_add(v4_mul(v4_splat(k), rel), chassis));
    return 0;
}
int bike_wheels_spin_relative(rx_addr S) {
    SU32(S, 0xa8, 0);
    const rx_v4 q = rx_ld4(S + 0x730), w = rx_ld4(S + 0x740);
    int locked = wheel_spin(S, S + 0x140 /* body 2: front wheel */, bike_rotate(q, w), F32(S, 0x24));
    locked += wheel_spin(S, S + 0x1e0 /* body 3: rear wheel */, bike_rotate(q, w), F32(S, 0x28));
    if (locked == 2) SU32(S, 0xa8, 1);
    return locked;
}
RX_FUNCTION(0x00447810, bike_wheels_spin_relative)

/* 0085d330 Mechanism_NodesSlot7 (phys): stack M; ret 4. */
static void mech_nodes_slot7(rx_addr M) { PHYS_CALL_SP(4, (void)phys_mech_nodes_slot7(M)); }
/* 0085c4b0 Mechanism_SetVelocity (phys): EDX = M, ESI = &linear velocity (all bodies; other velocities zero). */
static void mech_set_velocity(rx_addr M, rx_addr vel) { PHYS_CALL_SP(0, phys_mech_set_velocity(M, vel)); }
/* a virtual test of the simulation (slot 25 enter-rest +0x64, slot 26 leave-rest +0x68): thiscall, returns EAX */
static uint32_t sim_vcall(rx_addr S, uint32_t slot_offset) {
    return rx_call_original(U32(U32(S, 0), slot_offset), RX_THISCALL, S, 0, NULL);
}
/* the wheels spin with the chassis (resting / pose override): wheel spin = chassis spin in the wheel frame */
static void wheels_follow_chassis(rx_addr S) {
    const rx_v4 q = rx_ld4(S + 0x730);
    rx_st4(S + 0x880, bike_rotate_inv(rx_ld4(S + 0x870), bike_rotate(q, rx_ld4(S + 0x740))));
    rx_st4(S + 0x920, bike_rotate_inv(rx_ld4(S + 0x910), bike_rotate(q, rx_ld4(S + 0x740))));
}

/* 00443c30 RavenSimulationMotocross::vf15 (ECX = S, stack dt; ret 4): slot 15, between velocity and pose integration. */
void bike_slot15(rx_addr S, float dt) {
    (void)dt;
    const rx_v4 G = rx_ld4(G_ZERO);
    bike_contact_response(S);
    bike_clamp_angular_velocities(S);

    if (U32(S, 0x5c) != 0) {
        /* planar constraint in the frame F = S+70: no linear motion along its x, none forward / backward along z when S+60 /
         * S+64 forbid it; no rotation about its y / z */
        const rx_v4 F = rx_ld4(S + 0x70), q = rx_ld4(S + 0x730);
        rx_v4 vl = v4_movss(bike_rotate_inv(F, rx_ld4(S + 0x100)), G);
        const rx_v4 wl = bike_rotate_inv(F, bike_rotate(q, rx_ld4(S + 0x740)));
        if (bike_all3(v4_cmplt(G, v4_lane(vl, 2))) && U32(S, 0x60) != 0) vl = G;
        if (bike_all3(v4_cmplt(v4_lane(vl, 2), G)) && U32(S, 0x64) != 0) vl = G;
        const rx_v4 wl2 = v4_shuf(v4_movss(v4_shuf(G, wl, 0xe5), wl), G, 0xa4);   /* (wl.x, G.y, G.z, G.z) */
        rx_st4(S + 0x100, bike_rotate(F, vl));
        rx_st4(S + 0x740, bike_rotate_inv(q, bike_rotate(F, wl2)));
    }

    if (U32(S, 0x2de0) != 0) {          /* pose override (slot 33..34): not resting; mode 2 keeps only the vertical speed */
        SU32(S, 0x90, 0);
        if (U32(S, 0x2de4) == 2) {
            const rx_v4 up = bike_rotate(rx_ld4(S + 0x730), rx_ld4(0x00d616a0));
            rx_st4(S + 0x100, v4_mul(v4_splat(bike_dot3(rx_ld4(S + 0x100), up)), up));
            rx_st4(S + 0x740, G); rx_st4(S + 0x880, G); rx_st4(S + 0x920, G);
        }
        mech_nodes_slot7(S + 0xd0);
    } else {
        mech_nodes_slot7(S + 0xd0);
        bike_wheels_spin_relative(S);
        if (U32(S, 0x90) != 0) {
            if (sim_vcall(S, 0x68) != 0) SU32(S, 0x90, 0);          /* leave rest */
        } else if (sim_vcall(S, 0x64) != 0) {                          /* enter rest: stop, remember the wheel geometry */
            SU32(S, 0x90, 1);
            mech_set_velocity(S + 0xd0, 0x00d61670);
            rx_st4(S + 0x8b00, rx_ld4(S + 0x87e0));
            rx_st4(S + 0x8b10, rx_ld4(S + 0x8930));
            const rx_v4 q = rx_ld4(S + 0x730);
            rx_st4(S + 0x8b20, bike_qnormalize(bike_quat_mul(bike_quat_conj(q), rx_ld4(S + 0x910))));
            rx_st4(S + 0x8b30, bike_qnormalize(bike_quat_mul(bike_quat_conj(bike_quat_mul(q, rx_ld4(S + 0x6eb0))), rx_ld4(S + 0x870))));
            rx_st4(S + 0x8af0, v4_add(rx_ld4(S + 0x8a10), rx_ld4(S + 0x88c0)));
            rx_st4(S + 0x8af0, bike_normalize(rx_ld4(S + 0x8af0)));   /* the resting "up": both contact normals */
        }
        if (U32(S, 0x90) != 0) {
            const rx_v4 n = rx_ld4(S + 0x8af0);
            rx_st4(S + 0x100, v4_mul(v4_splat(bike_dot3(n, rx_ld4(S + 0x100))), n));
        }
    }
    if (U32(S, 0x90) != 0) wheels_follow_chassis(S);
}
RX_FUNCTION(0x00443c30, bike_slot15)
