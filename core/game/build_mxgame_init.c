/* build_mxgame_init.c -- unit 10: the callees of MXGame::Startup that build gameplay state (build_mxgame.h): the global channels
 * "ActivityManagerGlobalEvents" [0x00a93f74] (0055ed50) and "VehicleTrigger" / its twin [0x00a89bc8] / [0x00a89bcc] (00409690, with the
 * function handler 00419700), the registry-driven tuning globals (006c8ee0, 006caa80, 00549b30, 0051c620) and the stunt judging
 * parameters (006cc350 from FMXJudgingParameters.bxml). */
#include "build_mxgame_int.h"
#include "build_activity.h"
#include "build_xml.h"
#include "build_stream.h"
#include "world_misc.h"
#include "race_driver.h"
#include "race_misc.h"
#include "../src/fmt_vfs.h"
#include <string.h>

/* ---- the registry readers ---- */
typedef uint32_t (*reg_fn3)(rx_addr, uint32_t, uint32_t, uint32_t);
static uint32_t reg3(rx_addr reg, uint32_t slot, uint32_t a, uint32_t b, uint32_t c) {
#ifdef RX_ORACLE
    if (getenv("RX_B10_DEBUG")) fprintf(stderr, "reg3 reg %08x vt %08x slot %u fn %08x args %08x %08x %08x [reg+4] %08x [reg+8] %08x\n",
        reg, U32(reg, 0), slot, U32(U32(reg, 0), 4 * slot), a, b, c, U32(reg, 4), U32(reg, 8));
#endif
    reg_fn3 f = RX_FN(U32(U32(reg, 0), 4 * slot), reg_fn3);
    if (!f) mx_unreached(U32(U32(reg, 0), 4 * slot), "Registry virtual not ported");
    uint32_t r;
    MX_CALLR(r, 3, f(reg, a, b, c));
    return r;
}
/* one value: the record of path\key (vf1), then the slot's reader (vf8 bools, vf14 vec3, vf17 floats, vf19 ints) into dst when found */
typedef struct { uint32_t path, key, slot, dst; } reg_read;
static void reg_read_one(rx_addr reg, const reg_read* r) {
    const rx_addr rec = reg3(reg, 1, r->path, r->key, 1);
    if (rec) reg3(reg, r->slot, rec, r->dst, 1);
}

/* 006c8ee0: MXGame\Stunts (AerialStuntFailureOnLandingDisabled 0 when absent) */
void rx_stunt_settings_read(void) {
    MX_FRAME(F, 0, 1);
    const rx_addr reg = U32(U32(0, 0x00a95844), 0xc);
    static const reg_read t[5] = {
        {0x960884, 0x960860, 8, 0xa94308},   /* AerialStuntFailureOnLandingDisabled */
        {0x960884, 0x960894, 17, 0x9c3e24},  /* GroundStuntMinimumSpeed */
        {0x960884, 0x9608ac, 17, 0x9c3e1c},  /* WheelieTriggerTime */
        {0x960884, 0x9608c0, 17, 0x9c3e20},  /* StoppieTriggerTime */
        {0x960884, 0x9608d4, 17, 0x9c3e2c}}; /* WheelieAirborneTimeout */
    {
        const rx_addr rec = reg3(reg, 1, t[0].path, t[0].key, 1);
        if (rec) reg3(reg, 8, rec, t[0].dst, 1); else SU32(0, 0x00a94308, 0);
    }
    for (int k = 1; k < 5; k++) reg_read_one(reg, &t[k]);
    MX_END(F);
}
RX_FUNCTION(0x006c8ee0, rx_stunt_settings_read)

/* 006cc2e0 (ESI = reader; name; cdecl): the float attribute "value" of the root's first child `name` (vf1, vf8 with a cursor -1,
 * vf58, vf76), 0.0f when absent (xmm0) */
static float judging_value(rx_addr reader, rx_addr name) {
    MX_FRAME(F, 8, 0);
    SF32(F, 0, 0.0f);
    rx_addr root, node;
    MX_CALLR(root, 0, rx_xml_vf_root(reader));
    SU32(F, 4, 0xffffffffu);
    MX_CALLR(node, 3, rx_xml_vf_first(reader, root, name, F + 4));
    if (node) {
        rx_addr attr;
        MX_CALLR(attr, 2, rx_xml_vf_attr(reader, node, 0x0094fab4 /* "value" */));
        if (attr) MX_CALL(3, rx_xml_vf_attr_floats(reader, attr, F, 1));
    }
    const float v = F32(F, 0);
    MX_END(F);
    return v;
}
/* The session's install (set by rx_mxgame_startup through rx_mxgame_init_set_install): the file reads of MXGame::Startup. */
static fmt_install* g_in;
/* 006cc350: the stunt judging parameters of FMXJudgingParameters.bxml (an XMLReader on the frame, FUN_00825260's file load, the
 * values by name into 0x9c3e44 .. 0x9c3e74, FUN_00827e00 / 00824fe0 the reader released) */
