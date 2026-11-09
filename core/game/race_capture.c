/* race_capture.c -- MXActivityCapture (the Capture party mode, capture the flag; activity type 0x10, "CTF" in the data: the area
 * subscene "<level>MiniGameCTF", the HUD layer SinglePlayerCTF; vtable 0x00944fd4; 0x1014b0 bytes), its two MXCaptureFlag entities
 * (vtable 0x00944f1c: GreedObjectModel's vf16 / vf19 / vf28, race_egghunt.c), CaptureDriver (vtable 0x0093ff74, 0xf20 bytes) and
 * RuleCapture (vtable 0x0093ffc0, 0x20 bytes), hand-transliterated from the original (the address of each function is in its comment;
 * frames and the stack depth of each call are the original's). The audio logic is Tag's (TagActivityAudioLogic, race_tag_audio.c).
 *
 * The game: two teams (a driver's team +0xf14 = its index in the activity's drivers, odd / even). The red team (0) takes the blue flag
 * (+0xff200) and the blue team (1) the red one (+0xff070): a rider whose grab clock (RuleCapture +0x10, 5 s after a drop) has run and
 * who comes within 12 ft (the squared distance below +0x14 = 144) of the other team's flag carries it (it rides 6.5 above the vehicle);
 * back within 12 ft of its own base, with its own flag at home, it scores (00577c30: +0xff424 / +0xff428; vf18 puts both flags back on
 * their bases). A wreck drops the carried flag 7 ft above the vehicle; a rider who touches its own team's dropped flag returns it to its
 * base (and a rider carrying it is crashed: the vehicle's +0x1a0 -> +0x5b568 = 0x17). A rider that stays within 50 ft of its own base
 * for 5 s, or comes within 5 ft of an enemy within 700 ft of the enemy base, is crashed the same way. The first team to 30 (+0xff42c)
 * ends the match (vf42). The starts: 16 places alternating between the area's RedSpawn and BlueSpawn entities, i / 2 * 10 along the
 * spawn's x axis; the flags start on the RedBase / BlueBase entities (+ 5 up; the blue base keeps the red base's rotation: the
 * original's copy). With [0xaf6538] set twelve PlaceableParticleEffects (+0xff440, "Red_Smoke" x 6 / "Blue_Smoke" x 6) are each driver's
 * smoke (CaptureDriver +0xf18, claimed by vf44: shown while the driver is near a base).
 *
 * Content: no installed level has a MiniGameCTF subscene (the original faults at 005765f1 on every level: no "RedFlag"), so the session
 * refuses a Capture event before it loads (api/rx_session.c minigame_content_missing); vf2 keeps the original's lookups and stops
 * (act_unreached, STOP_AUDIT R125) where the original would fault.
 *
 * ONLINE (a Quazal session: [0x00da6cb0] + 0x1c4c set and the mode +0x1b0 not 1; none exists in the product): the network CTF object
 * (+0xff434: NetCTF, the flags' poses and the scores sent / taken, "NetworkCaptureActivity" 00577db0 / "NetworkObjectRemoved" 00577d00
 * handlers: not ported, as the other party modes' network handlers), the drivers' network objects (+0xef0), a spectator's join (vf38).
 * Each networked branch is marked "ONLINE" and stops with act_unreached (STOP_AUDIT R125). */
#include "race_minigames.h"
#include "race_snake.h"
#include "race_tag.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_activity_ui.h"
#include "race_challenge_activity.h"
#include "race_champ.h"
#include "race_finish.h"
#include "race_driver.h"
#include "race_misc.h"
#include "race_extern.h"
#include "race.h"
#include "build_activity.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "build_driver_reset.h"
#include "build_driver_dtor.h"
#include "build_rider.h"
#include "build_scene.h"
#include "build_scene_entity.h"
#include "build_activity_freeride_run.h"
#include "race_activity_stub.h"
#include "build_alloc.h"
#include "waypoint_activity.h"
#include "waypoint_driver.h"
#include "waypoint_audio.h"
#include "world_rider.h"
#include "loop_cam_build.h"
#include "loop_platform.h"
#include "phys_terrain.h"
#include "phys_crt.h"
#include "anim.h"
#include "anim_entity_dtors.h"
#include "fx.h"
#include "crt.h"
#include "../src/rx_os.h"
#include <string.h>

#define VT_CAPTURE          0x00944fd4u
#define VT_CAPTURE_DRIVER   0x0093ff74u
#define VT_RULE_CAPTURE     0x0093ffc0u
#define VT_CAPTURE_FLAG     0x00944f1cu
#define VT_CAPTURE_FLAG_O90 0x00944f94u
#define VT_AUDIO_LOGIC      0x009429fcu
#define VT_TAG_AUDIO        0x00942f7cu
#define TD_ACTIVITY         0x009cbd24u   /* void(Activity*) */
#define TD_DRIVER           0x009cda44u   /* void(BaseDriver*) */
#define STR_ACTIVITY_INIT   0x00935ea4u
#define RED(A)              ((A) + 0xff070u)   /* the red flag (MXCaptureFlag) */
#define BLUE(A)             ((A) + 0xff200u)   /* the blue flag */
#define SMOKE(A, i)         ((A) + 0xff440u + 0x2b0u * (uint32_t)(i))

static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
static int session_networked(void) {
    const rx_addr s = U32(0, 0x00da6cb0);
    return U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1;
}
static int session_host(void) { session_static(); return U32(U32(0, 0x00da6cb0), 0x1c48) != 0; }
/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static float x87_copy(float v) {
    uint32_t b = rx_fb(v);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) b |= 0x400000u;
    return rx_bf(b);
}
/* the terrain of a scene: the registry 0x9e30d4 (count 0x9e30dc, initialised under the guard bit 0x9e30e0) */
static rx_addr scene_terrain(rx_addr scene) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    for (uint32_t i = 0; i < U32(0, 0x009e30dc); i++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4 * i);
        if (t != 0 && U32(t, 0x44a8) == scene) return t;
    }
    return 0;
}
static inline void os_cs(uint32_t slot, rx_addr cs) { const uint32_t a[1] = {cs}; (void)rx_os_import(slot, 1, a); }
/* the entity's next scene id (+0x14, the scene's +0x19f4 under its lock), then the scene adds it (007d2f70); F = the caller's ESP */
static void scene_join(rx_addr F, rx_addr scene, rx_addr e) {
    rx_set_sp(F - 4);
    os_cs(0x0091600c, scene + 4);
    rx_set_sp(F);
    const uint32_t id = U32(scene, 0x19f4);
    SU32(scene, 0x19f4, id + 1);
    SU32(e, 0x14, id);
    rx_set_sp(F - 4);
    os_cs(0x009160f8, scene + 4);
    rx_set_sp(F);
    ACT_CALL(1, rx_scene_manager_add_entity(e, scene));
}
/* the vehicle of a driver and its position (+0xc0) */
static rx_addr drv_vehicle(rx_addr d) { return U32(U32(d, 0x74), 0x1ac); }
/* the squared distance of p (x, y, z) to the vehicle's position, in the original's order (z * z + y * y, + x * x) */
static float dist2_to(rx_addr v, float x, float y, float z) {
    const float dz = rx_subs(z, F32(v, 0xc8)), dy = rx_subs(y, F32(v, 0xc4)), dx = rx_subs(x, F32(v, 0xc0));
    return rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx));
}
/* SqrtF 0042c230 (push the argument; cdecl) */
static float sqrt_call(float x) { float r; ACT_CALLR(r, 1, phys_sqrtf(x)); return r; }
/* the vehicle's crash request (its +0x1a0 object's +0x5b568 = 0x17) */
static void vehicle_crash(rx_addr v) { SU32(U32(v, 0x1a0), 0x5b568, 0x17); }

/* ======================================================================================================================== */
/* construction / destruction                                                                                                */

