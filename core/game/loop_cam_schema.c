/* loop_cam_schema.c -- the camera tuning schema: the CRT static initializer 00908500 with its two helpers 006596f0 and
 * 00659ef0, hand transliterated from MXReflex.exe (the structures are 0065c200's, loop_cam_tuning.c).
 *
 * The tuning parser 0065c200 walks a <camera> bxml and matches each settings child (Base_FS_FOV, Distance_FS_*,
 * Effects_FS_CS_*, ...) against the record lists of one of 14 entries:
 *
 *   class record (0x1da80 bytes, 0x00af6d10 + class * 0x1da80):
 *     +0x0000  14 entries x 0x404: 256 record pointers at +0, the count at +0x400
 *     +0x3838  the first block of 14 records (0x48 = name[0x40] + kind at +0x40 + a float at +0x44), and 93 more
 *              blocks: 0x48 name/kind/float records, 0xa0 records (name/kind + a 0x5c curve at +0x44) and 0x60
 *              records (name/kind + the vec4 at +0x50), each block 14 records = the 14 entries' copies.
 *
 * The entry index is side + 2 * camera type (0 FirstPerson, 1 Visual, 2 GamePlay, 3 QuickLeft, 4 QuickRight,
 * 5 QuickBack, 6 Wreck; side 0 = "FS_" first-person camera settings, 1 = "SS_" side-scroll settings, from the
 * name). A row's records are therefore class + row.base + entry * row.size; the row table's order is 006596f0's
 * constructor order. Kinds: 0 one float (+0x44), 1 one vec4 (+0x50), 2 one curve (+0x44, zero until the parser
 * reads it). 00659ef0's per-iteration register advance (ebx / 0x3a67 / 0x491f + 0x90 / 0x140 / 0xc0 and the
 * entry count +0x808) is the base + entry * size indexing below.
 *
 * Both record names of a row: the FS entry and the SS entry take their own string (the Effects "On" / "Range"
 * settings are shared and have no SS_ in the name). The default floats are the rdata constants the original loads
 * (0x7f7fffff = FLT_MAX "not set"), the vec4s the original's xmm stores. The 94 rows' contents (names, kinds, defaults,
 * block order) were read from the original initializer's own output - the oracle probe's dump of the nine records before
 * the first parse - and cross-checked against 00659ef0's disassembly; the per-call Verify compares the complete
 * 9 x 0x1da80 bytes of the port against the original. */
#include "rx_game.h"
#include "loop_cam_schema.h"
#include "crt.h"
#include "build_globals_int.h"   /* RX_ATEXIT (the original's tail) */

/* 004db390 (ECX = entry): count 0 */
static void cam_schema_entry_ctor(rx_addr e) { SU32(e, 0x400, 0); }
/* 0065c970 (ECX = record): name = "unknown" (0x0094250c, 0x3f bytes, padded), byte at +0x3f = 0 (00659ef0 replaces
 * the name later) */
static void cam_schema_name_ctor(rx_addr e) {
    crt_strncpy(e, 0x0094250cu, 0x3f);
    SU8(e, 0x3f, 0);
}
/* 0065c990 (ECX = curve record): the name, and the curve at +0x44 zeroed */
static void cam_schema_curve_ctor(rx_addr e) {
    SU32(e, 0x44, 0);
    cam_schema_name_ctor(e);
}
/* `vector constructor iterator' 004022d0 (stdcall array, size, count, ctor): ctor(element) first to last */
static void cam_schema_ctor_iter(rx_addr base, uint32_t size, uint32_t count, void (*ctor)(rx_addr)) {
    for (uint32_t i = 0; i < count; i++) ctor(base + size * i);
}

/* the blocks: {base in the class record, element size, kind, FS name address, SS name address, defaults}.
 * The 14-element blocks are 006596f0's constructor list in its order (0x48 / 0x60 with 0065c970, 0xa0 with
 * 0065c990); the names, kinds and defaults are 00659ef0's (the FS and SS halves of each of its 7 iterations). */
