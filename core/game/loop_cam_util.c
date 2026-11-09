/* loop_cam_util.c -- smoothing and math helpers shared by the camera classes: angle wrapping, the three "spring" followers
 * (a value chasing a target with a limited acceleration and speed), the rotation of a camera matrix, tiny field accessors.
 *
 * A spring (angle / scalar) is {+0 position, +4 target, +8 velocity}; the vector spring is {+0 position xyz, +0x10 target xyz,
 * +0x20 velocity xyz}. The camera code calls them with the spring in ESI (LTCG). */
#include "loop_cam.h"

/* 0063c6a0 (stack: angle; result xmm0): the angle brought into [-pi, pi). In range it is returned as it is; below zero it is
 * first lifted by whole turns, then reduced by round((x + pi) / 2pi) turns. The roundings are x87 sums at the thread's
 * precision followed by _ftol2_sse (copysign(0.5, v) + v, truncated), v = (double)(t) - 0.49999999999 (0x977558). The lifted angle
 * of a negative input is written back into the argument slot (rx_sp() + 4: the caller pushed it). */
float rx_cam_wrap_angle(float x) {
    if (!(x < -3.1415927f /* 0x977530 */) && 3.1415927f /* 0x977498 */ > x) return x;
    if (0.0f > x) {
        double v = (double)(x / -6.2831855f /* 0x97767c */) - 0.49999999999 /* 0x977558 */;
        int32_t n = cam_round_half_away(v);
        x = ((float)n + 1.0f) * 6.2831855f /* 0x97749c */ + x;
        SF32(rx_sp(), 4, x);          /* the original keeps the lifted angle in its argument slot */
    }
    double v = (double)((x + 3.1415927f) / 6.2831855f) - 0.49999999999;
    int32_t n = cam_round_half_away(v);
    return x - (float)n * 6.2831855f;
}
RX_FUNCTION(0x0063c6a0, rx_cam_wrap_angle)

/* 0063c7c0 (LTCG: ESI = spring; stack dt, accel, max_speed, k; ret 0x10): an angle chasing its target along the shortest way.
 * d = wrap(target - pos); the speed grows by sign(d) * dt * accel, is limited to min(max_speed, |d| * k) and the angle moves by
 * speed * dt; it snaps to the target (speed 0) when the step would overshoot it. */
void rx_cam_angle_spring(rx_addr s, float dt, float accel, float max_speed, float k) {
    float d = rx_cam_wrap_angle(F32(s, 4) - F32(s, 0));
    float sign = (0.0f > d) ? -1.0f /* 0x9cd9e8 */ : 1.0f /* 0x931f24 */;
    float absd = fabsf(d);
    SF32(s, 8, ((sign * dt) * accel) + F32(s, 8));
    float limit = (max_speed > absd * k) ? absd * k : max_speed;
    if (F32(s, 8) * sign > limit) SF32(s, 8, sign * limit);
    if ((F32(s, 8) * sign) * dt > absd) {
        SF32(s, 0, F32(s, 4));
        SF32(s, 8, 0.0f);
    } else {
        SF32(s, 0, (F32(s, 8) * dt) + F32(s, 0));
    }
    SF32(s, 0, rx_cam_wrap_angle(F32(s, 0)));
}
RX_FUNCTION(0x0063c7c0, rx_cam_angle_spring)

/* 0063c8c0 (LTCG: ESI = spring; stack dt, accel, max_speed, k; ret 0x10): the same chase for a plain scalar (no wrapping). */
void rx_cam_spring(rx_addr s, float dt, float accel, float max_speed, float k) {
    float target = F32(s, 4);
    float d = target - F32(s, 0);
    float sign = (0.0f > d) ? -1.0f : 1.0f;
    float v = ((dt * accel) * sign) + F32(s, 8);
    SF32(s, 8, v);
    float limit = fabsf(d) * k;
    if (limit > max_speed) limit = max_speed;
    if (v * sign > limit) SF32(s, 8, limit * sign);
    float pos = (F32(s, 8) * dt) + F32(s, 0);
    SF32(s, 0, pos);
    if (0.0f > (target - pos) * sign) {
        SF32(s, 0, target);
        SF32(s, 8, 0.0f);
    }
}
RX_FUNCTION(0x0063c8c0, rx_cam_spring)

