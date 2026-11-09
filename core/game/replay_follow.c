/* Working translation, not composed: raw frame-spill audit remains in progress. */
#include "replay_follow.h"
#include "rider_cap.h"
#include "replay_decode.h"
#include "build_activity_util.h"
#include "phys_mech.h"
#include "phys_vec.h"
#include "phys_crt.h"
#define REPLAY_END(F,off) do {rx_set_sp((F)+8); ACT_CALL(0,phys_security_check_cookie(U32(F,off)^((F)+8)));ACT_END(F);}while(0)


void rx_replay_follow_begin(rx_addr simulation, int32_t end, int32_t start) {
    ACT_FRAME_EBP(f, 16, 0xb8, 2);
    SU32(f,0xbc,U32(0,0x009b45e4)^(f+8));
    if (U32(0, 0x00d92d44)) { start=(int32_t)((uint32_t)start+2u); end=(int32_t)((uint32_t)end-2u); }
    SU32(simulation, 0x2e84, start);
    SU32(simulation, 0x2e8c, start);
    SU32(simulation, 0x2e80, 1);
    SU32(simulation, 0x2e7c, 1);
    SU32(simulation, 0x2e88, end);
    rx_addr sample = rx_clip_record((uint32_t)start);
    SF32(simulation, 0x2e90, rx_f80_to_f32(rx_f80_from_f32(F32(sample, 0x20)), 0));
    SU32(simulation, 0x2e94, 0);
    replay_sample d = rx_replay_decode_record(sample);
    rx_st4(f + 0x90, d.q);
    rx_st4(f + 0xa0, d.p);
    rx_addr target = U32(U32(simulation, 0), 8);
    if (!rx_fn(target)) act_unreached(target, "ReplayFollow Begin simulation vf2");
    ACT_CALL(2, ((void (*)(rx_addr,rx_addr,rx_addr))rx_fn(target))(simulation, f + 0xa0, f + 0x90));
    ACT_CALL(0, phys_mech_set_velocity(simulation + 0xd0, 0x00d61670));
    rx_st4(simulation + 0x100, d.v);
    rx_st4(simulation + 0x740, d.w);
    rx_v4 world_w = phys_quat_mul(d.q, phys_quat_mul(v4_xyz(d.w), phys_quat_conj(d.q)));
    for (int32_t i = 1; i < I32(simulation, 0xec); ++i) {
        rx_addr body = simulation + 0xf0 + (uint32_t)i * 0xa0;
        rx_v4 offset = v4_sub(rx_ld4(body), d.p);
        rx_st4(body + 0x10, v4_add(phys_cross_ba(offset, world_w), d.v));
        rx_v4 body_q = rx_ld4(body + 0x640);
        rx_st4(body + 0x650, phys_quat_mul(phys_quat_conj(body_q), phys_quat_mul(v4_xyz(world_w), body_q)));
    }
    /* Original local-player path scans/decompresses all samples into private
     * scratch; retaining it is required for eventual frame comparison. */
    if (I32(simulation, 4) == 0) {
        for (int32_t i = I32(simulation,0x2e84); i <= I32(simulation,0x2e88); i=(int32_t)((uint32_t)i+1u)) {
            replay_sample scan = rx_replay_decode_record(rx_clip_record((uint32_t)i));
            rx_st4(f + 0x70, scan.v);
            rx_st4(f + 0x30, scan.p);
        }
    }
    if (I32(simulation, 4) == 0) {
        for (int32_t i = I32(simulation,0x2e84); i <= I32(simulation,0x2e88); i=(int32_t)((uint32_t)i+1u)) {
            rx_v4 q = rx_replay_half4_decode(rx_ld4(rx_clip_record((uint32_t)i) + 0x10));
            rx_st4(f + 0x90, q);
            rx_st4(f + 0x80, q);
        }
    }
    REPLAY_END(f,0xbc);
}
RX_FUNCTION(0x00433ec0, rx_replay_follow_begin)

