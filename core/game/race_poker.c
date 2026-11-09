/* race_poker.c -- MXActivityPokerRace (the PokerRace party mode, activity type 0xd; vtable 0x00944b74; 0x13e8e0 bytes) and its
 * PokerRaceDriver (vtable 0x0093fd64, 0x1170 bytes), hand-transliterated from the original (the address of each function is in its
 * comment; frames and the stack depth of each call are the original's).
 *
 * A lap race (MXActivityRaceStub: rx_race_stub_construct 00565830) on a National level's "MiniGamePokerRun" section, with cards. The
 * deck is 52 entries {rank, suit, available} of 12 bytes at +0x13e660 (+0x13e668 the available flag); the community cards ("the
 * flop", +0x13e650 .. +0x13e65c, -1 = none) are drawn at the countdown (vf38: three) and on the third lap (the fourth); each lap the
 * "Lap Completed" handler (00571d60) deals the drivers cards: their two hole cards (+0x1134, +0x1138; a third +0x113c on the fourth lap),
 * and the leading riders (place 0 .. 2) a choice of cards (+0x1140 .. +0x114c: four, three or two) a human picks one of with the
 * card buttons (00513630). The race's end (vf34) keeps the chosen card and evaluates every driver's best five-card hand (005723c0 with
 * 00572360 / 00572720) into +0x1150 .. +0x1164, the standings' order (vf43) for drivers who finished all four laps.
 * Cards are drawn from the game's LCG ([0x9cb860], counted at [0x9cb864]: 00571ca0).
 *
 * ONLINE (a Quazal session: [0x00da6cb0] + 0x1c4c set and the mode +0x1b0 not 1): the NetPokerRace object (+0x13e8d0) carries the
 * community cards, the hands and the choices (the host draws them; the network handlers 00572170 / 005721e0 / 005722b0 apply them on
 * the clients). Each networked branch is marked "ONLINE" and stops with act_unreached; the network handlers are not ported. */
#include "race_poker.h"
#include "race_section.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "race_activity_stub.h"
#include "race_activity_teardown.h"
#include "race_finish.h"
#include "race_driver.h"
#include "race_extern.h"
#include "race.h"
#include "build_activity.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "build_alloc.h"
#include "waypoint_activity.h"
#include "waypoint_in_progress.h"
#include "world_wreck.h"
#include "race_driver_reset.h"
#include "race_challenge_activity.h"
#include "loop_player.h"
#include "activity_cues.h"
#include "crt.h"
#include <string.h>
#include "rider_cap.h"

void rx_waypoint_network_vf14(rx_addr driver);   /* 0050ec30 NetworkDriver::vf14 (waypoint_driver_network.c) */

#define VT_POKER        0x00944b74u
#define VT_POKER_AUDIO  0x0094301cu
#define VT_POKER_DRIVER 0x0093fd64u
#define TD_ACTIVITY     0x009cbd24u   /* void(Activity*) */
#define STR_POKER_FLOP  0x009422d0u   /* "PokerFlop" */
#define STR_CARD_AVAIL  0x009422dcu   /* "PokerCardAvailable" */
#define STR_CARD_PICKED 0x009422f0u   /* "PokerCardSelected" */

static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
static int session_networked(void) {
    const rx_addr s = U32(0, 0x00da6cb0);
    return U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1;
}
/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static float x87_copy(float v) {
    uint32_t b = rx_fb(v);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) b |= 0x400000u;
    return rx_bf(b);
}
#define DECK_FLAG(A, c) ((A) + 0x13e668u + 0xcu * (uint32_t)(c))   /* the card's available flag */

/* ======================================================================================================================== */
/* small helpers                                                                                                             */

/* 00571ca0 (EDI = act) -> EAX: a card drawn from the deck: the game's LCG (0x41c64e6d, 0x3039; [0x9cb860], each draw counted at
 * [0x9cb864]) % 52 (a signed idiv) until an available card comes up (at most 50000 redraws: then -1); the card is taken. */
uint32_t rx_poker_draw_card(rx_addr act) {
    uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    int32_t card = (int32_t)((seed >> 16) & 0x7fff) % 0x34;
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    SU32(0, 0x009cb860, seed);
    for (int32_t tries = 0;; ) {
        if (U8(DECK_FLAG(act, card), 0) != 0) { SU8(DECK_FLAG(act, card), 0, 0); return (uint32_t)card; }
        SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
        seed = seed * 0x41c64e6du + 0x3039u;
        tries++;
        card = (int32_t)((seed >> 16) & 0x7fff) % 0x34;
        SU32(0, 0x009cb860, seed);
        if (tries >= 0xc350) return 0xffffffffu;
    }
}
RX_FUNCTION(0x00571ca0, rx_poker_draw_card)

/* 00557760 (EDX = activity, ESI = driver) -> EAX: the driver's place in the standings (+0x2d0), -1 when it is not listed */
int32_t rx_activity_driver_place(rx_addr act, rx_addr driver) {
    for (int32_t i = 0; i < I32(act, 0x26c); i++) if (rx_stand(act, (uint32_t)i) == driver) return i;
    return -1;
}
RX_FUNCTION(0x00557760, rx_activity_driver_place)

/* 00589b80 (ESI = manager, EDX = name, ECX = driver; stdcall activity; RET 4): a void(Activity*, BaseDriver*) event raised.
 * Frame: sub esp, 0xc; push edi; the payload {activity, driver} at F + 4. */
void rx_event_activity_driver_at(rx_addr manager, rx_addr name, rx_addr driver, rx_addr act) {
    ACT_FRAME(F, 0xc, 1);
    SU32(F, 8, driver);
    SU32(F, 4, act);
    act_raise(manager, name, 0x009cf6d4, F + 4, 8);
    ACT_END(F);
}
RX_FUNCTION(0x00589b80, rx_event_activity_driver_at)

/* 00571c20 (EDI = the level name [world + 0x20030], ESI = the area) -> EAX: the AI track asset kind of the poker race: the first of the
 * kinds 2, 0, 1, 9 whose asset exists (00536e80), else 5 if that exists, else 0. Frame: push ecx / esi. */
uint32_t rx_poker_track_audio_kind(rx_addr level, uint32_t area) {
    ACT_FRAME(F, 0, 2);
    static const uint8_t kinds[4] = {2, 0, 1, 9};
    uint32_t has;
    for (int k = 0; k < 4; k++) {
        ACT_CALLR(has, 1, rx_track_has_asset(kinds[k], level, area));
        if (has != 0) { ACT_END(F); return kinds[k]; }
    }
    ACT_CALLR(has, 1, rx_track_has_asset(5, level, area));
    ACT_END(F);
    return has != 0 ? 5 : 0;
}
RX_FUNCTION(0x00571c20, rx_poker_track_audio_kind)

/* ======================================================================================================================== */
/* construction / destruction                                                                                                */

/* 00584020 (ESI = logic) -> logic: PokerRaceActivityAudioLogic (0xa4 bytes): ActivityAudioLogic -> RaceActivityAudioLogic (+0x98 = 0),
 * the vtable 0x0094301c. Frame: push ebx. */
rx_addr rx_poker_audio_logic_construct(rx_addr logic) {
    ACT_FRAME(F, 0, 1);
    SU32(logic, 0, 0x009429fc);
    SU32(logic, 0x94, 0);
    SU8(logic, 4, 0);
    SU32(logic, 0x50, 0);
    SU32(logic, 0x54, 0);
    SU32(logic, 0x8c, 0);
    SU32(logic, 0x90, 0);
    ACT_CALL(0, rx_audio_logic_vf32(logic));
    SU32(logic, 0, 0x00942cfc);
    ACT_CALL(0, rx_audio_logic_vf32(logic));
    SU32(logic, 0x98, 0);
    SU32(logic, 0, VT_POKER_AUDIO);
    ACT_END(F);
    return logic;
}
RX_FUNCTION(0x00584020, rx_poker_audio_logic_construct)

