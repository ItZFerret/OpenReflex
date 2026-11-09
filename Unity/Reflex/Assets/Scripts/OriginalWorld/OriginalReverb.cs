using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using UnityEngine;
using Node = Reflex.Port.Content.BxmlData.Node;

namespace Reflex.Port.OriginalWorld
{
    // The original's reverb (core/plan/QUEUE.md "w-reverb"; _re/notes/audio-port.md 9.1), reproduced on Unity's reverb zones.
    //
    // What the game does (FMOD Ex 4.22):
    //   * the level's ambient reverb: every level scene has a LevelAmbientAudioZoneRSData (SoundEffectEntityAmbient, vf5 00808ca0). Its
    //     ReverbEnvironment k = 1 .. 25 is the inlined FMOD_PRESET_* number k - 1 (SoundEffect_ReverbPreset 00808f30: GENERIC, PADDEDCELL ... DIZZY);
    //     0 takes the explicit fields. The ambient goes to System::setReverbAmbientProperties (command 0x14) at the world switch. Supercross
    //     stadiums and the freestyle arenas are ARENA, the outdoor tracks MOUNTAINS / PLAIN, the Free Ride maps FOREST / MOUNTAINS.
    //   * 3D reverb zones: AudioZoneSphereRSData (SoundEffectZoneEntitySphere: its position, MinDistance .. MaxDistance) and AudioLineZoneSphereRSData
    //     (SoundEffectLineZoneEntitySphere: the point of its polyline nearest the listener, every frame), each a System::createReverb (command 0x0b)
    //     + set3DAttributes (0x0d), active once the listener is inside MaxDistance (SoundEffectZone_Activate 0080a400).
    //   * the first-person camera: CameraSoundEffectsManager (config CameraSoundEffectsManagerConfig: MXFirstPersonCamera -> Reverb "FirstPerson", the
    //     FEV reverb definition of the level's Default project; getReverbPreset 00805610) is a 3D reverb on the camera, 50 .. 51 ft, active while the
    //     first-person camera is the player's view (0068d4b0: command 0x11 set3DAttributes at camera + 0x60, 0x10 setActive).
    //   * FMOD Ex sums all of them (FMOD 4.22 ReverbI morph 10097cad, 1008715f, 100872b0): each active 3D reverb weighs linear = 1 at or inside its
    //     min distance falling to 0 at its max; the ambient takes the rest of the unit weight (1 - the sum, when the sum is below 1) and the total is
    //     scaled by 1 / max(sum, 1). Room / RoomHF / RoomLF / Reflections / Reverb are averaged as exp(mB * 0.002) and turned back as
    //     500 ln(x), the decay and delays linearly, HFReference / LFReference in the log domain. The result is the one system reverb every
    //     channel is sent to (a channel's default reverb level is 0 mB: all of it).
    //
    // Here: one AudioReverbZone that follows the listener (min distance far above the map) carries that morphed result; the zones' maths are the
    // original's, evaluated on the host. Sources that play the game's 3D events (OriginalAudio voices) and the engine packs (which replace the 3D
    // engine events) are sent to it; 2D events (UI, the announcer, music) are not (bypassReverbZones). Which of 2D events FMOD Ex sends to its reverb
    // is not settled by the code read; the shipped events all carry the same default reverb level, so the choice is OriginalReverb.ReverbFor2D.
    //
    // Differences: the custom (ReverbEnvironment 0) zones are OFF, as the game's (their Instance field is uninitialised stack in the original and
    // FMOD's setProperties refuses it: SX_FortDodge's two spheres); the occlusion of the reverb send (set3DOcclusion's second value, 0.6 under
    // water) is not applied to the wet signal; FMOD's own room-level smoothing is not repeated (Unity's zone does its own).
    public sealed class OriginalReverb
    {
        // ------------------------------------------------------------------------------------------------ FMOD_REVERB_PROPERTIES
        /// The fields Unity's AudioReverbZone has (mB, seconds, Hz). FMOD_PRESET_OFF is Room / RoomHF -10000.
        public struct Props
        {
            public float room, roomHF, roomLF, decay, decayHF, refl, reflDelay, rev, revDelay, diffusion, density, hfRef, lfRef;
            public static Props Off => new Props { room = -10000f, roomHF = -10000f, roomLF = 0f, decay = 1f, decayHF = 1f, refl = -2602f, reflDelay = 0.007f, rev = 200f, revDelay = 0.011f, diffusion = 0f, density = 0f, hfRef = 5000f, lfRef = 250f };
            public bool IsOff => room <= -9999f;
            public override string ToString() => $"room {room:0} hf {roomHF:0} decay {decay:0.00} refl {refl:0}/{reflDelay:0.000} rev {rev:0}/{revDelay:0.000}";
        }
        static Props P(string name, int room, int roomHF, int roomLF, float decay, float decayHF, int refl, float reflDelay, int rev, float revDelay, float diffusion, float density) =>
            new Props { room = room, roomHF = roomHF, roomLF = roomLF, decay = decay, decayHF = decayHF, refl = refl, reflDelay = reflDelay, rev = rev, revDelay = revDelay, diffusion = diffusion, density = density, hfRef = 5000f, lfRef = 250f };

