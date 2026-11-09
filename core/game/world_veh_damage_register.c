/* world_veh_damage_register.c -- the registration of a four-wheel vehicle's detachable body parts in the damage manager (the part of the
 * vehicle spawn that makes the trophy truck's, the buggy's and the UTV's hoods, roofs, doors and fenders breakable).
 *
 * The spawn calls 0047bb90 with the vehicle (EAX) and the damage manager (ESI = 0xdb1410: 12 entries of 0x410 bytes from +0x10); the
 * vehicle's class (+0x194: 3 trophy truck, 5 buggy, 7 UTV) selects the part tables:
 *     class   part names (0x350 bytes each)      count   descriptions (std::string, 0x1c each)
 *     3       0x9c6440 "OBJ_Hood" ...             8       0xad6530
 *     5       0x9c92a0                            4       0xad6610
 *     7       0x9c7ec0                            6       0xad3f08
 * Entry E: +0 count, +4 names, +8 descriptions, +0xc + 4i state, +0x2c + 4i the part entity, +0x4c + 8i the part's collision shape handle
 * (vtable 0x9765a8, the shape at +4), +0x90 + 0x10 i the part's offset, +0x130 / +0x1b0 + 0x10 i the rest / current orientation,
 * +0x370 + 4i the part's node in the vehicle model, +0x390 + 4i its parent node, +0x3b0 + 4i the effect id, +0x3fc the vehicle.
 *
 *   0047bb90  rx_damage_register_vehicle   the first free entry of the manager takes the vehicle
 *   00479a00  rx_damage_bind_vehicle       the tables of the class set, the parts made (00479ce0), their nodes found (0047ae00), the states reset
 *   00479ce0  rx_damage_split_parts        for each part still without an entity: its shape from the description (00479680), the entity (00479ef0)
 *   00479680  rx_damage_parse_part_shape   a part description "x y z N  {px py pz  hx hy hz} * N  name" -> a CollisionShapeDynamicComposite of
 *                                          N dynamic boxes (centres px py pz, half extents hx hy hz) and the part's offset (x y z)
 *   0047ae00  rx_damage_find_part_nodes
 *   0047a460  rx_damage_move_part_bodies   the part bodies re-made in the active scene's physics world (after the vehicle moved: 0047bc20)
 *   0047bc20  rx_damage_move_vehicle_parts the vehicle's entries' part bodies moved (Game_EnterRealWorldAudio / MXGame_SwitchToUIWorld)
 * The entry's release (the part entities deleted: 00479dd0 / 0047bbe0) is in build_activity_clear.c, the PhysicsEntity destructor in
 * world_veh_entity.c.
 * Hand transliterations of the disassembly. The std::istringstream the original parses the description with (msvcp90) is not run: its
 * tokens are read here; its heap objects (two 4-byte locales, a 24-byte state block and the copy of the text, freed in reverse order) are
 * allocated and freed as it does, so that every later allocation lands where the original's does. */
#include "world_util.h"
#include "rider_cap.h"
#include "world_veh_damage.h"
#include "build_asset.h"
#include "build_rider.h"
#include "build_model.h"
#include "world_veh.h"
#include "world_node.h"
#include "world.h"
#include "build_activity_util.h"
#include "build_alloc.h"
#include "build_world.h"
#include "build_mxgame.h"
#include "phys_shape_dtors.h"
#include "replay_damage.h"
#include "anim.h"
#include "bike_sim.h"
#include "crt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the text stream ----------------------------------------------------------------------------------------------------------- */

/* The word the 4-byte std::locale objects of the original's streams hold: the process-wide locale implementation (msvcp90's). The product
 * has no msvcp90, so there the objects (freed again at once) hold 0. */
static uint32_t locale_word(void) {
#ifdef RX_ORACLE
    return U32(0, 0x78507370);
#else
    return 0;
#endif
}

typedef struct { rx_addr loc1, state, loc2, buf; const char* text; uint32_t len, pos; char copy[512]; } text_stream;

/* basic_istringstream(string, in) of msvcp90: ios_base's locale (4 bytes), a 24-byte block, the stream buffer's locale (4 bytes) and the
 * copy of the text (its size; none for an empty text), in this order (freed buffer, locale, block, locale) */
