// phys_contact.cpp -- oracle adapters for core/game/phys_contact.c (the narrow phase).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_contact.h"
}
using namespace phys_oracle;
using namespace reflex_port;

// 0085ddd0 (thiscall M; 7 stack args; RET 0x1c). ECX / EDX are the pair generator's leftovers (not reproduced).
RX_PORT(0x0085ddd0, Collision_ContactDispatch, "rx_phys", 0) {
    phys_contact_dispatch(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), m.arg(6));
    return m.ret(0x1c);
}
