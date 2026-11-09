#include "../src/fmt_modtrack.h"
#include "race_outrun.h"
#include "rider_cap.h"
#include "waypoint_gate.h"
#include "loop_cam_replay.h"
/* build_scene.c -- see build_scene.h. Hand transliteration from the disassembly of FUN_004046a0 (the factory), FUN_0080df90,
 * FUN_0080de70, FUN_007d1dc0 / 007d1ef0 / 007d1fb0, FUN_007d2f70 / 007d3580 / 007d2500, Entity::vf5 0080ded0 and the RSData readers
 * 0080e180 .. 0080f370. The XMLReader of the original is replaced by the fmt readers (an entity = fmt_entity): the readers below
 * answer exactly what the original's helpers answer for the same document (absent field -> the helper's default). */
#include "build_scene_int.h"
#include "race_section.h"
#include "race_minigames.h"
#include "build_asset.h"
#include "build_world.h"
#include "build_terrain.h"
#include "world_node.h"
#include "build_collision.h"
#include "../src/fmt_bxml.h"
#include "activity_cues.h"
#include "crt.h"
#include "map_scale.h"

/* ---------------------------------------------------------------------------------------------------------------------------
 * the reader */

static const struct fmt_entity* g_rs;                    /* DAT_00a9583c */
const struct fmt_entity* rx_scene_rs(void) { return g_rs; }
void rx_scene_set_rs(const struct fmt_entity* rs) { g_rs = rs; }

static void fatal(const char* what, uint32_t a) { fprintf(stderr, "rxcore: scene: %s %08x\n", what, a); abort(); }

/* kind -> the tag of the value child (the table at 0x009b73e8) */
static const char* const kind_tag[14] = { 0, "String", "Enum", "EnumValue", "Bool", "Int", "UInt", "Float", "Color", "Matrix", "Vector3", "Vector4", "NodeId", "Asset" };

int rs_field(const char* field) { return g_rs ? fmt_entity_field(g_rs, field) : -1; }
/* FUN_0080e180: kind 0 (a null tag) asks for any value child */
int rs_has(const char* field, int kind) {
    if (!g_rs) return 0;
    return kind == 0 ? fmt_entity_field_child(g_rs, field, 0) >= 0 : fmt_entity_field_tagged(g_rs, field, kind_tag[kind]) >= 0;
}

/* the typed value of a field's child of that kind (NULL when the field or the child is missing) */
static int value_of(const char* field, int kind, fmt_bxml_value* v) {
    int c = g_rs ? fmt_entity_field_tagged(g_rs, field, kind_tag[kind]) : -1;
    return c >= 0 && fmt_bxml_node_value(g_rs->doc, c, v) == 0;
}
const char* rs_str(const char* field) {
    fmt_bxml_value v;
    if (value_of(field, RS_STRING, &v) && v.kind == FMT_BX_STRING) return v.str;
    return "";
}
uint32_t rs_bool(const char* field) {
    fmt_bxml_value v;
    if (value_of(field, RS_BOOL, &v) && v.count >= 1) return fmt_bx_u32(&v, 0);
    return 0;
}
int32_t rs_int(const char* field) {
    fmt_bxml_value v;
    if (value_of(field, RS_INT, &v) && v.count >= 1) return fmt_bx_i32(&v, 0);
    return 0;
}
uint32_t rs_uint(const char* field) {
    fmt_bxml_value v;
    if (value_of(field, RS_UINT, &v) && v.count >= 1) return fmt_bx_u32(&v, 0);
    return 0;
}
int32_t rs_enum(const char* field) {
    fmt_bxml_value v;
    if (value_of(field, RS_ENUMVALUE, &v) && v.count >= 1) return fmt_bx_i32(&v, 0);
    return 0;
}
const char* rs_enum_text(const char* field) {
    fmt_bxml_value v;
    if (value_of(field, RS_ENUM, &v) && v.kind == FMT_BX_STRING) return v.str;
    return NULL;
}
/* MAP SCALE (port, map_scale.h): the RSData fields the core reads that are world distances or points (the replay cameras' FOV ranges,
 * an animal's flee distance and homes, the 30-second board's sign, an AI avoid sphere, the snake area, the particle draw distances) */
static int ms_world_field(const char* f) {
    static const char* const names[] = {"ProximityDistance", "Home2", "Home3", "ShowSignPosition", "CloseRange", "MidRange", "FarRange",
                                        "SnakeAreaRadius", "Radius", "MaxViewDistance", "DrawDistance1", "DrawDistance2", "DrawDistance3",
                                        "DrawDistance4"};
    for (unsigned k = 0; k < sizeof names / sizeof *names; k++) if (!strcmp(f, names[k])) return 1;
    return 0;
}
static float ms_field(const char* field, float v) {
    const float k = rx_ms_load();
    return k != 1.0f && ms_world_field(field) ? v * k : v;
}
float rs_float(const char* field) {
    fmt_bxml_value v;
    if (value_of(field, RS_FLOAT, &v) && v.count >= 1) return ms_field(field, fmt_bx_f32(&v, 0, 0));
    return 0.0f;
}
int rs_matrix(const char* field, float out[16]) {
    if (fmt_modtrack_active() && g_rs && strstr(g_rs->type,"StartEntityFreeRide") && !strcmp(field,"Matrix")) { fmt_modtrack_start(out); rx_ms_scene_matrix(out); return 1; }
    fmt_bxml_value v;
    if (!value_of(field, RS_MATRIX, &v) || v.count < 1) return 0;
    fmt_bx_floats(&v, 0, out);
    rx_ms_scene_matrix(out);   /* MAP SCALE (port): the placement (and a volume's size) scaled; nothing at 1 */
    return 1;
}
int rs_vec3(const char* field, float out[3]) {
    fmt_bxml_value v;
    if (!value_of(field, RS_VEC3, &v) || v.count < 1) return 0;
    for (int k = 0; k < 3; k++) out[k] = ms_field(field, fmt_bx_f32(&v, 0, k));
    return 1;
}
int rs_vec4(const char* field, float out[4]) {
    fmt_bxml_value v;
    if (!value_of(field, RS_COLOR, &v) && !value_of(field, RS_VEC4, &v)) return 0;
    if (v.count < 1) return 0;
    for (int k = 0; k < 4; k++) out[k] = fmt_bx_f32(&v, 0, k);
    return 1;
}
/* FUN_0080ec70 (field, out): the handle of the Asset child (attributes name and type; the type is cut to 8 characters) */
rx_addr rs_asset(const char* field) {
    int c = g_rs ? fmt_entity_field_tagged(g_rs, field, "Asset") : -1;
    const char* name = c >= 0 ? fmt_bxml_attr_str(g_rs->doc, c, "name") : NULL;
    const char* type = c >= 0 ? fmt_bxml_attr_str(g_rs->doc, c, "type") : NULL;
    if (!name || !type) { fprintf(stderr, "rxcore: scene: entity %u: no asset field %s\n", g_rs ? g_rs->id : 0, field); abort(); }
    return rx_asset_handle(type, name);
}

