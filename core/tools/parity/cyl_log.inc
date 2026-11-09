/* cyl_log.inc -- w-parity2 (development only): one line per CollisionPair_CylinderVsCylinder 0086bd80 call whose A position lies
 * within PARITY_NEAR (parity_pair.inc's filter; the includer includes that first): "CY <seq> esp <ESP at the entry, return address
 * on top> pa <A position> st <the 0x400 bytes below ESP and 0x20 above, words>" -- the dead stack the generator and its kernels
 * (0081c590 / 0081beb0 / 00846f10) may read. Shared by core/oracle/parity_probe.cpp (rx_ParityCylLog) and the scratch product probe
 * (core/tools/parity/patch_cyllog.py). */
static unsigned long cy_seq;
#ifdef CY_WRITE
/* development test (the product side only): PARITY_CYFIX=<oracle CY log> PARITY_CYIDX=<n> [PARITY_CYLO / PARITY_CYHI = offsets from
 * ESP, hex, default -400 / 0]: at the n-th logged call, the oracle's stack words of that call written into [ESP+LO, ESP+HI).
 * w-aidrift: PARITY_CYIDX=n[,n...] several calls; PARITY_CYIDX=all every logged call (while the two call sequences are the same) */
static int cy_fix_wanted(unsigned long seq) {
    const char* t = getenv("PARITY_CYIDX"); if (!t) return 0;
    if (!strcmp(t, "all")) return 1;
    while (*t) { char* e; const unsigned long x = strtoul(t, &e, 10); if (e == t) break; if (x == seq) return 1; t = *e == ',' ? e + 1 : e; }
    return 0;
}
static void cy_fix(unsigned long seq, uint32_t esp) {
    const char* fn = getenv("PARITY_CYFIX"); if (!fn || !cy_fix_wanted(seq)) return;
    const long lo = getenv("PARITY_CYLO") ? strtol(getenv("PARITY_CYLO"), NULL, 16) : -0x400, hi = getenv("PARITY_CYHI") ? strtol(getenv("PARITY_CYHI"), NULL, 16) : 0;
    FILE* in = fopen(fn, "r"); if (!in) return;
    static char line[1 << 16];
    while (fgets(line, sizeof line, in)) {
        unsigned long s2; if (sscanf(line, "CY %lu", &s2) != 1 || s2 != seq) continue;
        char* p = strstr(line, " st "); if (!p) break; p += 4;
        for (long o = -0x400; o < 0x20; o += 4) {
            char* e; const uint32_t w = (uint32_t)strtoul(p, &e, 16); if (e == p) break; p = e;
            if (o >= lo && o < hi) CY_WRITE(esp + (uint32_t)o, w);
        }
        break;
    }
    fclose(in);
}
#endif
static void cy_entry(FILE* f, uint32_t esp, uint32_t pa) {
    if (!f || !parity_near(pa)) return;
#ifdef CY_WRITE
    cy_fix(cy_seq, esp);
#endif
    fprintf(f, "CY %lu esp %08x pa %08x %08x %08x st", cy_seq++, esp, PAR_RD(pa), PAR_RD(pa + 4), PAR_RD(pa + 8));
    for (uint32_t a = esp - 0x400; a < esp + 0x20; a += 4) fprintf(f, " %08x", PAR_RD(a));
    fprintf(f, "\n");
}
