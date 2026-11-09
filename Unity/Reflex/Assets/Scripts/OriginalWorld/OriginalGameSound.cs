using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.RegularExpressions;
using Reflex.Port.Content;
using Reflex.Port.RxCore;
using UnityEngine;
using Node = Reflex.Port.Content.BxmlData.Node;

namespace Reflex.Port.OriginalWorld
{
    // The original's vehicle sound managers besides the engine emitters (core/plan/QUEUE.md "w-sfx"), ported from the decompile onto the
    // core's read-only state (rx_session_sound, RxSoundState; rx_session_vehicle_audio, RxVehicleAudioState). Each part issues the FMOD
    // event commands the original issues to OriginalAudio (host wrappers: play, stop, parameter, 3D position); EVENT_FINISHED comes back
    // through OriginalAudio.HostFinished.
    //  - DriveOn: the drive-on / skid / gravel / landing / water sound set of the local human (manager slot + 0x1b10, 006827f0 -> 006828b0),
    //    data = the level's AudioTerrainZoneManagerRSData (zones by the terrain audio zone under the wheels, objects by DriveOnMaterial).
    //  - Foley: VehicleFoleySoundEmitter (slot + 0xa70, 0068b9e0; every rider): landings, the wind on speed, the rider's grunts / yells;
    //    data = VehicleFoleyAudioParameters (VehicleFoleyAudio/GeneralFoley).
    //  - Collisions: the AudioCollisionManager ([0xd94ad0]; 0067bac0 Add / 0067bd30 Play / 0067c2c0 Update / 0067c350 UpdateRoll, the
    //    splash 0067cbb0 and tyre hit 0067cc80 records of 00680ec0): impacts by the two objects' sound ids (the level's
    //    AudioCollisionManagerRSData MaterialList), Force 0 .. 10, rolling / sliding loops on the terrain zones' skid events.
    // Differences (outside what the game state depends on): the managers run once per collected core result instead of inside the game
    // frame; record lifetimes count game time instead of GetTickCount; a collision event steals the farthest playing one when the pool
    // of 25 is full (AudioCollision_StealLoudest 0067c990 takes the farthest too). The rider's own sounds (grunt, yell, the wreck-avoidance stingers)
    // follow the rider's pelvis (bone 0 of the exported pose, snapshot + 0x200) and, in the first-person view, the camera (0068bfb0).
    public sealed class OriginalGameSound
    {
        // ------------------------------------------------------------------------------------------------ host events
        /// One event wrapper of the host's sound layer (the original's 0x1a4-byte AudioEvent).
        sealed class Ev
        {
            public uint wrapper, serial; public string path = ""; public bool playing; public Vector3 at;
        }
        readonly OriginalAudio audio;
        readonly OriginalWorldRuntime runtime;
        readonly Dictionary<uint, Ev> byWrapper = new Dictionary<uint, Ev>();
        uint nextWrapper = OriginalAudio.HostWrapperBase + 0x100000;
        Ev NewEv() { var e = new Ev { wrapper = nextWrapper++ }; byWrapper[e.wrapper] = e; return e; }
        void OnFinished(uint wrapper, uint serial) { if (byWrapper.TryGetValue(wrapper, out var e) && e.serial == serial) e.playing = false; }
        void Play(Ev e, string path, Vector3 at, bool hasPosition = true)
        {
            if (e == null || string.IsNullOrEmpty(path)) return;
            e.path = path; e.serial++; e.playing = true;
            audio.Host(0, e.wrapper, e.serial, path, null, 0f, at, hasPosition);
        }
        void Stop(Ev e, bool now = false) { if (e == null || !e.playing) return; audio.Host(now ? 12 : 1, e.wrapper, e.serial, e.path, null, 0f, Vector3.zero, false); e.playing = false; }
        void Param(Ev e, string name, float v) { if (e != null && e.playing) audio.Host(4, e.wrapper, e.serial, e.path, name, v, Vector3.zero, false); }
        void Pos(Ev e, Vector3 at) { if (e != null && e.playing) audio.Host(5, e.wrapper, e.serial, e.path, null, 0f, at, true); }
        /// "<SoundAsset>/<path>" unless the path already names "<project>/<group>/<event>" (0078fcf0's rule)
        static string Join(string asset, string path)
        {
            if (string.IsNullOrEmpty(path)) return "";
            int slashes = 0; foreach (char c in path) if (c == '/') slashes++;
            return slashes >= 2 || string.IsNullOrEmpty(asset) ? path : asset + "/" + path;
        }

        // ------------------------------------------------------------------------------------------------ RSData helpers
        static Node Prop(Node n, string name) => n?.Property(name);
        static object Val(Node n) => n == null ? null : n.children.Count > 0 ? n.children[0].value : n.value;
        static string Str(Node n, string name) { var p = Prop(n, name); return p == null ? null : (p.Child("Enum")?.value ?? Val(p)) as string; }
        static float Num(Node n, string name, float d = 0f) { var v = Val(Prop(n, name)); return v is float f ? f : v is int i ? i : v is uint u ? u : v is bool b ? (b ? 1 : 0) : d; }
        static bool Flag(object v) => v is bool b ? b : v is int i ? i != 0 : v is uint u ? u != 0 : v is float f && f != 0f;
        static float[] Range(Node n, string name) => Val(Prop(n, name)) as float[];

        /// A FloatTrigger / BoolTrigger (the original creates the object only when its Enabled flag is set): {present, negate, value}.
        struct Trig
        {
            public bool present, negate; public float value; public float width;
            public static Trig Of(Node t, Node max = null)
            {
                var r = new Trig();
                if (t == null) return r;
                r.present = Flag(t.Child("Enabled")?.value);
                r.negate = Flag(t.Child("Inverse")?.value);
                var v = (t.Child("Float") ?? t.Child("Bool"))?.value;
                r.value = v is float f ? f : Flag(v) ? 1f : 0f;
                float xmax = max != null && Val(max) is float m ? m : r.value;
                r.width = Mathf.Max(xmax, r.value) - r.value;
                return r;
            }
            public static Trig Make(float value, float width) => new Trig { present = true, value = value, width = width };
            public bool Pass(float v) => negate ? v < value : v >= value;          // 0067f7f0
            public bool Match(bool v) => negate ? v != (value != 0f) : v == (value != 0f);   // 0067f910
            /// 0067f830: (v - thr) / |width| clamped to 0 .. 1 (1 - that when negated); 0 without the trigger
            public float Ratio(float v)
            {
                if (!present) return 0f;
                float t = (v - value) / Mathf.Abs(width);
                if (negate) t = 1f - t;
                return float.IsNaN(t) ? 0f : Mathf.Clamp01(t);
            }
        }

        // ================================================================================================ the level data
        /// AudioTerrainZoneManagerRSData (AudioTerrainZoneMappingEntity 0x95d2b4; vf5 00679a80 / 0067a080 / 00679b40)
        sealed class TerrainZones
        {
            public sealed class TireTriggers   // a FoleyTire{k}EventTriggers Item00 (15 words)
            {
                public bool enabled; public Trig side, roll, susp, ang, inAir, justLanded, inWater, hitWater, depth;
            }
            public sealed class Surface
            {
                public string driveOn = "", skid = "", landing = "";
                public readonly string[] foley = { "", "", "" }, foleyTire = { "", "", "" };
                public readonly bool[] useSkid = new bool[3], useLand = new bool[3];
                public readonly TireTriggers[] tire = { new TireTriggers(), new TireTriggers(), new TireTriggers() };
            }
            public sealed class SizeTriggers { public Trig side = Trig.Make(0, 1500), roll = Trig.Make(0, 1500), susp = Trig.Make(200, 500), inAir = Trig.Make(0, 0), justLanded = Trig.Make(1, 0); }
            public readonly Surface[] zones = new Surface[8], objects = new Surface[8];
            public readonly SizeTriggers[] sizes = new SizeTriggers[OriginalEngineAudio.Sizes];
            public string water = "";
            public TerrainZones() { for (int i = 0; i < 8; i++) { zones[i] = new Surface(); objects[i] = new Surface(); } for (int i = 0; i < sizes.Length; i++) sizes[i] = new SizeTriggers(); }

