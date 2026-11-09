#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Complete original vf13 callers: ECX simulation, stack dt, RET4, full10005.
 * No registration: each private dependency remains a separate proof boundary. */
void rx_utv_contact_solve_dispatch(rx_addr simulation,float dt);
void rx_buggy_contact_solve_dispatch(rx_addr simulation,float dt);
void rx_truck_contact_solve_dispatch(rx_addr simulation,float dt);
enum rx_contact_dispatch_role {
 RX_CD_QUERY, RX_CD_COLLISION, RX_CD_PROBE_FRAME, RX_CD_BODY_PROBE,
 RX_CD_CORRECTION, RX_CD_FILTERED, RX_CD_PROJECT, RX_CD_COOKIE, RX_CD_BOUNDS
};
/* Scoped private roles, not a generic target dispatcher. Return address and
 * all real arguments already occupy guest memory at rx_sp(). Implementations
 * must validate the family/site and actual return ESP, preserving callee stack
 * writes. Bounds receives live XMM0; other roles do not consume that argument.
 * family:0 UTV,1 Buggy,2 Truck. expected is post-RET, before caller cleanup. */
rx_v4 rx_fourwheel_contact_dispatch_dependency(enum rx_contact_dispatch_role role,
 unsigned family,rx_addr expected,rx_v4 xmm0);
void* rx_fourwheel_contact_dispatch_bind(void* machine);
#ifdef __cplusplus
}
#endif
