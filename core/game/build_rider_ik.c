/* build_rider_ik.c -- construction unit 7 (riders): the rider's ragdoll / IK data from the BXML "MXWreckingRagdoll" (FUN_004246c0:
 * the global parameters, the rigid bodies (0041fad0) with their mass / inertia, the pins (0041fc20 / 0041fcc0: the joint limits
 * 0041dca0), the solve order (00420090 / 00420100), the bones of the bodies on the Character (0082e160), the collision objects) and
 * the rider bodies in the physics world (00429700 / 00772760: one CollisionShapeCircle per collision object, a dynamic object and its
 * handle). Hand transliterations of the disassembly; the data layout in build_rider.h. */
#include "phys_shape_dtors.h"
#include "world_veh_transfer.h"
#include "build_alloc.h"
#include "build_rider.h"
#include "build_rider_util.h"
#include "build_vehicle.h"
#include "crt.h"
#include "race.h"
#include "loop_vehicle.h"

/* the reader's virtuals (build_xml.c) */
static uint32_t xv(rx_addr r, uint32_t off, int n, uint32_t a0, uint32_t a1, uint32_t a2) { uint32_t a[3] = {a0, a1, a2}; return rr_vcall(r, off, n, a); }
#define X_CHILD(r, n, nm)       xv(r, 0x1c, 2, n, nm, 0)
#define X_FIRST(r, n, nm, out)  xv(r, 0x20, 3, n, nm, out)
#define X_RELEASE(r, c)         xv(r, 0x24, 1, c, 0, 0)
#define X_NEXT(r, c)            xv(r, 0x28, 1, c, 0, 0)
#define X_ATTR(r, n, nm)        xv(r, 0xe8, 2, n, nm, 0)
#define X_CSTR(r, e)            xv(r, 0x110, 1, e, 0, 0)
static void a_ints(rx_addr r, rx_addr n, rx_addr name, rx_addr dst) { xv(r, 0x134, 3, X_ATTR(r, n, name), dst, 1); }    /* vf77 */
static void a_floats(rx_addr r, rx_addr n, rx_addr name, rx_addr dst) { xv(r, 0x130, 3, X_ATTR(r, n, name), dst, 1); }  /* vf76 */
static void a_vec3(rx_addr r, rx_addr n, rx_addr name, rx_addr dst) { xv(r, 0x124, 3, X_ATTR(r, n, name), dst, 1); }    /* vf73 */
static void a_vec4(rx_addr r, rx_addr n, rx_addr name, rx_addr dst) { xv(r, 0x11c, 3, X_ATTR(r, n, name), dst, 1); }    /* vf71 */
static void a_quat(rx_addr r, rx_addr n, rx_addr name, rx_addr dst) { xv(r, 0x118, 3, X_ATTR(r, n, name), dst, 1); }    /* vf70 */
/* the "Name" form: the string attribute's text (vf68), copied with the original's byte loop (from address 0 when absent) */
static void a_name(rx_addr r, rx_addr n, rx_addr name, rx_addr dst) {
    rx_addr e = X_ATTR(r, n, name);
    rx_addr s = e ? X_CSTR(r, e) : 0;
    for (uint32_t k = 0;; k++) { uint8_t c = U8(s, k); SU8(dst, k, c); if (!c) break; }
}

/* the link record of a node (chains + node * 0x230 + 0x39e0: {kind (0 body, 1 pin), the body / pin index, node, parent, child count
 * (+0x10), children (+0x14..)}) and the parent's child list */
