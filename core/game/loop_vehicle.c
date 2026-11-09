/* loop_vehicle.c -- vehicle glue: small RavenVehicle / RavenSimulationMotocross slots, the tuning and control-input setters, the
 * vehicle placement (pose from a matrix), the engine's dynamic-ray bounds and the object registry / vector helpers.
 *
 * Layout facts (V = RavenVehicle, S = RavenSimulationMotocross at V + 0x234):
 *   V +0x0f0 the 4x4 world matrix, +0x190 the player (MXPlayer), +0x1a0 the rider's controller, +0x1294 the model container
 *   (model +0x10, alternate +0x50), +0x1290.. the nine control inputs (written by 0045f530); S +0x2ef0.. +0x2efc the tuning
 *   factors the setter computes, +0x5c30 a table (+0x10 = the speed scale vf19 returns), +0x8aa0.. the scaled axis ranges. */
#include "loop_vehicle.h"
#include "fx.h"
#include "../src/rx_os.h"
#include "crt.h"
#include "loop_util.h"
#include "anim.h"
#include "anim_scenario.h"
#include "phys_body.h"
#include "phys_coll_misc.h"
#include "phys_coll_grid.h"
#include "phys_contact.h"
#include "build_world.h"
#include "fx_util.h"
#include "rays_box_observed.h"
#include <stdlib.h>

typedef struct { uint32_t eax, ecx, edx, ebx, esi, edi; } lv_in;

static rx_regs lv_run(rx_addr fn, lv_in in, int n, const uint32_t* args) {
    rx_regs r, o;
    memset(&r, 0, sizeof r);
    memset(&o, 0, sizeof o);
    r.r[0] = in.eax; r.r[1] = in.ecx; r.r[2] = in.edx; r.r[3] = in.ebx; r.r[6] = in.esi; r.r[7] = in.edi;
    rx_call_original_ex(fn, &r, &o, n, args);
    return o;
}
static uint32_t lv_call(rx_addr fn, lv_in in, int n, const uint32_t* args) { return lv_run(fn, in, n, args).r[0]; }
static uint32_t lv_call0(rx_addr fn, lv_in in) { return lv_run(fn, in, 0, 0).r[0]; }
static uint32_t lv_call1(rx_addr fn, lv_in in, uint32_t a) { return lv_run(fn, in, 1, &a).r[0]; }
static void lv_invalid_parameter(void) { crt_invalid_parameter_noinfo(); }   /* MSVCR90 _invalid_parameter_noinfo */
static float lv_abs(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu); }                                /* 00401000 Float_Abs */

/* ---- RavenVehicle slots ---- */

/* 004302f0 RavenVehicle::vf6_o94 (ECX = v; stack a0, a1, a2; ret 0xc): the root bounds of the vehicle's model (the alternate
 * model when the timing object's flag +0x54 is set): 007dd450 (EAX = model, EDI = a1; stack a0, a2). */
void rx_vehicle_root_bounds(rx_addr v, rx_addr a0, rx_addr a1, rx_addr a2) {
    rx_addr timing = U32(loop_ctx(), 0x10);
    rx_addr models = U32(v, 0x1294);
    const rx_addr model = U32(models, U32(timing, 0x54) == 0 ? 0x10 : 0x50);
    rx_set_sp(rx_sp() - 12u);                              /* the two arguments and the return address */
    rx_node_subtree_bounds(model, a1, a0, a2);
    rx_set_sp(rx_sp() + 12u);
}
RX_FUNCTION(0x004302f0, rx_vehicle_root_bounds)

/* 00430240 RavenVehicle::vf14: the address of the world matrix. */
rx_addr rx_vehicle_matrix_ptr(rx_addr v) { return v + 0xf0; }
RX_FUNCTION(0x00430240, rx_vehicle_matrix_ptr)

/* 00430220 RavenVehicle::vf15 (stack out; ret 4): copy the 64-byte world matrix. */
void rx_vehicle_get_matrix(rx_addr v, rx_addr out) {
    for (uint32_t i = 0; i < 0x40; i += 4) SU32(out, i, U32(v, 0xf0 + i));
}
RX_FUNCTION(0x00430220, rx_vehicle_get_matrix)

/* 004392e0 RavenSimulationMotocross::vf19: a scalar of the simulation's table (+0x5c30 -> +0x10), returned in st(0). */
float rx_sim_vf19(rx_addr sim) { return F32(U32(sim, 0x5c30), 0x10); }
RX_FUNCTION(0x004392e0, rx_sim_vf19)

/* 0043be80 RavenSimulationMotocross::vf35: scale the axis ranges by the tuning factors (+0x2ef0..0x2efc) and set the three "limit"
 * records (+0x5c34.. , +0x5c48.., +0x5c5c..: value, scale 1.0) for the finite ones. */
