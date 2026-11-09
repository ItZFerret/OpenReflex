/* activity_cues.c -- the activity audio cues of the level's ActivityAudioManager entity on the product (w-sfx, 2026-10-27).
 *
 * Why the product's cue map was empty: ActivityAudioLogic::vf29 (rx_audio_logic_vf29, build_activity_audio.c) takes the logic's cue
 * multimap from the activity audio manager [0x00d92f14] (+4 the UI world's entity, +8 the event world's: entity + 0x94), which the
 * audio world setup 0067da10 creates and the ActivityAudioManagerEntity classes (types 0x1089 .. 0x1093, vf5 0068d650: the EventList
 * items {ActivityEvent, EventName, ModifierName + the modifier's RSData}) fill. The product runs neither (audio: build_scene.c
 * outside_core), so the logic made its own empty map, its class handlers (vf3 00582610 ...: Holeshot, Player Eliminated, Best Lap,
 * EndOfRace, the lap counts, Player Wins, TimedSectionEntered, the section / challenge cues) were never subscribed, and every cue
 * call (rx_audio_cue 005811a0: Countdown Step, FinishCountdown Step, WindDown Entered, InProgress Entered, the stunts ...) found
 * nothing. The cue object, its modifier (0068dae0) and the map live in guest memory in the original; here the same data is kept on
 * the host side, read from the same RSData when the scene builds the entity, and the logic's three uses of the map get it:
 *   - vf3's presence test (cue_present), so the class handlers are subscribed as in the original;
 *   - rx_audio_cue: every cue under the name whose modifier accepts (DefaultModifier::vf2 00580870 and its subclasses' vf2 / vf3,
 *     the same tests as the ported ones: PlayOnlyOnce, RequireHumanPlayers (00559da0), ActivityModes (the activity's + 0x238),
 *     ActivityTypes (Activity_UiKind), CountDownStep (*a), HumanDriver / Driver (a player + 4 and not + 0x10), ChallengeAward (*b ==
 *     Award or Award 4), SelectSoundIndex (vf3: the cue's index = *a)) is played: rx_audio_play_cue's record (sound_state.c ring) and
 *     the cue's played flag / index reset;
 *   - vf32 (Restart): the played flags cleared.
 * The audio command itself (0067ead0 -> the FMOD event "<SoundAsset>/<EventName>" on the Effects channel) is the host's. Product only;
 * nothing here is in guest memory, so the game state is the same whether a cue plays or not (the original's cue objects are only read
 * by this logic and the sound layer). */
#include "rx_game.h"
#include "activity_cues.h"
#ifndef RX_ORACLE
#include <string.h>
#include <stdio.h>
#include "sound_state.h"
#include "race_activity.h"
#include "../src/fmt_scene.h"
#include "../src/fmt_bxml.h"

enum { MOD_NONE, MOD_DEFAULT, MOD_COUNTDOWN, MOD_HUMAN_DRIVER, MOD_DRIVER, MOD_SELECT_INDEX, MOD_CHALLENGE };
typedef struct {
    char event[48];                /* ActivityEvent (the map's key) */
    char path[RX_SOUND_CUE_PATH];  /* "<SoundAsset>/<EventName>" */
    int kind;                      /* MOD_* */
    uint32_t once, require_human, must_human;
    uint32_t modes;                /* ActivityModes: bit k = the mode names 0x9cf57c[k] (9) */
    uint64_t types;                /* ActivityTypes: bit k = the activity type names 0x9cf448[k] (62) */
    int has_modes, has_types;
    int32_t step, award;
    uint32_t played;               /* cue + 4 */
    int32_t index;                 /* cue + 0x10 */
} cue;
#define MAX_CUES 64
typedef struct { rx_addr scene; uint32_t entity; int n; cue c[MAX_CUES]; } table;
#define TABLES 4
static table tables[TABLES];
static uint32_t table_age[TABLES], table_clock;
static struct { rx_addr logic; table t; } binds[32];   /* each logic keeps its own copy (its teardown sees the names its vf3 saw) */

void rx_activity_cues_reset(void) { memset(tables, 0, sizeof tables); memset(binds, 0, sizeof binds); memset(table_age, 0, sizeof table_age); table_clock = 0; }

