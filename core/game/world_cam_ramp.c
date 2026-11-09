/* world_cam_ramp.c -- the parameter block at 0x00c023a8 .. 0x00c02400 that MXWorld::vf1 initialises from the camera tuning constants 0x009c47b8 ..
 * 0x009c47d8 (the block's per-frame state machine is 0067f450). 0067f3c0 (EAX unused; XMM2 = the limit, stack = the base value; RET 4) and 00674d90. */
#include "rx_game.h"
#include "../src/rx_x86.h"
#include "world_cam_ramp.h"
#include "world_crowd_audio.h"

/* fld m32 / fstp m32: a signalling NaN comes out quiet */
static uint32_t x87_copy(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }

/* 0067f3c0: +0xc023a8 = 0, +0xc023ac = 10.0 (0x92fd88); +0xc023b0 = the limit clamped to 0 .. 10 (a NaN limit is stored as is: the lower clamp is taken only
 * when 0 > limit, the upper one only when limit > 10); +0xc023b4 = |base|; +0xc023b8 .. +0xc023c4 = +0xc023b0; state +0xc02400 = 0 */
void rx_cam_ramp_init(float limit, uint32_t base_bits) {
    const float ten = F32(0, 0x0092fd88);
    SF32(0, 0x00c023a8, 0.0f);
    SF32(0, 0x00c023ac, ten);
    if (0.0f > limit) SF32(0, 0x00c023b0, 0.0f);          /* comiss xmm0, xmm2: jbe not taken */
    else if (limit > ten) SF32(0, 0x00c023b0, ten);
    else SF32(0, 0x00c023b0, limit);
    SU32(0, 0x00c023b4, base_bits & 0x7fffffffu);          /* Float_Abs 00401000 */
    for (uint32_t o = 0xc023b8; o <= 0xc023c4; o += 4) SU32(0, o, U32(0, 0x00c023b0));
    SU32(0, 0x00c02400, 0);
}

/* 00674d90 */
void rx_cam_ramp_setup(void) {
#ifndef RX_ORACLE
    rx_crowd_config_init();   /* w-sfx: the crowd unit's CrowdAudioParameters first (00674cd0, which the product's init does not run) */
#endif
    rx_cam_ramp_init(F32(0, 0x009c47b8), x87_copy(U32(0, 0x009c47c4)));
    SU32(0, 0x00c023d0, U32(0, 0x009c47c8));
    SU32(0, 0x00c023d4, U32(0, 0x009c47cc));
    SU32(0, 0x00c023d8, U32(0, 0x009c47d0));
    SU32(0, 0x00c023dc, U32(0, 0x009c47d4));
    SU32(0, 0x00c023e0, U32(0, 0x009c47d8));
    SU32(0, 0x00c023ec, U32(0, 0x009c47d4));
    SU32(0, 0x00c023f0, U32(0, 0x009c47d8));
    SU32(0, 0x00c023f4, U32(0, 0x009c47d4));
    if (I32(0, 0x00a941b4) != -1 && I32(0, 0x009c47bc) != -1) {
        SU32(0, 0x00c023b8, U32(0, 0x00c023b0));
        SU32(0, 0x00c02400, 4);
    }
}
RX_FUNCTION(0x00674d90, rx_cam_ramp_setup)
