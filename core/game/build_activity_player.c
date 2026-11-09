/* build_activity_player.c -- construction unit 8: the player slots (MXPlayer P, 0x12a0 bytes, MXGame + 0x7460 + 0x12a0 * slot;
 * build_activity.h): the slot allocation 00552ad0 and lookups 00552b20 / 00552b90, the player's initialisation 005490f0 with its
 * stunt system (P + 0xc70: 006ca680 / 006ca750 / 006caf20), its controls block (P + 0xb58: 006c8e80 / 006c9100) and its event
 * handlers (the session channel [0x00da6ca0], the stunt channel [0x00a94304]). The name plate (P + 0xa30, an
 * AnimatedTextIcon3dBillboard: render only) is made through act_render (oracle: the original; product: not built). */
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "build_rider.h"
#include "race.h"
#include "world_misc.h"
#include "race_misc.h"
#include "race_activity_misc.h"
#include "scene_fx_entities.h"
#include "loop_player.h"
#include "rider_cap.h"
#include <string.h>

/* render-only code (name plates / billboards): oracle the original, product nothing (not core: construction.md section 3) */
#ifdef RX_ORACLE
static inline uint32_t act_render_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a) { return rx_call_original(fn, conv, ecx, n, a); }
#else
static inline uint32_t act_render_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a) { (void)fn; (void)conv; (void)ecx; (void)n; (void)a; return 0; }
#endif

/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static uint32_t x87_float_copy(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }

/* ======================================================================================================================== */
/* event handlers (EventHandlerProxy, 0x14 bytes: vtable, +4 0, +8 the signature's type name, +0xc the object, +0x10 the callback;
 * vf1 008ad240 returns +8): appended to the event's handler list when the signature names match                               */

/* 005b9ff0 (EAX = &handler; list; RET 4): std::list<Handler*>::push_back (node 0xc: next, prev, value) */
void rx_event_handler_list_push_back(rx_addr list, rx_addr handler) {
    rx_addr head = U32(list, 0x14), prev = U32(head, 4);
    rx_addr node = rx_reflex_aligned_alloc(0xc);
    if (node) { SU32(node, 0, head); SU32(node, 4, prev); SU32(node, 8, handler); }
    if (0x3fffffffu - U32(list, 0x18) < 1) act_unreached(0x005ba000, "list<T> too long");
    SU32(list, 0x18, U32(list, 0x18) + 1);
    SU32(head, 4, node);
    SU32(prev, 0, node);
}
RX_FUNCTION(0x005b9ff0, rx_event_handler_list_push_back)
static void handler_list_push(rx_addr list, rx_addr handler) { rx_event_handler_list_push_back(list, handler); }
static void register_proxy(rx_addr channel, rx_addr name, rx_addr vtable, rx_addr type_info, rx_addr obj, uint32_t cb) {
    rx_addr ev;
    ACT_CALLR(ev, 2, rx_world_registry_find(channel, name));          /* 00593220 */
    rx_addr h = rx_reflex_aligned_alloc(0x14);
    if (h) {
        SU32(h, 4, 0); SU32(h, 0, vtable); SU32(h, 0xc, obj); SU32(h, 0x10, cb);
        SU32(h, 8, crt_type_info_name(type_info, 0x009e1a7c));        /* typeid(signature).name() */
    }
    if (crt_strcmp(U32(ev, 4), U32(h, 8)) == 0) handler_list_push(ev + 8, h);   /* (the event's critical section +0x24: one thread) */
}
/* the session's event channel [0x00da6ca0] (its static initialiser 006de1f0 under the guard 0x00dcf304 bit 0 has run at startup) */
static rx_addr session_channel(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x00549326, "the session's static initialiser has not run");
    return U32(0, 0x00da6ca0);
}

/* ======================================================================================================================== */
/* the stunt system (P + 0xc70, 0x43c bytes)                                                                                */

/* FUN_006ca980 (ECX = s): +0x84 .. +0x120 cleared */
static void stunt_clear_slots(rx_addr s) { for (uint32_t o = 0x84; o <= 0x120; o += 4) SU32(s, o, 0); }
/* FUN_006ca750 (ESI = s): the stunt system reset: both trick sets emptied, the stunt records cleared, "StuntSystemReset" raised on
 * the stunt event channel [0x00a94318] with the owner (s + 0x128), the counters and timers zeroed, five empty trick names */