        /// The inlined FMOD_PRESET_* of SoundEffect_ReverbPreset 00808f30 in the order of ReverbEnvironment 1 .. 26 (FMOD_REVERB_PROPERTIES of the same
        /// names; EnvSize / EnvDiffusion / echo / modulation are not Unity zone fields). Only 1 .. 25 are reachable (the entity clamps the rest to OFF).
        static readonly Props[] Presets =
        {
            P("Generic", -1000, -100, 0, 1.49f, 0.83f, -2602, 0.007f, 200, 0.011f, 100.0f, 100.0f),
            P("Paddedcell", -1000, -6000, 0, 0.17f, 0.1f, -1204, 0.001f, 207, 0.002f, 100.0f, 100.0f),
            P("Room", -1000, -454, 0, 0.4f, 0.83f, -1646, 0.002f, 53, 0.003f, 100.0f, 100.0f),
            P("Bathroom", -1000, -1200, 0, 1.49f, 0.54f, -370, 0.007f, 1030, 0.011f, 60.0f, 60.0f),
            P("Livingroom", -1000, -6000, 0, 0.5f, 0.1f, -1376, 0.003f, -1104, 0.004f, 100.0f, 100.0f),
            P("Stoneroom", -1000, -300, 0, 2.31f, 0.64f, -711, 0.012f, 83, 0.017f, 100.0f, 100.0f),
            P("Auditorium", -1000, -476, 0, 4.32f, 0.59f, -789, 0.02f, -289, 0.03f, 100.0f, 100.0f),
            P("Concerthall", -1000, -500, 0, 3.92f, 0.7f, -1230, 0.02f, -2, 0.029f, 100.0f, 100.0f),
            P("Cave", -1000, 0, 0, 2.91f, 1.3f, -602, 0.015f, -302, 0.022f, 100.0f, 100.0f),
            P("Arena", -1000, -698, 0, 7.24f, 0.33f, -1166, 0.02f, 16, 0.03f, 100.0f, 100.0f),
            P("Hangar", -1000, -1000, 0, 10.05f, 0.23f, -602, 0.02f, 198, 0.03f, 100.0f, 100.0f),
            P("Carpettedhallway", -1000, -4000, 0, 0.3f, 0.1f, -1831, 0.002f, -1630, 0.03f, 100.0f, 100.0f),
            P("Hallway", -1000, -300, 0, 1.49f, 0.59f, -1219, 0.007f, 441, 0.011f, 100.0f, 100.0f),
            P("Stonecorridor", -1000, -237, 0, 2.7f, 0.79f, -1214, 0.013f, 395, 0.02f, 100.0f, 100.0f),
            P("Alley", -1000, -270, 0, 1.49f, 0.86f, -1204, 0.007f, -4, 0.011f, 100.0f, 100.0f),
            P("Forest", -1000, -3300, 0, 1.49f, 0.54f, -2560, 0.162f, -229, 0.088f, 100.0f, 100.0f),
            P("City", -1000, -800, 0, 1.49f, 0.67f, -2273, 0.007f, -1691, 0.011f, 100.0f, 100.0f),
            P("Mountains", -1000, -2500, 0, 1.49f, 0.21f, -2780, 0.3f, -1434, 0.1f, 27.0f, 100.0f),
            P("Quarry", -1000, -1000, 0, 1.49f, 0.83f, -10000, 0.061f, 500, 0.025f, 100.0f, 100.0f),
            P("Plain", -1000, -2000, 0, 1.49f, 0.5f, -2466, 0.179f, -1926, 0.1f, 100.0f, 100.0f),
            P("Parkinglot", -1000, 0, 0, 1.65f, 1.5f, -1363, 0.008f, -1153, 0.012f, 100.0f, 100.0f),
            P("Sewerpipe", -1000, -1000, 0, 2.81f, 0.14f, 429, 0.014f, 1023, 0.021f, 60.0f, 60.0f),
            P("Underwater", -1000, -4000, 0, 1.49f, 0.1f, -449, 0.007f, 1700, 0.011f, 100.0f, 100.0f),
            P("Drugged", -1000, 0, 0, 8.39f, 1.39f, -115, 0.002f, 985, 0.03f, 100.0f, 100.0f),
            P("Dizzy", -1000, -400, 0, 17.23f, 0.56f, -1713, 0.02f, -613, 0.03f, 100.0f, 100.0f),
            P("Psychotic", -1000, -151, 0, 7.56f, 0.91f, -626, 0.02f, 774, 0.03f, 100.0f, 100.0f),
        };
        /// ReverbEnvironment k of a zone entity: 1 .. 25 the preset, anything else OFF (SoundEffectEntity::vf5 clamps k outside 0 .. 25 to -1).
        public static Props PresetOf(int k) => k >= 1 && k <= 25 ? Presets[k - 1] : Props.Off;
        public static string PresetName(int k) => k >= 1 && k <= 25 ? new[] { "Generic", "Padded cell", "Room", "Bathroom", "Living room", "Stone room", "Auditorium", "Concert hall", "Cave", "Arena", "Hangar", "Carpetted hallway", "Hallway", "Stone corridor", "Alley", "Forest", "City", "Mountains", "Quarry", "Plain", "Parking lot", "Sewer pipe", "Underwater", "Drugged", "Dizzy" }[k - 1] : k == 0 ? "Custom" : "Off";

