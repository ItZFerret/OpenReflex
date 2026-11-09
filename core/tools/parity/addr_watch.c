/* addr_watch.c -- w-parity2 (development only): the rx_watch_access hook of a -DRX_WATCH product build: every write to the guest
 * word ADDR_WATCH (hex) whose new value... is not known here (the hook runs before the store), so every write is printed with the
 * old word and the host backtrace while the word ADDR_WATCH_GATE (hex address) holds ADDR_WATCH_GATE_VAL (hex; unset: always). */
#include "rx_mem.h"
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
int rx_watch_on = 1;
void rx_watch_access(rx_addr a, uint32_t n, int write) {
    static uint32_t w = 0, gate = 0, gv = 0; static int init = 0;
    if (!init) { init = 1; w = (uint32_t)strtoul(getenv("ADDR_WATCH") ? getenv("ADDR_WATCH") : "0", NULL, 16);
        gate = (uint32_t)strtoul(getenv("ADDR_WATCH_GATE") ? getenv("ADDR_WATCH_GATE") : "0", NULL, 16);
        gv = (uint32_t)strtoul(getenv("ADDR_WATCH_GATE_VAL") ? getenv("ADDR_WATCH_GATE_VAL") : "0", NULL, 16); }
    if (!write || !(w >= a && w < a + n)) return;
    /* ADDR_WATCH_VAL (hex): the writer that stored that word is the previous one when a write finds it as the old word */
    static void* last[12]; static int lastk = 0;
    uint32_t old; memcpy(&old, rx_base + w, 4);
    if (getenv("ADDR_WATCH_VAL") && old == (uint32_t)strtoul(getenv("ADDR_WATCH_VAL"), NULL, 16) && lastk) {
        char** s = backtrace_symbols(last, lastk);
        fprintf(stderr, "AW stored by |");
        for (int i = 1; i < lastk && i < 11; i++) fprintf(stderr, " %s", s[i] + 59);
        fprintf(stderr, "\n");
        free(s);
    }
    (void)gate; (void)gv;
    lastk = backtrace(last, 12);
}
void rx_watch_mark(int kind, rx_addr arg) { (void)kind; (void)arg; }
