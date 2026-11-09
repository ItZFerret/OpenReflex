/* build_rider_model.c -- construction unit 7 (riders): the entity's model setup (006b5de0: the hierarchy model copied into the
 * entity's embedded ModelAsset 007d4d60, the model instance and the Entity description 0041c700, the animation tree 0082dd00, the
 * Character bound to the node set 0082e560) and the node set copy with the extra root node (NodeSet init 007ddb40 with the flag).
 * Hand transliterations of the disassembly. */
#include "build_rider.h"
#include "build_rider_util.h"
#include "build_scene_entity.h"
#include "crt.h"
#include "anim.h"

static uint32_t a16(uint32_t v) { return (v + 0xf) & ~0xfu; }

/* 007dd540 (EDX = source set, s0 flag): the block size of a set laid out like the source (+1 node with the flag) */
static uint32_t nodeset_block_size(rx_addr src, uint32_t flag) {
    uint32_t n = ((uint32_t)U16(src, 0x38) | (flag != 0)) + U32(src, 0xc);
    uint32_t ex = (int32_t)n > 1 ? a16(n << 6) * 2 : 0;
    uint32_t m = a16(n << 6) + n * 8;
    return a16(n * 0x50) + m * 2 + U32(src, 0x14) + ex;
}
/* 007ddb40 (ECX = destination, EDX = source; s0 flag; RET 4): NodeSet init from another set: the 0x80-aligned block (0078fb40), the
 * matrices / bounds / names copied, the records re-linked; with the flag an extra root record (index = count) above the source's root,
 * its local / world matrices the identity (0x00d61600) and zero bounds */
