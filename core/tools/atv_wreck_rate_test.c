/* Native-only WreckChecker virtual-return ABI regression. Compile this instead
 * of game/world_wreck_checker.c, then link the usual core objects, excluding
 * world_wreck_checker.o. Including the source exercises the actual private
 * call site, rather than duplicating the typed dispatch in the fixture.
 * Do not compile with RX_ORACLE: that path deliberately calls the original. */
#ifdef RX_ORACLE
#error This contract must exercise the native product dispatch
#endif
#include "../game/world_wreck_checker.c"
#include "rx_mem.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void) {
    CHECK(rx_mem_init());
    const rx_addr v=0x02100000,s=0x02110000,vt=0x02120000;
    const rx_addr sp=0x03000000;
    memset(rx_base+v,0,0x1800);memset(rx_base+s,0,0x9be0);
    SU32(v,0,vt);SU32(v,0x1700,s);rx_set_sp(sp);
    SU32(vt,0xe0,0x004867b0);SU32(vt,0xe4,0x00486850);
    SF32(s,0x9220,2);SF32(s,0x9370,4);SF32(s,0x94c0,6);SF32(s,0x9610,8);
    const float loads[3]={3,7,5};
    for(uint32_t axle=0;axle<3;axle++) {
        CHECK(vehicle_rate(v,0xe0,axle,WCTX())==loads[axle]);
        CHECK(rx_sp()==sp);
    }
    /* A contact with zero longitudinal/normal projection: the authored .8 lower
     * bound yields sqrt(.8), not
     * zero. A zero result forces wreck type 0x13 on the next checker update. */
    /* Match CRT startup on the supported SSE2 reference machine; otherwise
     * CIpow correctly retains the explicit unsupported legacy-x87 boundary. */
    SU32(0,0x785bd7b0,1);
    SF32(0,0x931f24,1);SF32(0,0x9774c4,2);SF32(0,0x931d94,1);SF32(0,0x9cc8a8,.8f);
    SF32(s,0x7cc,1);SF32(0,0xad66bc,1);
    SU32(s,0x9240,1);SU32(s,0x9390,1);SU32(s,0x94e0,1);SU32(s,0x9630,1);
    for(uint32_t axle=0;axle<2;axle++) {
        const float rate=vehicle_rate(v,0xe4,axle,WCTX());
        CHECK(rx_fb(rate)==0x3f64f92eu); /* correctly rounded sqrt(float(.8)) */
        CHECK(rx_sp()==sp);
    }
    rx_mem_shutdown();puts("PASS native ATV WreckChecker wheel/grip returns and stack preservation");return 0;
}
