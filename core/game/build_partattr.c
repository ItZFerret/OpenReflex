/* build_partattr.c -- the partattr (ParticleAttrib) asset: ParticleAttribAssetIO::vf3 006940a0, the BXML parser FUN_00693870 and
 * its helpers (FUN_00691a50 / 00691a90 / 00691ad0 / 00691b10: the enum lookups; 00691b50: the emitter shape; 00694010: one 8-byte
 * trigger / modifier block; 00693f70: the record initializer), plus the destructor FUN_00693810 / sdtor 006937f0.
 * Hand transliteration from the disassembly; notes/fx.md "The partattr asset (partattr)".
 *
 * The reference asset (builds/gridsubscene/UI_Compound_Particles.partattr, extracted read-only with _re/formats/package_extract.py)
 * parses to 370 records; the event world's Nat_Concord_A has 171. The record layout (0xa0 bytes) is the one fx_worldparticles.c
 * uses: +0 name, +4 EmitterShape, +8 ParticleZone, +0xc distance zones, +0x10 Offset, +0x20 VehicleType, +0x24 EngineClass,
 * +0x28 Tire, +0x2c body flag, +0x30 location, +0x34 .. +0x8c the trigger / modifier blocks. */
#include "build_partattr.h"
#include "build_alloc.h"
#include "build_xml.h"
#include "crt.h"
#include <stdio.h>
#include <string.h>

#define PARTATTR_VT 0x0095defcu            /* ParticleAttribAsset::vftable */
#define IASSET_VT   0x0093eb78u            /* IAsset::vftable (the destructor leaves it) */

/* 00693f70 (ECX = the record): the fields the record initializer clears. It does NOT touch +0 (the name, written by the parser),
 * +4 / +8 / +0xc..+0xf (shape / zone / distance flags), +0x10..+0x1f (the offset, written or zeroed), +0x20..+0x30 (the type
 * fields) nor the padding +0x92..+0x9f, which stay as the heap gave them. */
static void rec_init(rx_addr r) {
    SF32(r, 0x34, 0.0f); SU8(r, 0x38, 0); SU8(r, 0x39, 0);
    SF32(r, 0x3c, 0.0f); SU8(r, 0x40, 0); SU8(r, 0x41, 0);
    SF32(r, 0x44, 0.0f); SU8(r, 0x48, 0); SU8(r, 0x49, 0);
    SF32(r, 0x4c, 0.0f); SU8(r, 0x50, 0); SU8(r, 0x51, 0);
    SF32(r, 0x54, 0.0f); SU8(r, 0x58, 0); SU8(r, 0x59, 0);
    SU32(r, 0x5c, 0);    SU8(r, 0x60, 0);
    SU32(r, 0x64, 0);    SU8(r, 0x68, 0);
    SF32(r, 0x6c, 0.0f); SU8(r, 0x70, 0); SU8(r, 0x71, 0);
    SF32(r, 0x74, 0.0f); SU8(r, 0x78, 0); SU8(r, 0x79, 0);
    SF32(r, 0x7c, 0.0f); SU8(r, 0x80, 0); SU8(r, 0x81, 0);
    SF32(r, 0x84, 0.0f); SU8(r, 0x88, 0); SU8(r, 0x89, 0);
    SF32(r, 0x8c, 0.0f); SU8(r, 0x90, 0); SU8(r, 0x91, 0);
}

/* the string of a type-1 attribute: an index into the reader's string pointer table (+0xc), as the original reads it */
static rx_addr attr_str(rx_addr rd, rx_addr e) {
    if (!e || U16(e, 0xa) != 1) return 0;              /* unreachable for the shipped assets (the original dereferences it) */
    return U32(U32(rd, 0xc), 4u * U32(e, 4));
}

/* the enum lookups FUN_00691a50 (VehicleType, 8 names), 00691a90 (EngineClass, 4), 00691ad0 (Tire, 6), 00691b10 (Tire on a bike, 8),
 * 00691b50 (EmitterShape, 3): _stricmp against the .rdata name table; the first match's index, else the table size. */
static uint32_t enum_index(rx_addr name, rx_addr table, uint32_t n) {
    if (!name) return n;                               /* the original would crash on a missing attribute */
    for (uint32_t i = 0; i < n; i++) if (crt_stricmp(name, U32(0, table + 4u * i)) == 0) return i;
    return n;
}

/* FUN_00694010 (ECX = the block name, EDX = the parent node, ESI = the reader; the block on the stack): a trigger block is the
 * child node's own float value at +0, Inverse at +4, Enabled at +5. */
static void trigger_block(rx_addr rd, rx_addr parent, const char* name, rx_addr blk, rx_addr scratch) {
    const rx_addr c = rx_xml_find_child(rd, parent, name);
    if (!c) return;
    rx_addr e = rx_xml_find_attr(rd, c, "Enabled");
    if (e) { SU32(scratch, 0, 0); rx_xml_attr_bools(rd, e, scratch, 1); SU8(blk, 5, U32(scratch, 0) == 1); }
    e = rx_xml_find_attr(rd, c, "Inverse");
    if (e) { SU32(scratch, 0, 0); rx_xml_attr_bools(rd, e, scratch, 1); SU8(blk, 4, U32(scratch, 0) == 1); }
    rx_xml_node_floats(rd, c, blk, 1);                 /* vf51 */
}

