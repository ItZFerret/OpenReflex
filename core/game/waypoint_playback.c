/* Complete stop/lookup owners; no partial start or playback activation. */
#include "waypoint_playback.h"
#include "build_activity_util.h"
#include "world_board_handlers.h"
#include "loop_world.h"

int32_t rx_clip_recorder_find_playback_type(int32_t type) {
    int32_t index = 0;
    for (rx_addr p = 0x00d323c8; p < 0x00d5a988; p += 0x35d0, ++index) {
        if (type == 0 ? U32(p, 0) != 0 : I32(p, 0) == type) return index;
    }
    return -1;
}
RX_FUNCTION(0x006aaee0, rx_clip_recorder_find_playback_type)

void rx_clip_recorder_clear_follow(rx_addr activity) {
    ACT_FRAME(f, 0, 1);
    int32_t count = I32(activity, 0x26c);
    for (int32_t i = 0; i < count; ++i) {
        rx_addr driver = U32(U32(activity, 0x268), 4u * (uint32_t)i);
        rx_addr simulation = U32(U32(U32(driver, 0x74), 0x1ac), 0x234);
        SU32(simulation, 0x2e80, 0);
        SU32(simulation, 0x2e7c, 0);
    }
    ACT_END(f);
}
RX_FUNCTION(0x006ac3f0, rx_clip_recorder_clear_follow)

void rx_clip_recorder_stop_playback(void) {
    ACT_FRAME(f, 0, 1);
    int32_t current = I32(0, 0x009c4070);
    if (current >= 0) {
        rx_set_sp(rx_sp() - 4); /* original saved ESI around this helper */
        ACT_CALL(0, rx_clip_recorder_clear_follow(U32(0x00d323c4, (uint32_t)current * 0x35d0)));
        rx_set_sp(rx_sp() + 4);
    }
    SU32(0, 0x00a942b4, 0);
    /* Existing product audio boundary; original FMOD in oracle only. This port
     * neither implements nor claims audio output. */
    const uint32_t sound[2] = {0, 0};
    act_call_fn(0x00805000, RX_CDECL, 0, 2, sound);
    if (U32(0, 0x00a942b8)) {
        rx_addr world = U32(U32(0, 0x00a942b8), 0x234);
        SU32(world, 0x21de0, 0);
        ACT_CALL(2, rx_replay_cameras_update(world, F32(0, 0x00977818)));
        world = U32(U32(0, 0x00a942b8), 0x234);
        if (U32(world, 0x20138)) {
            if (!(U8(0, 0x009e30e0) & 1)) {
                SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
                SU32(0, 0x009e30dc, 0);
            }
            rx_addr terrain;
            rx_set_sp(rx_sp() - 4); /* original saved EDI */
            ACT_CALLR(terrain, 0, rx_terrain_for_scene(0x009e30d4, U32(world, 0x20138)));
            rx_set_sp(rx_sp() + 4);
            if (terrain) SU32(terrain, 0x449c, 0);
        }
    }
    ACT_END(f);
}
RX_FUNCTION(0x006aa990, rx_clip_recorder_stop_playback)
