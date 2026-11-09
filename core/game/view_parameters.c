/* Actual viewport parameter ownership for product scenes. These are the CPU
 * stores of00794ca0/007b6f20, not a default color or a fake renderer/effect. */
#include "view_parameters.h"
#include "build_alloc.h"
static void blur_parameters_construct(rx_addr b){
    SU32(b,0xc,0);SU32(b,0x14,0);SU32(b,4,0);SU32(b,8,1);
    SU32(b,0,0x00971c50);
    SU32(b,0x30,0);SU32(b,0x34,0);SU32(b,0x44,0);SU32(b,0x54,0);
    SU32(b,0x5c,0);SU32(b,0x64,0);SU32(b,0x6c,0);SU32(b,0x48,0);
    SU32(b,0x50,0);SU32(b,0x58,0);SU32(b,0x70,0);SU32(b,0x4c,0);
    SU32(b,0x60,0);SU32(b,0x68,0);SU32(b,0x7c,0);SU32(b,0x80,0);
    SU32(b,0x78,0);SU32(b,0x20,0);SU32(b,0x24,0);SU32(b,0x28,0);
    SU32(b,0x2c,U32(0,0x00931f24));SU32(b,0x3c,0);
}
rx_addr rx_view_parameters_construct(rx_addr p){
    SU32(p,0x14,0);SU32(p,0x40,0);SU32(p,0x44,0);SU32(p,0x48,0);
    SU32(p,0x60,0);SU32(p,0x58,0);SU32(p,0x54,0);SU32(p,0x5c,0);
    SU32(p,0x64,0);SU32(p,0x1c,0);SU32(p,0x24,0);
    SU32(p,0x10,0x00971c64);SU32(p,0x18,1);SU32(p,0x74,0);
    SU32(p,0x78,1);SU32(p,0x7c,0);SU32(p,0x84,0);
    SU32(p,0x70,0x00971c78);blur_parameters_construct(p+0xb0);
    /* In particular p+30..3c retained RGBA is NOT initialized by this ctor.
     * Canonical00559ac0/00559b60/0074b200 must own those actual values. */
    return p;
}
rx_addr rx_view_parameters_create(void){
    rx_addr p=rx_reflex_aligned_alloc(0x1d0);
    return p?rx_view_parameters_construct(p):0;
}
