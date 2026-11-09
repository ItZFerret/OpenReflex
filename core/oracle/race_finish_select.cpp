/* Candidate adapters only. All masks exactly native/game/plan/live.tsv10005.
 * FxBind exposes structural MOV outputs and real dependency EDX/x87 state;
 * none of these integer helpers manufactures or clears x87 outputs. */
#include "fx_oracle.h"
extern "C" {
#include "../game/race_finish_select.h"
}
using namespace reflex_port;
#define FINISH_PORT(address,name) \
    static uint32_t finish_body_##name(reflex_vm::Machine& m); \
    RX_PORT(address,name,"rx_RaceFinishSelect",OUT_EAX|OUT_EDX|OUT_FPU) { \
        FxBind observer(m); return finish_body_##name(m); \
    } \
    static uint32_t finish_body_##name(reflex_vm::Machine& m)
FINISH_PORT(0x0074a070,FinishSelectAdvance) { m.r[0]=rx_finish_select_advance(m.r[0],m.r[1]);return m.ret(0); }
FINISH_PORT(0x0074a110,FinishSelectNext) { m.r[0]=rx_finish_select_next(m.r[1]);return m.ret(0); }
FINISH_PORT(0x0074f0f0,FinishSelectPrevious) { m.r[0]=rx_finish_select_previous(m.r[1]);return m.ret(0); }
FINISH_PORT(0x0074f1a0,FinishSelectLocalFlags) { rx_finish_select_local_flags(m.r[0]);return m.ret(0); }
FINISH_PORT(0x0074fad0,FinishSelectCancel) { m.r[0]=rx_finish_select_cancel(m.r[1]);return m.ret(0); }
