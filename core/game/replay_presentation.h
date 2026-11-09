#pragma once
#include "rx_game.h"
typedef struct rx_replay_presentation {
    void *context;
    uint64_t generation;
    /* 0 means copied request accepted by the session bridge, NOT rendered.
     * Session advance stays gated until matching applied+painted receipts. */
    int (*viewport)(void*,uint64_t,rx_addr,int32_t);
    /* kind2 keeps current RGB (NULL); kind3 supplies original new RGB/alpha. */
    int (*fade)(void*,uint64_t,uint32_t,float,const uint32_t*);
} rx_replay_presentation;
int rx_replay_presentation_install(const rx_replay_presentation *host);
void rx_replay_presentation_clear(void);
