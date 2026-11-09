/* build_mxgame_event.c -- unit 10: the game's global event channels created by the MXGame constructor and the save-game object
 * (build_mxgame.h): "Stunts" (006c8fd0), "StuntScoring" (006cb870), the SavedGameManager's "StorageNotification" (005a61b0), and the
 * void(float) event type of 005ed6e0 / 005f2460. The channel / event machinery is unit 8's (build_activity.h: rx_event_channel_*,
 * rx_event_type_register_at with each instantiation's signature and stack depth, the table in oracle/build_activity.cpp). */
#include "build_mxgame_int.h"
#include "build_activity.h"

/* the inlined EventSystem::Channel constructor (0x5c: base channel without parent 005930a0, vtable 0x00946258, +0x40 a std::list
 * {proxy, +0x54 head 007ce320, +0x58 size}), as each creator writes it */
static rx_addr channel_new_at(uint32_t inner_push) {
    const rx_addr sp0 = rx_sp();
    rx_addr ch;
    MX_CALLR(ch, 1, rx_reflex_aligned_alloc(0x5c));
    if (!ch) return 0;
    rx_set_sp(sp0 - inner_push);                               /* registers the creator saves after the allocation */
    MX_CALL(1, rx_event_base_channel_construct(ch, 0));
    SU32(ch, 0, 0x00946258);
    rx_addr p;
    MX_CALLR(p, 1, rx_reflex_aligned_alloc(4));
    if (p) SU32(p, 0, ch + 0x40);
    SU32(ch, 0x40, p);
    rx_addr h;
    MX_CALLR(h, 0, rx_event_list_head_alloc());
    SU32(ch, 0x54, h);
    SU32(ch, 0x58, 0);
    rx_set_sp(sp0);
    return ch;
}
static rx_addr channel_new(void) { return channel_new_at(0); }

/* the event-type instantiations the channels below use (EAX = channel; the name on the stack (RET 4) or fixed (RET 0)); signature
 * descriptor and stack depth per instantiation as oracle/build_activity.cpp's table */
typedef struct { uint32_t fn, name, td; int aligned; int32_t sp; } event_type;
static const event_type T_006ce210 = {0x006ce210, 0, 0x009cbf38, 0, -128};
static const event_type T_006ce320 = {0x006ce320, 0x009607b4, 0x009d9a04, 0, -132};
static const event_type T_006ce430 = {0x006ce430, 0, 0x009ce768, 0, -128};
static const event_type T_005c0df0 = {0x005c0df0, 0, 0x009cbeb4, 1, -136};
static const event_type T_00591080 = {0x00591080, 0, 0x009cf610, 1, -136};
static const event_type T_00591980 = {0x00591980, 0, 0x009cbe2c, 1, -136};
static const event_type T_006ce540 = {0x006ce540, 0x0092fc60, 0x009cbda8, 1, -136};
static const event_type T_006ce660 = {0x006ce660, 0x0092fca0, 0x009d9c80, 1, -136};
static const event_type T_005c0bb0 = {0x005c0bb0, 0, 0x009d24bc, 1, -136};
static const event_type T_005c0cd0 = {0x005c0cd0, 0, 0x009d24d0, 1, -136};
static const event_type T_005c13e0 = {0x005c13e0, 0x0094c810, 0x009d28c4, 1, -136};
static const event_type T_005c1500 = {0x005c1500, 0, 0x009d256c, 1, -136};
static const event_type T_005c1620 = {0x005c1620, 0, 0x009d2514, 1, -136};
static const event_type T_005c1740 = {0x005c1740, 0, 0x009d2540, 1, -136};
static const event_type T_005c1860 = {0x005c1860, 0x0094c8a8, 0x009d2528, 1, -136};
static const event_type T_00586ca0 = {0x00586ca0, 0, 0x009cf3f0, 1, -136};
static void add_type(const event_type* t, rx_addr ch, rx_addr name) {
    if (t->name) MX_CALL(0, rx_event_type_register_at(ch, t->name, t->td, t->aligned, t->sp));
    else MX_CALL(1, rx_event_type_register_at(ch, name, t->td, t->aligned, t->sp));
}

