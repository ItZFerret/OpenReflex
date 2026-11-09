#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RX_CAREER_HOST_QUEUE_CAPACITY 32u
#define RX_CAREER_HOST_TEXT_CAPACITY 256u
typedef enum rx_career_host_kind {
 RX_CAREER_HOST_UI_EVENT=1,RX_CAREER_HOST_UI_ROUTE,RX_CAREER_HOST_HUB,
 RX_CAREER_HOST_FADE_OUT,RX_CAREER_HOST_PAGE_WIPE,RX_CAREER_HOST_CHECKPOINT
} rx_career_host_kind;
typedef enum rx_career_host_result {
 RX_CAREER_HOST_OK=0,RX_CAREER_HOST_REPEAT,RX_CAREER_HOST_INVALID,
 RX_CAREER_HOST_INACTIVE,RX_CAREER_HOST_FULL,RX_CAREER_HOST_STALE,
 RX_CAREER_HOST_WRONG_KIND,RX_CAREER_HOST_NOT_DELIVERED,
 RX_CAREER_HOST_CONFLICT,RX_CAREER_HOST_BUSY,RX_CAREER_HOST_EXHAUSTED,
 RX_CAREER_HOST_NOT_FOUND,RX_CAREER_HOST_EMPTY,RX_CAREER_HOST_OUT_OF_ORDER
} rx_career_host_result;
typedef enum rx_career_host_wipe_milestone {
 RX_CAREER_HOST_WIPE_NONE=0,
 RX_CAREER_HOST_WIPE_IN_COMPLETE=1,
 RX_CAREER_HOST_WIPE_OUT_COMPLETE=2
} rx_career_host_wipe_milestone;
typedef enum rx_career_host_state {
 RX_CAREER_HOST_PENDING=1,RX_CAREER_HOST_ACCEPTED,RX_CAREER_HOST_REJECTED,
 RX_CAREER_HOST_CANCELLED
} rx_career_host_state;
/* Host pointers are transient enqueue inputs. Names must be readable and
 * NUL-terminated within 256 bytes. Irrelevant payload fields must be zero. */
typedef struct rx_career_host_input {
 uint32_t kind;const char*name;float seconds;uint32_t color,controller;
} rx_career_host_input;
/* Copies only; no guest or managed pointers. IDs are monotonic across begin/end.
 * Token zero means a new occurrence. Nonzero retry identity is guaranteed only
 * while that record remains in this generation's bounded history. */
typedef struct rx_career_host_request {
 uint64_t generation,id,token;uint32_t kind,state,delivered,wipe_milestone;
 float seconds;uint32_t color,controller;char name[RX_CAREER_HOST_TEXT_CAPACITY];
} rx_career_host_request;
/* Worker-confined PERSISTENT HOST storage, outside guest/static restoration.
 * Initialize ONCE for the entire callback/acknowledgement lifetime, not once per
 * rx_session. Reinitializing or replacing this storage loses stale-ack protection.
 * Use begin/end for sessions; expose copied requests through the Results path. */
typedef struct rx_career_host_queue {
 uint64_t generation,next_id;uint32_t active,first,count;
 rx_career_host_request records[RX_CAREER_HOST_QUEUE_CAPACITY];
} rx_career_host_queue;
void rx_career_host_queue_init(rx_career_host_queue*q);
rx_career_host_result rx_career_host_queue_begin(rx_career_host_queue*q,uint64_t*generation);
/* end refuses pending requests. abort cancels them; neither acknowledges them. */
rx_career_host_result rx_career_host_queue_end(rx_career_host_queue*q);
rx_career_host_result rx_career_host_queue_abort(rx_career_host_queue*q);
rx_career_host_result rx_career_host_queue_enqueue(rx_career_host_queue*q,
 uint64_t generation,uint64_t retry_token,const rx_career_host_input*input,uint64_t*id);
/* Returns the oldest pending copy; repeated reads return the same delivered ID. */
rx_career_host_result rx_career_host_queue_next(rx_career_host_queue*q,
 uint64_t generation,rx_career_host_request*out);
rx_career_host_result rx_career_host_queue_ack(rx_career_host_queue*q,
 uint64_t generation,uint64_t id,uint32_t kind,uint32_t accepted);
/* PageWipe cannot be accepted by generic ack. Explicit rejection is permitted
 * before In; after In, finish Out or abort the session. In remains pending and
 * delivered, so next returns the SAME request carrying its new milestone.
 * A new milestone is committed before the caller synchronously emits its event.
 * out is an independent copy, safe if that event enqueues and evicts history. */
rx_career_host_result rx_career_host_queue_wipe_milestone(rx_career_host_queue*q,
 uint64_t generation,uint64_t id,uint32_t kind,const char*name,
 uint32_t milestone,rx_career_host_request*out);
size_t rx_career_host_queue_pending(const rx_career_host_queue*q);
#ifdef __cplusplus
}
#endif
