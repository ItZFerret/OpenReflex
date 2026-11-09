/* build_activity_spawn.c -- construction unit 11: the player slot's vehicle / rider spawn: PlayerSlot_SpawnStep 00767200 and the
 * front-end spawn wrappers it calls.  The world's player pass 00766ec0 (loop_world.c rx_world_update_players) calls it for every
 * local player that still needs spawning: 0054a500 (the old vehicle released), 0053f3f0 (the rider display name / number), 00549f30
 * (the vehicle spec made: VehicleList_CreateSpec); on the following passes 00542ac0 / 0053e880 (the spec's and the rider entry's
 * packages ready) lead to 0054a050 (VehicleFactory_CreateVehicle) and 0054a3f0 (the rider entry) / 0054a320 (0053e920 rider spawn).
 *
 * The functions keep the original's field order and register arguments (each comment).  Callees that belong to other, unported
 * units (the spawned vehicle's teardown hooks, the rider object detach 0053ebc0 / 0053ece0, the vehicle list release, the front-end
 * player snapshot 0054bc00) go through rx_call_original: the oracle runs the original, the product stops naming the address; they
 * are NOT reached by the construction path (they need a spawned vehicle / rider).  Calls whose result no gameplay code reads (the
 * rider name / number text 0053f3f0, the number / name plates 0054bec0 / 0054bf50, the vehicle foley 00680b10, the "Numbers" texture
 * override, the pad input-availability virtuals) are boundaries (the fx slot 00691cc0 and the damage registration 0047bb90 are ported and
 * run in the product): the oracle runs
 * the original, the product does nothing (marked at each site).  The profiler scopes (00883d80 / 00883d90) are ported and run in
 * both builds. */
#include "build_activity_int.h"
#include "vehicle_audio.h"
#include "anim_scenario.h"
#include "race_misc.h"
#include "build_rider.h"
#include "build_vehicle.h"
#include "world_misc.h"
#include "world_rider.h"
#include "world_veh.h"
#include "world_veh_damage.h"
#include "phys_body.h"
#include "race_driver.h"
#include "build_activity_clear.h"
#include "race.h"
#include "loop_cam_build.h"
#include "scene_fx_entities.h"
#include <string.h>

/* ======================================================================================================================== */
/* the package readiness checks                                                                                              */

/* 0053fa30 (ECX = a part record, stdcall high_resolution; RET 4) -> EAX: the part's two package records (+4 / +0x20: the package
 * flag at +0x10, the name at +0x14, SSO) are empty or loaded in the gear loader's package cache ([0xd92ef0] + 4); with
 * high_resolution and the part's +0x38 set the name is prefixed "HR_" (0078fd60) */
uint32_t rx_part_packages_ready(rx_addr part, uint32_t hr) {
    ACT_FRAME(L, 0x50, 4);
    const rx_addr buf = L + 0x20;
    for (uint32_t k = 0; k < 2; k++) {
        const rx_addr rec = part + 4 + 0x1c * k;
        if (U32(rec, 0x10) == 0) continue;
        const rx_addr nm = U32(rec, 0x14) >= 0x10 ? U32(rec, 0) : rec;
        rx_addr name = nm;
        if (hr && U32(part, 0x38) != 0) {
            const rx_addr sp = rx_sp();
            SU32(sp, -8, 0x00940560 /* "HR_%s" */); SU32(sp, -4, nm);
            rx_set_sp(sp - 12);
            rx_format_buffer(buf, 0x40, 0x00940560, sp - 4);
            rx_set_sp(sp);
            name = buf;
        }
        if (name == 0 || U8(name, 0) == 0) continue;
        if (!rx_pkgcache_loaded(U32(0, 0x00d92ef0) + 4, name)) { ACT_END(L); return 0; }
    }
    ACT_END(L);
    return 1;
}
RX_FUNCTION(0x0053fa30, rx_part_packages_ready)

/* 00542ac0 (ECX = the vehicle list (MXGame + 0x154b0), EAX = the vehicle id; RET 0) -> EAX: the spec of that id has all 17 part
 * slots' packages ready */
uint32_t rx_vehicle_spec_ready(rx_addr list, uint32_t vehicle_id) {
    for (int32_t i = 0; i < I32(list, 0x2c); i++) {
        const rx_addr s = U32(U32(list, 0x28), 4u * (uint32_t)i);
        if (!s || U32(s, 4) != vehicle_id) continue;
        for (uint32_t j = 0; j < 0x11; j++) {
            const rx_addr p = U32(s, 0xc + 4 * j);
            if (p && !rx_part_packages_ready(p, U32(s, 0xc0))) return 0;
        }
        return 1;
    }
    return 0;
}
RX_FUNCTION(0x00542ac0, rx_vehicle_spec_ready)

