#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Original EAX=self; stack dt; RET4. Typed API retains entered rx_sp(). */
void rx_rider_pose_override(rx_addr rider,float dt);
/* Original EAX=self; RET0. All pose/dirty/listener branches are implemented. */
uint32_t rx_rider_override_select_pose(rx_addr rider);
/* Finite external semantic owner63b830. Entered SP retained by typed callee,
 * provider checks actual frame and models its RET8. */
void rx_rider_override_listeners(rx_addr list,rx_addr value,rx_addr return_site);
#ifdef __cplusplus
}
#endif
