/* fx_tutorial_ui.h -- the TutorialManager's calls into the front end's UI and localized audio (fx_tutorial_manager.c, _re/notes/tutorials.md).
 *
 * UI BOUNDARY. The original's TutorialManager talks to the UI manager ([0x00a93fcc]) and its Lua pages through:
 *   - UI events raised on the UI manager's channel (005db500 finds the event by name in ui's list, 004020c0 raises it on ui + 0x98):
 *     "ShowPreTutorialPopup", "ShowPostTutorialPopup", "ShowFailedTutorialPopup", "ShowTutorialHUD";
 *   - UI properties (UIProperty_SetInt 005dbfb0): "CurrentTutorial", "TutorialMode";
 *   - dynamic strings: the localized text of a STR_ key (005dfc90 the language, 005e0340 the entry's text) stored under a DYN_ key
 *     (005e03c0): DYN_TUTORIAL_DESCRIPTION / _ACTIVATION / _HUD_LABEL / _COMPLETION / _FAILED;
 *   - "is a popup up" (00743120: the PopupPageStack.pag page graph's vf54): the manager freezes the rider's input while one is;
 *   - the localized voice (UILocSound_Play 0068dd40 on [0x00d94ad4]: database, package, the event path, priority 9, owner).
 * The oracle build runs the originals (so the Verify of a calling function sees the same memory). The product has no UI manager and plays
 * no sound from the core: it RECORDS what the original hands the UI (the event names in order, the property values, the STR_ key each DYN_
 * key was set from, the voice event paths) for the host, which draws the pages in the port's own front end (Unity) and answers "is a popup
 * up" itself (rx_tutorial_ui_set_popup). Nothing the simulation reads comes from these calls except the popup answer, which is the UI's
 * state in the original too. PRODUCT ONLY: the record (fx_tutorial_ui.c); the API (include/rx_tutorial.h, api/tutorial_session.inc). */
#pragma once
#include "rx_game.h"

/* the calls (both builds) */
void     rx_tutorial_ui_raise(rx_addr name);                         /* a UI event by name (005db500 + 004020c0 on ui + 0x98) */
void     rx_tutorial_ui_set_int(rx_addr name, uint32_t value);       /* UIProperty_SetInt (ECX = ui; name, value) */
void     rx_tutorial_ui_dynamic_text(rx_addr dyn, rx_addr key);      /* DYN_<dyn> = the localized text of the key (005dfc90 / 005e0340 / 005e03c0) */
uint32_t rx_tutorial_ui_popup_active(void);                          /* 00743120 */
void     rx_tutorial_ui_voice(rx_addr database, rx_addr package, rx_addr path, rx_addr owner);   /* UILocSound_Play (9 = the priority) */

/* ---- PRODUCT ONLY: the record the host reads (never compiled into the oracle) ---- */
#ifndef RX_ORACLE
enum {
    RX_TUT_UI_EVENTS = 32,   /* the event ring */
    RX_TUT_UI_KEY = 64,
};
enum rx_tutorial_ui_event_kind {
    RX_TUT_EV_NONE = 0,
    RX_TUT_EV_SHOW_PRE_POPUP = 1,      /* "ShowPreTutorialPopup" */
    RX_TUT_EV_SHOW_POST_POPUP = 2,     /* "ShowPostTutorialPopup" */
    RX_TUT_EV_SHOW_FAILED_POPUP = 3,   /* "ShowFailedTutorialPopup" */
    RX_TUT_EV_SHOW_HUD = 4,            /* "ShowTutorialHUD" */
    RX_TUT_EV_OTHER = 9,
};
typedef struct rx_tutorial_ui_record {
    uint32_t serial;                                   /* events raised since the session's start */
    uint8_t events[RX_TUT_UI_EVENTS];                  /* events[serial % N] = the kind of event number serial */
    int32_t current_tutorial, tutorial_mode;           /* the UI properties (-1 never set) */
    char description[RX_TUT_UI_KEY], activation[RX_TUT_UI_KEY], hud_label[RX_TUT_UI_KEY], completion[RX_TUT_UI_KEY], failed[RX_TUT_UI_KEY];
    uint32_t voice_serial;                             /* voice requests since the start */
    char voice[RX_TUT_UI_KEY * 2];                     /* the last request's event path ("TUTORIAL_01/VO/INTRO" ...) */
    uint32_t stop_serial;                              /* StopTutorialAudio / the manager's own stops (LocSoundList_StopOwner "TutorialManager") */
    int32_t popup_up;                                  /* the host's answer to 00743120 */
} rx_tutorial_ui_record;
rx_tutorial_ui_record* rx_tutorial_ui_get(void);
void rx_tutorial_ui_reset(void);
void rx_tutorial_ui_set_popup(int up);
void rx_tutorial_ui_note_stop(void);
#endif
