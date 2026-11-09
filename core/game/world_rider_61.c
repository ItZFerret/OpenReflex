/* world_rider_61.c -- the rider animation's per-frame update (RiderAnimation, 0x0061xxxx .. 0x0062xxxx) as the physics frame
 * runs it: RiderAnimation::vf19 and the state / pose / input functions it calls.
 *
 * The rider animation object (`ra`, ~0x5bb70 bytes; as far as this code uses it):
 *   +0x2bc / +0x2c4 / +0x2c8 the worker job slot, its job and "no job" flag; +0x2d4 .. +0x458 the dt slots of its animators
 *   +0x308 / +0x334 / +0x360 / +0x38c float properties (listener vectors) of the pose weights
 *   +0x580 short property (pose state), +0x598 float property (target blend); +0x5a0 the short state the blend follows
 *   +0x5bc the skeleton (+0x60 bone count, +0x64 bones of 0x80 bytes: +0x10 -> the IK target vec3; +0x6c dt)
 *   +0x57e00 / +0x58760 / +0x590c0 / +0x59a20 / +0x5a380 poses; +0x5ace0 .. +0x5b104 keyframe tracks; +0x5adac bone count
 *   +0x5b1dc / +0x5b1e0 two node records, +0x5b1e4 / +0x5b1e8 their local points (index into the 0x30-byte table at
 *   ra + 0x5a390), +0x5b1ec / +0x5b1f0 their bones; +0x5b1f4 the overlay blend, +0x5b1f8 "overlay on"
 * core/notes/world.md (rider). */
#include "world_rider.h"
#include "anim_rider_stunt.h"
#include "world_util.h"
#include "world_wreck.h"
#include "world_rider_extern.h"
#include "world_node.h"
#include "anim_ik.h"
#include "world_rider_util.h"
#include "phys_misc.h"

/* 0061ca60 RiderAnim_BlendOverlay (ESI = ra, xmm1 = dt): with the overlay on (+0x5b1f8), its blend +0x5b1f4 follows the target
 * blend property (+0x598) while the pose state (+0x580) equals +0x5a0, else fades out at 3 / s (the rate is taken from the
 * first call's dt and kept in the static 0xdce9d8, guard bit 0xdce9dc; nothing happens once the blend is 0). Then three
 * keyframe blends into the poses +0x58760 / +0x57e00 / +0x59a20 with the weights of +0x38c / +0x360 (00840b70), two pose
 * blends weighted by +0x334 / +0x308 (00841c90), and the two IK targets (bones +0x5b1ec / +0x5b1f0) moved toward their nodes'
 * world points by the blend. */
