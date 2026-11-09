/* world_node_input.c -- the key maps (KeyMap / CompoundKeyMap / CumulativeKeyMap) and the Xenon input manager queries the
 * physics world's node share reaches, plus the clip event window 0083d3d0.
 *
 * KeyMap (thiscall, (mask, pad), RET 8): one 36-byte binding per pad (player slot) at this + pad * 36:
 *   +0x14 device bits (1 pad, 2 keyboard, 4 other; tested against the caller's mask)   +0x18 code   +0x1c opposite code
 *   +0x24 extra   +0x28 binding is set   +0x30 latched value   +0x34 enabled
 * Keyboard bindings (+0x14 & 2) first make sure the input name table exists (InputNames_Init 00812170, once, guard bit 0 of
 * 0xd92f48, atexit 0x9127c0) and are only live while a keyboard is attached (0xd92f80 != 0).
 * The device is the input manager behind the global 0xa95844 (its vf2 returns it; vf33 = 00814cd0 button state,
 * vf41 = 00814f30 the 24-byte control record, value float at +0xc).
 *
 * XenonInputManager layout (this = the manager): pads at +0x2dc * pad, +0x2a0 connected, +0x2c0 rumble capable,
 * +0x2c8 rumble dirty, +0x2cc + 8 * motor {low, high}; pad controls at +0x2dc * pad + 24 * code - 0x10 (state at -0xc);
 * keyboard records 24 * code + 0x82c (key codes start at 0x23: 24 * (code - 0x23) + 0xb74 {state, ?, last code});
 * other device 24 * code + 0x2fe0. The "nothing" control record is 0x92e0ec. */
#include "world_util.h"
#include "world_node.h"

#define KEYMAP_ENTRY(self, pad) ((self) + (pad) * 36u)
#define INPUT_DEVICE_GLOBAL 0x00a95844u
#define INPUT_NAMES_GUARD   0x00d92f48u
#define KEYBOARD_ATTACHED   0x00d92f80u
#define CONTROL_NONE        0x0092e0ecu

uint32_t rx_input_names_init(void);   /* 00812170 (build_globals_gen.c) */

/* the input name table, built on first use: InputNames_Init 00812170 + atexit(0x9127c0) */
static void input_names_once(void) {
    if (U32(0, INPUT_NAMES_GUARD) & 1u) return;
    SU32(0, INPUT_NAMES_GUARD, U32(0, INPUT_NAMES_GUARD) | 1u);
    WORLD_CALL_SP(0, rx_input_names_init());
    WORLD_ORIG(0x008f9fbf, RX_CDECL, 0, 0x009127c0);    /* _atexit */
}

/* the global input device: (*0xa95844)->vf2() */
static rx_addr input_device(void) {
    const rx_addr g = U32(0, INPUT_DEVICE_GLOBAL);
    const rx_addr fn = U32(U32(g, 0), 0x08);
    rx_addr (*f)(rx_addr) = RX_FN(fn, rx_addr (*)(rx_addr));
    return f ? f(g) : WORLD_ORIG0(fn, RX_THISCALL, g);
}
/* device vf33 (+0x84): button state (kind, a, b, c, d) */
static uint32_t device_button(rx_addr dev, uint32_t kind, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    const rx_addr fn = U32(U32(dev, 0), 0x84);
    uint32_t (*f)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) =
        RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t));
    return f ? f(dev, kind, a, b, c, d) : WORLD_ORIG(fn, RX_THISCALL, dev, kind, a, b, c, d);
}
/* device vf41 (+0xa4): the control record (kind, pad, code) */
static rx_addr device_control(rx_addr dev, uint32_t kind, uint32_t a, uint32_t b) {
    const rx_addr fn = U32(U32(dev, 0), 0xa4);
    rx_addr (*f)(rx_addr, uint32_t, uint32_t, uint32_t) = RX_FN(fn, rx_addr (*)(rx_addr, uint32_t, uint32_t, uint32_t));
    return f ? f(dev, kind, a, b) : WORLD_ORIG(fn, RX_THISCALL, dev, kind, a, b);
}

/* 00811420 KeyMap::vf1 (mask, pad): the latched value of a set, enabled binding of the mask's devices, else 0.
 * Keyboard bindings with no keyboard attached only pass for code 0x47. */