            static void ReadSurface(Surface s, Node z, string asset)
            {
                s.driveOn = Join(asset, Str(z, "DriveOnEventName")); s.skid = Join(asset, Str(z, "SkidEventName")); s.landing = Join(asset, Str(z, "LandingEventName"));
                for (int k = 0; k < 3; k++)
                {
                    s.foley[k] = Join(asset, Str(z, "Foley" + (k + 1) + "EventName"));
                    s.foleyTire[k] = Join(asset, Str(z, "FoleyTire" + (k + 1) + "EventName"));
                    var t = Prop(z, "FoleyTire" + (k + 1) + "EventTriggers");
                    s.useSkid[k] = Flag(Val(Prop(t, "UseSkidTrigger"))); s.useLand[k] = Flag(Val(Prop(t, "UseLandTrigger")));
                    var item = Prop(Prop(t, "FoleyTireEventTriggers"), "Item00");
                    var tt = s.tire[k];
                    if (item == null) continue;
                    tt.side = Trig.Of(Prop(item, "SideSlip"), Prop(item, "SideSlipMax"));
                    tt.roll = Trig.Of(Prop(item, "RollSlip"), Prop(item, "RollSlipMax"));
                    tt.susp = Trig.Of(Prop(item, "SuspensionForce"), Prop(item, "SuspensionForceMax"));
                    tt.ang = Trig.Of(Prop(item, "AngularVelocity"), Prop(item, "AngularVelocityMax"));
                    tt.inAir = Trig.Of(Prop(item, "InAir")); tt.justLanded = Trig.Of(Prop(item, "JustLanded"));
                    tt.inWater = Trig.Of(Prop(item, "TireInWater")); tt.hitWater = Trig.Of(Prop(item, "TireHitWater"));
                    tt.depth = Trig.Of(Prop(item, "WaterDepth"), Prop(item, "WaterDepthMax"));
                    tt.enabled = Flag(Val(Prop(item, "Enabled"))) && (tt.side.present || tt.roll.present || tt.susp.present || tt.ang.present || tt.inAir.present ||
                                 tt.justLanded.present || tt.inWater.present || tt.hitWater.present || tt.depth.present);
                }
            }
            public static TerrainZones Read(SceneEntity e)
            {
                var t = new TerrainZones();
                var root = e.rsdata;
                string asset = e.Asset("SoundAsset") ?? "";
                t.water = Join(asset, Str(root, "WaterEventName"));
                void List(string field, Surface[] into)
                {
                    var list = Prop(root, field); if (list == null) return;
                    foreach (var z in list.children)
                    {
                        int id = (int)Num(z, "ZoneID", -1);
                        if (id >= 0 && id < 8) ReadSurface(into[id], z, asset);
                    }
                }
                List("ZoneList", t.zones); List("MaterialList", t.objects);
                var triggers = Prop(root, "VehicleEventTriggers");
                for (int s = 0; s < t.sizes.Length; s++)
                {
                    var v = Prop(triggers, "MXVehicleEngineSize_" + OriginalEngineAudio.SizeNames[s]); if (v == null) continue;
                    var st = t.sizes[s];
                    var skid = Prop(v, "SkidEventTriggers");
                    var roll = Range(skid, "TireRollSlipRange"); if (roll != null && roll.Length >= 2) st.roll = Trig.Make(roll[0], roll[1] - roll[0]);
                    var side = Range(skid, "TireSideSlipRange"); if (side != null && side.Length >= 2) st.side = Trig.Make(side[0], side[1] - side[0]);
                    var land = Range(Prop(v, "LandingEventTriggers"), "SuspensionForceRange"); if (land != null && land.Length >= 2) st.susp = Trig.Make(land[0], land[1] - land[0]);
                }
                return t;
            }
            /// 00684300: a zone needs its drive-on and skid events, an object material its drive-on event
            public bool Enabled(int s) => s >= 0 && s < 8 ? zones[s].driveOn.Length > 0 && zones[s].skid.Length > 0 : s >= 8 && s < 16 && objects[s - 8].driveOn.Length > 0;
            public Surface Of(int s) => s >= 0 && s < 8 ? zones[s] : s >= 8 && s < 16 ? objects[s - 8] : null;
        }

        /// AudioCollisionManagerRSData (the table AudioCollision_SoundName 0067b890 -> AudioTable_Name 0067d310 reads): id -> "<asset>/<event>"
        sealed class CollisionNames
        {
            readonly Dictionary<int, string> byId = new Dictionary<int, string>();
            public static CollisionNames Read(SceneEntity e)
            {
                var c = new CollisionNames();
                string asset = e.Asset("SoundAsset") ?? "";
                var list = Prop(e.rsdata, "MaterialList");
                if (list != null)
                    foreach (var m in list.children)
                    {
                        int id = (int)Num(m, "MaterialID", -1); string ev = Str(m, "EventName");
                        if (id >= 0 && !string.IsNullOrEmpty(ev)) c.byId[id] = Join(asset, ev);
                    }
                return c;
            }
            /// the id's name, else the default id 0's, else none
            public string Name(int id) => byId.TryGetValue(id, out var n) ? n : byId.TryGetValue(0, out n) ? n : "";
        }

        // ================================================================================================ tunables (MXRegistry.xml <Audio>)
        sealed class Tunables
        {
            public float zoneTime = 0f;                                    // <DriveOnSoundEmitter><TerrainZoneTimeTheshold> [0x9ca7e0] (only when > 0)
            public float timeoutMs = 200f; public int soundPool = 25, dataPool = 75;
            public float forceMin = 0f, forceMax = 35f, grunt = 5f, yell = 5f, ragdollGrunt = 5f;
            public float soft = 0.2f, riderScale = 0.4f, riderSlide = 1f, vehScale = 5f, tireScale = 0.3f, vehVeh = 6f;   // vehVeh: the exe default (0x9ca6ac), not in the file
            public float vehSlideMin = 0f, vehSlideMax = 20f, riderSlideMin = 0f, riderSlideMax = 100f, objSlideMin = 0f, objSlideMax = 40f;
            public static Tunables Load()
            {
                var t = new Tunables();
                string xml;
                try { xml = File.ReadAllText(ReflexInstall.PathOf("MXRegistry.xml")); } catch (Exception) { return t; }
                xml = Regex.Replace(xml, "<!--.*?-->", "", RegexOptions.Singleline);
                string Block(string tag) { var m = Regex.Match(xml, "<" + tag + ">(.*?)</" + tag + ">", RegexOptions.Singleline); return m.Success ? m.Groups[1].Value : ""; }
                bool F(string body, string tag, out float v)
                {
                    v = 0; var m = Regex.Match(body, "<" + tag + ">(.*?)</" + tag + ">", RegexOptions.Singleline);
                    return m.Success && float.TryParse(m.Groups[1].Value.Trim().TrimEnd('f', 'F'), NumberStyles.Float, CultureInfo.InvariantCulture, out v);
                }
                var d = Block("DriveOnSoundEmitter"); if (F(d, "TerrainZoneTimeTheshold", out var f) && f > 0) t.zoneTime = f;
                var c = Block("AudioCollision");
                if (F(c, "CollisionTimeoutMS", out f)) t.timeoutMs = f;
                if (F(c, "CollisionSoundPoolSize", out f)) t.soundPool = Mathf.Max(1, (int)f);
                if (F(c, "CollisionDataPoolSize", out f)) t.dataPool = Mathf.Max(1, (int)f);
                if (F(c, "CollisionForceMin", out f)) t.forceMin = f;
                if (F(c, "CollisionForceMax", out f)) t.forceMax = f;
                if (F(c, "CollisionForceRiderGruntThreshold", out f)) t.grunt = f;
                if (F(c, "CollisionForceRiderYellThreshold", out f)) t.yell = f;
                if (F(c, "CollisionForceRagdollGruntThreshold", out f)) t.ragdollGrunt = f;
                if (F(c, "SoftCollisionScaleFactor", out f)) t.soft = f;
                if (F(c, "AudioRiderCollisionScaleFactor", out f)) t.riderScale = f;
                if (F(c, "AudioRiderSlideScaleFactor", out f)) t.riderSlide = f;
                if (F(c, "AudioVehCollisionScaleFactor", out f)) t.vehScale = f;
                if (F(c, "AudioTireCollisionScaleFactor", out f)) t.tireScale = f;
                if (F(c, "AudioVehVehColScaleFactor", out f)) t.vehVeh = f;
                if (F(c, "VehicleSlideRangeMin", out f)) t.vehSlideMin = f;
                if (F(c, "VehicleSlideRangeMax", out f)) t.vehSlideMax = f;
                if (F(c, "RiderSlideRangeMin", out f)) t.riderSlideMin = f;
                if (F(c, "RiderSlideRangeMax", out f)) t.riderSlideMax = f;
                if (F(c, "ObjectSlideRangeMin", out f)) t.objSlideMin = f;
                if (F(c, "ObjectSlideRangeMax", out f)) t.objSlideMax = f;
                return t;
            }
        }
        const float Mph = 0.68182f;   // [0x97748c] ft/s -> mph

