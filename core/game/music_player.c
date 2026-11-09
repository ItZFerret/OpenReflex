/* music_player.c -- the MusicPlayer's game state (MXGame + 0x15c6c) on the product (w-sfx, 2026-10-27).
 *
 * The music itself (the MUSIC_IN_GAME / MUSIC_SCORES programmer events and their stream banks) is the sound engine's: the host plays it
 * (Unity OriginalMusic, from rx_session_sound's music block). What gameplay code does with the player is ported here, on the player
 * object's own fields, because two of its functions draw the game's random generator (0x9cb860, count 0x9cb864) and every later draw
 * of the game depends on them: the playlist shuffle (00677220: 2 + 4 n draws and more while two draws repeat) when the activity hands
 * the jukebox the level's track list (ActivityAudioLogic::vf35 -> 00677440), and the scenario score's pick (00677750, one draw) at the
 * race intro's StartScenarioMusic. The product's boundary stubs skipped both. The tables the music init (00675990 + the MusicAudioMap
 * loader 00675f70) fills are read from the same document (Music / TrackList / MusicAudioMap) and kept on the host side: the jukebox
 * tracks (SongName / BankName), the scores (Name / BankName), ActivityStartDelay / ActivityTransitionTimes per activity kind (the
 * MusicAudioMap tags in the order of the kind enumeration) and the scenario fades (the ScenarioMusic pairs).
 * Player fields (the original's): +4 flags (1 playing, 2, 8 fading), +8 mode (0 none, 1 stopped, 2 jukebox, 3 UI, 4 score), +0xc /
 * +0x10 / +0x14 the three event slots' states, +0x1d0 the requested track, +0x1d4 the track being loaded / played, +0x1d8 the start
 * delay, +0x1dc the activity kind, +0x398 / +0x39c the score requested / played, +0x55c / +0x560 the playlist (vector of track
 * indices, count), +0x570 its position, +0x574 / +0x578 the level's score list. Product only (the oracle runs the originals). */
#include "rx_game.h"
#include "music_player.h"
#ifndef RX_ORACLE
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include "build_world.h"
#include "build_alloc.h"
#include "../src/fmt_db.h"
#include "../src/fmt_bxml.h"

#define MAX_TRACKS 64
#define MAX_KINDS 40
static struct {
    int loaded;
    int n_tracks; char song[MAX_TRACKS][64]; char bank[MAX_TRACKS][32];
    int n_scores; char score[MAX_TRACKS][48]; char score_bank[MAX_TRACKS][32];
    float start_delay[MAX_KINDS], transition[MAX_KINDS], scenario_in[MAX_KINDS], scenario_out[MAX_KINDS];
    uint32_t serial;
} T;
/* the activity kinds' names in the enumeration's order (MusicAudioMap's tag order; OriginalMusic's table) */
static const char* const k_kinds[] = { "National", "Supercross", "Opencross", "Supermoto", "Minimoto", "Freestyle", "WaypointRace", "Endurocross",
    "FreeRide", "Tag", "Snake", "Graffiti", "SectionRace", "PokerRace", "Infection", "EggHunt", "Capture", "Terracross", "UTVMoto", "Powercross",
    "FMXJam", "BigAir", "HillClimb", "Jumper", "Pathfinder", "StepUp", "Stoppie", "Trailblazer", "HitDiscovery", "TargetPractice", "Wheelie" };
#define N_KINDS ((int)(sizeof k_kinds / sizeof *k_kinds))

static uint32_t draw(void) {   /* the game's LCG (the inlined form of every caller): the new seed */
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    const uint32_t s = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, s);
    return s;
}
static int32_t r15(uint32_t s) { return (int32_t)((s >> 16) & 0x7fff); }