/* FUN_0080e940 (EDI = dest): a Color field read into 4 floats; absent field: dest untouched. The field's version attribute (default 1)
 * decides whether the values are 0..255 (version <= 1: scaled by 1/255 = 0x009775a0). */
void scene_rs_color_to(const char* field, rx_addr dest) {
    float c[4];
    int f = rs_field(field);
    if (f < 0) return;
    const struct fmt_entity* rs = rx_scene_rs();
    int version = fmt_bxml_attr_i32(rs->doc, f, "version", 1);
    if (!rs_vec4(field, c)) return;
    if (version <= 1) {
        const float k = rx_bf(0x3b808081u) /* 0x009775a0 */;
        for (int i = 0; i < 4; i++) c[i] = c[i] * k;
    }
    for (int i = 0; i < 4; i++) SF32(dest, 4 * i, c[i]);
}

/* FUN_0080ed50 (ESI = list {+0 data, +4 count, +8 capacity, +0xc growth}): the list of Asset handles of the field's RSData children */
void scene_list_append(rx_addr list, uint32_t v) {
    if (I32(list, 8) <= I32(list, 4)) {
        uint32_t cap = U32(list, 0xc) + U32(list, 8);
        rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(cap, 4));
        for (uint32_t k = 0; k < U32(list, 8); k++) SU32(nb, 4 * k, U32(U32(list, 0), 4 * k));
        rx_reflex_aligned_free(U32(list, 0));
        SU32(list, 0, nb); SU32(list, 8, cap);
    }
    SU32(U32(list, 0), 4u * U32(list, 4), v);
    SU32(list, 4, U32(list, 4) + 1);
}
void scene_rs_asset_list(const char* field, rx_addr list) {
    const struct fmt_entity* rs = rx_scene_rs();
    int f = rs_field(field);
    if (f < 0) return;
    int n = fmt_bxml_child_count(rs->doc, f);
    for (int k = 0; k < n; k++) {
        int c = fmt_bxml_child(rs->doc, f, k);
        if (strcmp(fmt_bxml_tag(rs->doc, c), "RSData")) continue;
        int a = fmt_bxml_find_child(rs->doc, c, "Asset");
        const char* name = a >= 0 ? fmt_bxml_attr_str(rs->doc, a, "name") : NULL;
        const char* type = a >= 0 ? fmt_bxml_attr_str(rs->doc, a, "type") : NULL;
        if (!name || !type) { fprintf(stderr, "rxcore: scene: list %s: asset missing\n", field); abort(); }
        scene_list_append(list, rx_asset_handle(type, name));
    }
}


/* the same list with elements of `words` 32-bit words (grows by the growth step: ReflexAlignedAlloc(elements * size), old part copied;
 * 0080f7a0 for 16-byte elements) */
static void list_grow_words(rx_addr list, uint32_t words) {
    if (I32(list, 8) <= I32(list, 4)) {
        uint32_t cap = U32(list, 0xc) + U32(list, 8);
        rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(cap, 4 * words));
        for (uint32_t k = 0; k < U32(list, 8) * words; k++) SU32(nb, 4 * k, U32(U32(list, 0), 4 * k));
        rx_reflex_aligned_free(U32(list, 0));
        SU32(list, 0, nb); SU32(list, 8, cap);
    }
}
static void list_push_words(rx_addr list, const uint32_t* v, uint32_t words) {
    list_grow_words(list, words);
    rx_addr dst = U32(list, 0) + 4 * words * U32(list, 4);
    for (uint32_t k = 0; k < words; k++) SU32(dst, 4 * k, v[k]);
    SU32(list, 4, U32(list, 4) + 1);
}
/* FUN_0080f020 (ESI = list): the Float of every RSData child of the field */
void scene_rs_float_list(const char* field, rx_addr list) {
    const struct fmt_entity* rs = rx_scene_rs();
    int f = rs_field(field);
    if (f < 0) return;
    int n = fmt_bxml_child_count(rs->doc, f);
    for (int k = 0; k < n; k++) {
        int c = fmt_bxml_child(rs->doc, f, k);
        if (strcmp(fmt_bxml_tag(rs->doc, c), "RSData")) continue;
        int v = fmt_bxml_find_child(rs->doc, c, "Float");
        fmt_bxml_value val; uint32_t bits = 0;
        if (v >= 0 && fmt_bxml_node_value(rs->doc, v, &val) == 0 && val.count >= 1) bits = rx_fb(fmt_bx_f32(&val, 0, 0));
        list_push_words(list, &bits, 1);
    }
}
/* FUN_0080f270 (EAX = list of 16-byte elements; stack the field name): the Vector3 value of every Vector3 child of the field's "ListV3"
 * node, three floats into a new 16-byte element (the fourth word stays as the allocation left it). The list grows by 0080f7a0. */
void scene_rs_vec3_list(const char* field, rx_addr list) {
    const struct fmt_entity* rs = rx_scene_rs();
    int f = rs_field(field);
    if (f < 0) return;
    int l = fmt_bxml_find_child(rs->doc, f, "ListV3");
    if (l < 0) return;
    int n = fmt_bxml_child_count(rs->doc, l);
    for (int k = 0; k < n; k++) {
        int c = fmt_bxml_child(rs->doc, l, k);
        if (strcmp(fmt_bxml_tag(rs->doc, c), "Vector3")) continue;
        fmt_bxml_value val; float v[3] = {0, 0, 0};
        if (fmt_bxml_node_value(rs->doc, c, &val) == 0 && val.count >= 1) for (int i = 0; i < 3; i++) v[i] = fmt_bx_f32(&val, 0, i);
        { const float k = rx_ms_load(); if (k != 1.0f) for (int i = 0; i < 3; i++) v[i] *= k; }   /* MAP SCALE (port): world points */
        list_grow_words(list, 4);
        rx_addr dst = U32(list, 0) + 16u * U32(list, 4);
        for (uint32_t i = 0; i < 3; i++) SF32(dst, 4 * i, v[i]);
        SU32(list, 4, U32(list, 4) + 1);
    }
}