/* 00576290 (stack act; ECX = type, EAX = world; RET 4) -> act: Activity's constructor, the two flags (ModelEntity's constructor
 * 00401db0, the class's vtables, the spin angle 0, +0x70 = 1), the twelve smoke effects (the vector constructor iterator with 005764e0:
 * 006964d0), both flags at home, no scores, the two base matrices identity, 30 to win, a new TagActivityAudioLogic (0x98 bytes) at +0xfd8.
 * Frame: push ecx / ebx / esi / edi. */
rx_addr rx_capture_construct(rx_addr act, uint32_t type, rx_addr world) {
    ACT_FRAME(F, 0, 4);
    ACT_CALL(1, rx_activity_construct(act, type, world));
    SU32(act, 0, VT_CAPTURE);
    for (int k = 0; k < 2; k++) {
        const rx_addr fl = k ? BLUE(act) : RED(act);
        ACT_CALL(1, (void)rx_model_entity_construct(fl));
        SU32(fl, 0, VT_CAPTURE_FLAG);
        SU32(fl, 0x90, VT_CAPTURE_FLAG_O90);
        SF32(fl, 0x140, 0.0f);
        SU32(fl, 0x70, 1);
    }
    ACT_CALL(4, rx_fx_vector_ctor_iterator(SMOKE(act, 0), 0x2b0, 0xc, 0x005764e0));
    SU8(act, 0xff430, 1);
    SU8(act, 0xff431, 1);
    SU32(act, 0x2c8, 0);
    SU32(act, 0xff424, 0);
    SU32(act, 0xff428, 0);
    SU32(act, 0x101498, 0);
    for (uint32_t k = 0; k < 2; k++)
        for (uint32_t i = 0; i < 16; i++) SF32(act + 0xff3a0 + 0x40 * k, 4 * i, (i % 5) == 0 ? 1.0f : 0.0f);
    SU32(act, 0xff42c, 0x1e);
    SU32(act, 0x1014ac, 0);
    rx_addr logic;
    ACT_CALLR(logic, 1, rx_reflex_aligned_alloc(0x98));
    if (logic != 0) {
        SU32(logic, 0, VT_AUDIO_LOGIC);
        SU32(logic, 0x94, 0);
        SU8(logic, 4, 0);
        SU32(logic, 0x50, 0);
        SU32(logic, 0x54, 0);
        SU32(logic, 0x8c, 0);
        SU32(logic, 0x90, 0);
        ACT_CALL(0, rx_audio_logic_vf32(logic));
        SU32(logic, 0, VT_TAG_AUDIO);
    }
    SU32(act, 0xfd8, logic);
    SU32(act, 0xff434, 0);
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x00576290, rx_capture_construct)

/* 005764e0 (ECX = the effect) -> the effect: PlaceableParticleEffect's constructor 006964d0. Frame: push esi. */
rx_addr rx_capture_smoke_construct(rx_addr e) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, (void)rx_placeable_effect_construct(e));
    ACT_END(F);
    return e;
}
RX_FUNCTION(0x005764e0, rx_capture_smoke_construct)

/* 005764f0 (ECX = act): the destructor's body: the audio logic released (00580d30) and freed, the twelve smoke effects destroyed from
 * the last (006965a0), the two flags (blue, then red) get the ModelEntity base's vtables and the two entity registries are told (vf1),
 * then a jump to Activity's destructor 005566b0. Frame: push esi / edi (+ ebx from the effects to the red flag). */
void rx_capture_destroy(rx_addr act) {
    ACT_FRAME(F, 0, 2);
    const rx_addr logic = U32(act, 0xfd8);
    SU32(act, 0, VT_CAPTURE);
    if (logic != 0) {
        ACT_CALL(1, rx_activity_audio_destroy(logic));
        ACT_CALL(1, rx_reflex_aligned_free(logic));
    }
    rx_set_sp(F - 4);   /* push ebx */
    for (int i = 11; i >= 0; i--) ACT_CALL(0, rx_particle_effect_destroy(SMOKE(act, i)));
    for (int k = 0; k < 2; k++) {
        const rx_addr fl = k ? RED(act) : BLUE(act);
        SU32(fl, 0x90, 0x0093708c);
        SU32(fl, 0, 0x0095a2e4);
        if (k) rx_set_sp(F);   /* pop ebx */
        for (int j = 0; j < 2; j++) {
            const rx_addr registry = U32(0, j ? 0x00a95828 : 0x00a95824);
            if (registry != 0) { const uint32_t a[1] = {fl}; (void)act_vcall(registry, 4, 1, a); }
        }
    }
    ACT_END(F);
    rx_waypoint_activity_base_destroy(act);
}
RX_FUNCTION(0x005764f0, rx_capture_destroy)

/* 005764c0 slot 0 (thiscall; flags; RET 4) -> act: the body (005764f0), freed with flags & 1. Frame: push esi. */
rx_addr rx_capture_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_capture_destroy(act));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(act));
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x005764c0, rx_capture_sdtor)

/* ======================================================================================================================== */
/* the listeners (as race_infection.c's templates)                                                                          */

/* 0058a170 (EAX = the session channel; stdcall act; RET 4): "NetworkObjectRemoved" with 00577d00 (proxy 0x945c64) */
void rx_capture_net_removed_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x0058a170, 1, 1, 2, channel, 0x0093ee98 /* "NetworkObjectRemoved" */, 0x00945c64, 0x009cdc7c, act, 0x00577d00));
}
RX_FUNCTION(0x0058a170, rx_capture_net_removed_listen)
/* 0058a230: "NetworkCaptureActivity" with 00577db0 (proxy 0x945c84: void(CTFEvent, NetCTF*, int, DOHandle, int, int)) */
void rx_capture_net_event_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x0058a230, 1, 1, 2, channel, 0x009423c4 /* "NetworkCaptureActivity" */, 0x00945c84, 0x009d0820, act, 0x00577db0));
}
RX_FUNCTION(0x0058a230, rx_capture_net_event_listen)

/* ======================================================================================================================== */
/* the flags, the starts, the bases, the scores                                                                              */

/* 00575fc0 (EAX = the area's flag entity, ESI = the activity's flag; stdcall scene, name; RET 8): the activity's flag takes the found
 * flag's model (its handle +0x100 marked in use around it): 0041c700 (the scene, the name, the handle, flags 2) with a 100-byte block
 * allocated and freed around it, the flags word +0xb0 (bit 4 off, bit 3 on), vf15 (slot 0x3c) with +0x150 (as the free ride's
 * 0055fce0). Frame: push ebx / edi. */
void rx_capture_flag_attach(rx_addr found, rx_addr flag, rx_addr scene, rx_addr name) {
    ACT_FRAME(F, 0, 2);
    const rx_addr handle = U32(found, 0x100);
    SU32(handle, 0x4c, 1);
    rx_addr tmp;
    ACT_CALLR(tmp, 1, rx_reflex_aligned_alloc(rx_mul_sat(0x19, 4)));
    ACT_CALL(3, rx_entity_attach_model(scene, name, flag, handle, 2));
    SU32(handle, 0x4c, 0);
    SU16(flag, 0xb0, (uint16_t)((U16(flag, 0xb0) & 0xffefu) | 8u));
    { const uint32_t a[1] = {flag + 0x150}; (void)act_vcall(flag, 0x3c, 1, a); }
    ACT_CALL(1, rx_reflex_aligned_free(tmp));
    ACT_END(F);
}
RX_FUNCTION(0x00575fc0, rx_capture_flag_attach)

/* 00576b60 (stdcall act; RET 4): the starts and the bases. The area's RedSpawn / BlueSpawn (007d2c80 in +0x118); the 16 starts
 * (+0x340 + 0x30 * i) alternate between them: the spawn's position + its x axis (row 0) * (i / 2) * 10, its direction (row 2); the
 * terrain's height query (007c5950) runs on the start's old position before it is written (the original's order, its result unused).
 * RedBase / BlueBase (+0x101480 / +0x101484): the red base's matrix into +0xff3a0 and its position on the terrain as its row 3
 * (00407410), the red flag placed there + 5 (in double; vf28 slot 0x70); the blue base the same into +0xff3e0 but with the red base's
 * matrix (the original's copy) and its own position; +0x880 = 0xc. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xa4;
 * push ebx / esi / edi (the spawns F + 0x70, the start's position F + 0x40, the red base F + 0x20, the blue base F + 0x30). */
