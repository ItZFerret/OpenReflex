/* Original RavenVehicleATV runtime leaves. Sources: original disassembly at the
 * registered addresses, _re/out/decomp/reflex_001_00459400.c. No motorcycle offsets. */
#include "vehicle_atv_runtime.h"
#include "build_vehicle_util.h"
#include "world_veh.h"
#include "vehicle_saved_pose.h"
#include "phys_terrain.h"
#include "phys_vec.h"
#include "world_veh_util.h"
#include "world_node.h"
#include "world_rider.h"
#include "world_rider_util.h"
#include "world_wreck.h"
#include "anim.h"
#include "world_misc.h"
#include "world_veh_transfer.h"
#include "build_activity_util.h"
#include "build_rider_util.h"
#include "build_vehicle.h"
#include "build_alloc.h"
#include "anim_extern.h"
#include "phys_body.h"
#include "bike_sim.h"
#include "bike_vehicle.h"
#include "phys_crt.h"
#include "phys_math.h"
#include "vehicle_atv_physics.h"
#include "vehicle_atv_wheels.h"

static void copy_sim_vector(rx_addr v,rx_addr out,uint32_t offset){rx_st4(out,rx_ld4(U32(v,0x1700)+offset));}
void rx_atv_vehicle_position(rx_addr v,rx_addr out){copy_sim_vector(v,out,0x180);} RX_FUNCTION(0x00486560,rx_atv_vehicle_position)
void rx_atv_vehicle_rotation(rx_addr v,rx_addr out){copy_sim_vector(v,out,0x7c0);} RX_FUNCTION(0x00486580,rx_atv_vehicle_rotation)
void rx_atv_vehicle_velocity(rx_addr v,rx_addr out){copy_sim_vector(v,out,0x100);} RX_FUNCTION(0x004865a0,rx_atv_vehicle_velocity)
void rx_atv_vehicle_angular_velocity(rx_addr v,rx_addr out){copy_sim_vector(v,out,0x740);} RX_FUNCTION(0x004865c0,rx_atv_vehicle_angular_velocity)
void rx_atv_vehicle_body_velocity(rx_addr v,rx_addr out){copy_sim_vector(v,out,0x160);} RX_FUNCTION(0x004865e0,rx_atv_vehicle_body_velocity)
void rx_atv_vehicle_post_step(rx_addr v){
 int32_t phase=I32(0,0x00ad3ed4)%3;
 if(phase==1||phase==2){
  rx_atv_post_step(U32(v,0x1700));
 }
}
RX_FUNCTION(0x00486520,rx_atv_vehicle_post_step)
uint32_t rx_atv_vehicle_safe_pose(rx_addr v,rx_addr position,rx_addr rotation,uint32_t thresholdBits,uint32_t unused){
 const float threshold=rx_bf(thresholdBits);(void)unused;
 if(!veh_vcall(v,0xb4,0,0,0,0)||veh_vcall(v,0xb8,0,0,0,0)||U32(v,0x280)||U32(U32(v,0x234),0xc))return 0;
 rx_addr s=U32(v,0x1700);uint32_t flags=0;
 for(int32_t i=0;i<I32(s,0x1318);i++)flags|=U32(s,0x1af0+4*(uint32_t)i)|U32(s,0x1b54+4*(uint32_t)i);
 if(flags)return 0;
 if(!(U32(0,0x009e30e0)&1)){SU32(0,0x009e30e0,U32(0,0x009e30e0)|1);SU32(0,0x009e30dc,0);}
 rx_addr terrain=rx_world_find_by_controller(0x009e30d4);
 if(phys_terrain_material_at(terrain,v+0xc0,0,NULL,NULL)==6)return 0;
 float radius=F32(terrain,0x44f0);
 /* COMISS/JA rejects ordered greater only; unordered bounds continue. */
 if(F32(terrain,0x44d8)>phys_add(F32(v,0xc0),radius)||phys_sub(F32(v,0xc0),radius)>F32(terrain,0x44e0)||
    F32(terrain,0x44dc)>phys_add(F32(v,0xc8),radius)||phys_sub(F32(v,0xc8),radius)>F32(terrain,0x44e4))return 0;
 s=U32(v,0x1700);
 if(!(F32(s,0x9274)>threshold)||!(F32(s,0x93c4)>threshold)||!(F32(s,0x9514)>threshold)||!(F32(s,0x9664)>threshold))return 0;
 rx_st4(position,rx_ld4(s+0x180));rx_st4(rotation,rx_ld4(U32(v,0x1700)+0x7c0));return 1;
}
RX_FUNCTION(0x00486100,rx_atv_vehicle_safe_pose)