/* 00582d50 RaceActivityAudioLogic::vf32 (ECX = logic): ActivityAudioLogic::vf32, +0x98 = 0. Frame: push esi. */
void rx_race_audio_logic_vf32(rx_addr logic) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_audio_logic_vf32(logic));
    SU32(logic, 0x98, 0);
    ACT_END(F);
}
RX_FUNCTION(0x00582d50, rx_race_audio_logic_vf32)

/* 00571000 (EDI = the allocated activity; stdcall type, world; RET 8): the activity factory's case 0xd. A new PokerRaceActivityAudioLogic,
 * the race stub (00565830), the community cards none, +0x2c8 = 1, no network object. Frame: push esi. */
rx_addr rx_poker_race_construct(rx_addr act, uint32_t type, rx_addr world) {
    ACT_FRAME(F, 0, 1);
    rx_addr logic;
    ACT_CALLR(logic, 1, rx_reflex_aligned_alloc(0xa4));
    rx_addr audio = 0;
    if (logic != 0) ACT_CALLR(audio, 0, rx_poker_audio_logic_construct(logic));
    ACT_CALL(2, rx_race_stub_construct(act, type, world, audio));
    SU32(act, 0x13e65c, 0xffffffffu);
    SU32(act, 0x13e658, 0xffffffffu);
    SU32(act, 0x13e654, 0xffffffffu);
    SU32(act, 0x13e650, 0xffffffffu);
    SU32(act, 0, VT_POKER);
    SU32(act, 0x2c8, 1);
    SU32(act, 0x13e8d0, 0);
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x00571000, rx_poker_race_construct)

/* 00571070 slot 0 (thiscall; flags; RET 4) -> this: the race stub's destructor (005659e0), freed with flags & 1. Frame: push esi. */
rx_addr rx_poker_race_sdtor(rx_addr act, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    SU32(act, 0, VT_POKER);
    ACT_CALL(0, rx_race_stub_destroy(act));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(act));
    ACT_END(F);
    return act;
}
RX_FUNCTION(0x00571070, rx_poker_race_sdtor)

/* ======================================================================================================================== */
/* the listeners (as race_tag.c's templates)                                                                                 */

/* 00589590 (EAX = the activity's channel; stdcall act; RET 4): "Lap Completed" with 00571d60 (proxy 0x945b44: void(BaseDriver*, int)) */
void rx_poker_lap_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00589590, 1, 1, 2, channel, 0x0093eefc /* "Lap Completed" */, 0x00945b44, 0x009cda64, act, 0x00571d60));
}
RX_FUNCTION(0x00589590, rx_poker_lap_listen)
/* 00589650 (EAX = the session channel): "NetworkObjectRemoved" with 00571d40 (proxy 0x945b64) */
void rx_poker_net_removed_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00589650, 1, 1, 2, channel, 0x0093ee98, 0x00945b64, 0x009cdc7c, act, 0x00571d40));
}
RX_FUNCTION(0x00589650, rx_poker_net_removed_listen)
/* 00589710: "NetPokerCommunityCardsDrawn" with 00572170 (proxy 0x945b84) */
void rx_poker_net_community_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00589710, 1, 1, 2, channel, 0x00942280, 0x00945b84, 0x009d0420, act, 0x00572170));
}
RX_FUNCTION(0x00589710, rx_poker_net_community_listen)
/* 005897d0: "NetPokerHandCardsDrawn" with 005721e0 (proxy 0x945ba4) */
void rx_poker_net_hand_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x005897d0, 1, 1, 2, channel, 0x0094229c, 0x00945ba4, 0x009d0494, act, 0x005721e0));
}
RX_FUNCTION(0x005897d0, rx_poker_net_hand_listen)
/* 00589890: "NetPokerChoiceCardsDrawn" with 005722b0 (proxy 0x945bc4) */
void rx_poker_net_choice_listen(rx_addr channel, rx_addr act) {
    ACT_CALL(1, act_subscribe(0x00589890, 1, 1, 2, channel, 0x009422b4, 0x00945bc4, 0x009d0520, act, 0x005722b0));
}
RX_FUNCTION(0x00589890, rx_poker_net_choice_listen)
/* 00589950 / 005899b0 / 00589a10 / 00589a70 / 00589ad0: their mirrors (frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x14; push esi) */
void rx_poker_lap_unlisten(rx_addr channel, rx_addr act) { act_unlisten(1, channel, 0x0093eefc, 0x00945b44, 0x009cda64, act, 0x00571d60); }
RX_FUNCTION(0x00589950, rx_poker_lap_unlisten)
void rx_poker_net_removed_unlisten(rx_addr channel, rx_addr act) { act_unlisten(1, channel, 0x0093ee98, 0x00945b64, 0x009cdc7c, act, 0x00571d40); }
RX_FUNCTION(0x005899b0, rx_poker_net_removed_unlisten)
void rx_poker_net_community_unlisten(rx_addr channel, rx_addr act) { act_unlisten(1, channel, 0x00942280, 0x00945b84, 0x009d0420, act, 0x00572170); }
RX_FUNCTION(0x00589a10, rx_poker_net_community_unlisten)
void rx_poker_net_hand_unlisten(rx_addr channel, rx_addr act) { act_unlisten(1, channel, 0x0094229c, 0x00945ba4, 0x009d0494, act, 0x005721e0); }
RX_FUNCTION(0x00589a70, rx_poker_net_hand_unlisten)
void rx_poker_net_choice_unlisten(rx_addr channel, rx_addr act) { act_unlisten(1, channel, 0x009422b4, 0x00945bc4, 0x009d0520, act, 0x005722b0); }
RX_FUNCTION(0x00589ad0, rx_poker_net_choice_unlisten)

/* 00571d40 (thiscall; kind, object; RET 8): a removed NetPokerRace object (kind 7) unlinks (ONLINE: raised only by a session) */
void rx_poker_net_object_removed(rx_addr act, uint32_t kind, rx_addr object) {
    (void)object;
    if (kind == 7) SU32(act, 0x13e8d0, 0);
}
RX_FUNCTION(0x00571d40, rx_poker_net_object_removed)

/* ======================================================================================================================== */
/* the activity's virtuals                                                                                                   */

/* 005710a0 slot 2 (thiscall; arg; RET 4): the race stub's start, the deck (rank i / 4 .. 12, suit i % 4, every card available), the
 * "Lap Completed" listener on the activity's channel and the four session listeners, +0xff070 = 4 laps, the host's NetPokerRace object
 * (ONLINE), the start grid (005666d0). Frame: push ebx / edi. */
void rx_poker_race_vf2(rx_addr act, rx_addr arg) {
    ACT_FRAME(F, 0, 2);
    ACT_CALL(1, rx_race_stub_vf2(act, arg));
    rx_addr card = act + 0x13e668;
    for (int32_t rank = 0; rank < 0xd; rank++) {
        for (int32_t suit = 0; suit < 4; suit++) {
            SU32(card, -8, (uint32_t)suit);
            SU32(card, -4, (uint32_t)rank);
            SU8(card, 0, 1);
            card += 0xc;
        }
    }
    ACT_CALL(1, rx_poker_lap_listen(U32(act, 0x290), act));
    session_static();
    ACT_CALL(1, rx_poker_net_removed_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_poker_net_community_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_poker_net_hand_listen(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_poker_net_choice_listen(U32(0, 0x00da6ca0), act));
    SU32(act, 0xff070, 4);
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) != 0) act_unreached(0x00571225, "ONLINE: the host's NetPokerRace object (008879b0)");
    }
    ACT_CALL(1, rx_race_grid_from_gate(act));
    ACT_END(F);
}
RX_FUNCTION(0x005710a0, rx_poker_race_vf2)

/* 00571260 slot 3 (thiscall): the teardown: a networked session's selection loop (ONLINE), every driver of the two lists leaves the
 * activity, the five listeners leave, then a jump to MXActivityRaceStub::vf3. Frame: sub esp, 0x24; push ebx / edi. */
