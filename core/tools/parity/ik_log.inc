/* ik_log.inc -- w-parity2 (development only): one line per InverseKinematics two-bone limb solve (006b0eb0) whose target pointer
 * (limb +0x10) is PARITY_IKTGT (hex; unset: every limb), shared by core/oracle/parity_probe.cpp (rx_ParityIKLog) and the scratch
 * product probe (core/tools/parity/patch_iklog.py). The includer defines PAR_RD(a) (a guest word).
 *   "IE <seq> fr <frame word> limb <limb> tgt <ptr> <x y z> eng <+8> t <+0x40> dur <+0x70> off <+0x30 .. +0x38> j <root mid eff>
 *    [pose <bone words: PARITY_IKPOSE bones of 0x30 bytes>]"  at the entry,
 *   "IX <seq> off <+0x30 .. +0x38> t <+0x40> [pose ...]"  after the call. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static unsigned long ik_seq;
static int ik_want(uint32_t limb) {
    const char* t = getenv("PARITY_IKTGT");
    return !t || !*t || PAR_RD(limb + 0x10) == (uint32_t)strtoul(t, NULL, 16);
}
static void ik_pose(FILE* f, uint32_t pose) {
    const char* n = getenv("PARITY_IKPOSE");
    const uint32_t nb = n ? (uint32_t)atoi(n) : 0;
    if (!nb) return;
    fprintf(f, " pose");
    for (uint32_t o = 0; o < nb * 0x30; o += 4) fprintf(f, " %08x", PAR_RD(pose + o));
}
/* PARITY_IKTREE=1: the slot-0 rider's animation tree (RiderAnimation +0x144 -> skeleton +0x224), every animator reached through a
 * word of an animator's first 0x100 bytes (vtables of the RTTI's Animator classes), breadth first: "AT <seq> <n> <addr> <words>" */
static int ik_is_animator(uint32_t p) {
    static const uint32_t vt[] = {0x975f50, 0x975f1c, 0x95ef44, 0x95efa0, 0x95efd8, 0x975f84, 0x975ee8, 0x976024, 0x95f00c, 0x975fb8};
    if (p < 0x10000000u || p >= 0x20000000u || (p & 3)) return 0;   /* the heap (the product faults on unmapped pages) */
    const uint32_t v = PAR_RD(p);
    for (unsigned i = 0; i < sizeof vt / sizeof vt[0]; i++) if (v == vt[i]) return 1;
    return 0;
}
static int ik_kids(uint32_t a, uint32_t* k) {   /* an animator's children (structural, the evaluators' fields) */
    const uint32_t v = PAR_RD(a); int n = 0;
    if (v == 0x95efd8) k[n++] = PAR_RD(a + 0x5c);
    else if (v == 0x95f00c || v == 0x95ef44) k[n++] = PAR_RD(a + 0x28);
    else if (v == 0x95efa0) k[n++] = PAR_RD(a + 0x30);
    else if (v == 0x975f1c) { k[n++] = PAR_RD(a + 0x58); k[n++] = PAR_RD(a + 0x5c); }
    else if (v == 0x975f50) { k[n++] = PAR_RD(a + 0x2c); k[n++] = PAR_RD(a + 0x30); }
    else if (v == 0x975fb8) { const uint32_t b = PAR_RD(a + 0xc4), e = PAR_RD(a + 0xc8); for (uint32_t p = b; p < e && n < 32; p += 4) k[n++] = PAR_RD(p); }
    else if (v == 0x976024) {
        const uint32_t arr = PAR_RD(a + 0x74), c = PAR_RD(a + 0x8c);
        for (uint32_t i = 0; arr && i < c && n < 31; i++) k[n++] = PAR_RD(arr + 4 * i);
        if (ik_is_animator(PAR_RD(a + 0x84))) k[n++] = PAR_RD(PAR_RD(a + 0x84));
    }
    return n;
}
static void ik_tree_node(FILE* f, unsigned long s, uint32_t a, const char* path, int depth) {
    if (!ik_is_animator(a) || depth > 24) return;
    fprintf(f, "AT %lu %s %08x", s, path, a);
    for (uint32_t o = 0; o < 0x100; o += 4) fprintf(f, " %08x", PAR_RD(a + o));
    if (PAR_RD(a) == 0x975ee8) { const uint32_t h = PAR_RD(a + 0x28), m = ik_is_animator(a) && h >= 0x10000000u && h < 0x20000000u ? PAR_RD(h + 0x44) : 0;
        fprintf(f, " dur %08x", m >= 0x10000000u && m < 0x20000000u ? PAR_RD(m + 0x4c) : 0); }
    fprintf(f, "\n");
    uint32_t k[32]; const int n = ik_kids(a, k);
    for (int i = 0; i < n; i++) { char p[256]; snprintf(p, sizeof p, "%s.%d", path, i); if (strlen(p) < 200) ik_tree_node(f, s, k[i], p, depth + 1); }
}
static void ik_tree(FILE* f, unsigned long s) {
    if (!getenv("PARITY_IKTREE")) return;
    const uint32_t G = PAR_RD(0x00a946b8), R = G ? PAR_RD(G + 0x7460 + 0x1b4) : 0, sk = R ? PAR_RD(R + 0x144) : 0;
    if (sk) ik_tree_node(f, s, PAR_RD(sk + 0x224), "r", 0);
}
static unsigned long ik_entry(FILE* f, uint32_t limb, uint32_t pose) {
    if (!f || !ik_want(limb)) return (unsigned long)-1;
    const unsigned long s = ik_seq++;
    const uint32_t tg = PAR_RD(limb + 0x10), j = PAR_RD(limb + 0x68);
    const uint32_t scene = PAR_RD(0x00a95844), fr = scene ? PAR_RD(PAR_RD(scene + 0x10) + 0x58) : 0;
    fprintf(f, "IE %lu fr %u limb %08x tgt %08x %08x %08x %08x eng %u t %08x dur %08x off %08x %08x %08x j %u %u %u", s, fr, limb, tg,
            PAR_RD(tg), PAR_RD(tg + 4), PAR_RD(tg + 8), PAR_RD(limb + 8), PAR_RD(limb + 0x40), PAR_RD(limb + 0x70),
            PAR_RD(limb + 0x30), PAR_RD(limb + 0x34), PAR_RD(limb + 0x38), PAR_RD(j), PAR_RD(j + 4), PAR_RD(limb + 0x64));
    ik_pose(f, pose);
    fprintf(f, "\n");
    ik_tree(f, s);
    return s;
}
static void ik_exit(FILE* f, unsigned long s, uint32_t limb, uint32_t pose) {
    if (!f || s == (unsigned long)-1) return;
    fprintf(f, "IX %lu off %08x %08x %08x t %08x", s, PAR_RD(limb + 0x30), PAR_RD(limb + 0x34), PAR_RD(limb + 0x38), PAR_RD(limb + 0x40));
    ik_pose(f, pose);
    fprintf(f, "\n");
}
