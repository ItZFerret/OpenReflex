// fx_oracle.h -- FX_PORT: RX_PORT that binds the machine for the effects code's register model (fx_reg / fx_set_reg, fx_util.h).
#pragma once
#include "oracle.h"
struct FxBind { reflex_vm::Machine* prev; explicit FxBind(reflex_vm::Machine& m); ~FxBind(); };
#define FX_PORT(addr, name, outputs)                                                          \
    static uint32_t fxport_body_##name(reflex_vm::Machine& m);                                \
    RX_PORT(addr, name, "rx_fx", outputs) { FxBind fb_(m); return fxport_body_##name(m); }    \
    static uint32_t fxport_body_##name(reflex_vm::Machine& m)
