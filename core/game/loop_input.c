/* loop_input.c -- input sampling: KeyMap (action bindings, vtable 00974b74) and the XenonInputManager queries they make
 * (vtable 00974e0c). The core gets the raw device state from Unity; these are the game's own mappings from raw keys / pads / mouse
 * to the actions the vehicle's input block reads (_re/notes/simulation-input.md, input-settings.md).
 *
 * KeyMap: +0x08 repeat enabled, +0x0c repeat delay (s), +0x10 repeat interval (s); four bindings (one per controller) at
 * +0x14 + 0x24 * i: +0x00 device (1 pad, 2 keyboard, 4 mouse), +0x04 input id, +0x08 secondary id, +0x10 key state asked,
 * +0x14 enabled, +0x18 time of the last repeat, +0x1c state (pressed / held this frame). */
#include "loop_input.h"
#include "loop_util.h"

#define KM_BIND(km, i) ((km) + 0x14u + 0x24u * (uint32_t)(i))

/* 00811490 KeyMap::vf5: per enabled binding, sample it: slot 7 (with key repeat) when +0x08 is set, else slot 6. */
void rx_keymap_update(rx_addr km, float dt) {
    for (uint32_t i = 0; i < 4; i++) {
        if (U32(KM_BIND(km, i), 0x14) == 0) continue;
        loop_tc_uf(loop_vslot(km, U32(km, 8) ? 0x1c : 0x18), km, i, dt);
    }
}
RX_FUNCTION(0x00811490, rx_keymap_update)

/* 008114e0 KeyMap::vf6: state = the input manager's "held" query (slot 27) for the binding. */
void rx_keymap_sample(rx_addr km, uint32_t i, float dt) {
    (void)dt;
    rx_addr im = loop_input_manager(), b = KM_BIND(km, i);
    SU32(b, 0x1c, loop_tc5(loop_vslot(im, 0x6c), im, U32(b, 0), i, U32(b, 4), U32(b, 0x10), 1));
}
RX_FUNCTION(0x008114e0, rx_keymap_sample)

/* 00811520 KeyMap::vf7: as vf6 with key repeat: held -> state 1 and the repeat clock restarts (0); otherwise the input
 * manager's key record (slot 41) of a keyboard key that has been down for at least the repeat delay fires again every repeat
 * interval (record +0x00 == 2: down, +0x10 time held). */
void rx_keymap_sample_repeat(rx_addr km, uint32_t i, float dt) {
    (void)dt;
    rx_addr im = loop_input_manager(), b = KM_BIND(km, i);
    if (loop_tc5(loop_vslot(im, 0x6c), im, U32(b, 0), i, U32(b, 4), U32(b, 0x10), 1) != 0) {
        SF32(b, 0x18, 0.0f);
        SU32(b, 0x1c, 1);
        return;
    }
    SU32(b, 0x1c, 0);
    rx_addr rec = loop_tc3(loop_vslot(im, 0xa4), im, U32(b, 0), i, U32(b, 4));
    if (U32(km, 8) != 0 && U32(rec, 0) == 2) {
        float held = F32(rec, 0x10);
        if (held >= F32(km, 0xc) && held - F32(b, 0x18) >= F32(km, 0x10)) {   /* comiss: jb skips (NaN too) */
            SF32(b, 0x18, held);
            SU32(b, 0x1c, 1);
            return;
        }
    }
    SU32(b, 0x1c, 0);   /* the original stores its zeroed argument slot */
}
RX_FUNCTION(0x00811520, rx_keymap_sample_repeat)

/* 00816530 (LTCG: EAX = pad record, ECX = button id - 1; result EAX): the XInput pad button / trigger state of a game input id
 * (jump table 0081660c). Ids 1..4: the analog values at +0x258 / +0x25c / +0x250 / +0x254, 5..8 none, 9..24: the button word
 * +0x246 masked, 19 / 20: +0x248 / +0x24c. */
uint32_t rx_pad_button(rx_addr pad, uint32_t idx) {
    static const uint16_t mask[24] = {0, 0, 0, 0, 0, 0, 0, 0, 0x1000, 0x2000, 0x4000, 0x8000, 0x100, 0x200, 0x10, 0x20,
                                      0x40, 0x80, 0, 0, 0x1, 0x8, 0x2, 0x4};
    switch (idx) {
    case 0: return U32(pad, 0x258);
    case 1: return U32(pad, 0x25c);
    case 2: return U32(pad, 0x250);
    case 3: return U32(pad, 0x254);
    case 4: case 5: case 6: case 7: return 0;
    case 18: return U32(pad, 0x248);
    case 19: return U32(pad, 0x24c);
    default: return U16(pad, 0x246) & mask[idx];
    }
}
RX_FUNCTION(0x00816530, rx_pad_button)

/* 008172b0 (LTCG: EAX = input id, EDX = state wanted, stack: the keyboard key records (manager + 0xb74, stride 0x18 from id
 * 0x23); ret 4): 1 when the key is down (+0x04 == 2) and state 2 is asked, or when its +0x08 equals the state and +0x00 is 2. */
uint32_t rx_keyboard_key_state(rx_addr keys, uint32_t id, uint32_t want) {
    rx_addr k = keys + 8u * (3u * id - 0x69u);
    if (want == 2 && U32(k, 4) == want) return 1;
    return U32(k, 8) == want && U32(k, 0) == 2;
}
RX_FUNCTION(0x008172b0, rx_keyboard_key_state)

/* 00817af0 (LTCG: EAX = mouse record (manager + 0x3234), ECX = id - 0x19; result AL): the mouse buttons (jump table 00817b40):
 * 0 / 1 the bytes +0x128 / +0x129, 2..6 the bits 0..4 of +0x120. */
uint32_t rx_mouse_button(rx_addr mouse, uint32_t idx) {
    if (idx == 0) return U8(mouse, 0x128);
    if (idx == 1) return U8(mouse, 0x129);
    return (U32(mouse, 0x120) >> (idx - 2)) & 1;
}
RX_FUNCTION(0x00817af0, rx_mouse_button)

/* 00814a70 XenonInputManager::vf27 (device, controller, input id, key state, flag; ret 0x14): is the input held?
 * Pad (1): the pad of that controller connected (+0x2a0 + 0x2dc * c) and a button id 1..24; keyboard (2): controller 0, the
 * key in the state asked (the binding's +0x10); mouse (4): controller 0, ids 0x19..0x1f. */
