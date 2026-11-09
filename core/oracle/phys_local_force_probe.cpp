/* Oracle-only argument probe for 0085c8b0. Private mechanism storage: root and three body indices; identity, rotated and arbitrary
 * quaternions, signed zero, infinities, NaNs and aliasing. Calls the original through Verify, never substitutes game logic. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace {
uint32_t frame, rng = 0x57cb102au;
uint32_t next() { rng = rng * 1664525u + 1013904223u; return rng; }
uint32_t value(unsigned k) {
    static const uint32_t special[] = {0,0x80000000u,0x3f800000u,0xbf800000u,0x7f800000u,0xff800000u,
                                     0x7fc01234u,0xffc05678u,0x7f801234u,1,0x80000001u};
    return k < 11 ? special[k] : (next() & 0x807fffffu) | ((116 + next()%20) << 23);
}
void run() {
    const uint32_t bytes = 0x1000;
    const uint32_t M = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, &bytes), force = M + 0xf00;
    for (unsigned i=0; i<0x1000; i+=4) rx_wr32(M+i,0);
    for (unsigned k=0; k<256; ++k) {
        const unsigned index = k % 4;
        const uint32_t p = M + index * 0xa0;
        for (unsigned j=0; j<4; ++j) {
            const uint32_t q = k%4==0 ? (j==3 ? 0x3f800000u : 0) :
                (j==3 || j==k%3 ? 0x3f3504f3u : 0);  // identity and quarter-turns about all three axes
            rx_wr32(p+0x660+4*j, k<128 ? q : value((k+j)%19));
            rx_wr32(p+0x70+4*j, k<128 ? rx_oracle::bits(float(int(next()%2001)-1000)*0.125f) : value((k+3*j)%21));
            rx_wr32(force+4*j, k<128 ? rx_oracle::bits(float(int(next()%2001)-1000)*0.0625f) : value((k+7*j)%23));
        }
        const uint32_t src = k%13==0 ? p+0x70 : k%17==0 ? p+0x660 : force;
        rx_regs in = {}, out = {};
        in.r[1] = M; in.r[2] = 0xabcdef12;
        const uint32_t args[2] = {index,src};
        rx_call_original_ex(0x0085c8b0, &in, &out, 2, args);
    }
    std::fprintf(stderr,"[localforce] 256 cases, mechanism %08x, all body indices and force/quaternion aliases\n",M);
    rx_call_original(0x0078fc50, RX_CDECL, 0, 1, &M);
}
}
RX_PORT(0x0084ed20, LocalForceProbeHook, "thunk", 0) {
    if (!std::getenv("RX_LOCAL_FORCE_PROBE")) return reflex_vm::RUN_ORIGINAL;
    if (++frame == 300) {
        uint32_t regs[8]; reflex_vm::Xmm xmm[8];
        std::memcpy(regs,m.r,sizeof regs); std::memcpy(xmm,m.xmm,sizeof xmm);
        const uint32_t flags=m.flags; const auto fpu=m.fpu;
        run();
        std::memcpy(m.r,regs,sizeof regs); std::memcpy(m.xmm,xmm,sizeof xmm);
        m.flags=flags; m.fpu=fpu;
    }
    return reflex_vm::RUN_ORIGINAL;
}