/* 0053cea0 (stdcall the rider entry; RET 4) -> EAX: the entry's eight part slots' packages are ready (0053ca70 with the entry's
 * +0x14 as the high-resolution flag) */
uint32_t rx_rider_entry_parts_ready(rx_addr entry) {
    for (uint32_t i = 0; i < 8; i++) {
        const rx_addr p = U32(entry, 0x1c + 4 * i);
        if (p && !rx_rider_part_packages_ready(p, U32(entry, 0x14))) return 0;
    }
    return 1;
}
RX_FUNCTION(0x0053cea0, rx_rider_entry_parts_ready)

/* 0053e880 (ECX = the rider manager (MXGame + 0x15504), EAX / EDI = the rider id; RET 0) -> EAX: the manager's entry of that id
 * (its +4) with its parts' packages ready */
uint32_t rx_rider_entry_ready(rx_addr mgr, uint32_t rider_id) {
    for (int32_t i = 0; i < I32(mgr, 0x18); i++) {
        const rx_addr e = U32(U32(mgr, 0x14), 4u * (uint32_t)i);
        if (U32(e, 4) == rider_id) return rx_rider_entry_parts_ready(e);
    }
    return 0;
}
RX_FUNCTION(0x0053e880, rx_rider_entry_ready)

/* ======================================================================================================================== */
/* the player's vehicle / rider (de)spawn                                                                                    */

/* 0054a730 (ESI = the player; RET 0): the player's rider object (+0x1b4) released (0053ebc0: EAX = the rider, EDI = the rider manager
 * MXGame + 0x15504) or its pending rider entry (+0x1d8 -> 0053ece0: EAX = the manager, the rider id +0x1b8); the pointers cleared.
 * Frame: push ecx / ebx / edi. */
static void player_rider_clear(rx_addr P) {
    ACT_FRAME(L, 0, 3);
    const rx_addr manager = U32(0, 0xa946b8) + 0x15504;
    if (U32(P, 0x1b4) != 0) {
        SU32(U32(P, 0x1ac), 0x1a0, 0);
        SU8(U32(P, 0x1b4), 0xa2, 0);
        ACT_CALL(0, rx_rider_remove(U32(P, 0x1b4), manager));          /* 0053ebc0 */
        SU32(P, 0x1b4, 0);
        SU32(P, 0x1d8, 0);
        ACT_END(L);
        return;
    }
    if (U32(P, 0x1d8) != 0) ACT_CALL(1, rx_rider_release(manager, U32(P, 0x1b8)));   /* 0053ece0 */
    SU32(P, 0x1d8, 0);
    ACT_END(L);
}

/* 0054a500 (stdcall the player; RET 4): the player's vehicle released: the two particle managers cleared while the vehicle's world
 * scene is active (0069a0c0), the free-ride reset hook (00550e10), 0054a730 (the rider), then the spawned vehicle's teardown - its
 * model flag, vf49 (+0xc4), the physics record's flag, the pose override (0047c380), the vehicle removed from the list (005408e0),
 * the vehicle audio record (00680d10), the damage record (0047bbe0), the fx set (00691d30), the mud record (00408fe0), the global
 * vehicle table entry, the player's camera / fx objects (0054bda0) - or, without a vehicle, the pending spec (00540dd0); the spawn
 * fields cleared. */
