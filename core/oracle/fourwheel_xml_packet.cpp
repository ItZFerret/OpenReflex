#include "oracle.h"
extern "C" {
#include "../game/fourwheel_xml_packet.h"
}
using namespace reflex_port;
RX_PORT(0x00401f90,TuningBufferAllocate,"rx_FourWheelXmlPacket",OUT_EAX){
    return m.ret(0,rx_tuning_buffer_alloc(m.arg(0)));
}
RX_PORT(0x007905d0,TuningPacketQueueAppend,"rx_FourWheelXmlPacket",OUT_ECX){
    m.r[1]=rx_tuning_packet_queue_append(m.r[6],m.arg(0));return m.ret(4);
}
RX_PORT(0x00790540,TuningPacketPublish,"rx_FourWheelXmlPacket",OUT_EAX|OUT_EDX){
    uint32_t eax=m.r[0],edx=m.r[2];
    rx_tuning_packet_publish_observe(m.arg(0),m.arg(1),m.arg(2),m.arg(3),&eax,&edx);
    m.r[0]=eax;m.r[2]=edx;return m.ret(16);
}