static void stream_open(text_stream* ss, rx_addr str) {
    const uint32_t len = U32(str, 0x14);
    const rx_addr chars = U32(str, 0x18) >= 0x10 ? U32(str, 4) : str + 4;
    ss->loc1 = crt_operator_new(4); SU32(ss->loc1, 0, locale_word());
    ss->state = crt_operator_new(24);
    SU32(ss->state, 0, 0); SU32(ss->state, 4, 0xffffffffu); SU32(ss->state, 8, 0); SU32(ss->state, 12, 0); SU32(ss->state, 16, 0); SU32(ss->state, 20, 0);
    ss->loc2 = crt_operator_new(4); SU32(ss->loc2, 0, locale_word());
    ss->buf = 0;
    if (len != 0) { ss->buf = crt_operator_new(len); crt_memcpy(ss->buf, chars, len); }
    if (len >= sizeof ss->copy) { fprintf(stderr, "rxcore: damage part description of %u characters\n", len); abort(); }
    for (uint32_t i = 0; i < len; i++) ss->copy[i] = (char)U8(chars, i);
    ss->copy[len] = 0;
    ss->text = ss->copy; ss->len = len; ss->pos = 0;
}
static void stream_close(text_stream* ss) {
    if (ss->buf) crt_operator_delete(ss->buf);
    crt_operator_delete(ss->loc2);
    crt_operator_delete(ss->state);
    crt_operator_delete(ss->loc1);
}
/* istream >> float: the next blank separated token (the descriptions hold plain decimals) */
static float stream_float(text_stream* ss) {
    while (ss->pos < ss->len && (ss->text[ss->pos] == ' ' || (ss->text[ss->pos] >= 9 && ss->text[ss->pos] <= 13))) ss->pos++;
    char* end;
    const double v = strtod(ss->text + ss->pos, &end);
    ss->pos = (uint32_t)(end - ss->text);
    return (float)v;
}
/* istream >> int */
static int32_t stream_int(text_stream* ss) {
    while (ss->pos < ss->len && (ss->text[ss->pos] == ' ' || (ss->text[ss->pos] >= 9 && ss->text[ss->pos] <= 13))) ss->pos++;
    char* end;
    const long v = strtol(ss->text + ss->pos, &end, 10);
    ss->pos = (uint32_t)(end - ss->text);
    return (int32_t)v;
}

/* ---- the composite shape and its vectors ---------------------------------------------------------------------------------------- */

/* 008494a0 (ECX = vector of 16-byte elements, stack n; RET 4) on an empty vector: _aligned_malloc(16 n, 16) */
static void vec16_reserve(rx_addr vec, uint32_t n) { rx_vec_reserve_empty(vec, n, 0x10, 0); }

/* 00847c80 (stack shape, n; RET 8): CollisionShapeComposite (type 0x8000, vtable 0x976774): the child pointer vector at +0x20 (proxy,
 * +0x2c .. +0x34), three vectors of 16-byte elements at +0x38 / +0x50 / +0x68 (child rotations, positions, the third unused here), each
 * with room for n elements */
rx_addr rx_shape_composite_construct(rx_addr s, uint32_t n) {
    ACT_CALL(0, rx_collision_shape_construct(s, 0x8000));
    SU32(s, 0, 0x00976774);
    {   /* the child vector's proxy: ReflexAlignedAlloc(4) */
        rx_addr p;
        ACT_CALLR(p, 1, rx_reflex_aligned_alloc(4));
        if (p) SU32(p, 0, s + 0x20);
        SU32(s, 0x20, p);
    }
    SU32(s, 0x2c, 0); SU32(s, 0x30, 0); SU32(s, 0x34, 0);
    ACT_CALL(1, rx_vec_proxy_init(s + 0x38)); SU32(s, 0x44, 0); SU32(s, 0x48, 0); SU32(s, 0x4c, 0);
    ACT_CALL(1, rx_vec_proxy_init(s + 0x50)); SU32(s, 0x5c, 0); SU32(s, 0x60, 0); SU32(s, 0x64, 0);
    ACT_CALL(1, rx_vec_proxy_init(s + 0x68)); SU32(s, 0x74, 0); SU32(s, 0x78, 0); SU32(s, 0x7c, 0);
    ACT_CALL(1, rx_vecp_reserve_ecx(s + 0x20, n));
    ACT_CALL(1, vec16_reserve(s + 0x50, n));
    ACT_CALL(1, vec16_reserve(s + 0x38, n));
    ACT_CALL(1, vec16_reserve(s + 0x68, n));
    return s;
}

/* 00849080 (EDI = shape, stack n; RET 4): CollisionShapeDynamicComposite (vtable 0x9767cc): the composite, then the vector of the boxes'
 * names (std::string, 0x1c bytes) at +0x80 with room for n */
rx_addr rx_shape_dynamic_composite_construct(rx_addr s, uint32_t n) {
    ACT_CALL(2, rx_shape_composite_construct(s, n));
    SU32(s, 0, 0x009767cc);
    {
        rx_addr p;
        ACT_CALLR(p, 1, rx_reflex_aligned_alloc(4));
        if (p) SU32(p, 0, s + 0x80);
        SU32(s, 0x80, p);
    }
    SU32(s, 0x8c, 0); SU32(s, 0x90, 0); SU32(s, 0x94, 0);
    ACT_CALL(2, rx_vec_reserve_empty(s + 0x80, n, 0x1c, 1));       /* 008496d0: ReflexAlignedAlloc(0x1c n) */
    return s;
}

/* the 16-byte element appended inside the reserved capacity (008495b0 with the element at a pointer; 00849420 appends the identity
 * quaternion 0xd61640); a vector that is full would reallocate (00849b00 / 00849d20): not reached, every vector was reserved for n elements */
static void vec16_push(rx_addr vec, rx_addr src) {
    const rx_addr first = U32(vec, 0xc), last = U32(vec, 0x10);
    const uint32_t cap = first ? (U32(vec, 0x14) - first) >> 4 : 0;
    if (((last - first) >> 4) >= cap) act_unreached(0x00849b00, "a collision composite's vector full (reserved for every box)");
    for (uint32_t k = 0; k < 16; k += 4) SU32(last, k, U32(src, k));
    SU32(vec, 0x10, last + 0x10);
}
/* 0042ae70 (stack vector, string; RET 8): push_back of a std::string inside the capacity (0042bf90 copy constructs it) */
static void names_push(rx_addr vec, rx_addr str) {
    const rx_addr first = U32(vec, 0xc), last = U32(vec, 0x10);
    const uint32_t cap = first ? (U32(vec, 0x14) - first) / 0x1c : 0;
    if ((last - first) / 0x1c >= cap) act_unreached(0x0042b1c0, "a composite's name vector full (reserved for every box)");
    crt_str_ctor_copy(last, str);
    SU32(vec, 0x10, last + 0x1c);
}

