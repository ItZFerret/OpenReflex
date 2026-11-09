/* build_tables_schemas.inc -- the static tables' column lists of the schema functions FUN_00756760 calls, extracted from their
 * executed code (scratchpad rx_build12/schemagen.py: every push / stack store / column call of the boot-executed path, the default and
 * flags at each call): {op, table, name, default (int value / float bits / string literal), flags, parent table, parent column, 7th
 * argument}. Literals are .rdata addresses. Included by build_tables_schema.c. */
/* 004decd0 ESI=00ae1b18 */
static const schema_op S_004decd0_ae1b18[] = {
    {OP_RESERVE, 0x00ae1b1c, 0, 6, 0, 0, 0, 0},
    {OP_INT, 0x00ae1b18, 0x0093dd90, 0x00000000, 1, 0x00000000, 0x00935d6c, 0x004dce80}, /* ActivityType */
    {OP_INT, 0x00ae1b18, 0x0093dda0, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* ActivityArea */
    {OP_INT, 0x00ae1b18, 0x00936e30, 0x00000001, 0, 0x00000000, 0x00935d6c, 0x004dced0}, /* ActivityMode */
    {OP_INT, 0x00ae1b18, 0x0093df20, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Spectator */
    {OP_INT, 0x00ae1b18, 0x0093dfd4, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* MiniGameStartZone */
    {OP_INT, 0x00ae1b18, 0x0093dd64, 0x00000003, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* NumLaps */
    {OP_FLOAT, 0x00ae1b18, 0x0093dd58, 0x43960000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Duration */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004decd0 ESI=00adffe0 */
static const schema_op S_004decd0_adffe0[] = {
    {OP_RESERVE, 0x00adffe4, 0, 6, 0, 0, 0, 0},
    {OP_INT, 0x00adffe0, 0x0093dd90, 0x00000000, 1, 0x00000000, 0x00935d6c, 0x004dce80}, /* ActivityType */
    {OP_INT, 0x00adffe0, 0x0093dda0, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* ActivityArea */
    {OP_INT, 0x00adffe0, 0x00936e30, 0x00000001, 0, 0x00000000, 0x00935d6c, 0x004dced0}, /* ActivityMode */
    {OP_INT, 0x00adffe0, 0x0093df20, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Spectator */
    {OP_INT, 0x00adffe0, 0x0093dfd4, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* MiniGameStartZone */
    {OP_INT, 0x00adffe0, 0x0093dd64, 0x00000003, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* NumLaps */
    {OP_FLOAT, 0x00adffe0, 0x0093dd58, 0x43960000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Duration */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004de770 */
static const schema_op S_004de770[] = {
    {OP_RESERVE, 0x00ae05cc, 0, 3, 0, 0, 0, 0},
    {OP_INT, 0x00ae05c8, 0x00936cb4, 0xffffffff, 3, 0x00000000, 0x00935d6c, 0x00000000}, /* VehicleId */
    {OP_INT, 0x00ae05c8, 0x0093df84, 0x00000009, 1, 0x00000000, 0x00935d6c, 0x004dcf20}, /* VehicleOwner */
    {OP_INT, 0x00ae05c8, 0x0093df94, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* OwnerVehicleId */
    {OP_GLOBAL, 0x00ae063c, 0, 0xffffffff, 0, 0, 0, 0},
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004de9e0 */
static const schema_op S_004de9e0[] = {
    {OP_RESERVE, 0x00ae19b4, 0, 4, 0, 0, 0, 0},
    {OP_INT, 0x00ae19b0, 0x0093dec8, 0xffffffff, 3, 0x00000000, 0x00935d6c, 0x00000000}, /* RiderId */
    {OP_INT, 0x00ae19b0, 0x0093dfa4, 0x00000007, 1, 0x00000000, 0x00935d6c, 0x004dcf70}, /* RiderOwner */
    {OP_INT, 0x00ae19b0, 0x0093dfb0, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* OwnerRiderId */
    {OP_INT, 0x00ae19b0, 0x0093dfc0, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* OwnerRiderGearSetId */
    {OP_GLOBAL, 0x00ae1a24, 0, 0xffffffff, 0, 0, 0, 0},
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004dcfc0 */
static const schema_op S_004dcfc0[] = {
    {OP_RESERVE, 0x00ae0e0c, 0, 21, 0, 0, 0, 0},
    {OP_INT, 0x00ae0e08, 0x00936ef4, 0x00000000, 1, 0x00000000, 0x00935d6c, 0x004dcd40}, /* PlayerType */
    {OP_INT, 0x00ae0e08, 0x0093de90, 0x00000001, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerLocal */
    {OP_STRING, 0x00ae0e08, 0x00936f00, 0x0093de9c, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerName */
    {OP_STRING, 0x00ae0e08, 0x0093deac, 0x0093de9c, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* DisplayName */
    {OP_STRING, 0x00ae0e08, 0x0093deb8, 0x00935d6c, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* MXProfileName */
    {OP_INT, 0x00ae0e08, 0x0093dd90, 0x0000003e, 0, 0x00000000, 0x00935d6c, 0x004dce80}, /* ActivityType */
    {OP_INT, 0x00ae0e08, 0x0093dda0, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* ActivityArea */
    {OP_INT, 0x00ae0e08, 0x00936cb4, 0xffffffff, 1, 0x00ae05c8, 0x00936cb4, 0x00000000}, /* VehicleId */
    {OP_INT, 0x00ae0e08, 0x0093dec8, 0xffffffff, 1, 0x00ae19b0, 0x0093dec8, 0x00000000}, /* RiderId */
    {OP_INT, 0x00ae0e08, 0x0093ded0, 0xffffffff, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* Controller */
    {OP_INT, 0x00ae0e08, 0x0093dedc, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Viewport */
    {OP_INT, 0x00ae0e08, 0x0093dee8, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Disconnected */
    {OP_INT, 0x00ae0e08, 0x0093def8, 0x00000001, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Active */
    {OP_INT, 0x00ae0e08, 0x0093df00, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* StartZone */
    {OP_INT, 0x00ae0e08, 0x0093df0c, 0xffffffff, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* UniqueId */
    {OP_INT, 0x00ae0e08, 0x0093de38, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Ready */
    {OP_INT, 0x00ae0e08, 0x0093de28, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Loaded */
    {OP_INT, 0x00ae0e08, 0x0093df18, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Team */
    {OP_INT, 0x00ae0e08, 0x0093df20, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Spectator */
    {OP_INT, 0x00ae0e08, 0x00936e48, 0x00000003, 0, 0x00000000, 0x00935d6c, 0x004dce70}, /* AIDifficulty */
    {OP_INT, 0x00ae0e08, 0x0093df2c, 0x0000003e, 0, 0x00000000, 0x00935d6c, 0x004dce80}, /* RequestedActivityType */
    {OP_INT, 0x00ae0e08, 0x0093df44, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* RequestedActivityArea */
    {OP_INT, 0x00ae0e08, 0x0093df5c, 0xffffffff, 1, 0x00ae05c8, 0x00936cb4, 0x00000000}, /* RequestedVehicleId */
    {OP_INT, 0x00ae0e08, 0x0093df70, 0xffffffff, 1, 0x00ae19b0, 0x0093dec8, 0x00000000}, /* RequestedRiderId */
    {OP_GLOBAL, 0x00ae0e7c, 0, 0xffffffff, 0, 0, 0, 0},
    {OP_GLOBAL, 0x00ae0e90, 0, 0x00000000, 0, 0, 0, 0},
    {OP_GLOBAL, 0x00ae0e94, 0, 0x00000000, 0, 0, 0, 0},
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e1820 EDI=00ae07a8 */
static const schema_op S_004e1820_ae07a8[] = {
    {OP_RESERVE, 0x00ae07ac, 0, 7, 0, 0, 0, 0},
    {OP_INT, 0x00ae07a8, 0x0093e23c, 0xffffffff, 3, 0x00000000, 0x00935d6c, 0x00000000}, /* NetworkRiderId */
    {OP_INT, 0x00ae07a8, 0x0093e24c, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* RiderFontId */
    {OP_INT, 0x00ae07a8, 0x0093e258, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* VehicleFontId */
    {OP_STRING, 0x00ae07a8, 0x0093a3bc, 0x00935d6c, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Name */
    {OP_STRING, 0x00ae07a8, 0x0093e26c, 0x0093e268, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Number */
    {OP_INT, 0x00ae07a8, 0x0093e274, 0x00000005, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Tier */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e1b90 ESI=00ae0820 */
static const schema_op S_004e1b90_ae0820[] = {
    {OP_RESERVE, 0x00ae0824, 0, 2, 0, 0, 0, 0},
    {OP_INT, 0x00ae0820, 0x0093e23c, 0xffffffff, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* NetworkRiderId */
    {OP_INT, 0x00ae0820, 0x0093e27c, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* RiderGearId */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e1d70 ESI=00ae0640 */
static const schema_op S_004e1d70_ae0640[] = {
    {OP_RESERVE, 0x00ae0644, 0, 2, 0, 0, 0, 0},
    {OP_INT, 0x00ae0640, 0x0093e288, 0xffffffff, 3, 0x00000000, 0x00935d6c, 0x00000000}, /* NetworkVehicleId */
    {OP_INT, 0x00ae0640, 0x0093e170, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* VehicleModelId */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e1f40 ESI=00ae06b8 */
static const schema_op S_004e1f40_ae06b8[] = {
    {OP_RESERVE, 0x00ae06bc, 0, 2, 0, 0, 0, 0},
    {OP_INT, 0x00ae06b8, 0x0093e288, 0xffffffff, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* NetworkVehicleId */
    {OP_INT, 0x00ae06b8, 0x0093e29c, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* VehiclePartId */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004df400 */
static const schema_op S_004df400[] = {
    {OP_RESERVE, 0x00ae0d7c, 0, 3, 0, 0, 0, 0},
    {OP_INT, 0x00ae0d78, 0x0093e018, 0x0000002b, 3, 0x00000000, 0x00935d6c, 0x00000000}, /* AttributeType */
    {OP_INT, 0x00ae0d78, 0x0093e028, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* AttributeValue */
    {OP_INT, 0x00ae0d78, 0x0093e038, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Published */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004df700 */
static const schema_op S_004df700[] = {
    {OP_RESERVE, 0x00ae0e9c, 0, 4, 0, 0, 0, 0},
    {OP_INT, 0x00ae0e98, 0x0093dd90, 0x0000003e, 0, 0x00000000, 0x00935d6c, 0x004dce80}, /* ActivityType */
    {OP_INT, 0x00ae0e98, 0x0093e044, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* NumSessions */
    {OP_INT, 0x00ae0e98, 0x0093e050, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* NumPlayers */
    {OP_INT, 0x00ae0e98, 0x0093dd84, 0x00000002, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* SessionType */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004df9f0 */
static const schema_op S_004df9f0[] = {
    {OP_RESERVE, 0x00ae0464, 0, 26, 0, 0, 0, 0},
    {OP_INT, 0x00ae0460, 0x0093e05c, 0x00000001, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* GameType */
    {OP_INT, 0x00ae0460, 0x0093dd90, 0x0000003e, 1, 0x00000000, 0x00935d6c, 0x004dce80}, /* ActivityType */
    {OP_INT, 0x00ae0460, 0x0093e068, 0xffffffff, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* LocationId */
    {OP_INT, 0x00ae0460, 0x0093dda0, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* ActivityArea */
    {OP_STRING, 0x00ae0460, 0x0093dd38, 0x00935d6c, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* HostName */
    {OP_INT, 0x00ae0460, 0x0093dd64, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* NumLaps */
    {OP_INT, 0x00ae0460, 0x0093dd58, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Duration */
    {OP_INT, 0x00ae0460, 0x0093e074, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* MinPlayers */
    {OP_INT, 0x00ae0460, 0x0093e050, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* NumPlayers */
    {OP_INT, 0x00ae0460, 0x0093e080, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* MaxPlayers */
    {OP_INT, 0x00ae0460, 0x0093dd24, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* SessionInProgress */
    {OP_INT, 0x00ae0460, 0x0093ddbc, 0x00000001, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* SessionJoinable */
    {OP_INT, 0x00ae0460, 0x00935df8, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* VehicleClass */
    {OP_INT, 0x00ae0460, 0x0093dd44, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* VehicleClassLevel */
    {OP_INT, 0x00ae0460, 0x0093dd84, 0x00000002, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* SessionType */
    {OP_STRING, 0x00ae0460, 0x0093e08c, 0x00935d6c, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* StationURL */
    {OP_INT, 0x00ae0460, 0x0093dd10, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* HostPID */
    {OP_INT, 0x00ae0460, 0x0093e098, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* GatheringID */
    {OP_INT, 0x00ae0460, 0x0093dd6c, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* PlaylistID */
    {OP_INT, 0x00ae0460, 0x0093dd78, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* IsSeries */
    {OP_INT, 0x00ae0460, 0x0093e0a4, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* SortVehicleClass */
    {OP_INT, 0x00ae0460, 0x0093e0b8, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* SortLocation */
    {OP_INT, 0x00ae0460, 0x0093e0c8, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* SortLaps */
    {OP_INT, 0x00ae0460, 0x0093e0d4, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* SortRiders */
    {OP_INT, 0x00ae0460, 0x0093e0e0, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* SortDuration */
    {OP_INT, 0x00ae0460, 0x0093e0f0, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* SortMinRiders */
    {OP_INT64, 0x00ae0460, 0x0093e100, 0x00000000, 0, 0, 0, 0}, /* LobbyID (004f7180) */
    {OP_INT64, 0x00ae0460, 0x0093dd04, 0x00000000, 0, 0, 0, 0}, /* HostSteamID (004f7180) */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e0a40 EDI=00ae04d8 */
static const schema_op S_004e0a40_ae04d8[] = {
    {OP_RESERVE, 0x00ae04dc, 0, 13, 0, 0, 0, 0},
    {OP_INT, 0x00ae04d8, 0x0093e138, 0xffffffff, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* LeaderboardId */
    {OP_INT, 0x00ae04d8, 0x0093e148, 0xffffffff, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* Rank */
    {OP_STRING, 0x00ae04d8, 0x00936f00, 0x00935d6c, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerName */
    {OP_INT, 0x00ae04d8, 0x0093e150, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerUIDHigh */
    {OP_INT, 0x00ae04d8, 0x0093e160, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerUIDLow */
    {OP_INT, 0x00ae04d8, 0x00935e68, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Vehicle */
    {OP_INT, 0x00ae04d8, 0x0093de0c, 0xffffffff, 0, PARENT_FIND(0x0093e180), 0x0093e170, 0x00000000}, /* Model */
    {OP_INT, 0x00ae04d8, 0x0093e190, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Connection */
    {OP_FLOAT, 0x00ae04d8, 0x0093e19c, 0xbf800000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* BLT */
    {OP_FLOAT, 0x00ae04d8, 0x0093e1a0, 0xbf800000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* HighScore */
    {OP_INT, 0x00ae04d8, 0x0093e1ac, 0x00000000, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* SkillLB */
    {OP_INT, 0x00ae04d8, 0x0093e1b4, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Skill */
    {OP_INT, 0x00ae04d8, 0x0093e1bc, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* GamesPlayed */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e1190 */
static const schema_op S_004e1190[] = {
    {OP_RESERVE, 0x00ae1bac, 0, 4, 0, 0, 0, 0},
    {OP_INT, 0x00ae1ba8, 0x0093e150, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerUIDHigh */
    {OP_INT, 0x00ae1ba8, 0x0093e160, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerUIDLow */
    {OP_INT, 0x00ae1ba8, 0x0093e1c8, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* ExpPoints */
    {OP_INT, 0x00ae1ba8, 0x0093e1d4, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* ExpLevel */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e1650 */
static const schema_op S_004e1650[] = {
    {OP_RESERVE, 0x00ae0f8c, 0, 11, 0, 0, 0, 0},
    {OP_STRING, 0x00ae0f88, 0x00936f00, 0x00935d6c, 3, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerName */
    {OP_INT, 0x00ae0f88, 0x0093ddd8, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Presence */
    {OP_INT, 0x00ae0f88, 0x0093de40, 0x00000000, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* Online */
    {OP_INT, 0x00ae0f88, 0x0093e1e0, 0x00000000, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* InviteReceived */
    {OP_INT, 0x00ae0f88, 0x0093e1f0, 0x00000000, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* InviteSent */
    {OP_INT, 0x00ae0f88, 0x0093dd18, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* GatheringId */
    {OP_INT, 0x00ae0f88, 0x0093e1fc, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* InSession */
    {OP_INT, 0x00ae0f88, 0x0093e208, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Activity */
    {OP_INT, 0x00ae0f88, 0x0093e214, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Area */
    {OP_INT, 0x00ae0f88, 0x0093ddb0, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Location */
    {OP_INT, 0x00ae0f88, 0x0093e21c, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* PlaylistId */
    {OP_INT, 0x00ae0f88, 0x0093e228, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* EventTypeLocationId */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e1820 EDI=00ae0f10 */
static const schema_op S_004e1820_ae0f10[] = {
    {OP_RESERVE, 0x00ae0f14, 0, 7, 0, 0, 0, 0},
    {OP_INT, 0x00ae0f10, 0x0093e23c, 0xffffffff, 3, 0x00000000, 0x00935d6c, 0x00000000}, /* NetworkRiderId */
    {OP_INT, 0x00ae0f10, 0x0093e24c, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* RiderFontId */
    {OP_INT, 0x00ae0f10, 0x0093e258, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* VehicleFontId */
    {OP_STRING, 0x00ae0f10, 0x0093a3bc, 0x00935d6c, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Name */
    {OP_STRING, 0x00ae0f10, 0x0093e26c, 0x0093e268, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Number */
    {OP_INT, 0x00ae0f10, 0x0093e274, 0x00000005, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Tier */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e1b90 ESI=00ae1a28 */
static const schema_op S_004e1b90_ae1a28[] = {
    {OP_RESERVE, 0x00ae1a2c, 0, 2, 0, 0, 0, 0},
    {OP_INT, 0x00ae1a28, 0x0093e23c, 0xffffffff, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* NetworkRiderId */
    {OP_INT, 0x00ae1a28, 0x0093e27c, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* RiderGearId */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e1d70 ESI=00ae0898 */
static const schema_op S_004e1d70_ae0898[] = {
    {OP_RESERVE, 0x00ae089c, 0, 2, 0, 0, 0, 0},
    {OP_INT, 0x00ae0898, 0x0093e288, 0xffffffff, 3, 0x00000000, 0x00935d6c, 0x00000000}, /* NetworkVehicleId */
    {OP_INT, 0x00ae0898, 0x0093e170, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* VehicleModelId */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e1f40 ESI=00ae0550 */
static const schema_op S_004e1f40_ae0550[] = {
    {OP_RESERVE, 0x00ae0554, 0, 2, 0, 0, 0, 0},
    {OP_INT, 0x00ae0550, 0x0093e288, 0xffffffff, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* NetworkVehicleId */
    {OP_INT, 0x00ae0550, 0x0093e29c, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* VehiclePartId */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e2120 */
static const schema_op S_004e2120[] = {
    {OP_RESERVE, 0x00ae0b0c, 0, 4, 0, 0, 0, 0},
    {OP_INT, 0x00ae0b08, 0x0093e150, 0x00000000, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerUIDHigh */
    {OP_INT, 0x00ae0b08, 0x0093e160, 0x00000000, 1, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerUIDLow */
    {OP_INT, 0x00ae0b08, 0x0093e2ac, 0x00000033, 0, 0x00000000, 0x00935d6c, 0x004dcf90}, /* Attribute */
    {OP_INT, 0x00ae0b08, 0x0093e2b8, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Value */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e2590 */
static const schema_op S_004e2590[] = {
    {OP_RESERVE, 0x00ae1aa4, 0, 10, 0, 0, 0, 0},
    {OP_INT, 0x00ae1aa0, 0x00936e90, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* EventTypeId */
    {OP_INT, 0x00ae1aa0, 0x00936e20, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* EventLocationId */
    {OP_INT, 0x00ae1aa0, 0x00936e40, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* TrackId */
    {OP_INT, 0x00ae1aa0, 0x0093e2c0, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Ordinal */
    {OP_INT, 0x00ae1aa0, 0x0093dd64, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* NumLaps */
    {OP_INT, 0x00ae1aa0, 0x0093dd58, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* Duration */
    {OP_INT, 0x00ae1aa0, 0x00935df8, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* VehicleClass */
    {OP_INT, 0x00ae1aa0, 0x0093e074, 0x00000002, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* MinPlayers */
    {OP_INT, 0x00ae1aa0, 0x0093e080, 0x0000000c, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* MaxPlayers */
    {OP_INT, 0x00ae1aa0, 0x0093ddcc, 0xffffffff, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* GameMode */
    {OP_GLOBAL, 0x00ae1b14, 0, 0xffffffff, 0, 0, 0, 0},
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e5eb0 */
static const schema_op S_004e5eb0[] = {
    {OP_RESERVE, 0x00ae107c, 0, 3, 0, 0, 0, 0},
    {OP_INT, 0x00ae1078, 0x0093e2f8, 0xffffffff, 3, 0x00000000, 0x00935d6c, 0x00000000}, /* ExperienceBonusId */
    {OP_INT, 0x00ae1078, 0x0093e30c, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* ExperienceReward */
    {OP_STRING, 0x00ae1078, 0x0093e32c, 0x0093e320, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* LocalizationKey */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e61f0 */
static const schema_op S_004e61f0[] = {
    {OP_RESERVE, 0x00ae0734, 0, 3, 0, 0, 0, 0},
    {OP_INT, 0x00ae0730, 0x0093e36c, 0x0000001e, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* VehicleLoadTimer */
    {OP_INT, 0x00ae0730, 0x0093e380, 0x0000001e, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerWaitTimer */
    {OP_INT, 0x00ae0730, 0x0093e390, 0x00000005, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* PlayerWaitDeltaTimer */
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e3c50 */
static const schema_op S_004e3c50[] = {
    {OP_RESERVE, 0x00ae1004, 0, 2, 0, 0, 0, 0},
    {OP_INT, 0x00ae1000, 0x0093e2dc, 0x00000000, 3, 0x00000000, 0x00935d6c, 0x00000000}, /* Level */
    {OP_INT, 0x00ae1000, 0x0093e2e4, 0x00000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* ExperienceRequired */
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000001, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00000000, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000002, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00000028, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000003, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x0000005a, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000004, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x000000b4, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000005, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x0000012c, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000006, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x000001cc, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000007, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x0000028f, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000008, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00000366, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000009, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x0000044c, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000000a, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00000546, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000000b, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00000659, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000000c, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x0000078f, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000000d, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x000008f7, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000000e, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00000a8c, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000000f, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00000c4e, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000010, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00000e33, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000011, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x0000102c, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000012, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x0000122f, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000013, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00001432, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000014, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00001644, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000015, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x0000186f, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000016, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00001ac2, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000017, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00001d56, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000018, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00002021, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000019, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00002328, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000001a, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x0000264d, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000001b, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00002981, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000001c, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00002cb0, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000001d, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00002fd5, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000001e, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x000032fa, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000001f, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00003638, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000020, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x000039ad, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000021, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00003d68, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000022, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00004173, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000023, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x000045ba, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000024, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00004a24, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000025, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00004e8e, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000026, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x000052e9, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000027, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x0000572b, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000028, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00005b68, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000029, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00005fbe, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000002a, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00006450, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000002b, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00006937, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000002c, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00006e7d, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000002d, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00007404, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000002e, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x000079ae, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x0000002f, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00007f58, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000030, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x000084da, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000031, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00008a3e, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_CRIT, 0, 0x0093e2dc, 0x00000032, 0, 0, 0, 0}, /* Level */
    {OP_CRIT, 0, 0x0093e2e4, 0x00008f8e, 0, 0, 0, 0}, /* ExperienceRequired */
    {OP_ADDROW, 0x00ae1000, 0, 0, 0, 0, 0, 0},
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
/* 004e5f70 */
static const schema_op S_004e5f70[] = {
    {OP_RESERVE, 0x00ae193c, 0, 10, 0, 0, 0, 0},
    {OP_FLOAT, 0x00ae1938, 0x0093e33c, 0x3f800000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* ga */
    {OP_FLOAT, 0x00ae1938, 0x0093e340, 0xc1200000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* gb */
    {OP_FLOAT, 0x00ae1938, 0x0093e344, 0xbc4ccccd, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* gc */
    {OP_FLOAT, 0x00ae1938, 0x0093e348, 0x41c1d11a, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* xpa */
    {OP_FLOAT, 0x00ae1938, 0x0093e34c, 0xc09bf3f5, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* xpb */
    {OP_FLOAT, 0x00ae1938, 0x0093e350, 0x3ec88f86, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* xpc */
    {OP_FLOAT, 0x00ae1938, 0x0093e354, 0xbc4fe9b8, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* xpd */
    {OP_FLOAT, 0x00ae1938, 0x0093e358, 0x41200000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* scale */
    {OP_FLOAT, 0x00ae1938, 0x0093e360, 0x40000000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* ma */
    {OP_FLOAT, 0x00ae1938, 0x0093e364, 0x402df854, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* mb */
    {OP_FLOAT, 0x00ae1938, 0x0093e368, 0x40a00000, 0, 0x00000000, 0x00935d6c, 0x00000000}, /* mc */
    {OP_CLEAR, 0, 0, 0, 0, 0, 0, 0},
    {OP_ADDROW, 0x00ae1938, 0, 0, 0, 0, 0, 0},
    {OP_END, 0, 0, 0, 0, 0, 0, 0}
};
static const schema_op* const g_schemas[] = {S_004decd0_ae1b18, S_004decd0_adffe0, S_004de770, S_004de9e0, S_004dcfc0, S_004e1820_ae07a8, S_004e1b90_ae0820, S_004e1d70_ae0640, S_004e1f40_ae06b8, S_004df400, S_004df700, S_004df9f0, S_004e0a40_ae04d8, S_004e1190, S_004e1650, S_004e1820_ae0f10, S_004e1b90_ae1a28, S_004e1d70_ae0898, S_004e1f40_ae0550, S_004e2120, S_004e2590, S_004e5eb0, S_004e61f0, S_004e3c50, S_004e5f70};