        /// The FEV reverb definition as a Props (EventSystem::getReverbPreset).
        public static Props FromDef(ReflexFmod.ReverbDef d) => new Props
        {
            room = d.room, roomHF = d.roomHF, roomLF = d.roomLF, decay = d.decayTime, decayHF = d.decayHFRatio, refl = d.reflections, reflDelay = d.reflectionsDelay,
            rev = d.reverb, revDelay = d.reverbDelay, diffusion = d.diffusion, density = d.density, hfRef = d.hfReference, lfRef = d.lfReference
        };

        // ------------------------------------------------------------------------------------------------ the morph (FMOD Ex 4.22 ReverbI)
        /// FMOD's distance weight of a 3D reverb: 1 inside the min distance, 0 beyond the max, linear between (10086f98).
        public static float Weight(float distance, float min, float max)
        {
            if (distance <= min) return 1f;
            if (distance >= max) return 0f;
            return 1f - (distance - min) / (max - min);
        }
        const float WeightFloor = 0.001f;   // 10097cad skips a reverb whose weight is below it

        /// The system reverb of the active 3D reverbs plus the ambient (null: none set = FMOD_PRESET_OFF with the 3D flag unset, nothing is sent).
        public static Props Morph(IList<(Props p, float w)> active, Props? ambient, bool ambientIsOff)
        {
            double room = 0, roomHF = 0, roomLF = 0, refl = 0, rev = 0;                       // Σ w exp(0.002 mB)
            double decay = 0, decayHF = 0, reflDelay = 0, revDelay = 0, diffusion = 0, density = 0;   // Σ w x
            double lnHf = 0, lnLf = 0;                                                         // Σ w ln x
            double total = 0;
            for (int i = 0; i < active.Count; i++)
            {
                var (p, w) = active[i];
                if (w < WeightFloor) continue;
                total += w;
                room += w * Math.Exp(0.002 * p.room); roomHF += w * Math.Exp(0.002 * p.roomHF); roomLF += w * Math.Exp(0.002 * p.roomLF);
                refl += w * Math.Exp(0.002 * p.refl); rev += w * Math.Exp(0.002 * p.rev);
                decay += w * p.decay; decayHF += w * p.decayHF; reflDelay += w * p.reflDelay; revDelay += w * p.revDelay; diffusion += w * p.diffusion; density += w * p.density;
                lnHf += w * Math.Log(p.hfRef); lnLf += w * Math.Log(p.lfRef);
            }
            if (ambient.HasValue && total < 1.0)
            {
                double w = 1.0 - total; var a = ambient.Value; total += w;
                room += w * Math.Exp(0.002 * a.room);                           // Environment -1 (OFF): only Room, at -10000 (1008715f)
                if (!ambientIsOff)
                {
                    roomHF += w * Math.Exp(0.002 * a.roomHF); roomLF += w * Math.Exp(0.002 * a.roomLF); refl += w * Math.Exp(0.002 * a.refl); rev += w * Math.Exp(0.002 * a.rev);
                    decay += w * a.decay; decayHF += w * a.decayHF; reflDelay += w * a.reflDelay; revDelay += w * a.revDelay; diffusion += w * a.diffusion; density += w * a.density;
                    lnHf += w * Math.Log(a.hfRef); lnLf += w * Math.Log(a.lfRef);
                }
            }
            if (total <= 0.0) return Props.Off;
            double scale = 1.0 / Math.Max(total, 1.0);
            float MB(double x) { x *= scale; return x <= 0.0 ? -10000f : (float)(int)(500.0 * Math.Log(x) + 0.5); }
            // setProperties' clamps (10086860)
            return new Props
            {
                room = Mathf.Clamp(MB(room), -10000f, 0f), roomHF = Mathf.Clamp(MB(roomHF), -10000f, 0f), roomLF = Mathf.Clamp(MB(roomLF), -10000f, 0f),
                refl = Mathf.Clamp(MB(refl), -10000f, 1000f), rev = Mathf.Clamp(MB(rev), -10000f, 2000f),
                decay = Mathf.Clamp((float)(decay * scale), 0.1f, 20f), decayHF = Mathf.Clamp((float)(decayHF * scale), 0.1f, 2f),
                reflDelay = Mathf.Clamp((float)(reflDelay * scale), 0f, 0.3f), revDelay = Mathf.Clamp((float)(revDelay * scale), 0f, 0.1f),
                diffusion = Mathf.Clamp((float)(diffusion * scale), 0f, 100f), density = Mathf.Clamp((float)(density * scale), 0f, 100f),
                hfRef = Mathf.Clamp((float)Math.Exp(lnHf * scale), 1000f, 20000f), lfRef = Mathf.Clamp((float)Math.Exp(lnLf * scale), 20f, 1000f)
            };
        }