void rx_player_vehicle_clear(rx_addr P) {
    const rx_addr E0 = rx_sp();
    rx_set_sp(E0 - 0x10);                              /* the original's frame: push ebx; push ebp; push esi; push edi */
    if (U32(P, 0x1ac) != 0) {
        rx_addr lv = U32(U32(P, 0x1ac), 0x188);
        lv = lv ? U32(lv, 0x20138) : 0;
        if (U32(lv, 0x34) != 0) {
            const rx_addr fobj = U32(U32(0, 0x00a95844), 0x10);
            ACT_CALL(0, rx_engine_frame_mark(fobj));        /* 0084ea00: EDI = the engine's frame object */
            ACT_CALL(0, rx_particle_manager2d_clear(0x00c11bd0));      /* 0069a0c0 (EDI = the manager) */
            ACT_CALL(0, rx_particle_manager2d_clear(0x00c11d80));
        }
    }
    {
        const rx_addr act = U32(P, 0x94);
        if (act != 0 && U32(P, 0x1ac) != 0 && U32(act, 0x23c) == 8 && U32(act, 0x21c) == 2 &&
            U32(U32(act, 0x234), 0x20018) == 0) {
            ACT_CALL(1, rx_player_save_respawn_pose(P));    /* 00550e10: the free ride's reset hook */
            SU32(P, 0x10e0, 1);
        }
    }
    player_rider_clear(P);
    if (U32(P, 0x1ac) == 0) {
        if (U32(P, 0x1d4) != 0) ACT_CALL(1, rx_vehicle_spec_free(U32(0, 0xa946b8) + 0x154b0, U32(P, 0x1b0)));   /* 00540dd0 (EAX = the vehicle id; push the list) */
    } else {
        SU8(U32(U32(P, 0x1ac), 0x1328), 0x14e, 0);
        act_vcall(U32(P, 0x1ac), 0xc4, 1, (const uint32_t[]){0});
        if (U32(U32(P, 0x1ac), 0x17c) != 0) {
            rx_addr rec;
            ACT_CALLR(rec, 0, phys_body_record(U32(U32(P, 0x1ac), 0x17c)));   /* 0042bd90: EAX = the vehicle's body handle (+0x178 + 4) */
            SU32(U32(rec, 0x2c), 0x14, 0xffffffffu);
        }
        ACT_CALL(0, rx_pose_override_release(U32(P, 0x1ac)));               /* 0047c380 (EDX = the vehicle) */
        ACT_CALL(0, rx_vehicle_remove(U32(P, 0x1ac), U32(0, 0xa946b8) + 0x154b0));   /* 005408e0 (EAX = the vehicle, EDI = the list) */
        ACT_CALL(1, rx_vehicle_audio_release(U32(P, 0x1ac), U32(0, 0x00d92f00)));   /* 00680d10 (ECX = EDX... see build_activity_clear.c) */
        rx_addr dmg;
        ACT_CALLR(dmg, 0, rx_world_damage_manager_instance());   /* 0047bb40 */
        ACT_CALL(0, rx_damage_release(dmg, U32(P, 0x1ac)));      /* 0047bbe0: EDX = the manager, ESI = the vehicle */
        ACT_CALL(0, rx_vehicle_effects_release(U32(P, 0x1ac)));  /* 00691d30 (EDX = the vehicle) */
        ACT_CALL(0, rx_mud_record_free(U32(P, 0x1ac)));          /* 00408fe0 MudRecord_Free (EDX = the vehicle) */
        for (uint32_t i = 0; i < 12; i++) {
            if (U32(P, 0x1ac) == U32(0, 0x00ad73b4u + i * 0x20)) { SU32(0, 0x00ad73b4u + i * 0x20, 0); break; }
        }
        SF32(P, 0x10bc, 0.0f);
        SU32(P, 0x1ac, 0);
        ACT_CALL(0, rx_player_release_objects(P));               /* 0054bda0 (ESI = the player) */
        if (I32(P, 0x1f0) != -1) {   /* the camera parameters written back to the player's controller table (0054a690 ..) */
            rx_addr tbl;
            ACT_CALLR(tbl, 1, rx_tbl_find(0x00ae10f0u + (uint32_t)I32(P, 0x1f0) * 0x1f4u, 0x00941650u));   /* 00402220 */
            if (tbl != 0) {
                static const uint32_t names[3] = {0x00941668u, 0x00941678u, 0x00941688u};
                for (uint32_t k = 0; k < 3; k++) ACT_CALL(2, rx_tbl_set_cell_float(tbl, names[k], 0, P + 0x224 + 4 * k));   /* 005063c0 */
            }
        }
    }
    SU32(P, 0x1d0, 0); SU32(P, 0x1d4, 0); SU32(P, 0x1f8, 0);
    rx_set_sp(E0);
}
RX_FUNCTION(0x0054a500, rx_player_vehicle_clear)

/* 0053f3f0 (cdecl name buffer (P + 0x200), number buffer (P + 0x210), &found, rider id; RET 0x10): the rider's display name and
 * number from the Rider List (the profile's PlayerRider Name / Number for a player, the AIRider row for an AI). The two buffers are
 * read by the number / name plates only (render: Unity's ReflexPlates makes the textures from them, rx_session_appearance); the
 * product runs the port (build_rider_entry.c), the oracle the original. */
static void player_rider_name(rx_addr nb, rx_addr numb, rx_addr found, uint32_t rider_id) {
#ifdef RX_ORACLE
    uint32_t a[4] = {nb, numb, found, rider_id};
    rx_call_original(0x0053f3f0, RX_CDECL, 0, 4, a);
#else
    rx_rider_name_number(nb, numb, found, rider_id);
#endif
}

/* 00549f30 (EAX = the player, stdcall with_vehicle; RET 4): the vehicle spec made (00542b20) for the player's +0x1b0 id, the spawn
 * state +0x1d4 = 1 and +0x1f8 = 0, a profiler scope (00883d80 / 00883d90 around 0054bec0) and the name plate update */
