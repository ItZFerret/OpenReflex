// WEATHER (the RACE / FREE RIDE setup page, per ride; LIGHTING: ENHANCED only; not part of the original game): rain, snow and changing
// weather over the original levels. CLEAR (the default) changes nothing: the shader globals stay off (_RfxWeather.w = 0), so every world
// shader takes its original path, no particle is drawn, no pass is added and the tyres keep the original's grip.
// TRACTION: a wet or snowed-over ground takes grip off every vehicle's tyres (GripFactor below; core/game/weather_grip.h).
//   RAIN     streaked, lit, wind-blown drops around the camera (near and far layers), splashes where they land, an overcast deck over the sky,
//            the sun dimmed behind it (softer shadows), mist; the ground darker and glossier as it gets wet, puddles collecting in the ruts'
//            bottoms, the tyre grooves and the hollows, mirroring the sky with the drops' ripples (Shaders/ReflexWeather.hlsl, ReflexTerrain.shader)
//   SNOW     falling flakes with depth (near large, far small), snow cover on the flatter ground, broken where the ground was ridden: the live
//            deformation (ruts) and tyre tracks of every rider - the player, the computer riders - show packed snow, and
//            churned mud where the ruts went deep
//   DYNAMIC  the weather changes by itself: clear spells, clouds building, showers of rain (or now and then snow) that come and go; the ground
//            wets and dries, puddles fill and drain, snow settles and melts at their own pace
//   INTENSITY LIGHT / MEDIUM / HEAVY: how hard it rains or snows (and how many particles: their count also follows ENHANCED QUALITY)
// Off in the supercross stadiums (indoors) and the menus. A race starts with the ground already wet (RAIN) or snowed over (SNOW).
// Cost: the terrain's weather terms are behind a uniform branch; the particles are 3 to 5 procedural draws (no CPU work per particle); the
// mist is one full-screen blend; a 64 x 64 height texture under the camera is refreshed when the camera has moved.
// Tests: REFLEX_WEATHER=clear|rain|snow|dynamic[:0..2] forces the weather (and intensity), REFLEX_WEATHER_SETTLE=1 skips every transition,
// REFLEX_WEATHER_DEBUG=1..4 shows the terrain's masks (puddles, snow / trail, deformation, tyre tracks).
using System;
using System.Collections.Generic;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.Universal;

namespace Reflex.Port.Rendering
{
    public sealed class ReflexWeather : MonoBehaviour
    {
        // ------------------------------------------------------------------------------------------------ options
        public static readonly string[] Names = { "CLEAR", "RAIN", "SNOW", "DYNAMIC" };
        public static readonly string[] IntensityNames = { "LIGHT", "MEDIUM", "HEAVY" };
        // WEATHER is chosen per ride (since 2026-11-07; before, OPTIONS > VIDEO held it for every ride): the RACE / FREE RIDE setup page's
        // WEATHER and WEATHER INTENSITY rows (OpenShell.Setup.cs, kept in each event's OpenSetup), handed over by ReflexEventSelect.Launch
        // (SetRide). Every other launch (career, MotoSkills, challenges, test ride, practice) rides CLEAR.
        /// The weather of the ride now loading or running: 0 CLEAR (default), 1 RAIN, 2 SNOW, 3 DYNAMIC; its intensity 0 LIGHT, 1 MEDIUM, 2 HEAVY.
        public static int RideMode { get; private set; }
        public static int RideIntensity { get; private set; } = 1;
        /// The launch's weather (ReflexEventSelect.Launch): the next frame starts it over (wet ground for RAIN, snow cover for SNOW).
        public static void SetRide(int weather, int intensity)
        {
            RideMode = Mathf.Clamp(weather, 0, 3); RideIntensity = Mathf.Clamp(intensity, 0, 2); restart = true;
            if (RideMode != 0) Debug.Log("[Reflex weather] this ride: " + Names[RideMode] + " " + IntensityNames[RideIntensity] + (LightingOn ? "" : " (LIGHTING is ORIGINAL: no weather drawn, no grip change)"));
        }