/* ---- the RSData (the entity's reader, fmt) ---- */
static int prop(const fmt_bxml* d, int node, const char* name) { return node < 0 ? -1 : fmt_bxml_find_child_attr(d, node, "name", name); }
static const char* leaf_str(const fmt_bxml* d, int p, const char* tag) {
    if (p < 0) return NULL;
    const int c = fmt_bxml_find_child(d, p, tag);
    fmt_bxml_value v;
    if (c < 0 || fmt_bxml_node_value(d, c, &v) != 0 || v.kind != FMT_BX_STRING) return NULL;
    return v.str;
}
static int leaf_int(const fmt_bxml* d, int p, int dflt) {
    if (p < 0) return dflt;
    const int c = fmt_bxml_first_child(d, p);
    fmt_bxml_value v;
    if (c < 0 || fmt_bxml_node_value(d, c, &v) != 0 || v.count == 0 || !v.data) return dflt;
    if (v.kind == FMT_BX_BOOL) return v.data[0] != 0;
    if (v.kind == FMT_BX_INT || v.kind == FMT_BX_UINT) { int32_t x; memcpy(&x, v.data, 4); return x; }
    if (v.kind == FMT_BX_FLOAT) { float x; memcpy(&x, v.data, 4); return (int)x; }
    return dflt;
}
/* a selection of names (ActivityModes / ActivityTypes): the Bool fields set, by the index of their name in the game's table */
static uint64_t selection(const fmt_bxml* d, int p, rx_addr names, int count, int* any) {
    uint64_t bits = 0; *any = 0;
    if (p < 0) return 0;
    for (int k = 0; k < count; k++) {
        const rx_addr s = U32(0, names + 4u * (uint32_t)k);
        char name[48]; uint32_t i = 0;
        for (; s && i < sizeof name - 1; i++) { const char ch = (char)U8(s, i); if (!ch) break; name[i] = ch; }
        name[i] = 0;
        if (!name[0]) continue;
        if (leaf_int(d, prop(d, p, name), 0)) { bits |= 1ull << k; *any = 1; }
    }
    return bits;
}

/* ActivityAudioManagerEntity::vf5 (0068d650) and the modifier factory 0068dae0 / the modifiers' vf1 (00580690 DefaultModifier: PlayOnlyOnce,
 * RequireHumanPlayers, ActivityModes, ActivityTypes; 00580980 CountDownStep; MustBeHumanPlayer; 00580b80 Award): the cues of an entity. */
void rx_activity_cues_capture(rx_addr scene, const struct fmt_entity* rs) {
    if (!rs || rs->rsdata < 0) return;
    const fmt_bxml* d = rs->doc;
    /* one manager per world: the scene's first ActivityAudioManagerEntity registers (vf1 0068dc80 fills an empty slot only); the same
     * entity again is the scene built anew (its table restarts) */
    int ti = -1;
    for (int k = 0; k < TABLES; k++) if (tables[k].scene == scene) ti = k;
    if (ti >= 0 && tables[ti].entity != rs->id) return;
    if (ti < 0) { ti = 0; for (int k = 1; k < TABLES; k++) if (table_age[k] < table_age[ti]) ti = k; }   /* an empty or the oldest table */
    table* t = &tables[ti];
    memset(t, 0, sizeof *t); t->scene = scene; t->entity = rs->id; table_age[ti] = ++table_clock;
    const int asset_p = prop(d, rs->rsdata, "SoundAsset");
    const int asset_c = asset_p >= 0 ? fmt_bxml_find_child(d, asset_p, "Asset") : -1;
    const char* asset = asset_c >= 0 ? fmt_bxml_attr_str(d, asset_c, "name") : NULL;
    if (!asset || !asset[0]) return;
    const int list = prop(d, rs->rsdata, "EventList");
    for (int k = 0; list >= 0 && k < fmt_bxml_child_count(d, list) && t->n < MAX_CUES; k++) {
        const int item = fmt_bxml_child(d, list, k);
        const char* ev = leaf_str(d, prop(d, item, "ActivityEvent"), "Enum");
        const char* name = leaf_str(d, prop(d, item, "EventName"), "Enum");
        const char* mod = leaf_str(d, prop(d, item, "ModifierName"), "String");
        if (!ev || !ev[0] || !name || !name[0]) continue;   /* both names non-empty (vf5) */
        cue* c = &t->c[t->n++];
        memset(c, 0, sizeof *c);
        snprintf(c->event, sizeof c->event, "%s", ev);
        snprintf(c->path, sizeof c->path, "%s/%s", asset, name);
        c->index = -1;
        c->kind = MOD_NONE;
        if (mod && mod[0]) {
            if (!strcmp(mod, "DefaultModifier")) c->kind = MOD_DEFAULT;
            else if (!strcmp(mod, "CountDownStepModifier")) c->kind = MOD_COUNTDOWN;
            else if (!strcmp(mod, "HumanDriverModifier")) c->kind = MOD_HUMAN_DRIVER;
            else if (!strcmp(mod, "DriverModifier")) c->kind = MOD_DRIVER;
            else if (!strcmp(mod, "SelectSoundIndexModifier")) c->kind = MOD_SELECT_INDEX;
            else if (!strcmp(mod, "ChallengeAwardModifier")) c->kind = MOD_CHALLENGE;
            const int m = prop(d, item, mod);
            c->once = (uint32_t)leaf_int(d, prop(d, m, "PlayOnlyOnce"), 0);
            c->require_human = (uint32_t)leaf_int(d, prop(d, m, "RequireHumanPlayers"), 0);
            c->modes = (uint32_t)selection(d, prop(d, m, "ActivityModes"), 0x009cf57c, 9, &c->has_modes);
            c->types = selection(d, prop(d, m, "ActivityTypes"), 0x009cf448, 0x3e, &c->has_types);
            c->step = leaf_int(d, prop(d, m, "CountDownStep"), -1);
            c->must_human = (uint32_t)leaf_int(d, prop(d, m, "MustBeHumanPlayer"), 0);
            c->award = leaf_int(d, prop(d, m, "Award"), 3);
        }
    }
}

