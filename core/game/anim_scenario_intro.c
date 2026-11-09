/* anim_scenario_intro.c -- the race / free-ride intro scenarios and their scenario animation manager (0x0074xxxx).
 * Scenario animation manager SM: +0x294 actors (count +0x744), +0x748 props (count +0x16e8), +0x16ec cameras (count +0x172c),
 * +0x1730 the active camera, +0x1734 12 attachments (entity, owner: the entity follows the owner's matrix +0x240).
 * A scenario entity: +0x18 name, +0x70 playing, +0xa2 flags byte, +0x350 / +0x378 states. (core/notes/anim.md) */
#include "anim_scenario.h"
#ifndef RX_ORACLE
#include "scenario_losing_player.h"
#endif
#include "anim_scenario_race_intro.h"
#include "crt.h"
#include "../src/rx_os.h"
#include "anim.h"
#include "anim_extern.h"
#include "anim_ik.h"
#include "rx_vcall.h"
#include "build_vehicle.h"
#include "loop_cam_entity.h"
#include "race_activity.h"
#include "race_finish.h"
#include "race.h"
#include "world_node.h"
#include "race_misc.h"
#include "build_activity_freeride_run.h"
#include <string.h>
#include "rider_cap.h"

/* A direct call at the original call site: the callee enters with its `n` stack arguments (and the return-address slot the
 * replaced call left) below the caller's stack pointer. Returns the caller's SP to restore after the call. */
static rx_addr intro_callsite(uint32_t n, const uint32_t* a) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * n - 4u;
    SU32(sp, 0, 0);
    for (uint32_t i = 0; i < n; i++) SU32(sp + 4 + 4u * i, 0, a[i]);
    rx_set_sp(sp);
    return sp0;
}

/* 00749770 ScenarioAnimEntity::vf22: the entity type 0x104f. */
uint32_t rx_scenario_anim_entity_type(rx_addr e) { (void)e; return 0x104f; }
RX_FUNCTION(0x00749770, rx_scenario_anim_entity_type)

/* 0074b060 ScenarioAnims_ClearAttachments (EAX = SM). */
void rx_scenario_anims_clear_attachments(rx_addr sm) {
    for (uint32_t i = 0; i < 12; i++) { SU32(sm, 0x1734 + 8 * i, 0); SU32(sm, 0x1738 + 8 * i, 0); }
}
RX_FUNCTION(0x0074b060, rx_scenario_anims_clear_attachments)

/* 0074b080 ScenarioCast_Find (EDI = cast, stack entity; RET 4): the 0x30-byte cast entry (+0x50 + i * 0x30, entity at +4) of the
 * entity among the 12 (the first empty one ends the search), or 0. */
rx_addr rx_scenario_cast_find(rx_addr cast, rx_addr e) {
    for (uint32_t i = 0; i < RX_RC_MAX; i++) {   /* (w-rc-core) entries 12..31 in side storage */
        rx_addr x = rx_cast_u32(cast, i, 4);
        if (x == 0) return 0;
        if (x == e) return rx_cast_rec_ro(cast, i);
    }
    return 0;
}
RX_FUNCTION(0x0074b080, rx_scenario_cast_find)

static void entity_stop(rx_addr e) { SU32(e, 0x70, 0); SU8(e, 0xa2, 0); }

/* 0074b450 ScenarioAnims_StopActors (EDX = SM). */
void rx_scenario_anims_stop_actors(rx_addr sm) {
    for (int32_t i = 0; i < I32(sm, 0x744); i++) entity_stop(U32(sm, 0x294 + 4u * (uint32_t)i));
}
RX_FUNCTION(0x0074b450, rx_scenario_anims_stop_actors)

/* 0074b480 ScenarioAnims_ResetCameras (EAX = SM): every camera (and the active one) back to state 1, stopped. */
void rx_scenario_anims_reset_cameras(rx_addr sm) {
    for (int32_t i = 0; i < I32(sm, 0x172c); i++) {
        rx_addr e = U32(sm, 0x16ec + 4u * (uint32_t)i);
        SU32(e, 0x350, 1); entity_stop(e);
    }
    rx_addr e = U32(sm, 0x1730);
    if (e) { SU32(e, 0x350, 1); entity_stop(e); }
}
RX_FUNCTION(0x0074b480, rx_scenario_anims_reset_cameras)

/* 0074b4f0 ScenarioCast_StopAll (EAX = cast) / 0074b610 ScenarioCast_ClearStates: the 12 entries' entities (+0x50 + i * 0x30). */
void rx_scenario_cast_stop_all(rx_addr cast) {
    for (uint32_t i = 0; i < RX_RC_MAX; i++) { rx_addr e = rx_cast_u32(cast, i, 0); if (e) entity_stop(e); }   /* (w-rc-core) entries 12..31 in side storage */
}
RX_FUNCTION(0x0074b4f0, rx_scenario_cast_stop_all)
void rx_scenario_cast_clear_states(rx_addr cast) {
    for (uint32_t i = 0; i < RX_RC_MAX; i++) { rx_addr e = rx_cast_u32(cast, i, 0); if (e) SU32(e, 0x378, 0); }   /* (w-rc-core) entries 12..31 in side storage */
}
RX_FUNCTION(0x0074b610, rx_scenario_cast_clear_states)

/* 00749590 Scenario_SetRidersPaused (EDI = scenario, EDX = flag): every rider's vehicle +0x1010 and rider animation +0x5c4e0 =
 * the flag; then the level cast (world +0x20150) paused (0074b5a0) or its states cleared (0074b610). */
void rx_scenario_set_riders_paused(rx_addr sc, uint32_t flag) {
    rx_addr a = U32(sc, 0x14);
    int32_t n = I32(a, 0x26c);
    for (int32_t i = 0; i < n; i++) {
        rx_addr r = U32(U32(U32(sc, 0x14), 0x268), 4u * (uint32_t)i);
        SU32(U32(U32(r, 0x74), 0x1ac), 0x1010, flag);
        SU32(U32(U32(r, 0x74), 0x1b4), 0x5c4e0, flag);
    }
    rx_addr cast = U32(U32(sc, 0x14), 0x234) + 0x20150;
    if (flag) { const rx_addr sp = rx_sp(); rx_set_sp(sp - 4u); rx_scenario_cast_pause(cast); rx_set_sp(sp); }
    else rx_scenario_cast_clear_states(cast);
}
RX_FUNCTION(0x00749590, rx_scenario_set_riders_paused)

/* Name lookups over a table (count at +cnt, entries at +tab, each entity's name +0x18; _stricmp, the operand order as the original). */
static rx_addr find_by_name(rx_addr sm, uint32_t tab, uint32_t cnt, rx_addr name, int name_first) {
    for (int32_t i = 0; i < I32(sm, cnt); i++) {
        rx_addr e = U32(sm, tab + 4u * (uint32_t)i);
        int32_t c = name_first ? crt_stricmp(name, U32(e, 0x18)) : crt_stricmp(U32(e, 0x18), name);
        if (c == 0) return U32(sm, tab + 4u * (uint32_t)i);
    }
    return 0;
}
/* 0074b0c0 / 0074b120 / 0074b680 ScenarioAnims_FindActor / FindCamera / FindProp (stdcall: SM, name; RET 8). */
rx_addr rx_scenario_anims_find_actor(rx_addr sm, rx_addr name) { return find_by_name(sm, 0x294, 0x744, name, 0); }
RX_FUNCTION(0x0074b0c0, rx_scenario_anims_find_actor)
rx_addr rx_scenario_anims_find_camera(rx_addr sm, rx_addr name) { return find_by_name(sm, 0x16ec, 0x172c, name, 1); }
RX_FUNCTION(0x0074b120, rx_scenario_anims_find_camera)
rx_addr rx_scenario_anims_find_prop(rx_addr sm, rx_addr name) { return find_by_name(sm, 0x748, 0x16e8, name, 0); }
RX_FUNCTION(0x0074b680, rx_scenario_anims_find_prop)

/* 0074b000 ScenarioAnims_UpdateAttachments (stdcall: SM; RET 4): each attached entity gets its owner's matrix (+0x240, copied to a
 * local) through vf16. */
