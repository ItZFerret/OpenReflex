#pragma once
#include "phys_pair2.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Complete source proposal; private-store/NaN/read-order self-audit exists,
 * independent review and original proof pending. No main activation.
 * Original ECX ends, EDX first vertices; six cdecl array pointers; RET0.
 * Recorded mask5 EAX|EDX; supplemental all GPR/frame proof required. */
uint32_t phys_rays6_triangles_observed(rx_addr ends,rx_addr v0,rx_addr v1,rx_addr v2,rx_addr starts,rx_addr hits,rx_addr points,rx_addr normals);
#ifdef __cplusplus
}
#endif