void rx_world_rider_blend_overlay(rx_addr ra, float dt) {
    world_frame f = world_frame_enter(0x58, 2, 1);   /* push ebp; and esp, -16; sub esp, 0x58; push ebx; push edi */
    if (U32(ra, 0x5b1f8) != 0) {
        float blend;
        const rx_addr state = U32(ra, 0x580);
        const uint16_t cur = state ? U16(state, 0) : 0;
        int run = 1;
        if (U16(ra, 0x5a0) == cur) {
            const rx_addr target = U32(ra, 0x598);
            blend = target ? F32(target, 0) : rx_bf(0);
        } else {
            blend = F32(ra, 0x5b1f4);
            if (blend == 0.0f) {
                run = 0;   /* ucomiss / lahf / test ah, 0x44 / jnp: exit when exactly equal (a NaN goes on) */
            } else {
                if (!(U32(0, 0x00dce9dc) & 1)) {
                    SF32(0, 0x00dce9d8, phys_mul(dt, 3.0f /* 0x92fe48 */));
                    SU32(0, 0x00dce9dc, U32(0, 0x00dce9dc) | 1);
                }
                blend = phys_sub(blend, F32(0, 0x00dce9d8));
                if (!(blend > 0.0f)) blend = rx_bf(0);
            }
        }
        if (run) {
            SF32(ra, 0x5b1f4, blend);
            const uint32_t w1 = world_x87_pass(rx_fb(world_float_get(ra + 0x38c, WCTX(.esi = ra))));
            const uint32_t w2 = world_x87_pass(rx_fb(world_float_get(ra + 0x360, WCTX(.esi = ra))));
            const uint32_t bones = U32(ra, 0x5adac);
            const world_ctx c = WCTX(.ebx = ra + 0x58760, .esi = ra, .edi = bones);
            world_pose_blend_keys(ra + 0x5ace0, ra + 0x5adb4, w1, w1, w2, ra + 0x58760, c);
            const uint32_t w3 = world_x87_pass(rx_fb(world_float_get(ra + 0x334, c)));
            world_pose_blend_keys(ra + 0x5b030, ra + 0x5b104, w1, w1, w2, ra + 0x57e00, c);
            const uint32_t w4 = world_x87_pass(rx_fb(world_float_get(ra + 0x308, c)));
            world_pose_blend_keys(ra + 0x5ae88, ra + 0x5af5c, w1, w1, w2, ra + 0x59a20, c);
            world_pose_blend(ra + 0x57e00, ra + 0x590c0, ra + 0x58760, bones, w3, c);
            world_pose_blend(ra + 0x59a20, ra + 0x5a380, ra + 0x590c0, bones, w4, c);
            for (uint32_t k = 0; k < 2; k++) {
                const rx_addr rec = U32(ra, 0x5b1dc + 4 * k);
                const rx_addr node = U32(U32(rec, 0), 8) + 16u * U8(rec, 0xf);
                WORLD_CALL_SP(4, rx_world_node_evaluate_world(node, 0));
                const rx_addr m = U32(U32(node, 0), 0x1c) + 64u * U8(node, 0xb);   /* the node's world matrix (rows) */
                const rx_addr local = ra + (U32(ra, 0x5b1e4 + 4 * k) + 0x1e13u) * 0x30u;
                const float x = F32(local, 0), y = F32(local, 4), z = F32(local, 8);
                const float px = ((F32(m, 0x10) * y + F32(m, 0x20) * z) + F32(m, 0x00) * x) + F32(m, 0x30);
                const float py = ((F32(m, 0x14) * y + F32(m, 0x24) * z) + F32(m, 0x04) * x) + F32(m, 0x34);
                const float pz = ((F32(m, 0x18) * y + F32(m, 0x28) * z) + F32(m, 0x08) * x) + F32(m, 0x38);
                const rx_addr skel = U32(ra, 0x5bc);
                const uint32_t bone = U32(ra, 0x5b1ec + 4 * k);
                const rx_addr entry = bone < U32(skel, 0x60) ? U32(skel, 0x64) + bone * 0x80u : 0;
                const rx_addr tgt = U32(entry, 0x10);
                const float b = F32(ra, 0x5b1f4);
                const float tx = F32(tgt, 0), ty = F32(tgt, 4), tz = F32(tgt, 8);
                SF32(tgt, 0, tx + (px - tx) * b);
                SF32(tgt, 4, ty + (py - ty) * b);
                SF32(tgt, 8, tz + (pz - tz) * b);
            }
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061ca60, rx_world_rider_blend_overlay)

/* 0061d350 RiderAnim_UpdateLean (ESI = ra; stack dt; RET 4): unless +0x5b2e0 (then only +0x5ba90 = 0.3), the lean angles
 * +0x5b96c = asin(+0x5bad4) * k and +0x5b970 = asin(+0x5baf4) * k with k = +0x5b974 * -0.7; from them e = clamp(clamp(lean2,
 * -0.5, 0.5) + 0.5, 0.25, 0.75), +0x5ba60 *= 1 - 4 |e - 0.5|; the lean weight w = (1 - 2 max(0.5 - e, 0)) * (k' clamp(-lean1,
 * -0.5, 0.5)) + 0.5 (k' = clamp(1.1 - |2 (+0x378 - 0.5)|, 0, 1)) written through the clamped float +0x3e0; the lag filter
 * +0x5b9c0 toward 0.5 / 0 (+0x5baa8) into +0x5bab4, and +0x5ba90 = min(that + e, 0.5). The debug bytes 0xd94a9c / 0xd94a9d
 * force the weight to [0x9d788c] / +0x5ba90 to 0.5. [esp + 0x14] holds w (passed by address). */
void rx_world_rider_update_lean(rx_addr ra, float dt) {
    world_frame f = world_frame_flat(1, 0x14);   /* sub esp, 0x14; push edi */
    const rx_addr w_slot = f.esp + 0x14;
    if (U32(ra, 0x5b2e0) == 0) {
        const float k = F32(ra, 0x5b974) * -0.7f /* 0x97781c */;
        const float lean1 = phys_mul(rider_asin(F32(ra, 0x5bad4)), k);
        SF32(ra, 0x5b96c, lean1);
        const float lean2 = phys_mul(rider_asin(F32(ra, 0x5baf4)), k);
        SF32(ra, 0x5b970, lean2);
        const float c1 = rider_clamp(phys_sub(rx_bf(0x80000000u) /* 0x9774a8 */, lean1), -0.5f /* 0x9774b0 */, 0.5f);
        const float c2 = rider_clamp(lean2, -0.5f, 0.5f);
        const float e = rider_clamp(phys_add(c2, 0.5f /* 0x9322cc */), 0.25f /* 0x92ff84 */, 0.75f /* 0x935aa8 */);
        float d;
        WORLD_CALL_SP(0, d = rx_world_float_abs(phys_sub(e, 0.5f)));
        SF32(ra, 0x5ba60, phys_mul(phys_sub(1.0f, phys_mul(d, 4.0f /* 0x9322c0 */)), F32(ra, 0x5ba60)));
        const float g = world_ranged_float_get(ra + 0x378, WCTX(.esi = ra));
        float h;
        WORLD_CALL_SP(0, h = rx_world_float_abs(phys_mul(phys_sub(g, 0.5f), 2.0f /* 0x92e330 */)));
        const float kk = rider_clamp(phys_add(phys_sub(1.0f, h), 0.1f /* 0x931d94 */), rx_bf(0), 1.0f);
        const float u = phys_add(phys_mul(kk, c1), 0.5f);
        float v = phys_sub(phys_sub(1.0f, e), 0.5f);
        if (!(v > 0.0f)) v = rx_bf(0);
        const float w = phys_add(phys_mul(phys_sub(1.0f, phys_mul(v, 2.0f)), phys_sub(u, 0.5f)), 0.5f);
        SF32(w_slot, 0, w);
        world_clamped_float_write(ra + 0x3e0, w_slot, WCTX(.esi = ra, .edi = ra + 0x3e0));
        SF32(ra, 0x5bab4, U32(ra, 0x5baa8) != 0 ? 0.5f : rx_bf(0));
        float r;
        WORLD_CALL_SP(4, r = phys_lag_filter_update(ra + 0x5b9c0, dt, rx_bf(world_x87_pass(U32(ra, 0x5bab4)))));
        SF32(ra, 0x5bab4, r);
        float x = phys_add(r, e);
        if (!(0.5f > x)) x = 0.5f;
        SF32(ra, 0x5ba90, x);
        if (U8(0, 0x00d94a9c) != 0) world_clamped_float_write(ra + 0x3e0, 0x009d788c, WCTX(.esi = ra, .edi = ra + 0x3e0));
        if (U8(0, 0x00d94a9d) != 0) SF32(ra, 0x5ba90, 0.5f);
    } else {
        SF32(ra, 0x5ba90, 0.3f /* 0x931da4 */);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061d350, rx_world_rider_update_lean)

/* 0061faa0 RiderAnim_UpdateBodyWeight (EAX = ra; stack dt (+ 4 unused words); RET 0x14): the weight of the body animation
 * layer, written through the clamped float +0x3f8 (inlined ClampedFloat_Write). Four lags {value, tau, k} (as 0042d6b0):
 * +0x5b984 toward the base 0.25 / 0.4 / 0.5 (by +0x5bab8 == 1 and +0x5babc) plus, when +0x5babc is clear, half the speed
 * excess clamp((+0x5bb30 - +0xaf60) / (65.9997 - +0xaf60), 0, 1); +0x5b998 toward min(+0x5bb30 / 23, 1) (1 with +0x5babc);
 * +0x5b9ac toward 0 / 0.25 / [0x9d78a0] (+0x5baa8, +0x5bb30 above [0x9d78a4]); +0x5b9d4 toward 0.5 / [0x9c4b4c].
 * A timer +0x5b980 (dt / 2 up with +0x5babc, else dt down, clamped to [0, [0x9c4754]]) and a decaying kick +0x5b97c (* 1.2,
 * clamped to +-5, * 0.2) add to the first. weight = min(lag 4, lag 3 + (1 - min(|w3 - 0.5| 2, 0.5)) (1 - min(w2 lag2, 0.5))
 * (1 - min(w1 lag2, 0.5)) (lag1 + timer + kick)) with w1..w3 the float properties +0x308 / +0x334 / +0x38c. The debug byte
 * 0xd94a9e then forces [0x9d78a8] (clamped). */
void rx_world_rider_update_body_weight(rx_addr ra, float dt) {
    world_frame f = rider_frame_enter(0x14, 1, 8);   /* push ebp; and esp, -8; sub esp, 0x14; push esi */
    const uint32_t boost = U32(ra, 0x5babc);
    float base;
    if (U32(ra, 0x5bab8) == 1) base = boost ? 0.4f /* 0x935860 */ : 0.25f /* 0x92ff84 */;
    else base = boost ? 0.5f /* 0x9322cc */ : 0.25f;
    if (boost == 0) {
        const float speed = F32(ra, 0x5bb30), start = F32(ra, 0xaf60);
        if (speed > start) {
            const float t = rider_clamp((speed - start) / (65.9997f /* 0x97774c */ - start), rx_bf(0), 1.0f);
            base = t * 0.5f + base;
        }
    }
    const float lag1 = rider_lag(ra + 0x5b984, base, dt);
    const float timer = boost ? dt * 0.5f + F32(ra, 0x5b980) : F32(ra, 0x5b980) - dt;
    SF32(ra, 0x5b980, timer);
    const float held = rider_clamp(timer, rx_bf(0), F32(0, 0x009c4754));
    SF32(ra, 0x5b980, held);
    float kick = F32(ra, 0x5b97c) * 1.2f /* 0x930064 */;
    SF32(ra, 0x5b97c, kick);
    kick = rider_clamp(kick, -5.0f /* 0x97739c */, 5.0f /* 0x935890 */) * 0.2f /* 0x92e388 */;
    SF32(ra, 0x5b97c, kick);
    const float layer = (kick + held) + lag1;
    float m = F32(ra, 0x5bb30) * 0.04347826f /* 0x977748 */;
    if (m > 1.0f) m = 1.0f;
    if (boost) m = 1.0f;
    const float lag2 = rider_lag(ra + 0x5b998, m, dt);
    const float w1 = world_float_get(ra + 0x308, WCTX(.esi = ra));
    const float w2 = world_float_get(ra + 0x334, WCTX(.esi = ra));
    const float w3 = world_float_get(ra + 0x38c, WCTX(.esi = ra));
    float a3 = rx_bf(rx_fb(w3 - 0.5f) & 0x7fffffffu /* 0x977570 */) * 2.0f;
    float p1 = w1 * lag2;
    if (p1 > 0.5f) p1 = 0.5f;
    const float q = (1.0f - p1) * layer;
    float p2 = w2 * lag2;
    if (p2 > 0.5f) p2 = 0.5f;
    const float r = (1.0f - p2) * q;
    if (a3 > 0.5f) a3 = 0.5f;
    const float body = (1.0f - a3) * r;
    float t3 = rx_bf(0);
    if (U32(ra, 0x5baa8) != 0) t3 = F32(ra, 0x5bb30) > F32(0, 0x009d78a4) ? F32(0, 0x009d78a0) : 0.25f;
    SF32(ra, 0x5baac, t3);
    const float lag3 = rider_lag(ra + 0x5b9ac, t3, dt);
    SF32(ra, 0x5baac, lag3);
    const float total = lag3 + body;
    const float lag4 = rider_lag(ra + 0x5b9d4, U32(ra, 0x5babc) != 0 ? 0.5f : F32(0, 0x009c4b4c), dt);
    const float w = rider_clamp(total > lag4 ? lag4 : total, F32(ra, 0x3f8), F32(ra, 0x3fc));
    if (U32(ra, 0x400) != 0) SF32(U32(ra, 0x400), 0, w);
    if (U8(0, 0x00d94a9e) != 0) {
        const float hi = F32(ra, 0x3fc), c = F32(0, 0x009d78a8);
        float x = c >= hi ? hi : c;
        if (F32(ra, 0x3f8) >= x) x = F32(ra, 0x3f8);
        if (U32(ra, 0x400) != 0) SF32(U32(ra, 0x400), 0, x);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061faa0, rx_world_rider_update_body_weight)

/* A ranged float {min +0, max +4, rate +8, limit +0xc, value +0x10, listeners / target +0x14} stepped toward `target` as the
 * rider code inlines RangedFloat_Step: value += (target - value) * min(rate, limit) / rate (the product's operand order differs
 * between the inlined copies: `step_first`); returns the new value. */
static float rider_ranged_advance(rx_addr rf, float target, int step_first) {
    const float rate = F32(rf, 8), limit = F32(rf, 0xc);
    const float m = rate > limit ? limit : rate;
    const float v = F32(rf, 0x10);
    const float next = (step_first ? (m / rate) * (target - v) : (target - v) * (m / rate)) + v;
    SF32(rf, 0x10, next);
    return next;
}
/* its 0..1 position: (clamp(value) - min) / (max - min) */
static float rider_ranged_step(rx_addr rf, float target, int step_first) {
    const float next = rider_ranged_advance(rf, target, step_first);
    const float lo = F32(rf, 0), hi = F32(rf, 4);
    return (rider_clamp(next, lo, hi) - lo) / (hi - lo);
}
/* the clamped value written to the target pointer +0x14 (when set) */
static void rider_ranged_write(rx_addr rf, float target, int step_first) {
    const float w = rider_clamp(rider_ranged_advance(rf, target, step_first), F32(rf, 0), F32(rf, 4));
    if (U32(rf, 0x14) != 0) SF32(U32(rf, 0x14), 0, w);
}

/* 006230c0 RiderAnim_UpdatePoseWeights (EAX = ra; stack dt; RET 4): three lags {target, value, tau, k} (+0x5ba60, +0x5ba78,
 * +0x5ba90; as 0042d6b0); the first two drive the ranged floats +0x2f4 / +0x320 whose positions are broadcast to the float
 * properties +0x308 / +0x334 (through the push-ecx slot [esp + 4]), the third is written through the clamped float +0x3ec. */
void rx_world_rider_update_pose_weights(rx_addr ra, float dt) {
    world_frame f = world_frame_flat(2, 0);   /* push ecx; push esi */
    const rx_addr slot = f.esp + 4;
    const float l1 = rider_lag(ra + 0x5ba64, F32(ra, 0x5ba60), dt);
    SF32(slot, 0, rider_ranged_step(ra + 0x2f4, l1, 1));
    world_float_set(ra + 0x308, slot, WCTX(.eax = slot, .ecx = ra + 0x308, .esi = ra));
    const float l2 = rider_lag(ra + 0x5ba7c, F32(ra, 0x5ba78), dt);
    SF32(slot, 0, rider_ranged_step(ra + 0x320, l2, 0));
    world_float_set(ra + 0x334, slot, WCTX(.eax = ra + 0x334, .edx = slot, .esi = ra));
    const float l3 = rider_lag(ra + 0x5ba94, F32(ra, 0x5ba90), dt);
    const float w = rider_clamp(l3, F32(ra, 0x3ec), F32(ra, 0x3f0));
    if (U32(ra, 0x3f4) != 0) SF32(U32(ra, 0x3f4), 0, w);
    world_frame_leave(&f);
}
RX_FUNCTION(0x006230c0, rx_world_rider_update_pose_weights)

/* 0061ecd0 RiderAnim_UpdateSteerWeight (EAX = ra; stack dt; RET 4): the steering layer weight, a lag +0x5ba4c written through
 * the clamped float +0x3d4. While the pose state (+0x580) differs from +0x5a0 the lags +0x5ba10 / +0x5ba24 / +0x5ba38 are
 * cleared and the weight goes to 0.5 at three times the rate. Otherwise lag A (+0x5ba38) follows the speed +0x5bb30 and lag
 * B (+0x5ba24, copied to +0x5ba08) the steering +0x5b9f8; each gives 1 - 4 (min(|x|, cap) * scale - 0.5)^2 (caps
 * [0x9d7898] / [0x9cc37c], scales 0x977750 / 0x977388; the square is the exe's integer power loop, 1 * x * x); the weight
 * target is 0.5 +- 0.5 q (by the sign of B; times A's term when B > 0), times 1 - min(|w - 0.5| 2, 0.5) of the property +0x38c. */
void rx_world_rider_update_steer_weight(rx_addr ra, float dt) {
    world_frame f = rider_frame_enter(0, 2, 8);   /* push ebp; and esp, -8; push ecx; push esi */
    const rx_addr state = U32(ra, 0x580);
    const uint16_t cur = state ? U16(state, 0) : 0;
    float w;
    if (cur != U16(ra, 0x5a0)) {
        SF32(ra, 0x5ba10, 0.0f); SF32(ra, 0x5ba24, 0.0f); SF32(ra, 0x5ba38, 0.0f);
        w = rider_lag(ra + 0x5ba4c, 0.5f /* 0x9322cc */, dt * 3.0f /* 0x92fe48 */);
    } else {
        const float a = rider_lag(ra + 0x5ba38, F32(ra, 0x5bb30), dt);
        float x = rx_bf(rx_fb(a) & 0x7fffffffu /* 0x977570 */);
        if (x > F32(0, 0x009d7898)) x = F32(0, 0x009d7898);
        x = x * 0.018518518656492233f /* 0x977750 */ - 0.5f;
        const float qa = 1.0f - (1.0f * (x * x)) * 4.0f /* 0x9322c0 */;
        const float b = rider_lag(ra + 0x5ba24, F32(ra, 0x5b9f8), dt);
        SF32(ra, 0x5ba08, b);
        float y = rx_bf(rx_fb(b) & 0x7fffffffu);
        if (y > F32(0, 0x009cc37c)) y = F32(0, 0x009cc37c);
        y = y * 0.025f /* 0x977388 */ - 0.5f;
        const float qb = 1.0f - (1.0f * (y * y)) * 4.0f;
        float out = 0.5f;
        if (b > 0.0f) out = (qb * qa) * 0.5f + 0.5f;
        else if (0.0f > b) out = 0.5f - qb * 0.5f;
        const float g = world_float_get(ra + 0x38c, WCTX(.esi = ra));
        float h = rx_bf(rx_fb(g - 0.5f) & 0x7fffffffu) * 2.0f /* 0x92e330 */;
        if (h > 0.5f) h = 0.5f;
        w = rider_lag(ra + 0x5ba4c, (1.0f - h) * out, dt);
    }
    w = rider_clamp(w, F32(ra, 0x3d4), F32(ra, 0x3d8));
    if (U32(ra, 0x3dc) != 0) SF32(U32(ra, 0x3dc), 0, w);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061ecd0, rx_world_rider_update_steer_weight)

/* 0061e770 RiderAnim_UpdateLimbRanges (EAX = ra): unless +0x5bab8, the four ranged floats +0x404 / +0x41c / +0x434 / +0x44c
 * step toward +0xaf50 and write their clamped values (inlined RangedFloat_Step). */
void rx_world_rider_update_limb_ranges(rx_addr ra) {
    if (U32(ra, 0x5bab8) != 0) return;
    rider_ranged_write(ra + 0x404, F32(ra, 0xaf50), 1);
    rider_ranged_write(ra + 0x41c, F32(ra, 0xaf50), 1);
    rider_ranged_write(ra + 0x434, F32(ra, 0xaf50), 1);
    rider_ranged_write(ra + 0x44c, F32(ra, 0xaf50), 0);
}
RX_FUNCTION(0x0061e770, rx_world_rider_update_limb_ranges)

/* *[598] == 1.0 as the original tests it (ucomiss / lahf / test ah, 0x44 / jp: exactly equal, ordered) */
static int target_blend_full(rx_addr ra) {
    const rx_addr t = U32(ra, 0x598);
    return t != 0 && F32(t, 0) == 1.0f;
}
static uint16_t pose_state(rx_addr ra) {
    const rx_addr state = U32(ra, 0x580);
    return state ? U16(state, 0) : 0;
}

/* 006206d0 RiderAnim_LeanTarget (ECX = float* steer, EAX = float* lean (lean, roll); xmm0 out): clamp(steer, [0x9cd9dc], 0.1)
 * + clamp(lean, -0.9, [0x9cd970]) = s, then |roll| * s + s. */
float rx_world_rider_lean_target(rx_addr steer, rx_addr lean) {
    world_frame f = rider_frame_enter(0, 0, 8);   /* push ebp; and esp, -8 */
    const float a = rider_clamp(F32(steer, 0), F32(0, 0x009cd9dc), 0.1f /* 0x931d94 */);
    const float s = rider_clamp(F32(lean, 0), -0.9f /* 0x977744 */, F32(0, 0x009cd970)) + a;
    const float r = rx_bf(rx_fb(F32(lean, 4)) & 0x7fffffffu /* 0x977570 */) * s + s;
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x006206d0, rx_world_rider_lean_target)

/* 00620110 RiderAnim_UpdateSteer (EAX = ra; stack steer, b, lean, roll; RET 0x10): the steering / lean animation inputs. The
 * debug flag 0xd94aac replaces lean / roll by 0xd94aa4 / 0xd94aa8 (written into the argument slots, whose addresses the
 * original passes on). The roll goes to +0x5ba60 (> 0) or +0x5ba78 (negated). Riding normally: the target blend +0x598 snaps
 * to 0 (and the pose state +0x580 to +0x5a0) when it is full while the state matches +0x5aa; the ranged float +0x378 steps to
 * the lean target (006206d0's formula, inlined) and +0x34c to 1 / 0 (|steer + lean| > 1). Boosting (+0x5babc): +0x34c steps to 1;
 * the heading h = (+0x5bae8 + +0x5bae0) * 0 + +0x5bae4 and the move +0x5bb50 pick the +0x378 target (moves 1..3: max(h < 0 ?
 * [0x9cd9e8] : +0x5b96c, min(lean, 0)); 4..6: min(h < 0 ? 1 : +0x5b96c, max(lean, 0)); else, when the state left +0x5aa with a
 * full blend, the lean target); then the state snaps as above (through the short property +0x578). Crashed (+0xaf10): +0x378
 * and the float +0x2d0 (listeners +0x2dc) step to -0. */
void rx_world_rider_update_steer(rx_addr ra, float steer, float b, float lean, float roll) {
    world_frame f = world_frame_enter(8, 2, 1);   /* push ebp; and esp, -16; sub esp, 8; push esi; push edi */
    (void)b;
    const rx_addr args = f.ebp + 8, slot = f.esp + 0xc, slot2 = f.esp + 8;
    if (U32(0, 0x00d94aac) != 0) {
        lean = F32(0, 0x00d94aa4);
        roll = F32(0, 0x00d94aa8);
        SF32(args, 8, lean);
        SF32(args, 0xc, roll);
    }
    if (U32(ra, 0xaf10) != 0) {
        SF32(slot, 0, rider_ranged_step(ra + 0x378, rx_bf(0x80000000u) /* 0x9774a8 */, 1));
        world_float_set(ra + 0x38c, slot, WCTX(.eax = slot, .ecx = ra + 0x38c, .edi = ra));
        const float rate = F32(ra, 0x2d0), limit = F32(ra, 0x2d4);
        const float m = rate > limit ? limit : rate;
        const float v = F32(ra, 0x2d8);
        const float next = (m / rate) * (rx_bf(0x80000000u) - v) + v;
        SF32(ra, 0x2d8, next);
        SF32(slot, 0, next);
        WORLD_CALL_SP(4, rx_rider_listeners_broadcast_vec(ra + 0x2dc, slot));   /* 0063b7d0 (trickstart's port) */
        world_frame_leave(&f);
        return;
    }
    if (roll > 0.0f) { SF32(ra, 0x5ba78, 0.0f); SF32(ra, 0x5ba60, roll); }
    else { SF32(ra, 0x5ba60, 0.0f); SF32(ra, 0x5ba78, rx_bf(0x80000000u) - roll); }
    if (U32(ra, 0x5babc) == 0) {
        if (U16(ra, 0x5aa) == pose_state(ra) && target_blend_full(ra)) {
            if (U32(ra, 0x57c) != 0) SU8(U32(ra, 0x57c), 0, 1);
            if (U32(ra, 0x580) != 0) SU16(U32(ra, 0x580), 0, U16(ra, 0x5a0));
            const float w = rider_clamp(0.0f, F32(ra, 0x590), F32(ra, 0x594));
            if (U32(ra, 0x598) != 0) SF32(U32(ra, 0x598), 0, w);
        }
        const float a = rider_clamp(steer, F32(0, 0x009cd9dc), 0.1f);
        const float s = rider_clamp(lean, -0.9f, F32(0, 0x009cd970)) + a;
        SF32(slot, 0, rx_bf(rx_fb(roll) & 0x7fffffffu) * s + s);
        world_ranged_float_step_any(ra + 0x378, slot, WCTX(.ecx = slot, .edi = ra));
        const float sum = rx_bf(rx_fb(steer + lean) & 0x7fffffffu);
        SF32(slot, 0, sum > 1.0f ? 0.0f : 1.0f);
        world_ranged_float_step(ra + 0x34c, slot, WCTX(.edi = ra));
        world_frame_leave(&f);
        return;
    }
    SF32(slot, 0, rider_ranged_step(ra + 0x34c, 1.0f, 1));
    world_float_set(ra + 0x360, slot, WCTX(.eax = ra + 0x360, .edx = slot, .edi = ra));
    const float h = (F32(ra, 0x5bae8) + F32(ra, 0x5bae0)) * 0.0f + F32(ra, 0x5bae4);
    const uint32_t move = U32(ra, 0x5bb50);
    if (move >= 1 && move <= 3) {
        const float t = 0.0f > h ? F32(0, 0x009cd9e8) : F32(ra, 0x5b96c);
        const float m = lean > 0.0f ? 0.0f : lean;
        SF32(slot, 0, t);
        SF32(slot2, 0, m);
        const float target = t > m ? t : m;
        SF32(slot, 0, rider_ranged_step(ra + 0x378, target, 0));
        world_float_set(ra + 0x38c, slot, WCTX(.ecx = slot, .edx = ra + 0x38c, .edi = ra));
    } else if (move >= 4 && move <= 6) {
        const float t = 0.0f > h ? 1.0f : F32(ra, 0x5b96c);
        SF32(slot2, 0, t);
        const float m = 0.0f > lean ? 0.0f : lean;
        SF32(slot, 0, m);
        world_ranged_float_step(ra + 0x378, m > t ? slot2 : slot, WCTX(.edi = ra));
    } else {
        if (pose_state(ra) != U16(ra, 0x5aa) && target_blend_full(ra)) {
            float v;
            SF32(args, 0, steer); SF32(args, 8, lean); SF32(args, 0xc, roll);   /* the slots it reads (as the caller left them) */
            WORLD_CALL_SP(0, v = rx_world_rider_lean_target(args, args + 8));
            SF32(slot, 0, v);
            world_ranged_float_step_any(ra + 0x378, slot, WCTX(.ecx = slot, .edi = ra));
        }
        world_frame_leave(&f);
        return;
    }
    if (pose_state(ra) != U16(ra, 0x5aa) && target_blend_full(ra)) {
        world_short_property_set(ra + 0x578, ra + 0x5aa, WCTX(.edi = ra));
        const float w = rider_clamp(0.0f, F32(ra, 0x590), F32(ra, 0x594));
        if (U32(ra, 0x598) != 0) SF32(U32(ra, 0x598), 0, w);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00620110, rx_world_rider_update_steer)

/* 0061feb0 RiderAnim_UpdateCrouch (EAX = ra; stack 4 unused floats; RET 0x10): the ranged float +0x3a4 (clamped value to its
 * target pointer). Below the speed 5 (+0x5bb10) it steps to 0.5; else the horizontal velocity (+0x5bb10, 0, +0x5bb18)
 * normalized gives v = |(x + 0 y) * 0 + z| * [0x9d789c] * 0.4 * 0.5 (1 - v when x > 0) and the target clamp(1 - v, 0.3,
 * [0x9c4b4c]) (clamp(v, 0.25, 0.75) with +0x5babc); the debug flag 0xd94aa0 then steps it to [0x9d78ac]. */
void rx_world_rider_update_crouch(rx_addr ra) {
    world_frame f = world_frame_enter(0x1c, 1, 1);   /* push ebp; and esp, -16; sub esp, 0x1c; push esi */
    if (5.0f /* 0x935890 */ > F32(ra, 0x5bb10)) {
        rider_ranged_write(ra + 0x3a4, 0.5f /* 0x9322cc */, 1);
    } else {
        float x = F32(ra, 0x5bb10), y = 0.0f, z = F32(ra, 0x5bb18);
        const float l2 = z * z + x * x;
        if (l2 > 0.0f) {
            float len;
            WORLD_CALL_SP(4, len = phys_sqrtf(rx_bf(world_x87_pass(rx_fb(l2)))));
            const float inv = 1.0f / len;
            x = inv * x; y = inv * 0.0f; z = z * inv;
        }
        float v = rx_bf(rx_fb((x + y) * 0.0f + z) & 0x7fffffffu /* 0x977570 */);
        v = ((v * F32(0, 0x009d789c)) * 0.4f /* 0x935860 */) * 0.5f;
        if (x > 0.0f) v = 1.0f - v;
        const float t = U32(ra, 0x5babc) == 0 ? rider_clamp(1.0f - v, 0.3f /* 0x931da4 */, F32(0, 0x009c4b4c))
                                               : rider_clamp(v, 0.25f /* 0x92ff84 */, 0.75f /* 0x935aa8 */);
        rider_ranged_write(ra + 0x3a4, t, 0);
        if (U32(0, 0x00d94aa0) != 0) rider_ranged_write(ra + 0x3a4, F32(0, 0x009d78ac), 0);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061feb0, rx_world_rider_update_crouch)

/* ---- the skeleton's IK chains (skeleton = ra +0x5bc: count +0x60, chains of 0x80 bytes at +0x64: +8 enabled, +0x40 the fade
 * timer, +0x70 the fade time) ---- */

/* one chain enabled / disabled with fade time t (unless it already is and `force` is clear): the timer restarts at t minus
 * what is left of it (0 when that is not less than t) */
static void chain_fade(rx_addr chain, float t, uint32_t enable, uint32_t force) {
    if (U32(chain, 8) != enable || force != 0) {
        const float timer = F32(chain, 0x40);
        SF32(chain, 0x70, t);
        SF32(chain, 0x40, timer >= t ? 0.0f : t - timer);
    }
    SU32(chain, 8, enable);
}
/* chains first .. last (each only when the skeleton has it) */
static void chains_fade(rx_addr ra, uint32_t first, uint32_t last, float t, uint32_t enable, uint32_t force) {
    for (uint32_t k = first; k <= last; k++) {
        const rx_addr skel = U32(ra, 0x5bc);
        if (!(U32(skel, 0x60) > k)) return;
        chain_fade(U32(skel, 0x64) + 0x80u * k, t, enable, force);
    }
}
/* the selector (jump tables 0x623944 / 0x623784; the original has no bound check): 0..3 one chain, 4 chains 0 and 1, 5 chains 2
 * and 3, 6 every chain */
static void chains_select(uint32_t sel, uint32_t* first, uint32_t* last) {
    static const uint8_t lo[6] = {0, 1, 2, 3, 0, 2}, hi[6] = {0, 1, 2, 3, 1, 3};
    *first = lo[sel]; *last = hi[sel];
}

/* 006237a0 RiderAnim_ChainsFadeIn (ECX = ra, EAX = selector, ESI = force; stack fade time; RET 4): enables the selected IK
 * chains; selector 6 = all of them through 006b04d0 (ECX = the skeleton). */
void rx_world_rider_chains_fade_in(rx_addr ra, uint32_t sel, uint32_t force, float t) {
    if (sel == 6) {
        WORLD_CALL_SP(0, (void)rx_ik_animator_engage_goals(U32(ra, 0x5bc)));   /* 006b04d0 */
        return;
    }
    if (sel > 6) return;
    uint32_t first, last;
    chains_select(sel, &first, &last);
    chains_fade(ra, first, last, t, 1, force);
}
RX_FUNCTION(0x006237a0, rx_world_rider_chains_fade_in)

/* 006235e0 RiderAnim_ChainsFadeOut (ECX = ra, EAX = selector; stack fade time, force; RET 8): disables the selected IK chains;
 * selector 6 = all of them through 006b0480 (ESI = the skeleton). */
void rx_world_rider_chains_fade_out(rx_addr ra, uint32_t sel, float t, uint32_t force) {
    world_frame f = world_frame_flat(1, 0);   /* push esi */
    if (sel == 6) {
        WORLD_CALL_SP(0, rx_world_ik_restart_fades(U32(ra, 0x5bc)));   /* 006b0480 */
    } else if (sel < 6) {
        uint32_t first, last;
        chains_select(sel, &first, &last);
        chains_fade(ra, first, last, t, 0, force);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x006235e0, rx_world_rider_chains_fade_out)

/* the animation-state switch the rider code inlines: mode +0x5b590 (previous +0x5b594; the targets +0x5b5e8 / +0x5b5ec are
 * dropped when the previous was 1) with the blend time 1.5 at +0x5b640 */
static void anim_switch(rx_addr ra, uint32_t mode) {
    if (U32(ra, 0x5b594) == 1) { SU32(ra, 0x5b5e8, 0); SU32(ra, 0x5b5ec, 0); }
    SU32(ra, 0x5b594, U32(ra, 0x5b590));
    SF32(ra, 0x5b640, 1.5f /* 0x9322a4 */);
    SU32(ra, 0x5b590, mode);
}

/* 0061ea90 RiderAnim_StartIdleGesture (ESI = ra): the idle rider's fidget (0054cec0) asks for a gesture: when the float
 * property +0x38c (FloatListeners_Get) is strictly between 0.2 (0x92e388) and 0.8 (0x9cc8a8) the gesture starts
 * (+0x5b6d4 = 1, the 0061eac0 state machine's start state). Frame: push ecx. */
void rx_world_rider_start_idle_gesture(rx_addr ra) {
    world_frame f = world_frame_flat(1, 0);   /* push ecx */
    float x;
    WORLD_CALL_SP(4, x = rx_world_float_listeners_get(ra + 0x38c));   /* stdcall, RET 4 */
    if (0.2f /* 0x92e388 */ < x && x < 0.8f /* 0x9cc8a8 */) SU32(ra, 0x5b6d4, 1);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061ea90, rx_world_rider_start_idle_gesture)

/* 0061eac0 RiderAnim_UpdateGesture (EDI = ra; stack dt; RET 4): the gesture state machine +0x5b6d4: 1 = start (animation state
 * 3, chain 0 released over 0.25 s) -> 2 = the clamped float +0x3bc ramps up by [0x9d7890] per frame; at [0x9d78c4] the
 * gesture prop is attached once (+0x5b6e0: the node +0x5b6d8 of the ra's vfunc 0x74 model given to the object +0x5b6dc
 * (+0x100) and its vfunc 3 (dt)); at [0x9d7894] -> 3 = end (chain 0 back over 0.25 s, animation state 4) -> 4 -> 0 (ramp
 * cleared). */
void rx_world_rider_update_gesture(rx_addr ra, float dt) {
    world_frame f = world_frame_flat(2, 0);   /* push ecx; push esi */
    switch (U32(ra, 0x5b6d4)) {
    case 1:
        anim_switch(ra, 3);
        WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(ra, 0, 0.25f /* 0x92ff84 */, 0));
        SU32(ra, 0x5b6d4, 2);
        break;
    case 2: {
        rx_addr p = U32(ra, 0x3c4);
        const float x = rider_clamp(F32(0, 0x009d7890) + (p ? F32(p, 0) : 0.0f), F32(ra, 0x3bc), F32(ra, 0x3c0));
        if (p) SF32(p, 0, x);
        p = U32(ra, 0x3c4);
        if (p == 0) break;
        const float v = F32(p, 0);
        SF32(f.esp, 4, v);
        if (v >= F32(0, 0x009d78c4) && U32(ra, 0x5b6e0) == 0) {
            SU32(ra, 0x5b6e0, 1);
            const rx_addr model = WORLD_CTX0(U32(U32(ra, 0), 0x74), &WCTX(.ecx = ra, .esi = U32(ra, 0x5b6d8), .edi = ra), 0);
            const rx_addr node = U32(model, 8) + (U32(ra, 0x5b6d8) << 4);
            if (node != 0) {
                const rx_addr obj = U32(ra, 0x5b6dc);
                SU32(obj, 0x100, node);
                /* the lens-swipe effect's vf3 0063c040 (fx unit) at the original's `push ecx; fstp [esp]; call [vtable + 0xc]` */
                const uint32_t arg = world_x87_pass(rx_fb(dt));
                SU32(rx_sp(), -4, arg);              /* the pushed ecx slot, overwritten with dt */
                SU32(rx_sp(), -8, 0x0061ebecU);      /* the call's return address */
                world_vcall_f(obj, 0xc, rx_bf(arg));
            }
        }
        if (v >= F32(0, 0x009d7894)) SU32(ra, 0x5b6d4, 3);
        break;
    }
    case 3:
        SU32(ra, 0x5b6d4, 4);
        WORLD_CALL_SP(4, rx_world_rider_chains_fade_in(ra, 0, 0, 0.25f));
        anim_switch(ra, 4);
        break;
    case 4: {
        SU32(ra, 0x5b6d4, 0);
        const float x = rider_clamp(0.0f, F32(ra, 0x3bc), F32(ra, 0x3c0));
        if (U32(ra, 0x3c4) != 0) SF32(U32(ra, 0x3c4), 0, x);
        SU32(ra, 0x5b6e0, 0);
        break;
    }
    default:
        break;
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061eac0, rx_world_rider_update_gesture)

/* 00620750 RiderAnim_UpdateArmChains (EDI = ra): with the target blend +0x598 full, which arm IK chains (2 / 3) follow: on the
 * bike (+0x5bab8 == 0) boosting (+0x5babc, not crashed) by the steering x = w(+0x38c) - (0.5 - lean +0x3e8) (through
 * ClampedFloat_Diff when the lean is at least 0.5) against the limits +0xaf58 / +0xaf5c and the lean angle +0x5b96c
 * (< [0x9cd9dc] / > 0.1), then the lean alone: chain 2 or 3 in (the other out first after +0x5bac4 > 1.5 s), else both (5).
 * Not boosting, above the speed +0xaf6c, not crashed and with the pose state and both blends full: the free chains 2 / 3 in;
 * then +0xaf44 = +0xaf48 = 0.5. Mode 1 (+0x5bab8): chain 2 / 3 out by the lean, else both in. */
static float lean_value(rx_addr ra) { const rx_addr l = U32(ra, 0x3e8); return l ? F32(l, 0) : 0.0f; }
static void arm_chains(rx_addr ra, rx_addr sp) {
    if (!target_blend_full(ra)) return;
    const uint32_t mode = U32(ra, 0x5bab8);
    if (mode != 0) {
        if (mode != 1 || U32(ra, 0xaf10) != 0) return;
        if (U32(ra, 0x5babc) != 0) {
            const float v = lean_value(ra);
            if (F32(ra, 0xaf58) > v) { WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(ra, 2, 0.25f, 0)); return; }
            if (v > F32(ra, 0xaf5c)) { WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(ra, 3, 0.25f, 0)); return; }
        }
        WORLD_CALL_SP(4, rx_world_rider_chains_fade_in(ra, 5, 0, 0.25f));
        return;
    }
    if (U32(ra, 0x5babc) != 0 && U32(ra, 0xaf10) == 0) {
        const float g = world_float_get(ra + 0x38c, WCTX(.edi = ra));
        SF32(sp, 8, g);
        const rx_addr lean = U32(ra, 0x3e8);
        float x;
        if (lean == 0) x = g - (0.5f - 0.0f);
        else if (0.5f > F32(lean, 0)) x = g - (0.5f - F32(lean, 0));
        else {
            SF32(sp, 4, 0.5f);
            x = world_clamped_float_diff(ra + 0x3e0, sp + 4, WCTX(.edi = ra)) + g;
        }
        const float hi = F32(ra, 0xaf58), lo = F32(ra, 0xaf5c);
        uint32_t which;   /* 2: chain 2 in, 3: chain 3 in, 5: both */
        if (hi > x && F32(0, 0x009cd9dc) > F32(ra, 0x5b96c)) which = 2;
        else if (x > lo && F32(ra, 0x5b96c) > 0.1f /* 0x931d94 */) which = 3;
        else {
            const float v = lean_value(ra);
            which = hi > v ? 2 : v > lo ? 3 : 5;
        }
        if (which != 5 && F32(ra, 0x5bac4) > 1.5f /* 0x9322a4 */)
            WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(ra, which == 2 ? 3 : 2, 0.25f, 0));
        WORLD_CALL_SP(4, rx_world_rider_chains_fade_in(ra, which, 0, 0.25f));
        return;
    }
    if (!(F32(ra, 0x5bb30) > F32(ra, 0xaf6c)) || U32(ra, 0xaf54) != 0 || U32(ra, 0xaf10) != 0) return;
    if (U16(ra, 0x5a0) == pose_state(ra) && U32(ra, 0x58c) != 0 && F32(U32(ra, 0x58c), 0) == 1.0f && target_blend_full(ra)) {
        if (U32(ra, 0x5b760) == 0) WORLD_CALL_SP(4, rx_world_rider_chains_fade_in(ra, 2, 0, 0.25f));
        if (U32(ra, 0x5b850) == 0) WORLD_CALL_SP(4, rx_world_rider_chains_fade_in(ra, 3, 0, 0.25f));
    }
    SF32(ra, 0xaf44, 0.5f);
    SF32(ra, 0xaf48, 0.5f);
}
void rx_world_rider_update_arm_chains(rx_addr ra) {
    world_frame f = world_frame_flat(1, 8);   /* sub esp, 8; push esi */
    arm_chains(ra, f.esp);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00620750, rx_world_rider_update_arm_chains)

/* the target blend +0x598 snapped to clamp(0) after the pose state +0x580 was set (the dirty byte +0x57c set) */
static void snap_state(rx_addr ra, uint32_t state_off) {
    if (U32(ra, 0x57c) != 0) SU8(U32(ra, 0x57c), 0, 1);
    if (U32(ra, 0x580) != 0) SU16(U32(ra, 0x580), 0, U16(ra, state_off));
}
static void blend_zero(rx_addr ra) {
    const float w = rider_clamp(0.0f, F32(ra, 0x590), F32(ra, 0x594));
    if (U32(ra, 0x598) != 0) SF32(U32(ra, 0x598), 0, w);
}

/* 00621030 RiderAnim_ChainEvent (EAX = clip event {+0 time, +8 code}, ECX = ra): codes 0..3 fade chain `code` in, 4..7 fade
 * chain `code - 4` out, over the event's time (jump table 0x6210e0; other codes nothing). */
void rx_world_rider_chain_event(rx_addr ev, rx_addr ra) {
    const uint32_t code = U32(ev, 8);
    if (code > 7) return;
    world_frame f = world_frame_flat(1, 0);   /* push esi */
    const float t = rx_bf(world_x87_pass(U32(ev, 0)));
    if (code < 4) WORLD_CALL_SP(4, rx_world_rider_chains_fade_in(ra, code, 0, t));
    else WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(ra, code - 4, t, 0));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00621030, rx_world_rider_chain_event)

/* 00620fc0 RiderAnim_PlayClipEvents (stdcall ra, clip, t1, t2; RET 0x10): the clip's chain events between t1 and t2 (8 slots of
 * 12 bytes in the frame, preset to -1, filled by 0083d3d0), each run through 00621030. */
void rx_world_rider_play_clip_events(rx_addr ra, rx_addr clip, float t1, float t2) {
    world_frame f = world_frame_flat(3, 0x60);   /* sub esp, 0x60; push ebx; push esi; push edi */
    const rx_addr events = f.esp + 0xc;
    crt_memset(events, -1, 0x60);
    WORLD_CALL_SP(4, rx_world_clip_events_between(events, clip, t1, t2));
    for (uint32_t k = 0; k < 8; k++) {
        const rx_addr ev = events + 12 * k;
        if (I32(ev, 8) != -1) WORLD_CALL_SP(0, rx_world_rider_chain_event(ev, ra));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00620fc0, rx_world_rider_play_clip_events)

/* 00621320 RiderAnim_EndTrick (ECX = ra): when a trick clip plays (+0x5b2dc): cleared with its clip +0xc920 and time
 * +0x5b2d8; with a full target blend the pose state back to +0x5a0 and the blend to clamp(0); the clamped float +0x464 to
 * clamp(0) (its listeners +0x46c, through [esp + 8]). */
void rx_world_rider_end_trick(rx_addr ra) {
    world_frame f = world_frame_flat(1, 8);   /* sub esp, 8; push esi */
    if (U32(ra, 0x5b2dc) != 0) {
        SU32(ra, 0x5b2dc, 0);
        SU32(ra, 0xc920, 0);
        SF32(ra, 0x5b2d8, 0.0f);
        if (target_blend_full(ra)) { snap_state(ra, 0x5a0); blend_zero(ra); }
        const rx_addr slot = f.esp + 8;
        SF32(slot, 0, rider_clamp(0.0f, F32(ra, 0x464), F32(ra, 0x468)));
        world_float_set(ra + 0x46c, slot, WCTX(.eax = slot, .ecx = ra + 0x46c));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00621320, rx_world_rider_end_trick)

/* 00621490 RiderAnim_EndPoseOverride (stdcall ra; RET 4): when the bike pose is overridden (+0x5b2e8): the blend to clamp(0),
 * the pose state to +0x5a6, +0xc598 / +0x5b2e8 / +0xc934 cleared, chain group 4 back in over 0.25 s, the bike simulation's
 * +0x2de4 = 1 and Simulation_EndPoseOverride; then +0xaf50 = 0.5. */
void rx_world_rider_end_pose_override(rx_addr ra) {
    world_frame f = world_frame_enter(4, 3, 1);   /* push ebp; and esp, -16; push ecx; push ebx; push esi; push edi */
    if (U32(ra, 0x5b2e8) != 0) {
        blend_zero(ra);
        snap_state(ra, 0x5a6);
        SU32(ra, 0xc598, 0);
        SU32(ra, 0x5b2e8, 0);
        WORLD_CALL_SP(4, rx_world_rider_chains_fade_in(ra, 4, 0, 0.25f));
        SU32(ra, 0xc934, 0);
        SU32(U32(U32(ra, 0xaf1c), 0x234), 0x2de4, 1);
        world_simulation_end_pose_override(U32(U32(ra, 0xaf1c), 0x234), WCTX(.ebx = ra));
    }
    SF32(ra, 0xaf50, 0.5f /* 0x9322cc */);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00621490, rx_world_rider_end_pose_override)

/* 0061dd60 RiderAnim_UpdateTrick (EAX = ra; stack dt; RET 4): the air trick. Without a trick request (+0x5bb4c) the trick ends
 * (00621320). Otherwise, unless the pose state is one of the trick states (+0xc8ec / +0xc914) or the blend is not full or the
 * speed +0x5bb30 is above [0x9322d0]: after the wind-up time +0x5b2d8 > 0.5 the trick entry +0xc91c (+0xc8cc for a left lean,
 * else +0xc8f4) is started: pose state from its +0x20, blend clamp(0), its clip +0xc920 (00830130), the trick time +0x464 to
 * 0 with the range (0, clip length +0x94), +0x5b2dc = 1, the pose override ended (00621490); before that the wind-up runs (+dt)
 * with arm chain 2 / 3 out. While the trick plays its time +0x46c advances by dt (clamped; through the argument slot), the
 * clip's chain events between the old and new time play (00620fc0), and past the entry's +0x24 that time is taken off
 * (0063b5f0). */
void rx_world_rider_update_trick(rx_addr ra, float dt) {
    world_frame f = world_frame_flat(4, 8);   /* sub esp, 8; push ebx; push ebp; push esi; push edi */
    const rx_addr slot = f.esp + 0x14, arg = f.esp + 0x1c;
    if (U32(ra, 0x5bb4c) == 0) {
        WORLD_CALL_SP(0, rx_world_rider_end_trick(ra));
    } else if (pose_state(ra) != U16(ra, 0xc8ec) && pose_state(ra) != U16(ra, 0xc914) && target_blend_full(ra)
               && F32(0, 0x009322d0) > F32(ra, 0x5bb30)) {
        const float wind = F32(ra, 0x5b2d8);
        if (wind > 0.5f /* 0x9322cc */) {
            SU32(ra, 0xc91c, 0.0f > F32(ra, 0x5b96c) ? ra + 0xc8cc : ra + 0xc8f4);
            world_short_property_set(ra + 0x578, U32(ra, 0xc91c) + 0x20, WCTX(.edi = ra));
            blend_zero(ra);
            SU32(ra, 0xc920, world_character_find_clip(U32(ra, 0x144), U32(ra, 0xc91c), WCTX(.edi = ra)));
            SF32(slot, 0, 0.0f);
            world_clamped_float_set(ra + 0x464, slot, WCTX(.edi = ra));
            const float len = F32(U32(ra, 0xc920), 0x94);
            if (!(0.0f >= len)) { SF32(ra, 0x464, 0.0f); SF32(ra, 0x468, len); }
            SU32(ra, 0x5b2dc, 1);
            WORLD_CALL_SP(4, rx_world_rider_end_pose_override(ra));
        } else {
            SF32(ra, 0x5b2d8, wind + dt);
            WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(ra, 0.0f > F32(ra, 0x5b96c) ? 2 : 3, 0.25f, 0));
        }
    }
    if (U32(ra, 0x5b2dc) != 0) {
        const world_ctx c = WCTX(.ebx = ra + 0x46c, .ebp = ra + 0x46c, .esi = ra + 0x464, .edi = ra);
        const float t0 = world_float_get(ra + 0x46c, c);
        SF32(slot, 0, t0);
        const float t = world_float_get(ra + 0x46c, c) + dt;
        SF32(arg, 0, rider_clamp(t, F32(ra, 0x464), F32(ra, 0x468)));
        world_float_set(ra + 0x46c, arg, c);
        const float t1 = world_float_get(ra + 0x46c, c);
        SF32(arg, 0, t1);
        WORLD_CALL_SP(0x10, rx_world_rider_play_clip_events(ra, U32(ra, 0xc920), rx_bf(world_x87_pass(rx_fb(t0))),
                                                             rx_bf(world_x87_pass(rx_fb(t1)))));
        const float t2 = world_float_get(ra + 0x46c, c);
        const rx_addr entry = U32(ra, 0xc91c);
        if (t2 >= F32(entry, 0x24)) world_clamped_float_sub(ra + 0x464, entry + 0x24, WCTX(.esi = ra + 0x464, .edi = entry));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061dd60, rx_world_rider_update_trick)

/* the clamped float {min +0, max +4, float* target +8} at cf, *target += x (inlined ClampedFloat_Add: *target + x) */
static void clamped_add_inline(rx_addr cf, float x) {
    const rx_addr p = U32(cf, 8);
    const float v = rider_clamp((p ? F32(p, 0) : 0.0f) + x, F32(cf, 0), F32(cf, 4));
    if (p) SF32(p, 0, v);
}

/* 0061d8b0 RiderAnim_UpdateBlends (EAX = ra; stack dt; RET 4): the pose blends +0x584 (target +0x58c) and +0x590 (+0x598) rise
 * by 3 dt (inline; crashed states other than 4 through ClampedFloat_Add with the frame slot). Crashed (+0xaf10): the crash
 * animation (00621f40, 00620a60). Else in the pose state +0x5a8 the wrapped phase +0x4d0 advances by rate (+0x4cc) * dt and
 * wraps into [+0x4c0, +0x4c4] (through the argument slot); in the free state (+0xaf54 == 1) the wrapped float +0x538 advances by
 * dt; boosting, the hold time +0xaf40 runs down and releases chain group 5 when it is out. */
void rx_world_rider_update_blends(rx_addr ra, float dt) {
    world_frame f = world_frame_flat(2, 8);   /* sub esp, 8; push esi; push edi */
    const rx_addr slot = f.esp + 0xc, arg = f.esp + 0x14;
    const float r = dt * 3.0f /* 0x92fe48 */;
    SF32(slot, 0, r);
    const uint32_t crash = U32(ra, 0xaf10);
    if (crash == 0 || crash == 4) {
        clamped_add_inline(ra + 0x584, r);
        clamped_add_inline(ra + 0x590, r);
    } else {
        world_clamped_float_add(ra + 0x584, slot, WCTX(.esi = ra, .edi = 1));
        world_clamped_float_add(ra + 0x590, slot, WCTX(.esi = ra, .edi = 1));
    }
    if (U32(ra, 0xaf10) != 0) {
        world_rider_crash_step(ra, WCTX(.edi = 1));
        world_rider_crash_update(ra, world_x87_pass(rx_fb(dt)), WCTX(.esi = ra, .edi = 1));
    } else if (U16(ra, 0x5a8) == pose_state(ra)) {
        const rx_addr phase = ra + 0x4d0;
        const float g = world_float_get(phase, WCTX(.esi = ra, .edi = phase));
        const float x = (U32(ra, 0x4cc) ? F32(U32(ra, 0x4cc), 0) : 0.0f) * dt + g;
        const float hi = F32(ra, 0x4c4), lo = F32(ra, 0x4c0);
        float w;
        if (x > hi) w = x - hi;
        else w = lo > x ? (x - lo) + hi : x;
        SF32(arg, 0, w);
        world_float_set(phase, arg, WCTX(.esi = ra, .edi = phase));
    } else if (U32(ra, 0xaf54) == 1) {
        SF32(arg, 0, dt);   /* the original passes its argument slot */
        world_wrapped_float_add(ra + 0x538, arg, WCTX(.edi = 1));
    } else if (U32(ra, 0x5babc) != 0 && F32(ra, 0xaf40) > 0.0f) {
        const float t = F32(ra, 0xaf40) - dt;
        SF32(ra, 0xaf40, t);
        if (0.0f >= t) WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(ra, 5, 0.25f, 0));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061d8b0, rx_world_rider_update_blends)

/* a wrapped phase (listeners at ph, range [lo, hi]): get + rate * dt wrapped once into the range */
static float wrap_phase(float x, float lo, float hi) {
    if (x > hi) return x - hi;
    return lo > x ? (x - lo) + hi : x;
}

/* 0061e120 RiderAnim_UpdateIdle (ESI = ra; stack dt; RET 4): the off-bike idle states. Entering (+0x5b0 valid, the state
 * property +0x568 at +0x5ae and the body upright, 0042aca0): the state becomes +0x5b0 (dirty byte +0x564), the blend +0x58c
 * clamp(0), the phase +0x520 starts at -(min(+0x514, 0)) or -(+0x510) + (+0x514) when +0x510 > 0, +0x668 = 1. In the state
 * +0x5b0: the blend rises by dt and the phase +0x520 advances by rate +0x51c * dt, wrapped into [+0x510, +0x514]; else (when
 * +0x5ae is valid) the blend rises by 3 dt and the phase +0x4f8 advances by +0x4f4 * dt in [+0x4e8, +0x4ec]. The phase values
 * pass through the argument slot (the original's). */
void rx_world_rider_update_idle(rx_addr ra, float dt) {
    world_frame f = world_frame_flat(2, 0);   /* push ecx; push edi */
    const rx_addr arg = f.esp + 0xc;
    const uint16_t s = U16(ra, 0x5b0);
    const uint16_t cur = U32(ra, 0x568) ? U16(U32(ra, 0x568), 0) : 0;
    uint32_t upright = 0;
    if (s != 0xffff && cur == U16(ra, 0x5ae)) upright = world_rider_body_upright(ra + 0x5d0, WCTX(.ecx = ra + 0x5d0, .esi = ra));
    if (upright != 0) {
        if (U32(ra, 0x564) != 0) SU8(U32(ra, 0x564), 0, 1);
        if (U32(ra, 0x568) != 0) SU16(U32(ra, 0x568), 0, s);
        const float w = rider_clamp(0.0f, F32(ra, 0x584), F32(ra, 0x588));
        if (U32(ra, 0x58c) != 0) SF32(U32(ra, 0x58c), 0, w);
        const float m = F32(ra, 0x514), lo = F32(ra, 0x510);
        float x = 0.0f;
        if (0.0f > m) x = rx_bf(0x80000000u) /* 0x9774a8 */ - m;
        else if (lo > 0.0f) x = (rx_bf(0x80000000u) - lo) + m;
        SF32(arg, 0, x);
        world_float_set(ra + 0x520, arg, WCTX(.eax = arg, .ecx = ra + 0x520, .esi = ra));
        SU32(ra, 0x668, 1);
    } else if ((U32(ra, 0x568) ? U16(U32(ra, 0x568), 0) : 0) == s) {
        clamped_add_inline(ra + 0x584, dt);
        const float g = world_float_get(ra + 0x520, WCTX(.esi = ra, .edi = ra + 0x520));
        const float x = (U32(ra, 0x51c) ? F32(U32(ra, 0x51c), 0) : 0.0f) * dt + g;
        SF32(arg, 0, wrap_phase(x, F32(ra, 0x510), F32(ra, 0x514)));
        world_float_set(ra + 0x520, arg, WCTX(.esi = ra, .edi = ra + 0x520));
    } else if (U16(ra, 0x5ae) != 0xffff) {
        clamped_add_inline(ra + 0x584, dt * 3.0f /* 0x92fe48 */);
        const float g = world_float_get(ra + 0x4f8, WCTX(.esi = ra, .edi = ra + 0x4f8));
        const float x = (U32(ra, 0x4f4) ? F32(U32(ra, 0x4f4), 0) : 0.0f) * dt + g;
        SF32(arg, 0, wrap_phase(x, F32(ra, 0x4e8), F32(ra, 0x4ec)));
        world_float_set(ra + 0x4f8, arg, WCTX(.esi = ra, .edi = ra + 0x4f8));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061e120, rx_world_rider_update_idle)

/* 0061dfd0 RiderAnim_UpdateScripted (ECX = ra; stack dt; RET 4): a scripted clip (+0xc824) plays: its time +0x46c advances by
 * dt (clamped to [+0x464, +0x468], through the frame slot) with its chain events (00620fc0); with an animation entity +0xc930
 * its time +0xdb4 follows, it is blended (006b6d80 when it has keys, +0xda8) and its pose (+0x450, +0x460, +0x490) goes to the
 * bike simulation's vfunc 10. */
void rx_world_rider_update_scripted(rx_addr ra, float dt) {
    world_frame f = world_frame_enter(0x54, 3, 1);   /* push ebp; and esp, -16; sub esp, 0x54; push ebx; push esi; push edi */
    const rx_addr sp = f.esp, cookie = sp + 0x5c, slot = sp + 0x18, pose = sp + 0x20;
    SU32(cookie, 0, U32(0, 0x009b45e4) ^ (sp + 0xc));
    if (U32(ra, 0xc824) != 0) {
        const rx_addr t = ra + 0x46c;
        const world_ctx c = WCTX(.ebx = t, .esi = ra, .edi = t);
        const float t0 = world_float_get(t, c);
        SF32(sp, 0x1c, t0);
        const float x = world_float_get(t, c) + dt;
        SF32(slot, 0, rider_clamp(x, F32(ra, 0x464), F32(ra, 0x468)));
        world_float_set(t, slot, c);
        const float t1 = world_float_get(t, c);
        SF32(slot, 0, t1);
        WORLD_CALL_SP(0x10, rx_world_rider_play_clip_events(ra, U32(ra, 0xc824), rx_bf(world_x87_pass(rx_fb(t0))),
                                                             rx_bf(world_x87_pass(rx_fb(t1)))));
        if (U32(ra, 0xc930) != 0) {
            const float t2 = world_float_get(t, c);
            const rx_addr ent = U32(ra, 0xc930);
            SF32(ent, 0xdb4, t2);
            if (U32(ent, 0xda8) != 0) world_anim_entity_blend(ent, c);
            const rx_addr e = U32(ra, 0xc930);
            v4_st(pose, v4_ld(e + 0x450));
            v4_st(pose + 0x10, v4_ld(e + 0x460));
            v4_st(pose + 0x20, v4_ld(e + 0x490));
            const rx_addr sim = U32(U32(ra, 0xaf1c), 0x234);
            WORLD_CTX(U32(U32(sim, 0), 0x28), &WCTX(.eax = pose, .ecx = sim, .ebx = t, .esi = U32(ra, 0xaf1c), .edi = t), 0, pose);
        }
    }
    phys_security_check_cookie(U32(cookie, 0) ^ (sp + 0xc));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061dfd0, rx_world_rider_update_scripted)

/* 00621400 RiderAnim_EndStunt (EAX = ra): when a stunt pose runs (+0x5b2e0): the clamped float +0x3c8 to clamp(0), +0x5b2e0
 * cleared, animation state 4 and +0x5b6f0 = 1. */
void rx_world_rider_end_stunt(rx_addr ra) {
    if (U32(ra, 0x5b2e0) == 0) return;
    const float w = rider_clamp(0.0f, F32(ra, 0x3c8), F32(ra, 0x3cc));
    if (U32(ra, 0x3d0) != 0) SF32(U32(ra, 0x3d0), 0, w);
    SU32(ra, 0x5b2e0, 0);
    anim_switch(ra, 4);
    SU32(ra, 0x5b6f0, 1);
}
RX_FUNCTION(0x00621400, rx_world_rider_end_stunt)

/* 00622e30 RiderAnim_EndCrash (stdcall ra; RET 4): leaving a crash state (+0xaf10 not 0 / 4): the blend of the crash layer
 * (+0x584 by +0xaf28 == 1, pose state +0x5ac; else +0x590, state +0x5a0) to clamp(0), the trick / crash times +0x464 / +0x484
 * to clamp(0) (listeners +0x46c / +0x48c, through the frame slot), the crash state to 4 with its fields cleared, the pose
 * override ended (+0xc928), chain group 4 back in over 0.25 s, the time listeners enabled / disabled, animation state 4. */
void rx_world_rider_end_crash(rx_addr ra) {
    world_frame f = rider_frame_enter(4, 3, 8);   /* push ebp; and esp, -8; push ecx; push ebx; push esi; push edi */
    const rx_addr slot = f.esp + 0xc;
    const uint32_t crash = U32(ra, 0xaf10);
    if (crash != 0 && crash != 4) {
        if (U32(ra, 0xaf28) == 1) {
            const float w = rider_clamp(0.0f, F32(ra, 0x584), F32(ra, 0x588));
            if (U32(ra, 0x58c) != 0) SF32(U32(ra, 0x58c), 0, w);
            if (U32(ra, 0x564) != 0) SU8(U32(ra, 0x564), 0, 1);
            if (U32(ra, 0x568) != 0) SU16(U32(ra, 0x568), 0, U16(ra, 0x5ac));
        } else {
            blend_zero(ra);
            snap_state(ra, 0x5a0);
        }
        SF32(slot, 0, rider_clamp(0.0f, F32(ra, 0x464), F32(ra, 0x468)));
        world_float_set(ra + 0x46c, slot, WCTX(.eax = slot, .ecx = ra + 0x46c, .ebx = ra, .edi = 1));
        SF32(slot, 0, rider_clamp(0.0f, F32(ra, 0x484), F32(ra, 0x488)));
        world_float_set(ra + 0x48c, slot, WCTX(.eax = ra + 0x48c, .edx = slot, .ebx = ra, .edi = 1));
        SU32(ra, 0xaf10, 4);
        SU32(ra, 0xaf14, 0); SU32(ra, 0xaf18, 0); SU32(ra, 0xaf28, 0); SU32(ra, 0xaf30, 0); SU32(ra, 0xaf38, 0);
        SU32(ra, 0xaf34, 1);
        SU32(ra, 0x5b2d4, 0);
        if (U32(ra, 0xc928) != 0) world_simulation_end_pose_override(U32(U32(ra, 0xaf1c), 0x234), WCTX(.ebx = ra));
        SU32(ra, 0xc928, 0);
        SU32(ra, 0xc92c, 0);
        WORLD_CALL_SP(4, rx_world_rider_chains_fade_in(ra, 4, 0, 0.25f));
        world_float_listeners_enable_all(ra + 0x46c, WCTX(.ebx = ra));
        world_float_listeners_disable_all(ra + 0x48c, WCTX(.ebx = ra));
        anim_switch(ra, 4);
        SU32(ra, 0x5b6f0, 1);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00622e30, rx_world_rider_end_crash)

/* 00621b90 RiderAnim_StartScripted (EDI = ra; stack index; RET 4): ends the pose override (00621490: its argument is the
 * original's pushed EDI), the stunt (00621400), the trick (00621320) and the crash (00622e30); entry index of the scripted
 * table (ra +0xc59c, 0x28 bytes) becomes +0xc820 and its clip is looked up in the character's map (+0x144 +0x248, 007fa030;
 * key and result in the frame) into +0xc824. With a clip: +0x5b57c = index, the idle state property +0x568 = the entry's +0x20
 * (dirty +0x564), the blend +0x58c clamp(1), the time +0x464 clamp(0) and its range (0, clip length +0x94), the animation
 * entity ra +0x49cf0 + 0xe10 index (+0xc930; none without keys +0xe00; else, unless 0xa942b4 is 2, the bike simulation's
 * +0x2de4 = 1 and its vfunc 0x84), animation state 3. */
void rx_world_rider_start_scripted(rx_addr ra, uint32_t index) {
    world_frame f = world_frame_enter(0x18, 3, 1);   /* push ebp; and esp, -16; sub esp, 0x18; push ebx; push esi; push edi */
    SU32(f.esp, 0, ra);                              /* the pushed EDI, 00621490's argument (its RET 4 pops it) */
    rx_set_sp(f.esp + 4);
    WORLD_CALL_SP(4, rx_world_rider_end_pose_override(ra));
    WORLD_CALL_SP(0, rx_world_rider_end_stunt(ra));
    WORLD_CALL_SP(0, rx_world_rider_end_trick(ra));
    WORLD_CALL_SP(4, rx_world_rider_end_crash(ra));
    const rx_addr sp = f.esp + 4, out = sp + 0x14, key = sp + 0x18;
    const rx_addr entry = ra + 0xc59c + index * 0x28u;
    SU32(sp, 0x1c, entry);
    SU32(ra, 0xc820, entry);
    SU32(key, 0, entry);
    SU32(out, 0, 0);
    world_map_find(U32(ra, 0x144) + 0x248, key, out, WCTX(.ebx = index, .edi = ra));
    const rx_addr clip = U32(out, 0);
    SU32(ra, 0xc824, clip);
    if (clip != 0) {
        SU32(ra, 0x5b57c, index);
        if (U32(ra, 0x564) != 0) SU8(U32(ra, 0x564), 0, 1);
        if (U32(ra, 0x568) != 0) SU16(U32(ra, 0x568), 0, U16(U32(sp, 0x1c), 0x20));
        const float w = rider_clamp(1.0f, F32(ra, 0x584), F32(ra, 0x588));
        if (U32(ra, 0x58c) != 0) SF32(U32(ra, 0x58c), 0, w);
        SF32(out, 0, rider_clamp(0.0f, F32(ra, 0x464), F32(ra, 0x468)));
        world_float_set(ra + 0x46c, out, WCTX(.eax = ra + 0x46c, .edx = out, .ebx = index, .edi = ra));
        const float len = F32(U32(ra, 0xc824), 0x94);
        if (!(0.0f >= len)) { SF32(ra, 0x464, 0.0f); SF32(ra, 0x468, len); }
        const rx_addr ent = ra + 0x49cf0 + index * 0xe10u;
        SU32(ra, 0xc930, ent);
        if (U32(ent, 0xe00) == 0) {
            SU32(ra, 0xc930, 0);
        } else if (U32(0, 0x00a942b4) != 2) {
            SU32(U32(U32(ra, 0xaf1c), 0x234), 0x2de4, 1);
            const rx_addr sim = U32(U32(ra, 0xaf1c), 0x234);
            WORLD_CTX0(U32(U32(sim, 0), 0x84), &WCTX(.ecx = sim, .ebx = ent, .edi = ra), 0);
        }
        anim_switch(ra, 3);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00621b90, rx_world_rider_start_scripted)

/* 00621b20 RiderAnim_StartScriptedMode (EAX = ra, ESI = index): 00621b90, then the scripted mode +0x5b580 by index: 0..3 -> 0,
 * 4..7 -> 1, 8..11 -> 2, else 3 (byte table 0x621b78). */
void rx_world_rider_start_scripted_mode(rx_addr ra, uint32_t index) {
    world_frame f = world_frame_flat(1, 0);   /* push edi */
    WORLD_CALL_SP(4, rx_world_rider_start_scripted(ra, index));
    SU32(ra, 0x5b580, 3);
    if (index <= 0xb) SU32(ra, 0x5b580, index / 4);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00621b20, rx_world_rider_start_scripted_mode)

/* 00621ab0 RiderAnim_UpdateScriptedStart (EAX = ra): the scripted start request +0x5b588: 1 waits for both blends (+0x58c,
 * +0x598) full and becomes 2; 2 starts the scripted clip +0x5b584 (00621b90) and clears the request. */
void rx_world_rider_update_scripted_start(rx_addr ra) {
    world_frame f = world_frame_flat(1, 0);   /* push edi */
    const uint32_t req = U32(ra, 0x5b588);
    if (req == 2) {
        SU32(ra, 0x5b588, 0);
        WORLD_CALL_SP(4, rx_world_rider_start_scripted(ra, U32(ra, 0x5b584)));
    } else if (req == 1 && U32(ra, 0x58c) != 0 && F32(U32(ra, 0x58c), 0) == 1.0f && target_blend_full(ra)) {
        SU32(ra, 0x5b588, 2);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00621ab0, rx_world_rider_update_scripted_start)
