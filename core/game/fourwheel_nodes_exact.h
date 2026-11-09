#pragma once
#include "rx_game.h"

/* These APIs retain entry ESP; their caller models the actual RET cleanup.
 * Narrow native dependency: genuine four-wheel SlidingRotor node objects,
 * separate from the live guest stack. No address registration is added. */
uint32_t phys_rotor_project_velocity_exact(rx_addr self);
uint32_t phys_mech_nodes_slot7_exact(rx_addr mechanism);
