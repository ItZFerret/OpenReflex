/* gridpair_line.inc -- one line of the paired product-vs-original ride trace (w-gridpair; development only, not in librxcore).
 * Included by the product driver core/tools/bike_parity_probe.c (PROBE_PAIR) and the oracle probe core/oracle/gridpair_probe.cpp
 * (rx_PairFrameTrace), so both write exactly the same fields in the same order. The includer defines
 *   PAIR_RD(a)   a 32-bit guest word at address a (0 when unmapped)
 * and passes the frame number and the main thread's CRT _holdrand. Every float is its raw 32-bit word.
 *   P <frame> st <raw activity state A+0x21c> ms <MXGame state [0xd728d8]+0x18> gt <engine clock 0xd728d0+0x3c> gs <g_GameRandSeed> gc <g_GameRandCount> rs <_holdrand>
 *     zone <local P+0x1dc> L <the local MX simulation, bike_parity_probe's trace_line fields without dt, or "-">
 *     R <slot>:<x> <y> <z> <vx> <vy> <vz> <C+68 .. C+74> <air> <wx> <wy> <wz> ... (every other MX rider's S+0xf0 / S+0x100, its
 *     controls, S+8d98 and S+740) */
/* PAIR_RESEED (both drivers, a test hook). Two parts, both at the start of a frame:
 *  - the event's Pre (the frame before ended in activity state 0, after the UI world's activity was seen in state 5): both generators
 *    set to constants (CRT _holdrand 0x2545f491, game LCG seed 0x9e3779b9), as the framecmp REFLEX_SEED / rng_frames RNG_SEED_PRE do:
 *    the intro's draws differ in order between the two (w-gridpair), and the countdown's length is a CRT draw at its entry (00565040);
 *  - from the frame after the countdown entry (the first frame ending in state 1 after one ending in 0) on: from k = frames since that
 *    entry, CRT k * 0x9e3779b9 + 1, LCG k * 0x85ebca6b + 7, as rng_frames.c / rng_probe.cpp RNG_RESEED.
 * Then both sides draw every frame from the same state whatever the draw counts before were. pair_next_seed closes a frame (its end
 * state st) and returns 1 with the seeds for the next frame's start, 0 for none. */
/* PAIR_RESEED_EVENT=1 (w-lcgdraws, with RESEED; pair.sh EVENT=1): also the event's first frames, before Pre: the frame after the last
 * loading frame (MXGame state [0xd728d8]+0x18 = 0xb) and every frame after one that ended in activity state 5 start from the Pre
 * constants. Those frames run the event's emitters for the first time (their fractional spawn counts S+0x98 start there) from a seed the
 * load's schedule decides (the front end's emitters draw while the original loads on its loader thread, w-rng): unpinned, the two
 * sides' carries differ from then on and so does every later frame's particle count. PAIR_RESEED_EVENT=only: the pin at the event's
 * first frame alone, no later reseed (Pre's and the countdown's included): both sides run on from the same seeds. */
/* PAIR_RESEED_GRID=1 (w-parity4w, with RESEED; pair.sh GRID=1): also every frame after one that ended in the loading page's MXGame state
 * 6 (before the event's first frame) starts from constants (CRT 0x6a09e667, LCG 0xbb67ae85), so the grid pass (0075f890, in the frame that
 * leaves state 6; its race branch draws the local StartZone from the game LCG) draws from the same state on both sides. The loading page's
 * emitters draw every frame of the wait for the accept (the original's boot accepts after 30 frames, the product after 1): on NAT_Bedford
 * the two sides' grid passes drew the local zones 12 / 8 (20 ft apart); NAT_Concord's two states happen to give zone 5 on both. */
static int pair_k = -1, pair_prev_st = -2, pair_seen5 = 0;
static int pair_next_seed(int st, uint32_t* crt, uint32_t* lcg) {
    if (pair_k < 0 && st == 1 && pair_prev_st == 0) pair_k = 0;
    pair_prev_st = st;
    if (getenv("PAIR_RESEED_GRID") && pair_k < 0 && !pair_seen5 && PAIR_RD(PAIR_RD(0x00d728d8) + 0x18) == 6) {
        *crt = 0x6a09e667u; *lcg = 0xbb67ae85u; return 1;
    }
    const char* ev = getenv("PAIR_RESEED_EVENT");
    if (ev && !strcmp(ev, "only")) {   /* the event-start pin alone: no later reseed, both sides run on from the same state */
        if (pair_seen5 || pair_k >= 0) return 0;
        if (PAIR_RD(PAIR_RD(0x00d728d8) + 0x18) == 0xb) { pair_seen5 = 1; *crt = 0x2545f491u; *lcg = 0x9e3779b9u; return 1; }
        return 0;
    }
    if (pair_k < 0 && ev) {
        const uint32_t ms = PAIR_RD(PAIR_RD(0x00d728d8) + 0x18);
        if (st == 5 || (ms == 0xb && !pair_seen5)) { if (st == 5) pair_seen5 = 1; *crt = 0x2545f491u; *lcg = 0x9e3779b9u; return 1; }
    }
    if (st == 5) pair_seen5 = 1;
    if (pair_k >= 0) {
        pair_k++;
        *crt = (uint32_t)pair_k * 0x9e3779b9u + 1u; *lcg = (uint32_t)pair_k * 0x85ebca6bu + 7u;
        return 1;
    }
    if (pair_seen5 && st == 0) { *crt = 0x2545f491u; *lcg = 0x9e3779b9u; return 1; }
    return 0;
}
static int pair_state(void) {
    const uint32_t G = PAIR_RD(0x00a946b8);
    if (!G) return -1;
    const uint32_t cnt = PAIR_RD(G + 0x1541c) & 0xff;
    for (uint32_t i = 0; i < cnt && i < 8; i++) {
        const uint32_t slot = PAIR_RD(G + 0x153ec + 4 * i);
        if (slot >= 16) continue;
        const uint32_t p = G + 0x7460 + slot * 0x12a0;
        if (PAIR_RD(p + 0x10) != 0) continue;
        const uint32_t A = PAIR_RD(p + 0x94);
        return A ? (int)PAIR_RD(A + 0x21c) : -1;
    }
    return -1;
}

/* w-parity4w: the four-wheel simulations (ATV RavenSimulationATV 0x93d054, the UTV / Buggy / TrophyTruck family 0x93d43c / 0x93d24c /
 * 0x93c93c: build_vehicle_wheeled.c, the Buggy's state after the parameter block 0x10 lower). Per class: the object's size, the body
 * count (RavenSimulation bodies at S+0xf0+0xa0 i: position +0, velocity +0x10, orientation +0x640, angular velocity +0x650), the four
 * wheel contact records (the class 0x150 bytes the MX's S+0x8790 / +0x88e0 are: +0x80..+0x88 the contact scalars, +0xc0 / +0xc4 the
 * grip flags, +0xe0 the suspension force, +0x100 active, +0x104 on the ground) front left, front right, rear left, rear right
 * (sound_state.c wheel_fill), the four wheel spins, gear and engine speed (rx_session.c k_vehicle_classes), the vehicle V's size (the
 * factory's allocation, _re/notes/atv.md / build_vehicle_wheeled.c). NULL: not a four-wheel sim. */
