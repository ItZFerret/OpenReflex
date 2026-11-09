#pragma once
#include "rx_game.h"
/* Complete CPU Camera constructor/factory, reusing build_mxworld's existing
 *007909f0 stores. Typed kernel interface; no new public/session ABI. */
rx_addr rx_view_camera_construct(rx_addr camera);
rx_addr rx_view_camera_create(void);
/* Existing managed-GPU product domain only: original794a80 CPU ownership and
 * borrowed-view guard. Unsupported native GPU owners reject explicitly. The
 * caller owns/removes the viewport record itself, as original007d3140 does. */
void rx_viewport_cpu_release(rx_addr view);