void rx_poker_race_vf3(rx_addr act) {
    ACT_FRAME(F, 0x24, 2);
    session_static();
    if (session_networked()) act_unreached(0x005712a2, "ONLINE: the networked poker race teardown (the selection iterator loop)");
    while (I32(act, 0x26c) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x268), 0)));
    while (I32(act, 0x280) > 0) ACT_CALL(2, rx_activity_on_player_left(act, U32(U32(act, 0x27c), 0)));
    ACT_CALL(1, rx_poker_lap_unlisten(U32(act, 0x290), act));
    session_static();
    ACT_CALL(1, rx_poker_net_removed_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_poker_net_community_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_poker_net_hand_unlisten(U32(0, 0x00da6ca0), act));
    session_static();
    ACT_CALL(1, rx_poker_net_choice_unlisten(U32(0, 0x00da6ca0), act));
    ACT_END(F);
    rx_race_stub_vf3(act);
}
RX_FUNCTION(0x00571260, rx_poker_race_vf3)

/* 005713e0 slot 4 (ECX = activity): the race stub's event types, plus "PokerFlop" (00590c00) and "PokerCardAvailable" /
 * "PokerCardSelected" (00591500). Frame: push esi. */
void rx_poker_race_vf4(rx_addr act) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_race_stub_vf4(act));
    act_event_type(0x00590c00, U32(act, 0x290), STR_POKER_FLOP);
    act_event_type(0x00591500, U32(act, 0x290), STR_CARD_AVAIL);
    act_event_type(0x00591500, U32(act, 0x290), STR_CARD_PICKED);
    ACT_END(F);
}
RX_FUNCTION(0x005713e0, rx_poker_race_vf4)

/* 00571420 slot 18 (ECX = activity): every driver resets (vf3), the community cards none, every card available, then a jump to
 * MXActivityRaceStub::vf18. Frame: push esi / edi. */
void rx_poker_race_vf18(rx_addr act) {
    ACT_FRAME(F, 0, 2);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) (void)act_vcall(U32(U32(act, 0x268), 4u * (uint32_t)i), 0xc, 0, 0);
    SU32(act, 0x13e65c, 0xffffffffu);
    SU32(act, 0x13e658, 0xffffffffu);
    SU32(act, 0x13e654, 0xffffffffu);
    SU32(act, 0x13e650, 0xffffffffu);
    for (uint32_t c = 0; c < 0x34; c++) SU8(DECK_FLAG(act, c), 0, 1);
    ACT_END(F);
    rx_race_stub_vf18(act);
}
RX_FUNCTION(0x00571420, rx_poker_race_vf18)

/* 005715e0 slot 23 (thiscall; dt; RET 4): MXActivityRaceStub::vf23 with dt through x87 */
void rx_poker_race_vf23(rx_addr act, float dt) {
    ACT_CALL(1, rx_race_stub_update(act, x87_copy(dt)));
}
RX_FUNCTION(0x005715e0, rx_poker_race_vf23)

/* 00571980 slot 27 (thiscall; dt; RET 4): WindDown: every local human driver's pending card choice (00513630), then Activity::vf27.
 * Frame: push ecx (the cookie) / ebx / edi (+ push esi in the loop). */
void rx_poker_race_vf27(rx_addr act, float dt) {
    ACT_FRAME(F, 0, 3);
    if (I32(act, 0x26c) > 0) {
        rx_set_sp(F - 4);
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
            const rx_addr p = U32(d, 0x74);
            if (U32(p, 4) != 0 && U32(p, 0x10) == 0) ACT_CALL(0, rx_poker_driver_pick(d));
        }
        rx_set_sp(F);
    }
    ACT_CALL(1, rx_activity_wind_down(act, x87_copy(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x00571980, rx_poker_race_vf27)

/* 00571ac0 slot 29 (thiscall; dt; RET 4): Post: at (or below) 0 of the Post clock +0xff064 the results are shown when the class allows
 * it (vf57, slot 0xe4: 00559a40); the clock grows by dt; MXActivityRace_UpdatePost (00569a90, EAX = activity; dt through x87).
 * Frame: push ecx (the cookie) / esi. */
void rx_poker_race_vf29(rx_addr act, float dt) {
    ACT_FRAME(F, 0, 2);
    if (F32(act, 0xff064) <= 0.0f) {   /* comiss 0, +0xff064; jb (NaN skips) */
        if (act_vcall(act, 0xe4, 0, 0) != 0) ACT_CALL(0, rx_waypoint_show_results(act));
    }
    SF32(act, 0xff064, dt + F32(act, 0xff064));
    ACT_CALL(1, rx_race_update_post(act, x87_copy(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x00571ac0, rx_poker_race_vf29)

/* 005719f0 slot 34 (thiscall; unused; RET 4): the race's end: each driver's chosen card (+0x1140, when it has a choice) becomes its
 * third card (+0x113c), the cards it did not choose go back to the deck; each driver's hand evaluated (005723c0); the standings sorted
 * (00559eb0); a waiting clip recorder takes the recorded activity's clock. Frame: push ebx / ebp / esi (+ push edi in the loop). */
void rx_poker_race_vf34(rx_addr act, uint32_t unused) {
    (void)unused;
    ACT_FRAME(F, 0, 3);
    if (I32(act, 0x26c) > 0) {
        rx_set_sp(F - 4);
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
            if (U32(d, 0x1140) != 0xffffffffu) {
                SU32(d, 0x113c, U32(d, 0x1140));
                for (uint32_t k = 0x1144; k <= 0x114c; k += 4) {
                    if (U32(d, k) == 0xffffffffu) continue;
                    SU8(DECK_FLAG(act, U32(d, k)), 0, 1);
                    SU32(d, k, 0xffffffffu);
                }
            }
            ACT_CALL(2, rx_poker_evaluate_driver(act, d));
        }
        rx_set_sp(F);
    }
    ACT_CALL(0, rx_activity_sort_standings(act));
    if (U32(0, 0x00a942b4) == 1) {
        const rx_addr rec = U32(0, 0x00a942b8);
        SU32(0, 0x00a942b4, 0);
        SU32(0, 0x00a942a4, U32(rec, 0x294));
    }
    ACT_END(F);
}
RX_FUNCTION(0x005719f0, rx_poker_race_vf34)

/* 00571500 slot 38 (thiscall; old state; RET 4): the countdown begins: MXActivityRaceStub::vf38, +0x2cc = 0; unless a client of a
 * network race (ONLINE: the host draws), the flop: three community cards drawn (00571ca0) and "PokerFlop" raised with the activity
 * (the argument slot is the payload); a network object gets them (ONLINE). Frame: push esi (+ push ebx / edi for the draws). */
void rx_poker_race_vf38(rx_addr act, uint32_t old_state) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_race_stub_vf38(act, old_state));
    SU32(act, 0x2cc, 0);
    if (U32(act, 0x13e8d0) != 0) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0) { ACT_END(F); return; }
    }
    rx_set_sp(F - 8);
    uint32_t c;
    ACT_CALLR(c, 0, rx_poker_draw_card(act)); SU32(act, 0x13e650, c);
    ACT_CALLR(c, 0, rx_poker_draw_card(act)); SU32(act, 0x13e654, c);
    ACT_CALLR(c, 0, rx_poker_draw_card(act));
    SU32(act, 0x13e658, c);
    SU32(F, 8, act);   /* the argument slot (entry ESP + 4) */
    act_raise(U32(act, 0x290), STR_POKER_FLOP, TD_ACTIVITY, F + 8, 4);
    rx_set_sp(F);
    if (U32(act, 0x13e8d0) != 0) act_unreached(0x005715d5, "ONLINE: the flop sent (006f4310)");
    ACT_END(F);
}
RX_FUNCTION(0x00571500, rx_poker_race_vf38)

/* 005715f0 slot 42 (ECX = activity): the race's end check. (ONLINE: a client marks the drivers its session says finished.) The human
 * drivers counted (+0x10 == 0); every driver that finished (the class's vf61, slot 0xf4) marked (+0x70 = 1), the finished humans counted
 * (and online the remote humans still riding); the race ends (WindDown) when every human finished, or after the first finished human
 * once the finish clock +0x2a4 passed +0x2ac (a negative clock is reset to 0). Frame: sub esp, 0xc; push ebp / esi (+ ebx / edi). */
