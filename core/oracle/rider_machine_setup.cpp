#include "oracle.h"
extern "C" {
#include "../game/build_rider.h"
}
using namespace reflex_port;
/* These three original routines return no semantic value. Tree walkers discard
 * vf0's return (walk_enter in build_rider_tree.c); original EAX is not zero on
 * all paths. Setup caller00619490 continues storing rider fields; switch's sole
 * caller0061ab07 returns without testing results. No volatile outputs are read. */
RX_PORT(0x0061aa10,RiderMachineSetup,"rx_RiderMachineSetup",0){rx_rider_truck_setup(m.r[0],m.r[1]);return m.ret(0);}
RX_PORT(0x0061aaa0,RiderMachineWalker,"rx_RiderMachineSetup",0){rx_rider_machine_walker_enter(m.r[1],m.arg(0));return m.ret(4);}
RX_PORT(0x0061ab30,RiderMachineBindSwitch,"rx_RiderMachineSetup",0){rx_rider_machine_bind_switch(m.arg(0),m.arg(1));return m.ret(8);}