        // ================================================================================================ per rider
        sealed class Rider
        {
            public uint vehicle; public int slot; public bool seen, have;
            public readonly RxVehicleAudioRider va = new RxVehicleAudioRider();
            public readonly RxSoundRider s = new RxSoundRider(), prev = new RxSoundRider();
            public bool prevAirborne; public float prevWater;
            public readonly bool[] justLanded = new bool[4];
            // the foley landing filter (w-landclank): per wheel the seconds in the air, the air time of the last landing, and the touch-down being measured
            public readonly float[] airSeconds = new float[4], lastAir = new float[4];
            public readonly Touchdown[] touch = new Touchdown[5 * 4];      // [event * 4 + wheel]
            public readonly float[] lastClankMs = { -1e9f, -1e9f, -1e9f, -1e9f, -1e9f };
            // the drive-on sound set (local human only)
            public DriveOnSet driveOn;
            // the foley emitter
            public readonly Ev[] vehicleEv = { null, null, null, null, null };       // Landing / LandingHard / LandingSoft ...
            public readonly Ev[] vehicleAlt = { null, null, null, null, null };      // the other wrapper of each: a new clank starts on it while the last one rings out
            public Ev wind;
            public readonly Ev[] riderEv = new Ev[6];                               // + 0x5a0: WreckRecoveryBegin .. WreckAvoidanceLoop (3 grunt, 4 yell)
            public readonly bool[] riderFlag = new bool[6];                         // + 0x758 (006817c0)
            public Vector3 Position => new Vector3(va.px, va.py, va.pz);
            public bool Local => (va.flags & RxVehicleAudioRider.LocalHuman) != 0;
        }
        struct Touchdown { public bool on; public float age, peak; }
        sealed class DriveOnSet
        {
            public int cur = -1; public float timer;
            public readonly Ev[] driveOn = new Ev[16], skid = new Ev[16], landing = new Ev[16];
            public readonly Ev[,] foley = new Ev[16, 3], tire = new Ev[16, 3];
            public Ev water;
        }

        readonly Dictionary<uint, Rider> riders = new Dictionary<uint, Rider>();
        readonly List<uint> gone = new List<uint>();
        readonly Tunables tun = Tunables.Load();
        TerrainZones zones; CollisionNames names; bool levelRead; int levelTries;
        Foley foley;
        float clockMs, frameDt;   // game time (ms) for the collision records' 200 ms keys; the seconds the collected result covers
        public int CollisionEvents { get; private set; }
        /// Whether the local rider plays an engine pack instead of the original's engine (set by OriginalVehicleSound each result): every rider's
        /// clank is trimmed against it, since the player hears it over his own engine.
        public bool LocalHasPack;

        // ---- the bike's landing clank (w-landclank, core/plan/QUEUE.md). BIKE_FOLEY/LANDING (SUS_MXB_LAND_01 .. 13) is a full-scale metallic
        // sample (energy centred at 5 kHz) with one fixed level: the original plays it on the rear wheel's air -> ground edge whenever the
        // wheel's suspension force (a smoothed value that is already high when the wheel only chatters over a bump) is >= SuspensionForce
        // (100 for the bike, 200 ATV). On the open core, measured on NAT_Concord with 12 riders, 85 % of those clanks come from contact flickers
        // of one to five frames (no landing at all) and about three in four real landings stay silent (the force is still rising on the touch-down
        // frame); a clank that finds the last one still playing cuts it (Stop now: a click). With the engine packs 7 .. 12 dB under the
        // original engine in A-weighting it stood out. FilterLandings = false is the original's trigger.
        /// Judge the landing instead of the contact edge: real air time, the force reached in the first frames, a level that follows it.
        public static bool FilterLandings = true;
        /// Seconds the wheel must have been off the ground before its touch-down counts as a landing.
        public const float LandingMinAir = 0.15f;
        /// Seconds after the touch-down in which the suspension force is read (it rises over the first frames): the clank starts then.
        public const float LandingWindow = 0.05f;
        /// The force above the item's own SuspensionForce threshold (the quietest clank, LandingQuietest) that gives the loudest: 100 + 350 = 450 for the bike.
        public const float LandingForceSpan = 350f;
        public const float LandingQuietest = 0.35f;
        /// The whole clank against the rest of the mix (the original's level is 1).
        public const float LandingGain = 0.85f;
        /// ... and against an engine pack. w-landclank's 0.35 made up for the packs playing 7 .. 12 dB under the original engine (A-weighted);
        /// since w-packloud every pack plays at the original engine's A-weighted level, so the clank sits against it as against the original: 1.
        public const float LandingPackTrim = 1f;
        /// Seconds between two clanks of one rider.
        public const float LandingCooldown = 0.30f;
        public int SurfaceOf(int slot) { foreach (var r in riders.Values) if (r.slot == slot && r.driveOn != null) return r.driveOn.cur; return -1; }

        public OriginalGameSound(OriginalAudio audio, OriginalWorldRuntime runtime)
        {
            this.audio = audio; this.runtime = runtime;
            foley = Foley.Load();
            if (audio) audio.HostFinished += OnFinished;
        }

        public void Dispose()
        {
            if (audio)
            {
                audio.HostFinished -= OnFinished;
                foreach (var e in byWrapper.Values) if (e.playing) audio.Host(12, e.wrapper, e.serial, e.path, null, 0f, Vector3.zero, false);
            }
            byWrapper.Clear(); riders.Clear(); records.Clear();
        }

        void ReadLevel()
        {
            if (levelRead || levelTries++ % 60 != 0) return;
            var lvl = UnityEngine.Object.FindAnyObjectByType<OriginalLevel>();
            var content = lvl ? lvl.Content : null;
            if (content == null || content.SceneEntities == null) return;
            foreach (var e in content.SceneEntities)
            {
                try
                {
                    if (zones == null && e.Type == "AudioTerrainZoneManagerRSData") zones = TerrainZones.Read(e);
                    else if (names == null && e.Type == "AudioCollisionManagerRSData") names = CollisionNames.Read(e);
                }
                catch (Exception x) { Debug.LogWarning("Game sound: " + e.Type + " not read: " + x.Message); }
            }
            zones ??= new TerrainZones(); names ??= new CollisionNames();
            levelRead = true;
        }

