/* build_activity_teardown.h -- the activity's teardown (Activity::vf3 and the event-system destruction it reaches,
 * build_activity_teardown.c) and the activity audio logic's teardown (ActivityAudioLogic::vf30 / RaceActivityAudioLogic::vf30 / vf4,
 * build_activity_audio_teardown.c). The mirror of the construction side (build_activity_start.c / build_activity_audio.c): every
 * subscription the activity made is removed (the handler-unsubscription templates), the activity's own event channel leaves the
 * global channel registry and is destroyed with its events and their handlers. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ---- the handler-unsubscription templates: the key {proxy vtable, 0, typeid(signature).name(), object, method} on the frame
 * (0x14 bytes at the template's ESP + 4), the event found in the registry / channel (00593220), the listener that accepts the key
 * removed (00592ef0). act_unlisten is the shared body, called at the template's entry ESP; aligned: the `push ebp; mov ebp, esp;
 * and esp, -8; sub esp, 0x14; push esi` prologue, else `sub esp, 0x14; push esi`. */
void act_unlisten(int aligned, rx_addr registry, rx_addr name, rx_addr vtable, rx_addr td, rx_addr self, rx_addr method);
/* the same body at a given frame F (the template's ESP after its prologue) */
void act_unlisten_at(rx_addr F, rx_addr registry, rx_addr name, rx_addr vtable, rx_addr td, rx_addr self, rx_addr method);

void rx_activity_unlisten_net_timer(rx_addr registry, rx_addr act);        /* 00587330 (EAX = registry; stdcall act; RET 4) */
void rx_activity_unlisten_net_session(rx_addr registry, rx_addr act);      /* 00587390 (EAX = registry; stdcall act; RET 4) */
void rx_activity_unlisten_net_positions(rx_addr registry, rx_addr act);    /* 005873f0 (EAX = registry; stdcall act; RET 4) */
void rx_activity_unlisten_net_mode(rx_addr registry, rx_addr act);         /* 00587450 (EAX = registry; stdcall act; RET 4) */
void rx_activity_unlisten_ghost_reset(rx_addr channel, rx_addr act);       /* 005874b0 (EAX = channel; stdcall act; RET 4) */
void rx_activity_unlisten_vehicle_entered(rx_addr act);                    /* 00587510 (stdcall act; RET 4) */
void rx_activity_unlisten_page_wipe(rx_addr channel, rx_addr act);         /* 00587580 (EAX = channel; stdcall act; RET 4) */
void rx_dataminer_unlisten(rx_addr channel, rx_addr name, rx_addr self, rx_addr method);   /* 00755880 (ECX = channel, EAX = name; stdcall self, method; RET 8) */
void rx_netmgr_unlisten(rx_addr channel, rx_addr name, rx_addr self, rx_addr method);      /* 007074d0 (ECX = channel, EAX = name; stdcall self, method; RET 8) */
void rx_netmgr_activity_unlisten(rx_addr stats, rx_addr act);              /* 006deff0 (ESI = the stats block 0xda6ca0, EAX = activity) */

/* ---- the event system's destruction ---- */
void     rx_channel_registry_remove(rx_addr registry, rx_addr ch);         /* 00593770 (stdcall registry, channel; RET 8) */
rx_addr  rx_channel_map_erase(rx_addr map, rx_addr out, rx_addr cont, rx_addr node);   /* 00593a70 (stdcall map, out, it; RET 0x10) -> out, ECX = the map's proxy */
void     rx_channel_map_erase_subtree(rx_addr map, rx_addr node);          /* 00851f10 (ECX = map; stdcall node; RET 4) */
rx_addr  rx_channel_map_erase_range(rx_addr map, rx_addr out, rx_addr fcont, rx_addr fnode, rx_addr lcont, rx_addr lnode);   /* 00705070 (EAX = map; RET 0x14) -> out */
void     rx_channel_list_clear(rx_addr list);                              /* 00593ef0 (ESI = list) */
rx_addr  rx_channel_sdtor(rx_addr ch, uint32_t flags);                     /* 00593310 EventSystem::Channel::sdtor (thiscall; RET 4) -> ch */
void     rx_channel_dtor(rx_addr ch);                                      /* 00593330 (ECX = channel) */
void     rx_base_channel_dtor(rx_addr ch);                                 /* 00593120 (ECX = channel) */
rx_addr  rx_event_sdtor(rx_addr ev, uint32_t flags);                       /* 00592c20 EventSystem::Event::sdtor (thiscall; RET 4) -> ev */
void     rx_event_dtor(rx_addr ev);                                        /* 00592c40 (ECX = event) */

