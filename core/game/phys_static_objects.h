/* phys_static_objects.h -- the physics world's static objects (phys_static_objects.c): the vector of 0x40-byte static records (the
 * world's +0x19b4 header: +0xc begin, +0x10 end, +0x18 the broadphase grid object, +0x1c the world tag) and the reference counted
 * tracking records / handles that remove a record when their last reference goes (CollisionEntity, the scene's static assets). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void    rx_static_record_free(rx_addr rec);                                /* 00773e10 (EDI = a tracking record {+0 refs, +4 world, +8 index}) */
void    rx_static_records_remove(rx_addr vec, uint32_t index);             /* 00773a60 (ESI = the vector; stack index; RET 4) */
void    rx_static_records_move_slot(rx_addr vec, uint32_t index, rx_addr src);   /* 007747f0 (stdcall; RET 0xc) */
rx_addr rx_static_record_assign(rx_addr dst, rx_addr src);                 /* 00774d10 (ESI = dst, EDI = src) -> dst */
void    rx_static_body_update_grid(rx_addr vec, int32_t index);            /* 00773ad0 (ESI = the vector, ECX = the index) */
void    rx_static_records_destroy(rx_addr first, rx_addr last);            /* 007772d0 (EAX = first; stack last) */
void    rx_static_handle_release(rx_addr h);                               /* 007735e0 StaticObjectHandle::vf1 (ECX = the handle) */
rx_addr rx_static_handle_sdtor(rx_addr h, uint32_t flags);                 /* 00773db0 StaticObjectHandle::sdtor (thiscall; RET 4) -> this */
rx_addr rx_static_handle_copy(rx_addr first, rx_addr last, rx_addr dest);  /* 008218c0 (cdecl, +3 unused words) -> the end of dest */
void    rx_static_handle_vector_free(rx_addr v);                           /* 0081ff30 (stdcall the vector; RET 4) */
rx_addr rx_static_handle_vector_erase(rx_addr vec, rx_addr out, rx_addr first_proxy, rx_addr first, rx_addr last_proxy, rx_addr last);   /* 0081fe40 (EDI = the vector) */
void    rx_static_handle_vector_clear(rx_addr vec);                        /* 0081f690 (EAX = the vector) */
#ifdef __cplusplus
}
#endif
