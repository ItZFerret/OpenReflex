/* rx_champ_weekend.h -- RESTORED CONTENT: the championship weekend (api/champ_weekend_session.inc, game/restored_champ_weekend.c).
 *
 * Not part of the original PC game. The original ships the championship sub-event classes (MXActivityRaceChampIntro / Practice /
 * Heat / MainEvent / LCQ, verified ports in game/race_champ.c) but no data or flow that reaches them. This API runs them as a weekend:
 * each stage is one session (one event per session, as every exhibition event): the host loads the discipline's exhibition event
 * (National: EventType "National" at a National location; Supercross: "Supercross" at an SX location), calls
 * rx_session_champ_stage_apply before the first advance, races, and at the activity's end (rx_session_get_state FINISHED) calls
 * rx_session_champ_stage_record, which adds the stage's places, gates and points to the weekend and moves to the next stage the local
 * rider rides. The weekend struct is host memory and outlives the sessions.
 *
 * TIMED RACES (PORT ADDITION, version 4; game/restored_champ_weekend.c): a National / Supercross stage runs on a clock plus laps, as real
 * motocross does ("15:00 + 1"): stage_minutes[s] > 0 is the clock and the stage's `laps` the extra laps after it; stage_minutes[s] == 0
 * is a plain lap race of `laps` (the version 3 behaviour, bit for bit). While the clock runs the race's lap total (A+0xff070) is
 * RX_CHAMP_TIMED_LAPS; when the activity clock (A+0x294, InProgress time) reaches the minutes, the lap total becomes the leader's
 * completed laps + 1 + laps (the leader's next crossing starts the extra laps; with 0 extra laps, a practice, it is the flag). Once the
 * leader has taken the flag (a driver reached the lap total) every other rider finishes at its next crossing of the line (the race's
 * vf61 "has the driver finished" answers yes for a driver whose lap count rose after the flag); the race waits for them (the
 * original ends it when the humans are done) at most 1.5 of the leader's average lap, 30 s at least, after the flag. A timed stage's places: the finished
 * riders by laps completed (more first), then by their race time at the line; then any rider still on track in the game's order (the
 * original's end of race projects the AI behind the last human). Waypoint (gate rounds) and
 * Freestyle (judged runs on the event's own session time) are not timed.
 * Defaults (rx_champ_weekend_begin; the host may change any with rx_champ_weekend_set_timing before the stage is applied):
 *   National   PRACTICE 10:00 + 0, HEAT 5:00 + 1, MOTO 1 15:00 + 1, MOTO 2 15:00 + 1
 *   Supercross PRACTICE 6:00 + 0, HEAT 4:00 + 1, LCQ 3:00 + 1, MAIN 10:00 + 1
 *   Waypoint / Freestyle: laps only (the event's own rounds / session time)
 *
 * National (RX_CHAMP_NATIONAL; the original names no National LCQ class):
 *   PRACTICE NationalChampPractice 0x22  the event's own gates
 *   HEAT     NationalChampHeat     0x23  gates by the practice's best laps
 *   MOTO 1   NationalChampMainEvent 0x24  gates by the heat's places; SeriesPoints (SeriesPointId 1: 12 .. 1)
 *   MOTO 2   NationalChampMainEvent2 0x25 gates by moto 1's places; SeriesPoints
 * Supercross (RX_CHAMP_SUPERCROSS; the qualification of the original's Heat format, MXSeriesEventHeat::vf13 005adde0):
 *   PRACTICE SupercrossChampPractice 0x26 the event's own gates
 *   HEAT     SupercrossChampHeat     0x27  the whole field, gates by the practice's best laps: 1-H to the MAIN, H+1 .. N-O to the LCQ,
 *                                          the last O out (12 riders: 1-5 / 6-11 / 12, the original's)
 *   LCQ      SupercrossChampLCQ      0x29  heat H+1 .. N-O, gates by the heat's places; 1-T to the MAIN. Without the local rider it is
 *                                          not raced: decided by the heat's order (the original decides it by the table order then)
 *   MAIN     SupercrossChampMainEvent 0x28 heat 1-H then LCQ 1-T (gates in that order); SeriesPoints. The local rider out of it:
 *                                          the weekend ends, `eliminated` (the original's "Failed")
 *   FIELD SIZE (PORT ADDITION: a weekend of 2 .. 32 riders, the field the host loads each stage's event with, rx_event_spec.riders; the
 *   riders 13+ are the rider cap's, rider_cap.h). The split scales with the heat's field N (the riders the heat placed) by the original's
 *   12-rider proportions (champ_split in the .inc): H = (5 N + 6) / 12, O = (N + 6) / 12, the LCQ's field L = N - O - H, T = (L + 1) / 3
 *   (at least 1; an LCQ whose riders would all go through is not run: H += L). N = 12 gives the original's split, so a 12-rider weekend
 *   is the one before the field size, byte for byte:
 *       N    2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31  32
 *       H    2   1   2   2   3   3   3   4   4   5   5   5   6   6   7   7   8   8   8   9   9  10  10  10  11  11  12  12  13  13  13
 *       LCQ  0   2   2   3   2   3   4   4   5   5   6   7   7   8   8   9   8   9  10  10  11  11  12  13  13  14  14  15  14  15  16
 *       T    0   1   1   1   1   1   1   1   2   2   2   2   2   3   3   3   3   3   3   3   4   4   4   4   4   5   5   5   5   5   5
 *       out  0   0   0   0   1   1   1   1   1   1   1   1   1   1   1   1   2   2   2   2   2   2   2   2   2   2   2   2   3   3   3
 *       MAIN 2   2   3   3   4   4   4   5   6   7   7   7   8   9  10  10  11  11  11  12  13  14  14  14  15  16  17  17  18  18  18
 *   At 24 the heat sends what the original's 24-rider Heat format sends from its two heats of 12 (10 to the main, 12 to the LCQ, 2 out);
 *   the LCQ keeps the 12-rider weekend's one-in-three transfer. SeriesPoints has places 1 .. 12: places 13+ of a main score 0.
 * Riders out of a stage are not Active in the Player List and request no activity (005a8af0's convention); the gates are the Player
 * List's StartZone (the column the career's gate pick writes, 005a9e00). The overall: points, then the places from the last stage back.
 * Waypoint (RX_CHAMP_WAYPOINT): INTRO WaypointRaceChampIntro 0x21, PRACTICE 0x30, MAIN 0x31 (SeriesPoints); the event's own rounds.
 * Freestyle (RX_CHAMP_FREESTYLE; the event's own session time): PRACTICE FreestyleChampPractice 0x3a, MAIN 1 FreestyleChampMainEvent
 *   0x3b, MAIN 2 FreestyleChampMainEvent2 0x3c. A stage's places are the original's own order (the standings +0x2d0 the game sorts by the
 *   judged TotalScore, P+0xcec); the weekend's standings are the two mains' TotalScores summed (`judged`), ties to the better MAIN 2 place.
 * RaceChampIntro (0x1f) is not a stage (see the .inc).
 *
 * Return values: 0 ok; 1 invalid session / argument or a core fault (the session is then in RX_SESSION_ERROR); 2 wrong state (last_error). */