void rx_scenario_anims_update_attachments(rx_addr sm) {
    ANIM_FRAME_ENTER(A, 0x50);
    const rx_addr m = A - 0x40;
    for (uint32_t i = 0; i < 12; i++) {
        rx_addr p = sm + 0x1734 + 8 * i;
        if (U32(p, 0) == 0) continue;
        rx_addr owner = U32(p, 4);
        for (uint32_t k = 0; k < 0x40; k += 4) SU32(m, k, U32(owner, 0x240 + k));
        rx_addr e = U32(p, 0); uint32_t a = m;
        (void)rx_vcall(e, 0x40, 1, &a);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0074b000, rx_scenario_anims_update_attachments)

/* 0074af60 ScenarioAnims_Attach (EAX = owner, EDX = SM, stack entity; RET 4): the entity takes the first free attachment and the
 * owner's matrix at once. */
void rx_scenario_anims_attach(rx_addr owner, rx_addr sm, rx_addr e) {
    uint32_t i = 0;
    while (i < 12 && U32(sm, 0x1734 + 8 * i) != 0) i++;
    if (i >= 12) return;
    ANIM_FRAME_ENTER(A, 0x50);
    const rx_addr m = A - 0x40;
    SU32(sm, 0x1738 + 8 * i, owner);
    SU32(sm, 0x1734 + 8 * i, e);
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(m, k, U32(owner, 0x240 + k));
    uint32_t a = m;
    (void)rx_vcall(e, 0x40, 1, &a);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0074af60, rx_scenario_anims_attach)

/* 0074c810 Scenario_PlayOn (ECX = target, EDX = what, stack arg; RET 4): target's vf69 (arg, what, 1); returns 1. */
uint32_t rx_scenario_play_on(rx_addr t, uint32_t what, uint32_t arg) {
    uint32_t a[3] = {arg, what, 1};
    rx_call_original(U32(U32(t, 0), 0x114), RX_THISCALL, t, 3, a);
    return 1;
}
RX_FUNCTION(0x0074c810, rx_scenario_play_on)

/* 0074c7e0 Scenario_NameOf (ECX = target, EDX = what, EDI = 64-byte buffer): the target's vf68 name of `what` formatted ("%s")
 * into the buffer; returns 1. */
uint32_t rx_scenario_name_of(rx_addr t, uint32_t what, rx_addr buf) {
    rx_set_sp(rx_sp() - 4);                                    /* push esi */
    uint32_t a = what;
    rx_addr name = rx_call_original(U32(U32(t, 0), 0x110), RX_THISCALL, t, 1, &a);
    const rx_addr esp = rx_sp(), cf = esp - 12;
    SU32(cf, 0, 0); SU32(cf + 4, 0, 0x009419e4); SU32(cf + 8, 0, name);
    rx_set_sp(cf);
    rx_format_buffer(buf, 0x40, 0x009419e4, cf + 8);
    rx_set_sp(esp);
    rx_set_sp(rx_sp() + 4);
    return 1;
}
RX_FUNCTION(0x0074c7e0, rx_scenario_name_of)

/* ---- scenario animation entities ---- */

/* 0074ab10 ScenarioAnimEntity_Job (cdecl: job {entity, dt}): under the entity's lock (+0x3e0) its character animation (+0x148)
 * advances by dt (Character_SampleKeyframes). The original keeps dt in its argument slot. (Not a plan row of its own: Ghidra
 * merges it into 0074aab0, whose instructions per frame are this job's.) */
void rx_scenario_anim_entity_job(rx_addr job) {
    const rx_addr sp0 = rx_sp();
    rx_addr e = U32(job, 0);
    float dt = F32(job, 4);
    SF32(sp0 + 4, 0, dt);
    rx_set_sp(sp0 - 8);                                       /* push esi, edi */
    anim_enter_cs(e + 0x3e0);
    ANIM_CALL_FRAME(4); rx_character_sample_keyframes(e + 0x148, dt); ANIM_CALL_FRAME_END(4);
    anim_leave_cs(e + 0x3e0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074ab10, rx_scenario_anim_entity_job)

/* 0074aab0 ScenarioAnimEntity_SetAnimation (ESI = entity, stack name; RET 4): under its lock the animation name (+0x3f8, 127
 * chars) set, +0x478 = 0.5, and vf19 run with dt 0 (to load it). */
void rx_scenario_anim_entity_set_animation(rx_addr e, rx_addr name) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);                                       /* push ebx, edi */
    anim_enter_cs(e + 0x3e0);
    crt_strncpy(e + 0x3f8, name, 0x7f);
    SU8(e, 0x3f8 + 0x7f, 0);
    SF32(e, 0x478, 0.5f /* 0x9322cc */);
    {   /* vf19 (0074ab50: a float argument, so the registry's integer table cannot dispatch it: typed here) with dt 0.0;
         * the original's call pushes the argument (the strncpy argument slot, reused: it stays on the stack, which is what the
         * port's frame and any dead-stack read expect) */
        const rx_addr target = U32(U32(e, 0), 0x4c);
        void (*f)(rx_addr, float) = RX_FN(target, void (*)(rx_addr, float));
        if (f) {
            const rx_addr s = rx_sp();
            SU32(s - 4, 0, 0);                            /* the argument slot (push 0) */
            SU32(s - 8, 0, 0);                            /* the return-address slot */
            rx_set_sp(s - 8u);
            f(e, 0.0f);
            rx_set_sp(s);
        } else { uint32_t zero = 0; rx_call_original(target, RX_THISCALL, e, 1, &zero); }
    }
    anim_leave_cs(e + 0x3e0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074aab0, rx_scenario_anim_entity_set_animation)

/* 0074a9f0 ScenarioAnimEntity_Play (stdcall: entity; RET 4): under its lock, +0x478 = -1; the named animation ('anim' resource
 * of +0x3f8) when it exists: vf15 (+0x390), its character slot (found, or made with looping on), played from the start, the entity
 * playing (+0x70 / +0xa2 = 1). The original keeps the lock's address in its argument slot. */
void rx_scenario_anim_entity_play(rx_addr e) {
    const rx_addr sp0 = rx_sp(), cs = e + 0x3e0;
    rx_set_sp(sp0 - 0x10);                                    /* push ebx, ebp, esi, edi */
    anim_enter_cs(cs);
    SU32(sp0 + 4, 0, cs);
    SF32(e, 0x478, -1.0f /* 0x9cd9e8 */);
    rx_addr mgr = U32(U32(0, 0x00a95844), 8);
    uint32_t a[4] = {e + 0x3f8, 0x6d696e61 /* 'anim' */, 0, 0};
    rx_addr res = rx_call_original(U32(U32(mgr, 0), 0x6c), RX_THISCALL, mgr, 4, a);
    if (res) {
        uint32_t b = e + 0x390;
        rx_call_original(U32(U32(e, 0), 0x3c), RX_THISCALL, e, 1, &b);
        rx_addr c = e + 0x148;
        ANIM_CALL_FRAME(0); rx_addr slot = rx_character_anim_find_slot(res, c); ANIM_CALL_FRAME_END(0);
        if (slot == 0) {
            ANIM_CALL_FRAME(8); slot = rx_character_anim_set_animation(c, res, slot); ANIM_CALL_FRAME_END(8);
            rx_addr h = U32(slot, 0);
            SU32(h, 0x4c, 1);
            SU32(U32(h, 0x44), 0x54, 1);
            SU32(U32(slot, 0), 0x4c, 0);
        }
        ANIM_CALL_FRAME(4); rx_character_anim_play(c, slot, 0.0f); ANIM_CALL_FRAME_END(4);
        SU8(e, 0xa2, 1);
        SU32(e, 0x70, 1);
    }
    anim_leave_cs(U32(sp0 + 4, 0));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074a9f0, rx_scenario_anim_entity_play)

/* ---- the free-ride intro ---- */
static rx_addr activity_rider(rx_addr a, uint32_t i) {                     /* 00559e60: EAX = activity; stdcall index */
    const rx_addr sp0 = intro_callsite(1, &i);
    const rx_addr r = rx_activity_human_driver(a, (int32_t)i);
    rx_set_sp(sp0);
    return r;
}
static uint32_t activity_human_count(rx_addr a) {                          /* 00559df0: EDX = activity, ESI = 0 (humans) */
    const rx_addr sp0 = intro_callsite(0, 0);
    const uint32_t r = rx_activity_count_kind(a, 0);
    rx_set_sp(sp0);
    return r;
}

/* 0074d080 FreerideIntroScenario::vf3 (thiscall): the intro's state +0x151b4: 1 picks the next (13 with several players, else 5 or
 * 4 by the first rider's vehicle +0x194), and the level's two cameras (world +0x20150 / +0x21d50) take the first rider's vehicle
 * matrix (vf14); returns 1. States 4, 5 and 13 end (state 0); any other returns 0. */
uint32_t rx_freeride_intro_update(rx_addr s) {
    uint32_t st = U32(s, 0x151b4);
    if (st == 4 || st == 5 || st == 13) { SU32(s, 0x151b4, 0); return 0; }
    if (st != 1) return 0;
    rx_addr a = U32(s, 0x14);
    if ((int32_t)activity_human_count(a) > 1) SU32(s, 0x151b4, 0xd);
    else {
        uint32_t v = U32(U32(U32(activity_rider(a, 0), 0x74), 0x1ac), 0x194);
        SU32(s, 0x151b4, 1u < v ? 5 : 4);
    }
    rx_addr veh = U32(U32(activity_rider(a, 0), 0x74), 0x1ac);
    rx_addr m = anim_vcall0(veh, 0x38);
    rx_addr w = U32(U32(s, 0x14), 0x234);
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(w + 0x20150, k, U32(m, k));
    w = U32(U32(s, 0x14), 0x234);
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(w + 0x21d50, k, U32(m, k));
    return 1;
}
RX_FUNCTION(0x0074d080, rx_freeride_intro_update)

/* 0074d460 FreerideIntroScenario::vf6 (thiscall): the scenario base reset (0047c3f0), every rider of the level cast frozen (rider
 * animation +0xa2 = 1), the riders unpaused (00749590 with 0). */
void rx_freeride_intro_end(rx_addr s) {
    rx_set_sp(rx_sp() - 4);                                   /* push edi */
    { const rx_addr sp = intro_callsite(0, 0); rx_pose_overrides_clear(); rx_set_sp(sp); }
    rx_addr w = U32(U32(s, 0x14), 0x234);
    rx_addr cast = w + 0x20150;
    int32_t n = I32(U32(w, 0x20190), 0x26c);
    for (int32_t i = 0; i < n; i++)
        SU8(U32(U32(U32(U32(U32(cast, 0x40), 0x268), 4u * (uint32_t)i), 0x74), 0x1b4), 0xa2, 1);
    ANIM_CALL_FRAME(0); rx_scenario_set_riders_paused(s, 0); ANIM_CALL_FRAME_END(0);
    rx_set_sp(rx_sp() + 4);
}
RX_FUNCTION(0x0074d460, rx_freeride_intro_end)




/* 00747750 Scenario_LoadIntroScript (ECX = scenario, EDX = kind): the intro script's name (00747940 for the event, the kind) and its
 * 'bxml' resource loaded into the scenario's cast (+0x20, 0074ce40); 0 when there is no such resource. */
uint32_t rx_scenario_load_intro_script(rx_addr sc, uint32_t kind) {
    const rx_addr sp0 = rx_sp(), A8 = (sp0 - 4u) & ~7u, body = A8 - 0x120, name = body + 0x10;
    rx_set_sp(body);
    ANIM_CALL_FRAME(0xc); rx_scenario_intro_script_name(kind, U32(sc, 0x14), name); ANIM_CALL_FRAME_END(0xc);
    rx_addr mgr = U32(U32(0, 0x00a95844), 8);
    uint32_t a[4] = {name, 0x6c6d7862 /* 'bxml' */, 0, 0};
    rx_addr res = rx_call_original(U32(U32(mgr, 0), 0x6c), RX_THISCALL, mgr, 4, a);
    uint32_t r = 0;
    if (res) {
        SU32(res, 0x4c, 1);
        rx_addr d = U32(res, 0x44);
        ANIM_CALL_FRAME(0); r = rx_scenario_cast_load_script(d ? d - 0x460 : 0, sc + 0x20); ANIM_CALL_FRAME_END(0);
        SU32(res, 0x4c, 0);
    }
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x00747750, rx_scenario_load_intro_script)

/* 00747720 Scenario_Begin (ESI = scenario, EDI = kind): vf1 (kind), then the intro script; loaded: clock +0x151a8 / step +0x151ac
 * reset. */
void rx_scenario_begin(rx_addr sc, uint32_t kind) {
    rx_set_sp(rx_sp() - 4);                                   /* push ecx */
    uint32_t a = kind;
    rx_call_original(U32(U32(sc, 0), 4), RX_THISCALL, sc, 1, &a);
    ANIM_CALL_FRAME(0); uint32_t ok = rx_scenario_load_intro_script(sc, kind); ANIM_CALL_FRAME_END(0);
    if (ok) { SF32(sc, 0x151a8, 0.0f); SU32(sc, 0x151ac, 0); }
    rx_set_sp(rx_sp() + 4);
}
RX_FUNCTION(0x00747720, rx_scenario_begin)

/* ---- the intro script (an XML element tree read through the parser's virtuals: +0xe8 attribute (element, name), +0x110 its
 *      string, +0x130 float (attribute, out, 1), +0x134 int (attribute, out, 1), +0x04 root, +0x20 first child (element, name,
 *      iterator out), +0x28 next (iterator), +0x24 release (iterator)) ---- */

static rx_addr xml_attr(rx_addr p, rx_addr elem, rx_addr name) {
    uint32_t a[2] = {elem, name}; return rx_call_original(U32(U32(p, 0), 0xe8), RX_THISCALL, p, 2, a);
}
static uint32_t xml_read(rx_addr p, uint32_t off, rx_addr attr, rx_addr out) {
    uint32_t a[3] = {attr, out, 1}; return rx_call_original(U32(U32(p, 0), off), RX_THISCALL, p, 3, a);
}
#define XML_FLOAT 0x130
#define XML_INT   0x134
/* An attribute's string formatted into a 64-byte field (Scenario_NameOf 0074c7e0); its matrix read into +0xd0 (0074c810). */
static int field_name(rx_addr p, rx_addr elem, rx_addr name, rx_addr dst) {
    rx_addr at = xml_attr(p, elem, name);
    if (!at) return 0;
    ANIM_CALL_FRAME(0); rx_scenario_name_of(p, at, dst); ANIM_CALL_FRAME_END(0);
    return 1;
}
static int field_matrix(rx_addr p, rx_addr elem, rx_addr ev) {
    rx_addr at = xml_attr(p, elem, 0x0094fc70 /* "Matrix" */);
    if (!at) return 0;
    ANIM_CALL_FRAME(4); rx_scenario_play_on(p, ev + 0xd0, at); ANIM_CALL_FRAME_END(4);
    return 1;
}
#define S_ANIM     0x0096c3dc   /* "AnimName" */
#define S_MODEL    0x0096c3d0   /* "ModelName" */
#define S_SOUND    0x0095f1ec   /* "Sound" */
#define S_CROWD    0x0096c3b4   /* "CrowdEventName" */
#define S_EMITTER  0x0095db5c   /* "Emitter" */
#define S_DURATION 0x0093dd58   /* "Duration" */
#define S_ANCHOR   0x0096c3c4   /* "AnchorName" */
#define S_FSFX     0x0096c3a8   /* "FSFXType" */
#define S_AMOUNT   0x0096c3a0   /* "Amount" */
#define S_TIMELINE 0x0096c37c   /* "TimelineName" */
#define S_VISNODE  0x0096c38c   /* "VisNodeName" */
#define S_VISBOOL  0x0096c398   /* "VisBool" */

/* 0074c830 ScenarioEvent_Read (stdcall: event, parser, element; RET 0xc): the event's fields by its type (+0): names into +0x08 /
 * +0x48 / +0x88 (64 bytes each), the matrix +0xd0, ints +0xc8 / +0x110, the floats +0x114. Returns 1. */
uint32_t rx_scenario_event_read(rx_addr ev, rx_addr p, rx_addr el) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x10);                                    /* push ebx, ebp, esi, edi */
    switch (U32(ev, 0)) {
    case 0: case 14:                                          /* CameraAnimation, RiderAnimation */
        field_name(p, el, S_ANIM, ev + 0x48); break;
    case 1: field_name(p, el, S_MODEL, ev + 8); break;        /* SetScenarioOrigin */
    case 2: case 6: break;                                    /* ClearScenarioOrigin, ScenarioEnd */
    case 3: case 12:                                          /* ModelAnimation, CrewChiefAnimation */
        field_name(p, el, S_MODEL, ev + 8); field_name(p, el, S_ANIM, ev + 0x48); break;
    case 4: case 13:                                          /* ModelPosition, CrewChiefPosition */
        field_name(p, el, S_MODEL, ev + 8); field_matrix(p, el, ev); break;
    case 5: field_name(p, el, S_SOUND, ev + 8); field_matrix(p, el, ev); break;    /* SoundEffect */
    case 7: case 9: {                                         /* Particle / FireworkEffectTrigger */
        field_name(p, el, S_EMITTER, ev + 8);
        rx_addr at = xml_attr(p, el, S_DURATION);
        if (at) xml_read(p, XML_INT, at, ev + 0x110);
        break;
    }
    case 8: case 10:                                          /* Particle / FireworkEffectPosition */
        field_name(p, el, S_EMITTER, ev + 8);
        if (field_name(p, el, S_ANCHOR, ev + 0x88)) break;
        field_matrix(p, el, ev);
        SU8(ev, 0x88, 0);
        break;
    case 11: {                                                /* FullScreenEffect */
        rx_addr at = xml_attr(p, el, S_FSFX);
        if (at) xml_read(p, XML_INT, at, ev + 0xc8);
        at = xml_attr(p, el, S_DURATION);
        if (at) xml_read(p, XML_INT, at, ev + 0x110);
        field_matrix(p, el, ev);
        at = xml_attr(p, el, S_AMOUNT);
        if (at) xml_read(p, XML_FLOAT, at, ev + 0x114);
        break;
    }
    case 15: case 17: field_matrix(p, el, ev); break;         /* RiderPosition, VehiclePosition */
    case 16: case 19:                                         /* VehicleAnimation, PlaceEntity */
        field_name(p, el, S_MODEL, ev + 8); field_name(p, el, S_ANCHOR, ev + 0x88); field_name(p, el, S_ANIM, ev + 0x48); break;
    case 18: field_name(p, el, S_MODEL, ev + 8); break;       /* DisableEntity */
    case 20: field_name(p, el, S_CROWD, ev + 8); break;       /* CrowdReaction */
    case 22: field_name(p, el, S_TIMELINE, ev + 8); break;    /* PlayTimeline */
    case 23: field_name(p, el, S_MODEL, ev + 8); field_name(p, el, S_ANCHOR, ev + 0x88); break;   /* PlaceVehicle */
    default: {                                                /* 21 SetNodeVisibility (and any other type) */
        const rx_addr flag = sp0 + 4;                         /* the original reads the flag into its first argument's slot */
        rx_addr at = xml_attr(p, el, S_MODEL);
        if (at) {
            uint32_t a = at;
            rx_addr s = rx_call_original(U32(U32(p, 0), 0x110), RX_THISCALL, p, 1, &a);
            const uint32_t f[2] = {0x009419e4, s};
            const rx_addr sp = intro_callsite(2, f);
            rx_format_buffer(ev + 8, 0x40, 0x009419e4, rx_sp() + 8);
            rx_set_sp(sp);
        }
        at = xml_attr(p, el, S_VISNODE);
        if (at) {
            uint32_t a = at;
            rx_addr s = rx_call_original(U32(U32(p, 0), 0x110), RX_THISCALL, p, 1, &a);
            const uint32_t f[2] = {0x009419e4, s};
            const rx_addr sp = intro_callsite(2, f);
            rx_format_buffer(ev + 0x48, 0x40, 0x009419e4, rx_sp() + 8);
            rx_set_sp(sp);
        }
        SU32(flag, 0, 1);
        at = xml_attr(p, el, S_VISBOOL);
        if (at) xml_read(p, XML_INT, at, flag);
        SF32(ev, 0x114, (float)(I32(flag, 0) > 0));
        break;
    }
    }
    rx_set_sp(sp0);
    return 1;
}
RX_FUNCTION(0x0074c830, rx_scenario_event_read)

