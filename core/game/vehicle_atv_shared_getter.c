/*00432190 RavenSimulation::vf27: original raw mode flag; ECX=self, EAX=result. */
#include "rx_game.h"
uint32_t rx_simulation_get_mode_flag(rx_addr s){return rx_rd32(s+0x90);}
RX_FUNCTION(0x00432190,rx_simulation_get_mode_flag)
