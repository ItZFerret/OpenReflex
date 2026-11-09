/* loop_cam_misc.c -- the smaller gameplay cameras: MXStartGateCamera (the grid camera before the gate drops), MXReplayCamera and
 * MXReplayTowerCamera (the replay cameras on towers around the track), and the player's analog-input check they use.
 *
 * Fields (camera `cam`, an MXPlayerCamera, loop_cam_player.c):
 *  start gate: +0x350 hold timer, +0x354 shake clock, +0x360 the looked-at point, +0x398 "still on the gate camera";
 *  replay: +0x318 retarget clock, +0x31c / +0x328 drift distance factor, +0x324 retarget period, +0x340 the vector spring
 *  {offset, target +0x350, velocity} (accel +0x320, max speed +0x32c, k +0x330), +0x370 drifting on;
 *  replay tower: +0x380 fixed pose, +0x3c0..+0x3c8 the fovs of three ranges +0x3cc..+0x3d4, +0x3d8 fov acceleration, +0x3f4 aimed,
 *  +0x3f8 / +0x3fc / +0x400 the fov spring {value, target, speed}, +0x404 the height spring {value, target, speed}. */
#include "loop_cam.h"
#include "loop_cam_entity.h"
#include "loop_cam_player.h"
#include "loop_cam_misc.h"
#include "loop_util.h"
#include "crt.h"
#include "phys_mech.h"

/* 00853180 Noise1D (cdecl t -> xmm0) (rx_phys) */
static float cam_noise(float t, rx_addr ret) {
    const uint32_t a[1] = {rx_fb(t)};
    const rx_addr s = loop_push(1, a, ret); const float r = phys_noise_1d(t); loop_pop(s); return r;
}

/* 00548620 (LTCG: EAX = player index) -> xmm0: 1.0 when the player's analog key map 7 (the input object of 0x9c52c4[index] +
 * 0x748, its vf4) reads nonzero, else 0. */
float rx_player_analog7_active(uint32_t index) {
    const rx_addr km = U32(0, 0x009c52c4 + 4u * index) + 0x748u;
    const float v = loop_tc2_x87(loop_vslot(km, 0x10), km, 7, index);
    return v != 0.0f ? 1.0f /* 0x931f24 */ : 0.0f;
}
RX_FUNCTION(0x00548620, rx_player_analog7_active)

/* The camera tweak key maps of the user camera (00649ed0 vf30): each reads the pressed state (KeyMap vf2, offset 8, arguments
 * 7 and the controller index) of the per-controller input object's blocks and returns +1.0 / -1.0 / 0.0. The original keeps the
 * controller index in ESI across the two queries. */
static uint32_t cam_keymap_pressed(rx_addr km, uint32_t index) {
    return loop_tc2(loop_vslot(km, 8), km, 7, index);
}
/* 005486c0 (ESI = controller index) -> xmm0: 1.0 the H control right (+0x3f8c), -1.0 the H control left (+0x3d8c), else 0. */
float rx_camera_hcontrol(uint32_t index) {
    const rx_addr in = U32(0, 0x009c52c4u + 4u * index);
    if (cam_keymap_pressed(in + 0x3f8cu, index) != 0) return 1.0f /* 0x931f24 */;
    if (cam_keymap_pressed(in + 0x3d8cu, index) != 0) return -1.0f /* 0x9cd9e8 */;
    return 0.0f;
}
RX_FUNCTION(0x005486c0, rx_camera_hcontrol)
/* 00548710 (ESI = controller index) -> xmm0: 1.0 the V control up (+0x398c), -1.0 the V control down (+0x3b8c), else 0. */
float rx_camera_vcontrol(uint32_t index) {
    const rx_addr in = U32(0, 0x009c52c4u + 4u * index);
    if (cam_keymap_pressed(in + 0x398cu, index) != 0) return 1.0f;
    if (cam_keymap_pressed(in + 0x3b8cu, index) != 0) return -1.0f;
    return 0.0f;
}
RX_FUNCTION(0x00548710, rx_camera_vcontrol)
/* 00548760 (ESI = controller index) -> xmm0: while CameraDolly (+0x33a0) is pressed, 1.0 the dolly forward (+0x358c), -1.0
 * the dolly back (+0x378c); otherwise (and with neither) 0. */
