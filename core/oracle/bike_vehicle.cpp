// bike_vehicle.cpp -- oracle adapters for core/game/bike_vehicle.c.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_vehicle.h"
}
using namespace bike_oracle;

// 0045d510 (listener slot 0): ECX = L, stack dt (ret 4). Its only caller World_PhysicsPreListeners 007699e0 reads no register.
RX_PORT(0x0045d510, RavenVehicle_vf0_o90, "rx_bike", 0) {
    Scratch s(m);
    bike_vehicle_pre_step(m.r[1], argf(m, 0));
    s.restore(m);
    return m.ret(4);
}
// 0045d820 (listener slot 1): ECX = L, stack dt (ret 4). The post-listener loop reads no register after it.
RX_PORT(0x0045d820, RavenVehicle_vf1_o90, "rx_bike", 0) {
    Scratch s(m);
    bike_vehicle_post_step(m.r[1], argf(m, 0));
    s.restore(m);
    return m.ret(4);
}
// 0045d910: EDI = vehicle, stack dt (ret 4). Leaves ECX = the security cookie (its check's argument).
RX_PORT(0x0045d910, Vehicle_DebugView, "rx_bike", OUT_ECX | OUT_FPU) {
    check_cw(m, "0045d910");
    Scratch s(m);
    bike_vehicle_debug_view(m.r[7], argf(m, 0));
    s.restore(m);
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(4);
}

// 0042d7f0 (LTCG): ESI = the position filters, EDX = the target vec3, EAX = the output vec3, xmm4 = dt -> EAX = out; the filter's last
// scalar is left in xmm0 / xmm1 / xmm2 / xmm4 / xmm5 (not read by the caller).
RX_PORT(0x0042d7f0, Vehicle_PositionFilter, "rx_bike", OUT_EAX) {
    m.r[0] = bike_vehicle_position_filter(m.r[6], m.r[2], m.r[0], rx_oracle::f32(uint32_t(m.xmm[4].lo)));
    return m.ret(0);
}