typedef struct pair_fw_class { uint32_t vt, size, bodies, wheel[4], spin, gear, rpm, vsize; const char* name; } pair_fw_class;
static const pair_fw_class* pair_fw(uint32_t S) {
    static const pair_fw_class k[4] = {
        {0x0093d054u, 0x9bd0, 6, {0x9140, 0x9290, 0x93e0, 0x9530}, 0x9680, 0x96f0, 0x96ec, 0x1770, "atv"},
        {0x0093d43cu, 0x8850, 5, {0x8080, 0x81d0, 0x8320, 0x8470}, 0x85c0, 0x8660, 0x865c, 0x15a0, "utv"},
        {0x0093d24cu, 0x8840, 5, {0x8070, 0x81c0, 0x8310, 0x8460}, 0x85b0, 0x8650, 0x864c, 0x1620, "buggy"},
        {0x0093c93cu, 0x8930, 5, {0x8080, 0x81d0, 0x8320, 0x8470}, 0x85c0, 0x8664, 0x865c, 0x1630, "truck"}};
    const uint32_t vt = S ? PAIR_RD(S) : 0;
    for (int i = 0; i < 4; i++) if (k[i].vt == vt) return &k[i];
    return NULL;
}
/* "Q <frame> <counter hi> <counter lo>": the performance counter Engine::UpdateTiming last took (engine 0xd728d0 + 0x68), whose
 * value / 10000 is the game's millisecond clock phase (timeGetTime): written with every line when PAIR_QPC is set */
static void pair_qpc(FILE* f, unsigned long frame) {   /* + the network session clock's offset / last value (0x9dfa88 / 0x9dfaa8, int64; 00892170) */
    fprintf(f, "Q %lu %08x %08x net %08x %08x %08x %08x\n", frame, PAIR_RD(0x00d728d0 + 0x6c), PAIR_RD(0x00d728d0 + 0x68),
            PAIR_RD(0x009dfa8c), PAIR_RD(0x009dfa88), PAIR_RD(0x009dfaac), PAIR_RD(0x009dfaa8));
}

/* w-damagetick (a test stimulus, both sides): PAIR_KPOKE=k:addr:word[,...] guest words written after the line of the frame k frames
 * after the countdown entry (k = 0: the first frame ending in activity state 1 after one ending in 0, gridpair_cmp.py's k), so the
 * two sides poke between the same two updates whatever their front-end frame counts. Each driver calls it after its pair_line. */
static void pair_kpoke(unsigned long frame, void (*wr)(uint32_t, uint32_t)) {
    static long k0 = -1; static int prev = -2;
    const int st = pair_state();
    if (k0 < 0 && st == 1 && prev == 0) k0 = (long)frame;
    prev = st;
    if (k0 < 0) return;
    const long k = (long)frame - k0;
    for (const char* pk = getenv("PAIR_KPOKE"); pk && *pk; ) {
        char* e; const long kk = strtol(pk, &e, 10); if (e == pk || *e != ':') break;
        const unsigned long a = strtoul(e + 1, &e, 16); if (*e != ':') break;
        const unsigned long w = strtoul(e + 1, &e, 16);
        if (kk == k) wr((uint32_t)a, (uint32_t)w);
        pk = *e == ',' ? e + 1 : e;
    }
}

/* w-damagetick (PAIR_DMG=1, both sides): the vehicle damage manager 0xdb1410 (12 entries of 0x410 bytes from +0x10) and every slot's
 * vehicle. "DV <frame> <slot>:<V+194 type> <matrix translation V+120..128> ..." then per entry with a vehicle (+0x3fc)
 * "DM <frame> <entry> slot <the vehicle's slot> n <+0> st <+0xc..> hp <+0x110..> q <+0x130..+0x370: rest / current orientations, hinge
 * angles +0x230, velocities +0x250, node positions +0x270> a <+0x3d0..+0x3f8: active flags, part timers> hit <+0x3f8> <+0x400>
 * | per part: the body record (entity +0x158 -> 00418f30: world +0x1994 +0xc + 0xe0 index) en <+0xb0 byte> k <+0x80> and the part
 * node's local matrix (the node's index in the vehicle model +0x1328's +0x10 set: H+0x18 + 0x40 i)". Words only, no pointers. */
static void pair_dmg_line(FILE* f, unsigned long frame) {
    const uint32_t G = PAIR_RD(0x00a946b8);
    fprintf(f, "DV %lu", frame);
    for (uint32_t k = 0; G && k < 16; k++) {
        const uint32_t V = PAIR_RD(G + 0x7460 + k * 0x12a0 + 0x1ac);
        if (V) fprintf(f, " %u:%u %08x %08x %08x", k, PAIR_RD(V + 0x194), PAIR_RD(V + 0x120), PAIR_RD(V + 0x124), PAIR_RD(V + 0x128));
    }
    fprintf(f, "\n");
    for (uint32_t i = 0; i < 12; i++) {
        const uint32_t e = 0x00db1410u + 0x10 + 0x410 * i, V = PAIR_RD(e + 0x3fc);
        if (!V) continue;
        int slot = -1;
        for (uint32_t k = 0; G && k < 16; k++) if (PAIR_RD(G + 0x7460 + k * 0x12a0 + 0x1ac) == V) slot = (int)k;
        const uint32_t n = PAIR_RD(e);
        fprintf(f, "DM %lu %u slot %d n %u st", frame, i, slot, n);
        for (uint32_t o = 0xc; o < 0x2c; o += 4) fprintf(f, " %u", PAIR_RD(e + o));
        fprintf(f, " hp"); for (uint32_t o = 0x110; o < 0x130; o += 4) fprintf(f, " %d", (int)PAIR_RD(e + o));
        fprintf(f, " q"); for (uint32_t o = 0x130; o < 0x370; o += 4) fprintf(f, " %08x", PAIR_RD(e + o));
        fprintf(f, " a"); for (uint32_t o = 0x3d0; o < 0x3f8; o += 4) fprintf(f, " %08x", PAIR_RD(e + o));
        fprintf(f, " hit %08x %08x |", PAIR_RD(e + 0x3f8), PAIR_RD(e + 0x400));
        const uint32_t models = PAIR_RD(V + 0x1328), rec = models ? PAIR_RD(models + 0x10) : 0, H = rec ? PAIR_RD(rec) : 0;
        for (uint32_t p = 0; p < n && p < 8; p++) {
            const uint32_t ent = PAIR_RD(e + 0x2c + 4 * p), h = ent ? PAIR_RD(ent + 0x158) : 0;
            uint32_t body = 0;
            if (h) {
                const uint32_t w = PAIR_RD(h + 4), idx = PAIR_RD(h + 8), cnt = (PAIR_RD(w + 0x19a4) - PAIR_RD(w + 0x19a0)) / 0xe0;
                if (idx < cnt) body = idx * 0xe0 + PAIR_RD(w + 0x1994 + 0xc);
            }
            fprintf(f, " p%u en %u k %d s %08x", p, body ? PAIR_RD(body + 0xb0) & 0xff : 0xffu, body ? (int)PAIR_RD(body + 0x80) : -2,
                    ent ? PAIR_RD(ent + 0x15c) : 0);   /* w-parity4w: the part entity's +0x15c enabled / +0x15d soft bytes */
            {   /* w-parity4w: the part's composite shape (E+0x4c + 8p: handle {vtable, shape}): its +0xc..+0x14 masks and each box child's
                 * half extents (+0x20, w lane included: the parse 00479680 leaves the frame's stale word there) */
                const uint32_t sh = PAIR_RD(e + 0x4c + 8 * p + 4);
                fprintf(f, " sh");
                if (sh) {
                    fprintf(f, " %08x %08x %08x", PAIR_RD(sh + 0xc), PAIR_RD(sh + 0x10), PAIR_RD(sh + 0x14));
                    const uint32_t c0 = PAIR_RD(sh + 0x2c), c1 = PAIR_RD(sh + 0x30);
                    for (uint32_t c = c0; c && c < c1 && c < c0 + 64; c += 4) {
                        const uint32_t b = PAIR_RD(c);
                        fprintf(f, " :"); for (uint32_t k = 0x20; k < 0x30; k += 4) fprintf(f, " %08x", PAIR_RD(b + k));
                    }
                }
            }
            const uint32_t node = PAIR_RD(e + 0x370 + 4 * p);
            const uint32_t ni = H && node >= PAIR_RD(H + 8) ? (node - PAIR_RD(H + 8)) / 16 : 0xffffffffu;
            fprintf(f, " m");
            if (H && ni < PAIR_RD(H + 0xc)) for (uint32_t o = 0; o < 0x40; o += 4) fprintf(f, " %08x", PAIR_RD(PAIR_RD(H + 0x18) + 0x40 * ni + o));
            else fprintf(f, " -");
        }
        fprintf(f, "\n");
    }
}

