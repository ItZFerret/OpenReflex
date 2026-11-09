#include "replay_restore.h"
#include "fx.h"
#include "world_rider.h"
#include "anim_scenario.h"
#include "race_activity.h"

/* 006aa230 event=EAX, clip one stack argument, RET4. */
void rx_replay_play_event(rx_addr event, rx_addr clip) {
    ACT_FRAME(f, 0, 2);
    uint32_t kind = U32(event, 4);
    rx_addr player;
    switch (kind) {
        case 0:
            player = U32(U32(event, 8), 0x74);
            ACT_CALL(0, rx_fx_trick_start(player + 0xb58, U32(event, 0x60)));
            break;
        case 1:
            player = U32(U32(event, 8), 0x74);
            ACT_CALL(0, rx_fx_trick_hold_begin(player + 0xb58));
            break;
        case 2:
            player = U32(U32(event, 8), 0x74);
            ACT_CALL(0, rx_fx_trick_hold_end(player + 0xb58));
            break;
        case 3:
            player = U32(U32(event, 8), 0x74);
            ACT_CALL(0, rx_fx_trick_complete(player + 0xb58));
            break;
        case 4:
            player = U32(U32(event, 8), 0x74);
            ACT_CALL(0, rx_replay_restore_wreck(player, U32(event, 0x60)));
            break;
        case 6:
            player = U32(U32(event, 8), 0x74);
            ACT_CALL(0, rx_world_rider_start_scripted_mode(U32(player, 0x1b4), U32(event, 0x60)));
            break;
        case 7:
            player = U32(U32(event, 8), 0x74);
            /* Original pops both nonvolatiles, replaces its sole stack arg,
             * then tail-jumps621d70. Do not add a second synthetic call frame. */
            SU32(f_sp,4,U32(player,0x1b4)); /* tail replaces sole argument */
            ACT_END(f);
            rx_rider_animation_stop(U32(player, 0x1b4));
            return;
        case 8:
            ACT_CALL(3, rx_activity_start_sequence(U32(clip, 4), event + 0x60, 0));
            break;
        default: break; /* Actual jump table kind5 and out-of-range no-op. */
    }
    ACT_END(f);
}
RX_FUNCTION(0x006aa230, rx_replay_play_event)

void rx_replay_playback_tick(rx_addr clip, float dt) {
    ACT_FRAME(f, 4, 1);
    if (!U32(clip, 0x35b8)) {
        rx_addr event = clip + 0x3b0 + U32(clip, 0x35c4) * 0x80u;
        if (F32(clip, 0x35bc) == 0.0f && F32(event, 0) == 0.0f) {
            do {
                ACT_CALL(1, rx_replay_play_event(event, clip));
                SU32(clip, 0x35c4, U32(clip, 0x35c4) + 1);
                event = clip + 0x3b0 + U32(clip, 0x35c4) * 0x80u;
            } while (F32(event,0) == F32(0,0x0092e12c));
        }
        SF32(clip, 0x35bc, rx_adds(dt, F32(clip,0x35bc)));
        event = clip + 0x3b0 + U32(clip, 0x35c4) * 0x80u;
        while (F32(event,0) > rx_adds(F32(clip,0x35c0), F32(clip,0x35b0))) {
            float now = rx_adds(F32(clip,0x35bc), F32(clip,0x35b0));
            if (!(now >= F32(event,0))) break; /* COMISS/JB unordered */
            ACT_CALL(1, rx_replay_play_event(event, clip));
            SU32(clip,0x35c4,U32(clip,0x35c4)+1);
            event = clip + 0x3b0 + U32(clip,0x35c4)*0x80u;
        }
        SF32(clip,0x35c0,rx_f80_to_f32(rx_f80_from_f32(F32(clip,0x35bc)),0));
    }
    ACT_END(f);
}
RX_FUNCTION(0x006aa0f0, rx_replay_playback_tick)