void rx_nodeset_init(rx_addr d, rx_addr src, uint32_t flag) {
    uint32_t N = ((uint32_t)U16(src, 0x38) | (flag != 0)) + U32(src, 0xc);
    uint32_t nsrc = U32(src, 0xc) + U16(src, 0x38);
    if (U32(d, 0xc) != 0) rr_fatal("NodeSet init on a used set (007dd890) not ported", d);
    {
        uint32_t total = a16(nodeset_block_size(src, flag));
        rx_addr blk = rx_crt_aligned_malloc(total, 0x80);   /* 0078fb40 */
        SU16(d, 0x3a, (uint16_t)total);
        SU32(d, 0x3c, blk);
        uint32_t mb = a16(N << 6);
        if ((int32_t)U32(src, 0xc) > 1) { SU32(d, 0x28, blk); blk += mb; SU32(d, 0x24, blk); blk += mb; }
        SU32(d, 0x18, blk); blk += mb;
        SU32(d, 0x1c, blk); blk += mb;
        SU32(d, 0x20, blk); blk += a16(N * 0x50);
        SU32(d, 8, blk);
        SU32(d, 0x14, U32(src, 0x14));
        SU32(d, 0x10, blk + (N << 4));
    }
    SU32(d, 0xc, U32(src, 0xc));
    SU16(d, 0x38, (uint16_t)((flag != 0) | U16(src, 0x38)));
    crt_memcpy(U32(d, 0x18), U32(src, 0x18), nsrc << 6);
    crt_memcpy(U32(d, 0x1c), U32(src, 0x1c), nsrc << 6);
    crt_memcpy(U32(d, 0x20), U32(src, 0x20), nsrc * 0x50);
    crt_memcpy(U32(d, 0x10), U32(src, 0x10), U32(d, 0x14));
    for (uint32_t i = 0; (int32_t)i < (int32_t)U32(d, 0xc); i++) {
        rx_addr r = U32(d, 8) + 16 * i;
        SU8(r, 0xf, 0xff); SU8(r, 0xd, 0xff); SU8(r, 0xe, 0xff); SU16(r, 8, 0xffff); SU32(r, 0, 0); SU8(r, 0xc, 0);
    }
    if ((int32_t)U32(d, 0xc) > 1) {
        if (U32(src, 0x24)) crt_memcpy(U32(d, 0x24), U32(src, 0x24), nsrc << 6);
        if (U32(src, 0x28)) crt_memcpy(U32(d, 0x28), U32(src, 0x28), nsrc << 6);
    }
    SU32(d, 0, U32(src, 0) ? (((int32_t)(U32(src, 0) - U32(src, 8)) >> 4) << 4) + U32(d, 8) : 0);
    SU32(d, 4, U32(src, 4) ? (((int32_t)(U32(src, 4) - U32(src, 8)) >> 4) << 4) + U32(d, 8) : 0);
    for (uint32_t i = 0; (int32_t)i < (int32_t)nsrc; i++) {
        uint32_t o = 16 * i;
        SU32(U32(d, 8) + o, 0, d);
        SU8(U32(d, 8) + o, 0xc, U8(U32(src, 8) + o, 0xc));
        SU8(U32(d, 8) + o, 0xb, (uint8_t)i);
        static const uint32_t link[3] = {0xf, 0xd, 0xe};
        for (int k = 0; k < 3; k++) {
            rx_addr sr = U32(src, 8) + o, dr = U32(d, 8) + o;
            uint8_t b = U8(sr, link[k]);
            if (b == 0xff) { SU8(dr, link[k], 0xff); continue; }
            /* ((b << 4) + [sr.set + 8] - src records) >> 4 << 4, - [dr.set + 8] + dst records, >> 4 */
            int32_t v = (int32_t)((((uint32_t)b << 4) + U32(U32(sr, 0), 8) - U32(src, 8)) >> 0);
            v = (v >> 4) << 4;
            uint32_t w = (uint32_t)v - U32(U32(dr, 0), 8) + U32(d, 8);
            SU8(dr, link[k], (uint8_t)((int32_t)w >> 4));
        }
        rx_addr sr = U32(src, 8) + o, dr = U32(d, 8) + o;
        if (U16(sr, 8) != 0xffff) {
            uint16_t bx = (uint16_t)U16(U32(sr, 0), 0x10);
            bx = (uint16_t)(bx - (uint16_t)U16(U32(dr, 0), 0x10));
            bx = (uint16_t)(bx + U16(sr, 8));
            bx = (uint16_t)(bx - (uint16_t)U16(src, 0x10));
            bx = (uint16_t)(bx + (uint16_t)U16(d, 0x10));
            SU16(dr, 8, bx);
        } else SU16(dr, 8, 0xffff);
    }
    if (flag) {
        if (U16(src, 0x38) == 0) {
            rx_addr old = U32(d, 0);
            rx_addr nr = (U32(d, 0xc) << 4) + U32(d, 8);
            SU32(d, 0, nr);
            SU8(nr, 0xf, 0xff); SU8(nr, 0xd, 0xff); SU8(nr, 0xe, 0xff);
            SU32(nr, 0, 0); SU8(nr, 0xc, 0); SU16(nr, 8, 0xffff);
            SU32(U32(d, 0), 0, d);
            SU8(U32(d, 0), 0xb, (uint8_t)U32(d, 0xc));
            SU8(old, 0xf, (uint8_t)((int32_t)(U32(d, 0) - U32(U32(old, 0), 8)) >> 4));
            rx_addr root = U32(d, 0);
            SU8(root, 0xe, (uint8_t)((int32_t)(old - U32(U32(root, 0), 8)) >> 4));
        }
        rx_addr root = U32(d, 0);
        rx_addr set = U32(root, 0);
        rx_addr lm = U32(set, 0x18) + ((uint32_t)U8(root, 0xb) << 6);
        for (uint32_t k = 0; k < 64; k += 4) SU32(lm, k, U32(0, 0x00d61600 + k));
        rx_addr wm = U32(set, 0x1c) + ((uint32_t)U8(root, 0xb) << 6);
        for (uint32_t k = 0; k < 64; k += 4) SU32(wm, k, U32(0, 0x00d61600 + k));
        rx_addr bd = U32(d, 0x20) + nsrc * 0x50;
        rr_v4(bd + 0x20, RR_RT_ZERO);
        rr_v4(bd + 0x30, RR_RT_ZERO);
        SF32(bd, 0x44, 0);
        rr_v4(bd, RR_RT_ZERO);
        rr_v4(bd + 0x10, RR_RT_ZERO);
        SF32(bd, 0x40, 0);
    }
    SU32(d, 0x34, 0);
}