/* w-parity: "O <frame> <index> <words>" for every generic dynamic object of the physics world (world +0x1960, 0xe0-byte records;
 * the world found through the first MX simulation's external object S+2ebc -> handle +4 -> body +4): the record's float part
 * +0x40 .. +0xd0 (orientation, position, velocity, angular velocity, contact results, accumulators), w lanes included. PAIR_DYNOBJ=1. */
static void pair_dynobj(FILE* f, unsigned long frame) {
    const uint32_t G = PAIR_RD(0x00a946b8);
    if (!G) return;
    uint32_t world = 0;
    for (uint32_t k = 0; k < 16 && !world; k++) {
        const uint32_t V = PAIR_RD(G + 0x7460 + k * 0x12a0 + 0x1ac), S = V ? PAIR_RD(V + 0x234) : 0;
        if (!S || (PAIR_RD(S) != 0x0093c8a4u && !pair_fw(S))) continue;   /* w-parity4w: or a four-wheel sim (the RavenSimulation base's) */
        const uint32_t h = PAIR_RD(S + 0x2ebc), b = h ? PAIR_RD(h + 4) : 0;
        world = b ? PAIR_RD(b + 4) : 0;
    }
    if (!world) return;
    const uint32_t base = PAIR_RD(world + 0x1960), n = (PAIR_RD(world + 0x1964) - base) / 0xe0;
    for (uint32_t i = 0; i < n && i < 512; i++) {
        fprintf(f, "O %lu %u", frame, i);
        for (uint32_t o = 0x40; o < 0xd0; o += 4) fprintf(f, " %08x", PAIR_RD(base + i * 0xe0 + o));
        fprintf(f, "\n");
    }
}

/* w-parity: "VT <frame> <slot>:<vehicle> <V+0x1558> <V+0x155c> ..." the vehicles' shake timers (world_veh_raven.c), PAIR_VTIMERS=1 */
static void pair_vtimers(FILE* f, unsigned long frame) {
    const uint32_t G = PAIR_RD(0x00a946b8);
    if (!G) return;
    fprintf(f, "VT %lu", frame);
    for (uint32_t k = 0; k < 16; k++) {
        const uint32_t V = PAIR_RD(G + 0x7460 + k * 0x12a0 + 0x1ac);
        if (V) fprintf(f, " %u:%08x %08x %08x", k, V, PAIR_RD(V + 0x1558), PAIR_RD(V + 0x155c));
    }
    fprintf(f, "\n");
}

/* w-parity2: "RT <frame> <slot> <R> <words>" for every MX rider: its RiderAnimation (P+0x1b4) from +0x5b300 to the end (+0x5bb80: the
 * noise wobble records +0x5b300 .. +0x5b510, the nod timer / start +0x5b514 / +0x5b518, the lag filters +0x5b984 .. +0x5bab4 the
 * locomotion runs, the timers +0x5b830 / +0x5b920), after the frames of PAIR_RTAIL=f[,f...] */
static void pair_rtail(FILE* f, unsigned long frame) {
    int want = 0;
    for (const char* t = getenv("PAIR_RTAIL"); t && *t; ) { char* e; const unsigned long x = strtoul(t, &e, 10); if (e == t) break; want |= x == frame; t = *e == ',' ? e + 1 : e; }
    const uint32_t G = PAIR_RD(0x00a946b8);
    if (!want || !G) return;
    for (uint32_t k = 0; k < 16; k++) {
        const uint32_t R = PAIR_RD(G + 0x7460 + k * 0x12a0 + 0x1b4);
        if (!R) continue;
        fprintf(f, "RT %lu %u %08x", frame, k, R);
        for (uint32_t o = 0x5b300; o < 0x5bb80; o += 4) fprintf(f, " %08x", PAIR_RD(R + o));
        fprintf(f, "\n");
    }
}

/* w-parity2: "VN <frame> <slot> <V> <set> <n> | per node: <local matrix 16 words> <world matrix 16 words> <bounds 0x50 bytes> <node record
 * 4 words>" the vehicle's model node set (the record 004302f0 picks: V+0x1328 +0x10 / +0x50 by the timing's +0x54), after the frames of
 * PAIR_VNODES=f[,f...], for the slots of PAIR_VNODES_SLOT (a bit mask, default all) */
static void pair_vnodes(FILE* f, unsigned long frame) {
    int want = 0;
    for (const char* t = getenv("PAIR_VNODES"); t && *t; ) { char* e; const unsigned long x = strtoul(t, &e, 10); if (e == t) break; want |= x == frame; t = *e == ',' ? e + 1 : e; }
    const uint32_t G = PAIR_RD(0x00a946b8);
    if (!want || !G) return;
    const uint32_t mask = getenv("PAIR_VNODES_SLOT") ? (uint32_t)strtoul(getenv("PAIR_VNODES_SLOT"), NULL, 16) : 0xffffu;
    const uint32_t timing = PAIR_RD(PAIR_RD(0x00a95844) + 0x10);
    for (uint32_t k = 0; k < 16; k++) {
        const uint32_t V = PAIR_RD(G + 0x7460 + k * 0x12a0 + 0x1ac), models = V ? PAIR_RD(V + 0x1328) : 0;
        if (!models || !((mask >> k) & 1)) continue;
        const uint32_t rec = PAIR_RD(models + (PAIR_RD(timing + 0x54) == 0 ? 0x10 : 0x50)), H = rec ? PAIR_RD(rec) : 0;
        const uint32_t n = H ? PAIR_RD(H + 0xc) : 0;
        fprintf(f, "VN %lu %u %08x %08x %u", frame, k, V, H, n);
        for (uint32_t i = 0; i < n && i < 64; i++) {
            const uint32_t nr = PAIR_RD(H + 8) + 16u * i, ix = PAIR_RD(nr + 8) >> 24;
            fprintf(f, " |");
            for (uint32_t o = 0; o < 0x40; o += 4) fprintf(f, " %08x", PAIR_RD(PAIR_RD(H + 0x18) + 0x40u * ix + o));
            for (uint32_t o = 0; o < 0x40; o += 4) fprintf(f, " %08x", PAIR_RD(PAIR_RD(H + 0x1c) + 0x40u * ix + o));
            for (uint32_t o = 0; o < 0x50; o += 4) fprintf(f, " %08x", PAIR_RD(PAIR_RD(H + 0x20) + 0x50u * ix + o));
            for (uint32_t o = 0; o < 0x10; o += 4) fprintf(f, " %08x", PAIR_RD(nr + o));
        }
        fprintf(f, "\n");
    }
}

