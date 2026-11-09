/* build_globals_class.c -- unit 12b: the classes the static initializers construct (core/notes/construction.md section 14).
 * Hand-transliterated from the decompile / disassembly; the generated initializers (build_globals_gen.c) call these. */
#include "build_globals_int.h"
#include <string.h>

/* ---- OS objects ---- */
void rx_critical_section_init(rx_addr cs) {
    SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 0);
}

/* ---- std::vector<T*> ---- */
uint32_t rx_vecp_size(rx_addr v)     { return U32(v, 0xc) ? (U32(v, 0x10) - U32(v, 0xc)) >> 2 : 0; }
uint32_t rx_vecp_capacity(rx_addr v) { return U32(v, 0xc) ? (U32(v, 0x14) - U32(v, 0xc)) >> 2 : 0; }

/* FUN_004170a0 (= FUN_00817e20): reserve(n) */
void rx_vecp_reserve(rx_addr v, uint32_t n) {
    if (rx_vecp_capacity(v) >= n) return;
    rx_addr nb = rx_reflex_aligned_alloc(n * 4);                       /* FUN_0042bf30: ReflexAlignedAlloc(n * 4) */
    uint32_t old = U32(v, 0xc), last = U32(v, 0x10);
    for (uint32_t k = 0; k < (last - old) >> 2; k++) SU32(nb, 4 * k, U32(old, 4 * k));   /* memmove_s */
    if (old) rx_reflex_aligned_free(old);
    SU32(v, 0x14, nb + n * 4);
    SU32(v, 0x10, nb + (((last - old) >> 2) * 4));
    SU32(v, 0xc, nb);
}

/* FUN_004171c0 with EDI = the vector */
void rx_vecp_push(rx_addr v, uint32_t value) {
    uint32_t cap = rx_vecp_capacity(v), size = rx_vecp_size(v);
    if (size < cap) { SU32(U32(v, 0x10), 0, value); SU32(v, 0x10, U32(v, 0x10) + 4); return; }
    /* FUN_004178e0 -> FUN_00418060 (insert at the end of a full vector): capacity + capacity / 2, at least size + 1 */
    uint32_t want = size + 1, ncap = cap + (cap >> 1);
    if (ncap < want) ncap = want;
    rx_addr nb = rx_reflex_aligned_alloc(ncap * 4);
    uint32_t old = U32(v, 0xc);
    for (uint32_t k = 0; k < size; k++) SU32(nb, 4 * k, U32(old, 4 * k));
    SU32(nb, 4 * size, value);
    if (old) rx_reflex_aligned_free(old);
    SU32(v, 0x14, nb + ncap * 4);
    SU32(v, 0x10, nb + (size + 1) * 4);
    SU32(v, 0xc, nb);
}

/* ---- KeyMap ---- */
/* Fn_00810410 (ECX = one of the four 0x24-byte elements at KeyMap + 0x14) */
static void keymap_element_ctor(rx_addr e) {
    SU32(e, 0x14, 0); SU32(e, 0x1c, 0); SF32(e, 0x18, 0.0f); SU32(e, 0x20, 1); SU32(e, 8, 0);
}

/* 00813300 (EAX = the registry vector, one stack argument): registers a KeyMap by name unless a map of that name (case insensitive) is already in */
void rx_keymap_register(rx_addr registry, rx_addr km) {
    uint32_t n = rx_vecp_size(registry);
    for (uint32_t i = 0; i < n; i++)
        if (crt_stricmp(U32(km, 4), U32(U32(U32(registry, 0xc), 4 * i), 4)) == 0) return;
    if (rx_vecp_size(registry) == rx_vecp_capacity(registry)) rx_vecp_reserve(registry, rx_vecp_capacity(registry) + 10);   /* FUN_00817e20 */
    rx_vecp_push(registry, km);
}

/* 00811230 KeyMap(name, id): vtable, four elements, the name copied into a block of its own, defaults 0.5 / 0.1, the registry (InputNames_Init
 * on first use) */