/* 0074cd80 ScenarioEvent_Parse (ESI = parser; stdcall: scenario, event, element; RET 0xc): the element's "Type" (one of the 24
 * names at 0x9b7e20, _stricmp) and "Time" (capped at the scenario's duration +0x15184), then its fields. 0 when either is
 * missing / unknown. */
uint32_t rx_scenario_event_parse(rx_addr p, rx_addr sc, rx_addr ev, rx_addr el) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);                                       /* push ebp */
    rx_addr at = xml_attr(p, el, 0x0093eae0 /* "Type" */);
    if (!at) { rx_set_sp(sp0); return 0; }
    rx_set_sp(sp0 - 0xc);                                     /* + ebx, edi */
    uint32_t a = at;
    rx_addr s = rx_call_original(U32(U32(p, 0), 0x110), RX_THISCALL, p, 1, &a);
    uint32_t t = 0;
    while (t < 0x18 && crt_stricmp(s, U32(0, 0x009b7e20 + 4 * t)) != 0) t++;
    uint32_t r = 0;
    if (t < 0x18) {
        SU32(ev, 0, t);
        at = xml_attr(p, el, 0x0094fb00 /* "Time" */);
        if (at && xml_read(p, XML_FLOAT, at, ev + 4)) {
            if (F32(ev, 4) > F32(sc, 0x15184)) SF32(ev, 4, F32(sc, 0x15184));
            rx_addr sp = rx_sp();
            SU32(sp - 0xc, 0, ev); SU32(sp - 8, 0, p); SU32(sp - 4, 0, el);
            rx_set_sp(sp - 0x10);
            r = rx_scenario_event_read(ev, p, el);
            rx_set_sp(sp);
        }
    }
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x0074cd80, rx_scenario_event_parse)

/* 0074ce40 ScenarioScript_Load (EAX = script parser, EDI = scenario): "NumEvents", "Duration" (+0x15184) and up to 300 "Event"
 * elements parsed into the events (0x120 each from the scenario, count +0x15180). Returns 1 when everything read. */
