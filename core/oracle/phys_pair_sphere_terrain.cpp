#include "oracle.h"
extern "C" {
#include "../game/phys_pair_sphere_terrain.h"
}
using namespace reflex_port;
RX_PORT(0x00864ec0, CollisionPair_SphereVsHFTerrain, "rx_SphereTerrain", OUT_ECX) {
    phys_pair_sphere_hfterrain(m.r[1], m.r[7], m.arg(0), m.arg(1), m.arg(2));
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(12);
}