static const char* attr(const fmt_bxml* d, int n, const char* a) { const char* v = fmt_bxml_attr_str(d, n, a); return v ? v : ""; }
static void floats(const fmt_bxml* d, int node, float* out, int max) {
    fmt_bxml_value v;
    if (node < 0 || fmt_bxml_node_value(d, node, &v) != 0) return;
    if (v.kind == FMT_BX_FLOAT) { for (int k = 0; k < max && k < (int)v.count; k++) memcpy(&out[k], v.data + 4 * k, 4); return; }
    if (v.kind != FMT_BX_STRING || !v.str) return;
    const char* p = v.str;
    for (int k = 0; k < max && *p; k++) {
        char* e; out[k] = strtof(p, &e);
        if (e == p) break;
        p = e; while (*p && *p != ',') p++; if (*p == ',') p++;
    }
}
static void per_kind(const fmt_bxml* d, int list, float* a, float* b) {
    for (int k = 0; list >= 0 && k < N_KINDS; k++) {
        float v[2] = {0.0f, 0.0f};
        floats(d, fmt_bxml_find_child(d, list, k_kinds[k]), v, 2);
        a[k] = v[0]; if (b) b[k] = v[1];
    }
}
/* the music init's tables (00675990 / 00675f70), once */
static void load(void) {
    if (T.loaded) return;
    struct fmt_install* in = rx_phys_props_install();
    if (!in) return;
    T.loaded = 1;
    fmt_buf b = {0};
    if (fmt_install_read_named(in, "Music", "MusicConfig", "MusicAudioMap", "bxml", &b) != 0 &&
        fmt_install_read_named(in, "Music", NULL, "MusicAudioMap", "bxml", &b) != 0) return;
    fmt_bxml* d = fmt_bxml_parse(b.data, b.size);
    fmt_buf_free(&b);
    if (!d) return;
    const int juke = fmt_bxml_find_child(d, 0, "Jukebox");
    for (int k = 0; juke >= 0 && k < fmt_bxml_child_count(d, juke); k++) {
        const int c = fmt_bxml_child(d, juke, k);
        if (strcmp(fmt_bxml_tag(d, c), "Track") || T.n_tracks >= MAX_TRACKS) continue;
        snprintf(T.song[T.n_tracks], sizeof T.song[0], "%s", attr(d, c, "SongName"));
        snprintf(T.bank[T.n_tracks], sizeof T.bank[0], "%s", attr(d, c, "BankName"));
        T.n_tracks++;
    }
    per_kind(d, juke >= 0 ? fmt_bxml_find_child(d, juke, "ActivityTransitionTimes") : -1, T.transition, NULL);
    per_kind(d, juke >= 0 ? fmt_bxml_find_child(d, juke, "ActivityStartDelay") : -1, T.start_delay, NULL);
    const int scen = fmt_bxml_find_child(d, 0, "ScenarioMusic");
    for (int k = 0; scen >= 0 && k < fmt_bxml_child_count(d, scen); k++) {
        const int c = fmt_bxml_child(d, scen, k);
        if (strcmp(fmt_bxml_tag(d, c), "Score") || T.n_scores >= MAX_TRACKS) continue;
        snprintf(T.score[T.n_scores], sizeof T.score[0], "%s", attr(d, c, "Name"));
        snprintf(T.score_bank[T.n_scores], sizeof T.score_bank[0], "%s", attr(d, c, "BankName"));
        T.n_scores++;
    }
    per_kind(d, scen >= 0 ? fmt_bxml_find_child(d, scen, "ActivityTransitionTimes") : -1, T.scenario_in, T.scenario_out);
    fmt_bxml_free(d);
}

