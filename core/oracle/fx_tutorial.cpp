// fx_tutorial.cpp -- the adapters of the MotoSkills TutorialManager (core/game/fx_tutorial.c, fx_tutorial_manager.c; group rx_tutorial). The boot
// that reaches them: the tutorial boot (REFLEX_EVENT_SPEC "Tutorial,...": native/drafts/world_runtime/tutorial.cpp start) with the lesson driver
// REFLEX_TUT_SCRIPT (tutorial.h script_frame: the game's own pages driven by keys / Lua). ShowLessonObjects keeps its group rx_fx too.
#include "oracle.h"
extern "C" {
#include "../game/fx_tutorial.h"
#include "../game/build_ui_world.h"
#include "../game/build_xml.h"
}
using namespace reflex_port;
using rx_oracle::f32;
#define G "rx_tutorial"

// 00751c40 (stdcall manager, lesson; RET 8: the adapter returned with no cleanup before w-tutorials)
RX_PORT(0x00751c40, TutorialShowLessonObjects, "rx_fx", 0) { rx_tutorial_show_lesson_objects(m.arg(0), (int32_t)m.arg(1)); return m.ret(8); }

RX_PORT(0x0074fcd0, TutorialLessonOfName, G, OUT_EAX) { const int32_t r = rx_tutorial_lesson_of_name(m.arg(0)); return m.ret(0, (uint32_t)r); }
RX_PORT(0x0074fd10, TutorialConstruct, G, OUT_EAX) { const uint32_t r = rx_tutorial_construct(m.r[6]); return m.ret(0, r); }
RX_PORT(0x0074fea0, TutorialEnableLesson, G, 0) { rx_tutorial_enable_lesson((int32_t)m.arg(0), m.r[6]); return m.ret(4); }
RX_PORT(0x007500c0, TutorialUpdate, G, 0) { rx_tutorial_update(m.arg(0), f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x00750a70, TutorialLandingFailed, G, OUT_EAX) { const uint32_t r = rx_tutorial_landing_failed(m.arg(0)); return m.ret(4, r); }
RX_PORT(0x00750b30, TutorialCheckWreck, G, 0) { rx_tutorial_check_wreck(f32(m.arg(0)), m.r[6]); return m.ret(4); }
RX_PORT(0x00750c00, TutorialCheckHoldTight, G, 0) { rx_tutorial_check_hold_tight(m.arg(0), f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x00750d00, TutorialCheckLeanIn, G, 0) { rx_tutorial_check_lean_in(m.arg(0), f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x00750e60, TutorialCheckTakeOff, G, 0) { rx_tutorial_check_take_off_and_landing(f32(m.arg(0)), m.r[6]); return m.ret(4); }
RX_PORT(0x007510c0, TutorialCheckWreckAvoidance, G, 0) { rx_tutorial_check_wreck_avoidance(f32(m.arg(0)), m.r[6]); return m.ret(4); }
RX_PORT(0x00751320, TutorialCheckFreestyle, G, 0) { rx_tutorial_check_freestyle_tricks(f32(m.arg(0)), m.r[6]); return m.ret(4); }
RX_PORT(0x007515b0, TutorialCheckRiderReflex, G, 0) { rx_tutorial_check_rider_reflex(m.arg(0)); return m.ret(4); }
RX_PORT(0x00751740, TutorialResetLesson, G, 0) { rx_tutorial_reset_lesson(m.arg(0), m.r[0]); return m.ret(4); }
RX_PORT(0x007517e0, TutorialStartLesson, G, 0) { rx_tutorial_start_lesson(m.arg(0)); return m.ret(4); }
RX_PORT(0x007520a0, TutorialOutcome, G, 0) { rx_tutorial_outcome(m.arg(0)); return m.ret(4); }
RX_PORT(0x00752630, TutorialOnEnable, G, 0) { rx_tutorial_on_enable(m.r[1], (int32_t)m.arg(0)); return m.ret(4); }
RX_PORT(0x00752650, TutorialOnDisable, G, 0) { rx_tutorial_on_disable(m.r[1]); return m.ret(0); }
RX_PORT(0x00752680, TutorialOnValidPlayerCount, G, 0) { rx_tutorial_on_valid_player_count(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x007526c0, TutorialOnValidVehicle, G, 0) { rx_tutorial_on_valid_vehicle(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00752700, TutorialOnInUIWorld, G, 0) { rx_tutorial_on_in_ui_world(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x007523f0, TutorialOnVehicleEntered, G, 0) { rx_tutorial_on_vehicle_entered(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00752500, TutorialOnVehicleExited, G, 0) { rx_tutorial_on_vehicle_exited(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00752740, TutorialOnRetry, G, 0) { rx_tutorial_on_retry(m.r[1]); return m.ret(0); }
RX_PORT(0x007527c0, TutorialPlayLessonVoice, G, 0) { rx_tutorial_play_lesson_voice((int32_t)m.arg(0), (int32_t)m.arg(1)); return m.ret(0); }
RX_PORT(0x00752a40, TutorialReadAudioSettings, G, 0) { rx_tutorial_read_audio_settings(); return m.ret(0); }
RX_PORT(0x00752b10, TutorialLoadAudio, G, 0) { rx_tutorial_load_audio(); return m.ret(0); }
RX_PORT(0x00752b60, TutorialUnloadAudio, G, 0) { rx_tutorial_unload_audio(); return m.ret(0); }
RX_PORT(0x004e8460, TutorialRaiseCompleted, G, 0) { rx_tutorial_raise_completed(m.r[0], (int32_t)m.arg(0)); return m.ret(4); }
RX_PORT(0x007532a0, TutorialProfilesMark, G, 0) { rx_tutorial_profiles_mark(m.r[2]); return m.ret(0); }
RX_PORT(0x007410f0, TutorialAppEnable, G, 0) { rx_tutorial_app_enable((int32_t)m.r[0]); return m.ret(0); }
RX_PORT(0x007424a0, AppPauseActiveWorld, G, 0) { rx_app_pause_active_world(m.r[2]); return m.ret(0); }
RX_PORT(0x00599820, UIEnableTutorialMode, G, 0) { rx_ui_enable_tutorial_mode(m.r[1], m.arg(0)); return m.ret(4); }
// MXUIWorld::vf1 007651d0 (thiscall scene, config, physics world, table; RET 0x10) / vf5 00765670 (thiscall dt; RET 4)
RX_PORT(0x007651d0, UIWorldStart, G, 0) { rx_mxuiworld_start(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x00765670, UIWorldUpdate, G, OUT_EAX) { const uint32_t r = rx_mxuiworld_vf5(m.r[1], f32(m.arg(0))); return m.ret(4, r); }
// XMLReader::vf56 00828630 (thiscall node; RET 4): the node's text (build_xml.c; the voice's TutorialAudioMappings read)
RX_PORT(0x00828630, XmlNodeText, G, OUT_EAX) { const uint32_t r = rx_xml_vf_node_text(m.r[1], m.arg(0)); return m.ret(4, r); }
