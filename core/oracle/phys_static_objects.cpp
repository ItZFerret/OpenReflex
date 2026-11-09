// phys_static_objects.cpp -- oracle adapters for core/game/phys_static_objects.c (group rx_phys): the removal of static objects from the
// physics world (the CollisionEntity teardown). The reference worlds never remove a static object; the probe core/oracle/dtor_probe.cpp
// (vf6 of the free ride / waypoint / race activity: the area's CollisionEntity instances) drives them.
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_static_objects.h"
}
using namespace phys_oracle;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

// 00773e10 (EDI = a tracking record; plain ret; EAX = EDI)
RX_PORT(0x00773e10, StaticRecord_Free, "rx_phys", OUT_EAX) { rx_static_record_free(m.r[7]); m.r[0] = m.r[7]; return m.ret(0); }
// 00773a60 (ESI = the vector; stack index; RET 4)
RX_PORT(0x00773a60, StaticRecords_Remove, "rx_phys", 0) { rx_static_records_remove(m.r[6], m.arg(0)); return m.ret(4); }
// 007747f0 (stdcall vector, index, source record; RET 0xc): EAX = the source's tracking record, EDX = the vector's world tag, ECX = the source
RX_PORT(0x007747f0, StaticRecords_MoveSlot, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t src = m.arg(2), vec = m.arg(0);
    rx_static_records_move_slot(vec, m.arg(1), src);
    m.r[0] = rx_rd32(src + 0x2c); m.r[2] = rx_rd32(vec + 0x1c); m.r[1] = src;
    return m.ret(0xc);
}
// 00774d10 (ESI = destination, EDI = source) -> EAX = ESI
RX_PORT(0x00774d10, StaticRecord_Assign, "rx_phys", OUT_EAX) { m.r[0] = rx_static_record_assign(m.r[6], m.r[7]); return m.ret(0); }
// 00773ad0 (ESI = the vector, ECX = the index)
RX_PORT(0x00773ad0, StaticBody_UpdateGrid, "rx_phys", 0) { rx_static_body_update_grid(m.r[6], (int32_t)m.r[1]); return m.ret(0); }
// 007772d0 (EAX = first; the end on the stack, popped by the caller)
RX_PORT(0x007772d0, StaticRecords_Destroy, "rx_phys", 0) { rx_static_records_destroy(m.r[0], m.arg(0)); return m.ret(0); }
// 007735e0 (ECX = the handle) / 00773db0 (ECX = the handle; flags; RET 4)
RX_PORT(0x007735e0, StaticHandle_Release, "rx_phys", 0) { rx_static_handle_release(m.r[1]); return m.ret(0); }
RX_PORT(0x00773db0, StaticHandle_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_static_handle_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 008218c0 (cdecl: source begin, source end, destination; three unused words) -> EAX = the destination's end
RX_PORT(0x008218c0, StaticHandle_Copy, "rx_phys", OUT_EAX) { m.r[0] = rx_static_handle_copy(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0); }
// 0081ff30 (stdcall the vector; RET 4)
RX_PORT(0x0081ff30, StaticHandleVector_Free, "rx_phys", 0) { rx_static_handle_vector_free(m.arg(0)); return m.ret(4); }
// 0081fe40 (EDI = the vector; stdcall out, first iterator, last iterator; RET 0x14) -> EAX = out
RX_PORT(0x0081fe40, StaticHandleVector_Erase, "rx_phys", OUT_EAX) {
    m.r[0] = rx_static_handle_vector_erase(m.r[7], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14);
}
// 0081f690 (EAX = the vector)
RX_PORT(0x0081f690, StaticHandleVector_Clear, "rx_phys", 0) { rx_static_handle_vector_clear(m.r[0]); return m.ret(0); }