/* ---- 00479680 -------------------------------------------------------------------------------------------------------------------- */

/* 00479680 (ECX = result handle, EDX = the offset vector, stack description std::string; the caller pops the argument) -> EAX = the handle
 * {vtable 0x9765a8, +4 the composite (one reference more)}. The 16-byte vectors the original copies from its frame have a fourth lane the
 * parse never writes: the frame's stale word is taken. Frame: push ebp; and esp, -16; sub esp, 0x114; push ebx / esi / edi; the
 * floats are read to +0x20 (offset) / +0x40 (centre) / +0x30 (half extents), the count to +0x10, "Box%d" is formatted to +0xfc and
 * the temporary name string is at +0x54. */
rx_addr rx_damage_parse_part_shape(rx_addr out, rx_addr offset_out, rx_addr desc) {
    world_frame f = world_frame_enter(0x114, 3, 1);
    const rx_addr S = f.esp;
    text_stream ss;
    stream_open(&ss, desc);
    SF32(S, 0x20, stream_float(&ss)); SF32(S, 0x24, stream_float(&ss)); SF32(S, 0x28, stream_float(&ss));
    for (uint32_t k = 0; k < 16; k += 4) SU32(offset_out, k, U32(S, 0x20 + k));
    const int32_t n = stream_int(&ss);
    SU32(S, 0x10, (uint32_t)n);
    rx_addr comp;
    ACT_CALLR(comp, 1, rx_reflex_aligned_alloc(0x98));
    if (comp) ACT_CALLR(comp, 1, rx_shape_dynamic_composite_construct(comp, (uint32_t)n));
    SU32(S, 0xc, comp);
    for (int32_t i = 0; i < n; i++) {
        SU32(S, 0x18, (uint32_t)i);
        {   /* 0078fd60 (ESI = 0x20, EDI = the buffer): "Box%d" */
            const rx_addr sp = rx_sp();
            SU32(sp, -4, (uint32_t)i); SU32(sp, -8, 0x0093c3ac); rx_set_sp(sp - 12);
            rx_format_buffer(S + 0xfc, 0x20, 0x0093c3ac, sp - 4);
            rx_set_sp(sp);
        }
        SF32(S, 0x40, stream_float(&ss)); SF32(S, 0x44, stream_float(&ss)); SF32(S, 0x48, stream_float(&ss));   /* the box's centre */
        SF32(S, 0x30, stream_float(&ss)); SF32(S, 0x34, stream_float(&ss)); SF32(S, 0x38, stream_float(&ss));   /* its half extents */
        rx_addr box = rx_crt_aligned_malloc(0x30, 0x10);
        if (box) {
            ACT_CALL(0, rx_collision_shape_construct(box, 4));
            SU32(box, 0, 0x00976698);                                         /* Box */
            for (uint32_t k = 0; k < 16; k += 4) SU32(box, 0x20 + k, U32(S, 0x30 + k));
            SU32(box, 0, 0x009766e0);                                         /* DynamicBox */
        }
        SU32(S, 0x14, box);
        ACT_CALL(1, rx_vec_push_back_ptr(comp + 0x20, S + 0x14));             /* 00849660: the child */
        vec16_push(comp + 0x50, S + 0x40);                                    /* 008495b0: its centre */
        vec16_push(comp + 0x38, 0x00d61640);                                  /* 00849420: the identity rotation */
        {   /* the name: basic_string(buffer), appended, destroyed */
            const rx_addr tmp = S + 0x54;
            crt_str_ctor_cstr(tmp, S + 0xfc);
            names_push(comp + 0x80, tmp);
            crt_str_dtor(tmp);
        }
    }
    SU32(out, 0, 0x009765b4);
    SU32(out, 4, comp);
    SU32(comp, 4, U32(comp, 4) + 1);
    SU32(out, 0, 0x009765a8);
    stream_close(&ss);
    world_frame_leave(&f);
    return out;
}
RX_FUNCTION(0x00479680, rx_damage_parse_part_shape)

/* ---- the part entity ------------------------------------------------------------------------------------------------------------ */

/* defined in build_scene_props.c / replay_damage.c / build_activity_clear.c without a header */
void    rx_damage_entry_release(rx_addr entry);                    /* 00479dd0 (ECX = entry) */
rx_addr rx_physics_entity_construct(rx_addr e);                    /* 004036d0 (ESI = entity) */
rx_addr rx_phys_prop_record_init(rx_addr r, rx_addr e);            /* 0084f4a0 (EDI = record; stack entity) */
void    rx_phys_keyed_push(rx_addr vec, rx_addr rec);              /* 007738e0 (EAX = the keyed objects' vector; stack record) */
void    rx_replay_physics_entity_deactivate(rx_addr entity);       /* 0041ce60 (ESI = entity) */

