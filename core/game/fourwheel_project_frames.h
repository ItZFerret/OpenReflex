#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004cdb70 UTV/Truck,108B: ECX self, no stack arguments, RET0.
 * No original frame/saves/spills/callees. Retains entered ESP and actual
 * conditional EAX/EDX outputs; the enclosing caller models RET0. */
void rx_utv_project_body_positions_exact(rx_addr self);
/* Distinct004b3370 Buggy108 leaf is owned by the existing
 * dispatch/game/buggy_body_projection.c, linked once alongside this variant. */
#ifdef __cplusplus
}
#endif
