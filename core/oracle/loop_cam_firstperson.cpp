// loop_cam_firstperson.cpp -- oracle adapter for core/game/loop_cam_firstperson.c (group "rx_loop").
// 00642680 MXFirstPersonCamera::vf19: thiscall (ECX = camera), stack dt, ret 4. The reference baton boot calls it 112 times.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/loop_cam_firstperson.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// 00641f20 MXFirstPersonCamera::vf32 (slots 0x80): thiscall, no stack args, ret 0.
RX_PORT(0x00641f20, MXFirstPersonCamera_vf32, "rx_loop", 0) {
    rx_first_person_camera_vf32(m.r[1]);
    return m.ret(0);
}

extern "C" uint32_t rx_fp_probe_cam;

// 00641460 / 00641d90 MXFirstPersonCamera::vf28 / vf29 (thiscall, no stack args).
RX_PORT(0x00641460, MXFirstPersonCamera_vf28, "rx_loop", 0) { rx_first_person_camera_vf28(m.r[1]); return m.ret(4); }
RX_PORT(0x00641d90, MXFirstPersonCamera_vf29, "rx_loop", 0) { rx_first_person_camera_vf29(m.r[1]); return m.ret(0); }
// 0054d8c0 / 0054d980 take the player in EAX, no other argument (ret 0).
RX_PORT(0x0054d8c0, FP_SoundEnable, "rx_loop", 0) { rx_fp_sound_enable(m.r[0]); return m.ret(0); }
RX_PORT(0x0054d980, FP_SoundDisable, "rx_loop", 0) { rx_fp_sound_disable(m.r[0]); return m.ret(0); }
// 0054d870 / 0054d820: the player in EDX, the value in EAX.
RX_PORT(0x0054d870, FP_SoundSlot870, "rx_loop", OUT_EAX) { m.r[0] = rx_fp_sound_w870(m.r[2]); return m.ret(0); }
RX_PORT(0x0054d820, FP_SoundSlot820, "rx_loop", OUT_EAX) { m.r[0] = rx_fp_sound_w820(m.r[2]); return m.ret(0); }

RX_PORT(0x00642680, MXFirstPersonCamera_vf19, "rx_loop", 0) {
    rx_fp_probe_cam = m.r[1];
    if (std::getenv("RX_FP_STATS")) {   /* coverage: did the game's own calls take the pose branch (+0x2f4)? */
        static unsigned n = 0, early = 0, posed = 0;
        if (rx_rd32(m.r[1] + 0x2f4u)) posed++; else early++;
        if (++n % 20 == 0 || n == 1)
            std::fprintf(stderr, "[fpstats] call %u cam %08x 2f4 %u (early %u posed %u)\n", n, m.r[1], rx_rd32(m.r[1] + 0x2f4u), early, posed);
    }
    rx_first_person_camera_vf19(m.r[1], f32(m.arg(0)));
    if (std::getenv("RX_FP_DUMP")) {
        static unsigned n = 0;
        if (++n <= 2 || n == 100) {
            const uint32_t c = m.r[1];
            std::fprintf(stderr, "[fpdump] %u cam %08x\n", n, c);
            for (uint32_t o = 0x30; o <= 0x68; o += 4) std::fprintf(stderr, "  +%02x %08x", o, rx_rd32(c + o));
            std::fprintf(stderr, "\n");
            for (uint32_t o = 0x430; o <= 0x458; o += 4) std::fprintf(stderr, "  +%03x %08x", o, rx_rd32(c + o));
            std::fprintf(stderr, "\n");
            const uint32_t F = ((m.r[4] - 4u) & ~0xfu) - 0x1a0u;
            std::fprintf(stderr, "  frame %08x hq", F);
            for (uint32_t o = 0x30; o <= 0x3c; o += 4) std::fprintf(stderr, " %08x", rx_rd32(F + o));
            std::fprintf(stderr, " hp");
            for (uint32_t o = 0xc0; o <= 0xc8; o += 4) std::fprintf(stderr, " %08x", rx_rd32(F + o));
            std::fprintf(stderr, " pos");
            for (uint32_t o = 0xc0; o <= 0xc8; o += 4) std::fprintf(stderr, " %08x", rx_rd32(F + o));
            std::fprintf(stderr, "\n");
        }
    }
    return m.ret(4);
}