        /// The setup rows' default (an event's setup that never chose takes it; the last pick on any setup page). The old OPTIONS > VIDEO
        /// WEATHER / WEATHER INTENSITY ("Options.Weather" / "Options.WeatherIntensity") carry over into it once, then are removed.
        const string KDefault = "Setup.Weather", KDefaultIntensity = "Setup.WeatherIntensity";
        static void MigrateVideoSetting()
        {
            if (ReflexSave.Has("Options.Weather") || ReflexSave.Has("Options.WeatherIntensity"))
            {
                if (!ReflexSave.Has(KDefault)) ReflexSave.SetInt(KDefault, Mathf.Clamp(ReflexSave.GetInt("Options.Weather", 0), 0, 3));
                if (!ReflexSave.Has(KDefaultIntensity)) ReflexSave.SetInt(KDefaultIntensity, Mathf.Clamp(ReflexSave.GetInt("Options.WeatherIntensity", 1), 0, 2));
                ReflexSave.Delete("Options.Weather"); ReflexSave.Delete("Options.WeatherIntensity"); ReflexSave.Save();
                Debug.Log("[Reflex weather] the OPTIONS > VIDEO weather moved to the race / Free Ride setup: its default is now " + Names[DefaultSetting] + " " + IntensityNames[DefaultIntensity]);
            }
        }
        public static int DefaultSetting { get { MigrateVideoSetting(); return Mathf.Clamp(ReflexSave.GetInt(KDefault, 0), 0, 3); } }
        public static int DefaultIntensity { get { MigrateVideoSetting(); return Mathf.Clamp(ReflexSave.GetInt(KDefaultIntensity, 1), 0, 2); } }
        /// A setup row's pick: becomes the default for setups that never chose; anything but CLEAR needs LIGHTING: ENHANCED, so it switches
        /// LIGHTING to ENHANCED too. Saved at once.
        public static void PickDefault(int weather)
        {
            MigrateVideoSetting();
            ReflexSave.SetInt(KDefault, Mathf.Clamp(weather, 0, 3));
            if (weather != 0 && ReflexSettings.Lighting != 1) ReflexSettings.Lighting = 1;
            ReflexSave.Save();
        }
        public static void PickDefaultIntensity(int i) { MigrateVideoSetting(); ReflexSave.SetInt(KDefaultIntensity, Mathf.Clamp(i, 0, 2)); ReflexSave.Save(); }

        // ------------------------------------------------------------------------------------------------ traction (rain / snow grip)
        // Rain and snow take grip off every vehicle's tyres (MX, ATV, UTV, trucks, buggies; the player's and the computer riders'): the core
        // multiplies the static rolling and cornering grip of every tyre by GripFactor (core/game/weather_grip.h, rx_session_set_weather_grip).
        // The loss follows the ground the weather leaves, not the falling rain: wetness for rain, snow cover for snow (DYNAMIC: as they build
        // up and dry / melt), times the intensity's strength (LIGHT 0.38, MEDIUM 0.68, HEAVY 1). CLEAR, LIGHTING: ORIGINAL, the stadiums and
        // everything without drawn weather: 1, the core untouched (bit for bit the original's physics).
        // TUNING: the two losses below (a share of the grip at full wetness / cover, HEAVY). Built in only: no launch option changes them
        // (REFLEX_WEATHER_GRIP was removed: a player could have set its own grip).
        public const float RainGripLoss = 0.15f, SnowGripLoss = 0.30f;
        /// This frame's tyre grip factor (1: no change); what OpenReflexSimulation hands the core.
        public static float GripFactor { get; private set; } = 1f;
        static float GripOf(float wetness, float cover, float k) => 1f - Mathf.Clamp01(Mathf.Max(RainGripLoss * wetness, SnowGripLoss * cover) * k);

        /// Tests: the weather / intensity instead of the ride's (-1: the ride's); Settle: every value at its target at once.
        public static int Override = -1, IntensityOverride = -1;
        public static bool Settle;
        /// Tests: forced levels (-1: the simulation's): wetness, puddle level, snow cover.
        public static float ForceWet = -1, ForcePuddles = -1, ForceCover = -1;
        /// Tests: the terrain's masks instead of the picture (1 puddles, 2 snow / trail, 3 deformation, 4 tyre tracks).
        public static int DebugView;

        public static int Mode => Override >= 0 ? Override : RideMode;
        public static int Intensity => IntensityOverride >= 0 ? IntensityOverride : RideIntensity;
        static bool LightingOn { get { try { return ReflexEnhancedLighting.Requested; } catch (Exception) { return false; } } }
        /// The weather would be drawn (an option other than CLEAR and LIGHTING: ENHANCED).
        public static bool Requested => Mode != 0 && LightingOn;

        // ------------------------------------------------------------------------------------------------ the live state (read by others)
        /// Drawing weather this frame.
        public static bool Active { get; private set; }
        /// Falling rain / snow 0..1, the cloud cover 0..1, the ground's wetness, puddle level and snow cover 0..1.
        public static float Rain, Snow, Overcast, Wetness, Puddles, SnowCover;
        /// The sun's share left by the clouds (VOLUMETRIC LIGHT's sun; 1 without weather).
        public static float SunScale => Active ? 1f - 0.72f * Overcast : 1f;
        /// The haze's extra density (VOLUMETRIC LIGHT's medium; 1 without weather).
        public static float MediumScale => Active ? 1f + 1.6f * Mathf.Max(Rain, Snow * 1.4f) + 0.4f * Overcast : 1f;
        /// A one-line description (perf overlay, tests).
        public static string Report { get; private set; } = "CLEAR";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void ReadEnvironment()
        {
            // the weather / intensity / settle launch options force the ground's wetness and snow, so the tyres' grip with it: the editor and
            // development players only (a release player rides the setup's CONDITIONS; the test fixtures set the fields themselves)
            bool dev = Application.isEditor || Debug.isDebugBuild;
            string v = dev ? Environment.GetEnvironmentVariable("REFLEX_WEATHER") : null;
            if (!string.IsNullOrEmpty(v))
            {
                var p = v.Split(':');
                int m = Array.FindIndex(Names, n => string.Equals(n, p[0], StringComparison.OrdinalIgnoreCase));
                if (m >= 0) Override = m;
                if (p.Length > 1 && int.TryParse(p[1], out int i)) IntensityOverride = Mathf.Clamp(i, 0, 2);
            }
            Settle = dev && Environment.GetEnvironmentVariable("REFLEX_WEATHER_SETTLE") == "1";
            if (int.TryParse(Environment.GetEnvironmentVariable("REFLEX_WEATHER_DEBUG"), out int dbg)) DebugView = dbg;
            Shader.SetGlobalVector(WeatherId, Vector4.zero);
        }