uint32_t rx_input_held(rx_addr im, uint32_t device, uint32_t ctl, uint32_t id, uint32_t state, uint32_t flag) {
    (void)flag;
    if (device == 1) {
        rx_addr pad = im + ctl * 0x2dcu;
        if (U32(pad, 0x2a0) != 0 && id - 1u < 0x18u && rx_pad_button(pad + 4, id - 1u) != 0) return 1;
        return 0;
    }
    if (device == 2) return ctl == 0 ? rx_keyboard_key_state(im + 0xb74, id, state) : 0;
    if (device == 4 && ctl == 0) return id - 0x19u < 7u ? (rx_mouse_button(im + 0x3234, id - 0x19u) & 0xff) : 0;
    return 0;
}
RX_FUNCTION(0x00814a70, rx_input_held)

/* 008117e0 AnalogKeyMap::vf6 (vtable 00974bdc; thiscall (i, dt); RET 8): the analog input without key repeat (KeyMap::vf5 picks slot 6 when
 * +0x08 is clear): the binding's state is cleared, then set (1) while the input manager's record (slot 41) value +0x0c lies in [+0xa4, +0xa8]
 * (comiss / jb: below, or NaN, stays 0). dt is not read. Read off the disassembly (w-vtunported: no run had an analog key map without repeat). */
void rx_analog_keymap_sample_plain(rx_addr km, uint32_t i, float dt) {
    (void)dt;
    rx_addr im = loop_input_manager(), b = KM_BIND(km, i);
    SU32(b, 0x1c, 0);
    rx_addr rec = loop_tc3(loop_vslot(im, 0xa4), im, U32(b, 0), i, U32(b, 4));
    const float v = F32(rec, 0xc);
    if (v >= F32(km, 0xa4) && F32(km, 0xa8) >= v) SU32(b, 0x1c, 1);
}
RX_FUNCTION(0x008117e0, rx_analog_keymap_sample_plain)

/* 00811850 AnalogKeyMap::vf7 (vtable 00974bdc): an analog input (trigger / stick axis) as a key with repeat. The input manager's
 * record (slot 41) value +0x0c must lie in [+0xa4, +0xa8], else the binding's hold timer (+0xac + 4 i), its last repeat and
 * state are cleared. Inside: the first frame with a positive hold time (+0x10) presses (state 1, timer = dt); later frames repeat
 * after the delay (+0x0c) every interval (+0x10) like KeyMap::vf7, the timer running on while positive. */
void rx_analog_keymap_sample(rx_addr km, uint32_t i, float dt) {
    rx_addr im = loop_input_manager(), b = KM_BIND(km, i), timer = km + 0xac + 4u * i;
    rx_addr rec = loop_tc3(loop_vslot(im, 0xa4), im, U32(b, 0), i, U32(b, 4));
    const float v = F32(rec, 0xc);
    if (!(v >= F32(km, 0xa4)) || !(F32(km, 0xa8) >= v)) {          /* comiss / jb: out of the range (or NaN) */
        SF32(timer, 0, 0.0f);
        SF32(b, 0x18, 0.0f);
        SU32(b, 0x1c, 0);
        return;
    }
    if (F32(rec, 0x10) > 0.0f && F32(timer, 0) == 0.0f) {          /* first pressed frame */
        SU32(b, 0x1c, 1);
        SF32(b, 0x18, 0.0f);
        SF32(timer, 0, dt);
        return;
    }
    const float t = F32(timer, 0);
    if (t >= F32(km, 0xc) && t - F32(b, 0x18) >= F32(km, 0x10)) {  /* repeat */
        SF32(b, 0x18, t);
        SF32(timer, 0, F32(timer, 0) + dt);
        SU32(b, 0x1c, 1);
        return;
    }
    if (t > 0.0f) {
        SF32(timer, 0, t + dt);
        SU32(b, 0x1c, 0);
    }
}
RX_FUNCTION(0x00811850, rx_analog_keymap_sample)

/* 00811a60 CompoundKeyMap::vf1 (vtable 00974c10; ret 8): pressed when one of its (up to four) child key maps (+0xa4..+0xb0)
 * reports pressed through their slot 1. */
uint32_t rx_compound_keymap_pressed(rx_addr km, uint32_t a, uint32_t b) {
    for (uint32_t i = 0; i < 4; i++) {
        rx_addr child = U32(km, 0xa4 + 4 * i);
        if (child != 0 && loop_tc2(loop_vslot(child, 4), child, a, b) != 0) return 1;
    }
    return 0;
}
RX_FUNCTION(0x00811a60, rx_compound_keymap_pressed)

/* 00811c50 CompoundKeyMap::vf9 (ret 8): slot 9 (enable / disable) passed on to every child key map. */
void rx_compound_keymap_set(rx_addr km, uint32_t a, uint32_t b) {
    for (uint32_t i = 0; i < 4; i++) {
        rx_addr child = U32(km, 0xa4 + 4 * i);
        if (child != 0) loop_tc2(loop_vslot(child, 0x24), child, a, b);
    }
}
RX_FUNCTION(0x00811c50, rx_compound_keymap_set)

/* 00811dc0 CumulativeKeyMap::vf5 (vtable 00974c44; ret 4): a key pair accumulated into an axis value +0xa4 in [-1, 1] (the
 * keyboard's rider reflex keys). After the bindings' own update (KeyMap::vf5): not active (slot 10 with controller 0) -> value
 * and idle time +0xa8 zero. Else the key input v (KeyMap::vf4(7, 0), -1..1) is added by g_CumulativeKeyMapMethod (PCControls.bxml):
 * 1 Hold: while the global key map 00d681e8 is held (KeyMap::vf2(7, 0)) value += v, else value = 0; 2 OverTime: once no key was
 * pressed for the delay, the value decays toward 0 at the rate per second; 0 OverDistance: with no key the value decays toward 0 at
 * the rate x +0xac per second. The decay step never overshoots 0. */
