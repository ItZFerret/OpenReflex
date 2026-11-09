/* loop_cam_build.c -- the player camera entities: creation (0054bc00) and the camera tuning apply (vf32) of the gameplay
 * cameras, hand transliterated from MXReflex.exe. Complements loop_cam_entity.c / loop_cam_player.c / loop_cam_tpc.c /
 * loop_cam_misc.c (the per-frame updates, already ported and verified) and build_scene_entity.c's 0063dae0.
 *
 * What the product was missing: the spawn's 0054bc00 (build_activity_spawn.c skipped it as a "camera unit" no-op), so
 * P+0x974..+0x984 / +0x998 / +0x99c stayed 0 and no camera entity existed for Player_UpdateCamera (0054c150) to write.
 * The render camera the old plugin read ([0xa7b1d8] -> the static Camera 0xd6f620, the scene's first view camera, is a
 * COPY of the active camera entity: 0063dba0 in Player_UpdateCameras, then Engine::Update's render block (0x84ef), then
 * ForestSystem::vf4 (0x6676ee) copies it to 0xa7b1d8. rx_session_camera reads the camera entity itself (its Camera at
 * +0x90), which is the object the original's camera update writes.
 *
 * Field layout (loop_cam_entity.c / _re/notes/game-camera.md): Camera +0x10 position, +0x20 right, +0x30 up, +0x40
 * forward, +0x50 fov, +0x54 aspect, +0x58 near, +0x5c far; MXCameraEntity embeds it at +0x90 and keeps +0x2d8 the camera
 * mode name (std::string), +0x2f4 "pose valid", +0x344 the controller (the MXPlayer), +0x36c the config record.
 *
 * 0054bc00's five 0063f160 calls use the player's camera config table P+0x910 + i*0x14 {localization key, mode name, type}:
 * type 1 = MXThirdPersonCamera, type 0 = MXFirstPersonCamera. The product makes all five view cameras (slots 0..4: the
 * first-person camera at slot 2, P+0x97c), the gate and the quick camera. The default MX camera is SLOT 1 ("GamePlay");
 * selecting the first-person camera runs its vf19 00642680 (loop_cam_firstperson.c). */
#include "rx_game.h"
#include "loop_cam_build.h"
#include "loop_cam.h"
#include "loop_cam_entity.h"
#include "loop_util.h"
#include "build_xml.h"
#include "build_mxworld.h"
#include "build_scene_entity.h"
#include "build_alloc.h"
#include "crt.h"
#include "bike_curve.h"
#include "loop_cam_tuning.h"
#include "world_wreck.h"

/* ======================================================================================================================== */
/* tuning load: 0063f240 + 0065bdb0 without the 0x180 render path records (0065bdb0 only reads the bxml and parses it)       */

/* 0065bdb0 (stack table, class id; RET): XMLReader on the frame, FUN_008251d0(path) loads "Cameras\<class>_Camera.bxml",
 * 0065c200 parses it into the class's 0x1da80-byte record. The load goes through the engine's asset system in the
 * original (008251d0 -> a task + LoadFromBXMLFromPath); the product reads the same install file and parses it with the
 * ported reader (build_xml.c) - the bytes and the parser are the same. */
static int cam_tuning_load(const struct fmt_vfs* vfs, rx_addr table, const char* path) {
    const rx_addr sp0 = rx_sp();
    const rx_addr F = sp0 - 0x530u;
    rx_set_sp(F);
    rx_xml_reader_construct(F + 0x10u);
    int ok = 0;
    if (rx_xml_reader_load_file(vfs, F + 0x10u, path)) ok = (int)rx_cam_tuning_parse(table, F + 0x10u);
    rx_xml_reader_destroy(F + 0x10u);
    rx_set_sp(sp0);
    return ok;
}

int rx_camera_tuning_load_all(const struct fmt_vfs* vfs) {
    /* 0063f240's six 0065bdb0 calls in order (record, the class id that picks the file). (0x00d72ad4 is the vehicle
     * tuning registry's one-time bit, shared by 0065bdb0 only for 00790400; it is already set by the vehicle startup, so
     * it must NOT guard the camera file load.) */
    const int mx = cam_tuning_load(vfs, 0x00af6d10u, "Cameras\\MX_Camera.bxml");
    cam_tuning_load(vfs, 0x00b14790u, "Cameras\\atv_Camera.bxml");
    cam_tuning_load(vfs, 0x00b4fc90u, "Cameras\\PROTruck_Camera.bxml");
    cam_tuning_load(vfs, 0x00b8b190u, "Cameras\\PRO2Truck_Camera.bxml");
    cam_tuning_load(vfs, 0x00bc6690u, "Cameras\\Buggy_Camera.bxml");
    cam_tuning_load(vfs, 0x00be4110u, "Cameras\\UTV_Camera.bxml");
    return mx;
}

/* ======================================================================================================================== */
/* the tuning index: the class record of the followed vehicle, the mode bit and the camera mode index                     */