void rx_poker_race_vf42(rx_addr act) {
    ACT_FRAME(F, 0xc, 2);
    session_static();
    if (session_networked()) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0 && U32(act, 0xbc0) == 0) act_unreached(0x0057166d, "ONLINE: a client's finished drivers");
    }
    rx_set_sp(F - 8);
    const int32_t n = I32(act, 0x26c);
    int32_t humans = 0, finished = 0, riding = 0;
    SU32(F, 8, 0); SU32(F, 0xc, 0); SU32(F, 0x10, 0);   /* the counters [esp + 0x10 / 0x14 / 0x18] at the pushes' depth (F - 8) */
    for (int32_t i = 0; i < n; i++) if (U32(U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74), 0x10) == 0) humans++;
    for (int32_t i = 0; i < n; i++) {
        const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        const uint32_t a[1] = {(uint32_t)i};
        if (act_vcall(act, 0xf4, 1, a) != 0) {
            if (U32(U32(d, 0x74), 0x10) == 0) finished++;
            SU32(d, 0x70, 1);
        }
        session_static();
        if (session_networked() && U32(U32(d, 0x74), 0x10) == 0 && U32(U32(d, 0x74), 0x1e4) != 0 && U32(d, 0x70) == 0) riding++;
    }
    if (n > 0) SU32(F, 0x10, (uint32_t)humans);
    SU32(F, 8, (uint32_t)finished); SU32(F, 0xc, (uint32_t)riding);
    rx_set_sp(F);
    if (humans <= 0) { ACT_END(F); return; }
    if (humans != riding + finished && humans != finished) {
        if (finished <= 0) { ACT_END(F); return; }
        const float clock = F32(act, 0x2a4);
        if (0.0f > clock) { SF32(act, 0x2a4, 0.0f); ACT_END(F); return; }
        if (!(clock > F32(act, 0x2ac))) { ACT_END(F); return; }
    }
    ACT_CALL(2, rx_activity_set_state(act, 3));
    ACT_END(F);
}
RX_FUNCTION(0x005715f0, rx_poker_race_vf42)

/* 00571b60 slot 43 (arguments driver a, driver b, tie; RET 0xc) -> the standings order: when both finished the four laps (+0xf1c == 4)
 * the better hand first (+0x1150 the hand's kind, then +0x1154 .. +0x1164 the kickers, higher first: -1 when a is ahead), else the
 * race stub's order (00567520). Frame: push esi. */
int32_t rx_poker_race_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    ACT_FRAME(F, 0, 1);
    if (U32(a, 0xf1c) != 4 || U32(b, 0xf1c) != 4) {
        int32_t r;
        ACT_CALLR(r, 3, rx_race_stub_compare(act, a, b, tie));
        ACT_END(F);
        return r;
    }
    ACT_END(F);
    for (uint32_t k = 0x1150; k < 0x1164; k += 4) {
        if (I32(a, k) > I32(b, k)) return -1;
        if (I32(a, k) < I32(b, k)) return 1;
    }
    if (I32(a, 0x1164) > I32(b, 0x1164)) return -1;
    return I32(a, 0x1164) < I32(b, 0x1164) ? 1 : 0;
}
RX_FUNCTION(0x00571b60, rx_poker_race_vf43)

/* 00571b30 slot 44 (thiscall; player; RET 4) -> a new PokerRaceDriver (0x1170 bytes), 0 when the allocation fails. Frame: push esi. */
rx_addr rx_poker_race_vf44(rx_addr act, rx_addr player) {
    (void)act;
    ACT_FRAME(F, 0, 1);
    rx_addr d;
    ACT_CALLR(d, 1, rx_reflex_aligned_alloc(0x1170));
    if (d != 0) ACT_CALLR(d, 0, rx_poker_driver_construct(d, player));
    ACT_END(F);
    return d;
}
RX_FUNCTION(0x00571b30, rx_poker_race_vf44)

/* ======================================================================================================================== */
/* PokerRaceDriver                                                                                                           */

/* 005134c0 (ESI = driver, EAX = player) -> driver: RaceDriver's constructor (0050ed80), the cards none, kind +0xee8 = 8, hand 0 */
rx_addr rx_poker_driver_construct(rx_addr d, rx_addr player) {
    ACT_CALL(0, rx_race_driver_construct(d, player));
    for (uint32_t k = 0x1134; k <= 0x1164; k += 4) SU32(d, k, 0xffffffffu);
    SU32(d, 0, VT_POKER_DRIVER);
    SU32(d, 0xee8, 8);
    SU32(d, 0x1150, 0);
    return d;
}
RX_FUNCTION(0x005134c0, rx_poker_driver_construct)

/* 00513560 slot 1 / 00513570 slot 2: jumps to RaceDriver::vf1 / vf2 */
void rx_poker_driver_vf1(rx_addr d) { rx_race_driver_vf1(d); }
RX_FUNCTION(0x00513560, rx_poker_driver_vf1)
void rx_poker_driver_vf2(rx_addr d) { rx_race_driver_vf2(d); }
RX_FUNCTION(0x00513570, rx_poker_driver_vf2)

/* 00513580 slot 3 (ECX = driver): RaceDriver::vf3, the cards none, hand 0. Frame: push esi. */
void rx_poker_driver_vf3(rx_addr d) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_race_driver_vf3(d));
    for (uint32_t k = 0x1134; k <= 0x1164; k += 4) SU32(d, k, 0xffffffffu);
    SU32(d, 0x1150, 0);
    ACT_END(F);
}
RX_FUNCTION(0x00513580, rx_poker_driver_vf3)

/* 005135e0 slot 7 (thiscall; dt; RET 4): a spectator (+0x908) runs NetworkDriver::vf7; else RaceDriver::vf7 and a local human's card
 * pick (00513630). Frame: push ebp; mov ebp, esp; and esp, -8; push ecx / esi; dt through x87. */
void rx_poker_driver_vf7(rx_addr d, float dt) {
    ACT_FRAME_EBP(F, 8, 0, 2);
    if (U32(U32(d, 0x74), 0x908) != 0) {
        ACT_CALL(1, rx_network_driver_update(d, x87_copy(dt)));
        ACT_END(F);
        return;
    }
    ACT_CALL(1, rx_race_driver_update(d, x87_copy(dt)));
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 4) != 0 && U32(p, 0x10) == 0) ACT_CALL(0, rx_poker_driver_pick(d));
    ACT_END(F);
}
RX_FUNCTION(0x005135e0, rx_poker_driver_vf7)

/* ======================================================================================================================== */
/* the cards                                                                                                                 */

/* 00571d60 (thiscall act; driver, lap, two more; RET 0x10): "Lap Completed". The
 * third lap draws the fourth community card (+0x13e65c, once; ONLINE: sent). Otherwise a driver behind the first three (place > 2)
 * gets its lap's card at once (lap 1 -> +0x1134, 2 -> +0x1138, 4 -> +0x113c) and "PokerCardSelected"; one of the first three first
 * keeps the card it was offered last (+0x1140 -> +0x112c + 4 * lap, "PokerCardSelected", the others back to the deck), then is offered
 * new cards by its place (0: four, 1: three, 2: two; "PokerCardAvailable"). A networked driver (+0xef0) is dealt only by the host
 * ([0xda6cb0] + 0x1c48). Frame: push ebp / edi (+ ebx, + esi). */
