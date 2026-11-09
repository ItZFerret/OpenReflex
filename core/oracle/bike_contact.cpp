// bike_contact.cpp -- oracle adapters for core/game/bike_contact.c (slot 13: wheel / body contacts and corrections).
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_contact.h"
}
using namespace bike_oracle;

// 00431ee0: ECX = W, EDX = S, stack out vec4 (ret 4). Left over: EAX = out (the stack argument); EDX = S unchanged (live.tsv: EAX, EDX).
RX_PORT(0x00431ee0, BikeWheel_ContactPositionCorrection, "rx_bike", OUT_EAX | OUT_EDX) {
    Scratch s(m);
    const uint32_t out = m.arg(0);
    const rx_v4 r = bike_wheel_position_correction(m.r[1], m.r[2]);
    rx_st4(out, r);
    s.restore(m);
    m.r[0] = out;
    return m.ret(4);
}

// 0043cfe0: as 00431ee0 (rider-off variant).
RX_PORT(0x0043cfe0, BikeWheel_ContactPositionCorrectionRiderOff, "rx_bike", OUT_EAX | OUT_EDX) {
    Scratch s(m);
    const uint32_t out = m.arg(0);
    const rx_v4 r = bike_wheel_position_correction_rider_off(m.r[1], m.r[2]);
    rx_st4(out, r);
    s.restore(m);
    m.r[0] = out;
    return m.ret(4);
}

// 00438180: ECX = S (ret 0). live.tsv lists EAX / EDX, a false positive: all four callers (4457ec, 49317d, 4b439d, 4cf49d) go on with
// mov ecx / call __security_check_cookie (reads ECX only) / ret, and the wrappers' own liveness (4456d0 and the ATV / truck / buggy / UTV
// copies) is ECX + x87 only; EAX / EDX stay at their entry values here (the original leaves loop leftovers).
RX_PORT(0x00438180, BodyProbe_ApplyAverageCorrection, "rx_bike", 0) {
    Scratch s(m);
    bike_body_probe_apply_average_correction(m.r[1]);
    s.restore(m);
    return m.ret(0);
}

// 004456d0: EDX = S (ret 0). Left over: ECX = the security cookie (cookie ^ esp ^ esp).
RX_PORT(0x004456d0, BikeWheel_ApplyContactCorrection, "rx_bike", OUT_ECX | OUT_FPU) {
    Scratch s(m);
    bike_wheel_apply_contact_correction(m.r[2]);
    s.restore(m);
    m.r[1] = uint32_t(m.mem.read(0x9b45e4, 4));
    return m.ret(0);
}

// 00444a40: ESI = W, stack S (ret 4). No register is read by its callers (live.tsv: none).
RX_PORT(0x00444a40, BikeWheel_QueryTerrainContact, "rx_bike", 0) {
    Scratch s(m);
    bike_wheel_query_terrain_contact(m.r[6], m.arg(0));
    s.restore(m);
    return m.ret(4);
}