void rx_sim_apply_tuning(rx_addr c) {
    const float f0 = F32(c, 0x2ef0);
    SF32(c, 0x8aa0, F32(c, 0x409c) * f0);
    SF32(c, 0x8aa4, F32(c, 0x40e4) * f0);
    SF32(c, 0x8aa8, F32(c, 0x412c) * f0);
    SF32(c, 0x8aac, F32(c, 0x3ea4) * f0);
    SF32(c, 0x8ab0, F32(c, 0x3eec) * f0);
    const float r5 = F32(c, 0x3f34) * f0;
    const float f1 = (F32(c, 0x54ec) * F32(c, 0x533c)) * F32(c, 0x2ef4);
    SF32(c, 0x8ab4, r5);
    SF32(c, 0x8ac4, F32(c, 0x5384) * f1);
    SF32(c, 0x8ac8, F32(c, 0x53cc) * f1);
    SF32(c, 0x8acc, F32(c, 0x5414) * f1);
    SF32(c, 0x8ad0, F32(c, 0x545c) * f1);
    const float r10 = F32(c, 0x54a4) * f1;
    const float x = F32(c, 0x35ec) * F32(c, 0x2ef8);
    SF32(c, 0x5cc0, x);
    const float y = F32(c, 0x3af4) * F32(c, 0x2efc);
    const float none = rx_bf(0x7f7fffffu);                /* 0x977494: FLT_MAX = "no limit" */
    SF32(c, 0x8ad4, r10);
    SF32(c, 0x5cc4, y);
    SF32(c, 0x5c34, 0.0f);
    if (y != none) { SF32(c, 0x5c38, y); SF32(c, 0x5c3c, 1.0f); }
    const float y2 = F32(c, 0x5cc4);
    SF32(c, 0x5c48, 0.0f);
    if (y2 != none) { SF32(c, 0x5c4c, y2); SF32(c, 0x5c50, 1.0f); }
    const float x2 = F32(c, 0x5cc0);
    SF32(c, 0x5c5c, 0.0f);
    if (x2 != none) { SF32(c, 0x5c60, x2); SF32(c, 0x5c64, 1.0f); }
}
RX_FUNCTION(0x0043be80, rx_sim_apply_tuning)

/* 0045f270 (EDI = v, ESI = cur; stack def; ret 4): the garage tuning sliders (cur / def: pairs of ints at +0, +4 / +8, +0xc / +0x10,
 * +0x14; +0x18 and +0x1c percentages) become the vehicle's ratios (+0x220 .. +0x230) and the simulation's tuning factors
 * (S +0x2ef0 .. +0x2efc); then the simulation's slot 0x8c (rx_sim_apply_tuning) runs. */
static float lv_tune_ratio(int32_t alo, int32_t ahi, int32_t blo, int32_t bhi) {
    float m = 0.0f;
    if (blo != alo && bhi != ahi) m = (float)(blo > bhi ? bhi : blo);
    float w = (float)ahi - m;                 /* xmm2 */
    float u = (float)alo - m;                 /* xmm0 */
    float t1 = rx_bf(0x80000000u /* 0x9774a8: -0.0 */) - w;
    float num = t1 + u;
    float den = lv_abs(u) + lv_abs(t1);
    return rx_divs(num, den);
}
void rx_vehicle_set_tuning(rx_addr v, rx_addr cur, rx_addr def) {
    SF32(v, 0x220, lv_tune_ratio(I32(cur, 8), I32(cur, 0xc), I32(def, 8), I32(def, 0xc)));
    SF32(v, 0x224, lv_tune_ratio(I32(cur, 0x10), I32(cur, 0x14), I32(def, 0x10), I32(def, 0x14)));
    SF32(v, 0x228, lv_tune_ratio(I32(cur, 0), I32(cur, 4), I32(def, 0), I32(def, 4)));
    const float scale = 0.01f /* 0x9322ac */;
    SF32(v, 0x22c, (float)I32(cur, 0x18) * scale);
    SF32(v, 0x230, (float)I32(cur, 0x1c) * scale);
    const float tenth = 0.1f /* 0x931d94 */;
    float r1 = F32(v, 0x220) * tenth;
    SF32(U32(v, 0x234), 0x2ef0, 1.0f - r1);
    SF32(v, 0x224, F32(v, 0x224) * tenth);
    SF32(U32(v, 0x234), 0x2ef4, F32(v, 0x228) * tenth + 1.0f);
    const float two = 2.0f /* 0x92e330 */, half = 0.5f /* 0x9322cc */;
    float a = two - F32(v, 0x230) * two;
    if (half > a) a = half;
    SF32(U32(v, 0x234), 0x2ef8, a);
    float b = two - F32(v, 0x22c) * two;
    if (half > b) b = half;
    SF32(U32(v, 0x234), 0x2efc, b);
    rx_addr s = U32(v, 0x234);
    loop_tc0(loop_vslot(s, 0x8c), s);
}
RX_FUNCTION(0x0045f270, rx_vehicle_set_tuning)

/* 0045f530 (ECX = v, EAX = src): the nine control inputs (0x24 bytes) into v + 0x1290. */
void rx_vehicle_set_control_inputs(rx_addr v, rx_addr src) {
    for (uint32_t i = 0; i < 0x24; i += 4) SU32(v, 0x1290 + i, U32(src, i));
}
RX_FUNCTION(0x0045f530, rx_vehicle_set_control_inputs)

/* 0045f590 (EDI = v): release the vehicle's controls: a local player's input manager is told (slot 0xc0: 1, controller slot), the
 * simulation's flag +0x280 and the object +0x2374 cleared, the rider's controller (+0x1a0) reset (00623500, ESI = it). */
void rx_vehicle_release_controls(rx_addr v) {
    rx_addr pl = U32(v, 0x190);
    if (pl && U32(pl, 4) != 0 && U32(pl, 0x10) == 0) {
        rx_addr ctx = loop_ctx();
        uint32_t slot = U32(pl, 0x1f0);
        rx_addr im = loop_tc0(loop_vslot(ctx, 8), ctx);
        loop_tc2(loop_vslot(im, 0xc0), im, 1, slot);
    }
    SU32(v, 0x280, 0);
    SU32(U32(v, 0x234), 0x2374, 0);
    rx_addr c = U32(v, 0x1a0);
    if (c) { rx_set_sp(rx_sp() - 4u); rx_rider_animation_restart(c); rx_set_sp(rx_sp() + 4u); }
}
RX_FUNCTION(0x0045f590, rx_vehicle_release_controls)

/* 004a3870 (ECX = v; stack on; ret 4): the world body's flags (record +0x2c -> +0x14 of the body handle v +0x17c): bits 8 / 0x10
 * cleared when `on`, set otherwise. */
