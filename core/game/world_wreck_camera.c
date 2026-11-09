/* world_wreck_camera.c -- MXThirdPersonCamera's reaction to bike impacts (0x00649bc0, 0x006467f0, 0x00651000-0x00653300): a
 * side kick from the impact direction, shake and kick vectors scaled by tuned curves (00438ee0) per camera mode and rider class.
 *
 * Camera: +0xb0 / +0xc0 the forward / side axes, +0x2d8 the mode name, +0x344 -> +0x1ac the rider (+0x170 / +0x194 class,
 * +0x280 down), +0x392 the last impact pointed down, +0x5f8 / +0x608 / +0x618 the fallback / shake / kick enables, +0x60c..+0x614
 * the shake band and duration, +0x61c..+0x624 the kick band and duration, +0x630 / +0x640 / +0x650 shake vectors, +0x660 / +0x670
 * kick vectors, +0x6a0 armed, +0x6a4 duration, +0x6a8 t, +0x6ac / +0x6b0 shake phases, +0x6b4 side kick, +0x6c0 / +0x6d0 / +0x6e0
 * the shake, +0x700 kick (1) or shake (0), +0x708 kick strength, +0x710 / +0x720 the kick. */
#include "phys_misc.h"
#include "bike_curve.h"
#include "world_wreck.h"
#include "crt_util.h"
#include "crt.h"
#include "world_util.h"

/* 00649bc0 MXThirdPersonCamera::vf22: the type id 0x1003. */
uint32_t rx_world_third_person_camera_vf22(rx_addr cam) { (void)cam; return 0x1003; }
RX_FUNCTION(0x00649bc0, rx_world_third_person_camera_vf22)

/* 006467f0 Camera_ModeIndex (EAX = camera; RET 0): the index 0..5 of the camera's mode name (+0x2d8) among six names (the string
 * compare of the IAT slot 0x91610c), else 0. */
uint32_t rx_world_camera_mode_index(rx_addr cam) {
    static const uint32_t names[6] = {0x00958980, 0x009589b8, 0x00941b28, 0x00958a60, 0x00958a6c, 0x00958a78};
    world_frame f = world_frame_flat(3, 0);
    const rx_addr compare = U32(0, 0x0091610c);   /* std::operator==(const string&, const char*) */
    uint32_t r = 0;
    for (uint32_t i = 0; i < 6; i++) {
        uint32_t eq;
        CRT_WITH_REGS(0, 0, cam + 0x2d8, compare, eq = crt_str_eq_cstr(cam + 0x2d8, names[i]));
        if (eq & 0xff) { r = i; break; }
    }
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x006467f0, rx_world_camera_mode_index)

/* The side kick of an impact v (x, y, z at v): its components along the camera's forward (+0xb0) and side (+0xc0) axes decide
 * +0x6b4: +-pi/2 (0x977508 / 0x977520) or 0. */
static void side_kick(rx_addr cam, rx_addr v) {
    const float vx = F32(v, 0), vy = F32(v, 4), vz = F32(v, 8);
    const float side = phys_add(phys_add(phys_mul(vy, F32(cam, 0xc4)), phys_mul(vx, F32(cam, 0xc0))), phys_mul(vz, F32(cam, 0xc8)));
    const float fwd = phys_add(phys_add(phys_mul(vy, F32(cam, 0xb4)), phys_mul(vx, F32(cam, 0xb0))), phys_mul(vz, F32(cam, 0xb8)));
    const float back = phys_sub(-0.0f /* 0x9774a8 */, fwd);
    SF32(cam, 0x6b4, rx_bf(0));
    if (fwd > side) SF32(cam, 0x6b4, back > fwd ? 1.57079637f /* 0x977508 */ : -1.57079637f /* 0x977520 */);
    else if (back > side) SF32(cam, 0x6b4, 1.57079637f);
}

/* A tuned curve of the impact magnitude (00438ee0: ESI = curve, xmm0 = x -> xmm0): table + rider class * 0x1da80 + (mode * 2 +
 * split) * 0xa0, the class 3 riders mapped by +0x170 (8 -> 3, else 8), split = the session type (0xa946b8 +0x15b24) is 2. */
static float impact_curve(rx_addr cam, rx_addr table, float x) {
    const rx_addr rider = U32(U32(cam, 0x344), 0x1ac);
    uint32_t cls = U32(rider, 0x194);
    if (cls == 3) cls = U32(rider, 0x170) != 8 ? 8u : 3u;
    const uint32_t session = U32(U32(0, 0x00a946b8), 0x15b24);
    const uint32_t split = session == 1 ? 0 : (session == 2 ? 1 : 0);
    uint32_t mode;
    WORLD_CALL_SP(0, mode = rx_world_camera_mode_index(cam));
    float y;
    WORLD_CALL_SP(0, y = bike_param_curve((split + mode * 2) * 0xa0 + cls * 0x1da80 + table, x));
    return y;
}
/* |v| as the original computes it: (x^2 + z^2) + y^2, sqrtps of the splat */
static rx_v4 length_splat(rx_v4 v) { return v4_sqrt(v4_splat(v4_len2_xzy(v))); }

