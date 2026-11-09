/* loop_input.h -- input sampling (loop_input.c): KeyMap bindings and the XenonInputManager queries. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void     rx_keymap_update(rx_addr km, float dt);                                   /* 00811490 KeyMap::vf5 */
void     rx_keymap_sample(rx_addr km, uint32_t i, float dt);                       /* 008114e0 KeyMap::vf6 */
void     rx_keymap_sample_repeat(rx_addr km, uint32_t i, float dt);                /* 00811520 KeyMap::vf7 */
uint32_t rx_pad_button(rx_addr pad, uint32_t idx);                                 /* 00816530 */
uint32_t rx_keyboard_key_state(rx_addr keys, uint32_t id, uint32_t want);          /* 008172b0 */
uint32_t rx_mouse_button(rx_addr mouse, uint32_t idx);                             /* 00817af0 */
uint32_t rx_input_held(rx_addr im, uint32_t device, uint32_t ctl, uint32_t id, uint32_t state, uint32_t flag); /* 00814a70 vf27 */
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
void     rx_analog_keymap_sample_plain(rx_addr km, uint32_t i, float dt);          /* 008117e0 AnalogKeyMap::vf6 */
void     rx_analog_keymap_sample(rx_addr km, uint32_t i, float dt);                /* 00811850 AnalogKeyMap::vf7 */
uint32_t rx_compound_keymap_pressed(rx_addr km, uint32_t a, uint32_t b);           /* 00811a60 CompoundKeyMap::vf1 */
void     rx_compound_keymap_set(rx_addr km, uint32_t a, uint32_t b);               /* 00811c50 CompoundKeyMap::vf9 */
void     rx_cumulative_keymap_update(rx_addr km, float dt);                        /* 00811dc0 CumulativeKeyMap::vf5 */
uint32_t rx_pad_active(rx_addr pad);                                               /* 00816b60 */
uint32_t rx_mouse_active(rx_addr mouse);                                           /* 00817c60 */
uint32_t rx_input_device_count(rx_addr im, uint32_t device);                       /* 00814750 XenonInputManager::vf2 */
uint32_t rx_input_connected(rx_addr im, uint32_t device, uint32_t ctl);            /* 008147f0 vf3 */
uint32_t rx_input_any_active(rx_addr im);                                          /* 00814830 vf23 */
uint32_t rx_input_active(rx_addr im, uint32_t device, uint32_t ctl);               /* 00814890 vf21 */
uint32_t rx_input_any_button(rx_addr im);                                          /* 00814920 vf26 */
uint32_t rx_input_button_down(rx_addr im, uint32_t device, uint32_t ctl);          /* 008149d0 vf24 */
uint32_t rx_input_mouse_active(rx_addr im);                                        /* 00815090 vf52 */
void     rx_input_stop_vibration(rx_addr im, uint32_t device, uint32_t ctl);       /* 00815290 vf48 */
uint32_t rx_input_set_vibration_enabled(rx_addr im, uint32_t what, uint32_t device, uint32_t value); /* 008151b0 vf53 */
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
void     rx_pad_update(rx_addr pad, float dt);                                     /* 008157a0 Pad_Update */
float    rx_pad_axis(rx_addr rec, uint32_t value, float dt);                       /* 008156e0 (EAX = record, CX = value; stack dt) -> xmm0 */
uint32_t rx_pad_sticks(rx_addr gamepad, rx_addr pad, rx_addr old, float dt);       /* 00815900 (EAX = gamepad, ESI = pad; stack old, dt; RET 8) -> ECX */
void     rx_pad_triggers(rx_addr gamepad, rx_addr pad, rx_addr old);               /* 00816400 (ESI = gamepad, EDI = pad; stack old; RET 4) */
void     rx_pad_buttons_held(rx_addr pad, float dt);                               /* 00815b60 (EAX = pad, xmm2 = dt) */
void     rx_pad_buttons_released(rx_addr pad, float dt);                           /* 00815fb0 (EAX = pad, xmm1 = dt) */
void     rx_pad_update_vibration(rx_addr pad, float dt);                           /* 008169f0 Pad_UpdateVibration (ESI = pad; stack dt; RET 4) */
uint32_t rx_pad_any_input(rx_addr pad);                                            /* 00816960 Pad_AnyInput */
void     rx_keyboard_poll(rx_addr kb);                                             /* 00816ef0 Keyboard_Poll */
void     rx_mouse_update_cursor(rx_addr mouse);                                    /* 00817400 Mouse_UpdateCursor */
void     rx_mouse_update_axes(rx_addr mouse, float dt);                            /* 00817590 Mouse_UpdateAxes */
void     rx_mouse_update_wheel(rx_addr mouse, float dt);                           /* 00817700 Mouse_UpdateWheel */
void     rx_mouse_update_buttons(rx_addr mouse, float dt);                         /* 00817850 Mouse_UpdateButtons */
void     rx_mouse_update(rx_addr mouse, float dt);                                 /* 00817340 Mouse_Update */
void     rx_input_update(rx_addr im, float dt);                                    /* 008150b0 XenonInputManager::vf43 */
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
void     rx_keymaps_update(rx_addr list, float dt);                                /* 00813de0 */
#ifdef __cplusplus
}
#endif