/* ATV V35, original SSE double-precision rotation blocks and rider publication. */
static void veh_rotate_d(float x, float y, float z, float w, float s0f, float s1f, float s2f, float out[3]) {
    const double s0 = s0f, s1 = s1f, s2 = s2f, c = 0.0, X = x, Y = y, Z = z, W = w;
    const float nx = phys_sub(-0.0f, x), ny = phys_sub(-0.0f, y), nz = phys_sub(-0.0f, z);
    const float A = (float)(((W * c - s0 * X) - s1 * Y) - s2 * Z);
    const float B = (float)(((s0 * W + X * c) + s2 * Y) - s1 * Z);
    const float C = (float)(((s1 * W + Y * c) + s0 * Z) - s2 * X);
    const float D = (float)(((s2 * W + Z * c) + s1 * X) - s0 * Y);
    out[0] = (float)(((W * B + (double)A * nx) + (double)nz * C) - (double)ny * D);
    out[1] = (float)(((W * C + (double)ny * A) + (double)D * nx) - (double)nz * B);
    out[2] = (float)(((W * D + (double)nz * A) + (double)ny * B) - (double)C * nx);
}

#define XYZ_MASK 0x0092db80u
static void rec_vec(rx_addr fn, rx_addr rec, rx_addr v) {
    void (*f)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
    if (f) WORLD_CALL_SP(0, f(rec, v)); else WORLD_CTX0(fn, &WCTX(.ecx = rec, .eax = v), 0);
}

static void rec_float(rx_addr fn, rx_addr rec, float x) {
    void (*f)(rx_addr, float) = RX_FN(fn, void (*)(rx_addr, float));
    if (f) WORLD_CALL_SP(0, f(rec, x)); else WORLD_CTX0(fn, &WCTX(.eax = rec, .xmm0 = v4(x, 0.0f, 0.0f, 0.0f)), 0);
}

static uint32_t vcall(rx_addr self, uint32_t slot) { return WORLD_VCALL0(self, slot); }

