/* lcg_watch.c -- w-parity2 (development only): the rx_watch_access hook of a -DRX_WATCH product build (core/plan/QUEUE.md
 * w-jobthreads4 gives the build): every write of the game LCG's draw count 0x9cb864 while the count is in
 * [LCG_WATCH_LO, LCG_WATCH_HI] (decimal) prints the count and the host backtrace to stderr ("LCGW <count> | frames"). */
#include "rx_mem.h"
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
int rx_watch_on = 1;
void rx_watch_access(rx_addr a, uint32_t n, int write) {
    if (!write || a != 0x009cb864u || n != 4) return;
    static long lo = -1, hi = -1;
    if (lo < 0) { lo = getenv("LCG_WATCH_LO") ? atol(getenv("LCG_WATCH_LO")) : 0; hi = getenv("LCG_WATCH_HI") ? atol(getenv("LCG_WATCH_HI")) : 0; }
    uint32_t c; memcpy(&c, rx_base + a, 4);
    if ((long)c < lo || (long)c > hi) return;
    void* bt[16]; const int k = backtrace(bt, 16);
    char** s = backtrace_symbols(bt, k);
    fprintf(stderr, "LCGW %u |", c);
    for (int i = 1; i < k && i < 10; i++) { const char* p = s[i]; const char* q = p; while (*q) q++; while (q > p && q[-1] != ' ' ) q--; (void)q; fprintf(stderr, " %s", s[i] + 59 > s[i] ? s[i] + 59 : s[i]); }
    fprintf(stderr, "\n");
    free(s);
}
void rx_watch_mark(int kind, rx_addr arg) { (void)kind; (void)arg; }
