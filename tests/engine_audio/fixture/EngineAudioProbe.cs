using System;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;

// Test only (tests/engine_audio, w-audio): watches the open path's vehicle sound layer while a race runs. Boots with REFLEX_PREVIEW_SHOTS (the
// run's screenshot folder, run.py sets it). ENGINE_AUDIO_TEST_PACK=<pack id or "first"> makes that engine pack the default for every bike
// (the test save copy). Every 0.5 s of racing it logs the local rider's telemetry, its original emitter's state and event, the emitters,
// and the pack voices; from 3 s of racing it records 14 s of the listener's output to engine-audio.wav (OnAudioFilterRead). Presses nothing.
public sealed class EngineAudioProbe : MonoBehaviour
{
    const string Tag = "[engine-audio] ";
    string directory; float racingSince = -1f, nextLog; bool recordingDone;
    Recorder recorder;

    sealed class Recorder : MonoBehaviour
    {
        public volatile bool on; public int channels, rate; readonly System.Collections.Generic.List<float> data = new System.Collections.Generic.List<float>(48000 * 2 * 16);
        void OnAudioFilterRead(float[] d, int ch) { if (!on) return; channels = ch; lock (data) data.AddRange(d); }
        public void Save(string path)
        {
            float[] s; lock (data) s = data.ToArray();
            using var w = new BinaryWriter(File.Create(path));
            int bytes = s.Length * 2;
            w.Write(Encoding.ASCII.GetBytes("RIFF")); w.Write(36 + bytes); w.Write(Encoding.ASCII.GetBytes("WAVEfmt ")); w.Write(16); w.Write((short)1); w.Write((short)channels);
            w.Write(rate); w.Write(rate * channels * 2); w.Write((short)(channels * 2)); w.Write((short)16); w.Write(Encoding.ASCII.GetBytes("data")); w.Write(bytes);
            foreach (var v in s) w.Write((short)Mathf.Clamp(v * 32767f, -32768f, 32767f));
            float peak = s.Length > 0 ? s.Max(Mathf.Abs) : 0f; double rms = s.Length > 0 ? Math.Sqrt(s.Select(v => (double)v * v).Average()) : 0;
            Debug.Log(Tag + "RECORDED " + path + " seconds " + (s.Length / (float)Math.Max(1, channels) / Math.Max(1, rate)).ToString("F1") + " peak " + peak.ToString("F3") + " rms " + rms.ToString("F4"));
        }
    }

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        string d = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
        if (string.IsNullOrEmpty(d)) return;
        var go = new GameObject("Engine audio probe (test)"); DontDestroyOnLoad(go);
        go.AddComponent<EngineAudioProbe>().directory = d;
        Debug.Log(Tag + "PACKS " + string.Join(" | ", EngineSoundPacks.All.Select(p => p.id + " = " + p.Title + " samples " + p.layers[0].Count + "/" + p.layers[1].Count + " max " + p.maxRpm + " idle " + (p.IdleNorm * p.maxValue).ToString("F0") + " limiter " + Path.GetFileName(p.limiterWav) + (p.warnings.Count > 0 ? " WARN " + string.Join(",", p.warnings) : ""))));
        if (Environment.GetEnvironmentVariable("ENGINE_AUDIO_TEST_SOLO") == "1") { OriginalVehicleSound.SoloLocal = true; Debug.Log(Tag + "SOLO local engine"); }
        string pack = Environment.GetEnvironmentVariable("ENGINE_AUDIO_TEST_PACK");
        if (!string.IsNullOrEmpty(pack))
        {
            string id = pack == "first" ? EngineSoundPacks.All.FirstOrDefault()?.id : pack;
            EngineSoundPacks.DefaultChoice = id ?? "";
            Debug.Log(Tag + "DEFAULT PACK " + EngineSoundPacks.DefaultChoice);
        }
    }

    void Update()
    {
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        var vs = rt ? rt.VehicleSound : null;
        bool racing = rt && rt.Race.state == 2 && vs != null;
        if (!racing) return;
        if (racingSince < 0f)
        {
            racingSince = Time.unscaledTime; Debug.Log(Tag + "RACING riders " + vs.Last.riderCount + " emitters " + vs.Engines.EmitterCount);
            Debug.Log(Tag + "AIFX " + string.Join(" ", Enumerable.Range(0, vs.Last.riderCount).Select(i => "slot" + vs.Last.riders[i].slot + "=" + vs.Engines.AifxOf(vs.Last.riders[i].slot).ToString("F0"))));
        }
        float t = Time.unscaledTime - racingSince;
        if (!recordingDone && t >= 3f && recorder == null)
        {
            var listener = FindAnyObjectByType<AudioListener>();
            if (listener) { recorder = listener.gameObject.AddComponent<Recorder>(); recorder.rate = AudioSettings.outputSampleRate; recorder.on = true; Debug.Log(Tag + "RECORD START on " + listener.name); }
        }
        if (recorder != null && !recordingDone && t >= 17f) { recorder.on = false; recordingDone = true; recorder.Save(Path.Combine(directory, "engine-audio.wav")); LogStats(rt); }
        if (Time.unscaledTime < nextLog) return;
        nextLog = Time.unscaledTime + 0.5f;
        var st = vs.Last; var sb = new StringBuilder();
        for (int i = 0; i < st.riderCount; i++)
        {
            var r = st.riders[i];
            if (i > 0 && (r.flags & 4) == 0 && i != st.riderCount - 1) continue;
            sb.Append(" | slot ").Append(r.slot).Append(" size ").Append(r.engineSize).Append(" rpm ").Append(r.rpm.ToString("F0")).Append('/').Append(r.maxRpm.ToString("F0"))
              .Append(" thr ").Append(r.throttle.ToString("F2")).Append(" gear ").Append(r.gear).Append(" air ").Append(r.airborne ? 1 : 0)
              .Append(" state ").Append(vs.Engines.StateOf(r.slot) is int s && s >= 0 ? OriginalEngineAudio.StateNames[s] : "-").Append(" event ").Append(vs.Engines.EventOf(r.slot));
            var p = vs.PackPlayerOfSlot(r.slot);
            if (p != null) sb.Append(" PACK ").Append(p.Pack.name).Append(" n ").Append(p.LastNorm.ToString("F3")).Append(" on ").Append(p.LastOn.ToString("F2")).Append(p.LimiterOn ? " LIMITER" : "")
                .Append(" [").Append(string.Join(", ", p.Voices().Where(v => v.volume > 0.01f).Select(v => v.layer + ":" + v.file + " v" + v.volume.ToString("F2") + " p" + v.pitch.ToString("F2")))).Append(']');
        }
        var audio = rt.gameAudio;
        Debug.Log(Tag + "t " + t.ToString("F1") + " events " + (audio ? audio.ActiveEvents : 0) + " voices " + (audio ? audio.ActiveVoices : 0) + " virt " + (audio ? audio.Virtualized : 0) + sb);
    }

    // w-sfx: every event the race played so far (OriginalAudio.Stats: starts / voices per event path) and the channel gains (level mix x settings)
    static void LogStats(OriginalWorldRuntime rt)
    {
        var audio = rt ? rt.gameAudio : null; if (!audio) return;
        foreach (var kv in audio.Stats.OrderByDescending(k => k.Value.starts))
            Debug.Log(Tag + "STAT " + kv.Key + " starts " + kv.Value.starts + " voices " + kv.Value.voices + " unknown " + kv.Value.unknown + " failed " + kv.Value.failed + " channel " + kv.Value.channel);
        var gains = string.Join(" ", Enum.GetValues(typeof(ReflexChannel)).Cast<ReflexChannel>().Select(c => c + "=" + audio.ChannelGain(c).ToString("F2") + "(mix " + audio.LevelMix(c).ToString("F2") + ")"));
        Debug.Log(Tag + "GAINS " + gains);
        var vs = rt.VehicleSound;
        if (vs != null && vs.Game != null) Debug.Log(Tag + "GAME SOUND collision events " + vs.Game.CollisionEvents + " local surface " + vs.Game.SurfaceOf(0));
        if (vs != null && vs.Level != null) Debug.Log(Tag + "LEVEL SOUND crowd emitters " + vs.Level.CrowdEmitters + " ambient emitters " + vs.Level.AmbientEmitters + " momentum " + vs.Level.LastMomentum.ToString("F2") + " cues " + vs.Level.CuesPlayed);
        var music = FindAnyObjectByType<OriginalMusic>();
        if (music) Debug.Log(Tag + "MUSIC following " + music.FollowingGame + " mode " + music.GameMode + " score " + music.ScorePlaying + " now '" + music.NowPlaying + "' ramp " + music.Ramp.ToString("F2") + " playlist [" + string.Join(",", music.GamePlaylist) + "]");
    }

    void OnDestroy() { if (recorder != null && !recordingDone) { recorder.on = false; recordingDone = true; recorder.Save(Path.Combine(directory, "engine-audio.wav")); } }
}