/* 0064a330 / 00647700 / 00646b70 prologue: class = V+0x194; the subclass 3 (MX) is widened: (subclass != 8 ? 5 : 0) + 3 */
static uint32_t cam_tuning_class(rx_addr cam) {
    const rx_addr v = U32(U32(cam, 0x344u), 0x1acu);
    uint32_t c = U32(v, 0x194u);
    if (c == 3u) c = ((U32(v, 0x170u) != 8u) ? 5u : 0u) + 3u;
    return c;
}
/* MXGame+0x15b24 == 2 -> 1, else 0 (the calibration's two table columns) */
static uint32_t cam_tuning_column(void) { return U32(0, 0x00a946b8u + 0x15b24u) == 2u ? 1u : 0u; }
static uint32_t cam_tuning_index(rx_addr cam, uint32_t stride) {
    return cam_tuning_class(cam) * 0x1da80u + (cam_tuning_column() + rx_world_camera_mode_index(cam) * 2u) * stride;
}

/* ======================================================================================================================== */
/* vf32 (slot 0x80): the camera tuning record -> the camera fields. 0064a330 (third person) is ~68 field loads from the    */
/* class record; the decompile repeats the prologue for each field, the values are the same, so the index is computed once. */
/* The kinds: D a 4-byte copy, X a float through fld/fstp (cam_x87), M a float * a constant through mulss, B a byte        */
/* (0 < f), V four dwords.                                                                                                  */
enum { CT_D = 0, CT_X, CT_M, CT_B, CT_V };
struct cam_tune_row { uint16_t off; uint8_t kind; uint32_t base; float scale; };

static const struct cam_tune_row third_tune_rows[] = {
    { 0x480, CT_D, 0x00b060d4u, 0 }, { 0x484, CT_D, 0x00b064c4u, 0 },
    { 0x488, CT_D, 0x00b068b4u, 0 }, { 0x4d0, CT_D, 0x00afd314u, 0 },
    { 0x4e0, CT_M, 0x00afd704u, 0.01745329052209854f }, { 0x4e4, CT_D, 0x00afdaf4u, 0 },
    { 0x4e8, CT_M, 0x00b00e24u, 0.01745329052209854f }, { 0x4ec, CT_M, 0x00b01214u, 0.01745329052209854f },
    { 0x4f0, CT_M, 0x00b01604u, 0.01745329052209854f }, { 0x500, CT_V, 0x00afb640u, 0 },
    { 0x510, CT_D, 0x00afe6c4u, 0 }, { 0x514, CT_D, 0x00b019f4u, 0 },
    { 0x518, CT_D, 0x00b01de4u, 0 }, { 0x51c, CT_D, 0x00b021d4u, 0 },
    { 0x520, CT_D, 0x00b025c4u, 0 }, { 0x524, CT_D, 0x00b029b4u, 0 },
    { 0x528, CT_D, 0x00b02da4u, 0 }, { 0x52c, CT_D, 0x00afeab4u, 0 },
    { 0x530, CT_D, 0x00afeea4u, 0 }, { 0x534, CT_D, 0x00aff294u, 0 },
    { 0x538, CT_D, 0x00aff684u, 0 }, { 0x53c, CT_D, 0x00affa74u, 0 },
    { 0x540, CT_D, 0x00affe64u, 0 }, { 0x544, CT_D, 0x00b00254u, 0 },
    { 0x548, CT_D, 0x00b00644u, 0 }, { 0x54c, CT_D, 0x00b00a34u, 0 },
    { 0x550, CT_D, 0x00afdee4u, 0 }, { 0x554, CT_M, 0x00b04154u, 0.01745329052209854f },
    { 0x558, CT_D, 0x00b04544u, 0 }, { 0x55c, CT_M, 0x00b04934u, 0.01745329052209854f },
    { 0x560, CT_D, 0x00b04d24u, 0 }, { 0x564, CT_D, 0x00b05114u, 0 },
    { 0x568, CT_D, 0x00b05504u, 0 }, { 0x56c, CT_D, 0x00b058f4u, 0 },
    { 0x570, CT_D, 0x00b05ce4u, 0 }, { 0x574, CT_D, 0x00b06ca4u, 0 },
    { 0x578, CT_B, 0x00b110b4u, 0 }, { 0x57c, CT_D, 0x00b114a4u, 0 },
    { 0x580, CT_D, 0x00b11894u, 0 }, { 0x584, CT_D, 0x00b11c84u, 0 },
    { 0x588, CT_D, 0x00b12074u, 0 }, { 0x58c, CT_D, 0x00b12464u, 0 },
    { 0x590, CT_D, 0x00b12854u, 0 }, { 0x594, CT_B, 0x00b12c44u, 0 },
    { 0x598, CT_D, 0x00b13034u, 0 }, { 0x59c, CT_D, 0x00b13424u, 0 },
    { 0x5a0, CT_D, 0x00b13814u, 0 }, { 0x5a4, CT_D, 0x00b13c04u, 0 },
    { 0x5a8, CT_D, 0x00b13ff4u, 0 }, { 0x5ac, CT_D, 0x00b143e4u, 0 },
    { 0x5f8, CT_B, 0x00b08834u, 0 }, { 0x5fc, CT_M, 0x00b08c24u, 1.4666670560836792f },
    { 0x600, CT_M, 0x00b09014u, 1.4666670560836792f }, { 0x604, CT_D, 0x00b0b004u, 0 },
    { 0x608, CT_B, 0x00b0b3f4u, 0 }, { 0x60c, CT_D, 0x00b0b7e4u, 0 },
    { 0x610, CT_D, 0x00b0bbd4u, 0 }, { 0x614, CT_D, 0x00b0e104u, 0 },
    { 0x618, CT_B, 0x00b0e4f4u, 0 }, { 0x61c, CT_M, 0x00b0e8e4u, 1.4666670560836792f },
    { 0x620, CT_M, 0x00b0ecd4u, 1.4666670560836792f }, { 0x624, CT_D, 0x00b10cc4u, 0 },
    { 0x630, CT_V, 0x00b0d150u, 0 }, { 0x640, CT_V, 0x00b0d690u, 0 },
    { 0x650, CT_V, 0x00b0dbd0u, 0 }, { 0x660, CT_V, 0x00b10250u, 0 },
    { 0x670, CT_V, 0x00b10790u, 0 }, { 0x680, CT_V, 0x00b0a590u, 0 },
    { 0x690, CT_V, 0x00b0aad0u, 0 },
};

