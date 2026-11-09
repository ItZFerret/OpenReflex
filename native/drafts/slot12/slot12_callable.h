#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Private, unregistered record ABI. Input39104/output39096 bytes.
 * S9700,control100,three filter vec3 records,148 bytes of sorted runtime
 * spans; input tail adds uint32 dt_bits/profile. All pointers are normalized
 * to Sim10000000,Control20000000,Filters30000000; aliases retain byte order.
 * 0success,1argument/size error,2unsupported domain. All failures are atomic.
 * Nearest/gradual-underflow host required. Sticky FP flags are outside scope. */
int reflex_bike_slot12_draft(const void*,size_t,void*,size_t);
#ifdef __cplusplus
}
#endif
