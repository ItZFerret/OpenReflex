/*Actual00578d20 ordering; constructors share the canonical driver kernel.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
static void bind_rule(rx_addr object,rx_addr driver){uint32_t arg=driver;act_vcall(object,4,1,&arg);}
void rx_waypoint_activity_vf47(rx_addr activity,rx_addr driver){
 ACT_FRAME(F,0,1);rx_addr object;
 if(U32(U32(activity,0x234),0x22144)!=1){ACT_CALLR(object,0,rx_waypoint_get_free_ride_rule(driver));bind_rule(object,driver);}
 ACT_CALLR(object,0,rx_waypoint_get_advance_rule(driver));bind_rule(object,driver);
 ACT_CALLR(object,0,rx_waypoint_get_avoid_wreck(driver));bind_rule(object,driver);
 ACT_CALLR(object,0,rx_waypoint_get_suck_racing(driver));bind_rule(object,driver);
 ACT_CALLR(object,0,rx_waypoint_get_air_leveling(driver));bind_rule(object,driver);
 ACT_CALLR(object,0,rx_waypoint_get_fix_forward(driver));bind_rule(object,driver);
 ACT_CALLR(object,0,rx_waypoint_get_brake_turn(driver));bind_rule(object,driver);
 ACT_CALLR(object,0,rx_waypoint_get_racing_lines(driver));bind_rule(object,driver);
 ACT_CALLR(object,0,rx_waypoint_get_waypoint_navigation(driver));bind_rule(object,driver);
 ACT_CALLR(object,0,rx_waypoint_get_avoid_objects(driver));bind_rule(object,driver);
 ACT_CALLR(object,0,rx_waypoint_get_cornering(driver));bind_rule(object,driver);
 ACT_CALLR(object,0,rx_waypoint_get_avoid_flip_back(driver));bind_rule(object,driver);
 ACT_END(F);
}
RX_FUNCTION(0x00578d20,rx_waypoint_activity_vf47)
