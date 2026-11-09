#pragma once
#include "phys_vec.h"
/* 00864ec0: ECX sphere, EDI position; stack manifold, terrain shape, flag. */
void phys_pair_sphere_hfterrain(rx_addr sphere, rx_addr position, rx_addr manifold, rx_addr terrain_shape, uint32_t flag);
