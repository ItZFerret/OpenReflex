/* build_globals_int.h -- shared by the files of unit 12b (core/notes/construction.md section 14): the classes the static initializers
 * construct (KeyMap, ConVar, ...), the MSVC 9 containers they use, and the stand-ins for OS objects. Internal to build_globals*.c. */
#pragma once
#include "rx_game.h"
#include "crt.h"
#include "build_alloc.h"
#include "build_globals.h"

/* ---- OS objects (the product has one thread and no kernel) ---- */
/* InitializeCriticalSection(cs): the object as the emulator's kernel32 stand-in leaves it (DebugInfo 0, LockCount -1, the rest 0). The
 * core never enters a critical section. */
void rx_critical_section_init(rx_addr cs);

/* ---- std::vector<T*> of MSVC 9 (Dinkumware): +0 proxy, +0xc first, +0x10 last, +0x14 end of storage ---- */
void rx_vecp_reserve(rx_addr v, uint32_t n);      /* FUN_004170a0 / FUN_00817e20: a block of n pointers (ReflexAlignedAlloc), old contents moved, old block freed */
void rx_vecp_push(rx_addr v, uint32_t value);     /* FUN_004171c0: store at last; at capacity the growth of FUN_00418060 (cap + cap / 2, at least size + 1) */
uint32_t rx_vecp_size(rx_addr v);
uint32_t rx_vecp_capacity(rx_addr v);

/* ---- KeyMap (input action by name): 0xa4 bytes; vtable 0x00974b74; the registry vector 0x00d92f50 ---- */
#define RX_KEYMAP_REGISTRY   0x00d92f50u
rx_addr rx_keymap_ctor(rx_addr self, rx_addr name, uint32_t id);   /* 00811230 (ESI = this, ret 8) */
void    rx_keymap_register(rx_addr registry, rx_addr keymap);      /* 00813300 (EAX = registry, ret 4) */
uint32_t rx_input_names_init(void);                               /* 00812170 InputNames_Init (build_globals_gen.c) */
void    rx_input_names_set_pad(uint32_t id, rx_addr name);         /* 00812e40 InputNames_SetPadName (EAX = id, one stack argument) */

/* ---- ConVar (console variable, 0x2c bytes: name[0x1f] at +4, current +0x24, default +0x28); the list vector 0x009e30e4 ---- */
#define RX_CONVAR_LIST       0x009e30e4u
void    rx_convar_ctor(rx_addr self, rx_addr name, uint32_t value);   /* 00417230 (EAX = name, ESI = this, value on the stack) */
void    rx_convar_list_init(void);                                    /* 00402be0 */
void    rx_convar_register(rx_addr list, rx_addr convar);             /* 00402b40 (EAX = list, ret 4) */

/* x87 `fld dword` + `fst dword` of a value (rx_x87): exact; a NaN keeps its payload with the quiet bit set, as on the hardware
 * (0x009cb448's 0x7fc00000 stays 0x7fc00000; reference worlds booted by the oracle before its 2026-10-27 NaN fix hold 0x7fffffff there). */
static inline uint32_t rx_x87_load_f32(uint32_t bits) { return rx_fb(rx_f80_to_f32(rx_f80_from_f32(rx_bf(bits)), 0)); }

/* sprintf(buf, "%s%u", s, n) -- the one format the initializers use (the CRT's sprintf needs per-thread data the product does not have yet) */
void rx_sprintf_su(rx_addr buf, rx_addr s, uint32_t n);

/* msvcr90's DllMain / _CRT_INIT data (build_globals_crt.c): after rx_image_load_crt */
void rx_crt_startup(void);

/* _atexit(destructor): RX_ATEXIT, crt_atexit.h (the product registers nothing; the oracle makes the original's call). */
#include "crt_atexit.h"

/* ---- the key binding loader of Engine::Startup (build_globals_input.c) ---- */
#include "../src/fmt_vfs.h"
rx_addr  rx_keymap_find(rx_addr reg, rx_addr name);                  /* 00813690 */
uint32_t rx_keyname_id(rx_addr reg, rx_addr name);                   /* 00813750 */
uint32_t rx_padname_id(rx_addr reg, rx_addr name);                   /* 00813790 */
uint32_t rx_mousename_id(rx_addr reg, rx_addr name);                 /* 008137e0 */
uint32_t rx_keybind_mod_mask(rx_addr reg, rx_addr mod);              /* 00813830 */
uint32_t rx_keymap_set_slot(rx_addr km, int32_t slot, uint32_t id, uint32_t esi, uint32_t edi, uint32_t ebx);   /* 008112e0 */
void     rx_keybind_load(const fmt_vfs* v, rx_addr reg, const char* name);   /* 00812ea0 */