void rx_cumulative_keymap_update(rx_addr km, float dt) {
    rx_keymap_update(km, dt);
    if (loop_tc1(loop_vslot(km, 0x28), km, 0) == 0) {
        SF32(km, 0xa8, 0.0f);
        SF32(km, 0xa4, 0.0f);
        return;
    }
    const float v = loop_tc2_x87(0x00811710, km, 7, 0);    /* KeyMap::vf4 */
    const uint32_t method = U32(0, 0x00d68138);               /* g_CumulativeKeyMapMethod */
    float step;
    if (method == 1) {
        if (loop_tc2(0x008115f0, 0x00d681e8, 7, 0) == 0) {   /* KeyMap::vf2 of the global hold key map */
            SF32(km, 0xa4, 0.0f);
            return;
        }
        step = v;
    } else if (method == 2 || method == 0) {
        const int idle = v > -0.01f /* 0x977d14 */ && 0.01f /* 0x9322ac */ > v;
        step = v;
        float decay = 0.0f;
        int decaying = 0;
        if (method == 2) {
            if (idle) SF32(km, 0xa8, dt + F32(km, 0xa8)); else SF32(km, 0xa8, 0.0f);
            if (F32(km, 0xa8) > F32(0, 0x009b73d4)) {        /* g_CumulativeOverTimeDelay */
                decay = F32(km, 0xa4) * F32(0, 0x009b73d8) * dt;   /* g_CumulativeOverTimeRate */
                decaying = 1;
            }
        } else if (idle) {
            decay = F32(km, 0xac) * F32(km, 0xa4) * F32(0, 0x009b73dc) * dt;   /* g_CumulativeOverDistanceRate */
            decaying = 1;
        }
        if (decaying) {
            /* step = -decay limited to [-value, 0] (value >= 0) or [0, -value] (value < 0), in the original's compare order */
            step = rx_subs(-0.0f /* 0x9774a8 */, decay);
            const float value = F32(km, 0xa4), neg = rx_subs(-0.0f, value);
            if (0.0f > value) {
                if (step >= neg) step = neg;
                if (0.0f >= step) step = 0.0f;
            } else {
                if (step >= 0.0f) step = 0.0f;
                if (neg >= step) step = neg;
            }
        }
    } else {
        return;
    }
    float value = F32(km, 0xa4) + step;
    SF32(km, 0xa4, value);
    if (value >= 1.0f /* 0x931f24 */) value = 1.0f;
    else if (F32(0, 0x009cd9e8) >= value) value = F32(0, 0x009cd9e8);   /* k_MinusOne (a NaN stays) */
    SF32(km, 0xa4, value);
}
RX_FUNCTION(0x00811dc0, rx_cumulative_keymap_update)

/* ---- XenonInputManager (vtable 00974e0c): 4 pads at +0x04 + 0x2dc * i (+0x29c connected), 4 keyboards at +0xb74 (stride 0x9b0;
 * +0x990 / +0x994 = manager +0x1504 / +0x1508 of keyboard 0), the mouse at +0x3234 ---- */

/* 00816b60 (ECX = pad record): not at rest: one of the four stick / trigger records (+0x08, +0x20, +0x38, +0x50) is not in
 * state 2 (idle), or the button records +0x1b8 / +0x1d0 are in state 1. */
uint32_t rx_pad_active(rx_addr pad) {
    if (U32(pad, 0x08) != 2 || U32(pad, 0x20) != 2 || U32(pad, 0x38) != 2 || U32(pad, 0x50) != 2) return 1;
    return U32(pad, 0x1b8) == 1 || U32(pad, 0x1d0) == 1;
}
RX_FUNCTION(0x00816b60, rx_pad_active)

/* 00817c60 (EAX = mouse; result AL): any mouse activity this frame (buttons +0x120 / +0x12c, wheel +0x124, moved flags +0x128 /
 * +0x129 / +0x130 / +0x131 / +0x132). */
uint32_t rx_mouse_active(rx_addr mouse) {
    return U32(mouse, 0x120) != 0 || U32(mouse, 0x124) != 0 || U8(mouse, 0x128) != 0 || U8(mouse, 0x129) != 0 ||
           U32(mouse, 0x12c) != 0 || U8(mouse, 0x130) != 0 || U8(mouse, 0x131) != 0 || U8(mouse, 0x132) != 0;
}
RX_FUNCTION(0x00817c60, rx_mouse_active)

/* 00814750 XenonInputManager::vf2 (device; ret 4): how many controllers of the device type (pads 4, keyboard / mouse 1). */
uint32_t rx_input_device_count(rx_addr im, uint32_t device) {
    (void)im;
    if (device == 1) return 4;
    return device == 2 || device == 4;
}
RX_FUNCTION(0x00814750, rx_input_device_count)

/* 008147f0 XenonInputManager::vf3 (device, controller; ret 8): is the device connected (pad +0x2a0, keyboard +0x1504, mouse 1). */
uint32_t rx_input_connected(rx_addr im, uint32_t device, uint32_t ctl) {
    if (device == 1) return U32(im, ctl * 0x2dcu + 0x2a0);
    if (device == 2) return U32(im, 0x1504);
    return device == 4;
}
RX_FUNCTION(0x008147f0, rx_input_connected)

/* 00814830 XenonInputManager::vf23 (no arguments): any input not at rest: a connected pad active (as 00816b60) or the mouse's
 * two axis records (+0x323c / +0x3254) not idle. */
uint32_t rx_input_any_active(rx_addr im) {
    for (uint32_t i = 0; i < 4; i++) {
        rx_addr pad = im + 4 + 0x2dcu * i;
        if (U32(pad, 0x29c) != 0 && rx_pad_active(pad)) return 1;
    }
    return !(U32(im, 0x323c) == 2 && U32(im, 0x3254) == 2);
}
RX_FUNCTION(0x00814830, rx_input_any_active)

/* 00814890 XenonInputManager::vf21 (device, controller; ret 8): is that controller active (pad: connected and 00816b60; mouse:
 * an axis record not idle). */
uint32_t rx_input_active(rx_addr im, uint32_t device, uint32_t ctl) {
    if (device == 1) {
        rx_addr pad = im + ctl * 0x2dcu;
        return U32(pad, 0x2a0) != 0 && rx_pad_active(pad + 4) != 0;
    }
    if (device == 4) return U32(im, 0x323c) != 2 || U32(im, 0x3254) != 2;
    return 0;
}
RX_FUNCTION(0x00814890, rx_input_active)

/* 00814920 XenonInputManager::vf26 (no arguments): a connected pad with a button down (word +0x248), else the mouse's +0x333c. */
uint32_t rx_input_any_button(rx_addr im) {
    for (uint32_t i = 0; i < 4; i++) {
        rx_addr p = im + 0x248 + 0x2dcu * i;
        if (U32(p, 0x58) != 0 && U16(p, 0) != 0) return 1;
    }
    return U32(im, 0x333c) != 0;
}
RX_FUNCTION(0x00814920, rx_input_any_button)

/* 008149d0 XenonInputManager::vf24 (device, controller; ret 8): a button down on that controller (pad word +0x248, keyboard
 * +0x1508, mouse +0x333c). */
uint32_t rx_input_button_down(rx_addr im, uint32_t device, uint32_t ctl) {
    if (device == 1) {
        rx_addr pad = im + ctl * 0x2dcu;
        return U32(pad, 0x2a0) != 0 && U16(pad, 0x248) != 0;
    }
    if (device == 2) return U32(im, 0x1508);
    return device == 4 && U32(im, 0x333c) != 0;
}
RX_FUNCTION(0x008149d0, rx_input_button_down)

/* 00815090 XenonInputManager::vf52 (no arguments): any mouse activity (00817c60). */
uint32_t rx_input_mouse_active(rx_addr im) { return rx_mouse_active(im + 0x3234) != 0; }
RX_FUNCTION(0x00815090, rx_input_mouse_active)

/* 00815290 XenonInputManager::vf48 (device, controller; ret 8): stop a connected pad's vibration: motor speeds and timers
 * (+0x2cc..+0x2d8) zero, dirty (+0x2c8 = 1) so Pad_UpdateVibration sends it. */