        /// A collected core result (one or more game frames).
        public void Frame(RxVehicleAudioState va, RxSoundState s, float dt)
        {
            if (!audio || va == null || s == null || !s.valid) return;
            ReadLevel();
            if (!levelRead) return;
            clockMs += dt * 1000f; frameDt = dt;
            frameNow = s.frame;
            foreach (var r in riders.Values) r.seen = false;
            for (int i = 0; i < va.riderCount; i++)
            {
                var v = va.riders[i];
                if ((v.flags & RxVehicleAudioRider.Valid) == 0) continue;
                RxSoundRider sr = null;
                for (int k = 0; k < s.riderCount; k++) if (s.riders[k].vehicle == v.vehicle) { sr = s.riders[k]; break; }
                if (sr == null) continue;
                if (!riders.TryGetValue(v.vehicle, out var r)) riders[v.vehicle] = r = new Rider { vehicle = v.vehicle, slot = v.slot };
                r.seen = true;
                r.va.CopyFrom(v);
                if (r.have) r.prev.CopyFrom(r.s);
                r.s.CopyFrom(sr);
                if (!r.have) { r.prev.CopyFrom(sr); r.prevAirborne = v.airborne; r.prevWater = v.water; }
                for (int w = 0; w < 4; w++) r.justLanded[w] = !r.s.wheels[w].InAir && r.prev.wheels[w].InAir;   // + 0x4c (00680ec0)
                for (int w = 0; w < 4; w++)
                {
                    if (r.s.wheels[w].InAir) r.airSeconds[w] += dt;
                    else { if (r.justLanded[w]) r.lastAir[w] = r.airSeconds[w]; r.airSeconds[w] = 0f; }
                }
                ManagerRecords(r);
                if (r.Local) { r.driveOn ??= new DriveOnSet(); DriveOnTick(r, dt); }
                else if (r.driveOn != null) { StopSet(r.driveOn); r.driveOn = null; }
                FoleyTick(r);
                r.prevAirborne = v.airborne; r.prevWater = v.water; r.have = true;
            }
            Contacts(s);
            Expire();
            // the rider sound slots gameplay code started (006817c0: WreckRecoverySuccess 1, WreckRecoveryFail 2): played by the foley tick next frame
            if (s.riderSoundSerial < lastRiderSound) lastRiderSound = 0;
            for (int k = 0; k < RxSoundState.RiderSounds; k++)
                if (s.riderSoundSerials[k] > lastRiderSound && s.riderSoundSerials[k] <= s.riderSoundSerial) RiderSound(s.riderSoundVehicles[k], s.riderSoundSlots[k]);
            lastRiderSound = Math.Max(lastRiderSound, s.riderSoundSerial);
            gone.Clear();
            foreach (var kv in riders) if (!kv.Value.seen) gone.Add(kv.Key);
            foreach (var k in gone) { var r = riders[k]; if (r.driveOn != null) StopSet(r.driveOn); foreach (var e in r.vehicleEv) Stop(e); foreach (var e in r.vehicleAlt) Stop(e); Stop(r.wind); foreach (var e in r.riderEv) Stop(e); riders.Remove(k); }
        }

        // ================================================================================================ drive-on (006827f0 / 006828b0)
        void DriveOnTick(Rider r, float dt)
        {
            var set = r.driveOn;
            // 00680ec0: + 0x264 / + 0x268 = the zone / contact id of the in-contact wheel nearest the listener; + 0x270 every wheel in water
            int n = r.va.vehicleKind != 0 ? 4 : 2, m264 = -1, c268 = -1, pm264 = -1;
            float best = float.MaxValue, bestPrev = float.MaxValue;
            bool allWater = true;
            var L = audio.Listener;
            for (int g = 0; g < n; g++)
            {
                var w = r.s.wheels[g];
                if ((w.flags & RxSoundWheel.Water) == 0) allWater = false;
                if (!w.InAir) { float d = (new Vector3(w.px, w.py, w.pz) - L).sqrMagnitude; if (d < best) { best = d; m264 = w.zone; c268 = w.contactId; } }
                var p = r.prev.wheels[g];
                if (!p.InAir) { float d = (new Vector3(p.px, p.py, p.pz) - L).sqrMagnitude; if (d < bestPrev) { bestPrev = d; pm264 = p.zone; } }
            }
            int s = c268 != -1 ? c268 + 8 : m264;
            if (c268 == -1 && m264 != -1 && pm264 != m264 && m264 != set.cur) set.timer = tun.zoneTime;
            if (set.timer > 0f) { set.timer -= dt; if (set.timer > 0f) s = set.cur; }
            if (s != set.cur) StopSurface(set, set.cur);
            set.cur = s;
            if (zones.Enabled(s)) UpdateSurface(r, set, s, n, allWater);
        }

        void StopSurface(DriveOnSet set, int s)   // 00684000
        {
            if (s < 0 || s >= 16) return;
            Stop(set.driveOn[s]); Stop(set.skid[s]); Stop(set.landing[s]);
            for (int k = 0; k < 3; k++) { Stop(set.foley[s, k]); Stop(set.tire[s, k]); }
        }
        void StopSet(DriveOnSet set) { for (int s = 0; s < 16; s++) StopSurface(set, s); Stop(set.water); }   // 00683f00

        float TriggerValue(TerrainZones.TireTriggers t, RxSoundWheel w, bool justLanded)   // 0067fc10
        {
            if (!t.enabled) return 0f;
            if (t.inAir.present && !t.inAir.Match(w.InAir)) return 0f;
            if (t.justLanded.present && !t.justLanded.Match(justLanded)) return 0f;
            if (t.inWater.present && !t.inWater.Match((w.flags & RxSoundWheel.Water) != 0)) return 0f;
            if (t.hitWater.present && !t.hitWater.Match((w.flags & RxSoundWheel.WaterEntered) != 0)) return 0f;
            float v = 0f;
            bool Float(Trig tr, float x) { if (!tr.present) return true; if (!tr.Pass(x)) return false; float q = tr.Ratio(x); if (q > v) v = q; return true; }
            if (!Float(t.side, w.sideSlip2) || !Float(t.roll, w.rollSlip2) || !Float(t.susp, w.suspensionForce) || !Float(t.ang, w.angularVelocity) || !Float(t.depth, w.depth)) return 0f;
            return v;
        }

        void UpdateSurface(Rider r, DriveOnSet set, int s, int n, bool allWater)
        {
            var rec = zones.Of(s);
            var size = zones.sizes[Mathf.Clamp(r.va.engineSize, 0, zones.sizes.Length - 1)];
            float skid = 0f, land = 0f, depth = 0f; var ft = new float[3];
            if (rec != null)
                for (int g = n - 1; g >= 0; g--)
                {
                    var w = r.s.wheels[g];
                    if (size.justLanded.Match(r.justLanded[g]) && size.susp.present && w.suspensionForce >= size.susp.value)
                        land = Mathf.Max(land, size.susp.Ratio(w.suspensionForce));
                    if (zones.water.Length > 0 && (w.flags & RxSoundWheel.Water) != 0 && w.depth > depth) depth = w.depth;   // 0067fa80
                    float a = w.sideSlip2, b = w.rollSlip2;
                    float sideR = a >= size.side.value ? size.side.Ratio(a) : 0f, rollR = b >= size.roll.value ? size.roll.Ratio(b) : 0f;
                    if (size.inAir.Match(w.InAir)) { skid = Mathf.Max(skid, sideR); skid = Mathf.Max(skid, rollR); }
                    for (int k = 0; k < 3; k++)
                    {
                        float v = rec.useLand[k] ? 0f : rec.useSkid[k] ? Mathf.Max(rollR, sideR) : TriggerValue(rec.tire[k], w, r.justLanded[g]);
                        if (v > ft[k]) ft[k] = v;
                    }
                }
            float mph = r.va.speed * Mph;
            var at = r.Position;
            // drive-on: every frame while on the surface (its MPH envelope silences it at a stop)
            var d = set.driveOn[s] ??= NewEv();
            if (allWater) Stop(d);
            else { Pos(d, at); if (!d.playing) Play(d, rec.driveOn, at); Param(d, "MPH", mph); }
            var sk = set.skid[s] ??= NewEv();
            if (skid <= 0f || allWater || rec.skid.Length == 0) Stop(sk);
            else { Pos(sk, at); if (!sk.playing) Play(sk, rec.skid, at); Param(sk, "SkidFactor", skid); }
            var ln = set.landing[s] ??= NewEv();
            if (land <= 0f || allWater || rec.landing.Length == 0) Stop(ln);
            else { Pos(ln, at); if (!ln.playing) Play(ln, rec.landing, at); Param(ln, "SkidFactor", land); }
            var wa = set.water ??= NewEv();
            if (depth <= 0f || zones.water.Length == 0) Stop(wa);
            else { Pos(wa, at); if (!wa.playing) Play(wa, zones.water, at); Param(wa, "MPH", mph); Param(wa, "Depth", depth); }
            for (int k = 0; k < 3; k++)
            {
                var f = set.foley[s, k] ??= NewEv();
                if (allWater || rec.foley[k].Length == 0) Stop(f);
                else { Pos(f, at); if (!f.playing) Play(f, rec.foley[k], at); Param(f, "MPH", mph); }
                var t = set.tire[s, k] ??= NewEv();
                if (ft[k] <= 0f || allWater || rec.foleyTire[k].Length == 0) Stop(t);
                else { Pos(t, at); if (!t.playing) Play(t, rec.foleyTire[k], at); Param(t, FirstParam(rec.foleyTire[k]), ft[k]); }
            }
        }
        // the foley tire event's parameter name, looked up once (command 0x15 via 00806d80: its first parameter; every shipped RollSkid event has one, "RollSkid")
        readonly Dictionary<string, string> firstParam = new Dictionary<string, string>();
        string FirstParam(string path)
        {
            if (firstParam.TryGetValue(path, out var p)) return p;
            p = audio.FirstEventParameter(path) ?? "RollSkid";
            firstParam[path] = p;
            return p;
        }