void rx_capture_place_starts(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0xa4, 3);
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    SU32(F, 0x18, scene);
    for (uint32_t k = 0; k < 0x3c; k += 4) SU32(F + 0x74, k, 0);
    rx_addr e;
    ACT_CALLR(e, 3, rx_scene_manager_find_in_subscene(scene, 0x00942490 /* "RedSpawn" */, act + 0x118));
    SU32(F, 0x70, e);
    ACT_CALLR(e, 3, rx_scene_manager_find_in_subscene(U32(U32(act, 0x234), 0x20138), 0x0094249c /* "BlueSpawn" */, act + 0x118));
    SU32(F, 0x74, e);
    for (uint32_t i = 0; i < 0x10; i++) {
        SU32(F, 0x10, i);
        const rx_addr spawn = U32(F + 0x70, 4 * (i % 2));
        SU32(F, 0x14, i / 2);
        rx_addr m = act_vcall(spawn, 0x38, 0, 0);
        SF32(F, 0x60, F32(m, 0x30)); SF32(F, 0x64, F32(m, 0x34)); SF32(F, 0x68, F32(m, 0x38));
        m = act_vcall(spawn, 0x38, 0, 0);
        SF32(F, 0x50, F32(m, 0x20)); SF32(F, 0x54, F32(m, 0x24)); SF32(F, 0x58, F32(m, 0x28));
        m = act_vcall(spawn, 0x38, 0, 0);
        const float half = (float)(i / 2);   /* fild + fstp: exact */
        SF32(F, 0x14, half);
        const float ten = 10.0f;   /* 0x92fd88 */
        SF32(F, 0x40, rx_adds(rx_muls(rx_muls(F32(m, 0), half), ten), F32(F, 0x60)));
        SF32(F, 0x44, rx_adds(rx_muls(rx_muls(F32(m, 4), half), ten), F32(F, 0x64)));
        SF32(F, 0x48, rx_adds(rx_muls(rx_muls(F32(m, 8), half), ten), F32(F, 0x68)));
        const rx_addr start = act + 0x340 + 0x30 * i;
        uint32_t edx;
        ACT_CALL(1, (void)phys_terrain_base_height_at(scene_terrain(U32(F, 0x18)), start, &edx));
        SF32(start, 0, F32(F, 0x40)); SF32(start, 4, F32(F, 0x44)); SF32(start, 8, F32(F, 0x48));
        SF32(start, 0x10, F32(F, 0x50)); SF32(start, 0x14, F32(F, 0x54)); SF32(start, 0x18, F32(F, 0x58));
        SU32(F, 0x10, i + 1);
    }
    rx_addr red, blue;
    ACT_CALLR(red, 3, rx_scene_manager_find_in_subscene(U32(U32(act, 0x234), 0x20138), 0x009424a8 /* "RedBase" */, act + 0x118));
    ACT_CALLR(blue, 3, rx_scene_manager_find_in_subscene(U32(U32(act, 0x234), 0x20138), 0x009424b0 /* "BlueBase" */, act + 0x118));
    SU32(act, 0x101480, red);
    SU32(F, 0x14, blue);
    const rx_addr rm = act_vcall(red, 0x38, 0, 0);
    SU32(F, 0x1c, rm);
    SF32(F, 0x20, F32(rm, 0x30)); SF32(F, 0x24, F32(rm, 0x34)); SF32(F, 0x28, F32(rm, 0x38));
    const rx_addr terrain = scene_terrain(U32(F, 0x18));
    SU32(F, 0x10, terrain);
    uint32_t edx;
    ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, F + 0x20, &edx));
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(act + 0xff3a0, k, U32(rm, k));
    ACT_CALL(1, rx_cam_pose_row(3, F + 0x20, act + 0xff3a0));
    SF32(F, 0x24, (float)((double)F32(F, 0x24) + 5.0 /* 0x977bc8 */));
    { const uint32_t a[1] = {F + 0x20}; (void)act_vcall(RED(act), 0x70, 1, a); }
    SU32(act, 0x101484, U32(F, 0x14));
    const rx_addr bm = act_vcall(U32(F, 0x14), 0x38, 0, 0);
    SF32(F, 0x30, F32(bm, 0x30)); SF32(F, 0x34, F32(bm, 0x34)); SF32(F, 0x38, F32(bm, 0x38));
    ACT_CALL(1, (void)phys_terrain_base_height_at(U32(F, 0x10), F + 0x30, &edx));
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(act + 0xff3e0, k, U32(U32(F, 0x1c), k));
    ACT_CALL(1, rx_cam_pose_row(3, F + 0x30, act + 0xff3e0));
    SF32(F, 0x34, (float)((double)F32(F, 0x34) + 5.0));
    { const uint32_t a[1] = {F + 0x30}; (void)act_vcall(BLUE(act), 0x70, 1, a); }
    SU32(act, 0x880, 0xc);
    ACT_END(F);
}
RX_FUNCTION(0x00576b60, rx_capture_place_starts)

/* 00577ad0 (EAX = out, EDX = team; stdcall act; RET 4): the team's base position (row 3 of +0xff3a0 / +0xff3e0), zeros for another
 * team. Frame: push ebp; mov ebp, esp; and esp, -16. */
void rx_capture_base_position(rx_addr out, uint32_t team, rx_addr act) {
    if (team == 0) { SU32(out, 4, U32(act, 0xff3d4)); SU32(out, 8, U32(act, 0xff3d8)); SU32(out, 0, U32(act, 0xff3d0)); }
    else if (team == 1) { SU32(out, 4, U32(act, 0xff414)); SU32(out, 8, U32(act, 0xff418)); SU32(out, 0, U32(act, 0xff410)); }
    else { SU32(out, 4, 0); SU32(out, 8, 0); SU32(out, 0, 0); }
}
RX_FUNCTION(0x00577ad0, rx_capture_base_position)

/* the team's flag back home: its base matrix with row 3's y + 5 (vf16 slot 0x40), at home (+0xff430 / +0xff431); F = the frame whose
 * F + 0x10 takes the matrix */
static void flag_home(rx_addr F, rx_addr act, uint32_t team) {
    const rx_addr fl = team ? BLUE(act) : RED(act), base = act + (team ? 0xff3e0u : 0xff3a0u);
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(F + 0x10, k, U32(base, k));
    SF32(F, 0x44, rx_adds(F32(F, 0x44), 5.0f /* 0x935890 */));
    { const uint32_t a[1] = {F + 0x10}; (void)act_vcall(fl, 0x40, 1, a); }
    SU8(act, team ? 0xff431 : 0xff430, 1);
}
/* 00577b80 (EAX = team; stdcall act; RET 4): the team's flag back on its base (another team: nothing). Frame: push ebp; mov ebp, esp;
 * and esp, -16; sub esp, 0x44; push ebx / esi / edi (the matrix F + 0x10). */
void rx_capture_flag_home(uint32_t team, rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0x44, 3);
    if (team == 0 || team == 1) flag_home(F, act, team);
    ACT_END(F);
}
RX_FUNCTION(0x00577b80, rx_capture_flag_home)

/* 00577c30 (ESI = act; stdcall team; RET 4): the team scores (+0xff424 / +0xff428 + 1; the carrier of the other team's flag noted for
 * the host's message), the flags reset (vf18 slot 0x48); (ONLINE: the host sends the score, 006f12d0). Frame: push ebp / edi (the
 * network handle at the argument slot F + 0xc). */
