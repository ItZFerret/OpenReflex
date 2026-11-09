#pragma once
#include <stdint.h>
/* Experimental gzip RXPK package with an RXM1 terrain: 2048 square world-space heights in feet. */
int fmt_modtrack_open(const char* folder);
void fmt_modtrack_clear(void);
int fmt_modtrack_active(void);
float fmt_modtrack_height(uint32_t i);
void fmt_modtrack_start(float out[16]);

int fmt_modtrack_collision_claim(uint32_t world);
uint32_t fmt_modtrack_collision_count(void);
const float* fmt_modtrack_collision_chunk(uint32_t index, uint32_t* vertices);

void fmt_modtrack_softness(uint32_t x,uint32_t z,float out[3]);

const float* fmt_modtrack_collision_anchors(uint32_t index);
void fmt_modtrack_scale_point(const float* point,const float* anchor,float scale,float out[3]);

/* Optional RXRA racing extension: source routes, never native database assets. */
int fmt_modtrack_racing(void);
int fmt_modtrack_recovery_retry(uint32_t driver,uint32_t node,float time);
const float* fmt_modtrack_gate(void);
const uint8_t* fmt_modtrack_race_stream(uint32_t* size);
/* Ordered course sectors for imported lap validation; state resets with the mod session. */
int fmt_modtrack_lap_gate(uint32_t driver,int laps,float fraction);
/* Riding layer nearest the authored route, including elevated mesh surfaces. */
float fmt_modtrack_ride_height(float x,float z,float source_y,float terrain_y,float scale);
