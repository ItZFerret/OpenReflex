// build_activity_clear.cpp -- oracle adapters for core/game/build_activity_clear.c (construction unit 11, group rx_build11): the
// player's vehicle / rider release and the small record releases it makes. Verified per call by the probe cases of
// race_wave_probe.cpp (no reference world releases a vehicle). ABI glue only.
#include "oracle.h"
extern "C" {
#include "../game/build_activity_clear.h"
}
using namespace reflex_port;

#define G "rx_build11"
// 00544c80 (ECX = vector, EDX = &value)
RX_PORT(0x00544c80, PtrVec_Remove, G, 0) { rx_ptr_vec_remove(m.r[1], m.r[2]); return m.ret(0); }
// the vehicle parts: 00540300 (ECX = part; loader; RET 4) / 00540450 (ECX = part; loader, hr; RET 8) / 0053f9f0 (EAX = part)
RX_PORT(0x00540300, Part_UnmarkPackages, G, 0) { rx_part_unmark_packages(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00540450, Part_ReleasePackages, G, 0) { rx_part_release_packages(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0053f9f0, Part_Destroy, G, 0) { rx_part_destroy(m.r[0]); return m.ret(0); }
// 00540dd0 (EAX = the vehicle id; the list; RET 4) / 005408e0 (EAX = the vehicle, EDI = the list)
RX_PORT(0x00540dd0, VehicleSpec_Free, G, 0) { rx_vehicle_spec_free(m.arg(0), m.r[0]); return m.ret(4); }
RX_PORT(0x005408e0, Vehicle_Remove, G, 0) { rx_vehicle_remove(m.r[0], m.r[7]); return m.ret(0); }
// the rider gear: 0053d020 / 0053d170 (ECX = gear part) / 0053ca30 (EAX = gear part)
RX_PORT(0x0053d020, Gear_UnmarkPackages, G, 0) { rx_gear_unmark_packages(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0053d170, Gear_ReleasePackages, G, 0) { rx_gear_release_packages(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0053ca30, GearPart_Destroy, G, 0) { rx_gear_part_destroy(m.r[0]); return m.ret(0); }
// 0053ed20 (EDI = the rider manager; the entry; RET 4) / 0053ece0 (EAX = the manager; the rider id; RET 4) / 0053ebc0 (EAX = the rider, EDI = the manager)
RX_PORT(0x0053ed20, RiderEntry_Free, G, 0) { rx_rider_entry_free(m.arg(0), m.r[7]); return m.ret(4); }
RX_PORT(0x0053ece0, Rider_Release, G, 0) { rx_rider_release(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0053ebc0, Rider_Remove, G, 0) { rx_rider_remove(m.r[0], m.r[7]); return m.ret(0); }
// 00408fe0 / 0047c380 / 00691d30 (EDX = the vehicle), 0069a0c0 (EDI = the manager), 00550e10 (the player; RET 4)
RX_PORT(0x00408fe0, MudRecord_Free, G, 0) { rx_mud_record_free(m.r[2]); return m.ret(0); }
RX_PORT(0x0047c380, PoseOverride_Release, G, 0) { rx_pose_override_release(m.r[2]); return m.ret(0); }
RX_PORT(0x00691d30, VehicleEffects_Release, G, 0) { rx_vehicle_effects_release(m.r[2]); return m.ret(0); }
RX_PORT(0x0069a0c0, ParticleManager2D_Clear, G, 0) { rx_particle_manager2d_clear(m.r[7]); return m.ret(0); }
RX_PORT(0x00550e10, Player_SaveRespawnPose, G, 0) { rx_player_save_respawn_pose(m.arg(0)); return m.ret(4); }
// 00680d10 (ECX = EDX..., EDX = the vehicle; the manager; RET 4)
RX_PORT(0x00680d10, VehicleAudio_Release, G, 0) { rx_vehicle_audio_release(m.r[2], m.arg(0)); return m.ret(4); }
// 00479dd0 (ECX = entry) / 0047bbe0 (EDX = the damage manager, ESI = the vehicle) / 0054bda0 (ESI = the player)
RX_PORT(0x00479dd0, DamageEntry_Release, G, 0) { rx_damage_entry_release(m.r[1]); return m.ret(0); }
RX_PORT(0x0047bbe0, Damage_Release, G, 0) { rx_damage_release(m.r[2], m.r[6]); return m.ret(0); }
RX_PORT(0x0054bda0, Player_ReleaseObjects, G, 0) { rx_player_release_objects(m.r[6]); return m.ret(0); }