void rx_judging_parameters_load(void) {
    MX_FRAME_ALIGN(F, 8, 0x46c, 1);
    SU32(F, 0x46c, U32(0, 0x009b45e4) ^ (F + 4));
    const rx_addr reader = F + 8;
    MX_CALL(1, rx_xml_reader_construct(reader));                 /* (ESI = reader; the allocator 0) */
#ifdef RX_ORACLE
    /* the boot runs the original MXGame::Startup: the install is not known here, so the oracle's rx_xml_reader_load_file reads
     * the file through the original CRT (the boot's C:\Reflex mapping); see build_xml.c */
    const int ok = rx_xml_reader_load_file(NULL, reader, "FMXJudgingParameters.bxml");   /* FUN_00825260 */
#else
    const int ok = g_in ? rx_xml_reader_load_file(fmt_install_vfs(g_in), reader, "FMXJudgingParameters.bxml") : 0;
#endif
    if (ok) {
        static const struct { uint32_t name, dst; } v[12] = {
            {0x9609bc, 0x9c3e44}, {0x9609cc, 0x9c3e48}, {0x9609e4, 0x9c3e4c}, {0x960a00, 0x9c3e54}, {0x960a18, 0x9c3e58},
            {0x960a2c, 0x9c3e5c}, {0x960a3c, 0x9c3e60}, {0x960a50, 0x9c3e64}, {0x960a64, 0x9c3e68}, {0x960a78, 0x9c3e6c},
            {0x960a88, 0x9c3e70}, {0x960aa0, 0x9c3e74}};
        for (int k = 0; k < 12; k++) { float x; MX_CALLR(x, 1, judging_value(reader, v[k].name)); SF32(0, v[k].dst, x); }
    }
    rx_xml_reader_destroy(reader);
    MX_END(F);
}

/* 006caa80: MXGame\Stunts InitialLinkAllowedTime / LinkTimerScale, then the judging parameters (006cc350) */
void rx_stunt_link_settings_read(void) {
    MX_FRAME(F, 0, 1);
    const rx_addr reg = U32(U32(0, 0x00a95844), 0xc);
    static const reg_read t[2] = {{0x960884, 0x960984, 17, 0x9c3e3c}, {0x960884, 0x96099c, 17, 0x9c3e40}};
    for (int k = 0; k < 2; k++) reg_read_one(reg, &t[k]);
    MX_END(F);
    rx_judging_parameters_load();                               /* jmp 006cc350 */
}
RX_FUNCTION(0x006caa80, rx_stunt_link_settings_read)

/* 00549b30: MXGame\EnvironmentalScreenEffects (mud splatter), MXGame\RaceLine and MXGame\PlayerReset. The RaceLine / PlayerReset
 * floats go through a frame local (stored to the global whether or not the key exists: the local keeps the previous value) */