void rx_poker_lap_completed(rx_addr act, rx_addr driver, int32_t lap, uint32_t c, uint32_t d) {
    (void)c; (void)d;
    ACT_FRAME(F, 0, 2);
    if (U32(driver, 0xef0) != 0) {
        session_static();
        if (U32(U32(0, 0x00da6cb0), 0x1c48) == 0) { ACT_END(F); return; }   /* a networked driver's cards come from the host */
    }
    rx_set_sp(F - 4);   /* push ebx */
    if (lap == 3) {
        if (I32(act, 0x13e65c) < 0) { uint32_t card; ACT_CALLR(card, 0, rx_poker_draw_card(act)); SU32(act, 0x13e65c, card); }
        if (U32(act, 0x13e8d0) != 0) act_unreached(0x00571df0, "ONLINE: the community cards sent (006f4310)");
        ACT_END(F);
        return;
    }
    if (lap == 0) { ACT_END(F); return; }
    rx_set_sp(F - 8);   /* push esi */
    int32_t place;
    ACT_CALLR(place, 0, rx_activity_driver_place(act, driver));
    if (place > 2) {
        uint32_t slot;
        if (lap == 1) slot = 0x1134;
        else if (lap == 2) slot = 0x1138;
        else if (lap == 4) slot = 0x113c;
        else { ACT_END(F); return; }
        uint32_t card;
        ACT_CALLR(card, 0, rx_poker_draw_card(act));
        SU32(driver, slot, card);
        ACT_CALL(1, rx_event_activity_driver_at(U32(act, 0x290), STR_CARD_PICKED, driver, act));
        if (U32(act, 0x13e8d0) != 0 && U32(driver, 0xef0) != 0) act_unreached(0x00571e65, "ONLINE: the hand card sent (006f4390)");
        ACT_END(F);
        return;
    }
    if (U32(driver, 0x1140) != 0xffffffffu) {
        SU32(driver, 0x112c + 4u * (uint32_t)lap, U32(driver, 0x1140));
        ACT_CALL(1, rx_event_activity_driver_at(U32(act, 0x290), STR_CARD_PICKED, driver, act));
        if (U32(act, 0x13e8d0) != 0 && U32(driver, 0xef0) != 0) act_unreached(0x00571f64, "ONLINE: the chosen card sent (006f4390)");
        for (uint32_t k = 0x1144; k <= 0x114c; k += 4) {
            if (U32(driver, k) == 0xffffffffu) continue;
            SU8(DECK_FLAG(act, U32(driver, k)), 0, 1);
            SU32(driver, k, 0xffffffffu);
        }
    }
    ACT_CALLR(place, 0, rx_activity_driver_place(act, driver));
    const int32_t offered = place == 0 ? 4 : place == 1 ? 3 : place == 2 ? 2 : 0;
    if (offered == 0) { ACT_END(F); return; }
    for (int32_t k = 0; k < offered; k++) {
        uint32_t card;
        ACT_CALLR(card, 0, rx_poker_draw_card(act));
        SU32(driver, 0x1140 + 4u * (uint32_t)k, card);
    }
    ACT_CALL(1, rx_event_activity_driver_at(U32(act, 0x290), STR_CARD_AVAIL, driver, act));
    if (U32(act, 0x13e8d0) != 0 && U32(driver, 0xef0) != 0) act_unreached(0x0057203f, "ONLINE: the offered cards sent (006f4400)");
    ACT_END(F);
}
RX_FUNCTION(0x00571d60, rx_poker_lap_completed)

/* 00513630 (ESI = driver): a local human's card pick while it has a choice (+0x1140 != -1): the card buttons of its controller's key
 * maps (+0x2ba0, +0x2fa0, +0x31a0: vf1 with 7 and the controller; +0x2da0: 00548850) pick the first .. fourth card offered (the fourth
 * and the later ones only when offered); the picked card becomes the hand's card for the lap (+0x1134 + 4 * k, k = 0 for the first
 * lap, 1 for the second, 2 for the fourth (+0xf1c the laps done)), the others go back to the deck, the choice is cleared (ONLINE: sent).
 * Frame: push edi (+ ebx / ebp). */
void rx_poker_driver_pick(rx_addr d) {
    ACT_FRAME(F, 0, 1);
    const rx_addr act = U32(U32(d, 0x74), 0x94);
    if (U32(d, 0x1140) == 0xffffffffu) { ACT_END(F); return; }
    rx_set_sp(F - 8);   /* push ebx / ebp */
    const uint32_t laps = U32(d, 0xf1c);
    const uint32_t k = laps == 1 ? 0 : laps == 2 ? 1 : laps == 4 ? 2 : 0;
    static const uint32_t maps[3] = {0x2ba0, 0x2fa0, 0x31a0};
    int pick = -1;
    for (int b = 0; b < 4 && pick < 0; b++) {
        const uint32_t controller = U32(U32(d, 0x74), 0x1f0);
        uint32_t pressed;
        if (b < 3) {
            const rx_addr km = U32(0, 0x009c52c4 + 4 * controller) + maps[b];
            const uint32_t a[2] = {7, controller};
            pressed = act_vcall(km, 4, 2, a);
        } else ACT_CALLR(pressed, 0, rx_player_pressed_2da0(controller) & 0xff);
        if (pressed == 0) continue;
        if (b > 0 && U32(d, 0x1140 + 4u * (uint32_t)b) == 0xffffffffu) { ACT_END(F); return; }
        pick = b;
    }
    if (pick < 0) { ACT_END(F); return; }
    SU32(d, 0x1134 + 4 * k, U32(d, 0x1140 + 4u * (uint32_t)pick));
    if (U32(act, 0x13e8d0) != 0 && U32(d, 0xef0) != 0) act_unreached(0x005136b7, "ONLINE: the picked card sent (006f4390)");
    for (int b = 0; b < 4; b++) {
        if (b == pick) continue;
        const uint32_t c = U32(d, 0x1140 + 4u * (uint32_t)b);
        if (c != 0xffffffffu) SU8(DECK_FLAG(act, c), 0, 1);
    }
    for (uint32_t o = 0x1140; o <= 0x114c; o += 4) SU32(d, o, 0xffffffffu);
    ACT_END(F);
}
RX_FUNCTION(0x00513630, rx_poker_driver_pick)

/* ======================================================================================================================== */
/* the hands                                                                                                                 */

#define CARD_SUIT(A, c) U32((A) + 0x13e660u + 0xcu * (uint32_t)(c), 0)
#define CARD_RANK(A, c) U32((A) + 0x13e664u + 0xcu * (uint32_t)(c), 0)
#define CARD_RANK_AT(A, c) ((A) + 0x13e664u + 0xcu * (uint32_t)(c))

/* 00572360 (ECX = five card indices): sorted ascending in place (a selection sort, signed). */
void rx_poker_sort5(rx_addr h) {
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t m = i;
        for (uint32_t k = i + 1; k < 5; k++) if (I32(h, 4 * k) < I32(h, 4 * m)) m = k;
        const uint32_t t = U32(h, 4 * i);
        SU32(h, 4 * i, U32(h, 4 * m));
        SU32(h, 4 * m, t);
    }
}
RX_FUNCTION(0x00572360, rx_poker_sort5)

/* 00572720 (ECX = act, EAX = the best hand's kind; stdcall the five sorted card indices, then the five tie-break ranks; RET 0x18): the
 * five cards' hand, kept when it beats the best kind so far (*kind, signed): 10 a royal flush (always kept), 9 a straight flush, 8 four
 * of a kind, 7 a full house, 6 a flush, 5 a straight, 4 three of a kind, 3 two pair, 2 a pair, 1 the high card; the tie-break ranks
 * are the original's for each case (some repeat a rank where a kicker would be expected; kept as they are). The straight needs
 * consecutive ranks in index order, the flush one suit. */
