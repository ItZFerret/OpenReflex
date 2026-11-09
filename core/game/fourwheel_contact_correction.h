#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004cf310/004b4210: EDX simulation, RET0; recorded10002 ECX/x87. */
void rx_utv_truck_contact_correction(rx_addr simulation);
void rx_buggy_contact_correction(rx_addr simulation);
/* Specific real caller roles; frame/RA are already written at rx_sp.
 * No generic original dispatcher or substitute semantic result. */
rx_addr rx_fourwheel_correction_wheel(rx_addr wheel,rx_addr simulation,rx_addr output,rx_addr return_pc);
void rx_fourwheel_correction_body_probe(rx_addr simulation,rx_addr return_pc);
void rx_fourwheel_correction_cookie(uint32_t cookie,rx_addr return_pc);
#ifdef __cplusplus
}
#endif