float rx_camera_dolly(uint32_t index) {
    const rx_addr in = U32(0, 0x009c52c4u + 4u * index);
    if (cam_keymap_pressed(in + 0x33a0u, index) != 0) {
        if (cam_keymap_pressed(in + 0x358cu, index) != 0) return 1.0f;
        if (cam_keymap_pressed(in + 0x378cu, index) != 0) return -1.0f;
    }
    return 0.0f;
}
RX_FUNCTION(0x00548760, rx_camera_dolly)

/* 006481a0 MXStartGateCamera::vf19 (dt; ret 4): the player camera's update (MXPlayerCamera::vf19), then while the pose is valid:
 * the hold timer +0x350 runs down; the camera leaves the gate (+0x398 = 0) once the vehicle is 10 units from it or the timer
 * ran out, or the player gives analog input (00548620 of the controller's player +0x1f0). The camera looks from where it is at
 * the point +0x360 shaken by Noise1D (amplitude 2, the three axes at clock, clock + 1, clock + 2; the clock runs at 3 x dt).
 * Frame: push ebp; and esp, -16; sub esp, 0x3c; push esi: E = the aligned base - 0x40. */
void rx_start_gate_camera_vf19(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x40u;
    rx_set_sp(E);
    { const uint32_t a[1] = {cam_x87_bits(rx_fb(dt))};
      const rx_addr s = loop_push(1, a, 0x006481b8); rx_player_camera_vf19(cam, rx_bf(a[0])); loop_pop(s); }
    if (U32(cam, 0x2f4) == 0) { rx_set_sp(sp0); return; }
    SF32(cam, 0x350, F32(cam, 0x350) - dt);
    const rx_addr L = E + 0x20u, P = E + 0x30u, T = E + 0x10u;
    { const rx_addr ent = U32(U32(cam, 0x344), 0x1ac); loop_tc1(loop_vslot(ent, 0x44), ent, L); }
    const rx_addr m = loop_tc0(loop_vslot(cam, 0x38), cam);
    const float mx = F32(m, 0x30), my = F32(m, 0x34), mz = F32(m, 0x38);
    SF32(P, 0, mx); SF32(P, 4, my); SF32(P, 8, mz);
    const float dx = F32(L, 0) - mx, dy = F32(L, 4) - my, dz = F32(L, 8) - mz;
    const float dist = rx_sqrts(((dz * dz) + (dy * dy)) + (dx * dx));
    if (dist >= 10.0f /* 0x92fd88 */ || 0.0f >= F32(cam, 0x350)) SU32(cam, 0x398, 0);
    const rx_addr C = U32(cam, 0x344);
    if (I32(C, 0x1f0) > -1) {
        const uint32_t a[1] = {0};
        const rx_addr s = loop_push(0, a, 0x0064829e);
        const float v = rx_player_analog7_active(U32(C, 0x1f0));
        loop_pop(s);
        if (v != 0.0f) SU32(cam, 0x398, 0);
    }
    SU32(T, 0, U32(cam, 0x360));
    SU32(T, 4, U32(cam, 0x364));
    SU32(T, 8, U32(cam, 0x368));
    rx_set_sp(E - 4u);
    const float clock = cam_x87_f32(cam + 0x354);
    SF32(T, 0, (cam_noise(clock, 0x006482ee) * 2.0f /* 0x92e330 */) + F32(T, 0));
    SF32(T, 4, (cam_noise(F32(cam, 0x354) + 1.0f /* 0x931f24 */, 0x00648320) * 2.0f) + F32(T, 4));
    SF32(T, 8, (cam_noise(F32(cam, 0x354) + 2.0f, 0x00648346) * 2.0f) + F32(T, 8));
    rx_set_sp(E);
    SF32(cam, 0x354, (dt * 3.0f /* 0x92fe48 */) + F32(cam, 0x354));
    { const uint32_t a[1] = {cam};
      const rx_addr s = loop_push(1, a, 0x00648384); rx_cam_entity_look_at(P, T, cam); loop_pop(s); }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006481a0, rx_start_gate_camera_vf19)

/* 00648990 -> EAX: the replay camera controller of the active mode (the player table of the game's mode object: 0xa946b8 +0x5a54
 * when the engine's scene +0x20 runs, else +0x20c8 when the scene +0x1c runs, else none: then the read of address 0x21de0). */
