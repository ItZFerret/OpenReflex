/* build_vehicle_utv_params.inc -- the parameter records of RavenSimulationUTV (004c9d40), in registration order: offset into the
 * parameter block, kind (0 float, 1 vector, 2 curve, 3 three floats), the .rdata name, the default value(s). Read off the
 * disassembly of 004c9d40 (tools: a symbolic run of the registration code). */
static const veh_param k_utv_params[] = {
    {0x0410, 1, 0x0093a78c, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* VehicleSim_Gravity */
    {0x0470, 0, 0x0093a7a0, {0x00000000u}},   /* VehicleSim_SpeedFilterTimeConstant */
    {0x04b8, 2, 0x0093a7c4, {0}},   /* VehicleSim_GroundDragCurve */
    {0x0558, 2, 0x0093a7e0, {0}},   /* VehicleSim_AirBorneDragCurve */
    {0x05f8, 2, 0x0093b4b8, {0}},   /* VehicleSim_AirBorneDownForceCurve */
    {0x0698, 2, 0x0093a800, {0}},   /* VehicleSim_MaxSteeringAngleCurve */
    {0x0738, 0, 0x0093a824, {0x00000000u}},   /* VehicleSim_SteeringTimeConstant */
    {0x0780, 1, 0x0093a8a4, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_MainBodyCMOffset */
    {0x07e0, 0, 0x0093a8c0, {0x00000000u}},   /* MassProps_MainBodyMass */
    {0x0828, 0, 0x0093b4dc, {0x00000000u}},   /* MassProps_FrontAArmMass */
    {0x0870, 0, 0x0093a8f0, {0x00000000u}},   /* MassProps_FrontTireMass */
    {0x08b8, 0, 0x0093b4f4, {0x00000000u}},   /* MassProps_RearTrailingArmMass */
    {0x0900, 0, 0x0093a908, {0x00000000u}},   /* MassProps_RearTireMass */
    {0x0950, 1, 0x0093a920, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_MainBodyInertia */
    {0x09b0, 1, 0x0093b514, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_FrontAArmInertia */
    {0x0a10, 1, 0x0093a958, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_FrontTireInertia */
    {0x0a70, 1, 0x0093b530, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_RearTrailingArmInertia */
    {0x0ad0, 1, 0x0093a974, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_RearTireInertia */
    {0x0b30, 0, 0x0093a990, {0x00000000u}},   /* MassProps_EngineInertia */
    {0x0b78, 0, 0x0093a9a8, {0x00000000u}},   /* MassProps_DriveTrainInertia */
    {0x0bc0, 0, 0x0093a9c4, {0x00000000u}},   /* Brakes_TimeConstant */
    {0x0c08, 0, 0x0093a9d8, {0x00000000u}},   /* Brakes_FrontBrakeMaxTorque */
    {0x0c50, 2, 0x0093a9f4, {0}},   /* Brakes_FrontBrakeTorqueCurve */
    {0x0cf0, 0, 0x0093aa14, {0x00000000u}},   /* Brakes_RearBrakeMaxTorque */
    {0x0d38, 2, 0x0093aa30, {0}},   /* Brakes_RearBrakeTorqueCurve */
    {0x0dd8, 2, 0x0093aa4c, {0}},   /* Brakes_WheelRollingFrictionTorsionDamping */
    {0x0e78, 0, 0x0093aab8, {0x00000000u}},   /* FrontSuspension_SpringRestLength */
    {0x0ec0, 0, 0x0093b554, {0x00000000u}},   /* FrontSuspension_SpringYAttachmentPoint */
    {0x0f08, 0, 0x0093ab4c, {0x00000000u}},   /* FrontSuspension_BottomOutLength */
    {0x0f50, 0, 0x0093aadc, {0x00000000u}},   /* FrontSuspension_ShockKSpring */
    {0x0f98, 0, 0x0093aafc, {0x00000000u}},   /* FrontSuspension_ShockDampCompression */
    {0x0fe0, 0, 0x0093ab24, {0x00000000u}},   /* FrontSuspension_ShockDampDeCompression */
    {0x1030, 1, 0x0093b57c, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontSuspension_AArmMainBodyAttachmentVector */
    {0x1090, 0, 0x0093b5ac, {0x00000000u}},   /* FrontSuspension_AArmLength */
    {0x10d8, 0, 0x0093ab90, {0x00000000u}},   /* RearSuspension_SpringRestLength */
    {0x1120, 0, 0x0093abb0, {0x00000000u}},   /* RearSuspension_SpringYAttachmentPoint */
    {0x1168, 0, 0x0093abd8, {0x00000000u}},   /* RearSuspension_ShockKSpring */
    {0x11b0, 0, 0x0093abf4, {0x00000000u}},   /* RearSuspension_ShockDampCompression */
    {0x11f8, 0, 0x0093ac18, {0x00000000u}},   /* RearSuspension_ShockDampDeCompression */
    {0x1240, 0, 0x0093ac40, {0x00000000u}},   /* RearSuspension_BottomOutLength */
    {0x1290, 1, 0x0093c6e0, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* RearSuspension_AArmMainBodyAttachmentVector */
    {0x12f0, 0, 0x0093c70c, {0x00000000u}},   /* RearSuspension_AArmLength */
    {0x1338, 0, 0x0093b620, {0x00000000u}},   /* FrontTires_Radius */
    {0x1380, 0, 0x0093b634, {0x00000000u}},   /* FrontTires_Width */
    {0x13c8, 3, 0x0093b648, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_TerrainDrag */
    {0x1418, 3, 0x0093b660, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_TerrainVelocityDrag */
    {0x1468, 3, 0x0093b680, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_StaticMuRoll */
    {0x14b8, 2, 0x0093b698, {0}},   /* FrontTires_StaticMuRollCurve */
    {0x1558, 3, 0x0093b6b8, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_DynamicMuRollMultiplier */
    {0x15a8, 3, 0x0093b6dc, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_StaticMuSideSlip */
    {0x15f8, 2, 0x0093b6f8, {0}},   /* FrontTires_StaticMuSideSlipCurve */
    {0x1698, 3, 0x0093b71c, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_DynamicMuSideMultiplier */
    {0x16e8, 3, 0x0093b740, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_SideSlipDragForce */
    {0x1738, 2, 0x0093b760, {0}},   /* FrontTires_SideSlipDragForceCurve */
    {0x17d8, 0, 0x0093b784, {0x00000000u}},   /* RearTires_Radius */
    {0x1820, 0, 0x0093b798, {0x00000000u}},   /* RearTires_Width */
    {0x1868, 3, 0x0093b7a8, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_TerrainDrag */
    {0x18b8, 3, 0x0093b7c0, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_TerrainVelocityDrag */
    {0x1908, 3, 0x0093b7e0, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_StaticMuRoll */
    {0x1958, 2, 0x0093b7f8, {0}},   /* RearTires_StaticMuRollCurve */
    {0x19f8, 3, 0x0093b814, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_DynamicMuRollMultiplier */
    {0x1a48, 3, 0x0093b838, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_StaticMuSideSlip */
    {0x1a98, 2, 0x0093b854, {0}},   /* RearTires_StaticMuSideSlipCurve */
    {0x1b38, 3, 0x0093b874, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_DynamicMuSideMultiplier */
    {0x1b88, 3, 0x0093b898, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_SideSlipDragForce */
    {0x1bd8, 2, 0x0093b8b4, {0}},   /* RearTires_SideSlipDragForceCurve */
    {0x1c78, 3, 0x0093b8d8, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_DoughnutForce */
    {0x1cc8, 2, 0x0093b05c, {0}},   /* Engine_TorqueCurve */
    {0x1d68, 0, 0x0093b098, {0x00000000u}},   /* Engine_PeakClutchRPM */
    {0x1db0, 0, 0x0093b8f0, {0x00000000u}},   /* Engine_ReverseTorque */
    {0x1df8, 0, 0x0093b070, {0x00000000u}},   /* Engine_MaximumSpeed */
    {0x1e40, 0, 0x0093b084, {0x00000000u}},   /* Engine_MinimumSpeed */
    {0x1e88, 0, 0x0093b0b0, {0x00000000u}},   /* Engine_MaximumDragTorque */
    {0x1ed0, 2, 0x0093b0cc, {0}},   /* Engine_MaximumDragTorqueCurve */
    {0x1f70, 0, 0x0093b0ec, {0x00000000u}},   /* Engine_MaximumTorqueFeedback */
    {0x1fb8, 0, 0x0093b10c, {0x00000000u}},   /* Engine_MinimumTorqueFeedback */
    {0x2000, 0, 0x0093b12c, {0x00000000u}},   /* Engine_DragCoefficient */
    {0x2048, 0, 0x0093b144, {0x00000000u}},   /* Engine_ClutchResetTime */
    {0x2090, 0, 0x0093b908, {0x00000000u}},   /* Engine_ClutchCoefficient */
    {0x20d8, 0, 0x0093b15c, {0x00000000u}},   /* DriveTrain_ManualTransmission */
    {0x2120, 0, 0x0093b17c, {0x00000000u}},   /* DriveTrain_PrimaryReduction */
    {0x2168, 0, 0x0093b198, {0x00000000u}},   /* DriveTrain_GearRatio1 */
    {0x21b0, 0, 0x0093b1b0, {0x00000000u}},   /* DriveTrain_GearRatio2 */
    {0x21f8, 0, 0x0093b1c8, {0x00000000u}},   /* DriveTrain_GearRatio3 */
    {0x2240, 0, 0x0093b1e0, {0x00000000u}},   /* DriveTrain_GearRatio4 */
    {0x2288, 0, 0x0093b1f8, {0x00000000u}},   /* DriveTrain_GearRatio5 */
    {0x22d0, 0, 0x0093b210, {0x00000000u}},   /* DriveTrain_FinalRatio */
    {0x2318, 0, 0x0093b26c, {0x00000000u}},   /* AirBorne_CourseCorrectionForce */
    {0x2360, 1, 0x0093b28c, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_AngularRateDamping */
    {0x23c0, 1, 0x0093b2a8, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_AngularRateDampingOnTakeoff */
    {0x2420, 0, 0x0093b924, {0x00000000u}},   /* AirBorne_LeftStickXAngularAcceleration */
    {0x2470, 1, 0x0093b94c, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_LeftStickXRPYMix */
    {0x24d0, 0, 0x0093b968, {0x00000000u}},   /* AirBorne_LeftStickYAngularAcceleration */
    {0x2520, 1, 0x0093b990, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_LeftStickYRPYMix */
    {0x2580, 0, 0x0093b9ac, {0x00000000u}},   /* AirBorne_RightStickXAngularAcceleration */
    {0x25d0, 1, 0x0093b9d4, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_RightStickXRPYMix */
    {0x2630, 0, 0x0093b9f0, {0x00000000u}},   /* AirBorne_RightStickYAngularAcceleration */
    {0x2680, 1, 0x0093ba18, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_RightStickYRPYMix */
};
