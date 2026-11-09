/* parity_pair.inc -- w-parity (development only, not in librxcore): one log line per CollisionPair_CompositeVsComposite 008732d0 call,
 * written by both sides of the paired ride (the oracle: core/oracle/parity_probe.cpp rx_ParityCompositeLog around the original; the
 * product: core/tools/parity/scratch_build.sh with patch_pairlog.py / patch_displog.py and parity_hook.c) so the first call whose
 * result differs can be found. The includer defines PAR_RD(a) (a guest word). Heap pointers differ between the two sides, so only
 * values are logged: "C <seq> sw <swap> n <count before> pA .. qA .. pB .. qB .. (raw words) kA <children of A> kB <children of B>
 * sh <FNV of the 4 KB of stack below the entry ESP>" at the entry, "c <seq> n <count after> r <each new record's point / normal /
 * depth words> ia <ids A> ib <ids B>" after it. */
static void parity_pair_entry(FILE* f, unsigned long seq, uint32_t esp, uint32_t M, uint32_t A, uint32_t qA, uint32_t pA, uint32_t B,
                              uint32_t qB, uint32_t pB, uint32_t swap) {
    fprintf(f, "C %lu sw %u n %u pA", seq, swap & 0xff, PAR_RD(M));
    for (uint32_t k = 0; k < 16; k += 4) fprintf(f, " %08x", PAR_RD(pA + k));
    fprintf(f, " qA"); for (uint32_t k = 0; k < 16; k += 4) fprintf(f, " %08x", PAR_RD(qA + k));
    fprintf(f, " pB"); for (uint32_t k = 0; k < 16; k += 4) fprintf(f, " %08x", PAR_RD(pB + k));
    fprintf(f, " qB"); for (uint32_t k = 0; k < 16; k += 4) fprintf(f, " %08x", PAR_RD(qB + k));
    fprintf(f, " kA %d kB %d", (int)((PAR_RD(A + 0x30) - PAR_RD(A + 0x2c)) >> 2), (int)((PAR_RD(B + 0x30) - PAR_RD(B + 0x2c)) >> 2));
    uint32_t h = 2166136261u;
    for (uint32_t a = esp - 0x1000; a < esp; a += 4) { h ^= PAR_RD(a); h *= 16777619u; }
    fprintf(f, " sh %08x\n", h);
}
static void parity_pair_exit(FILE* f, unsigned long seq, uint32_t M, uint32_t before, uint32_t swap) {
    const uint32_t n = PAR_RD(M);
    fprintf(f, "c %lu n %u r", seq, n);
    for (uint32_t i = before; i < n && i < 0x1e; i++)
        for (uint32_t k = 0x10; k < 0x40; k += 4) fprintf(f, " %08x", PAR_RD(M + 0x30 * i + k));
    const uint32_t ia = M + ((swap & 0xff) ? 0x628 : 0x5b0), ib = M + ((swap & 0xff) ? 0x5b0 : 0x628);
    fprintf(f, " ia"); for (uint32_t i = before; i < n && i < 0x1e; i++) fprintf(f, " %u", PAR_RD(ia + 4 * i));
    fprintf(f, " ib"); for (uint32_t i = before; i < n && i < 0x1e; i++) fprintf(f, " %u", PAR_RD(ib + 4 * i));
    fprintf(f, "\n");
}

/* Every Collision_ContactDispatch 0085ddd0 call (nested ones included) whose A or B position lies within PARITY_NEAR=x,z,r (feet,
 * horizontal) of a point: "D <seq> d <depth> sw <swap> n <count> tA <A type +8> tB <B type +8> pA .. qA .. pB .. qB ..", then
 * "d <seq> n <count after> r <new records>". */
static int parity_near(uint32_t p) {
    static int init; static float x, z, r2;
    if (!init) { init = 1; const char* e = getenv("PARITY_NEAR"); float rr = 0; if (!e || sscanf(e, "%f,%f,%f", &x, &z, &rr) != 3) rr = -1; r2 = rr < 0 ? -1 : rr * rr; }
    if (r2 < 0) return 0;
    uint32_t bx = PAR_RD(p), bz = PAR_RD(p + 8); float fx, fz; memcpy(&fx, &bx, 4); memcpy(&fz, &bz, 4);
    return (fx - x) * (fx - x) + (fz - z) * (fz - z) <= r2;
}
static void parity_dispatch_entry(FILE* f, unsigned long seq, int depth, uint32_t M, uint32_t A, uint32_t qA, uint32_t pA, uint32_t B,
                                  uint32_t qB, uint32_t pB, uint32_t swap) {
    fprintf(f, "D %lu d %d sw %u n %u tA %u tB %u pA", seq, depth, swap & 0xff, PAR_RD(M), PAR_RD(A + 8), PAR_RD(B + 8));
    for (uint32_t k = 0; k < 16; k += 4) fprintf(f, " %08x", PAR_RD(pA + k));
    fprintf(f, " qA"); for (uint32_t k = 0; k < 16; k += 4) fprintf(f, " %08x", PAR_RD(qA + k));
    fprintf(f, " pB"); for (uint32_t k = 0; k < 16; k += 4) fprintf(f, " %08x", PAR_RD(pB + k));
    fprintf(f, " qB"); for (uint32_t k = 0; k < 16; k += 4) fprintf(f, " %08x", PAR_RD(qB + k));
    fprintf(f, " mA %08x %08x %08x mB %08x %08x %08x", PAR_RD(A + 0xc), PAR_RD(A + 0x10), PAR_RD(A + 0x14), PAR_RD(B + 0xc), PAR_RD(B + 0x10),
            PAR_RD(B + 0x14));   /* w-parity4w: the dispatcher's reject / group masks */
    fprintf(f, "\n");
}
static void parity_dispatch_exit(FILE* f, unsigned long seq, uint32_t M, uint32_t before) {
    const uint32_t n = PAR_RD(M);
    fprintf(f, "d %lu n %u r", seq, n);
    for (uint32_t i = before; i < n && i < 0x1e; i++)
        for (uint32_t k = 0x10; k < 0x40; k += 4) fprintf(f, " %08x", PAR_RD(M + 0x30 * i + k));
    fprintf(f, "\n");
}
