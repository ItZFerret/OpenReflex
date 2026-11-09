/* phys_coll_misc.h -- small container and vector helpers of the collision code (MSVC std::vector / checked iterator internals, a hash
 * table lookup, an index mapping and a plane clip). The vector insert / grow pair keeps the original's checked-iterator
 * _invalid_parameter_noinfo tests. */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 008423d0 FUN_008423d0 (EDI vector, stack value*; RET 4): vector<T*>::push_back (begin +0xc, end +0x10, capacity end +0x14); returns EAX */
uint32_t phys_vector_push_back(rx_addr vec, rx_addr value_p);
/* 0080d280 FUN_0080d280 (EAX vector; stack result iterator*, value*, _Where proxy, _Where position; RET 0x10):
 * vector<T*>::_Insert_n (checked iterator tests, then _Reallocate); returns the result iterator buffer */
rx_addr phys_vector_insert_n(rx_addr vec, rx_addr out_it, rx_addr value_p, rx_addr proxy, rx_addr where);
/* 0080d860 FUN_0080d860 (stack vector, value*, proxy, position; RET 0x10): the exe's vector grow / in-place insert helper
 * (capacity + capacity / 2, ReflexAlignedAlloc, moves, free; or memmove_s + fill) */
void phys_vector_reallocate(rx_addr vec, rx_addr value_p, rx_addr proxy, rx_addr where);
/* 00842fa0 FUN_00842fa0 (ESI iterator {container proxy +0, position +4}, stack n; RET 4): iterator += n elements of 4 bytes */
void phys_iterator_advance(rx_addr it, int32_t n);
/* 0084fe70 FUN_0084fe70 (ESI object, stack a, b; RET 8): the index of the pair (a, b) among the unordered pairs of the object's items, clamped to the count */
uint32_t phys_pair_index(rx_addr obj, uint32_t a, uint32_t b);
/* 0084fe00 FUN_0084fe00 (ECX plane, stack v*; RET 4): removes the component of v along the plane normal (+0x10) when the plane is set (+0x00) and v is below it */
void phys_plane_clip(rx_addr plane, rx_addr v);
/* 00690e00 FUN_00690e00 (ESI hash table, EAX key*, stack out*; RET 4): looks the int key up; returns 1 and stores the value, else 0 */
uint32_t phys_hash_lookup(rx_addr table, rx_addr key_p, rx_addr out_p);
/* 007cf450 FUN_007cf450 (ESI vector, stack out*, ., first, ., last; RET 0x14): vector::erase(first, last) of 4-byte items; fills *out with the iterator; returns out */
rx_addr phys_vector_erase(rx_addr vec, rx_addr out, rx_addr first_pos, rx_addr last_pos);
#ifdef __cplusplus
}
#endif
