/* phys_studio.h -- PHYSICS STUDIO (port addition, not original code; core/plan/PHYSICS_STUDIO.md): player-made changes to the
 * vehicle parameter files and the terrain grip table, after Open Unleashed's Physics Studio.
 *
 * What it changes, and where:
 *   - the registered vehicle parameters (the 108 / 103 / 98 records of 004392f0 / 00489cb0 / 004c9d40 / 004af510): written into the
 *     simulation's parameter block P = S + 0x2f10 right after the parameter file is read (0043a3f0 / 0048b5d0 / 004cb360 / 004b0b30 /
 *     0044f690), so the masses (0043b750 / 0048c750) and the garage tuning (0045f270 -> vf35) are computed from them as the original
 *     computes them from the file's values. Only the local players' vehicles (a spec with a controller: 00543020 >= 0) get them,
 *     unless the config says all_vehicles (the computer riders too).
 *   - the terrain grip table of 00461680 (layer 0: grip per material) for the local vehicles, or for every vehicle when the config
 *     says so (grip_global); the drag factors of 00461760 (layer 1) for everyone (that function has no vehicle).
 * Nothing is active until a session sets a config (rx_session_phys_set): with none, every site below is one test of a static that
 * is 0 and the game runs exactly as without this file. */
#pragma once
#include "rx_game.h"
#include <stdint.h>

struct fmt_install;

enum {
    RX_PS_MX125, RX_PS_MX250, RX_PS_MX250F, RX_PS_MX450F, RX_PS_ATV, RX_PS_UTV, RX_PS_BUGGY, RX_PS_TRUCK2WD, RX_PS_TRUCK2WDLITE,
    RX_PS_CLASSES
};
#define RX_PS_MAX_PARAMS 108
#define RX_PS_MAX_COMPS  22      /* a curve: 10 y, 10 x, the two end slopes */
#define RX_PS_GRIP 6             /* 00461680's materials: 0, 1, 2 (and every other code), 3, 4, 5 */
#define RX_PS_DRAG 4             /* 00461760's materials: 0, 2, 3, every other code */

/* the hooks (each returns at once while no config is set) */
extern int g_rx_ps_armed;        /* a config is set, or live tuning was asked for: the factory tracks the local vehicles */
extern int g_rx_ps_grip_on;      /* a grip or drag value is overridden */
void  rx_ps_params_loaded(rx_addr P, int cls);              /* after a parameter file's apply, P = S + 0x2f10 */
/* PORT ADDITION (LEADERBOARDS C.1): the class (RX_PS_*) whose parameter file filled the simulation S's block S + 0x2f10, armed or
 * not (the class is recorded at every params_loaded); -1 unknown */
int   rx_ps_class_of_sim(rx_addr S);
void  rx_ps_vehicle_begin(rx_addr spec);                    /* VehicleFactory_CreateVehicle 00542b90 entry */
void  rx_ps_vehicle_end(rx_addr spec);                      /* its end (spec +0xc4 = the vehicle) */
float rx_ps_grip(uint32_t material, rx_addr vehicle, float k);   /* 00461680's material value k */
float rx_ps_drag(uint32_t material, float k);                    /* 00461760's factor k */
int   rx_ps_records_off(void);
void  rx_ps_guard(void);                                   /* before each frame: a local vehicle the config made diverge gets the file's values back */                              /* a config with changes is set: no offline records / highlights */

/* ALIVE CLASSES (port addition, core/plan/ALIVE_BIKES.md): a motorcycle parameter file of another game (MX vs ATV Alive's mx50_vehicle /
 * mx650f_vehicle, the same schema as the four mx*_vehicle files) written over a vehicle's own class file right after it is read (the
 * same place and the same record layout as the original's reader 0043a560 fills: the file's registered names, case-insensitive; a
 * name the file lacks keeps the class file's value; names the original does not register are ignored), for the LOCAL players' vehicles
 * of one VehicleModelId. The vehicle then reports its own physics class (rx_ps_class_of_sim: RX_VP_MX50 / RX_VP_MX650F). Nothing is
 * active until a session sets one (rx_session_vehicle_physics). */
enum { RX_VP_MX50 = RX_PS_CLASSES, RX_VP_MX650F, RX_VP_ATV1000, RX_VP_CLASS_END };   /* 9, 10 motorcycles (the mx table); 11 an ATV (the atv table) */
#define RX_VP_MAX 4
int   rx_vp_set(int32_t vehicle_model, int32_t cls, const uint8_t* bxml, uint32_t size, char* report, uint32_t cap);   /* records read, < 0 refused */
void  rx_vp_clear(void);
int   rx_vp_set_nodes(int32_t vehicle_model, const char* text);   /* the bike's own hierarchy rest locals (after rx_vp_set); nodes read, < 0 none */
extern int g_rx_vp_on;   /* an Alive class is set: the factory tracks the vehicles being made */

/* the session side (api/phys_studio_session.inc) */
int   rx_ps_catalog_build(struct fmt_install* in);          /* 0 ok; reads the nine parameter files with the format library */
int   rx_ps_catalog_text(char* out, uint32_t cap, uint32_t* required);
uint32_t rx_ps_param_set_hash(void);
int   rx_ps_set_text(const char* text, char* report, uint32_t cap);   /* replaces the config; values applied, or < 0 */
int   rx_ps_get_text(char* out, uint32_t cap, uint32_t* required);
int   rx_ps_live_text(char* out, uint32_t cap, uint32_t* required);   /* the local vehicles' parameter values now */
void  rx_ps_arm(int on);
int   rx_ps_active(void);                                  /* 1: a config with changes; | 2: the guard restored a vehicle */
int   rx_ps_apply_live(void);                               /* the running local vehicles (MX / ATV): vehicles updated */
void  rx_ps_reset(void);