#define CAM_SCHEMA_ROWS 94
static const struct cam_schema_row {
    uint32_t base, size;
    uint8_t kind;
    uint32_t name_fs, name_ss;
    uint32_t data[4];
} cam_schema_rows[CAM_SCHEMA_ROWS] = {
    { 0x03838, 0x48, 0, 0x00958bb4, 0x009596ec, { 0x7f7fffff } },   /* Base_FS_FOV */
    { 0x03c28, 0xa0, 2, 0x00958bc0, 0x009596f8, { 0x00000000 } },   /* Base_FS_FOVDynamicCurve */
    { 0x044e8, 0x48, 0, 0x00958bd8, 0x00959710, { 0x7f7fffff } },   /* Base_FS_NearClipDistance */
    { 0x048e0, 0x60, 1, 0x00958bf4, 0x0095972c, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } },   /* Base_FS_TargetLocalOffset */
    { 0x04e20, 0x60, 1, 0x00958c10, 0x00959748, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } },   /* Base_FS_DirectionalArrow */
    { 0x05360, 0x60, 1, 0x00958c2c, 0x00959764, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } },   /* Base_FS_LookFrom */
    { 0x058a0, 0x60, 1, 0x00958c40, 0x00959778, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } },   /* Base_FS_LookThrough */
    { 0x05de0, 0x48, 0, 0x00958c54, 0x0095978c, { 0x7f7fffff } },   /* Base_FS_AlwaysUpright */
    { 0x061d0, 0x48, 0, 0x00958c6c, 0x009597a4, { 0x7f7fffff } },   /* Base_FS_UseHeadOrientation */
    { 0x065c0, 0x48, 0, 0x00958c88, 0x009597c0, { 0x7f7fffff } },   /* Distance_FS_ViewDistance */
    { 0x069b0, 0x48, 0, 0x00958ca4, 0x009597dc, { 0x7f7fffff } },   /* Distance_FS_ViewHeadingAccel */
    { 0x06da0, 0x48, 0, 0x00958cc4, 0x009597fc, { 0x7f7fffff } },   /* Distance_FS_ViewHeadingDecelScale */
    { 0x07190, 0x48, 0, 0x00958ce8, 0x00959820, { 0x7f7fffff } },   /* Distance_FS_ReversePullbackDistance */
    { 0x07580, 0x48, 0, 0x00958d0c, 0x00959844, { 0x7f7fffff } },   /* Distance_FS_ForwardOffset */
    { 0x07970, 0x48, 0, 0x00958d28, 0x00959860, { 0x7f7fffff } },   /* Distance_FS_VerticalOffset */
    { 0x07d60, 0x48, 0, 0x00958d44, 0x0095987c, { 0x7f7fffff } },   /* Distance_FS_TargetVerticalOffsetAccel */
    { 0x08150, 0x48, 0, 0x00958d6c, 0x009598a4, { 0x7f7fffff } },   /* Distance_FS_TargetVerticalOffsetDecelScale */
    { 0x08540, 0x48, 0, 0x00958d98, 0x009598d0, { 0x7f7fffff } },   /* Distance_FS_VerticalFilterStrength */
    { 0x08930, 0x48, 0, 0x00958dbc, 0x009598f4, { 0x7f7fffff } },   /* Distance_FS_VelocityLagDistance */
    { 0x08d20, 0x48, 0, 0x00958ddc, 0x00959914, { 0x7f7fffff } },   /* Distance_FS_VelocityLagAccel */
    { 0x09110, 0x48, 0, 0x00958dfc, 0x00959934, { 0x7f7fffff } },   /* Distance_FS_VelocityDecelScale */
    { 0x09500, 0x48, 0, 0x00958e1c, 0x00959954, { 0x7f7fffff } },   /* Distance_FS_AirLagDistance */
    { 0x098f0, 0x48, 0, 0x00958e38, 0x00959970, { 0x7f7fffff } },   /* Distance_FS_AirLagAccel */
    { 0x09ce0, 0x48, 0, 0x00958e50, 0x00959988, { 0x7f7fffff } },   /* Distance_FS_AirDecelScale */
    { 0x0a0d0, 0x48, 0, 0x00958e6c, 0x009599a4, { 0x7f7fffff } },   /* Angle_FS_ViewPitch */
    { 0x0a4c0, 0x48, 0, 0x00958e80, 0x009599b8, { 0x7f7fffff } },   /* Angle_FS_SlotHeading */
    { 0x0a8b0, 0x48, 0, 0x00958e98, 0x009599d0, { 0x7f7fffff } },   /* Angle_FS_SlotHeadingAdjust */
    { 0x0aca0, 0x48, 0, 0x00958eb4, 0x009599ec, { 0x7f7fffff } },   /* Angle_FS_LateralShiftDistance */
    { 0x0b090, 0x48, 0, 0x00958ed4, 0x00959a0c, { 0x7f7fffff } },   /* Angle_FS_LateralShiftAccel */
    { 0x0b480, 0x48, 0, 0x00958ef0, 0x00959a28, { 0x7f7fffff } },   /* Angle_FS_LateralShiftDecelScale */
    { 0x0b870, 0x48, 0, 0x00958f10, 0x00959a48, { 0x7f7fffff } },   /* Angle_FS_CameraSlideDistance */
    { 0x0bc60, 0x48, 0, 0x00958f30, 0x00959a68, { 0x7f7fffff } },   /* Angle_FS_CameraSlideAccel */
    { 0x0c050, 0x48, 0, 0x00958f4c, 0x00959a84, { 0x7f7fffff } },   /* Angle_FS_CameraSlideDecelScale */
    { 0x0c440, 0x48, 0, 0x00958f6c, 0x00959aa4, { 0x7f7fffff } },   /* Angle_FS_Pitch */
    { 0x0c830, 0x48, 0, 0x00958f7c, 0x00959ab4, { 0x7f7fffff } },   /* Angle_FS_AirPitch */
    { 0x0cc20, 0x48, 0, 0x00958f90, 0x00959ac8, { 0x7f7fffff } },   /* Angle_FS_PitchFilterAccel */
    { 0x0d010, 0x48, 0, 0x00958fac, 0x00959ae4, { 0x7f7fffff } },   /* Angle_FS_PitchFilterDecelScale */
    { 0x0d400, 0x48, 0, 0x00958fcc, 0x00959b04, { 0x7f7fffff } },   /* LOS_FS_UpHillViewCone */
    { 0x0d7f0, 0x48, 0, 0x00958fe4, 0x00959b1c, { 0x7f7fffff } },   /* LOS_FS_UpHillSampleAdjust */
    { 0x0dbe0, 0x48, 0, 0x00959000, 0x00959b38, { 0x7f7fffff } },   /* LOS_FS_UpHillPitchAccel */
    { 0x0dfd0, 0x48, 0, 0x00959018, 0x00959b50, { 0x7f7fffff } },   /* LOS_FS_UpHillPitchDecelScale */
    { 0x0e3c0, 0x48, 0, 0x00959038, 0x00959b70, { 0x7f7fffff } },   /* LOS_FS_AboveScreenThreshold */
    { 0x0e7b0, 0x48, 0, 0x00959054, 0x00959b8c, { 0x7f7fffff } },   /* LOS_FS_BelowScreenThreshold */
    { 0x0eba0, 0x48, 0, 0x00959070, 0x00959ba8, { 0x7f7fffff } },   /* LOS_FS_MaxPitchAdjustAbove */
    { 0x0ef90, 0x48, 0, 0x0095908c, 0x00959bc4, { 0x7f7fffff } },   /* LOS_FS_MaxPitchAdjustBelow */
    { 0x0f380, 0x48, 0, 0x009590a8, 0x00959be0, { 0x7f7fffff } },   /* Collision_FS_CollisionDeflectionAccel */
    { 0x0f770, 0x48, 0, 0x009590d0, 0x00959c08, { 0x7f7fffff } },   /* Collision_FS_CollisionDeflectionMaxSpeed */
    { 0x0fb60, 0x48, 0, 0x009590fc, 0x00959c34, { 0x7f7fffff } },   /* Collision_FS_CollisionDeflectionDecel */
    { 0x0ff50, 0x48, 0, 0x00959124, 0x00959c5c, { 0x7f7fffff } },   /* Collision_FS_CollisionSphereRadius */
    { 0x10340, 0x48, 0, 0x00959148, 0x00959c80, { 0x7f7fffff } },   /* Other_FS_AirToGroundBlendTime */
    { 0x10730, 0x48, 0, 0x00959168, 0x00959ca0, { 0x7f7fffff } },   /* Other_FS_GroundToAirBlendTime */
    { 0x10b20, 0x48, 0, 0x00959188, 0x00959cc0, { 0x7f7fffff } },   /* Other_FS_MaxFirstPersonUserPitchAdjustUp */
    { 0x10f10, 0x48, 0, 0x009591b4, 0x00959cec, { 0x7f7fffff } },   /* Other_FS_MaxFirstPersonUserPitchAdjustDown */
    { 0x11300, 0x48, 0, 0x009591e0, 0x00959d18, { 0x7f7fffff } },   /* Other_FS_MaxFirstPersonUserHeadingAdjust */
    { 0x116f0, 0x48, 0, 0x0095920c, 0x00959d44, { 0x7f7fffff } },   /* Other_FS_RollScale */
    { 0x11ae0, 0x48, 0, 0x00959220, 0x00959d58, { 0x00000000 } },   /* Effects_FS_CS_Jitter_On */
    { 0x11ed0, 0x48, 0, 0x00959238, 0x00959d70, { 0x7f7fffff } },   /* Effects_FS_CS_Jitter_Range_Min */
    { 0x122c0, 0x48, 0, 0x00959258, 0x00959d8c, { 0x7f7fffff } },   /* Effects_FS_CS_Jitter_Range_Max */
    { 0x126b0, 0xa0, 2, 0x00959278, 0x00959da8, { 0x00000000 } },   /* Effects_FS_CS_Jitter_Force */
    { 0x12f70, 0xa0, 2, 0x00959294, 0x00959dc4, { 0x00000000 } },   /* Effects_FS_CS_Jitter_Speed */
    { 0x13830, 0x60, 1, 0x009592b0, 0x00959de0, { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 } },   /* Effects_FS_CS_Jitter_Look_Basis */
    { 0x13d70, 0x60, 1, 0x009592d0, 0x00959e00, { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 } },   /* Effects_FS_CS_Jitter_Pos_Basis */
    { 0x142b0, 0x48, 0, 0x009592f0, 0x00959e20, { 0x7f7fffff } },   /* Effects_FS_CS_Jitter_Duration */
    { 0x146a0, 0x48, 0, 0x00959310, 0x00959e40, { 0x00000000 } },   /* Effects_FS_CS_Shake_On */
    { 0x14a90, 0x48, 0, 0x00959328, 0x00959e54, { 0x7f7fffff } },   /* Effects_FS_CS_Shake_Range_Min */
    { 0x14e80, 0x48, 0, 0x00959348, 0x00959e70, { 0x7f7fffff } },   /* Effects_FS_CS_Shake_Range_Max */
    { 0x15270, 0xa0, 2, 0x00959368, 0x00959e8c, { 0x00000000 } },   /* Effects_FS_CS_Shake_Force */
    { 0x15b30, 0xa0, 2, 0x00959384, 0x00959ea8, { 0x00000000 } },   /* Effects_FS_CS_Shake_Speed */
    { 0x163f0, 0x60, 1, 0x009593a0, 0x00959ec4, { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 } },   /* Effects_FS_CS_Shake_Look_Basis */
    { 0x16930, 0x60, 1, 0x009593c0, 0x00959ee4, { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 } },   /* Effects_FS_CS_Shake_Pos_Basis */
    { 0x16e70, 0x60, 1, 0x009593e0, 0x00959f04, { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 } },   /* Effects_FS_CS_Shake_Speed_Basis */
    { 0x173b0, 0x48, 0, 0x00959400, 0x00959f24, { 0x7f7fffff } },   /* Effects_FS_CS_Shake_Duration */
    { 0x177a0, 0x48, 0, 0x00959420, 0x00959f44, { 0x00000000 } },   /* Effects_FS_CS_SpeedShudder_On */
    { 0x17b90, 0x48, 0, 0x00959440, 0x00959f60, { 0x7f7fffff } },   /* Effects_FS_CS_SpeedShudder_Range_Min */
    { 0x17f80, 0x48, 0, 0x00959468, 0x00959f84, { 0x7f7fffff } },   /* Effects_FS_CS_SpeedShudder_Range_Max */
    { 0x18370, 0xa0, 2, 0x00959490, 0x00959fa8, { 0x00000000 } },   /* Effects_FS_CS_SpeedShudder_Force */
    { 0x18c30, 0xa0, 2, 0x009594b4, 0x00959fcc, { 0x00000000 } },   /* Effects_FS_CS_SpeedShudder_Speed */
    { 0x194f0, 0x60, 1, 0x009594d8, 0x00959ff0, { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 } },   /* Effects_FS_CS_SpeedShudder_Look_Basis */
    { 0x19a30, 0x60, 1, 0x00959500, 0x0095a018, { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 } },   /* Effects_FS_CS_SpeedShudder_Pos_Basis */
    { 0x19f70, 0x48, 0, 0x00959528, 0x0095a040, { 0x7f7fffff } },   /* Effects_FS_CS_SpeedShudder_Duration */
    { 0x1a360, 0x48, 0, 0x0095954c, 0x0095a064, { 0x00000000 } },   /* Other_FS_CL_Enable */
    { 0x1a750, 0x48, 0, 0x00959560, 0x0095a078, { 0x3f400000 } },   /* Other_FS_CL_MinStickDisplacement */
    { 0x1ab40, 0x48, 0, 0x00959584, 0x0095a09c, { 0x3f19999a } },   /* Other_FS_CL_MaxCameraTargetDisplacement */
    { 0x1af30, 0x48, 0, 0x009595ac, 0x0095a0c4, { 0x3f800000 } },   /* Other_FS_CL_ApproachTargetSpeed */
    { 0x1b320, 0x48, 0, 0x009595cc, 0x0095a0e4, { 0x40a00000 } },   /* Other_FS_CL_ReturnFromTargetSpeed */
    { 0x1b710, 0x48, 0, 0x009595f0, 0x0095a108, { 0x41a00000 } },   /* Other_FS_CL_MinCornerLookSpeed */
    { 0x1bb00, 0x48, 0, 0x00959610, 0x0095a128, { 0x3e19999a } },   /* Other_FS_CL_OnStickTimer */
    { 0x1bef0, 0x48, 0, 0x0095962c, 0x0095a144, { 0x00000000 } },   /* Other_FS_UL_Enable */
    { 0x1c2e0, 0x48, 0, 0x00959640, 0x0095a158, { 0xc1a00000 } },   /* Other_FS_UL_FallSpeedMin */
    { 0x1c6d0, 0x48, 0, 0x0095965c, 0x0095a174, { 0xbf000000 } },   /* Other_FS_UL_DownAngleMin */
    { 0x1cac0, 0x48, 0, 0x00959678, 0x0095a190, { 0x41a00000 } },   /* Other_FS_UL_SpeedMin */
    { 0x1ceb0, 0x48, 0, 0x00959690, 0x0095a1a8, { 0x3e99999a } },   /* Other_FS_UL_MaxLookUp */
    { 0x1d2a0, 0x48, 0, 0x009596a8, 0x0095a1c0, { 0x41200000 } },   /* Other_FS_UL_ApproachTargetSpeed */
    { 0x1d690, 0x48, 0, 0x009596c8, 0x0095a1e0, { 0x40000000 } },   /* Other_FS_UL_ReturnFromTargetSpeed */
};

