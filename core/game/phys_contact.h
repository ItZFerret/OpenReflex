/* phys_contact.h -- the narrow phase (phys_contact.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 0085ddd0 Collision_ContactDispatch (thiscall M; A, qA*, pA*, B, qB*, pB*, flag; RET 0x1c) */
void phys_contact_dispatch(rx_addr M, rx_addr A, rx_addr QA, rx_addr PA, rx_addr B, rx_addr QB, rx_addr PB, uint32_t flag);
/* A pair role of the observed dispatcher not modelled by the finite closures (collision_triangle_ray.c): the generator `target`
 * entered at rx_sp() (return address `return_pc`, `nargs` stack arguments, the dispatcher's registers in the fx model); runs the
 * core port and models the RET (ESP = expected). Aborts when (target, return_pc, nargs) is not a case of 0085ddd0. */
void phys_contact_pair_observed(rx_addr target, rx_addr return_pc, unsigned nargs, rx_addr expected);
#ifdef __cplusplus
}
#endif