void rx_vehicle_set_body_flags(rx_addr v, uint32_t on) {
    for (int pass = 0; pass < 2; pass++) {
        const uint32_t bit = pass == 0 ? 8u : 0x10u;
        rx_addr h = U32(v, 0x17c);
        if (h == 0) continue;
        rx_set_sp(rx_sp() - 4u);
        rx_addr obj = phys_body_record(h);
        rx_set_sp(rx_sp() + 4u);
        rx_addr rec = U32(obj, 0x2c);
        SU32(rec, 0x14, on ? (U32(rec, 0x14) & ~bit) : (U32(rec, 0x14) | bit));
    }
}
RX_FUNCTION(0x004a3870, rx_vehicle_set_body_flags)

/* 0042a1c0 (stack obj; ret 4): for the 16 body handles at obj + 0xc (stride 8: world, body index) the body record's +0x14 is set to
 * -1 (the world's body vector +0x1960, 0xe0 bytes each, record pointer +0x2c); obj +0x94 = 1. */
void rx_vehicle_clear_handles(rx_addr obj) {
    for (uint32_t i = 0; i < 16; i++) {
        rx_addr e = U32(obj, 0xc + 8 * i);
        if (e != 0) {
            rx_addr world = U32(e, 4);
            uint32_t idx = U32(e, 8);
            int32_t sz = (int32_t)(U32(world, 0x1964) - U32(world, 0x1960));
            int32_t hi = (int32_t)(((int64_t)(int32_t)0x92492493 * sz) >> 32);   /* imul by the magic for / 224 */
            int32_t q = (hi + sz) >> 7;
            uint32_t count = (uint32_t)q + ((uint32_t)q >> 31);
            if (!(idx < count)) lv_invalid_parameter();
            rx_addr rec = U32(U32(world, 0x1954 + 0xc) + idx * 0xe0u, 0x2c);
            SU32(rec, 0x14, 0xffffffffu);
        }
        SU8(obj, 0x94, 1);
    }
}
RX_FUNCTION(0x0042a1c0, rx_vehicle_clear_handles)

/* 0042d8c0 (ECX = src vec3, EDX = three destination pointers; stack f; ret 4): per axis, a record (value, f, 1.0) is written to
 * the destination when the value / f differ from FLT_MAX ("no limit"). */
void rx_vehicle_set_limits(rx_addr src, rx_addr dst, float f) {
    const float none = rx_bf(0x7f7fffffu);   /* 0x977494 */
    for (uint32_t i = 0; i < 3; i++) {
        rx_addr d = U32(dst, 4 * i);
        float x = F32(src, 4 * i);
        if (x != none) SF32(d, 0, x);
        if (f != none) { SF32(d, 4, f); SF32(d, 8, 1.0f); }
    }
}
RX_FUNCTION(0x0042d8c0, rx_vehicle_set_limits)

/* 00845050 RavenPhysics::CollisionShapeDynamicRays::vf1 (ECX = rays; stack min, max; ret 8): the AABB of the ray segments
 * (count +0x20, array +0x24: 0x20 bytes per ray, start at +0, end at +0x10); an empty set gives the vector at 0x00d61670. */
void rx_rays_bounds(rx_addr rays, rx_addr mn, rx_addr mx) {
    uint32_t n = U32(rays, 0x20);
    if (n == 0) {
        rx_st4(mn, rx_ld4(0x00d61670));
        rx_st4(mx, rx_ld4(0x00d61670));
        return;
    }
    rx_addr a = U32(rays, 0x24);
    rx_v4 s = rx_ld4(a), e = rx_ld4(a + 0x10), r;
    for (int i = 0; i < 4; i++) r.v[i] = rx_minss(s.v[i], e.v[i]);
    rx_st4(mn, r);
    a = U32(rays, 0x24);
    s = rx_ld4(a); e = rx_ld4(a + 0x10);
    for (int i = 0; i < 4; i++) r.v[i] = rx_maxss(s.v[i], e.v[i]);
    rx_st4(mx, r);
    if ((int32_t)n > 1) {
        uint32_t off = 0x20;
        for (uint32_t k = n - 1; k != 0; k--, off += 0x20) {
            rx_v4 acc = rx_ld4(mn), t;
            t = rx_ld4(U32(rays, 0x24) + off);
            for (int i = 0; i < 4; i++) acc.v[i] = rx_minss(acc.v[i], t.v[i]);
            rx_st4(mn, acc);
            t = rx_ld4(U32(rays, 0x24) + off + 0x10);
            for (int i = 0; i < 4; i++) acc.v[i] = rx_minss(acc.v[i], t.v[i]);
            rx_st4(mn, acc);
            acc = rx_ld4(mx);
            t = rx_ld4(U32(rays, 0x24) + off);
            for (int i = 0; i < 4; i++) acc.v[i] = rx_maxss(acc.v[i], t.v[i]);
            rx_st4(mx, acc);
            t = rx_ld4(U32(rays, 0x24) + off + 0x10);
            for (int i = 0; i < 4; i++) acc.v[i] = rx_maxss(acc.v[i], t.v[i]);
            rx_st4(mx, acc);
        }
    }
}
RX_FUNCTION(0x00845050, rx_rays_bounds)

/* ---- the object registry (global std::vector of object pointers at 0x00d6d8e0, lock 0x00d6d8f8) ---- */

/* 008499d0 (EAX = newsize, EDI = vector; vector: +0 allocator proxy, +0xc begin, +0x10 end, +0x14 capacity): vector<void*>::resize
 * with value 0. Growing goes to 006c8260 (insert n copies), shrinking to the iterator helpers 00842fa0 / 007cf450. */
