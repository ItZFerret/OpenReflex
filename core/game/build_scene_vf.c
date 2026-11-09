/* build_scene_vf.c -- the constant virtual functions of the Entity classes (type ids and similar: `mov eax, imm; ret`), registered so that
 * the scene manager and other units can dispatch them without the original code. Generated from the vtables of every Entity class in the
 * RTTI (core/notes/construction.md section 12); one function per original address, named after the class' slot. */
#include "build_scene_int.h"

static uint32_t vf_00402050(void) { return 0x1072; }   /* MXActivityActivationArea::vf22 */
RX_FUNCTION(0x00402050, vf_00402050)

static uint32_t vf_00402cb0(void) { return 0xa; }   /* BehaviorBlueYellowFlag::vf5 */
RX_FUNCTION(0x00402cb0, vf_00402cb0)

static uint32_t vf_00402e00(void) { return 0x1074; }   /* MXRaceLine::vf22 */
RX_FUNCTION(0x00402e00, vf_00402e00)

static uint32_t vf_00402ed0(void) { return 0x1013; }   /* OpencrossStartEntity::vf22 */
RX_FUNCTION(0x00402ed0, vf_00402ed0)

static uint32_t vf_00402ee0(void) { return 0x1012; }   /* SupermotoStartEntity::vf22 */
RX_FUNCTION(0x00402ee0, vf_00402ee0)

static uint32_t vf_00402ef0(void) { return 0x1014; }   /* FreeRideStartEntity::vf22 */
RX_FUNCTION(0x00402ef0, vf_00402ef0)

static uint32_t vf_00402f00(void) { return 0x1011; }   /* FreestyleStartEntity::vf22 */
RX_FUNCTION(0x00402f00, vf_00402f00)

static uint32_t vf_00402f10(void) { return 0x1015; }   /* FreeRideStarStartEntity::vf22 */
RX_FUNCTION(0x00402f10, vf_00402f10)

static uint32_t vf_00402f20(void) { return 0x1016; }   /* FreeRideLineStartEntity::vf22 */
RX_FUNCTION(0x00402f20, vf_00402f20)

static uint32_t vf_00402f30(void) { return 0x1017; }   /* SnakeTagStartEntity::vf22 */
RX_FUNCTION(0x00402f30, vf_00402f30)

static uint32_t vf_00402f40(void) { return 0x103c; }   /* GraffitiStartEntity::vf22 */
RX_FUNCTION(0x00402f40, vf_00402f40)

static uint32_t vf_00402f50(void) { return 0x103d; }   /* HockeyStartEntity::vf22 */
RX_FUNCTION(0x00402f50, vf_00402f50)

static uint32_t vf_00402f60(void) { return 0x103e; }   /* CaptureStartEntity::vf22 */
RX_FUNCTION(0x00402f60, vf_00402f60)

static uint32_t vf_00402f70(void) { return 0x101b; }   /* BigAirStartEntity::vf22 */
RX_FUNCTION(0x00402f70, vf_00402f70)

static uint32_t vf_00402f80(void) { return 0x101c; }   /* HillClimbStartEntity::vf22 */
RX_FUNCTION(0x00402f80, vf_00402f80)

static uint32_t vf_00402f90(void) { return 0x101d; }   /* JumperStartEntity::vf22 */
RX_FUNCTION(0x00402f90, vf_00402f90)

static uint32_t vf_00402fa0(void) { return 0x101e; }   /* PathfinderStartEntity::vf22 */
RX_FUNCTION(0x00402fa0, vf_00402fa0)

static uint32_t vf_00402fb0(void) { return 0x1023; }   /* StepUpStartEntity::vf22 */
RX_FUNCTION(0x00402fb0, vf_00402fb0)

static uint32_t vf_00402fc0(void) { return 0x1024; }   /* StepUpBronzeStartEntity::vf22 */
RX_FUNCTION(0x00402fc0, vf_00402fc0)

static uint32_t vf_00402fd0(void) { return 0x1025; }   /* StepUpSilverStartEntity::vf22 */
RX_FUNCTION(0x00402fd0, vf_00402fd0)

static uint32_t vf_00402fe0(void) { return 0x1026; }   /* StepUpGoldStartEntity::vf22 */
RX_FUNCTION(0x00402fe0, vf_00402fe0)