uint32_t rx_world_keymap_latched(rx_addr self, uint32_t mask, uint32_t pad) {
    const rx_addr e = KEYMAP_ENTRY(self, pad);
    if (U32(e, 0x28) == 0) return 0;
    if (U32(e, 0x14) & 2u) {
        input_names_once();
        if (U32(0, KEYBOARD_ATTACHED) == 0 && U32(e, 0x18) != 0x47) return 0;
    }
    if ((U32(e, 0x14) & mask) == 0 || U32(e, 0x34) == 0) return 0;
    return U32(e, 0x30);
}
RX_FUNCTION(0x00811420, rx_world_keymap_latched)

/* 008115f0 KeyMap::vf2 (mask, pad): is the binding's button down (device vf33 (bits, pad, code, extra, 1)) */
uint32_t rx_world_keymap_down(rx_addr self, uint32_t mask, uint32_t pad) {
    const rx_addr e = KEYMAP_ENTRY(self, pad);
    if (U32(e, 0x14) & 2u) {
        if (U32(e, 0x28) == 0) return 0;
        input_names_once();
        if (U32(0, KEYBOARD_ATTACHED) == 0) return 0;
    }
    if ((U32(e, 0x14) & mask) == 0) return 0;
    const rx_addr dev = input_device();
    if (U32(e, 0x34) == 0) return 0;
    return device_button(dev, U32(e, 0x14), pad, U32(e, 0x18), U32(e, 0x24), 1);
}
RX_FUNCTION(0x008115f0, rx_world_keymap_down)

/* 00811710 KeyMap::vf4 (mask, pad): the binding's analog value (x87 result): the code's control value, minus the opposite
 * code's value when there is one ((-0 - opposite) + value). 0 for a binding that is not live.
 * The original keeps the running value in its pad argument slot: `slot` is that slot's address when called from original
 * code (0 from C; the slot is dead after the RET 8). */
float rx_world_keymap_value_at(rx_addr self, uint32_t mask, uint32_t pad, rx_addr slot) {
    const rx_addr e = KEYMAP_ENTRY(self, pad);
    if (U32(e, 0x14) & 2u) {
        if (U32(e, 0x28) == 0) return 0.0f;
        input_names_once();
        if (U32(0, KEYBOARD_ATTACHED) == 0) return 0.0f;
    }
    if ((U32(e, 0x14) & mask) == 0) return 0.0f;
    const rx_addr dev = input_device();
    if (U32(e, 0x34) == 0) return 0.0f;
    float v = F32(device_control(dev, U32(e, 0x14), pad, U32(e, 0x18)), 0x0c);
    if (slot) SF32(slot, 0, v);
    const uint32_t opposite = U32(e, 0x1c);
    if (opposite != 0) {
        const float o = F32(device_control(dev, U32(e, 0x14), pad, opposite), 0x0c);
        v = phys_add(phys_sub(-0.0f /* 0x9774a8 */, o), v);
        if (slot) SF32(slot, 0, v);
    }
    return v;
}
float rx_world_keymap_value(rx_addr self, uint32_t mask, uint32_t pad) { return rx_world_keymap_value_at(self, mask, pad, 0); }
RX_FUNCTION(0x00811710, rx_world_keymap_value)

/* 006d78a0 DebugKeyMap::vf4 (thiscall (mask, pad); RET 8 -> st0): 0.0 while the debug keys are off (byte 0x9b716d), else a tail jump to
 * KeyMap::vf4 00811710 (the binding's analog value). Same gate as DebugKeyMap::vf1 00508970. Reached by any analog query (slot 0x10) of a
 * debug action's key map ("FreezeGame", "DebugPhysics_*": build_globals_gen.c); read off the disassembly (w-vtunported). */
float rx_debug_keymap_value(rx_addr self, uint32_t mask, uint32_t pad) {
    if (U8(0, 0x009b716d) != 0) return 0.0f;
    return rx_world_keymap_value(self, mask, pad);
}
RX_FUNCTION(0x006d78a0, rx_debug_keymap_value)

/* 008115e0 KeyMap::vf8 (mask, pad): the binding's code */
uint32_t rx_world_keymap_code(rx_addr self, uint32_t mask, uint32_t pad) {
    (void)mask;
    return U32(KEYMAP_ENTRY(self, pad), 0x18);
}
RX_FUNCTION(0x008115e0, rx_world_keymap_code)