/* 0063c9b0 (LTCG: ESI = vector spring; stack dt, accel, max_speed, k; ret 0x10): the same chase for a point: acceleration along
 * the direction to the target, speed limited to min(max_speed, distance * k), snapping to the target when the step overshoots. */
void rx_cam_vec3_spring(rx_addr s, float dt, float accel, float max_speed, float k) {
    float dx = F32(s, 0x10) - F32(s, 0);
    float dy = F32(s, 0x14) - F32(s, 4);
    float dz = F32(s, 0x18) - F32(s, 8);
    float dist = rx_sqrts(((dy * dy) + (dx * dx)) + (dz * dz));
    if (!(dist > 0.0f)) return;
    float step = dt * accel;
    float vx = ((dx / dist) * step) + F32(s, 0x20);
    SF32(s, 0x24, F32(s, 0x24) + ((dy / dist) * step));
    float vz = F32(s, 0x28) + ((dz / dist) * step);
    SF32(s, 0x20, vx);
    SF32(s, 0x28, vz);
    float vy = F32(s, 0x24);
    float speed = rx_sqrts(((vx * vx) + (vy * vy)) + (vz * vz));
    float limit = dist * k;
    if (limit > max_speed) limit = max_speed;
    if (speed > limit) {
        SF32(s, 0x20, F32(s, 0x20) / speed);
        SF32(s, 0x24, F32(s, 0x24) / speed);
        SF32(s, 0x28, F32(s, 0x28) / speed);
        SF32(s, 0x20, F32(s, 0x20) * limit);
        SF32(s, 0x24, F32(s, 0x24) * limit);
        SF32(s, 0x28, F32(s, 0x28) * limit);
        speed = limit;
    }
    if (speed * dt > dist) {
        SF32(s, 0, F32(s, 0x10));
        SF32(s, 4, F32(s, 0x14));
        SF32(s, 8, F32(s, 0x18));
        SF32(s, 0x20, 0.0f);
        SF32(s, 0x24, 0.0f);
        SF32(s, 0x28, 0.0f);
        return;
    }
    float px = F32(s, 0) + (F32(s, 0x20) * dt);
    SF32(s, 4, F32(s, 4) + (F32(s, 0x24) * dt));
    float pz = F32(s, 8) + (F32(s, 0x28) * dt);
    SF32(s, 0, px);
    SF32(s, 8, pz);
}
RX_FUNCTION(0x0063c9b0, rx_cam_vec3_spring)

/* 0063cf00 + 0063cbb0 (stack: matrix, angle; plain ret, the caller pops): m = R(angle) * m for the rotation part of a 4x4 matrix at `m`
 * (translation +0x30..+0x38 kept). sin / cos are the game's range-reduced polynomials (cam_sin / cam_cos). The original reuses the
 * angle's stack slot as a temporary: the first polynomial term is left there (the result here, so that the adapter can store it). */