#ifndef RX_CHAMP_WEEKEND_H
#define RX_CHAMP_WEEKEND_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif

#define RX_CHAMP_WEEKEND_VERSION 4u
#define RX_CHAMP_MAX_STAGES 8
#define RX_CHAMP_MAX_RIDERS 32     /* the field size the struct can hold (a session's field is the activity's driver count) */
#define RX_CHAMP_TIMED_LAPS 0x60   /* the race's lap total while a stage's clock runs (under the lap counter's 0x61 cap) */
#define RX_CHAMP_MAX_MINUTES 60
#define RX_CHAMP_MAX_LAPS 30

enum { RX_CHAMP_NATIONAL = 0, RX_CHAMP_SUPERCROSS = 1, RX_CHAMP_WAYPOINT = 2, RX_CHAMP_FREESTYLE = 3 };
enum { RX_CHAMP_FIELD_ALL = 0, RX_CHAMP_FIELD_LCQ = 1, RX_CHAMP_FIELD_MAIN = 2 };
enum { RX_CHAMP_GRID_EVENT = 0, RX_CHAMP_GRID_BEST_LAP = 1, RX_CHAMP_GRID_PLACE = 2, RX_CHAMP_GRID_MAIN = 3 };

typedef struct rx_champ_stage {
    char label[16];           /* "PRACTICE", "MOTO 1", "LCQ" ... (restoration labels) */
    char activity[32];        /* the original's ActivityName */
    int32_t activity_type;    /* its enum, filled by rx_session_champ_stage_apply (-1 before) */
    int32_t laps;             /* the stage's NumLaps (the original's debug override [0x00af6564] still wins when set); timed: the extra laps */
    int32_t scored;           /* 1: places give SeriesPoints */
    int32_t raced;            /* 1: rx_session_champ_stage_record took its places */
    int32_t decided;          /* 1: not raced, its places decided by the previous stage (an LCQ without the local rider) */
    int32_t field;            /* RX_CHAMP_FIELD_* */
    int32_t grid;             /* RX_CHAMP_GRID_* */
    int32_t grid_from;        /* the stage the grid comes from (-1 none) */
} rx_champ_stage;

