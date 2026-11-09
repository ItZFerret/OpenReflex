/* rand_log.inc -- w-parity2 (development only): one line per AI track-node reaction call (005118c0, the driver's lane roll draws the
 * game LCG) with the game LCG at its entry: "RN <seq> slot <player slot> seed <0x9cb860> count <0x9cb864>", shared by
 * core/oracle/parity_probe.cpp (rx_ParityRandLog) and the scratch product probe (core/tools/parity/patch_randlog.py). PAR_RD as
 * ik_log.inc. */
#include <stdio.h>
#include <stdint.h>
static unsigned long rn_seq;
static void rn_entry(FILE* f, uint32_t d) {
    if (!f) return;
    const uint32_t G = PAR_RD(0x00a946b8), P = PAR_RD(d + 0x74);
    const int slot = G && P >= G + 0x7460 ? (int)((P - G - 0x7460) / 0x12a0) : -1;
    fprintf(f, "RN %lu slot %d seed %08x count %u\n", rn_seq++, slot, PAR_RD(0x009cb860), PAR_RD(0x009cb864));
}
