// race_oracle.h -- shared checks of the rx_race adapters (development only).
#pragma once
#include "oracle.h"
#include <cstdio>
// The x87 code of the race layer is ported at the main thread's 53-bit precision (control word bits 8-9 = 10): report once if a
// call runs with another precision.
inline void race_check_pc53(reflex_vm::Machine& m, const char* who) {
    static int warned = 0;
    if (((m.fpu_cw >> 8) & 3) != 2 && warned++ < 3) std::fprintf(stderr, "[rx_race] %s: x87 control word %04x (not 53-bit)\n", who, m.fpu_cw);
}
