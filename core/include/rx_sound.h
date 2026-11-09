#ifndef RX_SOUND_H
#define RX_SOUND_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The rest of the original's game sound, as read-only state for the host's sound layer (w-sfx, 2026-10-27; core/plan/QUEUE.md "w-sfx").
 * The engine emitters' input is rx_session_vehicle_audio (rx_vehicle_audio.h); this query carries what the other managers of the
 * original's sound layer read. Audio (FMOD) is a core boundary: the managers and emitters run on the host (Unity/Reflex/Assets/Scripts/
 * OriginalWorld/OriginalGameSound.cs), ported from the decompile; nothing here is written to the game. Additive: RX_SESSION_API_VERSION
 * unchanged, listed in rx_session_sizeof.
 *
 * Channels  the level volume mix the core's AudioVolumeMixEntity (scene_fx_entities.c audio_mix_apply, 0067d810) wrote into the channel
 *           records (0xd679a0 + 0x54 k, +0: the mix of the group (players - 1) / 3): Global Effects UI Music Engine Crowd DriveOn Skid
 *           Scenario VoiceOver (names 0x9c49a0). An event's volume is the user setting x this x the channel ramp x the event's own
 *           (FUN_0080b490); 1.0 where no mix entity ran.
 * Riders    per rider the manager snapshot's wheel groups (VehicleAudioManager_UpdateVehicles 00680ec0 + the vehicle's vf52, group
 *           0x70 bytes; 2 groups for a bike, 4 otherwise) as the drive-on emitter (006828b0), the foley emitter (0068b9e0) and the tyre
 *           hits / splashes (0067cc80 / 0067cbb0) read them, the terrain audio zone under each wheel (TerrainMaterialAt layer 3, as the
 *           manager computes snapshot group + 0x5c), the contact object's collision class / sound id / material, and the vehicle's own.
 * Contacts  the physics worlds' contact events (world + 0, 64 bytes each, count + 0x1900), copied in World_PostListeners (00769ab0)
 *           where the original hands them to the AudioCollisionManager (0067bac0 -> 0067bb10 Add), with what AudioCollision_Play
 *           (0067bd30) / _Update (0067c2c0 / 0067c350) ask the objects (class vf22, sound id vf12 / vf54, material vf13, rider tests,
 *           the slide speed, the terrain audio zone at the contact). A ring with serials; the host applies the entries newer than its
 *           last serial (the record pools, the 200 ms keys and the events are the host's, as in the manager).
 * Crowd     the crowd momentum block (0xc023a8 .., Crowd_Momentum = 0xc023b8), the crowd type / size words and a ring of the crowd
 *           reactions (006745d0 kind 1 negative, 2 positive, 3 positive start / end).
 * Cues      the activity audio cues the ActivityAudioLogic played (005812f0: the cue's "<SoundAsset>/<EventName>", its sound index).
 * Music     the MusicPlayer (G + 0x15c6c): mode, delay, requested track / score, playlist (its game state: game/music_player.c).
 * Riders'   the rider sound slots gameplay code starts (006817c0). */
#define RX_SOUND_VERSION 2u   /* 2 (w-rc-core): riders 16..31 in riders_ext (29320 bytes); version 1 (23944 bytes, 16 riders) is still served */
#define RX_SOUND_V1_SIZE 23944u
#define RX_SOUND_CHANNELS 16
#define RX_SOUND_RIDERS 16
#define RX_SOUND_CONTACTS 128
#define RX_SOUND_REACTIONS 16
#define RX_SOUND_CUES 16
#define RX_SOUND_CUE_PATH 88
#define RX_SOUND_PLAYLIST 32
#define RX_SOUND_NAME 32
#define RX_SOUND_RIDER_SOUNDS 8

/* collision classes (vf22, vtable + 0x58) the collision manager tests */
enum { RX_SC_RIDER_ENTITY = 0x13, RX_SC_TERRAIN = 0x10, RX_SC_WATER = 0xf, RX_SC_FOREST = 0x100f, RX_SC_COLLISION_ENTITY = 5,
       RX_SC_PHYSICS_ENTITY = 0x1029, RX_SC_VEHICLE = 0x1082 };
enum {
    RX_SW_AIR = 1u,          /* group + 0x48: not in contact */
    RX_SW_WATER = 2u,        /* + 0x50: the wheel is in water */
    RX_SW_WATER_ENTERED = 4u /* + 0x54: the wheel entered water */
};

typedef struct rx_sound_wheel {
    float position[3];            /* + 0x00 the contact / hub point */
    float normal[3];              /* + 0x10 the contact vector (the tyre-hit strength's direction) */
    float side_slip2;             /* |+ 0x20|^2 ((x^2 + y^2) + z^2: the drive-on / foley SideSlip value) */
    float roll_slip2;             /* |+ 0x30|^2 (RollSlip) */
    float suspension_force;       /* + 0x40 */
    float angular_velocity;       /* + 0x44 */
    uint32_t flags;               /* RX_SW_* */
    float depth;                  /* + 0x58 water depth 0 .. 1 */
    int32_t zone;                 /* + 0x5c the terrain audio zone (TerrainMaterialAt layer 3), -1 airborne / outside the terrain */
    int32_t contact_id;           /* + 0x60 the contact object's DriveOnMaterial (its + 0x158 when its class is 5), -1 none */
    uint32_t contact;             /* + 0x64 the contact object (identity; never dereferenced by the host), 0 none */
    int32_t contact_class, contact_sound, contact_material;   /* the contact object's vf22 / vf12 / vf13 (0 / 0 / 5 without one) */
} rx_sound_wheel;                 /* 72 bytes */

typedef struct rx_sound_rider {
    int32_t slot;                 /* the player slot */
    uint32_t vehicle;             /* identity (rx_vehicle_audio_rider.vehicle) */
    int32_t wheel_count;          /* 2 bike (V + 0x194 == 0), else 4 */
    int32_t vehicle_class;        /* RX_VA_CLASS_* */
    int32_t sound_id, material;   /* the vehicle's vf12 (MX / ATV: crashed (+0x280) ? 46 : 2; UTV / Buggy 0x31 : 0x30) and vf13 */
    float velocity[3];            /* snapshot + 0x1e0 (vf37) */
    int32_t sound_feature0;       /* the vehicle's vf54(0) (+ 0xd8): the sound id AudioCollision_Play uses for a vehicle side whose
                                     feature index is 0, as in the manager's own splash / tyre-hit records (0067cbb0 / 0067cc80) */
    int32_t reserved[2];
    rx_sound_wheel wheels[4];
} rx_sound_rider;                 /* 48 + 4 * 72 = 336 bytes */

enum {
    RX_SCF_SOFT = 1u,             /* the event's + 0x20 byte (a soft / tree contact) */
    RX_SCF_RIDER_A = 2u, RX_SCF_RIDER_B = 4u,           /* side is a rider (class 0x13 with vf12 == 0x31) */
    RX_SCF_VOCAL_A = 8u, RX_SCF_VOCAL_B = 16u,          /* side may make its rider grunt / yell (a bike with V + 0x194 <= 1; a rider with + 0xae70 == 0) */
    RX_SCF_RAGDOLL_A = 32u, RX_SCF_RAGDOLL_B = 64u,     /* a rider side's + 0x5bb68 (first ground hit: yell) */
    RX_SCF_SLIDE_B = 128u         /* B is a class 5 object with + 0x19c (its own slide loop, 0067c670) */
};
typedef struct rx_sound_contact {
    uint32_t serial;              /* 0 = empty */
    uint32_t frame;               /* the engine frame ([[0xa95844] + 0x10] + 0x58), the manager's "same frame" test */
    uint32_t a, b;                /* the two objects (identities; b 0 for none) */
    int32_t class_a, class_b;     /* vf22 (0 without the object) */
    int32_t sound_a, sound_b;     /* the sound id per side as Play chooses it (a bike with a feature index >= 0: vf54(index), else vf12) */
    int32_t material_a, material_b;   /* vf13 (5 without the object) */
    uint32_t flags;               /* RX_SCF_* */
    float strength_a, strength_b; /* + 0x24 / + 0x28 (raw: the host scales them as Play does) */
    int32_t sound_override;       /* + 0x2c (Play uses it for B when B is null or class 0x100f) */
    int32_t feature_a, feature_b; /* + 0x30 / + 0x34 */
    float position[3];            /* + 0x10 */
    uint32_t vocal_a, vocal_b;    /* the vehicle whose rider sounds a vocal (a bike side: itself; a rider: its + 0xaf1c), 0 none */
    float slide_speed;            /* A's speed for the roll / slide loop (0067c350; also RX_SCF_SLIDE_B contacts, 0067c670): a rider |mean velocity| x AudioRiderSlideScaleFactor
                                     is the host's; here the raw speed (ft/s): rider, physics entity (0041cf30) or vehicle (+ 0x150) */
    int32_t slide_kind;           /* 0 none (A's strength in the force range), 1 rider, 2 physics entity, 3 vehicle */
    int32_t zone;                 /* the terrain audio zone at the position (when B is class 0x10), else -1 */
    int32_t object_material;      /* B + 0x158 when RX_SCF_SLIDE_B */
    int32_t reserved[7];
} rx_sound_contact;               /* 128 bytes */

typedef struct rx_sound_crowd {
    uint32_t valid;               /* the crowd momentum block runs (the crowd unit ported in the product) */
    int32_t type;                 /* 0 stadium (CROWD_IND_*), 1 outdoor ([0xa941b4], 00674680), -1 none */
    int32_t size, size_requested; /* [0x9c47bc] / [0x9c47c0]: -1 none, 0 small, 1 medium, 2 large */
    float momentum;               /* [0xc023b8] Crowd_Momentum 0 .. 10 */
    int32_t state;                /* [0xc02400] */
    uint32_t reaction_serial;     /* the newest reaction's serial */
    int32_t reserved[2];
    struct { uint32_t serial; int32_t kind; } reactions[RX_SOUND_REACTIONS];   /* 006745d0: 1 CROWD_RCT_NEG, 2 _POS, 3 _POS_START_END */
} rx_sound_crowd;                 /* 36 + 128 = 164 bytes */

typedef struct rx_sound_cue {
    uint32_t serial;              /* 0 = empty */
    int32_t index;                /* the cue's sound index (+ 0x10), -1 none */
    char path[RX_SOUND_CUE_PATH]; /* "<SoundAsset>/<EventName>" (cue + 8), tokens ([LOC] ...) unexpanded */
} rx_sound_cue;                   /* 96 bytes */

typedef struct rx_sound_music {
    uint32_t valid;               /* the music player state is the game's (its init ran) */
    int32_t mode;                 /* + 8: 0 none, 1 stopped, 2 jukebox, 3 UI, 4 scenario score */
    uint32_t flags;               /* + 4 (8: fading out) */
    int32_t kind;                 /* + 0x1dc the activity kind */
    float delay;                  /* + 0x1d8 the jukebox start delay left (s) */
    int32_t requested, current;   /* + 0x1d0 / + 0x1d4 track indices (MusicAudioMap Jukebox order), -1 */
    int32_t score_requested, score_current;   /* + 0x398 / + 0x39c (ScenarioMusic order), -1 */
    int32_t position, count;      /* + 0x570, + 0x560 */
    float transition, start_delay;/* the activity kind's ActivityTransitionTimes / ActivityStartDelay (s) */
    uint32_t serial;              /* counts the player's transitions (start jukebox / score, stop, fade) */
    int32_t reserved[2];
    int32_t playlist[RX_SOUND_PLAYLIST];      /* + 0x55c */
    char track[RX_SOUND_NAME];    /* the requested track's BankName (MUS_INGAME_nn), "" none */
    char score[RX_SOUND_NAME];    /* the requested score's BankName (MUS_SCORE_nn), "" none */
} rx_sound_music;                 /* 64 + 128 + 64 = 256 bytes */

typedef struct rx_sound_state {
    uint32_t size, version, valid;
    int32_t rider_count, channel_count;
    uint32_t contact_serial;      /* the newest contact's serial */
    uint32_t cue_serial;          /* the newest cue's serial */
    uint32_t frame;               /* the engine frame now */
    float channel_mix[RX_SOUND_CHANNELS];
    rx_sound_rider riders[RX_SOUND_RIDERS];
    rx_sound_contact contacts[RX_SOUND_CONTACTS];
    rx_sound_crowd crowd;
    rx_sound_cue cues[RX_SOUND_CUES];
    rx_sound_music music;
    uint32_t rider_sound_serial;  /* the newest rider sound's serial */
    struct { uint32_t serial; uint32_t vehicle; int32_t slot; int32_t reserved; } rider_sounds[RX_SOUND_RIDER_SOUNDS];
                                  /* 006817c0: a rider's sound slot started by gameplay code (ActivityAudioLogic vf12 slot 1 WreckRecoverySuccess,
                                     vf13 slot 2 WreckRecoveryFail; the collisions' grunts / yells are the host's own) */
    rx_sound_rider riders_ext[RX_SOUND_RIDERS];   /* version 2: riders 16 .. 31 (rider_count counts both arrays) */
} rx_sound_state;                 /* 32 + 64 + 16 * 336 + 128 * 128 + 164 + 16 * 96 + 256 + 4 + 8 * 16 + 16 * 336 = 29320 bytes (version 1: 23944, without riders_ext) */

/* Caller sets size and version (1). 1 on bad arguments (output untouched), else 0 (valid 0 while no world runs). Session owner thread,
 * between advances. Nothing the game reads is changed (the terrain zone lookups restore what they touch). */
RX_API int rx_session_sound(struct rx_session* session, rx_sound_state* out);
/* Additive (w-reverb2): 1 while the local player's selected camera is the first-person camera (entity class 0x1002) with its pose valid (+ 0x2f4) -
 * the test of FUN_0068bfb0 (the foley emitter's rider vocal position) and of the camera sound effect's active flag (0068d2c0: the first-person reverb);
 * else 0 (also with no world). Read only. */
RX_API int rx_session_first_person_view(struct rx_session* session);
#ifdef __cplusplus
}
#endif
#endif
