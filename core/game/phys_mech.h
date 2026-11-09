/* phys_mech.h -- RavenPhysics articulated-body mechanism (phys_mech.c, phys_mech_matrix.c): matrix objects, per-node passes,
 * preparation / solve and small helpers. Notes: native/game/notes/mechanism.md. The C signatures mirror the originals' ABI
 * (this / first register input first, then the stack arguments); the address is in each comment. */
#pragma once
#include "rx_game.h"
#include "phys_mech_matrix.h"
#include "phys_mech_kernel.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Mechanism M (= simulation + 0xd0) passes */
void phys_mech_nodes_slot2(rx_addr M);                                            /* 0085c6d0 */
void phys_mech_nodes_slot3(rx_addr M);                                            /* 0085c700 */
uint32_t phys_mech_nodes_slot6(rx_addr M);                                        /* 0085d300 (EAX = last callee's) */
uint32_t phys_mech_nodes_slot7(rx_addr M);                                        /* 0085d330 */
void phys_mech_clear_forces(rx_addr M);                                           /* 0085c9d0 (EDX = M) */
void phys_mech_translate(rx_addr M, rx_addr vec);                                 /* 0085c500 (EDX = M, ESI = vec) */
void phys_mech_set_velocity(rx_addr M, rx_addr vec);                              /* 0085c4b0 (EDX = M, ESI = vec) */
uint32_t phys_mech_set_node_linear_velocity(rx_addr M, rx_addr vec, uint32_t index);   /* 0085c730 (ECX = M, EDX = vec) */
uint32_t phys_mech_set_node_angular_velocity(rx_addr M, rx_addr vec, uint32_t index);  /* 0085c750 (ECX = M, EDX = vec) */
void phys_mech_compose_forces(rx_addr M, float dt);                               /* 0085ca40 (EDI = M) */
void phys_mech_integrate_poses(rx_addr M, float dt);                              /* 0085d100 (ECX = M, XMM6 = dt) */
uint32_t phys_mech_add_force_at_point(rx_addr force, rx_addr M, rx_addr point, uint32_t index);  /* 0085c770 (ECX, EDX, ESI) */
uint32_t phys_mech_add_body_force(rx_addr M, uint32_t index, rx_addr force);         /* 0085c8b0 (ECX = M; stack index, force) */
void phys_mech_rotate(rx_addr M, rx_addr rot);                                    /* 0085c530 (EDX = M, ESI = rot) */
/* small helpers */
rx_addr phys_vec3_sub(rx_addr a, rx_addr b, rx_addr out);                        /* 00401700 (ECX = a, EDX = b, arg out) */
uint32_t phys_vec3_rotate_about_axis(rx_addr out, rx_addr v, rx_addr axis, float angle);   /* 00852f40 (EAX = out, ECX = v, XMM1 = angle) */
void phys_noise_init_tables(void);                                                 /* 00853220 */
float phys_noise_1d(float x);                                                     /* 00853180 (result XMM0) */
uint32_t phys_segment_plane_intersect(rx_addr p0, rx_addr p1, rx_addr q, rx_addr out, rx_addr n);  /* 008523b0 (EDI, EDX, ESI) */
uint32_t phys_contacts_average(rx_addr S, rx_addr out);                           /* 0085dcf0 (ESI = S) */
/* solver (notes: node object layout in phys_mech.c) */
void phys_mech_prepare(rx_addr M);                                                /* 0085cb70 */
void phys_mech_solve(rx_addr M);                                                  /* 0085cf80 */
void phys_mat6_mulvec_sub(rx_addr A, rx_addr x, rx_addr Y);                       /* 00857b30 (ECX = A, arg x, ESI = Y) */
void phys_mat_mulvec_sub(rx_addr A, rx_addr x, rx_addr Y);                        /* 00857590 (ECX = A, EDX = x, arg Y) */
void phys_mat_mulvec(rx_addr A, rx_addr x, rx_addr y);                            /* 00853900 (EAX = A, EDX = x, ESI = y) */
void phys_mat3_mul(rx_addr B, rx_addr A, rx_addr out);                            /* 00853740 (ECX = B, EDI = A, ESI = out) */
void phys_mat_congruence(rx_addr A, rx_addr B, rx_addr C, rx_addr D);             /* 00858fe0 (ECX = A, EDX = B, args C, D) */
void phys_mat_coupling(rx_addr A, rx_addr B, rx_addr C);                          /* 00855740 (EAX = A, ECX = B, EDI = C) */
#ifdef __cplusplus
}
#endif
