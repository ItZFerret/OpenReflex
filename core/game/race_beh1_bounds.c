/* race_beh1_bounds.c -- BehaviorAvoidTrackBoundaries (vtable 0x93fc98): the AI's repulsion from the track edges.
 * Each frame the behaviour adds to the driver's steering offset (+0x60) a push away from the edges of the current segment (the
 * driver's previous -> next node, edge points +0x10 / +0x30 on one side, +0x20 / +0x40 on the other): for the nearest point of an
 * edge, a repulsion strength from the distance d to it, f(d) = A / d^a - B / d^b (per-skill tuning 0xa04d18: B b A a, a Lennard-Jones
 * like curve: strong near the edge, attraction farther away), applied only while the rider heads toward the edge (heading . edge
 * normal < 0), scaled by 0.5 - that, along the edge -> rider direction and taken into the vehicle frame (its x = the lateral push).
 * It never takes control. Behaviour: +0x04 enabled, +0x08 driver, +0x0c type id 0x1b, +0x10 suspended. */
#include "race_beh1_extern.h"

/* 00521b40 / 00521ee0 (EAX = behaviour, args: edge start a, edge end b; RET 8) -> xmm0: the lateral push of one edge (vehicle frame
 * x; 0 when the rider is not heading toward it). The two differ in the edge normal: (ez, -ex) for 00521b40, (-ez, ex) for 00521ee0. */
static float b1_boundary_push(rx_addr b, rx_addr ea, rx_addr eb, int mirrored) {
    rx_addr veh = b1_vehicle(b1_driver(b));
    const float px = F32(veh, 0xc0), pz = F32(veh, 0xc8);
    float hx = F32(veh, 0x130), hz = F32(veh, 0x138);
    float len2 = hz * hz + hx * hx;
    if (len2 != 0.0f) {
        float inv = 1.0f / sqrtf(len2);
        hx = inv * hx;
        hz = hz * inv;
    }
    veh = b1_vehicle(b1_driver(b));
    const float az = F32(ea, 8), ax = F32(ea, 0);
    float ex = F32(eb, 0) - ax, ez = F32(eb, 8) - az;
    float t = ((F32(veh, 0xc8) - az) * ez + (F32(veh, 0xc0) - ax) * ex) / (ez * ez + ex * ex);
    if (1.0f > t && 0.0f > t) t = 0.0f;
    else if (!(1.0f > t)) t = 1.0f;
    float dx = px - (ex * t + ax);
    float dz = pz - (ez * t + az);
    float d2 = dz * dz + dx * dx;
    float nx = px, nz = pz;                     /* the original's frame keeps the position there when d2 == 0 */
    if (d2 != 0.0f) {
        float inv = 1.0f / sqrtf(d2);
        nx = dx * inv;
        nz = dz * inv;
    }
    float dist = sqrtf(d2);
    rx_addr row = 0x00a04d18u + 16u * b1_tuning_index(b1_driver(b));
    float f = F32(row, 8) / (float)race_pow((double)dist, (double)F32(row, 0xc));
    f = f - F32(row, 0) / (float)race_pow((double)dist, (double)F32(row, 4));
    float h;
    if (!mirrored) {
        float qx = ez, qz = -0.0f - ex;
        float q2 = qz * qz + qx * qx;
        if (q2 != 0.0f) {
            float inv = 1.0f / sqrtf(q2);
            qx = inv * ez;
            qz = qz * inv;
        }
        h = qz * hz + qx * hx;
    } else {
        float qx = -0.0f - ez, qz = ex;
        float q2 = qx * qx + qz * qz;
        if (q2 != 0.0f) {
            float inv = 1.0f / sqrtf(q2);
            qx = inv * qx;
            qz = inv * ex;
        }
        h = qz * hz + qx * hx;
    }
    if (!(0.0f > h)) return 0.0f;
    float g = -0.0f - (h - 0.5f /* 0x9322cc */) * f;
    RX_FRAME_ENTER(F, 0x10);
    SF32(F, 0, nx * g);
    SF32(F, 4, 0.0f);
    SF32(F, 8, nz * g);
    rx_race_to_vehicle_frame(b1_vehicle(b1_driver(b)), F);
    float r = F32(F, 0);
    RX_FRAME_LEAVE(F, 0x10);
    return r;
}

float rx_behavior_boundary_push(rx_addr b, rx_addr ea, rx_addr eb) { return b1_boundary_push(b, ea, eb, 0); }
RX_FUNCTION(0x00521b40, rx_behavior_boundary_push)
float rx_behavior_boundary_push_mirrored(rx_addr b, rx_addr ea, rx_addr eb) { return b1_boundary_push(b, ea, eb, 1); }
RX_FUNCTION(0x00521ee0, rx_behavior_boundary_push_mirrored)

/* 00522280 BehaviorAvoidTrackBoundaries::vf4 (ECX = behaviour, stack dt; RET 4) -> 0. While on the track: the push of the +0x10
 * edge (when it pushes negative, the +0x30 edge's instead, clamped to >= 0) and of the +0x20 edge (00521ee0; when positive, the
 * +0x40 edge's through 00521b40 instead, clamped to <= 0) are added to the driver's steering offset +0x60. */
uint32_t rx_behavior_avoid_boundaries_update(rx_addr b, float dt) {
    (void)dt;
    if (U32(b, 4) == 0 || U32(b, 0x10) != 0) return 0;
    rx_addr r1 = rx_driver_rule_1(b1_driver(b));
    rx_addr rb = rx_driver_offtrack_penalty_rule(b1_driver(b));
    if (r1 != 0) { if (U32(r1, 0x50) == 0) return 0; }
    else if (rb != 0 && U32(rb, 0x10) == 0) return 0;
    rx_addr drv = b1_driver(b);
    float a = rx_behavior_boundary_push(b, U32(drv, 0x18) + 0x10, U32(drv, 0x1c) + 0x10);
    if (0.0f > a) {
        drv = b1_driver(b);
        a = rx_behavior_boundary_push(b, U32(drv, 0x18) + 0x30, U32(drv, 0x1c) + 0x30);
        if (!(a > 0.0f)) a = 0.0f;
    }
    drv = b1_driver(b);
    SF32(drv, 0x60, F32(drv, 0x60) + a);
    drv = b1_driver(b);
    float c = rx_behavior_boundary_push_mirrored(b, U32(drv, 0x18) + 0x20, U32(drv, 0x1c) + 0x20);
    if (c > 0.0f) {
        drv = b1_driver(b);
        c = rx_behavior_boundary_push(b, U32(drv, 0x18) + 0x40, U32(drv, 0x1c) + 0x40);
        if (!(0.0f > c)) c = 0.0f;
    }
    drv = b1_driver(b);
    SF32(drv, 0x60, F32(drv, 0x60) + c);
    return 0;
}
RX_FUNCTION(0x00522280, rx_behavior_avoid_boundaries_update)