uint32_t rx_scenario_cast_load_script(rx_addr p, rx_addr sc) {
    if (p == 0) return 0;
    const rx_addr sp0 = rx_sp();
    const rx_addr num = sp0 - 4, dur = sp0 - 0xc, iter = sp0 - 8;
    rx_set_sp(sp0 - 0x18);                                    /* sub esp, 0xc; push esi, ebx, ebp */
    SU32(num, 0, 0);
    SU32(sc, 0x15180, 0);
    SF32(dur, 0, 0.0f);
    uint32_t ok = 1;
    rx_addr root = anim_vcall0(p, 4);
    if (!root) ok = 0;
    else {
        rx_addr at = xml_attr(p, root, 0x0096c3e8 /* "NumEvents" */);
        if (at && !xml_read(p, XML_INT, at, num)) ok = 0;
        at = xml_attr(p, root, S_DURATION);
        if (at) { if (xml_read(p, XML_FLOAT, at, dur)) SF32(sc, 0x15184, F32(dur, 0)); else ok = 0; }
        uint32_t a[3] = {root, 0x00946220 /* "Event" */, iter};
        rx_addr el = rx_call_original(U32(U32(p, 0), 0x20), RX_THISCALL, p, 3, a);
        while (el) {
            rx_addr sp = rx_sp();
            SU32(sp - 0xc, 0, sc); SU32(sp - 8, 0, sc + 0x120u * U32(sc, 0x15180)); SU32(sp - 4, 0, el);
            rx_set_sp(sp - 0x10);
            uint32_t r = rx_scenario_event_parse(p, sc, sc + 0x120u * U32(sc, 0x15180), el);
            rx_set_sp(sp);
            if (!r) ok = 0;
            SU32(sc, 0x15180, U32(sc, 0x15180) + 1);
            if (I32(sc, 0x15180) >= 0x12c) break;
            uint32_t it = U32(iter, 0);
            el = rx_call_original(U32(U32(p, 0), 0x28), RX_THISCALL, p, 1, &it);
        }
        uint32_t it = U32(iter, 0);
        rx_call_original(U32(U32(p, 0), 0x24), RX_THISCALL, p, 1, &it);
    }
    if (U32(sc, 0x15180) == 0) ok = 0;
    rx_set_sp(sp0);
    return ok;
}
RX_FUNCTION(0x0074ce40, rx_scenario_cast_load_script)

/* 0074cf70 ScenarioScript_Run (ESI = scenario; stdcall: dt, target; RET 8): at time 0 the events of time 0, then (while the clock
 * +0x15188 has not passed the duration) the clock advances and the events whose time falls in (before, now] run: their type's
 * handler (table 0x9c5120: thiscall target (event, flag +0x1518c)). The original keeps the previous clock in its argument slot. */
void rx_scenario_script_run(rx_addr sc, float dt, rx_addr target) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0xc);                                     /* push ebx, ebp, edi */
    if (F32(sc, 0x15188) == 0.0f)
        for (int32_t i = 0; i < I32(sc, 0x15180); i++) {
            rx_addr ev = sc + 0x120u * (uint32_t)i;
            if (!(F32(ev, 4) == 0.0f)) continue;
            rx_addr h = U32(0, 0x009c5120 + 4u * U32(ev, 0));
            if (h) { uint32_t a[2] = {ev, 0}; rx_call_original(h, RX_THISCALL, target, 2, a); }
        }
    float t = F32(sc, 0x15188);
    if (F32(sc, 0x15184) >= t) {
        SF32(sp0 + 8, 0, t);
        SF32(sc, 0x15188, t + dt);
        for (int32_t i = 0; i < I32(sc, 0x15180); i++) {
            rx_addr ev = sc + 0x120u * (uint32_t)i;
            float et = F32(ev, 4);
            if (!(et > F32(sp0 + 8, 0)) || !(F32(sc, 0x15188) >= et)) continue;
            rx_addr h = U32(0, 0x009c5120 + 4u * U32(ev, 0));
            if (h) { uint32_t a[2] = {ev, U32(sc, 0x1518c)}; rx_call_original(h, RX_THISCALL, target, 2, a); }
        }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074cf70, rx_scenario_script_run)

/* ---- name lookups for the intro script ---- */

static int32_t crt_atoi_iat(rx_addr s) {                                  /* atoi (IAT 0x916220) */
    SU32(rx_sp() - 4, 0, s); return crt_atoi(s);   /* atoi (IAT 0x916220) */
}
static uint32_t activity_count_with(rx_addr a, uint32_t v) {               /* 00559e20: EDX = activity, EDI = value */
    const rx_addr sp0 = intro_callsite(0, 0);
    const uint32_t r = rx_activity_count_humans_of(a, v);
    rx_set_sp(sp0);
    return r;
}
/* A prefix match (_strnicmp over the prefix's length); the text after it. */
static int has_prefix(rx_addr name, rx_addr prefix) { return crt_strnicmp(name, prefix, crt_strlen(prefix)) == 0; }

/* 0074b750 ScenarioAnims_FindPlayerEntity (EAX = name, stack: SM, out; RET 8): the names "HumanPlayerLost<n>",
 * "HumanPlayer<n>", "PlayerFinished<n>" (riders, through the SM's cast entries: their first dword) and "PlayerCC<n>" (the
 * cameras) put the entity in *out; returns 1 when the name has one of the prefixes (whatever the number). The original keeps
 * that flag in its first argument's slot. */
uint32_t rx_scenario_anims_find_player_entity(rx_addr name, rx_addr sm, rx_addr out) {
    const rx_addr sp0 = rx_sp(), flag = sp0 + 4;
    if (U32(sm, 0x40) == 0) return 0;
    SU32(flag, 0, 0);
    rx_set_sp(sp0 - 0x10);                                    /* push ebp, edi, ebx, esi */
    rx_addr a = U32(sm, 0x40);                                /* the activity */
    rx_addr found = 0;
    if (has_prefix(name, 0x0096cca8 /* "HumanPlayerLost" */)) {
        SU32(flag, 0, 1);
        rx_addr num = name + crt_strlen(0x0096cca8);
        if (num) {
            uint32_t n = (uint32_t)(crt_atoi_iat(num) - 1);
#ifdef RX_ORACLE
            rx_regs r = anim_regs(), o; r.r[ANIM_ECX] = sm;        /* 0074b6e0: the n-th human rider who lost */
            rx_call_original_ex(0x0074b6e0, &r, &o, 1, &n);
            if (o.r[ANIM_EAX]) {
                ANIM_CALL_FRAME(4); rx_addr x = rx_scenario_cast_find(sm, o.r[ANIM_EAX]); ANIM_CALL_FRAME_END(4);
                found = U32(x, 0);
            }
#else
            /* Product rx_call_original_ex is deliberately fail-closed. This
             * exact fixed callee is now a complete typed canonical owner. */
            const rx_addr caller = intro_callsite(1, &n);
            SU32(rx_sp(), 0, 0x0074b7ca); /* actual original return address */
            const rx_addr player = rx_scenario_cast_losing_human(sm, n);
            rx_set_sp(caller);
            if (player) {
                ANIM_CALL_FRAME(4); rx_addr x = rx_scenario_cast_find(sm, player); ANIM_CALL_FRAME_END(4);
                found = U32(x, 0);
            }
#endif
        }
    } else if (has_prefix(name, 0x0096cc8c /* "HumanPlayer" */)) {
        SU32(flag, 0, 1);
        rx_addr num = name + crt_strlen(0x0096cc8c);
        if (num) {
            int32_t n = crt_atoi_iat(num) - 1;
            if (n >= 0 && n < (int32_t)activity_count_with(a, 1)) {
                rx_addr rider = activity_rider(a, (uint32_t)n);
                ANIM_CALL_FRAME(4); rx_addr x = rx_scenario_cast_find(sm, U32(rider, 0x74)); ANIM_CALL_FRAME_END(4);
                found = U32(x, 0);
            }
        }
    } else if (has_prefix(name, 0x0096cc98 /* "PlayerFinished" */)) {
        SU32(flag, 0, 1);
        rx_addr num = name + crt_strlen(0x0096cc98);
        if (num) {
            int32_t n = crt_atoi_iat(num) - 1;
            if (n >= 0 && (uint32_t)n < 12) {
                rx_addr rider = rx_stand(U32(sm, 0x40), (uint32_t)n);   /* the finish order */
                if (rider) {
                    ANIM_CALL_FRAME(4); rx_addr x = rx_scenario_cast_find(sm, U32(rider, 0x74)); ANIM_CALL_FRAME_END(4);
                    found = U32(x, 0);
                }
            }
        }
    } else if (has_prefix(name, 0x0096ccb8 /* "PlayerCC" */)) {
        SU32(flag, 0, 1);
        rx_addr num = name + crt_strlen(0x0096ccb8);
        if (num) {
            int32_t n = crt_atoi_iat(num) - 1;
            if (I32(U32(sm, 0x40), 0x26c) > n) found = U32(sm, 0x16ec + 4u * (uint32_t)n);
        }
    }
    if (found) SU32(out, 0, found);
    rx_set_sp(sp0);
    return U32(flag, 0);
}
RX_FUNCTION(0x0074b750, rx_scenario_anims_find_player_entity)

/* 0074b970 ScenarioAnims_FindEntity (ESI = SM, EDI = name): a player name (0074b750), the active camera (+0x1730) or the rider
 * camera (+0x290) by name, a camera, an actor, a prop, or any scene entity of that name; 0 when none. */
rx_addr rx_scenario_anims_find_entity(rx_addr sm, rx_addr name) {
    if (U32(sm, 0x40) == 0) return 0;
    const rx_addr sp0 = rx_sp(), found = sp0 - 4;
    rx_set_sp(found);                                         /* push ecx: the result slot */
    SU32(found, 0, 0);
    ANIM_CALL_FRAME(8); uint32_t named = rx_scenario_anims_find_player_entity(name, sm, found); ANIM_CALL_FRAME_END(8);
    rx_addr r = U32(found, 0);
    if (r != 0 || named != 0) { rx_set_sp(sp0); return r; }
    rx_set_sp(sp0 - 8);                                       /* push ebx */
    rx_addr e = U32(sm, 0x1730);
    if (e && crt_stricmp(name, U32(e, 0x18)) == 0) r = U32(sm, 0x1730);
    else if ((e = U32(sm, 0x290)) && crt_stricmp(name, U32(e, 0x18)) == 0) r = U32(sm, 0x290);
    else {
        ANIM_CALL_FRAME(8); r = rx_scenario_anims_find_camera(sm, name); ANIM_CALL_FRAME_END(8);
        if (!r) { ANIM_CALL_FRAME(8); r = rx_scenario_anims_find_actor(sm, name); ANIM_CALL_FRAME_END(8); }
        if (!r) {
            rx_addr w = U32(U32(sm, 0x40), 0x234);
            ANIM_CALL_FRAME(8); r = rx_scenario_anims_find_prop(w + 0x20150, name); ANIM_CALL_FRAME_END(8);
        }
        if (!r) {
            rx_addr w = U32(U32(sm, 0x40), 0x234);
            ANIM_CALL_FRAME(8); r = rx_scene_manager_find_named(U32(w, 0x20138), name); ANIM_CALL_FRAME_END(8);
        }
    }
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x0074b970, rx_scenario_anims_find_entity)

