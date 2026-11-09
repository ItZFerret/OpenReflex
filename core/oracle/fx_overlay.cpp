#include "oracle.h"
extern "C" {
#include "../game/fx_overlay.h"
}
using namespace reflex_port;
RX_PORT(0x0040b1d0,OverlaySceneColors,"rx_Overlay",OUT_EAX) {
    const uint32_t scene=m.arg(0);
    rx_fx_overlay_scene_colors(scene,m.r[6],m.r[2],m.r[1]);
    m.r[0]=scene;return m.ret(4);
}