static void gesture_feed(rx_addr veh, rx_addr g, float dtp) {
    rec_vec(0x00430500, g, veh + 0xc0);                                   /* RiderRecord_SetVec60 */
    const uint32_t air = vcall(veh, 0xac);
    const uint32_t old = U32(g, 0x94);
    SU32(g, 0x94, air);
    SU32(g, 0x90, old);
    rec_vec(0x00430520, g, veh + 0x130);                                  /* RiderRecord_SetVec70 */
    rec_vec(0x00430540, g, veh + 0x140);                                  /* RiderRecord_SetVec80 */
    WORLD_CALL_SP(4, rx_world_gesture_update(g, dtp));
}
void rx_atv_vehicle_publish_rider(rx_addr veh, float dt) {
    world_frame f = world_frame_enter(0xa4, 3, 1);
    const rx_addr S = f.esp;
    const float dtp = rx_bf(world_x87_pass(rx_fb(dt)));
    WORLD_CALL_SP(4, (void)rx_hierarchy_find_node(U32(veh, 0x1328) + 0x10, 0x0093bff4));
    WORLD_VCALL(U32(veh, 0x1a0), 0x40, veh + 0x240);
    rx_addr sim = U32(veh, 0x1700);
    {
        const rx_v4 q = v4_ld(sim + 0x730), v = v4_and(v4_ld(sim + 0x160), v4_ld(XYZ_MASK));
        const rx_v4 t = veh_quat_mul(v, q);
        v4_st(U32(veh, 0x1a0) + 0x5b9f0, veh_quat_mul(v4_mul(v4(-1.0f, -1.0f, -1.0f, 1.0f), q), t));
    }
    {
        rx_addr ra = U32(veh, 0x1a0);
        SF32(ra, 0x5bad0, 1.0f); SF32(ra, 0x5bad4, 0.0f); SF32(ra, 0x5bad8, 0.0f);
        SF32(ra, 0x5bae0, 0.0f); SF32(ra, 0x5bae4, 1.0f); SF32(ra, 0x5bae8, 0.0f);
        SF32(ra, 0x5baf0, 0.0f); SF32(ra, 0x5baf4, 0.0f); SF32(ra, 0x5baf8, 1.0f);
        for (uint32_t a = 0x5bad0; a <= 0x5baf0; a += 0x10) {
            ra = U32(veh, 0x1a0);
            float o[3];
            veh_rotate_d(F32(veh, 0xe0), F32(veh, 0xe4), F32(veh, 0xe8), F32(veh, 0xec), F32(ra, a), F32(ra, a + 4), F32(ra, a + 8), o);
            SF32(ra, a, o[0]); SF32(ra, a + 4, o[1]); SF32(ra, a + 8, o[2]);
        }
    }
    {   /* the velocity in the model frame (rows of +0xf0) */
        const rx_addr m = veh + 0xf0;
        const float vx = F32(veh, 0x160), vy = F32(veh, 0x164), vz = F32(veh, 0x168);
        const rx_addr ra = U32(veh, 0x1a0);
        for (uint32_t r = 0; r < 3; r++) {
            const rx_addr row = m + 0x10 * r;
            const float d = phys_add(phys_add(phys_add(phys_mul(vy, F32(row, 4)), phys_mul(vx, F32(row, 0))),
                                              phys_mul(vz, F32(row, 8))), F32(row, 0xc));
            SF32(ra, 0x5bb10 + 4 * r, d);
        }
    }
    WORLD_VCALL(veh, 0x98, S + 0x30);
    {
        float o[3];
        veh_rotate_d(F32(veh, 0xe0), F32(veh, 0xe4), F32(veh, 0xe8), F32(veh, 0xec), F32(S, 0x30), F32(S, 0x34), F32(S, 0x38), o);
        SF32(S, 0x30, o[0]); SF32(S, 0x34, o[1]); SF32(S, 0x38, o[2]);
        const rx_addr ra = U32(veh, 0x1a0);
        SF32(ra, 0x5bb20, o[0]); SF32(ra, 0x5bb24, o[1]); SF32(ra, 0x5bb28, o[2]);
    }
    {
        rx_addr ra = U32(veh, 0x1a0);
        for (uint32_t i = 0; i < 0xc; i += 4) SU32(ra, 0x5bb00 + i, world_x87_pass(U32(veh, 0x160 + i)));
        sim = U32(veh, 0x1700);
        SF32(ra, 0x5bb30, U32(sim, 0x90) != 0 ? 0.0f : F32(veh, 0x150));
        const int fresh = (U32(0, 0x00db46f0) & 1u) == 0;
        SU32(ra, 0x5bb34, world_x87_pass(U32(sim, 0x1c)));
        SU32(ra, 0x5b97c, world_x87_pass(U32(sim, 0x99a0)));
        SU32(ra, 0x5b528, world_x87_pass(U32(sim, 0x2c)));
        SU32(ra, 0x5b52c, world_x87_pass(U32(sim, 0x30)));
        SU32(ra, 0x5b530, world_x87_pass(U32(sim, 0x34)));
        SU32(ra, 0x5b534, world_x87_pass(U32(sim, 0x38)));
        SU32(ra, 0x5b53c, 0);
        const float px = F32(veh, 0x270), py = F32(veh, 0x274), pz = F32(veh, 0x278);
        if (fresh) {
            SU32(0, 0x00db46f0, U32(0, 0x00db46f0) | 1u);
            SF32(0, 0x00db46e0, 0.0f); SF32(0, 0x00db46e4, 0.0f); SF32(0, 0x00db46e8, 0.0f);
        }
        ra = U32(veh, 0x1a0);
        const float dx = phys_sub(px, F32(0, 0x00db46e0)), dy = phys_sub(py, F32(0, 0x00db46e4)), dz = phys_sub(pz, F32(0, 0x00db46e8));
        const float d2 = phys_add(phys_add(phys_mul(dy, dy), phys_mul(dx, dx)), phys_mul(dz, dz));
        uint32_t moving = 0;
        if (0.0f > F32(ra, 0x5bb18)) {
            sim = U32(veh, 0x1700);
            if ((F32(sim, 0x1c) == 0.0f || !(F32(sim, 0x20) == 0.0f)) && U32(sim, 0x90) == 0 && d2 > F32(0, 0x009cc8a4))
                moving = 1;
        }
        SU32(ra, 0x5bb54, moving);
        SF32(0, 0x00db46e0, px); SF32(0, 0x00db46e4, py); SF32(0, 0x00db46e8, pz);
    }
    {
        rx_addr ra = U32(veh, 0x1a0);
        WORLD_CALL_SP(0, rx_world_wreck_state_ramp_to(ra + 0x5b940, F32(U32(veh, 0x1700), 0x1c)));
        const uint32_t trig = vcall(veh, 0xa8);
        const rx_addr ws = U32(veh, 0x1a0) + 0x5b940;
        if (trig != U32(ws, 0x10)) {
            if (U32(ws, 0) != 2) SU32(ws, 4, U32(ws, 0));
            SU32(ws, 0x14, 1);
            SU32(ws, 0, 2);
            SU32(ws, 0x10, trig);
        }
        const uint32_t lvl = vcall(veh, 0xa8);
        WORLD_CALL_SP(0, rx_world_gesture_set_level(U32(veh, 0x1a0) + 0x5b760, (int32_t)lvl));
        const uint32_t hold = !(F32(U32(veh, 0x1700), 0x28) == 0.0f);
        WORLD_CALL_SP(0, rx_world_gesture_set_hold(U32(veh, 0x1a0) + 0x5b850, hold));
        SU32(U32(veh, 0x1a0), 0x2b8, U32(U32(veh, 0x190), 8));
        ra = U32(veh, 0x1a0);
        rec_vec(0x00430490, ra + 0x5b590, veh + 0x160);                              /* RiderRecord_SetVec90 */
        rec_float(0x004304b0, U32(veh, 0x1a0) + 0x5b590, F32(veh, 0x150));           /* SetFloatA0 */
        ra = U32(veh, 0x1a0);
        rec_float(0x004304d0, ra + 0x5b590, F32(ra, 0x5b530));                       /* SetFloatA8 */
        SU32(U32(veh, 0x1a0), 0x5b63c, vcall(veh, 0xac));
        ra = U32(veh, 0x1a0);
        rec_float(0x004304c0, ra + 0x5b590, F32(ra, 0x5b528));                       /* SetFloatA4 */
        SU32(U32(veh, 0x1a0), 0x5b5dc, veh + 0xf0);
        rec_float(0x004304e0, U32(veh, 0x1a0) + 0x5b590, F32(U32(veh, 0x1700), 0x99a0));   /* SetFloat88 */
        WORLD_CALL_SP(4, rx_world_lookat_update(U32(veh, 0x1a0) + 0x5b590, dtp));
        const rx_addr lb = U32(veh, 0x1a0) + 0x5b6e4;
        if (U32(lb, 0xc) != 0) SU32(lb, 0x14, U32(U32(veh, 0x1700), 0x740));
        ra = U32(veh, 0x1a0);
        WORLD_CALL_SP(0, rx_world_blend_set_18(ra + 0x5b6e4, rider_asin(F32(ra, 0x5bad4))));
        WORLD_CALL_SP(0, rx_world_blend_set_1c(U32(veh, 0x1a0) + 0x5b6e4, F32(U32(veh, 0x1700), 0x99a0)));
        SU32(U32(veh, 0x1a0), 0x5b764, 0);
        gesture_feed(veh, U32(veh, 0x1a0) + 0x5b760, dtp);
        SU32(U32(veh, 0x1a0), 0x5b854, 1);
        gesture_feed(veh, U32(veh, 0x1a0) + 0x5b850, dtp);
        WORLD_CALL_SP(4, rx_world_wreck_state_advance(U32(veh, 0x1a0) + 0x5b940, dtp));
        SU32(U32(veh, 0x1a0), 0x5bab8, U32(veh, 0x194));
        ra = U32(veh, 0x1a0);
        SU32(ra, 0x5bac0, U32(ra, 0x5babc));
        SU32(U32(veh, 0x1a0), 0x5babc, vcall(veh, 0xac));
        SU32(U32(veh, 0x1a0), 0x5baa8, U32(U32(veh, 0x234), 0x2eb8));
        const rx_addr grp = U32(U32(veh, 0x190), 0x94);
        SU32(U32(veh, 0x1a0), 0x5b548, grp != 0 && U32(grp, 0x21c) == 0);
        SU32(U32(veh, 0x1a0), 0x5bb4c, vcall(veh, 0xb0));
        SU32(U32(veh, 0x1a0), 0x5bb50, vcall(veh, 0xe8));
        ra = U32(veh, 0x1a0);
        world_vcall_f(ra, 0x4c, dtp);             /* RiderAnimation::vf19 */
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00484c50,rx_atv_vehicle_publish_rider)

/* 004847e0: original ATV world transfer, all four track slots are type2. */

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
void rx_atv_vehicle_world_move(rx_addr v, rx_addr world) {
    ACT_FRAME_EBP(L, 8, 0xc, 3);
    SU32(L, 0x14, v);
    const rx_addr oldw = U32(v, 0x188);
    PUSHW(1, v ? v + 0x90 : 0);
    ACT_CALL(1, rx_world_physics_remove_listener(U32(oldw, 0x20134), v ? v + 0x90 : 0));
    PUSHW(1, U32(v, 0x1700));
    ACT_CALL(1, rx_world_physics_remove_simulation(U32(U32(v, 0x188), 0x20134), U32(v, 0x1700)));
    PUSHW(1, U32(U32(v, 0x188), 0x20138));
    ACT_CALL(1, rx_scene_manager_remove_entity(v, U32(U32(v, 0x188), 0x20138)));
    tracks_table_search(U32(U32(v, 0x188), 0x20138));
    const rx_addr t1 = U32(0, 0x00a95774), t3 = U32(0, 0x00a9577c);
    {
        const rx_addr t2 = U32(0, 0x00a95778);
        free_slot_word(U32(v, 0x1584), t1, t2, t3);
        free_slot_word(U32(v, 0x1588), t1, t2, t3);
        free_slot_word(U32(v, 0x158c), t1, t2, t3);
        free_slot_word(U32(v, 0x1590), t1, t2, t3);
    }
    {
        rx_addr p = v + 0x11b0;
        SU32(L, 0xc, p); SU32(L, 0x10, 0x19);
        for (int n = 0x19; n != 0; n--) {
            for (int k = 0; k < 2; k++) {
                int32_t w = (int32_t)U32(p, k == 0 ? 0x310 : 0);
                if (w >= 0) {
                    w ^= 0x4000;
                    if (w < I32(t3, 0xc) && w >= 0) {
                        ACT_CALL(0, phys_tracks_cut_strip(t3, w));
                        SU32(U32(t3, 0x24), 0x98 + 0xc0u * (uint32_t)w, 1);
                        SU32(t3, 0x28, U32(t3, 0x28) - 1);
                    }
                }
            }
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
        const rx_addr arr = pw + 0x1940, sim = U32(v, 0x1700);
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
        PUSHW(1, 2); ACT_CALLR(s, 1, rx_tracks_alloc_slot(ta, 2)); SU32(v, 0x1584, (uint32_t)s | 0x1000);
        PUSHW(1, 2); ACT_CALLR(s, 1, rx_tracks_alloc_slot(ta, 2)); SU32(v, 0x1588, (uint32_t)s | 0x1000);
        PUSHW(1, 2); ACT_CALLR(s, 1, rx_tracks_alloc_slot(tb, 2)); SU32(v, 0x158c, (uint32_t)s | 0x2000);
        PUSHW(1, 2); ACT_CALLR(s, 1, rx_tracks_alloc_slot(tb, 2)); SU32(v, 0x1590, (uint32_t)s | 0x2000);
    }
    SU32(v, 0x180, 0);
    ACT_END(L);
}
RX_FUNCTION(0x004847e0,rx_atv_vehicle_world_move)

/* 00489c80 EAX=V, stack dt (RET4); preserves the original x87 argument copy. */
void rx_atv_vehicle_contact_filters(rx_addr v,float dt){
 if(!U32(v,0x280))WORLD_CALL_SP(4,rx_atv_update_contact_filters(U32(v,0x1700),rx_bf(world_x87_pass(rx_fb(dt)))));
}
RX_FUNCTION(0x00489c80,rx_atv_vehicle_contact_filters)

static int frame_phase_02(void) {
    const int32_t n = I32(0, 0x00ad3ed4), m = n - (n / 3) * 3;
    return m == 0 || m == 2;
}

static void node_world_position(rx_addr node, rx_addr local, rx_addr out) {
    WORLD_CALL_SP(4, rx_world_node_evaluate_world(node, 0));
    const rx_addr m = U32(U32(node, 0), 0x1c) + ((uint32_t)U8(node, 0xb) << 6);
    for (uint32_t i = 0; i < 0x40; i += 4) SU32(local, i, U32(m, i));
    SU32(out, 0, U32(local, 0x30)); SU32(out, 4, U32(local, 0x34)); SU32(out, 8, U32(local, 0x38));
}

static void quat_matrix_rows(rx_addr out, float x, float y, float z, float w) {
    const veh_rot r = veh_quat_rows(x, y, z, w);
    for (int i = 0; i < 3; i++) {
        SF32(out, 0x10 * i + 0, r.m[i][0]); SF32(out, 0x10 * i + 4, r.m[i][1]); SF32(out, 0x10 * i + 8, r.m[i][2]);
        SF32(out, 0x10 * i + 0xc, 0.0f);
    }
}
void rx_atv_vehicle_update(rx_addr veh, float dt) {
    world_frame f = world_frame_enter(0xc4, 3, 1);
    const rx_addr S = f.esp;
    SU32(S, 0xcc, U32(0, 0x009b45e4) ^ (S + 0xc));
    const float dtp = rx_bf(world_x87_pass(rx_fb(dt)));
    rx_addr sim = U32(veh, 0x1700);
    uint32_t diverged;
    WORLD_CALL_SP(0, diverged = bike_state_divergence_check(sim));
    if (diverged != 0) {
        v4_st(S + 0x20, v4_ld(U32(veh, 0x1700) + 0x170));
        WORLD_CALL_SP(0, rx_vehicle_restore_saved_pose(veh));   /* 00460070 (EAX = veh); EBX / ESI are left alone */
    }
    sim = U32(veh, 0x1700);
    if (U32(sim, 4) == 0) WORLD_CALL_SP(4, bike_get_replay_state(sim, veh + 0x1220));   /* Simulation_GetReplayState */
    WORLD_CALL_SP(4, bike_replay_record_encode(veh + 0x12c0, U32(veh, 0x1700)));        /* ReplayRecord_Encode */
    WORLD_VCALL(veh, 0x90, S + 0xb0);
    v4_st(veh + 0xe0, v4_ld(S + 0xb0));
    sim = U32(veh, 0x1700);
    const rx_v4 com = veh_rotate(v4_ld(veh + 0xe0), v4_and(v4_ld(sim + 0x3640), v4_ld(XYZ_MASK)));
    v4_st(veh + 0xc0, v4_sub(v4_ld(sim + 0x180), com));
    WORLD_VCALL(veh, 0x94, S + 0xa0);
    v4_st(veh + 0x160, v4_ld(S + 0xa0));
    if (U32(U32(veh, 0x1700), 4) == 0) WORLD_CALL_SP(4, bike_vehicle_debug_view(veh, dtp));
    if ((U32(0, 0x00dcf2e0) & 1u) == 0) {
        SU32(0, 0x00dcf2e0, U32(0, 0x00dcf2e0) | 1u);
        SF32(0, 0x00dcf2d0, 0.0f); SF32(0, 0x00dcf2d4, -2.16700006f /* 0x977ee8 */); SF32(0, 0x00dcf2d8, 0.0f);
    }
    {   /* the model matrix: the offset (s0, s1, s2) rotated by the vehicle rotation, in double precision */
        const float x = F32(veh, 0xe0), y = F32(veh, 0xe4), z = F32(veh, 0xe8), w = F32(veh, 0xec);
        float o[3];
        veh_rotate_d(x, y, z, w, F32(0, 0x00dcf2d0), F32(0, 0x00dcf2d4), F32(0, 0x00dcf2d8), o);
        const float px = phys_add(F32(veh, 0xc0), o[0]), py = phys_add(F32(veh, 0xc4), o[1]), pz = phys_add(F32(veh, 0xc8), o[2]);
        quat_matrix_rows(veh + 0xf0, x, y, z, w);
        SF32(veh, 0x120, px); SF32(veh, 0x124, py); SF32(veh, 0x128, pz); SF32(veh, 0x12c, 1.0f);
        const veh_rot r = veh_quat_rows(F32(veh, 0xe0), F32(veh, 0xe4), F32(veh, 0xe8), F32(veh, 0xec));
        SF32(veh, 0x130, r.m[2][0]); SF32(veh, 0x134, r.m[2][1]); SF32(veh, 0x138, r.m[2][2]);
        SF32(veh, 0x140, r.m[1][0]); SF32(veh, 0x144, r.m[1][1]); SF32(veh, 0x148, r.m[1][2]);
    }
    {
        const float vx = F32(veh, 0x160), vy = F32(veh, 0x164), vz = F32(veh, 0x168);
        float speed;
        WORLD_CALL_SP(4, speed = phys_sqrtf(phys_add(phys_add(phys_mul(vx, vx), phys_mul(vy, vy)), phys_mul(vz, vz))));
        SF32(veh, 0x150, speed);
        if (1.00100005f /* 0x977ecc */ > speed)
            for (uint32_t i = 0; i < 0xc; i += 4) SU32(veh, 0x160 + i, world_x87_pass(U32(veh, 0x130 + i)));
    }
    if (frame_phase_02()) world_vcall_f(veh, 0xf0, dtp);   /* vf60 */
    node_world_position(U32(veh, 0x1a4), S + 0x20, veh + 0x1d0);
    node_world_position(U32(veh, 0x1a8), S + 0x20, veh + 0x1c0);
    node_world_position(U32(veh, 0x1ac), S + 0x20, veh + 0x1f0);
    node_world_position(U32(veh, 0x1b0), S + 0x20, veh + 0x1e0);
    if (U32(veh, 0x280) == 0) {
        sim = U32(veh, 0x1700);
        if (U32(sim, 0x2de0) != 0) {
            const rx_v4 q0 = v4_ld(sim + 0x2e00), v0 = v4_and(v4_ld(sim + 0x2df0), v4_ld(XYZ_MASK));
            const rx_v4 cj = v4(-1.0f, -1.0f, -1.0f, 1.0f);
            const rx_v4 u = veh_quat_mul(v4_mul(q0, cj), veh_quat_mul(v0, q0));
            const rx_v4 off = v4_and(v4_sub(v4_zero(), u), v4_ld(XYZ_MASK));
            const rx_v4 R = veh_quat_mul(v4_ld(veh + 0xe0), v4_ld(0x00ad66b0));
            const rx_v4 p = v4_add(veh_quat_mul(R, veh_quat_mul(off, v4_mul(R, cj))), v4_ld(veh + 0xc0));
            v4_st(S + 0x60, p);
            const rx_v4 r2 = veh_quat_mul(R, v4_mul(q0, cj));
            v4_st(S + 0x70, r2);
            quat_matrix_rows(veh + 0x240, r2.v[0], r2.v[1], r2.v[2], r2.v[3]);
            SF32(veh, 0x270, p.v[0]); SF32(veh, 0x274, p.v[1]); SF32(veh, 0x278, p.v[2]); SF32(veh, 0x27c, 1.0f);
        } else {
            WORLD_CALL_SP(0, rx_world_node_get_world_matrix(U32(veh, 0x1528), veh + 0x240));
        }
    } else if (U32(veh, 0x1a0) != 0) {
        WORLD_CALL_SP(0, rx_world_rider_root_matrix(U32(veh, 0x1a0), veh + 0x240));
    }
    if (frame_phase_02() && U32(veh, 0x1a0) != 0) world_vcall_f(veh, 0x8c, dtp);   /* vf35 */
    /* 00487b30 (stdcall veh, dt, RET8): the four-wheel contact / deformation / water update, called through its owner (vehicle_atv_wheels.c) */
    WORLD_CALL_SP(8, rx_atv_vehicle_wheels(veh, dtp));
    {
        sim=U32(veh,0x1700);float n[4];
        const uint32_t off[]={0x9210,0x94b0,0x9360,0x9600};
        for(unsigned i=0;i<4;i++){
            rx_v4 v=v4_ld(sim+off[i]);
            WORLD_CALL_SP(4,n[i]=phys_sqrtf(phys_add(phys_add(phys_mul(v.v[1],v.v[1]),phys_mul(v.v[0],v.v[0])),phys_mul(v.v[2],v.v[2]))));
        }
        float m=n[3]<n[1]?n[1]:n[3];float first=n[2]<n[0]?n[0]:n[2];
        if(m<first)m=first;if(m<0)m=0;
        WORLD_CALL_SP(0,rx_world_landing_rumble(veh,m));
        WORLD_CALL_SP(4,rx_world_vehicle_camera_update(veh,phys_div(m,3000.0f)));
        WORLD_CALL_SP(4,rx_atv_vehicle_contact_filters(veh,dtp));
    }
    phys_security_check_cookie(U32(S, 0xcc) ^ (S + 0xc));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0047d870,rx_atv_vehicle_update)
/* The original ATV predicate closure called by V34/V35. */
uint32_t rx_atv_vehicle_all_wheels_contact(rx_addr v){rx_addr s=U32(v,0x1700);return U32(s,0x9240)&&U32(s,0x9390)&&U32(s,0x94e0)&&U32(s,0x9630);}
RX_FUNCTION(0x00486720,rx_atv_vehicle_all_wheels_contact)
uint32_t rx_atv_vehicle_water_contact(rx_addr v){return U32(v,0x1630)||U32(v,0x1634)||U32(v,0x1628)||U32(v,0x162c);}
RX_FUNCTION(0x0047c470,rx_atv_vehicle_water_contact)
uint32_t rx_atv_vehicle_rider_trigger(rx_addr v){return U32(U32(v,0x1700),0x96f0)+1u;}
RX_FUNCTION(0x00486650,rx_atv_vehicle_rider_trigger)
uint32_t rx_atv_vehicle_airborne(rx_addr v){return U32(U32(v,0x1700),0x99a8);}
RX_FUNCTION(0x00486710,rx_atv_vehicle_airborne)
uint32_t rx_atv_vehicle_rider_flag(rx_addr v){return U32(U32(v,0x1700),0x99b0);}
RX_FUNCTION(0x00486780,rx_atv_vehicle_rider_flag)
uint32_t rx_atv_vehicle_rider_action(rx_addr v){
 rx_addr s=U32(v,0x1700);SU32(v,0x1760,0);
 const uint32_t field[]={0x98e0,0x98e4,0x98e8,0x9870,0x9874,0x9878,0x9954,0x996c};
 for(unsigned i=0;i<8;i++)if(U32(s,field[i]))SU32(v,0x1760,i+1);
 return U32(v,0x1760);
}
RX_FUNCTION(0x00486660,rx_atv_vehicle_rider_action)
