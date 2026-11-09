#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Actual vf29/30: ECX self, RET0, full recorded10005. */
uint32_t rx_utv_truck_rest_all_contacts(rx_addr simulation); /*004ce8e0*/
uint32_t rx_buggy_rest_all_contacts(rx_addr simulation);     /*004b40e0*/
uint32_t rx_utv_truck_rest_all_markers(rx_addr simulation);  /*004ce910*/
uint32_t rx_buggy_rest_all_markers(rx_addr simulation);      /*004b4110*/
/* Actual vf31: ECX self, stack output*, RET4, returns output in EAX;
 * EDX/x87 genuinely unchanged. Full10005 remains the adapter contract. */
rx_addr rx_utv_truck_rest_normal(rx_addr simulation,rx_addr output); /*004ce980*/
rx_addr rx_buggy_rest_normal(rx_addr simulation,rx_addr output);     /*004b4180*/
/* vf30's real dynamic vf29 call, not a hardcoded boolean. Entered RA already
 * written; actual target=EDX, ECX=self, expected=entry+4. */
void rx_fourwheel_rest_contact_dependency(unsigned family,rx_addr expected_sp);
void* rx_fourwheel_rest_bind(void* machine);
#ifdef __cplusplus
}
#endif