static uint32_t vf_00402ff0(void) { return 0x101f; }   /* StoppieStartEntity::vf22 */
RX_FUNCTION(0x00402ff0, vf_00402ff0)

static uint32_t vf_00403000(void) { return 0x1020; }   /* TargetPracticeStartEntity::vf22 */
RX_FUNCTION(0x00403000, vf_00403000)

static uint32_t vf_00403010(void) { return 0x1021; }   /* TrailBlazerStartEntity::vf22 */
RX_FUNCTION(0x00403010, vf_00403010)

static uint32_t vf_00403020(void) { return 0x1022; }   /* WheelieStartEntity::vf22 */
RX_FUNCTION(0x00403020, vf_00403020)

static uint32_t vf_00403030(void) { return 0x103f; }   /* CaptureBaseEntity::vf22 */
RX_FUNCTION(0x00403030, vf_00403030)

static uint32_t vf_00403040(void) { return 0x1010; }   /* CustomizeVehicleStartEntity::vf22 */
RX_FUNCTION(0x00403040, vf_00403040)

static uint32_t vf_004034b0(void) { return 0x1073; }   /* AIAvoidanceSphere::vf22 */
RX_FUNCTION(0x004034b0, vf_004034b0)

static uint32_t vf_004035a0(void) { return 0x1e; }   /* GeneralStorageEntity::vf22 */
RX_FUNCTION(0x004035a0, vf_004035a0)

static uint32_t vf_00403670(void) { return 0x1094; }   /* LocalizedPackageReferenceEntity::vf22 */
RX_FUNCTION(0x00403670, vf_00403670)

static uint32_t vf_00403680(void) { return 0xe; }   /* BehaviorRoamWorld::vf5 */
RX_FUNCTION(0x00403680, vf_00403680)

static uint32_t vf_00403690(void) { return 0x1086; }   /* SoundEffectZoneEntitySphere::vf22 */
RX_FUNCTION(0x00403690, vf_00403690)

static uint32_t vf_004036a0(void) { return 0x1087; }   /* SoundEffectLineZoneEntitySphere::vf22 */
RX_FUNCTION(0x004036a0, vf_004036a0)

static uint32_t vf_004036b0(void) { return 0x100e; }   /* GrassSystem::vf22 */
RX_FUNCTION(0x004036b0, vf_004036b0)

static uint32_t vf_004036c0(void) { return 0x1018; }   /* TagBallStart::vf22 */
RX_FUNCTION(0x004036c0, vf_004036c0)

static uint32_t vf_00403c00(void) { return 0x1028; }   /* MXTrigger::vf22 */
RX_FUNCTION(0x00403c00, vf_00403c00)

static uint32_t vf_00403dd0(void) { return 0x104d; }   /* MudManager::vf22 */
RX_FUNCTION(0x00403dd0, vf_00403dd0)

static uint32_t vf_00403de0(void) { return 0x1040; }   /* GraffitiCapturePoint::vf22 */
RX_FUNCTION(0x00403de0, vf_00403de0)

static uint32_t vf_00403df0(void) { return 0x1042; }   /* SectionRaceZone::vf22 */
RX_FUNCTION(0x00403df0, vf_00403df0)

static uint32_t vf_00403e00(void) { return 0x1043; }   /* WaypointRaceGate::vf22 */
RX_FUNCTION(0x00403e00, vf_00403e00)

static uint32_t vf_00403e10(void) { return 0x1044; }   /* EggHuntEggEntity::vf22 */
RX_FUNCTION(0x00403e10, vf_00403e10)

static uint32_t vf_00403e20(void) { return 0x106c; }   /* MXChallengeCollectable::vf22 */
RX_FUNCTION(0x00403e20, vf_00403e20)

static uint32_t vf_00403e30(void) { return 0x106e; }   /* MXTargetPracticeCollectable::vf22 */
RX_FUNCTION(0x00403e30, vf_00403e30)

static uint32_t vf_00403e40(void) { return 0x106d; }   /* MXHitDiscoveryCollectable::vf22 */
RX_FUNCTION(0x00403e40, vf_00403e40)

static uint32_t vf_00403e60(void) { return 0x106f; }   /* MXStepUpBarEntity::vf22 */
RX_FUNCTION(0x00403e60, vf_00403e60)

