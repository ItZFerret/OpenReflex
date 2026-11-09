/* build_scene_props.c -- the physics props of a scene: PhysicsEntity (PhysicsRSData, 0x1029 and StepUpBar 0x106f: a ModelEntity whose
 * rigid body lives in the physics world's keyed objects), PopUpBale / PopDownBale (props that rise / sink when a vehicle exits a
 * linked trigger), DropGate (a CollisionEntity: the start gate), and the event-handler registration those classes (and the flag
 * holders / crew chief / thirty second board) do at load. Hand transliteration from the disassembly; addresses in the comments.
 *
 * PhysicsEntity layout (0x170): ModelEntity (0x150) + +0x140 MaleCollisionAsset handle, +0x144 FemaleCollisionAsset, +0x148 collision
 * audio material (or hardness), +0x14c collision hardness, +0x150 PhysicsType id (index of the physics property record), +0x154
 * PhysicsObjectHandle vtable 0x937504, +0x158 its tracking record {+0 references, +4 physics world, +8 index in the world's keyed
 * objects (world + 0x1994, 0xe0-byte records)}, +0x15c byte enabled, +0x15d byte "soft" flag (set by nobody here), +0x160 audio emitter.
 * Not built: the audio emitter object of an AudioEmitter whose SoundAsset is not "Editor_Default" (FMOD: +0x160 stays 0; only the
 * sort kind +0x70 = 1 is set as the original does). Not ported (nothing at load or add runs them): vf2 Disable 0041cbe0, vf19 tick
 * 0041cc60, vf28 / vf29 SetPose 004037c0 / 00403940, the destructors and the bale / gate tick and event callbacks (the race layer). */
#include "world.h"
#include "phys_shape_dtors.h"
#include "build_scene_int.h"
#include "build_vehicle.h"
#include "world_veh.h"
#include "phys_body.h"
#include "world_misc.h"
#include "crt.h"

static void fatal(const char* what, uint32_t a) { fprintf(stderr, "rxcore: scene props: %s %08x\n", what, a); abort(); }

/* ---------------------------------------------------------------------------------------------------------------------------
 * the physics object (rigid body) of a prop */

/* 0084f390 (ECX = record, EAX = half extents (x, y, z), stack density; RET 4): mass = 8 x y z density and the box inertia (1/12 of
 * the mass times the squared full extents) */
void rx_phys_prop_mass_inertia(rx_addr rec, rx_addr ext, float density) {
    const float dx = F32(ext, 0), dy = F32(ext, 4), dz = F32(ext, 8);
    float m = rx_muls(rx_muls(rx_muls(rx_muls(dy, dx), dz), 8.0f /* 0x9322b8 */), density);
    SF32(rec, 0, m);
    const float k = rx_bf(0x3daaaaabu) /* 0x9776ac: 1/12 */;
    const float x2 = rx_muls(dx, 2.0f), y2 = rx_muls(dy, 2.0f), z2 = rx_muls(dz, 2.0f);
    const float zz = rx_muls(z2, z2), m12 = rx_muls(m, k), yy = rx_muls(y2, y2);
    const float xx = rx_muls(x2, x2);
    SF32(rec, 0x14, rx_muls(rx_adds(xx, zz), rx_muls(F32(rec, 0), k)));
    SF32(rec, 0x10, rx_muls(rx_adds(yy, zz), m12));
    SF32(rec, 0x18, rx_muls(rx_adds(xx, yy), rx_muls(F32(rec, 0), k)));
}
RX_FUNCTION(0x0084f390, rx_phys_prop_mass_inertia)

/* 0084f6b0 (stack record): the "GL_Pillon" prop: flags 0x17 (+0xb4, copied to +0xb8), bytes +0xd0 / +0xd1 set, the material's four
 * lanes at +0x80 (the shared PhysicsProperties record!) set to 2.0 */
void rx_phys_prop_pillon(rx_addr rec) {
    SU32(rec, 0xb4, U32(rec, 0xb4) | 0x17);
    SU8(rec, 0xd0, 1); SU8(rec, 0xd1, 1);
    SU32(rec, 0xb8, U32(rec, 0xb4));
    rx_addr mat = U32(rec, 0x20);
    for (uint32_t k = 0x80; k <= 0x8c; k += 4) SU32(mat, k, 0x40000000);
}
RX_FUNCTION(0x0084f6b0, rx_phys_prop_pillon)