        static ReflexWeather instance;
        static bool restart = true;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if (instance) return;
            var go = new GameObject("Reflex weather") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            instance = go.AddComponent<ReflexWeather>();
        }

        /// Starts the weather over (the next frame sets up the level's start state: wet ground for RAIN, snow cover for SNOW).
        public static void Restart() { restart = true; }

        // ------------------------------------------------------------------------------------------------ shader inputs
        static readonly int WeatherId = Shader.PropertyToID("_RfxWeather"), Weather2Id = Shader.PropertyToID("_RfxWeather2"),
            SkyId = Shader.PropertyToID("_RfxWeatherSky"), FogId = Shader.PropertyToID("_RfxWeatherFog"), WindId = Shader.PropertyToID("_RfxWeatherWind"),
            DebugId = Shader.PropertyToID("_RfxWeatherDebug"), RfxSunColorId = Shader.PropertyToID("_RfxSunColor"), SunColorId = Shader.PropertyToID("_SunColor");
        static readonly int PBox = Shader.PropertyToID("_PBox"), PFall = Shader.PropertyToID("_PFall"), PCamVel = Shader.PropertyToID("_PCamVel"),
            PLight = Shader.PropertyToID("_PLight"), PSun = Shader.PropertyToID("_PSun"), PGround = Shader.PropertyToID("_PGround"),
            PGroundRect = Shader.PropertyToID("_PGroundRect"), PMisc = Shader.PropertyToID("_PMisc");

        // ------------------------------------------------------------------------------------------------ state
        ReflexEnhancedLighting owner; Camera cam; OriginalLevel level; float findAt;
        float clock;                                     // the particles' and ripples' time (s, wrapped)
        Vector3 lastCamPos; bool hasCam; Vector3 camVel;
        Vector2 wind, windTarget; float windChange, cloudScroll;
        // DYNAMIC: the phase (0 clear, 1 building, 2 falling, 3 clearing), its time left, the shower's kind (0 rain, 1 snow) and strength
        int phase; float phaseLeft, peak; int kind;
        System.Random rng = new System.Random();
        // the sun as the level set it (restored when the weather is off) and what was last written
        Vector4 baseSun, wroteSun; bool sunHeld;
        readonly Dictionary<Material, (Vector4 base_, Vector4 wrote)> terrainSun = new Dictionary<Material, (Vector4, Vector4)>();
        Light sunLight; float baseShadow, wroteShadow; bool shadowHeld;
        Material precipMaterial; readonly MaterialPropertyBlock[] blocks = new MaterialPropertyBlock[5];
        Texture2D ground; float[] groundData; Vector2 groundOrigin; float groundAt; Vector3 groundCam = new Vector3(1e9f, 0, 0);
        const int GroundRes = 64; const float GroundSpan = 48f; bool groundLogged;
        ReflexWeatherMistPass mist; ReflexWeatherPrecipPass precip;
        /// This frame's particle layers (count, its properties), drawn by ReflexWeatherPrecipPass in every render of the game camera.
        internal readonly List<(int count, MaterialPropertyBlock mpb)> layers = new List<(int, MaterialPropertyBlock)>();
        internal Material PrecipMaterial => precipMaterial;

        void OnEnable() { RenderPipelineManager.beginCameraRendering += OnBegin; }
        void OnDisable() { RenderPipelineManager.beginCameraRendering -= OnBegin; Deactivate(); }
        void OnDestroy()
        {
            Deactivate();
            if (precipMaterial) Destroy(precipMaterial);
            if (ground) Destroy(ground);
            mist?.Dispose(); mist = null;
        }

        static bool Indoor(string lvl) => string.IsNullOrEmpty(lvl) || lvl.StartsWith("SX_", StringComparison.OrdinalIgnoreCase) || lvl.StartsWith("UI_", StringComparison.OrdinalIgnoreCase);

        void FindOwner()
        {
            if (owner && owner.active && owner.level && owner.level.Loaded && owner.isActiveAndEnabled) return;
            owner = null; cam = null;
            if (Time.unscaledTime < findAt) return;
            findAt = Time.unscaledTime + 0.5f;
            foreach (var e in FindObjectsByType<ReflexEnhancedLighting>(FindObjectsSortMode.None))
                if (e.active && e.level && e.level.Loaded && e.isActiveAndEnabled) { owner = e; break; }
            if (owner) cam = owner.GetComponent<Camera>();
        }

