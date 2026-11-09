using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using Reflex.Port.RxCore;
using UnityEngine;
using Node = Reflex.Port.Content.BxmlData.Node;

namespace Reflex.Port.OriginalWorld
{
    // The level's own sound entities on the open path (core/plan/QUEUE.md "w-sfx"), ported from the decompile; they depend only on
    // the level data, the listener and (the crowd) the core's crowd state (rx_session_sound: momentum, type / size, reactions):
    //  - the crowd emitters: CrowdLineSoundEmitterEntity (AudioCrowdEmitterRSData, vtable 0x95d14c; vf5 00673b70, vf19 00673ff0): four
    //    events per emitter (CROWD_AMB with Crowd_Momentum, CROWD_RCT_NEG / _POS / _POS_START_END on the crowd reactions 006745d0) at the
    //    point of its polyline nearest the listener (00680470), 3D distance MinDistance .. MaxDistance (0080b310), on the Crowd channel;
    //    the project of the crowd type (CrowdAudioParameters Assets: CROWD_IND_LRG for a stadium, CROWD_OUT_LRG outdoors).
    //  - the ambience: SoundEmitterEntity (AudioEmitterRSData 0x95d23c; vf5 00679110 -> 00678cf0, vf19 00679370 -> 00678980) and
    //    LineSoundEmitterEntity (AudioLineEmitterRSData 0x95d1c4; vf5 00675560, vf19 006757d0): "<SoundAsset>/<DesignerEventName>" (PA,
    //    wind, birds, water, the crowd beds ...), started while the listener is inside MaxDistance (AutoStart), 3D ones keep playing (their
    //    rolloff silences them), 2D ones stop outside; on the Effects channel (00678b80: record 1).
    //  - the activity audio cues (the announcer / stingers: DROP_GATE, HOLESHOT_WON, COUNTDOWN_TIMER, EVENT_FINISH, ELIMINATED, ...): the
    //    core's ActivityAudioLogic plays them (rx_session_sound cues: "<SoundAsset>/<EventName>", the sound index); each is a 2D event
    //    (0067ead0: one wrapper per path and owner, the Effects channel; [LOC] / [FORMAT] tokens as 0067e560).
    //  - the programmer-selected entries of the crowd events (SOUNDDEF_SELECTINDEX, 006743b0): each emitter's four wrappers carry the owner callback
    //    (host sub-command 14), OriginalAudio.PickProgrammer rotates the pick per kind as the callback does.
    //  - the reverb zones (LevelAmbientAudioZone / AudioZoneSphere / AudioLineZoneSphere) are OriginalReverb's (OriginalAudio.Reverb).
    public sealed class OriginalLevelSound
    {
        sealed class Ev { public uint wrapper, serial; public string path = ""; public bool playing; }
        readonly OriginalAudio audio;
        readonly Dictionary<uint, Ev> byWrapper = new Dictionary<uint, Ev>();
        uint nextWrapper = OriginalAudio.HostWrapperBase + 0x200000;
        Ev NewEv() { var e = new Ev { wrapper = nextWrapper++ }; byWrapper[e.wrapper] = e; return e; }
        void OnFinished(uint wrapper, uint serial) { if (byWrapper.TryGetValue(wrapper, out var e) && e.serial == serial) e.playing = false; }
        void Play(Ev e, string path, Vector3 at, bool hasPosition)
        {
            if (e == null || string.IsNullOrEmpty(path)) return;
            e.path = path; e.serial++; e.playing = true;
            audio.Host(0, e.wrapper, e.serial, path, null, 0f, at, hasPosition);
        }
        void Stop(Ev e) { if (e == null || !e.playing) return; audio.Host(1, e.wrapper, e.serial, e.path, null, 0f, Vector3.zero, false); e.playing = false; }
        void Param(Ev e, string name, float v) { if (e != null && e.playing) audio.Host(4, e.wrapper, e.serial, e.path, name, v, Vector3.zero, false); }
        void Pos(Ev e, Vector3 at) { if (e != null && e.playing) audio.Host(5, e.wrapper, e.serial, e.path, null, 0f, at, true); }
        void MinMax(Ev e, float min, float max) => audio.Host(6, e.wrapper, e.serial, e.path, null, min, new Vector3(max, 0f, 0f), false);
        void Channel(Ev e, ReflexChannel c) => audio.Host(13, e.wrapper, e.serial, e.path, null, (int)c, Vector3.zero, false);
        /// The wrapper's SOUNDDEF_SELECTINDEX owner callback (+0x19c = 006743b0), kind 0 amb / 1 neg / 2 pos / 3 pos start-end (the emitter's wrapper offsets 0x90 / 0x234 / 0x3d8 / 0x57c).
        void Selector(Ev e, int kind) => audio.Host(14, e.wrapper, e.serial, e.path, null, kind, Vector3.zero, false);

