// bike_force.cpp -- oracle adapters for core/game/bike_force.c (group "rx_bike"): the force stage (slot 11) of the motocross simulation.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_force.h"
}
using namespace bike_oracle;

// 00446a40: stack S, factor, dt (ret 12). The original leaves xmm2 = the root-local Y splatted; EDX untouched.
RX_PORT(0x00446a40, BikeEngine_CouplingSpring, "rx_bike", OUT_EDX | XMM(2)) {
    Scratch s(m);
    const uint32_t S = m.arg(0);
    const float y = bike_engine_coupling_spring(S, argf(m, 1), argf(m, 2));
    s.restore(m);
    set_xmm(m, 2, rx_v4{{y, y, y, y}});
    return m.ret(12);
}

// 00447cd0: stack S (ret 4); nothing live afterwards.
RX_PORT(0x00447cd0, BikeWheels_ApplyAxleTorques, "rx_bike", 0) {
    bike_wheels_apply_axle_torques(m.arg(0));
    return m.ret(4);
}

// 00445e80: EDI = S (ret 0). live.tsv lists ECX, a false positive: its only caller (vf11 at 0043e146) continues at 0043e15c with
// "fld dt ; push ecx ; fstp [esp]", the pushed ECX is just the slot dt overwrites. (The original leaves ECX = the best index.)
RX_PORT(0x00445e80, BikeGear_AutoSelect, "rx_bike", OUT_FPU) {
    bike_gear_auto_select(m.r[7]);
    return m.ret(0);
}

// 004460f0: stack S (ret 4). Same false-positive ECX as above (the original leaves the ParamCurve loop's pointer in it).
RX_PORT(0x004460f0, BikeGear_SelectDownshift, "rx_bike", OUT_FPU) {
    bike_gear_select_downshift(m.arg(0));
    return m.ret(4);
}

// 00446340: EDI = S, stack dt (ret 4); nothing live afterwards.
RX_PORT(0x00446340, BikeEngine_DriveForces, "rx_bike", 0) {
    bike_engine_drive_forces(m.r[7], argf(m, 0));
    return m.ret(4);
}

// 0043c590: EDI = S (ret 0). EDX = the mechanism S+d0 (left by the last 0085c770 call).
RX_PORT(0x0043c590, BikeSuspension_SpringDamperForces, "rx_bike", OUT_EDX | OUT_FPU) {
    Scratch s(m);
    const uint32_t S = m.r[7];
    bike_suspension_spring_damper_forces(S);
    s.restore(m);
    m.r[2] = S + 0xd0;
    return m.ret(0);
}

// 0043efa0: EDI = S (ret 0). EDX = the mechanism S+d0 (left by the last 0085c770 call).
RX_PORT(0x0043efa0, BikeWheel_DragForces, "rx_bike", OUT_EDX | OUT_FPU) {
    Scratch s(m);
    const uint32_t S = m.r[7];
    bike_wheel_drag_forces(S);
    s.restore(m);
    m.r[2] = S + 0xd0;
    return m.ret(0);
}

// 0043e100: ECX = S, stack dt (ret 4). Virtual slot 11: EAX / EDX come out of the last call (the front 00447f10), which the core also
// runs through rx_call_original, so they are left as they are.
RX_PORT(0x0043e100, RavenSimulationMotocross_vf11, "rx_bike", OUT_EAX | OUT_EDX | OUT_FPU) {
    bike_force_stage(m.r[1], argf(m, 0));
    return m.ret(4);
}
