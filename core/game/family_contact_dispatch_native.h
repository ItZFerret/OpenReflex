#pragma once
#include "fourwheel_contact_dispatch.h"
/* Link exactly one finite provider; no registration/activation. Explicit
 * family0 UTV/1 Buggy/2 Truck. Genuine caller already wrote all arguments/RA.
 * EnteredSP retained by typed owners; this provider models only exact RET. */
#ifdef __cplusplus
extern "C" {
#endif
#ifdef RX_FAMILY_VF13_COMPARE
void rx_family_vf13_counts_reset(void);
unsigned rx_family_vf13_count(unsigned role);
#endif

#ifdef __cplusplus
}
#endif