        // ================================================================================================ foley (VehicleFoleySoundEmitter 0068b9e0)
        sealed class Foley
        {
            public sealed class Cond { public bool enabled, inverse; public float value, delta; public bool present; }
            public sealed class Item
            {
                public string name, path = ""; public int tick;   // TickType: 0 Moving, 1 MovingWithFactor, 2 MovingNeedsKeyOff, 3 NonMoving, 4 NonMovingWithFactor, 5 NonMovingNeedsKeyOff
                public readonly Dictionary<string, Cond> conds = new Dictionary<string, Cond>(StringComparer.OrdinalIgnoreCase);
                public bool once;
            }
            public readonly Dictionary<string, List<Item>> byType = new Dictionary<string, List<Item>>(StringComparer.OrdinalIgnoreCase);
            public readonly string[] riderPaths = { "", "", "", "", "", "" };   // AudioRiderFoleyEvent: WreckRecoveryBegin .. WreckAvoidanceLoop
            static readonly string[] RiderNames = { "WreckRecoveryBegin", "WreckRecoverySuccess", "WreckRecoveryFail", "RiderGrunt", "RiderYell", "WreckAvoidanceLoop" };
            static readonly string[] TickNames = { "Moving", "MovingWithFactor", "MovingNeedsKeyOff", "NonMoving", "NonMovingWithFactor", "NonMovingNeedsKeyOff" };

            public static Foley Load()
            {
                var f = new Foley();
                Node root = null;
                try { root = Rfx.Bxml(ReflexInstall.Packages.Read("VehicleFoleyAudio", "GeneralFoley", "VehicleFoleyAudioParameters", "bxml")); }
                catch (Exception e) { Debug.LogWarning("Game sound: VehicleFoleyAudioParameters not read (" + e.Message + "); the shipped values are used"); }
                if (root == null) { f.Defaults(); return f; }
                foreach (var v in root.All())
                {
                    if (v.tag != "Vehicle") continue;
                    string type = Attr(v, "Type") as string ?? "", asset = Attr(v, "SoundAsset") as string ?? "";
                    if (type == "AudioRiderFoleyEvent")
                    {
                        for (int i = 0; i < RiderNames.Length; i++) { var c = v.Child(RiderNames[i]); if (c != null) f.riderPaths[i] = Join(asset, Attr(c, "EventPath") as string ?? ""); }
                        continue;
                    }
                    var list = new List<Item>();
                    foreach (var c in v.children)
                    {
                        var it = new Item { name = c.tag, path = Join(asset, Attr(c, "EventPath") as string ?? "") };
                        it.tick = Math.Max(0, Array.IndexOf(TickNames, Attr(c, "TickType") as string ?? "Moving"));
                        var trig = c.Child("Triggers");
                        if (trig != null)
                        {
                            it.once = Typed(Attr(trig, "TriggerOnlyOnce"));
                            foreach (var t in trig.children)
                            {
                                var cond = new Cond { present = true, enabled = Typed(Attr(t, "Enabled")), inverse = Typed(Attr(t, "Inverse")) };
                                cond.value = Number(t.value); cond.delta = Number(Attr(t, "Delta"));
                                it.conds[t.tag] = cond;
                            }
                        }
                        list.Add(it);
                    }
                    f.byType[type] = list;
                }
                return f;
            }
            // BXML attributes and values are typed (bool / int / float) or strings ("_bool:true", "25.0")
            static object Attr(Node n, string key) => n.attributes.TryGetValue(key, out var v) ? v : null;
            static string Untag(string s) { int i = s.IndexOf(':'); return (i >= 0 && s.StartsWith("_", StringComparison.Ordinal) ? s.Substring(i + 1) : s).Trim(); }
            static bool Typed(object o) => o is bool b ? b : o is int i ? i != 0 : o is uint u ? u != 0 : o is float f ? f != 0f : o is string s && (Untag(s).StartsWith("t", StringComparison.OrdinalIgnoreCase) || Untag(s) == "1");
            static float Number(object o)
            {
                if (o is float f) return f; if (o is int i) return i; if (o is uint u) return u; if (o is bool b) return b ? 1f : 0f;
                if (o is string s) { s = Untag(s); if (float.TryParse(s.TrimEnd('f', 'F'), NumberStyles.Float, CultureInfo.InvariantCulture, out var v)) return v; return s.StartsWith("t", StringComparison.OrdinalIgnoreCase) ? 1f : 0f; }
                return 0f;
            }
            void Defaults()
            {
                Item L(string name, string path, float susp) { var i = new Item { name = name, path = path }; i.conds["SuspensionForce"] = new Cond { present = true, enabled = true, value = susp }; i.conds["InAir"] = new Cond { present = true, enabled = true, value = 0 }; i.conds["JustLanded"] = new Cond { present = true, enabled = true, value = 1 }; return i; }
                var wind = new Item { name = "WindEffect", path = "VEHICLE_FOLEY_01/SHARED_FOLEY/WIND_ON_SPEED", tick = 4 };
                wind.conds["SpeedInMPH"] = new Cond { present = true, enabled = true, value = 25, delta = 25 }; wind.conds["Wrecking"] = new Cond { present = true, enabled = true, value = 0 }; wind.conds["IsLocalPlayer"] = new Cond { present = true, enabled = true, value = 1 };
                byType["AudioCommonFoleyEvent"] = new List<Item> { new Item { name = "Wreck", path = "VEHICLE_FOLEY_01/SHARED_FOLEY/BIKE_ATV_WRECK_2D" }, wind, new Item { name = "BigAir" } };
                byType["MotocrossBike"] = new List<Item> { L("Landing", "VEHICLE_FOLEY_01/BIKE_FOLEY/LANDING", 100) };
                byType["Atv"] = new List<Item> { L("LandingHard", "VEHICLE_FOLEY_01/BIKE_FOLEY/LANDING", 200) };
                byType["TrophyTruck"] = new List<Item> { L("LandingHard", "VEHICLE_FOLEY_01/MACHINE_FOLEY_LARGE/LANDING_HEAVY", 2000), L("LandingSoft", "VEHICLE_FOLEY_01/MACHINE_FOLEY_LARGE/LANDING_LIGHT", 300) };
                byType["Buggy"] = byType["TrophyTruck"];
                byType["Rhino"] = new List<Item> { L("LandingHard", "VEHICLE_FOLEY_01/MACHINE_FOLEY_LARGE/LANDING_HEAVY", 1750), L("LandingSoft", "VEHICLE_FOLEY_01/MACHINE_FOLEY_LARGE/LANDING_LIGHT", 200) };
                riderPaths[1] = "RIDER_AUDIO/FX/WRECK_AVOIDANCE_SUCCESS"; riderPaths[0] = "RIDER_AUDIO/FX/WRECK_AVOIDANCE_WARN";
                riderPaths[3] = "RIDER_AUDIO/VOCAL/RIDER_GRUNT"; riderPaths[4] = "RIDER_AUDIO/VOCAL/RIDER_YELL";
            }
            /// the vehicle type entry of a vehicle class (Type names of the file; the four-wheelers' index >= 2 by class: INFERRED mapping)
            public List<Item> ForClass(int cls)
            {
                string t = cls == RxVehicleAudioRider.ClassMx ? "MotocrossBike" : cls == RxVehicleAudioRider.ClassAtv ? "Atv" : cls == RxVehicleAudioRider.ClassUtv ? "Rhino" :
                           cls == RxVehicleAudioRider.ClassBuggy ? "Buggy" : cls == RxVehicleAudioRider.ClassTruck ? "TrophyTruck" : null;
                return t != null && byType.TryGetValue(t, out var l) ? l : null;
            }
            public List<Item> Common => byType.TryGetValue("AudioCommonFoleyEvent", out var l) ? l : null;
        }

