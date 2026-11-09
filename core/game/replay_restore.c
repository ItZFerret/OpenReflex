/* Complete original leaves. Working source; not composed or runtime accepted. */
#include "replay_restore.h"
#include "rider_cap.h"
#include "world_rider.h"
#include "anim_scenario.h"
#include "loop_world.h"

void rx_replay_rider_reset(rx_addr rider) {
    ACT_FRAME(f, 0, 1);
    if (!U32(rider, 0xaf08)) {
        ACT_CALL(1, rx_world_rider_end_pose_override(rider));
        ACT_CALL(0, rx_world_rider_end_stunt(rider));
        ACT_CALL(0, rx_world_rider_end_trick(rider));
        ACT_CALL(1, rx_world_rider_end_crash(rider));
        ACT_CALL(0, rx_rider_animation_reset_stunt(rider));
        ACT_CALL(0, rx_rider_animation_restart(rider));
        rx_addr p = U32(rider, 0x57c); if (p) SU8(p, 0, 1);
        p = U32(rider, 0x580); if (p) SU16(p, 0, U16(rider, 0x5a0));
        p = U32(rider, 0x564); if (p) SU8(p, 0, 1);
        p = U32(rider, 0x568); if (p) SU16(p, 0, U16(rider, 0x5ac));
        /* COMISS/JAE then COMISS/JB: unordered chooses the ceiling,
         * then retains it rather than selecting an unordered lower bound. */
        float ceiling = F32(0, 0x00931f24), value = F32(rider, 0x588);
        if (!(ceiling >= value)) value = ceiling;
        float floor_value = F32(rider, 0x584);
        if (floor_value >= value) value = floor_value;
        p = U32(rider, 0x58c); if (p) SF32(p, 0, value);
        value = F32(rider, 0x594);
        if (!(ceiling >= value)) value = ceiling;
        floor_value = F32(rider, 0x590);
        if (floor_value >= value) value = floor_value;
        p = U32(rider, 0x598); if (p) SF32(p, 0, value);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0061d270, rx_replay_rider_reset)

void rx_replay_restore_wreck(rx_addr player, uint32_t state) {
    ACT_FRAME(f, 4, 0);
    rx_addr vehicle = U32(player, 0x1ac);
    SU32(vehicle, 0x280, state);
    SU32(U32(vehicle, 0x234), 0x2374, 1);
    ACT_CALL(2, rx_world_rider_end_wreck(U32(player, 0x1b4), U32(player, 0xb30)));
    ACT_END(f);
}
RX_FUNCTION(0x0054edc0, rx_replay_restore_wreck)

void rx_replay_restore_model_states(rx_addr player, rx_addr registry, rx_addr values) {
    ACT_FRAME(f, 0, 1);
    for (uint32_t slot = 0; slot != 12; ++slot) {
        if (U32(registry, 0x40c + slot * 0x410) != player) continue;
        rx_addr entry = registry + 0x10 + slot * 0x410;
        for (uint32_t i = 0; i < U32(entry, 0); ++i)
            SU32(entry, 0x110 + 4 * i, U32(values, 4 * i));
        break;
    }
    ACT_END(f);
}
RX_FUNCTION(0x0047be80, rx_replay_restore_model_states)

void rx_replay_select_camera(rx_addr driver, rx_addr world) {
    ACT_FRAME(f, 0, 0);
    SU32(world, 0x21de0, driver);
    if (U8(world, 0x21de4) && driver) {
        rx_addr cameras = U32(driver, 0x190);
        rx_addr camera = U32(cameras, 0x974 + 4 * U32(cameras, 0x988));
        /* Canonical camera vf28 owners consume only self; retain the original
         * unused pushed argument in guest entry depth. vf19 consumes float dt. */
        rx_addr fn = U32(U32(camera, 0), 0x70);
        if (!rx_fn(fn)) act_unreached(fn, "replay camera vf28");
        ACT_CALL(1, ((void (*)(rx_addr))rx_fn(fn))(camera));
        driver = U32(world, 0x21de0);
        cameras = U32(driver, 0x190);
        camera = U32(cameras, 0x974 + 4 * U32(cameras, 0x988));
        fn = U32(U32(camera, 0), 0x4c);
        if (!rx_fn(fn)) act_unreached(fn, "replay camera vf19");
        ACT_CALL(1, ((void (*)(rx_addr,float))rx_fn(fn))(camera, rx_f80_to_f32(rx_f80_from_f32(F32(0,0x00977818)),0)));
    }
    ACT_CALL(2, rx_replay_cameras_update(world, rx_f80_to_f32(rx_f80_from_f32(F32(0,0x00977818)),0)));
    ACT_END(f);
}
RX_FUNCTION(0x007676f0, rx_replay_select_camera)

#include "replay_damage.h"
#include "world_veh.h"
#include "phys_body.h"
/* 006aa510 EDI=entry; alignment8, saved ECX and ESI. */
void rx_replay_restore_entry(rx_addr entry) {
    ACT_FRAME_EBP(f,8,4,1);
    rx_addr vehicle=U32(entry,0);
    if(vehicle) {
        rx_addr manager;
        ACT_CALLR(manager,0,rx_world_damage_manager_instance());
        int32_t type=I32(vehicle,0x194);
        if(type==3||type==5||type==7) {
            /* Position argument is already pushed while instance is resolved. */
            rx_set_sp(rx_sp()-4);
            ACT_CALLR(manager,0,rx_world_damage_manager_instance());
            rx_set_sp(rx_sp()+4);
            ACT_CALL(1,rx_replay_damage_restore(U32(entry,0),manager,entry+8));
            ACT_CALLR(manager,0,rx_world_damage_manager_instance());
            ACT_CALL(0,rx_replay_restore_model_states(U32(entry,0),manager,entry+0x28));
        }
        uint32_t clock=U32(entry,4);
        vehicle=U32(entry,0);
        int mud_found=0;
        for(uint32_t index=0;index<16;++index) {
            if(vehicle==U32(0,0x00a96dc0+index*0x40)) {
                rx_addr clock_state=0x00a96d90+index*0x40;
                SU32(clock_state,0,clock);SU32(clock_state,4,clock);SU32(clock_state,8,clock);SU32(clock_state,12,clock);
                mud_found=1;
                break;
            }
        }
        if(!mud_found&&rx_mud_side_of(vehicle)) {   /* (w-rc-core) records 12..31 */
            rx_addr clock_state=rx_mud_side_of(vehicle);
            SU32(clock_state,0,clock);SU32(clock_state,4,clock);SU32(clock_state,8,clock);SU32(clock_state,12,clock);
        }
        if(U32(entry,0x48)) {
            rx_addr handle=U32(U32(entry,0),0x17c);
            if(handle) {
                rx_addr body;
                ACT_CALLR(body,0,phys_body_record(handle));
                SU32(U32(body,0x2c),0x14,0xffffffffu);
            }
            vehicle=U32(entry,0);
            SU8(U32(vehicle,0x1328),0x14e,0);
            vehicle=U32(entry,0);
            SU8(U32(vehicle,0x1a0),0xa2,0);
        }
    }
    ACT_END(f);
}
RX_FUNCTION(0x006aa510,rx_replay_restore_entry)
