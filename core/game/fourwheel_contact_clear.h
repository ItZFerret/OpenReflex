#pragma once
#include "rx_game.h"
/* ECX simulation, RET0; incidental EAX=0, preserved EDX/x87 (full10005).
 * The common UTV/truck vtable target is truly shared, not a layout guess. */
uint32_t rx_fourwheel_clear_contact_words(rx_addr simulation); /*004c0340*/
uint32_t rx_buggy_clear_contact_words(rx_addr simulation); /*004a24f0*/