/* ---- CompoundKeyMap: up to four sub pad maps at +0xa4 .. +0xb0, combine mode +0xb4 ---- */

/* 00811ab0 CompoundKeyMap::vf2 (mask, pad): any sub map down (in order, stops at the first) */
uint32_t rx_world_compound_keymap_down(rx_addr self, uint32_t mask, uint32_t pad) {
    for (int i = 0; i < 4; i++) {
        const rx_addr sub = U32(self, 0xa4 + 4 * i);
        if (sub == 0) continue;
        const rx_addr fn = U32(U32(sub, 0), 0x08);
        uint32_t (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t));
        if ((f ? f(sub, mask, pad) : WORLD_ORIG(fn, RX_THISCALL, sub, mask, pad)) != 0) return 1;
    }
    return 0;
}
RX_FUNCTION(0x00811ab0, rx_world_compound_keymap_down)

static float sub_keymap_value(rx_addr sub, uint32_t mask, uint32_t pad) {
    const rx_addr fn = U32(U32(sub, 0), 0x10);
    float (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, float (*)(rx_addr, uint32_t, uint32_t));
    if (f) return f(sub, mask, pad);
    rx_regs o;
    WORLD_CTX(fn, &WCTX(.ecx = sub), &o, mask, pad);
    return o.st0;
}

/* 00811b50 CompoundKeyMap::vf4 (mask, pad): the sub maps' values combined (x87 result). Mode +0xb4:
 * 0 the mean (sum / count of present maps; 0 / 0 with none), 1 the largest, 2 the smallest, 3 the largest magnitude,
 * 4 the smallest magnitude (each "first present map wins" and replaced only on a strict compare); other modes 0. */
float rx_world_compound_keymap_value(rx_addr self, uint32_t mask, uint32_t pad) {
    float acc = 0.0f;
    int count = 0;
    for (int i = 0; i < 4; i++) {
        const rx_addr sub = U32(self, 0xa4 + 4 * i);
        if (sub == 0) continue;
        const float v = sub_keymap_value(sub, mask, pad);
        const uint32_t mode = U32(self, 0xb4);
        const float av = fabsf(v), aacc = fabsf(acc);   /* andps 0x977570 */
        if (mode == 1) { if (v > acc || count == 0) acc = v; }
        else if (mode == 3) { if (av > aacc || count == 0) acc = v; }
        else if (mode == 2) { if (acc > v || count == 0) acc = v; }
        else if (mode == 4) { if (aacc > av || count == 0) acc = v; }
        else if (mode == 0) acc = phys_add(v, acc);
        count++;
    }
    if (U32(self, 0xb4) == 0) acc = phys_div(acc, (float)count);
    return acc;
}
RX_FUNCTION(0x00811b50, rx_world_compound_keymap_value)

/* 00811db0 CumulativeKeyMap::vf4 (mask, pad): the accumulated value at +0xa4 */
float rx_world_cumulative_keymap_value(rx_addr self, uint32_t mask, uint32_t pad) {
    (void)mask; (void)pad;
    return F32(self, 0xa4);
}
RX_FUNCTION(0x00811db0, rx_world_cumulative_keymap_value)

/* ---- XenonInputManager ---- */

/* keyboard record of a key code (codes from 0x23): 24 * (code - 0x23) + 0xb74 {state, flag, last code} */
static rx_addr keyboard_record(rx_addr self, uint32_t code) { return self + (code * 3u - 0x69u) * 8u + 0xb74u; }

/* 00814c00 XenonInputManager::vf31 (code, which): the key went down with (which | 0x8000) as its last code (state 2).
 * (The compiler kept a test of (which | 0x8000) == 2 that can never hold.) */
uint32_t rx_world_xenon_key_pressed(rx_addr self, uint32_t code, uint32_t which) {
    const uint32_t tag = which | 0x8000u;
    const rx_addr k = keyboard_record(self, code);
    if (tag == 2 && U32(k, 4) == tag) return 1;
    return U32(k, 8) == tag && U32(k, 0) == 2;
}
RX_FUNCTION(0x00814c00, rx_world_xenon_key_pressed)

