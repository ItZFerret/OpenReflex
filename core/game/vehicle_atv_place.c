/* Original RavenVehicleATV V vf33 (004862e0): cut its four tyre strips and
 * reset contact/control state. Track pool selection and store order follow x86. */
#include "vehicle_atv_place.h"
#include "race_extern.h"
#include "phys_terrain.h"
void rx_atv_reset_ground(rx_addr v) {
    RACE_FRAME_EBP(F, 16, 4, 3);
    if (!(U8(0,0x9e30e0)&1)) { SU32(0,0x9e30e0,U32(0,0x9e30e0)|1); SU32(0,0x9e30dc,0); }
    for (unsigned i=0;i<4;i++) {
        uint32_t h=U32(v,0x1584+4*i), index; rx_addr pool;
        if(h&0x1000){pool=U32(0,0xa95774);index=h^0x1000;}
        else if(h&0x2000){pool=U32(0,0xa95778);index=h^0x2000;}
        else {pool=U32(0,0xa9577c);index=h^0x4000;}
        RACE_CALL_SP(0,phys_tracks_cut_strip(pool,(int32_t)index));
    }
    rx_addr s=U32(v,0x1700);
    for(unsigned o=0x1594;o<=0x15b8;o+=4)SU32(v,o,0);
    for(unsigned o=0x1628;o<=0x1644;o+=4)SU32(v,o,0);
    SU32(v,0x1310,0);SU32(v,0x1314,0);
    for(unsigned o=0x1648;o<=0x1654;o+=4)SU32(v,o,0);
    SU32(v,0x1318,0);SU32(v,0x131c,0);
    rx_st4(s+0x96b0,rx_ld4(0xd61670));
    rx_v4 q=rx_ld4(0xd61670);s=U32(v,0x1700);rx_st4(s+0x96c0,q);
    q=rx_ld4(0xd61670);s=U32(v,0x1700);rx_st4(s+0x9690,q);
    q=rx_ld4(0xd61670);s=U32(v,0x1700);rx_st4(s+0x96a0,q);
    for(unsigned o=0x1660;o<=0x166c;o+=4)SU32(v,o,0);
    SU32(v,0x1320,0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x004862e0,rx_atv_reset_ground)
