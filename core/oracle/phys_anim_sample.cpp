// phys_anim_sample.cpp -- oracle adapters for core/game/phys_anim_sample.c (callers read ECX).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_anim_sample.h"
}
using namespace phys_oracle;
using reflex_port::OUT_ECX;

RX_PORT(0x00840440, PoseSampler_Sample, "rx_phys", OUT_ECX) {
    m.r[1] = phys_pose_sample(m.arg(0), argf(m, 1), m.arg(2)); return m.ret(0xc);
}
RX_PORT(0x00840b70, PoseSampler_Blend, "rx_phys", OUT_ECX) {
    m.r[1] = phys_pose_sample_blend(m.arg(0), m.arg(1), argf(m, 2), argf(m, 3), argf(m, 4), m.arg(5)); return m.ret(0x18);
}