/* the TireInAir / TireInWater blocks: the child's own bool value at +0 (vf57), Enabled at +4. */
static void air_block(rx_addr rd, rx_addr parent, const char* name, rx_addr blk, rx_addr scratch) {
    const rx_addr c = rx_xml_find_child(rd, parent, name);
    if (!c) return;
    rx_addr e = rx_xml_find_attr(rd, c, "Enabled");
    if (e) { SU32(scratch, 0, 0); rx_xml_attr_bools(rd, e, scratch, 1); SU8(blk, 4, U32(scratch, 0) == 1); }
    rx_xml_node_bools(rd, c, blk, 1);                  /* vf57 */
}

/* FUN_00693870 (ECX = the stream, stack: the asset): load the BXML, count the "Emitter" children, allocate and initialise the
 * 0xa0-byte records, fill each from its node, then release the reader. */
void rx_particle_attrib_asset_parse(rx_addr a, rx_stream* s) {
    RX_FRAME_ENTER(R, 0x480);                          /* the XMLReader (0x460) + a 4-byte scratch */
    const rx_addr rd = R;
    const rx_addr scratch = R + 0x470;
    rx_xml_reader_construct(rd);
    rx_xml_reader_load(rd, s);
    const rx_addr root = rx_xml_root(rd);
    if (root != 0) {
        uint32_t n = 0;
        for (rx_addr c = rx_xml_find_child(rd, root, "Emitter"); c; c = rx_xml_next_child(rd, c)) n++;
        SU32(a, 0x10, n);
        rx_addr recs = rx_reflex_aligned_alloc(rx_mul_sat(n, 0xa0));
        if (recs) for (uint32_t i = 0; i < n; i++) rec_init(recs + 0xa0u * i);
        SU32(a, 0x14, recs);

        uint32_t idx = 0;
        for (rx_addr node = rx_xml_find_child(rd, root, "Emitter"); node; node = rx_xml_next_child(rd, node)) {
            const rx_addr rec = U32(a, 0x14) + 0xa0u * idx;
            idx++;
            /* the name: the attribute's string copied into its own ReflexAlignedAlloc block (strlen + 1) */
            {
                const rx_addr nm = attr_str(rd, rx_xml_find_attr(rd, node, "Name"));
                uint32_t len = 0;
                if (nm) while (U8(nm, len)) len++;
                const rx_addr p = rx_reflex_aligned_alloc(len + 1);
                SU32(rec, 0, p);
                if (p) for (uint32_t i = 0; i <= len; i++) SU8(p, i, nm ? U8(nm, i) : 0);
            }
            SU32(rec, 0x20, enum_index(attr_str(rd, rx_xml_find_attr(rd, node, "VehicleType")), 0x009d8b20u, 8));
            SU32(rec, 0x24, enum_index(attr_str(rd, rx_xml_find_attr(rd, node, "EngineClass")), 0x009d8b40u, 4));
            SU32(rec, 4, 0);
            {
                const rx_addr e = rx_xml_find_attr(rd, node, "EmitterShape");
                if (e) SU32(rec, 4, enum_index(attr_str(rd, e), 0x009d8b88u, 3));
            }
            SU32(rec, 8, 0);
            {
                const rx_addr e = rx_xml_find_attr(rd, node, "ParticleZone");
                if (e) rx_xml_attr_uints(rd, e, rec + 8, 1);
            }
            /* the four distance-zone bytes: DistanceZonesCount present -> (count <= 0), absent -> 1 */
            uint32_t nz = 0;
            uint8_t zone = 1;
            {
                const rx_addr e = rx_xml_find_attr(rd, node, "DistanceZonesCount");
                if (e) {
                    SU32(scratch, 0, 0);
                    rx_xml_attr_ints(rd, e, scratch, 1);
                    nz = U32(scratch, 0);
                    zone = ((int32_t)nz <= 0);
                }
            }
            for (uint32_t k = 0; k < 4; k++) SU8(rec, 0xc + k, zone);
            {
                const rx_addr e = rx_xml_find_attr(rd, node, "DistanceZones");
                if (e && (int32_t)nz > 0) {
                    const rx_addr buf = rx_reflex_aligned_alloc(rx_mul_sat(nz, 4));   /* the original's _aligned_free under the heap lock */
                    rx_xml_attr_uints(rd, e, buf, (int)nz);
                    for (uint32_t i = 0; i < nz; i++) SU8(rec, 0xb + U32(buf, 4u * i), 1);
                    rx_reflex_aligned_free(buf);
                }
            }
            /* Tire / the body flag / the location: bikes (VehicleType 0) read the 6-name table, the others the 8-name one */
            {
                const rx_addr tire = attr_str(rd, rx_xml_find_attr(rd, node, "Tire"));
                uint32_t v;
                if (U32(rec, 0x20) == 0) {
                    v = enum_index(tire, 0x009d8b50u, 6);
                    SU32(rec, 0x28, v);
                    SU8(rec, 0x2c, v == 2);
                    if (v == 4) { SU32(rec, 0x30, 2); SU8(rec, 0x2c, 1); }
                    else if (v == 5) { SU32(rec, 0x30, 3); SU8(rec, 0x2c, 1); }
                    else if (v == 3) { SU32(rec, 0x30, 1); SU8(rec, 0x2c, 1); }
                    else SU32(rec, 0x30, 0);
                } else {
                    v = enum_index(tire, 0x009d8b68u, 8);
                    SU32(rec, 0x28, v);
                    SU8(rec, 0x2c, v == 4);
                    if (v == 6) { SU32(rec, 0x30, 2); SU8(rec, 0x2c, 1); }
                    else if (v == 7) { SU32(rec, 0x30, 3); SU8(rec, 0x2c, 1); }
                    else if (v == 5) { SU32(rec, 0x30, 1); SU8(rec, 0x2c, 1); }
                    else SU32(rec, 0x30, 0);
                }
            }
            /* Offset: a vec3 (the 16-byte-stride writer leaves lane 3 untouched), or three zeros */
            {
                const rx_addr e = rx_xml_find_child(rd, node, "Offset");
                if (e) rx_xml_node_vec3(rd, e, rec + 0x10, 1);
                else { SF32(rec, 0x10, 0.0f); SF32(rec, 0x14, 0.0f); SF32(rec, 0x18, 0.0f); }
            }
            /* Triggers: the five threshold blocks, then TireInAir / TireInWater; an enabled TireInWater sets the zone to -1 */
            {
                const rx_addr e = rx_xml_find_child(rd, node, "Triggers");
                if (e) {
                    trigger_block(rd, e, "TireRollSlip", rec + 0x34, scratch);
                    trigger_block(rd, e, "TireSideSlip", rec + 0x3c, scratch);
                    trigger_block(rd, e, "VehicleVelocity", rec + 0x44, scratch);
                    trigger_block(rd, e, "SuspensionForce", rec + 0x4c, scratch);
                    trigger_block(rd, e, "TireRotationSpeed", rec + 0x54, scratch);
                    air_block(rd, e, "TireInAir", rec + 0x5c, scratch);
                    air_block(rd, e, "TireInWater", rec + 0x64, scratch);
                }
            }
            if (U8(rec, 0x68) != 0) SU32(rec, 8, 0xffffffffu);
            /* EmitModifiers: the same five blocks at +0x6c .. +0x8c */
            {
                const rx_addr e = rx_xml_find_child(rd, node, "EmitModifiers");
                if (e) {
                    trigger_block(rd, e, "TireRollSlip", rec + 0x6c, scratch);
                    trigger_block(rd, e, "TireSideSlip", rec + 0x74, scratch);
                    trigger_block(rd, e, "VehicleVelocity", rec + 0x7c, scratch);
                    trigger_block(rd, e, "SuspensionForce", rec + 0x84, scratch);
                    trigger_block(rd, e, "TireRotationSpeed", rec + 0x8c, scratch);
                }
            }
        }
    }
    rx_xml_reader_destroy(rd);
    RX_FRAME_LEAVE(R, 0x480);
}

