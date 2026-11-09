#pragma once
#include "rx_game.h"
/* ECX=self, RET0; declared10005 (EAX,EDX,x87). No child calls. */
uint32_t rx_fourwheel_activity_predicate(rx_addr self); /* UTV/Truck0045cc10 */
uint32_t rx_buggy_activity_predicate(rx_addr self); /* Buggy004bff90 */
/* ECX=self, RET0, EAX=[self+2e9c]. */
uint32_t rx_fourwheel_contact_mode(rx_addr self); /* shared0045ceb0 */