void rx_input_stop_vibration(rx_addr im, uint32_t device, uint32_t ctl) {
    if (device != 1) return;
    rx_addr pad = im + ctl * 0x2dcu;
    if (U32(pad, 0x2a0) == 0) return;
    SF32(pad, 0x2cc, 0.0f);
    SF32(pad, 0x2d0, 0.0f);
    SF32(pad, 0x2d4, 0.0f);
    SF32(pad, 0x2d8, 0.0f);
    SU32(pad, 0x2c8, 1);
}
RX_FUNCTION(0x00815290, rx_input_stop_vibration)

/* ---- device updates (XenonInputManager::vf43, once per frame) ---- */
#include "loop_platform.h"

static void zero_bytes(rx_addr a, uint32_t n) { for (uint32_t i = 0; i < n; i += 4) SU32(a, i, 0); }

/* Button record (stride 0x18; keyboard keys, mouse buttons and wheel): +0x00 state (2 down, 1 up), +0x04 pressed this frame,
 * +0x08 modifiers, +0x0c value, +0x10 time down, +0x14 time up. */
static void button_down(rx_addr r, float dt) {
    if (U32(r, 0) == 2) { SF32(r, 0x10, F32(r, 0x10) + dt); return; }
    SU32(r, 0, 2); SU32(r, 4, 1);
    SF32(r, 0xc, 1.0f /* 0x931f24 */); SF32(r, 0x10, 0.0f); SF32(r, 0x14, 0.0f);
}
static void button_up(rx_addr r, float dt, int released) {
    if (U32(r, 0) == 1) { SF32(r, 0x14, F32(r, 0x14) + dt); return; }
    if (!released) return;
    SU32(r, 0, 1); SU32(r, 4, 0);
    SF32(r, 0xc, 0.0f); SF32(r, 0x10, 0.0f); SF32(r, 0x14, 0.0f);
}

/* 008157a0 Pad_Update (EAX = pad record = manager + 4 + 0x2dc * i, stack dt; ret 4): one XInput pad (+0x00 user index). Off
 * (+0x2d8 == 0): the state block +0x04..+0x244 is cleared. Else XInputGetCapabilities (into +0x27c) and, when connected,
 * XInputGetState decide +0x29c connected, +0x2a0 connected / +0x2a4 disconnected this frame; with a state: the sticks' dead zone
 * and analog records (00815900), the triggers (00816400), the 12-byte gamepad block into +0x290, the button word +0x244 with its
 * pressed (+0x246) / released (+0x260) edges, the stick (00815b60) and button (00815fb0) records and the vibration (008169f0). */
static void pad_connection(rx_addr pad, uint32_t was, uint32_t ok) {
    SU32(pad, 0x29c, ok);
    SU32(pad, 0x2a4, was != 0 && !ok);
    SU32(pad, 0x2a0, was == 0 && ok);
}

/* x87 `fld m32; fstp m32` of a float argument: a NaN keeps its payload, an SNaN comes out quiet (loop_cam.h) */
static uint32_t pad_x87(uint32_t b) {
    return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) ? (b | 0x00400000u) : b;
}

/* 008156e0 (EAX = analog record, CX = the axis value (signed 16 bit); stack dt; plain ret; result xmm0): the analog record of one
 * stick axis (+0x04 state: 3 pushed positive / 4 negative / 2 centred; +0x10 time in that state, +0x14 time centred) past the
 * dead zone 0x1eb8: the axis value rescaled to (-1, 1] (* 0x977328 = 1 / 24903) beyond the dead zone, 0 inside it. */
float rx_pad_axis(rx_addr rec, uint32_t value, float dt) {
    const int16_t v = (int16_t)value;
    if (v > 0x1eb8) {
        float t = 0.0f;
        if (U32(rec, 4) == 3) t = F32(rec, 0x10) + dt;
        else { SU32(rec, 4, 3); SF32(rec, 0x14, 0.0f); }
        SF32(rec, 0x10, t);
        return (float)((int32_t)v - 0x1eb8) * 4.0155803e-05f /* 0x977328 */;
    }
    if (v < -0x1eb8) {
        float t = 0.0f;
        if (U32(rec, 4) == 4) t = F32(rec, 0x10) + dt;
        else { SU32(rec, 4, 4); SF32(rec, 0x14, 0.0f); }
        SF32(rec, 0x10, t);
        return ((float)((int32_t)v + 0x1eb8) + 1.0f /* 0x931f24 */) * 4.0155803e-05f /* 0x977328 */;
    }
    const float up = F32(rec, 0x14) + dt;
    SU32(rec, 4, 2);
    SF32(rec, 0x10, 0.0f);
    SF32(rec, 0x14, up);
    return 0.0f;
}
RX_FUNCTION(0x008156e0, rx_pad_axis)

/* a stick axis value with the dead zone [-0x1eb7, 0x1eb7] (the 16-bit sum v + 0x1eb7 unsigned at most 0x3d6e) zeroed, as the
 * zero-extended 16-bit word */
static uint32_t pad_dead_zone(uint16_t v) { return (uint16_t)(v + 0x1eb7u) > 0x3d6eu ? v : 0u; }

/* 00815900 (EAX = gamepad (the new XINPUT_GAMEPAD), ESI = pad; stack old gamepad (pad + 0x290), dt; RET 8): the four stick axes'
 * analog records (008156e0: RX +0x34, RY +0x4c, LX +0x04, LY +0x1c) and values (+0x2a8 / +0x2ac / +0x2b0 / +0x2b4, copied to
 * the records' +0xc), then per axis the dead-zoned old / new values give the "centred this frame" (+0x26c..+0x278) and "left
 * the centre this frame" (+0x250..+0x25c) flags. Frame: sub esp, 0xc; push ebx, ebp, edi; push ecx (the dt argument of the axis
 * calls); the dead-zoned old values are kept in the argument slots and the frame (entry + 8, + 4, - 0xc, - 8), new RY at - 4.
 * Returns ECX = the dead-zoned new RX (as the original leaves it). */