/* 007697e0 (ECX = entity, stack world, out, shape handle, soft flag byte; RET 0x10): the entity's rigid body from an existing shape
 * handle (00769670 builds the shape from the entity's two collision assets instead): the record built on the frame (0084f4a0), the tracking
 * entry {0, world, index} (ReflexAlignedAlloc(12)), the handle's shape taken for the record (the shape the record came with released),
 * the soft collision flag (shape +0x10 = 4 when asked) and the shape's bit 0x10, the record pushed into the world's keyed objects (+0x1994,
 * 007738e0); out = a PhysicsObjectHandle {vtable 0x937504, +4 the tracking entry (one reference more)}. Frame: push ebp; and esp, -16;
 * sub esp, 0x104; push ebx / esi / edi; the record at +0x20 (its +0x2c the shape, +0x30 the tracking entry). */
rx_addr rx_phys_object_create_from_handle(rx_addr entity, rx_addr world, rx_addr out, rx_addr handle, uint32_t soft) {
    world_frame f = world_frame_enter(0x104, 3, 1);
    const rx_addr rec = f.esp + 0x20;
    const rx_addr vec = world + 0x1994;
    const uint32_t index = (U32(world, 0x19a4) - U32(world, 0x19a0)) / 0xe0u;
    for (uint32_t k = 0; k < 0xe0; k += 4) SU32(rec, k, 0);   /* (the words the init never writes are zero in the original's frame) */
    ACT_CALL(1, rx_phys_prop_record_init(rec, entity));
    rx_addr t;
    ACT_CALLR(t, 1, rx_reflex_aligned_alloc(0xc));
    if (t) { SU32(t, 0, 0); SU32(t, 4, world); SU32(t, 8, index); }
    SU32(rec, 0x30, t);
    if (U32(handle, 4) != U32(rec, 0x2c)) {
        rx_collision_shape_release(U32(rec, 0x2c));
        SU32(rec, 0x2c, U32(handle, 4));
        if (U32(rec, 0x2c)) SU32(U32(rec, 0x2c), 4, U32(U32(rec, 0x2c), 4) + 1);
    }
    if (soft & 0xff) SU32(U32(rec, 0x2c), 0x10, 4);
    SU32(U32(rec, 0x2c), 0x10, U32(U32(rec, 0x2c), 0x10) | 0x10);
    ACT_CALL(1, rx_phys_keyed_push(vec, rec));
    SU32(out, 0, 0x00937510);
    SU32(out, 4, U32(rec, 0x30));
    SU32(U32(rec, 0x30), 0, U32(U32(rec, 0x30), 0) + 1);
    SU32(out, 0, 0x00937504);
    rx_collision_shape_release(U32(rec, 0x2c));
    world_frame_leave(&f);
    return out;
}
RX_FUNCTION(0x007697e0, rx_phys_object_create_from_handle)

/* 0041cd90 (ECX = the part's shape handle, stack entity, world; RET 8): the entity gets a physics body of that shape: 007697e0, the entity's
 * tracking entry (+0x158) taken (the one it had released: it has none yet), the temporary's reference dropped (the entity holds the entry),
 * then the body is deactivated (the tail of 0041ce60) with its disable flag +0xb0 cleared. Frame: sub esp, 0x10; push ebx / esi / edi. */
void rx_physics_entity_attach_body(rx_addr handle, rx_addr entity, rx_addr world) {
    world_frame f = world_frame_flat(3, 0x10);
    const rx_addr out = f.entry - 0xc;                                      /* the temporary handle: [esp+0x10] after the three pushes + 4 */
    WORLD_CALL_SP(16, rx_phys_object_create_from_handle(entity, world, out, handle, U8(entity, 0x15d)));
    const rx_addr t = U32(out, 4);
    const rx_addr old = U32(entity, 0x158);
    if (t != old) {
        if (old) {
            SU32(old, 0, U32(old, 0) - 1);
            if (U32(old, 0) == 0) act_unreached(0x00417680, "a part entity's previous tracking entry freed");
        }
        SU32(entity, 0x158, t);
        if (t) SU32(t, 0, U32(t, 0) + 1);
    }
    SU32(out, 0, 0x00937510);
    if (t) {
        SU32(t, 0, U32(t, 0) - 1);
        if (U32(t, 0) == 0) {   /* (not reached: the entity holds the entry) its body leaves the world, the entry is freed */
            WORLD_PUSHED(0x0041ce15, U32(t, 4));
            WORLD_CALL_SP(4, rx_world_keyed_body_remove(U32(t, 4), U32(t, 8)));
            WORLD_PUSHED(0x0041ce1b, t);
            WORLD_CALL_SP(4, rx_reflex_aligned_free(t));
        }
    }
    if (U32(entity, 0x158) != 0) ACT_CALL(0, rx_replay_physics_entity_deactivate(entity));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0041cd90, rx_physics_entity_attach_body)

/* 0041cb60 (ESI = entity, ECX = the model's asset handle, stack physics type, name pointer; RET 8): the entity set up as a "model only"
 * physics entity: no collision assets (+0x140 / +0x144), the physics type (+0x150), audio material 0x23 (+0x148) and hardness 5 (+0x14c), the
 * model attached (0041c700: no scene yet, flags 2), the entity's flag word +0xb0 with bit 4 (0x10) cleared and bit 3 (8) set */
