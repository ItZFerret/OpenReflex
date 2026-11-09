/* vehicle_audio.h -- the vehicle sound layer's simulation values and the sound-layer cues of gameplay code (vehicle_audio.c). */
#pragma once
#include "rx_game.h"
#include "rx_vehicle_audio.h"
void rx_vehicle_audio_reset(void);
/* 00680b10 / 00680d10 (product): the vehicle sound manager's slot assignment of a vehicle (the AIFX the engine emitter posts). Host-side table. */
void rx_vehicle_audio_register(rx_addr vehicle);
void rx_vehicle_audio_unregister(rx_addr vehicle);
/* 0055d610 SoundEffect (product): the scenario event's sound name (the event + 8), recorded as a cue. */
void rx_vehicle_audio_scenario_sound(rx_addr name);
/* ActivityAudioLogic::vf16 "Pre Exited" (00581f90, product): 006818e0(1), recorded as a cue. */
void rx_vehicle_audio_pre_exited(void);
/* The audio tick's engine-emitter start line (product: MXGameTickDuringSync::vf0's place of 0067dd10; w-rng) */
void rx_vehicle_audio_tick(float dt);
/* The host's report that a slot's rev one-shot finished (rx_session_vehicle_audio_finished). 0 = taken. */
int rx_vehicle_audio_finished(int32_t slot);
/* The query: g = the session's game block (0: cues only). 0 = no world (riders empty). */
int rx_vehicle_audio_fill(rx_addr g, rx_vehicle_audio_state* out);