static void link_node(rx_addr ch, uint32_t kind, uint32_t idx, int32_t node, int32_t parent) {
    rx_addr rec = ch + 0x230u * (uint32_t)node;
    SU32(rec, 0x39e0, kind);
    SU32(rec, 0x39e4, idx);
    SU32(rec, 0x39e8, (uint32_t)node);
    SU32(rec, 0x39ec, (uint32_t)parent);
    if (parent != -5) {
        rx_addr pr = ch + 0x230u * (uint32_t)parent;
        uint32_t n = U32(pr, 0x39f0);
        SU32(ch, 4u * ((uint32_t)parent * 0x8c + n) + 0x39f4, (uint32_t)node);
        SU32(pr, 0x39f0, n + 1);
    }
}
/* 0041fad0 (ESI = chains, ECX = the 0x80 body template, EDX = the 0x70 state template; s0 node, s1 parent; RET 8): a rigid body */
void rx_ragdoll_add_body(rx_addr ch, rx_addr body_t, rx_addr state_t, int32_t node, int32_t parent) {
    rx_addr d = ch + ((U32(ch, 0x7c) + 1) << 7);
    for (uint32_t k = 0; k < 0x70; k += 4) SU32(d, k, U32(body_t, k));   /* 0041fbc0 */
    SU32(d, 0x70, U32(body_t, 0x70));
    SU8(d, 0x74, U8(body_t, 0x74));
    rx_addr s = ch + 0xb00 + U32(ch, 0x7c) * 0x70;
    for (uint32_t k = 0; k < 0x70; k += 4) SU32(s, k, U32(state_t, k));
    SU32(ch, 4 * U32(ch, 0x7c) + 0x1970, (uint32_t)node);
    link_node(ch, 0, U32(ch, 0x7c), node, parent);
    SU32(ch, 0x7c, U32(ch, 0x7c) + 1);
    SU32(ch, 0x39d0, U32(ch, 0x39d0) + 1);
    SU32(ch, 0x9438, 0);
}
/* 0041fcc0 (ECX = source pin, s0 destination; RET 4): a pin record copy (the words, the vectors, the limits through the x87) */
static void pin_copy(rx_addr d, rx_addr s) {
    SU32(d, 0, U32(s, 0));
    rr_v4(d + 0x10, s + 0x10);
    SU32(d, 0x20, U32(s, 0x20));
    for (uint32_t o = 0x30; o < 0xc0; o += 0x10) rr_v4(d + o, s + o);
    for (uint32_t o = 0xc0; o <= 0xe0; o += 4) SU32(d, o, rr_x87_copy(U32(s, o)));
    for (uint32_t o = 0xe4; o < 0x164; o += 4) SU32(d, o, rr_x87_copy(U32(s, o)));
}
/* 0041fc20 (ESI = chains, EAX = parent; s0 node, s1 &pin; RET 8): a pin (joint) */
void rx_ragdoll_add_pin(rx_addr ch, int32_t parent, int32_t node, rx_addr pin) {
    pin_copy(ch + 0x1b30 + U32(ch, 0x1b20) * 0x170, pin);
    link_node(ch, 1, U32(ch, 0x1b20), node, parent);
    SU32(ch, 0x1b20, U32(ch, 0x1b20) + 1);
    SU32(ch, 0x39d0, U32(ch, 0x39d0) + 1);
    SU32(ch, 0x9438, 0);
}
/* 00420100 (ECX = chains, s0 node; RET 4): the post-order of the node tree into +0x9394 */
static void order_nodes(rx_addr ch, uint32_t node) {
    rx_addr rec = ch + 0x230 * node;
    for (int32_t i = 0; i < (int32_t)U32(rec, 0x39f0); i++) order_nodes(ch, U32(rec, 0x39f4 + 4u * (uint32_t)i));
    SU32(ch, 4 * U32(ch, 0x9390) + 0x9394, node);
    SU32(ch, 0x9390, U32(ch, 0x9390) + 1);
}
/* 00420090 (ESI = chains): the order, then the pins in it (+0x37f0), +0x9438 1 */
void rx_ragdoll_finish(rx_addr ch) {
    SU32(ch, 0x9390, 0);
    order_nodes(ch, 0);
    rx_addr out = ch + 0x37f0;
    for (int32_t k = 0; k < (int32_t)U32(ch, 0x9390); k++) {
        rx_addr rec = ch + 0x230 * U32(ch, 0x9394 + 4u * (uint32_t)k);
        if (U32(rec, 0x39e0)) { SU32(out, 0, U32(rec, 0x39e4)); out += 4; }
    }
    SU32(ch, 0x9438, 1);
}
/* 0082e160 (ESI = character, EAX = n; s0 bone list; RET 4): the ragdoll's bones on the Character: n poses (+0x1f8), the list (+0x1fc),
 * the bone flags (+0 .. +0x1f3 cleared, 1 for each listed bone) */