uint32_t rx_pad_sticks(rx_addr gamepad, rx_addr pad, rx_addr old, float dt) {
    const rx_addr E = rx_sp();
    const uint32_t dtb = pad_x87(rx_fb(dt));
    const float t = rx_bf(pad_x87(dtb));
    const rx_addr S = E - 0x1c;
    rx_set_sp(S);
    SU32(S, 0, pad_x87(dtb));
    float r;
    r = rx_pad_axis(pad + 0x34, U16(gamepad, 8), t);   SF32(pad, 0x2a8, r);
    r = rx_pad_axis(pad + 0x4c, U16(gamepad, 0xa), t); SF32(pad, 0x2ac, r);
    r = rx_pad_axis(pad + 0x04, U16(gamepad, 4), t);   SF32(pad, 0x2b0, r);
    r = rx_pad_axis(pad + 0x1c, U16(gamepad, 6), t);   SF32(pad, 0x2b4, r);
    rx_set_sp(E - 0x18);
    const uint32_t o_rx = pad_dead_zone(U16(old, 8)), o_ry = pad_dead_zone(U16(old, 0xa));
    const uint32_t o_lx = pad_dead_zone(U16(old, 4)), o_ly = pad_dead_zone(U16(old, 6));
    SU32(E, 8, o_rx); SU32(E, 4, o_ry); SU32(E, -0xc, o_lx); SU32(E, -8, o_ly);
    const uint32_t n_rx = pad_dead_zone(U16(gamepad, 8)), n_ry = pad_dead_zone(U16(gamepad, 0xa));
    const uint32_t n_lx = pad_dead_zone(U16(gamepad, 4)), n_ly = pad_dead_zone(U16(gamepad, 6));
    SU32(E, -4, n_ry);
    SU32(pad, 0x40, pad_x87(U32(pad, 0x2a8)));
    SF32(pad, 0x28, r);                                 /* movss of the last axis result (LY) */
    SU32(pad, 0x58, pad_x87(U32(pad, 0x2ac)));
    SU32(pad, 0x10, pad_x87(U32(pad, 0x2b0)));
    SU32(pad, 0x26c, n_rx == 0 && o_rx != 0);
    SU32(pad, 0x270, n_ry == 0 && o_ry != 0);
    SU32(pad, 0x274, n_lx == 0 && o_lx != 0);
    SU32(pad, 0x278, n_ly == 0 && o_ly != 0);
    SU32(pad, 0x250, n_rx != 0 && o_rx == 0);
    SU32(pad, 0x254, n_ry != 0 && o_ry == 0);
    SU32(pad, 0x258, n_lx != 0 && o_lx == 0);
    SU32(pad, 0x25c, n_ly != 0 && o_ly == 0);
    rx_set_sp(E);
    return n_rx;
}
RX_FUNCTION(0x00815900, rx_pad_sticks)

/* 00816400 (ESI = gamepad, EDI = pad; stack old gamepad; RET 4): the triggers: above 30 (LT byte +2, RT +3) a trigger is "held"
 * (+0x1b8 / +0x1d0) with the value byte / 255 (+0x1c0 / +0x1d8), else not (its "pressed" flag +0x248 / +0x24c cleared); then from
 * the old / new values with the threshold 30 (values below 30 count as 0): pressed (+0x248 / +0x24c) and released (+0x264 /
 * +0x268) this frame. Frame: push ecx, ebx, ebp: the old RT (thresholded) is kept in the argument slot, the new raw RT in the
 * push-ecx slot. */
void rx_pad_triggers(rx_addr gamepad, rx_addr pad, rx_addr old) {
    const rx_addr E = rx_sp();
    const float k = 0.003921569f /* 0x9775a0 */;
    if (U8(gamepad, 2) > 0x1e) {
        SU32(pad, 0x1b8, 1);
        SF32(pad, 0x1c0, (float)U8(gamepad, 2) * k);
    } else {
        SU32(pad, 0x1b8, 0);
        SF32(pad, 0x1c0, 0.0f);
        SU32(pad, 0x248, 0);
    }
    float rt = 0.0f;
    if (U8(gamepad, 3) > 0x1e) {
        SU32(pad, 0x1d0, 1);
        rt = (float)U8(gamepad, 3) * k;
    } else {
        SU32(pad, 0x1d0, 0);
        SU32(pad, 0x24c, 0);
    }
    SF32(pad, 0x1d8, rt);
    const uint8_t o_lt = U8(old, 2) >= 0x1e ? U8(old, 2) : 0, n_lt = U8(gamepad, 2) >= 0x1e ? U8(gamepad, 2) : 0;
    const uint8_t o_rt = U8(old, 3) >= 0x1e ? U8(old, 3) : 0;
    SU8(E, 4, o_rt);
    SU8(E, -4, U8(gamepad, 3));
    const uint8_t n_rt = U8(gamepad, 3) >= 0x1e ? U8(gamepad, 3) : 0;
    SU32(pad, 0x248, o_lt == 0 && n_lt != 0);
    SU32(pad, 0x24c, o_rt == 0 && n_rt != 0);
    SU32(pad, 0x264, o_lt != 0 && n_lt == 0);
    SU32(pad, 0x268, o_rt != 0 && n_rt == 0);
}
RX_FUNCTION(0x00816400, rx_pad_triggers)

/* The pad's digital button records (0x18 bytes: +0 state 2 down / 1 up, +4 pressed this frame, +0xc value, +0x10 time down,
 * +0x14 time up) by the XINPUT button bit: DPad up / down / left / right, Start, Back, LThumb, RThumb, LB, RB, A, B, X, Y. */
static const struct { uint16_t mask, rec; } pad_buttons[14] = {
    {0x1000, 0xc4}, {0x2000, 0xdc}, {0x4000, 0xf4}, {0x8000, 0x10c}, {0x100, 0x124}, {0x200, 0x13c}, {0x40, 0x184},
    {0x80, 0x19c}, {0x01, 0x1e4}, {0x08, 0x1fc}, {0x02, 0x214}, {0x04, 0x22c}, {0x10, 0x154}, {0x20, 0x16c}};

/* 00815b60 (EAX = pad, xmm2 = dt): the buttons held (+0x244): a held record's down time grows, a new one goes down (state 2,
 * pressed, value 1, times 0). */
void rx_pad_buttons_held(rx_addr pad, float dt) {
    const uint16_t held = U16(pad, 0x244);
    for (int i = 0; i < 14; i++) {
        if (!(held & pad_buttons[i].mask)) continue;
        const rx_addr r = pad + pad_buttons[i].rec;
        if (U32(r, 0) == 2) { SF32(r, 0x10, F32(r, 0x10) + dt); continue; }
        SU32(r, 0, 2); SU32(r, 4, 1);
        SF32(r, 0xc, 1.0f /* 0x931f24 */); SF32(r, 0x10, 0.0f); SF32(r, 0x14, 0.0f);
    }
}
RX_FUNCTION(0x00815b60, rx_pad_buttons_held)

/* 00815fb0 (EAX = pad, xmm1 = dt): the buttons released this frame (+0x260): an up record's up time grows, a down one goes up
 * (state 1, value 0, times 0). */
void rx_pad_buttons_released(rx_addr pad, float dt) {
    const uint16_t rel = U16(pad, 0x260);
    for (int i = 0; i < 14; i++) {
        if (!(rel & pad_buttons[i].mask)) continue;
        const rx_addr r = pad + pad_buttons[i].rec;
        if (U32(r, 0) == 1) { SF32(r, 0x14, F32(r, 0x14) + dt); continue; }
        SU32(r, 0, 1); SU32(r, 4, 0);
        SF32(r, 0xc, 0.0f); SF32(r, 0x10, 0.0f); SF32(r, 0x14, 0.0f);
    }
}
RX_FUNCTION(0x00815fb0, rx_pad_buttons_released)

