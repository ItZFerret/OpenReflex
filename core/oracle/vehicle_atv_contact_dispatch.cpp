#include "oracle.h"
#include "../game/vehicle_atv_contact_dispatch.h"
using namespace reflex_port;
RX_PORT(0x00497f00,AtvWheelTerrain,"rx_AtvContacts",0){rx_atv_query_wheel_terrain(m.r[6],m.arg(0));return m.ret(4);}
RX_PORT(0x00492ff0,AtvCorrectPositions,"rx_AtvContacts",0){rx_atv_correct_contact_positions(m.r[2]);return m.ret(0);}
RX_PORT(0x0048edc0,AtvPinRest,"rx_AtvContacts",0){rx_atv_pin_rest_position(m.r[1]);return m.ret(0);}
RX_PORT(0x00492320,AtvRollingOrientation,"rx_AtvContacts",0){rx_atv_update_rolling_orientation(m.r[6],rx_bf(m.arg(0)));return m.ret(4);}
RX_PORT(0x00490c80,AtvContacts,"rx_AtvContacts",0){rx_atv_contact_dispatch(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}