void rx_stunt_system_reset(rx_addr s) {
    ACT_FRAME(L, 8, 3);
    SU32(s, 0x30c, 0); SU32(s, 0x124, 0); SU32(s, 0x2dc, 0); SF32(s, 0x2d0, 0.0f); SU32(s, 0x310, 0); SU32(s, 0x2d8, 0);
    SF32(s, 0x2d4, 0.0f); SU32(s, 0x5c, 0); SU32(s, 0x41c, 0); SU32(s, 0x420, 0); SF32(s, 0x58, 0.0f);
    for (uint32_t k = 0; k < 2; k++) {                                 /* the sets at +0x324 / +0x344: clear() */
        const rx_addr set = s + 0x324 + 0x20 * k;
        ACT_CALL(1, rx_rider_strset_erase(set, U32(U32(set, 0x18), 4)));   /* 00851f10 (_Erase(root)) */
        rx_addr h = U32(set, 0x18);
        SU32(h, 4, h); SU32(set, 0x1c, 0); SU32(h, 0, h); SU32(h, 8, h);
    }
    const rx_addr channel = U32(0, 0x00a94318);
    for (uint32_t o = 0x140; o < 0x2d0; o += 4) SU32(s, o, 0);
    SU32(L, 0x10, U32(s, 0x128));
    SU32(s, 0x2d8, 0); SU32(s, 0x418, 0);
    {   /* "StuntSystemReset" (void(MXPlayer*)) */
        uint32_t p[1] = {U32(s, 0x128)};
        race_raise_event(channel, 0x0092fd60, 0x009cbeb4, p, 1);
    }
    SF32(s, 0x314, 0.0f); SU32(s, 0x31c, 0);
    for (uint32_t o = 4; o <= 0x38; o += 4) SF32(s, o, 0.0f);
    SF32(s, 0x7c, 0.0f); SU32(s, 0x54, 1); SF32(s, 0x58, 0.0f); SF32(s, 0x78, 0.0f);
    SU32(s, 0x60, 0);
    for (uint32_t o = 0x64; o <= 0x74; o += 4) SF32(s, o, 0.0f);
    SU32(s, 0x3c, 0); SU32(s, 0x40, 0); SU32(s, 0x44, 0); SU32(s, 0x48, 0); SF32(s, 0x4c, 0.0f); SF32(s, 0x50, 0.0f);
    SU32(s, 0x41c, 0); SU32(s, 0x420, 0); SU32(s, 0x430, 0); SF32(s, 0x434, 0.0f);
    stunt_clear_slots(s);                                              /* 006ca980 */
    SU32(s, 0x424, 0); SU32(s, 0x428, 0); SU32(s, 0x42c, 0); SU32(s, 0x438, U32(0, 0x009cd9e8));   /* -1.0 (.data) */
    for (uint32_t i = 0; i < 5; i++) {
        SU32(L, 0x10, 5 - i);
        crt_strncpy(s + 0x364 + 0x20 * i, 0x00935d6c, 0x20);            /* "" */
        SU32(s, 0x404 + 4 * i, 0);
    }
    SU32(s, 0x320, 0); SU32(s, 0x80, 0); SF32(s, 0x308, 0.0f);
    ACT_END(L);
}
RX_FUNCTION(0x006ca750, rx_stunt_system_reset)
/* FUN_006ca680 (EAX = s): the stunt system's two sets (proxy, head 00690910), +0 / +0x128 cleared, then the reset */
void rx_stunt_system_construct(rx_addr s) {
    ACT_FRAME(L, 0, 3);
    for (uint32_t k = 0; k < 2; k++) {
        const rx_addr set = s + 0x324 + 0x20 * k;
        rx_addr px = rx_reflex_aligned_alloc(4);
        if (px) SU32(px, 0, set);
        SU32(set, 0, px);
        rx_addr h = rx_rider_map30_head_alloc();                       /* 00690910 */
        SU32(set, 0x18, h);
        SU8(h, 0x2d, 1); SU32(h, 4, h); SU32(h, 0, h); SU32(h, 8, h);
        SU32(set, 0x1c, 0);
    }
    SU32(s, 0, 0); SU32(s, 0x128, 0);
    ACT_CALL(0, rx_stunt_system_reset(s));                             /* 006ca750 */
    ACT_END(L);
}
/* FUN_006caf20 (EDI = s): the stunt system's handlers on the stunt channel [0x00a94304] */
static void stunt_system_register(rx_addr s) {
    const rx_addr ch = U32(0, 0x00a94304);
    /* 006cd8d0 (void(MXPlayer*, ...) signature 0x9cbf38, proxy 0x960ae4), 006cd990 ("ComboFromStunt", 0x9d9a04, 0x960b04, fixed
     * callback 006cb560), 006cda50 (0x9ce768, 0x960b24), 006cdb10 (0x9cbeb4, 0x960b44) */
    register_proxy(ch, 0x0095ba28, 0x00960ae4, 0x009cbf38, s, 0x006cb1b0);   /* "StuntBegin" */
    register_proxy(ch, 0x00942784, 0x00960ae4, 0x009cbf38, s, 0x006cb3c0);   /* "StuntCompleted" */
    register_proxy(ch, 0x009607b4, 0x00960b04, 0x009d9a04, s, 0x006cb560);   /* "ComboFromStunt" */
    register_proxy(ch, 0x00960764, 0x00960ae4, 0x009cbf38, s, 0x006cb6a0);   /* "StuntFailed" */
    register_proxy(ch, 0x00960770, 0x00960ae4, 0x009cbf38, s, 0x006cb660);   /* "StuntKickOut" */
    register_proxy(ch, 0x00960790, 0x00960ae4, 0x009cbf38, s, 0x005a4150);   /* "StuntHoldBegin" */
    register_proxy(ch, 0x00960780, 0x00960ae4, 0x009cbf38, s, 0x005a4150);   /* "StuntHoldEnd" */
    register_proxy(ch, 0x009607a0, 0x00960b24, 0x009ce768, s, 0x006cbbe0);   /* "PhysicsStuntBegin" */
    register_proxy(ch, 0x00942794, 0x00960b24, 0x009ce768, s, 0x006cbdb0);   /* "PhysicsStuntEnd" */
    register_proxy(ch, 0x00960758, 0x00960ae4, 0x009cbf38, s, 0x006cb5a0);   /* "StuntLanded" */
    register_proxy(ch, 0x00960748, 0x00960ae4, 0x009cbf38, s, 0x006cb610);   /* "StuntTakeoff" */
    register_proxy(ch, 0x009407c4, 0x00960b44, 0x009cbeb4, s, 0x006cc0d0);   /* "VehicleLanded" */
    register_proxy(ch, 0x00960738, 0x00960b44, 0x009cbeb4, s, 0x006cc1a0);   /* "VehicleTakeoff" */
}

/* ======================================================================================================================== */
/* the controls block (P + 0xb58)                                                                                           */