void rx_vehicle_damage_entity_setup(rx_addr entity, rx_addr handle, uint32_t phys_type, rx_addr name) {
    SU32(entity, 0x144, 0); SU32(entity, 0x140, 0);
    SU32(entity, 0x150, phys_type);
    SU32(entity, 0x148, 0x23); SU32(entity, 0x14c, 5);
    WORLD_CALL_SP(12, rx_entity_attach_model(0, name, entity, handle, 2));
    SU16(entity, 0xb0, (uint16_t)((U16(entity, 0xb0) & 0xffefu) | 8u));
}
RX_FUNCTION(0x0041cb60, rx_vehicle_damage_entity_setup)

/* the part's name for the database: "%x_%x_%s" (a running number, the vehicle, the part's second node name) */
static void part_asset_name(rx_addr buf, uint32_t number, rx_addr vehicle, rx_addr node_name) {
    const rx_addr sp = rx_sp();
    SU32(sp, -4, node_name); SU32(sp, -8, vehicle); SU32(sp, -12, number); SU32(sp, -16, 0x0093c3f0);
    rx_set_sp(sp - 20);
    rx_format_buffer(buf, 0x40, 0x0093c3f0, sp - 12);
    rx_set_sp(sp);
}

/* 00479ef0 (ECX = entry E, stack part index; RET 4): the part's physics entity (0x180 bytes, vtables 0x93ccac / 0x93cd34) and its model: a
 * new ModelAsset (0x140 bytes) of the vehicle model's subtree under the part's second node (007dda20) with the node's LOD bindings (the
 * surface slots, re-found by node name), registered in the engine's database under "%x_%x_%s" as a "model" asset, shown by an entity of the
 * physics type "LightHollow" at the part's offset; the entity gets a body of the part's collision shape (0041cd90), is not moving yet
 * (record flags +0xb1 / +0xb2 = 0) and is flagged (+0xb0 bit 0). 0 when the entry has no vehicle or the vehicle's model lacks the node.
 * Frame: push ebp; and esp, -16; sub esp, 0xd4; push ebx / esi / edi; the name buffer at +0x90, the identity matrix at +0x50. */
