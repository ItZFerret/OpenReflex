/* phys_studio.c -- PHYSICS STUDIO (port addition, not original code): see phys_studio.h and core/plan/PHYSICS_STUDIO.md.
 *
 * The catalog: the nine parameter files (EngineDependencies' mx125_vehicle .. truck2WDlite_vehicle bxml assets) read with the format
 * library the way 0043a560 / 0048b680 read them (the root's type, the children in order, the registered names matched without case,
 * kind 0 one float, kind 1 an "x,y,z" vec3, kind 2 a curve: num_knots / mleft / mright and the 2n comma floats, kind 3 up to three
 * same-named siblings, the missing ones copies of the first; host atof, as build_xml.c's text forms), over the registration defaults
 * of the four tables. The catalog is what the editor shows as each value's default; the game itself never reads it: a config's
 * values are written over what the file put in P, and a value without an override stays the file's own bits.
 *
 * The config: per class / parameter / component an absolute value (clamped to the catalog's bounds, finite), the six grip and four
 * drag materials, grip_global and all_vehicles. Overrides reach only the local players' vehicles (the spec's controller, 00543020 >= 0)
 * unless all_vehicles is set. */
#include "phys_studio.h"
#include "build_vehicle.h"
#include "build_vehicle_int.h"
#include "../src/fmt_db.h"
#include "../src/fmt_bxml.h"
#include "../src/rx_reset.h"
#include <math.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "build_vehicle_params.inc"       /* k_moto_params (004392f0) */
#include "build_vehicle_atv_params.inc"   /* k_atv_params (00489cb0) */
#include "build_vehicle_utv_params.inc"   /* k_utv_params (004c9d40) */
#include "build_vehicle_bt_params.inc"    /* k_bt_params (004af510: Buggy, TrophyTruck) */
#include "phys_studio_names.inc"          /* k_ps_names_*: the tables' names as host strings (tools/gen_phys_studio_names.py) */
#include "phys_studio_desc.inc"           /* k_ps_desc: labels, descriptions, units and natural bounds per name (tools/gen_phys_studio_desc.py) */

void rx_sim_atv_setup_masses(rx_addr S);   /* 0048c750 (build_vehicle_atv.c) */

/* the class's masses after its parameter block changed, as the original's own reloads do (MX 0043b690 -> 0043b750, ATV 0048d320 ->
 * 0048c750, the four-wheel classes' pending reload (fourwheel_pending.c) -> 004cc030 / 004b1800 / 00450480); each ends in vf35 */
static void ps_masses(int ci, rx_addr S) {
    const rx_addr sp = rx_sp();
    rx_set_sp(sp - 0x40);
    switch (ci) {
    case RX_PS_ATV: rx_sim_atv_setup_masses(S); break;
    case RX_PS_UTV: rx_sim_utv_setup_masses(S); break;
    case RX_PS_BUGGY: rx_sim_buggy_setup_masses(S); break;
    case RX_PS_TRUCK2WD: case RX_PS_TRUCK2WDLITE: rx_sim_truck_setup_masses(S); break;
    default: rx_sim_moto_setup_masses(S); break;
    }
    rx_set_sp(sp);
}

typedef struct { const char* id; const char* file; const char* type; const char* label; const veh_param* tab; const char* const* names;
                 uint32_t n; int live; } ps_class_def;
static const ps_class_def k_cls[RX_PS_CLASSES] = {
    {"mx125", "mx125_vehicle", "mx", "MX 125 (2-STROKE)", k_moto_params, k_ps_names_moto, 108, 1},
    {"mx250", "mx250_vehicle", "mx", "MX 250 (2-STROKE)", k_moto_params, k_ps_names_moto, 108, 1},
    {"mx250f", "mx250f_vehicle", "mx", "MX 250F (4-STROKE)", k_moto_params, k_ps_names_moto, 108, 1},
    {"mx450f", "mx450f_vehicle", "mx", "MX 450F (4-STROKE)", k_moto_params, k_ps_names_moto, 108, 1},
    {"atv", "atv_vehicle", "atv", "ATV", k_atv_params, k_ps_names_atv, 103, 1},
    {"utv", "utv_vehicle", "UTV", "UTV", k_utv_params, k_ps_names_utv, 98, 1},
    {"buggy", "buggy_vehicle", "buggy", "BUGGY", k_bt_params, k_ps_names_bt, 98, 1},
    {"truck2wd", "truck2WD_vehicle", "truck2WD", "TRUCK", k_bt_params, k_ps_names_bt, 98, 1},
    {"truck2wdlite", "truck2WDlite_vehicle", "truck2WD", "TRUCK (LITE)", k_bt_params, k_ps_names_bt, 98, 1},
};
_Static_assert(sizeof k_moto_params / sizeof k_moto_params[0] == 108, "moto table");
_Static_assert(sizeof k_atv_params / sizeof k_atv_params[0] == 103, "atv table");
_Static_assert(sizeof k_utv_params / sizeof k_utv_params[0] == 98, "utv table");
_Static_assert(sizeof k_bt_params / sizeof k_bt_params[0] == 98, "bt table");

/* ---- the catalog ---------------------------------------------------------------------------------------------------------- */

typedef struct { uint8_t kind, ncomp, knots, in_file; uint32_t def[RX_PS_MAX_COMPS]; float lo[RX_PS_MAX_COMPS], hi[RX_PS_MAX_COMPS]; } ps_param;
typedef struct { int loaded; ps_param p[RX_PS_CLASSES][RX_PS_MAX_PARAMS]; uint32_t hash; } ps_catalog;
/* built once per process from the install and kept across sessions (RX_RESET_KEEP: the session reset leaves it; it depends on the
 * files only); published whole with one atomic store, so a reader (the front end's thread) never sees a half-built one */
static _Atomic(ps_catalog*) g_catp RX_RESET_KEEP;
static const ps_catalog k_cat_empty;
static const ps_catalog* ps_cat(void) { const ps_catalog* c = atomic_load(&g_catp); return c ? c : &k_cat_empty; }
#define g_cat (*ps_cat())

static const float k_grip_def[RX_PS_GRIP] = {0.1f, 0.3f, 0.5f, 0.65f, 0.8f, 0.9f};   /* 0x931d94, 0x931da4, 0.5, [0x9d78a0], [0x9cc8a8], [0x9cd970] */
static const char* const k_grip_label[RX_PS_GRIP] = {"MATERIAL 0 (SLICK: DOVER, FREIGHTYARD)", "MATERIAL 1 (LOOSE: ALPINE, CHEYENNE)",
    "MATERIAL 2 (DEFAULT DIRT)", "MATERIAL 3 (FIRM)", "MATERIAL 4 (HARD PACK: FRESNO)", "MATERIAL 5 (GRIPPY)"};
static const float k_drag_def[RX_PS_DRAG] = {0.0f, 0.75f, 1.0f, 0.25f};
static const char* const k_drag_label[RX_PS_DRAG] = {"DRAG MATERIAL 0", "DRAG MATERIAL 2", "DRAG MATERIAL 3", "DRAG OTHER MATERIALS"};

