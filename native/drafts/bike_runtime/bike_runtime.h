#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct reflex_test_bike_input {
    float throttle,reverse,front_brake,rear_brake,steer_x,steer_y,rider_x,rider_y;
    uint32_t clutch,substeps;
    float world_dt;
} reflex_test_bike_input;
typedef struct reflex_test_bike_frame {
    float positions[16],orientations[16],velocity[4],speed;
    int32_t gear;
    uint32_t steps;
} reflex_test_bike_frame;
void* reflex_test_bike_create(const void*,uint32_t);
void reflex_test_bike_destroy(void*);
uint32_t reflex_test_bike_update(void*,const reflex_test_bike_input*);
uint32_t reflex_test_bike_frame_read(void*,reflex_test_bike_frame*);
const char* reflex_test_bike_error(void*);
uint32_t reflex_test_bike_state(void*,void*,uint32_t);
#ifdef __cplusplus
}
#endif