/* 006596f0 (ECX = class record) -> EAX = the record: the 14 entry records, then every name-record block */
rx_addr rx_camera_schema_construct(rx_addr rec) {
    cam_schema_ctor_iter(rec, 0x404, 14, cam_schema_entry_ctor);
    for (uint32_t k = 0; k < CAM_SCHEMA_ROWS; k++) {
        const struct cam_schema_row* r = &cam_schema_rows[k];
        cam_schema_ctor_iter(rec + r->base, r->size, 14, r->size == 0xa0 ? cam_schema_curve_ctor : cam_schema_name_ctor);
    }
    rx_camera_schema_fill(rec);                   /* 00659ed0: push esi; call 00659ef0 */
    return rec;
}

/* 00659ef0 (stack = class record; RET 4): for each of the 7 camera types (00659ef0's iteration) the FS entry
 * (2 * type) then the SS entry (2 * type + 1): every row's record for that entry gets the side's name, the kind,
 * the default data and is appended to the entry's record list. */
void rx_camera_schema_fill(rx_addr rec) {
    for (uint32_t entry = 0; entry < 14; entry++) {
        const uint32_t ss = entry & 1u;
        for (uint32_t k = 0; k < CAM_SCHEMA_ROWS; k++) {
            const struct cam_schema_row* r = &cam_schema_rows[k];
            const rx_addr p = rec + r->base + entry * r->size;
            crt_strncpy(p, ss ? r->name_ss : r->name_fs, 0x3f);
            SU8(p, 0x3f, 0);
            SU32(p, 0x40, r->kind);
            if (r->kind == 0) SU32(p, 0x44, r->data[0]);
            else if (r->kind == 1) for (uint32_t i = 0; i < 4; i++) SU32(p, 0x50 + 4u * i, r->data[i]);
            const uint32_t n = U32(rec + entry * 0x404u, 0x400);
            SU32(rec + entry * 0x404u, 4u * n, p);
            SU32(rec + entry * 0x404u, 0x400, n + 1);
        }
    }
}

/* 00908500 (_initterm slot 0x009166e0 + 0x5f0): the nine class records 0x1da80 apart at 0x00af6d10, in order; then
 * the atexit(00912970) registration (the schema sdtor, nothing to free: the records are static). */
void rx_camera_schema_init(void) {
    for (uint32_t k = 0; k < 9; k++) {
        rx_camera_schema_construct(0x00af6d10u + 0x1da80u * k);   /* 006596f0 (fills too) */
    }
    RX_ATEXIT(0x00912970);                       /* _atexit(00912970): the schema sdtor (nothing to free) */
}
RX_FUNCTION(0x00908500, rx_camera_schema_init)
/* the helpers, for the oracle's per-call Verify of the parts (the original calls them from 00908500) */
RX_FUNCTION(0x006596f0, rx_camera_schema_construct)
RX_FUNCTION(0x00659ef0, rx_camera_schema_fill)
