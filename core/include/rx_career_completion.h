#ifndef RX_CAREER_COMPLETION_H
#define RX_CAREER_COMPLETION_H
#include "rx_career_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RX_CAREER_COMPLETION_VERSION 1u
/* Additive successor contract: actual finite National Series6/7/8/10/11/12;
 * every current-profile product and parent lock still applies. Required startup
 * presenter mask243; full completion requires explicit Rewards|Career as well
 * (mask255). Older header layouts remain byte-identical. No capability is
 * inferred from loading this header or from the native action succeeding. */
/* Action state:0 not submitted,1 consumed/in progress,2 completed. */
typedef struct rx_career_completion_status {
 uint32_t size,version;
 uint64_t generation,ticket,frontend_request_id,checkpoint_one_id,checkpoint_two_id;
 uint32_t post_state_observed,complete_event_state,frontend_ready,records_state;
 uint32_t complete_series_state,tour_enabled,in_memory_settled,durable;
 uint32_t complete_event_allowed,frontend_ready_allowed,complete_series_allowed,checkpoint_error;
 char frontend_route[64];
} rx_career_completion_status;
RX_API int rx_session_career_completion(struct rx_session*,uint64_t,rx_career_completion_status*);
/* Requires actual matching Post State Complete after authentic Continue. */
RX_API int rx_session_career_complete_event(struct rx_session*,uint64_t,uint64_t ticket);
/* Explicit owned frontend presentation boundary AFTER actual accepted route.
 * Performs actual final-leave record settlement and stops native frames. Does
 * not claim original SwitchToFrontEnd, destroy the world, or accept a save. */
RX_API int rx_session_career_frontend_ready(struct rx_session*,uint64_t,uint64_t ticket,uint64_t route_id);
/* Explicit no-unowned-reward-vehicle loading completion: canonical CompleteSeries
 * then exact EnableTourMode body. Two native checkpoint requests stay pending;
 * only the separate durable receipt API can accept them. */
RX_API int rx_session_career_complete_series(struct rx_session*,uint64_t,uint64_t ticket);
#ifdef __cplusplus
}
#endif
#endif
