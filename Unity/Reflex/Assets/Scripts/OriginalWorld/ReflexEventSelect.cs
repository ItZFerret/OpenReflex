using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.SceneManagement;
using UnityEngine.UI;
using Reflex.Port.Content;

namespace Reflex.Port.OriginalWorld
{
    // The event of the generic Reflex scene: any single-player event of the install's tables (ReflexEvents: Nationals,
    // Supercross, Waypoint races, Free Ride, Terracross). Before the other components wake it points the level content
    // (OriginalLevel), the running world (OriginalWorldRuntime: built from the install on first use), game sound and
    // music at the chosen event. Insert opens the event list (a port tool: F2 is the game's MapKey_UI_Misc1, KeyBind_Core.xml); choosing one reloads the scene with it.
    [DefaultExecutionOrder(-300)]
    [DisallowMultipleComponent]
    public sealed class ReflexEventSelect : MonoBehaviour
    {
        // The race's AI as the original front end offers it (LocationSelect): off, or a full gate at one difficulty.
        public const string PrefsKey = "ReflexEvent", AiKey = "ReflexAI";
        int aiChoice;   // 0 off, 1.. ReflexWorldFiles.AiSkills[aiChoice - 1]
        int AiSkill => aiChoice > 0 ? ReflexWorldFiles.AiSkills[aiChoice - 1].skill : 0;
        int Opponents => aiChoice > 0 ? ReflexWorldFiles.OpponentsFor(fieldRiders) : 0;
        int fieldRiders;   // RIDER CAP: the setup's RIDERS (0: the original's full gate of 12)
        string AiLabel => aiChoice > 0 ? ReflexWorldFiles.AiSkills[aiChoice - 1].name : "OFF";
        // The vehicle (Delete in the list; Tab is the game's MapKey_UI_Select1): the event's own (the profile's MX Lite) or an ATV, as Vehicle Select + LocationSelect would choose it
        // (ReflexEvents.Event.AtvGarage: the first pro with an ATV, the ATV class); kept across the list's reload, not saved.
        bool atv;
        // Delete cycles MX LITE -> ATV -> UTV -> SUPER BUGGY -> PRO LITE -> PRO 2 (ReflexVehicles; VehicleSelect's order); 0 = the event's own.
        static readonly (int type, string label)[] VehicleChoices = { (-1, "MX LITE"), (ReflexAtv.VehicleTypeAtv, "ATV"), (ReflexVehicles.TypeUtv, "UTV"), (ReflexVehicles.TypeSuperBuggy, "SUPER BUGGY"), (ReflexVehicles.TypeProLite, "PRO LITE"), (ReflexVehicles.TypePro2, "PRO 2") };
        int vehicleChoice;
        // Home cycles the vehicle class among LocationSelect's classes for the chosen type (ReflexEvents.Event.ClassesFor: the Single class
        // first, then the Rival / Multi ones the type belongs to, e.g. an ATV at a National: ATV, ATV VS MX LITE, MX VS ATV, MOTO); 0 = the first.
        int classChoice;
        int ChoiceType(ReflexEvents.Event e)
        {
            int t = VehicleChoices[vehicleChoice].type;
            if (t >= 0) return t;
            if (!e.MachineOnly) return ReflexEvents.DefaultVehicleType;   // the profile's own (MX Lite)
            return e.GarageFor(ReflexVehicles.TypeUtv) != null ? ReflexVehicles.TypeUtv : ReflexVehicles.TypeSuperBuggy;
        }
        int ChoiceClass(ReflexEvents.Event e)
        {
            var cs = e.ClassesFor(ChoiceType(e));
            return cs.Length == 0 ? -1 : cs[Mathf.Clamp(classChoice, 0, cs.Length - 1)];
        }
        string ClassLabel(ReflexEvents.Event e)
        {
            int c = e != null ? ChoiceClass(e) : -1; if (c < 0) return "-";
            string n = ReflexEvents.ClassName(c) ?? c.ToString(); return n.Replace("STR_VEHICLECLASS_", "").Replace('_', ' ');
        }
        ReflexWorldFiles.Garage ChoiceGarage(ReflexEvents.Event e)
        {
            int t = VehicleChoices[vehicleChoice].type;
            int cls = classChoice > 0 ? ChoiceClass(e) : -1;
            if (t < 0)
            {
                if (e.MachineOnly) return e.GarageFor(ChoiceType(e), cls);
                // the profile's own rider and MX Lite in a Rival / Multi class of LocationSelect's list (no Vehicle Select equips)
                return cls >= 0 && cls != e.spec.vehicleClass ? new ReflexWorldFiles.Garage { vehicleClass = cls } : null;
            }
            return t == ReflexAtv.VehicleTypeAtv && cls < 0 ? e.AtvGarage() : e.GarageFor(t, cls);
        }
        public string defaultEvent = "National/NAT_Concord";
        public OriginalLevel level;
        public OriginalWorldRuntime runtime;
        public OriginalAudio gameAudio;
        public OriginalMusic music;