void rx_player_spawn_spec(rx_addr P, uint32_t flag) {
    const rx_addr E0 = rx_sp();
    rx_set_sp(E0 - 0x10);                              /* the original's frame: push ebx; push ebp; push esi; push edi */
    const rx_addr game = U32(0, 0xa946b8);
    SU32(P, 0x1d4, 1);
    SF32(P, 0x1f8, 0.0f);
    ACT_CALL(3, rx_vehicle_list_create_spec(game + 0x154b0, U32(P, 0x1b0), 0));   /* 00542b20 */
    race_static_dcf304();   /* the session static: 006de1f0 behind its guard 0x00dcf304 + the atexit */
    (void)flag;
#ifdef RX_ORACLE
    rx_addr scope = 0x00da6cf0u;
    if (U32(0, 0x00da6d08) != 0) {
        rx_regs in, o; memset(&in, 0, sizeof in);
        rx_call_original_ex(0x007a0760, &in, &o, 0, 0);
        uint32_t a[2] = {3, o.r[0]};
        rx_regs in2; memset(&in2, 0, sizeof in2); in2.r[1] = 0x009df584;
        rx_call_original_ex(0x00402420, &in2, &o, 2, a);
        if (o.r[0] != 0 && U32(o.r[0], 0xc) != 0) scope = U32(o.r[0], 8) + 0x3c;
        else scope = 0x3c;
    }
    if (U8(0, 0x009df580) == 0) {
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[1] = scope;
        rx_call_original_ex(0x00883d80, &in, &o, 0, 0);   /* the profiler scope begin (Quazal: not core) */
    }
    if (U32(0, 0x00da6d08) != 0 && U32(U32(0, 0x00da6cb0), 0x1c4c) != 0 && U32(U32(0, 0x00da6cb0), 0x1b0) != 1 && U32(P, 0xb2c) != 0) {
        uint32_t valid;
        ACT_CALLR(valid, 0, rx_net_ref_valid(U32(P, 0xb2c)));   /* 00884950 (ECX = the player's network reference) */
        if ((valid & 0xff) != 0) { rx_regs i2, o2; memset(&i2, 0, sizeof i2); i2.r[6] = U32(P, 0xb2c); uint32_t a1[1] = {P}; rx_call_original_ex(0x006f25f0, &i2, &o2, 1, a1); }
    }
    { rx_regs in, o; memset(&in, 0, sizeof in); in.r[7] = P;   /* 0054bec0 reads the player in EDI */ rx_call_original_ex(0x0054bec0, &in, &o, 0, 0); }   /* the number plate (render) */
    if (U8(0, 0x009df580) == 0) {
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[1] = scope;
        rx_call_original_ex(0x00883d90, &in, &o, 0, 0);   /* the profiler scope end */
    }
#else
    (void)game;
#endif
    rx_set_sp(E0);
}
RX_FUNCTION(0x00549f30, rx_player_spawn_spec)

/* ======================================================================================================================== */
/* the vehicle / rider spawn                                                                                                 */

/* 0054a050 (EAX = the world, stdcall the player; RET 4) -> 1: the player's vehicle made (00542b90) in the world (the UI world
 * while the world's session has not started), linked to the player (+0x190), its input mode set (the vehicle's vf55), the driver's
 * start hooks, the vehicle's scene / sound / fx / damage hooks and the player's race fields cleared */
