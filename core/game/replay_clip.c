#include "replay_clip.h"
#include "build_activity_util.h"
#include "waypoint_playback.h"
#include "world_board_handlers.h"
#include "build_activity_pre.h"
#include "loop_world.h"
#include "build_activity_freeride_run.h"

void rx_replay_start(int32_t type) {
    ACT_FRAME(f, 0, 2);
    if (U32(0,0x00d5a9a4)) {
        rx_addr world = U32(U32(0,0x00a942b8),0x234);
        if (U32(world,0x20138)) {
            if (!(U8(0,0x009e30e0)&1)) {
                SU32(0,0x009e30e0,U32(0,0x009e30e0)|1u);
                SU32(0,0x009e30dc,0);
            }
            rx_addr terrain;
            ACT_CALLR(terrain,0,rx_terrain_for_scene(0x009e30d4,U32(world,0x20138)));
            if (terrain) SU32(terrain,0x449c,1);
        }
        int32_t slot;
        ACT_CALLR(slot,0,rx_clip_recorder_find_playback_type(type));
        if (slot < 0) {
            slot = 0;
            rx_addr cursor = 0x00d323c8;
            while (U32(cursor,0)==0) {
                cursor += 0x35d0; ++slot;
                if (cursor >= 0x00d5a988) { slot = -1; break; }
            }
        }
        if (slot >= 0) {
            ACT_CALL(0,rx_replay_begin_clip(slot));
            SU32(0,0x00a942b4,2);
            SU32(0,0x009c4074,slot);
        }
        rx_replay_require_viewport(U32(0,0x00a946b8),1); /* bridge owns actual two-arg entry depth */
    }
    ACT_END(f);
}
RX_FUNCTION(0x006aa8d0,rx_replay_start)

void rx_replay_begin_clip(int32_t slot) {
    ACT_FRAME(f,0,1);
    SU32(0,0x009c4070,slot);
    if (0.0f < F32(0,0x00a942a4)) {
        float ratio=rx_divs(F32(0,0x00d35970+(uint32_t)slot*0x35d0),F32(0,0x00a942a4));
        ACT_CALL(1,rx_world_set_terrain_wetness(U32(U32(0,0x00a942b8),0x234),ratio));
    }
    ACT_CALL(0,rx_physics_world_reset_bodies(U32(U32(U32(0,0x00a942b8),0x234),0x20134)));
    ACT_CALL(1,rx_replay_setup_clip(0x00d323c0+(uint32_t)slot*0x35d0));
    ACT_CALL(0,rx_replay_fade_in());
    const uint32_t sound[2]={0,0};
    act_call_fn(0x00805000,RX_CDECL,0,2,sound);
    ACT_END(f);
}
RX_FUNCTION(0x006aaa30,rx_replay_begin_clip)

void rx_replay_next_clip(void) {
    ACT_FRAME(f,0,1);
    if (I32(0,0x00a942b4)==2) {
        int32_t slot=I32(0,0x009c4070);
        ACT_CALL(0,rx_clip_recorder_clear_follow(U32(0,0x00d323c4+(uint32_t)slot*0x35d0)));
        int32_t reel=I32(0,0x009c4074);
        /* Original deliberately has no attempt cap: the existing enabled reel
         * invariant must supply an occupied mapped slot, not a fake fallback. */
        do {
            reel=(int32_t)((uint32_t)reel+1u)%12;
            slot=I32(0,0x00a7f630+(uint32_t)reel*4);
        } while (slot<0 || !U32(0,0x00d323c8+(uint32_t)slot*0x35d0));
        ACT_CALL(0,rx_replay_begin_clip(slot));
        SU32(0,0x009c4074,reel);
    }
    ACT_END(f);
}
RX_FUNCTION(0x006ab730,rx_replay_next_clip)