        public static string Pending;           // set by the list before the reload
        public static int PendingAi = -1;       // the front end's AI choice for the next load (-1: PlayerPrefs)
        public static int PendingRiders;        // RIDER CAP: the front end's RIDERS for the next load (0: the original's full gate of 12; REFLEX_RIDERS for diagnostics)
        public static int PendingLaps;          // the front end's Event Duration for the next load, in the type's unit (laps, minutes; 0: the table's)
        public static ReflexWorldFiles.Garage PendingGarage;   // the front end's Vehicle Select choice for the next load (null: the profile's)
        /// PRACTICE for the next / current load (Launch's practice; REFLEX_PRACTICE=1 with REFLEX_EVENT for a diagnostic launch). It stays set for
        /// the world's RESTART (a new session of the same practice) until the next launch.
        public static bool PendingPractice => Reflex.Port.RxCore.RxPracticeMode.Current;

        /// The front end's way into an event (FrontEnd scene, LocationSelect:OnAccept): the event key of ReflexEvents.All and the
        /// AI choice (0 off, 1.. ReflexWorldFiles.AiSkills), remembered like the event list's choice when `remember`, then the event scene.
        /// mapScale: MAP SCALE (the race setup's row, core/plan/MAP_SCALE.md) of the event's world, the world's own scale (the track's default
        /// times the row's percent); left out (NaN): the track's default (1 for the game's tracks; an Alive race level's base, BASE SCALE) for
        /// every other launch (career, MotoSkills, test ride, challenges): RxMapScale is set here before the scene loads, so the world is made at it.
        /// practice: PRACTICE (PLAY > PRACTICE): the lap race is endless, you alone (RxPracticeMode; every other
        /// launch clears it).
        /// weather / weatherIntensity: WEATHER (the RACE / FREE RIDE setup's rows, Rendering/ReflexWeather.cs: 0 CLEAR, 1 RAIN, 2 SNOW, 3 DYNAMIC;
        /// 0 LIGHT .. 2 HEAVY). Every other launch rides CLEAR; the pause page's reload of the same ride passes the ride's own (ReflexWeather.RideMode).
        public static void Launch(string eventKey, int ai, bool remember = true, string scene = "Reflex", int laps = 0, ReflexWorldFiles.Garage garage = null, float mapScale = float.NaN, bool practice = false, int riders = 0, int weather = 0, int weatherIntensity = 1)
        {
            Reflex.Port.Rendering.ReflexWeather.SetRide(practice ? 0 : weather, weatherIntensity);
            float mapBase = MapScaleBaseOf(eventKey);
            if (float.IsNaN(mapScale) && ReflexEvents.Find(eventKey)?.trackMod?.experimentalTerrain == true)
                mapScale = ExperimentalMtxScale.PercentFor(ReflexEvents.Find(eventKey).trackMod) / 100f;
            Reflex.Port.RxCore.RxMapScale.Set(float.IsNaN(mapScale) ? mapBase : mapScale, mapBase);
            Reflex.Port.RxCore.RxPracticeMode.Set(practice);
            if (practice) ai = 0;
            if (remember) { PlayerPrefs.SetString(PrefsKey, eventKey); PlayerPrefs.SetInt(AiKey, ai); PlayerPrefs.Save(); }
            Pending = eventKey; PendingAi = ai; PendingLaps = laps; PendingGarage = garage; PendingRiders = riders;
            SceneManager.LoadScene(scene);
        }
        public string Current { get; private set; }

        /// BASE SCALE: the default MAP SCALE of an event's track (Content/AliveLevels.MapScaleBase: an Alive race level's; 1 for the game's tracks and
        /// a custom track).
        public static float MapScaleBaseOf(string eventKey)
        {
            var ev = string.IsNullOrEmpty(eventKey) ? null : ReflexEvents.Find(eventKey);
            return ev == null || ev.trackMod != null || string.IsNullOrEmpty(ev.location) ? 1f : AliveLevels.MapScaleBase(ev.location);
        }

