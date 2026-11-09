/* Four-wheel world transfer, detach and scalar delete semantic owners.
 * Full family assembly evidence is retained alongside this candidate.
 * Child owners are canonical core code; full10005 composition remains unproven. */
#include "fourwheel_vehicle_lifecycle.h"
#include "vehicle_atv_lifecycle.h"
#include "world_veh_util.h"
#include "world_veh_transfer.h"
#include "build_activity_util.h"
#include "build_rider_util.h"
#include "build_vehicle.h"
#include "build_alloc.h"
#include "anim.h"
#include "anim_extern.h"
#include "phys_terrain.h"
#include "phys_body.h"
#include "world_misc.h"
typedef struct{uint32_t sim,tracks,track_type,vt[4];}life_layout;
static const life_layout layouts[]={
{0x1540,0x13c4,0x3,{0x93d4d4,0x93d5d4,0x93d5e4,0x93d624}},
{0x15c0,0x1410,0x5,{0x93d2e4,0x93d3e4,0x93d3f4,0x93d434}},
{0x15d0,0x144c,0x4,{0x93cee4,0x93cfe4,0x93cff4,0x93d034}},
};
#define PUSHW(n, ...) do { const uint32_t pw_[] = {__VA_ARGS__}; for (int i_ = 0; i_ < (n); i_++) SU32(rx_sp(), -4 * (n) + 4 * i_, pw_[i_]); } while (0)
static void move_body(rx_addr L, rx_addr src_world, uint32_t index, rx_addr dst_world) {
    const rx_addr svec = src_world + 0x1954, dvec = dst_world + 0x1954;
    const int32_t scount = ((int32_t)(U32(src_world, 0x1964) - U32(src_world, 0x1960))) / 0xe0;
    SU32(L, 0x1c, index);
    if (index >= (uint32_t)scount) crt_invalid_parameter_noinfo();
    const rx_addr rec = index * 0xe0u + U32(svec, 0xc);
    const uint32_t newidx = (uint32_t)(((int32_t)(U32(dvec, 0x10) - U32(dvec, 0xc))) / 0xe0);
    PUSHW(2, dvec, rec);
    SU32(L, 0x10, dvec);
    ACT_CALL(2, rx_phys_dynamic_push(dvec, rec));
    ACT_CALL(0, rx_phys_dynamic_insert(dvec, newidx));
    const rx_addr t = U32(rec, 0x30);
    SU32(t, 4, U32(dvec, 0x1c));
    SU32(t, 8, newidx);
    PUSHW(1, index);
    ACT_CALL(1, rx_dyn_records_remove(svec, index));
}

static void tracks_table_search(rx_addr scene) {
    if ((U8(0, 0x009e30e0) & 1) == 0) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    const uint32_t n = U32(0, 0x009e30dc);
    for (uint32_t k = 0; k < n; k++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4 * k);
        if (t != 0 && scene == U32(t, 0x44a8)) break;
    }
}