rx_addr rx_replay_camera_controller(void) {
    const rx_addr ctx = U32(0, 0x00a95844);
    rx_addr owner = 0;
    if (U32(U32(ctx, 0x20), 0x34) == 1) owner = U32(U32(0, 0x00a946b8), 0x5a54);
    else if (U32(U32(ctx, 0x1c), 0x34) == 1) owner = U32(U32(0, 0x00a946b8), 0x20c8);
    return U32(owner, 0x21de0);
}
RX_FUNCTION(0x00648990, rx_replay_camera_controller)

/* 006489e0 -> EAX = 0xa95990: the replay focus point (static): the controller's vf17 position, raised by 0x9c4a7c (3.0); zero
 * without a controller. */
rx_addr rx_replay_focus_point(void) {
    const rx_addr c = rx_replay_camera_controller();
    if (c != 0) {
        loop_tc1(loop_vslot(c, 0x44), c, 0x00a95990);
        SF32(0, 0x00a95994, F32(0, 0x00a95994) + F32(0, 0x009c4a7c));
    } else {
        SF32(0, 0x00a95990, 0.0f);
        SF32(0, 0x00a95998, 0.0f);
        SF32(0, 0x00a95994, 0.0f);
    }
    return 0x00a95990;
}
RX_FUNCTION(0x006489e0, rx_replay_focus_point)

/* 00648aa0 MXReplayCamera::vf19 (dt; ret 4): with the pose valid: the focus point is refreshed; while drifting (+0x370) the
 * retarget clock +0x318 runs, and past its period +0x324 a random direction (three rand() normalized) scaled by the distance to
 * the focus times +0x328 (kept at +0x31c) becomes the drift target +0x350 two times in three (rand() % 3 != 0), else zero; the
 * drift offset (+0x340) chases its target (vector spring). Frame: push ebp; and esp, -16; sub esp, 0x24; push ebx / esi / edi:
 * E = the aligned base - 0x30 (the camera's position at +0x20, the direction at +0x10). */