void rx_environment_settings_read(void) {
    MX_FRAME(F, 0, 2);
    const rx_addr reg = U32(U32(0, 0x00a95844), 0xc);
    static const reg_read mud[13] = {
        {0x9413a0, 0x94137c, 17, 0x9c5364}, {0x9413a0, 0x9413c4, 17, 0x9c53e8}, {0x9413a0, 0x9413e8, 17, 0x9c53ec},
        {0x9413a0, 0x941418, 17, 0x9c53f0}, {0x9413a0, 0x941448, 8, 0xa93f68}, {0x9413a0, 0x941464, 17, 0xa93f6c},
        {0x9413a0, 0x941484, 19, 0x9c53f4}, {0x9413a0, 0x9414a8, 17, 0x9c53fc}, {0x9413a0, 0x9414c8, 17, 0x9c53f8},
        {0x9413a0, 0x9414e8, 17, 0x9c5400}, {0x9413a0, 0x94150c, 17, 0x9c5404}, {0x9413a0, 0x941538, 19, 0x9c5408},
        {0x9413a0, 0x941558, 19, 0x9c540c}};
    for (int k = 0; k < 13; k++) reg_read_one(reg, &mud[k]);
    static const reg_read loc[6] = {
        {0x941594, 0x941578, 17, 0x9ca130},   /* RaceLine MaxCornerLookAheadDistance */
        {0x941594, 0x9415a4, 17, 0x9ca15c},   /* CornerDetectionAngle */
        {0x941594, 0x9415bc, 17, 0x9ca188},   /* MaxCorneringSpeed */
        {0x9415dc, 0x9415d0, 17, 0x9c5424},   /* PlayerReset FadeOutTime */
        {0x9415dc, 0x9415f0, 17, 0x9c5428},   /* FadeInTime */
        {0x9415dc, 0x9415fc, 17, 0x9c542c}};  /* InvulnerableTime */
    for (int k = 0; k < 6; k++) {
        const rx_addr rec = reg3(reg, 1, loc[k].path, loc[k].key, 1);
        if (rec) reg3(reg, 17, rec, F + 4, 1);
        SU32(0, loc[k].dst, U32(F, 4));
    }
    static const reg_read tail[3] = {{0x9415dc, 0x941610, 14, 0x9ca220}, {0x9415dc, 0x94161c, 17, 0x9c5430}, {0x9415dc, 0x94162c, 17, 0x9c5434}};
    for (int k = 0; k < 3; k++) reg_read_one(reg, &tail[k]);
    MX_END(F);
}
RX_FUNCTION(0x00549b30, rx_environment_settings_read)

/* 0051c620: the tag game's collision radii (MXGame\Tag, 324.0f each by default), squared */
void rx_tag_radii_read(void) {
    MX_FRAME(F, 0, 1);
    const rx_addr reg = U32(U32(0, 0x00a95844), 0xc);
    for (uint32_t k = 0; k < 8; k++) SU32(0, 0x9e38e8 + 4 * k, U32(0, 0x009358a8));   /* 324.0f */
    static const uint32_t keys[8] = {0x93f03c, 0x93f05c, 0x93f070, 0x93f08c, 0x93f0a8, 0x93f0c0, 0x93f0d8, 0x93f0f0};
    for (uint32_t k = 0; k < 8; k++) { const reg_read r = {0x93f050, keys[k], 17, 0x9e38e8 + 4 * k}; reg_read_one(reg, &r); }
    for (uint32_t k = 0; k < 8; k++) { const float x = F32(0, 0x9e38e8 + 4 * k); SF32(0, 0x9e38e8 + 4 * k, rx_muls(x, x)); }
    MX_END(F);
}
RX_FUNCTION(0x0051c620, rx_tag_radii_read)

/* ---- event handlers on channels ----
 * An EventHandlerProxy appended to the handler list of a channel's event (00593220) under the event's lock when its signature name
 * (vf1: +8) equals the event's (+4): `size` 0x10 for a function (vtable, 0, name, function) or 0x14 for a member (vtable, 0, name,
 * object, method). pushes = the registrar's saved registers (its frame), the proxy kept in its argument slot. */
void rx_mx_handler_register(rx_addr ch, rx_addr name, uint32_t size, rx_addr vt, uint32_t w3, uint32_t w4, rx_addr td, uint32_t pushes) {
    MX_FRAME(F, 0, pushes);
    rx_addr ev;
    MX_CALLR(ev, 2, rx_world_registry_find(ch, name));          /* 00593220 */
    rx_addr hp;
    MX_CALLR(hp, 1, rx_reflex_aligned_alloc(size));
    if (hp) {
        SU32(hp, 0xc, w3);
        SU32(hp, 4, 0); SU32(hp, 0, vt);
        if (size == 0x14) SU32(hp, 0x10, w4);
        rx_addr nm;
        nm = crt_type_info_name(td, 0x009e1a7c);                 /* push node; call (CRT convention: rx_sp() before the push) */
        SU32(hp, 8, nm);
    }
    const rx_addr a = U32(ev, 4), b = U32(hp, 8);
    int eq = 1;
    for (uint32_t i = 0;; i++) { if (U8(a, i) != U8(b, i)) { eq = 0; break; } if (!U8(a, i)) break; }
    if (eq) {
        SU32(F_sp, 4, hp);
        mx_cs_enter(ev + 0x24);
        MX_CALL(1, rx_event_handler_list_push_back(ev + 8, U32(F_sp, 4)));
        mx_cs_leave(ev + 0x24);
    }
    MX_END(F);
}
/* 00419700 (stdcall function; RET 4): a void(Activity*) function handler on "Activity Basic Essentials Loaded" of [0x00a93f74] */
void rx_activity_loaded_handler_add(rx_addr fn) {
    rx_mx_handler_register(U32(0, 0x00a93f74), 0x00936038, 0x10, 0x00939f6c, fn, 0, 0x009cbd24, 4);
}
RX_FUNCTION(0x00419700, rx_activity_loaded_handler_add)

