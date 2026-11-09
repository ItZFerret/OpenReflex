/* anim_scenario_race_intro.h -- RaceIntroScenario (vtable 0x00943394; also the Waypoint / FMX intro scenarios' shared members): the race's intro steps (anim_scenario_race_intro.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_scenario_start(rx_addr sc);                            /* 00747810 (ESI = the scenario) -> EAX: the scenario's started flag (+0x151b0) */
void     rx_race_intro_vf0(rx_addr sc);                            /* 00749d00 RaceIntroScenario::vf0 (thiscall): the first step */
void     rx_race_intro_vf1(rx_addr sc, uint32_t step);             /* 00749df0 RaceIntroScenario::vf1 (thiscall; RET 4): begin a step */
uint32_t rx_race_intro_vf3(rx_addr sc);                            /* 007497b0 RaceIntroScenario::vf3 (thiscall): the next step */
uint32_t rx_race_intro_vf4(rx_addr sc);                            /* 00749780 RaceIntroScenario::vf4 (thiscall): skip to the last step */
uint32_t rx_race_intro_vf5(rx_addr sc);                            /* 00749fb0 RaceIntroScenario::vf5 (thiscall) */
void     rx_freeride_intro_vf0(rx_addr sc);                        /* 0074d170 FreerideIntroScenario::vf0 (thiscall): the first step */
uint32_t rx_freeride_intro_advance(rx_addr sc);                    /* 0074d060 FreerideIntroScenario::vf4 (thiscall) */
uint32_t rx_freeride_intro_can_skip(rx_addr sc);                   /* 0074d440 FreerideIntroScenario::vf5 (thiscall) */
/* product only: the SoundEffect events the scenario scripts played (0055d610, outside guest memory; rx_session_scenario_sounds) */
int      rx_scenario_sounds_take(char (*names)[128], float (*pos)[3], int32_t* has_pos, int max);
void     rx_scenario_sounds_clear(void);
void     rx_intro_scenario_vf6(rx_addr sc);                        /* 0074eaf0 RaceIntro / WaypointIntroScenario::vf6 (thiscall) */
uint32_t rx_race_intro_vf13(rx_addr sc);                           /* 00749fd0 RaceIntroScenario::vf13 (thiscall): the previous step */
void     rx_intro_scenario_vf12(rx_addr sc);                       /* 0074dd20 FMX / RaceIntroScenario::vf12 (thiscall) */
uint32_t rx_scenario_step_is_7(rx_addr sc);                        /* 0055eb20 BaseScenario::vf9 */
uint32_t rx_scenario_step_is_b(rx_addr sc);                        /* 0055eb30 BaseScenario::vf10 */
rx_addr  rx_race_intro_human_player(rx_addr sc);                   /* 0074d6c0 (EAX = the scenario) -> EAX: the player of the human driver +0x151c0 or 0 */
void     rx_podium_riders_clear_mud(rx_addr sm);                   /* 0074bb90 PodiumRiders_ClearMud (stdcall; RET 4) */
uint32_t rx_race_intro_class_step(rx_addr player);                 /* 00749750 (ECX = the player) -> EAX: 5 for a vehicle class above 1, else 4 */
uint32_t rx_race_intro_series_step(rx_addr sc);                    /* 00749c00 (EDI = the scenario) -> EAX */
uint32_t rx_race_intro_career_step(uint32_t k);                    /* 00749680 (ECX = k 0 .. 7) -> EAX */
void     rx_race_intro_card_flags(uint32_t step, rx_addr sc);      /* 007496d0 (EAX = step, stack scenario; RET 4) */
void     rx_race_intro_card_mask(rx_addr sc, uint32_t k);          /* 00749600 (EAX = scenario, stack k; RET 4) */
rx_addr  rx_race_intro_chief_clip(rx_addr player, uint32_t index); /* 007498a0 (ECX = the player, stack index; RET 4) -> EAX: a name */
rx_addr  rx_race_intro_rider_clip(rx_addr player, uint32_t index); /* 00749ba0 (ECX = the player, stack index; RET 4) -> EAX: a name */
void     rx_race_intro_place_chiefs(rx_addr sc, uint32_t step);    /* 00749910 (stack scenario, step; RET 8) */
void     rx_race_intro_place_riders(uint32_t step, rx_addr sc);    /* 00749b10 (EAX = step, stack scenario; RET 4) */
void     rx_scenario_sim_flag(rx_addr sc);                         /* 00749550 (EDX = the scenario) */
void     rx_scenario_cast_pause(rx_addr sm);
/* the scenario script's event handlers (table 0x9c5120; ECX = the activity, stack: event, flag; RET 8) */
void rx_scenario_event_camera(rx_addr act, rx_addr ev, uint32_t flag);              /* 0055d290 CameraAnimation */
void rx_scenario_event_origin(rx_addr act, rx_addr ev, uint32_t flag);              /* 0055d300 SetScenarioOrigin */
void rx_scenario_event_disable(rx_addr act, rx_addr ev, uint32_t flag);             /* 0055ddc0 DisableEntity */
void rx_scenario_event_timeline(rx_addr act, rx_addr ev, uint32_t flag);            /* 0055e0e0 PlayTimeline */
void rx_camera_path_set_clip(rx_addr e, rx_addr name);                             /* 0063e7c0 (EDI = the path, stack name; RET 4) */
void rx_scenario_camera_setup(rx_addr camera, rx_addr name);
void rx_scenario_event_crowd(rx_addr act, rx_addr ev, uint32_t flag);                /* 0055d7b0 CrowdReaction */
void rx_scenario_event_particle(rx_addr act, rx_addr ev, uint32_t flag);             /* 0055d7e0 ParticleEffectTrigger */
void rx_scenario_event_firework(rx_addr act, rx_addr ev, uint32_t flag);             /* 0055d950 FireworkEffectTrigger */
uint32_t rx_world_crowd_event_name(rx_addr name);                                  /* 006744f0 (cdecl; 1) */
void rx_fx_triggered_effect_start(float delay, rx_addr e);                          /* 006924a0 (EAX = e, xmm0 = delay) */                       /* 0063d780 (stack: camera, name; RET 8) */
                        /* 0074b5a0 (EAX = the cast): every cast entity's +0x378 = 1 */
#ifdef __cplusplus
}
#endif
