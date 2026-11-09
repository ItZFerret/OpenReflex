/* build_vehicle_tree.h -- the MSVC 9 std::map / std::set red-black tree the vehicle construction code instantiates (internal to
 * game/build_vehicle*.c). A tree object: +0 the container proxy, +0x18 the head node (+0 leftmost, +4 root, +8 rightmost), +0x1c
 * the size. A node: +0 left, +4 parent, +8 right, +0xc the value (the key first), then the colour byte (0 red, 1 black) and the
 * isnil byte at the instantiation's offsets. Hand transliterations of one instantiation each, generic over those offsets:
 *   _Insert (00705de0 uint map / 00544840 string map), the rotations (00690540 / 00705020, 0041bb70 / 0041bbe0), --it (006909c0 /
 *   00544b90), ++it (00690280 / 0041bc30), insert(value) (00774f60 / 005446d0), insert(hint, value) (00773e80 / 00544480). */
#pragma once
#include "rx_game.h"

typedef struct vt_tree {
    uint32_t col;                                    /* the colour byte's offset (the isnil byte follows) */
    uint32_t max_size;                               /* max_size(): _Insert throws length_error at or above it */
    int (*less)(rx_addr a, rx_addr b);               /* key a < key b (a value's key / a node + 0xc) */
    rx_addr (*make)(rx_addr head, rx_addr where, rx_addr val);   /* _Buynode(head, where, head, val) (the colours cleared) */
} vt_tree;

static inline uint32_t vt_nil(const vt_tree* t, rx_addr n) { return U8(n, t->col + 1); }
rx_addr vt_prev(const vt_tree* t, rx_addr n);                                  /* --it */
rx_addr vt_next(const vt_tree* t, rx_addr n);                                  /* ++it */
rx_addr vt_insert_at(const vt_tree* t, rx_addr m, rx_addr where, uint32_t add_left, rx_addr val);   /* _Insert -> the node */
/* _Insert with the stack words the original's leaves (dead stack residue that later uninitialized locals copy): e = its return
 * slot, the callee-saved registers at the call, make = _Buynode with its frames (b = _Buynode's return slot) */
typedef struct vt_frame {
    rx_addr e;
    uint32_t ebx, ebp, esi, edi;
    rx_addr (*make)(rx_addr b, const struct vt_frame* f, rx_addr head, rx_addr where, rx_addr val);
} vt_frame;
rx_addr vt_insert_at_fr(const vt_tree* t, rx_addr m, rx_addr where, uint32_t add_left, rx_addr val, const vt_frame* f);
rx_addr vt_insert_unique(const vt_tree* t, rx_addr m, rx_addr val);           /* insert(value) -> the node (new or existing) */
rx_addr vt_insert_hint(const vt_tree* t, rx_addr m, rx_addr val, rx_addr where);   /* insert(hint, value) -> the node */
rx_addr vt_erase(const vt_tree* t, rx_addr m, rx_addr node, void (*dtor)(rx_addr node));   /* erase(it) -> the next node */