/* 0084f4a0 (EDI = record, stack entity; RET 4): the keyed-object record of a PhysicsEntity: mass 1 (inertia 1), the material of
 * the entity's PhysicsType (registry 0x00da6ad0 +0xc, 0xd0 bytes each), the entity back pointer +0x34, the pose of the entity's
 * matrix (position (x, y, z, z) at +0x40 / +0x60, the quaternion at +0x50 / +0x70), the mass and inertia from the model's box
 * (IRenderable vf6 at entity + 0x90: centre, half extents, radius) times the material's density (+0x40), flags */
rx_addr rx_phys_prop_record_init(rx_addr r, rx_addr e) {
    SF32(r, 0, 1.0f /* 0x931f24 */); SF32(r, 0x10, 1.0f); SF32(r, 0x14, 1.0f); SF32(r, 0x18, 1.0f);
    rx_addr reg = rx_phys_props_get();   /* the registry's one-time build (guard bit 0 of [0x00dce7d4]) */
    uint32_t id = U32(e, 0x150);
    if (id >= (U32(reg, 0x10) - U32(reg, 0xc)) / 0xd0) fatal("physics property id out of range", id);
    SU32(r, 0x20, id * 0xd0 + U32(reg, 0xc));
    SU32(r, 0x24, U32(e, 0x150));
    SU32(r, 0x2c, 0); SU32(r, 0x28, 0x009765a8); SU32(r, 0x30, 0); SU32(r, 0x34, e); SU32(r, 0x38, 0);
    SU32(r, 0x84, 0); SU32(r, 0x80, 0xffffffff);
    for (uint32_t k = 0; k < 16; k += 4) { SU32(r, 0x90 + k, U32(0, 0x00d61670 + k)); SU32(r, 0xa0 + k, U32(0, 0x00d61670 + k)); }
    const float lim = F32(0, 0x009d78dc);   /* ModelDrawDistance_RadiusLimit50 */
    SU8(r, 0xb0, 1); SU8(r, 0xb1, 1); SU8(r, 0xb2, 1);
    for (uint32_t k = 0; k < 16; k += 4) SF32(r, 0xc0 + k, lim);
    /* frame of the original: push ebp; and esp, -16; sub esp, 0x88; ebx, esi: the matrix at S + 0x38, the quaternion at S + 8 */
    RX_FRAME_ENTER(F, 0x90);
    rx_addr S = F;
    rx_addr M = S + 0x38, Q = S + 8, CTR = S + 0x78, EXT = S + 0x28, RAD = S + 0x24;
    uint32_t a1[1] = { M };
    scene_vcall(e, 0x3c, 1, a1);                                /* Entity::vf15: the matrix out */
    rx_world_matrix3_to_quaternion_to(Q, M);                      /* Matrix3_ToQuaternionToEsi */
    const float px = F32(M, 0x30), py = F32(M, 0x34), pz = F32(M, 0x38);
    SF32(r, 0x40, px); SF32(r, 0x44, py); SF32(r, 0x48, pz); SF32(r, 0x4c, pz);
    SF32(r, 0x60, px); SF32(r, 0x64, py); SF32(r, 0x68, pz); SF32(r, 0x6c, pz);
    for (uint32_t k = 0; k < 16; k += 4) { SU32(r, 0x50 + k, U32(Q, k)); SU32(r, 0x70 + k, U32(Q, k)); }
    uint32_t a3[3] = { CTR, EXT, RAD };
    scene_vcall(e + 0x90, 0x18, 3, a3);                         /* IRenderable::vf6: the model's box */
    float density = F32(U32(r, 0x20), 0x40);
    rx_phys_prop_mass_inertia(r, EXT, density);
    RX_FRAME_LEAVE(F, 0x90);
    SU32(r, 0xb4, 0); SU32(r, 0xb8, 0);
    SU8(r, 0xd0, 0); SU8(r, 0xd1, 0);
    rx_addr name = U32(e, 0x18);
    if (name) {
        const char* pil = "GL_Pillon";
        rx_addr p = name; int eq = 1;
        for (uint32_t k = 0; ; k++) {
            uint8_t a = U8(p, k), b = (uint8_t)pil[k];
            if (a != b) { eq = 0; break; }
            if (!a) break;
        }
        if (eq) rx_phys_prop_pillon(r);
    }
    return r;
}
RX_FUNCTION(0x0084f4a0, rx_phys_prop_record_init)

/* 00774740 (stack vector, stack record; RET 8): push_back into the keyed objects' vector: the same template as 007743a0 (the
 * dynamic objects'), the record copy and growth are the ones of build_vehicle_proxy.c */
