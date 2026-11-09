#include "replay_clip.h"
#include "rider_cap.h"
#include "replay_restore.h"
#include "build_activity_util.h"
#include "world_wreck.h"
/* Required genuine owners; deliberately no defaults or fallback bodies. */
void rx_replay_restore_entry(rx_addr entry);
void rx_replay_rewind_world_a(rx_addr clip);
void rx_replay_rewind_world_b(rx_addr clip);
void rx_replay_pose_vehicles(rx_addr activity, int32_t first, int32_t last);

void rx_replay_setup_clip(rx_addr clip) {
    ACT_FRAME(f, 0, 4);
    rx_addr driver = U32(clip,0);
    if (driver) {
        rx_addr player = U32(driver,0x74);
        if (player) {
            rx_addr vehicle = U32(player,0x1ac);
            if (vehicle && U32(0,0x009c407c)) {
                rx_addr world;
                ACT_CALLR(world,0,rx_world_active_sound_table(U32(0,0x00a946b8)));
                ACT_CALL(0,rx_replay_select_camera(vehicle,world));
            }
        }
    }
    for (int32_t i=0;i<I32(U32(clip,4),0x26c);++i) {
        rx_addr activity=U32(clip,4);
        driver=U32(U32(activity,0x268),4u*(uint32_t)i);
        rx_addr player=U32(driver,0x74),rider=U32(player,0x1b4);
        if(rider) ACT_CALL(0,rx_replay_rider_reset(rider));
        activity=U32(clip,4); player=U32(driver,0x74);
        if(I32(activity,0x23c)==5 && (!U32(player,4)||U32(player,0x10))) {
            rx_addr vehicle=U32(player,0x1ac);
            SU8(U32(vehicle,0x1328),0x14e,0);
            vehicle=U32(U32(driver,0x74),0x1ac);
            SU8(U32(vehicle,0x1a0),0xa2,0);
        } else {
            rx_addr vehicle=U32(player,0x1ac);
            SU8(U32(vehicle,0x1328),0x14e,1);
            vehicle=U32(U32(driver,0x74),0x1ac);
            SU8(U32(vehicle,0x1a0),0xa2,1);
            vehicle=U32(U32(driver,0x74),0x1ac);
            rx_addr animation=U32(vehicle,0x1a0);
            SU32(animation,0x5bb5c,1);SU8(animation,0xa2,1);SU8(animation,0x5bb64,1);SU32(animation,0x5b6d0,0);
            vehicle=U32(U32(driver,0x74),0x1ac);
            rx_addr target=U32(U32(vehicle,0),0xc4);
            if(!rx_fn(target))act_unreached(target,"replay vehicle vf49");
            ACT_CALL(1,((void(*)(rx_addr,uint32_t))rx_fn(target))(vehicle,1));
        }
    }
    for(int32_t i=0;i<I32(U32(clip,4),0x26c);++i)
        { const rx_addr e = rx_clip_entry(clip+0xc,(uint32_t)i,0); if (e) ACT_CALL(0,rx_replay_restore_entry(e)); }   /* (w-rc-core) entries 12..31 in side storage */
    ACT_CALL(0,rx_replay_rewind_world_a(clip));
    ACT_CALL(0,rx_replay_rewind_world_b(clip));
    SF32(U32(clip,4),0x294,rx_f80_to_f32(rx_f80_from_f32(F32(clip,0x35b0)),0));
    SU32(clip,0x35c4,0);SU32(clip,0x35b8,0);SU32(0,0x00a942e4,0);
    SU32(clip,0x35bc,0);SU32(clip,0x35c0,0);
    ACT_CALL(2,rx_replay_pose_vehicles(U32(clip,4),I32(clip,0x39c),I32(clip,0x3a0)));
    ACT_END(f);
}
RX_FUNCTION(0x006a9f30,rx_replay_setup_clip)