static void pose_reset(rx_addr rec) { rx_race_pose_reset(rec); }
void rx_character_set_ragdoll_bones(rx_addr c, uint32_t n, rx_addr bones) {
    SU32(c, 0x200, n);
    rx_addr p = rx_crt_aligned_malloc(rx_mul_sat(n, 0x30), 0x10);
    if (p) rx_rider_vector_ctor(p, 0x30, (int32_t)n, pose_reset);
    SU32(c, 0x1f8, p);
    SU32(c, 0x1fc, rr_alloc(rx_mul_sat(U32(c, 0x200), 4)));
    for (int32_t k = 0; k < (int32_t)U32(c, 0x200); k++) SU32(U32(c, 0x1fc), 4u * (uint32_t)k, U32(bones, 4u * (uint32_t)k));
    for (uint32_t k = 0; k < 0x7d * 4; k += 4) SU32(c, k, 0);
    for (int32_t k = 0; k < (int32_t)U32(c, 0x200); k++) SU32(c, 4 * U32(U32(c, 0x1fc), 4u * (uint32_t)k), 1);
}

/* the MXWorld of the running scene (the engine's scene +0x34: MXGame + 0x5a54, or the front end's + 0x20c8) */
static rx_addr current_mxworld(void) {
    rx_addr eng = U32(0, RR_ENGINE);
    if (U32(U32(eng, 0x20), 0x34) == 1) return U32(U32(0, RR_GAME), 0x5a54);
    if (U32(U32(eng, 0x1c), 0x34) == 1) return U32(U32(0, RR_GAME), 0x20c8);
    return 0;
}
/* a handle's release (ProxyObjectHandle record {refs, world, index}: at 0 the object leaves the world (0042af70) and the record is freed) */
static void handle_release(rx_addr h) {
    if (!h) return;
    SU32(h, 0, U32(h, 0) - 1);
    if (U32(h, 0)) return;
    rx_dyn_records_remove(U32(h, 4) + 0x1954, U32(h, 8));     /* 0042af70: the body leaves its world's dynamic objects ... */
    rx_reflex_aligned_free(h);                                /* ... and the record is freed */
}
/* 00772760 (ECX = owner; stdcall world, out handle, radius; RET 0xc): a rider body: a dynamic object record (0084fcc0, the owner,
 * a CollisionShapeCircle of the radius with flags 0x48, its handle record {refs, world, index}) pushed into the world's dynamic
 * objects (007743a0) and its broadphase (00774120); *out = {0x0093a644, the handle record} */