/* ParticleAttribAssetIO::vf3 006940a0 (stack: stream, name, tag lo, tag hi, out; RET 0x14): ReflexAlignedAlloc(0x18), the vtable
 * and four zero words, then the parser. */
rx_addr rx_particle_attrib_asset_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x18);
    if (!a) return 0;
    SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0x10, 0); SU32(a, 0x14, 0);
    SU32(a, 0, PARTATTR_VT);
    rx_particle_attrib_asset_parse(a, s);
    return a;
}

/* FUN_00693810 (ECX = the asset): the name strings and the record array released, then the IAsset vtable. */
void rx_particle_attrib_asset_destroy(rx_addr a) {
    SU32(a, 0, PARTATTR_VT);
    if (U32(a, 0x14) != 0) {
        for (uint32_t i = 0; i < U32(a, 0x10); i++) {
            const rx_addr nm = U32(U32(a, 0x14) + 0xa0u * i, 0);
            if (nm) rx_reflex_aligned_free(nm);
        }
        rx_reflex_aligned_free(U32(a, 0x14));
        SU32(a, 0x14, 0); SU32(a, 0x10, 0);
    }
    SU32(a, 0, IASSET_VT);
}
RX_FUNCTION(0x00693810, rx_particle_attrib_asset_destroy)

/* ParticleAttribAsset::sdtor 006937f0 (thiscall: flags; RET 4, returns the asset): the destroy, then the block itself for flag bit 0 */
rx_addr rx_particle_attrib_asset_sdtor(rx_addr a, uint32_t flags) {
    rx_particle_attrib_asset_destroy(a);
    if (flags & 1) rx_reflex_aligned_free(a);
    return a;
}
RX_FUNCTION(0x006937f0, rx_particle_attrib_asset_sdtor)