static uint32_t vf_00403e70(void) { return 0x1070; }   /* MXStepUpUprightEntity::vf22 */
RX_FUNCTION(0x00403e70, vf_00403e70)

static uint32_t vf_00403e80(void) { return 0x1019; }   /* MXChallengesSpecEntity::vf22 */
RX_FUNCTION(0x00403e80, vf_00403e80)

static uint32_t vf_00403ee0(void) { return 0x101a; }   /* AnchorEntity::vf22 */
RX_FUNCTION(0x00403ee0, vf_00403ee0)

static uint32_t vf_00403fe0(void) { return 0x107f; }   /* UIGateSelectionEntity::vf22 */
RX_FUNCTION(0x00403fe0, vf_00403fe0)

static uint32_t vf_00404060(void) { return 0x1080; }   /* UIGateSelectionCameraEntity::vf22 */
RX_FUNCTION(0x00404060, vf_00404060)

static uint32_t vf_00404140(void) { return 0x105b; }   /* JumbotronCameraEntity::vf22 */
RX_FUNCTION(0x00404140, vf_00404140)

static uint32_t vf_004041a0(void) { return 0x1060; }   /* LightFlareEntity::vf22 */
RX_FUNCTION(0x004041a0, vf_004041a0)

static uint32_t vf_00404250(void) { return 0x1067; }   /* MudSplatterEntity::vf22 */
RX_FUNCTION(0x00404250, vf_00404250)

static uint32_t vf_004042c0(void) { return 0x1066; }   /* ScreenDustEntity::vf22 */
RX_FUNCTION(0x004042c0, vf_004042c0)

static uint32_t vf_004042d0(void) { return 0x1068; }   /* ScreenWaterDropEntity::vf22 */
RX_FUNCTION(0x004042d0, vf_004042d0)

static uint32_t vf_00404340(void) { return 0x1063; }   /* OverlayColorEntity::vf22 */
RX_FUNCTION(0x00404340, vf_00404340)

static uint32_t vf_004043b0(void) { return 0x1064; }   /* HDRSettingsEntity::vf22 */
RX_FUNCTION(0x004043b0, vf_004043b0)

static uint32_t vf_004043c0(void) { return 0x104e; }   /* BillboardEntity::vf22 */
RX_FUNCTION(0x004043c0, vf_004043c0)

static uint32_t vf_00404470(void) { return 0x1065; }   /* ShadowMapEntity::vf22 */
RX_FUNCTION(0x00404470, vf_00404470)

static uint32_t vf_00404480(void) { return 0x105f; }   /* VolumeFogEntity::vf22 */
RX_FUNCTION(0x00404480, vf_00404480)

static uint32_t vf_004044e0(void) { return 0x1075; }   /* RaceLineArrow::vf22 */
RX_FUNCTION(0x004044e0, vf_004044e0)

static uint32_t vf_004044f0(void) { return 0x1076; }   /* TDFEntity::vf22 */
RX_FUNCTION(0x004044f0, vf_004044f0)

static uint32_t vf_00404560(void) { return 0x1077; }   /* VideoEntity::vf22 */
RX_FUNCTION(0x00404560, vf_00404560)

static uint32_t vf_00404570(void) { return 0x1079; }   /* CameraFacingModelEntity::vf22 */
RX_FUNCTION(0x00404570, vf_00404570)

static uint32_t vf_00404580(void) { return 0x1051; }   /* InstancedAnimationEntity::vf22 */
RX_FUNCTION(0x00404580, vf_00404580)

static uint32_t vf_00404660(void) { return 0x107b; }   /* TimedRaceSection::vf22 */
RX_FUNCTION(0x00404660, vf_00404660)

static uint32_t vf_00404670(void) { return 0x107c; }   /* MusicTrackListEntity::vf22 */
RX_FUNCTION(0x00404670, vf_00404670)

static uint32_t vf_00404680(void) { return 0x107d; }   /* MusicScoreListEntity::vf22 */
RX_FUNCTION(0x00404680, vf_00404680)

static uint32_t vf_00404690(void) { return 0x107e; }   /* StatusBoardEntity::vf22 */
RX_FUNCTION(0x00404690, vf_00404690)