/* ---- the race intro scenario ---- */

static uint32_t f32_quiet_bits(float f) {                     /* fld / fstp of a float: a signalling NaN comes out quiet */
    uint32_t b; memcpy(&b, &f, 4);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) b |= 0x00400000u;
    return b;
}
static float bits_f32(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }

/* 0080c880 LocSoundList_StopOwner (stdcall owner name or 0; RET 4): the localized sound list [00a957d4] (+0 the records, +4 the count; 0x1a4
 * bytes each): every record of that owner (+0x134, _stricmp; every record when the owner is 0 or empty) is stopped: one with an FMOD event
 * (+0x118) whose flags (+0x13c) have bit 3 is stopped now on the audio thread (0080ae10, ESI = the record) or by the queued command 1
 * (0080c280, its EBP = the audio thread id the function loaded into it); any other is released to the list (0080c810, EAX = the record,
 * stack the list). Frame: sub esp, 8; push ebx / ebp / esi / edi; the index at sp0 - 8. */
void rx_loc_sound_list_stop_owner(rx_addr owner) {
    const rx_addr sp0 = rx_sp();
    const uint32_t ebp0 = anim_reg_get(ANIM_EBP), esi0 = anim_reg_get(ANIM_ESI);
    SU32(sp0, -0xc, anim_reg_get(ANIM_EBX));
    SU32(sp0, -0x10, anim_reg_get(ANIM_EBP));
    SU32(sp0, -0x14, anim_reg_get(ANIM_ESI));
    SU32(sp0, -0x18, anim_reg_get(ANIM_EDI));
    rx_set_sp(sp0 - 0x18);
    const rx_addr list = U32(0, 0x00a957d4);
    if (U32(list, 0) != 0 && U32(list, 4) != 0) {
        uint32_t offset = 0;
        SU32(sp0, -8, 0);
        for (uint32_t i = 0; i < U32(list, 4); i++, offset += 0x1a4u) {
            if (owner != 0 && crt_strlen(owner) != 0) {
                const rx_addr name = U32(U32(list, 0) + offset, 0x134);
                if (name == 0 || crt_stricmp(name, owner) != 0) { SU32(sp0, -8, i + 1); continue; }
            }
            const rx_addr rec = U32(list, 0) + offset;
            if (U32(rec, 0x118) != 0 && (U8(rec, 0x13c) & 8) != 0) {
                const uint32_t audio_thread = U32(0, 0x00d67d1c);
                anim_reg_set(ANIM_EBP, audio_thread);
                if (audio_thread == rx_os_import(0x0091607c, 0, 0)) {
                    anim_reg_set(ANIM_ESI, rec);
                    ANIM_CALL_FRAME(0); rx_sound_stop_now(rec); ANIM_CALL_FRAME_END(0);      /* 0080ae10 */
                } else {
                    ANIM_CALL_FRAME(0x10); rx_world_event_queue(1, rec, 0, 0, audio_thread); ANIM_CALL_FRAME_END(0x10);   /* 0080c280 */
                }
            } else {
                ANIM_CALL_FRAME(4); rx_world_event_release(rec, list); ANIM_CALL_FRAME_END(4);   /* 0080c810 */
            }
            SU32(sp0, -8, i + 1);
        }
    }
    anim_reg_set(ANIM_EBP, ebp0);
    anim_reg_set(ANIM_ESI, esi0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0080c880, rx_loc_sound_list_stop_owner)

/* 007473f0 IntroScenario_Update (stdcall: scenario, dt; RET 8): unless the game is paused (+0x0c of the game's +0x10) or the
 * scenario is done (vf8 +0x20): the clock +0x10 advances (vf2 dt), the attachments follow, the script runs (+0x20, clock
 * +0x151a8, duration +0x151a4) with the activity as target; a pending fade (+0x151b8) starts once the clock is within +0x151bc of
 * the end. While the script runs (no skip by the first player's controller, no skip request 0xa7f748) the level's crowd / camera
 * (world +0x218f0) ticks; else the intro ends: fade reset, the SM's actors / cameras / cast stopped, attachments cleared, the rider
 * camera stopped, the scenario base reset, then vf4 (skipped) / vf3: a next intro (00747720 with +0x151b4) or vf6 and the race
 * start (00757660). */
void rx_scenario_intro_update(rx_addr sc, float dt) {
    ANIM_FRAME_ENTER(A, 0x20);                                /* push ebp; and esp, -16; sub esp, 0x14; push ebx, esi, edi */
    const rx_addr ended = A - 0x14, white = A - 0x10;
    const uint32_t dtq = f32_quiet_bits(dt);
    if (U32(U32(U32(0, 0x00a95844), 0x10), 0xc) != 0 || anim_vcall0(sc, 0x20) != 0) { ANIM_FRAME_LEAVE(A); return; }
    SF32(sc, 0x10, rx_adds(F32(sc, 0x10), dt));
    { uint32_t a = dtq; rx_call_original(U32(U32(sc, 0), 8), RX_THISCALL, sc, 1, &a); }
    rx_addr sm = U32(U32(sc, 0x14), 0x234) + 0x20150;
    ANIM_CALL_FRAME(4); rx_scenario_anims_update_attachments(sm); ANIM_CALL_FRAME_END(4);
    ANIM_CALL_FRAME(8); rx_scenario_script_run(sc + 0x20, bits_f32(dtq), U32(sc, 0x14)); ANIM_CALL_FRAME_END(8);
    uint32_t skip = 0;
    if (U32(sc, 0x151b8) != 0 && F32(sc, 0x151a8) >= rx_subs(F32(sc, 0x151a4), F32(sc, 0x151bc))) {
        ANIM_CALL_FRAME(4);                                                 /* 0055e990 (ECX = the page name, stack the activity; RET 4) */
        rx_show_page_wipe(0x0095b7c4, U32(sc, 0x14));                       /* the fade-out */
        ANIM_CALL_FRAME_END(4);
    }
    {
        rx_addr r0 = activity_rider(U32(sc, 0x14), 0);
        uint32_t pad = U32(U32(r0, 0x74), 0x1f0);
        rx_addr in = U32(0, 0x009c52c4 + 4 * pad) + 0x32fc;                 /* that player's input: button 7 pressed */
        uint32_t a[2] = {7, pad};
        if (rx_call_original(U32(U32(in, 0), 4), RX_THISCALL, in, 2, a) != 0 && anim_vcall0(sc, 0x14) != 0) SU32(ended, 0, 1);
        else SU32(ended, 0, 0);
    }
    if (U32(0, 0x00a7f748) != 0 && anim_vcall0(sc, 0x14) != 0 && U32(U32(sc, 0x14), 0xff06c) == 0) skip = 1;
    const int over = F32(sc, 0x20 + 0x15188) > F32(sc, 0x20 + 0x15184);
    SU32(0, 0x00a7f748, 0);
    if (!over && U32(ended, 0) == 0 && !skip) {
        rx_addr crowd = U32(U32(sc, 0x14), 0x234) + 0x218f0;
        {   /* vf19 (0063d7e0: a float argument, so the registry's integer table cannot dispatch it: typed here). The original's
             * call is `push ecx; fstp [esp]; call`: the dt is READ FROM THE ARGUMENT SLOT by the port (its first act), so the bits
             * must be written there (a typed C call alone would leave the port's dt stale). */
            const rx_addr target = U32(U32(crowd, 0), 0x4c);
            void (*f)(rx_addr, float) = RX_FN(target, void (*)(rx_addr, float));
            if (f) {
                const rx_addr s = rx_sp();
                SU32(s - 4, 0, dtq);                        /* the argument slot (the pushed dt) */
                SU32(s - 8, 0, 0);                          /* the return-address slot */
                rx_set_sp(s - 8u);
                f(crowd, bits_f32(dtq));
                rx_set_sp(s);
            } else { uint32_t a = dtq; rx_call_original(target, RX_THISCALL, crowd, 1, &a); }
        }
        rx_addr act = U32(sc, 0x14);
        if (activity_rider(act, 0) == 0) { ANIM_FRAME_LEAVE(A); return; }
        if (U8(U32(activity_rider(act, 0), 0x74), 0x21c) != 0) { ANIM_FRAME_LEAVE(A); return; }
        rx_addr w = U32(act, 0x234);
        { const uint32_t a[2] = {w + 0x218f0, U32(U32(w, 0x20138), 0x19b8)};
          const rx_addr sp = intro_callsite(2, a); rx_cam_entity_to_view(a[0], a[1]); rx_set_sp(sp); }
        rx_addr cam = U32(U32(U32(U32(U32(sc, 0x14), 0x234), 0x20138), 0x19b8), 0x64);
        if (cam) SF32(cam, 0x254, 0.0f);
        ANIM_FRAME_LEAVE(A);
        return;
    }
    for (uint32_t k = 0; k < 16; k += 4) SU32(white, k, U32(0, 0x00931f24));
    { const rx_addr sp = intro_callsite(0, 0); rx_cameras_fade(white); rx_set_sp(sp); }
    SU32(sc, 0x151b8, 0);
    sm = U32(U32(sc, 0x14), 0x234) + 0x20150;
    ANIM_CALL_FRAME(0); rx_scenario_anims_stop_actors(sm); ANIM_CALL_FRAME_END(0);
    sm = U32(U32(sc, 0x14), 0x234) + 0x20150;
    ANIM_CALL_FRAME(0); rx_scenario_anims_reset_cameras(sm); ANIM_CALL_FRAME_END(0);
    sm = U32(U32(sc, 0x14), 0x234) + 0x20150;
    ANIM_CALL_FRAME(0); rx_scenario_cast_stop_all(sm); ANIM_CALL_FRAME_END(0);
    sm = U32(U32(sc, 0x14), 0x234) + 0x20150;
    ANIM_CALL_FRAME(0); rx_scenario_anims_clear_attachments(sm); ANIM_CALL_FRAME_END(0);
    rx_addr rc = U32(U32(U32(sc, 0x14), 0x234), 0x203e0);    /* the SM's rider camera (+0x290) */
    if (rc) { SU8(rc, 0xa2, 0); SU32(rc, 0x70, 0); }
    { const rx_addr sp = intro_callsite(0, 0); rx_pose_overrides_clear(); rx_set_sp(sp); }
    { const uint32_t a = U32(sc, 0x14);
      const rx_addr sp = intro_callsite(1, &a); rx_activity_finish_sequences(U32(sc, 0x14)); rx_set_sp(sp); }
    uint32_t next;
    if (skip) {
        if (U32(0, 0x00a957d4) != 0) {
            SU32(rx_sp(), -4, 0x009419c4);                                   /* push the owner name */
            ANIM_CALL_FRAME(4); rx_loc_sound_list_stop_owner(0x009419c4); ANIM_CALL_FRAME_END(4);   /* 0080c880 */
        }
        next = anim_vcall0(sc, 0x10);
    } else next = anim_vcall0(sc, 0xc);
    if (next) {
        ANIM_CALL_FRAME(0); rx_scenario_begin(sc, U32(sc, 0x151b4)); ANIM_CALL_FRAME_END(0);
        ANIM_FRAME_LEAVE(A);
        return;
    }
    anim_vcall0(sc, 0x18);
    uint32_t humans = activity_count_with(U32(sc, 0x14), 1);
#ifdef RX_ORACLE
    /* 00757660 (MXGame viewports): the scene viewports / render targets for the player count (render setup: nothing in the game
     * state reads it; core/plan/QUEUE.md, miscport). PRODUCT BOUNDARY: the oracle runs the original, the product does nothing. */
    { uint32_t a[2] = {U32(0, 0x00a946b8), humans}; rx_call_original(0x00757660, RX_STDCALL, 0, 2, a); }
#else
    (void)humans;
#endif
    rx_addr cam = U32(U32(U32(U32(U32(sc, 0x14), 0x234), 0x20138), 0x19b8), 0x64);
    if (cam) SU32(cam, 0x254, U32(0, 0x00931d94));
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x007473f0, rx_scenario_intro_update)

/* ---- the intro script's name ---- */

static void cat(rx_addr buf, const char* s) {                 /* the inlined strcat of a literal */
    rx_addr d = buf + crt_strlen(buf);
    do SU8(d++, 0, (uint8_t)*s); while (*s++);
}
static void cat_game(rx_addr buf, rx_addr s) {                /* strcat of a string in game memory */
    rx_addr d = buf + crt_strlen(buf);
    uint8_t c;
    do { c = U8(s++, 0); SU8(d++, 0, c); } while (c);
}
static int same(rx_addr a, const char* s) {                   /* the inlined strcmp == 0 */
    for (;; a++, s++) { if (U8(a, 0) != (uint8_t)*s) return 0; if (!*s) return 1; }
}

/* The level (world +0x20030) to the intro script's track part (its scripts are shared between the variants of a level). */
static const struct { const char* level; const char* track; } k_intro_tracks[] = {
    {"FR_Alpine", "AL_AF"}, {"FR_Cheyenne", "CH_AF"}, {"FR_Gibson", "GI_AF"}, {"FMX_BuenaVista", "BV_AF"},
    {"FMX_BuenaVistaB", "BV_BF"}, {"FMX_BuenaVistaC", "BV_CF"}, {"FMX_Dover", "DO_AF"}, {"FMX_DoverB", "DO_BF"},
    {"NAT_Bedford", "BE_AF"}, {"NAT_BedfordB", "BE_BF"}, {"NAT_Concord", "CO_AF"}, {"NAT_ConcordB", "CO_BF"},
    {"NAT_Dalton", "DA_AF"}, {"NAT_DaltonB", "DA_BF"}, {"NAT_Renne", "RE_AF"}, {"NAT_RenneB", "RE_BF"},
    {"NAT_Stillwater", "ST_AF"}, {"NAT_StillwaterB", "ST_BF"}, {"PX_Laguna", "LA_AF"}, {"PX_Laguna_BW", "LA_AB"},
    {"PX_LagunaB", "LA_BF"}, {"PX_LagunaB_BW", "LA_BB"}, {"SX_FortDodge", "FD_AF"}, {"SX_FortDodgeB", "FD_BF"},
    {"SX_Fresno", "FR_AF"}, {"SX_FresnoB", "FR_BF"}, {"SX_Kingston", "KI_AF"}, {"SX_KingstonB", "KI_BF"},
    {"FMX_Kingston", "KI_AF"}, {"SX_Manchester", "MA_AF"}, {"SX_ManchesterB", "MA_BF"}, {"SX_Springfield", "SP_AF"},
    {"SX_SpringfieldB", "SP_BF"}, {"FMX_Springfield", "SP_AF"}, {"FMX_SpringfieldB", "SP_BF"}, {"TRX_Halifax", "HA_AF"},
    {"TRX_HalifaxBW", "HA_AB"}, {"TRX_Lynchburg", "LY_AF"}, {"TRX_LynchburgBW", "LY_AB"}, {"TRX_Verde", "VE_AF"},
    {"TRX_VerdeBW", "VE_AB"}, {"UI_Level", "UI_AF"}, {"UI_Compound", "COM_AF"}, {"UTVM_Shelby", "SH_AF"},
    {"UTVM_Shelby_BW", "SH_AB"}, {"UTVM_ShelbyB", "SH_BF"}, {"UTVM_ShelbyB_BW", "SH_BB"}, {"SM_Shelby", "SH_AF"},
    {"SM_Shelby_BW", "SH_AB"}, {"SM_ShelbyB", "SH_BF"}, {"SM_ShelbyB_BW", "SH_BB"}, {"WP_Alpine", "AL_AF"},
    {"WP_Cheyenne", "CH_AF"}, {"WP_Gibson", "GI_AF"},
};
/* The scenario kinds 1..14 (the third part). */
static const char* const k_intro_kinds[14] = {
    "Intro", "StartLine", "StartStaggered", "HeroBike", "HeroVehicle", "30SecGirl", "WinFinish", "WinMound", "WinPodium",
    "WinHeatChief", "LoseWalk", "LoseHeatChief", "2Heroes", "OutrunLose",
};

/* 00747940 Scenario_IntroScriptName (stdcall: kind, activity, buffer; RET 0xc): "<series>_<track>_<kind>" into the 0x104-byte
 * buffer. The series by the activity's event type (+0x23c): NAT, SX, SM, FMX, FMJ, PX, TX, UTV, FR, WP<n> (+0x220), the level
 * name's first three letters for type 12, else X; the track by the level name (the table; else the five letters after its first
 * '_', or X_X); the kind's name (none for other kinds). */
void rx_scenario_intro_script_name(uint32_t kind, rx_addr a, rx_addr buf) {
    const rx_addr sp0 = rx_sp(), num = sp0 - 0x14, three = sp0 - 0x10;
    SU32(sp0 - 4, 0, U32(0, 0x009b45e4) ^ (sp0 - 0x14));    /* the security cookie */
    rx_set_sp(sp0 - 0x24);                                    /* sub esp, 0x14; push ebx, ebp, esi, edi */
    crt_memset(buf, 0, 0x104);
    switch (U32(a, 0x23c)) {
    case 0: case 0x22: case 0x23: case 0x24: case 0x25: cat(buf, "NAT"); break;
    case 1: case 0x26: case 0x27: case 0x28: case 0x29: cat(buf, "SX"); break;
    case 3: case 0x32: case 0x33: case 0x34: case 0x35: cat(buf, "SM"); break;
    case 5: case 0x20: case 0x3a: case 0x3b: case 0x3c: cat(buf, "FMX"); break;
    case 0x14: cat(buf, "FMJ"); break;
    case 0x13: cat(buf, "PX"); break;
    case 0x11: cat(buf, "TX"); break;
    case 0x12: cat(buf, "UTV"); break;
    case 8: cat(buf, "FR"); break;
    case 6: case 0x21: case 0x30: case 0x31: {               /* WP and the round's digit ("%c", '1' + n) */
        cat(buf, "WP");
        const uint32_t f[2] = {0x009660c0 /* "%c" */, U32(a, 0x220) + 0x31};
        const rx_addr sp = intro_callsite(2, f);
        rx_format_buffer(num, 2, 0x009660c0 /* "%c" */, rx_sp() + 8);
        rx_set_sp(sp);
        cat_game(buf, num);
        break;
    }
    case 0xc: {                                               /* the level's own series: its name's first three letters */
        rx_addr lv = U32(a, 0x234) + 0x20030;
        const uint32_t f[4] = {0x009420e4 /* "%c%c%c" */, (uint32_t)(int8_t)U8(lv, 0), (uint32_t)(int8_t)U8(lv, 1),
                               (uint32_t)(int8_t)U8(lv, 2)};
        const rx_addr sp = intro_callsite(4, f);
        rx_format_buffer(three, 4, 0x009420e4 /* "%c%c%c" */, rx_sp() + 8);
        rx_set_sp(sp);
        if (crt_stricmp(three, 0x009420ec /* "NAT" */) == 0) cat(buf, "NAT");
        else if (crt_stricmp(three, 0x009420f0 /* "SX_" */) == 0) cat(buf, "SX");
        else if (crt_stricmp(three, 0x009420f4 /* "PX_" */) == 0) cat(buf, "PX");
        else if (crt_stricmp(three, 0x009420f8 /* "TX_" */) == 0) cat(buf, "TX");
        else if (crt_stricmp(three, 0x0093c734 /* "UTV" */) == 0) cat(buf, "UTV");
        else cat(buf, "X");
        break;
    }
    default: cat(buf, "X"); break;
    }
    cat(buf, "_");
    rx_addr lv = U32(a, 0x234) + 0x20030;
    uint32_t i = 0;
    const uint32_t n = sizeof k_intro_tracks / sizeof k_intro_tracks[0];
    while (i < n && !same(lv, k_intro_tracks[i].level)) i++;
    if (i < n) cat(buf, k_intro_tracks[i].track);
    else {
        rx_addr u = crt_strchr(lv, '_');
        if (u) {                                              /* the five bytes after it, terminated */
            SU32(three, 0, U32(u, 1)); SU8(three, 4, U8(u, 5)); SU8(three, 5, 0);
            cat_game(buf, three);
        } else cat(buf, "X_X");
    }
    cat(buf, "_");
    if (kind - 1 < 14) cat(buf, k_intro_kinds[kind - 1]);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00747940, rx_scenario_intro_script_name)

/* ---- full-screen effects and the scenario cast ---- */

/* 0040b4e0 OverlayColorEntity_StartBlend (ESI = colour A, EDX = colour C... as the screen effect passes them: ESI -> +0x120, EDX ->
 * +0x130, ECX -> +0x140 (4 floats each, copied through the x87); xmm0 / xmm1 the two midpoints, stored larger first as +0x150 / +0x154;
 * stdcall entity, duration; RET 8): active (+0x10c = 1), duration +0x110; the per-second rates +0x160.. (target - current) / duration:
 * +0x160 from +0x90 (four lanes), +0x170 from +0xa0, +0x180 from +0xb0, +0x190 from +0xc0 / +0x194 from +0xc4 (the last two
 * multiply by the reciprocal in the other operand order). SSE single precision. Frame: push ebp; and esp, -16. */
void rx_overlay_color_start_blend(rx_addr ent, float duration, rx_addr esi, rx_addr edx, rx_addr ecx, float x0, float x1) {
    ANIM_FRAME_ENTER(A, 0);
    float hi = x0, lo = x1;
    if (x1 > x0) { hi = x1; lo = x0; }
    SU32(ent, 0x10c, 1);
    SF32(ent, 0x110, duration);
    for (uint32_t i = 0; i < 16; i += 4) {
        anim_x87_copy_f32(ent + 0x120 + i, esi + i);
        anim_x87_copy_f32(ent + 0x130 + i, edx + i);
        anim_x87_copy_f32(ent + 0x140 + i, ecx + i);
    }
    SF32(ent, 0x150, hi); SF32(ent, 0x154, lo);
    for (uint32_t g = 0; g < 3; g++) {
        const uint32_t t = 0x120 + 0x10 * g, c = 0x90 + 0x10 * g, r = 0x160 + 0x10 * g;
        float d[4];
        for (uint32_t k = 0; k < 4; k++) d[k] = rx_subs(F32(ent, t + 4 * k), F32(ent, c + 4 * k));
        const float inv = rx_divs(1.0f /* 0x931f24 */, F32(ent, 0x110));
        for (uint32_t k = 0; k < 4; k++) SF32(ent, r + 4 * k, rx_muls(inv, d[k]));
    }
    const float d4 = rx_subs(F32(ent, 0x150), F32(ent, 0xc0)), d5 = rx_subs(F32(ent, 0x154), F32(ent, 0xc4));
    const float inv = rx_divs(1.0f, F32(ent, 0x110));
    SF32(ent, 0x190, rx_muls(d4, inv));
    SF32(ent, 0x194, rx_muls(d5, inv));
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0040b4e0, rx_overlay_color_start_blend)

/* 0074b200 Scenario_ScreenEffect (EAX = duration in ms; stack: mode, then 14 floats at `f`: colour A (4), colour C (4), colour B
 * (4), two more; RET 0x3c): on the active view's overlay (+0x19b8 of the current view): mode 0 fades from clear to white, mode 1
 * from white (overlay +0x70 state: +0x10.. the blend, its time max(ms * 0.001, K)); mode 2 blends every overlay colour entity
 * (type 0x1063) of the level, the last one found, from the colours over that time (OverlayColorEntity_StartBlend 0040b4e0). */
void rx_scenario_screen_effect(int32_t ms, uint32_t mode, rx_addr f) {
    ANIM_FRAME_ENTER(A, 0x440);                               /* push ebp; and esp, -16; sub esp, 0x43c; push esi */
    const rx_addr base = A - 0x440;
    rx_addr mgr = U32(0, 0x00a95844);
    rx_addr ov = U32(U32(mgr, 0x1c + 4 * U32(mgr, 4)), 0x19b8);
    if (ov == 0 || mode > 2) { ANIM_FRAME_LEAVE(A); return; }
    const float k = F32(0, 0x009322ac);
    float t = rx_muls((float)ms, F32(0, 0x00977470) /* 0.001 */);
    if (mode < 2) {
        rx_addr st = U32(ov, 0x70);
        SU32(st, 0, 0);
        rx_addr b = st + 0x10;
        if (b != 0) {
            SU32(b, 0x30, 1);
            if (mode == 0) { SF32(b, 0x20, 0.0f); SF32(b, 0x24, 0.0f); SF32(b, 0x28, 0.0f); SU32(b, 0x2c, U32(0, 0x00931f24)); }
            else for (uint32_t i = 0; i < 16; i += 4) SU32(b, 0x20 + i, U32(0, 0x00931f24));
            SF32(b, 0xc, t > k ? t : k);
            SF32(b, 0x10, 0.0f); SF32(b, 0x18, 0.0f);
            SU32(b, 8, 1); SU32(b, 4, 1);
        }
        ANIM_FRAME_LEAVE(A);
        return;
    }
    const rx_addr flag = base + 0xc, list = base + 0x40;
    SU32(list, 0, 0);
    crt_memset(list + 4, 0, 0x3f8);
    ANIM_CALL_FRAME(0); const rx_addr world = rx_game_current_world(); ANIM_CALL_FRAME_END(0);   /* 0075c530 */
    rx_addr scene = U32(world, 0x20138);
    ANIM_CALL_FRAME(0x10); int32_t n = (int32_t)rx_scene_manager_collect_typed(scene, 0x1063, list, 0xff, flag); ANIM_CALL_FRAME_END(0x10);
    if (n > 0) {
        SF32(flag, 0, t > k ? t : k);
        const rx_addr ca = base + 0x20, cc = base + 0x30, cb = base + 0x10;
        for (uint32_t i = 0; i < 16; i += 4) {
            SU32(ca, i, U32(f, i)); SU32(cc, i, U32(f, 0x10 + i)); SU32(cb, i, U32(f, 0x20 + i));
        }
        const uint32_t dur = U32(flag, 0), ent = U32(list, 4u * (uint32_t)(n - 1));
        const rx_addr sp0 = rx_sp();
        SU32(sp0, -4u, dur); SU32(sp0, -8u, ent); SU32(sp0, -0xcu, 0x0074b360u);   /* push duration; push entity; call */
        rx_set_sp(sp0 - 0xcu);
        rx_overlay_color_start_blend(ent, rx_bf(dur), ca, cb, cc, F32(f, 0x30), F32(f, 0x34));
        rx_set_sp(sp0);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0074b200, rx_scenario_screen_effect)

/* The game's thread gate (game +0x10: owner thread +0xcc, event +0xdc): off the owning thread, wait for / signal the event. */
static void game_gate_wait(void) {
    rx_addr g = U32(U32(0, 0x00a95844), 0x10);
    if (rx_os_import(0x0091607c, 0, 0) != U32(g, 0xcc)) {
        uint32_t a[2] = {U32(g, 0xdc), 0xffffffffu}; rx_os_import(0x00916008, 2, a);
    }
}
static void game_gate_signal(void) {
    rx_addr g = U32(U32(0, 0x00a95844), 0x10);
    if (rx_os_import(0x0091607c, 0, 0) != U32(g, 0xcc)) {
        uint32_t a = U32(g, 0xdc);
        if (rx_os_import(0x00916034, 1, &a) == 0) rx_os_import(0x00916078, 0, 0);
    }
}

/* 0074bd50 ScenarioCast_Release (stdcall: SM; RET 4): behind the game's thread gate, the 12 cast entries' scenario entities
 * (+0x50 + i * 0x30) leave the scene (SceneManager_RemoveEntity), vf9, and are deleted; the entries cleared. The original keeps the
 * entries' address in its argument slot. */
void rx_scenario_cast_release(rx_addr sm) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0xc);                                     /* push ebp, esi, edi */
    game_gate_wait();
    SU32(sp0 + 4, 0, sm + 0x50);
    for (uint32_t i = 0; i < RX_RC_MAX; i++) {   /* (w-rc-core) entries 12..31 in side storage */
        rx_addr p = rx_cast_rec_ro(sm, i);
        if (!p || U32(p, 0) == 0) continue;
        SU8(U32(p, 0), 0xa2, 0);
        rx_addr e = U32(p, 0);
        ANIM_CALL_FRAME(4); rx_scene_manager_remove_entity(e, U32(e, 0x10)); ANIM_CALL_FRAME_END(4);
        anim_vcall0(U32(p, 0), 0x24);
        rx_addr x = U32(p, 0);
        if (x) {
            uint32_t one = 1;
            rx_call_original(U32(U32(x, 0), 0), RX_THISCALL, x, 1, &one);
            SU32(p, 0, 0);
        }
    }
    crt_memset(sm + 0x50, 0, 0x240);
    rx_rc_act_clear(sm);   /* (w-rc-core) the cast's entries 12..31 */
    game_gate_signal();
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074bd50, rx_scenario_cast_release)

