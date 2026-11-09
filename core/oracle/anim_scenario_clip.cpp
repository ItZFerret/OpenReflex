// anim_scenario_clip.cpp -- oracle adapters for core/game/anim_scenario_clip.c.
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_scenario.h"
}
using namespace reflex_port;
static float xmm_f(reflex_vm::Machine& m, int i) { return rx_oracle::f32(uint32_t(m.xmm[i].lo)); }

ANIM_PORT(0x006aa750, ClipRecorder_ClearClips, "rx_anim", 0) { rx_clip_recorder_clear_clips(); return m.ret(0); }
ANIM_PORT(0x006ab530, ClipRecorder_RecordPlaces, "rx_anim", 0) { rx_clip_recorder_record_places(); return m.ret(0); }
ANIM_PORT(0x006abcb0, ClipRecorder_Snapshot, "rx_anim", OUT_EAX) { m.r[0] = rx_clip_recorder_snapshot(); return m.ret(0); }
ANIM_PORT(0x006aa7b0, ClipRecorder_StartRecording, "rx_anim", 0) { rx_clip_recorder_start_recording(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x006ab5b0, ClipRecorder_PlaceClips, "rx_anim", 0) { rx_clip_recorder_place_clips(); return m.ret(0); }
ANIM_PORT(0x006abae0, ClipRecorder_ClipAt, "rx_anim", OUT_EAX) { m.r[0] = rx_clip_recorder_clip_at(xmm_f(m, 0)); return m.ret(0); }
ANIM_PORT(0x006abb30, ClipRecorder_OrderClip, "rx_anim", 0) { rx_clip_recorder_order_clip(m.r[2]); return m.ret(0); }
ANIM_PORT(0x006aadc0, ClipRecorder_FindSlot, "rx_anim", OUT_EAX) { m.r[0] = rx_clip_recorder_find_slot(int32_t(m.arg(0))); return m.ret(0); }
ANIM_PORT(0x006ab160, ClipRecorder_CopyRecord, "rx_anim", 0) { rx_clip_recorder_copy_record(m.r[1], m.r[0]); return m.ret(0); }
ANIM_PORT(0x006ab110, ClipRecorder_CopyEvent, "rx_anim", OUT_EAX) { m.r[0] = rx_clip_recorder_copy_event(m.r[0], m.r[7]); return m.ret(0); }
ANIM_PORT(0x006ac050, ClipRecorder_CopySamples, "rx_anim", 0) { rx_clip_recorder_copy_samples(int32_t(m.r[2]), m.arg(0), m.arg(1)); return m.ret(0); }
ANIM_PORT(0x006aaf20, ClipRecorder_SaveClip, "rx_anim", 0) { rx_clip_recorder_save_clip(m.arg(0), m.arg(1)); return m.ret(0); }
ANIM_PORT(0x006ab220, ClipRecorder_CommitCandidate, "rx_anim", 0) { rx_clip_recorder_commit_candidate(); return m.ret(0); }
ANIM_PORT(0x006aab70, ClipRecorder_Record, "rx_anim", 0) { rx_clip_recorder_record(rx_oracle::f32(m.arg(0))); return m.ret(0); }
ANIM_PORT(0x006ab560, ClipRecorder_FillerClip, "rx_anim", 0) { rx_clip_recorder_filler_clip(); return m.ret(0); }
ANIM_PORT(0x006ab1c0, ClipRecorder_RiderReset, "rx_anim", 0) { rx_clip_recorder_rider_reset(m.r[0]); return m.ret(0); }
ANIM_PORT(0x006ab800, ClipRecorder_LogReset, "rx_anim", 0) { rx_clip_recorder_log_reset(m.r[0], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x006aba80, ClipRecorder_LogEffect, "rx_anim", 0) { rx_clip_recorder_log_effect(m.r[2]); return m.ret(0); }
