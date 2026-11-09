// race_wave.cpp -- oracle adapters for the small race / activity / audio-logic functions ported in the 2026-10-26 wave (agent
// "w-race"): core/game/race_activity*.c, race_misc_game.c, race_misc_pose.c, race_driver.c, race_sound.c. ABI glue only.
#include "race_oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../game/race.h"
#include "../game/race_activity.h"
#include "../game/race_misc.h"
#include "../game/build_activity.h"
#include "../game/race_activity_ui.h"
#include "../game/race_net_session.h"
#include "../game/build_tables.h"
}
using namespace reflex_port;

static void push_f80(reflex_vm::Machine& m, const rx_f80& r) {
    reflex_f80::F80 v; v.kind = r.kind; v.sign = r.sign; v.mant = r.mant; v.exp = r.exp;
    m.fpu.push_back(v);
}

// ---- race_activity_audio.c / race_activity.c
// 00585a40 (EDI = the std::set<int>; out iterator, key*; RET 8) -> EAX = out
RX_PORT(0x00585a40, SetInt_Find, "rx_race", OUT_EAX) { m.r[0] = rx_set_int_find(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
// 00559da0 (ECX = activity) -> EAX
RX_PORT(0x00559da0, Activity_HasLocalHuman, "rx_race", OUT_EAX) { m.r[0] = rx_activity_has_local_human(m.r[1]); return m.ret(0); }
// 005b9f50 UISettings_Get (ECX = settings, EAX = index, ESI = device) -> EAX
RX_PORT(0x005b9f50, UiSettings_Value, "rx_race", OUT_EAX) { m.r[0] = rx_ui_settings_value(m.r[1], m.r[0], m.r[6]); return m.ret(0); }
// 004fead0 (count; RET 4) -> EAX / 004f3320 (storage; RET 4) / 004faae0 (EAX = last, EDI = first, ECX = dest end) -> EAX
RX_PORT(0x004fead0, Vector_Alloc, "rx_race", OUT_EAX) { m.r[0] = rx_vector_alloc(m.arg(0)); return m.ret(4); }
RX_PORT(0x004faae0, Vector_CopyBackward, "rx_race", OUT_EAX) { m.r[0] = rx_vector_copy_backward(m.r[0], m.r[7], m.r[1]); return m.ret(0); }

// ---- race_misc_game.c
// 007556d0 (manager, a, b, c; RET 0x10)
RX_PORT(0x007556d0, PostMotocardData, "rx_race", 0) { rx_post_motocard_data(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }

// ---- build_activity_game.c (group rx_build8)
// 00756700 (EDX = the level config) -> st(0)
RX_PORT(0x00756700, LevelLoadProgress, "rx_build8", OUT_FPU) { push_f80(m, rx_level_load_progress(m.r[2])); return m.ret(0); }

// ---- race_activity_ui.c
// 005dc0c0 UIProperty_GetInt (EAX = name; ui, &out; RET 8) -> EAX
RX_PORT(0x005dc0c0, UiProperty_GetInt, "rx_race", OUT_EAX) { m.r[0] = rx_ui_property_get_int(m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
// 005eab30 (ECX = the map; &key, &out; RET 8) -> EAX / 005c3440 (ECX = stored, EAX = key) -> EAX
RX_PORT(0x005eab30, UiPropertyMap_Find, "rx_race", OUT_EAX) { m.r[0] = rx_ui_property_map_find(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005c3440, UiKey_Equal, "rx_race", OUT_EAX) { m.r[0] = rx_ui_key_equal(m.r[1], m.r[0]); return m.ret(0); }
// 005e0270 (EAX = id, EDI = the holder) -> EAX; 005e1190 (cdecl string) -> EAX; 005c3340 (ECX) -> EAX; 005c3370 (ECX; key) -> EAX; 005ee560 (ECX = map; &key, &out; RET 8) -> EAX
RX_PORT(0x005e0270, UiLanguage_Lookup, "rx_race", OUT_EAX) { m.r[0] = rx_ui_language_lookup(m.r[0], m.r[7]); return m.ret(0); }
RX_PORT(0x005e1190, Djb2_Hash, "rx_race", OUT_EAX) { m.r[0] = rx_djb2_hash(m.arg(0)); return m.ret(0); }
RX_PORT(0x005c3340, IntHash_Signed, "rx_race", OUT_EAX) { m.r[0] = rx_int_hash_signed(m.r[1]); return m.ret(0); }
RX_PORT(0x005c3370, IntKey_Equal, "rx_race", OUT_EAX) { m.r[0] = rx_int_key_equal(m.r[1], m.arg(0)); return m.ret(0); }
RX_PORT(0x005ee560, IntMap_Find, "rx_race", OUT_EAX) { m.r[0] = rx_int_map_find(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 0054d590 (player, index; RET 8)
RX_PORT(0x0054d590, FreeRide_CollectableFound, "rx_race", 0) { rx_free_ride_collectable_found(m.arg(0), m.arg(1)); return m.ret(8); }

// ---- race_sound.c
// 0080b490 AudioEvent_ApplyVolume (EAX = ESI = the event) / 0080a8e0 (ESI = event) / 0080bde0 (cdecl event) / 0080dcd0 (ECX = &event)
RX_PORT(0x0080b490, AudioEvent_ApplyVolume, "rx_race", 0) { rx_audio_event_apply_volume(m.r[0]); return m.ret(0); }
RX_PORT(0x0080a8e0, AudioEvent_InvalidHandle, "rx_race", 0) { rx_audio_event_invalid_handle(m.r[6]); return m.ret(0); }
RX_PORT(0x0080bde0, AudioEvent_ResetFlags, "rx_race", 0) { rx_audio_event_reset_flags(m.arg(0)); return m.ret(0); }
RX_PORT(0x0080dcd0, AudioEvent_UnlistEntry, "rx_race", 0) { rx_audio_event_unlist_entry(m.r[1]); return m.ret(0); }

// ---- race_net_session.c (the online session code; Quazal leaves run as the original)
RX_PORT(0x00700a70, Net_SnapshotsStore, "rx_race", 0) { rx_net_snapshots_store(m.r[0]); return m.ret(0); }
RX_PORT(0x00700e70, Net_SnapshotLoad, "rx_race", OUT_EAX) { m.r[0] = rx_net_snapshot_load(m.r[6]); return m.ret(0); }
RX_PORT(0x006fe870, Net_PlayerApply, "rx_race", 0) { rx_net_player_apply(m.r[7]); return m.ret(0); }
RX_PORT(0x006fe9c0, Net_PlayerSetFlag, "rx_race", 0) { rx_net_player_set_flag(m.r[0], m.r[1] & 0xff); return m.ret(0); }
RX_PORT(0x006ff8d0, Net_SendEvent, "rx_race", 0) { rx_net_send_event(m.r[7], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0088bcd0, Quazal_HandleInit, "rx_race", OUT_EAX) { m.r[0] = rx_quazal_handle_init(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0088bac0, Quazal_HandleRelease, "rx_race", 0) { rx_quazal_handle_release(m.r[1]); return m.ret(0); }
RX_PORT(0x00885050, Quazal_RefReady, "rx_race", OUT_EAX) { m.r[0] = rx_quazal_ref_ready(m.r[1]); return m.ret(0); }
RX_PORT(0x006f7810, Net_SessionRecordState, "rx_race", 0) { rx_net_session_record_state(m.arg(0) & 0xff); return m.ret(0); }
RX_PORT(0x005544c0, Net_SelectionBeginAll, "rx_race", OUT_EAX) { m.r[0] = rx_net_selection_begin_all(m.r[6]); return m.ret(0); }
RX_PORT(0x00585b20, Net_SelectionBeginReady, "rx_race", OUT_EAX) { m.r[0] = rx_net_selection_begin_ready(m.r[6]); return m.ret(0); }
RX_PORT(0x00508880, Net_SelectionNext, "rx_race", 0) { rx_net_selection_next(m.r[1]); return m.ret(0); }
// 00508800 (ECX = the iterator; plain ret), 00508770 (ECX = the iterator; stdcall out, key; RET 8), 0053a170 (ECX = a std::set<uint>; stdcall out iterator, &key; RET 8)
RX_PORT(0x00508800, Net_SelectionAdvance, "rx_race", 0) { rx_net_selection_advance(m.r[1]); return m.ret(0); }
RX_PORT(0x00508770, Net_SelectionSuccessor, "rx_race", 0) { rx_net_selection_successor(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0053a170, Net_SetLowerBound, "rx_race", 0) { rx_net_set_lower_bound(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }

// ---- race_misc_damage.c
// 00479970 (ESI = entry; dt; RET 4) / 0047b960 (entry, index; xmm1 = dt; RET 8)
RX_PORT(0x00479970, DamageEntry_Update, "rx_race", 0) { rx_damage_entry_update(m.r[6], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0047b960, DamagePart_Fall, "rx_race", 0) { rx_damage_part_fall(m.arg(0), m.arg(1), rx_oracle::f32(uint32_t(m.xmm[1].lo))); return m.ret(8); }

// ---- build_tables_rdb.c
// 004ebce0 (EDI = the new set; the source; RET 4) -> EAX
RX_PORT(0x004ebce0, RowSet_CopyConstruct, "rx_race", OUT_EAX) { m.r[0] = rx_tbl_rowset_copy_construct(m.r[7], m.arg(0)); return m.ret(4); }