        /// The launch's Vehicle Select choice as the world takes it: the front end's, or for a machine-only event without one the shell's
        /// setup's machine, else the list's first (its choices at their start: the event's own vehicle type, the first class).
        internal static ReflexWorldFiles.Garage LaunchGarage(ReflexEvents.Event ev, ReflexWorldFiles.Garage garage)
        {
            if (garage != null || !ev.MachineOnly) return garage;
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) garage = new Reflex.Port.OpenFrontEnd.OpenSetup().Garage(ev);
            return garage ?? ev.GarageFor(ev.GarageFor(ReflexVehicles.TypeUtv) != null ? ReflexVehicles.TypeUtv : ReflexVehicles.TypeSuperBuggy, -1);   // (ChoiceGarage at its start)
        }
        /// The AI choice (0 off, 1.. ReflexWorldFiles.AiSkills) as the world's opponents and AIDifficulty.
        internal static void LaunchAi(int aiChoice, out int opponents, out int aiSkill, int riders = 0)
        {
            aiChoice = Mathf.Clamp(aiChoice, 0, ReflexWorldFiles.AiSkills.Length);
            opponents = aiChoice > 0 ? ReflexWorldFiles.OpponentsFor(riders) : 0;
            aiSkill = aiChoice > 0 ? ReflexWorldFiles.AiSkills[aiChoice - 1].skill : 0;
        }

        Canvas canvas; RectTransform listRoot; Text title; readonly List<Text> rows = new List<Text>();
        IReadOnlyList<ReflexEvents.Event> events; int cursor, top;
        const int Visible = 14;
        bool open;
        public bool IsOpen => open;

        void Awake()
        {
            // diagnostics (players: the pacing / shot probes): REFLEX_EVENT=<event key> REFLEX_AI=<0..> pick the race without PlayerPrefs
            string envEvent = Environment.GetEnvironmentVariable("REFLEX_EVENT"), envAi = Environment.GetEnvironmentVariable("REFLEX_AI");
            if (Pending == null && !string.IsNullOrEmpty(envEvent))
            {
                Pending = envEvent;
                // REFLEX_MAP_SCALE=<0.5 .. 2>: the diagnostic launch's MAP SCALE (the probes; the front end passes its own to Launch)
                string envScale = Environment.GetEnvironmentVariable("REFLEX_MAP_SCALE");
                // (BASE SCALE: an Alive race level's default when it is left out; REFLEX_MAP_SCALE is the world's own scale)
                float mapBase = MapScaleBaseOf(envEvent);
                if (!string.IsNullOrEmpty(envScale) && float.TryParse(envScale, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float ms))
                    Reflex.Port.RxCore.RxMapScale.Set(ms, mapBase);
                else Reflex.Port.RxCore.RxMapScale.Set(mapBase, mapBase);
                // REFLEX_PRACTICE=1: the diagnostic launch is a practice session (a lap race only)
                if (Environment.GetEnvironmentVariable("REFLEX_PRACTICE") == "1") { Reflex.Port.RxCore.RxPracticeMode.Set(true); if (PendingAi < 0) PendingAi = 0; }
            }
            if (PendingAi < 0 && int.TryParse(envAi, out int ai)) PendingAi = ai;
            // REFLEX_RIDERS=<2..32>: the diagnostic launch's field (RIDER CAP; the front end passes its own to Launch)
            if (PendingRiders <= 0 && int.TryParse(Environment.GetEnvironmentVariable("REFLEX_RIDERS"), out int envRiders)) PendingRiders = envRiders;
            string key = Pending ?? PlayerPrefs.GetString(PrefsKey, defaultEvent);
            Pending = null;
            ReflexEvents.Event ev = null;
            try { ev = ReflexEvents.Find(key) ?? ReflexEvents.Find(defaultEvent); }
            catch (Exception e) { Debug.LogWarning("Reflex events not read from the install: " + e.Message); }
            TrackMods.Bind(ev?.trackMod);   // MODS/TRACKS: a custom track's files bound to its level name for this event (none for the original's)
            if (ev == null)
            {
                if (OwnCareerLaunch.HasPending) OwnCareerLaunch.Prepare(runtime ? runtime : FindAnyObjectByType<OriginalWorldRuntime>(), null);
                return;
            }
            Current = ev.key;
            if (!level) level = FindAnyObjectByType<OriginalLevel>();
            if (!runtime) runtime = FindAnyObjectByType<OriginalWorldRuntime>();
            if (!gameAudio) gameAudio = FindAnyObjectByType<OriginalAudio>();
            if (!music) music = FindAnyObjectByType<OriginalMusic>();
            // the activity's own scene section as the game names it (ReflexEvents.SceneSection, FUN_00555e60): a track-1 waypoint race
            // loads "<level>WaypointRace1", not track 0's "<level>WaypointRace"; the race types' "Exhibition<type>" sections do not exist
            if (level) { level.level = ev.location; level.activity = ev.SceneActivity; }
            aiChoice = Mathf.Clamp(PendingAi >= 0 ? PendingAi : PlayerPrefs.GetInt(AiKey, 2), 0, ReflexWorldFiles.AiSkills.Length);   // default: Normal
            if (PendingPractice) aiChoice = 0;   // PRACTICE: you alone (OriginalWorldRuntime.OpenEventSpec makes the spec one rider too)
            PendingAi = -1;
            fieldRiders = PendingPractice ? 0 : Math.Min(PendingRiders, Reflex.Port.OpenFrontEnd.OpenSetup.FieldCap(ev)); PendingRiders = 0;   // (RIDER CAP: the modes kept at 12)
            int laps = PendingLaps; PendingLaps = 0;
            var garage = LaunchGarage(ev, PendingGarage); PendingGarage = null;
            atv = garage != null && garage.vehicleTypeId == ReflexAtv.VehicleTypeAtv;
            vehicleChoice = Math.Max(0, Array.FindIndex(VehicleChoices, c => garage != null && c.type == garage.vehicleTypeId));
            classChoice = garage != null && garage.vehicleClass >= 0 ? Math.Max(0, Array.IndexOf(ev.ClassesFor(ChoiceType(ev)), garage.vehicleClass)) : 0;
            if (runtime) { runtime.level = ev.key; runtime.opponents = Opponents; runtime.aiSkill = AiSkill; runtime.laps = laps; runtime.garage = garage; }
            bool ownedCareer = OwnCareerLaunch.Prepare(runtime, ev);
            if (runtime && !ownedCareer) OriginalLiveUi.Attach(runtime, ev);   // the game modes: the game's own UI, live (OriginalLiveUi, ReflexModes)
            if (gameAudio) gameAudio.level = ev.location;
            if (music) { music.level = ev.location; music.activity = ev.spec.type == "WaypointRace" ? "FreeRide" : ev.spec.type; }
        }

