/* weather_grip.c -- PORT ADDITION (not original game code): WEATHER GRIP, see weather_grip.h. */
#include "weather_grip.h"

int   g_rx_weather_grip_on = 0;
float g_rx_weather_grip = 1.0f;

int rx_weather_grip_set(float factor) {
    if (!(factor >= RX_WEATHER_GRIP_MIN && factor <= RX_WEATHER_GRIP_MAX)) return 2;   /* (NaN fails both) */
    g_rx_weather_grip = factor;
    g_rx_weather_grip_on = factor != 1.0f;
    return 0;
}
