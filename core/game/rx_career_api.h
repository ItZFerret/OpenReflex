#ifndef RX_CAREER_API_H
#define RX_CAREER_API_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Isolated candidate v1. All calls belong to the session simulation worker,
 * between advances. Input/output structs contain copied values only. Profile
 * import uses the existing complete Player24 API BEFORE configure. No callback,
 * guest address, timer, save implementation or managed object crosses this ABI. */
#define RX_CAREER_API_VERSION 1u
#define RX_CAREER_TEXT_CAPACITY 256u
#define RX_CAREER_RESULT_CAPACITY 16u
enum rx_career_api_result {
 RX_CAREER_OK=0, RX_CAREER_REPEAT=1, RX_CAREER_INVALID=2,
 RX_CAREER_INACTIVE=3, RX_CAREER_FULL=4, RX_CAREER_STALE=5,
 RX_CAREER_WRONG_KIND=6, RX_CAREER_NOT_DELIVERED=7,
 RX_CAREER_CONFLICT=8, RX_CAREER_BUSY=9, RX_CAREER_EXHAUSTED=10,
 RX_CAREER_NOT_FOUND=11, RX_CAREER_EMPTY=12, RX_CAREER_OUT_OF_ORDER=13,
 RX_CAREER_UNSUPPORTED=100, RX_CAREER_BAD_STATE=101,
 RX_CAREER_FAULT=102, RX_CAREER_LOCKED=103
};
enum rx_career_presenter_capability {
 RX_CAREER_PRESENT_NATIONAL_HUD=1u, RX_CAREER_PRESENT_HUB=2u,
 RX_CAREER_PRESENT_REWARDS=4u, RX_CAREER_PRESENT_CAREER=8u,
 RX_CAREER_PRESENT_FADE=16u, RX_CAREER_PRESENT_WIPE=32u,
 RX_CAREER_PRESENT_RESULTS=64u
};
enum rx_career_request_kind {
 RX_CAREER_REQUEST_UI_EVENT=1, RX_CAREER_REQUEST_UI_ROUTE=2,
 RX_CAREER_REQUEST_HUB=3, RX_CAREER_REQUEST_FADE_OUT=4,
 RX_CAREER_REQUEST_PAGE_WIPE=5, RX_CAREER_REQUEST_CHECKPOINT=6
};
enum rx_career_phase {
 RX_CAREER_PHASE_NONE=0, RX_CAREER_PHASE_CONFIGURED=1,
 RX_CAREER_PHASE_LOADED=2, RX_CAREER_PHASE_VEHICLE_SELECTED=3,
 RX_CAREER_PHASE_INITIALIZED=4, RX_CAREER_PHASE_STARTED=5,
 RX_CAREER_PHASE_ABORTED=6
};
typedef struct rx_career_config {
 uint32_t size,version,controller,presenter_capabilities;
} rx_career_config;
typedef struct rx_career_series_info {
 uint32_t size,version,series_id,authored,supported,locked,eligible;
 int32_t product_id,blocking_product_id,vehicle_class,event_type_id;
 uint32_t unowned_reward_vehicle_count;
 char owner[32],format[32],activity[32];
} rx_career_series_info;
typedef struct rx_career_request {
 uint32_t size,version;
 uint64_t generation,id;
 uint32_t kind,state,delivered,wipe_milestone;
 float seconds;
 uint32_t color,controller,reserved;
 char name[RX_CAREER_TEXT_CAPACITY];
} rx_career_request;
typedef struct rx_career_result_row {
 int32_t rank,player_type,model_id;
 uint32_t points,gold,silver,bronze,holeshot,dnf;
 char name[128];
} rx_career_result_row;
typedef struct rx_career_status {
 uint32_t size,version;
 uint64_t generation;
 uint32_t phase,controller,series_id,session_state,pending_requests;
 uint32_t manager_active,event_present,event_phase,event_next_phase;
 uint32_t post_event_complete,activity_present,activity_state,activity_substate;
 uint32_t continue_allowed,continue_submitted,checkpoint_error,records_finalized;
 int32_t selected_vehicle_class,local_rank;
 uint32_t results_count,reward_valid,reward_base,reward_bonus;
 uint32_t reward_credits_flag,reward_purchase_flag,reward_unlock_flag;
 char event_status[32],location[64];
 rx_career_result_row results[RX_CAREER_RESULT_CAPACITY];
} rx_career_status;
/* Configure supports controller0/offline Tour only. Capability bits are the
 * caller's explicit implementation contract, not a discovery of legacy Lua UI.
 * Required: National HUD, Hub, Fade, Wipe, Results. Unknown bits reject.
 * Output generation is process-monotonic across destroy/recreate. */
RX_API int rx_session_career_configure(struct rx_session*,const rx_career_config*,uint64_t*generation);
/* Finite enumeration is series_id7 only. Authored data and native product/parent
 * locks are read from current imported profile; no unlocking is performed.
 * An unowned possible reward vehicle makes this minimal slice ineligible. */
RX_API int rx_session_career_series(struct rx_session*,uint64_t generation,uint32_t series_id,rx_career_series_info*);
RX_API int rx_session_career_load(struct rx_session*,uint64_t generation,uint32_t series_id);
/* Explicit original SetVehicle argument. This finite candidate supports 2;
 * it is not a vehicle model id or permission to invent a garage selection. */
RX_API int rx_session_career_select_vehicle(struct rx_session*,uint64_t generation,uint32_t vehicle_type);
RX_API int rx_session_career_initialize(struct rx_session*,uint64_t generation);
RX_API int rx_session_career_start(struct rx_session*,uint64_t generation);
RX_API int rx_session_career_status(struct rx_session*,uint64_t generation,rx_career_status*);
RX_API int rx_session_career_next(struct rx_session*,uint64_t generation,rx_career_request*);
/* Exact delivered request acknowledgement. PageWipe accepted=1 rejects; use
 * ordered milestones. Checkpoint accepted=1 is UNSUPPORTED until a distinct
 * nonterminal durable CAS bridge exists. accepted=0 records explicit failure.
 * Hub/route receipt never destroys the world or completes an event. */
RX_API int rx_session_career_ack(struct rx_session*,uint64_t generation,uint64_t id,uint32_t kind,uint32_t accepted);
/* milestone1=InComplete AFTER presenter started Out; 2=OutComplete AFTER hide.
 * Same bounded name is required; no automatic animation/time acknowledgement. */
RX_API int rx_session_career_wipe(struct rx_session*,uint64_t generation,uint64_t id,uint32_t kind,const char*name,uint32_t milestone);
/* User action: authentic App.PostActivityResultsPageDone00742aa0 only.
 * Never invokes CompleteEvent, CompleteSeries, saves or reward application. */
RX_API int rx_session_career_continue(struct rx_session*,uint64_t generation);
/* Explicit cancellation detaches/cancels presentation and requires destroy.
 * It does not claim native EndSeries/save/earned lifecycle completion. */
RX_API int rx_session_career_abort(struct rx_session*,uint64_t generation);
#ifdef __cplusplus
}
#endif
#endif
