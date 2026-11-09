#ifndef RX_CAREER_SERIES_H
#define RX_CAREER_SERIES_H
#include "rx_career_supercross.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Career series admission by the original's rules (2026-10-26, w-career). Additive: every existing struct, enum and capability
 * bit keeps its value.
 *
 * Which series the career API admits is no longer a list of ids. A series is admitted when the original would play it from the
 * Race & Ride events page and the core has the code it runs:
 *   - SeriesOwner "Tour" (RaceAndRideEvents.lua queries Tour rows; the API configures Tour mode);
 *   - its store product's parent is not 2904: RaceAndRide.lua sends that product to the challenges page, which starts the
 *     series as a plain Free Ride event without the series manager (series 0, 1, 2);
 *   - a SeriesEventRules row and at least one SeriesEventLocations row (LoadSeries reads them);
 *   - the series event object MXSeriesManager selects for the format (005b6900: Moto -> manager + 0x288, Heat -> + 0x194,
 *     Freestyle -> + 0x380, every other format -> the Single event + 0x46c) has every virtual slot ported to the core;
 *   - the EventType's ActivityName names an activity of the original's enum (0x009d3c00).
 * Eligibility (rx_career_series_info.eligible) adds the profile: the product and its parents unlocked (IsProductLocked), the
 * equipped vehicle's type in VehicleClassType of the series' class (RaceAndRideEvents:OnAcceptSeries), and the presenter
 * capabilities the race will request (below). */

/* Presenter capabilities, version 2. The HUD bit of a series is the bit of the route the original builds for its first race
 * (005ce910: "Hud" + the activity name, "Outrun" for activity mode 8, + "SinglePlayer"):
 *   HudNationalSinglePlayer 1, HudWaypointRaceSinglePlayer 256, HudSupercrossSinglePlayer 512, HudPowercrossSinglePlayer 1024,
 *   HudTerracrossSinglePlayer 2048, HudFreestyleSinglePlayer 4096, HudSectionRaceSinglePlayer 8192, HudOutrunSinglePlayer 16384.
 * The format bits are the HUD elements a format's activity mode needs on top of its HUD layer (the authored layers carry them:
 * the eliminator countdown and positions of mode 5, the overtake count of mode 6). */
#define RX_CAREER_PRESENT_TERRACROSS_HUD     2048u
#define RX_CAREER_PRESENT_FREESTYLE_HUD      4096u
#define RX_CAREER_PRESENT_SECTION_RACE_HUD   8192u
#define RX_CAREER_PRESENT_OUTRUN_HUD        16384u
#define RX_CAREER_PRESENT_ELIMINATOR        32768u
#define RX_CAREER_PRESENT_OVERTAKE          65536u
/* Capabilities version 3 (w-heat): the Heat format's pages (rx_career_heat.h): the round results with CONTINUE, the gate page of
 * every round, the AAA loading page between the rounds (rx_session_career_loading_enter). A Heat series needs it. */
#define RX_CAREER_PRESENT_HEAT             131072u
/* Every series needs the hub, the fades, the page wipes, the results page and the rules popup (2 + 16 + 32 + 64 + 128). Configure
 * accepts these plus at least one HUD bit; each series then needs its own HUD (and format) bit. */
#define RX_CAREER_BASE_REQUIRED_CAPABILITIES 242u
#define RX_CAREER_ALL_HUD_CAPABILITIES      (1u | 256u | 512u | 1024u | 2048u | 4096u | 8192u | 16384u)
#define RX_CAREER_ALL_PRESENTER_CAPABILITIES 262143u

#define RX_CAREER_SERIES_PRESENTATION_VERSION 1u
/* What a series needs from the presenter, and why it is not admitted when it is not. Copied values only. */
typedef struct rx_career_series_presentation {
    uint32_t size, version, series_id;
    uint32_t authored;                /* a Series row exists */
    uint32_t admitted;                /* rx_career_series_info.supported */
    uint32_t format;                  /* MXSeriesFormat: 0 Moto, 1 Heat, 2 SingleEvent, 3 Freestyle, 4 Overtake, 5 Eliminator,
                                         6 SpeednStyle, 7 Outrun, 8 Faceoff, 9 SectionRace (the names at 0x009d2380); 10 unknown */
    uint32_t event_vtable;            /* the series event class the manager selects for the format */
    uint32_t activity_type;           /* the activity enum of EventType.ActivityName; 0x3e = none */
    uint32_t activity_mode;           /* the activity mode of the first race: 1 race, 5 eliminator, 6 overtake, 7 speed and style,
                                         8 outrun (005b04e0) */
    uint32_t required_capabilities;   /* base + the HUD bit + the format bit */
    int32_t duration;                 /* SeriesEventRules: Duration (laps; minutes for the timed activities) */
    int32_t riders;                   /* SeriesEventRules: NumRiders */
    int32_t locations;                /* SeriesEventLocations rows (events of the series) */
    int32_t point_id;                 /* Series.SeriesPointId */
    char hud_route[64];               /* the route above */
    char ai_format[16];               /* Series.SeriesAIFormat: Matched, Fixed, Rival */
    char reason[128];                 /* empty when admitted */
} rx_career_series_presentation;
/* 0 ok (a missing row answers authored 0), RX_CAREER_INVALID on a bad size / version, the usual entry results otherwise. */
RX_API int rx_session_career_series_presentation(struct rx_session*, uint64_t generation, uint32_t series_id,
                                                 rx_career_series_presentation* out);
#ifdef __cplusplus
}
#endif
#endif
