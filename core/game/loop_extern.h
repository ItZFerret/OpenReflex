/* loop_extern.h -- the loop code's calls into the other subsystems' core functions. Their headers give the prototypes; the
 * function is taken from the registry by its original address, typed by the owner's prototype (no link dependency, so a build
 * with only some subsystems still links): NULL when not ported / not built, and the caller runs the original instead.
 *   __typeof__(&rx_world_physics_update) f = LOOP_CORE(0x00769b80, rx_world_physics_update); if (f) f(...); else original */
#pragma once
#include "world.h"
#include "world_rider.h"
#include "anim.h"
#include "phys_math.h"
#include "world_veh.h"
#include "world_node.h"
#include "anim_rider_a.h"
#include "crt.h"

#ifdef RX_FN_TRACE
#define LOOP_CORE(addr, fn) RX_FN(addr, __typeof__(&fn))   /* -DRX_FN_TRACE: recorded with the function's name (tools/vtrace_check.py checks it is the port registered at addr) */
#else
#define LOOP_CORE(addr, fn) ((__typeof__(&fn))rx_fn(addr))
#endif
