// crt_runtime.cpp -- oracle adapters for core/game/crt_runtime.c (cdecl unless noted; the result in EAX).
#include "crt_oracle.h"
using namespace reflex_port;

RX_CRT_PORT(0x785839df, Malloc, OUT_EAX) { m.r[0] = crt_malloc(m.arg(0)); return m.ret(0); }
RX_CRT_PORT(0x785837ee, Free, 0) { crt_free(m.arg(0)); return m.ret(0); }
RX_CRT_PORT(0x78583b39, OperatorNew, OUT_EAX) { m.r[0] = crt_operator_new(m.arg(0)); return m.ret(0); }
RX_CRT_PORT(0x78583ba3, OperatorDelete, 0) { crt_operator_delete(m.arg(0)); return m.ret(0); }
RX_CRT_PORT(0x78583bd3, AlignedOffsetMalloc, OUT_EAX) { m.r[0] = crt_aligned_offset_malloc(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0); }
RX_CRT_PORT(0x78583cee, AlignedMalloc, OUT_EAX) { m.r[0] = crt_aligned_malloc(m.arg(0), m.arg(1)); return m.ret(0); }
RX_CRT_PORT(0x78583ccf, AlignedFree, 0) { crt_aligned_free(m.arg(0)); return m.ret(0); }
RX_CRT_PORT(0x78594640, Qsort, 0) {     // the partition swap's temporary is the high byte of the base argument slot
    int32_t b; crt_qsort_ex(m.arg(0), m.arg(1), m.arg(2), m.arg(3), &b);
    if (b >= 0) m.mem.write(m.r[4] + 7, 1, uint32_t(b));
    return m.ret(0);
}
RX_CRT_PORT(0x78594bed, Rand, OUT_EAX) { m.r[0] = uint32_t(crt_rand()); return m.ret(0); }
// ldiv: the 8-byte result in EDX:EAX
RX_CRT_PORT(0x7859442e, Ldiv, OUT_EAX | OUT_EDX) {
    const crt_ldiv_t r = crt_ldiv(int32_t(m.arg(0)), int32_t(m.arg(1)));
    m.r[0] = uint32_t(r.quot); m.r[2] = uint32_t(r.rem); return m.ret(0);
}
// type_info::_name_internal_method: thiscall (ECX = this), ret 4
RX_CRT_PORT(0x7857bfaf, TypeInfoName, OUT_EAX) { m.r[0] = crt_type_info_name(m.r[1], m.arg(0)); return m.ret(4); }
RX_CRT_PORT(0x7857e090, RTDynamicCast, OUT_EAX) {
    m.r[0] = crt_rtdynamiccast(m.arg(0), int32_t(m.arg(1)), m.arg(2), m.arg(3), int32_t(m.arg(4))); return m.ret(0);
}
