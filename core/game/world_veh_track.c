/* world_veh_track.c -- the wheels' terrain marks (0x004607e0 .. 0x00461212, 0x00468120): the visual tire tracks (TerrainTracks
 * strips) and the terrain deformation stamps (ruts and berms) of a motocross wheel, and the slip track mark.
 * _re/notes/terrain-deformation.md 1.1 (the algorithm), rt-bump.md (the tracks). core/notes/world.md (veh). */
#include "world_veh.h"
#include "world_veh_util.h"

/* The wheel's parameter block (ECX of 004607e0, built on the stack by 00468300; vectors are pointers to 16-byte values):
 *   +0 the wheel state W (+0x80 tire force F, +0xc0 / +0xc4 contact flags, +0xe0 the filtered load Wf, +0x140 lateral axis),
 *   +4 contact point A, +8 the previous stamp point, +0xc the wheel axis N, +0x10 the berm axis B, +0x14 the velocity V,
 *   +0x18 timer (float), +0x1c previous depth (float), +0x20 radius r, +0x24 / +0x28 / +0x2c track width / alpha lerp ends,
 *   +0x30 the depth lag filter, +0x34 the track handle (bits 0x1000 / 0x2000 / 0x4000 pick the TerrainTracks instance),
 *   +0x38 stamping flag, +0x3c contact flag, +0x40 the "keep the strip" flag, +0x44 deformation enable, +0x48 the track uv
 *   vector, +0x4c the load divisor, +0x50 the uv default, +0x54 .. +0x84 and +0x90 .. +0x9c pointers to the constant vectors
 *   of the depth / berm formulas, +0x88 / +0x8c the berm width lerp ends. */
#define PV(o) v4_ld(U32(P, (o)))     /* a block vector */
#define PI(o) U32(U32(P, (o)), 0)    /* a block int through its pointer */

static rx_addr tracks_instance(uint32_t* handle) {
    if (*handle & 0x1000) { *handle ^= 0x1000; return U32(0, 0x00a95774); }
    if (*handle & 0x2000) { *handle ^= 0x2000; return U32(0, 0x00a95778); }
    *handle ^= 0x4000; return U32(0, 0x00a9577c);
}

/* 004607e0 Vehicle_WheelDeformAndTrackMarks (thiscall ECX = block, stdcall vehicle, terrain surface, dt; RET 0xc).
 * Tracks (vehicle +0x238): in contact, a strip point (A, A - N r) with width lerp(P24, r, m), lerp(P2c, P28, m) and the alpha
 * min(1, Wf / P4c + uv); out of contact the strip continues with alpha 0 while +0x40 is set, else it is cut.
 * Deformation (+0x44 and vehicle +0x23c): depth / berm from the tire force, the load and the speed; every 1/30.5 s in contact
 * (or once more when contact ends while stamping) a rut stamp (previous point -> A - U / 2) and a berm stamp beside it;
 * between stamps the depth filter follows; out of contact the state resets. */