void rx_phys_keyed_vector_push(rx_addr vec, rx_addr rec) { rx_phys_dynamic_push(vec, rec); }
RX_FUNCTION(0x00774740, rx_phys_keyed_vector_push)

/* 007738e0 (EAX = the keyed objects' vector (world + 0x1994), stack record; RET 4): the record pushed, its box inserted into the
 * vector's broadphase (00774120), and the record's tracking entry {+4 world, +8 index} pointed at the new slot */
void rx_phys_keyed_push(rx_addr vec, rx_addr rec) {
    int32_t idx = (int32_t)(U32(vec, 0x10) - U32(vec, 0xc)) / 0xe0;
    rx_phys_keyed_vector_push(vec, rec);
    rx_phys_dynamic_insert(vec, (uint32_t)idx);
    uint32_t w = U32(vec, 0x1c);
    rx_addr t = U32(rec, 0x30);
    if (w != 0 && w == U32(t, 4) && (uint32_t)idx != U32(t, 8)) {                 /* the body's lists follow its new slot (0076a9a0) */
        const rx_addr sp = rx_sp(); SU32(sp, -4, (uint32_t)idx); rx_set_sp(sp - 8);
        rx_world_keyed_record_moved(U32(t, 8), w, (uint32_t)idx);
        rx_set_sp(sp);
    }
    if (U32(t, 4) != 0 && w != U32(t, 4)) {                                        /* a body of another world leaves that world's lists (0076aa50) */
        const rx_addr sp = rx_sp(); rx_set_sp(sp - 4);
        rx_world_keyed_record_unlist(U32(t, 8), U32(t, 4));
        rx_set_sp(sp);
    }
    SU32(t, 8, (uint32_t)idx);
    SU32(t, 4, w);
}
RX_FUNCTION(0x007738e0, rx_phys_keyed_push)

/* a CollisionShape / handle release: one reference less, the last one runs the shape's scalar deleting destructor (phys_shape_dtors.c) */
static void shape_release(rx_addr s, const char* where) {
    (void)where;
    rx_collision_shape_release(s);
}

/* 00769670 (ECX = Male collis handle, EDX = Female collis handle, stack world, out PhysicsObjectHandle, entity, soft flag byte; RET 0x10):
 * the entity's rigid body: its record built on the frame, the shape of the two collision handles (shared when the shape list has
 * one made from the same pair), the tracking entry {0, world, index} and the record pushed into world + 0x1994. out = a
 * PhysicsObjectHandle {vtable 0x937504, +4 the tracking entry (references + 1)} */
rx_addr rx_phys_prop_object_create(rx_addr h1, rx_addr h2, rx_addr world, rx_addr out, rx_addr e, uint32_t soft) {
    rx_addr vec = world + 0x1994;
    int32_t idx = (int32_t)(U32(vec, 0x10) - U32(vec, 0xc)) / 0xe0;
    RX_FRAME_ENTER(F, 0x110);
    rx_addr rec = F, tmp = F + 0xf0;
    for (uint32_t k = 0; k < 0xe0; k += 4) SU32(rec, k, 0);   /* (the words the init never writes are dead stack in the original; zero here) */
    rx_phys_prop_record_init(rec, e);
    rx_addr t = rx_reflex_aligned_alloc(0xc);
    if (t) { SU32(t, 0, 0); SU32(t, 4, world); SU32(t, 8, (uint32_t)idx); }
    SU32(rec, 0x30, t);
    rx_shape_handle_for(tmp, h1, h2, 1);
    rx_addr cur = U32(rec, 0x2c), s = U32(tmp, 4);
    if (s != cur) {
        shape_release(cur, "00769670: shape destructor not ported");
        SU32(rec, 0x2c, s);
        if (s) SU32(s, 4, U32(s, 4) + 1);
    }
    SU32(tmp, 0, 0x009765b4);
    shape_release(U32(tmp, 4), "00769670: shape freed");
    rx_addr shape = U32(rec, 0x2c);
    if (soft & 0xff) SU32(shape, 0x10, 4);
    SU32(shape, 0x10, U32(shape, 0x10) | 0x10);
    rx_phys_keyed_push(vec, rec);
    SU32(out, 0, 0x00937510);
    SU32(out, 4, t);
    SU32(t, 0, U32(t, 0) + 1);
    SU32(out, 0, 0x00937504);   /* RavenPhysics::PhysicsObjectHandle::vftable */
    shape_release(U32(rec, 0x2c), "00769670: record shape freed");
    RX_FRAME_LEAVE(F, 0x110);
    return out;
}
RX_FUNCTION(0x00769670, rx_phys_prop_object_create)