void rx_vector_resize(rx_addr vec, uint32_t newsize) {
    RX_FRAME_ENTER(F, 0x20);
    const uint32_t begin = U32(vec, 0xc), end = U32(vec, 0x10);
    const uint32_t size = (uint32_t)((int32_t)(end - begin) >> 2);
    SU32(F, 8, 0);
    if (newsize > size) {
        if (begin > end) lv_invalid_parameter();
        uint32_t cur = (uint32_t)((int32_t)(U32(vec, 0x10) - U32(vec, 0xc)) >> 2);
        uint32_t a[5] = {vec, newsize - cur, F + 8, U32(vec, 0), end};
        {   /* the five pushed arguments and the return address of the call at 0x00849a16 */
            const rx_addr sp = rx_sp();
            for (uint32_t i = 0; i < 5; i++) SU32(sp, -4u * (5u - i), a[i]);
            SU32(sp, -24u, 0x00849a1bu);
            rx_set_sp(sp - 24u);
            rx_fx_vector_insert_n(a[0], a[1], a[2], a[3], a[4]);
            rx_set_sp(sp);
        }
    } else if (newsize < size) {
        if (begin > end) lv_invalid_parameter();
        uint32_t b2 = U32(vec, 0xc);
        SU32(F, 0x10, U32(vec, 0));
        SU32(F, 8, b2);
        if (b2 > U32(vec, 0x10)) { lv_invalid_parameter(); b2 = U32(F, 8); }
        uint32_t proxy = U32(vec, 0);
        SU32(F, 8, proxy);
        SU32(F, 0xc, b2);
        rx_set_sp(rx_sp() - 8u); phys_iterator_advance(F + 8, (int32_t)newsize); rx_set_sp(rx_sp() + 8u);
        uint32_t last_proxy = U32(F, 0x10), first_ptr = U32(F, 0xc), first_proxy = U32(F, 8);
        uint32_t a[5] = {F + 0x10, first_proxy, first_ptr, last_proxy, end};
        (void)a;
        rx_set_sp(rx_sp() - 24u); (void)phys_vector_erase(vec, F + 0x10, first_ptr, end); rx_set_sp(rx_sp() + 24u);
    }
    RX_FRAME_LEAVE(F, 0x20);
}
RX_FUNCTION(0x008499d0, rx_vector_resize)

/* 00844170 (ECX = self): the base-class destructor of a registered object: its vtable becomes 0x00976584 and every occurrence in
 * the registry is replaced by the last element, the registry shrinking by one (all under the critical section 0x00d6d8f8). */
void rx_registry_remove(rx_addr self) {
    const uint32_t lock = 0x00d6d8f8, vec = 0x00d6d8e0;
    SU32(self, 0, 0x00976584);
    uint32_t cs = lock;
    rx_os_import(0x0091600c, 1, &cs);
    uint32_t end = U32(0, 0x00d6d8f0), begin = U32(0, 0x00d6d8ec);
    fx_set_reg(FX_EDX,end); /*84418e: actual registry end; leave import preserves it.*/
    const uint32_t n = (uint32_t)((int32_t)(end - begin) >> 2);
    if (n != 0) {
        uint32_t i = 0;
        if (!(i < n)) { lv_invalid_parameter(); end = U32(0, 0x00d6d8f0); fx_set_reg(FX_EDX,end); begin = U32(0, 0x00d6d8ec); }
        for (;;) {
            if (U32(begin, i * 4) == self) {
                uint32_t last = n - 1;
                uint32_t cur = (uint32_t)((int32_t)(end - begin) >> 2);
                if (!(last < cur)) { lv_invalid_parameter(); end = U32(0, 0x00d6d8f0); fx_set_reg(FX_EDX,end); begin = U32(0, 0x00d6d8ec); }
                fx_set_reg(FX_EDX,(uint32_t)((int32_t)(end-begin)>>2)); /*8441e6..e8*/
                uint32_t src = begin + last * 4;
                cur = (uint32_t)((int32_t)(end - begin) >> 2);
                if (!(i < cur)) { lv_invalid_parameter(); begin = U32(0, 0x00d6d8ec); }
                uint32_t v = U32(src, 0); fx_set_reg(FX_EDX,v); /*8441fa: actual moved record, before resize*/
                SU32(begin, i * 4, v);
                rx_vector_resize(vec, last);
                end = U32(0, 0x00d6d8f0); fx_set_reg(FX_EDX,end); /*84420b: final live EDX*/
                begin = U32(0, 0x00d6d8ec);
            }
            uint32_t cnt = (uint32_t)((int32_t)(end - begin) >> 2);
            i++;
            if (!(i < cnt)) break;
        }
    }
    cs = lock;
    rx_os_import(0x009160f8, 1, &cs);
}
RX_FUNCTION(0x00844170, rx_registry_remove)

/* ---- ray cast against the dynamic bodies ---- */

/* the SSE vector helpers of the ray code: lane sums in the order the shuffles give them */
static float lv_sum_xyz(rx_v4 a) { return rx_adds(rx_adds(a.v[0], a.v[1]), a.v[2]); }   /* shufps 0x99: (x + y) + z */
static float lv_sum_xzy(rx_v4 a) { return rx_adds(rx_adds(a.v[0], a.v[2]), a.v[1]); }   /* shufps 0x66: (x + z) + y */
static rx_v4 lv_vsub(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_subs(a.v[i], b.v[i]); return r; }
static rx_v4 lv_vmul(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_muls(a.v[i], b.v[i]); return r; }

/* v / |v| with the game's SSE fallbacks (0x92dc20 = +inf lanes, 0x92dc30 = quiet NaN lanes): zero for a zero vector, NaN lanes when
 * the squared length overflows */