static void free_slot_word(uint32_t w, rx_addr t1, rx_addr t2, rx_addr t3) {
    if (w & 0x1000) { ACT_CALL(0, rx_tracks_free_slot(t1, (int32_t)(w ^ 0x1000))); }
    else if (w & 0x2000) { ACT_CALL(0, rx_tracks_free_slot(t2, (int32_t)(w ^ 0x2000))); }
    else { ACT_CALL(0, rx_tracks_free_slot(t3, (int32_t)(w ^ 0x4000))); }
}
static void vehicle_world_move(rx_addr v, rx_addr world,const life_layout*l) {
    ACT_FRAME_EBP(L, 8, 0xc, 3);
    SU32(L, 0x14, v);
    const rx_addr oldw = U32(v, 0x188);
    PUSHW(1, v ? v + 0x90 : 0);
    ACT_CALL(1, rx_world_physics_remove_listener(U32(oldw, 0x20134), v ? v + 0x90 : 0));
    PUSHW(1, U32(v,l->sim));
    ACT_CALL(1, rx_world_physics_remove_simulation(U32(U32(v, 0x188), 0x20134), U32(v,l->sim)));
    PUSHW(1, U32(U32(v, 0x188), 0x20138));
    ACT_CALL(1, rx_scene_manager_remove_entity(v, U32(U32(v, 0x188), 0x20138)));
    tracks_table_search(U32(U32(v, 0x188), 0x20138));
    const rx_addr t1 = U32(0, 0x00a95774), t3 = U32(0, 0x00a9577c);
    {
        const rx_addr t2 = U32(0, 0x00a95778);
        free_slot_word(U32(v, l->tracks+0), t1, t2, t3);
        free_slot_word(U32(v, l->tracks+4), t1, t2, t3);
        free_slot_word(U32(v, l->tracks+8), t1, t2, t3);
        free_slot_word(U32(v, l->tracks+12), t1, t2, t3);
    }
    {
        rx_addr p = v + 0x11b0;
        SU32(L, 0xc, p); SU32(L, 0x10, 0x19);
        for (int n = 0x19; n != 0; n--) {
            int32_t w=(int32_t)U32(p,0);
            if(w>=0){w^=0x4000;if(w<I32(t3,0xc)&&w>=0){
                ACT_CALL(0,phys_tracks_cut_strip(t3,w));
                SU32(U32(t3,0x24),0x98+0xc0u*(uint32_t)w,1);
                SU32(t3,0x28,U32(t3,0x28)-1);
            }}
            p += 4;
            SU32(L, 0xc, p); SU32(L, 0x10, (uint32_t)(n - 1));
        }
    }
    SU32(v, 0x188, world);
    const rx_addr pw = U32(world, 0x20134);
    SU32(L, 0x10, v + 0x90);
    PUSHW(1, L + 0x10);
    ACT_CALL(1, (void)rx_world_ptr_vector_push_unique(pw + 0x1914, L + 0x10));
    {   /* the simulation appended to the simulation array {data, count, capacity, growth} at +0x1940 */
        const rx_addr arr = pw + 0x1940, sim = U32(v,l->sim);
        SU32(L, 0xc, sim);
        if (I32(arr, 4) >= I32(arr, 8)) {
            const uint32_t ncap = U32(arr, 0xc) + U32(arr, 8);
            const uint64_t bytes = (uint64_t)ncap * 4u;
            rx_addr blk;
            PUSHW(1, (bytes >> 32) ? 0xffffffffu : (uint32_t)bytes);
            ACT_CALLR(blk, 1, rx_reflex_aligned_alloc((bytes >> 32) ? 0xffffffffu : (uint32_t)bytes));
            PUSHW(3, blk, U32(arr, 0), 4u * U32(arr, 8));
            ACT_CALL(3, (void)crt_memcpy(blk, U32(arr, 0), 4u * U32(arr, 8)));
            SU32(L, 0x14, blk);
            PUSHW(1, U32(arr, 0));
            ACT_CALL(1, rx_reflex_aligned_free(U32(arr, 0)));
            SU32(arr, 8, ncap);
            SU32(arr, 0, blk);
        }
        SU32(U32(arr, 0), 4u * U32(arr, 4), sim);
        SU32(arr, 4, U32(arr, 4) + 1);
    }
    if (U32(v, 0x17c) != 0) {
        const rx_addr h = U32(v, 0x17c);
        move_body(L, U32(h, 4), U32(h, 8), pw);
    }
    {
        const rx_addr scene = U32(U32(v, 0x188), 0x20138);
        rr_os_cs_enter(scene + 4);
        const uint32_t id = U32(scene, 0x19f4);
        SU32(scene, 0x19f4, id + 1);
        SU32(v, 0x14, id);
        rr_os_cs_leave(scene + 4);
        PUSHW(1, scene);
        ACT_CALL(1, rx_scene_manager_add_entity(v, scene));
    }
    if (U32(v, 0x1a0) != 0) {
        PUSHW(1, world);
        ACT_CALL(1, rx_rider_world_move(U32(v, 0x1a0), world));
    }
    tracks_table_search(U32(world, 0x20138));
    {
        const rx_addr ta = U32(0, 0x00a95774), tb = U32(0, 0x00a95778);
        int32_t s;
        PUSHW(1,l->track_type); ACT_CALLR(s, 1, rx_tracks_alloc_slot(ta,l->track_type)); SU32(v, l->tracks+0, (uint32_t)s | 0x1000);
        PUSHW(1,l->track_type); ACT_CALLR(s, 1, rx_tracks_alloc_slot(ta,l->track_type)); SU32(v, l->tracks+4, (uint32_t)s | 0x1000);
        PUSHW(1,l->track_type); ACT_CALLR(s, 1, rx_tracks_alloc_slot(tb,l->track_type)); SU32(v, l->tracks+8, (uint32_t)s | 0x2000);
        PUSHW(1,l->track_type); ACT_CALLR(s, 1, rx_tracks_alloc_slot(tb,l->track_type)); SU32(v, l->tracks+12, (uint32_t)s | 0x2000);
    }
    SU32(v, 0x180, 0);
    ACT_END(L);
}

