/* loop_player.h -- the player object (MXPlayer, 0x12a0 bytes per slot at game +0x7460 + slot * 0x12a0): per-frame update, camera,
 * reset / wreck recovery handling, input groups (loop_player.c). Notes: core/notes/loop.md.
 * Register arguments of LTCG originals are named in the comments (EAX = ..., stack = ...); the core signature lists `this`/EAX
 * first, then stack arguments, then the other registers. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void     rx_player_update(rx_addr p, float dt);                          /* 0054af90 MXPlayer_Update (BaseDriver::vf7), stdcall ret 8 */
void     rx_player_update_camera(rx_addr p, float dt);                   /* 0054c150 Player_UpdateCamera, stdcall ret 8 */
uint32_t rx_player_apply_driving_camera(rx_addr p); /*0054aef0 thiscall RET0; C return=liveECX*/
uint32_t rx_player_set_camera(rx_addr p, uint32_t camera);               /* 0054c9b0 Player_SetCamera (EAX = p, ECX = camera; RET 0) */
void     rx_player_camera_changed(rx_addr arg, rx_addr dispatcher, rx_addr player); /* 00554ce0 "Camera Changed" event (ECX = arg, ESI = dispatcher, stack player; RET 4) */
void     rx_player_update_cameras(rx_addr p, float dt);                  /* 0054cc10 Player_UpdateCameras (EAX = p; stack dt; ret 4) */
void     rx_player_update_idle_rider(rx_addr p, float dt);               /* 0054cec0 (EDI = p; stack dt; ret 4) */
void     rx_player_update_reset_point(rx_addr p, float dt);              /* 0054eee0 Player_UpdateResetPoint (ECX = p; stack dt; ret 4) */
void     rx_player_check_reset_terrain(rx_addr p);                       /* 0054f3e0 Player_CheckResetTerrain (EAX = p) */
void     rx_player_check_out_of_bounds(rx_addr p);                       /* 0054f330 Player_CheckOutOfBounds (EAX = p) */
void     rx_player_handle_reset_input(rx_addr p);                        /* 0054e4f0 Player_HandleResetInput (ECX = p) */
void     rx_player_update_wreck_recovery(rx_addr p, float dt);           /* 0054ea30 Player_UpdateWreckRecovery (EAX = p; stack dt; ret 4) */
void     rx_player_update_crash_reset_timer(rx_addr p, float dt);        /* 0054ee00 Player_UpdateCrashResetTimer (EAX = p; stack dt; ret 4) */
void     rx_player_update_respawn_fade(rx_addr p, float dt);             /* 0054b850 Player_UpdateRespawnFade (EAX = p; stack dt; ret 4) */
uint32_t rx_player_can_reset(rx_addr p);                                 /* 0054d7c0 (EAX = p; result EAX) */
void     rx_player_restore_input_groups(rx_addr p);                      /* 0054d480 (EAX = p) */
void     rx_player_cancel_wreck(rx_addr p);                              /* 0054ec40 (EAX = p) */
void     rx_player_wreck_recovery_success(rx_addr p);                    /* 0054ec80 WreckRecoverySuccess (stack p; ret 4) */
void     rx_player_set_fade_hold(rx_addr p);                             /* 0054b720 (ESI = p) */
void     rx_player_set_fade_out(rx_addr p);                              /* 0054b780 (ESI = p) */
void     rx_player_set_fade_clear(rx_addr p);                            /* 0054b7f0 (ESI = p) */
void     rx_player_reset(rx_addr p, uint32_t flag);                      /* 0054fff0 (EAX = p; stack flag; ret 4) */
void     rx_player_event_hold_begin(rx_addr p, rx_addr who, rx_addr trick);   /* 0054d1a0 (ECX = p; ret 8) */
void     rx_player_event_hold_end(rx_addr p, rx_addr who, rx_addr trick);     /* 0054d1e0 */
void     rx_player_event_landed(rx_addr p, rx_addr who, rx_addr trick);        /* 0054d310 "StuntLanded" */
void     rx_player_event_takeoff(rx_addr p, rx_addr who, rx_addr trick);       /* 0054d360 "StuntTakeoff" */
void     rx_player_event_a(rx_addr p, rx_addr who, rx_addr arg);         /* 0054d2a0 (ECX = p; stack who, arg; ret 8) */
void     rx_player_event_b(rx_addr p, rx_addr who, rx_addr arg, uint32_t kind); /* 0054d530 (ECX = p; stack 3; ret 0xc) */
void     rx_player_event_name(rx_addr p, rx_addr a, rx_addr b);          /* 0054f0b0 (ECX = p; stack a, b; ret 8) */
void     rx_player_post_vectors(rx_addr a, rx_addr b, rx_addr p, rx_addr c); /* 0054feb0 (EAX = a, ESI = b; stack p, c; ret 8) */
void     rx_player_set_group0(rx_addr p, uint32_t on);                   /* 0054fcd0 (EAX = p, EDI = on) */
void     rx_player_set_group3(rx_addr p, uint32_t on);                   /* 0054fd20 (EAX = p, EDI = on) */
void     rx_player_set_group_147c(rx_addr p, uint32_t on);               /* 0054fd70 (ECX = p, EDX = on) */
void     rx_player_set_group_167c(rx_addr p, uint32_t on);               /* 0054fdc0 (ECX = p, EDX = on) */
void     rx_player_set_group_748(rx_addr p, uint32_t on);                /* 0054fe10 (ECX = p, EDX = on) */
void     rx_player_set_group_f34(rx_addr p, uint32_t on);                /* 0054fe60 (ECX = p, EDX = on) */
void     rx_player_slot_group2(uint32_t slot, uint32_t on);              /* 00548bf0 (ESI = slot, EDI = on) */
void     rx_player_slot_group5(uint32_t slot, uint32_t on);              /* 00548c40 (ESI = slot, EDI = on) */
float    rx_player_analog_348(uint32_t slot);                            /* 005485c0 (EAX = slot; result st(0)) */
uint32_t rx_player_pressed_2fa0(uint32_t slot);                          /* 00548880 (EAX = slot; result EAX) */
uint32_t rx_player_pressed_2da0(uint32_t slot);                          /* 00548850 (EAX = slot; result EAX) */
uint32_t rx_player_pressed_31a0(uint32_t slot);                          /* 005488b0 (EAX = slot; result EAX) */
uint32_t rx_player_scripted_wreck_type(uint32_t slot);                   /* 00548960 (ESI = slot; result EAX) */
void     rx_player_tracker_land(rx_addr tracker, rx_addr pos);           /* 004db070 (EAX = pos, ESI = tracker) */
void     rx_player_tracker_update(rx_addr tracker, rx_addr vehicle, float dt); /* 004db160 (EAX = tracker, EDI = vehicle; stack dt; ret 4) */
void     rx_lock_enter_raw(rx_addr lock);                                /* 00883ac0 (ECX = lock) */
void     rx_lock_leave_raw(rx_addr lock);                                /* 00883ad0 (ECX = lock) */
void     rx_lock_enter(rx_addr lock);                                    /* 00883d80 (ECX = lock) */
void     rx_lock_leave(rx_addr lock);                                    /* 00883d90 (ECX = lock) */
uint32_t rx_delegate_invoke(rx_addr self, rx_addr arg);                  /* 00554fc0 (ECX = delegate; stack arg; ret 4; result EAX passed through) */
void     rx_entity_place_body(rx_addr self, rx_addr a, rx_addr b);       /* 0055dc20 (ECX = entity; stack a, b; ret 8) */
uint32_t rx_activity_can_restart(rx_addr self);                          /* 0055e820 Activity::vf12 (ECX = activity; result EAX) */
void     rx_race_audio_state(rx_addr self);                              /* 00565640 MXActivityRace::vf52 (ECX = activity) */
void     rx_race_pop_rider(rx_addr self, rx_addr who, int32_t n, uint32_t c, uint32_t d); /* 00567660 (ECX = race; stack 4; ret 0x10) */
void     rx_activity_apply_tuning(rx_addr self);                         /* 00525cd0 (EAX = object) */
void     rx_records_add_int(rx_addr rec, int32_t delta, uint32_t name);  /* 00764fc0 MXRecords_AddInt (cdecl) */
void     rx_records_holeshot_won(uint32_t index, rx_addr base);          /* 00764ef0 (EAX = index, ECX = base) */
void     rx_records_attempted_jump(uint32_t index, rx_addr base);        /* 00764f40 (EAX = index, ECX = base) */
void     rx_records_failed_jump(uint32_t index, rx_addr base);           /* 00764f80 (EAX = index, ECX = base) */
uint32_t rx_records_longest_wheelie(uint32_t index, rx_addr base, int32_t value); /* 00764b90 (EAX = index, ECX = base; stack value; ret 4) */
uint32_t rx_records_longest_stoppie(uint32_t index, rx_addr base, int32_t value); /*00764c20 EAX/ECX, stackint, RET4*/
rx_addr  rx_list_init(rx_addr header);                                   /* 005442c0 (ESI = header; result EAX = header) */
void     rx_records_free_range(rx_addr first, rx_addr last);             /* 00545120 (EAX = first; stack last; cdecl) */
uint32_t rx_table11_get(rx_addr table, uint32_t index);                  /* 008841d0 (ECX = table; stack index; ret 4) */
void     rx_thread_note(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3); /* 00884390 (cdecl) */
void     rx_package_manager_update(rx_addr eax_obj, rx_addr mgr, float dt); /* 00540130 (EAX = obj, EDI = manager; stack dt; ret 4) */
#ifdef __cplusplus
}
#endif

