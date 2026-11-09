/* race_freestyle_rules.h -- the Freestyle driver's rule and behaviours (race_freestyle_rules.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_crowd_region_contains(rx_addr region, rx_addr point);    /* 006a6e90 (EAX = region, ECX = point) -> EAX */
void     rx_rule_in_crowd_vf1(rx_addr rule, rx_addr driver);         /* 0051bc50 RuleInFreestyleCrowd::vf1 */
void     rx_rule_in_crowd_vf5(rx_addr rule, float dt);               /* 0051bc70 RuleInFreestyleCrowd::vf5 */
void     rx_rule_set_enabled(rx_addr rule, uint32_t enabled);        /* 00519970 RuleBackwards::vf4 */
void     rx_roam_track_vf3(rx_addr b);                               /* 0052b620 BehaviorRoamTrack::vf3 */
void     rx_roam_track_vf6(rx_addr b);                               /* 0052b640 BehaviorRoamTrack::vf6 */
void     rx_behavior_enable_reset(rx_addr b, uint32_t enabled);      /* 00530fd0 (shared vf7) */
void     rx_avoid_wreck_vf7(rx_addr b, uint32_t enabled);            /* 005223c0 BehaviorAvoidWreck::vf7 */
#ifdef __cplusplus
}
#endif