uint32_t rx_player_vehicle_spawn(rx_addr world, rx_addr P) {
    const rx_addr E0 = rx_sp();
    rx_set_sp(E0 - 0x10);                              /* the original's frame: push ebx; push ebp; push esi; push edi */
    const rx_addr game = U32(0, 0xa946b8);
    rx_addr w = world;
    if (U32(world, 0x20018) == 0) w = U32(game, 0x20c8);
    SU32(P, 0x1d0, w);
    rx_addr veh;
    ACT_CALLR(veh, 2, rx_vehicle_factory_create(world, game + 0x154b0, U32(P, 0x1b0)));   /* push id; push world; call: the callee's entry ESP. The argument world (EAX at the original's call), not the UI world in +0x1d0 */
    SU32(P, 0x1ac, veh);
    SU32(veh, 0x190, P);
    {
        uint32_t mode = 1; int call_it = 1;
        if (U32(P, 4) == 0) mode = 1;
        else if (U32(P, 0x10) == 0) mode = 0;
        else if (U32(P, 4) == 0) mode = 1;
        else if (U32(P, 0x10) == 1) mode = 2;
        else if (U32(P, 4) != 0) call_it = 0;
        else mode = 1;
        if (call_it) act_vcall(veh, 0xdc, 1, (const uint32_t[]){mode});   /* vf55: the input mode */
    }
    if (U32(P, 4) != 0 && U32(P, 0x10) == 0) {
        const uint32_t pad = U32(P, 0x1f0);
        uint32_t in_mode = 0;
#ifdef RX_ORACLE
        {   /* the pad control block's two input-availability virtuals (the input unit's) */
            const rx_addr block = U32(0, 0x009c52c4u + 4 * pad);
            const rx_addr f1 = U32(U32(block, 0xfec), 0x2c);
            rx_regs in, o; memset(&in, 0, sizeof in); in.r[1] = block + 0xfec;
            uint32_t a[1] = {pad};
            rx_call_original_ex(f1, &in, &o, 1, a);
            in_mode = o.r[0] ? 1 : 0;   /* the original's `mov eax,1` when the virtual returns non-zero */
            if (in_mode == 0) {
                const rx_addr f2 = U32(U32(block, 0x1090), 0x2c);
                memset(&in, 0, sizeof in); in.r[1] = block + 0x1090;
                rx_call_original_ex(f2, &in, &o, 1, a);
                in_mode = o.r[0] ? 1 : 0;
            }
        }
#else
        (void)pad;   /* boundary: the input unit's pad blocks; the product leaves the vehicle's +0x2eb0 */
#endif
        if (U32(P, 0x1ac) != 0) SU32(U32(U32(P, 0x1ac), 0x234), 0x2eb0, in_mode);
    }
    act_vcall(U32(P, 0x1ac), 0xc4, 1, (const uint32_t[]){0});      /* vf49(0) */
    SU8(U32(U32(P, 0x1ac), 0x1328), 0x14e, U32(P, 4) != 0);
    {
        const rx_addr act = U32(P, 0x94);
        if (act != 0) {
            const int32_t slot = rx_world_group_rider_index(act, P);
            const rx_addr d = U32(U32(act, U32(P, 0x908) != 0 ? 0x27c : 0x268), 4u * (uint32_t)slot);
            if (d != 0) { act_vcall(d, 0xc, 0, 0); act_vcall(d, 0x3c, 0, 0); }
        }
    }
    {   /* 0054bc00 (EAX = the player): the player's view / camera entities made: the five views P + 0x974 .. + 0x984 (0063f160,
         * the 5th's CameraParam1..3 from P + 0x224 / +0x228 / +0x22c), the start-gate camera P + 0x99c (0x647cf0) and the quick
         * camera P + 0x998 (0x646d70), each inserted into the scene (0080ddf0) with +0x2f8 = 1. Those fields are the camera
         * unit's (loop_player.c: +0x974..+0x984 the camera entities, +0x998 the quick camera, +0x99c the start-gate camera,
         * +0x9a8 the current one). The vehicle spawn itself ignores the result, but the player's per-frame camera update (0054c150,
         * loop_player.c) reads these entities and the activity's clear (build_activity_clear.c) releases them. The oracle runs the
         * original; the product runs the port (rx_player_cameras_create, loop_cam_build.c). */
#ifdef RX_ORACLE
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = P;
        rx_call_original_ex(0x0054bc00, &in, &o, 0, 0);
#else
        /* the port (loop_cam_build.c), called where the original calls it. The first-person slot is left 0 until its
         * vf19 00642680 is ported (the MX camera path selects the GamePlay third person camera). */
        rx_player_cameras_create(P);
#endif
    }
    if (U32(U32(P, 0x1ac), 0x194) <= 1) SU32(P, 0xb58, 1);
#ifndef RX_ORACLE
    rx_vehicle_audio_register(U32(P, 0x1ac));   /* w-sfx: 00680b10's slot choice, kept outside guest memory (game/vehicle_audio.c: the engine's AIFX) */
#endif
    /* vehicle foley (audio), fx, the damage manager and the "Numbers" texture override. The oracle runs the original; the product runs
     * the fx slot bind (the vehicle effect set the frame's idle effects are drawn from: the original's 00691cc0, FX unit) and the damage
     * registration (0047bb90: the four-wheel vehicles' detachable parts, world_veh_damage_register.c); the foley (00680b10: audio) and the
     * "Numbers" texture override (005510a0) are the oracle's only. */
#ifdef RX_ORACLE
    { rx_regs in, o; memset(&in, 0, sizeof in); uint32_t a[2] = {U32(0, 0x00d92f00), U32(P, 0x1ac)}; rx_call_original_ex(0x00680b10, &in, &o, 2, a); }   /* (manager, vehicle) */
    { rx_regs in, o; memset(&in, 0, sizeof in); in.r[7] = U32(P, 0x1ac); rx_call_original_ex(0x00691cc0, &in, &o, 0, 0); }   /* EDI = the vehicle (the player's fx slot) */
    {   /* 0047bb40 -> ESI = the damage manager; 0047bb90: EAX = the vehicle */
        rx_addr dmg;
        ACT_CALLR(dmg, 0, rx_world_damage_manager_instance());
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = U32(P, 0x1ac); in.r[6] = dmg; in.r[7] = U32(P, 0x1ac);
        rx_call_original_ex(0x0047bb90, &in, &o, 0, 0);
    }
    if (U32(P, 0x1ac) != 0) {
        rx_regs in, o; memset(&in, 0, sizeof in);
        uint32_t a[3] = {U32(P, 0x1ac) + 0x6a0, 0x00941750 /* "Numbers" */, U32(P, 0xb00)};
        rx_call_original_ex(0x005510a0, &in, &o, 3, a);
    }
#else
    /* 00691cc0 (EDI = the vehicle): the player's fx slot, the first free vehicle effect set (0xc11e90) gets the vehicle and the
     * active scene's ParticleEntity. Its records are the vehicle's idle effect set (fx_worldparticles.c); without this the UI
     * world's vehicle draws no particles in the load frames and the effect sets of the event world stay empty. */
    ACT_CALL(0, rx_vehicle_effects_slot_bind(U32(P, 0x1ac)));
    {   /* 0047bb40 -> ESI = the damage manager; 0047bb90: EAX = the vehicle. A trophy truck, buggy or UTV (class 3, 5, 7) takes an entry of the manager and
         * makes its detachable body parts (world_veh_damage_register.c): their shapes, models and physics bodies; without them the parts never break.
         * A motocross bike or an ATV takes none. (The oracle runs the original above.) */
        rx_addr dmg;
        ACT_CALLR(dmg, 0, rx_world_damage_manager_instance());
        ACT_CALL(0, rx_damage_register_vehicle(dmg, U32(P, 0x1ac)));
    }
#endif
    SF32(P, 0x1168, 0.0f); SF32(P, 0x116c, 0.0f);
    rx_world_rider_clear_wreck(P);                                                 /* 0054e890 */
    SU32(P, 0x1224, 0); SU32(P, 0x1280, 0);
    SF32(P, 0x122c, 0.0f); SF32(P, 0x1230, 0.0f); SF32(P, 0x1250, 0.0f); SF32(P, 0x1254, 0.0f);
    SF32(P, 0x1258, 0.0f); SF32(P, 0x1260, 0.0f); SF32(P, 0x1264, 0.0f); SF32(P, 0x1268, 0.0f);
    SF32(P, 0x1270, 0.0f); SF32(P, 0x1274, 0.0f); SF32(P, 0x1278, 0.0f);
    SU32(P, 0x1220, 1);
    if (U32(P, 0x10e0) != 0) {
        rx_vehicle_effect_query(U32(P, 0x1ac), P + 0x10c0, P + 0x10d0);            /* 00460020 */
        SU32(P, 0x10e0, 0);
    }
    rx_set_sp(E0);
    return 1;
}
RX_FUNCTION(0x0054a050, rx_player_vehicle_spawn)

