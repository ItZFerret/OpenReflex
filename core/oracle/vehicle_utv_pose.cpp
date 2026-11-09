#include "oracle.h"
extern "C" {
#include "../game/vehicle_utv_pose.h"
}
using namespace reflex_port;
RX_PORT(0x004c1fa0,Utv_UpdateModelPose,"rx_UtvPose",0){
    rx_utv_update_model_pose_regs(m.r[1],m.r[3],m.r[7]);return m.ret(4);
}
