#pragma once
#include "rx_career_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RX_CAREER_VEHICLE_VERSION 1u
/*48 bytes; copied IDs only, never guest pointers. Query failure preserves output.*/
typedef struct rx_career_vehicle_identity {
 uint32_t size,version;uint64_t generation;
 int32_t player_row,vehicle_list_id,owner,owner_vehicle_id,model_id,vehicle_type;
 int32_t authored_class,resolved_class;
} rx_career_vehicle_identity;
RX_API int rx_session_career_vehicle(struct rx_session*,uint64_t generation,rx_career_vehicle_identity*);
#ifdef __cplusplus
}
#endif
