/* Hand-translated Waypoint standings and in-progress state entry. */
#include "waypoint_activity.h"
#include "waypoint_driver.h"
#include "build_activity_int.h"
#include "race_activity.h"
#include "race.h"
#include "anim_scenario.h"
#include "world_misc.h"
#include "world_wreck.h"
#include "rider_cap.h"

/* 00579e10 (ESI=activity, EDI=driver, result XMM0). The first gate pose is
 * intentionally queried although only the NEXT gate position enters the result.
 * This is the original squared scalar sum, not squared Euclidean distance. */
float rx_waypoint_remaining_metric(rx_addr a, rx_addr driver) {
    ACT_FRAME_EBP(f, 16, 0xd0, 0);
    act_vcall(U32(U32(driver, 0x74), 0x1ac), 0x3c, 1, (uint32_t[]){f + 0x50});
    for (uint32_t k = 0; k < 3; ++k) SU32(f, 4*k, U32(f, 0x80 + 4*k));
    int32_t current = I32(driver, 0x10d4), count = I32(a, 0xff074);
    rx_addr next;
    if (current < (int32_t)((uint32_t)count - 1) && current >= 0) {
        act_vcall(U32(U32(a, 0xff078), 4u*(uint32_t)current), 0x3c, 1, (uint32_t[]){f + 0x90});
        next = U32(U32(a, 0xff078), 4u*U32(driver, 0x10d4) + 4);
    } else {
        act_vcall(U32(U32(a, 0xff078), 4u*(uint32_t)count - 4), 0x3c, 1, (uint32_t[]){f + 0x90});
        next = U32(U32(a, 0xff078), 0);
    }
    act_vcall(next, 0x3c, 1, (uint32_t[]){f + 0x10});
    float result = rx_subs(F32(f, 8), F32(f, 0x48));
    result = rx_adds(result, rx_subs(F32(f, 4), F32(f, 0x44)));
    result = rx_adds(result, rx_subs(F32(f, 0), F32(f, 0x40)));
    result = rx_muls(result, result);
    ACT_END(f); return result;
}
RX_FUNCTION(0x00579e10, rx_waypoint_remaining_metric)

/* 00578e10: RET12, the third explicit argument is unused. Equal progress sorts
 * as -1. Preserve the repeated pose queries: virtual pose getters may have effects. */
int32_t rx_waypoint_activity_vf43(rx_addr a, rx_addr first, rx_addr second, uint32_t unused) {
    (void)unused;
    ACT_FRAME_EBP(f, 8, 4, 3);
    int32_t result;
    if (I32(first, 0xf10) > I32(second, 0xf10)) result = -1;
    else if (I32(first, 0xf10) < I32(second, 0xf10)) result = 1;
    else if (I32(first, 0x10d4) > I32(second, 0x10d4)) result = -1;
    else if (I32(first, 0x10d4) < I32(second, 0x10d4)) result = 1;
    else if (I32(first, 0xf10) >= I32(a, 0xff090)) {
        float left, right;
        ACT_CALLR(left, 0, rx_waypoint_driver_total_time(first));
        ACT_CALLR(right, 0, rx_waypoint_driver_total_time(second));
        result = right >= left ? -1 : 1;
    } else {
        float left, right;
        ACT_CALLR(left, 0, rx_waypoint_remaining_metric(a, first)); SF32(f, 0xc, left);
        ACT_CALLR(right, 0, rx_waypoint_remaining_metric(a, second));
        if (right > F32(f, 0xc)) result = -1;
        else {
            ACT_CALLR(left, 0, rx_waypoint_remaining_metric(a, first)); SF32(f, 0xc, left);
            ACT_CALLR(right, 0, rx_waypoint_remaining_metric(a, second));
            result = F32(f, 0xc) > right ? 1 : -1;
        }
    }
    ACT_END(f); return result;
}
RX_FUNCTION(0x00578e10, rx_waypoint_activity_vf43)

/* 00579800: the special mode's best-time reply. No driver means no event. */
void rx_waypoint_activity_vf46(rx_addr a, uint32_t unused, uint32_t available, float time) {
    (void)unused;
    ACT_FRAME(f, 0, 1);
    if (U32(a, 0x238) == 3) {
        if (available) {
            SF32(a, 0xff08c, time); SU32(a, 0x228, 1);
            if (I32(a, 0x26c) > 0) {
                ACT_FRAME(t, 0, 2);
                for (int32_t i = 0; i < I32(a, 0x26c); ++i) {
                    rx_addr driver = U32(U32(a, 0x268), 4u*(uint32_t)i);
                    if (U32(U32(driver, 0x74), 0x10) == 2) {
                        SF32(driver, 0x10b0, time);
                        ACT_CALL(1, rx_event_best_lap(U32(a, 0x290), driver, F32(a, 0xff08c)));
                        break;
                    }
                }
                ACT_END(t);
            }
        }
        SU32(a, 0x224, 0);
    }
    ACT_END(f);
}
RX_FUNCTION(0x00579800, rx_waypoint_activity_vf46)

/* 00579090: in-progress enter. The first local human supplies the opening clip. */
void rx_waypoint_activity_vf39(rx_addr a, uint32_t previous) {
    (void)previous;
    ACT_FRAME(f, 4, 1);
    SU32(f, 4, a);
    ACT_CALL(1, rx_activity_in_progress_enter(a));
    if (U32(0, 0x00d5a9a4)) ACT_CALL(1, rx_clip_recorder_start_recording(a));
    ACT_FRAME(t, 0, 2);
    const uint32_t flags[] = {0xa7f38c,0xa7f388,0xa7f384,0xa7f390,0xa7f398,0xa7f39c,0xa7f3a0,0xa7f3a4};
    for (uint32_t i=0; i<8; ++i) SU32(0, flags[i], 1);
    rx_addr roster = U32(0, 0x00a946b8) + 0x7460, player = 0;
    if (I32(roster, 0xdf8c) != -1) {
        for (uint32_t i=0; i<U8(roster, 0xdfbc); ++i) {
            rx_addr p = rx_seat_active(roster, i);
            if (!U32(p, 0x10)) { player=p; break; }
        }
    }
    int32_t index; ACT_CALLR(index, 0, rx_world_group_rider_index(a, player));
    rx_addr array = U32(a, U32(player, 0x908) ? 0x27c : 0x268);
    rx_addr driver = U32(array, 4u*(uint32_t)index);
    ACT_END(t);
    ACT_CALL(2, rx_world_clip_recorder_candidate(8, driver, 0.0f, 5.0f));
    ACT_END(f);
}
RX_FUNCTION(0x00579090, rx_waypoint_activity_vf39)
