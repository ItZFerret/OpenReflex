#include "oracle.h"
extern "C" {
#include "../game/fourwheel_params.h"
}
using namespace reflex_port;
namespace {thread_local reflex_vm::Machine* observed;
struct Observe{reflex_vm::Machine* previous;explicit Observe(reflex_vm::Machine& m):previous(observed){observed=&m;}~Observe(){observed=previous;}};}
extern "C" void rx_fourwheel_params_reg(unsigned i,uint32_t v){if(observed&&i<8&&i!=4)observed->r[i]=v;}
extern "C" uint32_t rx_fourwheel_params_getreg(unsigned i){return observed&&i<8?observed->r[i]:0;}
#define G "rx_FourWheelParamsCandidate"
/* Recorded live.tsv masks: never weaken truck EDX or invent x87 output. */
RX_PORT(0x004cb3f0,UtvParamsCandidate,G,OUT_EAX|OUT_ECX){Observe o(m);return m.ret(8,rx_utv_params_apply(m.arg(0),m.arg(1)));}
RX_PORT(0x004b0bc0,BuggyParamsCandidate,G,OUT_EAX|OUT_ECX){Observe o(m);return m.ret(8,rx_buggy_params_apply(m.arg(0),m.arg(1)));}
RX_PORT(0x0044f7a0,Truck2wdParamsCandidate,G,OUT_EAX|OUT_ECX|OUT_EDX){Observe o(m);return m.ret(8,rx_truck2wd_params_apply(m.arg(0),m.arg(1)));}