/* FUN_006c9100 (EAX = c): the controls' state reset */
void rx_controls_reset(rx_addr c) {
    SU32(c, 8, 0); SU32(c, 0x1c, 0); SU32(c, 0x20, 0); SU32(c, 0, 1); SU32(c, 0x88, 0); SU32(c, 0x84, 0); SU32(c, 0x64, 0); SU32(c, 0x68, 0);
    SU32(c, 0x18, 0); SU32(c, 0x90, 0); SF32(c, 0x14, 0.0f); SU32(c, 0xc, 0); SF32(c, 0x6c, 0.0f); SU32(c, 0x5c, 1);
    SU32(c, 0x38, 0xffffffffu); SU32(c, 0x30, 0);
    SU32(c, 0x44, x87_float_copy(U32(c, 0x48)));
    SU32(c, 0x3c, 0); SF32(c, 0x40, 0.0f); SU32(c, 0x4c, 0); SU8(c, 0x34, 0x4e); SU8(c, 0x35, 0x4e);
    SU32(c, 0x60, 0); SU32(c, 0x24, 0);
    SF32(c, 0x70, 0.0f); SF32(c, 0x74, 0.0f); SF32(c, 0x78, 0.0f); SF32(c, 0x7c, 0.0f);
    SU32(c, 0x28, 0);
}
RX_FUNCTION(0x006c9100, rx_controls_reset)
static void controls_reset(rx_addr c) { rx_controls_reset(c); }
static void pair8_ctor(rx_addr e) { SU32(e, 4, 0); SU32(e, 0, 0); }   /* 006c8470 (ECX = e) */
/* FUN_006c8e80 (ESI = c) */
static void controls_construct(rx_addr c) {
    SF32(c, 0x48, 1.0f /* 0x931f24 */);
    SU32(c, 0x38, 0xffffffffu); SU32(c, 0x30, 0);
    SU32(c, 0x44, x87_float_copy(U32(c, 0x48)));
    SU8(c, 0x34, 0x4e); SU8(c, 0x35, 0x4e);
    SU32(c, 0x3c, 0); SF32(c, 0x40, 0.0f); SU32(c, 0x4c, 0);
    ACT_CALL(4, rx_rider_vector_ctor(c + 0x98, 8, 0xf, pair8_ctor));   /* `vector constructor iterator' 004022d0 */
    SU32(c, 4, 0);
    controls_reset(c);                                                 /* 006c9100 */
    SU32(c, 0x94, 0);
}

/* ======================================================================================================================== */
/* MXPlayer                                                                                                                 */

/* FUN_005490f0 (stdcall P; RET 4) -> P: the player's initial state: MXPlayer vtable 0x941784, no slot / vehicle / rider / activity,
 * the name plate (P + 0xa30), the controls (P + 0xb58), the stunt system (P + 0xc70) and the player's handlers ("NetworkObject
 * Removed / Migrated", "NetworkStuntUpdate", "NetworkVehicleReset" on the session channel, the stunt events on [0x00a94304]), the
 * camera presets (P + 0x910 from 0x009320c0), the tuning defaults (+0x8f4 / +0x8f8 2), the reset timers. */