void rx_capture_score(rx_addr act, uint32_t team) {
    ACT_FRAME(F, 0, 2);
    SU32(F, 0xc, 0);
    if (team == 0) SU32(act, 0xff424, U32(act, 0xff424) + 1);
    else if (team == 1) SU32(act, 0xff428, U32(act, 0xff428) + 1);
    else { ACT_END(F); return; }
    (void)act_vcall(act, 0x48, 0, 0);
    if (session_host() && U32(act, 0xff434) != 0) act_unreached(0x00577ca1, "ONLINE: the host sends the score (006f12d0)");
    ACT_END(F);
}
RX_FUNCTION(0x00577c30, rx_capture_score)

/* 00578200 (EDI = act): the two flags' matrices read (vf14 slot 0x38, three times each, blue first; the results unused). Frame: push ebp;
 * mov ebp, esp; and esp, -16; sub esp, 0xc; push esi. */
void rx_capture_flags_touch(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0xc, 1);
    for (int k = 0; k < 3; k++) (void)act_vcall(BLUE(act), 0x38, 0, 0);
    for (int k = 0; k < 3; k++) (void)act_vcall(RED(act), 0x38, 0, 0);
    ACT_END(F);
}
RX_FUNCTION(0x00578200, rx_capture_flags_touch)

/* ======================================================================================================================== */
/* the activity's virtuals                                                                                                   */

/* 005765c0 slot 2 (thiscall; arg; RET 4): Activity::vf2; the area's "RedFlag" hidden and its model given to the red flag (00575fc0 with
 * "RedCaptureBall"), a scene id and the scene; its +0xa2 = 1; the same for "BlueFlag" / "BlueCaptureBall"; +0xff390 / +0xff394 = 0.25;
 * the blue flag's +0xa2 = 1; both base matrices identity; the two network listeners, the host's network CTF object (ONLINE); the starts
 * and bases (00576b60); no scores; "Activity Init" raised; with [0xaf6538] the twelve smoke effects: a scene id and the scene, the
 * scene (+0x10), not taken (+0x2a4), "Red_Smoke" (team 0) for the first six, "Blue_Smoke" (team 1) for the others (+0x190, +0x2a0),
 * vf7 (slot 0x1c), +0x144 .. +0x150 = 1e6. Frame: sub esp, 0x10; push ebx / ebp / esi / edi (the payload F + 0x24). */
void rx_capture_vf2(rx_addr act, rx_addr arg) {
    ACT_FRAME(F, 0x10, 4);
    ACT_CALL(1, rx_activity_vf2(act, arg));
    SU32(F, -0xd4, 0x1f);   /* the dead stack the flags' cell records copy (0x22fbbc in the reference boot; race_egghunt.c vf2: msvcr90's malloc size push) */
    for (int k = 0; k < 2; k++) {
        const rx_addr scene0 = U32(U32(act, 0x234), 0x20138);
        rx_addr found;
        ACT_CALLR(found, 3, rx_scene_manager_find_in_subscene(scene0, k ? 0x009423a8u /* "BlueFlag" */ : 0x00942390u /* "RedFlag" */, act + 0x118));
        if (found == 0)   /* the original writes +0xa2 of 0 (005765f1 / 0057666e); the session refuses such a level first */
            act_unreached(k ? 0x0057666e : 0x005765f1, "Capture: no flag entity in the area subscene (the level lacks the mode's content)");
        SU8(found, 0xa2, 0);
        const rx_addr fl = k ? BLUE(act) : RED(act);
        ACT_CALL(2, rx_capture_flag_attach(found, fl, U32(U32(act, 0x234), 0x20138), k ? 0x009423b4u /* "BlueCaptureBall" */ : 0x00942398u /* "RedCaptureBall" */));
        scene_join(F, U32(U32(act, 0x234), 0x20138), fl);
        if (k == 0) SU8(act, 0xff112, 1);
    }
    SF32(act, 0xff390, 0.25f /* 0x92ff84 */);
    SF32(act, 0xff394, 0.25f);
    SU8(act, 0xff2a2, 1);
    for (uint32_t k = 0; k < 2; k++)
        for (uint32_t i = 0; i < 16; i++) SF32(act + 0xff3a0 + 0x40 * k, 4 * i, (i % 5) == 0 ? 1.0f : 0.0f);
    session_static();
    ACT_CALL(1, rx_capture_net_removed_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_capture_net_event_listen(U32(0, 0x00da6ca0), act));
    session_static();
    if (session_networked() && session_host()) act_unreached(0x005768db, "ONLINE: the host's network CTF object (008879b0 / 00887780)");
    ACT_CALL(1, rx_capture_place_starts(act));
    SU32(act, 0x101498, 0);
    SU32(act, 0xff424, 0);
    SU32(act, 0xff428, 0);
    SU32(F, 0x24, act);
    act_raise(U32(0, 0x00a93f74), STR_ACTIVITY_INIT, TD_ACTIVITY, F + 0x24, 4);
    if (U32(0, 0x00af6538) != 0) {
        for (uint32_t i = 0; i < 0xc; i++) {
            const rx_addr fx = SMOKE(act, i);
            SU32(F, 0x10, i);
            scene_join(F, U32(U32(act, 0x234), 0x20138), fx);
            SU32(fx, 0x10, U32(U32(act, 0x234), 0x20138));
            SU32(fx, 0x2a4, 0);
            const rx_addr name = i >= 6 ? 0x009423dcu /* "Blue_Smoke" */ : 0x009423e8u /* "Red_Smoke" */;
            act_strcpy(fx + 0x190, name);
            SU32(fx, 0x2a0, i >= 6 ? 1 : 0);
            (void)act_vcall(fx, 0x1c, 0, 0);
            for (uint32_t k = 0x144; k <= 0x150; k += 4) SU32(fx, k, 0x49742400u /* 1e6 */);
            SU32(F, 0x10, i + 1);
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x005765c0, rx_capture_vf2)

/* 00576f40 slot 3 (ECX = activity): every driver of the two lists leaves the activity, the two flags leave the scene (007d3040) and are
 * released (vf9 slot 0x24), with [0xaf6538] the twelve smoke effects leave the scene, then a jump to Activity::vf3 (the network
 * listeners are not left: the original's). Frame: push ebx / esi / edi. */
void rx_capture_vf3(rx_addr act) {
    ACT_FRAME(F, 0, 3);
    while (I32(act, 0x26c) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x268), 0)));
    while (I32(act, 0x280) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x27c), 0)));
    ACT_CALL(1, rx_scene_manager_remove_entity(RED(act), U32(U32(act, 0x234), 0x20138)));
    ACT_CALL(1, rx_scene_manager_remove_entity(BLUE(act), U32(U32(act, 0x234), 0x20138)));
    (void)act_vcall(RED(act), 0x24, 0, 0);
    (void)act_vcall(BLUE(act), 0x24, 0, 0);
    if (U32(0, 0x00af6538) != 0)
        for (uint32_t i = 0; i < 0xc; i++) ACT_CALL(1, rx_scene_manager_remove_entity(SMOKE(act, i), U32(U32(act, 0x234), 0x20138)));
    ACT_END(F);
    rx_activity_vf3(act);
}
RX_FUNCTION(0x00576f40, rx_capture_vf3)

/* 00576ab0 slot 4 (ECX = activity): the class's event types on the activity's channel (Activity::vf4 is not called): the flags' eight
 * (00592820: "Red Ball Captured" / "Dropped" / "Returned", "Red Team Scores", the blue ones), "Set Arrow Target" (00592280), "Show Arrow"
 * / "Hide Arrow" (00590f60). Frame: push esi. */
void rx_capture_vf4(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    static const uint32_t names[8] = {0x009423f4, 0x00942408, 0x0094241c, 0x00942430, 0x00942440, 0x00942454, 0x00942468, 0x0094247c};
    for (int k = 0; k < 8; k++) act_event_type(0x00592820, U32(act, 0x290), names[k]);
    act_event_type(0x00592280, U32(act, 0x290), 0);
    act_event_type(0x00590f60, U32(act, 0x290), 0x0093ef30 /* "Show Arrow" */);
    act_event_type(0x00590f60, U32(act, 0x290), 0x0093eef0 /* "Hide Arrow" */);
    ACT_END(F);
}
RX_FUNCTION(0x00576ab0, rx_capture_vf4)