void rx_poker_rank_hand(rx_addr act, rx_addr kind, rx_addr hand, rx_addr k1, rx_addr k2, rx_addr k3, rx_addr k4, rx_addr k5) {
    const uint32_t c0 = U32(hand, 0), c1 = U32(hand, 4), c2 = U32(hand, 8), c3 = U32(hand, 0xc), c4 = U32(hand, 0x10);
    const int32_t r0 = (int32_t)CARD_RANK(act, c0), r4 = (int32_t)CARD_RANK(act, c4);
    int straight = 0, flush = 0;
    if (r4 - r0 == 4 && (int32_t)CARD_RANK(act, c1) == r0 + 1 && (int32_t)CARD_RANK(act, c2) == r0 + 2 && (int32_t)CARD_RANK(act, c3) == r0 + 3)
        straight = 1;
    const uint32_t s1 = CARD_SUIT(act, c1);
    if (CARD_SUIT(act, c0) == s1 && CARD_SUIT(act, c2) == s1 && CARD_SUIT(act, c3) == CARD_SUIT(act, c2) && CARD_SUIT(act, c4) == CARD_SUIT(act, c3))
        flush = 1;
    const int32_t r1 = (int32_t)CARD_RANK(act, c1), r2 = (int32_t)CARD_RANK(act, c2), r3 = (int32_t)CARD_RANK(act, c3);
    const int32_t best = I32(kind, 0);
#define KEEP(K, a, b, c, d, e) do { SU32(kind, 0, (K)); SU32(k1, 0, (uint32_t)(a)); SU32(k2, 0, (uint32_t)(b)); SU32(k3, 0, (uint32_t)(c)); \
                                   SU32(k4, 0, (uint32_t)(d)); SU32(k5, 0, (uint32_t)(e)); return; } while (0)
    if (straight && flush) {
        if (r4 == 0xc) KEEP(10, 12, 12, 12, 12, 12);
        if (best < 9) KEEP(9, r4, r3, r2, r1, r0);
    }
    if (r0 == r1 && r1 == r2 && r2 == r3 && best < 8) KEEP(8, r3, r4, r4, r4, r4);
    if (r1 == r2 && r2 == r3 && r3 == r4 && best < 8) KEEP(8, r1, r0, r0, r0, r0);
    if (r0 == r1 && r1 == r2 && r3 == r4 && best < 7) KEEP(7, r0, r3, r3, r3, r3);
    if (r0 == r1 && r2 == r3 && r3 == r4 && best < 7) KEEP(7, r3, r0, r0, r0, r0);
    if (flush && best < 6) KEEP(6, r4, r3, r2, r1, r0);
    if (straight && best < 5) KEEP(5, r4, r3, r2, r1, r0);
    if (r0 == r1 && r1 == r2 && best < 4) KEEP(4, r0, r4, r3, r3, r3);
    if (r1 == r2 && r2 == r3 && best < 4) KEEP(4, r1, r4, r0, r0, r0);
    if (r2 == r3 && r3 == r4 && best < 4) KEEP(4, r2, r1, r0, r0, r0);
    if (r0 == r1 && r2 == r3 && best < 3) KEEP(3, r2, r0, r4, r4, r4);
    if (r0 == r1 && r3 == r4 && best < 3) KEEP(3, r3, r1, r2, r2, r2);
    if (r1 == r2 && r3 == r4 && best < 3) KEEP(3, r3, r1, r0, r0, r0);
    if (r0 == r1 && best < 2) KEEP(2, r0, r4, r3, r2, r2);
    if (r1 == r2 && best < 2) KEEP(2, r1, r4, r3, r0, r0);
    if (r2 == r3 && best < 2) KEEP(2, r2, r4, r1, r0, r0);
    if (r3 == r4 && best < 2) KEEP(2, r3, r2, r1, r0, r0);
    if (best < 1) KEEP(1, r4, r3, r2, r1, r0);
#undef KEEP
}
RX_FUNCTION(0x00572720, rx_poker_rank_hand)

/* 005723c0 (stdcall act, driver; RET 8): the driver's best five-card hand of its three hand cards (+0x1134 .. +0x113c; none when the
 * third is -1) and the four community cards, all 21 choices: kind -> +0x1150, the tie-break ranks -> +0x1154 .. +0x1164. The best
 * kind lives in the act argument's slot and the ranks in the frame (sub 0x50; push ebx / ebp / esi / edi), as in the original. */
void rx_poker_evaluate_driver(rx_addr act, rx_addr d) {
    ACT_FRAME(F, 0x50, 4);
    const rx_addr B = F + 16;          /* esp after sub 0x50 */
    const uint32_t h3 = U32(d, 0x113c);
    SU32(B, 0x14, h3);
    if (h3 == 0xffffffffu) { ACT_END(F); return; }
    const rx_addr kind = B + 0x54, keys = B, hand = B + 0x3c;
    SU32(kind, 0, 0);
    for (uint32_t k = 0; k < 5; k++) SU32(keys, 4 * k, 0);
    const rx_addr comm = act + 0x13e650;
#define TRY(a, b, c, e, f) do { SU32(hand, 0, (a)); SU32(hand, 4, (b)); SU32(hand, 8, (c)); SU32(hand, 0xc, (e)); SU32(hand, 0x10, (f)); \
        ACT_CALL(0, rx_poker_sort5(hand)); \
        ACT_CALL(6, rx_poker_rank_hand(act, kind, hand, keys, keys + 4, keys + 8, keys + 0xc, keys + 0x10)); } while (0)
    for (uint32_t i = 1; i < 4; i++)                       /* the three hand cards and two community cards */
        for (uint32_t j = i; j < 4; j++) TRY(U32(d, 0x1134), U32(d, 0x1138), U32(B, 0x14), U32(comm, 4 * (i - 1)), U32(comm, 4 * j));
    for (uint32_t k = 0; k < 3; k++)                       /* one hand card and the four community cards */
        TRY(U32(d, 0x1134 + 4 * k), U32(comm, 0), U32(comm, 4), U32(comm, 8), U32(comm, 0xc));
    for (uint32_t i = 1; i < 4; i++)                       /* two hand cards and three community cards */
        for (uint32_t j = i + 1; j < 4; j++)
            for (uint32_t k = j; k < 4; k++) {
                const uint32_t h1 = U32(d, 0x1134), h2 = U32(d, 0x1138);
                const uint32_t a = U32(comm, 4 * (i - 1)), b = U32(comm, 4 * (j - 1)), c = U32(comm, 4 * k);
                TRY(h1, h2, a, b, c);
                TRY(h1, U32(B, 0x14), a, b, c);
                TRY(h2, U32(B, 0x14), a, b, c);
            }
#undef TRY
    SU32(d, 0x1150, U32(kind, 0));
    for (uint32_t k = 0; k < 5; k++) SU32(d, 0x1154 + 4 * k, U32(keys, 4 * k));
    ACT_END(F);
}
RX_FUNCTION(0x005723c0, rx_poker_evaluate_driver)

/* 005717e0 slot 28 (thiscall; dt; RET 4): racing. The track debug view (+0xb8c: 005399e0); ONLINE: in a networked session without the
 * NetPokerRace object the host makes it (sending the community cards), a client finds it; then the lap flags (00567e30), the lapping
 * warnings (00567f30) and MXActivityRaceStub::vf28. Frame: push ebp; and esp, -8; sub 0x24 (the cookie at +0x20); push ebx / esi / edi. */
void rx_poker_race_vf28(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 8, 0x24, 3);
    const rx_addr L = F + 12;
    SU32(L, 0x20, U32(0, 0x009b45e4) ^ L);   /* the security cookie */
    if (U32(act, 0xb8c) != 0) ACT_CALL(0, rx_waypoint_track_debug(act + 0x890));
    if (U32(act, 0x13e8d0) == 0) {
        session_static();
        if (session_networked()) act_unreached(0x0057185d, "ONLINE: the NetPokerRace object made (host) or found (client)");
    }
    ACT_CALL(0, rx_race_lap_flags(act));
    ACT_CALL(1, rx_race_lapping_warnings(act));
    ACT_CALL(1, rx_race_stub_in_progress(act, x87_copy(dt)));
    ACT_END(F);
}
RX_FUNCTION(0x005717e0, rx_poker_race_vf28)

/* ======================================================================================================================== */
/* RuleOfftrack (vtable 0x0093fa90, 0x70 bytes; the PokerRace's: rx_poker_race_vf47): +4 the driver, +8 enabled, +0x40 .. +0x4c the
 * point to be put back at, +0x50 on the track, +0x54 left at the finish section, +0x58 handled, +0x60 (double) the clock after which
 * a rider off the track is put back (31.5 s after it left).                                                                  */

/* 0051ab70 slot 1 (thiscall; driver; RET 4) */
void rx_rule_offtrack_vf1(rx_addr rule, rx_addr d) {
    SU32(rule, 4, d); SU32(rule, 8, 1); SU32(rule, 0x50, 1); SU32(rule, 0x54, 0); SF64(rule, 0x60, 0.0);
}
RX_FUNCTION(0x0051ab70, rx_rule_offtrack_vf1)

