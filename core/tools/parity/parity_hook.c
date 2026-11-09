/* parity_hook.c -- w-parity (development only): the product side of core/tools/parity_pair.inc, linked into the scratch probe of
 * core/tools/parity/scratch_build.sh (PROBE_PARITY_LOG=<file>, PARITY_NEAR=x,z,r, PARITY_DEPTH0=1). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rx_mem.h"
#define PAR_RD(a) U32(0, (rx_addr)(a))
#include "../parity_pair.inc"
static FILE* lf(void) { static FILE* f; if (!f) { const char* p = getenv("PROBE_PARITY_LOG"); f = p && *p ? fopen(p, "w") : NULL; } return f; }
static unsigned long seq;
static uint32_t before_n, before_sw; static unsigned long cur;
void parity_pair_hook(int phase, rx_addr esp, rx_addr M, rx_addr A, rx_addr qA, rx_addr pA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap) {
    FILE* f = lf(); if (!f) return;
    if (phase == 0) { cur = seq++; before_n = U32(0, M); before_sw = swap; parity_pair_entry(f, cur, esp, M, A, qA, pA, B, qB, pB, swap); }
    else parity_pair_exit(f, cur, M, before_n, before_sw);
}
static unsigned long dseq; static int ddepth;
void parity_dispatch_hook(int phase, rx_addr M, rx_addr A, rx_addr qA, rx_addr pA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap, unsigned long* tok, uint32_t* before) {
    FILE* f = lf(); if (!f) return;
    if (phase == 0) {
        if ((!parity_near(pA) && !parity_near(pB)) || (getenv("PARITY_DEPTH0") && ddepth > 0)) { *tok = (unsigned long)-1; ddepth++; return; }
        *tok = dseq++; *before = U32(0, M); parity_dispatch_entry(f, *tok, ddepth, M, A, qA, pA, B, qB, pB, swap); ddepth++;
    } else { ddepth--; if (*tok != (unsigned long)-1) parity_dispatch_exit(f, *tok, M, *before); }
}
