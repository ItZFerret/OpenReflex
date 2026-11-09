// The driving cameras the player cycles (the Camera action, F7, the pause pages' CAMERA rows, OPTIONS > CAMERA & HUD > DRIVING CAMERA) and the
// player's own third-person presets (w-campresets).
//   entries   the game's four views (VISUAL, GAME PLAY, USER, 1ST PERSON: the core's DrivingCamera presets), DYNAMIC FPV (OriginalOnboardCamera) and every
//             custom preset, in that order (the built-ins in the old cycle's order: VISUAL, GAME PLAY, 1ST PERSON, DYNAMIC FPV, USER); each ON or OFF in the
//             cycle (the cycle skips OFF entries; the last ON entry cannot be turned off)
//   preset    a name (up to 20 letters, digits, spaces, - and _), a BASE (one of the game's third-person views) and the client's adjustments over that view's
//             pose each frame (OriginalWorld/ReflexCameraRig.cs): field of view (a scale of the game's or fixed degrees), distance, height, side, pitch,
//             position / rotation lag, how much it rolls with the bike, and shake (speed, landings and impacts, rough ground; a master amount)
//   current   the entry in use: a custom preset is its BASE in the core (ReflexSettings.DrivingCamera, so every session's players start on it:
//             ReflexClientLive.SyncGameSettings), plus its adjustments in the client; a built-in is what it always was. A deleted or turned-off current
//             entry falls back to the next ON entry
//   stored    save.json (ReflexSave, the client's own keys, so the backups carry them): "Camera.Presets" (JSON: the presets, the built-ins turned off) and
//             "Camera.Current" (the entry's id: "game:0".."game:3", "fpv", "custom:<n>"). Nothing stored: three starter presets (CLOSE CHASE, HIGH WIDE,
//             CINEMATIC), OFF in the cycle until the player turns them on, so the cycle is the old one until then
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Options
{
    /// One custom third-person camera (the fields are its JSON in save.json).
    [Serializable]
    public sealed class CameraPreset
    {
        public int id;
        public string name = "";
        /// The game's view under it: 0 VISUAL, 1 GAME PLAY, 2 USER (the core's DrivingCamera preset).
        public int baseView = 1;
        public bool on = true;
        /// FIELD OF VIEW: fovMode 0 = fovScale % of the game view's own (tangent space), 1 = fovDegrees fixed (horizontal, as the first-person view's).
        public int fovMode, fovScale = 100, fovDegrees = 85;
        /// Feet: back (+) / in (-) along the view, up / down, right / left; pitch in degrees (+ looks down).
        public float distance, height, side, pitch;
        /// Seconds: how far the camera's place (around the bike) and its aim trail (0 = locked to the game's view).
        public float posLag, rotLag;
        /// How much the horizon rolls with the bike (0 = level, 1 = the bike's whole lean).
        public float rollFollow;
        /// SHAKE: the master amount (0 .. 2) and its three sources (0 .. 1): speed, landings and impacts, rough ground.
        public float shake = 1f, shakeSpeed, shakeLanding, shakeRough;
        /// Who made it (CAMERA SHARING: the editor's AUTHOR, a share code's; "" none).
        public string author = "";

        public CameraPreset Clone() => (CameraPreset)MemberwiseClone();
        /// Its entry's id ("custom:<id>"; made once, read every frame).
        public string Id { get { if (idText == null || idOf != id) { idText = ReflexCameraPresets.CustomPrefix + id; idOf = id; } return idText; } }
        [NonSerialized] string idText; [NonSerialized] int idOf;
    }

    public static class ReflexCameraPresets
    {
        public const int MaxPresets = 16, MaxName = 20;
        public const string PresetsKey = "Camera.Presets", CurrentKey = "Camera.Current", FpvId = "fpv", GamePrefix = "game:", CustomPrefix = "custom:";
        /// The custom presets' BASE views (the game's third-person DrivingCamera presets 0..2).
        public static readonly string[] BaseNames = { "VISUAL", "GAME PLAY", "USER" };
        /// The built-in entries in the cycle's order (ReflexClientLive.CameraNames indices: 4 = DYNAMIC FPV).
        static readonly int[] BuiltinOrder = { 0, 1, 3, 4, 2 };

        public sealed class Entry
        {
            public string id, name;
            public bool on;
            /// The built-in's ReflexClientLive.CameraNames index (0..3 the game's views, 4 DYNAMIC FPV); -1 for a custom preset.
            public int builtin = -1;
            public CameraPreset preset;
            public bool Custom => preset != null;
        }

        [Serializable]
        sealed class Store
        {
            public int version = 1, nextId = 1;
            public List<CameraPreset> presets = new List<CameraPreset>();
            public List<string> off = new List<string>();
        }

        static Store s_store;
        static readonly List<Entry> s_entries = new List<Entry>();
        static readonly string[] GameIds = { GamePrefix + 0, GamePrefix + 1, GamePrefix + 2, GamePrefix + 3, FpvId };
        static string BuiltinId(int index) => GameIds[Mathf.Clamp(index, 0, 4)];
        static string s_current;            // the selected entry's id (this session; "Camera.Current" when persisted)
        static CameraPreset s_currentCustom; // the selected custom preset (null: a built-in)
        static bool s_writing, s_hooked;

        /// Bumped by every change of the list, a preset or the selection (the options pages, the HUDs).
        public static int Version { get; private set; }
        /// While the options' camera editor is open over a race: the preset it edits (its view shows at once; the shake moves while the race is paused).
        public static CameraPreset Preview;

        // ------------------------------------------------------------------------------------------------ store
        static void Load()
        {
            if (s_store != null) return;
            if (!s_hooked)
            {
                s_hooked = true;
                ReflexSettings.Changed += key => { if (!s_writing && (key == PresetsKey || key == CurrentKey || key == null)) { s_store = null; Version++; } };   // (a restored backup)
            }
            Store st = null;
            string json = ReflexSave.GetString(PresetsKey, "");
            if (json.Length > 0) try { st = JsonUtility.FromJson<Store>(json); } catch (Exception e) { Debug.LogWarning("[Camera] presets: " + e.Message); }
            if (st == null) st = Starters();
            st.presets ??= new List<CameraPreset>(); st.off ??= new List<string>();
            st.presets.RemoveAll(p => p == null);
            if (st.presets.Count > MaxPresets) st.presets.RemoveRange(MaxPresets, st.presets.Count - MaxPresets);
            foreach (var p in st.presets) { Sanitize(p); if (p.id <= 0 || st.presets.Count(q => q.id == p.id) > 1) p.id = 0; }
            foreach (var p in st.presets) if (p.id == 0) p.id = Math.Max(st.nextId, st.presets.Max(q => q.id) + 1);
            st.nextId = Math.Max(st.nextId, st.presets.Count == 0 ? 1 : st.presets.Max(q => q.id) + 1);
            s_store = st;
            s_current = ReflexSave.GetString(CurrentKey, "");
            Rebuild();
            if (!s_entries.Any(e => e.on)) { s_store.off.Remove(s_entries[0].id); Rebuild(); }   // (a store with every entry off: the first one stays on)
        }

        /// The starter presets (a store that was never written): OFF in the cycle until the player turns them on.
        static Store Starters()
        {
            var st = new Store();
            st.presets.Add(new CameraPreset { id = 1, name = "CLOSE CHASE", baseView = 1, on = false, distance = -4f, height = -0.8f, pitch = -1f, posLag = 0.04f, rotLag = 0.03f,
                                              shake = 1f, shakeSpeed = 0.35f, shakeLanding = 0.6f, shakeRough = 0.35f });
            st.presets.Add(new CameraPreset { id = 2, name = "HIGH WIDE", baseView = 1, on = false, fovMode = 1, fovDegrees = 95, distance = 7f, height = 6f, pitch = 7f, posLag = 0.1f, rotLag = 0.08f,
                                              shake = 0.6f, shakeSpeed = 0.2f, shakeLanding = 0.4f, shakeRough = 0.15f });
            st.presets.Add(new CameraPreset { id = 3, name = "CINEMATIC", baseView = 1, on = false, fovScale = 88, distance = 3f, height = 1f, side = 2.5f, pitch = 1f, posLag = 0.35f, rotLag = 0.28f,
                                              rollFollow = 0.35f, shake = 1.2f, shakeSpeed = 0.6f, shakeLanding = 1f, shakeRough = 0.6f });
            st.nextId = 4;
            return st;
        }

        static void Sanitize(CameraPreset p)
        {
            p.name = CleanName(p.name); if (p.name.Length == 0) p.name = "CAMERA";
            p.author = CameraConfig.CleanText(p.author, CameraConfig.MaxAuthor);
            p.baseView = Mathf.Clamp(p.baseView, 0, 2);
            p.fovMode = Mathf.Clamp(p.fovMode, 0, 1); p.fovScale = Mathf.Clamp(p.fovScale, FovScaleMin, FovScaleMax); p.fovDegrees = Mathf.Clamp(p.fovDegrees, FovDegreesMin, FovDegreesMax);
            p.distance = Clamp(p.distance, DistanceMin, DistanceMax); p.height = Clamp(p.height, HeightMin, HeightMax); p.side = Clamp(p.side, -SideMax, SideMax);
            p.pitch = Clamp(p.pitch, -PitchMax, PitchMax); p.posLag = Clamp(p.posLag, 0f, LagMax); p.rotLag = Clamp(p.rotLag, 0f, LagMax);
            p.rollFollow = Clamp(p.rollFollow, 0f, 1f); p.shake = Clamp(p.shake, 0f, 2f);
            p.shakeSpeed = Clamp(p.shakeSpeed, 0f, 1f); p.shakeLanding = Clamp(p.shakeLanding, 0f, 1f); p.shakeRough = Clamp(p.shakeRough, 0f, 1f);
        }
        static float Clamp(float v, float lo, float hi) => float.IsNaN(v) ? 0f : Mathf.Clamp(v, lo, hi);

        /// The editor's ranges (Sanitize clamps a stored preset to them).
        public const int FovScaleMin = 50, FovScaleMax = 200, FovDegreesMin = 40, FovDegreesMax = 140;
        public const float DistanceMin = -10f, DistanceMax = 30f, HeightMin = -6f, HeightMax = 20f, SideMax = 12f, PitchMax = 25f, LagMax = 1f;

        static void Write()
        {
            s_writing = true;
            try { ReflexSave.SetString(PresetsKey, JsonUtility.ToJson(s_store)); ReflexSave.Save(); }
            finally { s_writing = false; }
            Rebuild();
        }

        static void Rebuild()
        {
            s_entries.Clear();
            foreach (int b in BuiltinOrder)
            {
                string id = BuiltinId(b);
                s_entries.Add(new Entry { id = id, name = ReflexClientLive.CameraNames[b], builtin = b, on = !s_store.off.Contains(id) });
            }
            foreach (var p in s_store.presets) s_entries.Add(new Entry { id = p.Id, name = p.name, on = p.on, preset = p });
            s_currentCustom = s_current != null && s_current.StartsWith(CustomPrefix, StringComparison.Ordinal) ? s_store.presets.Find(p => p.Id == s_current) : null;
            Version++;
        }

        // ------------------------------------------------------------------------------------------------ the list
        /// Every entry in the cycle's order (built-ins, then the custom presets).
        public static IReadOnlyList<Entry> Entries { get { Load(); return s_entries; } }
        public static IReadOnlyList<CameraPreset> Presets { get { Load(); return s_store.presets; } }
        public static bool Full => Presets.Count >= MaxPresets;
        public static Entry Find(string id)
        {
            Load();
            for (int i = 0; i < s_entries.Count; i++) if (string.Equals(s_entries[i].id, id, StringComparison.Ordinal)) return s_entries[i];   // (every frame: no closure)
            return null;
        }
        public static CameraPreset FindPreset(int id)
        {
            Load();
            foreach (var p in s_store.presets) if (p.id == id) return p;
            return null;
        }

        /// The entry in use: the game's / client's view of the running race (or the saved one), the selected custom preset when it is that view's BASE.
        public static string CurrentId
        {
            get
            {
                Load();
                var rt = ReflexClientLive.Runtime;
                int mode = rt ? rt.cameraMode : ReflexSettings.CameraMode;
                if (mode == 1) return FpvId;
                int dc = ReflexClientLive.CameraIndex;
                if (s_currentCustom != null && s_currentCustom.baseView == dc) return s_currentCustom.Id;
                return BuiltinId(Mathf.Clamp(dc, 0, 3));
            }
        }
        public static Entry Current => Find(CurrentId) ?? Entries[0];

        /// The custom preset the game camera shows now (OriginalWorldRuntime.ApplyGameCamera): camera mode 0 (the game's camera) with the selected
        /// preset's BASE as the driving preset (dc); the editor's preset while it previews. Null: the game's view as it is.
        public static CameraPreset ActiveFor(int mode, int dc)
        {
            if (mode != 0) return null;
            if (Preview != null) return Preview;
            Load();
            return s_currentCustom != null && s_currentCustom.baseView == dc ? s_currentCustom : null;
        }

        /// The name the camera toast shows (the follow camera: "(FOLLOW)" after it).
        public static string DisplayName(int mode) => Current.name + (mode == 2 ? "  (FOLLOW)" : "");
        /// A number that changes with the view on screen (the toasts compare it): the camera mode and the entry.
        public static int ViewKey(int mode) { var e = Current; return mode * 100000 + (e.Custom ? 1000 + e.preset.id : e.builtin); }

        // ------------------------------------------------------------------------------------------------ selecting
        /// Makes the entry the camera (persist: saved as the camera of every session). A built-in sets the game's view / Dynamic FPV as it always did;
        /// a custom preset sets its BASE as the game's DrivingCamera and its adjustments over it.
        public static void Select(string id, bool persist = true)
        {
            Load();
            var e = Find(id);
            if (e == null) return;
            s_current = e.id;
            s_currentCustom = e.preset;
            if (e.Custom) ReflexClientLive.ApplyView(e.preset.baseView, persist);
            else ReflexClientLive.ApplyView(e.builtin, persist);
            if (persist) { s_writing = true; try { ReflexSave.SetString(CurrentKey, e.id); ReflexSave.Save(); } finally { s_writing = false; } }
            Version++;
        }

        /// A built-in view was chosen another way (ReflexClientLive.SelectCamera: the front end's and the pause pages' own lists).
        internal static void NoteBuiltin(int index, bool persist)
        {
            Load();
            s_current = BuiltinId(index); s_currentCustom = null;
            if (persist) { s_writing = true; try { ReflexSave.SetString(CurrentKey, s_current); } finally { s_writing = false; } }
            Version++;
        }

        /// The next (direction +1) or previous ON entry after the current one; the current one when it is the only one.
        public static string NextId(string from, int direction)
        {
            var list = Entries;
            int at = -1; for (int i = 0; i < list.Count; i++) if (list[i].id == from) at = i;
            if (at < 0) at = 0;
            int n = list.Count, d = direction < 0 ? -1 : 1;
            for (int k = 1; k <= n; k++)
            {
                var e = list[((at + d * k) % n + n) % n];
                if (e.on) return e.id;
            }
            return from;
        }

        /// The Camera action / F7 / the pause pages: the next ON entry.
        public static void Cycle(int direction = 1) => Select(NextId(CurrentId, direction));

        /// The ON entries in the cycle's order, and the current one where it sits when it is OFF (the pause pages' lists).
        public static List<Entry> Listed() { string cur = CurrentId; return Entries.Where(e => e.on || e.id == cur).ToList(); }

        // ------------------------------------------------------------------------------------------------ on / off
        /// False when it is the last ON entry (the cycle must keep one).
        public static bool CanTurnOff(string id) { var e = Find(id); return e != null && (!e.on || Entries.Count(x => x.on) > 1); }

        /// Puts the entry in or out of the cycle; false (nothing changes) for the last ON one. Turning off the current entry selects the next ON one.
        public static bool SetOn(string id, bool on)
        {
            var e = Find(id);
            if (e == null || e.on == on) return e != null;
            if (!on && !CanTurnOff(id)) return false;
            bool wasCurrent = CurrentId == id;
            if (e.Custom) e.preset.on = on;
            else if (on) s_store.off.Remove(id); else if (!s_store.off.Contains(id)) s_store.off.Add(id);
            Write();
            if (!on && wasCurrent) Select(NextId(id, 1));
            return true;
        }

        // ------------------------------------------------------------------------------------------------ the presets
        /// Letters, digits, spaces, "-" and "_", upper case, up to MaxName.
        public static string CleanName(string s)
        {
            var t = new string((s ?? "").ToUpperInvariant().Where(c => char.IsLetterOrDigit(c) && c < 128 || c == ' ' || c == '-' || c == '_').ToArray()).Trim();
            while (t.Contains("  ")) t = t.Replace("  ", " ");
            return t.Length > MaxName ? t.Substring(0, MaxName).TrimEnd() : t;
        }

        /// name, or name 2, name 3 ... when another preset has it.
        static string Unique(string name, int except)
        {
            name = CleanName(name); if (name.Length == 0) name = "CAMERA";
            bool Taken(string n) => s_store.presets.Any(p => p.id != except && string.Equals(p.name, n, StringComparison.Ordinal)) || ReflexClientLive.CameraNames.Contains(n);
            if (!Taken(name)) return name;
            for (int i = 2; ; i++)
            {
                string suffix = " " + i, stem = name.Length + suffix.Length > MaxName ? name.Substring(0, MaxName - suffix.Length).TrimEnd() : name;
                if (!Taken(stem + suffix)) return stem + suffix;
            }
        }

        /// NEW PRESET: a preset over the view in use (a custom one is copied; the game's third-person views are its BASE; first person: GAME PLAY), ON in
        /// the cycle. Null when there are MaxPresets already.
        public static CameraPreset Create()
        {
            Load();
            if (Full) return null;
            var cur = Current;
            CameraPreset p = cur.Custom ? cur.preset.Clone() : new CameraPreset { baseView = cur.builtin >= 0 && cur.builtin <= 2 ? cur.builtin : 1 };
            p.on = true;
            p.id = s_store.nextId++;
            p.name = Unique(cur.Custom ? cur.preset.name : "MY CAMERA", p.id);
            s_store.presets.Add(p);
            Write();
            return p;
        }

        /// DUPLICATE: a copy after the original ("<name> 2"), ON in the cycle. Null when full.
        public static CameraPreset Duplicate(int id)
        {
            var src = FindPreset(id);
            if (src == null || Full) return null;
            var p = src.Clone();
            p.id = s_store.nextId++; p.on = true;
            p.name = Unique(src.name, p.id);
            s_store.presets.Insert(s_store.presets.IndexOf(src) + 1, p);
            Write();
            return p;
        }

        /// IMPORT (CAMERA SHARING: a share code's or a file's camera): a new preset with p's values after the others, its name made unique, ON in the
        /// cycle. Null when there are MaxPresets already.
        public static CameraPreset Import(CameraPreset p)
        {
            Load();
            if (p == null || Full) return null;
            p = p.Clone();
            p.id = s_store.nextId++; p.on = true;
            Sanitize(p);
            p.name = Unique(p.name, p.id);
            s_store.presets.Add(p);
            Write();
            return p;
        }

        /// RENAME: the cleaned name (empty: no change; another preset's name: numbered). The name it has now.
        public static string Rename(int id, string name)
        {
            var p = FindPreset(id);
            if (p == null) return null;
            if (CleanName(name).Length == 0) return p.name;
            p.name = Unique(name, id);
            Write();
            return p.name;
        }

        /// DELETE: the preset goes; the current camera falls back to the next ON entry when it was this one (and something stays ON).
        public static void Delete(int id)
        {
            var p = FindPreset(id);
            if (p == null) return;
            string pid = p.Id;
            bool wasCurrent = CurrentId == pid || s_current == pid;
            string next = NextId(pid, 1);
            if (Preview == p) Preview = null;
            s_store.presets.Remove(p);
            Write();
            if (!s_entries.Any(e => e.on)) { s_store.off.Remove(s_entries[0].id); Write(); }   // (it was the only ON entry: the first built-in comes back)
            if (wasCurrent) Select(next != pid && Find(next) != null ? next : s_entries.First(e => e.on).id);
        }

        /// A preset's value changed in the editor (shown at once; saved by Changed when the editor is left).
        public static void Touch(CameraPreset p) { if (p == null) return; Sanitize(p); Version++; }

        /// A preset's values changed (the editor): clamped and saved.
        public static void Changed(CameraPreset p)
        {
            if (p == null) return;
            Load();
            Sanitize(p);
            Write();
            if (Preview == null && s_current == p.Id && s_store.presets.Contains(p)) Select(p.Id, true);   // (the camera in use: its BASE may have changed)
        }

        /// Back to the starter presets' values (the editor's RESET of a starter keeps its name, id and ON).
        public static CameraPreset StarterValues(string name) => Starters().presets.Find(p => p.name == name);

        /// Tests: forget what was read (the next use reads the save again).
        public static void ReloadForTest() { s_store = null; Preview = null; }
    }
}