/* the row reader of the three vf32 functions (see the kinds above) */
static void cam_tune_apply(rx_addr cam, const struct cam_tune_row* rows, uint32_t n) {
    const uint32_t cls = cam_tuning_class(cam);
    const uint32_t col = cam_tuning_column();
    const uint32_t mode = rx_world_camera_mode_index(cam);
    for (uint32_t i = 0; i < n; i++) {
        const struct cam_tune_row* r = &rows[i];
        if (r->kind == CT_V) {
            const rx_addr s = r->base + cls * 0x1da80u + (col + mode * 2u) * 0x60u;
            for (uint32_t k = 0; k < 4; k++) SU32(cam, r->off + 4u * k, U32(s, 4u * k));
        } else {
            const rx_addr a = r->base + cls * 0x1da80u + (col + mode * 2u) * 0x48u;
            switch (r->kind) {
            case CT_D: SU32(cam, r->off, U32(a, 0)); break;
            case CT_X: SU32(cam, r->off, cam_x87_f32(a)); break;
            case CT_M: SF32(cam, r->off, F32(a, 0) * r->scale); break;
            case CT_B: SU8(cam, r->off, (0.0f < F32(a, 0)) ? 1 : 0); break;
            }
        }
    }
}

/* 00646b70 MXPlayerCamera::vf32 (ECX = camera): +0x310 four dwords from the class record (row stride 0x60) */
void rx_player_camera_vf32(rx_addr cam) {
    const uint32_t idx = cam_tuning_index(cam, 0x60u);
    for (uint32_t k = 0; k < 4; k++) SU32(cam, 0x310u + 4u * k, U32(0x00afbb80u + idx, 4u * k));
    (void)0;
}
RX_FUNCTION(0x00646b70, rx_player_camera_vf32)

/* 0064a330 MXThirdPersonCamera::vf32 (ECX = camera): MXPlayerCamera::vf32, the camera's fov curve (0xafa97c, evaluated
 * at 0 -> 0x4cc), the field table, then the derived fov fields (+0xe0 / +0x5c4 / +0x5c8). */
void rx_third_person_camera_vf32(rx_addr cam) {
    rx_player_camera_vf32(cam);
    const uint32_t cls = cam_tuning_class(cam);
    const uint32_t row = cam_tuning_column() + rx_world_camera_mode_index(cam) * 2u;
    /* 0064a395: xorps xmm0, xmm0; lea esi,[edx+esi+0xafa97c]; call ParamCurve_EvalCubic; mulss xmm0,[0x977678] */
    const float fov = bike_param_curve(0x00afa97cu + cls * 0x1da80u + row * 0xa0u, 0.0f) * F32(0, 0x00977678u);
    SF32(cam, 0x4cc, fov);
    cam_tune_apply(cam, third_tune_rows, (uint32_t)(sizeof third_tune_rows / sizeof *third_tune_rows));
    /* 0064b861..: the fov is copied to the Camera (+0xe0) and the two derived lens fields; +0x164..+0x16c cleared */
    SU32(cam, 0xe0, U32(cam, 0x4cc));
    SU32(cam, 0x164, 0); SU32(cam, 0x168, 0); SU32(cam, 0x16c, 0);
    SU32(cam, 0x5c4, U32(cam, 0x4cc));
    SF32(cam, 0x5c8, F32(cam, 0x4cc) * F32(0, 0x00977810u));
}
RX_FUNCTION(0x0064a330, rx_third_person_camera_vf32)

/* 00647700 MXQuickCamera::vf32 (ECX = camera): the fov from 0xafa58c (degrees -> radians), +0x164..+0x16c cleared, the
 * +0x350 / +0x360 vec4s, the +0x374 and +0x370 fields. */
void rx_quick_camera_vf32(rx_addr cam) {
    rx_player_camera_vf32(cam);
    const uint32_t cls = cam_tuning_class(cam);
    const uint32_t row = cam_tuning_column() + rx_world_camera_mode_index(cam) * 2u;
    SF32(cam, 0xe0, F32(0x00afa58cu + cls * 0x1da80u + row * 0x48u, 0) * F32(0, 0x00977678u));
    SU32(cam, 0x164, 0); SU32(cam, 0x16c, 0); SU32(cam, 0x168, 0);
    for (uint32_t k = 0; k < 4; k++) SU32(cam, 0x350u + 4u * k, U32(0x00afc0c0u + cls * 0x1da80u + row * 0x60u + 4u * k, 0));
    for (uint32_t k = 0; k < 4; k++) SU32(cam, 0x360u + 4u * k, U32(0x00afc600u + cls * 0x1da80u + row * 0x60u + 4u * k, 0));
    SU32(cam, 0x374, U32(0x00b06ca4u + cls * 0x1da80u + row * 0x48u, 0));
    SU32(cam, 0x370, U32(0x00afd314u + cls * 0x1da80u + row * 0x48u, 0));
}
RX_FUNCTION(0x00647700, rx_quick_camera_vf32)

