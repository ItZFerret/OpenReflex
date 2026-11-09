// phys_cylinder.cpp -- oracle adapters for core/game/phys_cylinder.c (callers read ECX).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_cylinder.h"
}
using reflex_port::OUT_ECX;

// 00846c70 (EDI cylinder, ESI point*)
RX_PORT(0x00846c70, CylinderClosestPoint, "rx_phys", OUT_ECX) {
    m.r[1] = phys_cylinder_closest_point(m.r[7], m.r[6]); return m.ret(0);
}
// 00846f10 (ESI cylinder, EDI point*; stack normal*; RET 4)
RX_PORT(0x00846f10, CylinderClosestPointNormal, "rx_phys", OUT_ECX) {
    m.r[1] = phys_cylinder_closest_point_normal(m.r[6], m.r[7], m.arg(0)); return m.ret(4);
}