float rx_cam_rotate_matrix(rx_addr m, float angle) {
    float s = cam_sin(angle);
    float y = cam_trig_reduce(fabsf(angle));
    float c = -0.0f - cam_trig_poly(y);
    float scratch = (y * y) * -2.197296e-07f /* 0x935d50 */ + 2.420294e-05f /* 0x935d48 */;   /* left in the angle's stack slot (0063d00b) */
    float tx = F32(m, 0x30), ty = F32(m, 0x34), tz = F32(m, 0x38);
    float f1 = s * 0.0f;
    float f2 = 1.0f - c;
    float f3 = f2 * 0.0f;
    float v14 = f2 * 0.0f;
    float f4 = f3 - f1;
    float f5 = f2 * 0.0f;
    float v28 = f5 - f1;
    float f6 = f5 * 0.0f;
    float v30 = f6 + s;
    float v2c = f6 + c;
    float v38 = v14 + f1;
    float f7 = v14 * 0.0f;
    float v3c = f7 - s;
    float v40 = f7 + c;
    float o[16];
    o[0] = ((F32(m, 0x10) * (f3 + f1)) + (F32(m, 0x00) * (f2 + c))) + (F32(m, 0x20) * f4);
    o[1] = ((F32(m, 0x14) * (f3 + f1)) + (F32(m, 0x04) * (f2 + c))) + (F32(m, 0x24) * f4);
    o[2] = ((F32(m, 0x18) * (f3 + f1)) + (F32(m, 0x08) * (f2 + c))) + (F32(m, 0x28) * f4);
    o[3] = 0.0f;
    o[4] = ((F32(m, 0x20) * v30) + (F32(m, 0x00) * v28)) + (F32(m, 0x10) * v2c);
    o[5] = ((F32(m, 0x24) * v30) + (F32(m, 0x14) * v2c)) + (F32(m, 0x04) * v28);
    o[6] = ((F32(m, 0x28) * v30) + (F32(m, 0x18) * v2c)) + (F32(m, 0x08) * v28);
    o[7] = 0.0f;
    o[8] = ((F32(m, 0x00) * v38) + (F32(m, 0x20) * v40)) + (F32(m, 0x10) * v3c);
    o[9] = ((F32(m, 0x24) * v40) + (F32(m, 0x04) * v38)) + (F32(m, 0x14) * v3c);
    o[10] = ((F32(m, 0x28) * v40) + (F32(m, 0x08) * v38)) + (F32(m, 0x18) * v3c);
    o[11] = 0.0f;
    o[12] = (((F32(m, 0x00) + F32(m, 0x20)) + F32(m, 0x10)) * 0.0f) + F32(m, 0x30);
    o[13] = (((F32(m, 0x24) + F32(m, 0x04)) + F32(m, 0x14)) * 0.0f) + F32(m, 0x34);
    o[14] = (((F32(m, 0x28) + F32(m, 0x08)) + F32(m, 0x18)) * 0.0f) + F32(m, 0x38);
    o[15] = 1.0f;
    for (int i = 0; i < 16; i++) SF32(m, 4 * i, o[i]);
    SF32(m, 0x30, tx);
    SF32(m, 0x34, ty);
    SF32(m, 0x38, tz);
    return scratch;
}
RX_FUNCTION(0x0063cf00, rx_cam_rotate_matrix)

/* 0063c7b0 (EAX = spring): the target. */
float rx_cam_spring_target(rx_addr s) { return F32(s, 4); }
RX_FUNCTION(0x0063c7b0, rx_cam_spring_target)

/* 0063c8a0 (EAX = spring): jump to the target and stop. */
void rx_cam_spring_snap(rx_addr s) {
    SU32(s, 0, U32(s, 4));
    SF32(s, 8, 0.0f);
}
RX_FUNCTION(0x0063c8a0, rx_cam_spring_snap)

/* 0063c8b0 (EAX = spring): the velocity. */
float rx_cam_spring_speed(rx_addr s) { return F32(s, 8); }
RX_FUNCTION(0x0063c8b0, rx_cam_spring_speed)

/* 0063d920 (EAX = object, xmm0 = value): object +0x254 = value. */
void rx_cam_set_float_254(rx_addr obj, float v) { SF32(obj, 0x254, v); }
RX_FUNCTION(0x0063d920, rx_cam_set_float_254)

/* 0063c990 (EAX = source vec3, ECX = destination object): destination +0x10..+0x18 = the three floats. */
void rx_cam_copy_vec3_10(rx_addr dst, rx_addr src) {
    SU32(dst, 0x10, U32(src, 0));
    SU32(dst, 0x14, U32(src, 4));
    SU32(dst, 0x18, U32(src, 8));
}
RX_FUNCTION(0x0063c990, rx_cam_copy_vec3_10)