/* 007f92d0 the LOD vector assignment: build_rider_gear.c (with the LOD vector's other operations) */

/* 007d4d60 (ESI = destination model, s0 source; RET 4): ModelAsset copy: both node sets (the second with the extra root too), the
 * root bounds evaluated, the name / flags / draw distance, the LODs, the surface handle */
void rx_model_asset_copy(rx_addr dst, rx_addr src) {
    if ((int32_t)(U32(dst, 0x134) - U32(dst, 0x130)) / 0x1c != 0) rr_fatal("model copy over LODs (007d4c70) not ported", dst);
    rx_nodeset_init(dst + 0x10, src + 0x10, 1);
    RX_FRAME_ENTER(F, 0x40);
    rx_node_subtree_bounds(U32(dst, 0x10), F + 0x18, F + 8, F + 4);
    RX_FRAME_LEAVE(F, 0x40);
    rx_nodeset_init(dst + 0x50, src + 0x10, 1);
    SU32(dst, 0x94, U32(src, 0x94));
    crt_strncpy(dst + 0x98, src + 0x98, 0x7f);
    SU8(dst + 0x98, 0x7f, 0);
    SU32(dst, 0x118, U32(src, 0x118));
    SU32(dst, 0x120, U32(src, 0x120));
    SU32(dst, 0x13c, U32(src, 0x13c));
    rx_model_lods_assign(dst + 0x124, src + 0x124);
}
RX_FUNCTION(0x007d4d60, rx_model_asset_copy)

/* 0080ddf0 Entity's base description and 007f1810 ModelInstance set-model: build_scene_entity.c */
static void entity_describe(rx_addr e, rx_addr desc) { rx_entity_describe(e, desc); }   /* 0080ddf0 */
static void instance_set_model(rx_addr inst, rx_addr handle, uint32_t flags, rx_addr overrides) {
#ifdef RX_ORACLE
    rx_regs in, out; rr_regs_clear(&in);
    in.r[0] = handle; in.r[1] = flags;
    uint32_t a[2] = {inst, overrides};
    rx_call_original_ex(0x007f1810, &in, &out, 2, a);   /* the oracle runs the original under its adapter (rx_build_rider) */
#else
    rx_model_instance_set_model(inst, handle, flags, overrides);   /* 007f1810 */
#endif
}
/* 0041c700 (EAX = scene, ECX = p; s0 entity, s1 model handle, s2 flags; RET 0xc): the model instance (+0xa0), the Entity description
 * {scene, p, 0}, the matrix copied to +0xc0 */
void rx_entity_attach_model(rx_addr scene, uint32_t p, rx_addr e, rx_addr handle, uint32_t flags) {
    RX_FRAME_ENTER(F, 0x10);
    SU32(F, 0, scene); SU32(F, 4, p); SU32(F, 8, 0);
    instance_set_model(e + 0xa0, handle, flags, 0);
    entity_describe(e, F);
    rr_copy(e + 0xc0, e + 0x30, 0x40);
    RX_FRAME_LEAVE(F, 0x10);
}

/* 006b5de0 (EDI = the hierarchy handle; stdcall entity, scene, tree handle; RET 0xc): the model copy, the instance, the tree document
 * (+0x140) and its tree, vf29 (the node set: AnimationEntity::vf29 = +0x180) bound to the Character */
void rx_rider_entity_set_model(rx_addr hier, rx_addr e, rx_addr scene, rx_addr tree) {
    SU32(hier, 0x4c, 1);
    rx_model_asset_copy(e + 0x170, U32(hier, 0x44));
    SU32(hier, 0x4c, 0);
    rx_entity_attach_model(scene, 0, e, hier, 2);
    SU32(e, 0x140, tree);
    if (tree) {
        rx_animation_entity_load_tree(e);
        rx_addr ns = rr_vcall(e, 0x74, 0, NULL);
        rx_character_bind_hierarchy(U32(e, 0x144), ns);
    }
}