        // ---- where the rider's sounds are (FUN_0068bfb0, the position the foley emitter's rider events follow; w-reverb)
        readonly Dictionary<int, Vector3> pelvisOfSlot = new Dictionary<int, Vector3>();
        /// The player's view is the first-person camera (set by OriginalVehicleSound).
        public Func<bool> FirstPersonView;
        /// A rider's pelvis: the translation of bone 0 (MXRider_Root) of the 69 bone world matrices the core exports (row vectors, elements 12 .. 14).
        public void SetPelvis(int slot, float[] bones, int bone)
        {
            int o = bone * 16;
            var p = new Vector3(bones[o + 12], bones[o + 13], bones[o + 14]);
            if (float.IsNaN(p.x + p.y + p.z) || float.IsInfinity(p.x + p.y + p.z)) return;
            pelvisOfSlot[slot] = p;
        }
        /// 0068bfb0: while the player's view is the first-person camera the local rider's events sit on the camera (the listener); otherwise on the rider's
        /// pelvis node (snapshot + 0x200: FUN_00623bd0 evaluates the node at rider + 0x144, set while the vehicle has a rider), else on the vehicle (+ 0x1c0).
        Vector3 RiderEventPosition(Rider r)
        {
            if (r.Local && FirstPersonView != null && FirstPersonView()) return audio.Listener;
            return pelvisOfSlot.TryGetValue(r.slot, out var p) ? p : r.Position;
        }

        // the vehicle-level condition values (block + 0x78) and the per-wheel ones (block + 0x3c)
        static bool VehicleValue(string name, Rider r, out float v)
        {
            var a = r.va; v = 0f;
            switch (name)
            {
                case "SpeedInMPH": v = a.speed * Mph; return true;
                case "EngineSpeedRPM": v = a.rpm; return true;
                case "Throttle": v = a.throttle; return true;
                case "EngineLoad": v = a.load; return true;
                case "Clutch": v = a.clutch ? 1 : 0; return true;
                case "Airborne": v = a.airborne ? 1 : 0; return true;
                case "ChainSlapping": v = a.belowHalf ? 1 : 0; return true;
                case "Flooding": v = a.underwater ? 1 : 0; return true;
                case "Flooded": v = a.drowned ? 1 : 0; return true;
                case "Wrecking": v = a.wrecked ? 1 : 0; return true;
                case "IsLocalPlayer": v = r.Local ? 1 : 0; return true;   // INFERRED: player + 4 && !player + 0x10 (vtable 0x95d134's site not traced)
                case "VehicleJustLanded": v = !a.airborne && r.prevAirborne ? 1 : 0; return true;
            }
            return false;
        }
        static bool WheelValue(string name, Rider r, int g, out float v)
        {
            var w = r.s.wheels[g]; v = 0f;
            switch (name)
            {
                case "SuspensionForce": v = w.suspensionForce; return true;
                case "AngularVelocity": v = w.angularVelocity; return true;
                case "SideSlip": v = w.sideSlip2; return true;
                case "RollSlip": v = w.rollSlip2; return true;
                case "InAir": v = w.InAir ? 1 : 0; return true;
                case "JustLanded": v = r.justLanded[g] ? 1 : 0; return true;
                case "TireInWater": v = (w.flags & RxSoundWheel.Water) != 0 ? 1 : 0; return true;
                case "TireHitWater": v = (w.flags & RxSoundWheel.WaterEntered) != 0 ? 1 : 0; return true;
                case "WaterDepth": v = w.depth; return true;
            }
            return false;
        }
        static readonly HashSet<string> BoolConds = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { "Clutch", "Airborne", "ChainSlapping", "Flooding", "Flooded", "Wrecking", "IsLocalPlayer", "InAir", "JustLanded", "TireInWater", "TireHitWater", "VehicleJustLanded" };
        /// a condition block: matched when every enabled condition of it passes; the factor = the max over its float conditions (0067f830)
        // the item's conditions against the rider's values (wheelIndex < 0) or one wheel's (wheelIndex; no delegate: FoleyTick runs for every rider and item
        // each frame, and a capturing lambda per call was the race's largest garbage source, w-gcspikes)
        static bool Match(Foley.Item it, Rider r, int wheelIndex, out float factor, bool wheelBlock)
        {
            factor = 0f; int used = 0;
            foreach (var kv in it.conds)
            {
                float v;
                bool known = wheelIndex < 0 ? VehicleValue(kv.Key, r, out v) : WheelValue(kv.Key, r, wheelIndex, out v);
                if (!known) continue;
                bool wheel = kv.Key is "SuspensionForce" or "AngularVelocity" or "SideSlip" or "RollSlip" or "InAir" or "TireInWater" or "TireHitWater" or "WaterDepth" || (kv.Key == "JustLanded");
                if (wheel != wheelBlock) continue;
                var c = kv.Value; used++;
                if (!c.enabled) return false;
                if (BoolConds.Contains(kv.Key)) { bool ok = c.inverse ? (v != 0) != (c.value != 0) : (v != 0) == (c.value != 0); if (!ok) return false; }
                else
                {
                    bool ok = c.inverse ? v < c.value : v >= c.value; if (!ok) return false;
                    if (c.delta != 0f) { float t = (v - c.value) / Mathf.Abs(c.delta); if (c.inverse) t = 1f - t; factor = Mathf.Max(factor, Mathf.Clamp01(t)); }
                }
            }
            return used > 0;
        }

        void FoleyTick(Rider r)
        {
            var at = r.Position;
            // the common events (+0x24: Wreck, WindEffect, BigAir)
            var common = foley.Common;
            if (common != null)
                foreach (var it in common)
                {
                    if (it.name != "WindEffect" || it.path.Length == 0) continue;   // Wreck: BIKE_ATV_WRECK_2D has volume 0 (silent); BigAir: no event
                    bool hit = Match(it, r, -1, out var factor, false);
                    r.wind ??= NewEv();
                    if (hit && !r.wind.playing) Play(r.wind, it.path, at, false);   // NonMoving: started once, never stopped by the emitter
                    if (it.tick == 1 || it.tick == 4) Param(r.wind, "Factor", hit ? factor : 0f);
                }
            // the vehicle events (+0x1c): landings by the wheels' conditions
            var list = foley.ForClass(r.va.vehicleClass);
            if (list != null)
                for (int k = 0; k < list.Count && k < r.vehicleEv.Length; k++)
                {
                    var it = list[k];
                    if (it.path.Length == 0 || it.conds.Count == 0) continue;
                    if (FilterLandings && IsLanding(it)) { FilteredLanding(r, it, k, at); continue; }
                    bool vehicleHit = Match(it, r, -1, out var vf, false);
                    bool anyVehicleCond = false; foreach (var key in it.conds.Keys) if (VehicleValue(key, r, out _)) anyVehicleCond = true;
                    int wheels = r.va.vehicleClass == RxVehicleAudioRider.ClassMx && k == 0 ? 1 : r.s.wheelCount;   // the bike's Landing: the rear wheel only
                    bool wheelHit = false;
                    for (int g = 0; g < wheels && !wheelHit; g++) wheelHit = Match(it, r, g, out _, true);
                    bool hit = anyVehicleCond ? vehicleHit || wheelHit : wheelHit;
                    var e = r.vehicleEv[k] ??= NewEv();
                    if (e.playing) { Pos(e, at); if (hit && r.va.vehicleClass == RxVehicleAudioRider.ClassMx && k == 0) { Stop(e, true); Play(e, it.path, at); } }
                    else if (hit) Play(e, it.path, at);
                }
            // the rider events (+0x5a0): Moving, started by their flags (006817c0)
            for (int k = 0; k < 6; k++)
            {
                if (!r.riderFlag[k]) continue;
                r.riderFlag[k] = false;
                var e = r.riderEv[k] ??= NewEv();
                var rat = RiderEventPosition(r);
                if (e.playing) Pos(e, rat);
                else Play(e, foley.riderPaths[k], rat);
            }
        }
        static bool IsLanding(Foley.Item it) => it.conds.TryGetValue("JustLanded", out var j) && j.enabled && it.conds.TryGetValue("SuspensionForce", out var f) && f.enabled && it.conds.ContainsKey("InAir");

