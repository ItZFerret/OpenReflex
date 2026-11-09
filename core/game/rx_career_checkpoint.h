#pragma once
#include "rx_career_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Owner-thread host receipt ONLY AFTER durable CAS succeeds. Exact delivered
 * checkpoint id/controller/generation required. Generic accepted ack stays unsupported.
 * Native validates request identity; it does not verify host disk durability. */
RX_API int rx_session_career_checkpoint_committed(struct rx_session*,uint64_t generation,uint64_t id);
/* Visible failure, request remains pending; not a rejected/durable acknowledgement. */
RX_API int rx_session_career_checkpoint_failed(struct rx_session*,uint64_t generation,uint64_t id);
#ifdef __cplusplus
}
#endif
