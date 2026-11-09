/* 0040b1d0: scene RGBA overlays, ESI top, EDX middle, ECX bottom, stack scene.
 * Twelve sequential FLD/FSTP copies, preserving overlap order and quieting SNaNs. */
#include "fx_overlay.h"
#include "fx_util.h"
void rx_fx_overlay_scene_colors(rx_addr scene, rx_addr top, rx_addr middle, rx_addr bottom) {
    for (uint32_t i=0;i<16;i+=4) fx_x87_copy(scene+0x6a0+i,top+i);
    for (uint32_t i=0;i<16;i+=4) fx_x87_copy(scene+0x6b0+i,middle+i);
    for (uint32_t i=0;i<16;i+=4) fx_x87_copy(scene+0x6c0+i,bottom+i);
}
RX_FUNCTION(0x0040b1d0,rx_fx_overlay_scene_colors)
