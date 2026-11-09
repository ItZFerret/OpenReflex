// anim_oracle.h -- ANIM_PORT: RX_PORT that also binds the running machine for the animation code's register access
// (anim_reg_get / anim_reg_set, game/anim_extern.h): the frame macros push / restore the original's frame pointer (EBP) and the
// wrappers start the originals they call from the current callee-saved registers, as the original code would.
#pragma once
#include "oracle.h"

struct AnimBind { reflex_vm::Machine* prev; explicit AnimBind(reflex_vm::Machine& m); ~AnimBind(); };

#define ANIM_PORT(addr, name, group, outputs)                                                 \
    static uint32_t animport_##name(reflex_vm::Machine& m);                                   \
    RX_PORT(addr, name, group, outputs) { AnimBind ab_(m); return animport_##name(m); }       \
    static uint32_t animport_##name(reflex_vm::Machine& m)
