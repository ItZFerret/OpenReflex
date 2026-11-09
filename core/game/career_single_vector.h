#pragma once
#include "build_activity_util.h"
/*Four-byte identity vectors have no element destructors. Their proxy layout is
 * shared with criterion vectors, but their destruction must NOT walk0xb0 cells.*/
static inline void career_id_vector_destroy(rx_addr v) {
    const rx_addr data=U32(v,0xc),proxy=U32(v,0);
    if(data)rx_reflex_aligned_free(data);
    SU32(v,0xc,0);SU32(v,0x10,0);SU32(v,0x14,0);
    rx_reflex_aligned_free(proxy);
}