static rx_v4 lv_normalize(rx_v4 v) {
    rx_v4 sq = lv_vmul(v, v);
    float l2 = lv_sum_xyz(sq);
    float len = rx_sqrts(l2);
    rx_v4 r;
    for (int i = 0; i < 4; i++) {
        uint32_t q = rx_fb(rx_divs(v.v[i], len));
        if (!(len != 0.0f)) q = 0;
        r.v[i] = rx_bf(l2 != rx_bf(0x7f800000u) ? q : 0x7fc00000u);
    }
    return r;
}

/* 0085d360 PhysicsWorld_RayCastBodies (cdecl world, rays, n, hits, mask): for each of the n rays (start +0, end +0x10, 0x20 bytes)
 * find the nearest point on the dynamic bodies of the world's broad-phase cells (bodies 0x40 bytes at world +0x19c0; record
 * +0x28 the shape, shape +0x10 the layer bits tested against `mask`). hits: 0x30 bytes per ray: +0 byte hit, +0x10 point (starts
 * at 1e10), +0x20 normal. The rays are wrapped in a temporary RavenPhysics::CollisionShapeDynamicRays object that lives in this
 * function's frame and is registered in the global object registry (0x00d6d8e0) for the duration -- so the frame is the original's. */
void rx_physics_world_raycast_bodies(rx_addr world, rx_addr rays, int32_t n, rx_addr hits, uint32_t mask) {
    const rx_addr sp0 = rx_sp();
    const rx_addr B = ((sp0 - 4) & ~15u) - 0x734, S = B - 12;
    SU32(B, 0x730, U32(0, 0x009b45e4) ^ B);
    SU32(B, 4, world);
    SU32(B, 8, hits);
    rx_set_sp(S);
    const rx_addr obj = S + 0x18, set = S + 0x80, box_min = S + 0x60, box_max = S + 0x50;
    if (n > 0) {
        for (int32_t i = 0; i < n; i++) {
            rx_addr h = hits + 0x30u * (uint32_t)i;
            SU8(h, 0, 0);
            rx_st4(h + 0x10, rx_v4_splat(rx_bf(U32(0, 0x00977b88))));
            rx_st4(h + 0x20, rx_ld4(0x00d61670));
        }
    }
    /* the temporary shape: a CollisionShape base object (vtable 0x976584) registered, then turned into the dynamic-rays shape */
    uint32_t cs = 0x00d6d8f8;
    SU32(obj, 4, 0);
    SU32(obj, 0, 0x00976584);
    SU32(obj, 8, 0x20);
    SU32(obj, 0xc, 1);
    SU32(obj, 0x10, 1);
    SU32(obj, 0x14, 0);
    SU32(obj, 0x18, 0);
    SU32(obj, 0x1c, 0);
    rx_os_import(0x0091600c, 1, &cs);
    SU32(S, 0xc, obj);                      /* the pointer the push_back reads */
    {
        rx_set_sp(rx_sp() - 8u); rx_vec_push_back_ptr(0x00d6d8e0, S + 0xc); rx_set_sp(rx_sp() + 8u);
    }
    cs = 0x00d6d8f8;
    rx_os_import(0x009160f8, 1, &cs);
    SU32(obj, 0x24, rays);
    SU32(obj, 0x20, (uint32_t)n);
    SU32(obj, 0, 0x00976650);               /* CollisionShapeDynamicRays */
    SU32(obj, 0xc, 0xa1);
    for (uint32_t k = 0; k < 0x1e; k++) { SU32(S, 0x630 + 4 * k, 0xffffffffu); SU32(S, 0x6a8 + 4 * k, 0xffffffffu); }
    rx_rays_bounds(obj, box_min, box_max);
    rx_addr grid_it = U32(world, 0x19cc);
    {
        rx_set_sp(rx_sp() - 8u); phys_grid_query_boxes(U32(grid_it, 0x30), box_min, box_max); rx_set_sp(rx_sp() + 8u);
    }
    SU32(grid_it, 0, 0);
    for (;;) {
        rx_addr it = U32(world, 0x19cc);
        rx_addr cells = U32(it, 0x30);
        int32_t cursor = I32(it, 0);
        if (cursor >= I32(cells, 0x400)) break;
        cursor++;
        SU32(it, 0, (uint32_t)cursor);
        uint32_t bi = U32(cells, (uint32_t)cursor * 4 - 4);
        if (bi == 0xffffffffu) break;
        uint32_t nb = (uint32_t)((int32_t)(U32(world, 0x19c4) - U32(world, 0x19c0)) >> 6);
        if (!(bi < nb)) lv_invalid_parameter();
        rx_addr body = (bi << 6) + U32(world, 0x19c0);
        if (mask != 0) {
            if ((U32(U32(body, 0x28), 0x10) & mask) == 0) continue;
        }
        rx_st4(S + 0x60, rx_ld4(body));
        rx_st4(S + 0x70, rx_ld4(body + 0x10));
        SU32(S, 0x720, 0);
        SU32(S, 0x80, 0);
        for (uint32_t k = 0; k < 0x1e; k++) { SU32(S, 0x630 + 4 * k, 0xffffffffu); SU32(S, 0x6a8 + 4 * k, 0xffffffffu); }
        {
            rx_set_sp(rx_sp() - 32u);
            phys_contact_dispatch(set, obj, 0x00d61640, 0x00d61670, U32(body, 0x28), S + 0x70, S + 0x60, 0);
            rx_set_sp(rx_sp() + 32u);
        }
        if (U32(S, 0x720) == 0) continue;
        int32_t count = I32(S, 0x80);
        if (count <= 0) continue;
        rx_addr rec = S + 0x90;
        for (int32_t c = count; c != 0; c--, rec += 0x30) {
            rx_v4 p = rx_ld4(rec);                         /* contact point (xmm5) */
            rx_v4 best = rx_ld4(0x00d615e0);              /* xmm6: best distance, splat */
            uint32_t bray = 0;
            for (int32_t r = 0; r < n; r++) {
                rx_addr ray = rays + 0x20u * (uint32_t)r;
                rx_v4 st = rx_ld4(ray), en = rx_ld4(ray + 0x10);
                rx_v4 dir = lv_normalize(lv_vsub(en, st));
                rx_v4 rel = lv_vsub(p, st);
                float dot = lv_sum_xyz(lv_vmul(rel, dir));
                rx_v4 proj;
                for (int i = 0; i < 4; i++) proj.v[i] = rx_muls(dot, dir.v[i]);
                rx_v4 perp = lv_vsub(rel, proj);
                float d = rx_sqrts(lv_sum_xzy(lv_vmul(perp, perp)));
                if (d < best.v[0]) { bray = (uint32_t)r; best = rx_v4_splat(d); }
            }
            rx_addr h = hits + 0x30u * bray;
            rx_v4 old = rx_ld4(h + 0x10);
            SU8(h, 0, 1);
            rx_v4 st = rx_ld4(rays + 0x20u * bray);
            float d_old2 = lv_sum_xzy(lv_vmul(lv_vsub(old, st), lv_vsub(old, st)));
            rx_v4 dn = lv_vsub(p, st);
            float d_new2 = lv_sum_xzy(lv_vmul(dn, dn));
            if (rx_sqrts(d_new2) < rx_sqrts(d_old2)) {
                rx_st4(h + 0x10, p);
                rx_v4 nrm = rx_ld4(rec + 0x10), z = rx_v4_splat(0.0f);
                rx_st4(h + 0x20, lv_vsub(z, nrm));
            }
        }
    }
    rx_set_sp(S - 4);                        /* call 00844170 (ECX = obj) */
    rx_registry_remove(obj);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0085d360, rx_physics_world_raycast_bodies)