        // ------------------------------------------------------------------------------------------------ the level's reverb entities
        sealed class Zone { public Props p; public Vector3[] pts; public bool line; public float min, max; public string label; }
        readonly List<Zone> zones = new List<Zone>();
        Props ambient = Props.Off; bool haveAmbient, ambientOff = true; string ambientName = "";
        Props? camera; string cameraName;
        bool levelRead; int levelTries;
        readonly List<(Props, float)> active = new List<(Props, float)>();

        /// Whether the reverb is sent at all (save key "OpenReflex.AudioReverb", default on = the original's; OPTIONS > AUDIO > REVERB, applied live).
        public static bool Enabled { get { if (EnabledOverride.HasValue) return EnabledOverride.Value; Refresh(); return enabled; } }
        /// Tests: forces the reverb on or off whatever the preference says.
        public static bool? EnabledOverride;
        /// The first-person camera's reverb (CameraSoundEffectsManager) on or off; on = the original's.
        public static bool FirstPersonReverb { get { Refresh(); return firstPerson; } }
        static bool enabled = true, firstPerson = true; static float prefsAt = -10f;
        static bool hooked;
        static void Refresh()
        {
            if (!hooked)
            {
                hooked = true;
                // the options rows change the keys: re-read at once (the zone follows on the next frame)
                try { Reflex.Port.Options.ReflexSave.Changed += k => { if (k != null && k.StartsWith("OpenReflex.AudioReverb", StringComparison.Ordinal)) prefsAt = -10f; }; } catch (Exception) { }
            }
            if (Time.unscaledTime - prefsAt < 0.5f && Time.unscaledTime >= prefsAt) return;
            prefsAt = Time.unscaledTime;
            try
            {
                enabled = Reflex.Port.Options.ReflexSave.GetBool("OpenReflex.AudioReverb", true); firstPerson = Reflex.Port.Options.ReflexSave.GetBool("OpenReflex.AudioReverbFirstPerson", true);
                // tests / diagnostics: OPENREFLEX_AUDIO_REVERB=0 (off) overrides the preference
                if (Environment.GetEnvironmentVariable("OPENREFLEX_AUDIO_REVERB") == "0") enabled = false;
            }
            catch (Exception) { }
        }
        /// Whether 2D events (UI, announcer, music) are sent to the reverb too. FMOD Ex gives every channel the default reverb level (Room 0 mB) and the
        /// shipped events all carry the same defaults, so the original may well reverberate them; the code read does not say. Off: they stay dry.
        public static bool ReverbFor2D = false;
        /// A zone whose ReverbEnvironment is 0 ("Custom Reverb": SX_FortDodge's two spheres) keeps FMOD_PRESET_OFF, as the game's (see the header). False: it takes its
        /// explicit Room .. LFReference fields, what the level's designer authored.
        public static bool CustomZonesRefused = true;

