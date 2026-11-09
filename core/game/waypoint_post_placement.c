/* Shared post-scenario placement owners. Handwritten from complete original spans. */
#include "waypoint_post_steps.h"
#include "build_activity_int.h"
#include "race_activity.h"
#include "race_misc.h"
#include "anim_scenario.h"
#include "anim_ik.h"
#include "phys_crt.h"
#include "rider_cap.h"

/*0055a560 EAX=locator name, stack activity/object/animation, RET12. */
void rx_post_place_object(rx_addr locator_name,rx_addr activity,rx_addr object,rx_addr animation) {
    ACT_FRAME(f,0,4);
    if (locator_name) {
        rx_addr locator;
        ACT_CALLR(locator,2,rx_scenario_anims_find_prop(U32(activity,0x234)+0x20150,locator_name));
        if (!locator) ACT_CALLR(locator,0,rx_scenario_anims_find_entity(U32(activity,0x234)+0x20150,locator_name));
        if (locator) {
            rx_addr object_vtable=U32(object,0);
            rx_addr matrix=act_vcall(locator,0x38,0,0);
            act_call_fn(U32(object_vtable,0x40),RX_THISCALL,object,1,(uint32_t[]){matrix});
        }
    }
    if (animation) ACT_CALL(2,rx_activity_setup_object(animation,activity,object));
    ACT_END(f);
}
RX_FUNCTION(0x0055a560,rx_post_place_object)

/*0055e310 EAX=vehicle class, ECX=base name, stackoutput, RET4. */
void rx_post_vehicle_animation_name(uint32_t kind,rx_addr name,rx_addr out) {
    ACT_FRAME(f,0,2);
    rx_addr format=kind==0 ? 0x00941e88 : kind==1 ? 0x00941e80 : 0x00941e90;
    SU32(f-4,0,name);SU32(f-8,0,format);
    ACT_CALL(2,rx_format_buffer(out,0x104,format,f-4));
    ACT_END(f);
}
RX_FUNCTION(0x0055e310,rx_post_vehicle_animation_name)

/*0074c1e0 ESI=vehicle, stackposition/forward/animation, RET12. Ground placement
 * is the original simulation vf20; the animation is fetched through assets. */
void rx_post_place_vehicle(rx_addr vehicle,rx_addr position,rx_addr forward,rx_addr animation) {
    ACT_FRAME_EBP(f,16,0x3c,1);
    act_vcall(U32(vehicle,0x234),0x50,5,(uint32_t[]){position,forward,f+0x20,f+0x10,f+0x30});
    ACT_CALL(3,rx_vehicle_place(vehicle,f+0x20,f+0x10,f+0x30));
    rx_addr resource=act_vcall(U32(U32(0,0x00a95844),8),0x6c,4,(uint32_t[]){animation,0x6d696e61,0,1});
    if (resource) {
        SU8(U32(vehicle,0x1328),0x14e,1);SU8(U32(vehicle,0x1328),0x148,0xff);
        act_vcall(vehicle,0x80,1,(uint32_t[]){1});
        act_vcall(vehicle,0xc4,1,(uint32_t[]){1});
        ACT_CALL(2,rx_pose_override_set(vehicle,resource,1));
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074c1e0,rx_post_place_vehicle)

/*0074c0d0 ECX=vehicle, EDI=podium, stackchief/locator, RET8. */
void rx_post_push_bike(rx_addr vehicle,rx_addr chief,rx_addr locator,rx_addr podium) {
    ACT_FRAME_EBP(f,16,0x138,2);
    SU32(f,0x13c,U32(0,0x009b45e4)^(f+8));SU32(f,8,locator);
    rx_addr alias;ACT_CALLR(alias,1,rx_anim_alias_pick(0,0,0x0096ccf4));SU32(f,0xc,alias);
    ACT_CALL(1,rx_post_vehicle_animation_name(U32(vehicle,0x194),alias,f+0x30));
    rx_addr matrix=act_vcall(U32(f,8),0x38,0,0);
    for (uint32_t k=0;k<3;++k) {
        SU32(f,0x10+4*k,U32(matrix,0x30+4*k));
        SU32(f,0x20+4*k,U32(matrix,0x20+4*k));
    }
    SF32(f,0x14,rx_adds(F32(f,0x14),F32(0,0x00931f24)));
    rx_addr rider=U32(vehicle,0x1a0);
    SU32(rider,0x5bb5c,0);SU8(rider,0xa2,0);SU8(rider,0x5bb64,0);SU32(rider,0x5b6d0,0);
    ACT_CALL(3,rx_post_place_vehicle(vehicle,f+0x10,f+0x20,f+0x30));
    ACT_CALL(1,rx_scenario_anims_attach(vehicle,podium,chief));
    ACT_CALL(2,rx_activity_setup_object(U32(f,0xc),U32(podium,0x40),chief));
    rx_set_sp(f+8);ACT_CALL(0,phys_security_check_cookie(U32(f,0x13c)^(f+8)));
    ACT_END(f);
}
RX_FUNCTION(0x0074c0d0,rx_post_push_bike)

/*0074c480 EAX=actor, EDX=podium, stacklocator name, RET4. For a non-bike
 * winner move its real vehicle to the named locator and restore model flags. */
void rx_post_vehicle_at_locator(rx_addr actor,rx_addr podium,rx_addr locator_name) {
    ACT_FRAME_EBP(f,16,0x68,2);
    rx_addr cast=0;
    for (uint32_t i=0;i<RX_RC_MAX;++i) {   /* (w-rc-core) entries 12..31 in side storage */
        rx_addr c=rx_cast_rec_ro(podium,i);
        if (!c||!U32(c,4)) break;
        if (U32(c,0)==actor) {cast=c;break;}
    }
    if (cast) {
        rx_addr locator;ACT_CALLR(locator,2,rx_scenario_anims_find_prop(podium,locator_name));
        if (locator) {
            act_vcall(locator,0x3c,1,(uint32_t[]){f+0x30});
            for (uint32_t k=0;k<3;++k) {
                SU32(f,0x20+4*k,U32(f,0x60+4*k));SU32(f,0x10+4*k,U32(f,0x50+4*k));
            }
            rx_addr vehicle=U32(U32(cast,4),0x1ac);
            if (U32(vehicle,0x194)) {
                ACT_CALL(2,rx_vehicle_effect_query(vehicle,f+0x20,f+0x10));
                vehicle=U32(U32(cast,4),0x1ac);
                SU8(U32(vehicle,0x1328),0x148,0xff);
                act_vcall(vehicle,0x80,1,(uint32_t[]){1});
                SU8(U32(U32(U32(cast,4),0x1ac),0x1328),0x14e,1);
            }
        }
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074c480,rx_post_vehicle_at_locator)
