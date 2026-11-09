/* race_beh2_extern.h -- what the behaviours of race_beh2_*.c borrow: the vehicle's virtual functions (physics, not in the core yet),
 * the driver / activity helpers of other race clusters that race.h does not declare yet, and small shared pieces (the tuning-table
 * index, the inlined Activity_UiKind). One wrapper per callee. */
#pragma once
#include "race_activity.h"
#include "race.h"
#include "race_driver.h"
#include "race_extern.h"
#include "race_beh2.h"
#include "build_series_init.h"
#include "career_activity.h"
#include "crt_atexit.h"

/* The driver's vehicle (driver +0x74 = MXPlayer, player +0x1ac = RavenVehicle). */
static inline rx_addr beh2_vehicle(rx_addr driver) { return U32(U32(driver, 0x74), 0x1ac); }

/* The AI tuning index of a driver: player +0x8f4 (the tuning set) + 4 * (3 * driver +0x328 + driver +0x32c) (difficulty row and
 * column). The per-index float tables (0x009e3108.., 0x009e4598, 0x00a04b98 ...) are .data (configurable): always read. */
static inline uint32_t beh2_tune(rx_addr driver) {
    return U32(U32(driver, 0x74), 0x8f4) + (U32(driver, 0x328) * 3u + U32(driver, 0x32c)) * 4u;
}
static inline float beh2_tune_f(rx_addr table, rx_addr driver) { return F32(0, table + 4u * beh2_tune(driver)); }

/* Vehicle class tables (vehicle +0x194): 0x0093580c width, 0x0093582c length. */
static inline float beh2_class_width(rx_addr vehicle) { return F32(0, 0x0093580c + 4u * U32(vehicle, 0x194)); }
static inline float beh2_class_length(rx_addr vehicle) { return F32(0, 0x0093582c + 4u * U32(vehicle, 0x194)); }

/* RavenVehicle::vf14 (slot 14): a pointer to the vehicle's world matrix (rows x, y, z, translation). */
typedef rx_addr (*beh2_vehicle_ptr_fn)(rx_addr vehicle);
static inline rx_addr beh2_vehicle_matrix(rx_addr vehicle) {
    rx_addr fn = race_vfn(vehicle, 14);
    beh2_vehicle_ptr_fn f = RX_FN(fn, beh2_vehicle_ptr_fn);
    return f ? f(vehicle) : race_thiscall(fn, vehicle, 0, 0, 0, 0, 0);
}
/* RavenVehicle slot 17 (+0x44): a vector of the vehicle into out (used by the avoidance as the rider's point to steer from). */
typedef void (*beh2_vehicle_vec_fn)(rx_addr vehicle, rx_addr out);
static inline void beh2_vehicle_point(rx_addr vehicle, rx_addr out) {
    rx_addr fn = race_vfn(vehicle, 17);
    beh2_vehicle_vec_fn f = RX_FN(fn, beh2_vehicle_vec_fn);
    if (f) f(vehicle, out); else race_thiscall(fn, vehicle, 1, out, 0, 0, 0);
}
/* RavenVehicle slot 43 (+0xac): a state flag of the vehicle (the avoidance only steers / follows when it is set / clear). */
typedef uint32_t (*beh2_vehicle_u32_fn)(rx_addr vehicle);
static inline uint32_t beh2_vehicle_flag43(rx_addr vehicle) {
    rx_addr fn = race_vfn(vehicle, 43);
    beh2_vehicle_u32_fn f = RX_FN(fn, beh2_vehicle_u32_fn);
    return f ? f(vehicle) : race_thiscall(fn, vehicle, 0, 0, 0, 0, 0);
}
/* The vehicle's collision object (vehicle +0x94) slot 6: its bounds in the vehicle frame (centre, extents, radius). */
typedef void (*beh2_bounds_fn)(rx_addr shape, rx_addr centre, rx_addr extents, rx_addr radius);
static inline void beh2_shape_bounds(rx_addr shape, rx_addr centre, rx_addr extents, rx_addr radius) {
    rx_addr fn = race_vfn(shape, 6);
    beh2_bounds_fn f = RX_FN(fn, beh2_bounds_fn);
    if (f) f(shape, centre, extents, radius); else race_thiscall(fn, shape, 3, centre, extents, radius, 0);
}

/* Behaviour slot 6 (reset) and slot 7 (enable, 0052dc40 for most classes). */
typedef uint32_t (*beh2_beh_fn)(rx_addr beh);
static inline void beh2_behavior_reset(rx_addr beh) {
    rx_addr fn = race_vfn(beh, 6);
    beh2_beh_fn f = RX_FN(fn, beh2_beh_fn);
    if (f) f(beh); else race_thiscall(fn, beh, 0, 0, 0, 0, 0);
}
typedef uint32_t (*beh2_beh_u32_fn)(rx_addr beh, uint32_t value);
static inline void beh2_behavior_enable(rx_addr beh, uint32_t on) {
    rx_addr fn = race_vfn(beh, 7);
    beh2_beh_u32_fn f = RX_FN(fn, beh2_beh_u32_fn);
    if (f) f(beh, on); else race_thiscall(fn, beh, 1, on, 0, 0, 0);
}

/* 0050a570 (race_math.c): the vector's x / z, normalized in the ground plane. */
static inline void beh2_flat_dir(rx_addr v, rx_addr out) { rx_race_flat_direction(v, out); }
/* 0050deb0 (race_driver.c) -> out: the vector projected onto the unit direction. */
static inline rx_addr beh2_project_on(rx_addr dir, rx_addr v, rx_addr out) { return rx_race_project_on(dir, v, out); }
/* 00556280 Activity_UiKind (activity cluster; EAX = activity) -> EAX: the activity's kind (+0x23c, or the series manager's for
 * kinds >= 0x1f). */
static inline uint32_t beh2_activity_ui_kind(rx_addr activity) {
    uint32_t r;
    RACE_CALL_SP(0, r = rx_activity_ui_kind(activity));
    return r;
}
/* The same, inlined in 0052a380 / 0052a6d0 / 00535090: kind +0x23c; from 0x1f on the MXSeriesManager's (constructed on first use
 * behind the static guard 0x9e275c bit 0: 005b2370, _atexit(00911da0)), 005b6a30(0x9e2188). */
static inline uint32_t beh2_activity_kind_inline(rx_addr activity) {
    uint32_t kind = U32(activity, 0x23c);
    if (kind >= 0x1f) {
        if ((U32(0, 0x009e275c) & 1) == 0) {
            SU32(0, 0x009e275c, U32(0, 0x009e275c) | 1);
            RACE_CORE_CALL(0, (void)rx_series_manager_construct());
            RX_ATEXIT(0x00911da0);
        }
        RACE_CORE_CALL(1, kind = rx_career_current_event_type(0x009e2188));   /* 005b6a30 (stdcall manager; RET 4) */
    }
    return kind;
}
/* 0055a000 (activity cluster; EAX = activity, args: player kind, index; RET 8) -> EAX: the index-th driver whose player has that
 * kind (player +0x10: 0 = local human). */
static inline rx_addr beh2_activity_driver_of_kind(rx_addr activity, uint32_t kind, uint32_t index) {
    rx_addr r;
    RACE_CALL_SP(8, r = rx_activity_driver_of_kind(activity, kind, (int32_t)index));
    return r;
}
