#include "oracle.h"
extern "C" {
#include "../game/tdf_owner.h"
}
using namespace reflex_port;
namespace{thread_local reflex_vm::Machine* observed;struct Observe{reflex_vm::Machine* prior;Observe(reflex_vm::Machine&m):prior(observed){observed=&m;}~Observe(){observed=prior;}};}
extern "C" void rx_tdf_owner_reg(unsigned i,uint32_t v){if(observed&&i<8&&i!=4)observed->r[i]=v;}
extern "C" uint32_t rx_tdf_owner_getreg(unsigned i){return observed&&i<8?observed->r[i]:0;}
RX_PORT(0x00539ff0,TdfOwnerDelete,"rx_TdfOwner",OUT_EAX|OUT_EDX|OUT_FPU){Observe o(m);return m.ret(4,rx_tdf_owner_delete(m.r[1],m.arg(0)));}
RX_PORT(0x00810890,OwnedMemoryStreamDelete,"rx_TdfOwner",OUT_EAX|OUT_EDX|OUT_FPU){Observe o(m);return m.ret(4,rx_owned_memory_stream_delete(m.r[1],m.arg(0)));}
RX_PORT(0x008108b0,OwnedMemoryStreamDestroy,"rx_TdfOwner",OUT_EAX|OUT_EDX|OUT_FPU){Observe o(m);rx_owned_memory_stream_destroy(m.r[6]);return m.ret(0);}
RX_PORT(0x00810960,OwnedMemoryStreamClose,"rx_TdfOwner",OUT_EAX|OUT_EDX|OUT_FPU){Observe o(m);rx_owned_memory_stream_close(m.r[1]);return m.ret(0);}
