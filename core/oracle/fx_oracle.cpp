// fx_oracle.cpp -- the effects code's register model over the emulated machine (fx_oracle.h): EBX / EBP / ESI / EDI are the
// machine's (RX_PORT restores them after the outermost port).
#include "fx_oracle.h"
namespace { thread_local reflex_vm::Machine* g_fx_m = nullptr; }
FxBind::FxBind(reflex_vm::Machine& m) : prev(g_fx_m) { g_fx_m = &m; }
FxBind::~FxBind() { g_fx_m = prev; }
extern "C" {
uint32_t fx_reg(int r) { return g_fx_m ? g_fx_m->r[r] : 0; }
void fx_set_reg(int r, uint32_t v) { if (g_fx_m && r != 4) g_fx_m->r[r] = v; }
}