rx_addr rx_mxplayer_init(rx_addr P) {
    ACT_FRAME(L, 0x10, 4);
    SU32(P, 8, 0xffffffffu); SU32(P, 0x10, 0); SU8(P, 0x14, 0); SU8(P, 0x54, 0);
    SU32(P, 0, 0x00941784);
    SU32(P, 0x94, 0); SU32(P, 0x98, 0); SU32(P, 0x9c, 0x3e); SU32(P, 0x1ac, 0); SU32(P, 0x1b4, 0);
    SU32(P, 0x1e0, 0); SU32(P, 0x1e4, 0); SU32(P, 0x1e8, 0); SU32(P, 0x1f0, 0xffffffffu); SF32(P, 0x1f4, 5.0f /* 0x935890 */);
    SF32(P, 0x218, 0.0f); SU8(P, 0x21c, 0); SU32(P, 0x220, 0);
    SF32(P, 0x224, 0.0f); SF32(P, 0x228, 0.0f); SF32(P, 0x22c, 0.0f); SU32(P, 0x230, 0);
    for (uint32_t i = 0; i < 0x18; i++) { SU32(P, 0x234 + 0x24 * i, 0); SU8(P, 0x238 + 0x24 * i, 0); }
    for (uint32_t i = 0; i < 0x18; i++) { SU32(P, 0x594 + 0x24 * i, 0); SU8(P, 0x598 + 0x24 * i, 0); }
    SU32(P, 0x900, 0); SU32(P, 0x904, 0); SU32(P, 0x994, 0xffffffffu);
    SU32(P, 0x998, 0); SU32(P, 0x99c, 0); SU32(P, 0x9a0, 0); SU32(P, 0x9a4, 0); SU32(P, 0x9a8, 0);
    {   /* 006b62a0 (ESI = P + 0xa30): the name plate (render only) */
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[6] = P + 0xa30;
#ifdef RX_ORACLE
        rx_call_original_ex(0x006b62a0, &in, &o, 0, 0);
#else
        (void)o;
#endif
    }
    SU32(P, 0xb00, 0); SU32(P, 0xb24, 0); SU32(P, 0xb28, 0); SU32(P, 0xb2c, 0);
    controls_construct(P + 0xb58);                                     /* 006c8e80 */
    rx_stunt_system_construct(P + 0xc70);                              /* 006ca680 */
    SF32(P, 0x10b0, 0.0f); SU32(P, 0x10b4, 0);
    for (uint32_t o = 0x10b8; o <= 0x10c8; o += 4) SF32(P, o, 0.0f);
    SF32(P, 0x10d0, 0.0f); SF32(P, 0x10d4, 0.0f); SF32(P, 0x10d8, 1.0f /* 0x931f24 */);
    SF32(P, 0x10e8, 0.0f); SF32(P, 0x1104, 0.0f); SU32(P, 0x1138, 0); SF32(P, 0x1168, 0.0f); SF32(P, 0x116c, 0.0f);
    for (uint32_t o = 0x11c8; o <= 0x11d8; o += 4) SU32(P, o, 0);
    SU32(P, 0x1220, 1); SF32(P, 0x1290, 0.0f); SF32(P, 0x1294, 0.0f); SU32(P, 0x1160, 1); SU32(P, 0x1130, 0); SU32(P, 0x1134, 1);
    SF32(P, 0x1164, 0.0f);
    if (U8(0, 0x00a93f60) == 0) { SU32(0, 0x00a7a990, 0); SU8(0, 0x00a93f60, 1); }
    register_proxy(session_channel(), 0x0093ee98, 0x00941794, 0x009cdc7c, P, 0x0054b950);   /* "NetworkObjectRemoved" (00554570) */
    register_proxy(session_channel(), 0x0093eeb0, 0x00941794, 0x009cdc7c, P, 0x0054b9d0);   /* "NetworkObjectMigrated" */
    register_proxy(session_channel(), 0x00941354, 0x009417b4, 0x009ce5f4, P, 0x0054bb90);   /* "NetworkStuntUpdate" (00554630) */
    register_proxy(session_channel(), 0x00941368, 0x009417d4, 0x009ce674, P, 0x0054ff30);   /* "NetworkVehicleReset" (005546f0) */
    {
        const rx_addr ch = U32(0, 0x00a94304);                         /* the stunt channel: 005547b0 (0x9cbf38, proxy 0x9417f4) */
        register_proxy(ch, 0x0095ba28, 0x009417f4, 0x009cbf38, P, 0x0054d120);   /* "StuntBegin" */
        register_proxy(ch, 0x00960770, 0x009417f4, 0x009cbf38, P, 0x005a4150);   /* "StuntKickOut" */
        register_proxy(ch, 0x00960790, 0x009417f4, 0x009cbf38, P, 0x0054d1a0);   /* "StuntHoldBegin" */
        register_proxy(ch, 0x00960780, 0x009417f4, 0x009cbf38, P, 0x0054d1e0);   /* "StuntHoldEnd" */
        register_proxy(ch, 0x00942784, 0x009417f4, 0x009cbf38, P, 0x0054d220);   /* "StuntCompleted" */
        register_proxy(ch, 0x00960764, 0x009417f4, 0x009cbf38, P, 0x0054d2a0);   /* "StuntFailed" */
        register_proxy(ch, 0x00960758, 0x009417f4, 0x009cbf38, P, 0x0054d310);   /* "StuntLanded" */
        register_proxy(ch, 0x00960748, 0x009417f4, 0x009cbf38, P, 0x0054d360);   /* "StuntTakeoff" */
        register_proxy(ch, 0x009607a0, 0x00941814, 0x009ce768, P, 0x0083d480);   /* "PhysicsStuntBegin" (00554870) */
        register_proxy(ch, 0x00942794, 0x00941814, 0x009ce768, P, 0x0054d530);   /* "PhysicsStuntEnd" */
    }
    register_proxy(U32(0, 0x00a89bc8), 0x00935e84, 0x00941834, 0x009cbb7c, P, 0x0054f0b0);   /* "VehicleEntered" (00554930) */
    for (uint32_t k = 0; k < 0x64; k += 4) SU32(P, 0x910 + k, U32(0, 0x009320c0 + k));     /* the camera presets */
    SU32(P, 0x974, 0); SU32(P, 0x978, 0); SU32(P, 0x97c, 0); SU32(P, 0x980, 0); SU32(P, 0x984, 0);
    SU32(P, 0x1cc, 0); SU32(P, 0x1b0, 0xffffffffu); SU32(P, 0x1b8, 0xffffffffu); SU32(P, 0xa0, 0);
    crt_memset(P + 0xa4, 0, 0x104);
    SU32(P, 0x1a8, 0); SU32(P, 0x1bc, 0x3d); SU32(P, 0x1c4, 0xffffffffu); SU32(P, 0x1c0, 0xffffffffu); SU32(P, 0x1dc, 0xffffffffu);
    SU32(P, 0x1d0, 0); SU32(P, 0x1d4, 0); SU32(P, 0x1d8, 0); SF32(P, 0x1f8, 0.0f); SU32(P, 0x1fc, 0);
    SU8(P, 0x200, 0); SU8(P, 0x210, 0); SU32(P, 0x8fc, 0xffffffffu); SU8(P, 0x98c, 0); SF32(P, 0x990, 3.8f /* 0x977f18 */);
    SU32(P, 0x214, 1); SU32(P, 0x8f4, 2); SU32(P, 0x8f8, 2);
    controls_reset(P + 0xb58);                                         /* (inline in the original) */
    rx_stunt_system_reset(P + 0xc70);                                  /* 006ca750 */
    SU32(P, 0xb5c, P); SU32(P, 0xd98, P);
    stunt_system_register(P + 0xc70);                                  /* 006caf20 */
    SU32(P, 0x9ac, 0);
    {   /* 007ff4b0 (ECX = P + 0xa40; 0x40, 1; RET 8): the name plate's text (render only) */
        const uint32_t a[2] = {0x40, 1};
        act_render_call(0x007ff4b0, RX_THISCALL, P + 0xa40, 2, a);
    }
    SU32(P, 0x10e4, 0xffffffffu); SU32(P, 0x988, 1); SU32(P, 0x1ec, 0);
    SF32(P, 0x11a4, 0.0f); SF32(P, 0x11a8, 0.0f); SU32(P, 0x11a0, 0); SU32(P, 0x90c, 0);
    SU32(P, 0xb30, 0); SU32(P, 0xb34, 0); SU32(P, 0xb44, 0); SF32(P, 0xb3c, 0.0f); SF32(P, 0xb38, 0.0f); SF32(P, 0xb50, 0.0f); SF32(P, 0xb54, 0.0f);
    if (U32(P, 0x94)) act_unreached(0x0054976d, "player init with an activity");
    SU32(P, 0x11ac, 0xffffffffu); SU32(P, 0x11b0, 0xffffffffu); SF32(P, 0x11b4, 0.0f);
    ACT_END(L);
    return P;
}
RX_FUNCTION(0x005490f0, rx_mxplayer_init)

/* ======================================================================================================================== */
/* the slots (the player array A = MXGame + 0x7460: +0xdf80 12 used bytes, +0xdf8c the active slots, +0xdfbc their count)        */

