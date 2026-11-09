// ABI glue for original face kernels0081da00/dc30/de70, sum0081e8e0,
// and mask0400/box0086b7f0. EDX from the pair is live through dispatch.
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_pair_rodbox.h"
}
using namespace reflex_port;
using phys_oracle::argf;
#define FACE_ADAPTER(addr,Name,fn) RX_PORT(addr,Name,"rx_phys",0){fn(m.r[6],m.r[7],argf(m,0),argf(m,1),argf(m,2),argf(m,3),m.arg(4),m.arg(5),m.arg(6),m.arg(7));return m.ret(0x20);}
FACE_ADAPTER(0x0081da00,RodBoxFaceX,phys_rodbox_face_x)
FACE_ADAPTER(0x0081dc30,RodBoxFaceY,phys_rodbox_face_y)
FACE_ADAPTER(0x0081de70,RodBoxFaceZ,phys_rodbox_face_z)
RX_PORT(0x0081e8e0,RodBoxFaceSum,"rx_phys",OUT_EDX){
 phys_rodbox_face_sum(m.r[0],m.r[1],m.arg(0),argf(m,1),m.arg(2),m.arg(3));m.r[2]=m.arg(3);return m.ret(0x10);
}
RX_PORT(0x0086b7f0,CollisionPair_0400VsBox,"rx_phys",OUT_ECX|OUT_EDX){
 uint32_t edx=m.r[2];phys_pair_0400_box(m.r[1],m.r[2],m.arg(0),m.arg(1),m.arg(2),m.arg(3),m.arg(4),m.arg(5),&edx);
 m.r[2]=edx;m.r[1]=m.mem.read(0x009b45e4,4);return m.ret(0x18);
}