void rx_replay_follow_step(rx_addr simulation, float dt) {
    ACT_FRAME_EBP(f, 16, 0xe8, 2);
    SU32(f,0xec,U32(0,0x009b45e4)^(f+8));
    if (I32(simulation, 0x2e94) > 0) { REPLAY_END(f,0xec); return; }
    int32_t index = I32(simulation, 0x2e8c);
    rx_addr sample = rx_clip_record((uint32_t)index);
    if (F32(simulation, 0x2e90) > F32(sample, 0x70)) {
        if (index >= (int32_t)(U32(simulation, 0x2e88) - 1u)) {
            SU32(simulation, 0x2e94, 1); REPLAY_END(f,0xec); return;
        }
        index=(int32_t)((uint32_t)index+1u); SU32(simulation, 0x2e8c, index);
        sample = rx_clip_record((uint32_t)index);
        uint32_t offbike = (U32(sample, 0x40) >> 19) & 1u;
        if (offbike && !U32(simulation, 0x2374)) SU32(simulation, 0x2e9c, 1);
        if (!offbike && U32(simulation, 0x2374)) {
            SU32(simulation, 0x2e94, 3); REPLAY_END(f,0xec); return;
        }
    }
    float duration = phys_sub(F32(sample, 0x70), F32(sample, 0x20));
    SF32(f, 8, duration);
    if (0.0f >= duration || duration > F32(0, 0x009322cc)) {
        SU32(simulation, 0x2e94, 2); REPLAY_END(f,0xec); return;
    }
    replay_sample a = rx_replay_decode_record(sample), b = rx_replay_decode_record(sample + 0x50);
    rx_v4 delta = v4_sub(b.p, a.p), squares = v4_mul(delta, delta);
    float length = phys_sqrt(phys_add(phys_add(squares.v[0], squares.v[2]), squares.v[1]));
    rx_st4(f + 0xb0, b.p); rx_st4(f + 0xd0, b.q);
    if (F32(0, 0x009322d0) < length || (U32(sample, 0x90) & 0x40000u)) {
        rx_addr target = U32(U32(simulation, 0), 8);
        if (!rx_fn(target)) act_unreached(target, "ReplayFollow Step simulation vf2");
        ACT_CALL(2, ((void (*)(rx_addr,rx_addr,rx_addr))rx_fn(target))(simulation, f + 0xb0, f + 0xd0));
        SF32(simulation, 0x2e90, phys_add(F32(simulation, 0x2e90), dt));
        REPLAY_END(f,0xec); return;
    }
    float alpha = phys_div(phys_sub(F32(simulation, 0x2e90), F32(sample, 0x20)), duration);
    SF32(f, 0xc, alpha);
    rx_v4 t = v4_splat(alpha);
    rx_v4 correction = v4_sub(v4_add(v4_mul(delta, t), a.p), rx_ld4(simulation + 0xf0));
    rx_v4 target_q;
    ACT_CALLR(target_q, 0, phys_quat_slerp(a.q, b.q, t));
    rx_v4 relative_q = phys_quat_mul(phys_quat_conj(rx_ld4(simulation + 0x730)), target_q);
    float raw_factor = phys_div(dt, phys_mul(duration, F32(0, 0x0092fe48)));
    SF32(f, 8, raw_factor);
    float one = F32(0, 0x00931f24);
    float factor = one > raw_factor ? raw_factor : one; /* unordered selects one */
    correction = v4_mul(v4_splat(factor), correction);
    rx_st4(f + 0x10, correction);
    rx_v4 rotation;
    ACT_CALLR(rotation, 0, phys_quat_slerp(rx_ld4(0x00d61640), relative_q, v4_splat(factor)));
    rx_v4 r2 = v4_mul(rotation, rotation);
    /* Exact 0043487b..489a shuffle/add association. */
    rx_v4 r1 = v4_shuf(r2, r2, 0xee);
    rx_v4 sum = v4_add(r1, r2);
    sum = v4_shuf(sum, sum, 0x40);
    r1 = v4_shuf(r1, sum, 0xf0);
    r1 = v4_add(r1, sum);
    r1 = v4_shuf(r1, r1, 0xaa);
    rx_v4 root = v4_sqrt(r1), normalized = v4_div(rotation, root);
    for (int lane = 0; lane < 4; ++lane) {
        uint32_t nonzero = root.v[lane] != 0.0f ? ~0u : 0;
        uint32_t finite = r1.v[lane] != F32(0,0x0092dc20 + (uint32_t)lane*4) ? ~0u : 0;
        normalized.v[lane] = rx_bf((rx_fb(normalized.v[lane]) & nonzero & finite)
                            | (U32(0,0x0092dc30 + (uint32_t)lane*4) & ~finite));
    }
    rx_st4(f + 0x60, normalized);
    ACT_CALL(0, phys_mech_translate(simulation + 0xd0, f + 0x10));
    ACT_CALL(0, phys_mech_rotate(simulation + 0xd0, f + 0x60));
    rx_v4 linear = v4_sub(v4_add(v4_mul(v4_sub(b.v, a.v), t), a.v), rx_ld4(simulation + 0x100));
    rx_v4 angular = v4_sub(v4_add(v4_mul(v4_sub(b.w, a.w), t), a.w), rx_ld4(simulation + 0x740));
    linear = v4_mul(v4_splat(factor), linear);
    angular = v4_mul(v4_splat(factor), angular);
    for (int32_t i = 0; i < I32(simulation, 0xec); ++i) {
        rx_addr velocity = simulation + 0x100 + (uint32_t)i * 0xa0;
        rx_st4(velocity, v4_add(linear, rx_ld4(velocity)));
    }
    rx_v4 root_q = rx_ld4(simulation + 0x7c0);
    rx_v4 world_w = phys_quat_mul(root_q, phys_quat_mul(v4_xyz(angular), phys_quat_conj(root_q)));
    rx_st4(simulation + 0x740, v4_add(angular, rx_ld4(simulation + 0x740)));
    for (int32_t i = 1; i < I32(simulation, 0xec); ++i) {
        rx_addr velocity = simulation + 0x100 + (uint32_t)i * 0xa0;
        rx_v4 offset = v4_sub(rx_ld4(velocity + 0x80), rx_ld4(simulation + 0x180));
        rx_st4(velocity, v4_add(phys_cross_ba(offset, world_w), rx_ld4(velocity)));
        rx_v4 body_q = rx_ld4(velocity + 0x6c0);
        rx_v4 local_w = phys_quat_mul(phys_quat_conj(body_q), phys_quat_mul(v4_xyz(world_w), body_q));
        rx_st4(velocity + 0x640, v4_add(local_w, rx_ld4(velocity + 0x640)));
    }
    const uint32_t input_offsets[7] = {0x1c,0x24,0x28,0x2c,0x30,0x34,0x38};
    float complement = phys_sub(one, alpha);
    for (uint32_t i = 0; i < 7; ++i)
        SF32(simulation, input_offsets[i], phys_add(phys_mul(F32(sample,0x24+4*i),complement),
                                                    phys_mul(F32(sample,0x74+4*i),alpha)));
    SU32(simulation, 0x48, U16(sample, 0x42) & 1u);
    SF32(simulation, 0x20, U32(sample,0x40) & 0x20000u ? one : 0.0f);
    SF32(simulation, 0x2e90, phys_add(F32(simulation, 0x2e90), dt));
    REPLAY_END(f,0xec);
}
RX_FUNCTION(0x004343e0, rx_replay_follow_step)