/* 00577a00 slot 15 (ECX = activity): the round's start: the starts and bases (00576b60), Activity::vf15, +0x10149c / +0x1014a0 = 0, the
 * registry's "MXGame\\Tag" "WaterKickTime" into the global [0x9c521c] (vf1 / vf17 of the engine registry [0xa95844] + 0xc).
 * Frame: push esi. */
void rx_capture_vf15(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_capture_place_starts(act));
    ACT_CALL(0, rx_activity_vf15(act));
    SU32(act, 0x10149c, 0);
    SU32(act, 0x1014a0, 0);
    const rx_addr reg = U32(U32(0, 0x00a95844), 0xc);
    const uint32_t find[3] = {0x0093f050 /* "MXGame\\Tag" */, 0x00942270 /* "WaterKickTime" */, 1};
    const rx_addr rec = act_vcall(reg, 4, 3, find);
    if (rec != 0) { const uint32_t read[3] = {rec, 0x009c521c, 1}; (void)act_vcall(reg, 0x44, 3, read); }
    ACT_END(F);
}
RX_FUNCTION(0x00577a00, rx_capture_vf15)

/* 00577a60 slot 16 (ECX = activity): a new round: every driver takes its start zone (0054cd20), no scores, Activity::vf16,
 * +0x10149c / +0x1014a0 = 0. Frame: push ebx / esi / edi. */
void rx_capture_vf16(rx_addr act) {
    ACT_FRAME(F, 0, 3);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) ACT_CALL(1, rx_player_start_zone(U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74)));
    SU32(act, 0xff424, 0);
    SU32(act, 0xff428, 0);
    ACT_CALL(0, rx_activity_vf16(act));
    SU32(act, 0x10149c, 0);
    SU32(act, 0x1014a0, 0);
    ACT_END(F);
}
RX_FUNCTION(0x00577a60, rx_capture_vf16)

/* 00577010 slot 18 (ECX = activity): the round's reset: Activity::vf18; no carriers; +0x101488 .. +0x101498 cleared; both flags back on
 * their bases (as 00577b80). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x44; push ebx / esi / edi (the matrix F + 0x10). */
void rx_capture_vf18(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0x44, 3);
    ACT_CALL(0, rx_activity_vf18(act));
    SU32(act, 0xff398, 0);
    SU32(act, 0xff39c, 0);
    SF32(act, 0x101488, 0.0f);
    SF32(act, 0x10148c, 0.0f);
    SU32(act, 0x101490, 0);
    SU32(act, 0x101494, 0);
    SU32(act, 0x101498, 0);
    flag_home(F, act, 0);
    flag_home(F, act, 1);
    ACT_END(F);
}
RX_FUNCTION(0x00577010, rx_capture_vf18)

/* 00577d20 slot 19 (thiscall; player, position out, direction out, up out; RET 0x10) -> 1: the player's start (its zone +0x1dc: the
 * activity's +0x340 + 0x30 * zone) position and direction (x87 copies), up [0x9cb4b0]. */
uint32_t rx_capture_vf19(rx_addr act, rx_addr player, rx_addr pos, rx_addr dir, rx_addr up) {
    rx_addr s = act + 0x340 + 0x30u * U32(player, 0x1dc);
    SF32(pos, 0, x87_copy(F32(s, 0))); SF32(pos, 4, x87_copy(F32(s, 4))); SF32(pos, 8, x87_copy(F32(s, 8)));
    s = act + 0x350 + 0x30u * U32(player, 0x1dc);
    SF32(dir, 0, x87_copy(F32(s, 0))); SF32(dir, 4, x87_copy(F32(s, 4))); SF32(dir, 8, x87_copy(F32(s, 8)));
    SU32(up, 0, U32(0, 0x009cb4b0)); SU32(up, 4, U32(0, 0x009cb4b4)); SU32(up, 8, U32(0, 0x009cb4b8));
    return 1;
}
RX_FUNCTION(0x00577d20, rx_capture_vf19)