rx_addr rx_keymap_ctor(rx_addr self, rx_addr name, uint32_t id) {
    SU32(self, 0, 0x00974b74);                                          /* KeyMap::vftable */
    for (uint32_t k = 0; k < 4; k++) keymap_element_ctor(self + 0x14 + 0x24 * k);   /* `vector constructor iterator'(+0x14, 0x24, 4, Fn_00810410) */
    uint32_t len = crt_strlen(name);
    rx_addr p = rx_reflex_aligned_alloc(len + 1);
    SU32(self, 4, p);
    crt_strncpy(p, name, len);
    SU8(p, len, 0);
    SF32(self, 0xc, F32(0, 0x009322cc));                                /* 0.5f */
    SU32(self, 8, id);
    SF32(self, 0x10, F32(0, 0x00931d94));                               /* 0.1f */
    if (!(U32(0, 0x00d92f48) & 1)) {                                    /* function-local static guard */
        SU32(0, 0x00d92f48, U32(0, 0x00d92f48) | 1);
        rx_input_names_init();                                          /* InputNames_Init */
        RX_ATEXIT(0x009127c0);
    }
    rx_keymap_register(RX_KEYMAP_REGISTRY, self);
    return self;
}
RX_FUNCTION(0x00811230, rx_keymap_ctor)

/* ---- ConVar ---- */
/* 00402be0: the list's first construction (a proxy block holding the container's address, empty storage) */
void rx_convar_list_init(void) {
    rx_addr p = rx_reflex_aligned_alloc(4);
    SU32(0, RX_CONVAR_LIST, p);
    if (p) SU32(p, 0, RX_CONVAR_LIST);
    SU32(0, 0x009e30f0, 0); SU32(0, 0x009e30f4, 0); SU32(0, 0x009e30f8, 0);
}

/* 00402b40 (EAX = list, one stack argument): add a ConVar unless one of that name (case insensitive) is in; the list grows by 25 */
void rx_convar_register(rx_addr list, rx_addr cv) {
    uint32_t n = rx_vecp_size(list);
    for (uint32_t i = 0; i < n; i++)
        if (crt_stricmp(cv + 4, U32(U32(list, 0xc), 4 * i) + 4) == 0) return;
    if (rx_vecp_size(list) == rx_vecp_capacity(list)) rx_vecp_reserve(list, rx_vecp_capacity(list) + 0x19);   /* FUN_004170a0 */
    rx_vecp_push(list, cv);
}

/* 00417230 ConVar<int>(name, value): EAX = name, ESI = this */
void rx_convar_ctor(rx_addr self, rx_addr name, uint32_t value) {
    SU32(self, 0, 0x00937740);                                          /* ConVar<HU, ConVarTraits<H>>::vftable */
    crt_strncpy(self + 4, name, 0x1f);
    SU8(self, 0x23, 0);
    SU32(self, 0x28, value);
    SU32(self, 0x24, value);
    if (!(U32(0, 0x009e30fc) & 1)) {                                    /* guard of the list (function-local static) */
        SU32(0, 0x009e30fc, U32(0, 0x009e30fc) | 1);
        rx_convar_list_init();
        RX_ATEXIT(0x00911d40);
    }
    rx_convar_register(RX_CONVAR_LIST, self);
}

/* ---- pad input names ---- */
/* 00812e40 InputNames_SetPadName (EAX = the pad input id, one stack argument): the short name (0x20 bytes per id at 0x00d93c24) and the wide
 * name (0x40 bytes per id at 0x00d94044, mbstowcs_s of the ASCII name including its NUL) */
void rx_input_names_set_pad(uint32_t id, rx_addr name) {
    crt_strncpy(0x00d93c24 + id * 0x20, name, 0x1f);
    SU8(0, 0x00d93c43 + id * 0x20, 0);
    uint32_t n = crt_strlen(name) + 1;
    for (uint32_t k = 0; k < n; k++) SU16(0, 0x00d94044 + id * 0x40 + 2 * k, U8(name, k));
}
RX_FUNCTION(0x00812e40, rx_input_names_set_pad)

/* sprintf(buf, "%s%u", s, n) */
void rx_sprintf_su(rx_addr buf, rx_addr s, uint32_t n) {
    uint32_t k = 0;
    for (uint32_t i = 0; U8(s, i); i++) SU8(buf, k++, U8(s, i));
    char d[12]; int nd = 0;
    do { d[nd++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (nd) SU8(buf, k++, d[--nd]);
    SU8(buf, k, 0);
}