rx_addr rx_rider_body_create(rx_addr owner, rx_addr pw, rx_addr out, float radius) {
    RX_FRAME_ENTER(F, 0x120);
    rx_addr dims = F + 0x14, rec = F + 0x24;
    rx_addr vec = pw + 0x1954;
    uint32_t index = (uint32_t)((int32_t)(U32(pw, 0x1964) - U32(pw, 0x1960)) / 0xe0);
    SF32(dims, 0, 1.0f); SF32(dims, 4, 1.0f); SF32(dims, 8, 1.0f);
    rx_phys_dynamic_record_init(rec, dims, 0);
    SU32(rec, 0xd0, 0);
    rr_v4(rec + 0x40, RR_RT_ZERO);
    rr_v4(rec + 0x50, RR_RT_W1);
    for (uint32_t o = 0x60; o <= 0xa0; o += 0x10) rr_v4(rec + o, RR_RT_ZERO);
    SU32(rec, 0x34, owner);
    rx_addr sh = rx_crt_aligned_malloc(0x40, 0x10);   /* 0070cf80 */
    if (sh) sh = rx_rider_shape_circle_construct(sh, rx_bf(rr_x87_copy(rx_fb(radius))));
    SU32(sh, 0x18, 0); SU32(sh, 0x1c, 0);
    rx_addr old = U32(rec, 0x2c);
    SU32(sh, 4, U32(sh, 4) + 1);
    if (sh != old) {
        rx_collision_shape_release(old);
        SU32(sh, 4, U32(sh, 4) + 1);
        SU32(rec, 0x2c, sh);
    }
    rx_collision_shape_release(sh);
    SU32(U32(rec, 0x2c), 0x10, U32(U32(rec, 0x2c), 0x10) | 0x48);
    rx_addr h = rr_alloc(0xc);
    if (h) { SU32(h, 0, 0); SU32(h, 4, pw); SU32(h, 8, index); }
    SU32(rec, 0x30, h);
    uint32_t idx2 = (uint32_t)((int32_t)(U32(vec, 0x10) - U32(vec, 0xc)) / 0xe0);
    rx_phys_dynamic_push(vec, rec);
    rx_phys_dynamic_insert(vec, idx2);
    h = U32(rec, 0x30);
    SU32(h, 8, idx2);
    SU32(h, 4, U32(vec, 0x1c));
    SU32(out, 0, 0x0093a650);
    SU32(out, 4, h);
    SU32(h, 0, U32(h, 0) + 1);
    SU32(out, 0, 0x0093a644);
    rx_addr s2 = U32(rec, 0x2c);
    rx_collision_shape_release(s2);
    RX_FRAME_LEAVE(F, 0x120);
    return out;
}
/* 00429700 (stdcall data, ra; RET 8): the 16 collision objects' bodies (radius +0xa7b8 + 4k), their handles at data + 8 + 8k, the
 * bodies' names (+0xd0) and flag byte (+0xd4) in the world's records; data + 0x88 0, 0042a1c0(data) */
void rx_rider_bodies_create(rx_addr d, rx_addr ra) {
    rx_addr objs = d + 0xa764, slot = d + 0xc;
    RX_FRAME_ENTER(F, 0x20);
    for (int k = 0; k < 0x10; k++, objs += 4, slot += 8) {
        rx_addr w = current_mxworld();
        rx_addr pw = U32(w, 0x20134);
        rx_rider_body_create(ra, pw, F + 0x18, rx_bf(rr_x87_copy(U32(objs, 0x54))));
        rx_addr h = U32(F + 0x18, 4);
        if (U32(slot, 0) != h) {
            handle_release(U32(slot, 0));
            SU32(slot, 0, h);
            if (h) SU32(h, 0, U32(h, 0) + 1);
        }
        SU32(F + 0x18, 0, 0x0093a650);
        handle_release(U32(F + 0x18, 4));
        rx_addr nm = d + (U32(objs, 0) << 7) + 0x9ac0;
        rx_addr hh = U32(slot, 0);
        if (hh) {
            rx_addr dv = U32(hh, 4) + 0x1954;
            uint32_t i = U32(hh, 8);
            SU32(U32(dv, 0xc) + i * 0xe0, 0xd0, nm);
            hh = U32(slot, 0);
            if (hh) {
                dv = U32(hh, 4) + 0x1954; i = U32(hh, 8);
                SU8(U32(dv, 0xc) + i * 0xe0, 0xd4, 0);
            }
        }
    }
    RX_FRAME_LEAVE(F, 0x20);
    SU8(d, 0x88, 0);
    rx_vehicle_clear_handles(d);
}