        void LateUpdate()
        {
            float dt = Mathf.Min(Time.unscaledDeltaTime, 0.1f);
            if (!Requested) { Deactivate(); return; }
            FindOwner();
            if (!owner || !cam || Indoor(owner.level.level)) { Deactivate(); return; }
            if (level != owner.level) { level = owner.level; restart = true; terrainSun.Clear(); }
            if (restart) { StartState(); restart = false; }
            Simulate(dt);
            Active = true;
            GripFactor = GripOf(Wetness, SnowCover, K(Intensity));
            Reflex.Port.RxCore.OpenReflexSimulation.GameWeatherGrip = GripFactor;
            ApplyGlobals(dt);
            ApplySun();
            UpdateGround();
            Draw();
        }

        // ------------------------------------------------------------------------------------------------ the weather itself
        static float K(int intensity) => intensity == 0 ? 0.38f : intensity == 1 ? 0.68f : 1f;

        void StartState()
        {
            int mode = Mode; float k = K(Intensity);
            Rain = Snow = Overcast = Wetness = Puddles = SnowCover = 0f;
            wind = windTarget = RandomWind(k);
            if (mode == 1) { Rain = k; Overcast = 0.6f + 0.38f * k; Wetness = 1f; Puddles = 0.5f + 0.5f * k; }
            else if (mode == 2) { Snow = k; Overcast = 0.55f + 0.4f * k; SnowCover = 1f; }
            else if (mode == 3)
            {
                double r = rng.NextDouble();
                kind = rng.NextDouble() < 0.3 ? 1 : 0;
                peak = 0.35f + 0.65f * (float)rng.NextDouble();
                if (r < 0.45) { phase = 0; phaseLeft = Range(40f, 120f); Overcast = 0.15f; }
                else if (r < 0.7) { phase = 0; phaseLeft = Range(60f, 150f); Overcast = 0.3f; Wetness = 0.7f; Puddles = 0.45f; kind = 0; }
                else
                {
                    phase = 2; phaseLeft = Range(90f, 200f); Overcast = 0.65f + 0.35f * peak;
                    if (kind == 0) { Rain = peak; Wetness = 1f; Puddles = 0.6f * peak + 0.2f; } else { Snow = peak; SnowCover = 0.9f; }
                }
            }
            Debug.Log("[Reflex weather] start " + Names[mode] + " " + IntensityNames[Intensity] + " on " + (level ? level.level : "?"));
        }

        float Range(float a, float b) => a + (b - a) * (float)rng.NextDouble();
        Vector2 RandomWind(float k)
        {
            float a = (float)rng.NextDouble() * Mathf.PI * 2f, s = Range(1.0f, 2.5f) + 3.5f * k;
            return new Vector2(Mathf.Cos(a), Mathf.Sin(a)) * s;
        }

        void Simulate(float dt)
        {
            int mode = Mode; float k = K(Intensity);
            float rainT = 0, snowT = 0, overT = 0;
            if (mode == 1) { rainT = k; overT = 0.6f + 0.38f * k; }
            else if (mode == 2) { snowT = k; overT = 0.55f + 0.4f * k; }
            else if (mode == 3)
            {
                phaseLeft -= dt;
                if (phaseLeft <= 0f)
                {
                    phase = (phase + 1) % 4;
                    phaseLeft = phase == 0 ? Range(60f, 180f) : phase == 1 ? Range(25f, 50f) : phase == 2 ? Range(90f, 240f) : Range(30f, 60f);
                    if (phase == 1) { kind = rng.NextDouble() < 0.3 ? 1 : 0; peak = Mathf.Clamp01((0.3f + 0.7f * (float)rng.NextDouble()) * (0.6f + 0.4f * k / 0.68f)); }
                }
                float gust = 0.82f + 0.18f * Mathf.PerlinNoise(clock * 0.05f, 3.1f);
                if (phase == 0) overT = 0.12f;
                else if (phase == 1) overT = 0.65f + 0.35f * peak;
                else if (phase == 2) { overT = 0.65f + 0.35f * peak; if (kind == 0) rainT = peak * gust; else snowT = peak * gust; }
                else overT = 0.25f;
            }
            if (Settle) { Rain = rainT; Snow = snowT; Overcast = overT; }
            else
            {
                Rain = Mathf.MoveTowards(Rain, rainT, dt / 10f);
                Snow = Mathf.MoveTowards(Snow, snowT, dt / 10f);
                Overcast = Mathf.MoveTowards(Overcast, overT, dt / 25f);
                // the ground: wets in about half a minute of rain, dries in a few minutes; puddles fill over a minute and a half and drain
                // slowly; snow settles over two minutes of snowfall and melts in ten (faster in rain)
                float puddleCap = Mathf.Clamp01(0.45f + 0.55f * Mathf.Max(Rain, mode == 1 ? k : 0f));
                if (Rain > 0.02f) Wetness = Mathf.Min(1f, Wetness + dt * Rain / 25f);
                else Wetness = Mathf.Max(0f, Wetness - dt * (1f - 0.6f * Overcast) / 180f);
                if (Rain > 0.05f && Puddles < puddleCap) Puddles = Mathf.Min(puddleCap, Puddles + dt * Rain / 90f);
                else if (Rain <= 0.05f) Puddles = Mathf.Max(0f, Puddles - dt / 420f);
                if (Snow > 0.02f) SnowCover = Mathf.Min(1f, SnowCover + dt * Snow / 120f);
                else SnowCover = Mathf.Max(0f, SnowCover - dt * (Rain > 0.05f ? 1f / 150f : 1f / 600f));
                if (SnowCover > 0.05f && Snow < 0.02f && Rain < 0.02f) Wetness = Mathf.Max(Wetness, SnowCover * 0.4f);   // (melting snow wets the ground)
            }
            if (ForceWet >= 0) Wetness = ForceWet;
            if (ForcePuddles >= 0) Puddles = ForcePuddles;
            if (ForceCover >= 0) SnowCover = ForceCover;
            // the wind: a new direction / strength now and then, gusting around it
            windChange -= dt;
            if (windChange <= 0f) { windChange = Range(15f, 45f); windTarget = RandomWind(Mathf.Max(Rain, Snow, 0.2f)); }
            wind = Vector2.Lerp(wind, windTarget, 1f - Mathf.Exp(-dt * 0.15f));
            clock += dt; if (clock > 3600f) clock -= 3600f;
            cloudScroll += dt * (6f + wind.magnitude * 4f);
            Report = $"{Names[mode]} {IntensityNames[Intensity]}: rain {Rain:0.00} snow {Snow:0.00} clouds {Overcast:0.00} wet {Wetness:0.00} puddles {Puddles:0.00} cover {SnowCover:0.00} grip {GripOf(Wetness, SnowCover, k):0.000}" + (mode == 3 ? $" (phase {phase}, {phaseLeft:0} s)" : "");
        }

