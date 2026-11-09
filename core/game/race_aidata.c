/* race_aidata.c -- the AIData tables: 0051e350 and what it loads (see race_aidata.h).
 *
 * The original init thread 0075ed60 loads "Database\AIData" and (after the package) calls 0051e350 at 0075f01a. 0051e350 asks
 * the engine's Database (engine + 8) for three BXML handles (tag "bxml" = 0x6c6d7862) and loads them:
 *   AIBehavior        -> 0051eac0: rows named NavigateTrack .. SuckRacing; each row's <...Values> children fill the
 *                        behaviour tuning tables (0x9e3xxx .. 0xa04xxx) through the behaviour vf1s / value readers.
 *   AIAbility         -> 0051e430: rows <DriverType_%s> (13 driver types) x <DriverAbility_%s> (17 abilities) x four tier
 *                        rows (Pro/Hard/Intermediate/Beginner) x Tier1/2/3 -> the 0xc7c-byte personality row of each type
 *                        (memcpy target 0x00a89d08, size 0xa24c = 13 * 0xc7c; ability block 0xbc, difficulty slot 8 bytes).
 *   AIAbilityOverride -> 0051e7b0 (optional): Player01..Player11 of each ability override the default explicit slots.
 * 0051e1d0 fills an ability's explicit slots with the tier values (called for slots 0..10 after each ability's tier rows).
 * The tables are read by the frame code: rx_driver_start_chance (0x00a89d68), race_ai_params.c (block + 8*index, +0x60) and
 * the behaviours' tuning arrays (0x9e...) through race_driver_ai.c / race_beh*.c.
 *
 * Hand transliteration from the disassembly. The XMLReader attribute cursor the loaders use (vf59 / vf60 / vf61) is local to
 * this file (the AIData loaders are its only caller); the reader's other virtuals are build_xml.c's ports. */
#include "race_aidata.h"
#include "build_asset.h"
#include "build_xml.h"
#include "crt.h"
#include "world_misc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the .rdata strings (addresses the original passes; never copied to host strings) */
#define S_AI_BEHAVIOR     0x0093f108u
#define S_AI_ABILITY      0x0093f114u
#define S_AI_OVERRIDE     0x0093f120u
#define S_DRIVERTYPE      0x0093f134u
#define S_DRIVERABILITY   0x0093f144u
#define S_TIER1           0x0093f158u
#define S_GROUPTYPE       0x0093f160u
#define S_VECTOR          0x0093f16cu
#define S_TIER2           0x0093f174u
#define S_TIER3           0x0093f17cu
#define S_PLAYER          0x0093f184u
#define TAG_BXML          0x6c6d7862u

/* =============================================================================================================================
 * XMLReader::vf59 / vf60 / vf61 00828b50 / 00828c00 / 00828c30: the nested attribute cursor (the reader's second pool, separate
 * from vf8's child cursors): top +0x180 (initial 0x14), free slots +0x40 + 4 * top, entries +0x90 + 12 * slot {node, index, name}.
 * vf59(node, name, &cursor) allocates a slot and advances to the first attribute of `name` (the first attribute for name 0) and
 * returns the attribute entry (or 0); vf60(cursor|0x80000000) frees it; vf61(cursor) advances (the original's vtable +0xf4). */
static uint32_t xml_attr_advance(rx_addr r, uint32_t slot) {          /* vf61 00828c30 */
    slot &= 0x7fffffffu;
    rx_addr cur = r + 12u * slot;
    rx_addr node = U32(cur, 0x90);
    int32_t n = (int32_t)U32(node, 0x20);
    int32_t i = (int32_t)U32(cur, 0x94);
    if (i >= n - 1) { SU32(cur, 0x94, (uint32_t)n); return 0; }
    i++;
    uint32_t name = U32(cur, 0x98);
    if (name != 0xffffffffu) {
        if (i < n) {
            rx_addr e = U32(node, 0x1c) + 12u * (uint32_t)i;
            for (; i < n; i++, e += 12) if (U32(e, 0) == name) break;
        }
        if (i >= n) { SU32(cur, 0x94, (uint32_t)i); return 0; }
    }
    SU32(cur, 0x94, (uint32_t)i);
    return U32(node, 0x1c) + 12u * (uint32_t)i;
}

/* the string-table lookup 007fa030 over the reader's table +0x10: key is the address of the string pointer's slot */
static int xml_string_index(rx_addr r, rx_addr name, uint32_t* out_index) {
    RX_FRAME_ENTER(F, 8);
    SU32(F, 0, name); SU32(F, 4, 0);
    uint32_t found = rx_world_string_table_find(U32(r, 0x10), F, F + 4);
    *out_index = U32(F, 4);
    RX_FRAME_LEAVE(F, 8);
    return found != 0;
}