typedef struct rx_champ_rider {
    char name[16];            /* the number plate's name (P+0x200) */
    char key[32];             /* the Player List's PlayerName (P+0x14): the rider's identity across the stages */
    int32_t local;            /* 1: the local player */
    int32_t points;           /* SeriesPoints of the scored stages (race disciplines) */
    float total_score;        /* Freestyle: the judged TotalScore (P+0xcec) summed over the scored stages */
    int32_t place[RX_CHAMP_MAX_STAGES];   /* 1-based place per stage, 0 = not in it */
    float best_lap[RX_CHAMP_MAX_STAGES];  /* seconds, 0 = none */
    int32_t gate[RX_CHAMP_MAX_STAGES];    /* 1-based start slot taken (P+0x1dc + 1), 0 = not raced */
    float score[RX_CHAMP_MAX_STAGES];     /* Freestyle: the stage's judged TotalScore (P+0xcec), 0 elsewhere */
} rx_champ_rider;

typedef struct rx_champ_weekend {
    uint32_t size, version;   /* sizeof / RX_CHAMP_WEEKEND_VERSION, set by begin */
    int32_t discipline;       /* RX_CHAMP_NATIONAL / RX_CHAMP_SUPERCROSS */
    int32_t stage;            /* the next stage to run */
    int32_t stage_count;
    int32_t finished;         /* 1 after the last stage (or the local rider's elimination) */
    int32_t series_point_id;  /* the SeriesPoints set (1) */
    int32_t eliminated;       /* 1: the local rider did not make the main event */
    int32_t judged;           /* 1: the standings are judged scores (Freestyle), 0: SeriesPoints by place */
    rx_champ_stage stages[RX_CHAMP_MAX_STAGES];
    int32_t rider_count;
    rx_champ_rider riders[RX_CHAMP_MAX_RIDERS];
    int32_t overall[RX_CHAMP_MAX_RIDERS];   /* rider indices, best first */
    /* version 4: TIMED RACES */
    int32_t stage_minutes[RX_CHAMP_MAX_STAGES];               /* the stage's clock in minutes, 0 = laps only */
    int32_t rider_laps[RX_CHAMP_MAX_RIDERS][RX_CHAMP_MAX_STAGES];   /* laps completed per rider and stage (0 = none / not in it) */
} rx_champ_weekend;

/* The running stage's clock (rx_session_champ_clock). */
typedef struct rx_champ_clock {
    uint32_t size;            /* sizeof, set by the caller */
    int32_t timed;            /* 1: the session runs a timed stage */
    float seconds;            /* the clock's length */
    float elapsed;            /* the activity's InProgress clock (A+0x294) */
    int32_t expired;          /* 1: the clock ran out; num_laps is final */
    int32_t flag;             /* 1: the leader took the flag; every other rider finishes at its next crossing */
    int32_t extra_laps;       /* the laps after the clock */
    int32_t num_laps;         /* the race's lap total now (RX_CHAMP_TIMED_LAPS while the clock runs) */
    float expired_at, flag_at;   /* the activity clock at the expiry / the flag (0 before) */
} rx_champ_clock;

/* A new weekend of `discipline` (no session needed). 0 ok, 1 bad argument / unsupported discipline. */
RX_API int rx_champ_weekend_begin(rx_champ_weekend* w, int32_t discipline);
/* The loaded exhibition event (rx_session_event_load done, no frame advanced yet) becomes the weekend's current stage: its field and
 * its grid from the stages recorded so far. */
RX_API int rx_session_champ_stage_apply(struct rx_session* s, rx_champ_weekend* w);
/* A stage's clock (minutes 0 .. RX_CHAMP_MAX_MINUTES, 0 = laps only; National / Supercross only) and laps (1 .. RX_CHAMP_MAX_LAPS
 * without a clock; 0 .. RX_CHAMP_MAX_LAPS extra laps with one). Before the stage is applied. 0 ok, 1 bad argument / not timeable. */
RX_API int rx_champ_weekend_set_timing(rx_champ_weekend* w, int32_t stage, int32_t minutes, int32_t laps);
/* FIELD SIZE: the Supercross split of a heat of `field` riders (1 .. RX_CHAMP_MAX_RIDERS; see the table above): heat 1 .. *to_main to
 * the main, *to_main + 1 .. *lcq_last to the LCQ (none when equal), LCQ 1 .. *lcq_to_main to the main. No weekend or session needed.
 * 0 ok, 1 bad argument. */
RX_API int rx_champ_weekend_split(int32_t field, int32_t* to_main, int32_t* lcq_last, int32_t* lcq_to_main);
/* The running stage's clock (any time between frames; timed 0 when the session runs no timed stage). 0 ok, 1 bad argument. */
RX_API int rx_session_champ_clock(struct rx_session* s, rx_champ_clock* out);
/* At the stage's end (the activity in WindDown / Post, or every driver finished): its places into the weekend, the points, the next
 * stage the local rider rides; `finished` after the last. */
RX_API int rx_session_champ_stage_record(struct rx_session* s, rx_champ_weekend* w);
/* The local rider left the stage before its end (a quit / retire, any time after the apply): the stage is recorded with the local rider
 * last and everybody else in the current order, and the weekend moves on as after a record. */
RX_API int rx_session_champ_stage_retire(struct rx_session* s, rx_champ_weekend* w);


#ifdef __cplusplus
}
#endif
#endif