/* 006510a0 Camera_ImpactShake (EDI = camera; stdcall impact vector; RET 4; AL = started): within the shake band (+0x60c ..
 * +0x610) of |v|: the side kick, two curves of |v| (0xb0bfc4 / 0xb0c884) scale the shake vectors (+0x630 / +0x640 by the first,
 * +0x650 by the second), started by Camera_StartShake for +0x614. */
int rx_world_camera_impact_shake(rx_addr cam, rx_addr v) {
    world_frame f = world_frame_enter(0x48, 2, 1);
    const rx_addr F = f.esp + 8, vcopy = F + 8, mag = F + 0x18, shake1 = F + 0x28;
    v4_st(mag, length_splat(v4_ld(v)));
    const float len = F32(mag, 0);
    int r = 0;
    if (!(F32(cam, 0x60c) > len) && !(len > F32(cam, 0x610))) {
        v4_st(vcopy, v4_ld(v));
        side_kick(cam, vcopy);
        const float c1 = impact_curve(cam, 0x00b0bfc4, F32(mag, 0));
        v4_st(vcopy, v4_splat(c1));
        const float c2 = impact_curve(cam, 0x00b0c884, F32(mag, 0));
        const rx_v4 s1 = v4_ld(vcopy);
        const uint32_t duration = world_x87_pass(U32(cam, 0x614));
        v4_st(shake1, v4_mul(v4_ld(cam + 0x640), s1));
        v4_st(vcopy, v4_mul(v4_ld(cam + 0x630), s1));
        v4_st(mag, v4_mul(v4_ld(cam + 0x650), v4_splat(c2)));
        SU8(cam, 0x700, 0);
        WORLD_CALL_SP(0xc, rx_world_camera_start_shake(cam, mag, rx_bf(duration), shake1, vcopy));
        r = 1;
    }
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x006510a0, rx_world_camera_impact_shake)

/* 00651320 Camera_ImpactKick (EDI = camera; stdcall impact vector; RET 4; AL = started): within the kick band (+0x61c .. +0x620)
 * of |v|: two curves of |v| (0xb0f0c4: the kick vectors' scale, 0xb0f984: the strength) arm Camera_StartKick for +0x624. */
int rx_world_camera_impact_kick(rx_addr cam, rx_addr v) {
    world_frame f = world_frame_enter(0x38, 2, 1);
    const rx_addr F = f.esp + 8, mag = F + 8, kick2 = F + 0x18;
    v4_st(mag, length_splat(v4_ld(v)));
    const float len = F32(mag, 0);
    int r = 0;
    if (!(F32(cam, 0x61c) > len) && !(len > F32(cam, 0x620))) {
        const float c1 = impact_curve(cam, 0x00b0f0c4, F32(mag, 0));
        SF32(F, 4, c1);
        const float c2 = impact_curve(cam, 0x00b0f984, F32(mag, 0));
        const uint32_t duration = world_x87_pass(U32(cam, 0x624));
        SF32(F, 0, c2);
        const uint32_t strength = world_x87_pass(U32(F, 0));
        const rx_v4 s = v4_splat(F32(F, 4));
        v4_st(kick2, v4_mul(v4_ld(cam + 0x670), s));
        v4_st(mag, v4_mul(v4_ld(cam + 0x660), s));
        WORLD_CALL_SP(0x10, rx_world_camera_start_kick(cam, mag, rx_bf(strength), rx_bf(duration), kick2));
        r = 1;
    }
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x00651320, rx_world_camera_impact_kick)

/* 00651940 Camera_StartKick (stdcall camera, kick vector, strength, duration; EDX = second kick vector; RET 0x10). */
void rx_world_camera_start_kick(rx_addr cam, rx_addr kick, float strength, float duration, rx_addr kick2) {
    SU8(cam, 0x700, 1); SU8(cam, 0x6a0, 1);
    v4_st(cam + 0x710, v4_ld(kick));
    v4_st(cam + 0x720, v4_ld(kick2));
    SF32(cam, 0x6a4, duration);
    SU32(cam, 0x6a8, 0);
    SF32(cam, 0x708, strength);
}
RX_FUNCTION(0x00651940, rx_world_camera_start_kick)

/* A shake phase: x folded into one period with its sign (sgn x mod 2 pi, t = pi - that), then
 * sgn * t * sin-poly(t^2) / (-0 - cos-poly(t^2)) (the polynomials 0x935d3c..0x935d68 in the original's order). */