        public bool HasAmbient => haveAmbient;
        public string AmbientName => ambientName;
        public int ZoneCount => zones.Count;
        public string CameraName => cameraName;
        public Props Current { get; private set; } = Props.Off;
        public float LastTotalWeight { get; private set; }

        /// The camera reverb: the FEV definition of the level's Default project (null: the game's getReverbPreset fails and no camera reverb is made).
        public void SetCameraDef(ReflexFmod.ReverbDef d) { camera = d != null ? FromDef(d) : (Props?)null; cameraName = d?.name; }

        static float NumOf(SceneEntity e, string field, float d = 0f)
        {
            var n = e.Field(field); object v = n != null && n.children.Count > 0 ? n.children[0].value : n?.value;
            return v is float f ? f : v is int i ? i : v is uint u ? u : d;
        }
        static int EnvironmentOf(SceneEntity e)
        {
            var n = e.Field("ReverbEnvironment")?.Child("EnumValue"); object v = n?.value;
            return v is int i ? i : v is uint u ? (int)u : v is float f ? (int)f : 0;
        }
        /// The entity's FMOD_REVERB_PROPERTIES (+0x98): the preset of its ReverbEnvironment, or its explicit fields; refused custom zones stay OFF.
        static Props PropsOf(SceneEntity e, out int k)
        {
            k = EnvironmentOf(e);
            if (k >= 1 && k <= 25) return PresetOf(k);
            if (k == 0 && !CustomZonesRefused)
                return new Props
                {
                    room = NumOf(e, "Room"), roomHF = NumOf(e, "RoomHF"), roomLF = NumOf(e, "RoomLF"), decay = NumOf(e, "DecayTime", 1f), decayHF = NumOf(e, "DecayHFRatio", 1f),
                    refl = NumOf(e, "Reflections"), reflDelay = NumOf(e, "ReflectDelay"), rev = NumOf(e, "Reverb"), revDelay = NumOf(e, "ReverbDelay"),
                    diffusion = NumOf(e, "Diffusion"), density = NumOf(e, "Density"), hfRef = NumOf(e, "HFReference", 5000f), lfRef = NumOf(e, "LFReference", 250f)
                };
            return Props.Off;
        }

        /// Reads the level's reverb entities once its content is there (retried every 60 calls). `mapScale`: MAP SCALE moves and scales the zones.
        public void ReadLevel()
        {
            if (levelRead || levelTries++ % 60 != 0) return;
            var lvl = UnityEngine.Object.FindAnyObjectByType<OriginalLevel>();
            var content = lvl ? lvl.Content : null;
            if (content == null || content.SceneEntities == null) return;
            float ms = lvl.MapScale;
            foreach (var e in content.SceneEntities)
            {
                try
                {
                    string t = e.Type;
                    if (t == "LevelAmbientAudioZoneRSData")
                    {
                        // the manager keeps two ambient entities (the front end's and the level's); the level's scene is the last one read
                        ambient = PropsOf(e, out int k); ambientOff = ambient.IsOff; haveAmbient = true; ambientName = PresetName(k);
                    }
                    else if (t == "AudioZoneSphereRSData" || t == "AudioLineZoneSphereRSData")
                    {
                        var z = new Zone { p = PropsOf(e, out int k), line = t == "AudioLineZoneSphereRSData", min = NumOf(e, "MinDistance"), max = NumOf(e, "MaxDistance") };
                        z.pts = z.line ? OriginalLevelSound.Points(e) : new[] { e.Matrix != null ? new Vector3(e.Matrix[12], e.Matrix[13], e.Matrix[14]) : Vector3.zero };
                        z.label = (z.line ? "line " : "sphere ") + PresetName(k) + " " + z.min.ToString("0") + ".." + z.max.ToString("0");
                        if (ms != 1f) { for (int i = 0; i < z.pts.Length; i++) z.pts[i] *= ms; z.min *= ms; z.max *= ms; }
                        zones.Add(z);
                    }
                }
                catch (Exception x) { Debug.LogWarning("Reverb: " + e + " not read: " + x.Message); }
            }
            levelRead = true;
            Debug.Log("[reverb] ambient " + (haveAmbient ? ambientName : "none") + ", zones " + zones.Count + (zones.Count > 0 ? " (" + string.Join(", ", zones.ConvertAll(z => z.label)) + ")" : "")
                      + (camera.HasValue ? ", first-person " + cameraName : ""));
        }

