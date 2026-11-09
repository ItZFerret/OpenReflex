#pragma once
#include "rx_game.h"
/* Original ECXself, stack flags,RET4; incidental EAXself; fullmask10005. */
rx_addr rx_tdf_owner_delete(rx_addr self,uint32_t flags); /*00539ff0*/
rx_addr rx_owned_memory_stream_delete(rx_addr self,uint32_t flags); /*00810890*/
/* Original ESIself,noargs,RET0,fullmask10005; never clears retained fields. */
void rx_owned_memory_stream_destroy(rx_addr self); /*008108b0*/
/* Original ECXself,noargs,RET0,fullmask10005; frees aligned owned buffer. */
void rx_owned_memory_stream_close(rx_addr self); /*00810960 replacement candidate only*/