static uint32_t vf_00410c60(void) { return 0x1078; }   /* ScrollingTextureEntity::vf22 */
RX_FUNCTION(0x00410c60, vf_00410c60)

static uint32_t vf_0041b230(void) { return 0x107a; }   /* LevelInfoEntity::vf22 */
RX_FUNCTION(0x0041b230, vf_0041b230)

static uint32_t vf_0051da50(void) { return 0x8; }   /* rtti_shared_vf_0051da50 */
RX_FUNCTION(0x0051da50, vf_0051da50)

static uint32_t vf_00530fc0(void) { return 0x15; }   /* BehaviorGenericBrake::vf5 */
RX_FUNCTION(0x00530fc0, vf_00530fc0)

static uint32_t vf_00535700(void) { return 0x18; }   /* rtti_shared_vf_00535700 */
RX_FUNCTION(0x00535700, vf_00535700)

static uint32_t vf_005969f0(void) { return 0x1008; }   /* MXUIManager::vf22 */
RX_FUNCTION(0x005969f0, vf_005969f0)

static uint32_t vf_005c3830(void) { return 0x19; }   /* BehaviorAvoidFlipBack::vf5 */
RX_FUNCTION(0x005c3830, vf_005c3830)

static uint32_t vf_0063d5f0(void) { return 0x100b; }   /* MXAnimatedCamera::vf22 */
RX_FUNCTION(0x0063d5f0, vf_0063d5f0)

static uint32_t vf_0063e840(void) { return 0x104b; }   /* MXCameraPath::vf22 */
RX_FUNCTION(0x0063e840, vf_0063e840)

static uint32_t vf_00641450(void) { return 0x1002; }   /* MXFirstPersonCamera::vf22 */
RX_FUNCTION(0x00641450, vf_00641450)

static uint32_t vf_006465b0(void) { return 0x1045; }   /* MXFixedCamera::vf22 */
RX_FUNCTION(0x006465b0, vf_006465b0)

static uint32_t vf_006465f0(void) { return 0x1069; }   /* MXLevelOverviewScreenshotCamera::vf22 */
RX_FUNCTION(0x006465f0, vf_006465f0)

static uint32_t vf_00646670(void) { return 0x106a; }   /* MXLevelOverviewScreenshotPanoCamera::vf22 */
RX_FUNCTION(0x00646670, vf_00646670)

static uint32_t vf_00646fe0(void) { return 0x100c; }   /* MXQuickCamera::vf22 */
RX_FUNCTION(0x00646fe0, vf_00646fe0)

static uint32_t vf_00647df0(void) { return 0x100d; }   /* MXStartGateCamera::vf22 */
RX_FUNCTION(0x00647df0, vf_00647df0)

static uint32_t vf_00649150(void) { return 0x1046; }   /* MXReplayTowerCamera::vf22 */
RX_FUNCTION(0x00649150, vf_00649150)

static uint32_t vf_00653a90(void) { return 0x1047; }   /* MXZipLineCamera::vf22 */
RX_FUNCTION(0x00653a90, vf_00653a90)

static uint32_t vf_00654710(void) { return 0x1048; }   /* MXSplineCamera::vf22 */
RX_FUNCTION(0x00654710, vf_00654710)

static uint32_t vf_00655720(void) { return 0x1049; }   /* MXAnimatedReplayCamera::vf22 */
RX_FUNCTION(0x00655720, vf_00655720)

static uint32_t vf_00656060(void) { return 0x1005; }   /* MXUIPartsViewCamera::vf22 */
RX_FUNCTION(0x00656060, vf_00656060)

static uint32_t vf_0066a580(void) { return 0x1062; }   /* TrackSystem::vf22 */
RX_FUNCTION(0x0066a580, vf_0066a580)

static uint32_t vf_006737f0(void) { return 0x1085; }   /* CrowdLineSoundEmitterEntity::vf22 */
RX_FUNCTION(0x006737f0, vf_006737f0)

static uint32_t vf_00675400(void) { return 0x1084; }   /* LineSoundEmitterEntity::vf22 */
RX_FUNCTION(0x00675400, vf_00675400)

static uint32_t vf_00678ff0(void) { return 0x1083; }   /* SoundEmitterEntity::vf22 */
RX_FUNCTION(0x00678ff0, vf_00678ff0)

