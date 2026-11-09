#pragma once
#include "rx_game.h"
/*0085d750 ECX=collisionworld, cdecl output; EAX count, caller removes4bytes.*/
int32_t rx_waypoint_avoid_collect(rx_addr collision_world,rx_addr output);
/*00847730 EAX=shape, stacklength RET4; full temporary dynamic cylinder ctor.*/
rx_addr rx_avoid_cylinder_construct(rx_addr shape,float length);

float rx_avoid_atan2f(float y,float x); /*0042c210 cdecl2float, x87 float-rounded result*/