/* 004246c0 (stdcall data (ra + 0x5d0), character, ra; RET 0xc) */
void rx_rider_ik_setup(rx_addr d, rx_addr chr, rx_addr ra) {
    uint32_t a4[4] = {0x0093a394, 0x6c6d7862, 0, 1};   /* "MXWreckingRagdoll", bxml */
    rx_addr h = rr_vcall(rr_database(), 0x6c, 4, a4);
    rx_addr r = U32(h, 0x44) ? U32(h, 0x44) - 0x460 : 0;
    SU32(h, 0x4c, 1);
    RX_FRAME_ENTER(F, 0x300);
    rx_addr root = rr_vcall(r, 4, 0, NULL);
    rx_addr gp = X_CHILD(r, root, 0x0093a3a8);
    a_name(r, gp, 0x0093a3bc, d + 0x9a40);
    a_ints(r, gp, 0x0093a3c4, d + 0x390);
    a_floats(r, gp, 0x0093a3d0, d + 0x394);
    a_floats(r, gp, 0x0093a3e0, d + 0x3ec);
    a_floats(r, gp, 0x0093a3f8, d + 0x3f0);
    a_floats(r, gp, 0x0093a414, d + 0x3f4);
    a_floats(r, gp, 0x0093a434, d + 0xa5b0);
    rr_set4(d + 0x380, 0.0f, -32.2f /* [0x977688] */, 0.0f, 0.0f);
    rx_addr cur = F + 0x28, nbodies = F + 0x40;
    rx_addr rb = X_CHILD(r, root, 0x0093a43c);
    a_ints(r, rb, 0x0093a448, nbodies);
    rx_addr node = X_FIRST(r, rb, 0x0093a454, cur);
    rx_addr body_t = F + 0x90, state_t = F + 0x280;
    for (uint32_t o = 0; o < 0x70; o += 0x10) rr_set4(body_t + o, 0, 0, 0, 0);
    SU32(body_t, 0x70, 0); SU8(body_t, 0x74, 0);
    rx_rider_ik_rec_q_init(state_t);
    for (uint32_t k = 0; node; k++) {
        a_name(r, node, 0x0093a3bc, d + 0x9ac0 + 0x80 * k);
        a_floats(r, node, 0x0093a460, d + 0xa5b4 + 4 * k);
        a_vec3(r, node, 0x0093a470, d + 0xa610 + 0x10 * k);
        a_ints(r, node, 0x0093a484, F + 0x34);
        a_ints(r, node, 0x0093a490, F + 0x38);
        rx_ragdoll_add_body(d + 0x380, body_t, state_t, (int32_t)U32(F, 0x34), (int32_t)U32(F, 0x38));
        node = X_NEXT(r, U32(cur, 0));
    }
    X_RELEASE(r, U32(cur, 0));
    int32_t n = (int32_t)U32(nbodies, 0);
    float total = 0.0f;
    for (int32_t k = 0; k < n; k++) total = (float)((double)F32(d, 0xa5b4 + 4u * (uint32_t)k) + (double)total);
    for (int32_t k = 0; k < n; k++) SF32(d, 0xa5b4 + 4u * (uint32_t)k, (float)((double)F32(d, 0xa5b4 + 4u * (uint32_t)k) / (double)total));
    float m = (float)((double)F32(d, 0xa5b0) / 0x1.01999ap+5 /* [0x977e90] 32.2 */);
    for (int32_t k = 0; k < n; k++) {
        uint32_t uk = (uint32_t)k;
        float mk = (float)((double)m * (double)F32(d, 0xa5b4 + 4 * uk));
        rr_splat(d + 0x420 + 0x80 * uk, mk);
        float inv = (float)(1.0 / (double)mk);
        rr_splat(d + 0x430 + 0x80 * uk, inv);
        rx_addr vi = d + 0xa610 + 0x10 * uk;
        rx_addr I = d + 0xea0 + 0x70 * uk;
        for (uint32_t j = 0; j < 4; j++) SF32(I, 4 * j, rx_muls(mk, F32(vi, 4 * j)));
        for (uint32_t j = 0; j < 4; j++) SF32(I + 0x10, 4 * j, rx_divs(F32(0, 0x00d61650 + 4 * j), F32(I, 4 * j)));
    }
    SF32(d, 0x3f8, (float)((double)m / (double)(float)n));
    /* the pins */
    rx_addr pins = X_CHILD(r, root, 0x0093a4a0);
    a_ints(r, pins, 0x0093a4a8, F + 0x7c);
    node = X_FIRST(r, pins, 0x0093a4b0, cur);
    rx_addr pin = F + 0x110;
    rx_rider_joint_init(pin);
    while (node) {
        a_ints(r, node, 0x0093a484, F + 0x34);
        a_ints(r, node, 0x0093a490, F + 0x38);
        a_ints(r, node, 0x0093a4b4, pin);
        a_ints(r, node, 0x0093a4c4, pin + 0x20);
        a_vec3(r, node, 0x0093a4d4, pin + 0x10);
        a_vec3(r, node, 0x0093a4e8, pin + 0x30);
        a_vec4(r, node, 0x0093a4fc, F + 0x50);
        a_vec4(r, node, 0x0093a510, F + 0x60);
        a_floats(r, node, 0x0093a528, F + 0x48);
        a_quat(r, node, 0x0093a53c, pin + 0xb0);
        for (uint32_t j = 0; j < 0x20; j += 4) SU32(pin, 0xc0 + j, U32(F, 0x50 + j));
        rx_rider_joint_limits(pin);
        SF32(pin, 0xe0, (float)((double)F32(F, 0x48) * 0x1.921fb6p+1 /* [0x977528] */ / 180.0 /* [0x977690] */));
        rx_ragdoll_add_pin(d + 0x380, (int32_t)U32(F, 0x38), (int32_t)U32(F, 0x34), pin);
        node = X_NEXT(r, U32(cur, 0));
    }
    X_RELEASE(r, U32(cur, 0));
    SU32(h, 0x4c, 0);
    SU32(d, 0x37c, chr);
    for (int32_t k = 0; k < (int32_t)U32(d, 0x3fc); k++)
        SU32(d, 0xa540 + 4u * (uint32_t)k, (uint32_t)rx_rider_bone_index(U32(d, 0x37c), d + 0x9ac0 + 0x80u * (uint32_t)k));
    rx_ragdoll_finish(d + 0x380);
    /* the collision objects */
    rx_addr co = X_CHILD(r, root, 0x0093a550);
    a_ints(r, co, 0x0093a564, d + 0xa760);
    a_floats(r, co, 0x0093a578, d + 0xa868);
    a_floats(r, co, 0x0093a594, d + 0xa86c);
    a_floats(r, co, 0x0093a5b0, d + 0xa870);
    a_floats(r, co, 0x0093a5cc, d + 0xa874);
    a_ints(r, co, 0x0093a5e4, F + 0x3c);
    SU32(d, 0xa878, U32(F, 0x3c));
    a_ints(r, co, 0x0093a604, F + 0x3c);
    SU32(d, 0xa87c, U32(F, 0x3c));
    node = X_FIRST(r, co, 0x0093a624, cur);
    for (rx_addr slot = d + 0xa764; node; slot += 4) {
        a_name(r, node, 0x0093a634, F + 0x280);
        SU32(slot, 0, 0xffffffffu);
        for (int32_t j = 0; j < (int32_t)U32(d, 0x3fc); j++)
            if (crt_strcmp(d + 0x9ac0 + 0x80u * (uint32_t)j, F + 0x280) == 0) { SU32(slot, 0, (uint32_t)j); break; }
        a_floats(r, node, 0x00935e70, slot + 0x54);
        node = X_NEXT(r, U32(cur, 0));
    }
    X_RELEASE(r, U32(cur, 0));
    SU32(d, 0xa870, U32(0, 0x009c4750));
    rx_character_set_ragdoll_bones(U32(d, 0x37c), U32(d, 0x3fc), d + 0xa540);
    RX_FRAME_LEAVE(F, 0x300);
    rx_rider_bodies_create(d, ra);
}
