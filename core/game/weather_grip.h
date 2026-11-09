/* weather_grip.h -- PORT ADDITION (not original game code): WEATHER GRIP, rain and snow taking grip off the tyres (2026-11-07).
 *
 * The client's WEATHER (the race / Free Ride setup, Unity Rendering/ReflexWeather.cs) wets the ground or snows it over; this makes the tyres
 * feel it. One factor for every vehicle of the event (MX, ATV, UTV, buggy, trucks; the players' and the computer riders'), applied where
 * each class's tyre function reads its two static grip coefficients (the kind-3 tyre parameters StaticMuRoll and StaticMuSideSlip,
 * blended by the ground value under the tyre): the indexed-curve value times the factor, before it scales the load into the rolling and
 * lateral friction limits. The sliding limits, the demand circle and the rear brake blend are all made from those two, so the whole
 * friction circle shrinks by the factor; the dynamic multipliers (the share kept while sliding) stay the file's.
 *   bike_tire.c 00447f10 (MX)     vehicle_atv_physics_tire.c 0049a340 (ATV)     tyre_prefix.c 004d50e0's friction phase (UTV)
 *   buggy_tyre.c 004bad20's (buggy)     truck_tyre.c 00457070's (trucks)
 * The client computes the factor (ReflexWeather.GripFactor: 1 - max(rain loss x wetness, snow loss x snow cover) x intensity; rain 15 %,
 * snow 30 % at HEAVY) and sets it between frames (rx_session_set_weather_grip); DYNAMIC follows the ground as it wets / dries / melts.
 * Factor 1 (CLEAR, the default, every session's start: the session reset restores these statics) = g_rx_weather_grip_on 0: each hook is
 * one test of a static that is 0 and the game runs bit for bit as without this file. Offline only (the client keeps 1 online). */
#pragma once

extern int   g_rx_weather_grip_on;   /* a factor other than 1 is set */
extern float g_rx_weather_grip;      /* the factor (1: the original's grip) */

#define RX_WEATHER_GRIP_MIN 0.1f
#define RX_WEATHER_GRIP_MAX 1.5f
/* 0 ok (the factor set; 1 switches the hooks off), 2 refused (NaN or outside RX_WEATHER_GRIP_MIN .. MAX) */
int rx_weather_grip_set(float factor);
