// world_phys.cpp -- oracle adapters for core/game/world_phys.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// 00769b80 (EAX = world, stack dt; RET 4).
RX_PORT(0x00769b80, World_PhysicsUpdate, "rx_world", 0) {
    rx_world_physics_update(m.r[0], f32(m.arg(0)));
    return m.ret(4);
}
// 007699e0 (EDI = world, stack dt; RET 4). ECX comes back from its push / pop slot: the entry ECX.
RX_PORT(0x007699e0, World_PreListeners, "rx_world", OUT_ECX) {
    const uint32_t ecx = m.r[1];
    rx_world_pre_listeners(m.r[7], f32(m.arg(0)));
    m.r[1] = ecx;
    return m.ret(4);
}
// 00769ab0 (ESI = world, stack dt; RET 4).
RX_PORT(0x00769ab0, World_PostListeners, "rx_world", 0) {
    rx_world_post_listeners(m.r[6], f32(m.arg(0)));
    return m.ret(4);
}
// 0076a010 (EAX = world, stack dt; RET 4).
RX_PORT(0x0076a010, World_SolverDispatch, "rx_world", 0) {
    rx_world_solver_dispatch(m.r[0], f32(m.arg(0)));
    return m.ret(4);
}
// 00768750 (EAX = list, stack world; RET 4).
RX_PORT(0x00768750, HandlerList_CompactAndRun, "rx_world", 0) {
    rx_world_handler_list_compact_and_run(m.r[0], m.arg(0));
    return m.ret(4);
}
// The generic dynamic objects' collision passes (stdcall world; RET 4).
RX_PORT(0x0076bf60, DynObj_CollideAll, "rx_world", 0) { rx_world_dynobj_collide_all(m.arg(0)); return m.ret(4); }
RX_PORT(0x0076d3c0, DynObj_CollideTerrain, "rx_world", 0) { rx_world_dynobj_collide_terrain(m.arg(0)); return m.ret(4); }
RX_PORT(0x0076b4a0, DynObj_CollideStatic, "rx_world", 0) { rx_world_dynobj_collide_static(m.arg(0)); return m.ret(4); }
RX_PORT(0x0076b9b0, DynObj_CollideKinematic, "rx_world", 0) { rx_world_dynobj_collide_kinematic(m.arg(0)); return m.ret(4); }
// Register-convention passes (no stack arguments; RET 0) and the water pass (stdcall world).
RX_PORT(0x0076c510, DynObj_CollideWater, "rx_world", 0) { rx_world_dynobj_collide_water(m.r[6]); return m.ret(0); }
RX_PORT(0x0076ac40, Kinematic_CollideWater, "rx_world", 0) { rx_world_kinematic_collide_water(m.arg(0)); return m.ret(4); }
RX_PORT(0x0076d9a0, Kinematic_CollideTerrain, "rx_world", 0) { rx_world_kinematic_collide_terrain(m.r[6]); return m.ret(0); }
RX_PORT(0x0076b070, Kinematic_CollideStatic, "rx_world", 0) { rx_world_kinematic_collide_static(m.r[7]); return m.ret(0); }
RX_PORT(0x0076cd00, Kinematic_CollideKeyed, "rx_world", 0) { rx_world_kinematic_collide_keyed(m.r[7]); return m.ret(0); }
RX_PORT(0x0076c880, Rider_CollideKeyed, "rx_world", 0) { rx_world_rider_collide_keyed(m.r[7]); return m.ret(0); }
RX_PORT(0x0076a1e0, Kinematic_ResetLost, "rx_world", 0) { rx_world_kinematic_reset_lost(m.r[6]); return m.ret(0); }
// 0076a360 (ECX = world, xmm0 lane 0 = dt; RET 0).
RX_PORT(0x0076a360, Kinematic_Integrate, "rx_world", 0) {
    rx_world_kinematic_integrate(m.r[1], f32(uint32_t(m.xmm[0].lo)));
    return m.ret(0);
}
// 007729f0 (ECX = types out, EDX = shape; stdcall world, pos, results, ids; RET 0x10): EAX = the count.
RX_PORT(0x007729f0, World_QueryShapeContacts, "rx_world", OUT_EAX) {
    const uint32_t n = rx_world_query_shape_contacts(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10, n);
}
// 00770e80 (EDI = list, ECX = query max; stdcall world, box min; RET 8). EAX / EDX end as the broad-phase cursor's last read
// leaves them (its position and its grid); the vehicles' vf62 return that EAX.
RX_PORT(0x00770e80, Vehicle_CollectNearbyStatics, "rx_world", OUT_EAX | OUT_EDX) {
    const uint32_t world = m.arg(0);
    rx_world_vehicle_collect_nearby_statics(m.r[7], m.r[1], world, m.arg(1));
    const uint32_t cursor = U32(world, 0x19cc);
    m.r[2] = U32(cursor, 0x30);
    return m.ret(8, U32(cursor, 0));
}

RX_PORT(0x0076abb0, Kinematic_ResetAuthoredPose, "rx_KinematicReset", 0) {
    rx_world_kinematic_reset_authored_pose(m.r[1], m.r[2]);
    return m.ret(0);
}
