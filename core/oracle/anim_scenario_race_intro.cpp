// anim_scenario_race_intro.cpp -- oracle adapters for core/game/anim_scenario_race_intro.c (r: 0 EAX, 1 ECX, 2 EDX, 3 EBX,
// 4 ESP, 6 ESI, 7 EDI). The scenario entry points are reached by the baton boot / the event start, not by the reference race
// worlds (the intro runs before the race); see plan/QUEUE.md "chain round 3" for the Verify counts.
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_scenario_race_intro.h"
}
using namespace reflex_port;

ANIM_PORT(0x00747810, Scenario_Start, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_start(m.r[6]); return m.ret(0); }
ANIM_PORT(0x00749d00, RaceIntroScenario_vf0, "rx_anim", 0) { rx_race_intro_vf0(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00749df0, RaceIntroScenario_vf1, "rx_anim", 0) { rx_race_intro_vf1(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x007497b0, RaceIntroScenario_vf3, "rx_anim", OUT_EAX) { m.r[0] = rx_race_intro_vf3(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00749780, RaceIntroScenario_vf4, "rx_anim", OUT_EAX) { m.r[0] = rx_race_intro_vf4(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00749fb0, RaceIntroScenario_vf5, "rx_anim", OUT_EAX) { m.r[0] = rx_race_intro_vf5(m.r[1]); return m.ret(0); }
// FreerideIntroScenario (vtable 0x00944014, MXActivityFreeRide +0xff200; w-frscene): vf0 / vf4 / vf5 (vf1 / vf3 / vf6: anim_scenario_intro.cpp)
ANIM_PORT(0x0074d170, FreerideIntroScenario_vf0, "rx_anim", 0) { rx_freeride_intro_vf0(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074d060, FreerideIntroScenario_vf4, "rx_anim", OUT_EAX) { m.r[0] = rx_freeride_intro_advance(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074d440, FreerideIntroScenario_vf5, "rx_anim", OUT_EAX) { m.r[0] = rx_freeride_intro_can_skip(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074eaf0, RaceIntroScenario_vf6, "rx_anim", 0) { rx_intro_scenario_vf6(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00749fd0, RaceIntroScenario_vf13, "rx_anim", OUT_EAX) { m.r[0] = rx_race_intro_vf13(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0074dd20, RaceIntroScenario_vf12, "rx_anim", 0) { rx_intro_scenario_vf12(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0055eb20, BaseScenario_vf9, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_step_is_7(m.r[1]); return m.ret(0); }
ANIM_PORT(0x0055eb30, BaseScenario_vf10, "rx_anim", OUT_EAX) { m.r[0] = rx_scenario_step_is_b(m.r[1]); return m.ret(0); }
ANIM_PORT(0x00749600, Scenario_CardMask, "rx_anim", 0) { rx_race_intro_card_mask(m.r[0], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x007496d0, Scenario_CardFlags, "rx_anim", 0) { rx_race_intro_card_flags(m.r[0], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x007498a0, Scenario_ChiefClip, "rx_anim", OUT_EAX) { m.r[0] = rx_race_intro_chief_clip(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00749ba0, Scenario_RiderClip, "rx_anim", OUT_EAX) { m.r[0] = rx_race_intro_rider_clip(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00749910, Scenario_PlaceChiefs, "rx_anim", 0) { rx_race_intro_place_chiefs(m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x00749b10, Scenario_PlaceRiders, "rx_anim", 0) { rx_race_intro_place_riders(m.r[0], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074b5a0, ScenarioCast_Pause, "rx_anim", 0) { rx_scenario_cast_pause(m.r[0]); return m.ret(0); }
// XMLReader::vf69 00829460 (attribute matrices): the reader unit (game/build_xml.c) has no adapter family of its own; the intro
// scenario's script parse reaches this slot in the baton boot, so it is verified here.
extern "C" {
#include "../game/build_xml.h"
}
ANIM_PORT(0x00829460, XMLReader_vf69, "rx_anim", OUT_EAX) { m.r[0] = rx_xml_vf_attr_mat4(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// The scenario script's event handlers (table 0x9c5120): ECX = the activity, stack: event, flag; RET 8.
ANIM_PORT(0x0055d290, ScenarioEvent_Camera, "rx_anim", 0) { rx_scenario_event_camera(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0055d300, ScenarioEvent_Origin, "rx_anim", 0) { rx_scenario_event_origin(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0055ddc0, ScenarioEvent_Disable, "rx_anim", 0) { rx_scenario_event_disable(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0055e0e0, ScenarioEvent_Timeline, "rx_anim", 0) { rx_scenario_event_timeline(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 0063e7c0 (EDI = the camera path, stack: name; RET 4) / 0063d780 (stack: camera, name; RET 8)
ANIM_PORT(0x0063e7c0, CameraPath_SetClip, "rx_anim", 0) { rx_camera_path_set_clip(m.r[7], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0063d780, ScenarioCamera_Setup, "rx_anim", 0) { rx_scenario_camera_setup(m.arg(0), m.arg(1)); return m.ret(8); }
// The intro's FX / crowd handlers.
ANIM_PORT(0x0055d7b0, ScenarioEvent_Crowd, "rx_anim", 0) { rx_scenario_event_crowd(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0055d7e0, ScenarioEvent_Particle, "rx_anim", 0) { rx_scenario_event_particle(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x0055d950, ScenarioEvent_Firework, "rx_anim", 0) { rx_scenario_event_firework(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
ANIM_PORT(0x006744f0, World_CrowdEventName, "rx_anim", OUT_EAX) { m.r[0] = rx_world_crowd_event_name(m.arg(0)); return m.ret(0); }
// 006924a0 (EAX = the effect, xmm0 = the delay)
ANIM_PORT(0x006924a0, Fx_TriggeredEffectStart, "rx_anim", 0) {
    rx_fx_triggered_effect_start(rx_oracle::f32(uint32_t(m.xmm[0].lo)), m.r[0]); return m.ret(0);
}
// 005798f0 MXActivityRaceStub::vf31 (ECX = the activity, arg: the old state; RET 4)
extern "C" {
#include "../game/race_activity_stub.h"
}
ANIM_PORT(0x005798f0, RaceStub_vf31, "rx_anim", 0) { rx_race_stub_vf31(m.r[1]); return m.ret(4); }
// 00568340 MXActivityRaceStub::vf38 (ECX = the activity, stack the old state; RET 4) / 005695b0 (ECX = the activity, stack the
// activity; RET 4)
ANIM_PORT(0x00568340, RaceStub_vf38, "rx_anim", 0) { rx_race_stub_vf38(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x005695b0, RaceStub_ResetVehicleMaps, "rx_anim", 0) { rx_race_stub_reset_vehicle_maps(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x00565040, RaceActivity_vf38, "rx_anim", 0) { rx_race_vf38(m.r[1], m.arg(0)); return m.ret(4); }