static float bf(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t fb(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* the value words of a record by kind (offsets from the record): kind 0 +0x44; kind 1 +0x50 x / y / z; kind 3 +0x44 / +0x48 / +0x4c;
 * kind 2: comps 0..n-1 the knots' y (+0x70 + 4k), n..2n-1 their x (+0x48 + 4k), 2n mleft (+0x98), 2n+1 mright (+0x9c) */
static uint32_t comp_off(const ps_param* p, int c) {
    switch (p->kind) {
    case 0: return 0x44;
    case 1: return 0x50 + 4u * (uint32_t)c;
    case 3: return 0x44 + 4u * (uint32_t)c;
    default: {
        const int n = p->knots;
        if (c < n) return 0x70 + 4u * (uint32_t)c;
        if (c < 2 * n) return 0x48 + 4u * (uint32_t)(c - n);
        return c == 2 * n ? 0x98 : 0x9c;
    }
    }
}

/* host text forms as build_xml.c's parse_list / parse_vec3_list: atof at the cursor, the cursor after the next comma */
static int text_floats(const char* s, float* out, int n) {
    const char* p = s ? s : "";
    for (int i = 0; i < n; i++) {
        out[i] = (float)atof(p);
        const char* c = strchr(p, ',');
        if (!c) return i == n - 1;
        p = c + 1;
    }
    return 1;
}
static int node_floats(const fmt_bxml* d, int node, float* out, int n) {
    fmt_bxml_value v;
    if (fmt_bxml_node_value(d, node, &v) != 0) return 0;
    if (v.kind == FMT_BX_STRING) return text_floats(v.str, out, n);
    if (v.kind == FMT_BX_FLOAT && (int)v.count >= n) { for (int i = 0; i < n; i++) out[i] = fmt_bx_f32(&v, (uint32_t)i, 0); return 1; }
    if (v.kind == FMT_BX_VEC3 && n == 3 && v.count >= 1) { for (int i = 0; i < 3; i++) out[i] = fmt_bx_f32(&v, 0, i); return 1; }
    return 0;
}
static float attr_float(const fmt_bxml* d, int node, const char* name, float def) {
    const char* s = fmt_bxml_attr_str(d, node, name);
    if (s) return (float)atof(s);
    return fmt_bxml_attr_f32(d, node, name, def);
}
static int32_t attr_int(const fmt_bxml* d, int node, const char* name) {
    const char* s = fmt_bxml_attr_str(d, node, name);
    if (s) { if (*s == 'T' || *s == 't') return 1; if (*s == 'F' || *s == 'f') return 0; return (int32_t)strtol(s, NULL, 0); }
    return fmt_bxml_attr_i32(d, node, name, 0);
}

/* the description row of a parameter name (every registered name has one: tools/gen_phys_studio_desc.py checks it) */
static const ps_desc* desc_of(const char* nm) {
    for (size_t i = 0; i < sizeof k_ps_desc / sizeof k_ps_desc[0]; i++) if (strcmp(k_ps_desc[i].name, nm) == 0) return &k_ps_desc[i];
    return NULL;
}

static int positive_only(const char* nm) {
    static const char* const k[] = {"Mass", "Inertia", "Radius", "Width", "TimeConstant", "Length"};
    if (strstr(nm, "CMOffset") || strstr(nm, "CenterOfMass")) return 0;
    for (size_t i = 0; i < sizeof k / sizeof k[0]; i++) if (strstr(nm, k[i])) return 1;
    return 0;
}
/* the editor's bounds: the data define none, so: 0 .. 5x a positive value (5 % .. 5x for masses, inertias, radii, lengths and time
 * constants, which must stay positive), 5x .. 0 a negative one, +-max(1, the largest of the record's values) for a 0 (0 .. that when
 * the record's other values are never negative); a switch 0..1; a curve's x between its neighbours */
static void set_bounds(const char* nm, ps_param* p) {
    const ps_desc* desc = desc_of(nm);
    float big_all = 1.0f, big_y = 1.0f, big_m = 1.0f;   /* a curve's y, its slopes and its x are scaled apart */
    for (int c = 0; c < p->ncomp; c++) {
        const float a = fabsf(bf(p->def[c]));
        if (!isfinite(a)) continue;
        if (a > big_all) big_all = a;
        if (p->kind == 2 && c < p->knots && a > big_y) big_y = a;
        if (p->kind == 2 && c >= 2 * p->knots && a > big_m) big_m = a;
    }
    /* the record's values are never negative (a curve: its y), one of them positive: a mu modifier, a mass, a rate */
    int nonneg = 0, neg = 0;
    for (int c = 0; c < (p->kind == 2 ? p->knots : p->ncomp); c++) { const float d = bf(p->def[c]); if (d > 0.0f) nonneg = 1; if (d < 0.0f) neg = 1; }
    if (neg) nonneg = 0;
    for (int c = 0; c < p->ncomp; c++) {
        const float d = bf(p->def[c]);
        const float big = p->kind != 2 ? big_all : c < p->knots ? big_y : c >= 2 * p->knots ? big_m : big_all;
        float lo, hi;
        if (!isfinite(d)) { lo = -big; hi = big; }
        else if (strstr(nm, "ManualTransmission")) { lo = 0.0f; hi = 1.0f; }
        else if (d > 0.0f) { lo = positive_only(nm) ? d * 0.05f : 0.0f; hi = d * 5.0f; }
        else if (d < 0.0f) { lo = d * 5.0f; hi = 0.0f; }
        else { lo = nonneg && !(p->kind == 2 && c >= p->knots) ? 0.0f : -big; hi = big; }   /* a 0 among values that are never negative stays >= 0 */
        if (desc && !(p->kind == 2 && c >= p->knots)) {   /* the natural limits of phys_studio_desc.txt (a value, or a curve's y knots) */
            if (desc->lo_t == 1) { if (d > 0.0f) lo = d * desc->lo_v; }
            else if (desc->lo_t == 2) lo = desc->lo_v;
            if (desc->hi_t == 1) { if (d > 0.0f) hi = d * desc->hi_v; }
            else if (desc->hi_t == 2) hi = desc->hi_v;
        }
        if (p->kind == 2 && c >= p->knots && c < 2 * p->knots) {   /* a knot's x: strictly between the neighbours' (by default) */
            const int k = c - p->knots, n = p->knots;
            const float span = n > 1 ? bf(p->def[n + n - 1]) - bf(p->def[n]) : 1.0f;
            lo = k > 0 ? bf(p->def[n + k - 1]) : d - fabsf(span);
            hi = k < n - 1 ? bf(p->def[n + k + 1]) : d + fabsf(span);
            if (!(hi > lo)) { lo = d; hi = d; }
        }
        if (lo > d) lo = d;
        if (hi < d) hi = d;
        p->lo[c] = lo; p->hi[c] = hi;
    }
}

static uint32_t fnv(uint32_t h, const void* data, size_t n) {
    const uint8_t* b = (const uint8_t*)data;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
    return h;
}

static int load_class(fmt_install* in, int ci, ps_catalog* K) {
    const ps_class_def* C = &k_cls[ci];
    for (uint32_t i = 0; i < C->n; i++) {   /* the registration defaults (004392f0 ..): kind 1 4 words, kind 2 count 0, kind 3 zeros */
        ps_param* p = &K->p[ci][i];
        memset(p, 0, sizeof *p);
        p->kind = C->tab[i].kind;
        p->ncomp = p->kind == 0 ? 1 : p->kind == 2 ? 0 : 3;
        if (p->kind == 0) p->def[0] = C->tab[i].v[0];
        if (p->kind == 1) for (int k = 0; k < 3; k++) p->def[k] = C->tab[i].v[k];
    }
    fmt_buf b = {0};
    if (fmt_install_read_named(in, "EngineDependencies", NULL, C->file, "bxml", &b) != 0) return -1;
    fmt_bxml* d = fmt_bxml_parse(b.data, b.size);
    free(b.data);
    if (!d) return -1;
    const char* type = fmt_bxml_attr_str(d, 0, "type");
    if (!type || strcmp(type, C->type) != 0) { fmt_bxml_free(d); return -1; }
    const int nch = fmt_bxml_child_count(d, 0);
    for (int k = 0; k < nch;) {
        const int node = fmt_bxml_child(d, 0, k);
        const char* tag = fmt_bxml_tag(d, node);
        int run = 1;   /* the following siblings of the same name */
        while (k + run < nch && strcasecmp(fmt_bxml_tag(d, fmt_bxml_child(d, 0, k + run)), tag) == 0) run++;
        for (uint32_t i = 0; i < C->n; i++) {
            if (strcasecmp(C->names[i], tag) != 0) continue;
            ps_param* p = &K->p[ci][i];
            float f[20];
            p->in_file = 1;
            if (p->kind == 0 || p->kind == 1) {   /* what the reader does not write keeps the registration's default */
                const int nv = p->kind == 0 ? 1 : 3;
                for (int c = 0; c < nv; c++) f[c] = bf(p->def[c]);
                node_floats(d, node, f, nv);
                for (int c = 0; c < nv; c++) p->def[c] = fb(f[c]);
            }
            else if (p->kind == 3) {
                for (int j = 0; j < 3; j++) {
                    float v = 0.0f;
                    if (j < run) node_floats(d, fmt_bxml_child(d, 0, k + j), &v, 1); else v = bf(p->def[0]);
                    p->def[j] = fb(v);
                }
            } else {   /* 0048bd20 */
                int n = attr_int(d, node, "num_knots");
                if (n < 0) n = 0;
                if (n > 10) n = 10;
                memset(f, 0, sizeof f);
                node_floats(d, node, f, 2 * n);
                p->knots = (uint8_t)n;
                p->ncomp = (uint8_t)(2 * n + 2);
                for (int j = 0; j < n; j++) { p->def[j] = fb(f[2 * j + 1]); p->def[n + j] = fb(f[2 * j]); }
                p->def[2 * n] = fb(attr_float(d, node, "mleft", 0.0f));
                p->def[2 * n + 1] = fb(attr_float(d, node, "mright", 0.0f));
            }
            break;
        }
        k += run;
    }
    fmt_bxml_free(d);
    for (uint32_t i = 0; i < C->n; i++) set_bounds(C->names[i], &K->p[ci][i]);
    return 0;
}

int rx_ps_catalog_build(struct fmt_install* in) {
    if (atomic_load(&g_catp)) return 0;
    if (!in) return -1;
    ps_catalog* K = calloc(1, sizeof *K);
    if (!K) return -1;
    for (int ci = 0; ci < RX_PS_CLASSES; ci++) if (load_class(in, ci, K) != 0) { free(K); return -1; }
    uint32_t h = 2166136261u;
    for (int ci = 0; ci < RX_PS_CLASSES; ci++) {
        h = fnv(h, k_cls[ci].id, strlen(k_cls[ci].id) + 1);
        for (uint32_t i = 0; i < k_cls[ci].n; i++) {
            const ps_param* p = &K->p[ci][i];
            h = fnv(h, k_cls[ci].names[i], strlen(k_cls[ci].names[i]) + 1);
            h = fnv(h, &p->kind, 1); h = fnv(h, &p->ncomp, 1);
            h = fnv(h, p->def, 4u * p->ncomp);
        }
    }
    for (int g = 0; g < RX_PS_GRIP; g++) h = fnv(h, &k_grip_def[g], 4);
    for (int g = 0; g < RX_PS_DRAG; g++) h = fnv(h, &k_drag_def[g], 4);
    K->hash = h ? h : 1;
    K->loaded = 1;
    ps_catalog* none = NULL;
    if (!atomic_compare_exchange_strong(&g_catp, &none, K)) free(K);   /* another thread published first: the same files, keep its */
    return 0;
}
uint32_t rx_ps_param_set_hash(void) { return g_cat.loaded ? g_cat.hash : 0; }

static const char* group_of(const char* nm) {
    if (!strncmp(nm, "Engine_", 7)) return "ENGINE & POWER";
    if (!strncmp(nm, "DriveTrain_", 11)) return "GEARING";
    if (!strncmp(nm, "Brakes_", 7)) return "BRAKES";
    if (!strncmp(nm, "MassProps_", 10)) return "CHASSIS & MASS";
    if (strstr(nm, "Suspension_")) return "SUSPENSION";
    if (strstr(nm, "Tire_") || strstr(nm, "Tires_")) return "TYRES & GRIP";
    if (!strncmp(nm, "AirBorne_", 9)) return "AIR CONTROL";
    if (!strncmp(nm, "Rider_", 6)) return "RIDER";
    if (strstr(nm, "Steer") || strstr(nm, "Lean")) return "STEERING & LEAN";
    return "CHASSIS & MASS";   /* VehicleSim_Gravity, the drag curves, the speed filter, the up vector */
}
/* the unit of a component, from the description table: a value its unit; a curve's knot y its unit, a knot x the x unit, an end
 * slope "y unit per x unit" (when both are known) */
static const char* unit_of(const char* nm, const ps_param* p, int c, char* buf, size_t n) {
    const ps_desc* D = desc_of(nm);
    if (!D) return "";
    if (p->kind != 2 || c < p->knots) return D->unit;
    if (c < 2 * p->knots) return D->unit_x;
    if (!D->unit[0] || !D->unit_x[0] || D->unit[0] == '(' || D->unit_x[0] == '(') return "";
    snprintf(buf, n, "%s per %s", D->unit, D->unit_x);
    return buf;
}
static void comp_label(const char* nm, const ps_param* p, int c, char* out, size_t n) {
    static const char* const xyz[3] = {"X", "Y", "Z"}, *rpy[3] = {"ROLL", "PITCH", "YAW"};
    static const char* const grip[3] = {"ON SURFACE 0", "ON SURFACE 0.5", "ON SURFACE 1"};   /* the surface value under the tyre (00461680 + the tyre term), 004390d0 blends them */
    if (p->kind == 0) snprintf(out, n, "VALUE");
    else if (p->kind == 1) snprintf(out, n, "%s", strstr(nm, "RPY") ? rpy[c] : xyz[c]);
    else if (p->kind == 3) snprintf(out, n, "%s", grip[c]);
    else if (c < p->knots) snprintf(out, n, "KNOT %d Y", c + 1);
    else if (c < 2 * p->knots) snprintf(out, n, "KNOT %d X", c - p->knots + 1);
    else snprintf(out, n, c == 2 * p->knots ? "SLOPE IN" : "SLOPE OUT");
}

/* ---- a bounded text writer --------------------------------------------------------------------------------------------------- */
typedef struct { char* out; uint32_t cap, len; } ps_w;
static void wr(ps_w* w, const char* fmt, ...) {
    char line[512];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((uint32_t)n >= sizeof line) n = (int)sizeof line - 1;
    if (w->out && w->len + (uint32_t)n < w->cap) memcpy(w->out + w->len, line, (size_t)n);
    w->len += (uint32_t)n;
}
static int wr_end(ps_w* w, uint32_t* required) {
    if (required) *required = w->len + 1;
    if (!w->out || w->cap == 0) return w->out ? -1 : 0;
    if (w->len + 1 > w->cap) { w->out[0] = 0; return -1; }
    w->out[w->len] = 0;
    return 0;
}

int rx_ps_catalog_text(char* out, uint32_t cap, uint32_t* required) {
    if (!g_cat.loaded) return -2;
    ps_w w = {out, cap, 0};
    uint32_t values = 0;
    for (int ci = 0; ci < RX_PS_CLASSES; ci++) for (uint32_t i = 0; i < k_cls[ci].n; i++) values += g_cat.p[ci][i].ncomp;
    wr(&w, "H\t%08x\t%d\t%u\t%d\t%d\n", g_cat.hash, RX_PS_CLASSES, values, RX_PS_GRIP, RX_PS_DRAG);
    /* N: one line per distinct parameter name (the classes share them): label, toggle switch, unit, unit of a curve's x, description */
    for (size_t i = 0; i < sizeof k_ps_desc / sizeof k_ps_desc[0]; i++) {
        const ps_desc* D = &k_ps_desc[i];
        wr(&w, "N\t%s\t%s\t%d\t%s\t%s\t%s\n", D->name, D->label, D->toggle, D->unit, D->unit_x, D->desc);
    }
    for (int ci = 0; ci < RX_PS_CLASSES; ci++) {
        const ps_class_def* C = &k_cls[ci];
        wr(&w, "C\t%d\t%s\t%s\t%s\t%u\t%d\n", ci, C->id, C->file, C->label, C->n, C->live);
        for (uint32_t i = 0; i < C->n; i++) {
            const ps_param* p = &g_cat.p[ci][i];
            wr(&w, "P\t%d\t%u\t%s\t%s\t%d\t%d\t%d\n", ci, i, C->names[i], group_of(C->names[i]), p->kind, p->ncomp, p->in_file);
            for (int c = 0; c < p->ncomp; c++) {
                char lab[32], ubuf[48]; comp_label(C->names[i], p, c, lab, sizeof lab);
                wr(&w, "V\t%d\t%u\t%d\t%s\t%.9g\t%.9g\t%.9g\t%s\n", ci, i, c, lab, (double)bf(p->def[c]), (double)p->lo[c], (double)p->hi[c],
                   unit_of(C->names[i], p, c, ubuf, sizeof ubuf));
            }
        }
    }
    for (int g = 0; g < RX_PS_GRIP; g++) wr(&w, "G\t%d\t%s\t%.9g\t0\t2\n", g, k_grip_label[g], (double)k_grip_def[g]);
    for (int g = 0; g < RX_PS_DRAG; g++) wr(&w, "D\t%d\t%s\t%.9g\t0\t2\n", g, k_drag_label[g], (double)k_drag_def[g]);
    return wr_end(&w, required);
}

/* ---- the config ----------------------------------------------------------------------------------------------------------- */
static struct {
    int on;                                           /* any override */
    uint32_t mask[RX_PS_CLASSES][RX_PS_MAX_PARAMS];   /* the overridden components */
    uint32_t val[RX_PS_CLASSES][RX_PS_MAX_PARAMS][RX_PS_MAX_COMPS];
    uint8_t grip_mask, drag_mask, grip_global, all_vehicles;   /* all_vehicles: the vehicle values for every rider (AI too) */
    float grip[RX_PS_GRIP], drag[RX_PS_DRAG];
} g_cfg;
int g_rx_ps_armed, g_rx_ps_grip_on;
static int g_live_asked;

/* the vehicles of this session the config reached (VehicleFactory_CreateVehicle with a controller; every one with all_vehicles) */
#define PS_LOCAL 16
static struct { rx_addr V, S; int cls, local; uint8_t snap[0x2d20]; uint32_t snap_len; int has_snap;
                int good, off, touched; uint32_t body[8][16]; } g_local[PS_LOCAL];   /* touched: holds config values */   /* good / body: the last sane frame's bodies (the guard) */
static int g_restored;   /* vehicles the guard put back on the file's values (rx_ps_active bit 1) */
static int g_nlocal, g_ctx_local, g_ctx_cls = -1, g_ctx_touched;
static rx_addr g_ctx_P;
static uint8_t g_ctx_snap[0x2d20];
static uint32_t g_ctx_snap_len;

static uint32_t param_block_len(int ci) {
    uint32_t end = 0;
    for (uint32_t i = 0; i < k_cls[ci].n; i++) if (k_cls[ci].tab[i].off + 0xa0u > end) end = k_cls[ci].tab[i].off + 0xa0u;
    return end;
}

static void apply_overrides(rx_addr P, int ci) {
    for (uint32_t i = 0; i < k_cls[ci].n; i++) {
        const uint32_t m = g_cfg.mask[ci][i];
        if (!m) continue;
        const ps_param* p = &g_cat.p[ci][i];
        const rx_addr rec = P + k_cls[ci].tab[i].off;
        if (U32(rec, 0x40) != p->kind) continue;   /* not this table's record (never: the registration wrote it) */
        if (p->kind == 2 && (int)U32(rec, 0x44) != p->knots) continue;   /* the file gave another knot count than the catalog read */
        if (p->kind == 2) {   /* the x knots must stay in order: a config that would unorder them leaves the x as they are */
            float x[10]; int ok = 1;
            for (int k = 0; k < p->knots; k++) {
                const int c = p->knots + k;
                x[k] = (m >> c & 1) ? bf(g_cfg.val[ci][i][c]) : F32(rec, comp_off(p, c));
                if (k > 0 && !(x[k] >= x[k - 1])) ok = 0;
            }
            for (int c = 0; c < p->ncomp; c++) {
                if (!(m >> c & 1)) continue;
                if (!ok && c >= p->knots && c < 2 * p->knots) continue;
                SU32(rec, comp_off(p, c), g_cfg.val[ci][i][c]);
            }
            continue;
        }
        for (int c = 0; c < p->ncomp; c++) if (m >> c & 1) SU32(rec, comp_off(p, c), g_cfg.val[ci][i][c]);
    }
}

/* PORT ADDITION (core/plan/LEADERBOARDS.md C.1): the class of every parameter block read this session, armed or not, so the session can
 * name the local human's vehicle class (rx_session_local_phys_class) without PHYSICS STUDIO. Only host statics: no snapshot, no override. */
#define PS_CLASS_SEEN 32
static struct { rx_addr P; int cls; } g_cls_seen[PS_CLASS_SEEN];
static uint32_t g_cls_seen_n;
int rx_ps_class_of_sim(rx_addr S) {
    if (!S) return -1;
    for (uint32_t k = 0; k < g_cls_seen_n && k < PS_CLASS_SEEN; k++) {   /* newest first (a simulation block may be reused) */
        const uint32_t i = (g_cls_seen_n - 1 - k) % PS_CLASS_SEEN;
        if (g_cls_seen[i].P == S + 0x2f10) return g_cls_seen[i].cls;
    }
    return -1;
}

/* ---- ALIVE CLASSES (phys_studio.h) ------------------------------------------------------------------------------------------- */

typedef struct { char name[32]; uint32_t m[16]; } vp_node;
typedef struct { int32_t model, cls, atv; ps_param p[108]; uint32_t n_in_file; vp_node nodes[64]; uint32_t n_nodes; } vp_entry;
/* the class's registered table: the motocross bikes' (108 records, root type "mx") or the ATV's (103, "atv") */
static int vp_is_atv(int32_t cls) { return cls == RX_VP_ATV1000; }
static vp_entry g_vp[RX_VP_MAX];
static int g_vp_n;
int g_rx_vp_on;
static int32_t g_vp_ctx_model = -1;
static int g_vp_ctx_local, g_vp_ctx_entry = -1;

void rx_vp_clear(void) { g_vp_n = 0; g_rx_vp_on = 0; g_vp_ctx_model = -1; g_vp_ctx_local = 0; g_vp_ctx_entry = -1; }

/* the bike's own hierarchy: "<node name> <16 floats>" lines (a row-major local matrix, the game's row-vector convention, translation in
 * 12..14), written over the made vehicle's model hierarchy (V+0x1328 + 0x10) node locals of the same names (rx_ps_vehicle_end) */
int rx_vp_set_nodes(int32_t vehicle_model, const char* text) {
    vp_entry* e = NULL;
    for (int i = 0; i < g_vp_n; i++) if (g_vp[i].model == vehicle_model) e = &g_vp[i];
    if (!e || !text) return -1;
    e->n_nodes = 0;
    for (const char* p = text; *p && e->n_nodes < 64;) {
        vp_node* nd = &e->nodes[e->n_nodes];
        int used = 0; float f[16];
        if (sscanf(p, "%31s %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f%n", nd->name, &f[0], &f[1], &f[2], &f[3], &f[4], &f[5], &f[6], &f[7],
                   &f[8], &f[9], &f[10], &f[11], &f[12], &f[13], &f[14], &f[15], &used) == 17) {
            int ok = 1; for (int k = 0; k < 16; k++) { if (!isfinite(f[k]) || fabsf(f[k]) > 100.0f) ok = 0; nd->m[k] = fb(f[k]); }
            if (ok) e->n_nodes++;
        }
        const char* nl = strchr(p, '\n'); if (!nl) break; p = nl + 1;
    }
    return (int)e->n_nodes;
}

/* the guest string at a equals the host string s, case-insensitively */
static int guest_ieq(rx_addr a, const char* s) {
    for (uint32_t k = 0;; k++) {
        int x = U8(a, k), y = (unsigned char)s[k];
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
        if (!x) return 1;
        if (k > 64) return 0;
    }
}

/* ALIVE CLASSES: the made vehicle's model hierarchy takes the bike's own rest locals (the nodes the bike names), and the fender
 * vibration nodes' rest translations (vf28's copies V+0x1530 / +0x1540) follow */
static void vp_nodes(rx_addr V, const vp_entry* e) {
    const rx_addr model = V ? U32(V, 0x1328) : 0;
    if (!model || !e->n_nodes) return;
    const rx_addr h = model + 0x10;
    uint32_t done = 0;
    for (int32_t i = 0; i < I32(h, 0xc) && i < 128; i++) {
        const rx_addr rec = U32(h, 8) + 16u * (uint32_t)i;
        if (U16(rec, 8) == 0xffff) continue;
        const rx_addr nm = U16(rec, 8) + U32(U32(rec, 0), 0x10);
        for (uint32_t k = 0; k < e->n_nodes; k++) {
            if (!guest_ieq(nm, e->nodes[k].name)) continue;
            const rx_addr L = U32(U32(rec, 0), 0x18) + ((uint32_t)U8(rec, 0xb) << 6);
            for (int c = 0; c < 16; c++) SU32(L, 4u * (uint32_t)c, e->nodes[k].m[c]);
            done++;
            break;
        }
        SU8(rec, 0xc, U8(rec, 0xc) & ~1u);   /* the world matrices evaluated again */
    }
    const rx_addr fv = e->atv ? 0 : U32(V, 0x1550), bv = e->atv ? 0 : U32(V, 0x1554);   /* (the motocross vf28's copies: an ATV's +0x1550 are other nodes) */
    if (fv) for (uint32_t c = 0; c < 0x10; c += 4) SU32(V, 0x1530 + c, U32(U32(U32(fv, 0), 0x18) + ((uint32_t)U8(fv, 0xb) << 6) + 0x30, c));
    if (bv) for (uint32_t c = 0; c < 0x10; c += 4) SU32(V, 0x1540 + c, U32(U32(U32(bv, 0), 0x18) + ((uint32_t)U8(bv, 0xb) << 6) + 0x30, c));
    fprintf(stderr, "rxcore: ALIVE CLASS %d: %u of the model's hierarchy nodes take the bike's own rest pose\n", (int)e->cls, done);
}

/* the file's values as the original's reader would read them (load_class's reading, over the registration defaults) */
int rx_vp_set(int32_t vehicle_model, int32_t cls, const uint8_t* data, uint32_t size, char* report, uint32_t cap) {
    if (report && cap) report[0] = 0;
    if (cls < RX_VP_MX50 || cls >= RX_VP_CLASS_END || vehicle_model < 0 || !data || size < 16 || size > (1u << 20)) return -1;
    vp_entry* e = NULL;
    for (int i = 0; i < g_vp_n; i++) if (g_vp[i].model == vehicle_model) e = &g_vp[i];
    if (!e) { if (g_vp_n >= RX_VP_MAX) return -2; e = &g_vp[g_vp_n]; }
    vp_entry t; memset(&t, 0, sizeof t);
    t.model = vehicle_model; t.cls = cls; t.atv = vp_is_atv(cls);
    const veh_param* tab = t.atv ? k_atv_params : k_moto_params;
    const char* const* names = t.atv ? k_ps_names_atv : k_ps_names_moto;
    const uint32_t ntab = t.atv ? 103u : 108u;
    fmt_bxml* d = fmt_bxml_parse(data, size);
    if (!d) return -3;
    const char* type = fmt_bxml_attr_str(d, 0, "type");
    if (!type || strcmp(type, t.atv ? "atv" : "mx") != 0) { fmt_bxml_free(d); return -4; }   /* 0043a560's / 0048b680's root check */
    const int nch = fmt_bxml_child_count(d, 0);
    for (int k = 0; k < nch;) {
        const int node = fmt_bxml_child(d, 0, k);
        const char* tag = fmt_bxml_tag(d, node);
        int run = 1;
        while (k + run < nch && strcasecmp(fmt_bxml_tag(d, fmt_bxml_child(d, 0, k + run)), tag) == 0) run++;
        for (uint32_t i = 0; i < ntab; i++) {
            if (strcasecmp(names[i], tag) != 0) continue;
            ps_param* p = &t.p[i];
            p->kind = tab[i].kind; p->in_file = 1; t.n_in_file++;
            float f[20]; memset(f, 0, sizeof f);
            if (p->kind == 0 || p->kind == 1) {
                const int nv = p->kind == 0 ? 1 : 3;
                for (int c = 0; c < nv; c++) f[c] = bf(tab[i].v[c]);
                node_floats(d, node, f, nv);
                p->ncomp = (uint8_t)nv;
                for (int c = 0; c < nv; c++) p->def[c] = fb(f[c]);
            } else if (p->kind == 3) {
                p->ncomp = 3;
                for (int j = 0; j < 3; j++) {
                    float v = 0.0f;
                    if (j < run) node_floats(d, fmt_bxml_child(d, 0, k + j), &v, 1); else v = bf(p->def[0]);
                    p->def[j] = fb(v);
                }
            } else {
                int n = attr_int(d, node, "num_knots");
                if (n < 0) n = 0;
                if (n > 10) n = 10;
                node_floats(d, node, f, 2 * n);
                p->knots = (uint8_t)n; p->ncomp = (uint8_t)(2 * n + 2);
                for (int j = 0; j < n; j++) { p->def[j] = fb(f[2 * j + 1]); p->def[n + j] = fb(f[2 * j]); }
                p->def[2 * n] = fb(attr_float(d, node, "mleft", 0.0f));
                p->def[2 * n + 1] = fb(attr_float(d, node, "mright", 0.0f));
            }
            break;
        }
        k += run;
    }
    fmt_bxml_free(d);
    if (t.n_in_file == 0) return -5;
    *e = t;
    if (e == &g_vp[g_vp_n]) g_vp_n++;
    g_rx_vp_on = 1;
    if (report && cap) snprintf(report, cap, "%u of %u records for VehicleModelId %d (class %d)", t.n_in_file, ntab, (int)vehicle_model, (int)cls);
    return (int)t.n_in_file;
}

static void vp_apply(rx_addr P, const vp_entry* e) {
    const veh_param* tab = e->atv ? k_atv_params : k_moto_params;
    for (uint32_t i = 0; i < (e->atv ? 103u : 108u); i++) {
        const ps_param* p = &e->p[i];
        if (!p->in_file) continue;
        const rx_addr rec = P + tab[i].off;
        if (U32(rec, 0x40) != p->kind) continue;
        if (p->kind == 2) SU32(rec, 0x44, p->knots);
        for (int c = 0; c < p->ncomp; c++) SU32(rec, comp_off(p, c), p->def[c]);
    }
}

void rx_ps_params_loaded(rx_addr P, int cls) {
    int seen = cls;
    if (g_vp_n && g_vp_ctx_local && ((cls >= RX_PS_MX125 && cls <= RX_PS_MX450F) || cls == RX_PS_ATV))
        for (int i = 0; i < g_vp_n; i++)
            if (g_vp[i].model == g_vp_ctx_model && g_vp[i].atv == (cls == RX_PS_ATV)) {
                vp_apply(P, &g_vp[i]); seen = g_vp[i].cls; g_vp_ctx_entry = i;
                fprintf(stderr, "rxcore: ALIVE CLASS %d: VehicleModelId %d's parameters (class %s) overwritten by %u records of its own file\n",
                        (int)g_vp[i].cls, (int)g_vp_ctx_model, k_cls[cls].id, g_vp[i].n_in_file);
                break;
            }
    if (seen >= 0 && seen < RX_VP_CLASS_END) { g_cls_seen[g_cls_seen_n % PS_CLASS_SEEN].P = P; g_cls_seen[g_cls_seen_n % PS_CLASS_SEEN].cls = seen; g_cls_seen_n++; }
    if (!g_rx_ps_armed || !(g_ctx_local || g_cfg.all_vehicles) || cls < 0 || cls >= RX_PS_CLASSES) return;
    g_ctx_cls = cls; g_ctx_P = P;
    g_ctx_snap_len = param_block_len(cls);   /* the file's values, for a live change later */
    for (uint32_t k = 0; k < g_ctx_snap_len; k += 4) { const uint32_t v = U32(P, k); memcpy(g_ctx_snap + k, &v, 4); }
    g_ctx_touched = g_cfg.on && g_cat.loaded;
    if (g_ctx_touched) apply_overrides(P, cls);
}

void rx_ps_vehicle_begin(rx_addr spec) {
    if (g_vp_n) {   /* ALIVE CLASSES: the vehicle being made (its model record's VehicleModelId, a local player's) */
        const rx_addr M = U32(spec, 8);
        g_vp_ctx_model = M ? I32(M, 0) : -1;
        g_vp_ctx_local = rx_vehicle_spec_controller_of(I32(spec, 4)) >= 0;
    }
    if (!g_rx_ps_armed) return;
    g_ctx_cls = -1; g_ctx_P = 0;
    g_ctx_local = rx_vehicle_spec_controller_of(I32(spec, 4)) >= 0;
}
void rx_ps_vehicle_end(rx_addr spec) {
    if (g_vp_ctx_entry >= 0 && g_vp_ctx_entry < g_vp_n) vp_nodes(U32(spec, 0xc4), &g_vp[g_vp_ctx_entry]);
    g_vp_ctx_model = -1; g_vp_ctx_local = 0; g_vp_ctx_entry = -1;
    if (!g_rx_ps_armed) return;
    const rx_addr V = U32(spec, 0xc4);
    if ((g_ctx_local || g_cfg.all_vehicles) && V && g_nlocal < PS_LOCAL) {
        const rx_addr S = U32(V, 0x234);
        int k = g_nlocal++;
        g_local[k].V = V; g_local[k].S = S; g_local[k].local = g_ctx_local; g_local[k].touched = g_ctx_touched;
        g_local[k].cls = g_ctx_P == S + 0x2f10 ? g_ctx_cls : -1;
        g_local[k].has_snap = g_local[k].cls >= 0;
        g_local[k].snap_len = g_ctx_snap_len;
        if (g_local[k].has_snap) memcpy(g_local[k].snap, g_ctx_snap, g_ctx_snap_len);
        fprintf(stderr, "rxcore: PHYSICS STUDIO: %s vehicle %08x (simulation %08x, class %s)%s\n", g_ctx_local ? "local" : "computer rider's", V, S,
                g_local[k].cls >= 0 ? k_cls[g_local[k].cls].id : "?", g_cfg.on ? ": the config's values are applied" : "");
    }
    g_ctx_local = 0; g_ctx_cls = -1; g_ctx_P = 0; g_ctx_touched = 0;
}

float rx_ps_grip(uint32_t material, rx_addr vehicle, float k) {
    if (!g_rx_ps_grip_on) return k;
    const int g = material == 0 ? 0 : material == 1 ? 1 : material == 3 ? 3 : material == 4 ? 4 : material == 5 ? 5 : 2;
    if (!(g_cfg.grip_mask >> g & 1)) return k;
    if (!g_cfg.grip_global) {
        int local = 0;
        for (int i = 0; i < g_nlocal; i++) if (g_local[i].V == vehicle && g_local[i].local) local = 1;
        if (!local) return k;
    }
    return g_cfg.grip[g];
}
float rx_ps_drag(uint32_t material, float k) {
    if (!g_rx_ps_grip_on) return k;
    const int g = material == 0 ? 0 : material == 2 ? 1 : material == 3 ? 2 : 3;
    return (g_cfg.drag_mask >> g & 1) ? g_cfg.drag[g] : k;
}
/* a config with changes was set at some point of this session (a live edit too, a later revert to the original's values included):
 * the event keeps no offline records or highlights (the leave's commit and the highlight progress test it) */
static int g_tainted;
int rx_ps_records_off(void) { return g_cfg.on || g_tainted; }
int rx_ps_active(void) { return (g_cfg.on ? 1 | (g_restored ? 2 : 0) : 0) | (g_tainted ? 4 : 0); }
void rx_ps_arm(int on) { g_live_asked = on; g_rx_ps_armed = g_cfg.on || g_live_asked; }

/* ---- the config text --------------------------------------------------------------------------------------------------------
 *   v <class id> <parameter name> <component> <value>      a vehicle value (tab separated; '#' starts a comment line)
 *   g <material 0..5> <grip>                              00461680's material grip
 *   d <material 0..3> <factor>                            00461760's factor (everyone: the function has no vehicle)
 *   o grip_global <0 | 1>                                 the grip table for every vehicle, not only the local players' */
static int find_class(const char* id) {
    for (int ci = 0; ci < RX_PS_CLASSES; ci++) if (strcasecmp(k_cls[ci].id, id) == 0) return ci;
    return -1;
}
static int find_param(int ci, const char* nm) {
    for (uint32_t i = 0; i < k_cls[ci].n; i++) if (strcasecmp(k_cls[ci].names[i], nm) == 0) return (int)i;
    return -1;
}
static int parse_value(const char* s, float* out) {
    char* end = NULL;
    const double d = strtod(s, &end);
    if (!s[0] || end == s || !isfinite(d)) return 0;
    while (*end == ' ' || *end == '\r') end++;
    if (*end) return 0;
    *out = (float)d;
    return isfinite(*out);
}
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void rx_ps_reset(void) {
    memset(&g_cfg, 0, sizeof g_cfg);
    g_rx_ps_grip_on = 0;
    g_rx_ps_armed = g_live_asked;
}

int rx_ps_set_text(const char* text, char* report, uint32_t cap) {
    ps_w rep = {report, cap, 0};
    if (!g_cat.loaded) { wr(&rep, "the parameter catalog is not loaded\n"); wr_end(&rep, NULL); return -2; }
    rx_ps_reset();
    int applied = 0, line_no = 0, clamped = 0, bad = 0;
    const char* p = text ? text : "";
    while (*p && line_no < 100000) {
        const char* e = strchr(p, '\n');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        char line[512];
        line_no++;
        if (n >= sizeof line) { bad++; wr(&rep, "line %d: too long\n", line_no); p = e ? e + 1 : p + n; continue; }
        memcpy(line, p, n); line[n] = 0;
        p = e ? e + 1 : p + n;
        if (n && line[n - 1] == '\r') line[--n] = 0;
        if (!line[0] || line[0] == '#') continue;
        char* f[6] = {0}; int nf = 0;
        for (char* q = line; nf < 6;) { f[nf++] = q; char* t = strchr(q, '\t'); if (!t) break; *t = 0; q = t + 1; }
        float v;
        if (f[0][0] == 'v' && !f[0][1] && nf == 5) {
            const int ci = find_class(f[1]);
            const int pi = ci >= 0 ? find_param(ci, f[2]) : -1;
            char* end = NULL;
            const long c = strtol(f[3], &end, 10);
            if (ci < 0) { bad++; wr(&rep, "line %d: unknown vehicle class '%.40s'\n", line_no, f[1]); continue; }
            if (pi < 0) { bad++; wr(&rep, "line %d: %s has no parameter '%.64s'\n", line_no, k_cls[ci].id, f[2]); continue; }
            const ps_param* pp = &g_cat.p[ci][pi];
            if (!end || *end || c < 0 || c >= pp->ncomp) { bad++; wr(&rep, "line %d: %s has no value %.8s\n", line_no, f[2], f[3]); continue; }
            if (!parse_value(f[4], &v)) { bad++; wr(&rep, "line %d: '%.32s' is not a number\n", line_no, f[4]); continue; }
            const float cv = clampf(v, pp->lo[c], pp->hi[c]);
            if (cv != v) clamped++;
            g_cfg.mask[ci][pi] |= 1u << c;
            g_cfg.val[ci][pi][c] = fb(cv);
            applied++;
        } else if ((f[0][0] == 'g' || f[0][0] == 'd') && !f[0][1] && nf == 3) {
            const int dg = f[0][0] == 'd', lim = dg ? RX_PS_DRAG : RX_PS_GRIP;
            char* end = NULL;
            const long m = strtol(f[1], &end, 10);
            if (!end || *end || m < 0 || m >= lim) { bad++; wr(&rep, "line %d: no material %.8s\n", line_no, f[1]); continue; }
            if (!parse_value(f[2], &v)) { bad++; wr(&rep, "line %d: '%.32s' is not a number\n", line_no, f[2]); continue; }
            const float cv = clampf(v, 0.0f, 2.0f);
            if (cv != v) clamped++;
            if (dg) { g_cfg.drag[m] = cv; g_cfg.drag_mask |= (uint8_t)(1u << m); }
            else { g_cfg.grip[m] = cv; g_cfg.grip_mask |= (uint8_t)(1u << m); }
            applied++;
        } else if (f[0][0] == 'o' && !f[0][1] && nf == 3 && strcmp(f[1], "grip_global") == 0) {
            g_cfg.grip_global = atoi(f[2]) != 0;
        } else if (f[0][0] == 'o' && !f[0][1] && nf == 3 && strcmp(f[1], "all_vehicles") == 0) {
            g_cfg.all_vehicles = atoi(f[2]) != 0;
        } else { bad++; wr(&rep, "line %d: not a config line\n", line_no); }
    }
    g_cfg.on = applied > 0;
    if (g_cfg.on) g_tainted = 1;
    g_rx_ps_grip_on = g_cfg.grip_mask || g_cfg.drag_mask;
    g_rx_ps_armed = g_cfg.on || g_live_asked;
    if (clamped) wr(&rep, "%d value(s) clamped to their bounds\n", clamped);
    wr(&rep, "%d value(s) set, %d line(s) refused\n", applied, bad);
    wr_end(&rep, NULL);
    return applied;
}

int rx_ps_get_text(char* out, uint32_t cap, uint32_t* required) {
    ps_w w = {out, cap, 0};
    for (int ci = 0; ci < RX_PS_CLASSES; ci++)
        for (uint32_t i = 0; i < k_cls[ci].n; i++)
            for (int c = 0; c < RX_PS_MAX_COMPS; c++)
                if (g_cfg.mask[ci][i] >> c & 1) wr(&w, "v\t%s\t%s\t%d\t%.9g\n", k_cls[ci].id, k_cls[ci].names[i], c, (double)bf(g_cfg.val[ci][i][c]));
    for (int g = 0; g < RX_PS_GRIP; g++) if (g_cfg.grip_mask >> g & 1) wr(&w, "g\t%d\t%.9g\n", g, (double)g_cfg.grip[g]);
    for (int g = 0; g < RX_PS_DRAG; g++) if (g_cfg.drag_mask >> g & 1) wr(&w, "d\t%d\t%.9g\n", g, (double)g_cfg.drag[g]);
    if (g_cfg.grip_global) wr(&w, "o\tgrip_global\t1\n");
    if (g_cfg.all_vehicles) wr(&w, "o\tall_vehicles\t1\n");
    return wr_end(&w, required);
}

int rx_ps_live_text(char* out, uint32_t cap, uint32_t* required) {
    ps_w w = {out, cap, 0};
    for (int k = 0; k < g_nlocal; k++) {
        const int ci = g_local[k].cls;
        if (ci < 0 || !g_cat.loaded) { wr(&w, "L\t%d\t?\t%08x\t%d\n", k, g_local[k].S, g_local[k].local); continue; }
        const rx_addr P = g_local[k].S + 0x2f10;
        wr(&w, "L\t%d\t%s\t%08x\t%d\n", k, k_cls[ci].id, g_local[k].S, g_local[k].local);
        for (uint32_t i = 0; i < k_cls[ci].n; i++) {
            const ps_param* p = &g_cat.p[ci][i];
            const rx_addr rec = P + k_cls[ci].tab[i].off;
            for (int c = 0; c < p->ncomp; c++) {
                const uint32_t now = U32(rec, comp_off(p, c));
                wr(&w, "l\t%d\t%s\t%d\t%08x\t%08x\n", k, k_cls[ci].names[i], c, now, p->def[c]);
            }
        }
    }
    return wr_end(&w, required);
}

/* the original's own live reload (bike_sim.c 0043b690's pending-file path / vehicle_atv_leaves.c 0048d320): the parameter block
 * rewritten, then the masses 0043b750 / 0048c750 (which end in vf35, the garage tuning over the new values). Here the block is the
 * file's values kept at the load, the config over them. */
int rx_ps_apply_live(void) {
    int n = 0;
    g_restored = 0;   /* a new edit: rx_ps_active's bit 2 tells about this one */
    for (int k = 0; k < g_nlocal; k++) {
        const int ci = g_local[k].cls;
        const int reach = g_local[k].local || g_cfg.all_vehicles;   /* the config's vehicles; one that held values before gets the file's back */
        if (ci < 0 || !(reach || g_local[k].touched) || !g_local[k].has_snap || !k_cls[ci].live || !g_cat.loaded) continue;
        g_local[k].off = 0; g_local[k].good = 0;   /* a new edit: the guard watches it afresh */
        const rx_addr S = g_local[k].S, P = S + 0x2f10;
        for (uint32_t i = 0; i < k_cls[ci].n; i++) {   /* only the value words: names, kinds and the record list stay */
            const ps_param* p = &g_cat.p[ci][i];
            const uint32_t off = k_cls[ci].tab[i].off;
            for (int c = 0; c < p->ncomp; c++) {
                uint32_t v; memcpy(&v, g_local[k].snap + off + comp_off(p, c), 4);
                SU32(P + off, comp_off(p, c), v);
            }
        }
        g_local[k].touched = g_cfg.on && reach;
        if (g_local[k].touched) apply_overrides(P, ci);
        ps_masses(ci, S);
        n++;
    }
    return n;
}

/* ---- the guard ---------------------------------------------------------------------------------------------------------------
 * A config can make the original's solver diverge (a 5 % mass on a 5x spring: the bike's speed goes 800 -> 24,000 -> 680,000 ft/s in
 * three frames); far outside the world the terrain grid's lookups then walk garbage records (a hang in 007..'s overlap pass). The
 * original never gets there with its own values. So while a config is set, before each frame, every local vehicle the config reached
 * is checked: its bodies' positions (S + 0xf0 + 0xa0 i) and velocities (+0x100), orientations (S + 0x730 + 0xa0 i) and angular
 * velocities (+0x740) finite, speed under 2000 ft/s (1360 mph), within 60,000 ft of the origin. A vehicle that fails gets the file's
 * values back (the live reload: 0043b750 / 0048c750 + vf35), its bodies the last sane frame's positions and orientations with no
 * velocity, and the config is off for it until the next event. Nothing runs without a config. */
static int sane(float x, float lim) { return isfinite(x) && fabsf(x) < lim; }
void rx_ps_guard(void) {
    if (!g_cfg.on) return;
    for (int k = 0; k < g_nlocal; k++) {
        const rx_addr S = g_local[k].S;
        const int ci = g_local[k].cls;
        if (!S || ci < 0 || g_local[k].off) continue;
        uint32_t nb = U32(S + 0xd0, 0x1c);
        if (nb > 8) nb = 8;
        int ok = 1;
        for (uint32_t i = 0; i < nb && ok; i++) {
            const rx_addr b = S + 0xa0 * i;
            float v2 = 0.0f;
            for (int c = 0; c < 3; c++) {
                const float p = F32(b, 0xf0 + 4 * c), v = F32(b, 0x100 + 4 * c), w = F32(b, 0x740 + 4 * c);
                if (!sane(p, 60000.0f) || !sane(v, 2000.0f) || !sane(w, 1000.0f)) ok = 0;
                v2 += v * v;
            }
            for (int c = 0; c < 4; c++) if (!sane(F32(b, 0x730 + 4 * c), 2.0f)) ok = 0;
            if (!(v2 < 2000.0f * 2000.0f)) ok = 0;
        }
        if (ok) {   /* the last sane frame */
            for (uint32_t i = 0; i < nb; i++)
                for (int c = 0; c < 4; c++) { g_local[k].body[i][c] = U32(S, 0xf0 + 0xa0 * i + 4 * c); g_local[k].body[i][4 + c] = U32(S, 0x730 + 0xa0 * i + 4 * c); }
            g_local[k].good = 1;
            continue;
        }
        g_local[k].off = 1; g_restored++;
        fprintf(stderr, "rxcore: PHYSICS STUDIO: vehicle %08x (%s) diverged under the config: the file's values are back for it, its bodies at "
                        "the last sane frame\n", g_local[k].V, k_cls[ci].id);
        if (g_local[k].has_snap && g_cat.loaded) {   /* the file's values (only the value words) and the masses again */
            const rx_addr P = S + 0x2f10;
            for (uint32_t i = 0; i < k_cls[ci].n; i++) {
                const ps_param* p = &g_cat.p[ci][i];
                const uint32_t off = k_cls[ci].tab[i].off;
                for (int c = 0; c < p->ncomp; c++) { uint32_t v; memcpy(&v, g_local[k].snap + off + comp_off(p, c), 4); SU32(P + off, comp_off(p, c), v); }
            }
            ps_masses(ci, S);
        }
        for (uint32_t i = 0; i < nb; i++) {
            if (g_local[k].good) for (int c = 0; c < 4; c++) { SU32(S, 0xf0 + 0xa0 * i + 4 * c, g_local[k].body[i][c]); SU32(S, 0x730 + 0xa0 * i + 4 * c, g_local[k].body[i][4 + c]); }
            for (int c = 0; c < 4; c++) { SU32(S, 0x100 + 0xa0 * i + 4 * c, 0); SU32(S, 0x740 + 0xa0 * i + 4 * c, 0); }
        }
    }
}