/* FUN_0080f130 (ESI = list): the raw Color (4 floats) of every RSData child of "LensFlareColorList" */
void scene_rs_color_list(const char* field, rx_addr list) {
    const struct fmt_entity* rs = rx_scene_rs();
    int f = rs_field(field);
    if (f < 0) return;
    int n = fmt_bxml_child_count(rs->doc, f);
    for (int k = 0; k < n; k++) {
        int c = fmt_bxml_child(rs->doc, f, k);
        if (strcmp(fmt_bxml_tag(rs->doc, c), "RSData")) continue;
        int v = fmt_bxml_find_child(rs->doc, c, "Color");
        fmt_bxml_value val; uint32_t bits[4] = {0, 0, 0, 0};
        if (v >= 0 && fmt_bxml_node_value(rs->doc, v, &val) == 0 && val.count >= 1) for (int i = 0; i < 4; i++) bits[i] = rx_fb(fmt_bx_f32(&val, 0, i));
        list_push_words(list, bits, 4);
    }
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * virtual calls */

typedef uint32_t (*vf_fn0)(rx_addr);
typedef uint32_t (*vf_fn1)(rx_addr, uint32_t);
typedef uint32_t (*vf_fn2)(rx_addr, uint32_t, uint32_t);
typedef uint32_t (*vf_fn3)(rx_addr, uint32_t, uint32_t, uint32_t);
typedef uint32_t (*vf_fn4)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t);

/* CharacterEntity::vf16 00618070 (stack m; RET 4): the matrix becomes the local matrix of the character's root node (the clone's node set at
 * +0x210), then Node_Invalidate. The registered port (anim_scenario_rider.c) invalidates through the original, which the product cannot run:
 * until the integration sweep fixes it the scene unit calls this copy instead (not registered). */
static uint32_t scene_character_set_root_matrix(rx_addr e, rx_addr m) {
    rx_addr rec = U32(e, 0x210);
    rx_addr dst = U32(U32(rec, 0), 0x18) + ((uint32_t)U8(rec, 0xb) << 6);
    scene_copy_words(dst, m, 16);
    rx_world_node_invalidate(rec, 0, 0, 0);
    return 0;
}

uint32_t scene_vcall(rx_addr obj, uint32_t off, int n, const uint32_t* a) {
    rx_addr target = U32(U32(obj, 0), off);
    if (!target) { fprintf(stderr, "rxcore: scene: vcall of slot %#x of object %08x (vtable %08x) has no function\n", off, obj, U32(obj, 0)); abort(); }
    if (target == 0x00618070 && n == 1) return scene_character_set_root_matrix(obj, a[0]);
    void* f = RX_FN_INTS(target, n + 1);
    if (!f) return rx_call_original(target, RX_THISCALL, obj, n, a);
    switch (n) {
    case 0: return ((vf_fn0)f)(obj);
    case 1: return ((vf_fn1)f)(obj, a[0]);
    case 2: return ((vf_fn2)f)(obj, a[0], a[1]);
    case 3: return ((vf_fn3)f)(obj, a[0], a[1], a[2]);
    case 4: return ((vf_fn4)f)(obj, a[0], a[1], a[2], a[3]);
    }
    fatal("vcall with too many arguments", target);
    return 0;
}
static uint32_t vcall0(rx_addr obj, uint32_t off) { return scene_vcall(obj, off, 0, 0); }

/* ---------------------------------------------------------------------------------------------------------------------------
 * the Entity base */

void scene_copy_words(rx_addr dst, rx_addr src, uint32_t n) { for (uint32_t k = 0; k < n; k++) SU32(dst, 4 * k, U32(src, 4 * k)); }
void scene_set_matrix_identity(rx_addr dst) { scene_copy_words(dst, 0x009cb510, 16); }

/* the constructor every class inlines (AnchorEntity 00403e90, ModelEntity 00401db0, ...): the Entity fields, then the class' vtable */
void rx_entity_construct(rx_addr e, uint32_t vtable) {
    SU32(e, 0, 0x0095a2e4);   /* Entity::vftable */
    SU32(e, 0x18, 0); SU32(e, 0x10, 0);
    SF32(e, 0x24, 0.0f); SF32(e, 0x20, 0.0f);
    SU32(e, 0x70, 0);
    scene_set_matrix_identity(e + 0x30);
    SU32(e, 0x1c, 0); SU32(e, 0x7c, 0); SF32(e, 0x78, 0.0f); SU32(e, 0x74, 0); SU32(e, 0x28, 0); SU32(e, 0x2c, 0);
    SU32(e, 0, vtable);
}

/* Entity::vf16 00401d00 (stdcall m): the matrix +0x30 set; Entity::vf15 00401ce0 (stdcall out): copied out */
uint32_t rx_entity_vf16(rx_addr e, rx_addr m) { scene_copy_words(e + 0x30, m, 16); return 0; }
RX_FUNCTION(0x00401d00, rx_entity_vf16)
uint32_t rx_entity_vf15(rx_addr e, rx_addr out) { scene_copy_words(out, e + 0x30, 16); return 0; }
RX_FUNCTION(0x00401ce0, rx_entity_vf15)
/* Entity::vf17 00402c90 (stdcall out; RET 4): the translation row x, y, z (fld / fstp dword copies) */
uint32_t rx_entity_vf17(rx_addr e, rx_addr out) { for (uint32_t k = 0; k < 12; k += 4) SU32(out, k, scene_x87c(U32(e, 0x60 + k))); return 0; }
RX_FUNCTION(0x00402c90, rx_entity_vf17)

/* Entity::vf5 0080ded0 (ECX = e): defaults, the Name (a copy in a ReflexAlignedAlloc block), the Matrix through vf16 (slot 0x40), the
 * ExportName flag into +0x1c */
void rx_entity_vf5(rx_addr e) {
    SF32(e, 0x20, 0.0f); SF32(e, 0x24, 0.0f);
    scene_set_matrix_identity(e + 0x30);
    SU32(e, 0x1c, 0);
    const char* name = rs_str("Name");
    uint32_t n = (uint32_t)strlen(name);
    rx_addr s = rx_reflex_aligned_alloc(n + 1);
    for (uint32_t k = 0; k < n; k++) SU8(s, k, (uint8_t)name[k]);   /* strncpy(dst, name, n) */
    SU8(s, n, 0);
    SU32(e, 0x18, s);
    RX_FRAME_ENTER(F, 0x40);   /* the matrix local (esp + 0x20 of the original's frame) */
    float m[16];
    if (rs_matrix("Matrix", m)) for (uint32_t k = 0; k < 16; k++) SF32(F, 4 * k, m[k]);
    uint32_t a = F;
    scene_vcall(e, 0x40, 1, &a);
    RX_FRAME_LEAVE(F, 0x40);
    SU32(e, 0x1c, rs_bool("ExportName"));
}

