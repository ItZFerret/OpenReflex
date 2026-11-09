/* MTX race smoke: actual local input, native opponents, lap timing and results. */
#include "rx_api.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static float wrap_angle(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
struct autopilot { float stuck_clock, reverse_clock; };
static void autopilot_pad(struct rx_session* s, struct autopilot* pilot, rx_pad* pad) {
    rx_bike_state bike; float line[64 * 3];
    const int points = rx_session_bike(s, &bike) == 0 ? rx_session_race_line(s, line, 64) : 0;
    if (points < 2) return;
    const float x = bike.position[0][0], z = bike.position[0][2];
    int nearest = 0; float best = 1e30f;
    for (int i = 0; i < points && i < 6; i++) { const float dx = line[3 * i] - x, dz = line[3 * i + 2] - z, d = dx * dx + dz * dz; if (d < best) { best = d; nearest = i; } }
    const float look = 18.0f + bike.speed * 0.6f;
    int target = nearest;
    for (int i = nearest; i < points; i++) { const float dx = line[3 * i] - x, dz = line[3 * i + 2] - z; target = i; if (dx * dx + dz * dz >= look * look) break; }
    const float tx = line[3 * target] - x, tz = line[3 * target + 2] - z;
    const float* q = bike.orientation[0];
    const float fx = 2.0f * (q[0] * q[2] + q[3] * q[1]), fz = 1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]);
    const float error = wrap_angle(atan2f(tx, tz) - atan2f(fx, fz));
    float steer = error * 2.2f; if (steer > 1.0f) steer = 1.0f; if (steer < -1.0f) steer = -1.0f;
    const float speed = bike.speed, turn = fabsf(error);
    const float desired = turn > 1.2f ? 8.0f : turn > 0.6f ? 14.0f : turn > 0.3f ? 20.0f : 30.0f;
    if (speed < 1.0f) pilot->stuck_clock += 1.0f / 60.0f; else pilot->stuck_clock = 0.0f;
    if (pilot->stuck_clock > 4.0f) { pilot->reverse_clock = 1.5f; pilot->stuck_clock = 0.0f; }
    if (pilot->reverse_clock > 0.0f) { pilot->reverse_clock -= 1.0f / 60.0f; pad->left_trigger = 255; pad->lx = (int16_t)(-steer * 32000.0f); }
    else {
        pad->lx = (int16_t)(steer * 32000.0f);
        if (speed < desired) pad->right_trigger = 255; else if (speed > desired + 6.0f) pad->left_trigger = 160; else pad->right_trigger = 120;
    }
}

int main(int argc,char**argv) {
    if(argc!=5)return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    rx_session* s=rx_session_create(argv[1]);if(!s)return 3;
    if(rx_session_mount_level(s,"NAT_Concord",argv[2]) || rx_session_set_map_scale(s,atof(argv[4])))return 4;
    rx_event_spec e;rx_session_event_default(&e);
    snprintf(e.event_type,sizeof e.event_type,"%s",argv[3]);e.event_type_id=!strcmp(argv[3],"Supercross")?8:10;
    strcpy(e.location,"NAT_Concord");e.location_id=-1;e.track_id=0;e.vehicle_class=2;e.riders=4;e.ai_difficulty=50;e.duration=1;strcpy(e.duration_type,"Laps");e.event_activation[0]=0;
    if(rx_session_event_load(s,&e)){fprintf(stderr,"%s\n",rx_session_last_error(s));return 5;}
    struct autopilot pilot={0};int countdown=0,started=0,finished=0;
    for(int f=0;f<72000;f++) {
        rx_race_state r={0};rx_session_race(s,&r);rx_pad pad={0};
        if(r.valid&&r.state==1&&r.countdown_step>=1)countdown=1;
        if(r.valid&&r.state==2){started=1;autopilot_pad(s,&pilot,&pad);}
        if(rx_session_set_input(s,(uint8_t[256]){0},&pad,1)||rx_session_advance_dt(s,1.f/60)){fprintf(stderr,"frame %d %s\n",f,rx_session_last_error(s));return 6;}
        rx_session_race(s,&r);
        if(f%3600==0)printf("frame %d state %d lap %d progress %.3f time %.2f\n",f,r.state,r.laps_completed,r.lap_progress,r.race_time);
        if(r.valid&&r.state>=3&&r.laps_completed==1) {
            rx_driver_result results[32];int n=rx_session_standings(s,results,32),local=0;
            for(int i=0;i<n;i++)if(results[i].local) local=results[i].finished&&results[i].best_lap>20&&isfinite(results[i].race_time);
            finished=n==4&&local;printf("RESULT human finished: %d standings, lap %.2f, race %.2f\n",n,r.best_lap,r.race_time);break;
        }
    }
    printf("RESULT countdown %d start %d finish %d\n",countdown,started,finished);
    rx_session_destroy(s);return countdown&&started&&finished?0:7;
}