/* 006c8fd0: the "Stunts" channel [0x00a94304] and its event types (VehicleLanded, StuntCompleted, ... : the names at the pushes) */
void rx_stunt_channel_create(void) {
    MX_FRAME_ALIGN(F, 8, 4, 3);
    rx_addr reg;
    MX_CALLR(reg, 0, rx_event_channel_registry());
    const rx_addr ch = channel_new();
    MX_CALL(2, rx_event_channel_register(reg, 0x009608ec /* "Stunts" */, ch));
    SU32(0, 0x00a94304, ch);
    add_type(&T_006ce210, ch, 0x0095ba28);
    add_type(&T_006ce210, U32(0, 0x00a94304), 0x00942784);
    add_type(&T_006ce320, U32(0, 0x00a94304), 0);
    add_type(&T_006ce430, U32(0, 0x00a94304), 0x009607a0);
    add_type(&T_006ce430, U32(0, 0x00a94304), 0x00942794);
    static const uint32_t names[6] = {0x00960770, 0x00960790, 0x00960780, 0x00960764, 0x00960758, 0x00960748};
    for (int k = 0; k < 6; k++) add_type(&T_006ce210, U32(0, 0x00a94304), names[k]);
    add_type(&T_005c0df0, U32(0, 0x00a94304), 0x009407c4);
    add_type(&T_005c0df0, U32(0, 0x00a94304), 0x00960738);
    MX_END(F);
}
RX_FUNCTION(0x006c8fd0, rx_stunt_channel_create)

/* 006cb870: the "StuntScoring" channel [0x00a94318] and its 8 event types */
void rx_stunt_scoring_channel_create(void) {
    MX_FRAME(F, 0, 2);
    rx_addr reg;
    MX_CALLR(reg, 0, rx_event_channel_registry());
    rx_addr ch;
    ch = channel_new_at(4);                                    /* push esi after the allocation */
    MX_CALL(2, rx_event_channel_register(reg, 0x009609ac /* "StuntScoring" */, ch));
    SU32(0, 0x00a94318, ch);
    add_type(&T_006ce540, ch, 0);
    add_type(&T_00591080, U32(0, 0x00a94318), 0x0092fc80);
    add_type(&T_006ce660, U32(0, 0x00a94318), 0);
    add_type(&T_005c0df0, U32(0, 0x00a94318), 0x0092fcc0);
    add_type(&T_005c0df0, U32(0, 0x00a94318), 0x0092fce0);
    add_type(&T_00591980, U32(0, 0x00a94318), 0x0092fd00);
    add_type(&T_00591080, U32(0, 0x00a94318), 0x0092fd40);
    add_type(&T_005c0df0, U32(0, 0x00a94318), 0x0092fd20);
    add_type(&T_005c0df0, U32(0, 0x00a94318), 0x0092fd60);
    MX_END(F);
}
RX_FUNCTION(0x006cb870, rx_stunt_scoring_channel_create)

/* 005ed6e0 (ECX = channel; name; RET 4) -> 005f2460 (thiscall channel, name; RET 4): a void(float) event type (descriptor 0x009d00dc)
 * under the channel's lock: the same steps as the other instantiations, at 005f2460's depth (type_info call at its entry - 0x130) */
void rx_channel_add_float_event(rx_addr ch, rx_addr name) {
    MX_FRAME(F, 0x118, 1);                                     /* push ebp; mov ebp, esp; sub esp, 0x118 */
    MX_CALL(1, rx_event_type_register_at(ch, name, 0x009d00dc, 0, -0x130));
    MX_END(F);
}
RX_FUNCTION(0x005ed6e0, rx_channel_add_float_event)

/* FUN_00596730 (ECX = slot): a save slot (0x70: four pointer vectors at +0x10 / +0x28 / +0x40 / +0x58 {proxy, +0xc first, +0x10
 * last, +0x14 end}, +0 .. +0xc zero) */
static void save_slot_construct(rx_addr s) {
    MX_FRAME(F, 0, 3);
    static const uint8_t v[4] = {0x10, 0x28, 0x40, 0x58};
    for (int k = 0; k < 4; k++) {
        const rx_addr vec = s + v[k];
        rx_addr p;
        MX_CALLR(p, 1, rx_reflex_aligned_alloc(4));
        if (p) SU32(p, 0, vec);
        SU32(vec, 0, p);
        SU32(vec, 0xc, 0); SU32(vec, 0x10, 0); SU32(vec, 0x14, 0);
    }
    SU32(s, 0, 0); SU32(s, 4, 0); SU32(s, 8, 0); SU32(s, 0xc, 0);
    MX_END(F);
}
/* FUN_005967d0 (stdcall slot, reset; RET 8): a slot cleared (+0 = 0 when reset, +4 = 0, the two owned objects +8 / +0xc deleted, the
 * four vectors' elements freed and erased). Only reached on fresh slots here: the vectors are empty. */