/* 0054a320 (EAX = the player, stdcall the world; RET 4) -> 1: the player's rider spawned (0053e920) in the world (the UI world
 * while the world's session has not started): +0x1b4 the rider, +0xa2 1, the driver's start hook and the rider pose reset */
uint32_t rx_player_rider_spawn(rx_addr P, rx_addr world) {
    const rx_addr E0 = rx_sp();
    rx_set_sp(E0 - 0x14);                              /* the original's frame: sub esp,0xc; push esi; push edi */
    const rx_addr game = U32(0, 0xa946b8);
    rx_addr w = world;
    if (U32(world, 0x20018) == 0) w = U32(game, 0x20c8);
    SU32(P, 0x1d0, w);
    /* 0053e920 (EAX = the rider id; stdcall manager, world, vehicle, callback; RET 0x10): the callback is the stack object
     * {MudAndNumberCallback vtable, 0, 0} (the mud / number overrides are render) */
    rx_addr ra;
    {
        const rx_addr cb = E0 - 0xc;   /* the MudAndNumberCallback stack object (the original's [esp + 8] after sub esp,0xc): vtable 0x0094178c, the player */
        SU32(cb, 0, 0x0094178c); SU32(cb, 4, P); SU32(cb, 8, 0);
        /* 0053e920 (EAX = the rider id; stdcall manager, world, vehicle, callback): the rider unit's port in the product; the
         * original in the oracle build, so this unit's Verify compares against the original rather than inheriting the rider
         * unit's work-in-progress mismatches (their group verifies the port through the original 00767200 -> 0054a320 chain). */
#ifdef RX_ORACLE
        rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = U32(P, 0x1b8);
        uint32_t a4[4] = {game + 0x15504, world, U32(P, 0x1ac), cb};
        rx_call_original_ex(0x0053e920, &in, &o, 4, a4);
        ra = o.r[0];
#else
        ACT_CALLR(ra, 4, rx_rider_spawn(U32(P, 0x1b8), game + 0x15504, world, U32(P, 0x1ac), cb));
#endif

    }
    SU32(P, 0x1b4, ra);
    SU8(ra, 0xa2, 1);
    {
        const rx_addr act = U32(P, 0x94);
        if (act != 0) {
            const int32_t slot = rx_world_group_rider_index(act, P);
            const rx_addr d = U32(U32(act, U32(P, 0x908) != 0 ? 0x27c : 0x268), 4u * (uint32_t)slot);
            if (d != 0) act_vcall(d, 0x3c, 0, 0);
        }
    }
    rx_rider_animation_reset_pose(U32(P, 0x1b4));
    if (U32(P, 0x10) == 1 || U32(P, 4) == 0) {
        const rx_addr r = U32(P, 0x1b4);
        rx_world_rider_end_pose_override(r);                       /* 00621490 */
        SU32(r, 0x5bb60, 0);
    }
    rx_set_sp(E0);
    return 1;
}
RX_FUNCTION(0x0054a320, rx_player_rider_spawn)