/* vf59 00828b50 (node, name, out*; RET 0xc): the attribute entry */
static rx_addr xml_attr_first(rx_addr r, rx_addr node, rx_addr name, rx_addr out) {
    uint32_t top = U32(r, 0x180);
    uint32_t slot = U32(r, 0x3c + 4u * top);      /* the free stack at +0x40, one below top */
    rx_addr cur = r + 12u * slot;
    SU32(cur, 0x90, node);
    SU32(cur, 0x94, 0xffffffffu);
    uint32_t key = 0xffffffffu;
    if (name) {
        uint32_t idx;
        if (!xml_string_index(r, name, &idx)) {
            SU32(cur, 0x90, 0);
            SU32(out, 0, 0xffffffffu);
            return 0;
        }
        key = idx;
    }
    SU32(cur, 0x98, key);
    rx_addr e = xml_attr_advance(r, slot);
    if (!e) {
        SU32(cur, 0x90, 0);
        SU32(out, 0, 0xffffffffu);
        return 0;
    }
    SU32(r, 0x180, top - 1u);
    SU32(out, 0, slot | 0x80000000u);
    return e;
}

/* vf60 00828c00 (cursor; RET 4) */
static void xml_attr_release(rx_addr r, uint32_t cursor) {
    if (cursor == 0xffffffffu) return;
    uint32_t slot = cursor & 0x7fffffffu;
    SU32(r + 12u * slot, 0x90, 0);
    uint32_t top = U32(r, 0x180);
    SU32(r, 0x40 + 4u * top, slot);
    SU32(r, 0x180, top + 1u);
}

/* =============================================================================================================================
 * 0051e350: the loader. The engine's Database is [engine + 8] (engine 0x00a95844). The handle's asset + 0x44 is the BXMLAsset;
 * the XMLReader is asset - 0x460 (build_xml.c's BXMLAsset). +0x4c is set while the asset is in use. */
void rx_aidata_load(void) {
    const rx_addr db = U32(U32(0, 0x00a95844u), 8);
    rx_addr h = rx_database_vf_handle(db, S_AI_BEHAVIOR, TAG_BXML, 0, 1);
    SU32(h, 0x4c, 1);
    rx_addr asset = U32(h, 0x44);
    rx_aidata_behaviors(asset ? asset - 0x460u : 0);
    SU32(h, 0x4c, 0);

    h = rx_database_vf_handle(db, S_AI_ABILITY, TAG_BXML, 0, 1);
    SU32(h, 0x4c, 1);
    asset = U32(h, 0x44);
    rx_aidata_abilities(asset ? asset - 0x460u : 0);
    SU32(h, 0x4c, 0);

    h = rx_database_vf_handle(db, S_AI_OVERRIDE, TAG_BXML, 0, 0);
    if (!h) return;
    SU32(h, 0x4c, 1);
    asset = U32(h, 0x44);
    rx_aidata_overrides(asset ? asset - 0x460u : 0);
    SU32(h, 0x4c, 0);
}
RX_FUNCTION(0x0051e350, rx_aidata_load)

/* =============================================================================================================================
 * 0051e1d0 (EAX = the driver type, ECX = the PlayerNN slot): copies each ability's first difficulty value into its explicit
 * slot. The original is a compiler-strength-reduced loop over 0, 8, 16 of the 17 abilities; transliterated as it stands
 * (including the high-word copies of the *4th* ability of each chunk, condition `index + k == 3`). */
void rx_aidata_ability_defaults(uint32_t type, uint32_t slot) {
    const uint32_t base = type * 0x11u;
    uint32_t i = 0;
    do {
        uint32_t a = (base + i) * 0xbcu;
        SF32(0, 0xa89d68u + a + slot * 8u, F32(0, 0xa89d08u + a));
        if (i == 3) SF32(0, 0xa89d6cu + a + slot * 8u, F32(0, 0xa89d0cu + a));
        SF32(0, 0xa89e24u + a + slot * 8u, F32(0, 0xa89dc4u + a));
        if (i + 1u == 3) SF32(0, 0xa89e28u + a + slot * 8u, F32(0, 0xa89dc8u + a));
        SF32(0, 0xa89ee0u + a + slot * 8u, F32(0, 0xa89e80u + a));
        if (i + 2u == 3) SF32(0, 0xa89ee4u + a + slot * 8u, F32(0, 0xa89e84u + a));
        SF32(0, 0xa89f9cu + a + slot * 8u, F32(0, 0xa89f3cu + a));
        if (i + 3u == 3) SF32(0, 0xa89fa0u + a + slot * 8u, F32(0, 0xa89f40u + a));
        SF32(0, 0xa8a058u + a + slot * 8u, F32(0, 0xa89ff8u + a));
        if (i + 4u == 3) SF32(0, 0xa8a05cu + a + slot * 8u, F32(0, 0xa89ffcu + a));
        SF32(0, 0xa8a114u + a + slot * 8u, F32(0, 0xa8a0b4u + a));
        if (i + 5u == 3) SF32(0, 0xa8a118u + a + slot * 8u, F32(0, 0xa8a0b8u + a));
        SF32(0, 0xa8a1d0u + a + slot * 8u, F32(0, 0xa8a170u + a));
        if (i + 6u == 3) SF32(0, 0xa8a1d4u + a + slot * 8u, F32(0, 0xa8a174u + a));
        SF32(0, 0xa8a28cu + a + slot * 8u, F32(0, 0xa8a22cu + a));
        if (i + 7u == 3) SF32(0, 0xa8a290u + a + slot * 8u, F32(0, 0xa8a230u + a));
        i += 8u;
    } while (i < 0xau);
    for (; i < 0x11u; i++) {
        uint32_t a = (base + i) * 0xbcu;
        SF32(0, 0xa89d68u + a + slot * 8u, F32(0, 0xa89d08u + a));
        if (i == 3) SF32(0, 0xa89d6cu + a + slot * 8u, F32(0, 0xa89d0cu + a));
    }
}RX_FUNCTION(0x0051e1d0, rx_aidata_ability_defaults)