/* FUN_00552ad0 (ECX = A) -> the new player: the first free slot marked used, listed, its player initialised */
rx_addr rx_player_slot_alloc(rx_addr A) {
    uint32_t i = 0;
    while (rx_seat_used(A, i) && (i += 1) < RX_RC_MAX) {}             /* (w-rc-core) 12 in the original: seats 12..31 in side storage */
    const uint32_t n = U8(A, 0xdfbc);
    rx_seat_set_order(A, n, i);
    SU8(A, 0xdfbc, n + 1);
    rx_seat_set_used(A, i, 1);
    rx_addr P = rx_seat(A, i);
    if (!P) act_unreached(0x00552ae7, "slot 0 of a null array");
    ACT_CALL(1, rx_mxplayer_init(P));                                  /* 005490f0 */
    return P;
}
/* FUN_00552b20 (ESI = A; name; RET 4) -> the active player of that name (P + 0x14, _stricmp) or 0 */
rx_addr rx_player_find_by_name(rx_addr A, rx_addr name) {
    for (uint32_t k = 0; k < U8(A, 0xdfbc); k++) {
        rx_addr P = rx_seat_active(A, k);
        if (crt_stricmp(name, P + 0x14) == 0) return P;
    }
    return 0;
}
RX_FUNCTION(0x00552b20, rx_player_find_by_name)
/* FUN_00552b90 (EDX = A; index, kind; RET 8) -> the index-th active player (kind 3: the slot's player) */
rx_addr rx_player_active(rx_addr A, uint32_t index, uint32_t kind) {
    uint32_t slot = rx_seat_order(A, index);
    if (slot == 0xffffffffu || kind != 3) act_unreached(0x00552ba5, "player lookup of an empty slot / kind");
    return rx_seat(A, slot);
}

/* ======================================================================================================================== */
/* the Player List row setters (the player's current activity / vehicle / rider: requested -> current)                        */

/* FUN_004ddf30 (ESI = row; type, area (char); RET 8): ActivityType / ActivityArea = the arguments, RequestedActivityType 0x3d,
 * RequestedActivityArea -1; every changed cell counts in the Player List's change counter (0x00ae0e70). The original reuses its
 * argument slots for the values. */
void rx_player_list_set_activity(uint32_t row, uint32_t type, uint32_t area) {
    ACT_FRAME(L, 0, 3);
    const rx_addr s0 = L + 0x10, s1 = L + 0x14;
    SU32(s0, 0, type);
    if (type != act_cell(ACT_PLAYER_LIST, 0x0093dd90, row)) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);   /* "ActivityType" */
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, 0x0093dd90), row, s0));
    const uint32_t a = (uint32_t)(int32_t)(int8_t)area;
    SU32(s1, 0, a);
    if (a != act_cell(ACT_PLAYER_LIST, 0x0093dda0, row)) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);   /* "ActivityArea" */
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, 0x0093dda0), row, s1));
    SU32(s1, 0, 0x3d);
    if (act_cell(ACT_PLAYER_LIST, 0x0093df2c, row) != 0x3d) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);   /* "RequestedActivityType" */
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, 0x0093df2c), row, s1));
    SU32(s1, 0, 0xffffffffu);
    if (act_cell(ACT_PLAYER_LIST, 0x0093df44, row) != 0xffffffffu) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);   /* "RequestedActivityArea" */
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, 0x0093df44), row, s1));
    ACT_END(L);
}
/* FUN_004de110 / FUN_004de230 (ESI = row; id; RET 4): VehicleId / RiderId = the id, RequestedVehicleId / RequestedRiderId -1 */
static void set_current(uint32_t row, uint32_t id, rx_addr cur, rx_addr req) {
    ACT_FRAME(L, 0, 2);
    const rx_addr s0 = L + 0xc;
    SU32(s0, 0, id);
    if (id != act_cell(ACT_PLAYER_LIST, cur, row)) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, cur), row, s0));
    SU32(s0, 0, 0xffffffffu);
    if (act_cell(ACT_PLAYER_LIST, req, row) != 0xffffffffu) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, req), row, s0));
    ACT_END(L);
}
void rx_player_list_set_vehicle(uint32_t row, uint32_t vid) { set_current(row, vid, 0x00936cb4, 0x0093df5c); }   /* 004de110 */
void rx_player_list_set_rider(uint32_t row, uint32_t rid) { set_current(row, rid, 0x0093dec8, 0x0093df70); }     /* 004de230 */

/* ======================================================================================================================== */
/* PlayerSlot_FromPlayerList 0054a7b0 (stdcall P, &row ref; RET 8)                                                          */

/* network-only code (the session's players): oracle the original, product the offline result */
static uint32_t act_session_player(uint32_t unique_id) {   /* FUN_006e3030 (EAX = unique id; the session [0x00da6cb0]; RET 4) */
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = unique_id;
    const uint32_t a[1] = {U32(0, 0x00da6cb0)};
    rx_call_original_ex(0x006e3030, &in, &o, 1, a);
    return o.r[0];
#else
    (void)unique_id;
    return 0;
#endif
}
/* the player's fields from its Player List row: names, local / type / viewport / unique id / team / spectator; for a local player
 * its controller (the controller's key maps 00547f30 from the remapping table [0x00d92efc], the PlayerControllerSetting camera
 * parameters, the driving camera 0054aef0); start zone, ready / disconnected / loaded, the camera kind (+0x988), the network player,
 * the requested activity / vehicle / rider (made current in the row: 004ddf30 / 004de110 / 004de230), the AI difficulty bucket
 * (+0x8f4) and a random AI personality (+0x8f8 = rand % 3: the game's generator), the network flag (0054e110), the name plate
 * (render only) and its matrices. */