static float shake_phase(float x, float sgn) {
    float a = phys_mul(sgn, x);
    const int32_t k = rx_cvtt(phys_mul(a, 0.159154937f /* 0x9774a0 */));
    a = phys_sub(a, phys_mul((float)k, 6.28318548f /* 0x97749c */));
    const float t = phys_sub(3.14159274f /* 0x977498 */, a);
    const float t2 = phys_mul(t, t), t4 = phys_mul(t2, t2);
    float cosp = phys_mul(phys_add(phys_mul(t2, -2.19729600e-07f /* 0x935d50 */), 2.42029400e-05f /* 0x935d48 */), t4);
    cosp = phys_add(cosp, phys_add(phys_mul(t2, -1.38587900e-03f /* 0x935d44 */), 4.16597798e-02f /* 0x935d4c */));
    cosp = phys_add(phys_mul(cosp, t4), phys_add(phys_mul(t2, -0.499994189f /* 0x935d40 */), 0.999999225f /* 0x935d3c */));
    const float denom = phys_sub(-0.0f /* 0x9774a8 */, cosp);
    float sinp = phys_mul(phys_add(phys_mul(t2, -2.03622399e-08f /* 0x935d68 */), 2.69971497e-06f /* 0x935d64 */), t4);
    sinp = phys_add(sinp, phys_add(phys_mul(t2, -1.98086302e-04f /* 0x935d60 */), 8.33240338e-03f /* 0x935d5c */));
    sinp = phys_add(phys_mul(sinp, t4), phys_add(phys_mul(t2, -0.166665494f /* 0x935d58 */), 0.999999583f /* 0x935d54 */));
    return phys_div(phys_mul(phys_mul(sinp, t), sgn), denom);
}

/* 006514e0 Camera_WreckKick (EDI = camera; stdcall impact vector; RET 4; AL = started): the fallback kick of a wreck impact
 * (+0x5f8) within the band [+0x5fc, +0x600] of |v| (the Camera_ImpactKick pattern, with its own tables and vectors): two curves of
 * |v| (0xb09404: the scale of the kick vectors +0x680 (kick) and +0x690 (second kick), 0xb09cc4: the strength) arm
 * Camera_StartKick for +0x604. The curve value and the duration pass through the x87 stack (fld / fstp: a signalling NaN comes
 * out quiet).
 * Frame: push ebp; and esp, -16; sub esp, 0x38; push ebx, esi. |v| (splat) at esp + 0x10 is overwritten by the kick vector, the
 * second kick sits at esp + 0x20; the call's four argument words are the camera, the kick, the strength and the duration. */
int rx_world_camera_wreck_kick(rx_addr cam, rx_addr v) {
    world_frame f = world_frame_enter(0x38, 2, 1);
    const rx_addr S = f.esp, mag = S + 0x10, kick2 = S + 0x20;
    v4_st(mag, length_splat(v4_ld(v)));
    const float len = F32(mag, 0);
    int r = 0;
    if (!(F32(cam, 0x5fc) > len) && !(len > F32(cam, 0x600))) {
        const float scale = impact_curve(cam, 0x00b09404, F32(mag, 0));
        const float strength_curve = impact_curve(cam, 0x00b09cc4, F32(mag, 0));
        const uint32_t duration = world_x87_pass(U32(cam, 0x604));
        const uint32_t strength = world_x87_pass(rx_fb(strength_curve));
        const rx_v4 s = v4_splat(scale);
        v4_st(kick2, v4_mul(v4_ld(cam + 0x690), s));
        v4_st(mag, v4_mul(v4_ld(cam + 0x680), s));
        SU32(S, -16, cam);
        SU32(S, -12, mag);
        SU32(S, -8, strength);
        SU32(S, -4, duration);
        WORLD_CALL_SP(0x10, rx_world_camera_start_kick(cam, mag, rx_bf(strength), rx_bf(duration), kick2));
        r = 1;
    }
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x006514e0, rx_world_camera_wreck_kick)

/* 006516a0 Camera_StartShake (stdcall camera, scale vector, duration; ECX / EDX = the two shake vectors; RET 0xc): arms the
 * shake (+0x6c0 / +0x6d0, +0x6e0 = 30 * scale) and its phases +0x6b0 / +0x6ac from the x / y of the sum of the two vectors. */
