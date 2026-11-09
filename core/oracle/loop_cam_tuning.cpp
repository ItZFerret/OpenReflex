// loop_cam_tuning.cpp -- oracle adapters for core/game/loop_cam_tuning.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_tuning.h"
}
using namespace reflex_port;

// the compiler's inlined strcmp's result as the original leaves it in ECX (sbb / sbb: -1 / 1, 0 when equal)
static uint32_t inline_strcmp(uint32_t a, uint32_t b) {
    for (uint32_t i = 0;; i++) {
        const uint8_t x = uint8_t(rx_rd8(a + i)), y = uint8_t(rx_rd8(b + i));
        if (x != y) return x < y ? 0xffffffffu : 1u;
        if (x == 0) return 0;
    }
}
// ESI = the name; EAX = the type, ECX = the last comparison ("Wreck")
RX_PORT(0x0065c070, CamTuning_PresetType, "rx_loop", OUT_EAX | OUT_ECX) {
    m.r[0] = uint32_t(rx_cam_tuning_preset_type(m.r[6]));
    m.r[1] = inline_strcmp(m.r[6], 0x00958414);
    return m.ret(0);
}
// stdcall (table, reader); ret 8; EAX = 1 / 0, ECX = the security cookie (the xor of __security_check_cookie's argument)
RX_PORT(0x0065c200, CamTuning_Parse, "rx_loop", OUT_EAX | OUT_ECX) {
    m.r[0] = rx_cam_tuning_parse(m.arg(0), m.arg(1));
    m.r[1] = uint32_t(rx_rd32(0x009b45e4));
    return m.ret(8);
}
