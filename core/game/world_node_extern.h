/* world_node_extern.h -- the node share's calls into functions it does not own and that are not in the core yet: each runs the
 * original through rx_call_original_ex (oracle build) until its owner ports it. One wrapper per callee. */
#pragma once
#include "world_util.h"
#include "anim.h"

/* 007deaa0 NodeSet_NextUnvisited (EDI = node set): the set's traversal cursor (+4) record, marked visited (bit 1 of +0xc), the
 * cursor advanced to the next unvisited record (first child +0xe, next sibling +0xd, else up the parents +0xf); EAX = the
 * record, or 0 when the cursor is empty. */
static inline rx_addr world_node_set_next_unvisited(rx_addr set, world_ctx c) {
    (void)c;
    return rx_nodeset_next_unvisited(set);   /* ported by the anim unit (anim_node.c); was WORLD_CTX0(0x007deaa0) -- asset loaders (build_model.c) need it in the product */
}