void rx_player_from_player_list(rx_addr P, rx_addr ref) {
    ACT_FRAME(L, 0x9c, 4);
    const uint32_t row = U32(ref, 0);
    rx_addr t = U32(ref, 8);
    SU32(L, 0x14, row);
    act_strcpy(P + 0x14, rx_tbl_str_cstr(rx_tbl_cell_string(act_column(t, 0x00936f00), row)));   /* "PlayerName" */
    t = U32(ref, 8);
    act_strcpy(P + 0x54, rx_tbl_str_cstr(rx_tbl_cell_string(act_column(t, 0x0093deac), row)));   /* "DisplayName" */
    SU32(P, 4, act_cell(U32(ref, 8), 0x0093de90, row));               /* "PlayerLocal" */
    SU32(P, 0x10, act_cell(U32(ref, 8), 0x00936ef4, row));            /* "PlayerType" */
    SU32(P, 8, act_cell(U32(ref, 8), 0x0093dedc, row));               /* "Viewport" */
    SU32(P, 0xc, act_cell(U32(ref, 8), 0x0093df0c, row));             /* "UniqueId" */
    SU32(P, 0x1fc, act_cell(U32(ref, 8), 0x0093df18, row));           /* "Team" */
    SU32(P, 0x908, act_cell(U32(ref, 8), 0x0093df20, row));           /* "Spectator" */
    if (I32(P, 0x1f0) >= 0) { SU8(0, 0x00a7a990 + U32(P, 0x1f0), 0); SU32(P, 0x1f0, 0xffffffffu); }
    if (U32(P, 4)) {
        const int32_t ctl = (int32_t)act_cell(U32(ref, 8), 0x0093ded0, row);   /* "Controller" */
        SU32(P, 0x1f0, (uint32_t)ctl);
        if (ctl >= 0) {
            const rx_addr remap = U32(0, 0x00d92efc), rec = remap + 0x48 * (uint32_t)ctl;
            const uint32_t a[4] = {(uint32_t)ctl, U32(rec, 0x18), U32(rec, 0x30), U32(rec, 0x2c)};
            act_call_fn(0x00547f30, RX_CDECL, 0, 4, a);   /* Controller_BuildKeyMaps (build_activity_controller.c) */
            SU8(0, 0x00a7a990 + U32(P, 0x1f0), 1);
            const rx_addr pcs = rx_tbl_find(0x00ae10f0u + 0x1f4u * U32(P, 0x1f0), 0x00941650);   /* "PlayerControllerSetting" */
            if (pcs) {
                SU32(P, 0x224, x87_float_copy(act_cell(pcs, 0x00941668, 0)));   /* "CameraParam1" (fld / fstp) */
                SU32(P, 0x228, x87_float_copy(act_cell(pcs, 0x00941678, 0)));   /* "CameraParam2" */
                SU32(P, 0x22c, x87_float_copy(act_cell(pcs, 0x00941688, 0)));   /* "CameraParam3" */
            }
            /* Apply the actual saved DrivingCamera through the ported selector. */
            ACT_CALL(0, (void)rx_player_apply_driving_camera(P));
        }
    }
    SU32(P, 0x1dc, act_cell(U32(ref, 8), 0x0093df00, row));           /* "StartZone" */
    SU32(P, 0x1e0, act_cell(U32(ref, 8), 0x0093de38, row));           /* "Ready" */
    SU32(P, 0x1e4, act_cell(U32(ref, 8), 0x0093dee8, row));           /* "Disconnected" */
    SU32(P, 0x1e8, act_cell(U32(ref, 8), 0x0093de28, row));           /* "Loaded" */
    SF32(P, 0x218, 0.0f);
    const uint32_t ctl = U32(P, 0x1f0);
    SU32(P, 0x988, (U32(P, 0x10) == 0 && ctl < 4) ? U32(0, 0x009c5410 + 4 * ctl) : 1);   /* the camera kind per controller */
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x0054ab69, "the session's static initialiser has not run");
    SU32(P, 0xb2c, act_session_player(U32(P, 0xc)));                   /* 006e3030 */
    SU32(P, 0x1bc, act_cell(U32(ref, 8), 0x0093df2c, row));           /* "RequestedActivityType" */
    SU32(P, 0x1c4, act_cell(U32(ref, 8), 0x0093df5c, row));           /* "RequestedVehicleId" */
    SU32(P, 0x1c0, act_cell(U32(ref, 8), 0x0093df70, row));           /* "RequestedRiderId" */
    const uint8_t area = (uint8_t)act_cell(U32(ref, 8), 0x0093df44, row);   /* "RequestedActivityArea" (movzx byte) */
    SU8(P, 0x1c8, area);
    ACT_CALL(2, rx_player_list_set_activity(U32(ref, 0), U32(P, 0x1bc), area));   /* 004ddf30 */
    ACT_CALL(1, rx_player_list_set_vehicle(U32(ref, 0), U32(P, 0x1c4)));          /* 004de110 */
    ACT_CALL(1, rx_player_list_set_rider(U32(ref, 0), U32(P, 0x1c0)));            /* 004de230 */
    if (U32(P, 0x1b0) == U32(P, 0x1c4)) SU32(P, 0x1c4, 0xffffffffu);
    if (U32(P, 0x1b8) == U32(P, 0x1c0)) SU32(P, 0x1c0, 0xffffffffu);
    SU32(P, 0x8f4, act_cell(U32(ref, 8), 0x00936e48, U32(ref, 0)));   /* "AIDifficulty" */
    {   /* the game's generator: rand % 3 (idiv of a non-negative value) */
        uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
        SU32(0, 0x009cb860, seed);
        SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
        SU32(P, 0x8f8, ((seed >> 16) & 0x7fff) % 3);
    }
    {   /* FUN_0054e110 (ESI = P): +0x9ac = online play (player mode 2 / 3) and the controller's remapping record says so */
        uint32_t pm;
        ACT_CALLR(pm, 0, rx_game_player_mode());
        uint32_t on = (pm == 2 || pm == 3);
        SU32(P, 0x9ac, on);
        if (on) act_unreached(0x0054e132, "online player setup");
    }
    const rx_addr text = U32(P, 0xaa8);
    if (text) {   /* the name plate's text (render only): the font "ParalucentCondDemBol48", the display name, flags */
#ifdef RX_ORACLE
        if (U32(text, 0x11c) != 1) {
            if (U32(text, 0x120)) { rx_reflex_aligned_free(U32(text, 0x120)); SU32(text, 0x120, 0); }
            SU32(text, 0x11c, 1);
            rx_regs in, o; memset(&in, 0, sizeof in); in.r[6] = text;
            rx_call_original_ex(0x00402720, &in, &o, 0, 0);
        }
        uint32_t mgr = rx_call_original(0x007fb950, RX_CDECL, 0, 0, 0);
        const uint32_t nm[1] = {0x00941698};
        uint32_t font = rx_call_original(0x007fb8e0, RX_THISCALL, mgr, 1, nm);
        if (font) font = U32(font, 0);
        const uint32_t fa[1] = {font};
        rx_call_original(0x007ff120, RX_THISCALL, U32(P, 0xaa8), 1, fa);
        const rx_addr t2 = U32(P, 0xaa8);
        if (!(F32(t2, 0x118) == 1.5f /* 0x9322a4 */)) SF32(t2, 0x118, 1.5f);
        const rx_addr wbuf = L + 0x18;
        const uint32_t mb[6] = {0xfde9, 0, P + 0x54, 0xffffffffu, wbuf, 0x44};
        rx_call_original(U32(0, 0x009160a4), RX_STDCALL, 0, 6, mb);   /* MultiByteToWideChar */
        SU16(wbuf, 0x86, 0);
        uint32_t len = 0;
        while (U16(wbuf, 2 * len)) len++;
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[6] = U32(P, 0xaa8);
        const uint32_t sa[2] = {wbuf, len};
        rx_call_original_ex(0x004029a0, &in, &o, 2, sa);
        const rx_addr t3 = U32(P, 0xaa8);
        SU8(t3, 0xfa, U8(t3, 0xfa) | 2);
        SU32(t3, 0xdc, 1); SU32(t3, 0xe0, 1);
#else
        act_unreached(0x0054ace6, "the name plate text in the product");
#endif
    }
    if (!(U8(0, 0x00dcf3a0) & 1)) {                                    /* the name plate offset (a static vector) */
        SU32(0, 0x00dcf3a0, U32(0, 0x00dcf3a0) | 1);
        SF32(0, 0x00dcf390, 0.0f); SF32(0, 0x00dcf394, 2.0f /* 0x92e330 */); SF32(0, 0x00dcf398, 0.0f);
    }
    for (uint32_t m = 0; m < 2; m++)                                   /* two identity matrices (+0x9b0, +0x9f0) */
        for (uint32_t i = 0; i < 16; i++) SF32(P, 0x9b0 + 0x40 * m + 4 * i, (i % 5 == 0) ? 1.0f : 0.0f);
    SU32(P, 0xad8, P + 0x9f0); SU32(P, 0xad4, P + 0x9b0); SU32(P, 0xad0, 0x00dcf390);
    ACT_END(L);
}
RX_FUNCTION(0x0054a7b0, rx_player_from_player_list)