static rx_addr player(void) { const rx_addr g = U32(0, 0x00a946b8); return g ? g + 0x15c6c : 0; }
/* the playlist vector (+0x55c data, +0x560 count) assigned (FUN_00690ba0: the data block grows when needed) */
static void playlist_assign(rx_addr P, const int32_t* v, int n) {
    rx_addr data = U32(P, 0x55c);
    if (n > 0 && (data == 0 || I32(P, 0x564) < n)) { data = rx_reflex_aligned_alloc(4u * (uint32_t)(n < 32 ? 32 : n)); SU32(P, 0x55c, data); SU32(P, 0x564, (uint32_t)(n < 32 ? 32 : n)); }
    for (int k = 0; k < n; k++) SU32(data, 4u * (uint32_t)k, (uint32_t)v[k]);
    SU32(P, 0x560, (uint32_t)n);
}
static rx_addr g_inited;   /* the player the music init ran on (00675990 runs in the init thread, which the product does not run) */
static int inited(rx_addr P) { return P != 0 && P == g_inited; }
static void init_player(rx_addr P) {
    if (inited(P)) return;
    load();
    g_inited = P;
    int32_t v[MAX_TRACKS];
    for (int k = 0; k < T.n_tracks; k++) v[k] = k;
    playlist_assign(P, v, T.n_tracks);           /* 00675990: the playlist 0 .. n - 1, position n */
    SU32(P, 0x570, (uint32_t)T.n_tracks);
    SU32(P, 0x1d0, 0xffffffffu); SU32(P, 0x1d4, 0xffffffffu); SU32(P, 0x398, 0xffffffffu); SU32(P, 0x39c, 0xffffffffu);
    SU32(P, 0x578, 0);
    SU32(P, 8, 1);
}

/* 00677510 (EDI = player): the three event slots stopped (the sound engine's), their states 4, mode 1, flags 0, the delay 0 */
static void stop_all(rx_addr P) {
    SU32(P, 0xc, 4); SU32(P, 0x10, 4); SU32(P, 0x14, 4);
    SU32(P, 8, 1); SU32(P, 4, 0);
    SF32(P, 0x1d8, 0.0f);
    T.serial++;
}
/* 006771b0 (EAX = player): the next playlist entry requested */
static void next_track(rx_addr P) {
    if (I32(P, 8) != 2) return;
    if (T.n_tracks < 1) { stop_all(P); return; }
    int32_t pos = I32(P, 0x570) + 1;
    if (pos >= I32(P, 0x560)) pos = 0;
    SU32(P, 0x570, (uint32_t)pos);
    const int32_t t = I32(U32(P, 0x55c), 4u * (uint32_t)pos);
    if (I32(P, 0x1d0) != t && t >= 0 && t < T.n_tracks) SU32(P, 0x1d0, (uint32_t)t);
}

/* 00677440 (EDI = the music entity + 0x90: {names, count}; stdcall player): the level's track names matched to the jukebox's (006773a0,
 * case-insensitive SongName), then 00677220: the playlist := them, shuffled with the game's generator, the position a random one - 1 */
void rx_music_tracks(rx_addr list) {
    const rx_addr P = player();
    if (!P) return;
    init_player(P);
    const int32_t n = I32(list, 4);
    if (n <= 0) return;
    int32_t m[MAX_TRACKS]; int cnt = 0;
    for (int32_t i = 0; i < n && cnt < MAX_TRACKS; i++) {
        const rx_addr s = U32(U32(list, 0), 4u * (uint32_t)i);
        char name[64]; uint32_t k = 0;
        for (; s && k < sizeof name - 1; k++) { const char c = (char)U8(s, k); if (!c) break; name[k] = c; }
        name[k] = 0;
        for (int t = 0; t < T.n_tracks; t++) if (!strcasecmp(name, T.song[t])) { m[cnt++] = t; break; }
    }
    if (cnt <= 0) return;
    playlist_assign(P, m, cnt);
    const rx_addr pl = U32(P, 0x55c);
    const int32_t c = I32(P, 0x560);
    if (c == 2) {
        if (((draw() >> 16) & 1) == 0) { const uint32_t a = U32(pl, 0); SU32(pl, 0, U32(pl, 4)); SU32(pl, 4, a); }
    } else if (c > 2) {
        int32_t i = 0, j = r15(draw()) % (c - 1) + 1;
        for (int32_t left = c * 2; left > 0; left--) {
            const uint32_t a = U32(pl, 4u * (uint32_t)i);
            SU32(pl, 4u * (uint32_t)i, U32(pl, 4u * (uint32_t)j));
            SU32(pl, 4u * (uint32_t)j, a);
            do { i = r15(draw()) % c; j = r15(draw()) % c; } while (i == j);
        }
    }
    SU32(P, 0x570, (uint32_t)(r15(draw()) % c - 1));
    T.serial++;
}