        // ------------------------------------------------------------------------------------------------ globals, sun
        void ApplyGlobals(float dt)
        {
            var l = level.Content != null ? level.Content.lighting : null;
            Color fogA = l != null ? l.fogColorStart : new Color(0.6f, 0.62f, 0.65f), fogB = l != null ? l.fogColorEnd : fogA;
            Vector3 fog = new Vector3((fogA.r + fogB.r) * 0.5f, (fogA.g + fogB.g) * 0.5f, (fogA.b + fogB.b) * 0.5f);
            float lum = 0.2126f * fog.x + 0.7152f * fog.y + 0.0722f * fog.z;
            // the cloud base: the fog's colour, desaturated towards a cool grey, a little darker (rain clouds darker than snow clouds)
            float storm = Mathf.Max(Rain, 0.4f * Snow);
            Vector3 grey = new Vector3(lum * 0.97f, lum * 1.0f, lum * 1.05f);
            Vector3 horizon = Vector3.Lerp(fog, grey, 0.7f) * Mathf.Lerp(0.95f, 0.78f, storm);
            Vector3 zenith = horizon * Mathf.Lerp(0.86f, 0.62f, storm);
            float mistDensity = Rain * 0.0022f + Snow * 0.0034f + Overcast * 0.0004f;
            Shader.SetGlobalVector(WeatherId, new Vector4(Wetness, SnowCover, Overcast, 1f));
            Shader.SetGlobalVector(Weather2Id, new Vector4(Puddles, Rain, Snow, clock));
            Shader.SetGlobalVector(SkyId, new Vector4(zenith.x, zenith.y, zenith.z, Mathf.Lerp(0.25f, 0.45f, storm)));
            Shader.SetGlobalVector(FogId, new Vector4(horizon.x, horizon.y, horizon.z, mistDensity));
            Shader.SetGlobalVector(WindId, new Vector4(wind.x, 0, wind.y, cloudScroll));
            Shader.SetGlobalVector(DebugId, new Vector4(DebugView, 0, 0, 0));
            if (hasCam && dt > 1e-4f)
            {
                var v = (cam.transform.position - lastCamPos) / dt;
                if (v.sqrMagnitude > 90f * 90f) v = camVel;   // a cut / reset: keep the last velocity
                camVel = Vector3.Lerp(camVel, v, 1f - Mathf.Exp(-dt * 12f));
            }
            lastCamPos = cam.transform.position; hasCam = true;
        }