/* 0041ccf0 (EAX = entity, EDX = physics world): the body made and its tracking entry taken as the entity's +0x158 */
void rx_physics_entity_create_body(rx_addr e, rx_addr world) {
    RX_FRAME_ENTER(F, 0x10);
    rx_addr out = F;
    rx_phys_prop_object_create(U32(e, 0x140), U32(e, 0x144), world, out, e, U8(e, 0x15d));
    rx_addr t = U32(out, 4);
    rx_addr old = U32(e, 0x158);
    if (t != old) {
        if (old) {
            SU32(old, 0, U32(old, 0) - 1);
            if (U32(old, 0) == 0) fatal("0041ccf0: tracking entry freed (0x00417680) not ported", old);
        }
        SU32(e, 0x158, t);
        if (t) SU32(t, 0, U32(t, 0) + 1);
    }
    SU32(out, 0, 0x00937510);
    if (t) {
        SU32(t, 0, U32(t, 0) - 1);
        if (U32(t, 0) == 0) fatal("0041ccf0: tracking entry freed (0x00402360) not ported", t);
    }
    RX_FRAME_LEAVE(F, 0x10);
}
RX_FUNCTION(0x0041ccf0, rx_physics_entity_create_body)

/* ---------------------------------------------------------------------------------------------------------------------------
 * PhysicsEntity */

/* 004036d0 (ESI = entity): after the ModelEntity constructor */
rx_addr rx_physics_entity_construct(rx_addr e) {
    rx_model_entity_construct(e);
    SU32(e, 0, 0x0093743c);
    SU32(e, 0x90, 0x009374c4);
    SU32(e, 0x158, 0);
    SU32(e, 0x154, 0x00937504);
    SU32(e, 0x74, U32(e, 0x74) | 4);
    SU32(e, 0x70, 0);
    SU32(e, 0x148, 0);
    SU32(e, 0x160, 0);
    SU8(e, 0x15c, 1);
    return e;
}
RX_FUNCTION(0x004036d0, rx_physics_entity_construct)

/* the Asset child of a sub-RSData: the name attribute of <field><RSData name=sub><Asset name=...> (NULL when absent) */
static const char* rs_nested_asset_name(const char* field, const char* sub) {
    const struct fmt_entity* rs = rx_scene_rs();
    int f = rs ? rs_field(field) : -1;
    if (f < 0) return NULL;
    int n = fmt_bxml_child_count(rs->doc, f);
    for (int k = 0; k < n; k++) {
        int c = fmt_bxml_child(rs->doc, f, k);
        const char* nm = fmt_bxml_attr_str(rs->doc, c, "name");
        if (!nm || strcmp(nm, sub)) continue;
        int a = fmt_bxml_find_child(rs->doc, c, "Asset");
        return a >= 0 ? fmt_bxml_attr_str(rs->doc, a, "name") : NULL;
    }
    return NULL;
}

rx_addr rx_physics_entity_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x170);
    return rx_physics_entity_construct(e);
}