/* 0054a3f0 (EDI = the player, stdcall with_vehicle; RET 4): the player's rider entry made (0053e8c0) for +0x1b8, the spawn state
 * +0x1d8 = 1, a profiler scope and the name plate update */
void rx_player_rider_attach(rx_addr P, uint32_t flag) {
    const rx_addr E0 = rx_sp();
    rx_set_sp(E0 - 0x10);                              /* the original's frame: push ecx; push ebx; push ebp; push esi */
    const rx_addr game = U32(0, 0xa946b8);
    SU32(P, 0x1d8, 1);
    ACT_CALL(3, rx_rider_entry_create(game + 0x15504, U32(P, 0x1b8), 0));   /* 0053e8c0 */
    race_static_dcf304();   /* the session static: 006de1f0 behind its guard 0x00dcf304 + the atexit */
    (void)flag;
#ifdef RX_ORACLE
    {
        rx_regs in, o; memset(&in, 0, sizeof in);
        rx_addr scope = 0x00da6cf0u;
        if (U32(0, 0x00da6d08) != 0) {
            memset(&in, 0, sizeof in);
            rx_call_original_ex(0x007a0760, &in, &o, 0, 0);
            uint32_t a[2] = {3, o.r[0]};
            rx_regs in2; memset(&in2, 0, sizeof in2); in2.r[1] = 0x009df584;
            rx_call_original_ex(0x00402420, &in2, &o, 2, a);
            if (o.r[0] != 0 && U32(o.r[0], 0xc) != 0) scope = U32(o.r[0], 8) + 0x3c;
            else scope = 0x3c;
        }
        if (U8(0, 0x009df580) == 0) {
            rx_regs i0; memset(&i0, 0, sizeof i0); i0.r[1] = scope;
            rx_regs o0; rx_call_original_ex(0x00883d80, &i0, &o0, 0, 0);
        }
        if (U32(0, 0x00da6d08) != 0 && U32(U32(0, 0x00da6cb0), 0x1c4c) != 0 && U32(U32(0, 0x00da6cb0), 0x1b0) != 1 && U32(P, 0xb2c) != 0) {
            uint32_t valid;
            ACT_CALLR(valid, 0, rx_net_ref_valid(U32(P, 0xb2c)));   /* 00884950 (ECX = the player's network reference) */
            if ((valid & 0xff) != 0) { rx_regs i2, o2; memset(&i2, 0, sizeof i2); i2.r[6] = U32(P, 0xb2c); rx_call_original_ex(0x006f2730, &i2, &o2, 0, 0); }
        }
        rx_regs i3, o3; memset(&i3, 0, sizeof i3); uint32_t ap[1] = {P};
        rx_call_original_ex(0x0054bf50, &i3, &o3, 1, ap);   /* the name plates (render); P is the stack argument */
        if (U8(0, 0x009df580) == 0) {
            rx_regs i4, o4; memset(&i4, 0, sizeof i4); i4.r[1] = scope;
            rx_call_original_ex(0x00883d90, &i4, &o4, 0, 0);
        }
    }
#endif
    rx_set_sp(E0);
}
RX_FUNCTION(0x0054a3f0, rx_player_rider_attach)

/* ======================================================================================================================== */
/* PlayerSlot_SpawnStep 00767200                                                                                             */

/* 00767200 (EAX = the player, stdcall the world; RET 4) -> EAX: one spawn step (1 the vehicle spec made, 2 the vehicle, 3 the
 * rider entry, 4 the rider spawned; 0 when nothing happened). The world's player pass stops unless the result is 4. The original
 * keeps its result in the first argument slot; the C return is equivalent (that slot is dead after the ret 4). */