/* =============================================================================================================================
 * The behaviour rows (0051eac0 and the vf1s). A vf1 iterates the children of its row, compares the child's name with the value
 * group name and calls the group's value reader (EDX = the child, ESI = the reader). */
typedef struct { uint32_t name; void (*fn)(rx_addr, rx_addr); } aidata_group;
static void aidata_dispatch(rx_addr r, rx_addr node, const aidata_group* g, int n) {
    RX_FRAME_ENTER(F, 0x108);
    crt_memset(F + 1, 0, 0x103); SU8(F, 0, 0);
    rx_addr child = rx_xml_vf_first(r, node, 0, F + 0x104);
    while (child) {
        rx_xml_vf_node_name(r, child, F, 0x104);
        for (int i = 0; i < n; i++) if (crt_stricmp(F, g[i].name) == 0) g[i].fn(r, child);
        child = rx_xml_vf_next(r, U32(F + 0x104, 0));
    }
    rx_xml_vf_release(r, U32(F + 0x104, 0));
    RX_FRAME_LEAVE(F, 0x108);
}

/* the original's strstr(grouptype, "vector") over guest memory */
static int guest_has_vector(rx_addr s) {
    static const char v[] = "vector";
    for (uint32_t i = 0;; i++) {
        uint8_t c = U8(s, i);
        if (!c) return 0;
        uint32_t k = 0;
        while (v[k] && U8(s, i + k) == (uint8_t)v[k]) k++;
        if (!v[k]) return 1;
    }
}

/* a field of a leaf: vf7 child by name, vf51 node floats (1) */
static void field1(rx_addr r, rx_addr node, rx_addr name, rx_addr dst) {
    rx_addr e = rx_xml_vf_child(r, node, name);
    rx_xml_vf_node_floats(r, e, dst, 1);
}
/* the 2 x 3 x 4 leaf walk every value reader uses (the original's three nested vf8 cursors) */
static void read_leaves(rx_addr r, rx_addr node, void (*leaf)(rx_addr, rx_addr, uint32_t)) {
    RX_FRAME_ENTER(F, 12);
    uint32_t k = 0;
    rx_addr a = rx_xml_vf_first(r, node, 0, F);
    for (uint32_t i = 0; i < 2; i++) {
        rx_addr b = rx_xml_vf_first(r, a, 0, F + 4);
        for (uint32_t j = 0; j < 3; j++) {
            rx_addr c = rx_xml_vf_first(r, b, 0, F + 8);
            for (uint32_t l = 0; l < 4; l++) {
                leaf(r, c, k);
                k++;
                c = rx_xml_vf_next(r, U32(F + 8, 0));
            }
            rx_xml_vf_release(r, U32(F + 8, 0));
            b = rx_xml_vf_next(r, U32(F + 4, 0));
        }
        rx_xml_vf_release(r, U32(F + 4, 0));
        a = rx_xml_vf_next(r, U32(F, 0));
    }
    rx_xml_vf_release(r, U32(F, 0));
    RX_FRAME_LEAVE(F, 12);
}

/* ---- value readers (all 24 leaves, one float each) ---- */
/* 0051ef80 GeneralBehaviorValues: four arrays (stride 4) */
static void leaf_general(rx_addr r, rx_addr node, uint32_t k) {
    field1(r, node, 0x0093f254u, 0x009e43b8u + 4u * k);   /* Pullback */
    field1(r, node, 0x0093f260u, 0x009e3ff8u + 4u * k);   /* PushForward */
    field1(r, node, 0x0093f26cu, 0x009e4478u + 4u * k);   /* PushForwardOnTurn */
    field1(r, node, 0x0093f280u, 0x009e4838u + 4u * k);   /* MaxAngleBeforeSlowdown */
}
/* 0051f240 AirLevelingValues */
static void leaf_airlevel(rx_addr r, rx_addr node, uint32_t k) { field1(r, node, 0x0093f2acu, 0x00a04c58u + 4u * k); }
/* 005204b0 / 00520620 / 00520790 / 00520900 / 005219c0 / 005337e0: a 4-float record per leaf */
#define LEAF_AVOID(NAME, BASE) \
    static void leaf_##NAME(rx_addr r, rx_addr node, uint32_t k) { \
        const rx_addr d = (BASE) + 0x10u * k; \
        field1(r, node, 0x0093f320u, d);        /* Attract */ \
        field1(r, node, 0x0093f328u, d + 4);    /* AttractExp */ \
        field1(r, node, 0x0093f334u, d + 8);    /* Repulsion */ \
        field1(r, node, 0x0093f340u, d + 12);   /* RepulsionExp */ \
    }