/* PhysicsEntity::vf5 0041c8a0 (ECX = entity, stack reader; RET 4) */
uint32_t rx_physics_entity_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    SU8(e, 0x15d, 0);
    SU32(e, 0x144, rs_asset("FemaleCollisionAsset"));
    SU32(e, 0x140, rs_asset("MaleCollisionAsset"));
    if (rs_has("CollisionAudioMaterial", RS_ENUMVALUE)) SU32(e, 0x148, (uint32_t)rs_enum("CollisionAudioMaterial"));
    else SU32(e, 0x148, 0);
    if (rs_has("CollisionAudioHardness", RS_UINT)) {
        uint32_t v = rs_uint("CollisionAudioHardness");
        SU32(e, 0x148, v);
        if (v > 10) SU32(e, 0x14c, 10);   /* (up to 10: the word keeps what the allocation left) */
    } else if (rs_has("UseSoftCollision", RS_BOOL)) {
        SU32(e, 0x14c, rs_bool("UseSoftCollision") ? 0u : 5u);
    } else SU32(e, 0x14c, 5);
    if (rs_has("AIAvoid", 0)) { if (rs_bool("AIAvoid")) SU32(e, 0x74, U32(e, 0x74) | 8); }
    if (rs_has("PhysicsType", 0)) {
        const char* name = rs_enum_text("PhysicsType");
        rx_addr reg = rx_phys_props_get();
        SU32(e, 0x150, name ? rx_phys_props_find(reg, name) : 0);
    } else SU32(e, 0x150, 0);
    if (rs_has("AudioEmitter", 0)) {
        const char* snd = rs_nested_asset_name("AudioEmitter", "SoundAsset");
        if (snd && strcasecmp(snd, "Editor_Default") != 0) {
            /* the emitter object (ReflexAlignedAlloc(0x1e0), FUN_006788f0 + FUN_00678c10 at the entity's position) is the audio layer's:
             * +0x160 stays 0 here; the sort kind is set as the original does */
            SU32(e, 0x70, 1);
        }
    }
    rx_model_instance_init(e + 0xa0);
    rx_entity_vf5(e);
    scene_copy_words(e + 0xc0, e + 0x30, 16);
    SU16(e, 0xb0, (U16(e, 0xb0) & 0xffef) | 8);
    rx_addr world = rx_phys_world_of_scene(U32(e, 0x10));
    rx_physics_entity_create_body(e, world);
    return 0;
}
RX_FUNCTION(0x0041c8a0, rx_physics_entity_vf5)

/* vf1 0041cbc0 (ECX = entity): Enable: the body's flag +0xb0 set when the entity is enabled and has a body */
uint32_t rx_physics_entity_vf1(rx_addr e) {
    if (U8(e, 0x15c) != 0 && U32(e, 0x158) != 0) SU8(rx_world_physics_handle_body(U32(e, 0x158)), 0xb0, 1);
    return 0;
}
RX_FUNCTION(0x0041cbc0, rx_physics_entity_vf1)

/* vf31 00403b20 (ECX = entity, stack position, quaternion; RET 8): the entity's pose: the quaternion of the matrix, the position
 * (x, y, z, z) of the matrix' translation row */
uint32_t rx_physics_entity_get_pose(rx_addr e, rx_addr pos, rx_addr quat) {
    RX_FRAME_ENTER(F, 0x20);
    rx_addr Q = F + 0x14;
    rx_world_matrix3_to_quaternion_to(Q, e + 0x30);
    for (uint32_t k = 0; k < 16; k += 4) SU32(quat, k, U32(Q, k));
    const float x = F32(e, 0x60), y = F32(e, 0x64), z = F32(e, 0x68);
    SF32(pos, 0, x); SF32(pos, 4, y); SF32(pos, 8, z); SF32(pos, 12, z);
    RX_FRAME_LEAVE(F, 0x20);
    return 0;
}
RX_FUNCTION(0x00403b20, rx_physics_entity_get_pose)

/* vf16 00403720 (ECX = entity, stack matrix; RET 4): the matrix set; with a body its position / orientation follow, and its box
 * is updated in the broadphase of the keyed objects */
uint32_t rx_physics_entity_vf16(rx_addr e, rx_addr m) {
    scene_copy_words(e + 0x30, m, 16);
    RX_FRAME_ENTER(F, 0x40);
    rx_addr pos = F + 4, quat = F + 0x14;
    uint32_t a2[2] = { pos, quat };
    scene_vcall(e, 0x7c, 2, a2);                 /* vf31 */
    if (U32(e, 0x158) != 0) {
        rx_addr b = rx_world_physics_handle_body(U32(e, 0x158));
        for (uint32_t k = 0; k < 16; k += 4) SU32(b, 0x40 + k, U32(pos, k));
    }
    if (U32(e, 0x158) != 0) {
        rx_addr b = rx_world_physics_handle_body(U32(e, 0x158));
        for (uint32_t k = 0; k < 16; k += 4) SU32(b, 0x50 + k, U32(quat, k));
    }
    rx_addr t = U32(e, 0x158);
    if (t != 0) phys_body_update_grid(U32(t, 4) + 0x1994, (int32_t)U32(t, 8));
    RX_FRAME_LEAVE(F, 0x40);
    return 0;
}
RX_FUNCTION(0x00403720, rx_physics_entity_vf16)

