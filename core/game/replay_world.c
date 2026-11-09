#include "replay_restore.h"
#include "world_veh.h"
#include "world.h"
#include <string.h>

/* Both originals have the same aligned frame. Keep every handle re-read and
 * body lookup: a native helper may mutate the handle/physics object. */
static void replay_world_section(rx_addr entity, uint32_t state, int second) {
    ACT_FRAME_EBP(f, 16, 0x18, 2);
    const uint32_t pose_offset = second ? (state ? 0x1c0 : 0x1d0) : (state ? 0x1d0 : 0x1c0);
    rx_addr handle = U32(entity, 0x158), body;
    if (handle) {
        ACT_CALLR(body, 0, rx_world_physics_handle_body(handle));
        rx_st4(body + 0x40, rx_ld4(entity + pose_offset));
    }
    handle = U32(entity, 0x158);
    if (handle) {
        ACT_CALLR(body, 0, rx_world_physics_handle_body(handle));
        rx_st4(body + 0x60, rx_ld4(entity + pose_offset));
    }
    handle = U32(entity, 0x158);
    if (!state) {
        if (handle) {
            ACT_CALLR(body, 0, rx_world_physics_handle_body(handle));
            SU8(body, 0xb0, 1);
        }
        SU32(entity, 0x174, second ? 1 : 0);
        if (second) SU32(entity, 0x170, 1);
        SU8(entity, 0xa2, 1);
    } else {
        if (handle) {
            rx_addr world = U32(handle, 4);
            ACT_CALLR(body, 0, rx_world_physics_handle_body(handle));
            ACT_CALL(1, rx_world_kinematic_deactivate(body, world));
            ACT_CALLR(body, 0, rx_world_physics_handle_body(U32(entity, 0x158)));
            SU8(body, 0xb0, 0);
        }
        SU32(entity, 0x174, 1);
        if (second) SU32(entity, 0x170, 0);
        SU8(entity, 0xa2, 0);
    }
    rx_st4(f + 0x10, rx_ld4(entity + pose_offset));
    SU32(entity, 0x60, U32(f, 0x10));
    SU32(entity, 0x64, U32(f, 0x14));
    SU32(entity, 0x68, U32(f, 0x18));
    if (!second) SU32(entity, 0x178, 1);
    ACT_END(f);
}
void rx_replay_world_section_a(rx_addr entity, uint32_t state) { replay_world_section(entity, state, 0); }
RX_FUNCTION(0x006c0c80, rx_replay_world_section_a)
void rx_replay_world_section_b(rx_addr entity, uint32_t state) { replay_world_section(entity, state, 1); }
RX_FUNCTION(0x006c1110, rx_replay_world_section_b)

#include "anim.h"
#include "crt.h"
static void replay_rewind_world(rx_addr clip, int second) {
    ACT_FRAME_EBP(f, 8, 0x40c, 3);
    rx_addr threshold = second ? 0x00a942cc : 0x00a942d0;
    if (F32(0,threshold) > F32(0,0x0092e12c)) {
        SU32(f,0x18,0);
        ACT_CALL(3,crt_memset(f+0x1c,0,0x3f8));
        float split=F32(0,threshold),start=F32(clip,0x35b0);
        SU32(f,0x10,second ? !(split > start) : split > start);
        rx_addr world=U32(U32(clip,4),0x234);
        int32_t count;
        ACT_CALLR(count,4,rx_scene_manager_collect_typed(U32(world,0x20138),second?0x1032:0x1031,f+0x18,0xff,f+0x14));
        for(int32_t i=0;i<count;++i) {
            rx_addr entity=U32(f,0x18+4u*(uint32_t)i);
            if(second) ACT_CALL(1,rx_replay_world_section_a(entity,U32(f,0x10)));
            else ACT_CALL(1,rx_replay_world_section_b(entity,U32(f,0x10)));
        }
    }
    ACT_END(f);
}
void rx_replay_rewind_world_a(rx_addr clip){replay_rewind_world(clip,0);}
RX_FUNCTION(0x006aa320,rx_replay_rewind_world_a)
void rx_replay_rewind_world_b(rx_addr clip){replay_rewind_world(clip,1);}
RX_FUNCTION(0x006aa3c0,rx_replay_rewind_world_b)
