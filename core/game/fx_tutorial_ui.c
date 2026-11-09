/* fx_tutorial_ui.c -- the TutorialManager's UI / localized-audio boundary (fx_tutorial_ui.h). Oracle: the originals. Product: the record. */
#include "fx_tutorial_ui.h"
#include "build_activity_util.h"
#include <string.h>

#define TUT_UI_MANAGER 0x00a93fccu   /* g_MXUIManager */

#ifdef RX_ORACLE
void rx_tutorial_ui_raise(rx_addr name) {
    const rx_addr ui = U32(0, TUT_UI_MANAGER);
    rx_regs in, o;
    memset(&in, 0, sizeof in);
    in.r[6] = ui;                                                      /* 005db500 (ESI = ui; stack name; RET 4) */
    const uint32_t a[1] = {name};
    rx_call_original_ex(0x005db500, &in, &o, 1, a);
    const rx_addr ev = o.r[0];
    if (!ev) return;
    const rx_addr channel = U32(ui, 0x98);
    RX_FRAME_ENTER(F, 8);
    SU32(F, 0, ev);
    uint32_t node = 0x009e1a7c;
    rx_call_original(U32(0, 0x00916388), RX_THISCALL, 0x009cdb68, 1, &node);   /* type_info::_name_internal_method (void (*)(const char*)) */
    memset(&in, 0, sizeof in);
    in.r[0] = channel; in.r[6] = channel; in.r[7] = ev;
    const uint32_t b[2] = {F, 4};
    rx_call_original_ex(0x004020c0, &in, &o, 2, b);
    RX_FRAME_LEAVE(F, 8);
}
void rx_tutorial_ui_set_int(rx_addr name, uint32_t value) {
    const uint32_t a[2] = {name, value};
    (void)rx_call_original(0x005dbfb0, RX_THISCALL, U32(0, TUT_UI_MANAGER), 2, a);
}
void rx_tutorial_ui_dynamic_text(rx_addr dyn, rx_addr key) {
    rx_regs in, o;
    const uint32_t zero[1] = {0};
    const rx_addr lang = rx_call_original(0x005dfc90, RX_CDECL, 0, 1, zero);     /* the current language */
    memset(&in, 0, sizeof in); in.r[0] = key; in.r[7] = lang;
    rx_call_original_ex(0x005e0340, &in, &o, 0, 0);                              /* (EAX = key, EDI = language) -> the entry's text */
    const uint32_t text = o.r[0];
    const rx_addr lang2 = rx_call_original(0x005dfc90, RX_CDECL, 0, 1, zero);
    memset(&in, 0, sizeof in); in.r[0] = dyn; in.r[7] = lang2;
    const uint32_t t[1] = {text};
    rx_call_original_ex(0x005e03c0, &in, &o, 1, t);                              /* (EAX = the DYN_ key, EDI = language; text) */
}
uint32_t rx_tutorial_ui_popup_active(void) { return rx_call_original(0x00743120, RX_CDECL, 0, 0, 0); }
void rx_tutorial_ui_voice(rx_addr database, rx_addr package, rx_addr path, rx_addr owner) {
    const uint32_t a[5] = {database, package, path, 9, owner};
    (void)rx_call_original(0x0068dd40, RX_THISCALL, U32(0, 0x00d94ad4), 5, a);
}
#else
static rx_tutorial_ui_record g_rec = {.current_tutorial = -1, .tutorial_mode = -1};
rx_tutorial_ui_record* rx_tutorial_ui_get(void) { return &g_rec; }
void rx_tutorial_ui_reset(void) { memset(&g_rec, 0, sizeof g_rec); g_rec.current_tutorial = -1; g_rec.tutorial_mode = -1; }
void rx_tutorial_ui_set_popup(int up) { g_rec.popup_up = up ? 1 : 0; }
void rx_tutorial_ui_note_stop(void) { g_rec.stop_serial++; }

static int game_streq(rx_addr s, const char* c) {
    for (uint32_t i = 0;; i++) { const uint8_t x = U8(s, i); if (x != (uint8_t)c[i]) return 0; if (!x) return 1; }
}
static void game_strcpy(char* d, size_t n, rx_addr s) {
    size_t i = 0;
    for (; s && i + 1 < n; i++) { const char c = (char)U8(s, (uint32_t)i); d[i] = c; if (!c) return; }
    d[i] = 0;
}
void rx_tutorial_ui_raise(rx_addr name) {
    uint8_t k = RX_TUT_EV_OTHER;
    if (game_streq(name, "ShowPreTutorialPopup")) k = RX_TUT_EV_SHOW_PRE_POPUP;
    else if (game_streq(name, "ShowPostTutorialPopup")) k = RX_TUT_EV_SHOW_POST_POPUP;
    else if (game_streq(name, "ShowFailedTutorialPopup")) k = RX_TUT_EV_SHOW_FAILED_POPUP;
    else if (game_streq(name, "ShowTutorialHUD")) k = RX_TUT_EV_SHOW_HUD;
    g_rec.events[g_rec.serial % RX_TUT_UI_EVENTS] = k;
    g_rec.serial++;
}
void rx_tutorial_ui_set_int(rx_addr name, uint32_t value) {
    if (game_streq(name, "CurrentTutorial")) g_rec.current_tutorial = (int32_t)value;
    else if (game_streq(name, "TutorialMode")) g_rec.tutorial_mode = (int32_t)value;
}
void rx_tutorial_ui_dynamic_text(rx_addr dyn, rx_addr key) {
    char* d = 0;
    if (game_streq(dyn, "DYN_TUTORIAL_DESCRIPTION")) d = g_rec.description;
    else if (game_streq(dyn, "DYN_TUTORIAL_ACTIVATION")) d = g_rec.activation;
    else if (game_streq(dyn, "DYN_TUTORIAL_HUD_LABEL")) d = g_rec.hud_label;
    else if (game_streq(dyn, "DYN_TUTORIAL_COMPLETION")) d = g_rec.completion;
    else if (game_streq(dyn, "DYN_TUTORIAL_FAILED")) d = g_rec.failed;
    if (d) game_strcpy(d, RX_TUT_UI_KEY, key);
}
uint32_t rx_tutorial_ui_popup_active(void) { return g_rec.popup_up ? 1u : 0u; }
void rx_tutorial_ui_voice(rx_addr database, rx_addr package, rx_addr path, rx_addr owner) {
    (void)database; (void)package; (void)owner;
    game_strcpy(g_rec.voice, sizeof g_rec.voice, path);
    g_rec.voice_serial++;
}
#endif
