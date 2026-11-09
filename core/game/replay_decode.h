#ifndef RX_REPLAY_DECODE_H
#define RX_REPLAY_DECODE_H
#include "phys_math.h"
/* Original packed low four uint16 lanes in XMM0; float4 result in XMM0. */
rx_v4 rx_replay_half4_decode(rx_v4 packed);
typedef struct replay_sample { rx_v4 p, v, q, w; } replay_sample;
replay_sample rx_replay_decode_record(rx_addr sample);
#endif