LEAF_AVOID(avoidobjects, 0x009e32e8u)
LEAF_AVOID(avoidsphere,  0x009e46b8u)
LEAF_AVOID(superavoid,   0x009e4058u)
LEAF_AVOID(avoideco,     0x009e3de8u)
LEAF_AVOID(boundary,     0x00a04d18u)
LEAF_AVOID(splitsection, 0x009e3a28u)
/* 00522c80 BrakeTurnValues: a 7-float record per leaf */
static void leaf_braketurn(rx_addr r, rx_addr node, uint32_t k) {
    const rx_addr d = 0x009e3648u + 0x1cu * k;
    field1(r, node, 0x0093f37cu, d);           /* RateChange */
    field1(r, node, 0x0093f388u, d + 4);       /* MinEndAngle */
    field1(r, node, 0x0093f394u, d + 8);       /* MinEndSpeed */
    field1(r, node, 0x0093f3a0u, d + 12);      /* PauseBeforeClutchTime */
    field1(r, node, 0x0093f3b8u, d + 16);      /* PauseBeforePullBackTime */
    field1(r, node, 0x0093f3d0u, d + 20);      /* PullBackTime */
    field1(r, node, 0x0093f3e0u, d + 24);      /* PullBackValue */
}
/* 005246e0 HandleBrakingValues: five arrays */
static void leaf_handlebraking(rx_addr r, rx_addr node, uint32_t k) {
    field1(r, node, 0x0093f404u, 0x009e4358u + 4u * k);   /* RedZonePercent */
    field1(r, node, 0x0093f414u, 0x00a04898u + 4u * k);   /* YellowZonePercent */
    field1(r, node, 0x0093f428u, 0x009e3ba8u + 4u * k);   /* GreenZonePercent */
    field1(r, node, 0x0093f43cu, 0x009e39c8u + 4u * k);   /* RayExtent */
    field1(r, node, 0x0093f448u, 0x009e41d8u + 4u * k);   /* NonCornerBrakeTurnTargetScalar */
}
/* 00526f30 CenterPotential: a 4-float record plus thirteen arrays */
static void leaf_center(rx_addr r, rx_addr node, uint32_t k) {
    const rx_addr d = 0x00a049b8u + 0x10u * k;
    field1(r, node, 0x0093f320u, d);            /* Attract */
    field1(r, node, 0x0093f328u, d + 4);        /* AttractExp */
    field1(r, node, 0x0093f334u, d + 8);        /* Repulsion */
    field1(r, node, 0x0093f340u, d + 12);       /* RepulsionExp */
    field1(r, node, 0x0093f478u, 0x009e3908u + 4u * k);   /* MaxSteerToBallAngle */
    field1(r, node, 0x0093f48cu, 0x009e4418u + 4u * k);   /* MaxSteerToBallFunnelAngle */
    field1(r, node, 0x0093f4a8u, 0x009e4298u + 4u * k);   /* CenterLookAheadNormal */
    field1(r, node, 0x0093f4c0u, 0x009e44d8u + 4u * k);   /* CornerApproachLookAhead */
    field1(r, node, 0x0093f4d8u, 0x00a04958u + 4u * k);   /* CornerApproachBeginLookIntoDist */
    field1(r, node, 0x0093f4f8u, 0x009e35e8u + 4u * k);   /* CornerApproachLookInto */
    field1(r, node, 0x0093f510u, 0x009e4238u + 4u * k);   /* LaneSwitchThreshold */
    field1(r, node, 0x0093f524u, 0x009e4538u + 4u * k);   /* InCornerLookAhead */
    field1(r, node, 0x0093f538u, 0x009e45f8u + 4u * k);   /* ExitCornerLookAhead */
    field1(r, node, 0x0093f54cu, 0x009e42f8u + 4u * k);   /* OutsideCornerOffsetScalar */
    field1(r, node, 0x0093f568u, 0x009e3f98u + 4u * k);   /* InsideCornerOffsetScalar */
    field1(r, node, 0x0093f584u, 0x009e3288u + 4u * k);   /* ExitCornerOffsetScalar */
    field1(r, node, 0x0093f59cu, 0x00a048f8u + 4u * k);   /* NormalOffsetScalar */
}
/* 00528f30 OfftrackPotential: a 4-float record plus CenterLookAhead */
static void leaf_offtrack(rx_addr r, rx_addr node, uint32_t k) {
    const rx_addr d = 0x009e3468u + 0x10u * k;
    field1(r, node, 0x0093f320u, d); field1(r, node, 0x0093f328u, d + 4);
    field1(r, node, 0x0093f334u, d + 8); field1(r, node, 0x0093f340u, d + 12);
    field1(r, node, 0x0093f5c4u, 0x00a04b38u + 4u * k);   /* CenterLookAhead */
}
/* 005299d0 OverrideSplineValues: a 4-float record plus LookAhead */
static void leaf_overridespline(rx_addr r, rx_addr node, uint32_t k) {
    const rx_addr d = 0x009e3c68u + 0x10u * k;
    field1(r, node, 0x0093f320u, d); field1(r, node, 0x0093f328u, d + 4);
    field1(r, node, 0x0093f334u, d + 8); field1(r, node, 0x0093f340u, d + 12);
    field1(r, node, 0x0093f5ecu, 0x009e3c08u + 4u * k);   /* LookAhead */
}
/* 00531700 SeperatePotential: five arrays */
static void leaf_separate(rx_addr r, rx_addr node, uint32_t k) {
    field1(r, node, 0x0093f614u, 0x00a04cb8u + 4u * k);   /* NoTakeLineTime */
    field1(r, node, 0x0093f624u, 0x009e4658u + 4u * k);   /* ExitCornerWaitTime */
    field1(r, node, 0x0093f638u, 0x00a04bf8u + 4u * k);   /* BrakeTireIntersectMaxTime */
    field1(r, node, 0x0093f654u, 0x00a04b98u + 4u * k);   /* BrakeVelocityIntersectMaxTime */
    field1(r, node, 0x0093f674u, 0x009e4598u + 4u * k);   /* PassingTireIntersectMaxTime */
}
/* 00534280 TakeLinePotential: a 4-float record plus TakeLineMinSpeed */
static void leaf_takeline(rx_addr r, rx_addr node, uint32_t k) {
    const rx_addr d = 0x009e3108u + 0x10u * k;
    field1(r, node, 0x0093f320u, d); field1(r, node, 0x0093f328u, d + 4);
    field1(r, node, 0x0093f334u, d + 8); field1(r, node, 0x0093f340u, d + 12);
    field1(r, node, 0x0093f6bcu, 0x009e3968u + 4u * k);   /* TakeLineMinSpeed */
}
/* 00534ee0 HandleSuckingValues: the walk only (the original reads and writes nothing) */
static void leaf_suckracing(rx_addr r, rx_addr node, uint32_t k) { (void)r; (void)node; (void)k; }