void rx_world_wheel_deform_and_track_marks(rx_addr P, rx_addr veh, rx_addr surf, float dt) {
    world_frame f = world_frame_enter(0xf4, 3, 1);   /* push ebp; and esp, -16; sub esp, 0xf4; push ebx, esi, edi */
    const rx_addr sp = f.esp;
    const rx_addr s20 = sp + 0x20, s80 = sp + 0x80, s90 = sp + 0x90;   /* vectors passed by address */
    const world_ctx cx = WCTX(.ebx = surf, .edi = P);
    const rx_addr W = U32(P, 0);
    const rx_v4 zero = v4_ld(WORLD_ZERO4), one = v4_ld(0x00d61650);
    const rx_v4 A = PV(4), N = PV(0xc), V = PV(0x14), F = v4_ld(W + 0x80);
    const rx_v4 r = v4_splat(F32(P, 0x20));
    const rx_v4 A_back = v4_sub(A, v4_mul(N, r));
    /* the axis and the velocity in the ground plane (y from the zero vector), their mean direction U, m = |U . Nh| */
    const rx_v4 Nh = v4_normalize_or_nan(v4(N.v[0], zero.v[1], N.v[2], N.v[3]));
    const rx_v4 VN = v4_add(v4(V.v[0], zero.v[1], V.v[2], V.v[3]), Nh);
    const rx_v4 U = v4_normalize_or_nan(VN);
    const rx_v4 c = v4_splat(phys_dot3(U, Nh));
    const rx_v4 m = v4_max(v4_sub(v4_zero(), c), c), one_m = v4_sub(one, m);
    const rx_v4 Wf = v4_splat(F32(W, 0xe0));
    const rx_v4 width = v4_add(v4_mul(v4_splat(F32(P, 0x24)), one_m), v4_mul(m, r));
    const rx_v4 width2 = v4_add(v4_mul(v4_splat(F32(P, 0x2c)), one_m), v4_mul(v4_splat(F32(P, 0x28)), m));
    rx_v4 uv = PV(0x48);
    uint32_t uv_flag = 1;
    if (U32(W, 0xc4) == 0 || U32(W, 0xc0) == 0) { uv = v4_splat(F32(P, 0x50)); uv_flag = 0; }

    if (U32(veh, 0x238) != 0) {
        uint32_t h = U32(P, 0x34);
        if (PI(0x3c) != 0) {
            v4_st(s80, A);
            v4_st(s20, A_back);
            const rx_v4 alpha = v4_min(v4_splat(1.0f /* 0x931f24 */), v4_add(v4_div(Wf, PV(0x4c)), uv));
            const rx_addr tracks = tracks_instance(&h);
            veh_tracks_add_point(tracks, h, s80, s20, world_x87_pass(v4_lb(width, 0)), world_x87_pass(v4_lb(width2, 0)),
                                 world_x87_pass(v4_lb(alpha, 0)), uv_flag, cx);
        } else if (PI(0x40) != 0) {
            v4_st(s20, A);
            v4_st(s80, A_back);
            const rx_addr tracks = tracks_instance(&h);
            veh_tracks_add_point(tracks, h, s20, s80, world_x87_pass(v4_lb(width, 0)), world_x87_pass(v4_lb(width2, 0)), 0,
                                 uv_flag, cx);
        } else {
            const rx_addr tracks = tracks_instance(&h);
            veh_tracks_cut_strip(tracks, h, WCTX(.edx = U32(P, 0x3c), .ebx = surf, .edi = P));
        }
    }

    if (U32(P, 0x44) == 0) { world_frame_leave(&f); return; }
    const rx_addr timer = U32(P, 0x18), prev_z = U32(P, 0x1c), filter = U32(P, 0x30), prev = U32(P, 8);
    if (U32(veh, 0x23c) != 0) {
        /* rut depth: T1 from |F.x|, T2 from |F.z|, T3 from the load, times the speed factor */
        const rx_v4 fx = v4_lane(F, 0), fz = v4_lane(F, 2);
        const rx_v4 afx = v4_max(v4_sub(v4_zero(), fx), fx);
        const rx_v4 nfz = v4_sub(v4_zero(), fz), afz = v4_max(v4_sub(v4_zero(), nfz), nfz);
        const rx_v4 T1 = v4_min(PV(0x58), v4_max(zero, v4_mul(v4_div(afx, PV(0x54)), PV(0x58))));
        const rx_v4 T2 = v4_min(one, v4_div(afz, PV(0x5c)));
        const float speed2 = v4_len2_xzy(V);
        const rx_v4 T3 = v4_min(PV(0x6c), v4_max(zero, v4_mul(v4_div(v4_sub(Wf, PV(0x64)), PV(0x68)), PV(0x6c))));
        const rx_v4 T2c = v4_min(PV(0x60), v4_max(zero, v4_mul(T2, PV(0x60))));
        const rx_v4 sum = v4_add(T3, v4_add(T2c, T1));
        const rx_v4 spd = v4_min(one, v4_div(v4_sqrt(v4_splat(speed2)), PV(0x70)));
        const rx_v4 depth = v4_min(PV(0x74), v4_mul(sum, spd));
        /* berm: max(|U . lateral|, (1 - e) e) with e = min(1, (VN . Nh) / P80), times the load / force factor */
        const rx_v4 du = v4_splat(phys_dot3(U, v4_ld(W + 0x140)));
        const float e1 = phys_dot3(VN, Nh);
        const rx_v4 G = v4_mul(v4_sub(one, v4_min(one, v4_div(v4_splat(e1), PV(0x80)))),
                               v4_min(one, v4_div(v4_splat(e1), PV(0x80))));
        const rx_v4 adu = v4_max(v4_sub(v4_zero(), du), du);
        const rx_v4 k = v4_add(v4_mul(PV(0x78), T2), T1);
        const rx_v4 berm = v4_mul(v4_max(PV(0x84), v4_max(adu, G)),
                                  v4_min(PV(0x7c), v4_mul(v4_min(one, v4_div(Wf, v4_splat(400.0f /* 0x977b50 */))), k)));
        const float t = F32(timer, 0) + dt;
        SF32(timer, 0, t);
        const uint32_t contact = PI(0x3c);
        int stamp;
        if (contact != 0) stamp = F32(timer, 0) > 0.0327868834f /* 0x977b4c */;
        else stamp = PI(0x38) != 0;
        if (stamp) {
            SF32(timer, 0, 0.0f);
            const float spacing = F32(surf, 0x44e8);
            const float hw = (F32(P, 0x20) > spacing ? F32(P, 0x20) : spacing) * 2.0f /* 0x92e330 */;
            const rx_v4 PA = v4_sub(A, v4_mul(U, v4_splat(0.5f /* 0x9322cc */)));
            v4_st(s20, PA);
            v4_st(s90, v4_ld(prev));
            const float len = veh_sqrtf(phys_add(phys_add(phys_mul(VN.v[1], VN.v[1]), phys_mul(VN.v[0], VN.v[0])),
                                                 phys_mul(VN.v[2], VN.v[2])), cx);
            const float x = phys_div(phys_sub(len, 10.0f /* 0x92fd88 */), 15.0f /* 0x9322d0 */);
            const float xc = x >= 1.0f ? 1.0f : (0.0f >= x ? 0.0f : x);
            const float Fs = veh_lerp_clamped(0x3e800000u /* 0.25, 0x92ff84 */, 0x3f800000u /* fld1 */, v4(xc, 0, 0, 0), cx);
            const float target = phys_mul(depth.v[0], Fs);
            float zf = target;
            if (PI(0x38) != 0) zf = veh_lag_filter_update(filter, target, dt, cx);
            else veh_lag_filter_set(filter, target, 0x7f7fffffu /* FLT_MAX: tau kept, 0x977494 */, cx);
            veh_deform_add_stamp(surf + 0x230, s90, s20, world_x87_pass(rx_fb(zf)), world_x87_pass(U32(prev_z, 0)),
                                 world_x87_pass(rx_fb(hw)), world_x87_pass(rx_fb(hw)), cx);
            SF32(prev_z, 0, zf);
            /* the berm beside the rut, along B */
            const rx_v4 B = PV(0x10);
            const rx_v4 am = v4_max(v4_sub(v4_zero(), m), m);
            const rx_v4 Q1 = v4_sub(A, v4_mul(v4_add(v4_mul(v4_sub(one, am), PV(0x90)), PV(0x94)), B));
            const rx_v4 Q2 = v4_sub(Q1, v4_mul(v4_add(v4_mul(PV(0x9c), T2), PV(0x98)), B));
            v4_st(s20, Q1);
            v4_st(s90, Q2);
            const float am_x = rx_world_float_abs(m.v[0]);
            const float bw = veh_lerp_clamped(world_x87_pass(U32(P, 0x8c)), world_x87_pass(U32(P, 0x88)), v4(am_x, 0, 0, 0), cx);
            const float zb = phys_mul(rx_bf(rx_fb(berm.v[0]) ^ 0x80000000u), Fs);   /* fld; fchs; fmul (exact in the x87) */
            veh_deform_add_stamp(surf + 0x230, s90, s20, rx_fb(zb), rx_fb(zb), rx_fb(bw), rx_fb(bw), cx);
            SU32(U32(P, 0x38), 0, (PI(0x3c) != 0 && depth.v[0] > 0.0f /* 0x92e12c */) ? 1 : 0);
            v4_st(prev, v4_mul(v4_add(PA, v4_ld(prev)), v4_ld(0x00d615d0)));
            world_frame_leave(&f);
            return;
        }
        if (contact != 0) {
            veh_lag_filter_update(filter, depth.v[0], dt, cx);
            world_frame_leave(&f);
            return;
        }
    }
    /* out of contact (or deformation off): reset */
    SF32(prev_z, 0, 0.0f);
    v4_st(prev, A);
    veh_lag_filter_set(filter, 0.0f, 0x7f7fffffu, cx);
    SF32(timer, 0, 0.0f);
    world_frame_leave(&f);
}
RX_FUNCTION(0x004607e0, rx_world_wheel_deform_and_track_marks)

