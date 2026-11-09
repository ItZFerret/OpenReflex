#pragma once
#include <stdint.h>
#include "rx_career_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Copied fixed version 2: no guest pointers, host pointers or table handles. */
#define RX_CAREER_MOTO_VERSION 2u
#define RX_CAREER_MOTO_ROWS 12u
#define RX_CAREER_MOTO_GATES 16u
typedef struct {int32_t row,player_type,player_state,score_one,score_two;float sort_value;char name[128];} rx_career_moto_row;
typedef struct {uint32_t size,version;uint64_t generation;uint32_t series_id,sub_event,phase,next_phase,post_complete,row_count,gate_active,gate_cursor,gate_count;int32_t device;uint32_t first_continue_submitted,first_post_observed;int32_t gates[16];rx_career_moto_row rows[12];} rx_career_moto_status;
#ifdef __cplusplus
static_assert(sizeof(rx_career_moto_row)==152,"Moto row ABI");
static_assert(sizeof(rx_career_moto_status)==1952,"Moto status ABI");
#else
_Static_assert(sizeof(rx_career_moto_row)==152,"Moto row ABI");
_Static_assert(sizeof(rx_career_moto_status)==1952,"Moto status ABI");
#endif
/* Commands are canonical page actions, never generic accepted acknowledgements.
 * expected cursor/sub-event/phase defeats delayed buttons after a world phase moves. */
enum { RX_MOTO_GATE_BEGIN=1,RX_MOTO_GATE_SELECT=2,RX_MOTO_NEXT=3,RX_MOTO_FIRST_CONTINUE=4 };
RX_API int rx_session_career_moto_practice_choice(struct rx_session*,uint64_t,uint32_t);
RX_API int rx_session_career_moto_status(struct rx_session*,uint64_t,rx_career_moto_status*);
RX_API int rx_session_career_moto_action(struct rx_session*,uint64_t,uint64_t,uint32_t,uint32_t,uint32_t,uint32_t,int32_t);
#ifdef __cplusplus
}
#endif