/* 0051aba0 slot 4 (thiscall; enabled; RET 4): disabled, it is on the track again */
void rx_rule_offtrack_vf4(rx_addr rule, uint32_t on) {
    SU32(rule, 8, on);
    if (on == 0) { SU32(rule, 0x50, 1); SU32(rule, 0x54, on); SF64(rule, 0x60, 0.0); }
}
RX_FUNCTION(0x0051aba0, rx_rule_offtrack_vf4)

/* 0051ae90 (EDX = rule): the rider left the track (while enabled, on it and not handled): the clock limit (+0x130 + 31.5), whether it
 * left at the finish section (the driver's node or the next one is the track's +0x9c), the point to be put back at (the track's
 * window along the nodes ahead of +0x90: each node's +0x4f0 segment length), then the driver hears of it (vf9). */
void rx_rule_offtrack_left(rx_addr rule) {
    if (U32(rule, 8) == 0 || U32(rule, 0x50) == 0 || U32(rule, 0x58) != 0) return;
    const rx_addr d = U32(rule, 4);
    SU32(rule, 0x50, 0);
    SF64(rule, 0x60, (double)(F32(d, 0x130) + 31.5f /* 0x9ca014 */));
    const uint32_t finish = U32(U32(d, 0x10), 0x9c);
    SU32(rule, 0x54, (U32(d, 0x18) == finish || U32(d, 0x1c) == finish) ? 1 : 0);
    rx_addr node = U32(d, 0x90);
    float window = F32(U32(d, 0x10), 0x1b8 + 4 * U32(U32(U32(d, 0x74), 0x1ac), 0x194));
    for (; node != 0 && window > 0.0f; node = U32(node, 0x64)) {
        SF32(rule, 0x40, x87_copy(F32(node, 0)));
        window = window - F32(node, 0x4f0);
        SF32(rule, 0x44, x87_copy(F32(node, 4)));
        SF32(rule, 0x4c, 1.0f);
        SF32(rule, 0x48, x87_copy(F32(node, 8)));
    }
    (void)act_vcall(d, 0x24, 1, &rule);
}
RX_FUNCTION(0x0051ae90, rx_rule_offtrack_left)

/* 0051af60 (EAX = rule): put back on the track (005507a0) unless wrecked (+0x280) or [0xa942b4] is 2. Frame: push ebp; and esp, -8;
 * push ecx / esi. */
void rx_rule_offtrack_put_back(rx_addr rule) {
    ACT_FRAME_EBP(F, 8, 0, 2);
    const rx_addr player = U32(U32(rule, 4), 0x74);
    if (U32(U32(player, 0x1ac), 0x280) == 0 && U32(0, 0x00a942b4) != 2) ACT_CALL(0, rx_player_reset_offtrack(player));
    ACT_END(F);
}
RX_FUNCTION(0x0051af60, rx_rule_offtrack_put_back)

/* 0051abc0 slot 5 (thiscall; dt; RET 4): the vehicle's segment on the track (005387c0 in the driver's window; second edges for a
 * human (+4) not replaying (+0x10)). On the track: lost (no segment, or a segment flagged 0x1000 more than 8 above the vehicle) is
 * leaving it (0051ae90); else +0x58 = 0. Off the track: found again from the track's +0x9c section on (inside: the driver's position
 * +0x80 kept; at a node (3): the driver's node / next / +0x90 and section moved there, the track-advance rule told), else still off;
 * back on: +0x58 = 0, the driver hears of it (vf9), +0x54 = 0; still off past the clock limit: put back (0051af60), +0x54 = 0.
 * Frame: push ebp; and esp, -16; sub 0x44; push ebx / esi / edi (F + 0x20 the query, F + 0x14 its result pair). */