        void ApplySun()
        {
            float s = 1f - 0.72f * Overcast;
            // the scenery / vehicles' sun (a global): what the level set, unless it was changed since the last write
            Vector4 g = Shader.GetGlobalVector(RfxSunColorId);
            if (!sunHeld || g != wroteSun) { baseSun = g; sunHeld = true; }
            wroteSun = baseSun * s; Shader.SetGlobalVector(RfxSunColorId, wroteSun);
            // the terrain's sun (per material)
            foreach (var t in Terrain.activeTerrains)
            {
                var m = t ? t.materialTemplate : null;
                if (!m || !m.HasProperty(SunColorId)) continue;
                var cur = m.GetVector(SunColorId);
                if (!terrainSun.TryGetValue(m, out var e) || cur != e.wrote) e = (cur, cur);
                e.wrote = e.base_ * s; m.SetVector(SunColorId, e.wrote);
                terrainSun[m] = e;
            }
            // softer shadows under the clouds
            var sun = RenderSettings.sun;
            if (sun != sunLight) { RestoreShadow(); sunLight = sun; }
            if (sunLight)
            {
                if (!shadowHeld || !Mathf.Approximately(sunLight.shadowStrength, wroteShadow)) { baseShadow = sunLight.shadowStrength; shadowHeld = true; }
                wroteShadow = baseShadow * Mathf.Lerp(1f, 0.45f, Overcast); sunLight.shadowStrength = wroteShadow;
            }
        }

        void RestoreShadow()
        {
            if (sunLight && shadowHeld && Mathf.Approximately(sunLight.shadowStrength, wroteShadow)) sunLight.shadowStrength = baseShadow;
            shadowHeld = false;
        }

        void Deactivate()
        {
            GripFactor = 1f;   // (no weather drawn: the tyres' own grip)
            Reflex.Port.RxCore.OpenReflexSimulation.GameWeatherGrip = 1f;
            if (!Active && !sunHeld && terrainSun.Count == 0 && !shadowHeld) return;
            Active = false;
            Shader.SetGlobalVector(WeatherId, Vector4.zero);
            if (sunHeld && Shader.GetGlobalVector(RfxSunColorId) == wroteSun) Shader.SetGlobalVector(RfxSunColorId, baseSun);
            sunHeld = false;
            foreach (var kv in terrainSun) if (kv.Key && kv.Key.GetVector(SunColorId) == kv.Value.wrote) kv.Key.SetVector(SunColorId, kv.Value.base_);
            terrainSun.Clear();
            RestoreShadow();
            Report = Requested ? Names[Mode] + " (not here)" : "CLEAR";
            restart = true;
        }

        // ------------------------------------------------------------------------------------------------ the ground under the camera (splashes)
        void UpdateGround()
        {
            if (Rain < 0.02f) return;
            var p = cam.transform.position;
            if (ground && (new Vector2(p.x - groundCam.x, p.z - groundCam.z)).sqrMagnitude < 6f * 6f && Time.unscaledTime - groundAt < 2f) return;
            if (!ground)
            {
                ground = new Texture2D(GroundRes, GroundRes, TextureFormat.RFloat, false, true) { name = "Reflex weather ground", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.HideAndDontSave };
                groundData = new float[GroundRes * GroundRes];
            }
            groundCam = p; groundAt = Time.unscaledTime;
            groundOrigin = new Vector2(p.x - GroundSpan * 0.5f, p.z - GroundSpan * 0.5f);
            var terrains = Terrain.activeTerrains;
            float step = GroundSpan / (GroundRes - 1);
            for (int r = 0; r < GroundRes; r++)
                for (int c = 0; c < GroundRes; c++)
                {
                    float x = groundOrigin.x + c * step, z = groundOrigin.y + r * step, h = -1e6f;
                    if (level && level.terrain)
                    {
                        float ch = level.terrain.CoreHeightAt(x, z, out bool have);   // (the core's own grid: what the bikes ride on)
                        if (have) { groundData[r * GroundRes + c] = ch; continue; }
                    }
                    foreach (var t in terrains)
                    {
                        if (!t || !t.terrainData) continue;
                        var tp = t.GetPosition(); var sz = t.terrainData.size;
                        if (x < tp.x || z < tp.z || x > tp.x + sz.x || z > tp.z + sz.z) continue;
                        h = t.SampleHeight(new Vector3(x, 0, z)) + tp.y; break;
                    }
                    groundData[r * GroundRes + c] = h;
                }
            ground.SetPixelData(groundData, 0); ground.Apply(false, false);
            if (!groundLogged)
            {
                groundLogged = true; int found = 0; float lo = 1e9f, hi = -1e9f;
                foreach (var h in groundData) if (h > -1e5f) { found++; lo = Mathf.Min(lo, h); hi = Mathf.Max(hi, h); }
                Debug.Log($"[Reflex weather] ground under the camera {p}: {found} / {groundData.Length} terrain samples, heights {lo:0.0} .. {hi:0.0} ({terrains.Length} terrains)");
            }
        }