/* 00814e60 XenonInputManager::vf37 (code, which): the key is held (flag 1) with (which | 0x8000) as its last code */
uint32_t rx_world_xenon_key_held(rx_addr self, uint32_t code, uint32_t which) {
    const uint32_t tag = which | 0x8000u;
    const rx_addr k = keyboard_record(self, code);
    if (tag == 2 && U32(k, 4) == 1) return 1;
    return U32(k, 8) == tag && U32(k, 4) == 1;
}
RX_FUNCTION(0x00814e60, rx_world_xenon_key_held)

/* 00814cd0 XenonInputManager::vf33 (kind, pad, code, which, unused; RET 0x14): is the control down.
 * kind 1 pad: connected pad and control state 1; kind 2 keyboard: held key (as vf37, with `which` as given); kind 4: other
 * device state 1. */
uint32_t rx_world_xenon_button(rx_addr self, uint32_t kind, uint32_t pad, uint32_t code, uint32_t which, uint32_t unused) {
    (void)unused;
    if (kind == 1) {
        const rx_addr p = self + pad * 0x2dcu;
        if (U32(p, 0x2a0) == 0) return 0;
        return U32(p + code * 24u, -0x0c) == 1;
    }
    if (kind == 2) {
        const rx_addr k = keyboard_record(self, code);
        if (which == 2 && U32(k, 4) == 1) return 1;
        return U32(k, 8) == which && U32(k, 4) == 1;
    }
    if (kind == 4) return U32(self + code * 24u, 0x2fe4) == 1;
    return 0;
}
RX_FUNCTION(0x00814cd0, rx_world_xenon_button)

/* 00814f30 XenonInputManager::vf41 (kind, pad, code; RET 0xc): the 24-byte control record (value float at +0xc), or the
 * "nothing" record 0x92e0ec for a disconnected pad or an unknown kind */
rx_addr rx_world_xenon_control(rx_addr self, uint32_t kind, uint32_t pad, uint32_t code) {
    if (kind == 1) {
        const rx_addr p = self + pad * 0x2dcu;
        if (U32(p, 0x2a0) == 0) return CONTROL_NONE;
        return p + code * 24u - 0x10u;
    }
    if (kind == 2) return self + code * 24u + 0x82cu;
    if (kind == 4) return self + code * 24u + 0x2fe0u;
    return CONTROL_NONE;
}
RX_FUNCTION(0x00814f30, rx_world_xenon_control)

/* 008151e0 XenonInputManager::vf46 (kind, pad, motor, high, low; RET 0x14): set a pad motor's rumble {low, high} and mark it
 * dirty, for a connected, rumble-capable pad (kind 1 only). The speeds are copied as raw float bits. */
void rx_world_xenon_set_rumble(rx_addr self, uint32_t kind, uint32_t pad, uint32_t motor, uint32_t high, uint32_t low) {
    if (kind != 1) return;
    const rx_addr p = self + pad * 0x2dcu;
    if (U32(p, 0x2a0) == 0 || U32(p, 0x2c0) == 0) return;
    SU32(p + motor * 8u, 0x2cc, low);
    SU32(p + motor * 8u, 0x2d0, high);
    SU32(p, 0x2c8, 1);
}
RX_FUNCTION(0x008151e0, rx_world_xenon_set_rumble)

/* ---- 0083d3d0 (ECX = 8 out events of 12 bytes, xmm2 = from, xmm1 = to; stdcall clip; RET 4): copy the clip's events
 * (count +0x90, 12-byte records at +0x78, time at +4) with from < time <= to, at most 8. Returns the count (EDX). */
uint32_t rx_world_clip_events_between(rx_addr out, rx_addr clip, float from, float to) {
    uint32_t n = 0;
    for (int32_t i = 0; i < I32(clip, 0x90); i++) {
        const rx_addr ev = U32(clip, 0x78) + 12u * (uint32_t)i;
        const float t = F32(ev, 4);
        if (!(t > from) || !(to >= t)) continue;
        SU32(out, 0, U32(ev, 0)); SU32(out, 4, U32(ev, 4)); SU32(out, 8, U32(ev, 8));
        out += 12; n++;
        if (n >= 8) break;
    }
    return n;
}
RX_FUNCTION(0x0083d3d0, rx_world_clip_events_between)