/* 008169f0 Pad_UpdateVibration (ESI = pad; stack dt; RET 4): with a vibration-capable pad (XInputGetCapabilities: the
 * capabilities' motor words) and vibration enabled (+0x2b8): a pending change (+0x2c4) sends the motor speeds +0x2cc / +0x2d4
 * (* 65535, truncated; XInputSetState: +0x2c0 = motors running, +0x2c4 cleared on success); then the two motor timers +0x2c8 /
 * +0x2d0 run down and stop their motor (speed 0, change pending) at zero. Without one, running motors are stopped (the zero
 * XINPUT_VIBRATION in the dt argument slot). Frame: sub esp, 0x18: the vibration at +0, the capabilities at +4. */
void rx_pad_update_vibration(rx_addr pad, float dt) {
    const rx_addr E = rx_sp(), S = E - 0x18;
    rx_set_sp(S);
    const uint32_t res = loop_xinput_get_capabilities(U32(pad, 0), 1, S + 4);
    if (res != 0 || (U16(S, 0x14) == 0 && U16(S, 0x16) == 0) || U32(pad, 0x2b8) == 0) {
        if (U32(pad, 0x2c0) != 0) {
            SU16(E, 4, 0);
            SU16(E, 6, 0);
            loop_xinput_set_state(U32(pad, 0), E + 4);
            SU32(pad, 0x2c0, 0);
        }
        rx_set_sp(E);
        return;
    }
    const float k = 65535.0f /* 0x97759c */;
    if (U32(pad, 0x2c4) != 0) {
        SU16(S, 0, (uint16_t)rx_cvtt(F32(pad, 0x2cc) * k));
        SU16(S, 2, (uint16_t)rx_cvtt(F32(pad, 0x2d4) * k));
        const uint32_t ok = loop_xinput_set_state(U32(pad, 0), S);
        SU32(pad, 0x2c0, !(U16(S, 0) == 0 && U16(S, 2) == 0));
        if (ok == 0) SU32(pad, 0x2c4, 0);
    }
    float t = F32(pad, 0x2c8);
    if (t > 0.0f) {
        t = t - dt;
        SF32(pad, 0x2c8, t);
        if (!(0.0f < t || t != t)) { SF32(pad, 0x2c8, 0.0f); SF32(pad, 0x2cc, 0.0f); SU32(pad, 0x2c4, 1); }
    }
    t = F32(pad, 0x2d0);
    if (t > 0.0f) {
        t = t - dt;
        SF32(pad, 0x2d0, t);
        if (!(0.0f < t || t != t)) { SF32(pad, 0x2d0, 0.0f); SF32(pad, 0x2d4, 0.0f); SU32(pad, 0x2c4, 1); }
    }
    rx_set_sp(E);
}
RX_FUNCTION(0x008169f0, rx_pad_update_vibration)

void rx_pad_update(rx_addr pad, float dt) {
    if (U32(pad, 0x2d8) == 0) { zero_bytes(pad + 4, 0x240); return; }
    const uint32_t was = U32(pad, 0x29c);
    uint32_t ok = loop_xinput_get_capabilities(U32(pad, 0), 1, pad + 0x27c) == 0;
    pad_connection(pad, was, ok);
    if (!ok) return;
    RX_FRAME_ENTER(F, 0x20);                       /* sub esp, 0x14; 3 pushes: XINPUT_STATE at +0x10: packet number, gamepad */
    const rx_addr state = F + 0x10, gamepad = state + 4;
    ok = loop_xinput_get_state(U32(pad, 0), state) == 0;
    pad_connection(pad, was, ok);
    if (ok) {
        const uint32_t dtb = pad_x87(rx_fb(dt));
        {
            const uint32_t a[2] = {pad + 0x290, dtb};
            const rx_addr s0 = loop_push(2, a, 0x00815885);
            rx_pad_sticks(gamepad, pad, pad + 0x290, rx_bf(dtb));
            loop_pop(s0);
        }
        {
            const uint32_t a[1] = {pad + 0x290};
            const rx_addr s0 = loop_push(1, a, 0x0081588f);
            rx_pad_triggers(gamepad, pad, pad + 0x290);
            loop_pop(s0);
        }
        const uint16_t old = U16(pad, 0x244);
        SU32(pad, 0x290, U32(gamepad, 0)); SU32(pad, 0x294, U32(gamepad, 4)); SU32(pad, 0x298, U32(gamepad, 8));
        const uint16_t now = U16(pad, 0x290), changed = (uint16_t)(old ^ now);
        SU16(pad, 0x260, changed & old);
        SU16(pad, 0x246, changed & now);
        SU16(pad, 0x244, now);
        {
            rx_addr s0 = loop_push(0, 0, 0x008158d9);
            rx_pad_buttons_held(pad, dt);
            loop_pop(s0);
            s0 = loop_push(0, 0, 0x008158e1);
            rx_pad_buttons_released(pad, dt);
            loop_pop(s0);
        }
        {
            const uint32_t a[1] = {dtb};
            const rx_addr s0 = loop_push(1, a, 0x008158f0);
            rx_pad_update_vibration(pad, rx_bf(dtb));
            loop_pop(s0);
        }
    }
    RX_FRAME_LEAVE(F, 0x20);
}
RX_FUNCTION(0x008157a0, rx_pad_update)

/* 00816960 Pad_AnyInput (EAX = pad record): any button / stick / trigger edge this frame (+0x246 pressed word, the analog
 * edge flags +0x248..+0x25c, +0x260 released word, +0x264..+0x278). */
uint32_t rx_pad_any_input(rx_addr pad) {
    if (U16(pad, 0x246) != 0) return 1;
    for (uint32_t o = 0x248; o <= 0x25c; o += 4) if (U32(pad, o) != 0) return 1;
    if (U16(pad, 0x260) != 0) return 1;
    for (uint32_t o = 0x264; o <= 0x278; o += 4) if (U32(pad, o) != 0) return 1;
    return 0;
}
RX_FUNCTION(0x00816960, rx_pad_any_input)

/* 00816ef0 Keyboard_Poll (ESI = keyboard = manager + 0xb74 + 0x9b0 * i): enabled (+0x99c): the pressed-this-frame flags of the
 * 0x66 key records cleared, +0x994 (a key down) = 0; GetDeviceState(256) on the DirectInput device +0x9ac (Acquire and retry on
 * failure) -> +0x990 ok. Each scan code (255 down to 0) maps through the table 00a95848 to a game key id 0x23..0x88 (record
 * (id - 0x23) * 0x18): down (0x80) and not held -> state 2, held, value 1, times 0, modifiers = the Shift / Ctrl / Alt keys
 * (4 / 8 left / right shift, 0x10 / 0x20 ctrl, 0x40 / 0x80 alt) | +0x9a8; any down key sets +0x994. Up and held -> state 1. */