void rx_aidata_read_general(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_general); }
void rx_aidata_read_airlevel(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_airlevel); }
void rx_aidata_read_avoidobjects(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_avoidobjects); }
void rx_aidata_read_avoidsphere(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_avoidsphere); }
void rx_aidata_read_superavoid(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_superavoid); }
void rx_aidata_read_avoideco(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_avoideco); }
void rx_aidata_read_boundary(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_boundary); }
void rx_aidata_read_braketurn(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_braketurn); }
void rx_aidata_read_handlebraking(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_handlebraking); }
void rx_aidata_read_center(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_center); }
void rx_aidata_read_offtrack(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_offtrack); }
void rx_aidata_read_overridespline(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_overridespline); }
void rx_aidata_read_separate(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_separate); }
void rx_aidata_read_splitsection(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_splitsection); }
void rx_aidata_read_takeline(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_takeline); }
void rx_aidata_read_suckracing(rx_addr r, rx_addr node) { read_leaves(r, node, leaf_suckracing); }

/* ---- the behaviour vf1s (0051eed0 .. 00534e30): one row name each, AvoidObjects four ---- */
static const aidata_group g_general[]      = {{0x0093f23cu, rx_aidata_read_general}};
static const aidata_group g_airlevel[]     = {{0x0093f298u, rx_aidata_read_airlevel}};
static const aidata_group g_avoidobjects[] = {{0x0093f2c0u, rx_aidata_read_avoidobjects}, {0x0093f2d8u, rx_aidata_read_avoidsphere},
                                              {0x0093f2f0u, rx_aidata_read_superavoid}, {0x0093f30cu, rx_aidata_read_avoideco}};
static const aidata_group g_boundary[]     = {{0x0093f358u, rx_aidata_read_boundary}};
static const aidata_group g_braketurn[]    = {{0x0093f36cu, rx_aidata_read_braketurn}};
static const aidata_group g_handlebraking[]= {{0x0093f3f0u, rx_aidata_read_handlebraking}};
static const aidata_group g_center[]       = {{0x0093f468u, rx_aidata_read_center}};
static const aidata_group g_offtrack[]     = {{0x0093f5b0u, rx_aidata_read_offtrack}};
static const aidata_group g_overridespline[]={{0x0093f5d4u, rx_aidata_read_overridespline}};
static const aidata_group g_separate[]     = {{0x0093f600u, rx_aidata_read_separate}};
static const aidata_group g_splitsection[] = {{0x0093f690u, rx_aidata_read_splitsection}};
static const aidata_group g_takeline[]     = {{0x0093f6a8u, rx_aidata_read_takeline}};
static const aidata_group g_suckracing[]   = {{0x0093f6d0u, rx_aidata_read_suckracing}};

