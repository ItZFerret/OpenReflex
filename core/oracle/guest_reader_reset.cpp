#include "oracle.h"
extern "C" {
#include "../game/guest_reader_reset.h"
}
using namespace reflex_port;
namespace {thread_local reflex_vm::Machine* observed;
struct Observe{reflex_vm::Machine* previous;explicit Observe(reflex_vm::Machine& m):previous(observed){observed=&m;}~Observe(){observed=previous;}};}
extern "C" void rx_reader_reset_reg(unsigned i,uint32_t v){if(observed&&i<8&&i!=4)observed->r[i]=v;}
/* Exact recorded mask5, no FPU mask invented or removed. Real dynamic callees
 * retain their CRT/register effects; observer tracks reset's structural writes. */
RX_PORT(0x00827e00,GuestReaderReset,"rx_GuestReaderReset",OUT_EAX|OUT_EDX){Observe o(m);rx_guest_reader_reset(m.r[6]);return m.ret(0);}