void rx_world_camera_start_shake(rx_addr cam, rx_addr scale, float duration, rx_addr shake1, rx_addr shake2) {
    SU8(cam, 0x700, 0); SU8(cam, 0x6a0, 1);
    v4_st(cam + 0x6c0, v4_ld(shake1));
    v4_st(cam + 0x6d0, v4_ld(shake2));
    SF32(cam, 0x6a4, duration);
    SU32(cam, 0x6a8, 0);
    v4_st(cam + 0x6e0, v4_mul(v4_splat(30.0000019f /* 0x977720 */), v4_ld(scale)));
    const rx_v4 sum = v4_add(v4_ld(cam + 0x6d0), v4_ld(cam + 0x6c0));
    const float m1 = F32(0, 0x009cd9e8);   /* -1 (a runtime global) */
    SF32(cam, 0x6b0, shake_phase(sum.v[0], !(sum.v[0] >= 0.0f) ? m1 : 1.0f));
    SF32(cam, 0x6ac, shake_phase(sum.v[1], !(sum.v[1] >= 0.0f) ? m1 : 1.0f));
}
RX_FUNCTION(0x006516a0, rx_world_camera_start_shake)

/* a ramp of the impact magnitude: 00424370 (xmm0 = 10, xmm1 = 70; stdcall x, lo, hi passed through the x87) */
static float camera_ramp(rx_addr cam, uint32_t x, uint32_t lo, uint32_t hi) {
    (void)cam;
    float y;
    WORLD_CALL_SP(12, y = phys_remap_clamped(10.0f /* 0x92fd88 */, 70.0f /* 0x931f2c */, rx_bf(x), rx_bf(world_x87_pass(lo)), rx_bf(world_x87_pass(hi))));
    return y;
}

/* 00653300 MXThirdPersonCamera::vf34 (thiscall camera; stdcall impact vector, duration; RET 8): the side kick and a shake from
 * three fixed ramps of |v| (00424370 with xmm0 = 10, xmm1 = 70: 0.15..0.6, 0.1 (a runtime global 0x9cf5a8 .. 0.1), 15..35). */
void rx_world_third_person_camera_vf34(rx_addr cam, rx_addr v, float duration) {
    world_frame f = world_frame_enter(0x4c, 1, 1);
    const rx_addr F = f.esp + 4, mag = F + 0xc, vcopy = F + 0x1c, shake1 = F + 0x2c;
    v4_st(mag, length_splat(v4_ld(v)));
    v4_st(vcopy, v4_ld(v));
    side_kick(cam, vcopy);
    const uint32_t len = world_x87_pass(U32(mag, 0));
    v4_st(shake1, v4_splat(camera_ramp(cam, len, rx_fb(0.15f) /* 0x9322c8 */, rx_fb(0.6f) /* 0x931da8 */)));
    v4_st(vcopy, v4_splat(camera_ramp(cam, len, U32(0, 0x009cf5a8), rx_fb(0.1f) /* 0x931d94 */)));
    const float s3 = camera_ramp(cam, len, rx_fb(15.0f) /* 0x9322d0 */, rx_fb(35.0f) /* 0x9774d4 */);
    v4_st(mag, v4_splat(s3));
    SU8(cam, 0x700, 0);
    WORLD_CALL_SP(0xc, rx_world_camera_start_shake(cam, mag, rx_bf(world_x87_pass(rx_fb(duration))), shake1, vcopy));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00653300, rx_world_third_person_camera_vf34)

/* 00651000 MXThirdPersonCamera::vf33 (thiscall camera; stdcall ?, shake vector, impact vector, no-kick flag; RET 0x10): unless the
 * rider is down: a shake (when enabled +0x608 and the last impact pointed down +0x392), else a kick (+0x618) or the fallback
 * 006514e0 (+0x5f8); then +0x392 = this impact points down (y < 0). */
void rx_world_third_person_camera_vf33(rx_addr cam, uint32_t unused, rx_addr shake, rx_addr impact, uint32_t no_kick) {
    (void)unused;
    const rx_addr rider = U32(U32(cam, 0x344), 0x1ac);
    if (U32(rider, 0x280) != 0) return;
    world_frame f = world_frame_enter(0x14, 3, 1);
    const rx_addr P = f.esp;
    v4_st(P + 0x10, v4_ld(impact));
    const uint8_t down = rx_bf(0) > F32(P, 0x14) ? 1 : 0;
    int done = 0;
    if (U8(cam, 0x608) != 0 && U8(cam, 0x392) != 0) WORLD_CALL_SP(4, done = rx_world_camera_impact_shake(cam, shake));
    if (!done && (no_kick & 0xff) == 0) {
        if (U8(cam, 0x618) != 0) WORLD_CALL_SP(4, done = rx_world_camera_impact_kick(cam, impact));
        if (!done && U8(cam, 0x5f8) != 0) WORLD_CALL_SP(4, rx_world_camera_wreck_kick(cam, impact));
    }
    SU8(cam, 0x392, down);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00651000, rx_world_third_person_camera_vf33)