        /// The landing events (w-landclank): a wheel that was in the air for LandingMinAir and touches down is measured for LandingWindow, the
        /// peak suspension force of those frames must reach the item's threshold (as the original's condition, a frame or two later), the
        /// clank's level follows the force, and a rider clanks at most every LandingCooldown. The bike's rear wheel only (as the original),
        /// every wheel for the four-wheelers. A clank still playing is released (it rings out) and the new one starts on the other wrapper.
        void FilteredLanding(Rider r, Foley.Item it, int k, Vector3 at)
        {
            int wheels = r.va.vehicleClass == RxVehicleAudioRider.ClassMx && k == 0 ? 1 : r.s.wheelCount;
            float threshold = it.conds["SuspensionForce"].value, best = -1f;
            for (int g = 0; g < wheels; g++)
            {
                var w = r.s.wheels[g]; var t = r.touch[k * 4 + g];
                if (r.justLanded[g] && r.lastAir[g] >= LandingMinAir && !t.on) t = new Touchdown { on = true, age = 0f, peak = w.suspensionForce };
                else if (t.on)
                {
                    if (w.InAir) t.on = false;                               // off the ground again before the force was read: a bounce, not a landing
                    else { t.peak = Mathf.Max(t.peak, w.suspensionForce); t.age += frameDt; }
                }
                if (t.on && (t.age >= LandingWindow || t.peak >= threshold + LandingForceSpan))
                {
                    t.on = false;
                    if (t.peak >= threshold) best = Mathf.Max(best, t.peak);
                }
                r.touch[k * 4 + g] = t;
            }
            var cur = r.vehicleEv[k];
            if (cur != null && cur.playing) Pos(cur, at);
            if (best < 0f || clockMs - r.lastClankMs[k] < LandingCooldown * 1000f) return;
            float x = Mathf.Clamp01((best - threshold) / LandingForceSpan); x = x * x * (3f - 2f * x);
            float volume = LandingGain * (LandingQuietest + (1f - LandingQuietest) * x) * (LocalHasPack ? LandingPackTrim : 1f);
            if (cur != null && cur.playing) { Stop(cur); r.vehicleEv[k] = r.vehicleAlt[k]; r.vehicleAlt[k] = cur; }   // a graceful stop: the old clank rings out
            var next = r.vehicleEv[k] ??= NewEv();
            r.lastClankMs[k] = clockMs;
            Play(next, it.path, at);
            audio.Host(2, next.wrapper, next.serial, next.path, null, volume, Vector3.zero, false);   // Event::setVolume
        }

        void RiderSound(uint vehicle, int kind)   // 006817c0: the rider's sound slot flagged
        {
            if (vehicle != 0 && riders.TryGetValue(vehicle, out var r) && kind >= 0 && kind < 6) r.riderFlag[kind] = true;
        }

        // ================================================================================================ collisions (AudioCollisionManager)
        sealed class Record
        {
            public uint a, b; public byte soft; public float px, py, pz, sA, sB; public int over, featA, featB;
            public int classA, classB, soundA, soundB, matA, matB; public uint flags, vocalA, vocalB;
            public float slideSpeed; public int slideKind, zone, objectMaterial;
            public float lastMs; public uint startFrame; public int rollSound = -1; public Ev impact, roll;
            public Vector3 Position => new Vector3(px, py, pz);
        }
        readonly Dictionary<(uint, uint, byte), Record> records = new Dictionary<(uint, uint, byte), Record>();
        readonly List<(uint, uint, byte)> expired = new List<(uint, uint, byte)>();
        readonly List<Ev> collisionEvents = new List<Ev>();
        uint lastContact, lastRiderSound;

        void Contacts(RxSoundState s)
        {
            if (s.contactSerial < lastContact) lastContact = 0;   // a new session
            uint from = lastContact;
            if (s.contactSerial - from > RxSoundState.Contacts) from = s.contactSerial - RxSoundState.Contacts;
            for (uint serial = from + 1; serial <= s.contactSerial; serial++)
            {
                var c = s.contacts[serial % RxSoundState.Contacts];
                if (c.serial != serial) continue;
                Add(new Record
                {
                    a = c.a, b = c.b, soft = (byte)((c.flags & RxSoundContact.Soft) != 0 ? 1 : 0), px = c.px, py = c.py, pz = c.pz, sA = c.strengthA, sB = c.strengthB,
                    over = c.soundOverride, featA = c.featureA, featB = c.featureB, classA = c.classA, classB = c.classB, soundA = c.soundA, soundB = c.soundB,
                    matA = c.materialA, matB = c.materialB, flags = c.flags, vocalA = c.vocalA, vocalB = c.vocalB, slideSpeed = c.slideSpeed, slideKind = c.slideKind,
                    zone = c.zone, objectMaterial = c.objectMaterial
                }, c.frame);
            }
            lastContact = s.contactSerial;
        }

        // 00680ec0's own records: the splashes (0067cbb0) and the tyre hits on new contact objects (0067cc80)
        void ManagerRecords(Rider r)
        {
            var v = r.va; float mph = v.speed * Mph;
            Record Base(Vector3 p) => new Record { a = r.vehicle, classA = RxSoundContact.ClassVehicle, soundA = r.s.soundFeature0, matA = r.s.material,
                vocalA = r.vehicle, flags = v.vehicleKind <= 1 ? RxSoundContact.VocalA : 0, px = p.x, py = p.y, pz = p.z, sA = mph, zone = -1 };
            if (v.water > 0f && r.prevWater == 0f && v.airborne)
            {
                var rec = Base(r.Position); rec.soft = 1; rec.flags |= RxSoundContact.Soft; rec.over = 37; rec.soundB = 37; rec.matB = 5;
                Add(rec, frameNow);
            }
            for (int g = 0; g < r.s.wheelCount; g++)
            {
                var w = r.s.wheels[g]; var hub = new Vector3(w.px, w.py, w.pz);
                if ((w.flags & RxSoundWheel.WaterEntered) != 0 && (r.justLanded[g] || w.InAir))
                {
                    var rec = Base(hub); rec.soft = 1; rec.flags |= RxSoundContact.Soft; rec.over = 37; rec.soundB = 37; rec.matB = 5;
                    Add(rec, frameNow);
                }
                if (w.contactId != -1 && w.contactId != r.prev.wheels[g].contactId && w.contact != 0)
                {
                    var n = new Vector3(w.nx, w.ny, w.nz); var vel = new Vector3(r.s.vx, r.s.vy, r.s.vz);
                    float dot = n.sqrMagnitude > 0f && vel.sqrMagnitude > 0f ? Mathf.Abs(Vector3.Dot(n.normalized, vel.normalized)) : 0f;
                    var rec = Base(hub); rec.b = w.contact; rec.classB = w.contactClass; rec.soundB = w.contactSound; rec.matB = w.contactMaterial;
                    rec.sA = tun.tireScale != 0f ? dot * mph * tun.tireScale : dot * mph;
                    Add(rec, frameNow);
                }
            }
        }
        uint frameNow;

        void Add(Record rec, uint frame)   // 0067bb10
        {
            if (rec.sB <= 0f) rec.sB = rec.sA;   // the stored B strength (Add: B = A when B <= 0)
            var key = (rec.a, rec.b, rec.classB == RxSoundContact.ClassForest ? rec.soft : (byte)0);
            if (!records.TryGetValue(key, out var stored))
            {
                if (records.Count >= tun.dataPool) return;   // the record pool is empty
                rec.lastMs = clockMs; rec.startFrame = frame;
                records[key] = rec;
                CollisionPlay(rec);
                return;
            }
            stored.lastMs = clockMs;
            if (frame == stored.startFrame)
            {
                if (rec.sA > stored.sA || rec.sB > stored.sB) { stored.px = rec.px; stored.py = rec.py; stored.pz = rec.pz; }
                stored.sA = Mathf.Max(stored.sA, rec.sA); stored.sB = Mathf.Max(stored.sB, rec.sB);
            }
            else { stored.sA = rec.sA; stored.sB = rec.sB; stored.px = rec.px; stored.py = rec.py; stored.pz = rec.pz; }
            stored.zone = rec.zone; stored.slideSpeed = rec.slideSpeed; stored.slideKind = rec.slideKind;
            CollisionUpdate(stored);
        }