/* ======================================================================================================================== */
/* the constructors                                                                                                        */

/* a helper: the four dwords of an identity quaternion at 0xd61670 */
static void cam_copy_ident4(rx_addr cam, uint32_t off) {
    for (uint32_t k = 0; k < 4; k++) SU32(cam, off + 4u * k, U32(0, 0x00d61670u + 4u * k));
}

/* 00646730 MXPlayerCamera (ECX = camera, stack controller; RET 4): the MXCameraEntity base (0063d930), the controller
 * +0x344, the vtable, +0x340 = 1, and the last forward / position from 0x9cb4c0..c8 / 0xa95d50..58. */
rx_addr rx_player_camera_construct(rx_addr cam, rx_addr p) {
    rx_camera_entity_construct(cam);
    SU32(cam, 0x344, p);
    SU32(cam, 0, 0x0095a774u);
    SU8(cam, 0x340, 1);
    for (uint32_t k = 0; k < 3; k++) SU32(cam, 0x320u + 4u * k, U32(0, 0x009cb4c0u + 4u * k));
    for (uint32_t k = 0; k < 3; k++) SU32(cam, 0x330u + 4u * k, U32(0, 0x00a95d50u + 4u * k));
    return cam;
}
RX_FUNCTION(0x00646730, rx_player_camera_construct)

/* 00641280 MXFirstPersonCamera (ECX = camera, EAX = controller; stack the config record; RET 4): MXPlayerCamera, the vtable
 * 0x0095a80c, +0x340 = 1 (from 00646730), the config at +0x36c, +0x350..+0x368 and +0x380..+0x388 zero, the basis rows
 * +0x460 / +0x470 / +0x480 (x / y / z from 0x9cb4c0 / 0x9cb4b0 / 0x9cb4a0), the pitch / heading limits +0x390 / +0x394
 * (20 / 80 degrees), +0x3c4 = 0.5, +0x3c8 / +0x38c / +0x3cc = 0 and four quaternions +0x3f0 / +0x400 / +0x410 / +0x420
 * (x, y, z, 1). The factory's type 0 (0063f160) allocates 0x490 bytes for it; 0054bc00 puts it at P+0x97c. Its per-frame
 * update is vf19 00642680 (loop_cam_firstperson.c). */
rx_addr rx_first_person_camera_construct(rx_addr cam, rx_addr p, rx_addr desc) {
    rx_player_camera_construct(cam, p);
    SU32(cam, 0x350, 0); SU32(cam, 0x354, 0); SU32(cam, 0x358, 0); SU32(cam, 0x35c, 0);
    SU32(cam, 0x360, 0); SU32(cam, 0x364, 0); SU32(cam, 0x368, 0);
    SU32(cam, 0, 0x0095a80cu);
    SU32(cam, 0x36c, desc);
    SU32(cam, 0x380, 0); SU32(cam, 0x384, 0); SU32(cam, 0x388, 0);
    for (uint32_t k = 0; k < 3; k++) SU32(cam, 0x460u + 4u * k, U32(0, 0x009cb4c0u + 4u * k));
    for (uint32_t k = 0; k < 3; k++) SU32(cam, 0x470u + 4u * k, U32(0, 0x009cb4b0u + 4u * k));
    for (uint32_t k = 0; k < 3; k++) SU32(cam, 0x480u + 4u * k, U32(0, 0x009cb4a0u + 4u * k));
    SU32(cam, 0x3c4, 0x3f000000u);                       /* 0x9322cc: 0.5 */
    SF32(cam, 0x390, 0.3490658104419708f);               /* 0x977970: 20 degrees */
    SF32(cam, 0x394, 1.3962632417678833f);               /* 0x977aac: 80 degrees */
    SU32(cam, 0x3cc, 0);
    SU8(cam, 0x3c8, 0);
    SU8(cam, 0x38c, 0);
    SU32(cam, 0x3f0, 0); SU32(cam, 0x3f4, 0); SU32(cam, 0x3f8, 0); SU32(cam, 0x3fc, 0x3f800000u);
    SU32(cam, 0x400, 0); SU32(cam, 0x404, 0); SU32(cam, 0x408, 0); SU32(cam, 0x40c, 0x3f800000u);
    SU32(cam, 0x420, 0); SU32(cam, 0x424, 0); SU32(cam, 0x428, 0); SU32(cam, 0x42c, 0x3f800000u);
    SU32(cam, 0x410, 0); SU32(cam, 0x414, 0); SU32(cam, 0x418, 0); SU32(cam, 0x41c, 0x3f800000u);
    return cam;
}
RX_FUNCTION(0x00641280, rx_first_person_camera_construct)

/* 00407410 (EAX = row index, ECX = source vec3, stack the 4x4; RET 0): row index of the matrix = the vec3, each component
 * through x87 fld / fstp (a signalling NaN comes out quiet, as everywhere in the camera code). */
void rx_cam_pose_row(uint32_t index, rx_addr src, rx_addr rows) {
    const rx_addr d = rows + 16u * index;
    cam_x87_copy(d, src);
    cam_x87_copy(d + 4, src + 4);
    cam_x87_copy(d + 8, src + 8);
}
RX_FUNCTION(0x00407410, rx_cam_pose_row)