uint32_t rx_entity_base_vf5(rx_addr e, uint32_t reader) { (void)reader; rx_entity_vf5(e); return 0; }
RX_FUNCTION(0x0080ded0, rx_entity_base_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * containers of the scene manager */

/* FUN_00544cc0 (ESI = list {+0 data, +4 count, +8 capacity, +0xc growth}, stack &value): appended unless present */
static int list_push_unique(rx_addr list, uint32_t value) {
    int32_t n = I32(list, 4);
    for (int32_t k = 0; k < n; k++) if (U32(U32(list, 0), 4u * (uint32_t)k) == value) return 0;
    if (n >= I32(list, 8)) {   /* grow by the growth step: ReflexAlignedAlloc(growth + capacity) pointers, old ones copied */
        uint32_t cap = rx_rc_grow(U32(list, 8), U32(list, 0xc));   /* (w-rc-core) a zero growth with 13+ players: rider_cap.h */
        rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(cap, 4));
        for (uint32_t k = 0; k < U32(list, 8); k++) SU32(nb, 4 * k, U32(U32(list, 0), 4 * k));
        rx_reflex_aligned_free(U32(list, 0));
        SU32(list, 0, nb); SU32(list, 8, cap);
    }
    SU32(U32(list, 0), 4u * U32(list, 4), value);
    SU32(list, 4, U32(list, 4) + 1);
    return 1;
}

int scene_list_push_unique(rx_addr list, uint32_t value) { return list_push_unique(list, value); }

/* FUN_007cff20 (ESI = list, stack capacity): the array reallocated to `capacity` pointers (the grow step of the tick list) */
static void list_reserve(rx_addr list, uint32_t cap) {
    rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(cap, 4));
    for (uint32_t k = 0; k < U32(list, 8); k++) SU32(nb, 4 * k, U32(U32(list, 0), 4 * k));
    rx_reflex_aligned_free(U32(list, 0));
    SU32(list, 0, nb); SU32(list, 8, cap);
}

/* vector<Entity*>::insert(pos, value) of the scene's sorted vectors (FUN_007f90a0 -> 007f95a0 for growth): +0 proxy, +0xc begin, +0x10
 * end, +0x14 end of capacity. Capacity grows by capacity / 2 (at least size + 1). */
static void vec_insert(rx_addr v, rx_addr pos, uint32_t value) {
    rx_addr first = U32(v, 0xc), last = U32(v, 0x10), cap_end = U32(v, 0x14);
    uint32_t size = (last - first) >> 2, cap = first ? (cap_end - first) >> 2 : 0;
    if (cap < size + 1) {
        uint32_t half = cap >> 1, newcap = (0x3fffffffu - half < cap) ? 0 : cap + half;
        if (newcap < size + 1) newcap = size + 1;
        rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(newcap, 4));
        uint32_t idx = (pos - first) >> 2;
        for (uint32_t k = 0; k < idx; k++) SU32(nb, 4 * k, U32(first, 4 * k));
        SU32(nb, 4 * idx, value);
        for (uint32_t k = idx; k < size; k++) SU32(nb, 4 * (k + 1), U32(first, 4 * k));
        if (first) rx_reflex_aligned_free(first);
        SU32(v, 0xc, nb); SU32(v, 0x10, nb + 4 * (size + 1)); SU32(v, 0x14, nb + 4 * newcap);
    } else {
        for (rx_addr p = last; p > pos; p -= 4) SU32(p, 0, U32(p - 4, 0));
        SU32(pos, 0, value);
        SU32(v, 0x10, last + 4);
    }
}
/* the sorted insert of 007d3580: upper bound by the key at +0x78 */
static void insert_sorted(rx_addr v, rx_addr e) {
    rx_addr first = U32(v, 0xc), last = U32(v, 0x10);
    int32_t n = (int32_t)(last - first) >> 2;
    float key = F32(e, 0x78);
    rx_addr lo = first;
    while (n > 0) {
        int32_t half = n / 2;
        if (key > F32(U32(lo, 4u * (uint32_t)half), 0x78)) { lo = lo + 4u * (uint32_t)half + 4; n += -1 - half; }
        else n = half;
    }
    vec_insert(v, lo, e);
}

/* FUN_007d3580 (stdcall scene, entity): an entity without a sort list (+0x70 == 0) goes to the unsorted list +0x2c28; otherwise it gets a
 * sort key (+0x78; 0 = the next of the global counter 0xa95838, step 0.1) and is inserted by key into the kind-0 (+0x2c3c) or kind-1
 * (+0x2c84) list (vf18), and the +0x2c54 / +0x2c6c lists as its flags +0x28 / +0x2c say. */
static void scene_link(rx_addr s, rx_addr e) {
    if (U32(e, 0x70) == 0) { list_push_unique(s + 0x2c28, e); return; }
    if (F32(e, 0x78) == 0.0f) {
        float k = F32(0, 0x00a95838) + 0.1f /* 0x931d94 */;
        SF32(0, 0x00a95838, k);
        SF32(e, 0x78, k);
    }
    uint32_t kind = vcall0(e, 0x48);
    if (kind == 0) insert_sorted(s + 0x2c3c, e);
    else if (vcall0(e, 0x48) == 1) insert_sorted(s + 0x2c84, e);
    if (U32(e, 0x28) != 0) insert_sorted(s + 0x2c54, e);
    if (U32(e, 0x2c) != 0) insert_sorted(s + 0x2c6c, e);
}

/* FUN_007d2500 (EAX = scene, stack entity): the entity's name in the scene's name map (+0x19f8) unless that name is already there
 * (the map keeps the name pointer of the first entity) */
static void scene_register_name(rx_addr s, rx_addr e) {
    uint32_t found;
    if (!rx_strtab_find(s + 0x19f8, e + 0x18, &found)) rx_strtab_insert(s + 0x19f8, e + 0x18, e);
}