        // ------------------------------------------------------------------------------------------------ data
        static Node Prop(Node n, string name) => n?.Property(name);
        static object Val(Node n) => n == null ? null : n.children.Count > 0 ? n.children[0].value : n.value;
        static string Str(Node n, string name) { var p = Prop(n, name); return p == null ? null : (p.Child("Enum")?.value ?? Val(p)) as string; }
        static bool Has(Node n, string name) => Prop(n, name) != null && Val(Prop(n, name)) != null;
        static float Num(Node n, string name, float d = 0f) { var v = Val(Prop(n, name)); return v is float f ? f : v is int i ? i : v is uint u ? u : v is bool b ? (b ? 1 : 0) : d; }

        /// The entity's points in world space: each through the matrix's normalised axes plus its translation (006803a0, row vectors).
        internal static Vector3[] Points(SceneEntity e)
        {
            var m = e.Matrix ?? new float[] { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
            Vector3 ax = new Vector3(m[0], m[1], m[2]).normalized, ay = new Vector3(m[4], m[5], m[6]).normalized, az = new Vector3(m[8], m[9], m[10]).normalized;
            var t = new Vector3(m[12], m[13], m[14]);
            var list = new List<Vector3>();
            var pl = Prop(e.rsdata, "PointList");
            var lv = pl?.Child("ListV3") ?? (pl != null && pl.children.Count > 0 ? pl.children[0] : null);
            if (lv != null) foreach (var c in lv.children) if (c.value is float[] p && p.Length >= 3) list.Add(t + ax * p[0] + ay * p[1] + az * p[2]);
            if (list.Count == 0) list.Add(t);
            return list.ToArray();
        }
        /// 00680470: the point of the polyline nearest the listener (per segment t = (L - a).(b - a) / |b - a|^2; above 1 b, below 0 or NaN a)
        internal static Vector3 Nearest(Vector3[] pts, Vector3 L)
        {
            if (pts.Length == 1) return pts[0];
            Vector3 best = pts[0]; float bd = float.MaxValue;
            for (int i = 0; i + 1 < pts.Length; i++)
            {
                Vector3 a = pts[i], b = pts[i + 1], ab = b - a;
                float t = Vector3.Dot(L - a, ab) / ab.sqrMagnitude;
                Vector3 q = t > 1f ? b : !(t >= 0f) ? a : a + ab * t;
                float d = (q - L).sqrMagnitude;
                if (d < bd) { bd = d; best = q; }
            }
            return best;
        }
        /// AudioEmitter's MinDistance / MaxDistance, or InnerRadius / OuterRadius when MinDistance is absent (00673b70 / 00678cf0)
        static void Distances(Node ae, float dmin, float dmax, out float min, out float max)
        {
            if (Has(ae, "MinDistance")) { min = Num(ae, "MinDistance", dmin); max = Num(ae, "MaxDistance", dmax); }
            else { min = Num(ae, "InnerRadius", dmin); max = Num(ae, "OuterRadius", dmax); }
        }

        sealed class Crowd { public Vector3[] pts; public float min, max; public Ev amb, neg, pos, posEnd; public bool sized; public bool inRange; public Vector3 at; }
        sealed class Ambient { public string path; public Vector3[] pts; public bool line, autostart, is3d, sized; public float min, max; public Ev ev; }
        readonly List<Crowd> crowds = new List<Crowd>();
        readonly List<Ambient> ambient = new List<Ambient>();
        readonly string[] crowdProject = { "CROWD_IND_LRG/CROWD_IND_LRG", "CROWD_OUT_LRG/CROWD_OUT_LRG" };   // CrowdAudioParameters Assets: SoundAsset / EventCategory by type
        bool levelRead; int levelTries; uint lastReaction, lastCue;
        readonly Dictionary<string, Ev> cueEvents = new Dictionary<string, Ev>(StringComparer.OrdinalIgnoreCase);
        public int CuesPlayed { get; private set; }
        public int CrowdEmitters => crowds.Count;
        public int AmbientEmitters => ambient.Count;
        public float LastMomentum { get; private set; }

        public OriginalLevelSound(OriginalAudio audio)
        {
            this.audio = audio;
            if (audio) { audio.HostFinished += OnFinished; audio.ResetCrowdPicks(); }   // 00674cd0: the crowd unit's init zeroes the rotation counters
            try
            {
                var root = Rfx.Bxml(ReflexInstall.Packages.Read("CrowdSoundProjects", "Crowd_Config", "CrowdAudioParameters", "bxml"));
                foreach (var n in root.All())
                {
                    if (n.tag != "Assets") continue;
                    string type = n.attributes.TryGetValue("Type", out var t) ? t as string : null, asset = n.attributes.TryGetValue("SoundAsset", out var a) ? a as string : null,
                           cat = n.attributes.TryGetValue("EventCategory", out var c) ? c as string : null;
                    if (string.IsNullOrEmpty(asset) || string.IsNullOrEmpty(cat)) continue;
                    if (type == "CROWDTYPE_STADIUM") crowdProject[0] = asset + "/" + cat; else if (type == "CROWDTYPE_OUTDOOR") crowdProject[1] = asset + "/" + cat;
                }
            }
            catch (Exception e) { Debug.LogWarning("Level sound: CrowdAudioParameters not read (" + e.Message + "); the shipped projects are used"); }
        }

        public void Dispose()
        {
            if (audio) { audio.HostFinished -= OnFinished; foreach (var e in byWrapper.Values) if (e.playing) audio.Host(12, e.wrapper, e.serial, e.path, null, 0f, Vector3.zero, false); }
            byWrapper.Clear(); crowds.Clear(); ambient.Clear();
        }

        void ReadLevel()
        {
            if (levelRead || !audio.LibraryReady || levelTries++ % 60 != 0) return;   // the events' 3D modes come from the race library
            var lvl = UnityEngine.Object.FindAnyObjectByType<OriginalLevel>();
            var content = lvl ? lvl.Content : null;
            if (content == null || content.SceneEntities == null) return;
            foreach (var e in content.SceneEntities)
            {
                try
                {
                    var t = e.Type;
                    if (t == "AudioCrowdEmitterRSData")
                    {
                        var c = new Crowd { pts = Points(e) };
                        Distances(Prop(e.rsdata, "AudioEmitter"), 1f, 10000f, out c.min, out c.max);   // the ctor's defaults 0x93005c / 0x977de8 (INFERRED 1 / 10000)
                        crowds.Add(c);
                    }
                    else if (t == "AudioEmitterRSData" || t == "AudioLineEmitterRSData")
                    {
                        var ae = Prop(e.rsdata, "AudioEmitter");
                        string asset = Prop(ae, "SoundAsset")?.Asset ?? "", ev = Str(ae, "DesignerEventName");
                        if (string.IsNullOrEmpty(asset) || string.IsNullOrEmpty(ev)) continue;
                        var a = new Ambient { path = asset + "/" + ev, line = t == "AudioLineEmitterRSData" };
                        a.pts = a.line ? Points(e) : new[] { e.Matrix != null ? new Vector3(e.Matrix[12], e.Matrix[13], e.Matrix[14]) : Vector3.zero };
                        Distances(ae, 1f, 10000f, out a.min, out a.max);
                        a.autostart = Has(ae, "AutoStart") ? Num(ae, "AutoStart") != 0 : Num(ae, "StartEnabled") != 0;
                        a.is3d = audio.IsEvent3D(a.path);
                        ambient.Add(a);
                    }
                }
                catch (Exception x) { Debug.LogWarning("Level sound: " + e + " not read: " + x.Message); }
            }
            // MAP SCALE (core/plan/MAP_SCALE.md): the emitters stand where the scaled track puts them; their ranges follow
            float ms = lvl.MapScale;
            if (ms != 1f)
            {
                foreach (var c in crowds) { for (int i = 0; i < c.pts.Length; i++) c.pts[i] *= ms; c.min *= ms; c.max *= ms; }
                foreach (var a in ambient) { for (int i = 0; i < a.pts.Length; i++) a.pts[i] *= ms; a.min *= ms; a.max *= ms; }
            }
            levelRead = true;
            Debug.Log("[level-sound] crowd emitters " + crowds.Count + ", ambient emitters " + ambient.Count + " (" + string.Join(", ", ambient.ConvertAll(a => a.path)) + ")");
        }

        /// A collected core result.
        public void Frame(RxSoundState s)
        {
            if (!audio || s == null || !s.valid) return;
            ReadLevel();
            if (!levelRead) return;
            var L = audio.Listener;
            // ---- the crowd emitters' vf19 (00673ff0)
            bool active = s.crowdValid && s.crowdType >= 0 && s.crowdType <= 1 && s.crowdSize >= 0;
            string project = active ? crowdProject[s.crowdType] : "";
            LastMomentum = s.crowdMomentum;
            foreach (var c in crowds)
            {
                if (c.amb == null) { c.amb = NewEv(); c.neg = NewEv(); c.pos = NewEv(); c.posEnd = NewEv(); Selector(c.amb, 0); Selector(c.neg, 1); Selector(c.pos, 2); Selector(c.posEnd, 3); }
                if (!active) { Stop(c.amb); Stop(c.neg); Stop(c.pos); Stop(c.posEnd); c.inRange = false; continue; }
                c.at = Nearest(c.pts, L);
                c.inRange = (c.at - L).sqrMagnitude < c.max * c.max;
                if (!c.inRange) { Stop(c.amb); Stop(c.neg); Stop(c.pos); Stop(c.posEnd); continue; }
                if (!c.sized) { foreach (var e in new[] { c.amb, c.neg, c.pos, c.posEnd }) MinMax(e, c.min, c.max); c.sized = true; }
                Pos(c.amb, c.at); Pos(c.neg, c.at); Pos(c.pos, c.at); Pos(c.posEnd, c.at);
                if (!c.amb.playing) Play(c.amb, project + "/CROWD_AMB", c.at, true);
                Param(c.amb, "Crowd_Momentum", s.crowdMomentum);
            }
            // ---- the crowd reactions (006745d0): every emitter plays the kind's event (an emitter out of range is stopped by its next vf19)
            if (s.reactionSerial < lastReaction) lastReaction = 0;
            for (int k = 0; k < RxSoundState.Reactions; k++)
            {
                uint serial = s.reactionSerials[k];
                if (serial <= lastReaction || serial > s.reactionSerial) continue;
                int kind = s.reactionKinds[k];
                string ev = kind == 1 ? "CROWD_RCT_NEG" : kind == 2 ? "CROWD_RCT_POS" : kind == 3 ? "CROWD_RCT_POS_START_END" : null;
                if (ev == null || !active) continue;
                foreach (var c in crowds)
                {
                    if (!c.inRange) continue;
                    var e = kind == 1 ? c.neg : kind == 2 ? c.pos : c.posEnd;
                    Play(e, project + "/" + ev, c.at, true);
                }
            }
            lastReaction = Math.Max(lastReaction, s.reactionSerial);
            // ---- the activity cues (005812f0 -> 0067ead0)
            if (s.cueSerial < lastCue) lastCue = 0;
            for (uint serial = Math.Max(lastCue + 1, s.cueSerial >= RxSoundState.Cues ? s.cueSerial - RxSoundState.Cues + 1 : 1); serial <= s.cueSerial; serial++)
            {
                var c = s.cues[serial % RxSoundState.Cues];
                if (c.serial != serial || string.IsNullOrEmpty(c.path)) continue;
                string path = ReflexAudioSet.ResolveTokens(c.path, audio.Spec);
                if (!cueEvents.TryGetValue(path, out var e)) { e = cueEvents[path] = NewEv(); Channel(e, ReflexChannel.Effects); }
                Play(e, path, L, false);
                CuesPlayed++;
                Debug.Log("[level-sound] cue " + path + (c.index >= 0 ? " index " + c.index : ""));
            }
            lastCue = Math.Max(lastCue, s.cueSerial);
            // ---- the ambience (00678980)
            foreach (var a in ambient)
            {
                a.ev ??= NewEv();
                var at = a.line ? Nearest(a.pts, L) : a.pts[0];
                float d = (at - L).magnitude;
                if (!a.sized)
                {
                    Channel(a.ev, ReflexChannel.Effects);
                    if (a.is3d) MinMax(a.ev, a.min, a.max);
                    a.sized = true;
                }
                if (!a.autostart) continue;
                if (a.is3d)
                {
                    Pos(a.ev, at);
                    if (d < a.max && !a.ev.playing) Play(a.ev, a.path, at, true);
                }
                else if (a.max < d && a.ev.playing) Stop(a.ev);
                else if (d < a.max && !a.ev.playing) Play(a.ev, a.path, at, false);
            }
        }
    }
}
