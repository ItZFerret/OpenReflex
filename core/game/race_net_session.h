/* race_net_session.h -- the online session's small helpers of the race layer (race_net_session.c): the player snapshot ring's copies,
 * the replicated player's apply / flag / event, the session recorder, the Quazal reference handle and the selection iterator
 * constructors. Only reached online: the product has no Quazal session (see the .c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void     rx_net_snapshots_store(rx_addr net);                          /* 00700a70 (EAX = the network object) */
rx_addr  rx_net_snapshot_load(rx_addr holder);                         /* 00700e70 (ESI = the holder (a pointer to the network object)) -> EAX = the network object */
void     rx_net_player_apply(rx_addr player_net);                      /* 006fe870 (EDI = the player's network object O) */
void     rx_net_player_set_flag(rx_addr player_net, uint32_t flag);    /* 006fe9c0 (EAX = O, CL = flag) */
void     rx_net_send_event(rx_addr player_net, uint32_t a, uint32_t b, uint32_t c);   /* 006ff8d0 (EDI = O; stdcall a, b, c; RET 0xc) */
rx_addr  rx_quazal_handle_init(rx_addr handle, rx_addr object);        /* 0088bcd0 (ECX = the handle; stdcall object; RET 4) -> EAX = the handle */
void     rx_quazal_handle_release(rx_addr handle);                     /* 0088bac0 (ECX = the handle) */
uint32_t rx_quazal_ref_ready(rx_addr handle);                          /* 00885050 (ECX = the handle) -> AL */
void     rx_net_session_record_state(uint32_t state);                  /* 006f7810 (cdecl state byte) */
rx_addr  rx_net_selection_begin_all(rx_addr it);                       /* 005544c0 (ESI = the iterator) -> EAX = it */
rx_addr  rx_net_selection_begin_ready(rx_addr it);                     /* 00585b20 (ESI = the iterator) -> EAX = it */
void     rx_net_selection_next(rx_addr it);                            /* 00508880 (ECX = the iterator) */
void     rx_net_selection_advance(rx_addr it);                         /* 00508800 (ECX = the iterator): its handle (it + 4) set to the element after the current key (it + 0x14) */
void     rx_net_selection_successor(rx_addr it, rx_addr out, uint32_t key);   /* 00508770 (ECX = the iterator; stdcall out, key; RET 8) */
void     rx_net_set_lower_bound(rx_addr set, rx_addr out, rx_addr key); /* 0053a170 (ECX = a std::set<uint>; stdcall out iterator, &key; RET 8) */
#ifdef __cplusplus
}
#endif