void rx_keyboard_poll(rx_addr kb) {
    if (U32(kb, 0x99c) == 0) return;
    SU32(kb, 0x994, 0);
    for (uint32_t k = 0; k < 0x66; k++) SU32(kb, 0x18 * k, 0);
    RX_FRAME_ENTER(F, 0x100);
    int32_t hr = loop_dinput_get_device_state(U32(kb, 0x9ac), 0x100, F);
    if (hr < 0) {
        loop_dinput_acquire(U32(kb, 0x9ac));
        hr = loop_dinput_get_device_state(U32(kb, 0x9ac), 0x100, F);
    }
    SU32(kb, 0x990, hr >= 0);
    if (hr >= 0) {
        for (int32_t s = 255; s >= 0; s--) {
            const uint32_t id = U8(0, 0x00a95848 + (uint32_t)s) - 0x23u;
            if (id > 0x65) continue;
            const rx_addr rec = kb + 0x18 * id;
            if (U8(F, s) & 0x80) {
                if (U32(rec, 4) == 0) {
                    uint32_t mods = 0;
                    if (U8(F, 0x2a) & 0x80) mods = 4;          /* DIK_LSHIFT */
                    if (U8(F, 0x36) & 0x80) mods |= 8;         /* DIK_RSHIFT */
                    if (U8(F, 0x1d) & 0x80) mods |= 0x10;      /* DIK_LCONTROL */
                    if (U8(F, 0x9d) & 0x80) mods |= 0x20;      /* DIK_RCONTROL */
                    if (U8(F, 0x38) & 0x80) mods |= 0x40;      /* DIK_LMENU */
                    if (U8(F, 0xb8) & 0x80) mods |= 0x80;      /* DIK_RMENU */
                    SU32(rec, 8, mods);
                    SU32(rec, 0, 2);
                    SU32(rec, 4, 1);
                    SF32(rec, 0xc, 1.0f /* 0x931f24 */);
                    SF32(rec, 0x10, 0.0f);
                    SF32(rec, 0x14, 0.0f);
                    SU32(rec, 8, U32(kb, 0x9a8) | mods);
                }
                SU32(kb, 0x994, 1);
            } else if (U32(rec, 4) == 1) {
                SU32(rec, 0, 1);
                SU32(rec, 4, 0);
                SF32(rec, 0xc, 0.0f);
                SF32(rec, 0x10, 0.0f);
                SF32(rec, 0x14, 0.0f);
            }
        }
    }
    RX_FRAME_LEAVE(F, 0x100);
}
RX_FUNCTION(0x00816ef0, rx_keyboard_poll)

/* The display (00a95960, slot 22 = 0x58): its viewport block (+0x08 / +0x0c width / height in pixels, +0x70..+0x7c the 16:9
 * view rectangle x, y, w, h as floats). */
static rx_addr display_viewport(void) { return loop_tc0(loop_vslot(U32(0, 0x00a95960), 0x58), U32(0, 0x00a95960)); }

/* 00817400 Mouse_UpdateCursor (EAX = mouse): the cursor in client coordinates (GetCursorPos - ClientToScreen(hwnd 00a95984,
 * (0, 0))) -> +0x13c / +0x140, clamped to the display's view rectangle and scaled to the 1280 x 720 UI space. */
void rx_mouse_update_cursor(rx_addr mouse) {
    RX_FRAME_ENTER(F, 0x3c);
    const rx_addr cursor = F + 0x20, rect = F + 0x28, origin = F + 0x18;
    loop_get_cursor_pos(cursor);
    loop_get_client_rect(U32(0, 0x00a95984), rect);
    SU32(origin, 4, 0);
    SU32(origin, 0, 0);
    loop_client_to_screen(U32(0, 0x00a95984), origin);
    const rx_addr px = mouse + 0x13c, py = mouse + 0x140;
    SU32(px, 0, U32(cursor, 0) - U32(origin, 0));
    SU32(py, 0, U32(cursor, 4) - U32(origin, 4));
    RX_FRAME_LEAVE(F, 0x3c);
    if (U32(0, 0x00a95960) == 0) return;
    const int32_t x0 = rx_cvtt(F32(display_viewport(), 0x70));
    const int32_t w = rx_cvtt(F32(display_viewport(), 0x78));
    const int32_t x1 = rx_cvtt(F32(display_viewport(), 0x70)) + w;
    const int32_t y0 = rx_cvtt(F32(display_viewport(), 0x74));
    const int32_t h = rx_cvtt(F32(display_viewport(), 0x7c));
    const int32_t y1 = rx_cvtt(F32(display_viewport(), 0x74)) + h;
    if (I32(px, 0) < x0) SU32(px, 0, x0); else if (I32(px, 0) > x1) SU32(px, 0, x1);
    if (I32(py, 0) < y0) SU32(py, 0, y0); else if (I32(py, 0) > y1) SU32(py, 0, y1);
    SU32(px, 0, I32(px, 0) - rx_cvtt(F32(display_viewport(), 0x70)));
    SU32(py, 0, I32(py, 0) - rx_cvtt(F32(display_viewport(), 0x74)));
    SU32(px, 0, rx_cvtt((float)I32(px, 0) / (float)I32(display_viewport(), 8) * 1280.0f /* 0x977598 */));
    SU32(py, 0, rx_cvtt((float)I32(py, 0) / (float)I32(display_viewport(), 0xc) * 720.0f /* 0x977594 */));
}
RX_FUNCTION(0x00817400, rx_mouse_update_cursor)

/* 00817590 Mouse_UpdateAxes (EAX = mouse, xmm3 = dt): dt > 0: the frame's motion (+0xfc dx, +0x100 dy) in screens (1 / 1280,
 * 1 / 720) -> +0x134 / +0x138, then as a rate per 1/30 s times the sensitivity 009cb7b0 (y negated by k_MinusOne), clamped to
 * [-1, 1]. Always: the axis records' values (+0x10 RiderReflexMouseX, +0x28 RiderReflexMouseY) and the started (+0x128 / +0x129) /
 * stopped (+0x130 / +0x131) / moving (+0x132) flags against the last frame's motion (+0x10c / +0x110). */
