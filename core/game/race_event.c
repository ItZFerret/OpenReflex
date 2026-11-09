/* race_event.c -- raising the race layer's typed events ("Backwards", "Lap Completed", "Countdown Step" ...). Each raiser is a
 * template instance in the original: it asks the payload type's type_info for its name (msvcr90 type_info::_name_internal_method,
 * which caches the name in the type_info) and hands the event name and the payload (the handler arguments, by value) to the
 * event manager's dispatch (FUN_004020c0: EAX = manager, EDI = name, payload, size). (core/notes/race.md "Events") */
#include "race.h"
#include "crt.h"
#include "race_extern.h"
#include "world_veh.h"

void race_raise_event(rx_addr manager, rx_addr name, rx_addr type_info, const uint32_t* payload, int words) {
    const uint32_t size = 4u * (uint32_t)words;
    RX_FRAME_ENTER(F, 0x10);
    for (int i = 0; i < words; i++) SU32(F, 4u * (uint32_t)i, payload[i]);
    uint32_t node = 0x009e1a7c;                                   /* __type_info_root_node */
    crt_type_info_name(type_info, node);   /* type_info::_name_internal_method (IAT 0x916388) */
    /* 004020c0 (world owner; EAX = manager, EDI = name): the payload by its address; a void() event (no words) passes none,
     * `push 0; push 0` as the original's void() raisers do (00560a38, 0075c3f1) */
    RACE_CORE_CALL(2, rx_world_event_dispatch(manager, name, words ? F : 0, size));
    RX_FRAME_LEAVE(F, 0x10);
}

/* 0053a470 (ESI = manager, EDX = name, ECX = value, arg = driver; RET 4): an event void(BaseDriver*, int). */
void rx_event_driver_int(rx_addr manager, rx_addr name, rx_addr driver, uint32_t value) {
    uint32_t p[2] = {driver, value};
    race_raise_event(manager, name, 0x009cd9f8, p, 2);
}
RX_FUNCTION(0x0053a470, rx_event_driver_int)

/* 0053a6d0 (ESI = manager, EDX = name, ECX = driver, arg = activity; RET 4): void(Activity*, BaseDriver*). */
void rx_event_activity_driver(rx_addr manager, rx_addr name, rx_addr activity, rx_addr driver) {
    uint32_t p[2] = {activity, driver};
    race_raise_event(manager, name, 0x009cda18, p, 2);
}
RX_FUNCTION(0x0053a6d0, rx_event_activity_driver)

/* 005875e0 (ESI = manager, EDX = name, ECX = value, arg = activity; RET 4): void(Activity*, int). */
void rx_event_activity_int(rx_addr manager, rx_addr name, rx_addr activity, uint32_t value) {
    uint32_t p[2] = {activity, value};
    race_raise_event(manager, name, 0x009cf5b4, p, 2);
}
RX_FUNCTION(0x005875e0, rx_event_activity_int)

/* 00587d00 "Player Wins" (ESI = manager, ECX = driver, EDX = text; args: activity, value; RET 8): void(Activity*, BaseDriver*,
 * const char*, int). */
void rx_event_player_wins(rx_addr manager, rx_addr activity, rx_addr driver, rx_addr text, uint32_t value) {
    uint32_t p[4] = {activity, driver, text, value};
    race_raise_event(manager, 0x00941ca8, 0x009cf630, p, 4);
}
RX_FUNCTION(0x00587d00, rx_event_player_wins)

/* 0053a9e0 (ESI = manager, args: name, driver, c; ECX = a, EDX = b; RET 0xc): void(BaseDriver*, int, int, int). */
void rx_event_driver_3int(rx_addr manager, rx_addr name, rx_addr driver, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t p[4] = {driver, a, b, c};
    race_raise_event(manager, name, 0x009cda64, p, 4);
}
RX_FUNCTION(0x0053a9e0, rx_event_driver_3int)

/* 0053ac80 "Player Eliminated" (ESI = manager, ECX = driver, EDX = next driver, arg = activity; RET 4): void(Activity*, BaseDriver*,
 * BaseDriver*). */
void rx_event_player_eliminated(rx_addr manager, rx_addr activity, rx_addr driver, rx_addr next) {
    uint32_t p[3] = {activity, driver, next};
    race_raise_event(manager, 0x0093ef48, 0x009cdaa8, p, 3);
}
RX_FUNCTION(0x0053ac80, rx_event_player_eliminated)

/* 0053a770 (arg = manager, EDI = name, EAX = driver; RET 4): void(BaseDriver*). */
void rx_event_driver(rx_addr manager, rx_addr name, rx_addr driver) {
    uint32_t p[1] = {driver};
    race_raise_event(manager, name, 0x009cda44, p, 1);
}
RX_FUNCTION(0x0053a770, rx_event_driver)

/* 0058d7d0 (ESI = manager, args: name, player; ECX = a, EDX = b; RET 8): void(MXPlayer*, int, int). */
void rx_event_player_2int(rx_addr manager, rx_addr name, rx_addr player, uint32_t a, uint32_t b) {
    uint32_t p[3] = {player, a, b};
    race_raise_event(manager, name, 0x009cbe2c, p, 3);
}
RX_FUNCTION(0x0058d7d0, rx_event_player_2int)

/* 0053aa40 (ESI = manager, stack driver, xmm0 = time; RET 4): the "Best Lap" event void(BaseDriver*, float) (the name at 0x93ef0c). */
void rx_event_best_lap(rx_addr manager, rx_addr driver, float t) {
    uint32_t p[2] = {driver, rx_fb(t)};
    race_raise_event(manager, 0x0093ef0c, 0x009cda88, p, 2);
}
RX_FUNCTION(0x0053aa40, rx_event_best_lap)

/* 00554c40 (ESI = manager, ECX = the jump record, arg = player; RET 4): the "VehicleLanded" event
 * void(MXPlayer*, const VehicleJumpRecord&) (type_info 0x9ce4b0; payload {player, record}). Newly reached once the AI rides. */
void rx_event_vehicle_landed(rx_addr manager, rx_addr player, rx_addr record) {
    uint32_t p[2] = {player, record};
    race_raise_event(manager, 0x009407c4, 0x009ce4b0, p, 2);
}
RX_FUNCTION(0x00554c40, rx_event_vehicle_landed)
