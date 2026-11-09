/* build_globals_input.c -- unit 12b: the key binding loader of Engine::Startup (core/notes/construction.md section 14).
 *
 * Engine::Startup (0084e320) calls FUN_00812ea0 twice on the KeyMap registry (0x00d92f50): "KeyBind_Core.bxml", "KeyBind_App.bxml". Each file is
 * <KeyBindings><Actions><keyboard|mouse|xbox360><bind>"[mod-]key  ActionName"</bind>...</Actions><Commands>...<Loader><name>file</name></Loader></Commands>.
 * Every line is parsed by FUN_00813b00 and bound into the KeyMap of that name (KeyMap_SetSlot 008112e0 for pad buttons, the first element for
 * keys / mouse buttons); lines of the Commands section make a KeyMapCommand named ":mod:key:" (FUN_00813940 / FUN_00812000). The original reads
 * the file through its XMLReader; the core parses the same BXML with fmt_bxml (the reader's own state -- cursor pool, string table -- is not
 * reproduced). Registry layout: +0xc first / +0x10 last / +0x14 end of the KeyMap* vector, +0x18 the KeyMapCommand* vector (+0x24 .. +0x2c),
 * +0x34 the key name table (0x66 x 0x20), +0x794 the modifier names, +0xcf4 the pad button names, +0xff4 the mouse button names. */
#include "build_globals_int.h"
#include "../src/fmt_bxml.h"
#include "../src/fmt_vfs.h"

/* 00813690 KeyMapRegistry_Find (ESI = registry, name on the stack, ret 4) */
rx_addr rx_keymap_find(rx_addr reg, rx_addr name) {
    uint32_t n = rx_vecp_size(reg);
    for (uint32_t i = 0; i < n; i++)
        if (crt_stricmp(U32(U32(U32(reg, 0xc), 4 * i), 4), name) == 0) return U32(U32(reg, 0xc), 4 * i);
    return 0;
}

/* 00813750 (EAX = registry, name): 0x23 + the index in the key name table, 0 when absent */
uint32_t rx_keyname_id(rx_addr reg, rx_addr name) {
    for (uint32_t i = 0; i < 0x66; i++) if (crt_stricmp(name, reg + 0x34 + 0x20 * i) == 0) return i + 0x23;
    return 0;
}
/* 00813790 (EAX = registry, name): 1 + the index in the pad button name table, 1 when absent */
uint32_t rx_padname_id(rx_addr reg, rx_addr name) {
    for (uint32_t i = 0; i < 0x18; i++) if (crt_stricmp(name, reg + 0xcf4 + 0x20 * i) == 0) return i + 1;
    return 1;
}
/* 008137e0 (EAX = registry, name): 0x19 + the index in the mouse button names, 0x19 when absent */
uint32_t rx_mousename_id(rx_addr reg, rx_addr name) {
    for (uint32_t i = 0; i < 10; i++) if (crt_stricmp(name, reg + 0xff4 + 0x20 * i) == 0) return i + 0x19;
    return 0x19;
}
/* 00813830 (ECX = registry, modifier name): the modifier mask of "lalt" / "ralt" / "lctrl" / "rctrl" / "lshift" (and the names without the
 * side letter: both sides); 0 for no modifier */
uint32_t rx_keybind_mod_mask(rx_addr reg, rx_addr mod) {
    if (!mod) return 0;
    rx_addr s = reg + 0x794;
    for (uint32_t i = 0x3b; i < 0x40; i++, s += 0x20) {
        if (crt_stricmp(mod, s) == 0) {
            switch (i) { case 0x3b: return 0x40; case 0x3c: return 0x80; case 0x3d: return 0x10; case 0x3e: return 0x20; case 0x3f: return 4; default: return 8; }
        } else if (crt_stricmp(mod, s + 1) == 0) {
            switch (i) { case 0x3b: case 0x3c: return 0xc0; case 0x3d: case 0x3e: return 0x30; default: return 0xc; }
        }
    }
    return 0;
}

/* 008112e0 KeyMap_SetSlot(EAX = slot or -1 for all four, EDX = id, ESI, EDI, keymap, ebx): the element(s) at keymap + 0x14 + 0x24 * slot get
 * {+0 ebx, +4 id, +8 esi, +0x10 edi, +0x14 1, +0x18 0.0f, +0x1c 0}; 0 when id is 0 */
static void element_set(rx_addr e, uint32_t ebx, uint32_t id, uint32_t esi, uint32_t edi) {
    SU32(e, 0, ebx); SU32(e, 4, id); SU32(e, 8, esi); SU32(e, 0x10, edi); SU32(e, 0x14, 1); SF32(e, 0x18, 0.0f); SU32(e, 0x1c, 0);
}
uint32_t rx_keymap_set_slot(rx_addr km, int32_t slot, uint32_t id, uint32_t esi, uint32_t edi, uint32_t ebx) {
    if (id == 0) return 0;
    if (slot == -1) {
        for (uint32_t k = 1; k < 4; k++) element_set(km + 0x14 + 0x24 * k, ebx, id, esi, edi);
        element_set(km + 0x14, ebx, id, esi, edi);
    } else element_set(km + 0x14 + 0x24 * (uint32_t)slot, ebx, id, esi, edi);
    return 1;
}

