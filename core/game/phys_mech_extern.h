/* phys_mech_extern.h -- calls of the mechanism package into functions it does not own that are not in the core yet: each one runs
 * the original through rx_call_original_ex (oracle build) with the original's register convention until its owner ports it. */
#pragma once
#include "rx_game.h"

enum { MX_EAX, MX_ECX, MX_EDX, MX_EBX, MX_ESP, MX_EBP, MX_ESI, MX_EDI };

/* Run original code at fn with the given input registers and stack arguments; returns the output registers. */
static inline rx_regs mx_call_regs(rx_addr fn, uint32_t eax, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi, int nargs,
                                   const uint32_t* args) {
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[MX_EAX] = eax; in.r[MX_ECX] = ecx; in.r[MX_EDX] = edx; in.r[MX_ESI] = esi; in.r[MX_EDI] = edi;
    memset(&out, 0, sizeof out);
    rx_call_original_ex(fn, &in, &out, nargs, args);
    return out;
}
