// Runs the original engine emitter (OriginalEngineAudio, the C# the Unity client runs) on a recorded telemetry trace (aitrace.c) and writes
// the FMOD command stream it issues: play / stop / parameter per rider, with the frame. The harness plays the sound engine's part: a
// one-shot event ends after its entry's length (EVENT_FINISHED), a loop never does, entries are picked random-no-repeat from a seeded RNG.
//   engine_logic <trace.csv> <events.csv> <states.csv> <MXRegistry.xml dir> > commands.tsv
// events.csv: path,loop(0|1),len1;len2;..   states.csv: size,state,path
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;

static class Program
{
    sealed class Ev { public bool loop; public float[] len; }
    sealed class Playing { public string path; public int entry; public bool loop; public int endFrame; public uint serial; public int last = -1; }

    static int Main(string[] a)
    {
        var ci = CultureInfo.InvariantCulture;
        Reflex.Port.Content.ReflexInstall.Root = a[3];
        var events = new Dictionary<string, Ev>(StringComparer.OrdinalIgnoreCase);
        foreach (var l in File.ReadLines(a[1])) { var p = l.Split(','); if (p.Length < 3) continue; events[p[0]] = new Ev { loop = p[1] == "1", len = p[2].Split(';').Select(x => float.Parse(x, ci)).ToArray() }; }
        var audio = new OriginalAudio();
        var eng = new OriginalEngineAudio(audio);
        // inject the state table (the install's AudioEngineStates, exported to states.csv)
        var tableF = typeof(OriginalEngineAudio).GetField("table", BindingFlags.NonPublic | BindingFlags.Instance);
        var table = tableF.GetValue(eng);
        var pathF = table.GetType().GetField("path");
        var paths = (string[,])pathF.GetValue(table);
        string[] names = OriginalEngineAudio.StateNames;
        foreach (var l in File.ReadLines(a[2])) { var p = l.Split(','); int size = int.Parse(p[0]), st = Array.IndexOf(names, p[1]); if (st >= 0) paths[size, st] = p[2]; }

        var emittersF = typeof(OriginalEngineAudio).GetField("emitters", BindingFlags.NonPublic | BindingFlags.Instance);
        var mapped = new HashSet<uint>();
        var rng = new Random(12345);
        var play = new Dictionary<uint, Playing>();
        int frame = 0;
        var o = Console.Out;
        audio.Sink = (sub, w, serial, path, param, value, pos, hasPos) =>
        {
            switch (sub)
            {
                case 0:
                    {
                        if (!events.TryGetValue(path, out var ev)) { o.WriteLine($"{frame}\tmissing\t{w}\t{path}"); return; }
                        int n = ev.len.Length; play.TryGetValue(w, out var prev);
                        int e = rng.Next(n); if (!ev.loop && n > 1 && prev != null && prev.path == path) while (e == prev.last) e = rng.Next(n);
                        if (ev.loop) e = 0;
                        var pl = new Playing { path = path, entry = e, loop = ev.loop, serial = serial, last = e, endFrame = ev.loop ? int.MaxValue : frame + (int)Math.Ceiling(ev.len[e] * 60f) };
                        play[w] = pl;
                        o.WriteLine($"{frame}\tplay\t{w}\t{path}\t{e}");
                        break;
                    }
                case 1: case 12: if (play.Remove(w)) o.WriteLine($"{frame}\tstop\t{w}"); break;
                case 2: o.WriteLine($"{frame}\tvol\t{w}\t{value.ToString("0.####", ci)}"); break;
                case 4: o.WriteLine($"{frame}\tparam\t{w}\t{param}\t{value.ToString("0.####", ci)}"); break;
                case 10: if (value > 0) o.WriteLine($"{frame}\tocc\t{w}\t{value.ToString("0.###", ci)}"); break;
            }
        };

        var state = new RxVehicleAudioState { valid = true };
        var cueRing = new List<RxVehicleAudioCue>();
        uint cueSerial = 0;
        // read the trace frame by frame
        var rows = new List<string[]>(); int curFrame = -1; var cueRows = new List<string[]>();
        void Flush()
        {
            if (curFrame < 0) return;
            frame = curFrame;
            foreach (var c in cueRows) { var cue = new RxVehicleAudioCue { serial = uint.Parse(c[2]), kind = int.Parse(c[3]), name = c.Length > 4 ? c[4] : "" }; cueRing.Add(cue); cueSerial = Math.Max(cueSerial, cue.serial); }
            cueRows.Clear();
            state.cueSerial = cueSerial; state.riderCount = rows.Count;
            for (int i = 0; i < 16; i++) { var s = state.cues[i]; if (i < cueRing.Count) { var c = cueRing[Math.Max(0, cueRing.Count - 16) + i]; s.serial = c.serial; s.kind = c.kind; s.name = c.name; } }
            for (int i = 0; i < rows.Count; i++)
            {
                var c = rows[i]; var r = state.riders[i];
                r.slot = int.Parse(c[3]); r.flags = uint.Parse(c[4]); r.vehicleClass = int.Parse(c[5]); r.vehicleKind = int.Parse(c[6]); r.pad = int.Parse(c[7]); r.engineSize = int.Parse(c[8]); r.vehicle = uint.Parse(c[9]);
                r.px = float.Parse(c[10], ci); r.py = float.Parse(c[11], ci); r.pz = float.Parse(c[12], ci); r.speed = float.Parse(c[13], ci); r.rpm = float.Parse(c[14], ci); r.maxRpm = float.Parse(c[15], ci); r.idleRpm = float.Parse(c[16], ci);
                r.throttle = float.Parse(c[17], ci); r.control2 = float.Parse(c[18], ci); r.gear = int.Parse(c[19]); r.load = float.Parse(c[20], ci); r.clutch = c[21] != "0"; r.airborne = c[22] != "0"; r.belowHalf = c[23] != "0";
                r.water = float.Parse(c[24], ci); r.underwater = c[25] != "0"; r.drowned = c[26] != "0"; r.wrecked = c[27] != "0";
                for (int k = 0; k < 4; k++) r.wheelAir[k] = c[28 + k] != "0";
                r.audioSlot = (r.flags & RxVehicleAudioRider.AudioSlot) != 0 ? int.Parse(c[32]) : -1;
                bool sl = (r.flags & RxVehicleAudioRider.StartLine) != 0;
                r.startLineState = sl ? int.Parse(c[33]) : -1; r.startLineSerial = sl ? uint.Parse(c[34]) : 0u; r.startLineTarget = sl ? float.Parse(c[35], ci) : 0f;
            }
            // the sound engine's part: one-shots that ended since the last frame
            var ended = play.Where(kv => kv.Value.endFrame <= frame).Select(kv => kv.Key).ToList();
            foreach (var w in ended) { var pl = play[w]; play.Remove(w); o.WriteLine($"{frame}\tend\t{w}"); audio.Finish(w, pl.serial); }
            for (int i = 0; i < rows.Count; i++) if ((state.riders[i].flags & RxVehicleAudioRider.LocalHuman) != 0) audio.Listener = new Vector3(state.riders[i].px, state.riders[i].py, state.riders[i].pz);
            if (Environment.GetEnvironmentVariable("FIELD_BUDGET") is string fb) OriginalEngineAudio.FieldBudget = float.Parse(fb, ci);
            eng.Frame(state, 1f / 60f);
            foreach (System.Collections.DictionaryEntry kv in (System.Collections.IDictionary)emittersF.GetValue(eng))   // wrapper -> slot, once
            {
                var em = kv.Value; var t = em.GetType(); uint w = (uint)t.GetField("wrapperId").GetValue(em);
                if (mapped.Add(w)) o.WriteLine($"{frame}\tmap\t{w}\t{(int)t.GetField("slot").GetValue(em)}");
            }
            // rider telemetry for the mixer: position + slot + aifx per rider, once a second and at the first frame
            if (frame % 60 == 1 || frame < 3)
                for (int i = 0; i < rows.Count; i++) { var r = state.riders[i]; o.WriteLine($"{frame}\trider\t{r.slot}\t{eng.AifxOf(r.slot).ToString("0.##", ci)}\t{((r.flags & RxVehicleAudioRider.LocalHuman) != 0 ? 1 : 0)}"); }
            rows.Clear();
        }
        foreach (var l in File.ReadLines(a[0]).Skip(1))
        {
            var c = l.Split(',');
            if (c[0] == "C") { cueRows.Add(c); continue; }
            if (c[0] == "F") continue;
            int f = int.Parse(c[0]);
            if (f != curFrame) { Flush(); curFrame = f; }
            rows.Add(c);
        }
        Flush();
        return 0;
    }
}
