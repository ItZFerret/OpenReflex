/* Complete hand-written0085c9a0. Angular torque is already in the required
 * frame; unlike AddBodyForce this operation neither rotates nor masks w. */
#include "mechanism_angular_torque.h"
#include "fx_util.h"
#include "phys_vec.h"
rx_addr rx_mechanism_add_body_torque(rx_addr mechanism, uint32_t index, rx_addr torque) {
    fx_frame frame;
    fx_frame_enter0(&frame, 0, 16);
    const rx_addr destination = mechanism + 0x6b0u + index * 160u;
    const rx_v4 accumulated = rx_ld4(destination);
    fx_set_reg(FX_EAX, destination);
    fx_set_reg(FX_ECX, torque);
    const rx_v4 applied = rx_ld4(torque);
    rx_st4(destination, v4_add(accumulated, applied));
    fx_frame_leave(&frame);
    return destination;
}
RX_FUNCTION(0x0085c9a0, rx_mechanism_add_body_torque)
