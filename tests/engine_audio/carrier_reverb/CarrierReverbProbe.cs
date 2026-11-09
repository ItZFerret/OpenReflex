using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

// Test only (tests/engine_audio/carrier_reverb, w-fpaudio): why an engine pack under the first-person camera's reverb went quiet. Unity sends a source to a
// reverb zone from its clip, BEFORE OnAudioFilterRead; the pack's clip is a constant-1 carrier (EnginePackPlayer), so the zone reverberated a DC level.
// Plays a two-tone signal through (a) an ordinary clip source ("clip": what the original engine events are), (b) the pack's way ("carrier": a constant-1
// clip multiplied in OnAudioFilterRead), (c) the fix ("carrier+filter": the carrier source bypasses the zones and an AudioReverbFilter after the
// filter carries the zone's reverb), each with no reverb and with the zone's reverb (the first-person camera's = FMOD_PRESET_UNDERWATER's numbers, and the
// Mountains / Plain ambients), 3 ft below and to the side of the listener. Logs "PROBE <case> rms ..." of the master output (AudioListener.GetOutputData
// sees the zone's output). The signal is 0.066 peak: quiet on the speakers. Boots from RuntimeInitializeOnLoadMethod in a scratch player (run.sh).
public sealed class CarrierReverbSignal : MonoBehaviour
{
    public volatile float gain = 0.1f; double ph1, ph2;
    void OnAudioFilterRead(float[] d, int ch)
    {
        double r = AudioSettings.outputSampleRate;
        for (int i = 0; i < d.Length; i += ch)
        {
            float y = gain * (float)(Math.Sin(ph1) + 0.5 * Math.Sin(ph2)) * 0.66f;
            ph1 += 2 * Math.PI * 220 / r; ph2 += 2 * Math.PI * 331 / r;
            for (int c = 0; c < ch; c++) d[i + c] *= y;
        }
    }
}

public sealed class CarrierReverbProbe : MonoBehaviour
{
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot() { new GameObject("carrier reverb probe").AddComponent<CarrierReverbProbe>(); }

    // room, roomHF, roomLF, decay, decayHF, reflections, reflectionsDelay, reverb, reverbDelay, diffusion, density (OriginalReverb's Props, mB / s / %)
    static readonly Dictionary<string, float[]> Presets = new Dictionary<string, float[]>
    {
        { "firstperson", new float[] { -1000, -4000, 0, 1.49f, 0.1f, -449, 0.007f, 1700, 0.011f, 100, 100 } },
        { "mountains", new float[] { -1000, -2500, 0, 1.49f, 0.21f, -2780, 0.3f, -1434, 0.1f, 27, 100 } },
        { "plain", new float[] { -1000, -200, 0, 1.49f, 0.5f, -1500, 0.179f, -700, 0.1f, 21, 100 } },
    };

    void Start() { StartCoroutine(Run()); }

    IEnumerator Run()
    {
        new GameObject("listener").AddComponent<AudioListener>();
        int rate = AudioSettings.outputSampleRate;
        var sine = AudioClip.Create("signal", rate, 1, rate, false);
        var d = new float[rate]; for (int i = 0; i < rate; i++) d[i] = 0.1f * (float)(Math.Sin(2 * Math.PI * 220.0 * i / rate) + 0.5 * Math.Sin(2 * Math.PI * 331.0 * i / rate)) * 0.66f;
        sine.SetData(d, 0);
        var ones = AudioClip.Create("carrier", rate, 1, rate, false);
        var o = new float[rate]; for (int i = 0; i < rate; i++) o[i] = 1f; ones.SetData(o, 0);

        var go = new GameObject("source"); go.transform.position = new Vector3(1.5f, -2.6f, 0f);   // 3 ft away, below and to the right
        var src = go.AddComponent<AudioSource>(); var signal = go.AddComponent<CarrierReverbSignal>(); var filter = go.AddComponent<AudioReverbFilter>(); filter.enabled = false;
        src.loop = true; src.spatialBlend = 1f; src.dopplerLevel = 0f; src.rolloffMode = AudioRolloffMode.Linear; src.minDistance = 9.84f; src.maxDistance = 300f;
        var zone = new GameObject("zone").AddComponent<AudioReverbZone>(); zone.minDistance = 100000f; zone.maxDistance = 100001f; zone.reverbPreset = AudioReverbPreset.User; zone.enabled = false;
        yield return new WaitForSeconds(1f);

        var lines = new List<string>(); var rms = new Dictionary<string, double>();
        foreach (string preset in new[] { "", "firstperson", "mountains", "plain" })
            foreach (string way in new[] { "clip", "carrier", "carrier+filter" })
            {
                src.Stop(); src.clip = way == "clip" ? sine : ones; signal.enabled = way != "clip";
                bool fix = way == "carrier+filter", reverbOn = preset != "";
                src.bypassReverbZones = fix; src.reverbZoneMix = 1f;
                var pr = reverbOn ? Presets[preset] : null;
                zone.enabled = reverbOn;   // the zone follows the listener-sized map: on for everything that does not bypass it
                filter.enabled = fix && reverbOn;
                if (reverbOn)
                {
                    zone.room = (int)pr[0]; zone.roomHF = (int)pr[1]; zone.roomLF = (int)pr[2]; zone.decayTime = pr[3]; zone.decayHFRatio = pr[4]; zone.reflections = (int)pr[5]; zone.reflectionsDelay = pr[6];
                    zone.reverb = (int)pr[7]; zone.reverbDelay = pr[8]; zone.diffusion = pr[9]; zone.density = pr[10]; zone.HFReference = 5000; zone.LFReference = 250;
                    filter.reverbPreset = AudioReverbPreset.User; filter.dryLevel = 0f; filter.room = pr[0]; filter.roomHF = pr[1]; filter.roomLF = pr[2]; filter.decayTime = pr[3]; filter.decayHFRatio = pr[4];
                    filter.reflectionsLevel = pr[5]; filter.reflectionsDelay = pr[6]; filter.reverbLevel = pr[7]; filter.reverbDelay = pr[8]; filter.diffusion = pr[9]; filter.density = pr[10]; filter.hfReference = 5000; filter.lfReference = 250;
                }
                src.Play();
                yield return new WaitForSeconds(2.5f);
                var blk = new float[1024]; double eL = 0, eR = 0; int blocks = 0; float t0 = Time.realtimeSinceStartup;
                while (Time.realtimeSinceStartup - t0 < 1.5f)
                {
                    AudioListener.GetOutputData(blk, 0); for (int i = 0; i < blk.Length; i++) eL += blk[i] * blk[i];
                    AudioListener.GetOutputData(blk, 1); for (int i = 0; i < blk.Length; i++) eR += blk[i] * blk[i];
                    blocks++; yield return null;
                }
                double r = Math.Sqrt((eL + eR) / (2.0 * blocks * blk.Length));
                string key = (preset == "" ? "no reverb" : preset) + " / " + way; rms[key] = r;
                string refKey = (preset == "" ? "no reverb" : preset) + " / clip";
                string line = string.Format("PROBE {0,-30} rms {1:F5}  x{2:F2} of the clip source's", key, r, r / Math.Max(1e-9, rms[refKey]));
                lines.Add(line); Debug.Log(line);
            }
        File.WriteAllLines(Path.Combine(Application.persistentDataPath, "carrier-reverb.txt"), lines);
        Debug.Log("PROBE DONE");
        Application.Quit();
    }
}
