/* engine_startline.h -- the engine sound emitters' start line on the product's side of the audio boundary (engine_startline.c, w-rng). */
#pragma once
#include "rx_game.h"

#define RX_SL_SLOTS 32   /* (w-rc-core) the original's 12 emitters + 20 for riders 13..32 (host-side; slots 12.. stay empty at <= 12 riders) */
/* the emitter's state (+0x2ac) as far as the start line needs it: 2 Rev, 4 StartLine, anything else is "other" */
enum { RX_SL_OTHER = -1, RX_SL_REV = 2, RX_SL_START = 4 };

/* The ValueSmoother_M at the emitter's +0x2bc (0067f1c0 / 0067f210's layout: value, prev, up, hold, down, time, phase). */
typedef struct { float value, prev, up, hold, down, time; uint32_t phase; } rx_sl_ramp;

/* One emitter (VehicleAudioManager [0xd92f00] + 0x520 + k * 0x1b10): the words the start line reads and writes. */
typedef struct {
    rx_addr vehicle;     /* +0x10 (0: the slot is free) */
    int32_t size;        /* +0xc the engine size (V + 0x170 at the bind; 10 before) */
    int32_t state;       /* +0x2ac: RX_SL_* */
    uint32_t start_line; /* +0x2b8 */
    float timer;         /* +0x2e0: the StartLine's countdown to the next rev */
    int32_t visits;      /* +0x2e4: the Rev entries since the start line began (StartLineMaxRevs) */
    int32_t seq;         /* +0x2e8: the revs of this Rev (StartLineMaxSeqRevs) */
    float target;        /* +0x2d8: a car's rev target rpm (vf48 x the drawn fraction) */
    float rev_rpm;       /* +0x2dc: a car's rev rpm */
    rx_sl_ramp ramp;     /* +0x2bc: a car's rev ramp */
    uint32_t finished;   /* a bike's rev one-shot reported finished (the event wrapper +0x33c's state 7, +0x474) */
    uint32_t serial;     /* counts every StartLine / Rev entry and every rev replay (the host plays the event) */
} rx_sl_emitter;

/* What VehicleAudioManager_UpdateVehicles (00680ec0) hands the emitter of a slot this frame: the snapshot's words (+0x14 -> +0x224 rpm,
 * +0x258 underwater, +0x25c drowned, +0x260 wrecked) and the vehicle's vf48 (max rpm). */
typedef struct { uint32_t underwater, drowned, wrecked; float rpm, max_rpm; } rx_sl_input;

void rx_engine_start_line_reset(void);
rx_sl_emitter* rx_engine_start_line_slot(int k);          /* NULL out of range */
/* 006855c0's start-line part (the first bind of a vehicle to its slot, 00680b10): the ramp's times, the state leaves the start line */
void rx_engine_start_line_bind(int k, rx_addr vehicle, int32_t size);
void rx_engine_start_line_unbind(int k);
/* 00684c10's start-line keys (MXRegistry <Audio><EngineSoundEmitter>): StartLineRPMRampTime / DownTime / HoldTime / RPMMinPerc / RPMMaxPerc /
 * MaxRevs / MaxSeqRevs / MinTime / MaxTime into their ConVar globals, once per session (the product does not run the audio init 0067da10) */
void rx_engine_start_line_tunables(void);
void rx_engine_start_line_enable(void);                  /* 00681880 ("{EnableStartLineState}") */
void rx_engine_start_line_disable(void);                 /* Scenario_AudioCommand 0067ecf0 "DisableStartLineState" */
void rx_engine_start_line_pre_exited(uint32_t to_initial);   /* 006818e0 (ActivityAudioLogic::vf16 "Pre Exited" passes 1) */
/* VehicleEngineAudio_Update 00685610 of slot k, the start-line part (dt = the manager update's) */
void rx_engine_start_line_update(int k, const rx_sl_input* in, float dt);
void rx_engine_start_line_finished(int k);