        // ------------------------------------------------------------------------------------------------ the particles
        void Draw()
        {
            if (!precipMaterial)
            {
                var sh = Resources.Load<Shader>("ReflexPrecipitation");
                if (!sh || !sh.isSupported) { Debug.LogError("[Reflex weather] Resources/ReflexPrecipitation not found or not supported"); enabled = false; return; }
                precipMaterial = new Material(sh) { hideFlags = HideFlags.HideAndDontSave };
            }
            int tier = ReflexEnhancedLighting.Tier;
            float q = tier == 0 ? 0.45f : tier == 1 ? 0.7f : 1f;
            var l = level.Content != null ? level.Content.lighting : null;
            Vector4 fog = Shader.GetGlobalVector(FogId);
            // the light on a particle: the sky under the clouds (the mist colour) and the dimmed sun
            Vector3 sky = new Vector3(fog.x, fog.y, fog.z);
            Vector4 sunC = Shader.GetGlobalVector(RfxSunColorId);
            float pix = 2f * Mathf.Tan(cam.fieldOfView * 0.5f * Mathf.Deg2Rad) / Mathf.Max(1, cam.pixelHeight);
            var camV = new Vector4(camVel.x, camVel.y, camVel.z, pix);
            int b = 0;
            layers.Clear();
            if (Rain > 0.01f)
            {
                // near: dense thin streaks; far: fewer, larger, fainter (the rain's depth)
                Layer(ref b, 0, (int)(16000 * q * Rain), new Vector4(34, 22, 34, 4), new Vector4(Mathf.Lerp(6.5f, 9.5f, Rain), 0.0075f, 0.04f, 0.6f), camV, sky, 1.45f, sunC, 0, 0);
                Layer(ref b, 0, (int)(7000 * q * Rain), new Vector4(110, 46, 110, 10), new Vector4(Mathf.Lerp(6.5f, 9.5f, Rain), 0.02f, 0.05f, 0.22f), camV, sky, 1.4f, sunC, 0, 200000);
                if (Wetness > 0.2f) Layer(ref b, 2, (int)(1400 * q * Rain), Vector4.zero, new Vector4(0, 0.16f, 0, 0.8f), camV, sky, 2.2f, sunC, 0, 400000);
            }
            if (Snow > 0.01f)
            {
                Layer(ref b, 1, (int)(9000 * q * Snow), new Vector4(26, 16, 26, 3), new Vector4(1.1f, 0.022f, 0.01f, 0.9f), camV, sky, 1f, sunC, 0.35f, 600000);
                Layer(ref b, 1, (int)(14000 * q * Snow), new Vector4(90, 40, 90, 8), new Vector4(1.3f, 0.035f, 0.01f, 0.6f), camV, sky, 1f, sunC, 0.6f, 800000);
            }
        }

        void Layer(ref int b, int mode, int count, Vector4 box, Vector4 fall, Vector4 camV, Vector3 sky, float bright, Vector4 sun, float sway, int first)
        {
            if (count < 1 || b >= blocks.Length) return;
            var mpb = blocks[b] ??= new MaterialPropertyBlock();
            b++;
            mpb.Clear();
            mpb.SetVector(PBox, box); mpb.SetVector(PFall, fall); mpb.SetVector(PCamVel, camV);
            mpb.SetVector(PLight, new Vector4(sky.x, sky.y, sky.z, bright));
            mpb.SetVector(PSun, new Vector4(sun.x, sun.y, sun.z, sway));
            mpb.SetVector(PMisc, new Vector4(mode, 0.32f, 17f, first));
            if (ground) mpb.SetTexture(PGround, ground); else mpb.SetTexture(PGround, Texture2D.blackTexture);
            mpb.SetVector(PGroundRect, ground ? new Vector4(groundOrigin.x, groundOrigin.y, 1f / GroundSpan, 1f) : Vector4.zero);
            layers.Add((count, mpb));
        }

