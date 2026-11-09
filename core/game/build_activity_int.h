/* build_activity_int.h -- internal declarations shared by the files of construction unit 11 (build_activity_start.c,
 * build_activity_grid.c, build_activity_records.c, build_activity_frame.c, build_freeride*.c). Not an API. */
#pragma once
#include "build_activity_util.h"

/* an event-type instantiation of FUN_00590c00's template (build_activity_events.inc) called on a channel at the original's depth:
 * the current stack pointer is the caller's ESP before the call (the name is pushed when the instantiation takes it) */
void act_event_type(rx_addr fn, rx_addr ch, rx_addr name);

/* the handler-subscription template (00588130, 005881f0, 00586dc0 ...): called as a core callee with the template's stack
 * arguments (nargs) at the original's depth; pushes = the registers the template saves before 00593220; under the event's lock an
 * EventHandlerProxy (0x14: vtable, +4 0, +8 the signature's type name, +0xc object, +0x10 method) is appended to the event's
 * handler list (005b9ff0) when the signature names match; the template stores the proxy in its first stack argument slot.
 * tmpl / conv: the template's address and register arguments (1 EAX = channel, 2 ECX = channel, 4 EAX = name, 8 ECX = name). */
void act_subscribe(rx_addr tmpl, uint32_t conv, int nargs, uint32_t pushes, rx_addr ch, rx_addr name, rx_addr vtable, rx_addr td, rx_addr obj, rx_addr method);

/* an event raised on a manager / channel (the inlined raisers: typeid(signature).name() caches the type's name; FUN_004020c0 with
 * EAX = manager, EDI = name, the payload's address and size); the current stack pointer is the raiser's ESP at the type_info call */
void act_raise(rx_addr manager, rx_addr name, rx_addr td, rx_addr payload, uint32_t size);

/* build_activity_frame.c */
void     rx_engine_frame_mark(rx_addr e);                    /* FUN_0084ea00 (EDI = the engine's frame object [core + 0x10]) */
void     rx_rumble_frame(void);                              /* FUN_0081f290 */
void     rx_scene_set_active(uint32_t active, rx_addr s);    /* FUN_007d20b0 (EAX = active; stdcall scene; RET 4) */

/* build_activity_start.c */
void     rx_activity_name_state(rx_addr A, uint32_t state, rx_addr name);   /* 005585e0 (stdcall A, state, std::string by value; RET 0x24) */
void     rx_activity_vf2(rx_addr A, rx_addr arg);            /* Activity::vf2 00556ec0 (thiscall; RET 4) */
void     rx_race_stub_vf2(rx_addr A, rx_addr arg);           /* MXActivityRaceStub::vf2 00566170 (thiscall; RET 4) */
void     rx_race_vf2(rx_addr A, rx_addr arg);                /* MXActivityRace::vf2 00564b20 (thiscall; RET 4) */
void     rx_race_stub_vf4(rx_addr A);                        /* MXActivityRaceStub::vf4 00566310 (thiscall) */
void     rx_world_link_lights(rx_addr w);                    /* 00767ba0 (ESI = world) */
uint32_t rx_scene_collect_prefixed(rx_addr s, rx_addr prefix, rx_addr out, int32_t cap, rx_addr overflow);   /* 007d29e0 (EAX = scene; RET 0x10) */

/* build_activity_audio.c */
void     rx_audio_logic_vf32(rx_addr l);                     /* ActivityAudioLogic::vf32 00581030 (thiscall) */
void     rx_audio_logic_vf29(rx_addr l, rx_addr A);          /* ActivityAudioLogic::vf29 00580e70 (thiscall; RET 4) */
void     rx_race_audio_logic_vf29(rx_addr l, rx_addr A);     /* RaceActivityAudioLogic::vf29 00582ed0 (thiscall; RET 4) */
void     rx_race_audio_logic_vf3(rx_addr l);                 /* RaceActivityAudioLogic::vf3 00582610 (thiscall) */

/* build_activity_grid.c */
void     rx_race_grid_from_gate(rx_addr A);                  /* 005666d0 (stdcall; RET 4) */
void     rx_race_grid_from_track(rx_addr A);                 /* 00564c90 (EDI = activity) */
uint32_t rx_track_quad_contains(rx_addr n, rx_addr e, rx_addr p);   /* 00538bb0 (ECX = node, EAX = next, EDI = position) */
void     rx_track_nearest_in_radius(rx_addr p, rx_addr t, rx_addr out, float radius, uint32_t skip4, uint32_t skip2);   /* 005385f0 (EAX = p; RET 0x14) */
uint32_t rx_game_update_load_wait(rx_addr g, float dt);      /* MXGame_UpdateLoadWait 0075d760 (stdcall; RET 8) */
uint32_t rx_game_can_begin_world_load(rx_addr g);            /* MXGame_CanBeginWorldLoad 0075f500 (stdcall; RET 4) */
uint32_t rx_game_players_ready(rx_addr g);                   /* 0075f2b0 (stdcall; RET 4) */
void     rx_game_grid_pass(rx_addr g, uint32_t type, uint32_t mode, uint32_t assign);   /* 0075f890 (stdcall; RET 0x10) */
void     rx_game_race_start(void);                           /* 0075e020 (cdecl) */

/* build_activity_records.c */
void     rx_activity_entities_start(rx_addr A);              /* 00557fe0 (stdcall; RET 4) */
void     rx_activity_vf50(rx_addr A);                        /* Activity::vf50 0055a4c0 (thiscall) */
void     rx_records_join(uint32_t slot, rx_addr rec, uint32_t vid, uint32_t type, uint32_t area, rx_addr level);   /* MXRecords_Join 00764170 (EAX = slot; RET 0x14) */
