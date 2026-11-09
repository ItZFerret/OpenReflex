// anim_scenario_intro.cpp -- oracle adapters for core/game/anim_scenario_intro.c (r: 0 EAX, 1 ECX, 2 EDX, 3 EBX, 4 ESP, 6 ESI, 7 EDI).
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_scenario.h"
}
using namespace reflex_port;

ANIM_PORT(0x00749770, ScenarioAnimEntity_vf22, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_anim_entity_type(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074b060, ScenarioAnims_ClearAttachments, "rx_anim", 0) { rx_scenario_anims_clear_attachments(m.r[0]); return m.ret(0); }
ANIM_PORT(0x0074b080, ScenarioCast_Find, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_cast_find(m.r[7], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074b450, ScenarioAnims_StopActors, "rx_anim", 0) { rx_scenario_anims_stop_actors(m.r[2]); return m.ret(0); }
ANIM_PORT(0x0074b480, ScenarioAnims_ResetCameras, "rx_anim", 0) { rx_scenario_anims_reset_cameras(m.r[0]); return m.ret(0); }
ANIM_PORT(0x0074b4f0, ScenarioCast_StopAll, "rx_anim", 0) { rx_scenario_cast_stop_all(m.r[0]); return m.ret(0); }
ANIM_PORT(0x0074b610, ScenarioCast_ClearStates, "rx_anim", 0) { rx_scenario_cast_clear_states(m.r[0]); return m.ret(0); }
ANIM_PORT(0x00749590, Scenario_SetRidersPaused, "rx_anim", 0) { rx_scenario_set_riders_paused(m.r[7], m.r[2]); return m.ret(0); }
ANIM_PORT(0x0074b0c0, ScenarioAnims_FindActor, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_anims_find_actor(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0074b120, ScenarioAnims_FindCamera, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_anims_find_camera(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0074b680, ScenarioAnims_FindProp, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_anims_find_prop(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0074b000, ScenarioAnims_UpdateAttachments, "rx_anim", 0) { rx_scenario_anims_update_attachments(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074af60, ScenarioAnims_Attach, "rx_anim", 0) { rx_scenario_anims_attach(m.r[0], m.r[2], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074c810, Scenario_PlayOn, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_play_on(m.r[1], m.r[2], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074c7e0, Scenario_NameOf, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_name_of(m.r[1], m.r[2], m.r[7]); return m.ret(0); }
ANIM_PORT(0x0074ab10, ScenarioAnimEntity_Job, "rx_anim", 0) { rx_scenario_anim_entity_job(m.arg(0)); return m.ret(0); }
ANIM_PORT(0x0074aab0, ScenarioAnimEntity_SetAnimation, "rx_anim", 0) { rx_scenario_anim_entity_set_animation(m.r[6], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074a9f0, ScenarioAnimEntity_Play, "rx_anim", 0) { rx_scenario_anim_entity_play(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074d080, FreerideIntroScenario_vf3, "rx_anim", OUT_EAX) { m.r[0] = rx_freeride_intro_update(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074d460, FreerideIntroScenario_vf6, "rx_anim", 0) { rx_freeride_intro_end(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00747750, Scenario_LoadIntroScript, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_load_intro_script(m.r[1], m.r[2]); return m.ret(0); }
ANIM_PORT(0x00747720, Scenario_Begin, "rx_anim", 0) { rx_scenario_begin(m.r[6], m.r[7]); return m.ret(0); }
ANIM_PORT(0x0074c830, ScenarioEvent_Read, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_event_read(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
ANIM_PORT(0x0074cd80, ScenarioEvent_Parse, "rx_anim", OUT_EAX) {
    m.r[0] = rx_scenario_event_parse(m.r[6], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc);
}
ANIM_PORT(0x0074ce40, ScenarioScript_Load, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_cast_load_script(m.r[0], m.r[7]); return m.ret(0); }
ANIM_PORT(0x0074cf70, ScenarioScript_Run, "rx_anim", 0) { rx_scenario_script_run(m.r[6], rx_oracle::f32(m.arg(0)), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0074b750, ScenarioAnims_FindPlayerEntity, "rx_anim", OUT_EAX) {
    m.r[0] = rx_scenario_anims_find_player_entity(m.r[0], m.arg(0), m.arg(1)); return m.ret(8);
}
ANIM_PORT(0x0074b970, ScenarioAnims_FindEntity, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_anims_find_entity(m.r[6], m.r[7]); return m.ret(0); }
ANIM_PORT(0x007473f0, IntroScenario_Update, "rx_anim", 0) { rx_scenario_intro_update(m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
ANIM_PORT(0x00747940, Scenario_IntroScriptName, "rx_anim", 0) { rx_scenario_intro_script_name(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
ANIM_PORT(0x0074b200, Scenario_ScreenEffect, "rx_anim", 0) {
    rx_scenario_screen_effect((int32_t)m.r[0], m.arg(0), m.r[4] + 8); return m.ret(0x3c);
}
// 0040b4e0 OverlayColorEntity_StartBlend (ESI / EDX / ECX = the colours, xmm0 / xmm1, stdcall entity, duration; RET 8). Void.
ANIM_PORT(0x0040b4e0, OverlayColorEntity_StartBlend, "rx_anim", 0) {
    rx_overlay_color_start_blend(m.arg(0), rx_oracle::f32(m.arg(1)), m.r[6], m.r[2], m.r[1],
                                 rx_oracle::f32(uint32_t(m.xmm[0].lo)), rx_oracle::f32(uint32_t(m.xmm[1].lo)));
    return m.ret(8);
}
ANIM_PORT(0x0074bd50, ScenarioCast_Release, "rx_anim", 0) { rx_scenario_cast_release(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074be10, ScenarioCast_PlaceEntity, "rx_anim", 0) {
    rx_scenario_cast_place_entity(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10);
}
ANIM_PORT(0x0074d4c0, Rider_SetStartLineAnimation, "rx_anim", 0) { rx_rider_set_start_line_animation(m.r[0], m.r[6]); return m.ret(0); }
ANIM_PORT(0x0074d2b0, FreerideIntroScenario_vf1, "rx_anim", 0) { rx_freeride_intro_begin(m.r[1], m.arg(0)); return m.ret(4); }

// 0080c880 LocSoundList_StopOwner (stdcall owner; RET 4). Void.
ANIM_PORT(0x0080c880, LocSoundList_StopOwner, "rx_anim", 0) { rx_loc_sound_list_stop_owner(m.arg(0)); return m.ret(4); }
