// fx_types.cpp -- oracle adapters for core/game/fx_types.c (group "rx_fx").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx.h"
}
using namespace reflex_port;
using rx_oracle::f32;


FX_PORT(0x00402be0, Static_00402be0, OUT_EAX) { m.r[0] = rx_fx_static_9e30e4_construct(); return m.ret(0); }
FX_PORT(0x00402b10, Static_00402b10, OUT_EAX) { m.r[0] = rx_fx_static_9e30e4(); return m.ret(0); }
FX_PORT(0x00413d20, SeriesManager_00413d20, OUT_EAX) { m.r[0] = rx_fx_series_manager(); return m.ret(0); }
FX_PORT(0x004022d0, VectorCtorIterator_004022d0, 0) { rx_fx_vector_ctor_iterator(m.arg(0), m.arg(1), int32_t(m.arg(2)), m.arg(3)); return m.ret(0x10); }
FX_PORT(0x00402500, VectorDtorIterator_00402500, 0) { rx_fx_vector_dtor_iterator(m.arg(0), m.arg(1), int32_t(m.arg(2)), m.arg(3)); return m.ret(0x10); }
FX_PORT(0x006c8230, FillDwords_006c8230, OUT_EAX | OUT_ECX) {
    const uint32_t n = m.r[6], d = m.r[7]; m.r[0] = rx_fx_fill_dwords(n, d, m.r[2]); m.r[1] = d + 4 * n; return m.ret(0);
}
FX_PORT(0x006cb180, StuntComboStart_006cb180, OUT_EAX | OUT_ECX) { rx_fx_stunt_combo_start(m.r[2]); m.r[0] = 0; m.r[1] = 0; return m.ret(0); }
FX_PORT(0x006ccf60, StuntAdd30_006ccf60, OUT_ECX | OUT_EDX | OUT_XMM0) {
    const uint32_t P = m.r[0];
    m.r[1] = U32(P, 0x128); m.r[2] = U32(m.r[1], 0x94);
    if (rx_fx_stunt_add_30(P, f32(m.arg(0)))) { m.xmm[0].lo = U32(P, 0x30); m.xmm[0].hi = 0; }
    return m.ret(4);
}
FX_PORT(0x0041b410, PackageQueueDrain_0041b410, 0) { rx_fx_package_queue_drain(m.arg(0)); return m.ret(4); }
// The package request queue (the database manager 0x00d72a30).
// 0041b4b0 (ECX = package name characters, EDX = database name characters; stdcall Q; RET 4)
FX_PORT(0x0041b4b0, PackageRequest_0041b4b0, 0) { rx_fx_package_request(m.r[1], m.r[2], m.arg(0)); return m.ret(4); }
// 0041bca0 (EDI = the list; two std::strings by value = 0x38 bytes of arguments; RET 0x38)
FX_PORT(0x0041bca0, PackageQueuePush_0041bca0, 0) { rx_fx_package_queue_push(m.r[7], m.r[4] + 4); return m.ret(0x38); }
// 0041bdd0 (ESI = the node; two std::strings by value; RET 0x38) -> EAX = the node
FX_PORT(0x0041bdd0, PackageNodeConstruct_0041bdd0, OUT_EAX) { m.r[0] = rx_fx_package_node_construct(m.r[6], m.r[4] + 4); return m.ret(0x38); }
// 0041bd10 (ESI = the pending list; stdcall out; RET 4) -> EAX = out
FX_PORT(0x0041bd10, PackageQueuePop_0041bd10, OUT_EAX) { m.r[0] = rx_fx_package_queue_pop(m.arg(0), m.r[6]); return m.ret(4); }
// 005dbf00 (EAX = the destination; stdcall name; RET 4) -> EAX = the destination
FX_PORT(0x005dbf00, LocalizedName_005dbf00, OUT_EAX) { m.r[0] = rx_fx_localized_name(m.r[0], m.arg(0)); return m.ret(4); }
// 005dffc0 (ESI = the language index) -> EAX = the name's characters
FX_PORT(0x005dffc0, LanguageName_005dffc0, OUT_EAX) { m.r[0] = rx_fx_language_name(m.r[6]); return m.ret(0); }