        public static string DisplayName(ReflexEvents.Event e)
        {
            string loc = e.location;
            int us = loc.IndexOf('_'); if (us >= 0) loc = loc.Substring(us + 1);
            if (loc.EndsWith("BW")) loc = loc.Substring(0, loc.Length - 2) + " (BW)";
            else if (loc.EndsWith("B") && loc.Length > 1 && char.IsLower(loc[loc.Length - 2])) loc = loc.Substring(0, loc.Length - 1) + " B";
            string type = e.typeName ?? e.spec.type;
            return type.ToUpperInvariant() + "  ·  " + loc.ToUpperInvariant() + (e.spec.trackId > 0 ? "  #" + e.spec.trackId : "");
        }

        void Update()
        {
            if (runtime && (runtime.OwnMenuActive || runtime.OwnedCareerActive)) return;
            var kb = Keyboard.current; if (kb == null) return;
            if (kb.insertKey.wasPressedThisFrame) { if (open) Close(); else Open(); }
            if (!open) return;
            if (kb.escapeKey.wasPressedThisFrame) { Close(); return; }
            if (kb.downArrowKey.wasPressedThisFrame) Move(1);
            if (kb.upArrowKey.wasPressedThisFrame) Move(-1);
            if (kb.pageDownKey.wasPressedThisFrame) Move(Visible);
            if (kb.pageUpKey.wasPressedThisFrame) Move(-Visible);
            if (kb.leftArrowKey.wasPressedThisFrame) StepAi(-1);
            if (kb.rightArrowKey.wasPressedThisFrame) StepAi(1);
            if (kb.deleteKey.wasPressedThisFrame) { vehicleChoice = (vehicleChoice + 1) % VehicleChoices.Length; atv = VehicleChoices[vehicleChoice].type == ReflexAtv.VehicleTypeAtv; classChoice = 0; Refresh(); }
            if (kb.homeKey.wasPressedThisFrame && events != null && cursor < events.Count) { int n = events[cursor].ClassesFor(ChoiceType(events[cursor])).Length; classChoice = n > 0 ? (classChoice + 1) % n : 0; Refresh(); }
            if (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame) Choose(cursor);
        }

