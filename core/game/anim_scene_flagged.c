/* anim_scene_flagged.c -- 007d2ab0: the scene's entities with flag 4 in +0x74 collected into the global list {data 0x00a95d74, count 0x00a95d78, capacity
 * 0x00a95d7c, growth 0x00a95d80}, and 0040a9e0 (the current scene's). */
#include "anim.h"
#include "anim_extern.h"
#include "build_rider_util.h"
#include "build_alloc.h"
#include "anim_scene_flagged.h"

static void list_add(rx_addr e) {
    if (I32(0, 0x00a95d7c) <= I32(0, 0x00a95d78)) {
        const uint32_t cap = U32(0, 0x00a95d80) + U32(0, 0x00a95d7c);
        const rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(cap, 4));
        crt_memcpy(nb, U32(0, 0x00a95d74), U32(0, 0x00a95d7c) * 4u);
        rx_reflex_aligned_free(U32(0, 0x00a95d74));
        SU32(0, 0x00a95d7c, cap);
        SU32(0, 0x00a95d74, nb);
    }
    SU32(U32(0, 0x00a95d74), 4u * U32(0, 0x00a95d78), e);
    SU32(0, 0x00a95d78, U32(0, 0x00a95d78) + 1);
}

/* 007d2ab0 (stdcall scene; RET 4): under the scene's lock the list's count is reset, then the entities of the checked vector +0x2c48 (pointers; the
 * debug iterator's bound check) and of the list +0x2c28 / +0x2c2c whose +0x74 has bit 2 are appended */
void rx_scene_collect_flag4(rx_addr s) {
    rr_os_cs_enter(s + 4);
    SU32(0, 0x00a95d78, 0);
    const int32_t n = (int32_t)(U32(s, 0x2c4c) - U32(s, 0x2c48)) >> 2;
    for (int32_t i = 0; i < n; i++) {
        if ((uint32_t)i >= ((U32(s, 0x2c4c) - U32(s, 0x2c48)) >> 2)) crt_invalid_parameter_noinfo();
        const rx_addr e = U32(U32(s, 0x2c48), 4u * (uint32_t)i);
        if (U8(e, 0x74) & 4) list_add(e);
    }
    const int32_t m = I32(s, 0x2c2c);
    for (int32_t i = 0; i < m; i++) {
        const rx_addr e = U32(U32(s, 0x2c28), 4u * (uint32_t)i);
        if (U8(e, 0x74) & 4) list_add(e);
    }
    rr_os_cs_leave(s + 4);
}
RX_FUNCTION(0x007d2ab0, rx_scene_collect_flag4)

/* 0040a9e0: the scene of the engine's current scene index ([0x00a95844] + 0x1c + 4 * [+4]) */
void rx_current_scene_collect_flag4(void) {
    const rx_addr ctx = U32(0, 0x00a95844);
    ANIM_CALL_FRAME(4);
    rx_scene_collect_flag4(U32(ctx, 0x1c + 4 * U32(ctx, 4)));
    ANIM_CALL_FRAME_END(4);
}
RX_FUNCTION(0x0040a9e0, rx_current_scene_collect_flag4)