/* 006770d0 (EDI = the score list entity + 0x90 / + 0xa4; stdcall player): the level's score names matched (006773f0) into +0x574 / +0x578 */
void rx_music_scores(rx_addr list) {
    const rx_addr P = player();
    if (!P) return;
    init_player(P);
    const int32_t n = I32(list, 4);
    if (n < 1) { SU32(P, 0x578, 0); return; }
    int32_t m[MAX_TRACKS]; int cnt = 0;
    for (int32_t i = 0; i < n && cnt < MAX_TRACKS; i++) {
        const rx_addr s = U32(U32(list, 0), 4u * (uint32_t)i);
        char name[64]; uint32_t k = 0;
        for (; s && k < sizeof name - 1; k++) { const char c = (char)U8(s, k); if (!c) break; name[k] = c; }
        name[k] = 0;
        for (int t = 0; t < T.n_scores; t++) if (!strcasecmp(name, T.score[t])) { m[cnt++] = t; break; }
    }
    if (cnt <= 0) return;
    rx_addr data = rx_reflex_aligned_alloc(4u * (uint32_t)cnt);
    for (int k = 0; k < cnt; k++) SU32(data, 4u * (uint32_t)k, (uint32_t)m[k]);
    SU32(P, 0x574, data); SU32(P, 0x578, (uint32_t)cnt);
}

/* 00677700 (EAX = player, ESI = the activity kind): the jukebox of the activity: the start delay of the kind, the next entry */
void rx_music_jukebox(uint32_t kind) {
    const rx_addr P = player();
    if (!P) return;
    init_player(P);
    if (I32(P, 8) == 0 || I32(P, 8) == 2) return;
    stop_all(P);
    if (T.n_tracks <= 0) return;
    SU32(P, 8, 2); SU32(P, 4, 1); SU32(P, 0x1dc, kind);
    SF32(P, 0x1d8, kind < (uint32_t)N_KINDS ? T.start_delay[kind] : 0.0f);
    next_track(P);
    T.serial++;
}

/* 00677750 MusicPlayer_StartScenarioMusic (EAX = player; the activity kind): a score: the level's list (1: it; more: one draw), else
 * one draw over the scores */
void rx_music_start_score(uint32_t kind) {
    const rx_addr P = player();
    if (!P) return;
    init_player(P);
    if (I32(P, 8) == 0 || I32(P, 8) == 4) return;
    stop_all(P);
    if (T.n_scores <= 0) return;
    SU32(P, 0x1dc, kind); SU32(P, 8, 4); SU32(P, 4, 1); SF32(P, 0x1d8, 0.0f);
    int32_t pick;
    const int32_t ln = I32(P, 0x578);
    if (ln < 2) {
        if (ln == 1) pick = I32(U32(P, 0x574), 0);
        else pick = r15(draw()) % T.n_scores;
    } else pick = I32(U32(P, 0x574), 4u * (uint32_t)(r15(draw()) % ln));
    if (I32(P, 8) == 4 && I32(P, 0x398) != pick && pick >= 0 && pick < T.n_scores) SU32(P, 0x398, (uint32_t)pick);
    T.serial++;
}

/* 00677600 MusicPlayer_FadeOut (EAX = player): a score fades over its kind's second ScenarioMusic value, the jukebox / UI over 1 s
 * (flag 8, the music channel ramp to 0, then 00677510); a silent channel or another mode stops at once */
void rx_music_fade_out(void) {
    const rx_addr P = player();
    if (!P) return;
    init_player(P);
    const int32_t mode = I32(P, 8);
    const uint32_t kind = U32(P, 0x1dc);
    const float t = mode == 4 ? (kind < (uint32_t)N_KINDS ? T.scenario_out[kind] : 1.0f) : 1.0f;
    if (mode == 2 || mode == 3 || mode == 4) {
        const int audible = mode == 4 ? I32(P, 0x39c) >= 0 : I32(P, 0x1d4) >= 0;   /* the channel ramp is not 0 once a track / score started */
        if ((U32(P, 4) & 2) == 0 && t > 0.0f && audible) { SU32(P, 4, U32(P, 4) | 8); T.serial++; return; }
        stop_all(P);
    }
}