        // ------------------------------------------------------------------------------------------------ the mist
        void OnBegin(ScriptableRenderContext ctx, Camera c)
        {
            if (!Active || c != cam || !c) return;
            var data = c.GetUniversalAdditionalCameraData();
            if (!data || data.scriptableRenderer == null) return;
            mist ??= new ReflexWeatherMistPass();
            mist.ConfigureInput(ScriptableRenderPassInput.Depth);
            data.scriptableRenderer.EnqueuePass(mist);
            if (layers.Count > 0 && precipMaterial)
            {
                precip ??= new ReflexWeatherPrecipPass(this);
                precip.ConfigureInput(ScriptableRenderPassInput.Depth);
                data.scriptableRenderer.EnqueuePass(precip);
            }
        }
    }

    /// WEATHER's mist: the distance haze of rain and snow over the opaque world (after the volumetric composite, before the transparent
    /// objects: the precipitation fades by distance itself), blended in the mist colour (_RfxWeatherFog) by the distance; the sky's horizon too.
    sealed class ReflexWeatherMistPass : ScriptableRenderPass, IDisposable
    {
        Material material;
        static readonly int DepthId = Shader.PropertyToID("_RfxWDepth"), ZId = Shader.PropertyToID("_RfxWZParams"), TlId = Shader.PropertyToID("_RfxWTopLeft"),
            XeId = Shader.PropertyToID("_RfxWXExtent"), YeId = Shader.PropertyToID("_RfxWYExtent"), CamId = Shader.PropertyToID("_RfxWCam");
        readonly MaterialPropertyBlock mpb = new MaterialPropertyBlock();

        public ReflexWeatherMistPass()
        {
            renderPassEvent = (RenderPassEvent)((int)RenderPassEvent.BeforeRenderingTransparents + 1);   // after VOLUMETRIC LIGHT's composite
            profilingSampler = new ProfilingSampler("Reflex weather mist");
        }

        public void Dispose() { if (material) CoreUtils.Destroy(material); material = null; }

        class PassData { internal Material material; internal MaterialPropertyBlock mpb; internal TextureHandle depth; internal Vector4 z, tl, xe, ye, cam; }

        public override void RecordRenderGraph(RenderGraph rg, ContextContainer frameData)
        {
            if (!material)
            {
                var sh = Resources.Load<Shader>("ReflexWeatherMist");
                if (!sh || !sh.isSupported) return;
                material = CoreUtils.CreateEngineMaterial(sh);
            }
            var res = frameData.Get<UniversalResourceData>();
            var camData = frameData.Get<UniversalCameraData>();
            var depth = res.cameraDepthTexture;
            if (!depth.IsValid() || res.isActiveTargetBackBuffer) return;
            // the camera as the volumetric pass sees it: rays through the near-plane corners in world orientation (GL projection, uv.y 0 = bottom)
            var cam = camData.camera;
            Matrix4x4 view = camData.GetViewMatrix(), proj = camData.GetProjectionMatrix();
            var camView = view; camView.SetColumn(3, new Vector4(0, 0, 0, 1));
            var inv = (proj * camView).inverse;
            Vector3 tl = inv.MultiplyPoint(new Vector3(-1, 1, -1)), tr = inv.MultiplyPoint(new Vector3(1, 1, -1)), bl = inv.MultiplyPoint(new Vector3(-1, -1, -1));
            float n = cam.nearClipPlane, f = cam.farClipPlane;
            Vector4 zp = SystemInfo.usesReversedZBuffer
                ? new Vector4(-1f + f / n, 1f, (-1f + f / n) / f, 1f / f)
                : new Vector4(1f - f / n, f / n, (1f - f / n) / f, f / n / f);
            var p = cam.transform.position;
            using (var builder = rg.AddRasterRenderPass<PassData>("Reflex weather mist", out var d, profilingSampler))
            {
                d.material = material; d.mpb = mpb; d.depth = depth; d.z = zp; d.tl = tl; d.xe = tr - tl; d.ye = bl - tl; d.cam = new Vector4(p.x, p.y, p.z, n);
                builder.UseTexture(depth);
                builder.SetRenderAttachment(res.activeColorTexture, 0, AccessFlags.ReadWrite);
                builder.AllowPassCulling(false);
                builder.SetRenderFunc(static (PassData data, RasterGraphContext ctx) =>
                {
                    var m = data.mpb; m.Clear();
                    m.SetTexture(DepthId, data.depth); m.SetVector(ZId, data.z); m.SetVector(TlId, data.tl); m.SetVector(XeId, data.xe);
                    m.SetVector(YeId, data.ye); m.SetVector(CamId, data.cam);
                    ctx.cmd.DrawProcedural(Matrix4x4.identity, data.material, 0, MeshTopology.Triangles, 3, 1, m);
                });
            }
        }
    }

    /// WEATHER's rain, snow and splashes (Resources/ReflexPrecipitation.shader): the layers ReflexWeather prepared this frame, drawn over the
    /// camera's colour with its depth (test only, no writes) after the transparent objects, in every render of the game camera (a replay's,
    /// a photo's, a test's offscreen shot too).
    sealed class ReflexWeatherPrecipPass : ScriptableRenderPass
    {
        readonly ReflexWeather owner;
        public ReflexWeatherPrecipPass(ReflexWeather owner)
        {
            this.owner = owner;
            renderPassEvent = RenderPassEvent.AfterRenderingTransparents;
            profilingSampler = new ProfilingSampler("Reflex weather precipitation");
        }

        class PassData { internal Material material; internal (int count, MaterialPropertyBlock mpb)[] layers; }

        public override void RecordRenderGraph(RenderGraph rg, ContextContainer frameData)
        {
            var mat = owner.PrecipMaterial;
            if (!mat || owner.layers.Count == 0) return;
            var res = frameData.Get<UniversalResourceData>();
            if (res.isActiveTargetBackBuffer || !res.activeDepthTexture.IsValid()) return;
            using (var builder = rg.AddRasterRenderPass<PassData>("Reflex weather precipitation", out var d, profilingSampler))
            {
                d.material = mat; d.layers = owner.layers.ToArray();
                builder.SetRenderAttachment(res.activeColorTexture, 0, AccessFlags.ReadWrite);
                builder.SetRenderAttachmentDepth(res.activeDepthTexture, AccessFlags.Read);
                builder.UseAllGlobalTextures(true);   // (the volumetric haze's result, the scene depth: ReflexVolumetricApply.hlsl)
                builder.AllowPassCulling(false);
                builder.AllowGlobalStateModification(true);
                builder.SetRenderFunc(static (PassData data, RasterGraphContext ctx) =>
                {
                    foreach (var l in data.layers) ctx.cmd.DrawProcedural(Matrix4x4.identity, data.material, 0, MeshTopology.Triangles, l.count * 6, 1, l.mpb);
                });
            }
        }
    }
}