/* 00649610 MXThirdPersonCamera (ECX = camera, stack controller, config; RET 4): MXPlayerCamera, then ~0x750 bytes of
 * zeros / constants / class-record-free defaults. The stores are the original's, in its order; the copies of the identity
 * quaternion (0xd61670) go to +0x6c0 / +0x6d0 / +0x6e0 / +0x6f0. */
rx_addr rx_third_person_camera_construct(rx_addr cam, rx_addr p, rx_addr desc) {
    rx_player_camera_construct(cam, p);
    SU32(cam, 0x350, 0); SU32(cam, 0x354, 0); SU32(cam, 0x358, 0); SU32(cam, 0x35c, 0);
    SU32(cam, 0x360, 0); SU32(cam, 0x364, 0); SU32(cam, 0x368, 0);
    SU32(cam, 0, 0x0095a8a4u);
    SU32(cam, 0x36c, desc);
    SU8(cam, 0x390, 0);
    for (uint32_t k = 0; k < 3; k++) SU32(cam, 0x3a0u + 4u * k, U32(0, 0x00a95d50u + 4u * k));
    { static const uint16_t z[] = {
        0x3b0, 0x3b4, 0x3b8, 0x3bc, 0x3c0, 0x3c4, 0x3c8, 0x3cc,
        0x3f4, 0x3f8, 0x3fc, 0x400, 0x404, 0x408, 0x40c, 0x410, 0x414, 0x418,
        0x420, 0x424, 0x428, 0x42c, 0x430, 0x434, 0x438, 0x43c, 0x440, 0x444, 0x448, 0x44c,
        0x450, 0x454, 0x458, 0x45c, 0x460, 0x464, 0x468, 0x46c, 0x470, 0x474, 0x478, 0x47c,
        0x490, 0x494, 0x498, 0x4b0, 0x4b4, 0x4b8, 0x4c0, 0x4c4, 0x4c8 };
      for (unsigned k = 0; k < sizeof z / sizeof *z; k++) SU32(cam, z[k], 0); }
    SU32(cam, 0x5b4, 0x3f000000u); SU32(cam, 0x5b0, 0);
    SU32(cam, 0x5bc, 0x7f7fffffu); SU32(cam, 0x5c0, 0xff7fffffu);
    SU32(cam, 0x5b8, 0x3f800000u); SU32(cam, 0x5c4, 0x3f75be09u); SU32(cam, 0x5c8, 0x3fb2b8c1u);
    SU32(cam, 0x4d4, U32(0, 0x009cf5b0u));           /* g_SavedGameMinDisplayMs (1000.0) */
    SU32(cam, 0x4d8, 0x40800000u); SU32(cam, 0x4dc, 0x497423f0u);
    SU32(cam, 0x5cc, 0); SU32(cam, 0x5e0, 0); SU32(cam, 0x5e4, 0); SU32(cam, 0x5e8, 0);
    SU8(cam, 0x5ec, 0); SU8(cam, 0x5ed, 1);
    SU32(cam, 0x5f0, 0); SU32(cam, 0x5f4, 0);
    SU8(cam, 0x391, 0); SU8(cam, 0x392, 0);
    SU32(cam, 0x4a0, 0); SU32(cam, 0x4a4, 0); SU32(cam, 0x4a8, 0);
    SU32(cam, 0x490, U32(cam, 0x4a0)); SU32(cam, 0x494, U32(cam, 0x4a4)); SU32(cam, 0x498, U32(cam, 0x4a8));
    SU32(cam, 0x4b0, 0); SU32(cam, 0x4b4, 0); SU32(cam, 0x4b8, 0);
    SU32(cam, 0x4c4, U32(0, 0x009cd9e8u)); SU32(cam, 0x4c0, U32(cam, 0x4c4)); SU32(cam, 0x4c8, 0);
    SU8(cam, 0x6a0, 0);
    cam_copy_ident4(cam, 0x6c0); cam_copy_ident4(cam, 0x6d0);
    SU32(cam, 0x6a8, 0x3f800000u); SU32(cam, 0x6a4, 0);
    cam_copy_ident4(cam, 0x6e0); cam_copy_ident4(cam, 0x6f0);
    SU32(cam, 0x6ac, 0); SU32(cam, 0x6b0, 0);
    SU32(cam, 0x73c, 0x3f800000u);
    SU32(cam, 0x730, 0); SU32(cam, 0x734, 0); SU32(cam, 0x738, 0); SU32(cam, 0x740, 0); SU32(cam, 0x744, 0); SU32(cam, 0x748, 0);
    SU32(cam, 0x704, 0);
    for (uint32_t off = 0x630; off <= 0x69c; off += 4) SU32(cam, off, 0);
    SU8(cam, 0x5f8, 0); SU32(cam, 0x5fc, 0); SU32(cam, 0x600, 0);
    SU8(cam, 0x608, 0); SU32(cam, 0x60c, 0); SU32(cam, 0x610, 0);
    SU8(cam, 0x618, 0); SU32(cam, 0x61c, 0); SU32(cam, 0x620, 0);
    SU8(cam, 0x578, 0); SU32(cam, 0x57c, 0); SU32(cam, 0x580, 0); SU32(cam, 0x584, 0);
    SU32(cam, 0x588, 0); SU32(cam, 0x58c, 0); SU32(cam, 0x590, 0); SU8(cam, 0x594, 0);
    SU32(cam, 0x598, 0); SU32(cam, 0x59c, 0); SU32(cam, 0x5a0, 0); SU32(cam, 0x5a4, 0); SU32(cam, 0x5a8, 0); SU32(cam, 0x5ac, 0);
    SU8(cam, 0x700, 0);
    {   /* the ctor's own name; 0063f160 replaces it with the config's mode name before vf32 */
        const rx_addr sp0 = rx_sp();
        const rx_addr S = sp0 - 0x20u;
        rx_set_sp(S);
        crt_str_ctor_cstr(S, 0x0092d0efu /* "MXThirdPersonCamera" */);
        crt_str_assign_sub(cam + 0x2d8u, S, 0, 0xffffffffu);
        crt_str_dtor(S);
        rx_set_sp(sp0);
    }
    return cam;
}
RX_FUNCTION(0x00649610, rx_third_person_camera_construct)