/* 00677510 */
void rx_music_stop(void) { const rx_addr P = player(); if (!P) return; init_player(P); stop_all(P); }

int32_t rx_music_mode(void) { const rx_addr P = player(); return inited(P) ? I32(P, 8) : 0; }

/* the per-frame part (00676ae0 / 00676d20 for the states the product reaches): a jukebox's delay counts down in game time, then the
 * requested track is the one played (+0x1d4); a score is played at once; a fade's end stops the player (the channel ramp's end,
 * 006776d0 -> 00677510, after its time) */
void rx_music_tick(float dt) {
    const rx_addr P = player();
    if (!inited(P)) return;
    static float fade_left; static int fading;
    const int32_t mode = I32(P, 8);
    if (U32(P, 4) & 8) {
        if (!fading) { fading = 1; const uint32_t kind = U32(P, 0x1dc); fade_left = mode == 4 ? (kind < (uint32_t)N_KINDS ? T.scenario_out[kind] : 1.0f) : 1.0f; }
        fade_left -= dt;
        if (fade_left <= 0.0f) { fading = 0; stop_all(P); SU32(P, 0x1d4, 0xffffffffu); SU32(P, 0x39c, 0xffffffffu); }
        return;
    }
    fading = 0;
    if (mode == 2) {
        float d = F32(P, 0x1d8);
        if (d > 0.0f) { d -= dt; SF32(P, 0x1d8, d); }
        if (d <= 0.0f && I32(P, 0x1d0) >= 0 && I32(P, 0x1d4) != I32(P, 0x1d0)) { SU32(P, 0x1d4, U32(P, 0x1d0)); T.serial++; }
    } else if (mode == 4) {
        if (I32(P, 0x398) >= 0 && I32(P, 0x39c) != I32(P, 0x398)) { SU32(P, 0x39c, U32(P, 0x398)); T.serial++; }
    } else if (mode == 1 && (I32(P, 0x1d4) >= 0 || I32(P, 0x39c) >= 0)) {
        SU32(P, 0x1d4, 0xffffffffu); SU32(P, 0x39c, 0xffffffffu); T.serial++;
    }
}

void rx_music_fill(rx_sound_music* m) {
    memset(m, 0, sizeof *m);
    const rx_addr P = player();
    if (!inited(P)) return;
    m->valid = 1;
    m->mode = I32(P, 8); m->flags = U32(P, 4); m->kind = I32(P, 0x1dc); m->delay = F32(P, 0x1d8);
    m->requested = I32(P, 0x1d0); m->current = I32(P, 0x1d4); m->score_requested = I32(P, 0x398); m->score_current = I32(P, 0x39c);
    m->position = I32(P, 0x570); m->count = I32(P, 0x560);
    const uint32_t kind = U32(P, 0x1dc);
    m->transition = kind < (uint32_t)N_KINDS ? (m->mode == 4 ? T.scenario_in[kind] : T.transition[kind]) : 0.0f;
    m->start_delay = kind < (uint32_t)N_KINDS ? T.start_delay[kind] : 0.0f;
    m->serial = T.serial;
    for (int k = 0; k < RX_SOUND_PLAYLIST && k < m->count; k++) m->playlist[k] = I32(U32(P, 0x55c), 4u * (uint32_t)k);
    if (m->current >= 0 && m->current < T.n_tracks) snprintf(m->track, sizeof m->track, "%s", T.bank[m->current]);
    if (m->score_current >= 0 && m->score_current < T.n_scores) snprintf(m->score, sizeof m->score, "%s", T.score_bank[m->score_current]);
}
void rx_music_reset(void) { T.serial = 0; g_inited = 0; }
#else
void rx_music_reset(void) {}
#endif
