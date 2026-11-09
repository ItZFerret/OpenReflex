/* Actual shared finish/results scenario steps. No replacement scenario starts. */
#include "waypoint_post_steps.h"
#include "build_activity_int.h"
#include "race_activity.h"
#include "anim.h"
#include "anim_scenario.h"
#include "anim_scenario_misc.h"
#include "phys_crt.h"
#include "rider_cap.h"

/*0074f380 EDX=actor name, stackscenario/player/locator/flag, RET16. */
void rx_post_place_chief(rx_addr sc,rx_addr player,rx_addr locator_name,uint32_t push_bike,rx_addr actor_name) {
    ACT_FRAME_EBP(f,8,4,3);
    rx_addr podium=U32(U32(sc,0x14),0x234)+0x20150;
    rx_addr chief;ACT_CALLR(chief,2,rx_scenario_anims_find_camera(podium,actor_name));
    if (chief) {
        rx_addr vehicle=U32(player,0x1ac);
        ACT_CALL(1,rx_crew_chief_set_shirt(vehicle,chief));
        if (push_bike && !U32(vehicle,0x194)) {
            rx_addr locator;ACT_CALLR(locator,2,rx_scenario_anims_find_prop(podium,locator_name));
            if (locator) ACT_CALL(2,rx_post_push_bike(vehicle,chief,locator,podium));
        } else ACT_CALL(3,rx_post_place_object(locator_name,U32(sc,0x14),chief,0x0096cd14));
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074f380,rx_post_place_chief)

/*0074f410 stackscenario RET4. The two formatting calls share their original
 * cdecl argument area until both finish; crew flags come from the full switch. */
void rx_post_lose_walk(rx_addr sc) {
    ACT_FRAME(f,0x214,2);
    SU32(f,0x218,U32(0,0x009b45e4)^(f+8));
    if (I32(U32(sc,0x14),0x26c)>=4) {
        ACT_FRAME(t,0,2);
        SU32(t,0x10,0x2dc);
        for (int32_t place=4;place<=I32(U32(sc,0x14),0x26c);++place) {
            uint32_t push_bike=place==4||place==6||place==9||place==10;
            SU32(t,0x14,push_bike);
            SU32(t-4,0,(uint32_t)place);SU32(t-8,0,0x0096cd48);
            rx_set_sp(t-12);rx_format_buffer(t+0x11c,0x104,0x0096cd48,t-4);rx_set_sp(t-8);
            SU32(t-12,0,(uint32_t)place);SU32(t-16,0,0x0096cd2c);SU32(t-20,0,0x0093f6f8);
            rx_set_sp(t-24);rx_format_buffer(t+0x18,0x104,0x0093f6f8,t-16);rx_set_sp(t);
            rx_addr driver=U32(U32(sc,0x14),U32(t,0x10));
            ACT_CALL(4,rx_post_place_chief(sc,U32(driver,0x74),t+0x18,U32(t,0x14),t+0x11c));
            SU32(t,0x10,U32(t,0x10)+4);
        }
        ACT_END(t);
    }
    rx_set_sp(f+8);ACT_CALL(0,phys_security_check_cookie(U32(f,0x218)^(f+8)));
    ACT_END(f);
}
RX_FUNCTION(0x0074f410,rx_post_lose_walk)

/*0074f510 EDI=sc, stackstep RET4. Stage11 is a genuine tail call. */
void rx_post_step_cameras(rx_addr sc,uint32_t step) {
    ACT_FRAME(f,0,1);
    rx_addr podium=U32(U32(sc,0x14),0x234)+0x20150;
    if (step==11) {ACT_END(f);SU32(rx_sp(),4,sc);rx_post_lose_walk(sc);return;}
    if (step==10 || step==12) {
        ACT_CALL(0,rx_scenario_anims_reset_cameras(podium));
        rx_addr camera=U32(podium,0x1730);
        if (camera) ACT_CALL(3,rx_post_place_object(0x0096cd70,U32(sc,0x14),camera,step==10 ? 0x0096cd84 : 0x0096cd58));
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074f510,rx_post_step_cameras)

/*0074f7e0 stackscenario RET4: winner's mound actor and actual vehicle locator. */
void rx_post_winner_mound(rx_addr sc) {
    ACT_FRAME_EBP(f,8,4,3);
    rx_addr a=U32(sc,0x14),player=U32(U32(a,0x2d0),0x74);
    rx_addr cast;ACT_CALLR(cast,1,rx_scenario_cast_find(U32(a,0x234)+0x20150,player));
    rx_addr actor=U32(cast,0),animation;
    if (U32(U32(player,0x1ac),0x194)) {
        ACT_CALL(1,rx_post_vehicle_at_locator(actor,U32(U32(sc,0x14),0x234)+0x20150,0x0096ce6c));
        animation=0x0096ce54;
    } else animation=0x0096ce44;
    rx_addr podium=U32(U32(sc,0x14),0x234)+0x20150,locator;
    ACT_CALLR(locator,2,rx_scenario_anims_find_prop(podium,0x0096ce8c));
    if (locator && actor) ACT_CALL(4,rx_scenario_cast_place_entity(U32(U32(sc,0x14),0x234)+0x20150,0,actor,locator,animation,0));
    ACT_END(f);
}
RX_FUNCTION(0x0074f7e0,rx_post_winner_mound)

/*0074f620 stackscenario RET4: local podium cast at authored placement names.
 * Preserve the original null-cast dereference rather than inventing an actor. */
void rx_post_podium_actors(rx_addr sc) {
    ACT_FRAME_EBP(f,8,0x124,3);
    SU32(f,0x12c,U32(0,0x009b45e4)^(f+0xc));
    SU32(f,0x10,sc);SU32(f,0x14,0x2d0);
    for (int32_t index=0;index<(int32_t)U8(U32(0,0x00a946b8),0x1541c);++index) {
        SU32(f,0x18,(uint32_t)index);
        rx_addr player=U32(U32(U32(sc,0x14),U32(f,0x14)),0x74);SU32(f,0x1c,player);
        if ((index==11||index==10||index==2||index==6) && (!U32(player,4)||U32(player,0x10))) {
            SU32(f,0x14,U32(f,0x14)+4);continue;
        }
        rx_addr vehicle=U32(player,0x1ac);
        rx_addr animation=index<=1 ? (U32(vehicle,0x194)==1 ? 0x0096cda0 : 0x0096cdb8) :
                                             (U32(vehicle,0x194)==1 ? 0x0096cdcc : 0x0096cde4);
        SU32(f,0xc,animation);SU8(U32(vehicle,0x1328),0x14e,1);
        SU32(f-4,0,(uint32_t)index+1);SU32(f-8,0,0x0096cdf8);
        ACT_CALL(2,rx_format_buffer(f+0x20,0x104,0x0096cdf8,f-4));
        if (U32(sc,0x151b4)==12 && U32(player,4) && !U32(player,0x10)) {
            SU32(f-4,0,0x0096ce1c);ACT_CALL(1,rx_format_buffer(f+0x20,0x104,0x0096ce1c,f));
        }
        rx_addr podium=U32(U32(sc,0x14),0x234)+0x20150,cast=0;
        for (uint32_t j=0;j<RX_RC_MAX;++j) {   /* (w-rc-core) entries 12..31 in side storage */
            rx_addr c=rx_cast_rec_ro(podium,j),who=c?U32(c,4):0;
            if (!who) break;
            if (who==U32(f,0x1c)) {cast=c;break;}
        }
        rx_addr actor=U32(cast,0),locator;
        ACT_CALLR(locator,2,rx_scenario_anims_find_prop(podium,f+0x20));
        ACT_CALL(4,rx_scenario_cast_place_entity(U32(U32(U32(f,0x10),0x14),0x234)+0x20150,0,actor,locator,U32(f,0xc),0));
        float scale=rx_f80_to_f32(rx_f80_from_f32(F32(0,0x009d3bfc)),0);
        act_vcall_f(actor,0x4c,scale);
        SU32(f,0x14,U32(f,0x14)+4);
    }
    rx_set_sp(f+0xc);ACT_CALL(0,phys_security_check_cookie(U32(f,0x12c)^(f+0xc)));
    ACT_END(f);
}
RX_FUNCTION(0x0074f620,rx_post_podium_actors)

/*0074f590 EAX=step, ESI=sc. The switch table maps8winner,10/12podium,
 *9/11 winner-simulation off; other steps do nothing as in the original. */
void rx_post_step_entities(rx_addr sc,uint32_t step) {
    if (step==8) {
        ACT_CALL(1,rx_post_winner_mound(sc));
        rx_addr vehicle=U32(U32(U32(U32(sc,0x14),0x2d0),0x74),0x1ac);
        SU32(U32(vehicle,0x234),0x54,1);
    } else if (step==9||step==11) {
        rx_addr vehicle=U32(U32(U32(U32(sc,0x14),0x2d0),0x74),0x1ac);
        SU32(U32(vehicle,0x234),0x54,0);
    } else if (step==10||step==12) ACT_CALL(1,rx_post_podium_actors(sc));
}
RX_FUNCTION(0x0074f590,rx_post_step_entities)

/*0074f260 EAX=step, EDI=sc. Debug-only rearrangement of the real standings. */
void rx_post_debug_standings(rx_addr sc,int32_t step) {
    if (!U32(0,0x00d61048)||step<7) return;
    ACT_FRAME(f,0,1);
    rx_addr a=U32(sc,0x14);
    if (step>10) {
        if (step==11) {
            int32_t last=I32(a,0x26c)-1;
            for (;last>=0;--last) {
                rx_addr player=U32(rx_stand(a, (uint32_t)last),0x74);
                if (U32(player,4)&&!U32(player,0x10)) break;
            }
            if (last<=2) for (uint32_t i=0;i<3;++i) {
                rx_addr player=U32(rx_stand(a, i),0x74);
                if (U32(player,4)&&!U32(player,0x10)) {
                    rx_addr displaced=U32(a,0x2dc);SU32(a,0x2dc,rx_stand(a, i));
                    if (displaced) rx_stand_set(U32(sc,0x14), i, displaced);
                    break;
                }
            }
        }
    } else if (U32(U32(U32(a,0x2d0),0x74),0x10)) {
        int32_t count=I32(a,0x26c);
        for (int32_t i=1;i<count;++i) if (!U32(U32(rx_stand(a, (uint32_t)i),0x74),0x10)) {
            a=U32(sc,0x14);rx_addr first=U32(a,0x2d0);
            SU32(a,0x2d0,rx_stand(a, (uint32_t)i));
            rx_stand_set(U32(sc,0x14), (uint32_t)i, first);break;
        }
    }
    ACT_END(f);
}
RX_FUNCTION(0x0074f260,rx_post_debug_standings)
