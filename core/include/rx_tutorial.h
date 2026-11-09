#ifndef RX_TUTORIAL_H
#define RX_TUTORIAL_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The MotoSkills tutorials (w-tutorials, 2026-10-27; additive, RX_SESSION_API_VERSION unchanged; api/tutorial_session.inc,
 * game/fx_tutorial_manager.c, _re/notes/tutorials.md).
 * The original rides the six lessons in the front end's own world UI_Compound (the MXUIWorld every session builds at create, game +
 * 0x20c8) with its FreeRide activity, game state 2, like the garage's test drive; its TutorialManager (MXUIWorld + 0x22180) runs the
 * lessons. The pages are the host's (the port's front end draws them); the game's side of them is these calls:
 *   rx_session_tutorial_start   MainMenu:OnTutorialKeypress + the Tutorial Load Page, on a fresh session (after the profile / player
 *                               import with the player's equipped setup): the local player, the equipped vehicle kept
 *                               (MXTableHelper:GetEquippedVehicle 004e7c60), App.ChangeVehicle( row, 2 ) (005a0670: the player's
 *                               Favorite MX Lite, else the MX Lite preset), App.SyncDatabaseWithManagers, App.EnableTutorialMode( TRUE )
 *                               (MXUIManager_EnableTutorialMode 00599820: the UI world's tutorial mode, the players active) and the
 *                               loading page's Game_SetupActivityArea. The session is LOADING until the local slot has its vehicle and
 *                               rider.
 *   rx_session_tutorial_pause   App.PauseActiveWorld( paused ) (007424a0): the TutorialPauseMenu's Start (1, with App.DisableTutorial)
 *                               and End (0).
 *   rx_session_tutorial_lesson  App.EnableTutorial( lesson ) (007410f0: "EnableTutorialManager"): the lesson 0..5 starts at its start
 *                               trigger (TutorialPauseMenu:OnAccept).
 *   rx_session_tutorial_disable App.DisableTutorial( ) ("DisableTutorialManager": the pause menu, the Post / Failed popups' QUIT).
 *   rx_session_tutorial_retry   the FailedTutorialPopup's RETRY ("RetryTutorial").
 *   rx_session_tutorial_popup   the host's popup stack: 1 while one of its popups is up (the manager holds the rider: 00743120).
 *   rx_session_tutorial_suspend_input  App.SetUIVariableInt( "SuspendPlayerInput", value ), where the original's pages set it: every
 *                               tutorial popup's Start (1) and End (0), TutorialPauseMenu:Start (1, after App.DisableTutorial, before
 *                               App.PauseActiveWorld( TRUE )), TutorialPauseMenu:OnAccept (0, before App.EnableTutorial). While it is set
 *                               the UI manager's update suspends every local player's input (Player_SetInputSuspended 0054f540: the input
 *                               groups off, their state kept, the pad's vibration paused); clearing it gives the groups back (005a3b00).
 *   rx_session_tutorial_end     the pause menu's QUIT MOTOSKILLS / the conclusion popup's End: App.EnableTutorialMode( FALSE ),
 *                               SuspendPlayerInput 0, the vehicle equipped before the start again (MXTableHelper:EquipVehicle 004e7930).
 *                               The original's save is written after this (RequestSaveGame's worker): the host saves the profile after it.
 *   rx_session_tutorial         the state: the manager's words, the UI events it raised (ShowPreTutorialPopup ...), the STR_ keys it
 *                               set its DYN_ texts from, the voice event paths it asked for, the profile's TutorialsCompleted.
 * Return values: 0, 1 fault / invalid session (last_error), 2 wrong state. */
#define RX_TUTORIAL_API_VERSION 1u
enum {
    RX_TUTORIAL_EVENT_NONE = 0, RX_TUTORIAL_EVENT_PRE_POPUP = 1, RX_TUTORIAL_EVENT_POST_POPUP = 2, RX_TUTORIAL_EVENT_FAILED_POPUP = 3,
    RX_TUTORIAL_EVENT_SHOW_HUD = 4, RX_TUTORIAL_EVENT_OTHER = 9,
    RX_TUTORIAL_EVENTS = 32, RX_TUTORIAL_KEY = 64
};
typedef struct rx_tutorial_state {
    uint32_t size, version;              /* in: sizeof, RX_TUTORIAL_API_VERSION */
    int32_t active;                      /* 0 none, 1 tutorial mode on, 2 ended */
    int32_t spawned;                     /* the local slot's vehicle and rider exist */
    int32_t mode;                        /* MXUIManager +0x298 (UI TutorialMode) */
    int32_t manager_off;                 /* the manager's +0: 1 not running */
    int32_t lesson;                      /* +4: 0..5 (after a pass: the next lesson) */
    int32_t lesson_state;                /* +8: 0 idle, 1 waits, 2 runs */
    int32_t step;                        /* +0x7c */
    int32_t passed, wrecked, boundary;   /* +0x64, +0x70, +0x74 */
    int32_t good, bad;                   /* +0x6c, +0x68 (lesson 4's jumps) */
    int32_t valid;                       /* +0x60 */
    int32_t current_tutorial;            /* UI CurrentTutorial (1..6, -1 never set) */
    int32_t completed;                   /* PlayerRider.TutorialsCompleted (the local controller's table) */
    float step_timer, lean_time, hold_time, wreck_timer;   /* +0x88, +0x8c, +0x90, +0x84 */
    int32_t lean_flags;                  /* lesson 1's four directions: 1 forward (+0xa0), 2 left (+0xa4), 4 back (+0xa8), 8 right (+0xac) */
    int32_t world_paused;                /* the UI world's +0x20014 */
    uint32_t event_serial;               /* UI events raised so far; events[n % 32] = the kind of event n */
    uint8_t events[RX_TUTORIAL_EVENTS];
    char description[RX_TUTORIAL_KEY], activation[RX_TUTORIAL_KEY], hud_label[RX_TUTORIAL_KEY], completion[RX_TUTORIAL_KEY], failed[RX_TUTORIAL_KEY];
    uint32_t voice_serial, stop_serial;  /* voice requests / the manager's voice stops so far */
    char voice[RX_TUTORIAL_KEY * 2];     /* the last voice request's event path ("TUTORIAL_01/VO/INTRO" ...) */
    uint32_t frames;                     /* frames advanced since the start */
} rx_tutorial_state;
RX_API int rx_session_tutorial_start(struct rx_session* s, const rx_event_spec* spec);
RX_API int rx_session_tutorial_pause(struct rx_session* s, int paused);
RX_API int rx_session_tutorial_lesson(struct rx_session* s, int lesson);
RX_API int rx_session_tutorial_disable(struct rx_session* s);
RX_API int rx_session_tutorial_retry(struct rx_session* s);
RX_API int rx_session_tutorial_popup(struct rx_session* s, int up);
RX_API int rx_session_tutorial_suspend_input(struct rx_session* s, int suspended);
RX_API int rx_session_tutorial_end(struct rx_session* s);
RX_API int rx_session_tutorial(struct rx_session* s, rx_tutorial_state* out);
/* The lessons' scene objects as TutorialManager_ShowLessonObjects (00751c40) leaves them: the UI scene's model (4) and scrolling-texture
 * (0x1078) entities named "TUT<n>..." (the chevron walls, the trajectory arrows, the road flares' models) with their draw flag (+0xa2): only
 * the running lesson's set (and lesson 3's step) is shown. Returns the count written (at most max), -1 on an invalid session. */
typedef struct rx_tutorial_object {
    int32_t entity_id;                   /* the scene entity id (+0x14) */
    int32_t shown;                       /* +0xa2 */
    char name[RX_TUTORIAL_KEY];          /* the entity's Name (+0x18) */
} rx_tutorial_object;
RX_API int rx_session_tutorial_objects(struct rx_session* s, rx_tutorial_object* out, uint32_t max);
#ifdef __cplusplus
}
#endif
#endif
