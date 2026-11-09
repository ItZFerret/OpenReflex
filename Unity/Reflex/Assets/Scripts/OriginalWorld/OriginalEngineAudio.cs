using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.RegularExpressions;
using Reflex.Port.Content;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // The original's engine sound: the vehicle sound layer's EngineSoundEmitter (one per vehicle, VehicleAudioManager 0x00d92f00 + 0x520 +
    // slot * 0x1b10), ported from the decompile (_re/notes/engine-audio.md; core/plan/QUEUE.md "w-audio"). It is the audio side of the core
    // boundary: the core hands over the simulation values the manager's snapshot holds (rx_session_vehicle_audio, RxVehicleAudioState) and
    // this file runs what the original runs on them every frame -- VehicleEngineAudio_Update (00685610) and the state machine of 26 states
    // (table 0x00c0a138 + state * 0x4b4: tick / enter / exit) -- and issues the same FMOD event commands to OriginalAudio (play, stop,
    // setParameter, set3DAttributes, set3DOcclusion) on one event wrapper per vehicle. OriginalAudio's EVENT_FINISHED comes back as the
    // wrapper's state 7, which the ticks wait for (the sustain band re-selection, the rev counts, the idle restart).
    //
    // Data, all from the install: the state -> event table per engine size (VehicleEngineAudio/EngineState AudioEngineStates: sound="<fev
    // project>" + "<group>/<event>" per state, notused), the emitter's tunables (MXRegistry.xml <Audio><EngineSoundEmitter>, read the way the
    // loader 00684c10 reads them, defaults = the shipped values), the debug ConVar EngineAudioUseAccelStates (0x00c0a0f4, default 0).
    //
    // Differences from the original (each is outside what the game state depends on):
    //  - the start line (StartLine / Rev: the time to the next rev, the revs in a row, a car's rev target) draws the GAME's random
    //    generator (g_GameRandSeed) inside the game frame, so the core runs that part at the original's points (core/game/engine_startline.c,
    //    w-rng) and this file follows it: rx_vehicle_audio_rider start_line_state / serial / target (RxVehicleAudioRider.StartLine) enter
    //    StartLine / Rev or play the rev event again, a bike's rev event end goes back to the core (ReportFinished ->
    //    rx_session_vehicle_audio_finished). Without that export (an older core) the emitter decides itself with its own generator.
    //  - the emitter's rank by listener distance (VehicleEngineAudio_CompareDistance -> +0x334, gating 00685a00's restart) is replaced by
    //    OriginalAudio's per-event max playbacks (the loudest instances stay audible).
    //  - AIFX: the original posts (float)emitter+0 for every rider but the local human (0 for the local player). The emitter ctor
    //    (006848a0) writes -1 there, but the manager's set-up (006809f0) then numbers its 12 emitters 0 .. 11, so +0 is the manager slot
    //    the vehicle got at registration (00680b10: the first free one). An oracle run with the game's FMOD command stream (NAT_Concord,
    //    11 AI: AIFX 0 .. 11 on the 12 engine wrappers, slot order) confirms it (core/plan/QUEUE.md "w-sfx"). The core keeps the slot
    //    table (rx_session_vehicle_audio audio_slot); every engine event's AIFX envelopes then do what they were made for: the computer
    //    riders' engines at volume x 0.596, low-passed (~8.7 kHz) and each detuned a little (pitch 1.0 at AIFX 1 down to x 0.917 at 11).
    //
    //  - the AI field trim (w-aiengine, an addition: the original has no such control). The original mixes every engine at its event's own
    //    level (AIFX 1 .. 11 gives the computer riders volume x 0.596) with FMOD's linear 3D rolloff 1 .. 300 ft, which hardly attenuates a
    //    start line of 12 bikes 10 .. 60 ft apart, and FMOD Ex's mixer has no limiter: the summed field is ~8 dB over the local engine's
    //    level and clips on the launch (core/plan/QUEUE.md "w-aiengine", measured). FieldBudget caps the power of the computer riders'
    //    engines at that of FieldBudget single engines at full rolloff: trim = min(1, sqrt(FieldBudget / sum of rolloff^2)) over the
    //    computer riders (floor FieldTrimFloor, smoothed over FieldTrimSeconds), posted as each computer rider's event volume. One or two
    //    bikes nearby keep the original level; the full start line is ~8 dB down. FieldBudget 0 turns it off (the original's mix).
    public sealed class OriginalEngineAudio
    {
        // ------------------------------------------------------------------------------------------------ the AI field trim
        /// Tuning constant (ours, not the original's): the computer riders' engines together never exceed the power of this many engines
        /// heard at full rolloff. 1.0 keeps the local rider's engine level with or above the whole field of 11 at the launch and the summed start
        /// line (stereo) below full scale (tests/engine_audio/offline/field_run.py); 0 = off.
        public static float FieldBudget = 1.0f;
        /// The lowest trim (a very crowded field, e.g. a pile-up at the line).
        public static float FieldTrimFloor = 0.25f;
        /// The trim follows the field with this time constant (riders passing, the start line dispersing): no steps.
        public const float FieldTrimSeconds = 0.15f;
        /// The engine events' 3D range (all 1 .. 300 game feet), for the rolloff weights.
        public const float FieldRolloffMin = 1f, FieldRolloffMax = 300f;
        /// The current trim (1 = the original level), also applied to the computer riders' engine packs (OriginalVehicleSound.Tick).
        public float FieldTrim => fieldTrim;
        float fieldTrim = 1f; bool fieldTrimInit;

        void UpdateFieldTrim(RxVehicleAudioState state, float dt)
        {
            float raw = 1f;
            if (FieldBudget > 0f)
            {
                var lis = audio.Listener; float sum = 0f;
                for (int i = 0; i < state.riderCount; i++)
                {
                    var r = state.riders[i];
                    if ((r.flags & RxVehicleAudioRider.Valid) == 0 || (r.flags & RxVehicleAudioRider.LocalHuman) != 0) continue;
                    float dx = r.px - lis.x, dy = r.py - lis.y, dz = r.pz - lis.z, d = Mathf.Sqrt(dx * dx + dy * dy + dz * dz);
                    float ro = d <= FieldRolloffMin ? 1f : d >= FieldRolloffMax ? 0f : 1f - (d - FieldRolloffMin) / (FieldRolloffMax - FieldRolloffMin);
                    sum += ro * ro;
                }
                if (sum > 1e-4f) raw = Mathf.Clamp(Mathf.Sqrt(FieldBudget / sum), FieldTrimFloor, 1f);
            }
            if (!fieldTrimInit) { fieldTrim = raw; fieldTrimInit = true; }
            else fieldTrim += (raw - fieldTrim) * (1f - Mathf.Exp(-dt / FieldTrimSeconds));
        }
        // the computer riders' events carry the trim as their event volume (Host sub-command 2, the same one the game's SetVolume uses)
        void PostFieldTrim(Emitter e)
        {
            if (e.load == 0 || LocalHuman(e) || Mathf.Abs(e.postedTrim - fieldTrim) < 0.004f) return;
            e.postedTrim = fieldTrim;
            audio.Host(2, e.wrapperId, e.serial, e.path, null, fieldTrim, Vector3.zero, false);
        }

        // ------------------------------------------------------------------------------------------------ the emitter's tunables
        public sealed class Config
        {
            // MXRegistry.xml <Audio><EngineSoundEmitter> (the convars' globals in brackets) and the shipped defaults
            public float fullThrottleThreshold = 0.1f;     // [0x9ca80c]
            public float accelMph = 20f;                   // [0x9ca838]
            public float landingMph = 20f;                 // [0x9ca864]
            public float halfThrottle = 0.5f;              // [0x9ca890]
            public float throttleEngaged = 0f;             // [0x9ca8bc] (block 0x9ca898 + 0x24)
            public float aiThrottleEngaged = 0.1f;         // [0x9ca8e8] (block 0x9ca8c4 + 0x24)
            public float airborneTime = 0.03f;             // [0x9ca914]
            public float panicTime = 0f;                   // [0x9ca940]
            public bool autoDecelAirborne = false;         // [0xc022fc]
            public bool useHalfAccelSustain = true;        // [0xc022d0]
            public bool useShiftAuto = false;              // [0xc09ff4]
            public bool useShiftManual = true;             // [0xc01fd4]
            public float startRamp = 0.5f, startHold = 0.1f, startDown = 0.5f;   // [0x9ca998] [0x9ca9c4] [0x9ca9f0]
            public float startMinPerc = 0.65f, startMaxPerc = 0.9f;            // [0x9caa1c] [0x9caa48]
            public int startMaxRevs = 2, startMaxSeqRevs = 2;                  // [0xc01f7c] [0xc0a080]
            public float startMinTime = 2f, startMaxTime = 10f;                // [0x9caa74] [0x9caaa0]
            public float rpmSmooth = 0.9f;                 // [0x9caacc]
            public float inAirLoad = 1f, clutchLoad = 0.7f;   // [0x9caaf8] [0x9cab24] (-1 disables)
            public float whoopsTime = 0.03f, whoopsMaxTime = 0.03f;   // [0x9cab50] [0x9cab7c]
            public float underwaterOcclusion = 0.6f;       // [0x9ca96c]
            public readonly float[] bandThreshold = { 0f, 0.45f, 0.78f, 0.90f };   // PowerBands 0xc01f38: Power / High / Med / Low
            public bool useAccelStates = false;            // ConVar EngineAudioUseAccelStates (0xc0a0f4 + 0x24 = 0xc0a118), default 0

            public static Config Load()
            {
                var c = new Config();
                string xml;
                try { xml = File.ReadAllText(ReflexInstall.PathOf("MXRegistry.xml")); } catch (Exception) { return c; }
                xml = Regex.Replace(xml, "<!--.*?-->", "", RegexOptions.Singleline);
                var m = Regex.Match(xml, "<EngineSoundEmitter>(.*?)</EngineSoundEmitter>", RegexOptions.Singleline);
                if (!m.Success || m.Groups[1].Value.Contains("<bxml>")) { var all = Regex.Matches(xml, "<EngineSoundEmitter>(.*?)</EngineSoundEmitter>", RegexOptions.Singleline); m = null; foreach (Match x in all) if (!x.Groups[1].Value.Contains("<bxml>")) m = x; if (m == null) return c; }
                string body = m.Groups[1].Value;
                string Tag(string t) { var x = Regex.Match(body, "<" + t + ">(.*?)</" + t + ">", RegexOptions.Singleline); return x.Success ? x.Groups[1].Value.Trim() : null; }
                bool F(string t, out float v) { v = 0; var s = Tag(t); return s != null && float.TryParse(s.TrimEnd('f', 'F'), NumberStyles.Float, CultureInfo.InvariantCulture, out v); }
                bool B(string t, out bool v) { v = false; var s = Tag(t); if (s == null) return false; v = s.StartsWith("T", StringComparison.OrdinalIgnoreCase) || s == "1"; return true; }
                bool I(string t, out int v) { v = 0; var s = Tag(t); return s != null && int.TryParse(s, NumberStyles.Integer, CultureInfo.InvariantCulture, out v); }
                // the loader's acceptance rules per value (00684c10)
                if (F("UnderwaterOcclusionFactor", out var f)) c.underwaterOcclusion = f;
                if (F("FullThrottleThreshold", out f) && f > 0) c.fullThrottleThreshold = f;
                if (F("AccelMPHThreshold", out f) && f > 0) c.accelMph = f;
                if (F("LandingSpeedMPHThresh", out f) && f > 0) c.landingMph = f;
                if (F("HalfThrottleThreshold", out f) && f >= 0) c.halfThrottle = f;
                if (F("ThrottleEngagedThreshold", out f)) c.throttleEngaged = f;
                if (F("AIThrottleEngagedThreshold", out f)) c.aiThrottleEngaged = f;
                if (F("AirborneTimeThreshold", out f)) c.airborneTime = f;
                if (F("PanicTimeThresh", out f)) c.panicTime = f;
                if (B("AutoDecelWhenGoingAirborne", out var b)) c.autoDecelAirborne = b;
                if (B("UseHalfAccelAndHalfSustain", out b)) c.useHalfAccelSustain = b;
                if (B("UseShiftStateWithAutoTrans", out b)) c.useShiftAuto = b;
                if (B("UseShiftStateWithManTrans", out b)) c.useShiftManual = b;
                if (F("StartLineRPMRampTime", out f)) c.startRamp = f;
                if (F("StartLineRPMDownTime", out f)) c.startDown = f;
                if (F("StartLineRPMHoldTime", out f)) c.startHold = f;
                if (F("StartLineRPMMinPerc", out f)) c.startMinPerc = Mathf.Clamp01(f);
                if (F("StartLineRPMMaxPerc", out f)) c.startMaxPerc = Mathf.Clamp01(f);
                if (I("StartLineMaxRevs", out var i)) c.startMaxRevs = i;
                if (I("StartLineMaxSeqRevs", out i)) c.startMaxSeqRevs = i < 1 ? 1 : i;
                if (F("StartLineMinTime", out f)) c.startMinTime = f;
                if (F("StartLineMaxTime", out f)) c.startMaxTime = f;
                if (F("RPMSmoothFactor", out f)) c.rpmSmooth = Mathf.Clamp01(f);
                if (F("InAirLoadOverride", out f)) c.inAirLoad = f == -1f ? -1f : Mathf.Clamp01(f);
                if (F("ClutchLoadOverride", out f)) c.clutchLoad = f == -1f ? -1f : Mathf.Clamp01(f);
                if (F("WhoopsTimeThresh", out f) && f >= 0) c.whoopsTime = f;
                if (F("WhoopsStateMaxTime", out f) && f > 0) c.whoopsMaxTime = f;
                if (c.startMaxTime < c.startMinTime) c.startMaxTime = c.startMinTime;
                var pb = Regex.Match(body, "<PowerBands>(.*?)</PowerBands>", RegexOptions.Singleline);
                if (pb.Success)
                {
                    float P(string t, float d) { var x = Regex.Match(pb.Groups[1].Value, "<" + t + ">(.*?)</" + t + ">", RegexOptions.Singleline); return x.Success && float.TryParse(x.Groups[1].Value.Trim().TrimEnd('f', 'F'), NumberStyles.Float, CultureInfo.InvariantCulture, out var v) ? v : d; }
                    c.bandThreshold[3] = P("LowRangeThresholdPercent", c.bandThreshold[3]);
                    c.bandThreshold[2] = P("MedRangeThresholdPercent", c.bandThreshold[2]);
                    c.bandThreshold[1] = P("HighRangeThresholdPercent", c.bandThreshold[1]);
                }
                return c;
            }
        }

        // ------------------------------------------------------------------------------------------------ the state table
        public const int States = 26, Sizes = 11;
        public static readonly string[] StateNames = { "Off", "Drive", "Rev", "Idle", "StartLine", "AccelLight", "AccelPwr", "AccelHalf", "Choke",
            "DecelHigh", "DecelLow", "Downshift", "Flooding", "SustainHalf", "PanicStart", "PanicLoop", "Shift", "SustainPower", "SustainHigh",
            "SustainMed", "SustainLow", "Wreck", "EnterAir", "Landing", "ReverseStart", "ReverseLoop" };   // 0x009d8520
        public static readonly string[] SizeNames = { "MX50cc", "MX125cc", "MX250cc", "MX250f", "MX450f", "ATV450f", "UTV660cc", "Pro2V8", "ProLiteLS7", "SuperBuggy16V", "Undefined" };   // 0x009d83f8
        const int Off = 0, Drive = 1, Rev = 2, Idle = 3, StartLine = 4, AccelLight = 5, AccelPwr = 6, AccelHalf = 7, Choke = 8, DecelHigh = 9, DecelLow = 10,
            Downshift = 11, SustainHalf = 13, PanicStart = 14, PanicLoop = 15, Shift = 16, SustainPower = 17, SustainHigh = 18, SustainMed = 19, SustainLow = 20,
            Wreck = 21, EnterAir = 22, Landing = 23, ReverseStart = 24, ReverseLoop = 25;
        static readonly int[] BandState = { SustainPower, SustainHigh, SustainMed, SustainLow };   // 0xc01f3c ..: 0x11 .. 0x14

        /// path[size, state] = "<project>/<group>/<event>" ("" = notused / no text: the record's +0x64 used word is 0)
        public sealed class StateTable
        {
            public readonly string[,] path = new string[Sizes, States];
            public readonly float[,] rpmFactor = new float[Sizes, States];   // 0xc0a1b0 (+0x68): the event's rpm range max / the vehicle's max rpm
            public static StateTable Load()
            {
                var t = new StateTable();
                for (int s = 0; s < Sizes; s++) for (int k = 0; k < States; k++) t.path[s, k] = "";
                try
                {
                    var root = Rfx.Bxml(ReflexInstall.Packages.Read("VehicleEngineAudio", "EngineState", "AudioEngineStates", "bxml"));
                    foreach (var n in root.All())
                    {
                        if (n.tag == null || !n.tag.StartsWith("MXVehicleEngineSize_", StringComparison.Ordinal)) continue;
                        int size = Array.IndexOf(SizeNames, n.tag.Substring("MXVehicleEngineSize_".Length));
                        string sound = n.S("sound");
                        if (size < 0 || string.IsNullOrEmpty(sound)) continue;
                        foreach (var c in n.children)
                        {
                            int state = Array.IndexOf(StateNames, c.tag);
                            if (state < 0) continue;
                            bool notused = c.attributes.TryGetValue("notused", out var nu) && Convert.ToString(nu, CultureInfo.InvariantCulture).StartsWith("T", StringComparison.OrdinalIgnoreCase);
                            string text = c.value as string;
                            if (!notused && !string.IsNullOrWhiteSpace(text)) t.path[size, state] = sound + "/" + text.Trim();
                        }
                    }
                }
                catch (Exception e) { Debug.LogWarning("Engine audio: AudioEngineStates not read: " + e.Message); }
                return t;
            }
        }

        /// 0x00a7b1dc: the emitter's first state per engine size (emitter ctor 006848a0): 3 Idle for the bikes / ATV / Undefined, 1 Drive for the cars.
        static int Initial(int size) => size >= 6 && size <= 9 ? Drive : Idle;

        // ------------------------------------------------------------------------------------------------ the snapshot
        /// The manager's telemetry snapshot (slot + 0x20, 0x280 bytes): the fields the emitter reads, by snapshot offset.
        sealed class Snap
        {
            public bool w48, wb8;                            // +0x48 / +0xb8: the first two wheel groups' "not in contact"
            public float speed, rpm, maxRpm, throttle, control2, load;   // +0x220 +0x224 +0x228 +0x22c +0x230 +0x240
            public int gear;                                 // +0x234
            public bool gearUp, gearDown;                    // +0x238 / +0x23c (the manager: previous gear < / > this one)
            public bool clutch, airborne, landed, belowHalf; // +0x244 +0x248 +0x24c (manager: airborne before, not now) +0x250
            public float water; public bool underwater, drowned, wrecked;   // +0x254 +0x258 +0x25c +0x260
            public void CopyFrom(Snap o)
            {
                w48 = o.w48; wb8 = o.wb8; speed = o.speed; rpm = o.rpm; maxRpm = o.maxRpm; throttle = o.throttle; control2 = o.control2; load = o.load;
                gear = o.gear; gearUp = o.gearUp; gearDown = o.gearDown; clutch = o.clutch; airborne = o.airborne; landed = o.landed; belowHalf = o.belowHalf;
                water = o.water; underwater = o.underwater; drowned = o.drowned; wrecked = o.wrecked;
            }
        }

        /// The ValueSmoother_M at emitter + 0x2bc (0067f210 / 0067f1c0 / 00672c30): ramp up, hold, ramp down.
        struct Ramp
        {
            public float value, prev, up, hold, down, time; public int phase;
            public void Start() { if (up == 0f) { value = prev = 1f; phase = 2; time = 0; } else { value = prev = 0f; time = up * 0f; phase = 1; } }
            public void Step(float dt)
            {
                switch (phase)
                {
                    case 0: prev = 0f; return;
                    case 1: { float t = time + dt; prev = value; float f = t / up; time = t; value = f >= 1f ? 1f : f; if (up <= t) { phase = 2; time = 0f; value = 1f; } return; }
                    case 2: { float t = time; prev = value; time = t + dt; if (hold <= t + dt) { phase = 3; time = 0f; } return; }
                    case 3: { float t = time + dt; prev = value; float f = 1f - t / down; time = t; value = f <= 0f ? 0f : f; if (down <= t) { phase = 0; time = 0f; value = 0f; } return; }
                }
            }
        }

        /// One EngineSoundEmitter (0x4e8+ bytes) and its event wrapper (+0x33c, 0x1a4 bytes).
        sealed class Emitter
        {
            public uint vehicle; public int slot; public uint wrapperId;
            public RxVehicleAudioRider rider = new RxVehicleAudioRider();
            public readonly Snap s = new Snap(), prev = new Snap(), entry = new Snap();   // the manager's current / previous snapshot, +0x20 the state entry copy
            public bool hasSnap;
            public int size = 10;                        // +0xc
            public float aifx = -1f;                     // +0x00 (int -1 converted)
            public float airTime, whoopsTimer, fullTimer;// +0x2a0 +0x2a4 +0x2a8
            public int state, prevState;                 // +0x2ac +0x2b0
            public bool latch, startLine;                // +0x2b4 +0x2b8
            public Ramp rev;                             // +0x2bc ..
            public float revTarget, revRpm, startTimer;  // +0x2d8 +0x2dc +0x2e0
            public int revCount, seqRevs;                // +0x2e4 +0x2e8
            public float rpmOut, rpmFilter, rpmClamped, rpmMin, rpmMax, rpmSmooth; public bool rpmInit, rpmActive;   // +0x2ec ValueSmoother: +0x2f0 .. +0x310
            public Vector3 position;                     // +0x320
            public bool underwaterOcc;                   // +0x338
            // the wrapper
            public string path = "";                     // +0x18 (wrapper)
            public int load;                             // +0x138: 0 none, 5 loaded, 6 playing, 7 finished
            public bool playing;                         // +0x13c bit 8
            public uint serial;
            public bool seen;
            public float postedTrim = -1f;               // w-aiengine: the field trim last posted as this emitter's event volume
            public bool coreStartLine; public uint coreSerial; public bool finishReported;   // w-rng: the core's start line (RxVehicleAudioRider.StartLine)
            public bool Finished => load == 7;
            public bool silent;                          // Silenced last frame (a culled rider, RIDER CAP): re-entered from its first state on return
        }

        // ------------------------------------------------------------------------------------------------ the manager
        readonly OriginalAudio audio;
        readonly Config cfg;
        readonly StateTable table;
        readonly Dictionary<uint, Emitter> emitters = new Dictionary<uint, Emitter>();
        readonly List<uint> gone = new List<uint>();
        readonly System.Random random = new System.Random(0x3039);
        uint nextWrapper, lastCue;
        bool cueInit;
        /// Riders the host plays another way (a custom engine pack): their emitters are silent. Set per frame by the caller.
        public Func<RxVehicleAudioRider, bool> Silenced;
        /// A bike's rev one-shot played to its end on an emitter the core's start line drives: the sound slot, for
        /// rx_session_vehicle_audio_finished (set by the owner; w-rng).
        public Action<int> ReportFinished;
        public int EmitterCount => emitters.Count;
        public Config Tunables => cfg;

        public OriginalEngineAudio(OriginalAudio audio)
        {
            this.audio = audio;
            cfg = Config.Load();
            table = StateTable.Load();
            nextWrapper = OriginalAudio.HostWrapperBase + 0x100;
            if (audio) audio.HostFinished += OnFinished;
        }

        public void Dispose()
        {
            if (audio) { audio.HostFinished -= OnFinished; foreach (var e in emitters.Values) audio.Host(12, e.wrapperId, 0, "", null, 0f, Vector3.zero, false); }
            emitters.Clear();
        }

        void OnFinished(uint wrapper, uint serial)
        {
            foreach (var e in emitters.Values)
                if (e.wrapperId == wrapper && e.serial == serial) { e.load = 7; e.playing = false; return; }   // 0080b7d0 type 4 -> 0080b9a0: state 7
        }

        /// The current state of a rider's emitter (diagnostics), -1 none.
        public int StateOf(int slot) { foreach (var e in emitters.Values) if (e.slot == slot) return e.state; return -1; }
        public string EventOf(int slot) { foreach (var e in emitters.Values) if (e.slot == slot) return e.path; return ""; }
        /// The AIFX a rider's emitter posts (diagnostics): 0 for the local human, else its manager slot; NaN without an emitter.
        public float AifxOf(int slot) { foreach (var e in emitters.Values) if (e.slot == slot) return LocalHuman(e) ? 0f : e.aifx; return float.NaN; }

        /// One game frame (a collected core result): VehicleAudioManager_UpdateVehicles' snapshot step and VehicleEngineAudio_Update per vehicle.
        public void Frame(RxVehicleAudioState state, float dt)
        {
            if (!audio || state == null) return;
            ApplyCues(state);
            UpdateFieldTrim(state, dt);
            foreach (var e in emitters.Values) e.seen = false;
            for (int i = 0; i < state.riderCount; i++)
            {
                var r = state.riders[i];
                if ((r.flags & RxVehicleAudioRider.Valid) == 0) continue;
                if (!emitters.TryGetValue(r.vehicle, out var e)) e = Register(r);
                e.seen = true;
                e.rider.CopyFrom(r);
                if (r.audioSlot >= 0) e.aifx = r.audioSlot;                 // the emitter's +0 (006809f0: the manager slot), w-sfx
                bool silent = Silenced != null && Silenced(r);
                if (silent) { if (e.load != 0) { StopForget(e); } e.coreSerial = r.startLineSerial; e.silent = true; continue; }
                if (e.silent)
                {   // back from silence (RIDER CAP: a culled rider came near again): its sound starts over from the size's first state, as a new
                    // emitter's (Register), so no state waits on an event that was stopped
                    e.silent = false; e.postedTrim = -1f;
                    Snapshot(e, r); e.prev.CopyFrom(e.s); e.s.gearUp = e.s.gearDown = e.s.landed = false;
                    e.state = Off; SetState(e, Initial(e.size));
                }
                Snapshot(e, r);
                FollowCore(e, r);
                Update(e, dt);
                PostFieldTrim(e);
            }
            gone.Clear();
            foreach (var kv in emitters) if (!kv.Value.seen) gone.Add(kv.Key);
            foreach (var k in gone) { StopForget(emitters[k]); emitters.Remove(k); }   // 00680d10: the vehicle left the manager
        }

        // 00680b10 (register) + 00685a40 (bind) + 006855c0 (first bind): the emitter starts in Off, then the size's first state.
        Emitter Register(RxVehicleAudioRider r)
        {
            var e = new Emitter { vehicle = r.vehicle, slot = r.slot, wrapperId = nextWrapper++ };
            e.size = r.engineSize >= 0 && r.engineSize < Sizes ? r.engineSize : 10;
            if (r.audioSlot >= 0) e.aifx = r.audioSlot;
            e.rev.up = cfg.startRamp; e.rev.hold = cfg.startHold; e.rev.down = cfg.startDown;
            emitters[r.vehicle] = e;
            e.rider.CopyFrom(r);
            Snapshot(e, r); e.prev.CopyFrom(e.s); e.s.gearUp = e.s.gearDown = e.s.landed = false;
            e.underwaterOcc = false;
            SetState(e, Initial(e.size));
            return e;
        }

        // VehicleAudioManager_UpdateVehicles (00680ec0): the previous snapshot (006814d0 copy), vf52's fields, the derived words.
        void Snapshot(Emitter e, RxVehicleAudioRider r)
        {
            var s = e.s;
            if (e.hasSnap) e.prev.CopyFrom(s);
            s.w48 = r.wheelAir[0]; s.wb8 = r.wheelAir[1];
            s.speed = r.speed; s.rpm = r.rpm; s.maxRpm = r.maxRpm; s.throttle = r.throttle; s.control2 = r.control2; s.load = r.load; s.gear = r.gear;
            s.clutch = r.clutch; s.airborne = r.airborne; s.belowHalf = r.belowHalf; s.water = r.water;
            s.underwater = r.underwater; s.drowned = r.drowned; s.wrecked = r.wrecked;
            if (e.hasSnap)
            {
                s.gearUp = e.prev.gear < s.gear;               // +0x238 = previous +0x234 < +0x234
                s.gearDown = s.gear < e.prev.gear;             // +0x23c
                s.landed = !s.airborne && e.prev.airborne;     // +0x24c (the water-entry branch gives the same word)
            }
            else s.gearUp = s.gearDown = s.landed = false;
            e.hasSnap = true;
            e.position = new Vector3(r.px, r.py, r.pz);
        }

        void ApplyCues(RxVehicleAudioState st)
        {
            if (!cueInit) { cueInit = true; lastCue = 0; }
            if (st.cueSerial < lastCue) lastCue = 0;    // a new session
            if (st.cueSerial == lastCue) return;
            uint from = lastCue;
            for (uint n = from + 1; n <= st.cueSerial; n++)
            {
                RxVehicleAudioCue cue = null;
                foreach (var c in st.cues) if (c.serial == n) { cue = c; break; }
                if (cue == null) continue;
                switch (cue.kind)
                {
                    case RxVehicleAudioState.CueStartLineEnable:      // 00681880: every emitter with a vehicle
                        foreach (var e in emitters.Values) { e.startLine = true; e.revCount = 0; e.seqRevs = 0; e.startTimer = 0; e.revRpm = 0; if (!e.coreStartLine) SetState(e, StartLine); }
                        break;
                    case RxVehicleAudioState.CueStartLineDisable:     // Scenario_AudioCommand: +0x2b8 = 0
                        foreach (var e in emitters.Values) e.startLine = false;
                        break;
                    case RxVehicleAudioState.CuePreExited:            // 006818e0(1)
                        foreach (var e in emitters.Values) { e.startLine = false; e.underwaterOcc = false; SetState(e, Initial(e.size)); }
                        break;
                }
            }
            lastCue = st.cueSerial;
        }

        // w-rng: the core's start line. A new serial: the core entered StartLine (enter it here: the start-line event), entered Rev (the rev
        // event, a car's target from the core) or replays the rev (the event again); the state machine below then only plays it.
        void FollowCore(Emitter e, RxVehicleAudioRider r)
        {
            e.coreStartLine = (r.flags & RxVehicleAudioRider.StartLine) != 0;
            if (!e.coreStartLine || r.startLineSerial == e.coreSerial) return;
            e.coreSerial = r.startLineSerial;
            if (r.startLineState == StartLine) { e.startLine = true; if (e.state == StartLine) Enter(e, true); else SetState(e, StartLine); }
            else if (r.startLineState == Rev) { e.revTarget = r.startLineTarget; if (e.state == Rev) { e.finishReported = false; Play(e); } else SetState(e, Rev); }
        }

        // ------------------------------------------------------------------------------------------------ the shared predicates
        bool HasPlayer(Emitter e) => (e.rider.flags & RxVehicleAudioRider.HasPlayer) != 0;
        int Kind(Emitter e) => e.rider.vehicleKind;
        /// V && V+0x190 && (V+0x194 is 0 or 1) && player+0x1f0 < 0: a computer rider on a bike / ATV (the AI block 0x9ca8c4)
        bool Ai(Emitter e) => HasPlayer(e) && (Kind(e) == 0 || Kind(e) == 1) && e.rider.pad < 0;
        float T(Emitter e) => Ai(e) ? cfg.aiThrottleEngaged : cfg.throttleEngaged;
        bool Engaged(Emitter e, float x) => x > T(e);
        /// (V == 0 || V+0x194 != 0) ? +0x48 && +0xb8 : +0x48
        bool WheelsUp(Emitter e) => Kind(e) != 0 ? e.s.w48 && e.s.wb8 : e.s.w48;
        bool AirLong(Emitter e) => WheelsUp(e) && cfg.airborneTime <= e.airTime;
        bool AiAirLong(Emitter e) => Ai(e) && AirLong(e);
        bool ShiftCfg(Emitter e) => (e.rider.flags & RxVehicleAudioRider.ManualGearbox) != 0 ? cfg.useShiftManual : cfg.useShiftAuto;   // 0xc01fb0 / 0xc09fd0 + 0x24
        bool GearChanged(Emitter e) => e.s.gearUp || e.s.gearDown;
        bool LocalHuman(Emitter e) => (e.rider.flags & RxVehicleAudioRider.LocalHuman) != 0;
        bool Car(Emitter e) => Initial(e.size) == Drive;

        // ------------------------------------------------------------------------------------------------ the wrapper (FMOD commands)
        void Play(Emitter e)   // AudioEvent_Play (sub-command 0 with the position; wrapper state 6)
        {
            if (string.IsNullOrEmpty(e.path)) return;
            e.serial++; e.load = 6; e.playing = true;
            audio.Host(0, e.wrapperId, e.serial, e.path, null, 0f, e.position, true);
        }
        void StopForget(Emitter e)   // 0080a810: Event::stop(true) and forget (sub-command 12)
        {
            if (e.load != 0) audio.Host(12, e.wrapperId, e.serial, "", null, 0f, Vector3.zero, false);
            e.load = 0; e.playing = false;
        }
        void Param(Emitter e, string name, float value) { if (e.load != 0) audio.Host(4, e.wrapperId, e.serial, e.path, name, value, Vector3.zero, false); }
        void PlayIfIdle(Emitter e) { if (!e.playing) Play(e); }   // 00685a00 (without the distance rank)

        // 00686250: exit the old state, keep the previous id and the entry snapshot (not when leaving Downshift / Shift), enter the new one
        void SetState(Emitter e, int next)
        {
            if (next == e.state) return;
            if (e.state == SustainHalf) e.whoopsTimer = 0f;             // SustainHalf's exit 00688190
            if (e.state != Downshift && e.state != Shift) { e.prevState = e.state; if (e.hasSnap) e.entry.CopyFrom(e.s); }
            e.state = next;
            switch (next)                                                 // the enter functions
            {
                case Off: case 12: break;                                 // no enter: the old event plays on
                case Drive: EnterDrive(e); break;
                case Rev: EnterRev(e); break;
                case StartLine: EnterStartLine(e); break;
                default: Enter(e, true); break;                           // 006881b0 -> 00686370(emitter, 1)
            }
        }

        // 00686370 (emitter, play): the state's event for the engine size, "_AI" for a car that is not the local player, stop the old event,
        // play (or prepare) the new one, AIFX for the bikes.
        void Enter(Emitter e, bool play)
        {
            string p = table.path[e.size, e.state];
            if (!string.IsNullOrEmpty(p))
            {
                if (!LocalHuman(e) && Car(e)) p += "_AI";                // 006891d0 "%s_AI"
                StopForget(e);
                e.path = p;
                if (play) Play(e); else e.load = 5;
                if (Car(e)) return;
                Param(e, "AIFX", LocalHuman(e) ? 0f : e.aifx);
                return;
            }
            StopForget(e); e.path = "";
        }

        // ------------------------------------------------------------------------------------------------ VehicleEngineAudio_Update 00685610
        void Update(Emitter e, float dt)
        {
            var s = e.s;
            audio.Host(5, e.wrapperId, e.serial, e.path, null, 0f, e.position, true);     // 0080ae80: set3DAttributes
            if (e.state == Off && !s.wrecked) { e.underwaterOcc = false; SetState(e, Initial(e.size)); }   // 00685af0
            // +0x2a8: the time from the throttle engaging until it reaches the half-throttle threshold
            if (!Engaged(e, s.throttle) || AiAirLong(e)) e.fullTimer = 0f;
            else if (s.throttle < cfg.halfThrottle || cfg.fullThrottleThreshold <= 0f || e.fullTimer < cfg.fullThrottleThreshold) e.fullTimer += dt;
            if (e.whoopsTimer > 0f) e.whoopsTimer -= dt;
            // +0x2a0: the airborne time
            if (WheelsUp(e)) e.airTime += dt; else e.airTime = 0f;
            // +0x2b4: the auto-decel latch holds while airborne past the threshold with the throttle engaged (or the brake / reverse)
            if (e.latch)
            {
                bool keep = false;
                if (AirLong(e) && !s.clutch)
                {
                    if (!Engaged(e, s.throttle) || (Ai(e) && (Kind(e) == 0 || s.wb8))) keep = Engaged(e, s.control2);
                    else keep = true;
                }
                if (!keep) e.latch = false;
            }
            // underwater: occlusion, choke (the engine drowned) or wreck
            float occ = 0f;
            if (!s.underwater || e.state == Off) { if (e.underwaterOcc) e.underwaterOcc = false; }
            else
            {
                e.underwaterOcc = true;
                if (!s.drowned || e.state == Choke || e.state == Wreck) { if (s.wrecked && e.state != Wreck) WreckSelect(e); }
                else ChokeSelect(e);
                occ = e.underwaterOcc ? cfg.underwaterOcclusion : 0f;
            }
            audio.Host(10, e.wrapperId, e.serial, e.path, null, occ, Vector3.zero, false);   // 0080b680: set3DOcclusion
            Tick(e, dt);
        }

        void Tick(Emitter e, float dt)
        {
            switch (e.state)
            {
                case Drive: TickDrive(e); break;
                case Rev: TickRev(e, dt); break;
                case Idle: TickIdle(e); break;
                case StartLine: TickStartLine(e, dt); break;
                case AccelLight: TickAccelLight(e); break;
                case AccelPwr: TickAccelPwr(e); break;
                case AccelHalf: TickAccelHalf(e); break;
                case Choke: case Wreck: if (e.Finished) SetState(e, Off); break;   // 00687870
                case DecelHigh: TickDecel(e, DecelHigh); break;
                case DecelLow: TickDecel(e, DecelLow); break;
                case Downshift: TickDownshift(e); break;
                case SustainHalf: TickSustainHalf(e); break;
                case PanicStart: case EnterAir: TickPanicStart(e); break;          // 006881a0 is a thunk to 006881c0
                case PanicLoop: TickPanicLoop(e); break;
                case Shift: TickShift(e); break;
                case SustainPower: case SustainHigh: TickSustain(e, DecelHigh); break;   // 006886a0
                case SustainMed: case SustainLow: TickSustain(e, DecelLow); break;       // 00688990
                case Landing: TickLanding(e); break;
                case ReverseStart: TickReverse(e, true); break;
                case ReverseLoop: TickReverse(e, false); break;
            }
        }

        // ------------------------------------------------------------------------------------------------ the selections
        // 00685b50 / 00685b10: a car with no Wreck / Choke event drives on
        void WreckSelect(Emitter e) => SetState(e, Car(e) && table.path[e.size, Wreck].Length == 0 ? Drive : Wreck);
        void ChokeSelect(Emitter e) => SetState(e, Car(e) && table.path[e.size, Choke].Length == 0 ? Drive : Choke);

        // 00684840: the sustain band of rpm / max rpm (PowerBands, from the highest threshold down)
        int BandSelect(Emitter e)
        {
            float frac = e.s.rpm / e.s.maxRpm;
            for (int i = 3; i >= 0; i--) if (cfg.bandThreshold[i] < frac) return BandState[i];
            return SustainLow;
        }

        // 006861e0: the shift state when the transmission type's setting allows it, else the sustain selection
        void ShiftOrSustain(Emitter e) { if (ShiftCfg(e)) SetState(e, Shift); else SustainSelect(e); }

        // 00685b90: accel selection
        void AccelSelect(Emitter e)
        {
            var s = e.s;
            if (!cfg.useAccelStates)
            {
                if (cfg.useHalfAccelSustain && !Ai(e) && s.throttle < cfg.halfThrottle && Engaged(e, s.throttle)) { SetState(e, AccelHalf); return; }
                SustainSelect(e); return;
            }
            if (!Ai(e) && s.throttle < cfg.halfThrottle && Engaged(e, s.throttle)) { SetState(e, AccelHalf); return; }
            SetState(e, s.speed * 0.68182f >= cfg.accelMph ? AccelPwr : AccelLight);
        }

        // 00685dc0: sustain selection (half throttle, the whoops landing, the rpm band, the full-throttle demotion, the landing)
        void SustainSelect(Emitter e)
        {
            var s = e.s;
            if (cfg.useHalfAccelSustain && !Ai(e) && s.throttle < cfg.halfThrottle && Engaged(e, s.throttle)) { SetState(e, SustainHalf); return; }
            bool fromAir = Kind(e) != 0 ? (!s.w48 && !s.wb8 && (e.entry.w48 || e.entry.wb8)) : (!s.w48 && e.entry.w48);
            if (fromAir && cfg.whoopsTime > 0f && s.landed && e.airTime < cfg.whoopsTime && Engaged(e, s.throttle))
            { e.whoopsTimer = cfg.whoopsMaxTime; SetState(e, SustainHalf); return; }
            int b = BandSelect(e);
            if (b == SustainPower && (s.throttle < cfg.halfThrottle || cfg.fullThrottleThreshold <= 0f || e.fullTimer < cfg.fullThrottleThreshold)) b = SustainHigh;
            if (!s.clutch && e.entry.clutch) b = SustainHigh;
            if (s.landed && cfg.landingMph > 0f && s.speed * 0.68182f >= cfg.landingMph && Engaged(e, s.throttle) && !AiAirLong(e)) b = Landing;
            SetState(e, b);
        }

        // the airborne / clutch tail most ticks share: the auto-decel latch, else PanicStart with the clutch, EnterAir without
        void AirTail(Emitter e, int decel)
        {
            if (cfg.autoDecelAirborne && AirLong(e)) { e.latch = true; SetState(e, decel); return; }
            SetState(e, e.s.clutch ? PanicStart : EnterAir);
        }

        // ------------------------------------------------------------------------------------------------ the ticks
        // 00687060 AccelLight / 00687310 AccelPwr / 006875a0 AccelHalf
        void TickAccelLight(Emitter e)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (!Engaged(e, s.throttle) || AiAirLong(e)) { SetState(e, DecelLow); return; }
            if (AirLong(e) || s.clutch) { AirTail(e, DecelLow); return; }
            if (!Ai(e) && s.throttle < cfg.halfThrottle) { ShiftOrSustain(e); return; }
            if (GearChanged(e) && ShiftCfg(e)) { ShiftOrSustain(e); return; }
            if (e.Finished) SustainSelect(e);
        }
        void TickAccelPwr(Emitter e)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (!Engaged(e, s.throttle) || AiAirLong(e)) { SetState(e, DecelHigh); return; }
            if (AirLong(e) || s.clutch) { AirTail(e, DecelHigh); return; }
            if (!Ai(e) && s.throttle < cfg.halfThrottle) { ShiftOrSustain(e); return; }
            if (GearChanged(e)) { ShiftOrSustain(e); return; }
            if (e.Finished) SustainSelect(e);
        }
        void TickAccelHalf(Emitter e)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (!Engaged(e, s.throttle) || AiAirLong(e)) { SetState(e, DecelLow); return; }
            if (AirLong(e) || s.clutch) { AirTail(e, DecelLow); return; }
            if (!Ai(e) && s.throttle < cfg.halfThrottle)
            {
                if (GearChanged(e) && ShiftCfg(e)) { ShiftOrSustain(e); return; }
                if (e.Finished) SustainSelect(e);
                return;
            }
            AccelSelect(e);
        }

        // 00687890 DecelHigh / 00687b20 DecelLow
        void TickDecel(Emitter e, int own)
        {
            var s = e.s;
            if (s.wrecked) WreckSelect(e);
            else if (Engaged(e, s.throttle) && !AiAirLong(e))
            {
                if (!AirLong(e) && !s.clutch)
                {
                    if (!Ai(e) && s.throttle < cfg.halfThrottle) SustainSelect(e);
                    else AccelSelect(e);
                }
                else if (WheelsUp(e) && cfg.airborneTime <= e.airTime && cfg.panicTime > 0f && cfg.panicTime <= e.airTime) SetState(e, PanicStart);
                else if (!cfg.autoDecelAirborne || s.clutch || !e.latch) SetState(e, PanicStart);
            }
            if (e.state == own && e.Finished) SetState(e, Idle);
        }

        // 00687db0 Downshift
        void TickDownshift(Emitter e)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (Engaged(e, s.throttle) && !AiAirLong(e)) { if (e.Finished) SustainSelect(e); return; }
            SetState(e, DecelLow);
        }

        // 00687ea0 SustainHalf (the half-throttle sustain and the whoops state)
        void TickSustainHalf(Emitter e)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (!Engaged(e, s.throttle) || AiAirLong(e)) { SetState(e, DecelLow); return; }
            if (AirLong(e) || s.clutch) { AirTail(e, DecelLow); return; }
            if (!Ai(e) && s.throttle < cfg.halfThrottle && e.whoopsTimer > 0f)
            {
                if (GearChanged(e) && ShiftCfg(e)) { ShiftOrSustain(e); return; }
                if (!e.Finished) return;
                if (!GearChanged(e)) { Play(e); return; }
            }
            SustainSelect(e);
        }

        // 006881c0 PanicStart (EnterAir too)
        void TickPanicStart(Emitter e)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (!Engaged(e, s.throttle) || AiAirLong(e)) { SetState(e, DecelLow); return; }
            if (!AirLong(e) && !s.clutch) { SustainSelect(e); return; }
            if (e.Finished) SetState(e, PanicLoop);
        }

        // 00688300 PanicLoop
        void TickPanicLoop(Emitter e)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (!Engaged(e, s.throttle) || AiAirLong(e)) { SetState(e, DecelHigh); return; }
            if (!AirLong(e) && !s.clutch) { SustainSelect(e); return; }
            if (e.Finished) Play(e);
        }

        // 00688450 Shift: on the one-shot's end, the downshift when the gear went below the entry's, then accel or sustain by the state it came from
        void TickShift(Emitter e)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (!Engaged(e, s.throttle) || AiAirLong(e)) { SetState(e, DecelLow); return; }
            if (!e.Finished) return;
            bool halfCheck;
            switch (e.prevState)
            {
                case AccelLight: case AccelPwr: halfCheck = !Ai(e); break;
                case SustainPower: case SustainHigh: case SustainMed: case SustainLow:
                    if (s.gear < e.entry.gear) SetState(e, Downshift);
                    halfCheck = !Ai(e); break;
                default: halfCheck = false; break;
            }
            if (halfCheck && s.throttle < cfg.halfThrottle && Engaged(e, s.throttle)) { AccelSelect(e); return; }
            SustainSelect(e);
        }

        // 006886a0 SustainPower / High, 00688990 SustainMed / Low: the throttle loops; at each one-shot's end the band again; "load" every frame
        void TickSustain(Emitter e, int decel)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (!Engaged(e, s.throttle) || AiAirLong(e)) { SetState(e, decel); return; }
            if (AirLong(e) || s.clutch) { AirTail(e, decel); return; }
            if (cfg.useHalfAccelSustain && !Ai(e) && s.throttle < cfg.halfThrottle) { ShiftOrSustain(e); return; }
            if (GearChanged(e) && ShiftCfg(e)) { ShiftOrSustain(e); return; }
            if (e.Finished)
            {
                Play(e);
                int b = BandSelect(e);
                if (e.state != b) SetState(e, b);
            }
            Param(e, "load", s.load);
        }

        // 00688c80 Landing
        void TickLanding(Emitter e)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (!Engaged(e, s.throttle) || AiAirLong(e)) { SetState(e, DecelHigh); return; }
            if (AirLong(e) || s.clutch) { AirTail(e, DecelHigh); return; }
            if (GearChanged(e) && ShiftCfg(e)) { ShiftOrSustain(e); return; }
            if (e.Finished) SustainSelect(e);
        }

        // 00688f10 ReverseStart / 00689070 ReverseLoop (control2 = the reverse input)
        void TickReverse(Emitter e, bool start)
        {
            var s = e.s;
            if (s.wrecked) { WreckSelect(e); return; }
            if (!Engaged(e, s.control2)) { SetState(e, DecelLow); return; }
            if (!AirLong(e) && !s.clutch) { if (e.Finished) { if (start) SetState(e, ReverseLoop); else Play(e); } return; }
            AirTail(e, DecelLow);
        }

        // 00686be0 Idle
        void TickIdle(Emitter e)
        {
            var s = e.s;
            if (s.wrecked) WreckSelect(e);
            else if (!Engaged(e, s.throttle) || AiAirLong(e))
            {
                if (Engaged(e, s.control2))
                {
                    if (AirLong(e) || s.clutch) { if (!(cfg.autoDecelAirborne && !s.clutch && e.latch)) SetState(e, PanicStart); }
                    else if (table.path[e.size, ReverseStart].Length == 0) { if (e.state != Idle) SetState(e, Idle); }
                    else SetState(e, ReverseStart);
                }
            }
            else if (!AirLong(e) && !s.clutch) AccelSelect(e);
            else if (!(cfg.autoDecelAirborne && !s.clutch && e.latch)) SetState(e, PanicStart);
            if (e.state == Idle && e.Finished) Play(e);
        }

        // ---- the start line (the race intro's EnableStartLineState) and the cars' rpm
        // 00686e20 StartLine enter
        void EnterStartLine(Emitter e)
        {
            if (Car(e)) { InitRpmSmoother(e); if (e.revRpm <= 0f) e.revRpm = e.s.rpm; }
            if (!e.coreStartLine) e.startTimer = (float)random.NextDouble() * (cfg.startMaxTime - cfg.startMinTime) + cfg.startMinTime;   // else the core's (w-rng)
            Enter(e, true);
        }
        // 00686ec0 StartLine
        void TickStartLine(Emitter e, float dt)
        {
            if (!e.startLine) { e.underwaterOcc = false; SetState(e, Initial(e.size)); return; }
            if (cfg.startMaxRevs < 0 || e.revCount < cfg.startMaxRevs)
            {
                if (e.coreStartLine) return;                             // the core counts down and enters Rev (w-rng)
                e.startTimer -= dt;
                if (e.startTimer <= 0f) SetState(e, Rev);
            }
            else if (Car(e)) { SmoothRpm(e, e.s.rpm); SendRpm(e); Param(e, "load", e.s.load); PlayIfIdle(e); }
        }
        // 00686870 Rev enter
        void EnterRev(Emitter e)
        {
            e.revCount++;
            e.finishReported = false;
            if (!e.coreStartLine) e.seqRevs = cfg.startMaxSeqRevs == 1 ? 1 : 1 + (int)((float)random.NextDouble() * cfg.startMaxSeqRevs);
            if (Car(e))
            {
                if (!e.coreStartLine) { float perc = (cfg.startMaxPerc - cfg.startMinPerc) * (float)random.NextDouble() + cfg.startMinPerc; e.revTarget = e.s.maxRpm * perc; }   // else the core's target (FollowCore)
                e.rev.up = cfg.startRamp; e.rev.hold = cfg.startHold; e.rev.down = cfg.startDown;   // 00672c30: the ramp / hold / down times
            }
            Enter(e, true);
        }
        // 006869a0 Rev
        void TickRev(Emitter e, float dt)
        {
            if (!e.startLine) { e.underwaterOcc = false; SetState(e, Initial(e.size)); return; }
            if (Car(e))
            {
                if (e.rev.phase == 0) e.rev.Start();
                else
                {
                    e.rev.Step(dt);
                    if (e.rev.phase == 0) { if (e.coreStartLine) e.rev.Start(); else { if (--e.seqRevs < 1) { RevDone(e); return; } e.rev.Start(); } }   // the core ends the rev (w-rng)
                }
                float rpm = e.s.rpm; e.revRpm = (e.revTarget - rpm) * e.rev.value + rpm;
                SmoothRpm(e, e.revRpm); SendRpm(e); Param(e, "load", 0.5f); PlayIfIdle(e);
                return;
            }
            if (e.coreStartLine) { if (e.Finished && !e.finishReported) { e.finishReported = true; if (e.rider.audioSlot >= 0) ReportFinished?.Invoke(e.rider.audioSlot); } return; }   // w-rng
            if (e.Finished) { if (--e.seqRevs < 1) { RevDone(e); return; } Play(e); }
        }
        void RevDone(Emitter e) { if (e.startLine) SetState(e, StartLine); else { e.underwaterOcc = false; SetState(e, Initial(e.size)); } }   // 00686220

        // 00686640 Drive enter / 00686690 Drive (the cars: one looping event with rpm and load)
        void EnterDrive(Emitter e) { Enter(e, false); InitRpmSmoother(e); Play(e); }
        void TickDrive(Emitter e)
        {
            var s = e.s;
            float rpm = s.rpm;
            if (Engaged(e, s.control2) && rpm <= 2500f) rpm = 2500f;      // [0x9cd9d8]
            SmoothRpm(e, rpm); SendRpm(e);
            float load = LoadOverride(e);
            if (load == -1f) load = s.load;
            Param(e, "load", load);
            PlayIfIdle(e);
        }
        // 006864f0: the clutch / in-air load overrides while the throttle is engaged
        float LoadOverride(Emitter e)
        {
            var s = e.s;
            if (!Engaged(e, s.throttle) || AiAirLong(e)) return -1f;
            if (s.clutch && cfg.clutchLoad >= 0f && cfg.clutchLoad <= 1f) return cfg.clutchLoad;
            if (s.airborne && cfg.inAirLoad >= 0f && cfg.inAirLoad <= 1f) return cfg.inAirLoad;
            return -1f;
        }
        // 00686310 + 00691010: the rpm smoother (+0x2ec) from the current rpm, range [0, vf48], factor RPMSmoothFactor
        void InitRpmSmoother(Emitter e)
        {
            if (!e.rpmInit)
            {
                float max = e.s.maxRpm;
                if (max > 0f)
                {
                    e.rpmMin = 0f; e.rpmMax = max; e.rpmClamped = e.rpmOut = Mathf.Clamp(e.s.rpm, 0f, max);
                    e.rpmSmooth = cfg.rpmSmooth >= 1f ? 1f : Mathf.Max(0f, cfg.rpmSmooth);
                    e.rpmFilter = 1f - e.rpmClamped / max; e.rpmInit = true;
                }
            }
            if (e.rpmInit) e.rpmActive = true;
        }
        void SmoothRpm(Emitter e, float rpm)
        {
            e.rpmClamped = rpm < e.rpmMin ? e.rpmMin : rpm > e.rpmMax ? e.rpmMax : rpm;
            if (e.rpmInit && e.rpmActive && e.rpmMax > 0f)
            {
                float f = ((1f - e.rpmClamped / e.rpmMax) - e.rpmFilter) * (1f - e.rpmSmooth) + e.rpmFilter;
                e.rpmFilter = f;
                e.rpmOut = (e.rpmMax - e.rpmMin) * (1f - f) + e.rpmMin;
            }
        }
        // 00689220: the state's rpm factor (the event's rpm parameter max / the vehicle's max rpm, cached per size and state), then "rpm"
        void SendRpm(Emitter e)
        {
            float k = table.rpmFactor[e.size, e.state];
            if (k <= 0f && audio.TryEventParameterRange(e.path, "rpm", out _, out float max) && e.s.maxRpm > 0f) table.rpmFactor[e.size, e.state] = k = max / e.s.maxRpm;
            Param(e, "rpm", e.rpmOut * k);
        }
    }
}