/* 00577b60 slot 27 (thiscall; dt; RET 4): WindDown: the flags' matrices read (00578200), then Activity::vf27. Frame: push edi. */
void rx_capture_vf27(rx_addr act, float dt) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_capture_flags_touch(act));
    ACT_CALL(1, rx_activity_wind_down(act, x87_copy(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x00577b60, rx_capture_vf27)

/* 00577630 slot 28 (thiscall; dt; RET 4): InProgress: Activity::vf28, then under the engine's profiler scope (ONLINE: the network CTF
 * object found or made, the flags' poses sent or taken). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x44; push ebx / esi /
 * edi; dt through x87. */
void rx_capture_vf28(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 16, 0x44, 3);
    ACT_CALL(1, rx_activity_in_progress(act, x87_copy(dt)));
    session_static();
    const rx_addr scope = race_profile_begin();
    if (U32(act, 0xff434) == 0) {
        session_static();
        if (session_networked()) act_unreached(0x005776c0, "ONLINE: the network CTF object found or made (006e4fa0 / 008879b0)");
    }
    if (U32(act, 0xff434) != 0) act_unreached(0x00577870, "ONLINE: the flags' poses sent or taken (the network CTF object)");
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00577630, rx_capture_vf28)

/* 005772b0 slot 38 (thiscall; old state; RET 4): Countdown begins. With spectators (+0xfe8) the first local human with a profile
 * (+0xb2c) among the game's players: a spectator (+0x908) of an online session rejoins (ONLINE: its vehicle, the UI's spectator
 * properties, the session's tables); then the activity's vf16 (slot 0x40); Activity::vf38; +0x29c = 0.
 * Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x34; push ebx / esi / edi. */
void rx_capture_vf38(rx_addr act, uint32_t old_state) {
    ACT_FRAME_EBP(F, 8, 0x34, 3);
    (void)old_state;
    SU32(F, 0x14, act);
    if (U32(act, 0xfe8) != 0) {
        const rx_addr g = U32(0, 0x00a946b8);
        for (uint32_t i = 0; i < U8(g, 0x1541c); i++) {
            rx_addr p;
            ACT_CALLR(p, 2, rx_world_players_find(g + 0x7460, i, 3));
            if (U32(p, 4) == 0 || U32(p, 0x10) != 0 || U32(p, 0xb2c) == 0) continue;
            if (U32(p, 0x908) != 0) {
                session_static();
                if (session_networked() && U32(p, 0xb2c) != 0 && U32(U32(p, 0xb2c), 0x1d8) != 0)
                    act_unreached(0x00577394, "ONLINE: a spectator rejoins the session's Capture");
            }
            break;
        }
        (void)act_vcall(act, 0x40, 0, 0);
    }
    ACT_CALL(1, rx_activity_countdown_enter(act));
    SF32(act, 0x29c, 0.0f);
    ACT_END(F);
}
RX_FUNCTION(0x005772b0, rx_capture_vf38)

/* 00577280 slot 42 (ECX = activity): the match's end: a team with the score to win (+0xff42c, 30) -> WindDown (state 3), +0x1014ac = 1.
 * Frame: push esi. */
void rx_capture_vf42(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    const int32_t win = I32(act, 0xff42c);
    if (I32(act, 0xff424) >= win || I32(act, 0xff428) >= win) {
        ACT_CALL(2, rx_activity_set_state(act, 3));
        SU32(act, 0x1014ac, 1);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00577280, rx_capture_vf42)

/* 00577240 slot 43 (stdcall a, b, tie; RET 0xc; ECX unused) -> the standings' order: none but (with tie) the start zones (+0x1dc of the
 * players: a >= b gives 1, else -1) */
int32_t rx_capture_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    (void)act;
    if (tie == 0) return 0;
    return I32(U32(a, 0x74), 0x1dc) >= I32(U32(b, 0x74), 0x1dc) ? 1 : -1;
}
RX_FUNCTION(0x00577240, rx_capture_vf43)

/* 005770f0 slot 44 (thiscall; player; RET 4) -> a new CaptureDriver: its team the drivers so far modulo 2 (ONLINE: the profile's team,
 * +0xb2c -> +0x139 & 1); the first free smoke effect of the team (+0x2a0, +0x2a4 = 1: taken); 0xf20 bytes: NetworkDriver's constructor
 * (0050e1b0), the team (+0xf14), the effect (+0xf18), the class's vtable, the activity (+0xf10), +0xee8 = 6; 0 when the allocation
 * fails. Frame: push ecx / ebx / ebp / esi / edi (the activity F + 0x10). */
rx_addr rx_capture_vf44(rx_addr act, rx_addr player) {
    ACT_FRAME(F, 0, 5);
    SU32(F, 0x10, act);
    session_static();
    int32_t team;
    if (session_networked()) {
        act_unreached(0x00577135, "ONLINE: the team of a session's player (its profile's +0x139)");
        team = 0;
    } else {
        const int32_t n = I32(act, 0x26c);
        team = n % 2;   /* and 0x80000001 / the sign fix: the C remainder */
    }
    rx_addr fx = 0;
    for (uint32_t i = 0; i < 0xc; i++) {
        const rx_addr e = SMOKE(act, i);
        if (I32(e, 0x2a0) == team && U32(e, 0x2a4) == 0) { fx = e; SU32(e, 0x2a4, 1); break; }
    }
    rx_addr d;
    ACT_CALLR(d, 1, rx_reflex_aligned_alloc(0xf20));
    if (d != 0) {
        ACT_CALL(1, (void)rx_network_driver_construct(d, player));
        SU32(d, 0xf14, (uint32_t)team);
        SU32(d, 0xf18, fx);
        SU32(d, 0, VT_CAPTURE_DRIVER);
        SU32(d, 0xf10, U32(F, 0x10));
        SU32(d, 0xee8, 6);
    }
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x005770f0, rx_capture_vf44)

/* 0058a2f0 (ECX = driver) -> a RuleCapture from the driver's rule pool (+0x3e4, 0x20 bytes: the grab clock 5, enabled, not carrying,
 * +0x18 = 0) appended to its rules (+0xa0, count +0xee4, at most 0x12; 0 beyond) */
rx_addr rx_capture_rule_create(rx_addr d) {
    if (U32(d, 0xee4) >= 0x12) return 0;
    const rx_addr r = U32(d, 0x3e4);
    SU32(d, 0x3e4, r + 0x20);
    rx_addr rule = 0;
    if (r != 0) { SF32(r, 0x10, 5.0f); SU32(r, 0, VT_RULE_CAPTURE); SU32(r, 8, 1); SU32(r, 0xc, 0); SU32(r, 0x18, 0); rule = r; }
    SU32(d, 0xa0 + 4 * U32(d, 0xee4), rule);
    const rx_addr out = U32(d, 0xa0 + 4 * U32(d, 0xee4));
    SU32(d, 0xee4, U32(d, 0xee4) + 1);
    return out;
}
RX_FUNCTION(0x0058a2f0, rx_capture_rule_create)

/* 00577200 slot 47 (thiscall; driver; RET 4): the driver's rules: outside a free-ride area (the world's +0x22144 != 1) the free ride
 * rule (00587690), then RuleCapture (0058a2f0), each initialised with the driver (vf1, slot 4). Frame: push esi. */
void rx_capture_vf47(rx_addr act, rx_addr driver) {
    ACT_FRAME(F, 0, 1);
    const uint32_t a[1] = {driver};
    rx_addr rule;
    if (U32(U32(act, 0x234), 0x22144) != 1) {
        ACT_CALLR(rule, 0, rx_waypoint_get_free_ride_rule(driver));
        (void)act_vcall(rule, 4, 1, a);
    }
    ACT_CALLR(rule, 0, rx_capture_rule_create(driver));
    (void)act_vcall(rule, 4, 1, a);
    ACT_END(F);
}
RX_FUNCTION(0x00577200, rx_capture_vf47)

/* ======================================================================================================================== */
/* CaptureDriver                                                                                                             */

/* 005161d0 slot 0 (thiscall; flags; RET 4) -> driver: the class's vtable, NetworkDriver's destructor body (0050e2f0), freed with
 * flags & 1. Frame: push esi. */
rx_addr rx_capture_driver_sdtor(rx_addr d, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    SU32(d, 0, VT_CAPTURE_DRIVER);
    ACT_CALL(0, rx_network_driver_dtor(d));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(d));
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x005161d0, rx_capture_driver_sdtor)

/* 0053b290 (stdcall driver; RET 4) -> its first rule of kind 0xd (RuleCapture: the rule's vf3 slot 0xc), or 0. Frame: push ebx / esi /
 * edi. */
rx_addr rx_driver_rule_capture(rx_addr d) {
    ACT_FRAME(F, 0, 3);
    rx_addr found = 0;
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) {
        const rx_addr r = U32(d, 0xa0 + 4 * i);
        if (r != 0 && act_vcall(r, 0xc, 0, 0) == 0xd) { found = r; break; }
    }
    ACT_END(F);
    return found;
}
RX_FUNCTION(0x0053b290, rx_driver_rule_capture)

/* 00516200 slot 3 (ECX = driver): the reset. NetworkDriver::vf3; a player with a vehicle in the world (+4) gets its start place's
 * effect query (00460020); not carrying (+0xf1c), +0x3c0 = 0; its RuleCapture not carrying; "Show Arrow" raised with the driver on the
 * activity's channel; the behaviours' and rules' vf6 (005092b0 / 005092f0). Frame: push ebp; mov ebp, esp; and esp, -8; push ecx; push
 * ebp / esi / edi (the payload F + 0xc). */
void rx_capture_driver_vf3(rx_addr d) {
    ACT_FRAME_EBP(F, 8, 4, 3);
    ACT_CALL(0, rx_network_driver_vf3(d));
    if (U32(U32(d, 0x74), 4) != 0) {
        const rx_addr place = U32(d, 0x144);
        ACT_CALL(2, rx_vehicle_effect_query(drv_vehicle(d), place, place + 0x10));
    }
    SU8(d, 0xf1c, 0);
    SF32(d, 0x3c0, 0.0f);
    rx_addr rule;
    ACT_CALLR(rule, 1, rx_driver_rule_capture(d));
    SU32(rule, 0xc, 0);
    SU32(F, 0xc, d);
    act_raise(U32(U32(U32(d, 0x74), 0x94), 0x290), 0x0093ef30 /* "Show Arrow" */, TD_DRIVER, F + 0xc, 4);
    ACT_CALL(1, rx_driver_behaviors_vf6(d));
    ACT_CALL(1, rx_driver_rules_vf6(d));
    ACT_END(F);
}
RX_FUNCTION(0x00516200, rx_capture_driver_vf3)

