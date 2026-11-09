// anim_oracle.cpp -- the register access of the animation code over the emulated machine (anim_oracle.h).
#include "anim_oracle.h"

namespace { thread_local reflex_vm::Machine* g_anim_m = nullptr; }
AnimBind::AnimBind(reflex_vm::Machine& m) : prev(g_anim_m) { g_anim_m = &m; }
AnimBind::~AnimBind() { g_anim_m = prev; }

extern "C" {
uint32_t anim_reg_get(int i) { return g_anim_m ? g_anim_m->r[i] : 0; }
void anim_reg_set(int i, uint32_t v) { if (g_anim_m && i != 4) g_anim_m->r[i] = v; }
}