/* ---- dynamic rays vs box ---- */

/* the rotation matrix of a unit quaternion as the scalar code builds it (x2 = x * 2 ..., xx = x * x2 ...), rows m[r][c] */
typedef struct { float m[3][3]; } lv_rot;
/* Optional existing computed terms for exact frame observation; no second matrix evaluation. */
typedef struct {float x2,y2,z2,xx,yy,zz,wx,wy,wz,xy,yz,xz;} lv_rot_terms;
static lv_rot lv_quat_rows_terms(rx_addr q,lv_rot_terms*terms){
 const float x=F32(q,0),y=F32(q,4),z=F32(q,8),w=F32(q,12),two=2.0f,one=1.0f;
 const float x2=rx_muls(x,two),y2=rx_muls(y,two),z2=rx_muls(z,two);
 const float zz=rx_muls(z,z2),xx=rx_muls(x,x2),yy=rx_muls(y,y2);
 const float wx=rx_muls(w,x2),wy=rx_muls(w,y2),wz=rx_muls(w,z2);
 const float xy=rx_muls(y,x2),yz=rx_muls(z,y2),xz=rx_muls(z,x2);
 if(terms)*terms=(lv_rot_terms){x2,y2,z2,xx,yy,zz,wx,wy,wz,xy,yz,xz};
 lv_rot r;
 r.m[0][0]=rx_subs(one,rx_adds(zz,yy));r.m[0][1]=rx_adds(xy,wz);r.m[0][2]=rx_subs(xz,wy);
 r.m[1][0]=rx_subs(xy,wz);r.m[1][1]=rx_subs(one,rx_adds(zz,xx));r.m[1][2]=rx_adds(yz,wx);
 r.m[2][0]=rx_adds(xz,wy);r.m[2][1]=rx_subs(yz,wx);r.m[2][2]=rx_subs(one,rx_adds(yy,xx));return r;
}

static lv_rot lv_quat_rows(rx_addr q){return lv_quat_rows_terms(q,0);}

/* 0086a1e0 (LTCG: ECX = rays shape, EDX = the rays' frame translation, EDI = contact set; stack box, q_box, t_box, q_rays, invert;
 * ret 0x14): the segments of a DynamicRays shape against a box. Both segment ends are taken into the box's frame (the rays'
 * frame: rotation q_rays, translation EDX; the box's: q_box / t_box), and each of the six slab planes is intersected with the
 * segment (t = (+-half - start) / direction, the direction nudged by 1e-5 when >= 0); a hit inside the plane's rectangle and
 * 0 <= t <= 1 is appended to the contact set EDI (count +0 capped at 29, records of 0x30 bytes from +0x10: point, normal [flipped
 * when `invert`], then |(c - t) * direction| in all lanes; +0x6a0 = 1). The records are 16-byte copies of stack temporaries whose
 * 4th lanes the original never writes: they are read from the original's frame (so the frame address is the original's). */
/* Scalar semantic stages shared by ordinary and exact entries. Observation only
 * publishes already-computed values and the original frame/register dataflow. */
