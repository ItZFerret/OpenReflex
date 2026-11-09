// phys_tri.cpp -- oracle adapters for core/game/phys_tri.c.
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_tri.h"
}
using namespace phys_oracle;
using namespace reflex_port;

// 00860e90 (ECX, EDX, ESI; 5 cdecl args): EAX = any hit; EDX = 0 without a hit, else the last word the original moves
// through EDX (lane 3 of the z normal accumulator = the fourth normal's z).
RX_PORT(0x00860e90, Rays4_Triangles, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t normals = m.arg(1);
    const int32_t r = phys_rays4_triangles(int32_t(m.r[1]), m.r[2], m.r[6], m.arg(0), normals, m.arg(2), m.arg(3), m.arg(4));
    phys_rays4_saved_regs(m.r[4], m.r[7], m.r[5]);   // the caller's EDI / EBP in its frame (as the caller's port writes them)
    m.r[0] = uint32_t(r);
    m.r[2] = r ? uint32_t(m.mem.read(normals + 0x38, 4)) : 0;
    return m.ret(0);
}
RX_PORT(0x008639f0, Triangle_ClosestPoint, "rx_phys", 0) {
    phys_triangle_closest_point(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0);
}
// 00873540 (ECX, EDX; 6 cdecl args): EAX = any hit. (EDX: a pointer the update loop leaves, not reproduced.)
RX_PORT(0x00873540, Edges12_Triangles, "rx_phys", OUT_EAX) {
    m.r[0] = uint32_t(phys_edges12_triangles(m.r[1], m.r[2], m.arg(0), int32_t(m.arg(1)), m.arg(2), m.arg(3), m.arg(4), m.arg(5)));
    phys_edges12_saved_regs(m.r[4], m.r[6], m.r[7], m.r[5]);   // the caller's ESI / EDI / EBP in its frame (as the caller's port writes them)
    return m.ret(0);
}
