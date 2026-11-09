#ifndef RX_REPLAY_API_H
#define RX_REPLAY_API_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RX_REPLAY_API_VERSION 1u
/* Worker-only copied ABI. No guest pointer/callback/managed object is exported. */
typedef struct rx_replay_request {
 uint32_t size,version;
 uint64_t generation,id,frame;
 uint32_t operation,viewport_count,fade_kind,duration_bits,color_mask;
 uint32_t rgba_bits[4],flags;
} rx_replay_request;
typedef struct rx_replay_view {
 uint32_t size,version;
 uint64_t generation,frame;
 uint32_t active,camera_valid,engine_dt_bits,pending_requests;
 rx_camera camera;
 uint32_t reserved;
} rx_replay_view;
enum rx_replay_operation { RX_REPLAY_VIEWPORT=1,RX_REPLAY_FADE=2 };
enum rx_replay_receipt { RX_REPLAY_APPLIED=1,RX_REPLAY_PAINTED=2 };
/* Explicitly enable this real presenter before start; same Career generation.
 * Queued means NOT applied. Another simulation frame is blocked until copied
 * exact request receives APPLIED then PAINTED. Calls remain on session worker. */
RX_API int rx_session_replay_begin(struct rx_session*,uint64_t generation);
/* Exhibition sessions (no career configured): the same presenter for the event of
 * rx_session_event_load. The original plays the end-of-race replay for every race
 * that reaches Post (00569a40 -> 006aa8d0), so a plain event needs the host too.
 * Call it after create and BEFORE rx_session_event_load (before the first frame);
 * it returns the presenter's own generation, which next / peek / ack / view take
 * exactly as a career generation. One per session; a career cannot be configured
 * on it afterwards. rx_session_replay_end_event releases the presenter before the
 * host goes away (unpainted requests are discarded); no frame runs after it. */
RX_API int rx_session_replay_begin_event(struct rx_session*,uint64_t* generation);
RX_API int rx_session_replay_end_event(struct rx_session*,uint64_t generation);
RX_API int rx_session_replay_next(struct rx_session*,uint64_t generation,rx_replay_request*);
/* Enumerate the complete current-frame batch before a single paint; preserves
 * queue order and marks copies delivered, never applies/pops requests. */
RX_API int rx_session_replay_peek(struct rx_session*,uint64_t generation,uint32_t index,rx_replay_request*);
RX_API int rx_session_replay_ack(struct rx_session*,uint64_t generation,const rx_replay_request*,uint32_t milestone);
RX_API int rx_session_replay_view(struct rx_session*,uint64_t generation,rx_replay_view*);
#ifdef __cplusplus
}
#endif
#endif