/* ---- the activity area's release (Activity::vf6) ---- */
void     rx_scene_subscene_release(rx_addr rec, rx_addr scene);            /* 007d1470 (stdcall record, scene; RET 8) */
void     rx_scene_subscene_unload(rx_addr scene, rx_addr name);            /* 007d19f0 (stdcall scene, name; RET 8): the named branch */
void     rx_world_racing_line_release(rx_addr w);                          /* 00767850 (stdcall world; RET 4) */
void     rx_activity_vf6(rx_addr act, uint32_t flag);                      /* Activity::vf6 00557a10 (thiscall; RET 4) */

/* ---- the activity ---- */
void     rx_activity_vf3(rx_addr act);                                     /* 00557790 Activity::vf3 (thiscall) */

/* ---- the activity audio logic (build_activity_audio_teardown.c) ---- */
void rx_audio_unlisten_state(rx_addr channel, rx_addr name, rx_addr self, rx_addr method);    /* 0058b010 (ECX = channel, EAX = name; RET 8) */
void rx_audio_unlisten_stunt(rx_addr name, rx_addr self, rx_addr method);                     /* 0058ad70 (ECX = name; RET 8) */
void rx_audio_unlisten_physics_stunt(rx_addr name, rx_addr self, rx_addr method);             /* 0058add0 (ECX = name; RET 8) */
void rx_audio_unlisten_wreck(rx_addr channel, rx_addr name, rx_addr self, rx_addr method);    /* 0058aef0 (ECX = channel, EAX = name; RET 8) */
void rx_audio_unlisten_driver(rx_addr channel, rx_addr name, rx_addr self, rx_addr method);   /* 0058b850 (ECX = channel, EAX = name; RET 8) */
void rx_audio_unlisten_restart(rx_addr channel, rx_addr self);            /* 0058b370 (EAX = channel; stdcall self; RET 4) */
void rx_audio_unlisten_best_lap(rx_addr channel, rx_addr self);           /* 0058b8b0 (EAX = channel; stdcall self; RET 4) */
void rx_audio_unlisten_lapped(rx_addr channel, rx_addr self);             /* 0058b910 (EAX = channel; stdcall self; RET 4) */
void rx_audio_unlisten_player_wins(rx_addr channel, rx_addr self);        /* 0058b970 (EAX = channel; stdcall self; RET 4) */
void rx_audio_unlisten_eliminated(rx_addr channel, rx_addr self);         /* 0058b9d0 (EAX = channel; stdcall self; RET 4) */
void rx_audio_unlisten_timed_section(rx_addr channel, rx_addr self);      /* 0058ba30 (EAX = channel; stdcall self; RET 4) */
void rx_audio_unlisten_states(rx_addr l);                                 /* 00581e20 (ESI = logic) */
void rx_audio_unlisten_wrecks(rx_addr l);                                 /* 00581b10 (ESI = logic) */
void rx_audio_unlisten_challenge_award(rx_addr channel, rx_addr self);    /* 0058b130 (EAX = channel; stdcall self; RET 4) */
void rx_freeride_audio_logic_vf4(rx_addr l);                              /* FreeRideActivityAudioLogic::vf4 005823b0 (thiscall) */
void rx_freeride_audio_logic_vf30(rx_addr l);                             /* FreeRideActivityAudioLogic::vf30 00582260 (thiscall) */
void rx_audio_logic_vf30(rx_addr l);                                      /* ActivityAudioLogic::vf30 00580f80 (thiscall) */
void rx_race_audio_logic_vf30(rx_addr l);                                 /* RaceActivityAudioLogic::vf30 005825f0 (thiscall) */
void rx_race_audio_logic_vf4(rx_addr l);                                  /* RaceActivityAudioLogic::vf4 005829b0 (thiscall) */
#ifdef __cplusplus
}
#endif
