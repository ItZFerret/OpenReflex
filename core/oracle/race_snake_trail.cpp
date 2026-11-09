// race_snake_trail.cpp -- oracle adapters for core/game/race_snake_trail.c (group rx_party): the Snake party mode's trails, the
// activity's spatial hash of their collision records, the trail meshes (XenonSnakeVisibleMesh) and their shared GPU resources.
#include "race_oracle.h"
extern "C" {
#include "../game/race_snake_trail.h"
}
using namespace reflex_port;

namespace {
struct TrailFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit TrailFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~TrailFpuKeep() { m.fpu = fpu; } };
inline float argf(reflex_vm::Machine& m, int i) { return rx_oracle::f32(m.arg(i)); }
inline void set_al(reflex_vm::Machine& m, uint32_t v) { m.r[0] = (m.r[0] & 0xffffff00u) | (v & 0xffu); }
}

// ---- pools and the spatial hash ----
RX_PORT(0x006a2650, SnakeHash_PoolInit, "rx_party", OUT_EAX) { m.r[0] = rx_snake_hash_pool_init(m.r[6]); return m.ret(0); }
RX_PORT(0x006a24f0, SnakeSegment_PoolInit, "rx_party", OUT_EAX) { m.r[0] = rx_snake_segment_pool_init(m.r[6]); return m.ret(0); }
RX_PORT(0x006a25a0, SnakeChain_PoolInit, "rx_party", OUT_EAX) { m.r[0] = rx_snake_chain_pool_init(m.r[6]); return m.ret(0); }
RX_PORT(0x006a2600, SnakeChain_Take, "rx_party", OUT_EAX) { m.r[0] = rx_snake_chain_take(m.r[2]); return m.ret(0); }
RX_PORT(0x006a2550, SnakeSegment_Take, "rx_party", OUT_EAX) { m.r[0] = rx_snake_segment_take(m.r[2]); return m.ret(0); }
RX_PORT(0x006a21d0, SnakeHash_Init, "rx_party", 0) { rx_snake_hash_init(m.r[0]); return m.ret(0); }
RX_PORT(0x006a26b0, SnakeHash_Clear, "rx_party", 0) { rx_snake_hash_clear(m.r[2]); return m.ret(0); }
RX_PORT(0x0069b8e0, SnakeHash_Key, "rx_party", OUT_EAX) { m.r[0] = rx_snake_hash_key(m.r[1]); return m.ret(0); }
RX_PORT(0x006a24a0, SnakeHash_Find, "rx_party", OUT_EAX) { m.r[0] = rx_snake_hash_find(m.r[6], m.r[7]); return m.ret(0); }
RX_PORT(0x006a2280, SnakeHash_Insert, "rx_party", OUT_EAX) { m.r[0] = rx_snake_hash_insert(m.r[0], m.r[6]); return m.ret(0); }
RX_PORT(0x006a23b0, SnakeHash_Remove, "rx_party", 0) { rx_snake_hash_remove(m.r[0], m.r[6]); return m.ret(0); }
RX_PORT(0x0069b9b0, Snake_Cell, "rx_party", OUT_EAX) { TrailFpuKeep k(m); race_check_pc53(m, "0069b9b0"); m.r[0] = rx_snake_cell(m.r[7], m.r[6]); return m.ret(0); }
RX_PORT(0x0069bab0, SnakeHash_Add, "rx_party", 0) { TrailFpuKeep k(m); race_check_pc53(m, "0069bab0"); rx_snake_hash_add(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0069bb20, SnakeHash_Del, "rx_party", 0) { TrailFpuKeep k(m); race_check_pc53(m, "0069bb20"); rx_snake_hash_del(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0069bba0, SnakeHash_Hit, "rx_party", OUT_EAX) {
    TrailFpuKeep k(m); race_check_pc53(m, "0069bba0");
    m.r[0] = rx_snake_hash_hit(m.arg(0), m.arg(1), argf(m, 2), m.arg(3), m.arg(4)); return m.ret(0x14);
}

// ---- the shape's profile callbacks (cdecl) ----
RX_PORT(0x0069bcf0, SnakeShape_Side, "rx_party", 0) { rx_snake_shape_side(m.arg(0), argf(m, 1)); return m.ret(0); }
RX_PORT(0x0069bf00, SnakeShape_Cap, "rx_party", 0) { rx_snake_shape_cap(m.arg(0), argf(m, 1)); return m.ret(0); }
RX_PORT(0x0069c000, SnakeShape_Outline, "rx_party", 0) { rx_snake_shape_outline(m.arg(0), argf(m, 1)); return m.ret(0); }

// ---- the mesh's triangle ring ----
RX_PORT(0x006a0e70, SnakeMesh_CapFront, "rx_party", 0) { TrailFpuKeep k(m); set_al(m, rx_snake_mesh_cap_front(m.r[6], m.r[7], m.arg(0), argf(m, 1))); return m.ret(8); }
RX_PORT(0x006a1030, SnakeMesh_CapEnd, "rx_party", 0) { TrailFpuKeep k(m); set_al(m, rx_snake_mesh_cap_end(m.r[6], m.r[7], m.arg(0), argf(m, 1))); return m.ret(8); }
RX_PORT(0x006a1510, SnakeMesh_CapTail, "rx_party", 0) { TrailFpuKeep k(m); set_al(m, rx_snake_mesh_cap_tail(m.r[6], m.r[7], m.arg(0), argf(m, 1))); return m.ret(8); }
RX_PORT(0x006a11f0, SnakeMesh_Sides, "rx_party", 0) {
    TrailFpuKeep k(m); set_al(m, rx_snake_mesh_sides(m.r[6], m.r[7], m.arg(0), argf(m, 1), m.arg(2), argf(m, 3))); return m.ret(0x10);
}
RX_PORT(0x006a14e0, SnakeMesh_UncapHead, "rx_party", 0) { rx_snake_mesh_uncap_head(m.r[1], m.r[2]); return m.ret(0); }
RX_PORT(0x006a16c0, SnakeMesh_DropTail, "rx_party", 0) { rx_snake_mesh_drop_tail(m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x006a1700, SnakeMesh_DropSides, "rx_party", 0) { rx_snake_mesh_drop_sides(m.r[0], m.r[1]); return m.ret(0); }

// ---- collision records ----
RX_PORT(0x0069b530, Snake_QuadPlanes, "rx_party", 0) { TrailFpuKeep k(m); rx_snake_quad_planes(m.r[1], m.r[2], m.r[7], m.arg(0), m.arg(1), m.r[6]); return m.ret(8); }
RX_PORT(0x0069b450, SnakeRecord_Build, "rx_party", OUT_EAX) {
    TrailFpuKeep k(m);
    m.r[0] = rx_snake_record_build(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), m.arg(6)); return m.ret(0x1c);
}
RX_PORT(0x0069fb00, SnakeRecord_Copy, "rx_party", 0) { TrailFpuKeep k(m); rx_snake_record_copy(m.r[1], m.r[0]); return m.ret(0); }

// ---- the trail ----
RX_PORT(0x006a0470, SnakeTrail_FreeOldest, "rx_party", 0) { TrailFpuKeep k(m); race_check_pc53(m, "006a0470"); rx_snake_trail_free_oldest(m.r[1]); return m.ret(0); }
RX_PORT(0x0069fcd0, SnakeTrail_Close, "rx_party", 0) { TrailFpuKeep k(m); race_check_pc53(m, "0069fcd0"); rx_snake_trail_close(m.arg(0)); return m.ret(4); }
RX_PORT(0x0069ed90, SnakeTrail_Append, "rx_party", 0) {
    TrailFpuKeep k(m); race_check_pc53(m, "0069ed90");
    rx_snake_trail_append(m.r[1], m.arg(0), m.arg(1), m.arg(2), argf(m, 3), argf(m, 4)); return m.ret(0x14);
}
RX_PORT(0x0069ed10, SnakeTrail_NewChain, "rx_party", 0) { TrailFpuKeep k(m); rx_snake_trail_new_chain(m.r[6]); return m.ret(0); }
RX_PORT(0x0069e4c0, SnakeTrail_Update, "rx_party", 0) {
    TrailFpuKeep k(m); race_check_pc53(m, "0069e4c0");
    rx_snake_trail_update(m.r[0], m.arg(0), argf(m, 1), argf(m, 2), argf(m, 3)); return m.ret(0x10);
}
RX_PORT(0x0069e360, SnakeTrail_Refresh, "rx_party", 0) { TrailFpuKeep k(m); rx_snake_trail_refresh(m.r[0]); return m.ret(0); }
RX_PORT(0x0069ec90, SnakeTrail_Reset, "rx_party", 0) { TrailFpuKeep k(m); rx_snake_trail_reset(m.r[0]); return m.ret(0); }

// ---- XenonSnakeVisibleMesh and the trail object ----
RX_PORT(0x006a0b40, SnakeMesh_Construct, "rx_party", OUT_EAX) { TrailFpuKeep k(m); m.r[0] = rx_snake_mesh_construct(m.arg(0), m.arg(1), m.r[0]); return m.ret(8); }
RX_PORT(0x006a0db0, SnakeMesh_Destroy, "rx_party", 0) { rx_snake_mesh_destroy(m.r[6]); return m.ret(0); }
RX_PORT(0x006a0c50, SnakeMesh_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_snake_mesh_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006a2770, SnakeMesh_SdtorO90, "rx_party", OUT_EAX) { m.r[0] = rx_snake_mesh_sdtor_o90(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0069e3a0, SnakeTrail_Construct, "rx_party", OUT_EAX) {
    TrailFpuKeep k(m); m.r[0] = rx_snake_trail_construct(m.arg(0), m.arg(1), m.arg(2), m.r[1], m.r[0]); return m.ret(0xc);
}
RX_PORT(0x0069e460, SnakeTrail_Destroy, "rx_party", 0) { TrailFpuKeep k(m); rx_snake_trail_destroy(m.r[7]); return m.ret(0); }

// ---- the shared GPU resources ----
RX_PORT(0x006a0c70, SnakeMesh_ShadersLoad, "rx_party", 0) { rx_snake_mesh_shaders_load(); return m.ret(0); }
RX_PORT(0x006a0cd0, SnakeMesh_StaticsLoad, "rx_party", 0) { rx_snake_mesh_statics_load(); return m.ret(0); }
RX_PORT(0x006a0d50, SnakeMesh_StaticsRelease, "rx_party", 0) { rx_snake_mesh_statics_release(); return m.ret(0); }
RX_PORT(0x007b0ee0, Gpu_ReleaseRecord, "rx_party", 0) { rx_gpu_release_record(m.arg(0)); return m.ret(0); }
