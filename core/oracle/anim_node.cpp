// anim_node.cpp -- oracle adapters for core/game/anim_node.c.
#include "anim_oracle.h"
extern "C" {
#include "../game/anim.h"
}
using namespace reflex_port;

// 007dcdf0 (ESI = record, arg 0 = quaternion; RET 4). Leaves EAX = matrix index * 0x40, ECX = hierarchy, EDX = its matrices.
ANIM_PORT(0x007dcdf0, Node_SetLocalRotation, "rx_anim", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t rec = m.r[6];
    rx_node_set_local_rotation(rec, m.arg(0));
    m.r[0] = U8(rec, 0xb) * 0x40u; m.r[1] = U32(rec, 0); m.r[2] = U32(m.r[1], 0x18);
    return m.ret(4);
}
// 007dd160 Node_SetLocalTranslation (ECX = record, stack vec3; RET 4). Void.
ANIM_PORT(0x007dd160, Node_SetLocalTranslation, "rx_anim", 0) {
    rx_node_set_local_translation(m.r[1], m.arg(0));
    return m.ret(4);
}
// 007dd380 Node_LocalRotation (EAX = record, ESI = out). Void (ECX preserved by the original's push / pop).
ANIM_PORT(0x007dd380, Node_LocalRotation, "rx_anim", 0) { const uint32_t c = m.r[1]; rx_node_local_rotation(m.r[0], m.r[6]); m.r[1] = c; return m.ret(0); }
// 007deaa0 NodeSet_NextUnvisited (EDI = walk). EAX = the record walked (0 at the end).
ANIM_PORT(0x007deaa0, NodeSet_NextUnvisited, "rx_anim", OUT_EAX) { m.r[0] = rx_nodeset_next_unvisited(m.r[7]); return m.ret(0); }
// 007dd930 NodeSet_CopyFrom (ESI = destination, EDI = source). Void.
ANIM_PORT(0x007dd930, NodeSet_CopyFrom, "rx_anim", 0) { rx_nodeset_copy_from(m.r[6], m.r[7]); return m.ret(0); }
// 007de980 Hierarchy_HasNode (ESI = hierarchy, stack name; RET 4). EAX = 0 / 1.
ANIM_PORT(0x007de980, Hierarchy_HasNode, "rx_anim", OUT_EAX) { m.r[0] = rx_hierarchy_has_node(m.r[6], m.arg(0)); return m.ret(4); }
// 007de9e0 Hierarchy_FindNode (EDI = hierarchy, stack name; RET 4). EAX = record or 0.
ANIM_PORT(0x007de9e0, Hierarchy_FindNode, "rx_anim", OUT_EAX) { m.r[0] = rx_hierarchy_find_node(m.r[7], m.arg(0)); return m.ret(4); }
// 007df2e0 Octree_ReplaceItem (thiscall new, old; RET 8). Void.
ANIM_PORT(0x007df2e0, Octree_ReplaceItem, "rx_anim", 0) { rx_octree_replace_item(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 007dbde0 Node_UpdateSubtreeBounds (ESI = record). Void.
ANIM_PORT(0x007dbde0, Node_UpdateSubtreeBounds, "rx_anim", 0) { rx_node_update_subtree_bounds(m.r[6]); return m.ret(0); }
// 007dd450 Node_SubtreeBounds (EAX = record, EDI = extents out; stack center out, radius out; RET 8). Void.
ANIM_PORT(0x007dd450, Node_SubtreeBounds, "rx_anim", 0) { rx_node_subtree_bounds(m.r[0], m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
// 007dd4d0 (EDI = the node set): its element array and second block freed.
ANIM_PORT(0x007dd4d0, NodeSet_Release, "rx_anim", 0) { rx_nodeset_release(m.r[7]); return m.ret(0); }