static uint32_t vf_00679a70(void) { return 0x105c; }   /* AudioTerrainZoneMappingEntity::vf22 */
RX_FUNCTION(0x00679a70, vf_00679a70)

static uint32_t vf_0067acd0(void) { return 0x105d; }   /* AudioCollisionMappingEntity::vf22 */
RX_FUNCTION(0x0067acd0, vf_0067acd0)

static uint32_t vf_0067d370(void) { return 0x105e; }   /* AudioVolumeMixEntity::vf22 */
RX_FUNCTION(0x0067d370, vf_0067d370)

static uint32_t vf_0067d960(void) { return 0x1089; }   /* FreeRideActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067d960, vf_0067d960)

static uint32_t vf_0067d970(void) { return 0x108a; }   /* FreestyleActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067d970, vf_0067d970)

static uint32_t vf_0067d980(void) { return 0x108b; }   /* RaceActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067d980, vf_0067d980)

static uint32_t vf_0067d990(void) { return 0x108c; }   /* SectionRaceActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067d990, vf_0067d990)

static uint32_t vf_0067d9a0(void) { return 0x108d; }   /* WaypointRaceActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067d9a0, vf_0067d9a0)

static uint32_t vf_0067d9b0(void) { return 0x108e; }   /* PokerRaceActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067d9b0, vf_0067d9b0)

static uint32_t vf_0067d9c0(void) { return 0x108f; }   /* InfectionActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067d9c0, vf_0067d9c0)

static uint32_t vf_0067d9d0(void) { return 0x1090; }   /* EggHuntActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067d9d0, vf_0067d9d0)

static uint32_t vf_0067d9e0(void) { return 0x1091; }   /* SnakeActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067d9e0, vf_0067d9e0)

static uint32_t vf_0067d9f0(void) { return 0x1092; }   /* TagActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067d9f0, vf_0067d9f0)

static uint32_t vf_0067da00(void) { return 0x1093; }   /* ChallengeActivityAudioManagerEntity::vf22 */
RX_FUNCTION(0x0067da00, vf_0067da00)

static uint32_t vf_00691bb0(void) { return 0x1030; }   /* ParticleEntity::vf22 */
RX_FUNCTION(0x00691bb0, vf_00691bb0)

static uint32_t vf_00692110(void) { return 0x102a; }   /* PlaceableParticleEffect::vf22 */
RX_FUNCTION(0x00692110, vf_00692110)

static uint32_t vf_00692120(void) { return 0x102e; }   /* FireworksParticleEffect::vf22 */
RX_FUNCTION(0x00692120, vf_00692120)

static uint32_t vf_00692130(void) { return 0x102f; }   /* TriggeredPlaceableParticleEffect::vf22 */
RX_FUNCTION(0x00692130, vf_00692130)

static uint32_t vf_00696a10(void) { return 0x102b; }   /* PlaceableSphereParticleEffect::vf22 */
RX_FUNCTION(0x00696a10, vf_00696a10)

static uint32_t vf_00696b00(void) { return 0x102c; }   /* PlaceableYRingParticleEffect::vf22 */
RX_FUNCTION(0x00696b00, vf_00696b00)

static uint32_t vf_006a1740(void) { return 0x1009; }   /* XenonSnakeVisibleMesh::vf22 */
RX_FUNCTION(0x006a1740, vf_006a1740)

static uint32_t vf_006a44f0(void) { return 0x100a; }   /* OnFieldCrowd::vf22 */
RX_FUNCTION(0x006a44f0, vf_006a44f0)

static uint32_t vf_006a6cf0(void) { return 0x104c; }   /* OnFieldCrowdMap::vf22 */
RX_FUNCTION(0x006a6cf0, vf_006a6cf0)

static uint32_t vf_006b8d80(void) { return 0x1054; }   /* AnimEntityProximity::vf22 */
RX_FUNCTION(0x006b8d80, vf_006b8d80)

static uint32_t vf_006ba2d0(void) { return 0x1058; }   /* AnimEntitySingleAnimLoopWithParticles::vf22 */
RX_FUNCTION(0x006ba2d0, vf_006ba2d0)

