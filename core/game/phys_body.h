/* phys_body.h -- the physics world's generic dynamic objects as rider / bike bodies (phys_body.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr phys_body_record(rx_addr handle);                   /* 0042bd90 (EAX = handle) */
void    phys_body_update_grid(rx_addr vec, int32_t index);  /* 0042b000 (ESI = vector header, ECX = index) */
void    phys_rig_bodies_from_bones(rx_addr rig);            /* 00429910 (ECX = rig) */
#ifdef __cplusplus
}
#endif