/* ActivityAudioLogic::vf29 (product): the logic's table is the one of the current world's scene (the manager's +4 / +8 choice) */
void rx_activity_cues_bind(rx_addr logic) {
    const rx_addr g = U32(0, 0x00a946b8), core = U32(0, 0x00a95844);
    rx_addr w = 0;
    if (g) {
        if (U32(U32(core, 0x20), 0x34) == 1) w = U32(g, 0x5a54);
        else if (U32(U32(core, 0x1c), 0x34) == 1) w = U32(g, 0x20c8);
    }
    const rx_addr scene = w ? U32(w, 0x20138) : 0;
    int t = -1;
    for (int k = 0; k < TABLES; k++) if (scene && tables[k].scene == scene && tables[k].n > 0) t = k;
    int slot = -1;
    for (int k = 0; k < 32; k++) if (binds[k].logic == logic) slot = k;
    if (slot < 0) for (int k = 0; k < 32; k++) if (binds[k].logic == 0) { slot = k; break; }
    if (slot < 0) slot = 0;
    if (t < 0) { binds[slot].logic = 0; return; }
    binds[slot].logic = logic;
    binds[slot].t = tables[t];
    for (int k = 0; k < binds[slot].t.n; k++) { binds[slot].t.c[k].played = 0; binds[slot].t.c[k].index = -1; }
}
static table* bound(rx_addr logic) {
    for (int k = 0; k < 32; k++) if (logic && binds[k].logic == logic) return &binds[k].t;
    return NULL;
}
static int guest_eq(rx_addr s, const char* lit) {
    uint32_t i = 0;
    for (; lit[i]; i++) if ((char)U8(s, i) != lit[i]) return 0;
    return U8(s, i) == 0;
}

int rx_activity_cues_present(rx_addr logic, rx_addr name) {
    table* t = bound(logic);
    if (!t) return 0;
    for (int k = 0; k < t->n; k++) if (guest_eq(name, t->c[k].event)) return 1;
    return 0;
}

/* DefaultModifier::vf2 (00580870) */
static int default_ok(const cue* c, rx_addr act) {
    if (c->once && c->played) return 0;
    if (c->require_human && rx_activity_has_local_human(act) == 0) return 0;
    if (c->has_modes) { const uint32_t m = U32(act, 0x238); if (m >= 32 || !(c->modes & (1u << m))) return 0; }
    if (c->has_types) { const uint32_t k = rx_activity_ui_kind(act); if (k >= 64 || !(c->types & (1ull << k))) return 0; }
    return 1;
}
static int human(rx_addr p) { return p != 0 && U32(p, 4) != 0 && U32(p, 0x10) == 0; }

void rx_activity_cues_play(rx_addr logic, rx_addr name, uint32_t a, uint32_t b) {
    table* t = bound(logic);
    if (!t) return;
    const rx_addr act = U32(logic, 0x50);
    for (int k = 0; k < t->n; k++) {
        cue* c = &t->c[k];
        if (!guest_eq(name, c->event)) continue;
        int ok = 1;
        switch (c->kind) {
        case MOD_NONE: ok = 1; break;
        case MOD_DEFAULT: case MOD_SELECT_INDEX: ok = default_ok(c, act); break;
        case MOD_COUNTDOWN: ok = default_ok(c, act) && (a != 0 ? I32(a, 0) : -1) == c->step; break;            /* 005809d0 */
        case MOD_HUMAN_DRIVER: ok = default_ok(c, act) && human(a); break;                                         /* 00580a80 */
        case MOD_DRIVER: ok = default_ok(c, act) && (c->must_human == 0 || human(a)); break;                      /* 00580af0 */
        case MOD_CHALLENGE: ok = default_ok(c, act) && human(a) && b != 0 && (I32(b, 0) == c->award || c->award == 4); break;   /* 00580c20 */
        }
        if (!ok) continue;
        if (c->kind == MOD_SELECT_INDEX && a != 0) c->index = I32(a, 0);                                        /* 00580b70 */
        /* rx_audio_play_cue (005812f0): the cue's event (when the name is set), played, the index reset */
        rx_sound_cue_text(c->path, c->index);
        c->played = 1;
        c->index = -1;
    }
}

void rx_activity_cues_restart(rx_addr logic) {
    table* t = bound(logic);
    if (t) for (int k = 0; k < t->n; k++) { t->c[k].played = 0; t->c[k].index = -1; }
}
#else
void rx_activity_cues_reset(void) {}
#endif