static uint32_t vf_006bc080(void) { return 0x1053; }   /* AnimEntitySplineWithStopLogic::vf22 */
RX_FUNCTION(0x006bc080, vf_006bc080)

static uint32_t vf_006bcce0(void) { return 0x105a; }   /* AnimEntitySplineWithStopLogicWithParticles::vf22 */
RX_FUNCTION(0x006bcce0, vf_006bcce0)

static uint32_t vf_006bcf90(void) { return 0x1055; }   /* AnimEntityTrigger::vf22 */
RX_FUNCTION(0x006bcf90, vf_006bcf90)

static uint32_t vf_006bedc0(void) { return 0x1034; }   /* HoleShotLight::vf22 */
RX_FUNCTION(0x006bedc0, vf_006bedc0)

static uint32_t vf_006bfeb0(void) { return 0x1035; }   /* LeaderLight::vf22 */
RX_FUNCTION(0x006bfeb0, vf_006bfeb0)

static uint32_t vf_006c0590(void) { return 0x1032; }   /* PopDownBale::vf22 */
RX_FUNCTION(0x006c0590, vf_006c0590)

static uint32_t vf_006c0db0(void) { return 0x1031; }   /* PopUpBale::vf22 */
RX_FUNCTION(0x006c0db0, vf_006c0db0)

static uint32_t vf_006c15a0(void) { return 0x1039; }   /* ScoreTowerFlagHolder::vf22 */
RX_FUNCTION(0x006c15a0, vf_006c15a0)

static uint32_t vf_006c1d30(void) { return 0x106b; }   /* SXSeatedCameraMan::vf22 */
RX_FUNCTION(0x006c1d30, vf_006c1d30)

static uint32_t vf_006c2a30(void) { return 0x1038; }   /* TrackSideFlagHolder::vf22 */
RX_FUNCTION(0x006c2a30, vf_006c2a30)

static uint32_t vf_006c32a0(void) { return 0x1036; }   /* TripleLight::vf22 */
RX_FUNCTION(0x006c32a0, vf_006c32a0)

static uint32_t vf_006cece0(void) { return 0x1056; }   /* AirAnimalSoaring::vf22 */
RX_FUNCTION(0x006cece0, vf_006cece0)

static uint32_t vf_006cfb30(void) { return 0x1057; }   /* LandAnimalProximity::vf22 */
RX_FUNCTION(0x006cfb30, vf_006cfb30)

static uint32_t vf_006d32c0(void) { return 0x1061; }   /* LensFlareEntity::vf22 */
RX_FUNCTION(0x006d32c0, vf_006d32c0)

static uint32_t vf_006d8c70(void) { return 0x1027; }   /* TerrainEntity::vf22 */
RX_FUNCTION(0x006d8c70, vf_006d8c70)

static uint32_t vf_007607d0(void) { return 0x12; }   /* BehaviorMonitorCorner::vf5 */
RX_FUNCTION(0x007607d0, vf_007607d0)

static uint32_t vf_00794f70(void) { return 0x1; }   /* rtti_shared_vf_00794f70 */
RX_FUNCTION(0x00794f70, vf_00794f70)

static uint32_t vf_0080f3c0(void) { return 0x1c; }   /* TextureAnimEntity::vf22 */
RX_FUNCTION(0x0080f3c0, vf_0080f3c0)

static uint32_t vf_0080f730(void) { return 0x1f; }   /* ReflectionMapGenEntity::vf22 */
RX_FUNCTION(0x0080f730, vf_0080f730)

static uint32_t vf_0082b4d0(void) { return 0x16; }   /* ActionAssetPlayer::vf22 */
RX_FUNCTION(0x0082b4d0, vf_0082b4d0)

static uint32_t vf_008437f0(void) { return 0x14; }   /* BehaviorRoamToWaypointGate::vf5 */
RX_FUNCTION(0x008437f0, vf_008437f0)

static uint32_t vf_008a7e40(void) { return 0xd; }   /* BehaviorLeaveActivity::vf5 */
RX_FUNCTION(0x008a7e40, vf_008a7e40)

static uint32_t vf_008d3bf0(void) { return 0x1d; }   /* SoundEffectEntityAmbient::vf22 */
RX_FUNCTION(0x008d3bf0, vf_008d3bf0)