        // ------------------------------------------------------------------------------------------------ the Unity side
        AudioReverbZone zone; readonly Transform parent;
        public OriginalReverb(Transform parent) { this.parent = parent; }

        /// A frame: the listener (game coordinates) and whether the first-person camera is the player's view. Drives the one AudioReverbZone.
        public void Update(Vector3 listener, bool firstPerson)
        {
            lastListener = listener;
            if (!Enabled) { Apply(Props.Off); return; }
            ReadLevel();
            if (!levelRead || (!haveAmbient && zones.Count == 0 && !camera.HasValue)) { Apply(Props.Off); return; }
            active.Clear();
            foreach (var z in zones)
            {
                Vector3 at = z.line ? OriginalLevelSound.Nearest(z.pts, listener) : z.pts[0];
                float w = Weight((at - listener).magnitude, z.min, z.max);
                if (w >= WeightFloor) active.Add((z.p, w));
            }
            if (firstPerson && camera.HasValue && FirstPersonReverb) active.Add((camera.Value, 1f));   // 50 .. 51 ft around the camera: the listener sits at its centre
            float sum = 0f; foreach (var a in active) sum += a.Item2;
            LastTotalWeight = sum;
            Apply(Morph(active, ambient, ambientOff));   // (no ambient entity: FMOD's default ambient, OFF)
        }

        bool applied; Props last;
        void Apply(Props p)
        {
            Current = p;
            if (p.IsOff && !zone) return;
            if (!zone)
            {
                var go = new GameObject("reverb (the system reverb of the original)"); go.transform.SetParent(parent, false);
                zone = go.AddComponent<AudioReverbZone>();
                zone.minDistance = 100000f; zone.maxDistance = 100001f;   // the zone is the whole map: the morph of the zones is done above
                zone.reverbPreset = AudioReverbPreset.User;
            }
            zone.transform.position = lastListener;
            if (p.IsOff) { if (zone.enabled) zone.enabled = false; applied = false; return; }
            if (!zone.enabled) zone.enabled = true;
            if (applied && Near(p, last)) return;
            last = p; applied = true;
            zone.reverbPreset = AudioReverbPreset.User;
            zone.room = Mathf.RoundToInt(p.room); zone.roomHF = Mathf.RoundToInt(p.roomHF); zone.roomLF = Mathf.RoundToInt(p.roomLF);
            zone.decayTime = p.decay; zone.decayHFRatio = p.decayHF;
            zone.reflections = Mathf.RoundToInt(p.refl); zone.reflectionsDelay = p.reflDelay;
            zone.reverb = Mathf.RoundToInt(p.rev); zone.reverbDelay = p.revDelay;
            zone.HFReference = p.hfRef; zone.LFReference = p.lfRef; zone.diffusion = p.diffusion; zone.density = p.density;
        }
        public static bool Near(Props a, Props b) =>
            Mathf.Abs(a.room - b.room) < 1f && Mathf.Abs(a.roomHF - b.roomHF) < 1f && Mathf.Abs(a.roomLF - b.roomLF) < 1f && Mathf.Abs(a.refl - b.refl) < 1f && Mathf.Abs(a.rev - b.rev) < 1f &&
            Mathf.Abs(a.decay - b.decay) < 0.005f && Mathf.Abs(a.decayHF - b.decayHF) < 0.005f && Mathf.Abs(a.reflDelay - b.reflDelay) < 0.0005f && Mathf.Abs(a.revDelay - b.revDelay) < 0.0005f &&
            Mathf.Abs(a.diffusion - b.diffusion) < 0.5f && Mathf.Abs(a.density - b.density) < 0.5f && Mathf.Abs(a.hfRef - b.hfRef) < 5f && Mathf.Abs(a.lfRef - b.lfRef) < 1f;
        Vector3 lastListener;

        /// A voice's reverb send: the game's 3D events (and anything standing in for them) go to the system reverb, the 2D ones stay dry.
        public static void Route(AudioSource s, bool is3d)
        {
            if (!s) return;
            bool send = Enabled && (is3d || ReverbFor2D);
            s.bypassReverbZones = !send; s.reverbZoneMix = 1f;
        }

        public void Dispose() { if (zone) UnityEngine.Object.Destroy(zone.gameObject); zone = null; }
    }
}