/* 00647cf0 MXStartGateCamera (ECX = camera, EAX = controller; RET 0): MXPlayerCamera + the gate defaults. No tuning apply
 * at construction (the original's 00647cf0 does not call vf32; the observed gate fov / near / far are the Camera base's
 * 45 deg / 2 / 15000). */
rx_addr rx_start_gate_camera_construct(rx_addr cam, rx_addr p) {
    rx_player_camera_construct(cam, p);
    SU32(cam, 0, 0x0095a644u);
    SU32(cam, 0x3a0, 0); SU32(cam, 0x3a4, 0); SU32(cam, 0x3a8, 0);
    SU32(cam, 0x3b0, 0); SU32(cam, 0x3b8, 0); SU32(cam, 0x3bc, 0); SU32(cam, 0x3c0, 0);
    SU32(cam, 0x3c4, 0); SU32(cam, 0x3c8, 0); SU32(cam, 0x3cc, 0);
    SU32(cam, 0x350, 0); SU32(cam, 0x354, 0);
    SU32(cam, 0x370, 0x3f800000u);
    SU32(cam, 0x380, 0);
    SU32(cam, 0x374, 0x40400000u);
    SU32(cam, 0x384, U32(0, 0x009d78b8u));
    SU32(cam, 0x378, 0xc0c00000u);
    SU32(cam, 0x388, U32(0, 0x009d7b34u));
    SU32(cam, 0x398, 0);
    for (uint32_t k = 0; k < 3; k++) SU32(cam, 0x360u + 4u * k, U32(0, 0x00a95d50u + 4u * k));
    return cam;
}
RX_FUNCTION(0x00647cf0, rx_start_gate_camera_construct)

/* 00646d70 MXQuickCamera (ECX = camera, EAX = controller; RET 0): MXPlayerCamera, the name "QuickBack", vf32 (the quick
 * camera's own tuning apply at the end of the ctor), and the two last fields. */
rx_addr rx_quick_camera_construct(rx_addr cam, rx_addr p) {
    rx_player_camera_construct(cam, p);
    SU32(cam, 0, 0x0095a5acu);
    SU32(cam, 0x380, 0); SU32(cam, 0x384, 0); SU32(cam, 0x388, 0);
    SU32(cam, 0x390, 0); SU32(cam, 0x394, 0); SU32(cam, 0x398, 0);
    SU32(cam, 0x3a0, 0); SU32(cam, 0x3a4, 0); SU32(cam, 0x3a8, 0);
    SU32(cam, 0x378, 2);
    {   /* "QuickBack" */
        const rx_addr sp0 = rx_sp();
        const rx_addr S = sp0 - 0x20u;
        rx_set_sp(S);
        crt_str_ctor_cstr(S, 0x00940f87u /* "QuickBack" */);
        crt_str_assign_sub(cam + 0x2d8u, S, 0, 0xffffffffu);
        crt_str_dtor(S);
        rx_set_sp(sp0);
    }
    loop_tc0(loop_vslot(cam, 0x80u), cam);
    SU32(cam, 0x350, 0); SU32(cam, 0x358, 0); SU32(cam, 0x360, 0);
    SU32(cam, 0x354, 0x40c00000u); SU32(cam, 0x364, 0x40c00000u);
    SU32(cam, 0x368, U32(0, 0x009cd9e8u));           /* ScrollReverse_MinusOne */
    loop_tc_f(loop_vslot(cam, 0x4cu), cam, rx_bf(0x3d088889u));
    return cam;
}
RX_FUNCTION(0x00646d70, rx_quick_camera_construct)

/* ======================================================================================================================== */
/* the class methods the camera path calls through the vtable                                                               */

/* 0063de60 MXCameraEntity::vf28: "pose valid" +0x2f4 = 1 */
void rx_camera_entity_vf28(rx_addr cam) { SU32(cam, 0x2f4, 1); }
RX_FUNCTION(0x0063de60, rx_camera_entity_vf28)