/* ---------------------------------------------------------------------------------------------------------------------------
 * event handlers: the registration the classes do at load (006c3bb0 / 006c3cd0 / 006c4270 / 006c4390 / 006c4970). An event channel
 * (EventSystem::Channel, 0x5c bytes) keeps a registry name -> event record {+4 the name of the handler signature's type_info, +8 the
 * handler list (std::list: head node at +0x1c, size at +0x20), +0x24 its critical section}. The handler is a Proxy object (0x18
 * bytes: vtable, +4 0, +8 the signature type name, +0xc the entity, +0x10 the callback, +0x14 its argument); it is appended when its
 * signature type name equals the event's. The core is single threaded: the critical section (event + 0x24) is not entered. */

/* 005b9ff0 (EAX = &handler, stack list; RET 4): std::list<Handler*>::push_back (node 0xc: next, prev, value) */
static void handler_list_push_back(rx_addr list, rx_addr handler) {
    rx_addr head = U32(list, 0x14);
    rx_addr prev = U32(head, 4);
    rx_addr node = rx_reflex_aligned_alloc(0xc);
    if (node) { SU32(node, 0, head); SU32(node, 4, prev); SU32(node, 8, handler); }
    if (0x3fffffffu - U32(list, 0x18) < 1) fatal("list<T> too long", list);
    SU32(list, 0x18, U32(list, 0x18) + 1);
    SU32(head, 4, node);
    SU32(prev, 0, node);
}

/* the shared body: channel = the event channel (EAX / ECX / the global), name = C string of the event */
static void register_handler(rx_addr channel, rx_addr name, uint32_t proxy_vt, rx_addr type_info, rx_addr entity, uint32_t cb, uint32_t arg) {
    if (!channel) {
        /* the event system (boot: 00409690 "VehicleTrigger", the activity manager's "ActivityManagerGlobalEvents" channel) is not built */
        if (getenv("RX_SCENE_ALLOW_UNBUILT")) return;
        fatal("event channel missing for handler registration (event system not built)", name);
    }
    rx_addr ev = rx_world_registry_find(channel, name);
    if (!ev) fatal("event not registered in the channel", name);
    rx_addr h = rx_reflex_aligned_alloc(0x18);
    SU32(h, 0xc, entity); SU32(h, 4, 0); SU32(h, 0, proxy_vt); SU32(h, 0x10, cb); SU32(h, 0x14, arg);
    SU32(h, 8, crt_type_info_name(type_info, 0x009e1a7c));   /* typeid(proxy signature).name() */
    if (crt_strcmp(U32(ev, 4), U32(h, 8)) == 0) handler_list_push_back(ev + 8, h);
}

/* the shared registration for other units' subscription templates (world_flag_holders.c): the channel's event of the name gets the handler {vtable, signature type_info, entity, callback, argument} */
void rx_event_register_proxy(rx_addr channel, rx_addr name, uint32_t proxy_vt, rx_addr type_info, rx_addr entity, uint32_t cb, uint32_t arg) {
    register_handler(channel, name, proxy_vt, type_info, entity, cb, arg);
}

/* 006c3bb0 (ECX = channel, EAX = name, stack entity, callback, arg; RET 0xc): void(Activity*) handler of an entity (EventHandlerProxy
 * vtable 0x009603d4, signature type_info 0x009cbd24) */
void rx_event_register_activity(rx_addr channel, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg) {
    register_handler(channel, name, 0x009603d4, 0x009cbd24, entity, cb, arg);
}
RX_FUNCTION(0x006c3bb0, rx_event_register_activity)
/* 006c3cd0 (EAX = channel, name "Countdown Step"; stack entity, callback, arg; RET 0xc): void(Activity*, int) (vtable 0x009603f4, 0x009cf5b4) */
void rx_event_register_countdown_step(rx_addr channel, rx_addr entity, uint32_t cb, uint32_t arg) {
    register_handler(channel, 0x00941bdc, 0x009603f4, 0x009cf5b4, entity, cb, arg);
}
RX_FUNCTION(0x006c3cd0, rx_event_register_countdown_step)
/* 006c4270 (EAX = channel, name "VehicleExited"; stack entity, callback, arg; RET 0xc): void(RavenVehicle*, const MXTrigger*) of a PopDownBale */
void rx_event_register_vehicle_exited_down(rx_addr channel, rx_addr entity, uint32_t cb, uint32_t arg) {
    register_handler(channel, 0x00935e94, 0x00960494, 0x009cbb7c, entity, cb, arg);
}
RX_FUNCTION(0x006c4270, rx_event_register_vehicle_exited_down)
/* 006c4390: the same for a PopUpBale (vtable 0x009604b4) */
void rx_event_register_vehicle_exited_up(rx_addr channel, rx_addr entity, uint32_t cb, uint32_t arg) {
    register_handler(channel, 0x00935e94, 0x009604b4, 0x009cbb7c, entity, cb, arg);
}
RX_FUNCTION(0x006c4390, rx_event_register_vehicle_exited_up)
/* 006c4970 (ECX = name; the channel [0x00a93f74]; stack entity, callback, arg; RET 0xc): void(Activity*) (vtable 0x00960554, 0x009cbd24) */
void rx_event_register_global(rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg) {
    register_handler(U32(0, 0x00a93f74), name, 0x00960554, 0x009cbd24, entity, cb, arg);
}
RX_FUNCTION(0x006c4970, rx_event_register_global)

