/* build_vehicle_atv_params.inc -- the parameter records of RavenSimulationATV (00489cb0), in registration order: offset into the
 * parameter block, kind (0 float, 1 vector, 2 curve, 3 three floats), the .rdata name, the default value(s). Read off the
 * disassembly of 00489cb0 (tools: a symbolic run of the registration code). */
static const veh_param k_atv_params[] = {
    {0x0410, 1, 0x0093a78c, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* VehicleSim_Gravity */
    {0x0470, 0, 0x0093a7a0, {0x00000000u}},   /* VehicleSim_SpeedFilterTimeConstant */
    {0x04b8, 2, 0x0093a7c4, {0}},   /* VehicleSim_GroundDragCurve */
    {0x0558, 2, 0x0093a7e0, {0}},   /* VehicleSim_AirBorneDragCurve */
    {0x05f8, 2, 0x0093a800, {0}},   /* VehicleSim_MaxSteeringAngleCurve */
    {0x0698, 0, 0x0093a824, {0x00000000u}},   /* VehicleSim_SteeringTimeConstant */
    {0x06e0, 1, 0x0093a8a4, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_MainBodyCMOffset */
    {0x0740, 0, 0x0093a8c0, {0x00000000u}},   /* MassProps_MainBodyMass */
    {0x0788, 0, 0x0093a8d8, {0x00000000u}},   /* MassProps_RiderBodyMass */
    {0x07d0, 0, 0x0093a8f0, {0x00000000u}},   /* MassProps_FrontTireMass */
    {0x0818, 0, 0x0093a908, {0x00000000u}},   /* MassProps_RearTireMass */
    {0x0860, 1, 0x0093a920, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_MainBodyInertia */
    {0x08c0, 1, 0x0093a93c, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_RiderBodyInertia */
    {0x0920, 1, 0x0093a958, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_FrontTireInertia */
    {0x0980, 1, 0x0093a974, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* MassProps_RearTireInertia */
    {0x09e0, 0, 0x0093a990, {0x00000000u}},   /* MassProps_EngineInertia */
    {0x0a28, 0, 0x0093a9a8, {0x00000000u}},   /* MassProps_DriveTrainInertia */
    {0x0a70, 0, 0x0093a9c4, {0x00000000u}},   /* Brakes_TimeConstant */
    {0x0ab8, 0, 0x0093a9d8, {0x00000000u}},   /* Brakes_FrontBrakeMaxTorque */
    {0x0b00, 2, 0x0093a9f4, {0}},   /* Brakes_FrontBrakeTorqueCurve */
    {0x0ba0, 0, 0x0093aa14, {0x00000000u}},   /* Brakes_RearBrakeMaxTorque */
    {0x0be8, 2, 0x0093aa30, {0}},   /* Brakes_RearBrakeTorqueCurve */
    {0x0c88, 2, 0x0093aa4c, {0}},   /* Brakes_WheelRollingFrictionTorsionDamping */
    {0x0d28, 0, 0x0093aab8, {0x00000000u}},   /* FrontSuspension_SpringRestLength */
    {0x0d70, 0, 0x0093b554, {0x00000000u}},   /* FrontSuspension_SpringYAttachmentPoint */
    {0x0db8, 0, 0x0093aadc, {0x00000000u}},   /* FrontSuspension_ShockKSpring */
    {0x0e00, 0, 0x0093aafc, {0x00000000u}},   /* FrontSuspension_ShockDampCompression */
    {0x0e48, 0, 0x0093ab24, {0x00000000u}},   /* FrontSuspension_ShockDampDeCompression */
    {0x0e90, 0, 0x0093ab4c, {0x00000000u}},   /* FrontSuspension_BottomOutLength */
    {0x0ee0, 1, 0x0093b57c, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontSuspension_AArmMainBodyAttachmentVector */
    {0x0f40, 0, 0x0093b5ac, {0x00000000u}},   /* FrontSuspension_AArmLength */
    {0x0f88, 0, 0x0093ab90, {0x00000000u}},   /* RearSuspension_SpringRestLength */
    {0x0fd0, 0, 0x0093abb0, {0x00000000u}},   /* RearSuspension_SpringYAttachmentPoint */
    {0x1018, 0, 0x0093abd8, {0x00000000u}},   /* RearSuspension_ShockKSpring */
    {0x1060, 0, 0x0093abf4, {0x00000000u}},   /* RearSuspension_ShockDampCompression */
    {0x10a8, 0, 0x0093ac18, {0x00000000u}},   /* RearSuspension_ShockDampDeCompression */
    {0x10f0, 0, 0x0093ac40, {0x00000000u}},   /* RearSuspension_BottomOutLength */
    {0x1140, 1, 0x0093ac60, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* RearSuspension_SwingArmMainBodyAttachmentVector */
    {0x11a0, 0, 0x0093ac90, {0x00000000u}},   /* RearSuspension_SwingArmLength */
    {0x11e8, 0, 0x0093c538, {0x00000000u}},   /* RearSuspension_AxleLength */
    {0x1230, 0, 0x0093b620, {0x00000000u}},   /* FrontTires_Radius */
    {0x1278, 0, 0x0093b634, {0x00000000u}},   /* FrontTires_Width */
    {0x12c0, 3, 0x0093b648, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_TerrainDrag */
    {0x1310, 3, 0x0093b660, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_TerrainVelocityDrag */
    {0x1360, 3, 0x0093b680, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_StaticMuRoll */
    {0x13b0, 2, 0x0093b698, {0}},   /* FrontTires_StaticMuRollCurve */
    {0x1450, 3, 0x0093b6b8, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_DynamicMuRollMultiplier */
    {0x14a0, 3, 0x0093b6dc, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_StaticMuSideSlip */
    {0x14f0, 2, 0x0093b6f8, {0}},   /* FrontTires_StaticMuSideSlipCurve */
    {0x1590, 3, 0x0093b71c, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_DynamicMuSideMultiplier */
    {0x15e0, 3, 0x0093b740, {0x00000000u, 0x00000000u, 0x00000000u}},   /* FrontTires_SideSlipDragForce */
    {0x1630, 2, 0x0093b760, {0}},   /* FrontTires_SideSlipDragForceCurve */
    {0x16d0, 0, 0x0093b784, {0x00000000u}},   /* RearTires_Radius */
    {0x1718, 0, 0x0093b798, {0x00000000u}},   /* RearTires_Width */
    {0x1760, 3, 0x0093b7a8, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_TerrainDrag */
    {0x17b0, 3, 0x0093b7c0, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_TerrainVelocityDrag */
    {0x1800, 3, 0x0093b7e0, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_StaticMuRoll */
    {0x1850, 2, 0x0093b7f8, {0}},   /* RearTires_StaticMuRollCurve */
    {0x18f0, 3, 0x0093b814, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_DynamicMuRollMultiplier */
    {0x1940, 3, 0x0093b838, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_StaticMuSideSlip */
    {0x1990, 2, 0x0093b854, {0}},   /* RearTires_StaticMuSideSlipCurve */
    {0x1a30, 3, 0x0093b874, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_DynamicMuSideMultiplier */
    {0x1a80, 3, 0x0093b898, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_SideSlipDragForce */
    {0x1ad0, 2, 0x0093b8b4, {0}},   /* RearTires_SideSlipDragForceCurve */
    {0x1b70, 3, 0x0093b8d8, {0x00000000u, 0x00000000u, 0x00000000u}},   /* RearTires_DoughnutForce */
    {0x1bc0, 2, 0x0093b05c, {0}},   /* Engine_TorqueCurve */
    {0x1c60, 0, 0x0093b098, {0x00000000u}},   /* Engine_PeakClutchRPM */
    {0x1ca8, 0, 0x0093b070, {0x00000000u}},   /* Engine_MaximumSpeed */
    {0x1cf0, 0, 0x0093b084, {0x00000000u}},   /* Engine_MinimumSpeed */
    {0x1d38, 0, 0x0093b0b0, {0x00000000u}},   /* Engine_MaximumDragTorque */
    {0x1d80, 2, 0x0093b0cc, {0}},   /* Engine_MaximumDragTorqueCurve */
    {0x1e20, 0, 0x0093b0ec, {0x00000000u}},   /* Engine_MaximumTorqueFeedback */
    {0x1e68, 0, 0x0093b10c, {0x00000000u}},   /* Engine_MinimumTorqueFeedback */
    {0x1eb0, 0, 0x0093b12c, {0x00000000u}},   /* Engine_DragCoefficient */
    {0x1ef8, 0, 0x0093b144, {0x00000000u}},   /* Engine_ClutchResetTime */
    {0x1f40, 0, 0x0093b15c, {0x00000000u}},   /* DriveTrain_ManualTransmission */
    {0x1f88, 0, 0x0093b17c, {0x00000000u}},   /* DriveTrain_PrimaryReduction */
    {0x1fd0, 0, 0x0093b198, {0x00000000u}},   /* DriveTrain_GearRatio1 */
    {0x2018, 0, 0x0093b1b0, {0x00000000u}},   /* DriveTrain_GearRatio2 */
    {0x2060, 0, 0x0093b1c8, {0x00000000u}},   /* DriveTrain_GearRatio3 */
    {0x20a8, 0, 0x0093b1e0, {0x00000000u}},   /* DriveTrain_GearRatio4 */
    {0x20f0, 0, 0x0093b1f8, {0x00000000u}},   /* DriveTrain_GearRatio5 */
    {0x2138, 0, 0x0093b210, {0x00000000u}},   /* DriveTrain_FinalRatio */
    {0x2180, 0, 0x0093b26c, {0x00000000u}},   /* AirBorne_CourseCorrectionForce */
    {0x21d0, 1, 0x0093b28c, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_AngularRateDamping */
    {0x2230, 1, 0x0093b2a8, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_AngularRateDampingOnTakeoff */
    {0x2290, 0, 0x0093b924, {0x00000000u}},   /* AirBorne_LeftStickXAngularAcceleration */
    {0x22e0, 1, 0x0093b94c, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_LeftStickXRPYMix */
    {0x2340, 0, 0x0093b968, {0x00000000u}},   /* AirBorne_LeftStickYAngularAcceleration */
    {0x2390, 1, 0x0093b990, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_LeftStickYRPYMix */
    {0x23f0, 0, 0x0093b9ac, {0x00000000u}},   /* AirBorne_RightStickXAngularAcceleration */
    {0x2440, 1, 0x0093b9d4, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_RightStickXRPYMix */
    {0x24a0, 0, 0x0093b9f0, {0x00000000u}},   /* AirBorne_RightStickYAngularAcceleration */
    {0x24f0, 1, 0x0093ba18, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* AirBorne_RightStickYRPYMix */
    {0x2550, 2, 0x0093b3a0, {0}},   /* AirBorne_LeftStickXVelocityCurve */
    {0x25f0, 2, 0x0093b3c4, {0}},   /* AirBorne_LeftStickYVelocityCurve */
    {0x2690, 2, 0x0093b3e8, {0}},   /* AirBorne_RightStickXVelocityCurve */
    {0x2730, 2, 0x0093b40c, {0}},   /* AirBorne_RightStickYVelocityCurve */
    {0x27d0, 1, 0x0093b360, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* AirBorne_RPYTimeConstants */
    {0x2830, 0, 0x0093b37c, {0xc4426ccdu}},   /* AirBorne_UnwindFrameTimeConstant */
    {0x2880, 1, 0x0093b228, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},   /* Rider_RiderCenterOfMass */
    {0x28e0, 0, 0x0093b240, {0x00000000u}},   /* Rider_LeanForwardBack */
    {0x2928, 0, 0x0093b258, {0x00000000u}},   /* Rider_LeanLeftRight */
};