/* 00649bd0 MXThirdPersonCamera::vf28 (ECX = camera): the transition springs reset and two vf19 updates at dt 1.0 */
void rx_third_person_camera_vf28(rx_addr cam) {
    SU32(cam, 0x2f4, 1);
    SU8(cam, 0x391, 1);
    loop_tc_f(loop_vslot(cam, 0x4cu), cam, rx_bf(0x3f800000u));
    SU32(cam, 0x3b4, U32(cam, 0x3b8));
    SU32(cam, 0x3bc, 0);
    SU32(cam, 0x358, 0);
    SU32(cam, 0x350, U32(cam, 0x354));
    SU32(cam, 0x364, 0);
    SU32(cam, 0x35c, U32(cam, 0x360));
    SU32(cam, 0x400, 0); SU32(cam, 0x3f8, U32(cam, 0x3fc));
    SU32(cam, 0x40c, 0); SU32(cam, 0x404, U32(cam, 0x408));
    SU32(cam, 0x418, 0); SU32(cam, 0x410, U32(cam, 0x414));
    SU32(cam, 0x428, 0); SU32(cam, 0x420, U32(cam, 0x424));
    SU32(cam, 0x434, 0); SU32(cam, 0x42c, U32(cam, 0x430));
    SU32(cam, 0x440, 0); SU32(cam, 0x438, U32(cam, 0x43c));
    SU32(cam, 0x44c, 0); SU32(cam, 0x444, U32(cam, 0x448));
    SU32(cam, 0x458, 0); SU32(cam, 0x450, U32(cam, 0x454));
    SU32(cam, 0x464, 0); SU32(cam, 0x45c, U32(cam, 0x460));
    SU32(cam, 0x470, 0); SU32(cam, 0x468, U32(cam, 0x46c));
    SU32(cam, 0x47c, 0); SU32(cam, 0x474, U32(cam, 0x478));
    SU32(cam, 0x490, 0); SU32(cam, 0x494, 0); SU32(cam, 0x498, 0);
    SU32(cam, 0x4a0, 0); SU32(cam, 0x4a4, 0); SU32(cam, 0x4a8, 0);
    SU32(cam, 0x3fc, 0); SU32(cam, 0x408, 0); SU32(cam, 0x414, 0);
    SU32(cam, 0x4c4, U32(0, 0x009cd9e8u)); SU32(cam, 0x4c0, U32(cam, 0x4c4)); SU32(cam, 0x4c8, 0);
    SU32(cam, 0x490, U32(cam, 0x4a0)); SU32(cam, 0x494, U32(cam, 0x4a4)); SU32(cam, 0x498, U32(cam, 0x4a8));
    SU32(cam, 0x4b0, 0); SU32(cam, 0x4b4, 0); SU32(cam, 0x4b8, 0);
    SU32(cam, 0x3f4, 0);
    loop_tc_f(loop_vslot(cam, 0x4cu), cam, rx_bf(0x3f800000u));
    SU8(cam, 0x391, 0);
}
RX_FUNCTION(0x00649bd0, rx_third_person_camera_vf28)

/* 00647e00 MXStartGateCamera::vf28 (ECX = camera): the gate pose (00647e40), +0x350 = 60, "pose valid" +0x2f4 = 1,
 * +0x354 = 0, +0x398 = 1 */
void rx_start_gate_camera_vf28(rx_addr cam) {
    rx_start_gate_camera_place(cam);
    SU32(cam, 0x350, 0x42700000u);
    SU32(cam, 0x2f4, 1);
    SU32(cam, 0x354, 0);
    SU32(cam, 0x398, 1);
}
RX_FUNCTION(0x00647e00, rx_start_gate_camera_vf28)

/* 00647e40 (ESI = camera): the gate camera's pose from the vehicle: the vehicle position (vf17) plus the vehicle's
 * forward / up basis (V +0x130 / +0x140 and V +0x144 / +0x148) scaled by the camera's +0x370..+0x388: the look point
 * (+0x360) at +0x384 / +0x388 and the eye (+0x370) at +0x374 / +0x378, then look_at(eye, point). The basis rows are
 * r = (V144, V134, V130) x (V148, V138, V140), normalized. */
void rx_start_gate_camera_place(rx_addr cam) {
    const rx_addr sp0 = rx_sp();
    const rx_addr A = (sp0 - 4u) & ~0xfu;
    const rx_addr E = A - 0x70u;
    rx_set_sp(E);
    const rx_addr v = U32(U32(cam, 0x344u), 0x1acu);
    loop_tc1(loop_vslot(v, 0x44u), v, E + 0x50u);      /* vehicle vf17 -> E+0x50 */
    const float v144 = F32(v, 0x144), v148 = F32(v, 0x148), v134 = F32(v, 0x134), v138 = F32(v, 0x138);
    const float v130 = F32(v, 0x130), v140 = F32(v, 0x140);
    float cx = (v144 * v138) - (v148 * v134);
    float cy = (v148 * v130) - (v138 * v140);
    float cz = (v134 * v140) - (v130 * v144);
    const float len2 = (cx * cx + cy * cy) + cz * cz;
    if (len2 > 0.0f) {
        const float inv = 1.0f / rx_sqrts(len2);
        cx *= inv; cy *= inv; cz *= inv;
    }
    const float c388 = F32(cam, 0x388), c384 = F32(cam, 0x384), c380 = F32(cam, 0x380);
    const float c378 = F32(cam, 0x378), c374 = F32(cam, 0x374), c370 = F32(cam, 0x370);
    const float px = F32(E, 0x50), py = F32(E, 0x54), pz = F32(E, 0x58);
    /* +0x360: the look point (pos + corner) */
    SF32(cam, 0x360, ((px + ((0.0f - cx) * c380)) + (v140 * c384)) + (v130 * c388));
    SF32(cam, 0x364, ((py + ((0.0f - cy) * c380)) + (v144 * c384)) + (v134 * c388));
    SF32(cam, 0x368, ((pz + ((0.0f - cz) * c380)) + (v148 * c384)) + (v138 * c388));
    /* +0x370: the eye */
    const float ex = (((0.0f - cx) * c370) + px) + (v140 * c374) + (v130 * c378);
    const float ey = (((0.0f - cy) * c370) + py) + (v144 * c374) + (v134 * c378);
    const float ez = (((0.0f - cz) * c370) + pz) + (v148 * c374) + (v138 * c378);
    SF32(E, 0x60, ex); SF32(E, 0x64, ey); SF32(E, 0x68, ez);
    rx_cam_entity_look_at(E + 0x60u, cam + 0x360u, cam);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00647e40, rx_start_gate_camera_place)