void rx_rule_offtrack_vf5(rx_addr rule, float dt) {
    (void)dt;
    ACT_FRAME_EBP(F, 16, 0x44, 3);
    rx_addr d = U32(rule, 4);
    const rx_addr player = U32(d, 0x74);
    if (U32(player, 0x1ac) == 0) { ACT_END(F); return; }
    const rx_addr Q = F + 0x20, pair = F + 0x14;
    const rx_addr veh = U32(player, 0x1ac);
    for (uint32_t k = 0; k < 0x28; k += 4) SU32(Q, k, 0);
    SU32(Q, 0xc, U32(d, 0x18)); SU32(Q, 8, U32(d, 0x14)); SU32(Q, 0, pair); SU32(Q, 4, veh + 0xc0);
    const float window = F32(U32(d, 0x10), 0x1b8 + 4 * U32(veh, 0x194));
    SU32(Q, 0x20, 1); SU32(Q, 0x24, 0);
    SF32(Q, 0x14, window); SF32(Q, 0x18, window);
    if (U32(rule, 0x50) != 0) {
        const int second = (U32(player, 4) != 0 && U32(player, 0x10) == 0) ? 1 : 0;
        int found;
        ACT_CALLR(found, 1, rx_track_find_segment(Q, second));
        int lost = found == 0;
        if (!lost) {
            const rx_addr p = U32(d, 0x74), v = U32(p, 0x1ac);
            const uint32_t idx = U32(v, 0x198) + 4 * U32(v, 0x194);
            if ((U32(U32(d, 0x18), 0x68 + 4 * idx) & 0x1000) != 0 && F32(U32(pair, 4), 4) - F32(v, 0xc4) > 8.0f /* 0x9322b8 */) lost = 1;
        }
        if (lost) ACT_CALL(0, rx_rule_offtrack_left(rule));
        else SU32(rule, 0x58, 0);
        ACT_END(F);
        return;
    }
    SU32(rule, 0x50, 1);
    SU32(Q, 0x1c, 1);
    SU32(Q, 0x10, U32(U32(d, 0x10), 0x9c));
    {
        const int second = (U32(player, 4) != 0 && U32(player, 0x10) == 0) ? 1 : 0;
        int found;
        ACT_CALLR(found, 1, rx_track_find_segment(Q, second));
        if (found == 0) SU32(rule, 0x50, 0);
        else {
            d = U32(rule, 4);
            const rx_addr v = U32(U32(d, 0x74), 0x1ac);
            const uint32_t idx = U32(v, 0x198) + 4 * U32(v, 0x194);
            const rx_addr node = U32(pair, 4);
            if ((U32(node, 0x68 + 4 * idx) & 0x1000) != 0 && F32(node, 4) - F32(v, 0xc4) > 8.0f) SU32(rule, 0x50, 0);
            else if (found == 1) {
                SF32(d, 0x80, x87_copy(F32(v, 0xc0))); SF32(d, 0x84, x87_copy(F32(v, 0xc4))); SF32(d, 0x88, x87_copy(F32(v, 0xc8)));
            } else if (found == 3) {
                SU32(d, 0x18, node);
                SU32(d, 0x1c, U32(node, 0x64));
                SU32(d, 0x90, U32(d, 0x1c));
                const rx_addr v2 = U32(U32(d, 0x74), 0x1ac);
                SF32(d, 0x80, x87_copy(F32(v2, 0xc0))); SF32(d, 0x84, x87_copy(F32(v2, 0xc4))); SF32(d, 0x88, x87_copy(F32(v2, 0xc8)));
                const rx_addr section = U32(pair, 0);
                if (section != U32(d, 0x14)) {
                    rx_addr r;
                    ACT_CALLR(r, 1, rx_driver_track_advance_rule(d));
                    if (r != 0) {
                        SU32(r, 0x10, section);
                        if (U32(section, 0x14) != 0) ACT_CALL(0, rx_rule_track_advance_section_changed(r));
                        else SF32(r, 0xc, rx_bf(0x7f7fffffu) /* 0x977494 */);
                    }
                    SU32(U32(rule, 4), 0x14, U32(pair, 0));
                }
            }
        }
    }
    d = U32(rule, 4);
    if (U32(rule, 0x50) != 0) {
        SU32(rule, 0x58, 0);
        (void)act_vcall(d, 0x24, 1, &rule);
        SU32(rule, 0x54, 0);
    } else if ((double)F32(d, 0x130) > F64(rule, 0x60)) {
        ACT_CALL(0, rx_rule_offtrack_put_back(rule));
        SU32(rule, 0x54, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x0051abc0, rx_rule_offtrack_vf5)

/* ======================================================================================================================== */
/* RaceDriver's slots the PokerRaceDriver keeps                                                                              */

/* 00512bc0 RaceDriver::vf16 (thiscall; kind, object; RET 8): NetworkDriver::vf16, then for kind 1 with its own live network object the
 * race state sent (0050bad0): ONLINE. Frame: push ebx / esi / edi. */
void rx_race_driver_vf16(rx_addr d, uint32_t kind, rx_addr object) {
    ACT_FRAME(F, 0, 3);
    ACT_CALL(2, rx_network_driver_vf16(d, kind, object));
    if (kind == 1 && U32(d, 0xef0) == object && (rx_net_ref_valid(object) & 0xff) != 0)
        act_unreached(0x00512bec, "ONLINE: the race driver's network state (0050bad0)");
    ACT_END(F);
}
RX_FUNCTION(0x00512bc0, rx_race_driver_vf16)

/* 00512c30 RaceDriver::vf17 (ECX = driver): the driver leaves: with a network player (+0xb2c) gone or not live, a local one's laps
 * -1 and best lap FLT_MAX, its +0x10cc 0; then a jump to NetworkDriver::vf14. */
void rx_race_driver_vf17(rx_addr d) {
    const rx_addr net = U32(U32(d, 0x74), 0xb2c);
    if (net != 0 && ((U8(net, 0x128) & 8) != 0 || U8(net, 0x138) == 0)) {
        if (U32(d, 0x70) == 0) {
            SU32(d, 0xf1c, 0xffffffffu);
            SU32(d, 0x10c4, 0x7f7fffffu /* 0x977494 */);
        }
        SF32(d, 0x10cc, 0.0f);
    }
    rx_waypoint_network_vf14(d);
}
RX_FUNCTION(0x00512c30, rx_race_driver_vf17)

/* ======================================================================================================================== */
/* PokerRaceActivityAudioLogic (vtable 0x0094301c): RaceActivityAudioLogic plus the cues "PokerFlop", "PokerCardAvailable",
 * "PokerCardSelected" and "Player Wins" when the level's cue map (+0x8c) holds them (as race_tag_audio.c).                 */

#define ACH(l) U32(U32((l), 0x50), 0x290)   /* the logic's activity's channel */
#define STR_PLAYER_WINS 0x00941ca8u

static int cue_present(rx_addr logic, rx_addr L, rx_addr name) {
    const rx_addr s = L + 0x1c, it = L + 0x14;
    crt_str_ctor_cstr(s, name);
    const rx_addr map = U32(logic, 0x8c);
    const rx_addr end = U32(map, 0x18), proxy = U32(map, 0);
    rx_addr r;
    SU32(L, -4, s);
    SU32(L, -8, it);
    ACT_CALLR(r, 2, rx_world_string_map_find(map, it, s));
    if (U32(r, 0) == 0 || U32(r, 0) != proxy) crt_invalid_parameter_noinfo();
    int found = U32(r, 4) != end;
    crt_str_dtor(s);
#ifndef RX_ORACLE
    if (!found) found = rx_activity_cues_present(logic, name);   /* w-sfx: the level's cues (activity_cues.c) */
#endif
    return found;
}

/* 0058c930 (ECX = channel, EAX = name; stdcall logic, method; RET 8): a void(Activity*, BaseDriver*) event (proxy 0x946084).
 * Frame: push ebx / esi / edi. */
void rx_poker_audio_listen_driver(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_subscribe(0x0058c930, 2 | 4, 2, 3, channel, name, 0x00946084, 0x009cda18, logic, method);
}
RX_FUNCTION(0x0058c930, rx_poker_audio_listen_driver)
/* 0058c9f0 (EAX = channel; stdcall logic; RET 4): "Player Wins" with 00583fb0 (proxy 0x9460a4). Frame: push ecx / ebx / esi / edi. */
void rx_poker_audio_listen_player_wins(rx_addr channel, rx_addr logic) {
    act_subscribe(0x0058c9f0, 1, 1, 4, channel, STR_PLAYER_WINS, 0x009460a4, 0x009cf630, logic, 0x00583fb0);
}
RX_FUNCTION(0x0058c9f0, rx_poker_audio_listen_player_wins)
/* 0058cab0 / 0058cb10: they leave (frame: sub esp, 0x14; push esi) */
void rx_poker_audio_unlisten_driver(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_unlisten(0, channel, name, 0x00946084, 0x009cda18, logic, method);
}
RX_FUNCTION(0x0058cab0, rx_poker_audio_unlisten_driver)
void rx_poker_audio_unlisten_player_wins(rx_addr channel, rx_addr logic) {
    act_unlisten(0, channel, STR_PLAYER_WINS, 0x009460a4, 0x009cf630, logic, 0x00583fb0);
}
RX_FUNCTION(0x0058cb10, rx_poker_audio_unlisten_player_wins)

/* 00584070 slot 3 (ECX = logic): RaceActivityAudioLogic::vf3, then the cue handlers listen. Frame: push ebp; and esp, -8; sub esp,
 * 0x28; push ebx / ebp / esi / edi. */
void rx_poker_audio_vf3(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    ACT_CALL(0, rx_race_audio_logic_vf3(logic));
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, STR_POKER_FLOP)) ACT_CALL(2, rx_poker_audio_listen_driver(ACH(logic), STR_POKER_FLOP, logic, 0x005843f0));
        if (cue_present(logic, L, STR_CARD_AVAIL)) ACT_CALL(2, rx_poker_audio_listen_driver(ACH(logic), STR_CARD_AVAIL, logic, 0x00584410));
        if (cue_present(logic, L, STR_CARD_PICKED)) ACT_CALL(2, rx_poker_audio_listen_driver(ACH(logic), STR_CARD_PICKED, logic, 0x00584430));
        if (cue_present(logic, L, STR_PLAYER_WINS)) ACT_CALL(1, rx_poker_audio_listen_player_wins(ACH(logic), logic));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00584070, rx_poker_audio_vf3)

/* 00584230 slot 4 (ECX = logic): RaceActivityAudioLogic::vf4, then the cue handlers leave. Same frame. */
void rx_poker_audio_vf4(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    ACT_CALL(0, rx_race_audio_logic_vf4(logic));
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, STR_POKER_FLOP)) ACT_CALL(2, rx_poker_audio_unlisten_driver(ACH(logic), STR_POKER_FLOP, logic, 0x005843f0));
        if (cue_present(logic, L, STR_CARD_AVAIL)) ACT_CALL(2, rx_poker_audio_unlisten_driver(ACH(logic), STR_CARD_AVAIL, logic, 0x00584410));
        if (cue_present(logic, L, STR_CARD_PICKED)) ACT_CALL(2, rx_poker_audio_unlisten_driver(ACH(logic), STR_CARD_PICKED, logic, 0x00584430));
        if (cue_present(logic, L, STR_PLAYER_WINS)) ACT_CALL(1, rx_poker_audio_unlisten_player_wins(ACH(logic), logic));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00584230, rx_poker_audio_vf4)

/* 005843f0 / 00584410 / 00584430 (thiscall logic; activity, driver; RET 8): the cue of the event's name for the driver's player */
void rx_poker_audio_flop(rx_addr logic, rx_addr act, rx_addr driver) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_POKER_FLOP, U32(driver, 0x74), 0)); }
RX_FUNCTION(0x005843f0, rx_poker_audio_flop)
void rx_poker_audio_card_available(rx_addr logic, rx_addr act, rx_addr driver) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_CARD_AVAIL, U32(driver, 0x74), 0)); }
RX_FUNCTION(0x00584410, rx_poker_audio_card_available)
void rx_poker_audio_card_selected(rx_addr logic, rx_addr act, rx_addr driver) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_CARD_PICKED, U32(driver, 0x74), 0)); }
RX_FUNCTION(0x00584430, rx_poker_audio_card_selected)