void rx_aidata_vf_general(rx_addr r, rx_addr node)      { aidata_dispatch(r, node, g_general, 1); }
void rx_aidata_vf_airlevel(rx_addr r, rx_addr node)     { aidata_dispatch(r, node, g_airlevel, 1); }
void rx_aidata_vf_avoidobjects(rx_addr r, rx_addr node) { aidata_dispatch(r, node, g_avoidobjects, 4); }
void rx_aidata_vf_boundary(rx_addr r, rx_addr node)     { aidata_dispatch(r, node, g_boundary, 1); }
void rx_aidata_vf_braketurn(rx_addr r, rx_addr node)    { aidata_dispatch(r, node, g_braketurn, 1); }
void rx_aidata_vf_handlebraking(rx_addr r, rx_addr node){ aidata_dispatch(r, node, g_handlebraking, 1); }
void rx_aidata_vf_center(rx_addr r, rx_addr node)       { aidata_dispatch(r, node, g_center, 1); }
void rx_aidata_vf_offtrack(rx_addr r, rx_addr node)     { aidata_dispatch(r, node, g_offtrack, 1); }
void rx_aidata_vf_overridespline(rx_addr r, rx_addr node){ aidata_dispatch(r, node, g_overridespline, 1); }
void rx_aidata_vf_separate(rx_addr r, rx_addr node)     { aidata_dispatch(r, node, g_separate, 1); }
void rx_aidata_vf_splitsection(rx_addr r, rx_addr node) { aidata_dispatch(r, node, g_splitsection, 1); }
void rx_aidata_vf_takeline(rx_addr r, rx_addr node)     { aidata_dispatch(r, node, g_takeline, 1); }
void rx_aidata_vf_suckracing(rx_addr r, rx_addr node)   { aidata_dispatch(r, node, g_suckracing, 1); }
RX_FUNCTION(0x0051eed0, rx_aidata_vf_general)
RX_FUNCTION(0x0051f190, rx_aidata_vf_airlevel)
RX_FUNCTION(0x005203b0, rx_aidata_vf_avoidobjects)
RX_FUNCTION(0x00521910, rx_aidata_vf_boundary)
RX_FUNCTION(0x00522bd0, rx_aidata_vf_braketurn)
RX_FUNCTION(0x00524630, rx_aidata_vf_handlebraking)
RX_FUNCTION(0x00526e80, rx_aidata_vf_center)
RX_FUNCTION(0x00528e80, rx_aidata_vf_offtrack)
RX_FUNCTION(0x00529920, rx_aidata_vf_overridespline)
RX_FUNCTION(0x00531650, rx_aidata_vf_separate)
RX_FUNCTION(0x00533730, rx_aidata_vf_splitsection)
RX_FUNCTION(0x005341d0, rx_aidata_vf_takeline)
RX_FUNCTION(0x00534e30, rx_aidata_vf_suckracing)
/* the value readers (their own addresses, for the layered Verify runs) */
RX_FUNCTION(0x0051ef80, rx_aidata_read_general)
RX_FUNCTION(0x0051f240, rx_aidata_read_airlevel)
RX_FUNCTION(0x005204b0, rx_aidata_read_avoidobjects)
RX_FUNCTION(0x00520620, rx_aidata_read_avoidsphere)
RX_FUNCTION(0x00520790, rx_aidata_read_superavoid)
RX_FUNCTION(0x00520900, rx_aidata_read_avoideco)
RX_FUNCTION(0x005219c0, rx_aidata_read_boundary)
RX_FUNCTION(0x00522c80, rx_aidata_read_braketurn)
RX_FUNCTION(0x005246e0, rx_aidata_read_handlebraking)
RX_FUNCTION(0x00526f30, rx_aidata_read_center)
RX_FUNCTION(0x00528f30, rx_aidata_read_offtrack)
RX_FUNCTION(0x005299d0, rx_aidata_read_overridespline)
RX_FUNCTION(0x00531700, rx_aidata_read_separate)
RX_FUNCTION(0x005337e0, rx_aidata_read_splitsection)
RX_FUNCTION(0x00534280, rx_aidata_read_takeline)
RX_FUNCTION(0x00534ee0, rx_aidata_read_suckracing)

/* =============================================================================================================================
 * 0051eac0 (EDI = the AIBehavior reader): rows Behaviour name -> behaviour ctor. The name comparison is the original's inline
 * case-sensitive byte loop (strcmp), not _stricmp. */
