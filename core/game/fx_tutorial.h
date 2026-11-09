/* fx_tutorial.h -- the MotoSkills TutorialManager (fx_tutorial.c: ShowLessonObjects; fx_tutorial_manager.c: the manager; _re/notes/tutorials.md).
 * MXUIWorld::vf1 (007651d0) builds the manager (0xc0 bytes, MXUIWorld + 0x22180) and MXUIWorld::vf5 (00765670) updates it every frame of the
 * UI world. Its events come through its channel [0x00a946a8] ("TutorialManager": EnableTutorialManager, DisableTutorialManager, RetryTutorial,
 * SetValidPlayerCount, SetValidVehicle, InUIWorld) and the trigger registry's VehicleEntered / VehicleExited; its UI calls through
 * fx_tutorial_ui.h. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

void     rx_tutorial_show_lesson_objects(rx_addr mgr, int32_t lesson);   /* 00751c40 (stdcall: manager, lesson; RET 8) */

rx_addr  rx_tutorial_construct(rx_addr m);                               /* 0074fd10 TutorialManager_Construct (ESI = m; EAX = m) */
void     rx_tutorial_update(rx_addr m, float dt);                        /* 007500c0 TutorialManager_Update (stdcall m, dt; RET 8) */
int32_t  rx_tutorial_lesson_of_name(rx_addr name);                       /* 0074fcd0 (cdecl name) */
void     rx_tutorial_enable_lesson(int32_t lesson, rx_addr m);           /* 0074fea0 (ESI = m; stdcall lesson; RET 4) */
void     rx_tutorial_reset_lesson(uint32_t first, rx_addr m);            /* 00751740 (EAX = m; stdcall first; RET 4) */
void     rx_tutorial_start_lesson(rx_addr m);                            /* 007517e0 (stdcall m; RET 4) */
void     rx_tutorial_outcome(rx_addr m);                                 /* 007520a0 (stdcall m; RET 4) */
void     rx_tutorial_on_retry(rx_addr m);                                /* 00752740 (ECX = m) */
uint32_t rx_tutorial_landing_failed(rx_addr m);                          /* 00750a70 (stdcall m; RET 4) */
void     rx_tutorial_check_wreck(float dt, rx_addr m);                   /* 00750b30 (ESI = m; stdcall dt; RET 4) */
void     rx_tutorial_check_hold_tight(rx_addr m, float dt);              /* 00750c00 (stdcall m, dt; RET 8) */
void     rx_tutorial_check_lean_in(rx_addr m, float dt);                 /* 00750d00 (stdcall m, dt; RET 8) */
void     rx_tutorial_check_take_off_and_landing(float dt, rx_addr m);    /* 00750e60 (ESI = m; stdcall dt; RET 4) */
void     rx_tutorial_check_wreck_avoidance(float dt, rx_addr m);         /* 007510c0 (ESI = m; stdcall dt; RET 4) */
void     rx_tutorial_check_freestyle_tricks(float dt, rx_addr m);        /* 00751320 (ESI = m; stdcall dt; RET 4) */
void     rx_tutorial_check_rider_reflex(rx_addr m);                      /* 007515b0 (stdcall m; RET 4) */
void     rx_tutorial_knock_request(rx_addr req, uint32_t index);         /* 006381c0 (EAX = req, ECX = index) */
void     rx_tutorial_play_lesson_voice(int32_t lesson, int32_t kind);    /* 007527c0 (cdecl lesson, kind) */
void     rx_tutorial_read_audio_settings(void);                          /* 00752a40 */
void     rx_tutorial_load_audio(void);                                   /* 00752b10 */
void     rx_tutorial_unload_audio(void);                                 /* 00752b60 */
void     rx_tutorial_raise_completed(uint32_t controller, int32_t lesson);   /* 004e8460 (EAX = controller; stdcall lesson; RET 4) */
void     rx_tutorial_profiles_mark(rx_addr pm);                          /* 007532a0 (EDX = the profile manager) */
void     rx_tutorial_on_enable(rx_addr m, int32_t lesson);               /* 00752630 (ECX = m; stdcall lesson; RET 4) */
void     rx_tutorial_on_disable(rx_addr m);                              /* 00752650 (ECX = m) */
void     rx_tutorial_on_valid_player_count(rx_addr m, uint32_t v);       /* 00752680 (ECX = m; stdcall v; RET 4) */
void     rx_tutorial_on_valid_vehicle(rx_addr m, uint32_t v);            /* 007526c0 */
void     rx_tutorial_on_in_ui_world(rx_addr m, uint32_t v);              /* 00752700 */
void     rx_tutorial_on_vehicle_entered(rx_addr m, rx_addr vehicle, rx_addr trigger);   /* 007523f0 (ECX = m; stdcall vehicle, trigger; RET 8) */
void     rx_tutorial_on_vehicle_exited(rx_addr m, rx_addr vehicle, rx_addr trigger);    /* 00752500 */
void     rx_tutorial_app_enable(int32_t lesson);                         /* 007410f0 App_EnableTutorial (EAX = lesson) */
void     rx_tutorial_app_disable(void);                                  /* App.DisableTutorial (00729f90's event) */
void     rx_tutorial_app_retry(void);                                    /* FailedTutorialPopup RETRY: "RetryTutorial" */
void     rx_app_pause_active_world(uint32_t paused);                 /* 007424a0 (EDX = paused): App.PauseActiveWorld */
void     rx_ui_enable_tutorial_mode(rx_addr ui, uint32_t enable);        /* 00599820 MXUIManager_EnableTutorialMode (ECX = ui; stdcall enable; RET 4) */

#ifdef __cplusplus
}
#endif