/* 00813c80 (ECX = registry, EAX = action, stack: modifier, key): a keyboard key into the first element */
static uint32_t bind_key(rx_addr reg, rx_addr action, rx_addr mod, rx_addr key) {
    rx_addr km = rx_keymap_find(reg, action);
    if (!km) return 0;
    if (!mod) {
        if (crt_stricmp(key, 0x0097471c) == 0 || crt_stricmp(key, 0x00974724) == 0) return 0;     /* "pageup" / "pagedown" without a modifier */
    }
    uint32_t mask = rx_keybind_mod_mask(reg, mod);
    uint32_t id = rx_keyname_id(reg, key);
    if (!id) return 0;
    SU32(km, 0x18, id); SU32(km, 0x24, mask); SU32(km, 0x14, 2); SU32(km, 0x1c, 0);
    SU32(km, 0x28, 1); SF32(km, 0x2c, 0.0f); SU32(km, 0x30, 0);
    return 1;
}
/* 00813d10 (ECX = registry, EAX = action, stack: pad number string or 0, button name): a pad button; the number selects the slot (atoi - 1) */
static uint32_t bind_pad(rx_addr reg, rx_addr action, rx_addr num, rx_addr button) {
    rx_addr km = rx_keymap_find(reg, action);
    if (!km) return 0;
    int32_t slot = -1;
    if (num) slot = crt_atoi(num) - 1;
    rx_keymap_set_slot(km, slot, rx_padname_id(reg, button), 0, 0, 1);
    return 1;
}
/* 00813d80 (ECX = registry, EAX = action, stack: mouse button name) */
static uint32_t bind_mouse(rx_addr reg, rx_addr action, rx_addr button) {
    rx_addr km = rx_keymap_find(reg, action);
    if (!km) return 0;
    uint32_t id = rx_mousename_id(reg, button);
    if (id) {
        SU32(km, 0x14, 4); SU32(km, 0x18, id); SU32(km, 0x1c, 0); SU32(km, 0x24, 0);
        SU32(km, 0x28, 1); SF32(km, 0x2c, 0.0f); SU32(km, 0x30, 0);
    }
    return 1;
}

/* 00812000 KeyMapCommand(name EAX, command text): a KeyMap (vtable 0x00974ba8) that also keeps a copy of the command text at +0xa4 */
static rx_addr command_ctor(rx_addr self, rx_addr name, rx_addr text) {
    rx_keymap_ctor(self, name, 0);
    SU32(self, 0, 0x00974ba8);
    uint32_t len = crt_strlen(text);
    rx_addr p = rx_reflex_aligned_alloc(len + 1);
    SU32(self, 0xa4, p);
    crt_strncpy(p, text, len);
    SU8(p, len, 0);
    return self;
}
/* sprintf(buf, ":%s:%s:", a, b) with the CRT's "(null)" for a null string */
static void format_key(rx_addr buf, rx_addr a, rx_addr b) {
    uint32_t k = 0;
    SU8(buf, k++, ':');
    for (int pass = 0; pass < 2; pass++) {
        rx_addr s = pass ? b : a;
        if (!s) { const char* z = "(null)"; for (int i = 0; z[i]; i++) SU8(buf, k++, z[i]); }
        else for (uint32_t i = 0; U8(s, i); i++) SU8(buf, k++, U8(s, i));
        SU8(buf, k++, ':');
    }
    SU8(buf, k, 0);
}
/* 00813940 (ECX = modifier, stack: registry, key, command text): the KeyMapCommand ":mod:key:" -- the existing one (its text parsed again as a
 * binding line: chained commands) or a new one added to the registry's command vector (reserve +10 at capacity) */
static rx_addr keybind_line(rx_addr reg, rx_addr text, uint32_t is_action);
static rx_addr command_get(rx_addr reg, rx_addr mod, rx_addr key, rx_addr text) {
    RX_FRAME_ENTER(F, 0xa0);
    rx_addr buf = F + 0x20;
    format_key(buf, mod, key);
    rx_addr cmds = reg + 0x18;
    uint32_t n = rx_vecp_size(cmds);
    for (uint32_t i = 0; i < n; i++) {
        if (crt_strcmp(buf, U32(U32(U32(cmds, 0xc), 4 * i), 4)) == 0) {
            keybind_line(reg, text, 0);
            rx_addr r = U32(U32(cmds, 0xc), 4 * i);
            RX_FRAME_LEAVE(F, 0xa0);
            return r;
        }
    }
    if (rx_vecp_size(cmds) == rx_vecp_capacity(cmds)) rx_vecp_reserve(cmds, rx_vecp_capacity(cmds) + 10);     /* FUN_005ba080 */
    rx_addr obj = rx_reflex_aligned_alloc(0xa8);
    if (obj) obj = command_ctor(obj, buf, text);
    rx_vecp_push(cmds, obj);                                                                                  /* FUN_006b7590 */
    RX_FRAME_LEAVE(F, 0xa0);
    return obj;
}