        void Move(int d) { if (events == null || events.Count == 0) return; cursor = Mathf.Clamp(cursor + d, 0, events.Count - 1); if (cursor < top) top = cursor; if (cursor >= top + Visible) top = cursor - Visible + 1; Refresh(); }

        void StepAi(int d) { aiChoice = Mathf.Clamp(aiChoice + d, 0, ReflexWorldFiles.AiSkills.Length); Refresh(); }

        void Choose(int i)
        {
            if (events == null || i < 0 || i >= events.Count) return;
            var ev = events[i];
            PlayerPrefs.SetString(PrefsKey, ev.key); PlayerPrefs.SetInt(AiKey, aiChoice); PlayerPrefs.Save();
            Pending = ev.key; PendingGarage = ChoiceGarage(ev);
            SceneManager.LoadScene(SceneManager.GetActiveScene().buildIndex >= 0 ? SceneManager.GetActiveScene().buildIndex : 0);
        }

        void Open()
        {
            if (runtime && (runtime.OwnMenuActive || runtime.OwnedCareerActive)) return;
            try { events = ReflexEvents.All; } catch (Exception e) { Debug.LogWarning(e.Message); return; }
            if (!canvas) Build();
            cursor = 0; for (int i = 0; i < events.Count; i++) if (events[i].key == Current) cursor = i;
            top = Mathf.Max(0, cursor - Visible / 2);
            canvas.enabled = open = true; Refresh();
            if (runtime) runtime.paused = true;
        }
        void Close() { if (canvas) canvas.enabled = false; open = false; if (runtime) runtime.paused = false; }

        void Build()
        {
            var cgo = new GameObject("Event list (Insert)", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
            cgo.transform.SetParent(transform, false);
            canvas = cgo.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay; canvas.sortingOrder = 50;
            var sc = cgo.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize; sc.referenceResolution = new Vector2(1280, 720); sc.matchWidthOrHeight = 0.5f;
            var bg = new GameObject("Backdrop", typeof(RectTransform), typeof(Image)); bg.transform.SetParent(cgo.transform, false);
            var br = (RectTransform)bg.transform; br.anchorMin = br.anchorMax = new Vector2(0.5f, 0.5f); br.sizeDelta = new Vector2(620, 80 + Visible * 34);
            bg.GetComponent<Image>().color = new Color(0.03f, 0.04f, 0.06f, 0.92f);
            listRoot = br;
            var font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
            Text Label(string name, Vector2 pos, int size, Color c)
            {
                var go = new GameObject(name, typeof(RectTransform), typeof(Text)); go.transform.SetParent(listRoot, false);
                var r = (RectTransform)go.transform; r.anchorMin = r.anchorMax = r.pivot = new Vector2(0, 1); r.anchoredPosition = pos; r.sizeDelta = new Vector2(580, 32);
                var t = go.GetComponent<Text>(); t.font = font; t.fontSize = size; t.color = c; t.alignment = TextAnchor.MiddleLeft; t.raycastTarget = false;
                return t;
            }
            title = Label("Title", new Vector2(20, -12), 22, new Color(0.96f, 0.78f, 0.2f));
            for (int i = 0; i < Visible; i++) rows.Add(Label("Row " + i, new Vector2(20, -52 - 34 * i), 20, Color.white));
            canvas.enabled = false;
        }

        void Refresh()
        {
            if (title) title.text = $"EVENTS   AI ◀ {AiLabel} ▶   DEL {VehicleChoices[vehicleChoice].label}   HOME {ClassLabel(events != null && cursor < events.Count ? events[cursor] : null)}   ↑↓  ENTER  ESC";
            for (int i = 0; i < rows.Count; i++)
            {
                int k = top + i; var t = rows[i];
                if (events == null || k >= events.Count) { t.text = ""; continue; }
                var e = events[k];
                var g = ChoiceGarage(e); bool none = vehicleChoice > 0 && g == null || classChoice > 0 && ChoiceClass(e) < 0;
                bool built = false; try { built = ReflexWorldFiles.For(e.key, Opponents, AiSkill, 0, g).Ready; } catch (Exception) { }
                t.text = (k == cursor ? "▶ " : "   ") + DisplayName(e) + (e.key == Current ? "   (current)" : "") + (none ? "   · no " + VehicleChoices[vehicleChoice].label + " class" : "") + (built ? "" : "   · first run builds it");
                t.color = k == cursor ? new Color(0.96f, 0.78f, 0.2f) : Color.white;
            }
        }
    }
}