static void vehicle_detach(rx_addr v,const life_layout*l){
 /* Original pushECX local, then saved EBP and ESI; branch pushes EBX/EDI. */
 world_frame f=world_frame_flat(2,4);SU32(f.esp,8,v);
 const uint32_t scene=U32(U32(v,0x188),0x20138);
 if(!(U8(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}
 rx_addr terrain=0;
 for(uint32_t i=0;i<U32(0,0x9e30dc);++i){rx_addr t=U32(0,0x9e30d4+4*i);if(t&&scene==U32(t,0x44a8)){terrain=t;break;}}
 if(terrain){
  rx_set_sp(rx_sp()-8);const rx_addr t1=U32(0,0xa95774),t2=U32(0,0xa95778),t3=U32(0,0xa9577c);
  for(unsigned i=0;i<4;++i)free_slot_word(U32(v,l->tracks+4*i),t1,t2,t3);
  for(unsigned i=0;i<25;++i){int32_t w=I32(v,0x11b0+4*i);if(w>=0){w^=0x4000;if(w<I32(t3,0xc)&&w>=0){
   ACT_CALL(0,phys_tracks_cut_strip(t3,w));SU32(U32(t3,0x24),0x98+0xc0u*(uint32_t)w,1);SU32(t3,0x28,U32(t3,0x28)-1);
  }}}
  rx_set_sp(rx_sp()+8);
 }
 WORLD_CALL_SP(4,rx_world_physics_remove_simulation(U32(U32(v,0x188),0x20134),U32(v,l->sim)));
 if(U32(v,l->sim))WORLD_VCALL(U32(v,l->sim),0,1);
 SU32(v,l->sim,0);SU32(v,0x234,0);
 if(U8(v,0x174))WORLD_VCALL0(v+0x178,4);
 world_frame_leave(&f);rx_vehicle_base_detach(v);
}
static rx_addr vehicle_delete(rx_addr v,uint32_t flags,const life_layout*l){
 world_frame f=world_frame_flat(1,0);
 SU32(v,0,l->vt[0]);SU32(v,0x90,l->vt[1]);SU32(v,0x94,l->vt[2]);SU32(v,0x9c,l->vt[3]);
 WORLD_CALL_SP(0,rx_vehicle_base_destruct(v));
 if(flags&1)WORLD_CALL_SP(4,rx_reflex_aligned_free(v));
 world_frame_leave(&f);return v;
}
void rx_utv_vehicle_world_move(rx_addr v,rx_addr world){vehicle_world_move(v,world,&layouts[0]);}
RX_FUNCTION(0x004c6340,rx_utv_vehicle_world_move)
void rx_utv_vehicle_detach(rx_addr v){vehicle_detach(v,&layouts[0]);}
RX_FUNCTION(0x004c0a10,rx_utv_vehicle_detach)
rx_addr rx_utv_vehicle_delete(rx_addr v,uint32_t flags){return vehicle_delete(v,flags,&layouts[0]);}
RX_FUNCTION(0x004c06b0,rx_utv_vehicle_delete)
void rx_buggy_vehicle_world_move(rx_addr v,rx_addr world){vehicle_world_move(v,world,&layouts[1]);}
RX_FUNCTION(0x004ab9e0,rx_buggy_vehicle_world_move)
void rx_buggy_vehicle_detach(rx_addr v){vehicle_detach(v,&layouts[1]);}
RX_FUNCTION(0x004a2d10,rx_buggy_vehicle_detach)
rx_addr rx_buggy_vehicle_delete(rx_addr v,uint32_t flags){return vehicle_delete(v,flags,&layouts[1]);}
RX_FUNCTION(0x004a2910,rx_buggy_vehicle_delete)
void rx_truck_vehicle_world_move(rx_addr v,rx_addr world){vehicle_world_move(v,world,&layouts[2]);}
RX_FUNCTION(0x00472910,rx_truck_vehicle_world_move)
void rx_truck_vehicle_detach(rx_addr v){vehicle_detach(v,&layouts[2]);}
RX_FUNCTION(0x0046ad00,rx_truck_vehicle_detach)
rx_addr rx_truck_vehicle_delete(rx_addr v,uint32_t flags){return vehicle_delete(v,flags,&layouts[2]);}
RX_FUNCTION(0x0046a800,rx_truck_vehicle_delete)