void rx_mouse_update_axes(rx_addr mouse, float dt) {
    if (dt > 0.0f /* 0x92e12c */) {
        const float k = 0.033333335f /* 0x977470 */ / dt;
        const float x = (float)I32(mouse, 0xfc) * 0.00078125001f /* 0x977590 */;
        const float y = (float)I32(mouse, 0x100) * 0.0013888889f /* 0x97758c */;
        SF32(mouse, 0x134, x);
        SF32(mouse, 0x138, y);
        const float sens = F32(0, 0x009cb7b0), minus1 = F32(0, 0x009cd9e8);
        float ax = k * x * sens;
        SF32(mouse, 0x134, ax);
        float ay = k * y * sens * minus1;
        SF32(mouse, 0x138, ay);
        if (ax >= 1.0f /* 0x931f24 */) ax = 1.0f; else if (minus1 >= ax) ax = minus1;
        SF32(mouse, 0x134, ax);
        ay = F32(mouse, 0x138);
        if (ay >= 1.0f) ay = 1.0f; else if (minus1 >= ay) ay = minus1;
        SF32(mouse, 0x138, ay);
    }
    SU32(mouse, 0x10, U32(mouse, 0x134));     /* fld / fstp: a float copy */
    SU32(mouse, 0x28, U32(mouse, 0x138));
    const uint32_t dx = U32(mouse, 0xfc), pdx = U32(mouse, 0x10c), dy = U32(mouse, 0x100), pdy = U32(mouse, 0x110);
    SU8(mouse, 0x130, dx == 0 && pdx != 0);
    SU8(mouse, 0x131, dy == 0 && pdy != 0);
    SU8(mouse, 0x128, dx != 0 && pdx == 0);
    SU8(mouse, 0x129, dy != 0 && pdy == 0);
    SU8(mouse, 0x132, dx != 0 || dy != 0);
}
RX_FUNCTION(0x00817590, rx_mouse_update_axes)

/* 00817700 Mouse_UpdateWheel (EAX = mouse, xmm2 = dt): the frame's wheel (+0x104, short) as the button records wheel up +0xc4
 * and wheel down +0xdc. */
void rx_mouse_update_wheel(rx_addr mouse, float dt) {
    const int16_t w = (int16_t)U16(mouse, 0x104);
    if (w > 0) button_down(mouse + 0xc4, dt);
    else if (w < 0) button_down(mouse + 0xdc, dt);
    button_up(mouse + 0xc4, dt, w <= 0);
    button_up(mouse + 0xdc, dt, w >= 0);
}
RX_FUNCTION(0x00817700, rx_mouse_update_wheel)

/* 00817850 Mouse_UpdateButtons (EAX = mouse, xmm2 = dt): five buttons (+0x120 down bits, +0x12c released bits) as the button
 * records +0x34 + 0x18 * i. */
void rx_mouse_update_buttons(rx_addr mouse, float dt) {
    for (uint32_t i = 0; i < 5; i++)
        if ((U32(mouse, 0x120) >> i) & 1) button_down(mouse + 0x34 + 0x18 * i, dt);
    for (uint32_t i = 0; i < 5; i++)
        button_up(mouse + 0x34 + 0x18 * i, dt, (U32(mouse, 0x12c) >> i) & 1);
}
RX_FUNCTION(0x00817850, rx_mouse_update_buttons)

/* 008151b0 XenonInputManager::vf53 (ECX = manager; stack what, device, value; RET 0xc): what == 1 stores the pad record's
 * vibration-enable word (`manager + 0x2c0 + 0x2dc * device` = the record's +0x2bc) and returns 1; any other `what` returns 0.
 * UISettings_Apply calls it with (1, device, Vibration != 0). */
uint32_t rx_input_set_vibration_enabled(rx_addr im, uint32_t what, uint32_t device, uint32_t value) {
    if (what != 1) return 0;
    SU32(im, 0x2c0 + 0x2dcu * device, value);
    return 1;
}
RX_FUNCTION(0x008151b0, rx_input_set_vibration_enabled)

/* 00817340 Mouse_Update (EAX = mouse = manager + 0x3234, stack dt; ret 4). Off (+0xf4 byte 0): the records +0x04..+0xf4 are
 * cleared. Else the Raw Input sums of the frame (Mouse_AccumulateRawInput: +0x118 down bits, +0x11c up bits, +0x104 wheel,
 * +0xfc / +0x100 motion) become the frame's state (+0x120, +0x12c, +0x124), cursor, axes, wheel and buttons update, the motion
 * is kept as the last frame's (+0x108..+0x114) and the sums restart. */
void rx_mouse_update(rx_addr mouse, float dt) {
    if (U8(mouse, 0xf4) == 0) { zero_bytes(mouse + 4, 0xf0); return; }
    SU32(mouse, 0x12c, U32(mouse, 0x11c));
    SU32(mouse, 0x120, U32(mouse, 0x118));
    SU32(mouse, 0x124, (uint32_t)(int32_t)(int16_t)U16(mouse, 0x104));
    rx_mouse_update_cursor(mouse);
    rx_mouse_update_axes(mouse, dt);
    rx_mouse_update_wheel(mouse, dt);
    rx_mouse_update_buttons(mouse, dt);
    const uint64_t motion = rx_rd64(mouse + 0x100);
    rx_wr64(mouse + 0x108, rx_rd64(mouse + 0xf8));
    SU32(mouse, 0x100, 0);
    SU32(mouse, 0xfc, 0);
    SU32(mouse, 0x118, 0);
    SU32(mouse, 0x11c, 0);
    rx_wr64(mouse + 0x110, motion);
    SU16(mouse, 0x104, 0);
}
RX_FUNCTION(0x00817340, rx_mouse_update)

/* 008150b0 XenonInputManager::vf43 (dt; ret 4): the frame's device update: the idle timer +0x3378 (+dt, 0 on any pad input),
 * the four pads and keyboards, then the mouse. */
void rx_input_update(rx_addr im, float dt) {
    SF32(im, 0x3378, dt + F32(im, 0x3378));
    for (uint32_t i = 0; i < 4; i++) {
        const rx_addr pad = im + 4 + 0x2dcu * i;
        rx_pad_update(pad, dt);
        if (rx_pad_any_input(pad)) SF32(im, 0x3378, 0.0f);
        rx_keyboard_poll(im + 0xb74 + 0x9b0u * i);
    }
    rx_mouse_update(im + 0x3234, dt);
}
RX_FUNCTION(0x008150b0, rx_input_update)

/* 00813de0 (EDI = key map list 00d92f50, stack dt; ret 4): every registered key map's update (slot 5) with dt, in list order
 * (vector +0x0c..+0x10). Engine::Update calls it once per frame after the device update. */
void rx_keymaps_update(rx_addr list, float dt) {
    for (uint32_t i = 0; i < (U32(list, 0x10) - U32(list, 0xc)) >> 2; i++) {
        const rx_addr km = U32(U32(list, 0xc), 4 * i);
        loop_tc_f(loop_vslot(km, 0x14), km, dt);
    }
}
RX_FUNCTION(0x00813de0, rx_keymaps_update)
