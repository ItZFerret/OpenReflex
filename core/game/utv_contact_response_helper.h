#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004cea10 complete2289B, EDI simulation, RET0, recorded10005.
 * Typed body preserves entered SP. No native address registration. */
void rx_utv_contact_response_helper_exact(rx_addr simulation);
/* Real438ee0 entry RA already written; ESI=actual curve, XMM0=input.
 * Scoped four static sites only; actual RET0 must reach expected SP. */
float rx_utv_contact_response_curve(float input,rx_addr expected_sp);
void* rx_utv_contact_response_helper_bind(void* machine);
#ifdef __cplusplus
}
#endif
