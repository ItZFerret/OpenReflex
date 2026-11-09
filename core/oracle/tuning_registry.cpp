#include "oracle.h"
extern "C" {
#include "../game/tuning_registry.h"
}
using namespace reflex_port;
namespace {thread_local reflex_vm::Machine* observed;
struct Observe{reflex_vm::Machine* previous;explicit Observe(reflex_vm::Machine& m):previous(observed){observed=&m;}~Observe(){observed=previous;}};}
extern "C" void rx_tuning_observe_reg(unsigned index,uint32_t value){if(observed&&index<8&&index!=4)observed->r[index]=value;}
#define G "rx_TuningRegistry"
RX_PORT(0x0042fed0,TuningRegistryGet,G,OUT_EAX){Observe o(m);return m.ret(0,rx_tuning_registry_get());}
/*Void queue cleanup: only callers are destructors; no semantic volatile result.*/
RX_PORT(0x004db6d0,TuningRegistryQueueClear,G,0){Observe o(m);rx_tuning_registry_queue_clear(m.r[6]);return m.ret(0);}
/*Exit callbacks consume no return, but EAX is explicitly observed by these adapters.*/
RX_PORT(0x0042ff00,TuningRegistryCleanup,G,OUT_EAX|OUT_EDX|OUT_FPU){Observe o(m);return m.ret(0,rx_tuning_registry_cleanup());}
RX_PORT(0x00911ed0,TuningRegistryExit,G,OUT_EAX|OUT_EDX|OUT_FPU){Observe o(m);return m.ret(0,rx_tuning_registry_exit());}