uint32_t rx_player_slot_spawn_step(rx_addr P, rx_addr world) {
    ACT_FRAME(L, 0, 4);                                 /* push ebx, ebp, esi, edi */
    rx_addr result = 0;
    if (getenv("RX_SPAWN_DIAG"))                        /* diagnostics: the state the step sees (REFLEX_PORT_LOG style) */
        fprintf(stderr, "spawnstep P=%08x d4=%u d8=%u veh=%08x vid=%d rider=%08x rid=%d c0=%d c4=%d\n", P, U32(P, 0x1d4), U32(P, 0x1d8),
                U32(P, 0x1ac), (int32_t)U32(P, 0x1b0), U32(P, 0x1b4), (int32_t)U32(P, 0x1b8), (int32_t)U32(P, 0x1c0), (int32_t)U32(P, 0x1c4));

    if (U32(P, 0x1d4) == 0) {
        if (U32(P, 0x1c4) < 0x80000000u || U32(P, 0x1c0) < 0x80000000u) {
            ACT_CALL(1, rx_player_vehicle_clear(P));
            if (I32(P, 0x1c4) > -1) SU32(P, 0x1b0, U32(P, 0x1c4));
            if (I32(P, 0x1c0) > -1) {
                const rx_addr found = L - 8;   /* a game-stack slot: 0053f3f0 writes the flag through it */
                SU32(found, 0, 0);
                player_rider_name(P + 0x200, P + 0x210, found, U32(P, 0x1c0));
                if (U32(found, 0) == 0) { uint32_t pm; ACT_CALLR(pm, 0, rx_game_player_mode()); if (pm == 2) SU8(P, 0x200, 0); }
            }
            ACT_CALL(1, rx_player_spawn_spec(P, 1));
            SU32(P, 0x1c4, 0xffffffffu);
            ACT_END(L);
            return 1;
        }
        if (U32(P, 0x1ac) == 0 && I32(P, 0x1b0) > -1) { ACT_CALL(1, rx_player_spawn_spec(P, 0)); ACT_END(L); return 2; }
    } else {
        const rx_addr game = U32(0, 0xa946b8);
        if (rx_vehicle_spec_ready(game + 0x154b0, U32(P, 0x1b0)) != 0 &&
            rx_rider_entry_ready(game + 0x15504, U32(P, 0x1b8)) != 0) {
            if (U32(P, 0x1c4) < 0x80000000u || U32(P, 0x1c0) < 0x80000000u) {
                ACT_CALL(1, rx_player_vehicle_clear(P));
                if (I32(P, 0x1c4) > -1) SU32(P, 0x1b0, 0xffffffffu);
            } else if (U32(world, 0x20018) == 0 || U32(P, 0x10) == 0) {
                const uint32_t save = U32(P, 0x1d8);
                SU32(P, 0x1d4, 0);
                SU32(P, 0x1d8, 0);
                ACT_CALL(1, rx_player_vehicle_clear(P));
                ACT_CALL(1, rx_player_vehicle_spawn(world, P));   /* push esi (P); EAX = the world; call 0054a050 (RET 4) */
                SU32(P, 0x1d8, save);
                ACT_END(L);
                return 2;
            }
        }
    }
    if (U32(P, 0x1ac) == 0 || U32(P, 0x1d8) == 0) {
        if (I32(P, 0x1c0) > -1) {
            player_rider_clear(P);
            SU32(P, 0x1b8, U32(P, 0x1c0));
            const rx_addr found = L - 8;
            SU32(found, 0, 0);
            player_rider_name(P + 0x200, P + 0x210, found, U32(P, 0x1c0));
            if (U32(found, 0) == 0) { uint32_t pm; ACT_CALLR(pm, 0, rx_game_player_mode()); if (pm == 2) SU8(P, 0x200, 0); }
            ACT_CALL(1, rx_player_rider_attach(P, 1));
            SU32(P, 0x1c0, 0xffffffffu);
            ACT_END(L);
            return 3;
        }
        if (U32(P, 0x1b4) == 0 && U32(P, 0x1d8) == 0) {
            player_rider_clear(P);
            ACT_CALL(1, rx_player_rider_attach(P, 0));
            result = 3;
        }
    } else {
        const rx_addr game = U32(0, 0xa946b8);
        if (rx_rider_entry_ready(game + 0x15504, U32(P, 0x1b8)) != 0 &&
            (U32(world, 0x20018) == 0 || U32(P, 0x10) == 0)) {
            ACT_CALL(0, rx_player_rider_spawn(P, world));   /* EAX = P, ECX = world, no stack argument */
            SU32(P, 0x1d8, 0);
            ACT_END(L);
            return 4;
        }
    }
    ACT_END(L);
    return result;
}
RX_FUNCTION(0x00767200, rx_player_slot_spawn_step)