/* FUN_007d2f70 (EDI = entity, stack scene) */
void rx_scene_add_entity(rx_addr s, rx_addr e) {
    scene_link(s, e);
    if (U32(e, 0x1c) != 0) scene_register_name(s, e);
    if (vcall0(e, 0x64) != 0) rx_scene_lights_add(s + 0x1a18, e);
    if (vcall0(e, 0x2c) != 0) {
        rx_addr list = s + 0x2bfc;
        if (!(I32(list, 4) < I32(list, 8))) list_reserve(list, U32(list, 0xc) + U32(list, 8));
        SU32(U32(list, 0), 4u * U32(list, 4), e);
        SU32(list, 4, U32(list, 4) + 1);
    }
    if (vcall0(e, 0x58) == 0xe) SU32(s, 0x2cc0, e);
    vcall0(e, 4);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the factory */

typedef struct { const char* ns; uint32_t id; } type_row;
static const type_row g_types[] = {
    { "ET_Entity", 2 }, { "RSToolsNS_RSDataNS_LWModelRSData", 3 }, { "RSToolsNS_RSDataNS_ModelRSData", 4 },
    { "RSToolsNS_RSDataNS_CollisionRSData", 5 }, { "RSToolsNS_RSDataNS_AmbientLightRSData", 6 }, { "RSToolsNS_RSDataNS_PointLightRSData", 7 },
    { "RSToolsNS_RSDataNS_SpotLightRSData", 8 }, { "RSToolsNS_RSDataNS_DirectionalLightRSData", 9 },
    { "RSToolsNS_RSDataNS_ImageBasedLightRSData", 0xa }, { "RSToolsNS_RSDataNS_AmbientDirectionalLightRSData", 0xb },
    { "RSToolsNS_RSDataNS_SkyRSData", 0xc }, { "RSToolsNS_RSDataNS_PVSEntityRSData", 0xd }, { "RSToolsNS_RSDataNS_DistanceFogRSData", 0xe },
    { "RSToolsNS_RSDataNS_WaterSystemRSData", 0xf }, { "RSToolsNS_RSDataNS_HFTerrainRSData", 0x10 },
    { "RSToolsNS_RSDataNS_HFBumpModelRSData", 0x11 }, { "ET_CharacterEntity", 0x12 }, { "ET_AnimationEntity", 0x13 },
    { "ET_CameraRenderTargetEntity", 0x14 }, { "ET_CubeRenderTargetEntity", 0x15 }, { "ET_ActionAssetPlayer", 0x16 },
    { "ET_DebugOverlay", 0x17 }, { "ET_DevConsole", 0x18 }, { "ET_UIManager", 0x19 }, { "ET_ParticleManager2D", 0x1a },
    { "ET_IPropertyController", 0x1b }, { "RSToolsNS_RSDataNS_TextureAnimControllerRSData", 0x1c },
    { "RSToolsNS_RSDataNS_LevelAmbientAudioZoneRSData", 0x1d }, { "RSToolsNS_RSDataNS_GeneralStorageEntityRSData", 0x1e },
    { "RSToolsNS_RSDataNS_ReflectionMapGenRSData", 0x1f }, { "ET_MXDebugCamera", 0x1001 }, { "ET_MXFirstPersonCamera", 0x1002 },
    { "ET_MXThirdPersonCamera", 0x1003 }, { "ET_MXUICustomizationCamera", 0x1004 }, { "ET_MXUIPartsViewCamera", 0x1005 },
    { "ET_MXUIWorldViewCamera", 0x1006 }, { "ET_MXHockeyCamera", 0x1007 }, { "ET_MXUIManager", 0x1008 }, { "ET_SnakeVisibleMesh", 0x1009 },
    { "ET_OnFieldCrowd", 0x100a }, { "ET_AnimatedCamera", 0x100b }, { "ET_MXQuickCamera", 0x100c }, { "ET_MXStartGateCamera", 0x100d },
    { "RSToolsNS_RSDataNS_GrassSystemRSData", 0x100e }, { "RSToolsNS_RSDataNS_ForestSystemRSData", 0x100f },
    { "RSToolsNS_RSDataNS_CustomizeVehicleStartRSData", 0x1010 }, { "RSToolsNS_RSDataNS_StartEntityFreestyleRSData", 0x1011 },
    { "RSToolsNS_RSDataNS_StartEntitySupermotoRSData", 0x1012 }, { "RSToolsNS_RSDataNS_StartEntityOpencrossRSData", 0x1013 },
    { "RSToolsNS_RSDataNS_StartEntityFreeRideRSData", 0x1014 }, { "RSToolsNS_RSDataNS_StartEntityFreeRideStarRSData", 0x1015 },
    { "RSToolsNS_RSDataNS_StartEntityFreeRideLineRSData", 0x1016 }, { "RSToolsNS_RSDataNS_StartEntitySnakeTagRSData", 0x1017 },
    { "RSToolsNS_RSDataNS_TagBallStartRSData", 0x1018 }, { "RSToolsNS_RSDataNS_ChallengeSpecsRSData", 0x1019 },
    { "RSToolsNS_RSDataNS_AnchorEntityRSData", 0x101a }, { "RSToolsNS_RSDataNS_BigAirStartRSData", 0x101b },
    { "RSToolsNS_RSDataNS_HillClimbStartRSData", 0x101c }, { "RSToolsNS_RSDataNS_JumperStartRSData", 0x101d },
    { "RSToolsNS_RSDataNS_PathfinderStartRSData", 0x101e }, { "RSToolsNS_RSDataNS_StoppieStartRSData", 0x101f },
    { "RSToolsNS_RSDataNS_TargetPracticeStartRSData", 0x1020 }, { "RSToolsNS_RSDataNS_TrailBlazerStartRSData", 0x1021 },
    { "RSToolsNS_RSDataNS_WheelieStartRSData", 0x1022 }, { "RSToolsNS_RSDataNS_StepUpStartRSData", 0x1023 },
    { "RSToolsNS_RSDataNS_StepUpBronzeStartRSData", 0x1024 }, { "RSToolsNS_RSDataNS_StepUpSilverStartRSData", 0x1025 },
    { "RSToolsNS_RSDataNS_StepUpGoldStartRSData", 0x1026 }, { "RSToolsNS_RSDataNS_TerrainRSData", 0x1027 },
    { "RSToolsNS_RSDataNS_TriggerRSData", 0x1028 }, { "RSToolsNS_RSDataNS_PhysicsRSData", 0x1029 }, { "RSToolsNS_RSDataNS_ParticleRSData", 0x102a },
    { "RSToolsNS_RSDataNS_ParticleSphereRSData", 0x102b }, { "RSToolsNS_RSDataNS_ParticleYRingRSData", 0x102c },
    { "RSToolsNS_RSDataNS_BermParticleRSData", 0x102d }, { "RSToolsNS_RSDataNS_FireworksParticleRSData", 0x102e },
    { "RSToolsNS_RSDataNS_TriggeredPlaceableParticleRSData", 0x102f }, { "RSToolsNS_RSDataNS_ParticleManagerRSData", 0x1030 },
    { "RSToolsNS_RSDataNS_PopUpBaleRSData", 0x1031 }, { "RSToolsNS_RSDataNS_PopDownBaleRSData", 0x1032 }, { "RSToolsNS_RSDataNS_DropGateRSData", 0x1033 },
    { "RSToolsNS_RSDataNS_HoleShotLightRSData", 0x1034 }, { "RSToolsNS_RSDataNS_LeaderLightRSData", 0x1035 },
    { "RSToolsNS_RSDataNS_TripleLightRSData", 0x1036 }, { "RSToolsNS_RSDataNS_LeaderBoardRSData", 0x1037 },
    { "RSToolsNS_RSDataNS_TrackSideFlagHolderRSData", 0x1038 }, { "RSToolsNS_RSDataNS_ScoreTowerFlagHolderRSData", 0x1039 },
    { "RSToolsNS_RSDataNS_ThirtySecondBoardRSData", 0x103a }, { "RSToolsNS_RSDataNS_CrewChiefRSData", 0x103b },
    { "RSToolsNS_RSDataNS_GraffitiStartRSData", 0x103c }, { "RSToolsNS_RSDataNS_HockeyStartRSData", 0x103d },
    { "RSToolsNS_RSDataNS_CaptureStartRSData", 0x103e }, { "RSToolsNS_RSDataNS_CaptureBaseRSData", 0x103f },
    { "RSToolsNS_RSDataNS_GraffitiCapturePointRSData", 0x1040 }, { "RSToolsNS_RSDataNS_BillboardCrowdRSData", 0x1041 },
    { "RSToolsNS_RSDataNS_SectionRaceTriggerRSData", 0x1042 }, { "RSToolsNS_RSDataNS_WaypointGateRSData", 0x1043 },
    { "RSToolsNS_RSDataNS_EggHuntEggEntityRSData", 0x1044 }, { "RSToolsNS_RSDataNS_MXFixedCameraRSData", 0x1045 },
    { "RSToolsNS_RSDataNS_MXReplayTowerCameraRSData", 0x1046 }, { "RSToolsNS_RSDataNS_MXReplayZipLineCameraRSData", 0x1047 },
    { "RSToolsNS_RSDataNS_MXReplaySplineCameraRSData", 0x1048 }, { "RSToolsNS_RSDataNS_MXReplayAnimatedCameraRSData", 0x1049 },
    { "RSToolsNS_RSDataNS_MassiveAdEntityRSData", 0x104a }, { "RSToolsNS_RSDataNS_MXCameraPathRSData", 0x104b },
    { "RSToolsNS_RSDataNS_OnFieldCrowdMapRSData", 0x104c }, { "RSToolsNS_RSDataNS_MudManagerRSData", 0x104d },
    { "RSToolsNS_RSDataNS_BillboardRSData", 0x104e }, { "RSToolsNS_RSDataNS_ScenarioAnimEntityRSData", 0x104f },
    { "RSToolsNS_RSDataNS_AnimEntitySingleAnimLoopRSData", 0x1050 }, { "RSToolsNS_RSDataNS_AnimEntitySingleAnimLoopInstancedRSData", 0x1051 },
    { "RSToolsNS_RSDataNS_AnimEntitySplineAnimLoopRSData", 0x1052 }, { "RSToolsNS_RSDataNS_AnimEntitySplineWithStopLogicRSData", 0x1053 },
    { "RSToolsNS_RSDataNS_AnimEntityProximityRSData", 0x1054 }, { "RSToolsNS_RSDataNS_AnimEntityTriggerRSData", 0x1055 },
    { "RSToolsNS_RSDataNS_AnimEntitySoaringRSData", 0x1056 }, { "RSToolsNS_RSDataNS_AnimEntityRoamingProximityRSData", 0x1057 },
    { "RSToolsNS_RSDataNS_AnimEntitySingleAnimLoopWithParticlesRSData", 0x1058 },
    { "RSToolsNS_RSDataNS_AnimEntitySplineAnimLoopWithParticlesRSData", 0x1059 },
    { "RSToolsNS_RSDataNS_AnimEntitySplineWithStopLogicWithParticlesRSData", 0x105a }, { "RSToolsNS_RSDataNS_JumbotronCameraRSData", 0x105b },
    { "RSToolsNS_RSDataNS_AudioTerrainZoneManagerRSData", 0x105c }, { "RSToolsNS_RSDataNS_AudioCollisionManagerRSData", 0x105d },
    { "RSToolsNS_RSDataNS_AudioVolumeLevelMixEntityRSData", 0x105e }, { "RSToolsNS_RSDataNS_VolumeFogRSData", 0x105f },
    { "RSToolsNS_RSDataNS_LightFlareRSData", 0x1060 }, { "RSToolsNS_RSDataNS_LensFlareRSData", 0x1061 }, { "RSToolsNS_RSDataNS_TracksEntityRSData", 0x1062 },
    { "RSToolsNS_RSDataNS_OverlayLightRSData", 0x1063 }, { "RSToolsNS_RSDataNS_HDRSettingsRSData", 0x1064 }, { "RSToolsNS_RSDataNS_ShadowMapRSData", 0x1065 },
    { "RSToolsNS_RSDataNS_ScreenDustRSData", 0x1066 }, { "RSToolsNS_RSDataNS_MudSplatterRSData", 0x1067 },
    { "RSToolsNS_RSDataNS_ScreenWaterDropsRSData", 0x1068 }, { "RSToolsNS_RSDataNS_CameraScreenshotRSData", 0x1069 },
    { "RSToolsNS_RSDataNS_CameraPanoramicRSData", 0x106a }, { "RSToolsNS_RSDataNS_SXSeatedCameraManRSData", 0x106b },
    { "RSToolsNS_RSDataNS_CollectibleRSData", 0x106c }, { "RSToolsNS_RSDataNS_CollectibleHitDiscoveryRSData", 0x106d },
    { "RSToolsNS_RSDataNS_CollectibleTargetPracticeRSData", 0x106e }, { "RSToolsNS_RSDataNS_StepUpBarRSData", 0x106f },
    { "RSToolsNS_RSDataNS_StepUpUprightRSData", 0x1070 }, { "RSToolsNS_RSDataNS_TutorialTriggerRSData", 0x1071 }, { "RSToolsNS_RSDataNS_AAARSData", 0x1072 },
    { "RSToolsNS_RSDataNS_AIAvoidRSData", 0x1073 }, { "RSToolsNS_RSDataNS_MXRaceLine", 0x1074 }, { "RSToolsNS_RSDataNS_RaceLineArrowRSData", 0x1075 },
    { "RSToolsNS_RSDataNS_TDFEntityRSData", 0x1076 }, { "RSToolsNS_RSDataNS_VideoEntityRSData", 0x1077 },
    { "RSToolsNS_RSDataNS_ScrollingTextureRSData", 0x1078 }, { "RSToolsNS_RSDataNS_CameraFacingEntityRSData", 0x1079 },
    { "RSToolsNS_RSDataNS_LevelInfoRSData", 0x107a }, { "RSToolsNS_RSDataNS_TimedSectionTriggerRSData", 0x107b },
    { "RSToolsNS_RSDataNS_MusicTrackListRSData", 0x107c }, { "RSToolsNS_RSDataNS_MusicScoreListRSData", 0x107d },
    { "RSToolsNS_RSDataNS_StatusBoardRSData", 0x107e }, { "RSToolsNS_RSDataNS_UIGateSelectionRSData", 0x107f },
    { "RSToolsNS_RSDataNS_UIGateSelectionCameraRSData", 0x1080 }, { "ET_RiderCharacter", 0x1081 }, { "ET_RavenVehicle", 0x1082 },
    { "RSToolsNS_RSDataNS_AudioEmitterRSData", 0x1083 }, { "RSToolsNS_RSDataNS_AudioLineEmitterRSData", 0x1084 },
    { "RSToolsNS_RSDataNS_AudioCrowdEmitterRSData", 0x1085 }, { "RSToolsNS_RSDataNS_AudioZoneSphereRSData", 0x1086 },
    { "RSToolsNS_RSDataNS_AudioLineZoneSphereRSData", 0x1087 }, { "RSToolsNS_RSDataNS_AudioVolumeLevelMixRSData", 0x1088 },
    { "RSToolsNS_RSDataNS_FreeRideActivityAudioManagerRSData", 0x1089 }, { "RSToolsNS_RSDataNS_FreestyleActivityAudioManagerRSData", 0x108a },
    { "RSToolsNS_RSDataNS_RaceActivityAudioManagerRSData", 0x108b }, { "RSToolsNS_RSDataNS_SectionRaceActivityAudioManagerRSData", 0x108c },
    { "RSToolsNS_RSDataNS_WaypointRaceActivityAudioManagerRSData", 0x108d }, { "RSToolsNS_RSDataNS_PokerRaceActivityAudioManagerRSData", 0x108e },
    { "RSToolsNS_RSDataNS_InfectionActivityAudioManagerRSData", 0x108f }, { "RSToolsNS_RSDataNS_EggHuntActivityAudioManagerRSData", 0x1090 },
    { "RSToolsNS_RSDataNS_SnakeActivityAudioManagerRSData", 0x1091 }, { "RSToolsNS_RSDataNS_TagActivityAudioManagerRSData", 0x1092 },
    { "RSToolsNS_RSDataNS_ChallengeActivityAudioManagerRSData", 0x1093 },
    { "RSToolsNS_RSDataNS_LocalizedPackageReferenceEntityRSData", 0x1094 },
};

/* classes other units build: rx_scene_register_class (cameras, FX, audio, riders, ...) */
static struct { uint32_t id; rx_scene_create_fn fn; } g_classes[64];
static int g_nclasses;
void rx_scene_register_class(uint32_t id, rx_scene_create_fn fn) {
    for (int k = 0; k < g_nclasses; k++) if (g_classes[k].id == id) { g_classes[k].fn = fn; return; }
    if (g_nclasses == 64) fatal("class registry full", id);
    g_classes[g_nclasses].id = id; g_classes[g_nclasses].fn = fn; g_nclasses++;
}

/* The entity classes outside the core (core/notes/construction.md section 12): the render, audio, UI, FX and camera classes. The
 * factory leaves their slots empty (the original stores 0 for a namespace it cannot build); a unit that builds one registers it with
 * rx_scene_register_class. */
static int outside_core(uint32_t id) {
    static const uint16_t ids[] = {
        /* render: lights, visibility, fog, flares, screen effects, boards, the on-field crowd (absent from every level; the stands' BillboardCrowd 0x1041 is built), decals, grass, race line */
        0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xd, 0x1c, 0x100a, 0x100e, 0x1034, 0x1035, 0x1036, 0x1037, 0x104c, 0x104e, 0x105f, 0x1060, 0x1062,
        0x1066, 0x1067, 0x1068, 0x1072, 0x1074, 0x1075, 0x1077, 0x107e,
        /* audio */
        0x1d, 0x105c, 0x105d, 0x105e, 0x1083, 0x1084, 0x1085, 0x1086, 0x1087, 0x1088, 0x1089, 0x108a, 0x108b, 0x108c, 0x108d, 0x108e, 0x108f,
        0x1090, 0x1091, 0x1092, 0x1093,
        /* UI */
        0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1008, 0x107f, 0x1080, 0x1094,
        /* FX: 0x102a / 0x102e / 0x102f / 0x1030 are built by scene_fx_entities.c (registered); the list keeps the classes that are
         * absent from every level (0x102b / 0x102c / 0x102d) as empty slots */
        0x102a, 0x102b, 0x102c, 0x102d, 0x102e, 0x102f, 0x1030,
        /* cameras */
        0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006, 0x1007, 0x1009, 0x100b, 0x100c, 0x100d, 0x1045, 0x105b,   /* 0x1046 .. 0x1049: the replay cameras, built (loop_cam_replay.c) */
        0x1069, 0x106a, 0x106b,
    };
    for (unsigned k = 0; k < sizeof ids / sizeof *ids; k++) if (ids[k] == id) return 1;
    return 0;
}

/* FUN_004046a0: allocation and constructor of the class of a type id */
extern void rx_scene_fx_register(void);   /* scene_fx_entities.c: the FX / audio / status-board classes */
rx_addr rx_scene_create_entity(uint32_t id) {
    static int fx_registered;
    if (!fx_registered) { fx_registered = 1; rx_scene_fx_register(); }
    switch (id) {
    case 3: case 4: return rx_model_entity_create();
    case 0x1070: return rx_stepup_upright_create();
    case 0x1073: return rx_ai_avoid_sphere_create();
    case 0x5: return rx_collision_entity_create();
    case 0x1050: return rx_anim_single_loop_create();
    case 0x1051: return rx_instanced_anim_create();
    case 0x1052: return rx_anim_spline_loop_create();
    case 0x1057: return rx_land_animal_create();
    case 0x1058: return rx_anim_single_loop_particles_create();
    case 0x1059: return rx_anim_spline_particles_create();
    case 0x104f: return rx_scenario_anim_create();
    case 0x11: return rx_hfbump_create();
    case 0x104a: return rx_massive_ad_create();
    case 0x1028: case 0x1071: return rx_trigger_create();
    case 0x1043: return rx_waypoint_gate_create();
    case 0x1042: return rx_section_zone_create();   /* SectionRaceZone (race_section_zone.c) */
    case 0x107b: return rx_timed_section_create();  /* TimedRaceSection (race_outrun.c) */
    case 0xc: return rx_sky_create();
    case 0x1f: return rx_reflection_map_gen_create();
    case 0x1061: return rx_lens_flare_create();
    case 0xe: return rx_distance_fog_create();
    case 0x1e: return rx_general_storage_create();
    case 0x101a: return rx_anchor_entity_create();
    case 0x1065: return rx_shadow_map_create();
    case 0x1064: return rx_hdr_settings_create();
    case 0x1063: return rx_overlay_color_create();
    case 0x104d: return rx_mud_manager_create();
    case 0x1076: return rx_tdf_entity_create();
    case 0x107a: return rx_level_info_create();
    case 0x107c: return rx_music_track_list_create();
    case 0x10: return rx_hfterrain_entity_create();
    case 0x1029: case 0x106f: return rx_physics_entity_create();
    case 0x1031: return rx_popup_bale_create();
    case 0x1032: return rx_popdown_bale_create();
    case 0x1033: return rx_drop_gate_create();
    case 0x100f: return rx_forest_system_create();
    case 0x104b: return rx_camera_path_create();
    case 0x1046: return rx_replay_tower_camera_create();       /* MXReplayTowerCamera: the TV director's cameras (loop_cam_replay.c) */
    case 0x1047: return rx_zipline_camera_create();            /* MXZipLineCamera */
    case 0x1048: return rx_spline_camera_create();             /* MXSplineCamera */
    case 0x1049: return rx_animated_replay_camera_create();    /* MXAnimatedReplayCamera */
    case 0x103b: return rx_crew_chief_create();
    case 0x1038: return rx_trackside_flag_holder_create();
    case 0x1039: return rx_scoretower_flag_holder_create();
    case 0x103a: return rx_thirty_second_board_create();
    case 0x1019: return rx_challenge_specs_create();
    case 0x1078: return rx_scrolling_texture_create();
    case 0x1079: return rx_camera_facing_create();
    case 0xf: return rx_water_entity_create();
    case 0x1041: return rx_billboard_crowd_create();   /* BillboardCrowdEntity: the stands crowd sections (build_scene_crowd.c) */
    case 0x1040: return rx_graffiti_point_create();    /* GraffitiCapturePoint: the Graffiti party mode's capture points (race_graffiti_point.c) */
    default: { for (int k = 0; k < g_nclasses; k++) if (g_classes[k].id == id) return g_classes[k].fn();
               rx_addr st = rx_start_entity_create(id); if (st) return st; st = rx_collectable_create(id); if (st) return st; break; }
    }
    if (outside_core(id)) return 0;
    if (getenv("RX_SCENE_ALLOW_UNBUILT")) {   /* development: report the classes still missing, leave their slots empty */
        static uint32_t seen[512]; static int nseen;
        for (int k = 0; k < nseen; k++) if (seen[k] == id) return 0;
        if (nseen < 512) seen[nseen++] = id;
        fprintf(stderr, "rxcore: scene: entity type %#x is not built yet\n", id);
        return 0;
    }
    fprintf(stderr, "rxcore: scene: entity type %#x is not built yet\n", id);
    abort();
}

RX_FUNCTION(0x004046a0, rx_scene_create_entity)

/* FUN_0080de70: the id of an RSData namespace (the table at 0x009c5ad8 / 0x009c5da8, compared case-insensitively) */
uint32_t rx_scene_type_id(const char* ns) {
    for (unsigned k = 0; k < sizeof g_types / sizeof *g_types; k++) {
        const char* a = ns, * b = g_types[k].ns;
        while (*a && *b && (*a | 0x20) == (*b | 0x20)) { a++; b++; }
        if (!*a && !*b) return g_types[k].id;
    }
    return 0;
}

/* FUN_0080df90 */
rx_addr rx_scene_entity_from_rsdata(rx_addr scene, const struct fmt_entity* rs, int is_ui_node) {
    if (!rs->ns || rs->rsdata < 0) return 0;
    uint32_t id = rx_scene_type_id(rs->ns);
    g_rs = rs;   /* [0x00a9583c] = the reader */
    rx_ms_scene_class(id);   /* MAP SCALE (port): which entity class rs_matrix reads for */
#ifndef RX_ORACLE
    if (id >= 0x1089 && id <= 0x1093) rx_activity_cues_capture(scene, rs);   /* w-sfx: the activity audio manager's cues (audio, outside the core) */
#endif
    rx_addr e = rx_scene_create_entity(id);
    if (!e) return 0;
    SU32(e, 0x7c, is_ui_node ? 1 : 0);
    SU32(e, 0x10, scene);
    SU32(e, 0x14, rs->id);
    uint32_t reader = 0;
    scene_vcall(e, is_ui_node ? 0x10 : 0x14, 1, &reader);   /* vf4 (UI node) / vf5 (the reader's RSData) */
    return e;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the subscene loader */

/* FUN_007d1dc0 .. 007d1ef0 .. 007d1fb0: the subscene record (scene + 0x38 + k * 0x110, 24 records: name +0, array +0x104, count
 * +0x108, used +0x10c), every entity of the subscene constructed in file order, vf7 of each, then each one added to the scene */
static rx_addr load_subscene(rx_addr scene, struct fmt_scene* fs, int subscene, const char* name);
rx_addr rx_scene_load_subscene(rx_addr scene, struct fmt_scene* fs, int subscene, const char* name) {
    /* w-rng: the original builds a subscene's entities on its level loader thread (the scene's loader job), so the CRT rand() draws of
     * their RSData readers (AnimEntitySingleAnimLoop::vf5 006b9b90, AnimEntitySplineAnimLoop::vf5 006ba690, LandAnimalProximity::vf5
     * 006cfd80 ...) come from that thread's seed, not the main thread's (core/oracle/rng_probe.cpp RX_RNG_LOG: thread 1092f050 in
     * the PX_Laguna boot; crt_thread.c) */
    const int thread = crt_thread_switch(CRT_THREAD_LOADER);
    const int ms_was = rx_ms_level_begin(name);   /* MAP SCALE (port): the level's subscenes load scaled (map_scale.h) */
    const rx_addr arr = load_subscene(scene, fs, subscene, name);
    rx_ms_level_end(ms_was);
    crt_thread_switch(thread);
    return arr;
}
static rx_addr load_subscene(rx_addr scene, struct fmt_scene* fs, int subscene, const char* name) {
    int ne = 0;
    const struct fmt_entity* es = fmt_scene_entities(fs, subscene, &ne);
    if (!es) fatal("subscene without entities", 0);
    uint32_t k;
    for (k = 0; k < 0x18; k++) if (U32(scene, 0x38 + 0x110 * k + 0x10c) == 0) break;
    if (k == 0x18) k = 0;
    rx_addr rec = scene + 0x38 + 0x110 * k;
    uint32_t nl = (uint32_t)strlen(name);
    for (uint32_t i = 0; i < 0x104 && i <= nl; i++) SU8(rec, i, (uint8_t)name[i]);   /* snprintf(rec, 0x104, "%s", name) */
    SU32(rec, 0x10c, 1);
    uint32_t count = (uint32_t)ne;
    SU32(rec, 0x108, count);
    rx_addr arr = count ? rx_reflex_aligned_alloc(rx_mul_sat(count, 4)) : 0;
    SU32(rec, 0x104, arr);
    uint32_t built = 0;
    for (int i = 0; i < ne; i++) {
        rx_addr e = rx_scene_entity_from_rsdata(scene, &es[i], 0);
        if (getenv("RX_SCENE_TRACE")) fprintf(stderr, "entity %d %s -> %08x\n", i, es[i].ns, e);   /* development */
        /* PRODUCT BOUNDARY: an entity class outside the core (the dev cameras, the status board HUD billboard, the ambient directional light: render / UI
         * objects nothing in the simulation queries) has no object here. The original keeps its object in the group array; the product keeps no null slot
         * (the scene's group queries (007d2820 / 007d2920 / 007d25c0 ...) walk the array without a null test), so the group's count is smaller. */
        if (!e) continue;
        SU32(arr, 4 * built, e);
        built++;
    }
    SU32(rec, 0x108, built);
    for (uint32_t i = 0; i < built; i++) if (U32(arr, 4 * i)) vcall0(U32(arr, 4 * i), 0x1c);   /* vf7 */
    for (uint32_t i = 0; i < built; i++) if (U32(arr, 4 * i)) rx_scene_add_entity(scene, U32(arr, 4 * i));
    return arr;
}