/* 0074be10 ScenarioCast_PlaceEntity (thiscall ECX = SM, EDX = an alias to avoid (passed on to 006b6a30); stack: scenario entity,
 * placement (an entity, or 0), animation name, loops; RET 0x10): the name through the alias table (its flags byte); flag 2: the
 * entity rides with its rider's vehicle (attached; with a placement, the vehicle is put on the ground under the placement's
 * position (world vf20 / 0045f600) and reset, and plays "<name>_BIKE" / "_ATV" (0047c310) for bikes and ATVs); else the entity
 * is detached and takes the placement's matrix. Then the activity's event (0055e360) on the name. */
void rx_scenario_cast_place_entity(rx_addr sm, rx_addr avoid, rx_addr e, rx_addr place, rx_addr name, uint32_t loops) {
    ANIM_FRAME_ENTER(A, 0x1c0);                               /* push ebp; and esp, -16; sub esp, 0x1b4; push ebx, esi, edi */
    const rx_addr base = A - 0x1c0;
    const rx_addr flags = base + 0x13, nm = base + 0x14, ent = base + 0x18, self = base + 0x1c;
    const rx_addr pos = base + 0x20, dir = base + 0x30, hit = base + 0x40, out = base + 0x50, nrm = base + 0x60;
    const rx_addr m = base + 0x70, buf = base + 0xb0;
    SU32(A - 4, 0, U32(0, 0x009b45e4) ^ (A - 0x1b4));       /* the security cookie */
    SU32(nm, 0, name); SU32(self, 0, sm); SU32(ent, 0, e);
    rx_addr entry = sm;                                       /* (not found: the SM itself, as the original) */
    for (uint32_t i = 0; i < RX_RC_MAX; i++) if (rx_cast_u32(sm, i, 0) == e) { entry = rx_cast_rec_ro(sm, i); break; }   /* (w-rc-core) entries 12..31 in side storage */
    SU8(flags, 0, 0);
    {
        rx_set_sp(rx_sp() - 4); SU32(rx_sp(), 0, name);          /* push name (cdecl) */
        ANIM_CALL_FRAME(0); rx_addr alias = rx_anim_alias_pick(flags, avoid, name); ANIM_CALL_FRAME_END(0);
        rx_set_sp(rx_sp() + 4);
        if (alias) SU32(nm, 0, alias);
    }
    if (U8(flags, 0) & 2) {
        ANIM_CALL_FRAME(4); rx_scenario_anims_attach(U32(U32(entry, 4), 0x1ac), sm, U32(entry, 0)); ANIM_CALL_FRAME_END(4);
        if (place) {
            { uint32_t a = m; rx_call_original(U32(U32(place, 0), 0x3c), RX_THISCALL, place, 1, &a); }
            for (uint32_t k = 0; k < 12; k += 4) { SU32(pos, k, U32(m, 0x30 + k)); SU32(dir, k, U32(m, 0x20 + k)); }
            rx_addr veh = U32(U32(entry, 4), 0x1ac), w = U32(veh, 0x234);
            { uint32_t a[5] = {pos, dir, hit, nrm, out}; rx_call_original(U32(U32(w, 0), 0x50), RX_THISCALL, w, 5, a); }
            { const uint32_t a[3] = {veh, hit, nrm};
              const rx_addr sp = intro_callsite(3, a); rx_vehicle_place(veh, hit, nrm, out); rx_set_sp(sp); }
            veh = U32(U32(entry, 4), 0x1ac);
            SU8(U32(veh, 0x1328), 0x148, 0xff);
            { uint32_t a = 1; rx_call_original(U32(U32(veh, 0), 0x80), RX_THISCALL, veh, 1, &a); }
            SU8(U32(U32(U32(entry, 4), 0x1ac), 0x1328), 0x14e, 1);
        }
        rx_addr veh = U32(U32(entry, 4), 0x1ac);
        if (U32(veh, 0x194) <= 1) {
            const uint32_t fmt = U32(veh, 0x194) == 0 ? 0x00941e88u /* "%s_BIKE" */ : 0x00941e80u /* "%s_ATV" */;
            const uint32_t f[2] = {fmt, U32(nm, 0)};
            const rx_addr sp = intro_callsite(2, f);
            rx_format_buffer(buf, 0x104, fmt, rx_sp() + 8);
            rx_set_sp(sp);
            rx_addr mgr = U32(U32(0, 0x00a95844), 8);
            uint32_t a[4] = {buf, 0x6d696e61 /* 'anim' */, 0, 1};
            rx_addr res = rx_call_original(U32(U32(mgr, 0), 0x6c), RX_THISCALL, mgr, 4, a);
            if (res) {
                const uint32_t b[2] = {res, loops};
                const rx_addr sp = intro_callsite(2, b);
                rx_pose_override_set(U32(U32(entry, 4), 0x1ac), res, loops);
                rx_set_sp(sp);
                SU8(U32(U32(U32(entry, 4), 0x1ac), 0x1328), 0x14e, 1);
            }
        }
    } else {
        rx_addr x = U32(entry, 0);
        for (uint32_t i = 0; i < 12; i++)
            if (U32(sm, 0x1734 + 8 * i) == x) { SU32(sm, 0x1734 + 8 * i, 0); SU32(sm, 0x1738 + 8 * i, 0); break; }
        if (place) {
            rx_addr fn = U32(U32(e, 0), 0x40);
            uint32_t a = anim_vcall0(place, 0x38);
            rx_call_original(fn, RX_THISCALL, e, 1, &a);
        }
    }
    { const uint32_t a[2] = {U32(sm, 0x40), e};
      const rx_addr sp = intro_callsite(2, a); rx_activity_setup_object(U32(nm, 0), a[0], a[1]); rx_set_sp(sp); }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0074be10, rx_scenario_cast_place_entity)