rx_addr rx_damage_split_part(rx_addr e, uint32_t index) {
    world_frame f = world_frame_enter(0xd4, 3, 1);
    const rx_addr S = f.esp;
    const rx_addr vehicle = U32(e, 0x3fc);
    if (vehicle == 0) { world_frame_leave(&f); return 0; }
    const rx_addr src_model = U32(vehicle, 0x1328);
    const rx_addr name2 = U32(e, 4) + index * 0x350u + 0x104u;           /* the part's second node */
    const rx_addr src_hier = src_model + 0x10;
    SU32(S, 0x28, e); SU32(S, 0x24, name2); SU32(S, 0x10, src_model); SU32(S, 0x14, src_hier);
    rx_addr found;
    ACT_CALLR(found, 1, rx_hierarchy_find_node(src_hier, name2));
    if (found == 0) { world_frame_leave(&f); return 0; }
    rx_addr model;
    ACT_CALLR(model, 1, rx_reflex_aligned_alloc(0x140));
    if (model) ACT_CALLR(model, 0, rx_rider_model_asset_construct(model));
    SU32(S, 0xc, model);
    WORLD_CALL_SP(8, rx_nodeset_extract_subtree(src_hier, model + 0x10, name2));
    {   /* the identity matrix into the new hierarchy (004300a0 ModelNodeRef_SetMatrix) */
        for (uint32_t r = 0; r < 4; r++) for (uint32_t c = 0; c < 4; c++) SF32(S, 0x50 + 16 * r + 4 * c, r == c ? F32(0, 0x00931f24) : 0.0f);
        WORLD_CALL_SP(4, bike_model_node_set_matrix(model + 0x10, S + 0x50));
    }
    ACT_CALL(0, rx_nodeset_copy_from(model + 0x50, model + 0x10));
    {   /* the bindings of every LOD of the source model whose node exists in the copy */
        const uint32_t lods = U32(src_model, 0x118);
        for (uint32_t j = 0; j < lods; j++) {
            ACT_CALL(0, rx_model_push_empty_lod(model));
            const rx_addr slod = U32(src_model, 0x130) + 0x1cu * j, dlod = U32(model, 0x130) + 0x1cu * j;
            const uint32_t nb = (U32(slod, 0x14) - U32(slod, 0x10)) >> 4;
            for (uint32_t k = 0; k < nb; k++) {
                const rx_addr b = U32(slod, 0x10) + 0x10u * k;
                const rx_addr rec = U32(src_hier, 8) + (U32(b, 4) << 4);
                const rx_addr name = U32(U32(rec, 0), 0x10) + U16(rec, 8);
                rx_addr node;
                ACT_CALLR(node, 1, rx_hierarchy_find_node(model + 0x10, name));
                if (node == 0) continue;
                const int32_t idx = (int32_t)(node - U32(model, 0x18)) >> 4;
                if (idx < 0) continue;
                SU32(S, 0x34, U32(b, 0)); SU32(S, 0x38, (uint32_t)idx); SU32(S, 0x3c, U32(b, 8)); SU32(S, 0x40, U32(b, 0xc));
                WORLD_CALL_SP(4, rx_lod_entries_push_back(dlod + 4, S + 0x34));
            }
        }
    }
    ACT_CALL(0, rx_nodeset_copy_from(model + 0x50, model + 0x10));
    {   /* the draw distance: the source's rounded down to a multiple of the scene's step, at most 0xffff */
        const rx_addr ctx = U32(0, 0x00a95844);
        const uint32_t step = U32(U32(ctx, 0x1c + 4 * U32(ctx, 4)), 0x19d0);
        int32_t dist = (int32_t)((U32(src_model, 0x120) / step) * step);
        if (dist >= 0xffff) dist = 0xffff;
        SU32(model, 0x120, (uint32_t)dist);
    }
    uint64_t tag;
    ACT_CALLR(tag, 0, rx_asset_handle_type(model));                      /* the model's vf1: the "model" tag */
    const uint32_t tag_lo = (uint32_t)tag, tag_hi = (uint32_t)(tag >> 32);
    SU32(S, 0xd0, tag_lo); SU32(S, 0xd4, tag_hi);
    {
        const uint32_t number = U32(0, 0x00d92d4c);
        SU32(0, 0x00d92d4c, number + 1);
        part_asset_name(S + 0x90, number, vehicle, name2);
    }
    const rx_addr db = U32(U32(0, 0x00a95844), 8);
    char host_name[0x40];
    { uint32_t n = 0; while (n < 0x3f && U8(S + 0x90, n)) { host_name[n] = (char)U8(S + 0x90, n); n++; } host_name[n] = 0; }
    ACT_CALL(2, rx_database_add_asset(db, host_name, model, tag_lo, tag_hi));          /* vf23 0084aa10 */
    rx_addr handle;
    ACT_CALLR(handle, 4, rx_database_vf_handle(db, S + 0x90, tag_lo, tag_hi, 1));      /* vf27 0084ac70 */
    rx_addr entity = rx_crt_aligned_malloc(0x180, 0x10);
    if (entity) {
        ACT_CALL(0, rx_physics_entity_construct(entity));
        SU32(entity, 0, 0x0093ccac); SU32(entity, 0x90, 0x0093cd34);
        for (uint32_t k = 0; k < 16; k += 4) SU32(entity, 0x170 + k, U32(0, 0x00d61670 + k));
    }
    {
        crt_str_ctor_cstr(S + 0x34, 0x0093c3fc);                         /* "LightHollow" */
        if (!(U32(0, 0x00dce7d4) & 1)) { SU32(0, 0x00dce7d4, U32(0, 0x00dce7d4) | 1); (void)rx_phys_props_get(); }
        const uint32_t phys_type = rx_phys_props_find(RX_PHYS_PROPS, "LightHollow");   /* 00850bf0 */
        ACT_CALL(2, rx_vehicle_damage_entity_setup(entity, handle, phys_type, S + 0x90));
        crt_str_dtor(S + 0x34);
    }
    SU8(entity, 0xa2, 1); SU32(entity, 0x70, 1);                          /* (after vf7 0x1c: the empty shared virtual 007e3be0) */
    for (uint32_t k = 0; k < 16; k += 4) SU32(entity, 0x170 + k, U32(e, 0x90 + 0x10u * index + k));
    {   /* the physics world of the active scene kind, and the body */
        const rx_addr ctx = U32(0, 0x00a95844), game = U32(0, 0x00a946b8);
        rx_addr owner = 0;
        if (U32(U32(ctx, 0x20), 0x34) == 1) owner = U32(game, 0x5a54);
        else if (U32(U32(ctx, 0x1c), 0x34) == 1) owner = U32(game, 0x20c8);
        ACT_CALL(2, rx_physics_entity_attach_body(e + 0x4c + 8 * index, entity, U32(owner, 0x20134)));
    }
    for (uint32_t clear = 0xb1; clear <= 0xb2; clear++) {
        const rx_addr t = U32(entity, 0x158);
        const rx_addr w = U32(t, 4);
        const uint32_t n = (U32(w, 0x19a4) - U32(w, 0x19a0)) / 0xe0u;
        if (U32(t, 8) >= n) act_unreached(0x00916224, "a part entity's body index (_invalid_parameter_noinfo)");
        SU8(U32(w, 0x19a0) + U32(t, 8) * 0xe0u, clear, 0);
    }
    SU16(entity, 0xb0, U16(entity, 0xb0) | 1);
    world_frame_leave(&f);
    return entity;
}
RX_FUNCTION(0x00479ef0, rx_damage_split_part)

/* ---- 00479ce0 / 0047ae00 / 00479a00 / 0047bb90 ------------------------------------------------------------------------------------- */

/* 00479ce0 (ESI = entry E): each part that has no entity yet: its shape handle from the description (00479680, the offset vector at +0x90),
 * the handle stored in the entry (+0x4c + 8i: the previous shape released, the new one referenced; the temporary handle drops its own
 * reference), then its entity (00479ef0). Frame: push ebp; and esp, -8; sub esp, 0x18; push ebx / edi; the temporary handle at +0x18. */
