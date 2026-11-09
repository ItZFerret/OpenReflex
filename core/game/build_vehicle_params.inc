/* build_vehicle_params.inc -- the parameter records of RavenSimulationMotocross (004392f0), in registration order: offset into the
 * parameter block, kind (0 float, 1 vector, 2 curve, 3 three floats), the .rdata name, the default value(s). Read off the
 * disassembly of 004392f0 (every record: the strncpy of its name, the kind store and the default stores). */
static const veh_param k_moto_params[] = {
    {0x0410, 1, 0x0093a78c, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* VehicleSim_Gravity */
    {0x0470, 0, 0x0093a7a0, {0xc4426ccdu}},   /* VehicleSim_SpeedFilterTimeConstant */
    {0x04b8, 2, 0x0093a7c4, {0}},   /* VehicleSim_GroundDragCurve */
    {0x0558, 2, 0x0093a7e0, {0}},   /* VehicleSim_AirBorneDragCurve */
    {0x05f8, 2, 0x0093a800, {0}},   /* VehicleSim_MaxSteeringAngleCurve */
    {0x0698, 0, 0x0093a824, {0xc4426ccdu}},   /* VehicleSim_SteeringTimeConstant */
    {0x06e0, 0, 0x0093a844, {0xc4426ccdu}},   /* VehicleSim_BikeLeanTimeConstant */
    {0x0728, 2, 0x0093a864, {0}},   /* VehicleSim_ThetaLeanMaxCurve */
    {0x07c8, 0, 0x0093a884, {0xc4426ccdu}},   /* VehicleSim_UpVectorTimeConstant */
    {0x0810, 1, 0x0093a8a4, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* MassProps_MainBodyCMOffset */
    {0x0870, 0, 0x0093a8c0, {0xc4426ccdu}},   /* MassProps_MainBodyMass */
    {0x08b8, 0, 0x0093a8d8, {0xc4426ccdu}},   /* MassProps_RiderBodyMass */
    {0x0900, 0, 0x0093a8f0, {0xc4426ccdu}},   /* MassProps_FrontTireMass */
    {0x0948, 0, 0x0093a908, {0xc4426ccdu}},   /* MassProps_RearTireMass */
    {0x0990, 1, 0x0093a920, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* MassProps_MainBodyInertia */
    {0x09f0, 1, 0x0093a93c, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* MassProps_RiderBodyInertia */
    {0x0a50, 1, 0x0093a958, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* MassProps_FrontTireInertia */
    {0x0ab0, 1, 0x0093a974, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* MassProps_RearTireInertia */
    {0x0b10, 0, 0x0093a990, {0xc4426ccdu}},   /* MassProps_EngineInertia */
    {0x0b58, 0, 0x0093a9a8, {0xc4426ccdu}},   /* MassProps_DriveTrainInertia */
    {0x0ba0, 0, 0x0093a9c4, {0xc4426ccdu}},   /* Brakes_TimeConstant */
    {0x0be8, 0, 0x0093a9d8, {0xc4426ccdu}},   /* Brakes_FrontBrakeMaxTorque */
    {0x0c30, 2, 0x0093a9f4, {0}},   /* Brakes_FrontBrakeTorqueCurve */
    {0x0cd0, 0, 0x0093aa14, {0xc4426ccdu}},   /* Brakes_RearBrakeMaxTorque */
    {0x0d18, 2, 0x0093aa30, {0}},   /* Brakes_RearBrakeTorqueCurve */
    {0x0db8, 2, 0x0093aa4c, {0}},   /* Brakes_WheelRollingFrictionTorsionDamping */
    {0x0e60, 1, 0x0093aa78, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* FrontSuspension_ForkAttachmentPoint */
    {0x0ec0, 0, 0x0093aa9c, {0xc4426ccdu}},   /* FrontSuspension_RakeAngle */
    {0x0f08, 0, 0x0093aab8, {0xc4426ccdu}},   /* FrontSuspension_SpringRestLength */
    {0x0f50, 0, 0x0093aadc, {0xc4426ccdu}},   /* FrontSuspension_ShockKSpring */
    {0x0f98, 0, 0x0093aafc, {0xc4426ccdu}},   /* FrontSuspension_ShockDampCompression */
    {0x0fe0, 0, 0x0093ab24, {0xc4426ccdu}},   /* FrontSuspension_ShockDampDeCompression */
    {0x1028, 0, 0x0093ab4c, {0xc4426ccdu}},   /* FrontSuspension_BottomOutLength */
    {0x1070, 0, 0x0093ab6c, {0xc4426ccdu}},   /* FrontSuspension_FullyExtendedLength */
    {0x10b8, 0, 0x0093ab90, {0xc4426ccdu}},   /* RearSuspension_SpringRestLength */
    {0x1100, 0, 0x0093abb0, {0xc4426ccdu}},   /* RearSuspension_SpringYAttachmentPoint */
    {0x1148, 0, 0x0093abd8, {0xc4426ccdu}},   /* RearSuspension_ShockKSpring */
    {0x1190, 0, 0x0093abf4, {0xc4426ccdu}},   /* RearSuspension_ShockDampCompression */
    {0x11d8, 0, 0x0093ac18, {0xc4426ccdu}},   /* RearSuspension_ShockDampDeCompression */
    {0x1220, 0, 0x0093ac40, {0xc4426ccdu}},   /* RearSuspension_BottomOutLength */
    {0x1270, 1, 0x0093ac60, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* RearSuspension_SwingArmMainBodyAttachmentVector */
    {0x12d0, 0, 0x0093ac90, {0xc4426ccdu}},   /* RearSuspension_SwingArmLength */
    {0x1318, 0, 0x0093acb0, {0xc4426ccdu}},   /* RearSuspension_FullyExtendedLength */
    {0x1360, 0, 0x0093acd4, {0xc4426ccdu}},   /* FrontTire_Radius */
    {0x13a8, 3, 0x0093ace8, {0}},   /* FrontTire_TerrainDrag */
    {0x13f8, 3, 0x0093ad00, {0}},   /* FrontTire_TerrainVelocityDrag */
    {0x1448, 3, 0x0093ad20, {0}},   /* FrontTire_StaticMuRoll */
    {0x1498, 2, 0x0093ad38, {0}},   /* FrontTire_StaticMuRollCurve */
    {0x1538, 3, 0x0093ad54, {0}},   /* FrontTire_DynamicMuRollMultiplier */
    {0x1588, 3, 0x0093ad78, {0}},   /* FrontTire_StaticMuSideSlip */
    {0x15d8, 2, 0x0093ad94, {0}},   /* FrontTire_StaticMuSideSlipCurve */
    {0x1678, 3, 0x0093adb4, {0}},   /* FrontTire_DynamicMuSideMultiplier */
    {0x16c8, 3, 0x0093add8, {0}},   /* FrontTire_SideSlipDragForce */
    {0x1718, 2, 0x0093adf4, {0}},   /* FrontTire_SideSlipDragForceCurve */
    {0x17b8, 2, 0x0093ae18, {0}},   /* FrontTire_LeanAngleMuSideModifierCurve */
    {0x1858, 2, 0x0093ae40, {0}},   /* FrontTire_RiderShiftMuRollModifierCurve */
    {0x18f8, 2, 0x0093ae68, {0}},   /* FrontTire_RiderShiftMuSideModifierCurve */
    {0x1998, 0, 0x0093ae90, {0xc4426ccdu}},   /* RearTire_Radius */
    {0x19e0, 3, 0x0093aea0, {0}},   /* RearTire_TerrainDrag */
    {0x1a30, 3, 0x0093aeb8, {0}},   /* RearTire_TerrainVelocityDrag */
    {0x1a80, 3, 0x0093aed8, {0}},   /* RearTire_StaticMuRoll */
    {0x1ad0, 2, 0x0093aef0, {0}},   /* RearTire_StaticMuRollCurve */
    {0x1b70, 3, 0x0093af0c, {0}},   /* RearTire_DynamicMuRollMultiplier */
    {0x1bc0, 3, 0x0093af30, {0}},   /* RearTire_StaticMuSideSlip */
    {0x1c10, 2, 0x0093af4c, {0}},   /* RearTire_StaticMuSideSlipCurve */
    {0x1cb0, 3, 0x0093af6c, {0}},   /* RearTire_DynamicMuSideMultiplier */
    {0x1d00, 3, 0x0093af90, {0}},   /* RearTire_SideSlipDragForce */
    {0x1d50, 2, 0x0093afac, {0}},   /* RearTire_SideSlipDragForceCurve */
    {0x1df0, 2, 0x0093afcc, {0}},   /* RearTire_LeanAngleMuSideModifierCurve */
    {0x1e90, 2, 0x0093aff4, {0}},   /* RearTire_RiderShiftMuRollModifierCurve */
    {0x1f30, 2, 0x0093b01c, {0}},   /* RearTire_RiderShiftMuSideModifierCurve */
    {0x1fd0, 3, 0x0093b044, {0}},   /* RearTire_DoughnutForce */
    {0x2020, 2, 0x0093b05c, {0}},   /* Engine_TorqueCurve */
    {0x20c0, 0, 0x0093b070, {0xc4426ccdu}},   /* Engine_MaximumSpeed */
    {0x2108, 0, 0x0093b084, {0xc4426ccdu}},   /* Engine_MinimumSpeed */
    {0x2150, 0, 0x0093b098, {0xc4426ccdu}},   /* Engine_PeakClutchRPM */
    {0x2198, 0, 0x0093b0b0, {0xc4426ccdu}},   /* Engine_MaximumDragTorque */
    {0x21e0, 2, 0x0093b0cc, {0}},   /* Engine_MaximumDragTorqueCurve */
    {0x2280, 0, 0x0093b0ec, {0xc4426ccdu}},   /* Engine_MaximumTorqueFeedback */
    {0x22c8, 0, 0x0093b10c, {0xc4426ccdu}},   /* Engine_MinimumTorqueFeedback */
    {0x2310, 0, 0x0093b12c, {0xc4426ccdu}},   /* Engine_DragCoefficient */
    {0x2358, 0, 0x0093b144, {0xc4426ccdu}},   /* Engine_ClutchResetTime */
    {0x23a0, 0, 0x0093b15c, {0xc4426ccdu}},   /* DriveTrain_ManualTransmission */
    {0x23e8, 0, 0x0093b17c, {0xc4426ccdu}},   /* DriveTrain_PrimaryReduction */
    {0x2430, 0, 0x0093b198, {0xc4426ccdu}},   /* DriveTrain_GearRatio1 */
    {0x2478, 0, 0x0093b1b0, {0xc4426ccdu}},   /* DriveTrain_GearRatio2 */
    {0x24c0, 0, 0x0093b1c8, {0xc4426ccdu}},   /* DriveTrain_GearRatio3 */
    {0x2508, 0, 0x0093b1e0, {0xc4426ccdu}},   /* DriveTrain_GearRatio4 */
    {0x2550, 0, 0x0093b1f8, {0xc4426ccdu}},   /* DriveTrain_GearRatio5 */
    {0x2598, 0, 0x0093b210, {0xc4426ccdu}},   /* DriveTrain_FinalRatio */
    {0x25e0, 1, 0x0093b228, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* Rider_RiderCenterOfMass */
    {0x2640, 0, 0x0093b240, {0xc4426ccdu}},   /* Rider_LeanForwardBack */
    {0x2688, 0, 0x0093b258, {0xc4426ccdu}},   /* Rider_LeanLeftRight */
    {0x26d0, 0, 0x0093b26c, {0xc4426ccdu}},   /* AirBorne_CourseCorrectionForce */
    {0x2720, 1, 0x0093b28c, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* AirBorne_AngularRateDamping */
    {0x2780, 1, 0x0093b2a8, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* AirBorne_AngularRateDampingOnTakeoff */
    {0x27e0, 0, 0x0093b2d0, {0xc4426ccdu}},   /* AirBorne_ControlSpringK */
    {0x2828, 0, 0x0093b2e8, {0xc4426ccdu}},   /* AirBorne_ReturnSpringK */
    {0x2870, 1, 0x0093b300, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* AirBorne_LeftStickXRPY */
    {0x28d0, 1, 0x0093b318, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* AirBorne_LeftStickYRPY */
    {0x2930, 1, 0x0093b330, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* AirBorne_RightStickXRPY */
    {0x2990, 1, 0x0093b348, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* AirBorne_RightStickYRPY */
    {0x29f0, 1, 0x0093b360, {0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u, 0xc0f8a3d7u}},   /* AirBorne_RPYTimeConstants */
    {0x2a50, 0, 0x0093b37c, {0xc4426ccdu}},   /* AirBorne_UnwindFrameTimeConstant */
    {0x2a98, 2, 0x0093b3a0, {0}},   /* AirBorne_LeftStickXVelocityCurve */
    {0x2b38, 2, 0x0093b3c4, {0}},   /* AirBorne_LeftStickYVelocityCurve */
    {0x2bd8, 2, 0x0093b3e8, {0}},   /* AirBorne_RightStickXVelocityCurve */
    {0x2c78, 2, 0x0093b40c, {0}},   /* AirBorne_RightStickYVelocityCurve */
};