/* 006534e0 MXThirdPersonCamera::vf35 (ECX = camera): +0x6a0 = 0 */
void rx_third_person_camera_vf35(rx_addr cam) { SU8(cam, 0x6a0, 0); }
RX_FUNCTION(0x006534e0, rx_third_person_camera_vf35)

/* 0063f110 MXPlayerCamera::vf36 (ECX = camera): +0x340 = 1 */
void rx_player_camera_vf36(rx_addr cam) { SU8(cam, 0x340, 1); }
RX_FUNCTION(0x0063f110, rx_player_camera_vf36)

/* ======================================================================================================================== */
/* 0063f160: the camera factory (EAX = config record {localization key, mode name, type}; stack the player; caller cleans)   */

rx_addr rx_camera_factory(rx_addr desc, rx_addr p) {
    rx_addr cam = 0;
    const uint32_t type = U32(desc, 8);
    if (type == 1u) {                                   /* MXThirdPersonCamera, 0x750 bytes (0063f120) */
        const rx_addr mem = rx_reflex_aligned_alloc(0x750);   /* 0078fbe0 ReflexAlignedAlloc */
        if (mem) cam = rx_third_person_camera_construct(mem, p, desc);
    } else if (type == 0u) {                            /* MXFirstPersonCamera, 0x490 bytes (00641280) */
        const rx_addr mem = rx_reflex_aligned_alloc(0x490);
        if (mem) cam = rx_first_person_camera_construct(mem, p, desc);
    } else {
        /* 0063f160's "%d is not a valid preset camera type" (0078faf0) path: param_1 (= desc) unchanged and no camera. */
        cam = 0;
    }
    if (!cam) return 0;
    {
        const rx_addr sp0 = rx_sp();
        const rx_addr S = sp0 - 0x20u;
        rx_set_sp(S);
        crt_str_ctor_cstr(S, U32(desc, 4u));
        crt_str_assign_sub(cam + 0x2d8u, S, 0, 0xffffffffu);
        rx_set_sp(sp0);
    }
    loop_tc0(loop_vslot(cam, 0x80u), cam);              /* vf32 */
    {
        /* 0063dae0: the entity description, the scene id, the scene's add. The original passes the vehicle's scene
         * (P+0x1ac -> +0x188 -> +0x20138), or 0 when there is none. */
        const rx_addr v = U32(p, 0x1acu);
        const rx_addr h = v ? U32(v, 0x188u) : 0;
        const rx_addr scene = h ? U32(h, 0x20138u) : 0;
        rx_camera_entity_add_to_scene(cam, scene);
    }
    return cam;
}

/* 0054bc00 (EAX = the player): the five view cameras from the config table, the gate and the quick camera, each added to
 * the scene; 0068cd90 (the camera sound-effects manager) is an audio boundary (nothing gameplay reads it, the sound
 * effects follow the camera name / mode only). */
void rx_player_cameras_create(rx_addr p) {
    for (uint32_t i = 0; i < 5; i++) {
        const rx_addr desc = p + 0x910u + 0x14u * i;
        const rx_addr cam = rx_camera_factory(desc, p);
        SU32(p, 0x974u + 4u * i, cam);
        if (i == 4u && cam) {                           /* 0054bc2f: the fifth view's CameraParam1..3 */
            SU32(cam, 0x368, U32(p, 0x224u));
            SU32(cam, 0x354, U32(p, 0x228u));
            SU32(cam, 0x360, U32(p, 0x22cu));
        }
    }
    {   /* 0054bc7a: the start-gate camera, 0x3d0 bytes */
        const rx_addr mem = rx_reflex_aligned_alloc(0x3d0);
        const rx_addr cam = mem ? rx_start_gate_camera_construct(mem, p) : 0;
        SU32(p, 0x99cu, cam);
        if (cam) {
            const rx_addr v = U32(p, 0x1acu);
            const rx_addr h = v ? U32(v, 0x188u) : 0;
            rx_camera_entity_add_to_scene(cam, h ? U32(h, 0x20138u) : 0);
        }
    }
    {   /* 0054bcfc: the quick camera, 0x3b0 bytes */
        const rx_addr mem = rx_reflex_aligned_alloc(0x3b0);
        const rx_addr cam = mem ? rx_quick_camera_construct(mem, p) : 0;
        SU32(p, 0x998u, cam);
        if (cam) {
            const rx_addr v = U32(p, 0x1acu);
            const rx_addr h = v ? U32(v, 0x188u) : 0;
            rx_camera_entity_add_to_scene(cam, h ? U32(h, 0x20138u) : 0);
        }
    }
}