static void lv_box_xyz(rx_addr E,uint32_t off,const float v[3]){for(unsigned c=0;c<3;c++)SF32(E,off+4*c,v[c]);}
static rx_v4 lv_box_scalar_live(float x){rx_v4 r=rx_v4_splat(0.0f);r.v[0]=x;return r;}
static void lv_box_rows(rx_addr E,uint32_t off,const lv_rot*r){
 for(unsigned j=0;j<3;j++){rx_v4 v=rx_v4_splat(0.0f);for(unsigned c=0;c<3;c++)v.v[c]=r->m[j][c];rx_st4(E+off+16*j,v);}
}
static void collision_rays_box_kernel(rx_addr rays,rx_addr box,rx_addr q_box,rx_addr t_box,rx_addr q_rays,uint32_t invert,rx_addr t_rays,rx_addr set,rx_v4*live){
 const rx_addr entry=rx_sp();fx_frame f;
 if(live)fx_frame_enter(&f,0x318,16,FX_EBX,FX_ESI);
 const rx_addr E=live?f.E:((entry-4)&~15u)-0x320;
 const float tiny=rx_bf(U32(0,0x9774a4)),one=1.0f,negz=rx_bf(0x80000000u);
 lv_rot_terms rt;
 const lv_rot B=lv_quat_rows(q_box);
 const float tb[3]={F32(t_box,0),F32(t_box,4),F32(t_box,8)};
 const lv_rot R=lv_quat_rows_terms(q_rays,live?&rt:0);
 const float tr[3]={F32(t_rays,0),F32(t_rays,4),F32(t_rays,8)};
 const float h[3]={F32(box,0x20),F32(box,0x24),F32(box,0x28)};
 const float nh[3]={rx_subs(negz,h[0]),rx_subs(negz,h[1]),rx_subs(negz,h[2])};
 float N[6][3];
 for(unsigned a=0;a<3;a++)for(unsigned c=0;c<3;c++){float v=F32(0,0x9cb4a0+16*a+4*c);N[2*a][c]=v;N[2*a+1][c]=rx_subs(negz,v);}
 const int32_t n=I32(rays,0x20);
 if(live){
  /* First matrix/translation survive as full aligned copies at2e0/310;
   * the second uses the original40..78 scratch. All rowW=0, translationW=1. */
  rx_st4(E+0x30,rx_ld4(box+0x20));lv_box_rows(E,0x2e0,&B);lv_box_rows(E,0x40,&R);
  lv_box_xyz(E,0x310,tb);SF32(E,0x31c,one);lv_box_xyz(E,0x70,tr);SF32(E,0x7c,one);
  SF32(E,0xc,rt.wz);SF32(E,0x10,rt.z2);SF32(E,0x14,rt.xx);SF32(E,0x18,rt.yz);SF32(E,0x1c,rt.wy);SF32(E,0x28,rt.wx);
  SU32(E,0x88,rays);for(unsigned k=0;k<6;k++)lv_box_xyz(E,0x280+16*k,N[k]);
  fx_set_reg(FX_EAX,(uint32_t)n);fx_set_reg(FX_EDX,rays);fx_set_reg(FX_ECX,0);fx_set_reg(FX_ESI,t_box);fx_set_reg(FX_EBX,box);*live=lv_box_scalar_live(negz);
  if(n>0){SF32(E,0x2c,nh[2]);SF32(E,0x14,nh[1]);SF32(E,0x1c,nh[0]);SU32(E,0xc,0);SU32(E,0x18,(uint32_t)n);fx_set_reg(FX_EBX,1);*live=lv_box_scalar_live(nh[0]);}
 }
 for(int32_t r=0;r<n;r++){
  const rx_addr base=U32(rays,0x24),ray=base+0x20u*(uint32_t)r;
  float s[3],e[3];for(unsigned c=0;c<3;c++){s[c]=F32(ray,4*c);e[c]=F32(ray,0x10+4*c);}
  float ws[3],we[3];
  for(unsigned pass=0;pass<2;pass++){
   const float*v=pass?e:s;float*w=pass?we:ws;
   /* Original worldX starts R10*y before R00*x; Y/Z use X first. */
   w[0]=rx_adds(rx_adds(rx_adds(rx_muls(R.m[1][0],v[1]),rx_muls(v[0],R.m[0][0])),rx_muls(R.m[2][0],v[2])),tr[0]);
   w[1]=rx_adds(rx_adds(rx_adds(rx_muls(R.m[0][1],v[0]),rx_muls(R.m[1][1],v[1])),rx_muls(R.m[2][1],v[2])),tr[1]);
   w[2]=rx_adds(rx_adds(rx_adds(rx_muls(R.m[0][2],v[0]),rx_muls(R.m[1][2],v[1])),rx_muls(R.m[2][2],v[2])),tr[2]);
   for(unsigned c=0;c<3;c++)w[c]=rx_subs(w[c],tb[c]);
  }
  float ls[3],le[3];
  for(unsigned pass=0;pass<2;pass++){
   const float*w=pass?we:ws;float*l=pass?le:ls;
   for(unsigned j=0;j<3;j++)l[j]=rx_adds(rx_adds(rx_muls(B.m[j][0],w[0]),rx_muls(B.m[j][1],w[1])),rx_muls(B.m[j][2],w[2]));
  }
  const float d[3]={rx_subs(le[0],ls[0]),rx_subs(le[1],ls[1]),rx_subs(le[2],ls[2])};float dd[3]={d[0],d[1],d[2]};
  for(unsigned c=0;c<3;c++)if(d[c]>=0.0f)dd[c]=rx_adds(d[c],tiny);
  float t[6];for(unsigned a=0;a<3;a++){t[2*a]=rx_divs(rx_subs(h[a],ls[a]),dd[a]);t[2*a+1]=rx_divs(rx_subs(nh[a],ls[a]),dd[a]);}
  float P[6][3],products[6][3];for(unsigned k=0;k<6;k++)for(unsigned c=0;c<3;c++){products[k][c]=rx_muls(d[c],t[k]);P[k][c]=rx_adds(products[k][c],ls[c]);}
  if(live){
   SF32(E,0xbc,ws[0]);SF32(E,0x11c,ws[2]);SF32(E,0xdc,we[0]);SF32(E,0xfc,we[2]);
   SF32(E,0x10,d[0]);SF32(E,0x24,d[1]);SF32(E,0x20,d[2]);SF32(E,0x28,dd[2]);
   for(unsigned k=0;k<6;k++)SF32(E,0x8c+4*k,t[k]);
   const uint32_t cached[6]={0x100,0xe0,0xc0,0x140,0x120,0x200},product[6]={0x160,0x1a0,0x150,0x250,0x210,0x230};
   for(unsigned k=0;k<6;k++){
    lv_box_xyz(E,cached[k],P[k]);lv_box_xyz(E,0x1b0+16*k,P[k]);
    /*86acf5..86ae91 spill only X/Y for products3..5. Their Z values
     *remain in SIMD registers; E258/E218/E238 retain incoming bytes. */
    for(unsigned c=0;c<(k<3?3u:2u);c++)SF32(E,product[k]+4*c,products[k][c]);
   }
   for(unsigned off=0x170;off<=0x190;off+=0x10)lv_box_xyz(E,off,d);
   for(unsigned off=0x224;off<=0x274;off+=(off==0x224?0x40:0x10)){SF32(E,off,d[1]);SF32(E,off+4,d[2]);}
   fx_set_reg(FX_EDX,base);*live=lv_box_scalar_live(P[5][1]); /*86aeb4 MOVSS clears upper96*/
  }
  static const unsigned axA[6]={1,1,0,0,1,1},axB[6]={2,2,2,2,0,0};int hit[6];
  for(unsigned k=0;k<6;k++){
   unsigned a=axA[k],b=axB[k];hit[k]=t[k]>=0.0f&&one>=t[k]&&h[a]>=P[k][a]&&P[k][a]>=nh[a]&&h[b]>=P[k][b]&&P[k][b]>=nh[b];
   if(live)SU32(E,0xa4+4*k,(uint32_t)hit[k]);
  }
  for(unsigned k=0;k<6;k++){
   if(!hit[k])continue;const float px=P[k][0],py=P[k][1],pz=P[k][2];float wp[3],normal[3];
   wp[0]=rx_adds(rx_adds(rx_adds(rx_muls(px,B.m[0][0]),rx_muls(py,B.m[1][0])),rx_muls(pz,B.m[2][0])),tb[0]);
   wp[1]=rx_adds(rx_adds(rx_adds(rx_muls(py,B.m[1][1]),rx_muls(px,B.m[0][1])),rx_muls(pz,B.m[2][1])),tb[1]);
   wp[2]=rx_adds(rx_adds(rx_adds(rx_muls(py,B.m[1][2]),rx_muls(px,B.m[0][2])),rx_muls(pz,B.m[2][2])),tb[2]);
   for(unsigned c=0;c<3;c++){normal[c]=rx_adds(rx_adds(rx_muls(N[k][0],B.m[0][c]),rx_muls(N[k][1],B.m[1][c])),rx_muls(N[k][2],B.m[2][c]));if(invert&255)normal[c]=rx_subs(negz,normal[c]);}
   if(live){lv_box_xyz(E,0x1b0+16*k,wp);lv_box_xyz(E,0x130,normal);lv_box_xyz(E,0x240,d);}
   int32_t count=I32(set,0);if(!(count<0x1d))count=0x1d;const rx_addr rec=set+(uint32_t)count*0x30;
   lv_box_xyz(rec,0x10,wp);SU32(rec,0x1c,U32(E,0x1bc+16*k));lv_box_xyz(rec,0x20,normal);SU32(rec,0x2c,U32(E,0x13c));
   const rx_v4 c4=rx_ld4(0xd61650);float a[3];for(unsigned c=0;c<3;c++)a[c]=rx_muls(rx_subs(c4.v[c],t[k]),d[c]);
   const float distance=rx_sqrts(rx_adds(rx_adds(rx_muls(a[0],a[0]),rx_muls(a[2],a[2])),rx_muls(a[1],a[1])));
   for(unsigned c=0;c<4;c++)SF32(rec,0x30+4*c,distance);SU32(set,0,(uint32_t)(count+1));SU32(set,0x6a0,1);
   if(live){fx_set_reg(FX_EDX,(uint32_t)(count+1));*live=rx_v4_splat(distance);}
  }
  if(live){SU32(E,0xc,0x20u*((uint32_t)r+1));SU32(E,0x18,(uint32_t)n-(uint32_t)r-1);fx_set_reg(FX_EAX,0x60);fx_set_reg(FX_ECX,0);}
 }
 if(live){fx_set_reg(FX_ESI,U32(E,0));fx_set_reg(FX_EBX,U32(E,4));fx_set_reg(FX_EBP,U32(f.ebp,0));rx_set_sp(entry);}
}
void rx_collision_rays_vs_box(rx_addr rays,rx_addr box,rx_addr q_box,rx_addr t_box,rx_addr q_rays,uint32_t invert,rx_addr t_rays,rx_addr set){collision_rays_box_kernel(rays,box,q_box,t_box,q_rays,invert,t_rays,set,0);}
#ifdef RX_COLLISION_NATIVE_COUNTS
extern void rx_collision_native_count(uint32_t target,uint32_t return_pc);
#endif
void rx_collision_rays_vs_box_observed(rx_addr rays,rx_addr box,rx_addr q_box,rx_addr t_box,rx_addr q_rays,uint32_t invert,rx_addr t_rays,rx_addr set,rx_v4*live){
 if(!live)abort();
#ifdef RX_COLLISION_NATIVE_COUNTS
 rx_collision_native_count(0x0086a1e0,U32(rx_sp(),0));
#endif
 collision_rays_box_kernel(rays,box,q_box,t_box,q_rays,invert,t_rays,set,live);
}
RX_FUNCTION(0x0086a1e0, rx_collision_rays_vs_box)