typedef struct { uint32_t name; void (*fn)(rx_addr, rx_addr); } aidata_behaviour;
static const aidata_behaviour g_behaviours[] = {
    {0x0093f190u, rx_aidata_vf_general},        /* GeneralBehaviors */
    {0x0093f1a4u, rx_aidata_vf_center},         /* NavigateTrack */
    {0x0093f1b4u, rx_aidata_vf_boundary},       /* AvoidTrackBoundaries */
    {0x0093ed8cu, rx_aidata_vf_takeline},       /* TakeLine */
    {0x0093f1ccu, rx_aidata_vf_splitsection},   /* SplitSection */
    {0x0093f1dcu, rx_aidata_vf_avoidobjects},   /* AvoidObjects */
    {0x0093f1ecu, rx_aidata_vf_separate},       /* Seperate */
    {0x0093f1f8u, rx_aidata_vf_braketurn},      /* BrakeTurn */
    {0x0093f204u, rx_aidata_vf_handlebraking},  /* HandleBraking */
    {0x0093f214u, rx_aidata_vf_airlevel},       /* AirLeveling */
    {0x0093f01cu, rx_aidata_vf_offtrack},       /* Offtrack */
    {0x0093f220u, rx_aidata_vf_overridespline}, /* OverrideSpline */
    {0x0093f230u, rx_aidata_vf_suckracing},     /* SuckRacing */
};
void rx_aidata_behaviors(rx_addr r) {
    RX_FRAME_ENTER(F, 0x108);
    crt_memset(F + 1, 0, 0x103); SU8(F, 0, 0);
    rx_addr root = rx_xml_vf_root(r);
    rx_addr row = rx_xml_vf_first(r, root, 0, F + 0x104);
    while (row) {
        rx_xml_vf_node_name(r, row, F, 0x104);
        for (unsigned i = 0; i < sizeof g_behaviours / sizeof g_behaviours[0]; i++)
            if (crt_strcmp(F, g_behaviours[i].name) == 0) g_behaviours[i].fn(r, row);
        row = rx_xml_vf_next(r, U32(F + 0x104, 0));
    }
    rx_xml_vf_release(r, U32(F + 0x104, 0));
    RX_FRAME_LEAVE(F, 0x108);
}
RX_FUNCTION(0x0051eac0, rx_aidata_behaviors)

/* =============================================================================================================================
 * 0051e430 (ESI = the AIAbility reader): DriverType row -> (type index) -> DriverAbility row -> (ability index) -> four tier
 * rows -> Tier1/Tier2/Tier3 (one float, or a two-float vector for grouptype "vector") into the personality block; then the
 * explicit slots 0..10 for the type via 0051e1d0. */
void rx_aidata_abilities(rx_addr r) {
    RX_FRAME_ENTER(F, 0x640);
    const rx_addr abilitybuf = F + 0x020, namebuf = F + 0x130, gtype = F + 0, gtypestr = F + 0x240;
    const rx_addr typenum = F + 0x350;
    const rx_addr cur_root = F + 0x010, cur_type = F + 0x014, cur_ability = F + 0x018;
    crt_memset(namebuf + 1, 0, 0x103); SU8(namebuf, 0, 0);
    /* 0051e48a: the whole personality / chance block is cleared before the rows (the original's root-name read at 0051e47d, a
     * dead stack write, precedes it). The EXE's .data at 0xa89d08 is zero, so the first load is unchanged either way; it keeps a
     * second load of the assets (where the original clears the old values) exact. */
    crt_memset(0x00a89d08, 0, 0xa24c);
    rx_addr row = rx_xml_vf_first(r, rx_xml_vf_root(r), 0, cur_root);
    for (;;) {
        if (!row) { rx_xml_vf_release(r, U32(cur_root, 0)); break; }
        rx_xml_vf_node_name(r, row, namebuf, 0x104);
        int type = 0;
        for (; type < 0xe; type++) if (crt_stricmp(namebuf, U32(0, 0x009cd7d8u + 4u * (uint32_t)type)) == 0) break;
        SU32(typenum, 0, (uint32_t)type);
        rx_addr ab = rx_xml_vf_first(r, row, 0, cur_type);
        if (ab) {
            const uint32_t tb = (uint32_t)type * 0x11u;
            do {
                rx_xml_vf_node_name(r, ab, abilitybuf, 0x104);
                int a = 0;
                for (; a < 0x12; a++) if (crt_stricmp(abilitybuf, U32(0, 0x009cd810u + 4u * (uint32_t)a)) == 0) break;
                const uint32_t block = (tb + (uint32_t)a) * 0xbcu;
                SU32(0, 0xa89dc0u + block, 1);                       /* the ability is present */
                rx_addr tier = rx_xml_vf_first(r, ab, 0, cur_ability);
                uint32_t dst = 0xa89d10u + block;                    /* +8: the first value pair */
                for (uint32_t t = 0; t < 4; t++) {
                    rx_addr n1 = rx_xml_vf_child(r, tier, S_TIER1);
                    rx_addr attr = xml_attr_first(r, n1, S_GROUPTYPE, gtype);
                    rx_xml_vf_attr_string(r, attr, gtypestr, 0x104);
                    xml_attr_release(r, U32(gtype, 0));
                    const uint32_t n = guest_has_vector(gtypestr) ? 2u : 1u;
                    rx_xml_vf_node_floats(r, n1, dst - 8, n);
                    rx_addr n2 = rx_xml_vf_child(r, tier, S_TIER2);
                    rx_xml_vf_node_floats(r, n2, dst, n);
                    rx_addr n3 = rx_xml_vf_child(r, tier, S_TIER3);
                    rx_xml_vf_node_floats(r, n3, dst + 8, n);
                    tier = rx_xml_vf_next(r, U32(cur_ability, 0));
                    dst += 0x18;
                }
                for (uint32_t slot = 0; slot < 0xb; slot++) rx_aidata_ability_defaults((uint32_t)type, slot);
                rx_xml_vf_release(r, U32(cur_ability, 0));
                ab = rx_xml_vf_next(r, U32(cur_type, 0));
            } while (ab);
        }
        rx_xml_vf_release(r, U32(cur_type, 0));
        row = rx_xml_vf_next(r, U32(cur_root, 0));
    }
    RX_FRAME_LEAVE(F, 0x640);
}
RX_FUNCTION(0x0051e430, rx_aidata_abilities)

