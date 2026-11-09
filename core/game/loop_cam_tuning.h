/* loop_cam_tuning.h -- the camera tuning file's preset parser (0065c200) and its camera-name classifier (0065c070); miscport. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
int32_t  rx_cam_tuning_preset_type(rx_addr name);                               /* 0065c070 (ESI = the preset's camera name) -> EAX 0 .. 6 */
uint32_t rx_cam_tuning_parse(rx_addr table, rx_addr reader);                    /* 0065c200 (stdcall table, XML reader; RET 8) -> EAX 1 / 0; ECX = [0x9b45e4] */
#ifdef __cplusplus
}
#endif