/* 0074d690: the start-line animation for a rider's vehicle kind (+0x1ac +0x194: 0 bike, 1 ATV, else any vehicle). */
static rx_addr start_line_anim(rx_addr rider) {
    uint32_t k = U32(U32(rider, 0x1ac), 0x194);
    return k == 0 ? 0x0096cbd0 /* "RiderStartLineBike" */ : k == 1 ? 0x0096cbe4 /* "RiderStartLineATV" */
                  : 0x0096ccc4 /* "RiderStartLineVehicle" */;
}
static rx_addr intro_cast_entity(rx_addr sc, rx_addr rider) {       /* the rider's scenario entity in the level cast */
    rx_addr sm = U32(U32(sc, 0x14), 0x234) + 0x20150;
    ANIM_CALL_FRAME(4); rx_addr x = rx_scenario_cast_find(sm, rider); ANIM_CALL_FRAME_END(4);
    return U32(x, 0);
}

/* 0074d4c0 Rider_SetStartLineAnimation (EAX = intro state, ESI = scenario): state 1: every player's scenario entity hidden (+0xa2
 * = 0), then (no break: 0074d51c falls into the states 4 / 5 code at 0074d51e) as states 4 / 5: the chosen rider (+0x151c0, when a
 * human) shows and plays the start-line animation of its vehicle (cast slot +0x151c4), from time K (vf19) - in the Free Ride's arrival
 * (step 1) the rider rides in on the hovercraft / sky crane / box car; state 13: the first two riders show and play theirs. */
