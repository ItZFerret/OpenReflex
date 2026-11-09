/* phys_cylinder.h -- closest points on a collision cylinder (phys_cylinder.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t phys_cylinder_closest_point(rx_addr cylinder, rx_addr point);                   /* 00846c70 (EDI, ESI); returns ECX */
uint32_t phys_cylinder_closest_point_normal(rx_addr cylinder, rx_addr point, rx_addr normal);   /* 00846f10 (ESI, EDI; stack; RET 4) */
#ifdef __cplusplus
}
#endif
