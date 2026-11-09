#pragma once
#include "rx_replay_api.h"
#include "replay_presentation.h"
struct rx_replay_request_node;
typedef struct rx_replay_host_bridge {
 uint64_t generation,next_id,frame;
 uint32_t bound,count;
 struct rx_replay_request_node *head,*tail;
 rx_replay_request last;
} rx_replay_host_bridge;
int rx_replay_host_begin(rx_replay_host_bridge*,uint64_t generation);
void rx_replay_host_end(rx_replay_host_bridge*);
void rx_replay_host_frame(rx_replay_host_bridge*,uint64_t frame);
int rx_replay_host_next(rx_replay_host_bridge*,uint64_t generation,rx_replay_request*);
int rx_replay_host_peek(rx_replay_host_bridge*,uint64_t generation,uint32_t index,rx_replay_request*);
int rx_replay_host_ack(rx_replay_host_bridge*,uint64_t generation,const rx_replay_request*,uint32_t milestone);