/* 00468120 Vehicle_SlipTrackMark (EDI = block, stdcall vehicle; RET 4): the slip / skid track strip of a wheel. Block: +0 the
 * point, +4 the previous point, +8 the load, +0xc / +0x10 the two widths, +0x14 the track handle, +0x18 contact flag, +0x1c
 * the "keep the strip" flag, +0x20 / +0x24 the alpha offset / divisor vectors. Nothing while the rider (vehicle +0x1a0) is
 * off (+0xaf00). In contact a strip point with alpha min(1, load / div + off); out of contact alpha 0 while kept, else the
 * strip is cut; then previous = point. */
void rx_world_slip_track_mark(rx_addr Q, rx_addr veh) {
    world_frame f = world_frame_enter(0x3c, 1, 1);   /* push ebp; and esp, -16; sub esp, 0x3c; push esi */
    const rx_addr s10 = f.esp + 0x10, s20 = f.esp + 0x20;
    if (U32(U32(veh, 0x1a0), 0xaf00) != 0) { world_frame_leave(&f); return; }
    const world_ctx cx = WCTX(.edi = Q);
    const rx_v4 cur = v4_ld(U32(Q, 0)), prev = v4_ld(U32(Q, 4));
    uint32_t h = U32(Q, 0x14);
    const uint32_t w1 = world_x87_pass(U32(Q, 0xc)), w2 = world_x87_pass(U32(Q, 0x10));
    if (U32(U32(Q, 0x18), 0) != 0) {
        v4_st(s10, cur);
        v4_st(s20, prev);
        const rx_v4 alpha = v4_min(v4_splat(1.0f /* 0x931f24 */), v4_add(v4_div(v4_splat(F32(Q, 8)), v4_ld(U32(Q, 0x24))),
                                                                       v4_ld(U32(Q, 0x20))));
        const rx_addr tracks = tracks_instance(&h);
        veh_tracks_add_point(tracks, h, s10, s20, w1, w2, world_x87_pass(v4_lb(alpha, 0)), 0, cx);
    } else if (U32(U32(Q, 0x1c), 0) != 0) {
        v4_st(s20, cur);
        v4_st(s10, prev);
        const rx_addr tracks = tracks_instance(&h);
        veh_tracks_add_point(tracks, h, s20, s10, w1, w2, 0, 0, cx);
    } else {
        const rx_addr tracks = tracks_instance(&h);
        veh_tracks_cut_strip(tracks, h, WCTX(.ecx = U32(Q, 0x18), .edx = U32(Q, 0), .edi = Q));
    }
    v4_st(U32(Q, 4), v4_ld(U32(Q, 0)));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00468120, rx_world_slip_track_mark)
