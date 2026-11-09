#pragma once
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RX_WAYPOINT_API_VERSION 1u
/* Additive44-byte DTO; no guest addresses. gate_index is last passed (zero
 * based), -1 before first. UI GATES PASSED is gate_index+1, not next index.
 * remaining_distance is the original cached route metric in world feet;
 * it is neither a time gap nor Euclidean distance to the next gate.
 * distance_valid excludes the original uninitialized FLT_MAX sentinel and
 * nonfinite data. Unsupported/non-Waypoint returns0 with valid0. */
typedef struct rx_waypoint_state {
 uint32_t size,version,valid,distance_valid;
 int32_t activity_type,activity_mode,gate_index,gate_count,laps_completed,num_laps;
 float remaining_distance;
} rx_waypoint_state;
/* Caller sets exactsize and version1. Invalid arguments return1 and preserve
 * output. Invoke only on the session owner thread between advances. */
RX_API int rx_session_waypoint(struct rx_session* session,rx_waypoint_state* out);
#ifdef __cplusplus
}
#endif
