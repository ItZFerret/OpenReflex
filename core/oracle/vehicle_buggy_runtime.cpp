#include "oracle.h"
extern "C" {
#include "../game/vehicle_buggy_runtime.h"
}
using namespace reflex_port;
RX_PORT(0x004abe20,BuggyPublishRider,"rx_BuggyRuntime",0){
    rx_buggy_publish_rider(m.r[1],rx_bf(m.arg(0)));return m.ret(4);
}
