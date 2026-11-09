/* mechanism.c -- RavenPhysics articulated-body mechanism (the per-node passes of the vehicle simulation step).
 * Mechanism M (= simulation + 0xd0): +0x00 gravity, +0x1c node count (signed); node i (stride 0xa0) has its linear body at
 * +0x20 and its angular body at +0x660; within a body: pose +0x00, velocity +0x10, mass / inertia +0x20, inverse +0x30,
 * resolved force +0x40, applied force +0x50, impulse +0x60, acceleration +0x70. (native/game/notes/mechanism.md) */
#include "phys_mechanism.h"

/* 0085d270 Mechanism_IntegrateVelocities: per node, linear accel = resolved force * inverse mass, angular accel = resolved
 * torque * inverse inertia; velocity += accel * dt (linear: accel * dt + v, angular: dt * accel + w, in the original's order). */
void rx_mechanism_integrate_velocities(rx_addr M, float dt) {
    for (int32_t i = 0; i < I32(M, 0x1c); i++) {
        rx_addr lin = M + 0x20 + 0xa0u * (uint32_t)i, ang = lin + 0x640;
        rx_v4 a = rx_v4_mul(rx_ld4(lin + 0x40), rx_ld4(lin + 0x30));
        rx_st4(lin + 0x70, a);
        rx_v4 b = rx_v4_mul(rx_ld4(ang + 0x40), rx_ld4(ang + 0x30));
        rx_st4(ang + 0x70, b);
        rx_st4(lin + 0x10, rx_v4_add(rx_v4_mul(rx_ld4(lin + 0x70), rx_v4_splat(dt)), rx_ld4(lin + 0x10)));
        rx_st4(ang + 0x10, rx_v4_add(rx_v4_mul(rx_v4_splat(dt), rx_ld4(ang + 0x70)), rx_ld4(ang + 0x10)));
    }
}
RX_FUNCTION(0x0085d270, rx_mechanism_integrate_velocities)