/* 00516490 (stdcall driver, dt; RET 8): a human's rules: every rule's vf5 (slot 0x14) with dt. Frame: push ebx / esi (+ edi). */
void rx_capture_driver_human(rx_addr d, float dt) {
    ACT_FRAME(F, 0, 2);
    if (U32(d, 0xee4) != 0) {
        rx_set_sp(F - 4);   /* push edi */
        for (uint32_t i = 0; i < U32(d, 0xee4); i++) act_vcall_f(U32(d, 0xa0 + 4 * i), 0x14, x87_copy(dt));
        rx_set_sp(F);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00516490, rx_capture_driver_human)

/* 00516380 (ESI = driver; stdcall dt; RET 4): a computer rider's drive (as Tag's 00515330 without its null check of the AI ability): the
 * steering / throttle / brake (+0x60 .. +0x68) cleared, every rule's vf5 (slot 0x14) with 0, the behaviours' vf4 (slot 0x10) with dt
 * until one takes the drive (non-zero); the throttle kept (+0x3dc); unless the vehicle says otherwise (its vf43, slot 0xac) the speed
 * limit +0x3d4 = the throttle times the AI ability's speed percentage (+0x18 + 0x468: [class * 4 + skill], x 0.01) or x 0.5 without one
 * (0 and ordered); +0x3c0 = 1. Frame: push ebx / edi (popped before the tail). */
void rx_capture_driver_ai(rx_addr d, float dt) {
    ACT_FRAME(F, 0, 2);
    SF32(d, 0x60, 0.0f); SF32(d, 0x64, 0.0f); SF32(d, 0x68, 0.0f);
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) act_vcall_f(U32(d, 0xa0 + 4 * i), 0x14, 0.0f);
    for (uint32_t i = 0; i < U32(d, 0xee0); i++) {
        const rx_addr b = U32(d, 0xe8 + 4 * i);
        const rx_addr fn = U32(U32(b, 0), 0x10);
        uint32_t (*f)(rx_addr, float) = RX_FN(fn, uint32_t (*)(rx_addr, float));
        uint32_t took;
        if (f) ACT_CALLR(took, 1, f(b, x87_copy(dt)));
        else { const uint32_t a = rx_fb(x87_copy(dt)); took = rx_call_original(fn, RX_THISCALL, b, 1, &a); }
        if (took != 0) break;
    }
    SU32(d, 0x3dc, rx_fb(x87_copy(F32(d, 0x60))));
    const uint32_t own = act_vcall(drv_vehicle(d), 0xac, 0, 0);
    ACT_END(F);
    if (own == 0) {
        const rx_addr veh = drv_vehicle(d);
        const float pct = F32(U32(d, 0x18), 0x468 + 4u * (U32(veh, 0x198) + U32(veh, 0x194) * 4u));
        float v;
        if (!(pct == 0.0f)) v = rx_muls(rx_muls(pct, rx_bf(0x3c23d70au) /* 0.01, 0x9322ac */), F32(d, 0x3dc));
        else v = rx_muls(F32(d, 0x3dc), rx_bf(0x3f000000u) /* 0.5, 0x9322cc */);
        SF32(d, 0x3d4, v);
    }
    SF32(d, 0x3c0, 1.0f);
}
RX_FUNCTION(0x00516380, rx_capture_driver_ai)

/* 005162a0 slot 7 (thiscall; dt; RET 4): the driver's update: a spectator (+0x908) runs NetworkDriver::vf7 only; a human its rules
 * with dt (00516490), a local computer rider its drive (00516380); its RuleCapture's grab clock grows by dt while below 5; its smoke
 * effect (+0xf18) follows the vehicle (+0x60 .. +0x68: the position, y + [0x9d789c] (2.5)); NetworkDriver::vf7. Frame: push ebp;
 * mov ebp, esp; and esp, -16; sub esp, 0xc; push esi; dt through x87. */
void rx_capture_driver_vf7(rx_addr d, float dt) {
    ACT_FRAME_EBP(F, 16, 0xc, 1);
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x908) != 0) {
        ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
        ACT_END(F);
        return;
    }
    if (U32(p, 0x10) == 0) ACT_CALL(2, rx_capture_driver_human(d, x87_copy(dt)));
    else if (U32(p, 0x10) == 1 && U32(p, 4) != 0) ACT_CALL(1, rx_capture_driver_ai(d, x87_copy(dt)));
    rx_addr rule;
    ACT_CALLR(rule, 1, rx_driver_rule_capture(d));
    if (5.0f /* 0x935890 */ > F32(rule, 0x10)) SF32(rule, 0x10, rx_adds(F32(rule, 0x10), dt));
    const rx_addr fx = U32(d, 0xf18);
    if (fx != 0) {
        const rx_addr v = drv_vehicle(d);
        const float y = rx_adds(F32(v, 0xc4), F32(0, 0x009d789c));
        SF32(fx, 0x60, F32(v, 0xc0));
        SF32(fx, 0x64, y);
        SF32(fx, 0x68, F32(v, 0xc8));
    }
    ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x005162a0, rx_capture_driver_vf7)

/* 005164d0 slot 9 (thiscall; rule; RET 4): a rule's event. RuleCapture's (kind 0xd: a grab or a drop): the vehicle's world +0x2f00 += 3
 * (ONLINE: the carrier's flag event sent to the host, 006f1190). Frame: push esi / edi. */
void rx_capture_driver_vf9(rx_addr d, rx_addr rule) {
    ACT_FRAME(F, 0, 2);
    if (act_vcall(rule, 0xc, 0, 0) == 0xd) {
        const rx_addr act = U32(U32(d, 0x74), 0x94);
        const rx_addr w = U32(drv_vehicle(d), 0x234);
        SF32(w, 0x2f00, rx_adds(F32(w, 0x2f00), 3.0f /* 0x92fe48 */));
        if (U32(act, 0xff434) != 0 && U32(d, 0xef0) != 0) act_unreached(0x00516531, "ONLINE: the carrier's flag event sent to the host (006f1190)");
    }
    ACT_END(F);
}
RX_FUNCTION(0x005164d0, rx_capture_driver_vf9)

/* ======================================================================================================================== */
/* RuleCapture                                                                                                               */

/* 0051ca80 slot 1 (thiscall; driver; RET 4): the driver, enabled, not carrying, the grab radius^2 144 */
void rx_capture_rule_vf1(rx_addr rule, rx_addr driver) {
    SU32(rule, 4, driver);
    SU32(rule, 8, 1);
    SU32(rule, 0xc, 0);
    SF32(rule, 0x14, 144.0f /* 0x43100000 */);
}
RX_FUNCTION(0x0051ca80, rx_capture_rule_vf1)

/* 0051cab0 slot 5 (thiscall; dt; RET 4): the capture rules of the driver (+4; its team +0xf14; the activity of its player). Under the
 * engine's profiler scope: (see the file's comment) not carrying: with the grab clock run (+0x10 >= 5) and (offline, or the host's own
 * rider) the other team's flag within the grab radius is taken (carrying +0xc, the driver's +0xf1c, the activity's carrier +0xff39c /
 * +0xff398, that flag not at home; the driver's vf9 slot 0x24 with the rule); the smoke off; within 50 of its own base the smoke on and
 * after 5 s there the rider is crashed (+0x18 the clock); within 700 of the other team's base the smoke on and a rider within 5 of an
 * enemy is crashed. Carrying: the flag rides 6.5 above the vehicle (vf16 slot 0x40 with the vehicle's matrix, vf15); a wreck (+0x280)
 * drops it 7 above the vehicle (vf28 slot 0x70; no carrier; the grab clock 0; vf9); else (offline, or the host's own rider) back
 * within the grab radius of its own base with its own flag at home: the team scores (00577c30). Then (offline, or the host's own rider)
 * with the grab clock run, its own team's flag within the grab radius is returned home (its carrier crashed and its rule cleared,
 * 0053b290; 00577b80; ONLINE: the host's message, 006f1230). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x74; push ebx /
 * esi / edi (the team F + 0xc, the network object F + 0x10, the other team F + 0x14, the driver F + 0x18, the scope F + 0x1c, the flag's
 * position F + 0x20, the base's F + 0x30, the matrix F + 0x40); dt on the stack. */
