#ifndef RX_VEHICLE_AUDIO_H
#define RX_VEHICLE_AUDIO_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* What the original's vehicle sound layer reads from the simulation every frame (w-audio, 2026-10-27). Copies only; the sound layer
 * itself (the engine sound emitters, the FMOD event commands) is on the host's side of the audio boundary (core/plan/QUEUE.md w-audio).
 * Additive: a new query and struct, RX_SESSION_API_VERSION unchanged.
 *
 * Riders     VehicleAudioManager_UpdateVehicles (00680ec0) gives each registered vehicle (12 slots, 00680b10) a 0x280-byte telemetry
 *            snapshot through the vehicle's vf52 (RavenVehicleMotocross 00467070, RavenVehicleATV 00487510, RavenVehicleUTV 004c78b0,
 *            RavenVehicleBuggy 004acf50, RavenVehicleTrophyTruck 00473e80) and VehicleEngineAudio_Update (00685610) runs the engine
 *            emitter's state machine on it. Each rider here carries the snapshot fields the emitter and the manager read, with the
 *            snapshot offset in brackets, plus the vehicle / player words the emitter tests directly:
 *              position  V + 0xc0 (vf17 004301f0)          velocity  simulation + 0x100 (vf37)   speed [+0x220] = |velocity| (ft/s)
 *              rpm [+0x224] vf47 (engine rad/s * 9.55)     max_rpm [+0x228] vf48               idle_rpm (the engine step's minimum)
 *              throttle [+0x22c] / control2 [+0x230] (the controls block +0 / +4)   gear [+0x234] vf42 (gear + 1)   load [+0x240]
 *              clutch [+0x244] vf50   airborne [+0x248] (every contact flag clear)   below_half [+0x250] (MX: sim + 0x8adc < -0.5)
 *              water [+0x254] V + 0x131c   underwater [+0x258] V + 0x1310   drowned [+0x25c] V + 0x1314
 *              wrecked [+0x260] (MX: the rider V + 0x1a0 has +0xaf00 set; 0 for the other classes, as their vf52 writes)
 *              wheel_air[i] [+0x48 + 0x70 i] (the wheel / contact group's "not in contact" word; MX: groups 0 / 1 = sim + 0x8894 / 0x89e4)
 *              vehicle_kind V + 0x194 (0 bike, 1 ATV), engine_size V + 0x170 (the VehicleEngineSize index the emitter's +0xc copies),
 *              pad = player + 0x1f0 (-1 = no pad: a computer rider), flags (player V + 0x190 present, its +4 / +0x10, manual gearbox
 *              V + 0x234 -> +0x2eb0).
 *            The manager's derived words (landed [+0x24c], gear up / down [+0x238] / [+0x23c], wheel landed [+0x4c ..]) compare two
 *            consecutive snapshots: the host computes them from consecutive queries, as the manager does.
 * Cues       the sound-layer commands the gameplay code issues that the product's audio boundary drops: the race intro scenario's
 *            SoundEffect events (0055d610 -> 0067edc0 -> Scenario_AudioCommand 0067ecf0: "{EnableStartLineState}",
 *            "{DisableStartLineState}", "{StartScenarioMusic|<name>}", "{StopScenarioMusic}"; plain sound names are rx_session_scenario_sounds') and
 *            ActivityAudioLogic::vf16 "Pre Exited" (00581f90 -> 006818e0(1): every engine emitter leaves the start line). A ring of the
 *            last RX_VEHICLE_AUDIO_CUES, each with a serial that counts from 1 for the session; the host applies the entries newer than
 *            the last serial it saw (cue_serial = the newest). */
#define RX_VEHICLE_AUDIO_VERSION 2u   /* 2 (w-rc-core): riders 16..31 in riders_ext (5664 bytes); version 1 (3360 bytes, 16 riders) is still served */
#define RX_VEHICLE_AUDIO_V1_SIZE 3360u
#define RX_VEHICLE_AUDIO_RIDERS 16
#define RX_VEHICLE_AUDIO_CUES 16
#define RX_VEHICLE_AUDIO_CUE_NAME 56

enum {
    RX_VA_VALID = 1u,            /* the rider's vehicle exists and its class is known */
    RX_VA_HAS_PLAYER = 2u,       /* V + 0x190 (the vehicle's player) is set */
    RX_VA_LOCAL_HUMAN = 4u,      /* player + 4 != 0 and player + 0x10 == 0 (the emitter's "no _AI suffix, AIFX 0" test, 006891d0 / 00686370) */
    RX_VA_MANUAL_GEARBOX = 8u,   /* V + 0x234 -> +0x2eb0: picks UseShiftStateWithManTrans over ...AutoTrans */
    RX_VA_HUMAN_SLOT = 16u,      /* the player slot's type (P + 0x10) is 0 human */
    RX_VA_AUDIO_SLOT = 32u,      /* audio_slot holds the vehicle sound manager's slot (w-sfx; 0 .. 11) */
    RX_VA_START_LINE = 64u       /* start_line_* hold the core's start line of the slot's engine emitter (w-rng) */
};
enum { RX_VA_CLASS_NONE = 0, RX_VA_CLASS_MX = 1, RX_VA_CLASS_ATV = 2, RX_VA_CLASS_UTV = 3, RX_VA_CLASS_BUGGY = 4, RX_VA_CLASS_TRUCK = 5 };
enum {
    RX_VA_CUE_START_LINE_ENABLE = 1,   /* Scenario_AudioCommand "EnableStartLineState" (00681880) */
    RX_VA_CUE_START_LINE_DISABLE = 2,  /* "DisableStartLineState" */
    RX_VA_CUE_PRE_EXITED = 3,          /* ActivityAudioLogic::vf16 -> 006818e0(1) */
    RX_VA_CUE_SCENARIO_SOUND = 4,      /* reserved (plain scenario sound names are rx_session_scenario_sounds', w-frscene) */
    RX_VA_CUE_SCENARIO_MUSIC_START = 5,/* "StartScenarioMusic" (name: its argument) */
    RX_VA_CUE_SCENARIO_MUSIC_STOP = 6, /* "StopScenarioMusic" */
    RX_VA_CUE_OTHER = 7                /* another "{...}" command (name: the command) */
};

typedef struct rx_vehicle_audio_rider {
    int32_t slot;                 /* the player slot (G + 0x7460 + slot * 0x12a0) */
    uint32_t flags;               /* RX_VA_* */
    int32_t vehicle_class;        /* RX_VA_CLASS_* (from the vehicle's vtable) */
    int32_t vehicle_kind;         /* V + 0x194 */
    int32_t pad;                  /* player + 0x1f0 */
    int32_t engine_size;          /* V + 0x170 */
    uint32_t vehicle;             /* the vehicle's guest address (identity across frames; never dereferenced by the host) */
    float position[3];
    float velocity[3];
    float speed, rpm, max_rpm, idle_rpm, throttle, control2;
    int32_t gear;
    float load;
    uint32_t clutch, airborne, below_half;
    float water;
    uint32_t underwater, drowned, wrecked;
    uint32_t wheel_air[4];
    int32_t audio_slot;           /* the vehicle sound manager's slot 0 .. 11 (00680b10: the first free slot at registration), -1 none
                                     (w-sfx, was reserved: valid with RX_VA_AUDIO_SLOT). The engine emitter's +0: its AIFX for non-local riders. */
    /* w-rng (were reserved; valid with RX_VA_START_LINE): the engine emitter's start line, which the core runs because it draws the
     * game's random generator (game/engine_startline.c: 00681880 / 00686e20 / 00686ec0 / 00686870 / 006869a0 / 00686220 / 006818e0).
     *   start_line_state   the emitter's state as far as the start line: 4 StartLine, 2 Rev, -1 another state (the host's own machine)
     *   start_line_serial  +1 at every StartLine / Rev entry and every replay of the rev event: the host enters the state (or plays the
     *                      rev event again) when it changes; the core decided the time to the next rev, the revs in a row and the target
     *   start_line_target  a car's rev target rpm (+0x2d8: vf48 x the drawn fraction of StartLineRPMMin/MaxPerc)
     * A bike's rev one-shot ending is the sound engine's: the host reports it with rx_session_vehicle_audio_finished. */
    int32_t start_line_state;
    uint32_t start_line_serial;
    float start_line_target;
} rx_vehicle_audio_rider;          /* 144 bytes */

typedef struct rx_vehicle_audio_cue {
    uint32_t serial;              /* 0 = empty */
    int32_t kind;                 /* RX_VA_CUE_* */
    char name[RX_VEHICLE_AUDIO_CUE_NAME];
} rx_vehicle_audio_cue;            /* 64 bytes */

typedef struct rx_vehicle_audio_state {
    uint32_t size, version, valid;
    int32_t rider_count;
    uint32_t cue_serial;          /* the newest cue's serial (0 none) */
    int32_t reserved[3];
    rx_vehicle_audio_rider riders[RX_VEHICLE_AUDIO_RIDERS];
    rx_vehicle_audio_cue cues[RX_VEHICLE_AUDIO_CUES];
    rx_vehicle_audio_rider riders_ext[RX_VEHICLE_AUDIO_RIDERS];   /* version 2: riders 16 .. 31 (rider_count counts both arrays) */
} rx_vehicle_audio_state;          /* 32 + 16 * 144 + 16 * 64 + 16 * 144 = 5664 bytes (version 1: 3360, without riders_ext) */

/* Caller sets size and version (1). 1 on bad arguments (output untouched), else 0 (valid 0 while no world runs). Session owner thread,
 * between advances. Reads only; nothing is written to the game. */
RX_API int rx_session_vehicle_audio(struct rx_session* session, rx_vehicle_audio_state* out);
/* The host played the rev event of the engine emitter in sound manager slot `audio_slot` (0 .. 11) to its end (FMOD EVENT_FINISHED: the
 * event wrapper's state 7). The core's start line takes it at the next frame's audio tick (a bike's Rev: the next rev or back to
 * StartLine), as the original's tick does. 0 = taken, 1 = bad arguments / no vehicle in that slot. Session owner thread, between
 * advances (w-rng). */
RX_API int rx_session_vehicle_audio_finished(struct rx_session* session, int32_t audio_slot);
#ifdef __cplusplus
}
#endif
#endif