/* the inlined EventSystem::Channel constructor (as build_mxgame_event.c's) with the creator's saved register pushed after the allocation */
static rx_addr channel_new_at(uint32_t inner_push) {
    const rx_addr sp0 = rx_sp();
    rx_addr ch;
    MX_CALLR(ch, 1, rx_reflex_aligned_alloc(0x5c));
    if (!ch) return 0;
    rx_set_sp(sp0 - inner_push);
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

/* 0055ed50: "ActivityManagerGlobalEvents" [0x00a93f74] and its 12 event types */
void rx_activity_global_channel_create(void) {
    MX_FRAME(F, 0, 2);
    rx_addr reg;
    MX_CALLR(reg, 0, rx_event_channel_registry());
    const rx_addr ch = channel_new_at(4);
    MX_CALL(2, rx_event_channel_register(reg, 0x00941eb4 /* "ActivityManagerGlobalEvents" */, ch));
    SU32(0, 0x00a93f74, ch);
    static const struct { uint32_t td, name; } t[11] = {
        {0x9cbd24, 0x936038}, {0x9cbd24, 0x935ea4}, {0x9cbd24, 0x941df4}, {0x9cbd24, 0x941e10},
        {0x9cda18, 0x941ba8}, {0x9cda18, 0x941bb8},                                   /* 00591500 */
        {0x9cbd24, 0x941d90}, {0x9cbd24, 0x941da0}, {0x9cbd24, 0x941ed0},
        {0x9cf660, 0x941ee4},                                                         /* 005913e0 */
        {0x9cf3f0, 0x941ef4}};                                                        /* 00586ca0 */
    for (int k = 0; k < 11; k++) MX_CALL(1, rx_event_type_register_at(U32(0, 0x00a93f74), t[k].name, t[k].td, 1, -136));
    MX_END(F);
}
RX_FUNCTION(0x0055ed50, rx_activity_global_channel_create)

/* 00409690: the trigger list [0x00a95d74] {data (50), 0, 50, 50, max}, the function handler 0x0040a9e0 on "Activity Basic Essentials
 * Loaded", the channels "VehicleTrigger" [0x00a89bc8] and its twin [0x00a89bcc] (0x0093606c) with their event types */
void rx_vehicle_trigger_channels_create(void) {
    MX_FRAME(F, 0, 3);
    rx_addr p;
    MX_CALLR(p, 1, rx_reflex_aligned_alloc(mx_size4(0x32)));
    SU32(0, 0x00a95d74, p); SU32(0, 0x00a95d78, 0); SU32(0, 0x00a95d7c, 0x32); SU32(0, 0x00a95d80, 0x32); SU32(0, 0x00a95d84, 0x7fffffffu);
    MX_CALL(1, rx_activity_loaded_handler_add(0x0040a9e0));
    rx_addr reg;
    MX_CALLR(reg, 0, rx_event_channel_registry());
    rx_addr ch = channel_new_at(0);
    MX_CALL(2, rx_event_channel_register(reg, 0x0093605c /* "VehicleTrigger" */, ch));
    SU32(0, 0x00a89bc8, ch);
    MX_CALLR(reg, 0, rx_event_channel_registry());
    ch = channel_new_at(0);
    MX_CALL(2, rx_event_channel_register(reg, 0x0093606c, ch));
    SU32(0, 0x00a89bcc, ch);
    MX_CALL(1, rx_event_type_register_at(U32(0, 0x00a89bc8), 0x00935e84, 0x009cbb7c, 1, -136));   /* 0041a580 */
    MX_CALL(1, rx_event_type_register_at(U32(0, 0x00a89bcc), 0x00936084, 0x009cbbac, 1, -136));   /* 0041a6a0 */
    MX_CALL(1, rx_event_type_register_at(U32(0, 0x00a89bc8), 0x0093609c, 0x009cbb7c, 1, -136));
    MX_CALL(1, rx_event_type_register_at(U32(0, 0x00a89bcc), 0x009360b4, 0x009cbbac, 1, -136));
    MX_CALL(1, rx_event_type_register_at(U32(0, 0x00a89bc8), 0x00935e94, 0x009cbb7c, 1, -136));
    MX_END(F);
}
RX_FUNCTION(0x00409690, rx_vehicle_trigger_channels_create)

void rx_mxgame_init_set_install(fmt_install* in) { g_in = in; }

/* ==== the NetworkManager (the function-local static 0x00da6ca0, 006de1f0; MXGame::Startup's 006de390 and the handlers after it) ====
 * Layout (what the core builds; the rest is 006de1f0's, race_driver.c): +0 the "Network" EventSystem::Channel, +4 / +8 the activity
 * channel / activity it listens to (006deff0), +0xc the device manager (006e0d20), +0x10 the PCSession (006e9120), +0x44 / +0x48 two
 * session helpers (00702480 / 006ee720). */

/* the event-type instantiations of FUN_00590c00's template used on the "Network" channel (EAX = channel; the name on the stack when
 * `name` is 0, else fixed; signature descriptor and stack depth as build_activity_events.inc) */
typedef struct { uint32_t fn, name, td; int aligned; int32_t sp; } net_event_type;
static const net_event_type k_net_types[] = {
    {0x00709c80, 0x0094cba8, 0x009d2a9c, 1, -136}, {0x00709da0, 0x0096286c, 0x009dc878, 1, -136},
    {0x00709ec0, 0,          0x009cdc7c, 1, -136}, {0x00709fe0, 0x00958098, 0x009d7580, 1, -136},
    {0x0070a100, 0x00941d30, 0x009cf8a8, 1, -136}, {0x0070a220, 0x00941d1c, 0x009cf81c, 1, -136},
    {0x0070a340, 0x0093eed8, 0x009cdd3c, 1, -136}, {0x0070a460, 0x0093ef78, 0x009cde24, 1, -136},
    {0x0070a580, 0x00942144, 0x009d0140, 1, -136}, {0x005c0bb0, 0,          0x009d24bc, 1, -136},
    {0x0070a6a0, 0x00942230, 0x009d024c, 1, -136}, {0x0070a7c0, 0x00941ff4, 0x009cfcb0, 1, -136},
    {0x0070a8e0, 0x009420fc, 0x009cffcc, 1, -136}, {0x0070aa00, 0x009423c4, 0x009d0820, 1, -136},
    {0x0070ab20, 0x00942280, 0x009d0420, 1, -136}, {0x0070ac40, 0x0094229c, 0x009d0494, 1, -136},
    {0x0070ad60, 0x009422b4, 0x009d0520, 1, -136}, {0x0070ae80, 0x00942350, 0x009d0710, 1, -136},
    {0x005c0cd0, 0,          0x009d24d0, 1, -136}, {0x0070afa0, 0,          0x009d2c04, 1, -136},
    {0x00586ca0, 0,          0x009cf3f0, 1, -136}, {0x0070b0c0, 0x009628cc, 0x009dc8a0, 1, -136},
    {0x0070b1e0, 0x00962900, 0x009dcd00, 1, -136}, {0x0070b300, 0x0094cbb8, 0x009d2ba4, 1, -136},
    {0x0070b420, 0x00962914, 0x009dcac4, 1, -136}, {0x0070b540, 0x00941d44, 0x009cf940, 1, -136},
    {0x0070b660, 0x00941354, 0x009ce5f4, 1, -136}, {0x0070b780, 0x00941368, 0x009ce674, 1, -136},
    {0x0070b8a0, 0x00962930, 0x009dcd28, 1, -136}, {0x0070b9c0, 0x00941d60, 0x009cf9a4, 1, -136},
    {0x0070bae0, 0x00962944, 0x009dcd4c, 1, -136}, {0x0070bc00, 0x0094cbe8, 0x009d2c68, 1, -136},
};
/* one instantiation called as the original calls it (EAX = channel; the pushed name or none) */
static void net_type(uint32_t fn, rx_addr ch, rx_addr name) {
    for (size_t i = 0; i < sizeof k_net_types / sizeof k_net_types[0]; i++) {
        const net_event_type* t = &k_net_types[i];
        if (t->fn != fn) continue;
        if (t->name) MX_CALL(0, rx_event_type_register_at(ch, t->name, t->td, t->aligned, t->sp));
        else MX_CALL(1, rx_event_type_register_at(ch, name, t->td, t->aligned, t->sp));
        return;
    }
    mx_unreached(fn, "a Network event-type instantiation not in the table");
}

/* 007071a0 (EAX = the channel; stdcall manager; RET 4): the manager's member handler 006df1b0 (proxy 0x009655f8, void (*)(int)) on
 * "NetworkEventDetailsChanged" */
void rx_netmgr_listen_event_details(rx_addr ch, rx_addr mgr) {
    rx_mx_handler_register(ch, 0x0094be90, 0x14, 0x009655f8, mgr, 0x006df1b0, 0x009d24d0, 2);
}
RX_FUNCTION(0x007071a0, rx_netmgr_listen_event_details)
/* 00707260 (EAX = the channel; stdcall manager; RET 4): its handler 006df510 (proxy 0x00965618, void (*)(NetObjectType,
 * Quazal::DuplicatedObject*)) on "NetworkObjectAdded" */
void rx_netmgr_listen_object_added(rx_addr ch, rx_addr mgr) {
    rx_mx_handler_register(ch, 0x00941758, 0x14, 0x00965618, mgr, 0x006df510, 0x009cdc7c, 2);
}
RX_FUNCTION(0x00707260, rx_netmgr_listen_object_added)

/* 006de4b0 (ESI = the manager): the "Network" channel (registry 00593900 first, then the inlined Channel constructor with EBX pushed
 * after its allocation, registered 00593520) stored at +0, its 40 event types in the original's order, then the manager's two
 * handlers. Frame: push ebp; push edi. */
void rx_netmgr_channel_init(rx_addr mgr) {
    MX_FRAME(F, 0, 2);
    rx_addr reg;
    MX_CALLR(reg, 0, rx_event_channel_registry());
    const rx_addr ch = channel_new_at(4);
    MX_CALL(2, rx_event_channel_register(reg, 0x0093dac4 /* "Network" */, ch));
    SU32(mgr, 0, ch);
    net_type(0x00709c80, U32(mgr, 0), 0);                                   /* "NetworkStatus" */
    net_type(0x00709da0, U32(mgr, 0), 0);                                   /* "NetworkError" */
    net_type(0x00709ec0, U32(mgr, 0), 0x00941758);                          /* "NetworkObjectAdded" */
    net_type(0x00709ec0, U32(mgr, 0), 0x0093ee98);                          /* "NetworkObjectRemoved" */
    net_type(0x00709ec0, U32(mgr, 0), 0x0093eeb0);                          /* "NetworkObjectMigrated" */
    net_type(0x00709fe0, U32(mgr, 0), 0);                                   /* "NetworkLoginStatusChanged" */
    net_type(0x0070a100, U32(mgr, 0), 0);                                   /* "NetworkSessionState" */
    net_type(0x0070a220, U32(mgr, 0), 0);                                   /* "NetworkTimerEvents" */
    net_type(0x0070a340, U32(mgr, 0), 0);                                   /* "NetworkActivityEvent" */
    net_type(0x0070a460, U32(mgr, 0), 0);                                   /* "NetworkSnakeActivity" */
    MX_CALL(1, rx_channel_add_float_event(U32(mgr, 0), 0x00942128));        /* 005ed6e0 (ECX): "NetworkSnakeBoundaryChanged" */
    net_type(0x0070a580, U32(mgr, 0), 0);                                   /* "NetworkSnakeRoundComplete" */
    net_type(0x005c0bb0, U32(mgr, 0), 0x0096287c);                          /* "NetworkHockeyActivity" */
    /* Tag, Graffiti, SectionRace, Capture activities; NetPoker community / hand / choice cards; NetEggHuntEggPicked */
    static const uint32_t run1[8] = {0x0070a6a0, 0x0070a7c0, 0x0070a8e0, 0x0070aa00, 0x0070ab20, 0x0070ac40, 0x0070ad60, 0x0070ae80};
    for (int k = 0; k < 8; k++) net_type(run1[k], U32(mgr, 0), 0);
    net_type(0x005c0cd0, U32(mgr, 0), 0x0094be90);                          /* "NetworkEventDetailsChanged" */
    net_type(0x0070afa0, U32(mgr, 0), 0x0094cbd0);                          /* "NetworkPlayerVoteStatus" */
    net_type(0x0070afa0, U32(mgr, 0), 0x00962894);                          /* "NetworkPlayerVoteReceived" */
    net_type(0x00586ca0, U32(mgr, 0), 0x0094cbfc);                          /* "NetworkSyncLocationEventStart" */
    net_type(0x005c0cd0, U32(mgr, 0), 0x0094cc1c);                          /* "NetworkSyncLocationEventEnd" */
    net_type(0x00586ca0, U32(mgr, 0), 0x009628b0);                          /* "NetworkSyncLocationEvent" */
    net_type(0x0070b0c0, U32(mgr, 0), 0);                                   /* "NetworkStatsReadComplete" */
    net_type(0x005c0cd0, U32(mgr, 0), 0x009628e8);                          /* "NetworkExpReadComplete" */
    /* FriendStatus, InviteRecieved, PlayerVehicleUpdated, PositionListUpdate, StuntUpdate, VehicleReset, NetZStarted,
     * PlayerModeChanged, PlayerMotocardData */
    static const uint32_t run2[9] = {0x0070b1e0, 0x0070b300, 0x0070b420, 0x0070b540, 0x0070b660, 0x0070b780, 0x0070b8a0, 0x0070b9c0,
                                     0x0070bae0};
    for (int k = 0; k < 9; k++) net_type(run2[k], U32(mgr, 0), 0);
    net_type(0x00586ca0, U32(mgr, 0), 0x00962960);                          /* "NetworkPartyStatusChanged" */
    net_type(0x0070bc00, U32(mgr, 0), 0);                                   /* "NetworkStringVerify" */
    MX_CALL(1, rx_netmgr_listen_event_details(U32(mgr, 0), mgr));          /* 007071a0 */
    MX_CALL(1, rx_netmgr_listen_object_added(U32(mgr, 0), mgr));           /* 00707260 */
    MX_END(F);
}
RX_FUNCTION(0x006de4b0, rx_netmgr_channel_init)

/* 006de390 (stdcall manager, a1, a2: Engine::Startup's window arguments; RET 0xc): NetworkManager::Init. Composition, not registered
 * (its network part is a boundary, so a per-call Verify cannot match): the Quazal allocator hooks [0x009df5b8] = 00401f90
 * (ReflexAlignedAlloc's body) / [0x009df5bc] = 006de180 (its free), the channel (006de4b0, ported), then the network objects.
 * BOUNDARY (network play is not ported; QUEUE "presencefix" / "Product driver chain"): the device manager (0x1a0, 006e0d20, +0xc =
 * [0x00da6cac]) and the PCSession (0x1d40, 006e9120, +0x10 = [0x00da6cb0]) with their vf1 starts (Quazal / Steam), the atexit-guarded
 * static 0x00da6b40 (006df870), the session helpers +0x44 (0x40, 00702480) and +0x48 (0x98, 006ee720) are not built: they stay 0, the
 * offline stand-in every core reader checks ([0x00da6cb0] == 0). Nothing gameplay reads is in them (the core's readers: race_activity.c
 * stats / presence, loop_cam_player.c's pause test, build_activity_pre.c's timer event, all gated on [0x00da6cb0]). */
static void netmgr_init(rx_addr mgr, uint32_t p2, uint32_t p3) {
    MX_FRAME(F, 0, 4);                                                     /* push ebx / ebp / esi / edi */
    (void)p2; (void)p3;                                                    /* the PCSession's vf1 arguments (boundary) */
    SU32(0, 0x009df5b8, 0x00401f90);
    SU32(0, 0x009df5bc, 0x006de180);
    MX_CALL(0, rx_netmgr_channel_init(mgr));                               /* 006de4b0 (ESI = mgr) */
    MX_END(F);
}

/* 007604b0 (EAX = the "Network" channel; stdcall game; RET 4): MXGame's handler 0075c9d0 (proxy 0x0096d814) on "NetworkSessionState" */
void rx_mxgame_listen_session_state(rx_addr ch, rx_addr game) {
    rx_mx_handler_register(ch, 0x00941d30, 0x14, 0x0096d814, game, 0x0075c9d0, 0x009cf8a8, 2);
}
RX_FUNCTION(0x007604b0, rx_mxgame_listen_session_state)
/* 0075cf50 (ECX = game; stack kind, float seconds; RET 8): MXGame's "NetworkTimerEvents" handler (through 007606d0): the startup table's
 * value of 0xd (004df6a0, its result unused); under the debug override [0x00d611f8] the kind 2 event outside a track view
 * (0075c5f0 == 0) sets +0x15b3c = [0x009cb234] and, with the UI quitting ([[0xa93fcc] + 0x288]), the UI's "GotoHub" (0059b7e0:
 * UIProperty_SetString 0x0096d718 + the UI event: UI, not core; the oracle runs the original, the product nothing). Offline the
 * channel only carries kind 1 (006e4a90's timer). Frame: push esi. */
void rx_mxgame_on_net_timer(rx_addr game, uint32_t kind, float seconds) {
    MX_FRAME(F, 0, 1);
    (void)seconds;
    MX_CALL(0, rx_startup_table_value(0xd));
    if (U32(0, 0x00d611f8) != 0 && kind == 2) {
        uint32_t view;
        MX_CALLR(view, 0, rx_game_is_view_track(game));
        if (view == 0) {
            SU32(game, 0x15b3c, U32(0, 0x009cb234));
            const rx_addr ui = U32(0, 0x00a93fcc);
            if (U32(ui, 0x288) != 0) {
#ifdef RX_ORACLE
                rx_regs in, out; memset(&in, 0, sizeof in); in.r[0] = 0x0096d718; in.r[1] = ui;
                const uint32_t a0 = 0;
                MX_CALL(1, rx_call_original_ex(0x0059b7e0, &in, &out, 1, &a0));
#endif
            }
        }
    }
    MX_END(F);
}
RX_FUNCTION(0x0075cf50, rx_mxgame_on_net_timer)

/* 00760570 (EAX = the channel; stdcall game; RET 4): MXGame's handler 0075cf50 (proxy 0x0096d834) on "NetworkTimerEvents" */
void rx_mxgame_listen_timer_events(rx_addr ch, rx_addr game) {
    rx_mx_handler_register(ch, 0x00941d1c, 0x14, 0x0096d834, game, 0x0075cf50, 0x009cf81c, 2);
}
RX_FUNCTION(0x00760570, rx_mxgame_listen_timer_events)
/* 00554eb0 (ECX = channel, EAX = name; stdcall object, method; RET 8): a PlayerManager member handler (proxy 0x00941854, void (*)(
 * NetObjectType, Quazal::DuplicatedObject*)) on the named event */
void rx_player_manager_listen(rx_addr ch, rx_addr name, rx_addr obj, rx_addr method) {
    rx_mx_handler_register(ch, name, 0x14, 0x00941854, obj, method, 0x009cdc7c, 2);
}
RX_FUNCTION(0x00554eb0, rx_player_manager_listen)
/* 00552d20 (ESI = the player slots, MXGame + 0x7460): their handlers 00552e80 on "NetworkObjectAdded" and 00552ea0 on
 * "NetworkObjectRemoved" / "NetworkObjectMigrated" of the "Network" channel (the session static's guard before each). Frame: push ebx. */
void rx_player_manager_net_listen(rx_addr players) {
    MX_FRAME(F, 0, 1);
    static const uint32_t ev[3][2] = {{0x00941758, 0x00552e80}, {0x0093ee98, 0x00552ea0}, {0x0093eeb0, 0x00552ea0}};
    for (int k = 0; k < 3; k++) {
        race_static_dcf304();
        MX_CALL(2, rx_player_manager_listen(U32(0, 0x00da6ca0), ev[k][0], players, ev[k][1]));
    }
    MX_END(F);
}
RX_FUNCTION(0x00552d20, rx_player_manager_net_listen)
/* 007558e0 (EAX = the channel; stdcall data miner; RET 4): the DataMiner's handler 007536c0 (proxy 0x0096d4d0) on
 * "NetworkLoginStatusChanged" */
void rx_data_miner_listen_login(rx_addr ch, rx_addr miner) {
    rx_mx_handler_register(ch, 0x00958098, 0x14, 0x0096d4d0, miner, 0x007536c0, 0x009d7580, 2);
}
RX_FUNCTION(0x007558e0, rx_data_miner_listen_login)

/* MXGame::Startup 00756f3c .. 00756fdc: NetworkManager::Init on the static 0x00da6ca0 (006de390), then (each behind the session static's
 * guard) MXGame's two network handlers, the player slots' (ESI = game + 0x7460) and the DataMiner's ([0x00d92f10]) on its channel. */
void rx_mxgame_network_startup(rx_addr game, uint32_t a1, uint32_t a2) {
    race_static_dcf304();
    MX_CALL(3, netmgr_init(0x00da6ca0, a2, a1));                           /* push edi ([ebp + 8]); push ecx ([ebp + 0xc]); push 0xda6ca0 */
    race_static_dcf304();
    MX_CALL(1, rx_mxgame_listen_session_state(U32(0, 0x00da6ca0), game)); /* 007604b0 */
    race_static_dcf304();
    MX_CALL(1, rx_mxgame_listen_timer_events(U32(0, 0x00da6ca0), game));  /* 00760570 */
    MX_CALL(0, rx_player_manager_net_listen(game + 0x7460));               /* 00552d20 */
    race_static_dcf304();
    MX_CALL(1, rx_data_miner_listen_login(U32(0, 0x00da6ca0), U32(0, 0x00d92f10)));   /* 007558e0 */
}
