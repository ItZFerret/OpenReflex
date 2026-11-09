// anim_scenario_rider.cpp -- oracle adapters for core/game/anim_scenario_rider.c.
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_scenario.h"
}
using namespace reflex_port;

ANIM_PORT(0x00617e10, AnimationEntity_vf29, "rx_anim", OUT_EAX) { m.r[0] = rx_animation_entity_nodes(m.r[1]); return m.ret(0); }
ANIM_PORT(0x006180c0, CharacterEntity_vf28, "rx_anim", OUT_EAX) { m.r[0] = rx_character_entity_root_slot(m.r[1]); return m.ret(0); }
ANIM_PORT(0x006180d0, RiderCharacter_vf22, "rx_anim", OUT_EAX) { m.r[0] = rx_rider_character_type(m.r[1]); return m.ret(0); }
// 00617fd0: EAX = matrix, ECX = the hierarchy (left by the original).
ANIM_PORT(0x00617fd0, CharacterEntity_vf14, "rx_anim", OUT_EAX | OUT_ECX) {
    const uint32_t e = m.r[1]; m.r[0] = rx_character_entity_root_matrix(e); m.r[1] = U32(U32(e, 0x210), 0); return m.ret(0);
}
ANIM_PORT(0x00617ff0, CharacterEntity_vf15, "rx_anim", 0) { rx_character_entity_get_root_matrix(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00618020, CharacterEntity_vf17, "rx_anim", 0) { rx_character_entity_get_root_position(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00618070, CharacterEntity_vf16, "rx_anim", 0) { rx_character_entity_set_root_matrix(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00618100, CharacterEntity_vf14_o90, "rx_anim", 0) { rx_character_entity_reset_nodes(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00624ad0, RiderCharacter_vf19, "rx_anim", 0) { rx_rider_character_animate(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
ANIM_PORT(0x00616f30, RiderParts_SetFloat, "rx_anim", 0) { rx_rider_parts_set_float(m.r[0], rx_oracle::f32(m.arg(0))); return m.ret(4); }
ANIM_PORT(0x0061d060, RiderAnimation_KickJobs, "rx_anim", 0) { rx_rider_animation_kick_jobs(); return m.ret(0); }
ANIM_PORT(0x0061d130, RiderAnimation_WaitJobs, "rx_anim", 0) { rx_rider_animation_wait_jobs(); return m.ret(0); }
ANIM_PORT(0x0061c5f0, RiderAnimation_ResetStunt, "rx_anim", 0) { rx_rider_animation_reset_stunt(m.r[0]); return m.ret(0); }
ANIM_PORT(0x006215e0, RiderAnimation_SetMotionOutputs, "rx_anim", 0) { rx_rider_animation_set_motion_outputs(m.r[0]); return m.ret(0); }
ANIM_PORT(0x00621640, RiderAnimation_Stance2Outputs, "rx_anim", 0) { rx_rider_animation_stance2_outputs(m.r[0]); return m.ret(0); }
ANIM_PORT(0x00621720, RiderAnimation_SetStance, "rx_anim", 0) { rx_rider_animation_set_stance(m.r[7], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00621d70, RiderAnimation_Stop, "rx_anim", 0) { rx_rider_animation_stop(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00619050, RiderAnimation_ResetPose, "rx_anim", 0) { rx_rider_animation_reset_pose(m.r[7]); return m.ret(0); }
ANIM_PORT(0x00623500, RiderAnimation_Restart, "rx_anim", 0) { rx_rider_animation_restart(m.r[6]); return m.ret(0); }
ANIM_PORT(0x006248c0, RiderCharacter_ReleaseAnimation, "rx_anim", 0) { rx_rider_character_release_animation(m.r[7]); return m.ret(0); }
ANIM_PORT(0x006b9b20, AnimSingleLoop_ScalarDeletingDestructor, "rx_anim", OUT_EAX) { m.r[0] = rx_anim_single_loop_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00624930, RiderCharacter_Destroy, "rx_anim", 0) { rx_rider_character_destroy(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00624870, RiderCharacter_sdtor, "rx_anim", OUT_EAX) { m.r[0] = rx_rider_character_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00612c50, RiderList_Splice, "rx_anim", 0) { rx_rider_list_splice(m.r[1], m.r[4] + 4); return m.ret(0x24); }
ANIM_PORT(0x00612850, RiderList_Construct, "rx_anim", OUT_EAX) { m.r[0] = rx_rider_list_construct(m.r[1]); return m.ret(4); }
ANIM_PORT(0x00612890, RiderList_Swap, "rx_anim", 0) { rx_rider_list_swap(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00612a00, RiderList_SpliceAll, "rx_anim", 0) { rx_rider_list_splice_all(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
ANIM_PORT(0x00612aa0, RiderList_Merge, "rx_anim", 0) { rx_rider_list_merge(m.r[1], m.arg(0)); return m.ret(4); }
// The std::list<int> helpers of the rider lists (register conventions; the registers they leave are set as the original does).
// 005c0470: ECX = list, EAX = iterator out. ECX = the proxy, EDX = the first node afterwards.
ANIM_PORT(0x005c0470, RiderList_Begin, "rx_anim", 0) {
    rx_rider_list_begin(m.r[1], m.r[0]);
    m.r[1] = rx_rd32(m.r[0]); m.r[2] = rx_rd32(m.r[0] + 4);
    return m.ret(0);
}
// 005c0540: EDI = list, EAX = head node, ESI = iterator out -> EAX = out; ECX = the proxy.
ANIM_PORT(0x005c0540, RiderList_End, "rx_anim", OUT_EAX) {
    m.r[0] = rx_rider_list_end(m.r[7], m.r[0], m.r[6]);
    m.r[1] = rx_rd32(m.r[7]);
    return m.ret(0);
}
// 005bb950: EDI = a, ESI = b -> AL = a != b (the upper bytes of EAX: a's node word); ECX = the flag.
ANIM_PORT(0x005bb950, RiderList_IterNe, "rx_anim", OUT_EAX) {
    m.r[0] = rx_rider_list_iter_ne(m.r[7], m.r[6]);
    m.r[1] = m.r[0] & 1;
    return m.ret(0);
}
// 00706600: ESI = iterator -> EAX = the iterator; ECX / EDX = the node before the increment.
ANIM_PORT(0x00706600, RiderList_IterNext, "rx_anim", OUT_EAX) {
    const uint32_t old = rx_rd32(m.r[6] + 4);
    m.r[0] = rx_rider_list_iter_next(m.r[6]);
    m.r[1] = old; m.r[2] = old;
    return m.ret(0);
}
// 005c0900: ECX = count, EDX = list -> EAX = the new size.
ANIM_PORT(0x005c0900, RiderList_GrowCheck, "rx_anim", OUT_EAX) { m.r[0] = rx_rider_list_grow_check(m.r[1], m.r[2]); return m.ret(0); }
// 00594080: ESI = list; stdcall, one dead argument -> EAX = the list.
ANIM_PORT(0x00594080, RiderList_ProxyConstruct, "rx_anim", OUT_EAX) { m.r[0] = rx_rider_list_proxy_construct(m.r[6]); return m.ret(4); }
// 00842df0: cdecl -> EAX = the head node.
ANIM_PORT(0x00842df0, RiderList_HeadAlloc, "rx_anim", OUT_EAX) { m.r[0] = rx_rider_list_head_alloc(); return m.ret(0); }
// 006084d0: cdecl (a, b).
ANIM_PORT(0x006084d0, RiderSwapDwords, "rx_anim", 0) { rx_rider_swap_dwords(m.arg(0), m.arg(1)); return m.ret(0); }
// 005f3520: ESI = other, EDI = self.
ANIM_PORT(0x005f3520, RiderList_SwapProxies, "rx_anim", 0) { rx_rider_list_swap_proxies(m.r[6], m.r[7]); return m.ret(0); }
// 006893b0 (ESI = the tree): the map emptied and its blocks freed. 00705150 (EAX = the map, out iterator*, begin / end; RET 0x14)
ANIM_PORT(0x006893b0, RiderTree_Release, "rx_anim", 0) { rx_rider_tree_release(m.r[6]); return m.ret(0); }
ANIM_PORT(0x00705150, StlMap_EraseRange, "rx_anim", 0) {
    rx_stl_map_erase_range(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14);
}
