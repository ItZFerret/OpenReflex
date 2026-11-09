/* build_activity_clear.c -- construction unit 11: the player's vehicle and rider released (Player_ClearVehicle 0054a500, the
 * restart / next-race path of build_activity_spawn.c) and the small record releases it makes. Hand transliterations of the
 * disassembly (addresses in each comment).
 *
 * Vehicle list L (MXGame + 0x154b0): +0 the part records (a pointer vector {+0 data, +4 count}), +0x14 the model records, +0x28
 * the specs, +0x40 the made vehicles. A spec S: +4 the vehicle id, +8 its model M, +0xc + 4k the parts, +0x50 + 4k the pending
 * parts, +0xc0 high resolution. A part P: two std::strings (+0, +0x1c: the material / model package; size +0x14, capacity +0x18,
 * buffer +4), +0x38 HRAvailable, +0x40 the part id, +0x44 the reference count. A model M: +0xc factory, +0x18 references, +0x3c / +0x50
 * the part arrays. The package caches: the vehicle loader 0xd92ef0 (+4), the gear loader 0xd92ef4 (+8).
 * Rider manager G (MXGame + 0x15504): +0 the gear part records (a pointer vector), +0x14 the rider entries {+0 data, +0x18 count},
 * +0x28 the made riders (+0x2c count). A rider entry R: +4 the rider id, +0x14 high resolution, +0x1c + 4k the gear parts, +0x3c + 4k
 * the pending ones; a gear part: the same two strings, +0x38 HRAvailable, +0x40 id, +0x48 references.
 *
 * The package cache calls are the vehicle unit's (rx_pkgcache_*); the stack cookie of the originals' frames and the debug name
 * copies of 005400c0 (kept: they are calls of the original) leave only dead stack. */
#include "build_activity_clear.h"
#include "rider_cap.h"
#include "vehicle_audio.h"
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "build_vehicle.h"
#include "build_rider.h"
#include "build_alloc.h"
#include "anim.h"
#include "anim_scenario.h"
#include "bike_sim.h"
#include "fx.h"
#include "loop_engine.h"
#include "loop_platform.h"
#include "crt.h"
#include "replay_damage.h"
#include "scene_fx_entities.h"
#include "world_veh.h"
#include <string.h>

#define VEHICLE_LOADER 0x00d92ef0u
#define GEAR_LOADER    0x00d92ef4u
#define PLAYER_LIST    0x00ae0e08u
#define VEHICLE_LIST   0x00ae05c8u
#define RIDER_LIST     0x00ae19b0u

/* the package name of a std::string (the buffer is inline below capacity 0x10) */
static rx_addr str_chars(rx_addr s) { return U32(s, 0x18) >= 0x10 ? U32(s, 4) : s + 4; }

/* The first element equal to `value` of a pointer vector {+0 data, +4 count} removed by moving the last element into its place. */
static void vec_erase_value(rx_addr vec, uint32_t value) {
    const int32_t n = I32(vec, 4);
    if (n <= 0) return;
    const rx_addr data = U32(vec, 0);
    for (int32_t i = 0; i < n; i++) {
        if (U32(data, 4 * (uint32_t)i) == value) {
            SU32(data, 4 * (uint32_t)i, U32(data, 4 * (uint32_t)(n - 1)));
            SU32(vec, 4, U32(vec, 4) - 1);
            return;
        }
    }
}

/* 00544c80 (ECX = a pointer vector {+0 data, +4 count}, EDX = &value): the first element equal to *value removed (the last element
 * takes its place). */
void rx_ptr_vec_remove(rx_addr vec, rx_addr valp) { vec_erase_value(vec, U32(valp, 0)); }
RX_FUNCTION(0x00544c80, rx_ptr_vec_remove)

/* ---- part records: the packages ------------------------------------------------------------------------------------------------ */

/* The package names of a part / gear part (two std::strings at +0 and +0x1c, empty ones skipped): `fn(name)` for the name and,
 * when the part has high-resolution packages (+0x38), `fn(name_hr)` for the "HR_<name>" one (a frame buffer). */
typedef void (*package_fn)(rx_addr cache, rx_addr name);

/* 00540450 (ECX = part; stdcall loader, hr; RET 8): each package of the part released in the loader's cache (loader + 4): with `hr`
 * and the part's HRAvailable the "HR_" name replaces the name. Frame: push ebp; and esp, -8; sub 0x54; 3 pushes (the name buffer at
 * +0x20). */