/* the local four-wheel sim's L fields: "cls <vtable> off <S+2374> in <S+1c..5c> pos v q w (body 0) b <bodies 1.. : position, velocity,
 * orientation, angular velocity> wh <per wheel: +100 +104 +c0 +c4 +80 +84 +88 +e0> spin <4> gear rpm h <FNV-1a 64 of the whole object>" */
static void pair_fw_local(FILE* f, uint32_t S, const pair_fw_class* c) {
    fprintf(f, " L cls %08x off %u in", c->vt, PAIR_RD(S + 0x2374));
    for (uint32_t o = 0x1c; o < 0x5c; o += 4) fprintf(f, " %08x", PAIR_RD(S + o));
    fprintf(f, " pos"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", PAIR_RD(S + 0xf0 + 4 * k));
    fprintf(f, " v"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", PAIR_RD(S + 0x100 + 4 * k));
    fprintf(f, " q"); for (uint32_t k = 0; k < 4; k++) fprintf(f, " %08x", PAIR_RD(S + 0x730 + 4 * k));
    fprintf(f, " w"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", PAIR_RD(S + 0x740 + 4 * k));
    fprintf(f, " b");
    for (uint32_t i = 1; i < c->bodies; i++) {
        const uint32_t B = S + 0xf0 + 0xa0 * i;
        for (uint32_t k = 0; k < 6; k++) fprintf(f, " %08x", PAIR_RD(B + 4 * (k < 3 ? k : k + 1)));
        for (uint32_t k = 0; k < 4; k++) fprintf(f, " %08x", PAIR_RD(B + 0x640 + 4 * k));
        for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", PAIR_RD(B + 0x650 + 4 * k));
    }
    fprintf(f, " wh");
    for (int w = 0; w < 4; w++) {
        static const uint32_t o[8] = {0x100, 0x104, 0xc0, 0xc4, 0x80, 0x84, 0x88, 0xe0};
        for (int k = 0; k < 8; k++) fprintf(f, k < 4 ? " %u" : " %08x", PAIR_RD(S + c->wheel[w] + o[k]));
    }
    fprintf(f, " spin"); for (uint32_t k = 0; k < 4; k++) fprintf(f, " %08x", PAIR_RD(S + c->spin + 4 * k));
    fprintf(f, " gear %u rpm %08x", PAIR_RD(S + c->gear), PAIR_RD(S + c->rpm));
    uint64_t h = 1469598103934665603ull;
    for (uint32_t o = 0; o < c->size; o += 4) { h ^= PAIR_RD(S + o); h *= 1099511628211ull; }
    fprintf(f, " h %016llx", (unsigned long long)h);
}
/* an AI four-wheel sim's R entry: position, velocity, the input block S+1c..5c, angular velocity, orientation, the wheels' +100 / +104,
 * gear */
static void pair_fw_rider(FILE* f, uint32_t s, const pair_fw_class* c) {
    for (uint32_t o = 0; o < 6; o++) fprintf(f, "%s%08x", o ? " " : "", PAIR_RD(s + (o < 3 ? 0xf0 + 4 * o : 0x100 + 4 * (o - 3))));
    for (uint32_t o = 0x1c; o < 0x5c; o += 4) fprintf(f, " %08x", PAIR_RD(s + o));
    for (uint32_t o = 0; o < 3; o++) fprintf(f, " %08x", PAIR_RD(s + 0x740 + 4 * o));
    for (uint32_t o = 0; o < 4; o++) fprintf(f, " %08x", PAIR_RD(s + 0x730 + 4 * o));
    for (int w = 0; w < 4; w++) fprintf(f, " %u %u", PAIR_RD(s + c->wheel[w] + 0x100), PAIR_RD(s + c->wheel[w] + 0x104));
    fprintf(f, " %u", PAIR_RD(s + c->gear));
}

static void pair_line(FILE* f, unsigned long frame, uint32_t holdrand) {
    if (getenv("PAIR_RTAIL")) pair_rtail(f, frame);
    if (getenv("PAIR_VNODES")) pair_vnodes(f, frame);
    if (getenv("PAIR_WORDS")) {   /* w-parity2: "W <frame> <word>..." the guest words at PAIR_WORDS=addr[,addr...] (hex), every frame */
        fprintf(f, "W %lu", frame);
        for (const char* t = getenv("PAIR_WORDS"); *t; ) { char* e; const unsigned long a = strtoul(t, &e, 16); if (e == t) break; fprintf(f, " %08x", PAIR_RD(a)); t = *e == ',' ? e + 1 : e; }
        fprintf(f, "\n");
    }
    if (getenv("PAIR_QPC")) pair_qpc(f, frame);
    if (getenv("PAIR_SEQ")) {   /* w-parity4w: "SQ <frame> <i>:<count> <end> <clock> <name> ..." the local's activity's 12 scripted sequences
                                 * (0055e260: A+0x1070+0x15190 i: +0x15180 events, +0x15184 end, +0x15188 clock; names at A+0xfe330+0x104 i)
                                 * and the scenario A+0xfd8's clock words +0x15180..+0x1518c */
        const uint32_t G0 = PAIR_RD(0x00a946b8), A0 = G0 ? PAIR_RD(G0 + 0x7460 + 0x94) : 0;
        fprintf(f, "SQ %lu", frame);
        for (uint32_t i = 0; A0 && i < 12; i++) {
            const uint32_t q = A0 + 0x1070 + 0x15190 * i, nm = A0 + 0xfe330 + 0x104 * i;
            if (!PAIR_RD(q + 0x15180)) continue;
            char name[24]; for (uint32_t k = 0; k < 23; k++) { const uint32_t w = PAIR_RD((nm + k) & ~3u); name[k] = (char)(w >> (8 * ((nm + k) & 3))); if (!name[k]) break; } name[23] = 0;
            fprintf(f, " %u:%u %08x %08x %s", i, PAIR_RD(q + 0x15180), PAIR_RD(q + 0x15184), PAIR_RD(q + 0x15188), name);
        }
        fprintf(f, "\n");
    }
    if (getenv("PAIR_VTIMERS")) pair_vtimers(f, frame);
    if (getenv("PAIR_MUD")) {   /* w-parity4w: "MU <frame> <world> <mud manager +0x80 .. +0x130>": the level world's MudManager (G+0x5a54 or the UI
                                 * world G+0x20c8, +0x2214c) whose +0xe8 + 4 m are the per-material mud rates (MudRecord_Update 00408c50) */
        const uint32_t G0 = PAIR_RD(0x00a946b8), ctx = PAIR_RD(0x00a95844);
        const uint32_t lw = !G0 || !ctx ? 0 : PAIR_RD(PAIR_RD(ctx + 0x20) + 0x34) == 1 ? PAIR_RD(G0 + 0x5a54) : PAIR_RD(PAIR_RD(ctx + 0x1c) + 0x34) == 1 ? PAIR_RD(G0 + 0x20c8) : 0;
        const uint32_t mm = lw ? PAIR_RD(lw + 0x2214c) : 0;
        fprintf(f, "MU %lu %u", frame, lw ? 1u : 0u);
        {   /* the mud clock [0xa89bc4] (the records update when it passes [0x931d94]) and record 0 (0xa96d90: amount, wetness, vehicle,
             * the vehicle's +0x150 speed) */
            const uint32_t V0 = PAIR_RD(0x00a96d90 + 0x30);
            fprintf(f, " clk %08x r0 %08x %08x %08x sp %08x", PAIR_RD(0x00a89bc4), PAIR_RD(0x00a96d90), PAIR_RD(0x00a96da0), V0, V0 ? PAIR_RD(V0 + 0x150) : 0);
            fprintf(f, " vv");   /* the vehicle block V+0xb0 .. +0x170 (rotation, position, model matrix, speed, vf37 velocity) */
            for (uint32_t o = 0xb0; V0 && o < 0x170; o += 4) fprintf(f, " %08x", PAIR_RD(V0 + o));
        }
        for (uint32_t o = 0x80; mm && o < 0x130; o += 4) fprintf(f, " %08x", PAIR_RD(mm + o));
        {   /* the active terrain's material grid ([terrain +0x44ac] +0x44, TerrainMaterialAt 007c5120): its first 0x80 bytes and an FNV of
             * each level's packed samples */
            const uint32_t scene = PAIR_RD(0x00a95844), id = scene ? PAIR_RD(scene + 0x1c + 4 * PAIR_RD(scene + 4)) : 0;
            uint32_t T = 0;
            for (uint32_t k = 0; k < PAIR_RD(0x009e30dc) && k < 4; k++) { const uint32_t t = PAIR_RD(0x009e30d4 + 4 * k); if (t && PAIR_RD(t + 0x44a8) == id) T = t; }
            const uint32_t gr = T ? PAIR_RD(PAIR_RD(T + 0x44ac) + 0x44) : 0;
            fprintf(f, " grid");
            for (uint32_t o = 0; gr && o < 0x80; o += 4) fprintf(f, " %08x", PAIR_RD(gr + o));
            for (uint32_t L = 0; gr && L < 4; L++) {   /* PackedGrid_SampleUV 007f8e00: width float +0x48 [L], stride +0x38 [L], bits +0x68 [L],
                                                         * data +0x88 [L] */
                const uint32_t d = PAIR_RD(gr + 0x88 + 4 * L), wb = PAIR_RD(gr + 0x48 + 4 * L);
                float w; memcpy(&w, &wb, 4);
                const uint32_t bytes = (uint32_t)(w * w) * PAIR_RD(gr + 0x68 + 4 * L) / 8u;
                uint64_t h = 1469598103934665603ull;
                for (uint32_t k = 0; d && k + 4 <= bytes && k < 0x1000000; k += 4) { h ^= PAIR_RD(d + k); h *= 1099511628211ull; }
                fprintf(f, " L%u %x %016llx", L, bytes, (unsigned long long)h);
            }
        }
        fprintf(f, "\n");
    }
    if (getenv("PAIR_VCTL")) {   /* w-parity4w: "VC <frame> <slot>:<V+0x1290 .. +0x12b0> ..." the vehicles' control records (0045f530: the AI
                                  * driver's controls; until the driver's first write +0xc is the heap's old word in the original) */
        const uint32_t G0 = PAIR_RD(0x00a946b8);
        fprintf(f, "VC %lu", frame);
        for (uint32_t k = 0; G0 && k < 16; k++) {
            const uint32_t V0 = PAIR_RD(G0 + 0x7460 + k * 0x12a0 + 0x1ac);
            if (!V0) continue;
            fprintf(f, " %u:", k);
            for (uint32_t o = 0; o < 0x24; o += 4) fprintf(f, "%s%08x", o ? " " : "", PAIR_RD(V0 + 0x1290 + o));
        }
        fprintf(f, "\n");
    }
    if (getenv("PAIR_DYNOBJ")) pair_dynobj(f, frame);
    const uint32_t G = PAIR_RD(0x00a946b8);
    uint32_t P = 0;
    if (G) {
        const uint32_t cnt = PAIR_RD(G + 0x1541c) & 0xff;
        for (uint32_t i = 0; i < cnt && i < 8; i++) {
            const uint32_t slot = PAIR_RD(G + 0x153ec + 4 * i);
            if (slot >= 16) continue;
            const uint32_t p = G + 0x7460 + slot * 0x12a0;
            if (PAIR_RD(p + 0x10) == 0) { P = p; break; }
        }
    }
    const uint32_t A = P ? PAIR_RD(P + 0x94) : 0;
    const uint32_t MG = PAIR_RD(0x00d728d8);
    fprintf(f, "P %lu st %d ms %d gt %08x gs %08x gc %u rs %08x zone %d", frame, A ? (int)PAIR_RD(A + 0x21c) : -1, MG ? (int)PAIR_RD(MG + 0x18) : -1,
            PAIR_RD(0x00d728d0 + 0x3c), PAIR_RD(0x009cb860), PAIR_RD(0x009cb864), holdrand, P ? (int)PAIR_RD(P + 0x1dc) : -99);
    const uint32_t V = P ? PAIR_RD(P + 0x1ac) : 0, S = V ? PAIR_RD(V + 0x234) : 0;
    if (S && PAIR_RD(S) == 0x0093c8a4u) {
        const uint32_t C = PAIR_RD(S + 0x5c30);
        fprintf(f, " L air %u first %u c", PAIR_RD(S + 0x8d98), PAIR_RD(S + 0x8d9c));
        for (uint32_t o = 0x68; o <= 0x74; o += 4) fprintf(f, " %08x", C ? PAIR_RD(C + o) : 0);
        fprintf(f, " w"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", PAIR_RD(S + 0x740 + 4 * k));
        fprintf(f, " q"); for (uint32_t k = 0; k < 4; k++) fprintf(f, " %08x", PAIR_RD(S + 0x730 + 4 * k));
        fprintf(f, " flt"); for (uint32_t k = 0; k < 15; k++) fprintf(f, " %08x", PAIR_RD(S + 0x8b78 + 4 * k));
        fprintf(f, " fade %08x %08x t %08x", PAIR_RD(S + 0x8bd4), PAIR_RD(S + 0x8bd8), PAIR_RD(S + 0x8b74));
        fprintf(f, " g %u %u %u %u", PAIR_RD(S + 0x8d40), PAIR_RD(S + 0x8d44), PAIR_RD(S + 0x8d58), PAIR_RD(S + 0x8d5c));
        fprintf(f, " pos"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", PAIR_RD(S + 0xf0 + 4 * k));
        fprintf(f, " v"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", PAIR_RD(S + 0x100 + 4 * k));
        static const uint32_t W[2] = {0x8790, 0x88e0};
        for (int w = 0; w < 2; w++) {
            fprintf(f, " %s %u %u %u", w ? "front" : "rear", PAIR_RD(S + W[w] + 0x100), PAIR_RD(S + W[w] + 0xc0), PAIR_RD(S + W[w] + 0xc4));
            for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", PAIR_RD(S + W[w] + 0x80 + 4 * k));
        }
        fprintf(f, " grip %08x %08x gear %u rpm %08x", PAIR_RD(S + 0x88a0), PAIR_RD(S + 0x88a4), PAIR_RD(S + 0x8ac0), PAIR_RD(S + 0x8abc));
    } else if (pair_fw(S)) pair_fw_local(f, S, pair_fw(S));
    else fprintf(f, " L -");
    fprintf(f, " R");
    for (uint32_t k = 0; G && k < 16; k++) {
        const uint32_t p = G + 0x7460 + k * 0x12a0;
        if (p == P) continue;
        const uint32_t v = PAIR_RD(p + 0x1ac), s = v ? PAIR_RD(v + 0x234) : 0;
        if (s && pair_fw(s)) { fprintf(f, " %u:", k); pair_fw_rider(f, s, pair_fw(s)); continue; }   /* w-parity4w */
        if (!s || PAIR_RD(s) != 0x0093c8a4u) continue;
        fprintf(f, " %u:", k);
        for (uint32_t o = 0; o < 6; o++) fprintf(f, "%s%08x", o ? " " : "", PAIR_RD(s + (o < 3 ? 0xf0 + 4 * o : 0x100 + 4 * (o - 3))));
        {   /* its controls C+0x68..0x74, the airborne flag and the angular velocity (w-gridpair round 2: the AI's first divergence) */
            const uint32_t c = PAIR_RD(s + 0x5c30);
            for (uint32_t o = 0x68; o <= 0x74; o += 4) fprintf(f, " %08x", c ? PAIR_RD(c + o) : 0);
            fprintf(f, " %08x", PAIR_RD(s + 0x8d98));
            for (uint32_t o = 0; o < 3; o++) fprintf(f, " %08x", PAIR_RD(s + 0x740 + 4 * o));
        }
    }
    fprintf(f, "\n");
    if (getenv("PAIR_DMG")) pair_dmg_line(f, frame);
}

/* the local MX simulation's spec line and its 0x8fc0 bytes ("SPEC ..." / "S <frame> slot <k> S <addr> words ..."): RX_PAIR_SIMDUMP
 * (oracle) / PROBE_PAIR_SIMDUMP (product) = frame, for a word-by-word comparison of the two local bikes */
static void pair_simdump(FILE* f, unsigned long frame, int want) {   /* want: a player slot (P = G + 0x7460 + slot * 0x12a0), -1 = the local */
    const uint32_t G = PAIR_RD(0x00a946b8);
    const uint32_t cnt = G ? (want >= 0 ? 1 : (PAIR_RD(G + 0x1541c) & 0xff)) : 0;
    for (uint32_t i = 0; i < cnt && i < 8; i++) {
        const uint32_t slot = want >= 0 ? (uint32_t)want : PAIR_RD(G + 0x153ec + 4 * i), p = G + 0x7460 + slot * 0x12a0;
        if (slot >= 16 || (want < 0 && PAIR_RD(p + 0x10) != 0)) continue;
        const uint32_t V = PAIR_RD(p + 0x1ac), S = V ? PAIR_RD(V + 0x234) : 0;
        const uint32_t vn = PAIR_RD(G + 0x154dc), va = PAIR_RD(G + 0x154d8);
        uint32_t e = 0;
        for (uint32_t j = 0; j < vn && j < 64; j++) { const uint32_t x = PAIR_RD(va + 4 * j); if (x && PAIR_RD(x + 4) == PAIR_RD(p + 0x1b0)) { e = x; break; } }
        fprintf(f, "SPEC %lu slot %u model %d owner %d ovid %d tuning", frame, slot, e && PAIR_RD(e + 8) ? (int)PAIR_RD(PAIR_RD(e + 8)) : -1,
                e ? (int)PAIR_RD(e + 0x98) : -1, e ? (int)PAIR_RD(e + 0x9c) : -1);
        for (uint32_t o = 0xa0; e && o < 0xc0; o += 4) fprintf(f, " %d", (int)PAIR_RD(e + o));
        fprintf(f, " parts");
        for (uint32_t j = 0; e && j < 17; j++) { const uint32_t pt = PAIR_RD(e + 0xc + 4 * j); fprintf(f, " %d", pt ? (int)PAIR_RD(pt + 0x40) : -1); }
        fprintf(f, "\nS %lu slot %u S %08x words", frame, slot, S);
        const uint32_t ssize = pair_fw(S) ? pair_fw(S)->size : 0x8fc0u;   /* w-parity4w: a four-wheel sim's whole object */
        for (uint32_t o = 0; S && o < ssize; o += 4) fprintf(f, " %08x", PAIR_RD(S + o));
        fprintf(f, "\n");
        {   /* the slot's rider P+0x1b4 (0x3000 bytes) and its vehicle V (0x1600 bytes), the same "S"-style record with kinds R / V */
            const uint32_t R = PAIR_RD(p + 0x1b4);
            fprintf(f, "R %lu slot %u S %08x words", frame, slot, R);
            const uint32_t rsize = getenv("PAIR_RSIZE") ? (uint32_t)strtoul(getenv("PAIR_RSIZE"), NULL, 16) : 0x3000u;   /* w-parity: the whole RiderAnimation is 0x5b600 */
            for (uint32_t o = 0; R && o < rsize; o += 4) fprintf(f, " %08x", PAIR_RD(R + o));
            /* the sim's nearby collision object descriptors (S+0x130c .. +0x1310, 0x50 bytes each: bounds +0x20 / +0x30, surface +0x40,
             * object +0x44) and each object's first 0x40 words: "D <frame> slot <k> S <first> words ..." */
            const uint32_t d0 = S ? PAIR_RD(S + 0x130c) : 0, d1 = S ? PAIR_RD(S + 0x1310) : 0;
            fprintf(f, "\nD %lu slot %u S %08x words", frame, slot, d0);
            for (uint32_t d = d0; d && d < d1 && d < d0 + 0x50 * 32; d += 0x50) {
                for (uint32_t o = 0; o < 0x50; o += 4) fprintf(f, " %08x", PAIR_RD(d + o));
                const uint32_t ob = PAIR_RD(d + 0x44);
                for (uint32_t o = 0; o < 0x100; o += 4) fprintf(f, " %08x", ob ? PAIR_RD(ob + o) : 0);
            }
            {   /* the slot's race driver (the activity's driver list A+0x268 / +0x26c, the one whose +0x74 is P), 0x1400 bytes: "A" */
                const uint32_t Ac = PAIR_RD(p + 0x94), n = Ac ? PAIR_RD(Ac + 0x26c) : 0, list = Ac ? PAIR_RD(Ac + 0x268) : 0;
                uint32_t dr = 0;
                for (uint32_t q = 0; list && q < n && q < 32; q++) { const uint32_t x = PAIR_RD(list + 4 * q); if (x && PAIR_RD(x + 0x74) == p) { dr = x; break; } }
                fprintf(f, "\nA %lu slot %u S %08x words", frame, slot, dr);
                for (uint32_t o = 0; dr && o < 0x1400; o += 4) fprintf(f, " %08x", PAIR_RD(dr + o));
            }
            if (getenv("PAIR_RSKEL") && R) {   /* w-parity: the rider's skeleton R+0x5bc (0x100 bytes) and its bones (+0x64, +0x60 of 0x80 bytes) */
                const uint32_t K = PAIR_RD(R + 0x5bc), nb = K ? PAIR_RD(K + 0x60) : 0, B = K ? PAIR_RD(K + 0x64) : 0;
                fprintf(f, "\nK %lu slot %u S %08x words", frame, slot, K);
                for (uint32_t o = 0; K && o < 0x100; o += 4) fprintf(f, " %08x", PAIR_RD(K + o));
                for (uint32_t o = 0; B && o < nb * 0x80 && o < 0x10000; o += 4) fprintf(f, " %08x", PAIR_RD(B + o));
            }
            fprintf(f, "\nV %lu slot %u S %08x words", frame, slot, V);
            for (uint32_t o = 0; V && o < (pair_fw(S) ? pair_fw(S)->vsize : 0x1600u); o += 4) fprintf(f, " %08x", PAIR_RD(V + o));
            fprintf(f, "\n");
            if (getenv("PAIR_SIMDUMP_NODES")) {   /* w-parity4w: the node tables of the vehicle's two model node sets (V+0x1328 +0x10 / +0x50:
                                                   * H+0x18 local matrices, +0x1c world, +0x20 bounds, n = H+0xc) and of the rider's hierarchy
                                                   * (R+0x180: +0x18 / +0x1c / +0x20), "M1L / M1W / M1B / M5L / M5W / M5B / RHL / RHW / RHB" */
                const uint32_t models = V ? PAIR_RD(V + 0x1328) : 0;
                for (int set = 0; set < 3; set++) {
                    const uint32_t H = set == 2 ? (R ? R + 0x180 : 0) : models ? PAIR_RD(models + (set ? 0x50 : 0x10)) : 0;
                    const uint32_t HH = set == 2 ? H : H ? PAIR_RD(H) : 0, n = HH ? PAIR_RD(HH + 0xc) : 0;
                    static const char* const nm[3] = {"M1", "M5", "RH"};
                    for (int t = 0; t < 3 && HH && n <= 128; t++) {
                        const uint32_t base = PAIR_RD(HH + 0x18 + 4 * t), sz = t == 2 ? 0x50u : 0x40u;
                        fprintf(f, "%s%c %lu slot %u S %08x words", nm[set], "LWB"[t], frame, slot, base);
                        for (uint32_t o = 0; base && o < n * sz; o += 4) fprintf(f, " %08x", PAIR_RD(base + o));
                        fprintf(f, "\n");
                    }
                }
            }
        }
        return;
    }
}

/* the active terrain's base height grid as per-row FNV-1a hashes plus its header words (+0x4480..+0x4500): RX_PAIR_TERRAIN /
 * PROBE_PAIR_TERRAIN = frame. "T <frame> <terrain> hdr <words> rows <stride> <rows> <hash>..." (w-gridpair round 2) */
static void pair_terrain_dump(FILE* f, unsigned long frame) {
    const uint32_t scene = PAIR_RD(0x00a95844), id = scene ? PAIR_RD(scene + 0x1c + 4 * PAIR_RD(scene + 4)) : 0;
    uint32_t T = 0;
    for (uint32_t k = 0; k < PAIR_RD(0x009e30dc) && k < 4; k++) { const uint32_t t = PAIR_RD(0x009e30d4 + 4 * k); if (t && PAIR_RD(t + 0x44a8) == id) T = t; }
    fprintf(f, "T %lu %08x hdr", frame, T);
    for (uint32_t o = 0x4480; T && o < 0x4500; o += 4) fprintf(f, " %08x", PAIR_RD(T + o));
    fprintf(f, " gain %08x", T ? PAIR_RD(T + 0xbc) : 0);
    const uint32_t base = T ? PAIR_RD(T + 0x1c) : 0, stride = T ? PAIR_RD(T + 0x44bc) : 0;
    float cz; { const uint32_t w = T ? PAIR_RD(T + 0x44cc) : 0; memcpy(&cz, &w, 4); }
    const uint32_t rows = base && stride && stride <= 4096 ? (uint32_t)cz : 0;
    fprintf(f, " rows %u %u", stride, rows);
    for (uint32_t z = 0; z < rows && z < 4096; z++) {
        uint64_t h = 1469598103934665603ull;
        for (uint32_t x = 0; x < stride; x++) { h ^= PAIR_RD(base + 4 * (z * stride + x)); h *= 1099511628211ull; }
        fprintf(f, " %016llx", (unsigned long long)h);
    }
    fprintf(f, "\n");
}

/* w-hfterrain: the inputs BehaviorSeperate's target pick (00531a70) reads of every MX rider: "Bd <frame> <slot>:<P+11a0> <V+c0..c8>
 * <V+150> <V+160..168> <V+194> <V+198> c <bounds centre x y z radius (00533510 rx_vehicle_bound_centre, the collision model's root
 * bounds through the matrix V+f0)> m <V+f0 .. V+12c> t <V> <V+1558> <V+155c (004674e0's shake timers)> ...". The includer defines PAIR_CENTRE(vehicle, out): the four words at out
 * (guest memory) from rx_vehicle_bound_centre. RX_PAIR_BOUNDS (oracle) / PROBE_PAIR_BOUNDS (product) = frame[,frame...]. */
#ifdef PAIR_CENTRE
static void pair_bounds_dump(FILE* f, unsigned long frame, uint32_t out) {
    const uint32_t G = PAIR_RD(0x00a946b8);
    fprintf(f, "Bd %lu", frame);
    for (uint32_t k = 0; G && k < 16; k++) {
        const uint32_t p = G + 0x7460 + k * 0x12a0, V = PAIR_RD(p + 0x1ac);
        if (!V) continue;
        fprintf(f, " %u:%08x", k, PAIR_RD(p + 0x11a0));
        for (uint32_t o = 0xc0; o < 0xcc; o += 4) fprintf(f, " %08x", PAIR_RD(V + o));
        fprintf(f, " %08x", PAIR_RD(V + 0x150));
        for (uint32_t o = 0x160; o < 0x16c; o += 4) fprintf(f, " %08x", PAIR_RD(V + o));
        {   /* the model 004302f0 bounds (timing +0x54 picks models +0x10 / +0x50 of V+0x1328), its record's flags byte +0xc and the
             * cached subtree box (bounds +0x20 centre, +0x30 extents, +0x44 radius) before the call */
            const uint32_t timing = PAIR_RD(PAIR_RD(0x00a95844) + 0x10), models = PAIR_RD(V + 0x1328);
            const uint32_t rec = models ? PAIR_RD(models + (PAIR_RD(timing + 0x54) == 0 ? 0x10 : 0x50)) : 0;
            const uint32_t b = rec ? PAIR_RD(PAIR_RD(rec) + 0x20) + 0x50u * ((PAIR_RD(rec + 8) >> 24) & 0xff) : 0;
            fprintf(f, " f %u %08x %02x", PAIR_RD(timing + 0x54), rec, rec ? (PAIR_RD(rec + 0xc) & 0xff) : 0);
            for (uint32_t o = 0x20; o < 0x48; o += 4) fprintf(f, " %08x", b ? PAIR_RD(b + o) : 0);
            /* the subtree below the record (first child +0xe, next sibling +0xd, node records 16 bytes at H+8; 0xff = none): its node
             * count, the nodes with a box (extents not the 0xd61670 marker) and the nodes whose flags have bit 2 (box valid) set */
            uint32_t n = 0, boxed = 0, valid = 0, stack[64], sp = 0;
            const uint32_t H = rec ? PAIR_RD(rec) : 0;
            if (H && ((PAIR_RD(rec + 0xc) >> 16) & 0xff) != 0xff) stack[sp++] = PAIR_RD(H + 8) + 16u * ((PAIR_RD(rec + 0xc) >> 16) & 0xff);
            while (sp && n < 512) {
                const uint32_t r = stack[--sp]; n++;
                const uint32_t nb = PAIR_RD(H + 0x20) + 0x50u * ((PAIR_RD(r + 8) >> 24) & 0xff);
                if (PAIR_RD(nb + 0x10) != PAIR_RD(0x00d61670) || PAIR_RD(nb + 0x14) != PAIR_RD(0x00d61674)) boxed++;
                if (PAIR_RD(r + 0xc) & 4) valid++;
                const uint32_t c = (PAIR_RD(r + 0xc) >> 16) & 0xff, sib = (PAIR_RD(r + 0xc) >> 8) & 0xff;
                if (sib != 0xff && sp < 64) stack[sp++] = PAIR_RD(H + 8) + 16u * sib;
                if (c != 0xff && sp < 64) stack[sp++] = PAIR_RD(H + 8) + 16u * c;
            }
            fprintf(f, " sub %u %u %u of %d", n, boxed, valid, (int)PAIR_RD(H + 0xc));
        }
        fprintf(f, " %u %u c", PAIR_RD(V + 0x194), PAIR_RD(V + 0x198));
        PAIR_CENTRE(V, out);
        for (uint32_t o = 0; o < 0x10; o += 4) fprintf(f, " %08x", PAIR_RD(out + o));
        fprintf(f, " m");
        for (uint32_t o = 0xf0; o < 0x130; o += 4) fprintf(f, " %08x", PAIR_RD(V + o));
        fprintf(f, " t %08x %08x %08x", V, PAIR_RD(V + 0x1558), PAIR_RD(V + 0x155c));   /* 004674e0's two shake timers */
    }
    fprintf(f, "\n");
    if (getenv("PAIR_RSYNC")) {   /* w-rendersync: the render sync's object lists of every scene's render container (scene + 0x1a18):
                                   * "Rs <frame> <slot> <scene> act <+0x34> n <+0x10b8> <+0x10cc> <+0x10e0> <+0x11dc> | per +0x10b4 entry
                                   * <object> <vtable> <vf25 target> <+0x14> <+0x18> <the renderable's vtable> | per +0x10c8 entry <object> <vtable> <vf25 target>" */
        const uint32_t ctx = PAIR_RD(0x00a95844);
        for (uint32_t s = 0x1c; ctx && s < 0x28; s += 4) {
            const uint32_t sc = PAIR_RD(ctx + s), c = sc + 0x1a18;
            if (!sc) continue;
            fprintf(f, "Rs %lu %x %08x act %u n %u %u %u %u |", frame, s, sc, PAIR_RD(sc + 0x34), PAIR_RD(c + 0x10b8), PAIR_RD(c + 0x10cc),
                    PAIR_RD(c + 0x10e0), PAIR_RD(c + 0x11dc));
            for (uint32_t i = 0; i < PAIR_RD(c + 0x10b8) && i < 4096; i++) {
                const uint32_t e = PAIR_RD(c + 0x10b4) + 0x20u * i, o = PAIR_RD(e + 0x10);
                const uint32_t g = PAIR_RD(PAIR_RD(o) + 0x64), r = g == 0x00401ef0u ? o + 0x90 : g == 0x00430290u ? o + 0x94 : 0;
                fprintf(f, " %08x %08x %08x %08x %08x %08x", o, PAIR_RD(o), g, PAIR_RD(e + 0x14), PAIR_RD(e + 0x18), r ? PAIR_RD(r) : 0);
                if (getenv("PAIR_RSYNC_POS")) fprintf(f, " @ %08x %08x %08x", PAIR_RD(o + 0x60), PAIR_RD(o + 0x64), PAIR_RD(o + 0x68));   /* the entity's matrix translation */
            }
            fprintf(f, " |");
            for (uint32_t i = 0; i < PAIR_RD(c + 0x10cc) && i < 4096; i++) {
                const uint32_t e = PAIR_RD(c + 0x10c8) + 0x20u * i, o = PAIR_RD(e + 0x10);
                fprintf(f, " %08x %08x %08x", o, PAIR_RD(o), PAIR_RD(PAIR_RD(o) + 0x64));
            }
            fprintf(f, "\n");
        }
    }
    if (getenv("PAIR_BOUNDS_NODES")) {   /* per rider: both node sets of the vehicle model (+0x10 the source, +0x50 the render copy):
                                          * "Nd <frame> <slot> <set> <count> tables <H+0x18> <H+0x1c> l <local matrices> w <world matrices>" */
        for (uint32_t k = 0; G && k < 16; k++) {
            const uint32_t V = PAIR_RD(G + 0x7460 + k * 0x12a0 + 0x1ac), models = V ? PAIR_RD(V + 0x1328) : 0;
            for (uint32_t set = 0x10; models && set <= 0x50; set += 0x40) {
                const uint32_t rec = PAIR_RD(models + set), H = rec ? PAIR_RD(rec) : 0;
                const int n = H ? (int)PAIR_RD(H + 0xc) : 0;
                fprintf(f, "Nd %lu %u %x %d tables %08x %08x l", frame, k, set, n, H ? PAIR_RD(H + 0x18) : 0, H ? PAIR_RD(H + 0x1c) : 0);
                for (int i = 0; i < n && i < 64; i++) for (uint32_t o = 0; o < 0x40; o += 4) fprintf(f, " %08x", PAIR_RD(PAIR_RD(H + 0x18) + 0x40u * (uint32_t)i + o));
                fprintf(f, " w");
                for (int i = 0; i < n && i < 64; i++) for (uint32_t o = 0; o < 0x40; o += 4) fprintf(f, " %08x", PAIR_RD(PAIR_RD(H + 0x1c) + 0x40u * (uint32_t)i + o));
                fprintf(f, "\n");
            }
        }
    }
}
#endif
