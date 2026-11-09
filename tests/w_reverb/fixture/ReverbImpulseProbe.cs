using System;
using System.IO;
using System.Text;
using Reflex.Port.OriginalWorld;
using UnityEngine;

// Test only (tests/w_reverb, w-reverb): measures the open path's reverb as a room impulse response. REVERB_PROBE=1 and REFLEX_PREVIEW_SHOTS (the
// run's screenshot folder) switch it on. When the race runs it silences the game's own sound (OriginalAudio's channels at 0), then fires one
// click from a 3D source 3 ft from the listener in four windows of 5 s and records the listener's output (OnAudioFilterRead) of each:
//   reverb-off.wav (OriginalReverb.EnabledOverride false), reverb-on.wav (the level's ambient and zones), reverb-firstperson.wav (the first-person
//   camera's reverb on top), reverb-on-2d.wav (the click as a 2D source: must stay dry). Writes <folder>/reverb-<phase>.wav and logs "[reverb-probe]".
public sealed class ReverbImpulseProbe : MonoBehaviour
{
    const string Tag = "[reverb-probe] ";
    string directory; OriginalWorldRuntime rt; OriginalAudio audio; Recorder recorder; AudioSource click; AudioClip clip;
    int phase = -1; float phaseAt; bool started, done; float racingSince = -1f;
    static readonly string[] Names = { "reverb-off", "reverb-on", "reverb-firstperson", "reverb-on-2d" };

    sealed class Recorder : MonoBehaviour
    {
        public volatile bool on; public int channels, rate; readonly System.Collections.Generic.List<float> data = new System.Collections.Generic.List<float>(48000 * 2 * 10);
        void OnAudioFilterRead(float[] d, int ch) { if (!on) return; channels = ch; lock (data) data.AddRange(d); }
        public void Reset() { lock (data) data.Clear(); }
        public void Save(string path)
        {
            float[] s; lock (data) s = data.ToArray();
            using var w = new BinaryWriter(File.Create(path));
            int bytes = s.Length * 2;
            w.Write(Encoding.ASCII.GetBytes("RIFF")); w.Write(36 + bytes); w.Write(Encoding.ASCII.GetBytes("WAVEfmt ")); w.Write(16); w.Write((short)1); w.Write((short)channels);
            w.Write(rate); w.Write(rate * channels * 2); w.Write((short)(channels * 2)); w.Write((short)16); w.Write(Encoding.ASCII.GetBytes("data")); w.Write(bytes);
            foreach (var v in s) w.Write((short)Mathf.Clamp(v * 32767f, -32768f, 32767f));
        }
    }

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        string d = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
        if (string.IsNullOrEmpty(d) || Environment.GetEnvironmentVariable("REVERB_PROBE") != "1") return;
        var go = new GameObject("Reverb impulse probe (test)"); DontDestroyOnLoad(go);
        go.AddComponent<ReverbImpulseProbe>().directory = d;
        Debug.Log(Tag + "BOOT");
    }

    void Update()
    {
        if (done) return;
        if (!rt) rt = FindAnyObjectByType<OriginalWorldRuntime>();
        if (!rt || !rt.Running || rt.gameAudio == null) return;
        audio = rt.gameAudio;
        bool racing = rt.Race.state == 2;
        if (!started)
        {
            if (!racing) return;
            if (racingSince < 0f) { racingSince = Time.unscaledTime; return; }
            if (Time.unscaledTime - racingSince < 0.5f) return;
            started = true;
            audio.followSettings = false; audio.masterVolume = 0f; audio.engineVolume = 0f;      // the game's own sound is silent: the click is all there is
            var listener = FindAnyObjectByType<AudioListener>();
            recorder = listener.gameObject.AddComponent<Recorder>(); recorder.rate = AudioSettings.outputSampleRate;
            int n = 12000; var data = new float[n]; var rnd = new System.Random(5);
            for (int i = 0; i < 960; i++) data[i] = (float)(rnd.NextDouble() * 2 - 1) * 0.8f;           // a 20 ms noise burst, then silence
            clip = AudioClip.Create("click", n, 1, 48000, false); clip.SetData(data, 0);
            var go = new GameObject("click"); click = go.AddComponent<AudioSource>(); click.clip = clip; click.loop = false; click.playOnAwake = false; click.dopplerLevel = 0f;
            click.rolloffMode = AudioRolloffMode.Linear; click.minDistance = 1f; click.maxDistance = 500f; click.volume = 1f; click.priority = 0;
            Debug.Log(Tag + "START level " + audio.level + " ambient " + audio.Reverb.AmbientName + " zones " + audio.Reverb.ZoneCount + " camera " + audio.Reverb.CameraName);
            NextPhase();
            return;
        }
        if (phase >= 0 && phase < Names.Length)
        {
            // the master output's level per frame (AudioListener.GetOutputData sees the reverb zones' output, the listener filter does not)
            int take = Mathf.Clamp(Mathf.RoundToInt(Time.unscaledDeltaTime * AudioSettings.outputSampleRate), 64, 1024), skip = 1024 - take;
            double e0 = 0, e1 = 0;
            AudioListener.GetOutputData(block, 0); for (int i = skip; i < 1024; i++) e0 += block[i] * block[i];
            AudioListener.GetOutputData(block, 1); for (int i = skip; i < 1024; i++) e1 += block[i] * block[i];
            env.Add((Time.unscaledTime - phaseAt).ToString("F4") + "," + Math.Sqrt(e0 / take).ToString("E4") + "," + Math.Sqrt(e1 / take).ToString("E4"));
            // keep the click at the listener's side (the camera moves with the bike)
            var l = FindAnyObjectByType<AudioListener>();
            if (l) click.transform.position = l.transform.position + l.transform.right * 3f;
            float t = Time.unscaledTime - phaseAt;
            if (t >= 0.5f && !click.isPlaying && !fired) { click.Play(); fired = true; firedAt = Time.unscaledTime; Debug.Log(Tag + "CLICK " + Names[phase] + " current " + audio.Reverb.Current); }
            if (fired && !logged && Time.unscaledTime - firedAt > 0.1f) { logged = true; Debug.Log(Tag + "PLAYING " + click.isPlaying + " virtual " + click.isVirtual + " blend " + click.spatialBlend + " bypass " + click.bypassReverbZones + " pos " + click.transform.position + " listener " + FindAnyObjectByType<AudioListener>().transform.position + " pause " + AudioListener.pause); }
            if (t >= 5f) { recorder.on = false; recorder.Save(Path.Combine(directory, Names[phase] + ".wav")); File.WriteAllLines(Path.Combine(directory, Names[phase] + ".csv"), env); Debug.Log(Tag + "SAVED " + Names[phase] + " reverb " + audio.Reverb.Current + " weight " + audio.Reverb.LastTotalWeight.ToString("F2")); NextPhase(); }
        }
    }

    bool fired, logged; float firedAt; readonly float[] block = new float[1024]; readonly System.Collections.Generic.List<string> env = new System.Collections.Generic.List<string>();
    void NextPhase()
    {
        phase++; fired = false; logged = false;
        if (phase >= Names.Length) { done = true; Debug.Log(Tag + "DONE"); return; }
        OriginalReverb.EnabledOverride = phase != 0;
        audio.firstPersonOverride = phase == 2;
        click.spatialBlend = phase == 3 ? 0f : 1f; OriginalReverb.Route(click, phase != 3);
        recorder.Reset(); recorder.on = true; phaseAt = Time.unscaledTime; env.Clear();
    }
}