/* =============================================================================================================================
 * 0051e7b0 (ESI = the AIAbilityOverride reader): DriverType row -> DriverAbility row -> the RealPlayers group -> Player01..
 * Player11 (its grouptype "vector" writes two floats), into the ability's explicit slot 0x60 + 8 * (player - 1). */
void rx_aidata_overrides(rx_addr r) {
    RX_FRAME_ENTER(F, 0x740);
    const rx_addr abilitybuf = F + 0x020, namebuf = F + 0x130, gtype = F + 0, gtypestr = F + 0x240;
    const rx_addr typenum = F + 0x350, abilitynum = F + 0x354, playerbuf = F + 0x360;
    const rx_addr cur_root = F + 0x010, cur_type = F + 0x014, cur_group = F + 0x018;
    crt_memset(namebuf + 1, 0, 0x103); SU8(namebuf, 0, 0);
    rx_addr row = rx_xml_vf_first(r, rx_xml_vf_root(r), 0, cur_root);
    for (;;) {
        if (!row) { rx_xml_vf_release(r, U32(cur_root, 0)); break; }
        rx_xml_vf_node_name(r, row, namebuf, 0x104);
        int type = 0;
        for (; type < 0xe; type++) if (crt_stricmp(namebuf, U32(0, 0x009cd7d8u + 4u * (uint32_t)type)) == 0) break;
        SU32(typenum, 0, (uint32_t)type);
        rx_addr ab = rx_xml_vf_first(r, row, 0, cur_type);
        if (ab) {
            do {
                rx_xml_vf_node_name(r, ab, abilitybuf, 0x104);
                int a = 0;
                for (; a < 0x12; a++) if (crt_stricmp(abilitybuf, U32(0, 0x009cd810u + 4u * (uint32_t)a)) == 0) break;
                SU32(abilitynum, 0, (uint32_t)a);
                rx_addr group = rx_xml_vf_first(r, ab, 0, cur_group);
                if (group) {
                    const uint32_t block = ((uint32_t)type * 0x11u + (uint32_t)a) * 0xbcu;
                    for (uint32_t p = 0; p < 0xb; p++) {
                        char n[16];
                        snprintf(n, sizeof n, "Player%02u", p + 1u);
                        for (uint32_t k = 0; k <= strlen(n); k++) SU8(playerbuf, k, (uint8_t)n[k]);
                        rx_addr pn = rx_xml_vf_child(r, group, playerbuf);
                        if (!pn) continue;
                        rx_addr attr = xml_attr_first(r, pn, S_GROUPTYPE, gtype);
                        rx_xml_vf_attr_string(r, attr, gtypestr, 0x104);
                        xml_attr_release(r, U32(gtype, 0));
                        rx_xml_vf_node_floats(r, pn, 0xa89d68u + block + p * 8u, guest_has_vector(gtypestr) ? 2u : 1u);
                    }
                    rx_xml_vf_release(r, U32(cur_group, 0));
                }
                ab = rx_xml_vf_next(r, U32(cur_type, 0));
            } while (ab);
        }
        rx_xml_vf_release(r, U32(cur_type, 0));
        row = rx_xml_vf_next(r, U32(cur_root, 0));
    }
    RX_FRAME_LEAVE(F, 0x740);
}
RX_FUNCTION(0x0051e7b0, rx_aidata_overrides)

/* =============================================================================================================================
 * 006cc1a0 "VehicleTakeoff" (the stunt system's handler, registered by build_activity_player.c; ECX = the stunt system, one stack
 * argument: the vehicle): a new path the AI reaches once the AIData tables make it ride and jump. When the event is about the
 * system's current vehicle (+0x128): +0x5c = 1 if that vehicle is in one of its two airborne states (+0xbc0 / +0xb74), else 0;
 * the takeoff record +0x424..+0x42c cleared. RET 4. */
void rx_stunt_vehicle_takeoff(rx_addr s, uint32_t vehicle) {
    const rx_addr cur = U32(s, 0x128);
    if (vehicle != cur) return;
    uint32_t v = 0;
    if (U32(cur, 0xbc0) != 0 || U32(cur, 0xb74) != 0) v = 1;
    SU32(s, 0x5c, v);
    SU32(s, 0x424, 0); SU32(s, 0x428, 0); SU32(s, 0x42c, 0);
}
RX_FUNCTION(0x006cc1a0, rx_stunt_vehicle_takeoff)
