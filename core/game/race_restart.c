/* race_restart.c -- App.RestartActivity: the restart of the running event's activities.
 *
 * PostEventMenu's and PostChallengePopup's RESTART end their page and then call App.RestartActivity (Lua binding 0072e570), which
 * is MXGame_RestartActivity 00759d00:
 *   - the world that is the event's (the engine's scene 0x20 active -> MXGame +0x5a54, else scene 0x1c active -> +0x20c8) has its
 *     activity manager at +0x21da0 ({+4 the activity array, +8 their count});
 *   - 0055f670 calls the restart virtual (vf16, vtable +0x40) of every activity with drivers (+0x26c > 0): for a challenge
 *     0057afe0 (each driver's player back to the start zone, then Activity::vf16 00557b40);
 *   - 00757660 (the MXGame's viewports, with the player count it kept in +0x15b28): render targets only, a PRODUCT BOUNDARY as at
 *     its other call sites (build_activity_player.c, anim_scenario_intro.c): the oracle runs the original, the product builds the
 *     viewport list only when the count changed (rx_scene_viewports_create returns at once otherwise);
 *   - MXGame_ResumeFromUI 0075c8c0 with hide = 1: the scenes active, the worlds unpaused (the UI manager's part is the UI
 *     boundary of rx_game_resume_from_ui).
 * Every function is a hand transliteration of the disassembly; frames and call depths are the original's. */
#include "race_restart.h"
#include "build_activity_util.h"
#include "scene_fx_entities.h"

/* ActivityAudioLogic::vf33 005810e0 (ECX = logic; plain ret; shared by 13 audio logics): the logic's sounds stopped and the audio thread
 * flushed. Activity::vf16 (the restart) calls it through the activity's audio logic (+0xfd8, vtable +0x84). The original:
 *   - the owner name (vf34, +0x88) and, when the localized sound list exists ([0x00a957d4]), LocSoundList_StopOwner 0080c880;
 *   - an audio-thread round trip: a manual-reset event (CreateEventA), Audio_PostCommand 1 carrying its address (or SetEvent when
 *     the audio system is shutting down, [0x009b7430]) and WaitForSingleObject(event, INFINITE), CloseHandle.
 * PRODUCT BOUNDARY (audio, as rx_audio_logic_vf15's and rx_mxworld_load_base's): it writes only the localized sound list's records
 * (playing sounds; nothing the game's logic reads) and a private event handle; the product has no audio thread to flush (the host
 * plays the sounds the core reports), so the product asks the owner name and does no more. The oracle runs the original: its wait
 * switches the emulated audio thread, which a port cannot do. */
void rx_audio_logic_vf33(rx_addr logic) {
#ifdef RX_ORACLE
    (void)rx_call_original(0x005810e0, RX_THISCALL, logic, 0, NULL);
#else
    ACT_FRAME_EBP(L, 8, 0, 2);
    (void)act_vcall(logic, 0x88, 0, NULL);   /* vf34: the owner name */
    ACT_END(L);
#endif
}
RX_FUNCTION(0x005810e0, rx_audio_logic_vf33)

/* 0055f670 (EDI = the activity manager; plain ret): vf16 (+0x40) of each activity with drivers. Frame: push esi. */
void rx_activities_restart(rx_addr manager) {
    ACT_FRAME(L, 0, 1);
    for (int32_t i = 0; i < I32(manager, 8); i++) {
        const rx_addr activity = U32(U32(manager, 4), 4u * (uint32_t)i);
        if (I32(activity, 0x26c) > 0) (void)act_vcall(activity, 0x40, 0, NULL);
    }
    ACT_END(L);
}
RX_FUNCTION(0x0055f670, rx_activities_restart)

/* 00759d00 MXGame_RestartActivity (plain ret). Frame: push ecx; push esi; push edi (ecx is the unused local slot). */
void rx_game_restart_activity(void) {
    ACT_FRAME(L, 0, 3);
    const rx_addr core = U32(0, 0x00a95844);
    const rx_addr game = U32(0, 0x00a946b8);
    rx_addr world = 0;
    if (U32(U32(core, 0x20), 0x34) == 1) world = U32(game, 0x5a54);
    else if (U32(U32(core, 0x1c), 0x34) == 1) world = U32(game, 0x20c8);
    ACT_CALL(0, rx_activities_restart(world + 0x21da0));              /* 0055f670 (EDI = the manager) */
    {
        const uint32_t viewport_args[2] = {game, U32(game, 0x15b28)};
#ifdef RX_ORACLE
        ACT_CALL(2, (void)rx_call_original(0x00757660, RX_STDCALL, 0, 2, viewport_args));
#else
        (void)viewport_args;
        rx_scene_viewports_create(game, U32(game, 0x15b28));          /* PRODUCT BOUNDARY (above) */
#endif
    }
    ACT_CALL(1, (void)rx_game_resume_from_ui(game, 1));
    ACT_END(L);
}
RX_FUNCTION(0x00759d00, rx_game_restart_activity)