        float Level(float s) { float v = (s - tun.forceMin) / (tun.forceMax - tun.forceMin) * 10f; return v >= 10f ? 10f : !(0f >= v) ? v : 0f; }
        static bool Heavy(int cls) => cls == RxSoundContact.ClassPhysics || cls == RxSoundContact.ClassCollision;

        // the manager's event pool (CollisionSoundPoolSize 25): a free event, else the playing one farthest from the listener is stolen
        // (AudioCollision_StealLoudest 0067c990 takes the farthest); the record that owned it loses it
        Ev CollisionEvent(Vector3 at)
        {
            while (collisionEvents.Count < tun.soundPool) collisionEvents.Add(NewEv());
            Ev pick = null; float fd = -1f; var L = audio.Listener;
            foreach (var e in collisionEvents)
            {
                if (!e.playing) { pick = e; break; }
                float d = (e.at - L).sqrMagnitude; if (d > fd) { fd = d; pick = e; }
            }
            if (pick.playing) Stop(pick, true);
            foreach (var r in records.Values) { if (r.impact == pick) r.impact = null; if (r.roll == pick) { r.roll = null; r.rollSound = -1; } }
            pick.at = at; CollisionEvents++;
            return pick;
        }

        void CollisionPlay(Record rec)   // 0067bd30
        {
            bool bikeA = rec.classA == RxSoundContact.ClassVehicle, bikeB = rec.classB == RxSoundContact.ClassVehicle;
            bool forestB = rec.b != 0 && rec.classB == RxSoundContact.ClassForest;
            bool riderA = (rec.flags & RxSoundContact.RiderA) != 0, riderB = (rec.flags & RxSoundContact.RiderB) != 0;
            bool rollA = rec.classA == RxSoundContact.ClassTerrain, rollB = rec.classB == RxSoundContact.ClassTerrain;
            bool waterA = rec.classA == RxSoundContact.ClassWater, waterB = rec.classB == RxSoundContact.ClassWater;
            int soundA = rec.soundA, soundB = rec.soundB;
            if (rec.b == 0 || forestB) soundB = rec.over;
            if (rec.soft != 0 && forestB) { rec.sA *= tun.soft; rec.sB *= tun.soft; }
            if (riderA) { rec.sA *= tun.riderScale; rec.sB *= tun.riderScale; }
            if (bikeA && rollB) rec.sA *= tun.vehScale;
            if (bikeB && rollA) rec.sB *= tun.vehScale;
            if (bikeA && bikeB) { rec.sA *= tun.vehVeh; rec.sB *= tun.vehVeh; }
            if (rec.b != 0)
            {
                if (Heavy(rec.classA) && Heavy(rec.classB)) { float s = Mathf.Max(rec.sA, rec.sB); rec.sA = s; rec.sB = s; }
                if ((riderA || bikeA) && Heavy(rec.classB)) rec.sB = Mathf.Max(rec.sA, rec.sB);
            }
            float[] level = { Level(rec.sA), Level(rec.sB) };
            for (int k = 0; k < 2; k++)
            {
                bool skip = rec.soft != 0 ? k == 0 || rec.matA == 0 : k == 0 ? rec.matB == 0 : rec.matA == 0;
                if (skip) continue;
                bool bike = k == 0 ? bikeA : bikeB, rider = k == 0 ? riderA : riderB;
                uint vocal = k == 0 ? rec.vocalA : rec.vocalB;
                bool vocalOk = (rec.flags & (k == 0 ? RxSoundContact.VocalA : RxSoundContact.VocalB)) != 0;
                if (bike && vocalOk) { if (level[k] >= tun.yell) RiderSound(vocal, 4); else if (level[k] >= tun.grunt) RiderSound(vocal, 3); }
                if (rider && !(k == 0 ? waterB : waterA) && vocalOk)
                {
                    int kind = 0;
                    if ((rec.flags & (k == 0 ? RxSoundContact.RagdollA : RxSoundContact.RagdollB)) != 0 && level[k] >= tun.ragdollGrunt) kind = 4;
                    if (kind == 0 && level[k] >= tun.ragdollGrunt) kind = 3;
                    if (kind != 0) RiderSound(vocal, kind);
                }
                string name = names.Name(k == 0 ? soundA : soundB);
                if (name.Length > 0)
                {
                    rec.impact = CollisionEvent(rec.Position);
                    Play(rec.impact, name, rec.Position);
                    Param(rec.impact, "Force", level[k]);
                }
                if (soundA == soundB) break;
            }
        }

        void CollisionUpdate(Record rec)   // 0067c2c0
        {
            if (rec.soft != 0 && (rec.impact == null || !rec.impact.playing) && rec.b != 0 && rec.classB == RxSoundContact.ClassForest) { rec.impact = null; CollisionPlay(rec); return; }
            if (rec.b != 0 && rec.classB == RxSoundContact.ClassTerrain) { Roll(rec, rec.zone, zones.Of(rec.zone)?.skid); return; }
            // 0067c670 (checked against the decompile, w-reverb): B is a class 5 object with +0x19c set; the loop is the SKID event of the object table's entry
            // [B + 0x158] (FUN_00679680 = table + 0x1aa0 + 0x310 n, the zones' FUN_00679420 + 8 entries; none for n >= 8), its strength the same switch on A
            // as 0067c350 (rider / physics entity / vehicle speed between the manager's thresholds, else the record's force) written to "SkidFactor"; it is
            // restarted when the material changes (record +0x54 as the terrain's +0x50) and its position follows the record. Only the new event's owner link
            // (event +0x10 = the record) is not made there. The speed inputs of the A switch come with the contact (slide_kind / slide_speed are filled
            // for these contacts too since w-reverb2), so Roll needs nothing more.
            if (rec.b != 0 && rec.classB == RxSoundContact.ClassCollision && (rec.flags & RxSoundContact.SlideB) != 0) Roll(rec, 8 + rec.objectMaterial, zones.Of(8 + rec.objectMaterial)?.skid);
        }

        void Roll(Record rec, int sound, string path)   // 0067c350
        {
            float f;
            float Clamp(float t) => t >= 1f ? 1f : !(0f >= t) ? t : 0f;
            switch (rec.slideKind)
            {
                case 1: f = Clamp((tun.riderSlide * Mph * rec.slideSpeed - tun.riderSlideMin) / (tun.riderSlideMax - tun.riderSlideMin)); break;
                case 2: f = Clamp((rec.slideSpeed * Mph - tun.objSlideMin) / (tun.objSlideMax - tun.objSlideMin)); break;
                case 3: f = Clamp((rec.slideSpeed * Mph - tun.vehSlideMin) / (tun.vehSlideMax - tun.vehSlideMin)); break;
                default: f = Clamp((rec.sA - tun.forceMin) / (tun.forceMax - tun.forceMin)); break;
            }
            if (sound != rec.rollSound && rec.roll != null) { Stop(rec.roll, true); rec.roll = null; }
            if (rec.roll == null)
            {
                rec.rollSound = sound;
                if (!string.IsNullOrEmpty(path))
                {
                    rec.roll = CollisionEvent(rec.Position);
                    Play(rec.roll, path, rec.Position);
                    Param(rec.roll, "SkidFactor", f);
                }
            }
            else { Pos(rec.roll, rec.Position); Param(rec.roll, "SkidFactor", f); rec.roll.at = rec.Position; }
        }

        void Expire()   // 0067b670: a record CollisionTimeoutMS after its last contact
        {
            expired.Clear();
            foreach (var kv in records) if (clockMs - kv.Value.lastMs > tun.timeoutMs) expired.Add(kv.Key);
            foreach (var k in expired)
            {
                var rec = records[k];
                if (rec.roll != null) { Stop(rec.roll); rec.roll = null; }
                records.Remove(k);
            }
        }
    }
}