/* 00813b00 FUN_00813b00 (ECX = registry, stack: text, is_action): one "[mod-]key action" line; the text is edited in place */
static rx_addr keybind_line(rx_addr reg, rx_addr text, uint32_t is_action) {
    rx_addr p = text;
    while (U8(p, 0) == ' ') p++;
    rx_addr mod = 0, key = p, q = p;
    while (U8(q, 0) != ' ' && U8(q, 0) != 0) {
        if (U8(q, 0) == '-' && q != p && mod == 0) { key = q + 1; SU8(q, 0, 0); mod = p; }
        q++;
    }
    SU8(q, 0, 0);
    q++;
    while (U8(q, 0) == ' ') q++;
    rx_addr act = q;
    int after_plus = 0;
    if (U8(act, 0) == '+') { act++; after_plus = 1; }
    if (!after_plus && is_action != 1) {            /* a Commands line: the command object, then bind its name */
        rx_addr cmd = command_get(reg, mod, key, act);
        rx_addr name = U32(cmd, 4);
        if (mod && crt_strstr(mod, 0x0097488c)) {   /* "joy" */
            rx_addr num = mod + 3;
            return bind_pad(reg, name, U8(num, 0) ? num : 0, key);
        }
        return bind_key(reg, name, mod, key);
    }
    if (!mod) return bind_key(reg, act, 0, key);
    if (crt_strstr(mod, 0x0097488c)) {              /* "joy[n]" */
        if (U8(mod, 3) == 0) {
            rx_addr km = rx_keymap_find(reg, act);
            if (!km) return 0;
            rx_keymap_set_slot(km, -1, rx_padname_id(reg, key), 0, 0, 1);
            return 1;
        }
        return bind_pad(reg, act, mod + 3, key);
    }
    if (crt_strstr(mod, 0x0097486c)) return bind_mouse(reg, act, key);       /* "mouse" */
    return bind_key(reg, act, mod, key);
}

/* the children of a node: each one's string value is a line */
static void lines_of(const fmt_bxml* d, int node, rx_addr reg, uint32_t is_action, rx_addr buf) {
    for (int k = 0; k < fmt_bxml_child_count(d, node); k++) {
        fmt_bxml_value v;
        int c = fmt_bxml_child(d, node, k);
        if (fmt_bxml_node_value(d, c, &v) != 0 || v.kind != FMT_BX_STRING || !v.str) continue;
        uint32_t n = 0;
        while (n < 0xff && v.str[n]) { SU8(buf, n, v.str[n]); n++; }
        for (uint32_t i = n; i <= 0xff; i++) SU8(buf, i, 0);       /* strncpy(buf, s, 0xff): zero padded; buf[0xff] = 0 */
        keybind_line(reg, buf, is_action);
    }
}

/* 00812ea0 FUN_00812ea0 (ECX = registry, name): loads one KeyBind file of the install root. A missing file is skipped. */
void rx_keybind_load(const fmt_vfs* v, rx_addr reg, const char* name) {
    fmt_buf b = {0};
    if (!fmt_vfs_exists(v, name) || fmt_vfs_read_file(v, name, &b) != 0) return;
    fmt_bxml* d = fmt_bxml_parse(b.data, b.size);
    fmt_buf_free(&b);
    if (!d) return;
    RX_FRAME_ENTER(F, 0x110);
    rx_addr buf = F;
    static const char* const kinds[3] = {"keyboard", "mouse", "xbox360"};
    int actions = fmt_bxml_find_child(d, 0, "Actions");
    if (actions >= 0)
        for (int i = 0; i < 3; i++) { int n = fmt_bxml_find_child(d, actions, kinds[i]); if (n >= 0) lines_of(d, n, reg, 1, buf); }
    int commands = fmt_bxml_find_child(d, 0, "Commands");
    if (commands >= 0) {
        for (int i = 0; i < 3; i++) { int n = fmt_bxml_find_child(d, commands, kinds[i]); if (n >= 0) lines_of(d, n, reg, 0, buf); }
        int loader = fmt_bxml_find_child(d, commands, "Loader");
        if (loader >= 0) {
            for (int k = 0; k < fmt_bxml_child_count(d, loader); k++) {
                fmt_bxml_value val;
                int c = fmt_bxml_child(d, loader, k);
                if (fmt_bxml_node_value(d, c, &val) != 0 || val.kind != FMT_BX_STRING || !val.str) continue;
                rx_keybind_load(v, reg, val.str);
            }
        }
    }
    RX_FRAME_LEAVE(F, 0x110);
    fmt_bxml_free(d);
}