void rx_rider_set_start_line_animation(uint32_t state, rx_addr sc) {
    const rx_addr sp0 = rx_sp(), anim = sp0 - 4;
    rx_set_sp(sp0 - 0x10);                                    /* push ecx, ebx, ebp, edi */
    if (state == 1) {
        rx_addr g = U32(0, 0x00a946b8);
        for (uint32_t i = 0; i < U8(g, 0x1541c); i++) {
            const uint32_t a[2] = {i, 3};
            const rx_addr sp = intro_callsite(2, a);
            const rx_addr drv = rx_world_players_find(g + 0x7460, i, 3);
            rx_set_sp(sp);
            SU8(intro_cast_entity(sc, drv), 0xa2, 0);
        }
    }
    if (state == 1 || state == 4 || state == 5) {             /* state 1 falls through (w-frscene: was a separate branch) */
        rx_addr act = U32(sc, 0x14), rider = 0;
        if (I32(sc, 0x151c0) < (int32_t)activity_count_with(act, 1)) rider = U32(activity_rider(act, U32(sc, 0x151c0)), 0x74);
        SU32(anim, 0, start_line_anim(rider));
        rx_addr sm = U32(act, 0x234) + 0x20150;
        ANIM_CALL_FRAME(4); rx_addr x = rx_scenario_cast_find(sm, rider); ANIM_CALL_FRAME_END(4);
        rx_addr e = U32(x, 0);
        SU8(e, 0xa2, 1);
        ANIM_CALL_FRAME(0x10);
        rx_scenario_cast_place_entity(U32(U32(sc, 0x14), 0x234) + 0x20150, U32(sc, 0x151c4), e, 0, U32(anim, 0), 0);
        ANIM_CALL_FRAME_END(0x10);
        const uint32_t t = U32(0, 0x009322ac);
        const rx_addr target = U32(U32(e, 0), 0x4c);
        void (*animate)(rx_addr,float) = RX_FN(target,void (*)(rx_addr,float));
        if(animate) { const rx_addr site=intro_callsite(1,&t); animate(e,rx_bf(t));rx_set_sp(site); }
        else rx_call_original(target,RX_THISCALL,e,1,&t);
    } else if (state == 13) {
        for (uint32_t i = 0; i < 2; i++) {
            rx_addr rider = U32(activity_rider(U32(sc, 0x14), i), 0x74);
            rx_addr name = start_line_anim(rider);
            rx_addr e = intro_cast_entity(sc, rider);
            SU8(e, 0xa2, 1);
            ANIM_CALL_FRAME(0x10);
            rx_scenario_cast_place_entity(U32(U32(sc, 0x14), 0x234) + 0x20150, 0, e, 0, name, 0);
            ANIM_CALL_FRAME_END(0x10);
        }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074d4c0, rx_rider_set_start_line_animation)

/* 0074d2b0 FreerideIntroScenario::vf1 (thiscall: state; RET 4): the start-line animations, then the level's two cameras (world
 * +0x20150 / +0x21d50) take the chosen rider's entity matrix (vf14); in states 4 / 5 the second one at its chest
 * ("MXRider_Chest" of its hierarchy, vf28) when it has one. */
void rx_freeride_intro_begin(rx_addr sc, uint32_t state) {
    ANIM_FRAME_ENTER(A, 0x60);                                /* push ebp; and esp, -16; sub esp, 0x54; push ebx, esi, edi */
    const rx_addr base = A - 0x60, ent = base + 0x18, mat = base + 0x1c, local = base + 0x20;
    ANIM_CALL_FRAME(0); rx_rider_set_start_line_animation(state, sc); ANIM_CALL_FRAME_END(0);
    if ((int32_t)state < 4 || ((int32_t)state > 5 && state != 13)) { ANIM_FRAME_LEAVE(A); return; }
    rx_addr src;
    if (state == 13) {
        rx_addr e = intro_cast_entity(sc, U32(activity_rider(U32(sc, 0x14), 0), 0x74));
        rx_addr m = anim_vcall0(e, 0x38);
        for (uint32_t k = 0; k < 0x40; k += 4) SU32(U32(U32(sc, 0x14), 0x234) + 0x20150, k, U32(m, k));
        src = m;
    } else {
        rx_addr e = intro_cast_entity(sc, U32(activity_rider(U32(sc, 0x14), U32(sc, 0x151c0)), 0x74));
        SU32(ent, 0, e);
        rx_addr m = anim_vcall0(e, 0x38);
        for (uint32_t k = 0; k < 0x40; k += 4) SU32(U32(U32(sc, 0x14), 0x234) + 0x20150, k, U32(m, k));
        SU32(mat, 0, m);
        rx_addr h = anim_vcall0(U32(ent, 0), 0x70);
        ANIM_CALL_FRAME(4); uint32_t has = rx_hierarchy_has_node(h, 0x009581fc /* "MXRider_Chest" */); ANIM_CALL_FRAME_END(4);
        if (has) {
            ANIM_CALL_FRAME(4); rx_addr node = rx_hierarchy_find_node(h, 0x009581fc); ANIM_CALL_FRAME_END(4);
            SU32(mat, 0, node);
            rx_addr m2 = anim_vcall0(U32(ent, 0), 0x38);
            for (uint32_t k = 0; k < 0x40; k += 4) SU32(local, k, U32(m2, k));
            anim_node_evaluate_world(node, 0, 0x0074d3d2);
            rx_addr w = U32(U32(node, 0), 0x1c) + 64u * U8(node, 0xb);
            for (uint32_t k = 0; k < 12; k += 4) SU32(local, 0x30 + k, U32(w, 0x30 + k));
            src = local;
        } else src = U32(mat, 0);
    }
    rx_addr w = U32(U32(sc, 0x14), 0x234);
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(w + 0x21d50, k, U32(src, k));
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0074d2b0, rx_freeride_intro_begin)
