#ifndef RX_RACE_HUD_H
#define RX_RACE_HUD_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The values of four race HUD components that the original draws from typed events and activity fields (w-hud, 2026-10-26). Copies of
 * what the components read; the strings and the layout are the presenter's (the install's HUD layers and localization tables).
 * Additive: a new query and struct, RX_SESSION_API_VERSION and rx_race_format_state unchanged.
 *
 * Event timer     MXUITimerComponent::vf11 (005f7140): "EventTimerComponent" of the Freestyle and Section Race layers shows
 *                 clock = A + 0x294, limit = A + 0x2bc: the FMX jam (A + 0x23c == 22) and a counting-down activity (A + 0x2c8 == 0)
 *                 max(0, limit - clock), else the clock.
 * Eliminated      MXUIPlayerEliminatedComponentStub (00605de0, "Player Eliminated" {activity, driver, next}): the 3 s message
 *                 "<next's name>\n<STR_ELIMINATED>\n<driver's name>!" (names = the driver's player + 0x54). `eliminated_events` counts
 *                 the events of the bound activity; the slots are the drivers' player slots (-1 unknown).
 * Outrun gate     MXUIOutrunnerGateCounter (00605190, "EnteredTimedSection" {vehicle, TimedRaceSection}): once a section that gives
 *                 time (+0x280) with a number >= the last shown one is entered by any rider the text "<STR_GATE> number/total" shows,
 *                 total = A + 0xff08c. "Restart" sets the last shown number back to 0 and keeps the text. gate_events counts the shows.
 * Section Race    MXUISectionTimeComponent (00606cf0) / MXUIBestSectionTimeComponent (006055b0): the local driver's running time in
 *                 the section it is in (D + 0x1140[D + 0x113c], 0 when not in one) and the section's best time (A + 0x13e654[...],
 *                 FLT_MAX none); MXUISectionLeadersContainer (00606970, "SectionWinner" {driver or 0, section}): label i reads
 *                 "STR_POUND_SIGN" + (i + 1) + " " + the winner's name once the event for section i came (a name of "" and
 *                 leader_set bit set = the leader left); "Restart" clears the labels.
 * Freestyle       MXUIFMXFeedbackComponent (00607540): four judge columns whose lit bars (0..10) are the peak of trunc(value) over the
 *                 frames since the last "Restart": 1 = J+4 - J+0x38, 2 = J+8, 3 = J+0x10 - J+0x34, 4 = J+0xc - J+0x30 (J = the local
 *                 player + 0xc70, the stunt scorer); MXUIFMXFinalScoreCard (006039b0): at "WindDown Entered" the card shows the scorer's
 *                 ratings J+0x14 (Overall), +0x18 (Difficulty), +0x1c (Execution), +0x24 (Flow), +0x20 (Variety), copied at that
 *                 moment, until "Restart" or "Post Entered". MXUIStuntJudgeComponent ("ScoreCard") hides itself and is never updated.
 * Hit Discovery  MXUIHitDiscoveredComponent (006045b0, "HitDiscoveryFound" {player, found, total}: the Free Ride's discovery tracker 00585330
 *                 raises it when the local rider reaches a collectable): the "%d/%d" text shows for 3 s (00604670). `discovery_events`
 *                 counts the local player's events of the bound activity, `discovery_found` / `discovery_total` are the tracker's own
 *                 counts (activity + 0xff080: +0x84 found, +0 collectables), valid only in the Free Ride's discovery mode (the Race & Ride
 *                 challenges Free Ride: activity + 0xff180 == 1, type 8); 0 / 0 elsewhere (w-pausemodes).
 * activity_type (A + 0x23c): 5 Freestyle, 12 Section Race, 8 Free Ride. Everything is 0 / -1 when the bound activity is not the local player's or the kind does not apply (valid stays 1 with the activity). */
#define RX_RACE_HUD_VERSION 1u
#define RX_RACE_HUD_NAME 68          /* the player name buffer (player + 0x54, 0x44 bytes, UTF-8) */
#define RX_RACE_HUD_SECTIONS 5       /* MXUISectionLeadersContainer's labels */
typedef struct rx_race_hud_state {
    uint32_t size, version, valid;
    int32_t activity_type, activity_mode, activity_state;
    float clock, clock_limit;
    int32_t clock_counts_up;
    uint32_t restart_events;
    uint32_t eliminated_events;
    int32_t eliminated_slot, eliminator_slot;
    uint32_t gate_events;
    int32_t gate_number, gate_total;
    int32_t section_count, section_current;
    float section_time, section_best;
    uint32_t leader_set, leader_events;
    int32_t judge_bars[4];
    uint32_t final_events, final_shown;
    float final_scores[5];
    uint32_t discovery_events;
    int32_t discovery_found, discovery_total;
    char eliminated_name[RX_RACE_HUD_NAME], eliminator_name[RX_RACE_HUD_NAME];
    char leader_names[RX_RACE_HUD_SECTIONS][RX_RACE_HUD_NAME];
} rx_race_hud_state;
/* Caller sets size and version (1). 1 on bad arguments (output untouched), else 0. Session owner thread, between advances. */
RX_API int rx_session_race_hud(struct rx_session* session, rx_race_hud_state* out);
#ifdef __cplusplus
}
#endif
#endif