/* 006c3860 (stack entity, callback, arg; RET 0xc): "First Human Joined Activity" on the activity manager's channel [0x00a93f74]
 * (void(Activity*): vtable 0x00960394, signature type_info 0x009cbd24) */
void rx_event_register_first_human(rx_addr entity, uint32_t cb, uint32_t arg) {
    register_handler(U32(0, 0x00a93f74), 0x00941df4, 0x00960394, 0x009cbd24, entity, cb, arg);
}
RX_FUNCTION(0x006c3860, rx_event_register_first_human)
/* 006c44b0 (ECX = name; stack entity, callback, arg; RET 0xc): like 006c4970 with the proxy vtable 0x009604d4 */
void rx_event_register_global_b(rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg) {
    register_handler(U32(0, 0x00a93f74), name, 0x009604d4, 0x009cbd24, entity, cb, arg);
}
RX_FUNCTION(0x006c44b0, rx_event_register_global_b)

/* ---------------------------------------------------------------------------------------------------------------------------
 * PopUpBale (0x1f0) / PopDownBale (0x1f0) */

/* PopUpBale 006c0dc0 (EAX = entity) */
rx_addr rx_popup_bale_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x1f0);
    rx_physics_entity_construct(e);
    SU32(e, 0x170, 0);
    SU32(e, 0x1e4, 0);
    SU8(e, 0x178, 0);
    SU32(e, 0, 0x0095ffcc);
    SU32(e, 0x90, 0x00960054);
    SU32(e, 0x174, 1);
    SF32(e, 0x1e0, F32(0, 0x009d3bfc));
    return e;
}
/* the shared tail: the entity's position from vf17 (Entity::vf17 00402c90 copies x, y, z) */
static void bale_positions(rx_addr e, uint32_t up) {
    RX_FRAME_ENTER(F, 0x20);
    rx_addr v = F + 0x10;
    uint32_t a1[1] = { v };
    SU32(v, 0xc, 0);   /* the fourth lane is the original's stack leftover (unused) */
    scene_vcall(e, 0x44, 1, a1);                       /* vf17 */
    const float x = F32(v, 0), y = F32(v, 4), z = F32(v, 8);
    const float y5 = rx_subs(y, 5.0f /* 0x935890 */);
    if (up) {   /* PopUp: +0x1d0 = the position, +0x1c0 = 5 below it */
        for (uint32_t k = 0; k < 16; k += 4) SU32(e, 0x1d0 + k, U32(v, k));
        SF32(v, 4, y5);
        for (uint32_t k = 0; k < 16; k += 4) SU32(e, 0x1c0 + k, U32(v, k));
    } else {    /* PopDown: +0x1c0 = the position, +0x1d0 = 5 below it */
        for (uint32_t k = 0; k < 16; k += 4) SU32(e, 0x1c0 + k, U32(v, k));
        SF32(v, 4, y5);
        for (uint32_t k = 0; k < 16; k += 4) SU32(e, 0x1d0 + k, U32(v, k));
    }
    SF32(e, 0x64, y5); SF32(e, 0x60, x); SF32(e, 0x68, z);
    RX_FRAME_LEAVE(F, 0x20);
}
/* PopUpBale::vf5 006c0e70 */
uint32_t rx_popup_bale_vf5(rx_addr e, uint32_t reader) {
    rx_physics_entity_vf5(e, reader);
    const char* id = rs_str("LinkedTriggerID");
    uint32_t n = (uint32_t)strlen(id);
    for (uint32_t k = 0; k <= n; k++) SU8(e, 0x178 + k, (uint8_t)id[k]);
    uint32_t tick = rs_bool("EnableTick");
    rx_addr channel = U32(0, 0x00a89bc8);
    SU32(e, 0x1e4, channel);
    SU32(e, 0x70, tick);
    rx_event_register_vehicle_exited_up(channel, e, 0x006c14a0, 0);
    bale_positions(e, 1);
    SU8(e, 0xa2, 0);
    return 0;
}
RX_FUNCTION(0x006c0e70, rx_popup_bale_vf5)