uint32_t rx_capture_rule_vf5(rx_addr rule, float dt) {
    ACT_FRAME_EBP(F, 16, 0x74, 3);
    rx_addr d = U32(rule, 4);
    const uint32_t team = U32(d, 0xf14);
    const rx_addr act = U32(U32(d, 0x74), 0x94);
    SU32(F, 0x1c, 0);
    SU32(F, 0xc, team);
    session_static();
    const rx_addr scope = race_profile_begin();
    SU32(F, 0x1c, scope);
    d = U32(rule, 4);
    const rx_addr net = U32(d, 0xef0);
    SU32(F, 0x18, d);
    SU32(F, 0x10, net);
    /* (offline, or the host's own rider): the online condition of the original's three blocks */
    #define MAY_ACT(drv) (net == 0 || (session_static(), U32(U32(0, 0x00da6cb0), 0x1c48) != 0 && U32(U32((drv), 0x74), 0x1e4) == 0))
    if (U32(rule, 0xc) == 0) {
        if (F32(rule, 0x10) >= 5.0f /* 0x935890 */) {
            if (!MAY_ACT(d)) goto returns;
            const uint32_t other = team == 0;
            SU32(F, 0x14, other);
            const rx_addr flag = other == 0 ? RED(act) : other == 1 ? BLUE(act) : 0;
            { const uint32_t a[1] = {F + 0x20}; (void)act_vcall(flag, 0x44, 1, a); }
            rx_addr v = drv_vehicle(U32(rule, 4));
            if (F32(rule, 0x14) > dist2_to(v, F32(F, 0x20), F32(F, 0x24), F32(F, 0x28))) {
                SU32(rule, 0xc, 1);
                SU8(U32(rule, 4), 0xf1c, 1);
                if (team == 0) { SU32(act, 0xff39c, U32(rule, 4)); SU8(act, 0xff431, 0); }
                else if (team == 1) { SU32(act, 0xff398, U32(rule, 4)); SU8(act, 0xff430, 0); }
                { const uint32_t a[1] = {rule}; (void)act_vcall(U32(rule, 4), 0x24, 1, a); }
            }
            const rx_addr me = U32(rule, 4);
            { const uint32_t a[1] = {0}; (void)act_vcall(U32(me, 0xf18) + 0x90, 0x14, 1, a); }
            ACT_CALL(1, rx_capture_base_position(F + 0x30, team, act));
            v = drv_vehicle(U32(rule, 4));
            float dist = sqrt_call(dist2_to(v, F32(F, 0x30), F32(F, 0x34), F32(F, 0x38)));
            if (F32(0, 0x009d78dc) >= dist) {   /* comiss 50, dist; jb: within 50 of its own base */
                { const uint32_t a[1] = {1}; (void)act_vcall(U32(me, 0xf18) + 0x90, 0x14, 1, a); }
                const float t = rx_adds(F32(rule, 0x18), dt);
                SF32(rule, 0x18, t);
                if (t >= 5.0f) { SF32(rule, 0x18, 0.0f); vehicle_crash(drv_vehicle(U32(rule, 4))); }
            } else SF32(rule, 0x18, 0.0f);
            ACT_CALL(1, rx_capture_base_position(F + 0x30, U32(F, 0x14), act));
            v = drv_vehicle(U32(rule, 4));
            dist = sqrt_call(dist2_to(v, F32(F, 0x30), F32(F, 0x34), F32(F, 0x38)));
            if (700.0f /* 0x977ef8 */ >= dist) {
                { const uint32_t a[1] = {1}; (void)act_vcall(U32(me, 0xf18) + 0x90, 0x14, 1, a); }
                for (int32_t i = 0; i < I32(act, 0x26c); i++) {
                    SU32(F, 0x14, (uint32_t)i);
                    const rx_addr o = U32(U32(act, 0x268), 4u * (uint32_t)i);
                    if (U32(o, 0xf14) == team) continue;
                    const rx_addr ov = drv_vehicle(o), mv = drv_vehicle(U32(rule, 4));
                    const float dz = rx_subs(F32(ov, 0xc8), F32(mv, 0xc8)), dy = rx_subs(F32(ov, 0xc4), F32(mv, 0xc4)), dx = rx_subs(F32(ov, 0xc0), F32(mv, 0xc0));
                    const float od = sqrt_call(rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx)));
                    if (5.0f > od) vehicle_crash(mv);
                }
            }
        }
        goto returns;
    }
    {   /* carrying */
        const rx_addr veh = drv_vehicle(d);
        { const uint32_t a[1] = {F + 0x40}; (void)act_vcall(veh, 0x3c, 1, a); }
        SF32(F, 0x74, rx_adds(F32(F, 0x74), 6.5f /* 0x977ef4 */));
        if (team == 0) { const uint32_t a[1] = {F + 0x40}; (void)act_vcall(BLUE(act), 0x40, 1, a); }
        else if (team == 1) { const uint32_t a[1] = {F + 0x40}; (void)act_vcall(RED(act), 0x40, 1, a); }
        if (U32(drv_vehicle(U32(rule, 4)), 0x280) != 0) {   /* a wreck drops the flag */
            if (net != 0 && (rx_net_ref_valid(net) & 0xff) == 0) goto returns;
            SF32(rule, 0x10, 0.0f);
            SU32(rule, 0xc, 0);
            SU8(U32(rule, 4), 0xf1c, 0);
            const rx_addr v = drv_vehicle(U32(rule, 4));
            SF32(F, 0x20, F32(v, 0xc0));
            SF32(F, 0x24, F32(v, 0xc4));
            SF32(F, 0x28, F32(v, 0xc8));
            SF32(F, 0x24, rx_adds(F32(v, 0xc4), 7.0f /* 0x930004 */));
            if (team == 0) {
                { const uint32_t a[1] = {F + 0x20}; (void)act_vcall(BLUE(act), 0x70, 1, a); }
                SU32(act, 0xff39c, 0);
            } else if (team == 1) {
                { const uint32_t a[1] = {F + 0x20}; (void)act_vcall(RED(act), 0x70, 1, a); }
                SU32(act, 0xff398, 0);
            }
            { const uint32_t a[1] = {rule}; (void)act_vcall(U32(rule, 4), 0x24, 1, a); }
            goto returns;
        }
        if (!MAY_ACT(d)) goto returns;
        ACT_CALL(1, rx_capture_base_position(F + 0x30, team, act));
        const rx_addr v = drv_vehicle(U32(rule, 4));
        if (F32(rule, 0x14) > dist2_to(v, F32(F, 0x30), F32(F, 0x34), F32(F, 0x38))) {
            const uint32_t home = team == 0 ? U8(act, 0xff430) : team == 1 ? U8(act, 0xff431) : 0;
            if ((team == 0 || team == 1) && home != 0) {
                ACT_CALL(1, rx_capture_score(act, team));
                SU32(rule, 0xc, 0);
                SU8(U32(rule, 4), 0xf1c, 0);
            }
        }
    }
returns:
    /* its own team's dropped flag returned home */
    if (net != 0) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0 || U32(U32(U32(F, 0x18), 0x74), 0x1e4) != 0) goto end;
    }
    {
        const rx_addr own = team == 0 ? RED(act) : team == 1 ? BLUE(act) : 0;
        { const uint32_t a[1] = {F + 0x30}; (void)act_vcall(own, 0x44, 1, a); }
        const rx_addr v = drv_vehicle(U32(rule, 4));
        const float dx = rx_subs(F32(F, 0x30), F32(v, 0xc0)), dy = rx_subs(F32(F, 0x34), F32(v, 0xc4)), dz = rx_subs(F32(F, 0x38), F32(v, 0xc8));
        if (!(F32(rule, 0x10) >= 5.0f)) goto end;
        if (!(F32(rule, 0x14) > rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx)))) goto end;
        SU32(F, 0x18, 0);
        const uint32_t slot = team == 0 ? 0xff398u : 0xff39cu;
        if (team == 0 || team == 1) {
            const rx_addr carrier = U32(act, slot);
            if (carrier != 0) {
                const rx_addr cn = U32(carrier, 0xef0);
                if (team == 1) SU32(F, 0x18, cn);
                if (cn == 0 || (rx_net_ref_valid(cn) & 0xff) != 0) vehicle_crash(drv_vehicle(U32(act, slot)));
                rx_addr r;
                ACT_CALLR(r, 1, rx_driver_rule_capture(U32(act, slot)));
                SU32(r, 0xc, 0);
                SU8(U32(act, slot), 0xf1c, 0);
                SU32(act, slot, 0);
            }
            if (team == 0) ACT_CALL(1, rx_capture_flag_home(0, act));
            else flag_home(F + 0x30, act, 1);
        }
        session_static();
        if (session_networked() && U32(act, 0xff434) != 0) act_unreached(0x0051d378, "ONLINE: the host's flag-return message (006f1230)");
    }
end:
    race_profile_end(scope);
    #undef MAY_ACT
    ACT_END(F);
    return 0;
}
RX_FUNCTION(0x0051cab0, rx_capture_rule_vf5)
