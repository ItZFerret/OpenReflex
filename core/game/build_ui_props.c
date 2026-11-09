/* build_ui_props.c -- PRODUCT BOUNDARY (core/plan/QUEUE.md "chain"): the UI manager's property store (UIProperty_GetInt 005dc0c0 / GetString 005dc060: the front end's
 * settings the gameplay code reads back). The product has no UI: the properties gameplay reads come from the host's event spec (rx_event_spec); a property the spec
 * does not carry is absent, as in the original before a page set it (the call returns 0 and leaves the output alone: "WaitForUI", "InCareerMode", ...). Product
 * only (the oracle runs the originals). */
#include "build_activity_util.h"
#include "race_misc.h"
#include <string.h>

#include "career_properties.h"
#ifndef RX_ORACLE
uint32_t rx_ui_prop_get_int(rx_addr key,rx_addr out){return rx_owned_property_get_int(key,out,1);}
uint32_t rx_ui_prop_get_string(rx_addr key,rx_addr out,uint32_t size){return rx_owned_property_get_string(key,out,size);}
#endif
