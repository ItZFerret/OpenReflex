/* loop_cam_snap.h -- the camera cut-in snap 00649df0 (loop_cam_snap.c): the transition an MXThirdPersonCamera starts when the
 * active camera entity switches to it from a blendable one (the start-gate camera). Notes: core/notes/loop.md. */
#pragma once
#include "rx_game.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 00649df0 (LTCG: ESI = the new camera entity, EDI = the old camera entity; stack duration; ret 4) */
void rx_cam_snap(rx_addr cam, rx_addr old, float duration);

#ifdef __cplusplus
}
#endif
