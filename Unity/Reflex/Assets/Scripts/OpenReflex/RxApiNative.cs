using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Reflex.Port.RxCore
{
    // P/Invoke bindings of core/include/rx_api.h (librxcore), as delegates resolved by RxNativeLoader. One wrapper per export, same
    // name and argument order. Thread rule of the API: one session belongs to ONE simulation thread (OpenReflexSimulation's worker);
    // nothing here locks.
    public static unsafe class RxApiNative
    {
        /// RX_SESSION_API_VERSION this code was written against. Check() refuses any other version.
        public const int ExpectedApiVersion = 2;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int RemapDefaultsFn(ref RxRemapTable table);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int RemapGetFn(IntPtr session, ref RxRemapTable table);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int RemapSetFn(IntPtr session, [In] int[] values);
        static readonly RxNativeLoader.Fn<RemapDefaultsFn> remapDefaults = F<RemapDefaultsFn>("rx_remap_defaults");
        static readonly RxNativeLoader.Fn<RemapGetFn> remapGet = F<RemapGetFn>("rx_session_remap_get");
        static readonly RxNativeLoader.Fn<RemapSetFn> remapSet = F<RemapSetFn>("rx_session_remap_set");
        // w-fb-hud (core/api/fbhud_session.inc): the bindings applied in a loaded event (the local human's KeyMaps built again); RESET TRACK
        static readonly RxNativeLoader.Fn<RemapSetFn> remapApply = F<RemapSetFn>("rx_session_remap_apply");
        static readonly RxNativeLoader.Fn<TerrainResetFn> terrainReset = F<TerrainResetFn>("rx_session_terrain_reset");
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TerrainResetFn(IntPtr session);
        // MAP SCALE (port addition; core/include/rx_api.h): the event's track scaled about the origin, set before rx_session_event_load
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetMapScaleFn(IntPtr session, float scale);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate float MapScaleFn(IntPtr session);
        static readonly RxNativeLoader.Fn<SetMapScaleFn> setMapScale = F<SetMapScaleFn>("rx_session_set_map_scale");
        static readonly RxNativeLoader.Fn<MapScaleFn> mapScale = F<MapScaleFn>("rx_session_map_scale");
        public static int rx_session_set_map_scale(IntPtr session, float scale) => setMapScale.Get()(session, scale);
        public static float rx_session_map_scale(IntPtr session) => mapScale.Get()(session);
        // BASE SCALE (port addition, w-alivegates): the track's default scale (its records are kept at it), set before rx_session_event_load
        static readonly RxNativeLoader.Fn<SetMapScaleFn> setMapScaleBase = F<SetMapScaleFn>("rx_session_set_map_scale_base");
        public static int rx_session_set_map_scale_base(IntPtr session, float scale) => setMapScaleBase.Get()(session, scale);
        // MODS/TRACKS (port addition; core/include/rx_api.h): a custom track's files bound to its level name for this session, before rx_session_event_load
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int MountLevelFn(IntPtr session, [MarshalAs(UnmanagedType.LPUTF8Str)] string level, [MarshalAs(UnmanagedType.LPUTF8Str)] string folder);
        static readonly RxNativeLoader.Fn<MountLevelFn> mountLevel = F<MountLevelFn>("rx_session_mount_level");
        public static int rx_session_mount_level(IntPtr session, string level, string folder) => mountLevel.Get()(session, level, folder);
        // FREER AIR (port addition; core/include/rx_api.h): the in-air helpers left out for the local players (0 = the original's)
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetFreeAirFn(IntPtr session, int mode);
        static readonly RxNativeLoader.Fn<SetFreeAirFn> setFreeAir = F<SetFreeAirFn>("rx_session_set_free_air");
        public const int FreeAirNoHold = 1, FreeAirNoSettle = 2;
        public static int rx_session_set_free_air(IntPtr session, int mode) => setFreeAir.Get()(session, mode);
        // AUTO LEVEL / COUNTER-STEER SNAP strengths (port addition, w-airsliders): 0 .. 1 each, 1 = the original's helper, 0 = removed (the mode bits above)
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetAirHelpersFn(IntPtr session, float autoLevel, float snap);
        static readonly RxNativeLoader.Fn<SetAirHelpersFn> setAirHelpers = F<SetAirHelpersFn>("rx_session_set_air_helpers");
        public static int rx_session_set_air_helpers(IntPtr session, float autoLevel, float snap) => setAirHelpers.Get()(session, autoLevel, snap);
        // AIR LOOSENESS (port addition, w-freeair2): 0 .. 1, the local players' air damping / stick lag eased (0 = FREER AIR's model)
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetFreeAirLoosenessFn(IntPtr session, float looseness);
        static readonly RxNativeLoader.Fn<SetFreeAirLoosenessFn> setFreeAirLooseness = F<SetFreeAirLoosenessFn>("rx_session_set_free_air_looseness");
        public static int rx_session_set_free_air_looseness(IntPtr session, float looseness) => setFreeAirLooseness.Get()(session, looseness);
        // FREE RIDE WITHOUT BORDERS (port addition, w-frnowalls): no reset zones / reset triggers (but deep water) / edge resets in Free Ride (0 = the original's)
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetFreeRideBordersFn(IntPtr session, int mode);
        static readonly RxNativeLoader.Fn<SetFreeRideBordersFn> setFreeRideBorders = F<SetFreeRideBordersFn>("rx_session_set_free_ride_borders");
        public const int FreeRideBordersOriginal = 0, FreeRideBordersOpen = 1;
        public static int rx_session_set_free_ride_borders(IntPtr session, int mode) => setFreeRideBorders.Get()(session, mode);
        // WEATHER GRIP (port addition, 2026-11-07; core/game/weather_grip.h): every tyre's static rolling and cornering grip times the factor
        // (rain / snow; 1 = the original's, nothing touched), any time between frames; 0 ok, 2 refused (outside 0.1 .. 1.5)
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetWeatherGripFn(IntPtr session, float factor);
        static readonly RxNativeLoader.Fn<SetWeatherGripFn> setWeatherGrip = F<SetWeatherGripFn>("rx_session_set_weather_grip");
        public static int rx_session_set_weather_grip(IntPtr session, float factor) => setWeatherGrip.Get()(session, factor);
        // THE RIDER CAP (port addition, w-rc-core; core/include/rx_api.h, core/plan/RIDER_CAP.md): races of up to MaxRiders (32) riders;
        // an exhibition takes spec.riders up to 32 as is; FIELD SIZE (opt-in, 0 = the game's MaxRiders tables, 12) lets the career's and
        // the shell's field sizes go to 13 .. 32
        public const int MaxRiders = 32, OriginalRiders = 12;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int MaxRidersFn();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetFieldSizeFn(IntPtr session, int riders);
        static readonly RxNativeLoader.Fn<MaxRidersFn> maxRiders = F<MaxRidersFn>("rx_session_max_riders");
        static readonly RxNativeLoader.Fn<SetFieldSizeFn> setFieldSize = F<SetFieldSizeFn>("rx_session_set_field_size");
        static readonly RxNativeLoader.Fn<SessionIntGetFn> fieldSize = F<SessionIntGetFn>("rx_session_field_size");
        public static int rx_session_max_riders() => maxRiders.Get()();
        /// 0 = the game tables (the original), else 13 .. 32 (0 ok, 1 invalid session, 2 out of range)
        public static int rx_session_set_field_size(IntPtr session, int riders) => setFieldSize.Get()(session, riders);
        public static int rx_session_field_size(IntPtr session) => fieldSize.Get()(session);
        // PRACTICE / LAP LOG / the local vehicle's physics class (port additions, w-lb-practice; core/include/rx_api.h)
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetPracticeFn(IntPtr session, int on);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SessionIntGetFn(IntPtr session);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int LapLogFn(IntPtr session, uint afterSerial, [Out] RxLapRecord[] output, int max);
        static readonly RxNativeLoader.Fn<SetPracticeFn> setPractice = F<SetPracticeFn>("rx_session_set_practice");
        static readonly RxNativeLoader.Fn<SessionIntGetFn> practice = F<SessionIntGetFn>("rx_session_practice");
        static readonly RxNativeLoader.Fn<LapLogFn> lapLog = F<LapLogFn>("rx_session_lap_log");
        static readonly RxNativeLoader.Fn<SessionIntGetFn> localPhysClass = F<SessionIntGetFn>("rx_session_local_phys_class");
        /// Before rx_session_event_load: the exhibition lap race is endless (0 ok, 1 invalid session, 2 wrong state).
        public static int rx_session_set_practice(IntPtr session, bool on) => setPractice.Get()(session, on ? 1 : 0);
        public static bool rx_session_practice(IntPtr session) => practice.Get()(session) != 0;
        /// The records after afterSerial (oldest first) into output; the count, -1 on an error.
        public static int rx_session_lap_log(IntPtr session, uint afterSerial, RxLapRecord[] output) => lapLog.Get()(session, afterSerial, output, output?.Length ?? 0);
        /// The local human's vehicle physics class (0 mx125 .. 8 truck2wdlite, core/game/phys_studio.h RX_PS_*), -1 unknown.
        public static int rx_session_local_phys_class(IntPtr session) => localPhysClass.Get()(session);
        // SPAWN MARKER (port addition, w-spawnmarker): the local rider through the game's own reset to a spot (0 ok, 2 not a Free Ride / bad pose, 3 not now)
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate int SpawnAtFn(IntPtr session, float* pos, float* dir);
        static readonly RxNativeLoader.Fn<SpawnAtFn> spawnAt = F<SpawnAtFn>("rx_session_spawn_at");
        public static unsafe int rx_session_spawn_at(IntPtr session, float px, float py, float pz, float dx, float dz)
        {
            float* p = stackalloc float[3]; float* d = stackalloc float[3];
            p[0] = px; p[1] = py; p[2] = pz; d[0] = dx; d[1] = 0f; d[2] = dz;
            return spawnAt.Get()(session, p, d);
        }
        // (w-markerbtns) the same respawn at once: the reset's steps without the reset key's 0.5 s white fade (the same answers)
        static readonly RxNativeLoader.Fn<SpawnAtFn> spawnAtInstant = F<SpawnAtFn>("rx_session_spawn_at_instant");
        public static unsafe int rx_session_spawn_at_instant(IntPtr session, float px, float py, float pz, float dx, float dz)
        {
            float* p = stackalloc float[3]; float* d = stackalloc float[3];
            p[0] = px; p[1] = py; p[2] = pz; d[0] = dx; d[1] = 0f; d[2] = dz;
            return spawnAtInstant.Get()(session, p, d);
        }
        // RT_Bump cascade layout (w-rtbumpfit): the game's own RTBump_FitCascadeMatrices for a camera (between frames; 0 ok, 2 no terrain)
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] unsafe delegate int RTBumpFitFn(IntPtr session, float* pos, float* fwd, float* up, float fov, float aspect, float near, float* out12);
        static readonly RxNativeLoader.Fn<RTBumpFitFn> rtbumpFit = F<RTBumpFitFn>("rx_session_rtbump_fit");
        public static unsafe int rx_session_rtbump_fit(IntPtr session, float[] pos, float[] fwd, float[] up, float fov, float aspect, float near, float[] out12)
        {
            float* p = stackalloc float[3]; float* f = stackalloc float[3]; float* u = stackalloc float[3]; float* o = stackalloc float[12];
            for (int i = 0; i < 3; i++) { p[i] = pos[i]; f[i] = fwd[i]; u[i] = up[i]; }
            int rc = rtbumpFit.Get()(session, p, f, u, fov, aspect, near, o);
            for (int i = 0; i < 12; i++) out12[i] = o[i];
            return rc;
        }
        public static int rx_remap_defaults(ref RxRemapTable table) => remapDefaults.Get()(ref table);
        public static int rx_session_remap_get(IntPtr session, ref RxRemapTable table) => remapGet.Get()(session, ref table);
        public static int rx_session_remap_set(IntPtr session, int[] values)
        {
            if (values == null || values.Length != 38) throw new ArgumentException("Expected all 38 controller bindings", nameof(values));
            return remapSet.Get()(session, values);
        }
        /// rx_session_remap_set before the event loads; in a loaded event the table is written and the local human's KeyMaps are built again
        /// (the bindings apply from the next frame). 0 applied.
        public static int rx_session_remap_apply(IntPtr session, int[] values)
        {
            if (values == null || values.Length != 38) throw new ArgumentException("Expected all 38 controller bindings", nameof(values));
            return remapApply.Get()(session, values);
        }
        /// RESET TRACK: the game's own level reset of the event terrain at the next frame (the deformation and the tyre tracks); 0 armed.
        public static int rx_session_terrain_reset(IntPtr session) => terrainReset.Get()(session);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate uint VersionFn();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr CreateFn([MarshalAs(UnmanagedType.LPUTF8Str)] string installRoot);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void SessionVoidFn(IntPtr s);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate uint SessionUIntFn(IntPtr s);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void EventDefaultFn(ref RxEventSpec spec);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int EventLoadFn(IntPtr s, ref RxEventSpec spec);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SessionIntFn(IntPtr s);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int AdvanceDtFn(IntPtr s, float dt);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr LastErrorFn(IntPtr s);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate ulong FramesFn(IntPtr s);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate float SessionFloatFn(IntPtr s);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int OnboardFn(IntPtr s, RxOnboardState* state);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int CameraBindingsFn(IntPtr s, ref RxCameraBindings state);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int RaceLineExFn(IntPtr s, float* output, uint maxPoints, int* closed);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int BikeFn(IntPtr s, RxBikeState* state);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int PoseFn(IntPtr s, float* output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int PoseSlotFn(IntPtr s, int slot, float* output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int CameraFn(IntPtr s, RxCamera* camera);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetCameraFn(IntPtr s, int mode);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int RaceFn(IntPtr s, RxRaceState* race);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int HudFn(IntPtr s, RxHudState* hud);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int RaceLineFn(IntPtr s, float* output, uint maxPoints);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int VehicleInfoFn(IntPtr s, int slot, RxVehicleInfo* info);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int StandingsFn(IntPtr s, RxDriverResult* output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SlotsFn(IntPtr s, RxSlot* output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TerrainFn(IntPtr s, RxTerrainInfo* info);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TerrainTilesFn(IntPtr s, RxTerrainRect* output, int max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TerrainRectFn(IntPtr s, float* output, int x0, int z0, int w, int h);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ParticlesFn(IntPtr s, RxParticleInfo* info);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ParticlesArrayFn(IntPtr s, RxParticle* output, uint max, RxParticleInfo* info);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int AmbientFn(IntPtr s, RxAmbientInfo* info);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int AmbientEntitiesFn(IntPtr s, RxAmbientEntity* output, uint max, float* matrices, uint maxMatrices, RxAmbientInfo* info);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int PropsFn(IntPtr s, [Out] RxProp[] output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SceneObjectsFn(IntPtr s, [Out] RxSceneObject[] output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SceneObjectMatrixFn(IntPtr s, uint vtable, int entityId, uint offset, float* output);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int NodeFlagsFn(IntPtr s, int slot, byte* output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ScenarioModelsFn(IntPtr s, RxScenarioModel* output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int CrowdFlashesFn(IntPtr s, RxCrowdFlash* output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int AmbientNodeFlagsFn(IntPtr s, uint objectId, byte* output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ScenarioSoundsFn(IntPtr s, RxScenarioSound* output, uint max);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int MudFn(IntPtr s, int slot, RxMudState* state);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int PodiumFn(IntPtr s, RxPodiumInfo* info, RxPodiumRider* riders, uint maxRiders, float* matrices, uint maxMatrices, RxPodiumSlot* slots, uint maxSlots);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int StartGatesFn(IntPtr s, ref RxStartGates output);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TracksFn(IntPtr s, float* output, uint maxTriangles, RxTrackInfo* info);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TrackRibbonsFn(IntPtr s, RxTrackRibbon* output, uint max, RxTrackInfo* info);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetInputFn(IntPtr s, byte* keys, RxPad* pads, uint padMask);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SetMouseFn(IntPtr s, int dx, int dy, int wheel, uint buttonFlags);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void SetCursorFn(IntPtr s, int x, int y);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int RumbleFn(IntPtr s, uint pad, ushort* left, ushort* right, uint* calls);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int UiSettingFn(IntPtr s, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, int value, int mode, out int before);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate uint PaceFn(IntPtr s, float realDt, uint maxSteps);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int SizeofFn([MarshalAs(UnmanagedType.LPUTF8Str)] string name);

        static RxNativeLoader.Fn<T> F<T>(string name) where T : Delegate => new RxNativeLoader.Fn<T>(name);
        // The per-frame exports take pinned pointers (no marshalling stub copies the arrays / structs in and out on every call); a
        // count is never more than the array holds (the marshalled arrays were bounded by their length the same way).
        static uint Cap<T>(uint max, T[] a) => a == null ? 0u : Math.Min(max, (uint)a.Length);
        static void CheckRect(float[] output, int w, int h) { if (output == null || w < 0 || h < 0 || (long)w * h > output.Length) throw new ArgumentException("the output holds fewer than w * h floats"); }

        static readonly RxNativeLoader.Fn<VersionFn> apiVersion = F<VersionFn>("rx_session_api_version");
        static readonly RxNativeLoader.Fn<CreateFn> create = F<CreateFn>("rx_session_create");
        static readonly RxNativeLoader.Fn<SessionVoidFn> destroy = F<SessionVoidFn>("rx_session_destroy");
        static readonly RxNativeLoader.Fn<SessionUIntFn> version = F<SessionUIntFn>("rx_session_version");
        static readonly RxNativeLoader.Fn<EventDefaultFn> eventDefault = F<EventDefaultFn>("rx_session_event_default");
        static readonly RxNativeLoader.Fn<EventLoadFn> eventLoad = F<EventLoadFn>("rx_session_event_load");
        static readonly RxNativeLoader.Fn<SessionIntFn> advance = F<SessionIntFn>("rx_session_advance");
        static readonly RxNativeLoader.Fn<AdvanceDtFn> advanceDt = F<AdvanceDtFn>("rx_session_advance_dt");
        static readonly RxNativeLoader.Fn<SessionIntFn> getState = F<SessionIntFn>("rx_session_get_state");
        static readonly RxNativeLoader.Fn<LastErrorFn> lastError = F<LastErrorFn>("rx_session_last_error");
        static readonly RxNativeLoader.Fn<FramesFn> frames = F<FramesFn>("rx_session_frames");
        static readonly RxNativeLoader.Fn<SessionFloatFn> frameDt = F<SessionFloatFn>("rx_session_frame_dt");
        static readonly RxNativeLoader.Fn<SessionFloatFn> gameTime = F<SessionFloatFn>("rx_session_game_time");
        static readonly RxNativeLoader.Fn<OnboardFn> onboardState = F<OnboardFn>("rx_session_onboard_state");
        static readonly RxNativeLoader.Fn<CameraBindingsFn> cameraBindings = F<CameraBindingsFn>("rx_session_camera_bindings");
        static readonly RxNativeLoader.Fn<RaceLineExFn> raceLineEx = F<RaceLineExFn>("rx_session_race_line_ex");
        static readonly RxNativeLoader.Fn<BikeFn> bike = F<BikeFn>("rx_session_bike");
        static readonly RxNativeLoader.Fn<PoseFn> riderPose = F<PoseFn>("rx_session_rider_pose");
        static readonly RxNativeLoader.Fn<PoseFn> bikePose = F<PoseFn>("rx_session_bike_pose");
        static readonly RxNativeLoader.Fn<PoseSlotFn> riderPoseSlot = F<PoseSlotFn>("rx_session_rider_pose_slot");
        static readonly RxNativeLoader.Fn<PoseSlotFn> bikePoseSlot = F<PoseSlotFn>("rx_session_bike_pose_slot");
        static readonly RxNativeLoader.Fn<CameraFn> camera = F<CameraFn>("rx_session_camera");
        static readonly RxNativeLoader.Fn<SetCameraFn> setDrivingCamera = F<SetCameraFn>("rx_session_set_driving_camera");
        static readonly RxNativeLoader.Fn<SetCameraFn> setCamera = F<SetCameraFn>("rx_session_set_camera");
        static readonly RxNativeLoader.Fn<RaceFn> race = F<RaceFn>("rx_session_race");
        static readonly RxNativeLoader.Fn<HudFn> hud = F<HudFn>("rx_session_hud");
        static readonly RxNativeLoader.Fn<RaceLineFn> raceLine = F<RaceLineFn>("rx_session_race_line");
        static readonly RxNativeLoader.Fn<VehicleInfoFn> vehicleInfo = F<VehicleInfoFn>("rx_session_vehicle_info");
        static readonly RxNativeLoader.Fn<StandingsFn> standings = F<StandingsFn>("rx_session_standings");
        static readonly RxNativeLoader.Fn<SlotsFn> slots = F<SlotsFn>("rx_session_slots");
        static readonly RxNativeLoader.Fn<TerrainFn> terrain = F<TerrainFn>("rx_session_terrain");
        static readonly RxNativeLoader.Fn<TerrainTilesFn> terrainTiles = F<TerrainTilesFn>("rx_session_terrain_tiles");
        static readonly RxNativeLoader.Fn<TerrainRectFn> terrainHeights = F<TerrainRectFn>("rx_session_terrain_heights");
        static readonly RxNativeLoader.Fn<TerrainRectFn> terrainDeform = F<TerrainRectFn>("rx_session_terrain_deform");
        static readonly RxNativeLoader.Fn<ParticlesFn> particles = F<ParticlesFn>("rx_session_particles");
        static readonly RxNativeLoader.Fn<ParticlesArrayFn> particlesArray = F<ParticlesArrayFn>("rx_session_particles_array");
        static readonly RxNativeLoader.Fn<AmbientFn> ambient = F<AmbientFn>("rx_session_ambient");
        static readonly RxNativeLoader.Fn<AmbientEntitiesFn> ambientEntities = F<AmbientEntitiesFn>("rx_session_ambient_entities");
        static readonly RxNativeLoader.Fn<SessionIntFn> ambientRescan = F<SessionIntFn>("rx_session_ambient_rescan");
        static readonly RxNativeLoader.Fn<PropsFn> props = F<PropsFn>("rx_session_props");
        static readonly RxNativeLoader.Fn<SceneObjectsFn> sceneObjects = F<SceneObjectsFn>("rx_session_scene_objects");
        static readonly RxNativeLoader.Fn<SceneObjectMatrixFn> sceneObjectMatrix = F<SceneObjectMatrixFn>("rx_session_scene_object_matrix");
        static readonly RxNativeLoader.Fn<NodeFlagsFn> nodeFlags = F<NodeFlagsFn>("rx_session_node_flags");
        static readonly RxNativeLoader.Fn<ScenarioModelsFn> scenarioModels = F<ScenarioModelsFn>("rx_session_scenario_models");
        static readonly RxNativeLoader.Fn<AmbientNodeFlagsFn> ambientNodeFlags = F<AmbientNodeFlagsFn>("rx_session_ambient_node_flags");
        static readonly RxNativeLoader.Fn<CrowdFlashesFn> crowdFlashes = F<CrowdFlashesFn>("rx_session_crowd_flashes");
        static readonly RxNativeLoader.Fn<ScenarioSoundsFn> scenarioSounds = F<ScenarioSoundsFn>("rx_session_scenario_sounds");
        static readonly RxNativeLoader.Fn<MudFn> mud = F<MudFn>("rx_session_mud");
        static readonly RxNativeLoader.Fn<PodiumFn> podium = F<PodiumFn>("rx_session_podium");
        static readonly RxNativeLoader.Fn<StartGatesFn> startGates = F<StartGatesFn>("rx_session_start_gates");
        static readonly RxNativeLoader.Fn<SessionIntFn> skipScenario = F<SessionIntFn>("rx_session_skip_scenario");
        static readonly RxNativeLoader.Fn<SessionIntFn> mudCount = F<SessionIntFn>("rx_session_mud_count");
        static readonly RxNativeLoader.Fn<TracksFn> tracks = F<TracksFn>("rx_session_tracks");
        static readonly RxNativeLoader.Fn<TrackRibbonsFn> trackRibbons = F<TrackRibbonsFn>("rx_session_track_ribbons");
        static readonly RxNativeLoader.Fn<SetInputFn> setInput = F<SetInputFn>("rx_session_set_input");
        static readonly RxNativeLoader.Fn<SetMouseFn> setMouse = F<SetMouseFn>("rx_session_set_mouse");
        static readonly RxNativeLoader.Fn<UiSettingFn> uiSetting = F<UiSettingFn>("rx_session_ui_setting");
        static readonly RxNativeLoader.Fn<SetCursorFn> setCursor = F<SetCursorFn>("rx_session_set_cursor");
        static readonly RxNativeLoader.Fn<RumbleFn> rumble = F<RumbleFn>("rx_session_rumble");
        static readonly RxNativeLoader.Fn<SessionFloatFn> stepDt = F<SessionFloatFn>("rx_session_step_dt");
        static readonly RxNativeLoader.Fn<PaceFn> pace = F<PaceFn>("rx_session_pace");
        static readonly RxNativeLoader.Fn<SessionFloatFn> alpha = F<SessionFloatFn>("rx_session_alpha");
        static readonly RxNativeLoader.Fn<SessionFloatFn> engineDt = F<SessionFloatFn>("rx_session_engine_dt");
        static readonly RxNativeLoader.Fn<SizeofFn> sizeOf = F<SizeofFn>("rx_session_sizeof");

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ProfileImportFn(IntPtr s, [In] RxProfileCell[] cells, int count);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TuningSetFn(IntPtr s, [In] int[] values);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int TuningGetFn(IntPtr s, [Out] int[] values);
        static readonly RxNativeLoader.Fn<ProfileImportFn> profileImport = F<ProfileImportFn>("rx_session_profile_import");
        static readonly RxNativeLoader.Fn<TuningSetFn> tuningSet = F<TuningSetFn>("rx_session_tuning_set");
        static readonly RxNativeLoader.Fn<TuningGetFn> tuningGet = F<TuningGetFn>("rx_session_tuning_get");
        public static int rx_session_profile_import(IntPtr s, RxProfileCell[] cells) => profileImport.Get()(s,cells,cells?.Length ?? 0);
        public static int rx_session_tuning_set(IntPtr s, int[] values) { if(values == null || values.Length != 8) throw new ArgumentException("Eight tuning values required"); return tuningSet.Get()(s,values); }
        public static int rx_session_tuning_get(IntPtr s, int[] values) { if(values == null || values.Length != 8) throw new ArgumentException("Eight tuning values required"); return tuningGet.Get()(s,values); }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int AppearanceFn(IntPtr s, int slot, ref RxAppearance appearance);
        static readonly RxNativeLoader.Fn<AppearanceFn> appearance = F<AppearanceFn>("rx_session_appearance");
        public static int rx_session_appearance(IntPtr s, int slot, ref RxAppearance output) => appearance.Get()(s, slot, ref output);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int PlayerExportFn(IntPtr session, [Out] byte[] output, uint capacity, out uint required);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int PlayerImportFn(IntPtr session, [In] byte[] input, uint bytes);
        static readonly RxNativeLoader.Fn<PlayerExportFn> playerExport = F<PlayerExportFn>("rx_session_player_export");
        static readonly RxNativeLoader.Fn<PlayerImportFn> playerImport = F<PlayerImportFn>("rx_session_player_import");
        static readonly RxNativeLoader.Fn<SessionIntFn> finalizeRecords = F<SessionIntFn>("rx_session_finalize_records");
        public static void RequirePlayerPersistence()
        {
            // Detect an older core before importing or publishing any profile state.
            playerExport.Get(); playerImport.Get(); finalizeRecords.Get();
        }
        public static byte[] ExportPlayer(IntPtr session)
        {
            uint required;
            if (playerExport.Get()(session, null, 0, out required) != 0 || required < 32 || required > 64 * 1024 * 1024)
                throw new InvalidOperationException("Player snapshot size query failed: " + rx_session_last_error(session));
            var bytes = new byte[required];
            uint actual;
            if (playerExport.Get()(session, bytes, required, out actual) != 0 || actual != required)
                throw new InvalidOperationException("Player snapshot export failed: " + rx_session_last_error(session));
            return bytes;
        }
        public static int ImportPlayer(IntPtr session, byte[] bytes)
        {
            if (bytes == null || bytes.Length < 32 || bytes.Length > 64 * 1024 * 1024) throw new ArgumentException("Invalid player snapshot size.");
            return playerImport.Get()(session, bytes, (uint)bytes.Length);
        }
        public static int rx_session_finalize_records(IntPtr session) => finalizeRecords.Get()(session);

        // ---- lifecycle
        public static uint rx_session_api_version() => apiVersion.Get()();
        public static IntPtr rx_session_create(string installRoot) => create.Get()(installRoot);
        public static void rx_session_destroy(IntPtr s) => destroy.Get()(s);
        public static uint rx_session_version(IntPtr s) => version.Get()(s);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int EventUiFn(IntPtr s, RxEventUiState* state);
        static readonly RxNativeLoader.Fn<EventUiFn> eventUi = F<EventUiFn>("rx_session_event_ui_state");
        public static int rx_session_event_ui_state(IntPtr s, ref RxEventUiState state) { fixed (RxEventUiState* p = &state) return eventUi.Get()(s, p); }

        // ---- event
        public static void rx_session_event_default(ref RxEventSpec spec) => eventDefault.Get()(ref spec);
        public static int rx_session_event_load(IntPtr s, ref RxEventSpec spec) => eventLoad.Get()(s, ref spec);

        // ---- frame loop
        public static int rx_session_advance(IntPtr s) => advance.Get()(s);
        public static int rx_session_advance_dt(IntPtr s, float dt) => advanceDt.Get()(s, dt);
        public static RxSessionState rx_session_get_state(IntPtr s) => (RxSessionState)getState.Get()(s);
        /// rx_session_last_error as a string ("" if none). s = IntPtr.Zero returns the last create error.
        public static string rx_session_last_error(IntPtr s)
        {
            IntPtr p = lastError.Get()(s);
            return p == IntPtr.Zero ? "" : (Marshal.PtrToStringUTF8(p) ?? "");
        }
        public static ulong rx_session_frames(IntPtr s) => frames.Get()(s);
        public static float rx_session_frame_dt(IntPtr s) => frameDt.Get()(s);
        public static float rx_session_game_time(IntPtr s) => gameTime.Get()(s);

        // ---- first-ride state
        public static int rx_session_onboard_state(IntPtr s, ref RxOnboardState state) { fixed (RxOnboardState* p = &state) return onboardState.Get()(s, p); }
        public static int rx_session_camera_bindings(IntPtr s, ref RxCameraBindings state) => cameraBindings.Get()(s, ref state);
        public static int rx_session_race_line_ex(IntPtr s, float[] output, uint maxPoints, out int closed) { int c = 0; int r; fixed (float* p = output) r = raceLineEx.Get()(s, p, maxPoints, &c); closed = c; return r; }
        public static int rx_session_bike(IntPtr s, ref RxBikeState state) { fixed (RxBikeState* p = &state) return bike.Get()(s, p); }
        public static int rx_session_rider_pose(IntPtr s, float[] output, uint maxBones) { fixed (float* p = output) return riderPose.Get()(s, p, maxBones); }
        public static int rx_session_bike_pose(IntPtr s, float[] output, uint maxNodes) { fixed (float* p = output) return bikePose.Get()(s, p, maxNodes); }
        public static int rx_session_rider_pose_slot(IntPtr s, int slot, float[] output, uint maxBones) { fixed (float* p = output) return riderPoseSlot.Get()(s, slot, p, maxBones); }
        public static int rx_session_bike_pose_slot(IntPtr s, int slot, float[] output, uint maxNodes) { fixed (float* p = output) return bikePoseSlot.Get()(s, slot, p, maxNodes); }
        public static int rx_session_camera(IntPtr s, ref RxCamera cam) { fixed (RxCamera* p = &cam) return camera.Get()(s, p); }
        /// Selects the view through the game's own camera setter (the Camera key's Player_SetCamera): 0 = default third person,
        /// 2 = first person. 0 ok; 2 refused (no local player, or the game refuses outside an activity state 1..3).
        public static int rx_session_set_driving_camera(IntPtr s, int preset) => setDrivingCamera.Get()(s, preset);
        public static int rx_session_set_camera(IntPtr s, int mode) => setCamera.Get()(s, mode);
        public static int rx_session_race(IntPtr s, ref RxRaceState state) { fixed (RxRaceState* p = &state) return race.Get()(s, p); }
        public static int rx_session_hud(IntPtr s, ref RxHudState state) { fixed (RxHudState* p = &state) return hud.Get()(s, p); }
        /// The race line ahead of the local player (node centres, 3 floats each); the count written (0 outside a race).
        public static int rx_session_race_line(IntPtr s, float[] output, uint maxPoints) { fixed (float* p = output) return raceLine.Get()(s, p, maxPoints); }
        public static int rx_session_vehicle_info(IntPtr s, int slot, ref RxVehicleInfo info) { fixed (RxVehicleInfo* p = &info) return vehicleInfo.Get()(s, slot, p); }
        public static int rx_session_standings(IntPtr s, RxDriverResult[] output, uint max) { fixed (RxDriverResult* p = output) return standings.Get()(s, p, Cap(max, output)); }
        public static int rx_session_slots(IntPtr s, RxSlot[] output, uint max) { fixed (RxSlot* p = output) return slots.Get()(s, p, Cap(max, output)); }

        // ---- terrain
        public static int rx_session_terrain(IntPtr s, ref RxTerrainInfo info) { fixed (RxTerrainInfo* p = &info) return terrain.Get()(s, p); }
        /// The changed 32 x 32 tiles of this frame as at most `max` rectangles (a core of 2026-10-29 or later; EntryPointNotFoundException
        /// before: fall back to RxTerrainInfo's one dirty rectangle). The count written (0: nothing changed), &lt;0 on error.
        public static int rx_session_terrain_tiles(IntPtr s, RxTerrainRect[] output, int max)
        {
            if (output == null) throw new ArgumentNullException(nameof(output));
            if (max > output.Length) max = output.Length;
            fixed (RxTerrainRect* p = output) return terrainTiles.Get()(s, p, max);
        }
        public static int rx_session_terrain_heights(IntPtr s, float[] output, int x0, int z0, int w, int h) { CheckRect(output, w, h); fixed (float* p = output) return terrainHeights.Get()(s, p, x0, z0, w, h); }
        public static int rx_session_terrain_deform(IntPtr s, float[] output, int x0, int z0, int w, int h) { CheckRect(output, w, h); fixed (float* p = output) return terrainDeform.Get()(s, p, x0, z0, w, h); }

        // ---- fx / ambient / mud
        public static int rx_session_particles(IntPtr s, ref RxParticleInfo info) { fixed (RxParticleInfo* p = &info) return particles.Get()(s, p); }
        /// The live particles of the last frame (normal manager first, far to near) and the info block; the count written, &lt;0 on error.
        public static int rx_session_particles_array(IntPtr s, RxParticle[] output, uint max, ref RxParticleInfo info) { fixed (RxParticle* p = output) fixed (RxParticleInfo* i = &info) return particlesArray.Get()(s, p, Cap(max, output), i); }
        public static int rx_session_ambient(IntPtr s, ref RxAmbientInfo info) { fixed (RxAmbientInfo* p = &info) return ambient.Get()(s, p); }
        /// The live ambient entities and their node matrices (the old reflex_world_ambient); the entity count written (info has the totals).
        public static int rx_session_ambient_entities(IntPtr s, RxAmbientEntity[] output, uint max, float[] matrices, uint maxMatrices, ref RxAmbientInfo info)
        {
            if (matrices != null && maxMatrices > (uint)(matrices.Length / 16)) maxMatrices = (uint)(matrices.Length / 16);
            fixed (RxAmbientEntity* p = output) fixed (float* m = matrices) fixed (RxAmbientInfo* i = &info) return ambientEntities.Get()(s, p, Cap(max, output), m, maxMatrices, i);
        }
        public static int rx_session_ambient_rescan(IntPtr s) => ambientRescan.Get()(s);
        /// The event scene's live entities (props, gates, bales, boards, ...); the count written.
        public static int rx_session_props(IntPtr s, RxProp[] output, uint max) => props.Get()(s, output, max);
        public static int rx_session_scene_objects(IntPtr s, RxSceneObject[] output, uint max) => sceneObjects.Get()(s, output, max);
        /// The 4x4 world matrix at `offset` of the scene object with that class vtable and entity id; 0 = found.
        public static int rx_session_scene_object_matrix(IntPtr s, uint vtable, int entityId, uint offset, float[] output) { if (output == null || output.Length < 16) throw new ArgumentException("16 floats required"); fixed (float* p = output) return sceneObjectMatrix.Get()(s, vtable, entityId, offset, p); }
        /// A vehicle model's node flags (record +0xc bytes); slot -1 = the local player; the model's node count.
        public static int rx_session_node_flags(IntPtr s, int slot, byte[] output, uint max) { fixed (byte* p = output) return nodeFlags.Get()(s, slot, p, Cap(max, output)); }
        /// rx_podium_api.h v3: every static scenery ModelEntity's drawn flag and instance matrix (what a scenario script hides / moves);
        /// returns the scene's ModelEntity count (may exceed max).
        public static int rx_session_scenario_models(IntPtr s, RxScenarioModel[] output, uint max) { fixed (RxScenarioModel* p = output) return scenarioModels.Get()(s, p, Cap(max, output)); }
        /// rx_crowd_api.h v1: the ACTIVE camera-flash records of the stands crowd sections; the number of active records (all of them; at most `max` written).
        public static int rx_session_crowd_flashes(IntPtr s, RxCrowdFlash[] output, uint max) { fixed (RxCrowdFlash* p = output) return crowdFlashes.Get()(s, p, Cap(max, output)); }
        /// rx_podium_api.h v3: an ambient character's node flags (record +0xc bytes, bit 0x10 hidden) by its rx_ambient_entity object id.
        public static int rx_session_ambient_node_flags(IntPtr s, uint objectId, byte[] output, uint max) { fixed (byte* p = output) return ambientNodeFlags.Get()(s, objectId, p, Cap(max, output)); }
        /// rx_podium_api.h v3: the scenario scripts' sound effects since the last call (event path, 3D position), oldest first.
        public static int rx_session_scenario_sounds(IntPtr s, RxScenarioSound[] output, uint max) { fixed (RxScenarioSound* p = output) return scenarioSounds.Get()(s, p, Cap(max, output)); }
        public static int rx_session_mud(IntPtr s, int slot, ref RxMudState state) { fixed (RxMudState* p = &state) return mud.Get()(s, slot, p); }
        /// rx_podium_api.h: the podium RiderCharacters (69 bone matrices each into `matrices`), each slot's live rider / vehicle drawn flags,
        /// and which post scenario plays. Returns 0 (1: bad session).
        /// rx_gate_api.h: the start gate's frames for the career Gate Selection page's camera (read-only; call between advances).
        public static int rx_session_start_gates(IntPtr s, ref RxStartGates output) => startGates.Get()(s, ref output);
        /// rx_scene_pages_api.h: App.SkipToNextScenario ([0xa7f748] = 1), the Event Intro page's accept.
        public static int rx_session_skip_scenario(IntPtr s) => skipScenario.Get()(s);
        public static int rx_session_podium(IntPtr s, ref RxPodiumInfo info, RxPodiumRider[] riders, uint maxRiders, float[] matrices, uint maxMatrices, RxPodiumSlot[] slots, uint maxSlots)
        {
            if (matrices != null && maxMatrices > (uint)(matrices.Length / 16)) maxMatrices = (uint)(matrices.Length / 16);
            fixed (RxPodiumInfo* i = &info) fixed (RxPodiumRider* r = riders) fixed (float* m = matrices) fixed (RxPodiumSlot* sl = slots)
                return podium.Get()(s, i, r, Cap(maxRiders, riders), m, maxMatrices, sl, Cap(maxSlots, slots));
        }
        public static int rx_session_mud_count(IntPtr s) => mudCount.Get()(s);
        /// The live TerrainTracks ribbon triangles (18 floats each: 3 vertices * x, z, side, V, intensity, type). output may be
        /// null (info only). Returns the triangles written; info.triangles is the true total.
        public static int rx_session_tracks(IntPtr s, float[] output, uint maxTriangles, ref RxTrackInfo info)
        {
            if (output != null && maxTriangles > (uint)(output.Length / 18)) maxTriangles = (uint)(output.Length / 18);
            if (output == null) maxTriangles = 0;
            fixed (float* p = output) fixed (RxTrackInfo* i = &info) return tracks.Get()(s, p, maxTriangles, i);
        }
        public static int rx_session_track_ribbons(IntPtr s, RxTrackRibbon[] output, uint max, ref RxTrackInfo info) { fixed (RxTrackRibbon* p = output) fixed (RxTrackInfo* i = &info) return trackRibbons.Get()(s, p, Cap(max, output), i); }

        // ---- input / self-check
        public static int rx_session_set_input(IntPtr s, byte[] keys, RxPad[] pads, uint padMask) { fixed (byte* k = keys) fixed (RxPad* p = pads) return setInput.Get()(s, k, p, padMask); }
        /// Raw mouse input (WM_INPUT) since the last call: pixels (+y down), wheel delta (120 per notch), RAWMOUSE usButtonFlags. Once per display frame, on the simulation thread.
        public static int rx_session_set_mouse(IntPtr s, int dx, int dy, int wheel, uint buttonFlags) => setMouse.Get()(s, dx, dy, wheel, buttonFlags);
        public static void rx_session_set_cursor(IntPtr s, int x, int y) => setCursor.Get()(s, x, y);
        /// The motor speeds the game last sent to the pad (0 = at least one XInputSetState call was made).
        /// The game's UI setting `name` (include/rx_ui_settings.h): mode 0 reads, 1 writes (UISettings_Set + Apply); 0 ok, 2 unknown name.
        public static int rx_session_ui_setting(IntPtr s, string name, int value, int mode, out int before) => uiSetting.Get()(s, name, value, mode, out before);
        public static int rx_session_rumble(IntPtr s, uint pad, out ushort left, out ushort right, out uint calls) { ushort l = 0, r = 0; uint c = 0; int rc = rumble.Get()(s, pad, &l, &r, &c); left = l; right = r; calls = c; return rc; }

        // ---- frame clock (the game's own step: see rx_api.h "the frame clock")
        /// The game's frame cap in seconds (1/60); 0 = uncapped.
        public static float rx_session_step_dt(IntPtr s) => stepDt.Get()(s);
        /// Adds real time; returns the whole game steps due (at most maxSteps). Run each with rx_session_advance_dt(step).
        public static uint rx_session_pace(IntPtr s, float realDt, uint maxSteps) => pace.Get()(s, realDt, maxSteps);
        /// How far the display time is into the next step, [0, 1).
        public static float rx_session_alpha(IntPtr s) => alpha.Get()(s);
        /// The game's own dt of the last frame (after its smoothing and clamp).
        public static float rx_session_engine_dt(IntPtr s) => engineDt.Get()(s);
        public static int rx_session_sizeof(string name) => sizeOf.Get()(name);

        // ---- ABI check (RxApiLayout holds the managed side)

        public sealed class CheckResult
        {
            public bool Loaded, Compatible = true;
            public int Version = -1;
            public string Summary = "";
            public readonly List<string> Mismatches = new List<string>();
        }

        /// Loads nothing: RxNativeLoader.Load must have run. Compares the library's API version and rx_session_sizeof of every struct
        /// with the managed sizes; a stale or mismatched dylib fails with one clear message instead of corrupt memory.
        public static CheckResult Check()
        {
            var r = new CheckResult();
            try { r.Version = (int)rx_session_api_version(); r.Loaded = true; }
            catch (DllNotFoundException e) { r.Compatible = false; r.Summary = "librxcore not loaded: " + e.Message; return r; }
            catch (EntryPointNotFoundException e) { r.Compatible = false; r.Summary = "not an OpenReflex core: " + e.Message; return r; }
            if (r.Version != ExpectedApiVersion) { r.Compatible = false; r.Mismatches.Add("API version " + r.Version + " (expected " + ExpectedApiVersion + ")"); }
            int checkedCount = 0;
            foreach (var (name, size) in RxApiLayout.ManagedSizes())
            {
                int native;
                try { native = rx_session_sizeof(name); }
                catch (EntryPointNotFoundException) { r.Compatible = false; r.Mismatches.Add("rx_session_sizeof missing"); break; }
                checkedCount++;
                if (native < 0) { r.Compatible = false; r.Mismatches.Add(name + ": unknown to the core"); }
                else if (native != size) { r.Compatible = false; r.Mismatches.Add(name + ": native " + native + ", managed " + size); }
            }
            r.Summary = r.Compatible ? "API " + r.Version + ", " + checkedCount + " struct sizes match" : "API " + r.Version + ": " + string.Join("; ", r.Mismatches);
            return r;
        }
    }

    // The managed side of every struct of rx_api.h, by C name (shared by Check, the EditMode test and the standalone checker).
    public static class RxApiLayout
    {
        public static readonly (string name, Type type)[] Structs =
        {
            ("rx_event_spec", typeof(RxEventSpec)), ("rx_pad", typeof(RxPad)), ("rx_bike_state", typeof(RxBikeState)),
            ("rx_camera", typeof(RxCamera)), ("rx_race_state", typeof(RxRaceState)), ("rx_driver_result", typeof(RxDriverResult)),
            ("rx_remap_table", typeof(RxRemapTable)), ("rx_slot", typeof(RxSlot)), ("rx_appearance", typeof(RxAppearance)), ("rx_profile_cell", typeof(RxProfileCell)), ("rx_vehicle_info", typeof(RxVehicleInfo)), ("rx_hud_state", typeof(RxHudState)),
            ("rx_terrain_info", typeof(RxTerrainInfo)), ("rx_particle", typeof(RxParticle)), ("rx_particle_info", typeof(RxParticleInfo)),
            ("rx_ambient_info", typeof(RxAmbientInfo)), ("rx_ambient_entity", typeof(RxAmbientEntity)), ("rx_scene_object", typeof(RxSceneObject)), ("rx_prop", typeof(RxProp)),
            ("rx_mud_state", typeof(RxMudState)), ("rx_track_info", typeof(RxTrackInfo)), ("rx_track_ribbon", typeof(RxTrackRibbon)),
            ("rx_podium_info", typeof(RxPodiumInfo)), ("rx_podium_rider", typeof(RxPodiumRider)), ("rx_podium_slot", typeof(RxPodiumSlot)),
            ("rx_start_gates", typeof(RxStartGates)), ("rx_scenario_model", typeof(RxScenarioModel)), ("rx_scenario_sound", typeof(RxScenarioSound)), ("rx_crowd_flash", typeof(RxCrowdFlash)),
            ("rx_lap_record", typeof(RxLapRecord)),
        };

        public static List<(string name, int size)> ManagedSizes()
        {
            var l = new List<(string, int)>();
            foreach (var (name, type) in Structs) l.Add((name, Marshal.SizeOf(type)));
            return l;
        }

        public static Type TypeOf(string cName)
        {
            foreach (var (name, type) in Structs) if (name == cName) return type;
            return null;
        }
    }
}