void rx_replay_camera_vf19(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x30u;
    rx_set_sp(E);
    if (U32(cam, 0x2f4) == 0) { rx_set_sp(sp0); return; }
    loop_tc1(loop_vslot(cam, 0x44), cam, E + 0x20u);
    rx_replay_focus_point();
    if (U8(cam, 0x370) == 0) { rx_set_sp(sp0); return; }
    const float clock = F32(cam, 0x318) + dt;
    SF32(cam, 0x318, clock);
    if (clock > F32(cam, 0x324)) {
        const int32_t r1 = crt_rand();
        const int32_t r2 = crt_rand();
        SF32(E, 0x10, (float)r1);
        SF32(E, 0x14, (float)r2);
        const int32_t r3 = crt_rand();
        SF32(E, 0x18, (float)r3);
        const float x = F32(E, 0x10), y = F32(E, 0x14), z = F32(E, 0x18);
        const float l = ((z * z) + (y * y)) + (x * x);
        SF32(E, 0xc, l);
        if (l > 0.0f /* 0x92e12c */) {
            const float inv = 1.0f /* 0x931f24 */ / rx_sqrts(l);
            SF32(E, 0x10, F32(E, 0x10) * inv);
            SF32(E, 0x14, F32(E, 0x14) * inv);
            SF32(E, 0x18, F32(E, 0x18) * inv);
        }
        const rx_addr p = rx_replay_focus_point();
        const float dz = F32(p, 8) - F32(cam, 0x68), dy = F32(p, 4) - F32(cam, 0x64), dx = F32(p, 0) - F32(cam, 0x60);
        const float s = rx_sqrts(((dz * dz) + (dy * dy)) + (dx * dx)) * F32(cam, 0x328);
        SF32(E, 0x10, s * F32(E, 0x10));
        SF32(E, 0x14, F32(E, 0x14) * s);
        SF32(cam, 0x31c, s);
        SF32(E, 0x18, F32(E, 0x18) * s);
        const int32_t r4 = crt_rand();
        if (r4 % 3 != 0) {
            SU32(cam, 0x350, U32(E, 0x10));
            SU32(cam, 0x354, U32(E, 0x14));
            SU32(cam, 0x358, U32(E, 0x18));
        } else {
            SF32(cam, 0x350, 0.0f);
            SF32(cam, 0x354, 0.0f);
            SF32(cam, 0x358, 0.0f);
        }
    }
    {
        const uint32_t a[4] = {cam_x87_bits(rx_fb(dt)), cam_x87_bits(U32(cam, 0x320)), cam_x87_bits(U32(cam, 0x32c)),
                               cam_x87_bits(U32(cam, 0x330))};
        const rx_addr s = loop_push(4, a, 0x00648cb2);
        rx_cam_vec3_spring(cam + 0x340, rx_bf(a[0]), rx_bf(a[1]), rx_bf(a[2]), rx_bf(a[3]));
        loop_pop(s);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00648aa0, rx_replay_camera_vf19)

/* 00649160 MXReplayTowerCamera::vf19 (dt; ret 4): with the pose valid: the replay camera's update; the fov target is the fov of
 * the first range (+0x3cc / +0x3d0 / +0x3d4, squared against the squared distance from the camera to the controller's point
 * +0xc0) that contains the point (unchanged beyond the last); the fov +0x3f8 chases it (acceleration +0x3d8, speed at most
 * min(10, 2 |d|), snapping when it passes the target) and goes to the Camera (+0xe0, matrix flags cleared). Unaimed (+0x3f4 == 0)
 * the camera keeps its fixed pose +0x380 (0063dee0); aimed it looks from its position at the controller's point with the height
 * spring's value (+0x404, chasing point.y + 0x9c4a7c: spring(100, 999999, 10)) plus the drift offset +0x340 when drifting.
 * Frame: push ebp; and esp, -16; sub esp, 0x48; push esi / edi: E = the aligned base - 0x50 (point +0x20, position +0x30, camera
 * matrix position +0x40). */
void rx_replay_tower_camera_vf19(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x50u;
    rx_set_sp(E);
    if (U32(cam, 0x2f4) == 0) { rx_set_sp(sp0); return; }
    { const uint32_t a[1] = {cam_x87_bits(rx_fb(dt))};
      const rx_addr s = loop_push(1, a, 0x00649186); rx_replay_camera_vf19(cam, rx_bf(a[0])); loop_pop(s); }
    const rx_addr T = E + 0x20u, L = E + 0x30u;
    {
        const rx_addr c = rx_replay_camera_controller() + 0xc0u;
        SU32(T, 0, U32(c, 0)); SU32(T, 4, U32(c, 4)); SU32(T, 8, U32(c, 8));
    }
    loop_tc1(loop_vslot(cam, 0x44), cam, L);
    {
        const float dy = F32(T, 4) - F32(L, 4), dx = F32(T, 0) - F32(L, 0), dz = F32(T, 8) - F32(L, 8);
        const float d2 = ((dz * dz) + (dy * dy)) + (dx * dx);
        const float r0 = F32(cam, 0x3cc), r1 = F32(cam, 0x3d0), r2 = F32(cam, 0x3d4);
        if (r0 * r0 > d2) cam_x87_copy(cam + 0x3fc, cam + 0x3c0);
        else if (r1 * r1 > d2) cam_x87_copy(cam + 0x3fc, cam + 0x3c4);
        else if (r2 * r2 > d2) cam_x87_copy(cam + 0x3fc, cam + 0x3c8);
    }
    {
        const float target = F32(cam, 0x3fc);
        SF32(E, 0x1c, target);
        const float d = target - F32(cam, 0x3f8);
        const float sign = (0.0f > d) ? F32(0, 0x009cd9e8) : 1.0f /* 0x931f24 */;
        const float v = ((F32(cam, 0x3d8) * dt) * sign) + F32(cam, 0x400);
        SF32(E, 0x14, sign);
        SF32(E, 0x18, v);
        SF32(cam, 0x400, v);
        float lim = fabsf(d) * 2.0f /* 0x92e330 */;
        if (lim > 10.0f /* 0x92fd88 */) lim = 10.0f;
        if (v * sign > lim) SF32(cam, 0x400, lim * sign);
        const float pos = (F32(cam, 0x400) * dt) + F32(cam, 0x3f8);
        SF32(cam, 0x3f8, pos);
        if (0.0f > (target - pos) * sign) {
            SF32(cam, 0x3f8, target);
            SF32(cam, 0x400, 0.0f);
        }
    }
    SU32(cam, 0x164, 0);
    SU32(cam, 0x16c, 0);
    SU32(cam, 0xe0, U32(cam, 0x3f8));
    SU32(cam, 0x168, 0);
    if (U32(cam, 0x3f4) == 0) {
        const uint32_t a[2] = {cam, cam + 0x380};
        const rx_addr s = loop_push(2, a, 0x00649465);
        rx_cam_entity_set_pose(cam, cam + 0x380);
        loop_pop(s);
        rx_set_sp(sp0);
        return;
    }
    if (rx_replay_camera_controller() == 0) { rx_set_sp(sp0); return; }
    const rx_addr m = loop_tc0(loop_vslot(cam, 0x38), cam);
    const rx_addr X = E + 0x40u;
    SU32(X, 0, U32(m, 0x30));
    SU32(X, 4, U32(m, 0x34));
    SU32(X, 8, U32(m, 0x38));
    {
        SF32(cam, 0x408, F32(0, 0x009c4a7c) + F32(T, 4));
        SU32(T, 4, U32(cam, 0x404));
        const uint32_t a[4] = {cam_x87_bits(rx_fb(dt)), cam_x87_bits(rx_fb(100.0f /* 0x93005c */)), rx_fb(999999.0f /* 0x977aa8 */),
                               rx_fb(10.0f /* 0x92fd88 */)};
        const rx_addr s = loop_push(4, a, 0x006493fd);
        rx_cam_spring(cam + 0x404, rx_bf(a[0]), rx_bf(a[1]), rx_bf(a[2]), rx_bf(a[3]));
        loop_pop(s);
    }
    if (U8(cam, 0x370) != 0) {
        SF32(T, 0, F32(cam, 0x340) + F32(T, 0));
        SF32(T, 4, F32(cam, 0x344) + F32(T, 4));
        SF32(T, 8, F32(cam, 0x348) + F32(T, 8));
    }
    {
        const uint32_t a[1] = {cam};
        const rx_addr s = loop_push(1, a, 0x00649450);
        rx_cam_entity_look_at(X, T, cam);
        loop_pop(s);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00649160, rx_replay_tower_camera_vf19)

/* ---- MXQuickCamera (the quick-look camera: fixed offsets in the vehicle's frame) ---- */

/* The camera's water clearance (00650c60 / 00647950, the same code with another rise): out = position; four points around it (the
 * box offset +0xb0 and extents +0xc0, shifted by +0xd0 * +0xe8) are tested against the volumes of the world (007c5aa0, which moves
 * a point inside to the surface, a 16-byte store); each moved point rises by `margin` + 1 (00650c60) or 2 (00647950) more, and
 * the largest rise goes onto out.y. Frame: push ebp; and esp, -16; sub esp, 0x94; push ebx / esi / edi: E = the aligned base -
 * 0xa0 (the points at +0x60 / +0x70 / +0x80 / +0x90). */
void rx_cam_lift_common(rx_addr cam, rx_addr pos, rx_addr out, int fixed, float margin, rx_addr ret_reg, rx_addr ret_vol) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0xa0u;
    rx_set_sp(E);
    if (!(U8(0, 0x009e30e0) & 1)) {
        SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
        SU32(0, 0x009e30dc, 0);
    }
    const rx_addr world = cam_registry_entry(0x009e30d4, ret_reg);
    const float oy = F32(cam, 0xb4), ox = F32(cam, 0xb0), oz = F32(cam, 0xb8);
    const float scale = F32(cam, 0xe8);
    const float ey = F32(cam, 0xc4), ez = F32(cam, 0xc8), ex = F32(cam, 0xc0);
    const float dx = F32(cam, 0xd0) * scale, dy = F32(cam, 0xd4) * scale, dz = F32(cam, 0xd8) * scale;
    SF32(E, 0x44, oy); SF32(E, 0x48, oz); SF32(E, 0x40, ox);
    cam_x87_copy(out, pos);
    cam_x87_copy(out + 4, pos + 4);
    SU32(E, 0x54, world);
    cam_x87_copy(out + 8, pos + 8);
    const float px = F32(pos, 0), py = F32(pos, 4), pz = F32(pos, 8);
    const float lx = px - ox, ly = py - oy, lz = pz - oz;
    SF32(E, 0x58, lx); SF32(E, 0x5c, ly); SF32(E, 0x34, lz);
    const float hx = px + ox, hy = py + oy, hz = pz + oz;
    SF32(E, 0xc, hx); SF32(E, 0x3c, hy); SF32(E, 0x38, hz);
    SF32(E, 0x60, (lx + ex) + dx); SF32(E, 0x64, (ly + ey) + dy); SF32(E, 0x68, (lz + ez) + dz);
    SF32(E, 0x70, (hx + ex) + dx); SF32(E, 0x74, (hy + ey) + dy); SF32(E, 0x78, (hz + ez) + dz);
    SF32(E, 0x80, (lx - ex) + dx); SF32(E, 0x84, (ly - ey) + dy); SF32(E, 0x88, (lz - ez) + dz);
    SF32(E, 0x90, (hx - ex) + dx); SF32(E, 0x94, (hy - ey) + dy); SF32(E, 0x98, (hz - ez) + dz);
    SF32(E, 0x14, dy);
    float rise = 0.0f;
    SF32(E, 0xc, rise);
    for (uint32_t i = 0; i < 4; i++) {
        const rx_addr p = E + 0x60u + 0x10u * i;
        SF32(E, 0x34, F32(p, 4));
        if (cam_volume_test(U32(E, 0x54), p, ret_vol) != 0) {
            const float after = fixed ? F32(p, 4) + 2.0f /* 0x92e330 */ : (margin + 1.0f /* 0x931f24 */) + F32(p, 4);
            SF32(p, 4, after);
            const float d = after - F32(E, 0x34);
            if (!(rise > d)) { rise = d; SF32(E, 0xc, rise); }
        }
    }
    SF32(out, 4, F32(out, 4) + rise);
    rx_set_sp(sp0);
}

/* 00647950 (LTCG: ECX = camera, EAX = position vec3; stack out vec3; ret 4): the clearance with the fixed rise 2. */
void rx_cam_lift_above_water2(rx_addr cam, rx_addr pos, rx_addr out) {
    rx_cam_lift_common(cam, pos, out, 1, 0.0f, 0x0064798a, 0x00647c9f);
}
RX_FUNCTION(0x00647950, rx_cam_lift_above_water2)

/* 00646ff0 MXQuickCamera::vf19 (dt; ret 4): the player camera's update; the clearance offset (vector spring +0x380: accel 2000,
 * speed 999999, k 4) runs. The vehicle's frame: forward F (mode +0x378 == 2: its forward flattened to y = 0 and normalized), up U
 * = the world up (.data 0x9cb4b0), right R = U x F; when |R| < 0.001 the vehicle's own up (+0x140) replaces U (mode 2: the raw
 * forward replaces F), R normalized. The camera looks from L + U * (+0x354 + 2) - R * +0x350 + F * +0x358 (lifted out of the
 * water (00647950), the lift the clearance spring's target, its value added) at L - R * +0x360 + U * +0x364 + F * +0x368 (L = the
 * vehicle's position). Frame: push ebp; and esp, -16; sub esp, 0x64; push ebx / esi / edi: E = the aligned base - 0x70 (R +0x10,
 * F +0x20, U +0x30, L +0x40, scratch +0x50 / +0x60). */
void rx_quick_camera_vf19(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x70u;
    rx_set_sp(E);
    { const uint32_t a[1] = {cam_x87_bits(rx_fb(dt))};
      const rx_addr s = loop_push(1, a, 0x0064700a); rx_player_camera_vf19(cam, rx_bf(a[0])); loop_pop(s); }
    const rx_addr spring = cam + 0x380u;
    {
        const uint32_t a[4] = {cam_x87_bits(rx_fb(dt)), rx_fb(2000.0f /* 0x977e10 */), rx_fb(999999.0f /* 0x977aa8 */),
                               rx_fb(4.0f /* 0x9322c0 */)};
        const rx_addr s = loop_push(4, a, 0x0064703c);
        rx_cam_vec3_spring(spring, rx_bf(a[0]), rx_bf(a[1]), rx_bf(a[2]), rx_bf(a[3]));
        loop_pop(s);
    }
    const rx_addr R = E + 0x10u, F = E + 0x20u, U = E + 0x30u, L = E + 0x40u;
    { const rx_addr ent = U32(U32(cam, 0x344), 0x1ac); loop_tc1(loop_vslot(ent, 0x44), ent, L); }
    const rx_addr ent = U32(U32(cam, 0x344), 0x1ac);
    float fx, fy, fz;
    if (U32(cam, 0x378) != 2) {
        fx = F32(ent, 0x130); fz = F32(ent, 0x138); fy = F32(ent, 0x134);
    } else {
        fx = F32(ent, 0x130); fz = F32(ent, 0x138); fy = 0.0f;
        const float l = ((fx * fx) + (fy * fy)) + (fz * fz);
        SF32(F, 0, fx); SF32(F, 8, fz); SF32(F, 4, fy); SF32(E, 0xc, l);
        if (l > 0.0f) {
            const float inv = 1.0f /* 0x931f24 */ / rx_sqrts(l);
            fx = inv * fx;
            fy = inv * 0.0f /* 0x92e12c */;
            fz = fz * inv;
        }
    }
    float ux = F32(0, 0x009cb4b0), uy = F32(0, 0x009cb4b4), uz = F32(0, 0x009cb4b8);
    SF32(U, 8, uz); SF32(U, 4, uy); SF32(U, 0, ux);
    SF32(F, 0, fx); SF32(F, 8, fz); SF32(F, 4, fy);
    float r1 = (uz * fx) - (fz * ux);
    float r0 = (uy * fz) - (uz * fy);
    float r2 = (ux * fy) - (uy * fx);
    SF32(R, 4, r1); SF32(R, 0, r0); SF32(R, 8, r2);
    const float rl = rx_sqrts(((r1 * r1) + (r0 * r0)) + (r2 * r2));
    if (0.001 /* 0x977e08 */ > (double)rl) {
        if (U32(cam, 0x378) != 2) {      /* the vehicle's up replaces U */
            const float vy = F32(ent, 0x144), vz = F32(ent, 0x148), vx = F32(ent, 0x140);
            SF32(U, 0, vx); SF32(U, 8, vz); SF32(U, 4, vy);
            r0 = (vy * fz) - (vz * fy);
            r1 = (vz * fx) - (fz * vx);
            r2 = (vx * fy) - (vy * fx);
        } else {                         /* the raw forward replaces F */
            fy = F32(ent, 0x134); fz = F32(ent, 0x138); fx = F32(ent, 0x130);
            r0 = (uy * fz) - (uz * fy);
            r1 = (uz * fx) - (fz * ux);
            r2 = (ux * fy) - (uy * fx);
            SF32(F, 4, fy); SF32(F, 0, fx); SF32(F, 8, fz);
        }
        SF32(R, 0, r0); SF32(R, 4, r1); SF32(R, 8, r2);
    }
    {
        const float l = ((r1 * r1) + (r0 * r0)) + (r2 * r2);
        SF32(E, 0xc, l);
        if (l > 0.0f /* 0x92e12c */) {
            const float inv = 1.0f /* 0x931f24 */ / rx_sqrts(l);
            r0 = (U32(cam, 0x378) != 2) ? F32(R, 0) * inv : inv * F32(R, 0);
            r2 = F32(R, 8) * inv;
            SF32(R, 8, r2);
            r1 = F32(R, 4) * inv;
        } else {
            r2 = F32(R, 8);
        }
    }
    fz = F32(F, 8); uy = F32(U, 4);
    {
        const float k8 = F32(cam, 0x368);
        SF32(E, 0x60, k8 * F32(F, 0)); SF32(E, 0x64, k8 * F32(F, 4)); SF32(E, 0x68, fz * k8);
        const float k4 = F32(cam, 0x364);
        SF32(E, 0x50, k4 * F32(U, 0)); SF32(E, 0x54, uy * k4); SF32(E, 0x58, F32(U, 8) * k4);
        const float nz = -0.0f /* 0x9774a8 */ - r2, nx = -0.0f - r0, ny = -0.0f - r1;
        SF32(E, 0xc, nz);
        const float k0 = F32(cam, 0x360);
        const float px = ((F32(L, 0) + (nx * k0)) + F32(E, 0x50)) + F32(E, 0x60);
        const float py = ((F32(L, 4) + (ny * k0)) + F32(E, 0x54)) + F32(E, 0x64);
        const float pz = ((F32(L, 8) + (nz * k0)) + F32(E, 0x58)) + F32(E, 0x68);
        SF32(E, 0x10, px); SF32(E, 0x14, py); SF32(E, 0x18, pz);
        const float h = F32(cam, 0x358);
        const float hz = fz * h, hx = h * F32(F, 0), hy = h * F32(F, 4);
        const float up = F32(cam, 0x354) + 2.0f /* 0x92e330 */;
        SF32(E, 0x64, uy * up);
        SF32(E, 0x68, F32(U, 8) * up);
        const float upx = up * F32(U, 0);
        const float s = F32(cam, 0x350);
        const float tx = (((nx * s) + F32(L, 0)) + upx) + hx;
        const float ty = (((ny * s) + F32(L, 4)) + F32(E, 0x64)) + hy;
        const float tz = (((nz * s) + F32(L, 8)) + F32(E, 0x68)) + hz;
        SF32(E, 0x30, tx); SF32(E, 0x34, ty); SF32(E, 0x38, tz);
        SF32(E, 0x20, tx); SF32(E, 0x24, ty); SF32(E, 0x28, tz);
    }
    {
        const uint32_t a[1] = {E + 0x60u};
        const rx_addr s = loop_push(1, a, 0x00647672);
        rx_cam_lift_above_water2(cam, E + 0x20u, E + 0x60u);
        loop_pop(s);
    }
    SF32(E, 0x50, F32(E, 0x60) - F32(E, 0x30));
    SF32(E, 0x54, F32(E, 0x64) - F32(E, 0x34));
    SF32(E, 0x58, F32(E, 0x68) - F32(E, 0x38));
    rx_cam_copy_vec3_10(spring, E + 0x50u);
    SF32(E, 0x20, F32(spring, 0) + F32(E, 0x30));
    SF32(E, 0x24, F32(spring, 4) + F32(E, 0x34));
    SF32(E, 0x28, F32(spring, 8) + F32(E, 0x38));
    {
        const uint32_t a[1] = {cam};
        const rx_addr s = loop_push(1, a, 0x006476f3);
        rx_cam_entity_look_at(E + 0x20u, E + 0x10u, cam);
        loop_pop(s);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00646ff0, rx_quick_camera_vf19)

/* 00548f20 / 00548f70 (LTCG: EAX = player index -> EAX): the player's steer / lean key map of the current control scheme (the
 * input object 0x9c52c4[index]: scheme +0x5130, alternate +0x512c); trivial leaves of rx_world, inlined. */
static rx_addr player_keymap_x(uint32_t index) {
    const rx_addr b = U32(0, 0x009c52c4 + 4u * index);
    const uint32_t m = U32(b, 0x5130), alt = U32(b, 0x512c) == 1;
    if (m == 1) return b + (alt ? 0x17d8u : 0x1f20u);
    if (m == 2) return b + (alt ? 0x19c4u : 0x17d8u);
    return b + (alt ? 0x19c4u : 0x1f20u);
}
static rx_addr player_keymap_y(uint32_t index) {
    const rx_addr b = U32(0, 0x009c52c4 + 4u * index);
    const uint32_t m = U32(b, 0x5130), alt = U32(b, 0x512c) == 1;
    if (m == 1) return b + (alt ? 0x1734u : 0x21d0u);
    if (m == 2) return b + (alt ? 0x1bc4u : 0x1734u);
    return b + (alt ? 0x1bc4u : 0x21d0u);
}

/* 005487d0 (LTCG: ESI = player index; cdecl out float[2]): the player's stick: the x / y key maps' analog value (vf4 (7,
 * index), x87) into out[0] / out[1]. */
void rx_player_stick7(uint32_t index, rx_addr out) {
    const rx_addr kx = player_keymap_x(index);
    const rx_addr ky = player_keymap_y(index);
    SF32(out, 0, loop_tc2_x87(loop_vslot(kx, 0x10), kx, 7, index));
    SF32(out, 4, loop_tc2_x87(loop_vslot(ky, 0x10), ky, 7, index));
}
RX_FUNCTION(0x005487d0, rx_player_stick7)