/* ======================================================================================================================== */
/* the players from the Player List (FUN_0075ba20)                                                                           */

/* the inlined strcmp (byte loop): 0 when equal */
static int act_strcmp(rx_addr a, rx_addr b) {
    for (uint32_t i = 0;; i++) { uint8_t x = U8(a, i), y = U8(b, i); if (x != y) return x < y ? -1 : 1; if (!x) return 0; }
}
static uint32_t player_rows(void) {
    return U32(ACT_PLAYER_LIST, 0x14) == U32(ACT_PLAYER_LIST, 0x10) ? 0 : rx_tbl_column_rows(U32(U32(ACT_PLAYER_LIST, 0x10), 0));
}
/* FUN_0075ba20 (stdcall game; RET 4) -> 1 (0 when the Player List did not change since the last call: [0x00ae0e70] == game + 0x15b30):
 * the players whose row went (or is inactive) leave; every active row without a player gets a new slot (00552ad0) filled from its
 * row (0054a7b0); a row with a player gets its requested activity / vehicle / rider made current and its start zone, unique id,
 * ready / disconnected / loaded, spectator and team copied. Then the viewports for the number of players with a viewport
 * (00757660: the views and cameras, not ported here), "InitViewportContainer" when that changed, and the revision remembered. */
uint32_t rx_players_sync(rx_addr game) {
    if (U32(0, 0x00ae0e70) == U32(game, 0x15b30)) return 0;
    ACT_FRAME_EBP(L, 8, 0x2c, 3);
    {   /* FUN_0084ea00 (EDI = the engine [[0x00a95844] + 0x10]): the engine's job sync (JobPool_WaitIdle, the frame events), ported
         * as rx_engine_frame_mark (build_activity_frame.c) */
        const rx_addr fobj = U32(U32(0, 0x00a95844), 0x10);
        ACT_CALL(0, rx_engine_frame_mark(fobj));
    }
    const rx_addr A = game + 0x7460;
    SU32(L, 0x14, 0);
    SU32(L, 0x10, (uint32_t)((int32_t)U8(game, 0x1541c) - 1));
    if ((int32_t)U8(game, 0x1541c) - 1 >= 0) SU32(L, 0x18, A);
    for (int32_t k = (int32_t)U8(game, 0x1541c) - 1; k >= 0; k--) {
        SU32(L, 0x10, (uint32_t)k);
        rx_addr P;
        ACT_CALLR(P, 2, rx_player_active(A, (uint32_t)k, 3));         /* 00552b90 */
        SU32(L, 0x1c, P);
        uint32_t r = 0;
        for (; r < player_rows(); r++)
            if (act_strcmp(rx_tbl_str_cstr(rx_tbl_cell_string(act_column(ACT_PLAYER_LIST, 0x00936f00), r)), P + 0x14) == 0) break;
        if (r == player_rows() || act_cell(ACT_PLAYER_LIST, 0x0093def8, r) == 0) {   /* gone or inactive ("Active") */
            const rx_addr pact = U32(P, 0x94);
            if (pact != 0) {                                                /* leaves its activity: its driver (0055a050 -> the slot) */
                int32_t slot;
                ACT_CALLR(slot, 0, rx_world_group_rider_index(pact, P));
                const rx_addr dlist = U32(P, 0x908) != 0 ? U32(pact, 0x27c) : U32(pact, 0x268);
                ACT_CALL(2, rx_activity_on_player_left(pact, U32(dlist, 4u * (uint32_t)slot)));   /* 0055c1d0 (race_activity_misc.c) */
            }
            ACT_CALL(0, rx_player_slot_free(A, P));                         /* 00552c60 (EAX = player, ESI = the block) */
        }
    }
    for (uint32_t r = 0; r < player_rows(); r++) {
        SU32(L, 0x1c, 0x1c * r);
        SU32(L, 0x2c, r); SU32(L, 0x30, 0); SU32(L, 0x34, ACT_PLAYER_LIST);   /* the row reference */
        const rx_addr name = rx_tbl_str_cstr(rx_tbl_cell_string(act_column(ACT_PLAYER_LIST, 0x00936f00), r));
        rx_addr P;
        ACT_CALLR(P, 1, rx_player_find_by_name(A, name));              /* 00552b20 */
        SU32(L, 0x10, P);
        if (!P) {
            if (act_cell(ACT_PLAYER_LIST, 0x0093def8, r) == 0) continue;   /* "Active" */
            ACT_CALLR(P, 0, rx_player_slot_alloc(A));                  /* 00552ad0 */
            ACT_CALL(2, rx_player_from_player_list(P, L + 0x2c));      /* 0054a7b0 */
        } else {
            const uint32_t type = act_cell(ACT_PLAYER_LIST, 0x0093df2c, r);   /* "RequestedActivityType" */
            SU32(L, 0x24, type);
            SU8(L, 0x18, (uint8_t)act_cell(ACT_PLAYER_LIST, 0x0093df44, r));   /* "RequestedActivityArea" */
            const uint32_t veh = act_cell(ACT_PLAYER_LIST, 0x0093df5c, r);    /* "RequestedVehicleId" */
            SU32(L, 0x20, veh);
            const uint32_t rid = act_cell(ACT_PLAYER_LIST, 0x0093df70, r);    /* "RequestedRiderId" */
            SU32(L, 0x28, rid);
            if (type != 0x3d || (int8_t)U8(L, 0x18) > -1) {
                SU32(P, 0x1bc, type); SU8(P, 0x1c8, U8(L, 0x18));
                ACT_CALL(2, rx_player_list_set_activity(r, type, U32(L, 0x18)));   /* 004ddf30 */
            }
            if ((int32_t)veh > -1) {
                SU32(P, 0x1c4, veh);
                ACT_CALL(1, rx_player_list_set_vehicle(r, veh));       /* 004de110 */
                if (U32(P, 0x1b0) == U32(P, 0x1c4)) SU32(P, 0x1c4, 0xffffffffu);
            }
            if ((int32_t)rid > -1) {
                SU32(P, 0x1c0, rid);
                ACT_CALL(1, rx_player_list_set_rider(r, rid));         /* 004de230 */
                if (U32(P, 0x1b8) == U32(P, 0x1c0)) SU32(P, 0x1c0, 0xffffffffu);
            }
            const uint32_t sz = act_cell(ACT_PLAYER_LIST, 0x0093df00, r);     /* "StartZone" */
            if (sz != 0xffffffffu) SU32(P, 0x1dc, sz);
            const uint32_t uid = act_cell(ACT_PLAYER_LIST, 0x0093df0c, r);    /* "UniqueId" */
            if (uid != 0xffffffffu) SU32(P, 0xc, uid);
            if (U32(P, 4)) SU32(P, 0x1e0, act_cell(ACT_PLAYER_LIST, 0x0093de38, r));   /* "Ready" */
            SU32(P, 0x1e4, act_cell(ACT_PLAYER_LIST, 0x0093dee8, r));          /* "Disconnected" */
            SU32(P, 0x1e8, act_cell(ACT_PLAYER_LIST, 0x0093de28, r));          /* "Loaded" */
            SU32(P, 0x908, act_cell(ACT_PLAYER_LIST, 0x0093df20, r));          /* "Spectator" */
            SU32(P, 0x1fc, act_cell(ACT_PLAYER_LIST, 0x0093df18, r));          /* "Team" */
        }
        if (P && U32(P, 8) != 0xffffffffu) SU32(L, 0x14, U32(L, 0x14) + 1);
    }
    int32_t n = I32(L, 0x14);
    if (n <= 1) n = 1;
    SU32(L, 0x14, (uint32_t)n);
    uint32_t changed;
    {
        const uint32_t a[2] = {game, (uint32_t)n};
#ifdef RX_ORACLE
        ACT_CALLR(changed, 2, rx_call_original(0x00757660, RX_STDCALL, 0, 2, a));   /* the viewports / cameras: oracle */
#else
        (void)a; changed = 0;
        /* PRODUCT BOUNDARY for 00757660's render targets / cameras (the camera unit's); its viewport list is built because the FX
         * distance gate and the emission count read it (scene_viewports.c). The caller's revision branch still runs on +0x15b28. */
        rx_scene_viewports_create(game, (uint32_t)n);
#endif
    }
    if (changed || U32(game, 0x15b28) != (uint32_t)n) {
        const rx_addr ctx = U32(0, 0x00a95844);
        if (U32(U32(ctx, 0x20), 0x34) == 1) act_unreached(0x0075c3c1, "the viewports with the second scene active");
        if (U32(U32(ctx, 0x1c), 0x34) == 1 && U32(game, 0x20c8)) {
            uint32_t none[2] = {0, 0};
            race_raise_event(U32(0, 0x00a93f74), 0x00941ef4, 0x009cf3f0, none, 0);   /* "InitViewportContainer" */
        }
        SU32(game, 0x15b28, (uint32_t)n);
    }
    SU32(game, 0x15b30, U32(0, 0x00ae0e70));
    ACT_END(L);
    return 1;
}
RX_FUNCTION(0x0075ba20, rx_players_sync)