static void save_slot_clear(rx_addr s, uint32_t reset) {
    SU32(s, 0, reset ? 0 : U32(s, 0));
    SU32(s, 4, 0);
    if (U32(s, 8) || U32(s, 0xc) || U32(s, 0x50) != U32(s, 0x4c) || U32(s, 0x68) != U32(s, 0x64) || U32(s, 0x38) != U32(s, 0x34) ||
        U32(s, 0x20) != U32(s, 0x1c))
        mx_unreached(0x005967ec, "save slot with contents");
}

/* SavedGameManager_Construct 005a61b0 (the static object 0x00dce458 .. 0x00dce780, guard bit 0 of 0x00dcf2c4 at the callers): the
 * two OS events (+0 / +4), four save slots (0x00dce468, 0x70 each), the request list (0x00dce634 proxy, +0x640 .. +0x648), two
 * critical sections (0x00dce750 / 0x00dce768), the "StorageNotification" channel [0x00dce628] with its 17 event types. */
rx_addr rx_saved_game_manager_construct(void) {
    MX_FRAME(F, 0, 5);
    SU32(0, 0x00dce458, 0); SU32(0, 0x00dce45c, 0); SU32(0, 0x00dce460, 0);
    for (uint32_t k = 0; k < 4; k++) MX_CALL(0, save_slot_construct(0x00dce468 + 0x70 * k));   /* `vector constructor iterator' */
    rx_addr p;
    MX_CALLR(p, 1, rx_reflex_aligned_alloc(4));
    if (p) SU32(p, 0, 0x00dce634);
    SU32(0, 0x00dce634, p);
    SU32(0, 0x00dce640, 0); SU32(0, 0x00dce644, 0); SU32(0, 0x00dce648, 0);
    mx_cs_init(0x00dce750);
    mx_cs_init(0x00dce768);
    SU32(0, 0x00dce62c, 0); SU32(0, 0x00dce630, 0); SU8(0, 0x00dce64c, 0);
    for (uint32_t k = 0; k < 4; k++) MX_CALL(2, save_slot_clear(0x00dce468 + 0x70 * k, 1));
    rx_addr reg;
    MX_CALLR(reg, 0, rx_event_channel_registry());     /* the registry's static initializer (bit 0 of 0x00dce780), inline here */
    (void)reg;
    const rx_addr ch = channel_new();
    MX_CALL(2, rx_event_channel_register(0x00dcbeb4, 0x0094d3e4 /* "StorageNotification" */, ch));
    SU32(0, 0x00dce628, ch);
    struct { const event_type* t; uint32_t name; } ev[17] = {
        {&T_005c0bb0, 0x0094c7f0}, {&T_005c0cd0, 0x0094c804}, {&T_005c13e0, 0}, {&T_005c1500, 0x0094c81c},
        {&T_005c0cd0, 0x0094d3f8}, {&T_005c0cd0, 0x0094c830}, {&T_005c0cd0, 0x0094c83c}, {&T_005c1500, 0x0094c848},
        {&T_005c1620, 0x0094c85c}, {&T_005c1620, 0x0094c870}, {&T_005c1620, 0x0094c87c}, {&T_005c1740, 0x0094c888},
        {&T_005c1620, 0x0094c89c}, {&T_005c1860, 0}, {&T_005c1740, 0x0094c8b4}, {&T_005c0bb0, 0x0094d40c},
        {&T_00586ca0, 0x0094d428}};
    for (int k = 0; k < 17; k++) add_type(ev[k].t, U32(0, 0x00dce628), ev[k].name);
    SU32(0, 0x00dce458, mx_event_create(1, 0, 0x0094d43c /* "MXUISavedGameManager::EndEvent" */));
    SU32(0, 0x00dce45c, mx_event_create(1, 0, 0x0094d45c /* "MXUISavedGameManager::EndEventDone" */));
    { uint32_t a[1] = {U32(0, 0x00dce458)}; mx_os(0x00916090 /* ResetEvent */, 1, a); }
    { uint32_t a[1] = {U32(0, 0x00dce45c)}; mx_os(0x00916090, 1, a); }
    MX_END(F);
    return 0x00dce458;
}
RX_FUNCTION(0x005a61b0, rx_saved_game_manager_construct)