void rx_part_release_packages(rx_addr part, rx_addr loader, uint32_t hr) {
    ACT_FRAME_EBP(F, 8, 0x54, 3);
    for (uint32_t k = 0; k < 2; k++) {
        const rx_addr s = part + 0x1c * k;
        if (I32(s, 0x14) <= 0) continue;
        rx_addr name = str_chars(s);
        if (hr != 0 && U32(part, 0x38) != 0) {
            rx_gear_hr_name(F + 0x20, name);                        /* 0078fd60 "HR_%s" */
            name = F + 0x20;
        }
        ACT_CALL(0, rx_pkgcache_release(loader + 4, name));         /* 00543c30 */
    }
    ACT_END(F);
}
RX_FUNCTION(0x00540450, rx_part_release_packages)

/* 00540300 (ECX = part; stdcall loader; RET 4): each package of the part unmarked in the loader's cache (loader + 4), and its "HR_"
 * name too when the part has high-resolution packages. Frame: push ebp; and esp, -8; sub 0x5c; 3 pushes (the name buffer at +0x20). */
void rx_part_unmark_packages(rx_addr part, rx_addr loader) {
    ACT_FRAME_EBP(F, 8, 0x5c, 3);
    for (uint32_t k = 0; k < 2; k++) {
        const rx_addr s = part + 0x1c * k;
        if (I32(s, 0x14) <= 0) continue;
        const rx_addr name = str_chars(s);
        ACT_CALL(0, rx_pkgcache_unmark(loader + 4, name));          /* 00543990 */
        if (U32(part, 0x38) != 0) {
            rx_gear_hr_name(F + 0x28, name);
            ACT_CALL(0, rx_pkgcache_unmark(loader + 4, F + 0x28));
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x00540300, rx_part_unmark_packages)

/* 0053f9f0 (EAX = part): the part's packages unmarked (00540300 with the vehicle loader) while it has a part id (+0x40 != -1),
 * then its two std::strings destroyed (the second first). Frame: push esi; push edi. */
void rx_part_destroy(rx_addr part) {
    ACT_FRAME(F, 0, 2);
    if (I32(part, 0x40) != -1) ACT_CALL(1, rx_part_unmark_packages(part, U32(0, VEHICLE_LOADER)));
    crt_str_dtor(part + 0x1c);                                      /* ~basic_string (IAT 0x9161b4) */
    crt_str_dtor(part);
    ACT_END(F);
}
RX_FUNCTION(0x0053f9f0, rx_part_destroy)

/* The same three for the rider's gear parts (the gear loader's cache is at +8). */

/* 0053d170 (ECX = gear part; stdcall loader, hr; RET 8) */
void rx_gear_release_packages(rx_addr gear, rx_addr loader, uint32_t hr) {
    ACT_FRAME_EBP(F, 8, 0x54, 3);
    for (uint32_t k = 0; k < 2; k++) {
        const rx_addr s = gear + 0x1c * k;
        if (I32(s, 0x14) <= 0) continue;
        rx_addr name = str_chars(s);
        if (hr != 0 && U32(gear, 0x38) != 0) {
            rx_gear_hr_name(F + 0x20, name);
            name = F + 0x20;
        }
        ACT_CALL(0, rx_pkgcache_release(loader + 8, name));         /* 00543c30 */
    }
    ACT_END(F);
}
RX_FUNCTION(0x0053d170, rx_gear_release_packages)

/* 0053d020 (ECX = gear part; stdcall loader; RET 4) */
void rx_gear_unmark_packages(rx_addr gear, rx_addr loader) {
    ACT_FRAME_EBP(F, 8, 0x5c, 3);
    for (uint32_t k = 0; k < 2; k++) {
        const rx_addr s = gear + 0x1c * k;
        if (I32(s, 0x14) <= 0) continue;
        const rx_addr name = str_chars(s);
        ACT_CALL(0, rx_pkgcache_unmark(loader + 8, name));          /* 00543990 */
        if (U32(gear, 0x38) != 0) {
            rx_gear_hr_name(F + 0x28, name);
            ACT_CALL(0, rx_pkgcache_unmark(loader + 8, F + 0x28));
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x0053d020, rx_gear_unmark_packages)

/* 0053ca30 (EAX = gear part): unmarked in the gear loader (0053d020) while it has an id, its two strings destroyed. */
void rx_gear_part_destroy(rx_addr gear) {
    ACT_FRAME(F, 0, 2);
    if (I32(gear, 0x40) != -1) ACT_CALL(1, rx_gear_unmark_packages(gear, U32(0, GEAR_LOADER)));
    crt_str_dtor(gear + 0x1c);
    crt_str_dtor(gear);
    ACT_END(F);
}
RX_FUNCTION(0x0053ca30, rx_gear_part_destroy)

/* ---- the Player List / owner table rows of a released id --------------------------------------------------------------------- */

/* After a spec / rider entry is gone: when its id (>= 0) is neither the VehicleId / RiderId nor the requested id of any Player List
 * row, its row in the owner list (Vehicle List / Rider List) is erased. `key` = the address of the id; the row reference
 * {+0 row, +4 set, +8 table} of the owner list is kept (as a movq) in the frame at `ref`. */
static void erase_unreferenced_row(rx_addr key, rx_addr id_name, rx_addr requested_name, rx_addr owner_list, rx_addr ref) {
    rx_addr col = act_column(PLAYER_LIST, id_name);
    rx_addr rows_a, rows_b;
    ACT_CALLR(rows_a, 1, rx_tbl_rows(col, I32(key, 0)));              /* 00418e50 */
    col = act_column(PLAYER_LIST, requested_name);
    ACT_CALLR(rows_b, 1, rx_tbl_rows(col, I32(key, 0)));
    if (I32(key, 0) < 0 || U32(rows_a, 0x1c) != 0 || U32(rows_b, 0x1c) != 0) return;
    col = act_column(owner_list, id_name);
    rx_addr r;
    ACT_CALLR(r, 1, rx_tbl_row(col, I32(key, 0)));                    /* 00418ec0 */
    SU32(ref, 0, U32(r, 0));
    SU32(ref, 4, U32(r, 4));
    if (U32(r, 8) != 0 && I32(ref, 0) != -1) ACT_CALL(2, rx_tbl_erase_row(U32(r, 8), U32(ref, 0)));   /* 004ece50 */
}

/* ---- the vehicle spec ------------------------------------------------------------------------------------------------------- */

/* A part record dropped by one reference: at the last it leaves the list `vec`, its packages are unmarked and strings destroyed, and
 * its storage freed. `slot` is the frame word that holds the part for the vector removal. */
static void part_unref(rx_addr list, rx_addr part, rx_addr slot) {
    SU32(part, 0x44, U32(part, 0x44) - 1);
    if (U32(part, 0x44) != 0) return;
    SU32(slot, 0, part);
    ACT_CALL(0, rx_ptr_vec_remove(list, slot));                       /* 00544c80 (ECX = the part records) */
    ACT_CALL(0, rx_part_destroy(part));                               /* 0053f9f0 */
    ACT_CALL(1, rx_reflex_aligned_free(part));                        /* 0078fc50 */
}

/* 00540dd0 (EAX = a vehicle id; stdcall the vehicle list; RET 4): the spec of that id released: for each of its 17 slots the
 * part's packages are released in the vehicle loader and its (and the pending part's) reference dropped (00544c80 / 0053f9f0 at the
 * last), then the model's; the id's Vehicle List row is erased when no Player List row uses it (VehicleId / RequestedVehicleId),
 * the spec leaves the list's spec vector and is freed. Frame: sub 0x10; 4 pushes (the pointer being dropped at +0x10, the row
 * reference at +0x14). */
void rx_vehicle_spec_free(rx_addr list, uint32_t vehicle_id) {
    ACT_FRAME(F, 0x10, 4);
    rx_addr spec = 0;
    const int32_t count = I32(list, 0x2c);
    for (int32_t i = 0; i < count; i++) {
        const rx_addr s = U32(U32(list, 0x28), 4 * (uint32_t)i);
        if (s != 0 && U32(s, 4) == vehicle_id) { spec = s; break; }
    }
    if (spec == 0) { ACT_END(F); return; }
    for (uint32_t k = 0; k < 0x11; k++) {
        if (U32(spec, 0xc + 4 * k) != 0) {
            ACT_CALL(2, rx_names_to_frame(U32(U32(spec, 8), 0xc), k));               /* 005400c0 */
            ACT_CALL(2, rx_part_release_packages(U32(spec, 0xc + 4 * k), U32(0, VEHICLE_LOADER), U32(spec, 0xc0)));   /* 00540450 */
            ACT_CALL(2, rx_names_to_frame(U32(U32(spec, 8), 0xc), k));
            const rx_addr part = U32(spec, 0xc + 4 * k);
            if (part != 0) part_unref(list, part, F + 0x10);
        }
        const rx_addr pending = U32(spec, 0x50 + 4 * k);
        if (pending != 0) part_unref(list, pending, F + 0x10);
    }
    const rx_addr model = U32(spec, 8);
    if (model != 0) {
        SU32(model, 0x18, U32(model, 0x18) - 1);
        if (U32(model, 0x18) == 0) {
            SU32(F, 0x10, model);
            ACT_CALL(0, rx_ptr_vec_remove(list + 0x14, F + 0x10));                   /* 00544c80 (the model records) */
            ACT_CALL(1, rx_reflex_aligned_free(U32(model, 0x50)));
            ACT_CALL(1, rx_reflex_aligned_free(U32(model, 0x3c)));
            ACT_CALL(1, rx_reflex_aligned_free(model));
        }
    }
    erase_unreferenced_row(spec + 4, 0x00936cb4 /* "VehicleId" */, 0x0093df5c /* "RequestedVehicleId" */, VEHICLE_LIST, F + 0x14);
    vec_erase_value(list + 0x28, spec);
    ACT_CALL(1, rx_reflex_aligned_free(spec));
    ACT_END(F);
}
RX_FUNCTION(0x00540dd0, rx_vehicle_spec_free)

/* 005408e0 (EAX = the vehicle, EDI = the vehicle list): the vehicle taken out of the list's made vehicles (+0x40, count +0x44), out of
 * its world's scene (007d3040: ESI = the vehicle, the scene [[+0x188] + 0x20138]), its spec freed (00540dd0 with the vehicle's id
 * +0x18c), then its vf9 (+0x24) and the deleting destructor vf0(1). Frame: push ecx; push ebx; push esi. */
void rx_vehicle_remove(rx_addr vehicle, rx_addr list) {
    ACT_FRAME(F, 0, 3);
    vec_erase_value(list + 0x40, vehicle);
    const rx_addr world = U32(vehicle, 0x188);
    const rx_addr scene = world != 0 ? U32(world, 0x20138) : 0;
    ACT_CALL(1, rx_scene_manager_remove_entity(vehicle, scene));      /* 007d3040 */
    ACT_CALL(1, rx_vehicle_spec_free(list, U32(vehicle, 0x18c)));     /* 00540dd0 */
    act_vcall(vehicle, 0x24, 0, 0);
    act_vcall(vehicle, 0x00, 1, (const uint32_t[]){1});
    ACT_END(F);
}
RX_FUNCTION(0x005408e0, rx_vehicle_remove)

/* ---- the rider entry --------------------------------------------------------------------------------------------------------- */

/* 0053ed20 (EDI = the rider manager G; stdcall the rider entry; RET 4): the entry released: each of its 8 gear parts' packages
 * released in the gear loader and the part dropped (at the last reference out of G's part vector, destroyed, freed), the 8 pending
 * ones likewise, the rider id's Rider List row erased when no Player List row uses it (RiderId / RequestedRiderId), the entry out
 * of G's entry vector, freed. Frame: sub 0x14; push ebx / ebp / esi (the pointer being dropped at +0x10, the row reference at +0x14). */
void rx_rider_entry_free(rx_addr entry, rx_addr manager) {
    ACT_FRAME(F, 0x14, 3);
    if (entry == 0) { ACT_END(F); return; }
    for (uint32_t k = 0; k < 8; k++) {
        rx_addr gear = U32(entry, 0x1c + 4 * k);
        if (gear == 0) continue;
        ACT_CALL(2, rx_gear_release_packages(gear, U32(0, GEAR_LOADER), U32(entry, 0x14)));   /* 0053d170 */
        gear = U32(entry, 0x1c + 4 * k);
        SU32(F, 0x10, gear);
        if (gear == 0) continue;
        SU32(gear, 0x48, U32(gear, 0x48) - 1);
        if (U32(gear, 0x48) != 0) continue;
        ACT_CALL(0, rx_ptr_vec_remove(manager, F + 0x10));                          /* 00544c80 */
        ACT_CALL(0, rx_gear_part_destroy(gear));                                    /* 0053ca30 */
        ACT_CALL(1, rx_reflex_aligned_free(gear));
    }
    for (uint32_t k = 0; k < 8; k++) {
        const rx_addr gear = U32(entry, 0x3c + 4 * k);
        if (gear == 0) continue;
        SU32(gear, 0x48, U32(gear, 0x48) - 1);
        if (U32(gear, 0x48) != 0) continue;
        vec_erase_value(manager, gear);
        ACT_CALL(0, rx_gear_part_destroy(gear));
        ACT_CALL(1, rx_reflex_aligned_free(gear));
    }
    erase_unreferenced_row(entry + 4, 0x0093dec8 /* "RiderId" */, 0x0093df70 /* "RequestedRiderId" */, RIDER_LIST, F + 0x14);
    vec_erase_value(manager + 0x14, entry);
    ACT_CALL(1, rx_reflex_aligned_free(entry));
    ACT_END(F);
}
RX_FUNCTION(0x0053ed20, rx_rider_entry_free)

/* 0053ece0 (EAX = the rider manager; stdcall a rider id; RET 4): the manager's entry of that id released. Frame: push ecx / ebx /
 * esi / edi. */
void rx_rider_release(rx_addr manager, uint32_t rider_id) {
    ACT_FRAME(F, 0, 4);
    const int32_t count = I32(manager, 0x18);
    for (int32_t i = 0; i < count; i++) {
        const rx_addr entry = U32(U32(manager, 0x14), 4 * (uint32_t)i);
        if (U32(entry, 4) == rider_id) {
            ACT_CALL(1, rx_rider_entry_free(entry, manager));
            break;
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x0053ece0, rx_rider_release)

/* 0053ebc0 (EAX = the rider object, EDI = the rider manager): the rider's animation jobs waited for, the rider taken out of its scene
 * (007d3040: ESI = the rider, the scene [+0x10]) and its vf9 (+0x24) run, out of the manager's made riders (+0x28, count +0x2c); the
 * entry of its rider id (+0x5b51c) marked (+0x18 = 0: 0's own +0x18 when absent), the object deleted (vf0(1)), then the entry released
 * (0053ece0). Frame: sub 8; push ebx / ebp / esi (the rider id at +0x10). */
void rx_rider_remove(rx_addr rider, rx_addr manager) {
    ACT_FRAME(F, 8, 3);
    ACT_CALL(0, rx_rider_animation_wait_jobs());                      /* 0061d130 */
    const uint32_t rider_id = U32(rider, 0x5b51c);
    SU32(F, 0x10, rider_id);
    ACT_CALL(1, rx_scene_manager_remove_entity(rider, U32(rider, 0x10)));   /* 007d3040 */
    act_vcall(rider, 0x24, 0, 0);
    vec_erase_value(manager + 0x28, rider);
    rx_addr entry = 0;
    const int32_t count = I32(manager, 0x18);
    for (int32_t i = 0; i < count; i++) {
        const rx_addr e = U32(U32(manager, 0x14), 4 * (uint32_t)i);
        if (U32(e, 4) == rider_id) { entry = e; break; }
    }
    SU32(entry, 0x18, 0);
    act_vcall(rider, 0x00, 1, (const uint32_t[]){1});
    ACT_CALL(1, rx_rider_release(manager, U32(F, 0x10)));            /* 0053ece0 */
    ACT_END(F);
}
RX_FUNCTION(0x0053ebc0, rx_rider_remove)

/* ---- small record releases ---------------------------------------------------------------------------------------------------- */

/* 00408fe0 MudRecord_Free (EDX = the vehicle): the mud record (12 of them, 0xa96d90 + 0x40 k) owned by the vehicle (+0x30) cleared:
 * the four values +0 .. +0xc and the four +0x10 .. +0x1c zero, the owner 0, and the colour (+0x20 .. +0x2c) back to the default. */
void rx_mud_record_free(rx_addr vehicle) {
    for (uint32_t k = 0; k < rx_mud_n(); k++) {   /* (w-rc-core) + the side records taken (rider_cap.h) */
        const rx_addr rec = rx_mud_rec(k);
        if (U32(rec, 0x30) != vehicle) continue;
        for (uint32_t o = 0; o < 0x20; o += 4) SU32(rec, o, 0);
        SU32(rec, 0x30, 0);
        SU32(rec, 0x20, 0x3e830165 /* 0x9777c0 */);
        SU32(rec, 0x24, 0x3e2eac86 /* 0x9777bc */);
        SU32(rec, 0x28, 0x3df0ed3e /* 0x9777b8 */);
        SF32(rec, 0x2c, 1.0f /* 0x931f24 */);
        return;
    }
}
RX_FUNCTION(0x00408fe0, rx_mud_record_free)

/* 0047c380 (EDX = the vehicle): the scripted pose override (12 slots, 0xad81d0 + 0xa50 k, the vehicle at +0xa40) of the vehicle
 * ended (Simulation_EndPoseOverride 00434ef0 on the vehicle's simulation +0x234) and the slot cleared (+0xa40, the time +0xa3c and
 * the duration +0xa38). Frame: push ebp; and esp, -8; sub 8; push esi; push edi. */
void rx_pose_override_release(rx_addr vehicle) {
    ACT_FRAME_EBP(F, 8, 8, 2);
    for (uint32_t k = 0; k < 12; k++) {
        const rx_addr slot = 0x00ad81d0u + 0xa50 * k;
        if (U32(slot, 0xa40) != vehicle) continue;
        if (U32(slot, 0xa40) != 0) ACT_CALL(0, bike_end_pose_override(U32(U32(slot, 0xa40), 0x234)));
        SU32(slot, 0xa40, 0);
        SF32(slot, 0xa3c, 0.0f);
        SF32(slot, 0xa38, 0.0f);
        break;
    }
    ACT_END(F);
}
RX_FUNCTION(0x0047c380, rx_pose_override_release)

/* 00691d30 (EDX = the vehicle): the vehicle fx set (0xc11e90 + 0x17c30 k, the vehicle at +0) of the vehicle released: the vehicle and
 * the particle entity (+0x220) cleared, the set rebound (00697870, ESI = the set). Frame: push esi. */
void rx_vehicle_effects_release(rx_addr vehicle) {
    for (uint32_t k = 0; k < rx_rc_tab_n(0x00c11e90u); k++) {   /* (w-rc-core) + the side sets taken (rider_cap.h) */
        const rx_addr set = rx_rc_tab(0x00c11e90u, 0x17c30u, k);
        if (U32(set, 0) != vehicle) continue;
        ACT_FRAME(F, 0, 1);
        SU32(set, 0, 0);
        SU32(set, 0x220, 0);
        ACT_CALL(0, rx_vehicle_effects_rebind(set));
        ACT_END(F);
        return;
    }
}
RX_FUNCTION(0x00691d30, rx_vehicle_effects_release)

/* 0069a0c0 ParticleManager2D_Clear (EDI = a particle manager): under the manager's lock (+0xf4) the particle counts (+0xac, +0x74)
 * zero, the job pool (0xd94ae0, constructed by the engine) idle, then the emitter state reset: +0x20 = 1, +0xb8 / +0xc8 / +0xb0 /
 * +0x70 / +0xe4 = 0. Frame: push ebx / ebp / esi. */
void rx_particle_manager2d_clear(rx_addr manager) {
    ACT_FRAME(F, 0, 3);
    ACT_CALL(1, loop_enter_cs(manager + 0xf4));                       /* EnterCriticalSection (IAT 0x91600c) */
    SU32(manager, 0xac, 0);
    SU32(manager, 0x74, 0);
    if (!(U8(0, 0x00d94ad8) & 1)) act_unreached(0x0069a0e4, "the job pool's static constructor (00881a70) not run");
    ACT_CALL(0, rx_jobpool_wait_idle(0x00d94ae0));                    /* 00881e20 (ESI = the pool) */
    SU32(manager, 0x20, 1);
    SU32(manager, 0xb8, 0);
    SU32(manager, 0xc8, 0);
    SU32(manager, 0xb0, 0);
    SU32(manager, 0x70, 0);
    SU32(manager, 0xe4, 0);
    ACT_CALL(1, loop_leave_cs(manager + 0xf4));                       /* LeaveCriticalSection (IAT 0x9160f8) */
    ACT_END(F);
}
RX_FUNCTION(0x0069a0c0, rx_particle_manager2d_clear)

/* 00550e10 (stdcall the player; RET 4): the player's respawn pose saved from its vehicle (+0x1ac): the vehicle's position vector
 * (+0x200, 16 bytes) to +0x10c0 and its up vector (the orientation quaternion +0x210 rotated: x = qx 2qz + 2qy qw, y = 2qz qy - 2qx
 * qw, z = 1 - (qx 2qx + 2qy qy)) to +0x10d0; nothing without a vehicle. */
void rx_player_save_respawn_pose(rx_addr player) {
    const rx_addr vehicle = U32(player, 0x1ac);
    if (vehicle == 0) return;
    for (uint32_t o = 0; o < 16; o += 4) SU32(player, 0x10c0 + o, U32(vehicle, 0x200 + o));
    const rx_addr v = U32(player, 0x1ac);
    const float qx = F32(v, 0x210), qy = F32(v, 0x214), qw = F32(v, 0x21c);
    const float qz2 = F32(v, 0x218) * 2.0f /* 0x92e330 */;
    const float qx2 = qx * 2.0f, qy2 = qy * 2.0f;
    SF32(player, 0x10d0, qx * qz2 + qy2 * qw);
    SF32(player, 0x10d4, qz2 * qy - qx2 * qw);
    SF32(player, 0x10d8, 1.0f /* 0x931f24 */ - (qx * qx2 + qy2 * qy));
}
RX_FUNCTION(0x00550e10, rx_player_save_respawn_pose)

/* ---- the vehicle audio slot (an audio boundary) ----------------------------------------------------------------------------- */

/* 00680d10 (ECX = the vehicle audio manager [0xd92f00] (unused), EDX = the vehicle; stdcall the manager; RET 4): the manager's slot
 * of the vehicle (12 slots S = manager + 0x1b10 k, the vehicle at S + 0x10) returned to the free pool: the slot's vehicle fields
 * cleared, its sound record (S + 0x520) reset (00685a40: EDI = the record, EAX = 0: the engine / skid events stopped), the sound set
 * (the pointer at S + 0x1b10: its counters zeroed, 006826d0 frees its sounds, 00701c50 queues it on the manager's free deque
 * +0x144e8 while the deque exists (+0x144e4), the pointer cleared), the rider sounds (0068a0b0 on S + 0xa70, whose +0x14 / +0x18
 * are cleared first) and the record removed from the manager's active list (+0x144d0, count +0x144d4).
 * AUDIO BOUNDARY: the four callees are the audio unit's (FMOD events and the sound heap). A slot is assigned only by the spawn's
 * vehicle foley hook 00680b10 (and Player_JoinActivity), which the product does not run (build_activity_spawn.c: the oracle runs
 * the original, the product does nothing), so no slot of the product holds a vehicle: the search finds none and the product never
 * reaches the audio calls; the oracle runs them as the original. No gameplay code reads the slots or the sound sets. */
void rx_vehicle_audio_release(rx_addr vehicle, rx_addr manager) {
    ACT_FRAME(F, 0, 4);
#ifndef RX_ORACLE
    rx_vehicle_audio_unregister(vehicle);   /* w-sfx: the host-side slot table (game/vehicle_audio.c) */
#endif
    for (uint32_t k = 0; k < 12; k++) {
        if (U32(manager, 0x10 + 0x1b10 * k) != vehicle) continue;
        const rx_addr slot = manager + 0x1b10 * k;
        const rx_addr record = slot + 0x520;
        SU32(slot, 0x10, 0);
        SU32(slot, 0x14, 0);
#ifdef RX_ORACLE
        {
            rx_regs in, out;
            memset(&in, 0, sizeof in);
            in.r[7] = record;
            rx_call_original_ex(0x00685a40, &in, &out, 0, 0);               /* the record reset (EDI = the record, EAX = 0) */
            rx_addr set = U32(slot, 0x1b10);
            if (set != 0) {
                SU32(set, 4 * 0x3beb, 0);
                SU32(set, 4 * 0x3bec, 0);
                SU32(set, 4 * 0x3bed, 0);
                SF32(set, 4 * 0x3bea, 0.0f);
                if (U32(set, 0) != 0) {
                    const uint32_t a[1] = {set};
                    rx_call_original_ex(0x006826d0, &in, &out, 1, a);       /* the set's sounds freed */
                }
                set = U32(slot, 0x1b10);
                SU32(F, 0x14, set);
                if (set != 0 && U32(manager, 0x144e4) != 0) {
                    rx_regs q;
                    memset(&q, 0, sizeof q);
                    q.r[6] = manager + 0x144e8;
                    const uint32_t a[1] = {F + 0x14};
                    rx_call_original_ex(0x00701c50, &q, &out, 1, a);        /* the set queued on the free deque */
                }
                SU32(slot, 0x1b10, 0);
            }
            SU32(slot, 0xa70 + 0x14, 0);
            SU32(slot, 0xa70 + 0x18, 0);
            const uint32_t a[1] = {slot + 0xa70};
            rx_call_original_ex(0x0068a0b0, &in, &out, 1, a);               /* the rider sounds reset */
        }
#else
        act_unreached(0x00680d37, "a vehicle audio slot is assigned (the product runs no vehicle foley hook)");
#endif
        vec_erase_value(manager + 0x144d0, record);
        break;
    }
    ACT_END(F);
}
RX_FUNCTION(0x00680d10, rx_vehicle_audio_release)

/* ---- the damage manager ---------------------------------------------------------------------------------------------------- */

/* 00479dd0 (ECX = a damage manager entry: +0 the part count, +0x2c + 4i the part entities, +0x370 + 4i their nodes, +0x3d0 + i
 * "active" flags, +0x4c + 8i 8-byte sub-objects): each part released: if active, taken out of its scene and its physics entity
 * deactivated (0047a830), its vf9 (+0x24), its asset handle (vf23 +0x5c, +0x60) removed from the database (the engine's database
 * vf22 +0x58: handle, the frame number), the part deleted (vf0(1)) and cleared, and the sub-object's vf1 (+4) run. A debug text
 * names the handle ("Removing damage part asset handle %x from the database.\n", formatted into the frame). Frame: sub 0x94; 4 pushes
 * (the text buffer at +0x18, the sub-object pointer at +0x10, the entry at +0x14, the security cookie at +0x9c). */
void rx_damage_entry_release(rx_addr entry) {
    ACT_FRAME(F, 0x94, 4);
    SU32(F, 0x9c, U32(0, 0x009b45e4) ^ (F + 0x10));                     /* the security cookie */
    SU32(F, 0x14, entry);
    const uint32_t count = U32(entry, 0);
    if (count != 0) SU32(F, 0x10, entry + 0x4c);                        /* the sub-object cursor (+8 per part) */
    for (uint32_t i = 0; i < count; i++, SU32(F, 0x10, U32(F, 0x10) + 8)) {
        const rx_addr part = U32(entry, 0x2c + 4 * i);
        if (part == 0) continue;
        if (U32(entry, 0x370 + 4 * i) != 0 && U8(entry, 0x3d0 + i) != 0) ACT_CALL(1, rx_replay_damage_deactivate(entry, i));   /* the inlined 0047a830 */
        act_vcall(part, 0x24, 0, 0);
        const rx_addr handle = act_vcall(part, 0x5c, 0, 0);
        {
            const rx_addr sp = rx_sp();
            SU32(sp, -8, 0x0093c3b4 /* "Removing damage part asset handle %x from the database.\n" */);
            SU32(sp, -4, U32(handle, 0x60));
            rx_set_sp(sp - 12);
            rx_format_buffer(F + 0x18, 0x80, 0x0093c3b4, sp - 4);         /* lea edi, [esp + 0x20] after the two pushes */
            rx_set_sp(sp);
        }
        const rx_addr engine = U32(0, 0x00a95844);
        const rx_addr database = U32(engine, 8);
        const uint32_t frame = U32(U32(engine, 0x10), 0x58);
        const rx_addr handle2 = act_vcall(part, 0x5c, 0, 0);
        act_vcall(database, 0x58, 2, (const uint32_t[]){U32(handle2, 0x60), frame});
        if (part != 0) act_vcall(part, 0x00, 1, (const uint32_t[]){1});
        SU32(entry, 0x2c + 4 * i, 0);
        act_vcall(U32(F, 0x10), 0x04, 0, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00479dd0, rx_damage_entry_release)

/* 0047bbe0 (EDX = the damage manager, ESI = the vehicle): the manager's entry of the vehicle (12 entries of 0x410 bytes from +0x10,
 * the vehicle at +0x3fc of each) released: its vehicle cleared, then 00479dd0. Frame: push ecx. */
void rx_damage_release(rx_addr manager, rx_addr vehicle) {
    ACT_FRAME(F, 0, 1);
    for (uint32_t i = 0; i < rx_dmg_n(); i++) {   /* (w-rc-core) + the side entries made */
        if (U32(rx_dmg_entry(manager, i), 0x3fc) != vehicle) continue;
        const rx_addr entry = rx_dmg_entry(manager, i);
        SU32(entry, 0x3fc, 0);
        ACT_CALL(0, rx_damage_entry_release(entry));
        break;
    }
    ACT_END(F);
}
RX_FUNCTION(0x0047bbe0, rx_damage_release)

/* ---- the player's camera entities --------------------------------------------------------------------------------------------- */

/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static uint32_t x87_copy(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }

/* 0054bda0 (ESI = the player): the player's view entities released: the current camera (+0x9a8) told vf29 (+0x74) and cleared; for
 * each of the five camera entities (+0x974 + 4i) - the fifth first writing its CameraParam1..3 (+0x368 / +0x354 / +0x360) back to
 * the player (+0x224 / +0x228 / +0x22c), the values the next spawn's cameras start with - the camera sound effects manager
 * ([0xd92f04], 0068d200) drops the camera, then its vf9 (+0x24) and the deleting destructor vf0(1) run and the slot clears; then
 * the quick camera (+0x998), the start-gate camera (+0x99c) and the entity +0x220 likewise (vf9, vf0(1)). Frame: push ebp; and esp,
 * -8; push ebx; push edi.
 * AUDIO BOUNDARY: the camera sound effects manager is the audio unit's: the product builds none ([0xd92f04] is 0) and no camera
 * registers with it (the camera unit's constructors do not call 0068cd90), so its map holds nothing to drop; the oracle runs the
 * original 0068d200 (an audio command and the sound objects' destructors). */
void rx_player_release_objects(rx_addr player) {
    ACT_FRAME_EBP(F, 8, 0, 2);
    if (U32(player, 0x9a8) != 0) act_vcall(U32(player, 0x9a8), 0x74, 0, 0);
    SU32(player, 0x9a8, 0);
    for (uint32_t i = 0; i < 5; i++) {
        if (i == 4) {
            const rx_addr cam = U32(player, 0x984);
            SU32(player, 0x224, x87_copy(U32(cam, 0x368)));
            SU32(player, 0x228, x87_copy(U32(cam, 0x354)));
            SU32(player, 0x22c, x87_copy(U32(cam, 0x360)));
        }
#ifdef RX_ORACLE
        {
            rx_regs in, out;
            memset(&in, 0, sizeof in);
            in.r[0] = U32(0, 0x00d92f04);
            const uint32_t a[1] = {U32(player, 0x974 + 4 * i)};
            rx_call_original_ex(0x0068d200, &in, &out, 1, a);
        }
#endif
        act_vcall(U32(player, 0x974 + 4 * i), 0x24, 0, 0);
        if (U32(player, 0x974 + 4 * i) != 0) act_vcall(U32(player, 0x974 + 4 * i), 0x00, 1, (const uint32_t[]){1});
        SU32(player, 0x974 + 4 * i, 0);
    }
    static const uint32_t slots[3] = {0x998, 0x99c, 0x220};
    for (uint32_t k = 0; k < 3; k++) {
        if (U32(player, slots[k]) == 0) continue;
        act_vcall(U32(player, slots[k]), 0x24, 0, 0);
        if (U32(player, slots[k]) != 0) act_vcall(U32(player, slots[k]), 0x00, 1, (const uint32_t[]){1});
        SU32(player, slots[k], 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x0054bda0, rx_player_release_objects)