/* PopDownBale 006c05a0 */
rx_addr rx_popdown_bale_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x1f0);
    rx_physics_entity_construct(e);
    SU32(e, 0x170, 0);
    SU32(e, 0x174, 0);
    SU32(e, 0x1e4, 0);
    SU8(e, 0x17c, 0);
    SU32(e, 0, 0x0095ff04);
    SU32(e, 0x90, 0x0095ff8c);
    SU32(e, 0x178, 1);
    SF32(e, 0x1e0, F32(0, 0x009d3bfc));
    return e;
}
/* PopDownBale::vf5 006c0650 */
uint32_t rx_popdown_bale_vf5(rx_addr e, uint32_t reader) {
    rx_physics_entity_vf5(e, reader);
    const char* id = rs_str("LinkedTriggerID");
    uint32_t n = (uint32_t)strlen(id);
    for (uint32_t k = 0; k <= n; k++) SU8(e, 0x17c + k, (uint8_t)id[k]);
    uint32_t tick = rs_bool("EnableTick");
    rx_addr channel = U32(0, 0x00a89bc8);
    SU32(e, 0x1e4, channel);
    SU32(e, 0x70, tick);
    rx_event_register_vehicle_exited_down(U32(e, 0x1e4), e, 0x006c0b60, 0);
    bale_positions(e, 0);
    SU8(e, 0xa2, 1);
    return 0;
}
RX_FUNCTION(0x006c0650, rx_popdown_bale_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * DropGate (0x220): a CollisionEntity */

/* DropGate 006bd910 (EAX = entity) after the CollisionEntity constructor 00818f60 */
rx_addr rx_drop_gate_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x220);
    rx_collision_entity_construct(e);
    SF32(e, 0x1f0, F32(0, 0x00977d28));
    const float a = F32(0, 0x009ddfe4);
    SU32(e, 0, 0x0095fc24);
    SU32(e, 0x90, 0x0095fc9c);
    SU32(e, 0x1a0, 0);
    SF32(e, 0x1f4, a);
    SF32(e, 0x1f8, 0.0f);
    SF32(e, 0x1fc, F32(0, 0x00935860));
    SF32(e, 0x200, a);
    SF32(e, 0x204, 0.0f);
    return e;
}
/* DropGate::vf5 006bd9f0 */
uint32_t rx_drop_gate_vf5(rx_addr e, uint32_t reader) {
    rx_collision_entity_vf5(e, reader);
    uint32_t tick = rs_bool("EnableTick");
    rx_addr activity = U32(0, 0x00a93f74);
    SU32(e, 0x70, tick);
    SU32(e, 0x208, activity);
    rx_event_register_activity(activity, 0x00935ea4 /* "Activity Init" */, e, 0x006bdb50, 0);
    rx_event_register_activity(U32(e, 0x208), 0x00941d90 /* "Activity UnInit" */, e, 0x006bdb90, 0);
    uint32_t a1[1] = { e + 0x1b0 };
    scene_vcall(e, 0x3c, 1, a1);                       /* vf15: the matrix copy at +0x1b0 */
    SU16(e, 0xb0, (U16(e, 0xb0) & 0xffef) | 8);
    return 0;
}
RX_FUNCTION(0x006bd9f0, rx_drop_gate_vf5)

/* ---------------------------------------------------------------------------------------------------------------------------
 * HFTerrainEntity (0x45a0): the constructor 007c8cf0 (build_terrain.c's rx_hfterrain_entity_construct_temp: the Entity fields, the
 * vtable 0x00971d6c and the embedded surface at +0x90) and vf5 007c8db0 = Entity::vf5, then the terrain from the RSData */
rx_addr rx_hfterrain_entity_construct(rx_addr e) { return rx_hfterrain_entity_construct_temp(e); }
RX_FUNCTION(0x007c8cf0, rx_hfterrain_entity_construct)
rx_addr rx_hfterrain_entity_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x45a0);
    return rx_hfterrain_entity_construct(e);
}
uint32_t rx_hfterrain_entity_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    rx_hfterrain_entity_load(e, rx_scene_rs());
    return 0;
}
RX_FUNCTION(0x007c8db0, rx_hfterrain_entity_vf5)