void rx_damage_split_parts(rx_addr e) {
    ACT_FRAME_EBP(F, 8, 0x18, 2);
    const rx_addr tmp = F + 0x18;
    for (uint32_t i = 0; i < U32(e, 0); i++) {
        if (U32(e, 0x2c + 4 * i) != 0) continue;
        const rx_addr h = rx_damage_parse_part_shape(tmp, e + 0x90 + 0x10 * i, U32(e, 8) + 0x1c * i);
        const rx_addr slot = e + 0x50 + 8 * i;
        if (U32(h, 4) != U32(slot, 0)) {
            rx_collision_shape_release(U32(slot, 0));
            SU32(slot, 0, U32(h, 4));
            if (U32(slot, 0)) SU32(U32(slot, 0), 4, U32(U32(slot, 0), 4) + 1);
        }
        SU32(tmp, 0, 0x009765b4);
        rx_collision_shape_release(U32(tmp, 4));
        rx_addr part;
        ACT_CALLR(part, 1, rx_damage_split_part(e, i));
        SU32(e, 0x2c + 4 * i, part);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00479ce0, rx_damage_split_parts)

/* 0047ae00 (stdcall entry E; RET 4): each part's second node found in the vehicle's model (+0x370 + 4i), its parent record (+0x390 + 4i), its
 * rest orientation (the node's local rotation, +0x130 + 0x10 i) copied to the current one (+0x1b0), the effect id 2 (+0x3b0), 3 for a buggy's or
 * UTV's roof and door (the node's name contains "Roof" or "Door"). Frame: sub esp, 8; push ebx / ebp / esi / edi. */
void rx_damage_find_part_nodes(rx_addr e) {
    world_frame f = world_frame_flat(4, 8);
    const rx_addr vehicle = U32(e, 0x3fc);
    if (vehicle != 0) {
        const rx_addr hier = U32(vehicle, 0x1328) + 0x10;
        for (uint32_t i = 0; i < U32(e, 0); i++) {
            rx_addr node;
            ACT_CALLR(node, 1, rx_hierarchy_find_node(hier, U32(e, 4) + 0x350 * i + 0x104));
            SU32(e, 0x370 + 4 * i, node);
            if (node == 0) continue;
            SU32(e, 0x390 + 4 * i, U32(U32(node, 0), 8) + 16u * U8(node, 0xf));
            ACT_CALL(0, rx_node_local_rotation(node, e + 0x130 + 0x10 * i));
            for (uint32_t k = 0; k < 16; k += 4) SU32(e, 0x1b0 + 0x10 * i + k, U32(e, 0x130 + 0x10 * i + k));
            SU32(e, 0x3b0 + 4 * i, 2);
            const rx_addr name = U32(U32(node, 0), 0x10) + U16(node, 8);
            const uint32_t cls = U32(vehicle, 0x194);
            if (cls == 5 || cls == 7) {
                if (crt_strstr(name, 0x0093c408 /* "Roof" */) != 0 || crt_strstr(name, 0x0093c410 /* "Door" */) != 0) SU32(e, 0x3b0 + 4 * i, 3);
            }
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0047ae00, rx_damage_find_part_nodes)

/* 00479a00 (ECX = entry E, EAX = the vehicle): the entry takes the vehicle (+0x3fc); none releases the entry (00479dd0); otherwise the class's
 * tables (see the file heading), the parts made, their nodes found and their states reset (0047a880). */
void rx_damage_bind_vehicle(rx_addr e, rx_addr vehicle) {
    ACT_FRAME(F, 0, 1);                                                     /* push esi */
    SU32(e, 0x3fc, vehicle);
    if (vehicle == 0) { rx_set_sp(F_sp); ACT_CALL(0, rx_damage_entry_release(e)); return; }
    switch (U32(vehicle, 0x194)) {
    case 3: SU32(e, 4, 0x009c6440); SU32(e, 0, 8); SU32(e, 8, 0x00ad6530); break;
    case 5: SU32(e, 4, 0x009c92a0); SU32(e, 0, 4); SU32(e, 8, 0x00ad6610); break;
    case 7: SU32(e, 4, 0x009c7ec0); SU32(e, 0, 6); SU32(e, 8, 0x00ad3f08); break;
    default: break;
    }
    ACT_CALL(0, rx_damage_split_parts(e));
    ACT_CALL(1, rx_damage_find_part_nodes(e));
    ACT_CALL(1, rx_world_damage_reset_parts(e));
    ACT_END(F);
}
RX_FUNCTION(0x00479a00, rx_damage_bind_vehicle)

/* 0047bb90 (ESI = the damage manager, EAX = the vehicle): a trophy truck, buggy or UTV (class 3, 5, 7) takes the first free entry of the 12 */
void rx_damage_register_vehicle(rx_addr manager, rx_addr vehicle) {
    ACT_FRAME(F, 0, 1);                                                     /* push ecx */
    const uint32_t cls = U32(vehicle, 0x194);
    if (cls != 3 && cls != 5 && cls != 7) { ACT_END(F); return; }
    for (uint32_t i = 0; i < (rx_rc_more() ? RX_RC_MAX : 12u); i++) {   /* (w-rc-core) 12, then (13+ players) entries 12..31 (rider_cap.h), constructed on first use */
        if (i >= RX_RC_ORIG && i >= rx_dmg_n()) { ACT_CALL(0, (void)rx_damage_behavior_construct(rx_dmg_entry(manager, i))); rx_rc_tab_mark(0x00db1420u, i); }
        if (U32(rx_dmg_entry(manager, i), 0x3fc) != 0) continue;
        ACT_CALL(0, rx_damage_bind_vehicle(rx_dmg_entry(manager, i), vehicle));
        break;
    }
    ACT_END(F);
}
RX_FUNCTION(0x0047bb90, rx_damage_register_vehicle)

/* ---- 0047a460 / 0047bc20: the part bodies follow their vehicle into another world (w-partdtor) ------------------------------------------- */

/* 0047a460 (stdcall entry E; RET 4): each part entity's body moved into the physics world of the active scene: the old body taken out of
 * its world's active kinematic list (Kinematic_Deactivate 0076a8b0), the body handle (+0x154) released through its vf1 (00416f90: the last
 * reference removes the record from the old world and frees the tracking entry), a new body of the part's shape in the active scene's
 * world (0041cd90: the event world game +0x5a54 when the event scene [ctx +0x20] is active, else the UI world +0x20c8 when the UI scene
 * [ctx +0x1c] is), the entity's body-centre offset (+0x170) from the part's offset (+0x90 + 0x10 i) and the record not moving (+0xb1 /
 * +0xb2 = 0). Frame: push ebp; and esp, -16; sub esp, 0x34; push ebx / esi / edi: [esp + 0x3c] the entry, +0x38 the part index, +0x18 /
 * +0x1c / +0x20 the cursors of the shape handles / entities / offsets. */
void rx_damage_move_part_bodies(rx_addr e) {
    world_frame f = world_frame_enter(0x34, 3, 1);
    const rx_addr S = f.esp;
    SU32(S, 0x3c, e);
    SU32(S, 0x38, 0);
    if (U32(e, 0) != 0) {
        SU32(S, 0x20, e + 0x90); SU32(S, 0x18, e + 0x4c); SU32(S, 0x1c, e + 0x2c);
        for (uint32_t i = 0; i < U32(e, 0); i++) {
            const rx_addr ent = U32(e, 0x2c + 4 * i);
            if (ent != 0) {
                const rx_addr t = U32(ent, 0x158);
                if (t != 0) {
                    const rx_addr w = U32(t, 4);
                    const uint32_t idx = U32(t, 8);
                    if (idx >= (uint32_t)((I32(w, 0x19a4) - I32(w, 0x19a0)) / 0xe0)) crt_invalid_parameter_noinfo();
                    const rx_addr rec = idx * 0xe0u + U32(w + 0x1994, 0xc);
                    WORLD_PUSHED(0x0047a503, U32(U32(ent, 0x158), 4));
                    WORLD_CALL_SP(4, rx_world_kinematic_deactivate(rec, U32(U32(ent, 0x158), 4)));
                }
                act_vcall(ent + 0x154, 4, 0, 0);                                        /* 00416f90 */
                const rx_addr ctx = U32(0, 0x00a95844), game = U32(0, 0x00a946b8);
                rx_addr owner = 0;
                if (U32(U32(ctx, 0x20), 0x34) == 1) owner = U32(game, 0x5a54);
                else if (U32(U32(ctx, 0x1c), 0x34) == 1) owner = U32(game, 0x20c8);
                WORLD_PUSHED(0x0047a55b, ent, U32(owner, 0x20134));
                WORLD_CALL_SP(8, rx_physics_entity_attach_body(e + 0x4c + 8 * i, ent, U32(owner, 0x20134)));
                v4_st(ent + 0x170, v4_ld(e + 0x90 + 0x10 * i));
                for (uint32_t clear = 0xb1; clear <= 0xb2; clear++) {
                    const rx_addr tt = U32(ent, 0x158);
                    const rx_addr w = U32(tt, 4);
                    if (U32(tt, 8) >= (uint32_t)((I32(w, 0x19a4) - I32(w, 0x19a0)) / 0xe0)) crt_invalid_parameter_noinfo();
                    SU8(U32(w + 0x1994, 0xc) + U32(tt, 8) * 0xe0u, clear, 0);
                }
            }
            SU32(S, 0x1c, U32(S, 0x1c) + 4); SU32(S, 0x20, U32(S, 0x20) + 0x10); SU32(S, 0x18, U32(S, 0x18) + 8);
            SU32(S, 0x38, i + 1);
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0047a460, rx_damage_move_part_bodies)

/* 0047bc20 (EAX = the damage manager; stdcall the vehicle; RET 4): the vehicle's entries (12 of 0x410 bytes from +0x10, the vehicle at
 * +0x3fc) move their part bodies into the active scene's world (0047a460). Called right after the vehicle itself moved (vf31): the switch
 * from the front end's world to the event's (Game_EnterRealWorldAudio 0075a3f0) and back (MXGame_SwitchToUIWorld 0075ac90). Frame: push
 * ecx / ebx / esi / edi. */
void rx_damage_move_vehicle_parts(rx_addr manager, rx_addr vehicle) {
    world_frame f = world_frame_flat(4, 0);
    for (uint32_t k = 0; k < rx_dmg_n(); k++) {   /* (w-rc-core) + the side entries made */
        const rx_addr entry = rx_dmg_entry(manager, k);
        if (U32(entry, 0x3fc) != vehicle) continue;
        WORLD_PUSHED(0x0047bc40, entry);
        WORLD_CALL_SP(4, rx_damage_move_part_bodies(entry));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0047bc20, rx_damage_move_vehicle_parts)
