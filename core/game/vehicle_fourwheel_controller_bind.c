/* Simulation vf23: read the authored controller block, then publish shift axes.
 * 004d9460 and004be490 differ only in the two destination offsets. */
#include "vehicle_fourwheel_controller_bind.h"
#include "bike_sim.h"
#include "loop_util.h"
static void bind_controller(rx_addr sim,rx_addr controller,uint32_t shift,rx_addr ret) {
 /* Both originals push ESI and EDI before the LTCG EAX/EDI helper call. */
 const rx_addr entry=rx_sp();rx_set_sp(entry-8);
 const rx_addr saved=loop_push(0,0,ret);
 bike_read_controller_input(controller,sim+0x1c);
 loop_pop(saved);
 const uint32_t a=U32(sim,0x4c),b=U32(sim,0x50);
 SU32(sim,shift,a);SU32(sim,shift+4,b);
 rx_set_sp(entry);
}
void rx_fourwheel_bind_controller_truck(rx_addr sim,rx_addr controller) {
 bind_controller(sim,controller,0x56f0,0x004d9470);
}
void rx_fourwheel_bind_controller_buggy(rx_addr sim,rx_addr controller) {
 bind_controller(sim,controller,0x56e0,0x004be4a0);
}
RX_FUNCTION(0x004d9460,rx_fourwheel_bind_controller_truck)
RX_FUNCTION(0x004be490,rx_fourwheel_bind_controller_buggy)
