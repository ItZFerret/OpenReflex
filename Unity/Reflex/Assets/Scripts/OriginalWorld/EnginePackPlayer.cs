using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // Plays one vehicle's engine with an MX Bikes engine pack (Content/EngineSoundPacks.cs): the MX Bikes sound layer model on the core's
    // telemetry (rx_session_vehicle_audio: rpm, max / idle rpm, throttle).
    //
    //   rpm mapping   the Reflex engine runs between its idle and its max rpm (MX 250F 1500 .. 14000, 450F 1500 .. 11500; the engine
    //                 step's minimum and sim + 0x5014); the pack between its idle (the off-throttle layer's first sample's RefPitch x
    //                 MaxValue, e.g. 2700 of 14000) and sfx.cfg max_rpm. The fraction of the way from idle to max is kept:
    //                   packRpm = idleP + (rpm - idleR) / (maxR - idleR) x (maxP - idleP),  n = packRpm / MaxValue
    //                 so idle sounds like the pack's idle and the Reflex limiter like the pack's limiter, whatever the two engines' ranges.
    //   samples       every sample of both layers loops; its volume = the piecewise-linear Points over n (clamped to the end points), its
    //                 pitch (AutoPitch) = n / RefPitch clamped to [MinPitch, MaxPitch] (an AudioSource pitch is a playback rate ratio,
    //                 as MX Bikes' pitch is).
    //   layers        on = max(throttle, idlethrottle) ^ blend_exp, Layer0 x on, Layer1 x (1 - on).
    //   limiter       the limiter wav loops at its sfx.cfg volume only while the engine bounces on its limiter: on the ground, rpm at
    //                 >= LimiterOnFraction of the Reflex max rpm (the core clamps rpm there) with the throttle >= LimiterMinThrottle, held
    //                 for LimiterHoldSeconds; it stays on until the rpm falls below LimiterOffFraction or the throttle below
    //                 LimiterReleaseThrottle (hysteresis), then fades out over LimiterReleaseSeconds. In the air the Reflex engine
    //                 free-revs to its max (the original plays its PanicLoop there, not a limiter): no limiter (LimiterInAir).
    //                 (w-enginepacks: it used to start at 98.5 % with the throttle half open, in the air too, and play at least a whole
    //                 0.5 .. 1 s cycle each time: 17 .. 52 starts a minute for a player holding the throttle, audible 16 .. 39 % of the time.)
    //   loudness      (w-packloud) the Engine channel x the pack's loudness gain x pack.ini trim (default 1). The loudness gain is measured
    //                 when the pack loads (EnginePackLoudness, on the decode task): the A-weighted mean square of every sample at every
    //                 pitch (from its power spectrum), summed over a reference ride (RefRide: the share of race time the computer riders of
    //                 a NAT race spend at each rpm fraction / throttle) through the pack's own volume / pitch curves and layer blend; the
    //                 gain puts that at TargetALevelFor(engine size), the original Reflex engine's A-weighted level on the same ride for the
    //                 engine the pack replaces (the original 250F plays 3.7 dB under its 450F). So every pack plays at the original engine's
    //                 perceived loudness, whatever its mastering and spectrum (bass-heavy packs on a 450F: 0.66 .. 1.15;
    //                 w-enginepacks' one MixGain 0.30 left them 7 .. 12 dB under the original). pack.ini gain (w-enginepacks: a multiplier of
    //                 MixGain, a pack's own calibration) is used only when the measurement fails, so a calibrated pack is not boosted twice;
    //                 pack.ini trim is the knob on top of the measurement.
    //   mixing        (w-packpeak) the voices are mixed by EnginePackMixer, not by one AudioSource each: 4-point Hermite resampling of every
    //                 sample (its own rate to the output rate x pitch), volume / pitch ramps per buffer, into one mono sum that one AudioSource
    //                 (a constant-1 carrier, 3D as before) multiplies in EnginePackFilter.OnAudioFilterRead. The sum goes through
    //                 EnginePackLimiter: a 3 ms lookahead peak limiter at -1 dBFS with a 100 ms release, on the real mixed signal (the Engine
    //                 channel included): it acts only when the mix really peaks over the ceiling. (w-packloud held a worst-case bound,
    //                 sum(volume x sample peak), under the ceiling: engaged 59 .. 89 % of frames, a compressor that held punchy packs
    //                 2 .. 5 dB under their target.) Voice volumes are no longer clamped at 1 (an AudioSource limit).
    //   3D            linear rolloff from min_distance (metres -> game feet, x 3.28) to 300 (the original engine events' max distance), no
    //                 doppler (the game passes no event velocity), the Engine volume channel (MXRegistry MasterVolume x ENGINE VOLUME).
    //   reverb        (w-fpaudio) NOT through the reverb zone: Unity sends a source to the zone from its clip, before OnAudioFilterRead, and this
    //                 source's clip is the constant-1 carrier, so the zone would reverberate a DC level (measured: the master output at rms 2.56
    //                 under the first-person camera's reverb, 41 x a real signal's 0.062 -- the output clips and the engine and everything else
    //                 with it go quiet and distorted; 4.6 x the signal in a Mountains level, 12 x on a Plain). The source bypasses the zones and an
    //                 AudioReverbFilter after EnginePackFilter (so it reverberates the mix) carries the same morphed reverb the zone carries
    //                 (OriginalReverb.Current): measured rms 0.06177 against 0.06182 for an ordinary source in the zone
    //                 (tests/engine_audio/carrier_reverb).
    //   quality       the WAVs are read as PCM at their own rate (44.1 / 48 / 96 kHz, mono) and mixed as floats: no import, no re-encoding,
    //                 no filter or mixer group (measured: the mix keeps the raw samples' energy above 4 kHz).
    public sealed class EnginePackPlayer
    {
        // ---- the decoded samples per pack, shared by every player of the pack and kept for the session ----
        sealed class Loaded { public Dictionary<string, EngineWav.Data> data = new Dictionary<string, EngineWav.Data>(); public float level; }
        sealed class Bank { public Task<Loaded> load; public Dictionary<string, EngineWav.Data> data; public float level; public string error; }
        static readonly Dictionary<string, Bank> banks = new Dictionary<string, Bank>();
        static Bank BankOf(EnginePack p)
        {
            if (banks.TryGetValue(p.id, out var b)) return b;
            var files = p.layers.SelectMany(l => l).Select(s => s.wav).Append(p.limiterWav).Where(f => !string.IsNullOrEmpty(f)).Distinct().ToList();
            b = new Bank { load = Task.Run(() =>
            {
                var r = new Loaded();
                foreach (var f in files) r.data[f] = EngineWav.ReadMono(f);
                try { r.level = EnginePackLoudness.Level(p, r.data); }
                catch (Exception e) { Debug.LogWarning("[EngineSounds] loudness of " + p.name + ": " + e.Message); }
                return r;
            }) };
            banks[p.id] = b;
            return b;
        }
        static bool Ready(Bank b)
        {
            if (b.data != null) return true;
            if (b.error != null || !b.load.IsCompleted) return false;
            if (b.load.IsFaulted) { b.error = b.load.Exception?.GetBaseException().Message; Debug.LogWarning("[EngineSounds] " + b.error); return false; }
            b.data = b.load.Result.data; b.level = b.load.Result.level;
            Debug.Log("[EngineSounds] " + b.data.Count + " samples of " + b.data.Values.FirstOrDefault()?.name + "..: A-weighted level " + b.level.ToString("F4") + " at gain 1 (450F loudness gain " + (b.level > 0f ? TargetALevel / b.level : 0f).ToString("F3") + ")");
            return true;
        }
        /// Starts decoding a pack's samples (the race start calls it for the packs its riders use).
        public static void Preload(EnginePack p) { if (p != null) BankOf(p); }

        public const float FeetPerMetre = 3.28084f, MaxDistance = 300f;
        /// The pack mix's gain when its loudness cannot be measured (w-enginepacks' one gain for every pack; see the header).
        public const float MixGain = 0.30f;
        /// The original Reflex engine's A-weighted rms on the reference ride, before the Engine channel and the pan, per engine size (rx_vehicle_audio
        /// engine_size, the VehicleEngineSize index): the original emitter (engine_logic) on the same ride, race 25 .. 55 s, landing_mix.py's
        /// method (the local 450F engine alone 0.1057 with the channel 0.7 and the centred pan 0.7071 = 0.2136). 2 = MX 250 two-stroke
        /// (MX_250R_TEST_05) 0.1772, 3 = MX 250F (MX_250F_01) 0.1389, 4 = MX 450F (MX_450F_01) 0.2136; any other size: the 450F's.
        public const float TargetALevel = 0.2136f;
        public static float TargetALevelFor(int engineSize) => engineSize == 2 ? 0.1772f : engineSize == 3 ? 0.1389f : TargetALevel;
        /// The pack's gain for an engine size: the target over the pack's measured level; MixGain x pack.ini gain if it could not be measured.
        static float LoudnessOf(Bank b, EnginePack p, int engineSize) => b.level > 0f ? Mathf.Clamp(TargetALevelFor(engineSize) / b.level, 0.05f, 4f) : MixGain * (p.gainSet ? p.gain : 1f);
        /// The reference ride (loudness.py REF_RIDE, the same numbers): the share of race frames the 11 computer riders of the NAT_Concord
        /// default event spend in each [Reflex rpm fraction bin i = i / 25][throttle bin j = j / 4].
        public const int RefFractionBins = 26, RefThrottleBins = 5;
        public static float[] RefRideTable => RefRide;
        static readonly float[] RefRide = {
            0.0287f, 0.0041f, 0.0010f, 0.0002f, 0.0056f,
            0.0020f, 0.0002f, 0.0002f, 0.0001f, 0.0015f,
            0.0022f, 0.0006f, 0.0001f, 0.0001f, 0.0013f,
            0.0026f, 0.0006f, 0.0002f, 0.0000f, 0.0011f,
            0.0029f, 0.0006f, 0.0002f, 0.0000f, 0.0010f,
            0.0036f, 0.0007f, 0.0001f, 0.0001f, 0.0013f,
            0.0044f, 0.0006f, 0.0001f, 0.0000f, 0.0011f,
            0.0054f, 0.0006f, 0.0001f, 0.0001f, 0.0010f,
            0.0096f, 0.0007f, 0.0002f, 0.0001f, 0.0015f,
            0.0111f, 0.0008f, 0.0002f, 0.0000f, 0.0012f,
            0.0093f, 0.0106f, 0.0003f, 0.0001f, 0.0015f,
            0.0133f, 0.0107f, 0.0006f, 0.0002f, 0.0019f,
            0.0139f, 0.0159f, 0.0040f, 0.0008f, 0.0042f,
            0.0136f, 0.0101f, 0.0243f, 0.0070f, 0.0096f,
            0.0130f, 0.0062f, 0.0271f, 0.0185f, 0.0285f,
            0.0073f, 0.0024f, 0.0268f, 0.0189f, 0.0466f,
            0.0044f, 0.0023f, 0.0132f, 0.0177f, 0.0557f,
            0.0039f, 0.0033f, 0.0066f, 0.0086f, 0.0520f,
            0.0031f, 0.0036f, 0.0040f, 0.0054f, 0.0526f,
            0.0025f, 0.0038f, 0.0031f, 0.0029f, 0.0445f,
            0.0021f, 0.0028f, 0.0046f, 0.0029f, 0.0347f,
            0.0014f, 0.0024f, 0.0026f, 0.0029f, 0.0272f,
            0.0014f, 0.0022f, 0.0009f, 0.0027f, 0.0294f,
            0.0010f, 0.0019f, 0.0006f, 0.0006f, 0.0412f,
            0.0003f, 0.0012f, 0.0003f, 0.0005f, 0.0674f,
            0.0004f, 0.0001f, 0.0003f, 0.0005f, 0.0221f,
        };
        // the limiter (see the header); fractions are of the Reflex engine's max rpm (rx_vehicle_audio max_rpm)
        public const float LimiterOnFraction = 0.995f, LimiterOffFraction = 0.97f;
        public const float LimiterMinThrottle = 0.9f, LimiterReleaseThrottle = 0.6f;
        public const float LimiterHoldSeconds = 0.25f, LimiterReleaseSeconds = 0.08f;
        public static readonly bool LimiterInAir = false;   // a field, not a const: no unreachable-code warning
        readonly EnginePack pack;
        readonly GameObject node;
        readonly List<(EnginePack.Sample s, int layer)> voices = new List<(EnginePack.Sample, int)>();
        EnginePackMixer mixer;
        AudioSource output;
        readonly OriginalReverb reverb;
        AudioReverbFilter reverbFilter;
        OriginalReverb.Props reverbApplied; bool reverbHave;
        int limiterVoice = -1;
        bool built, atLimiter;
        float rpmNorm = -1f, throttle, limiterHeld, limiterGain;
        public EnginePack Pack => pack;
        /// w-packrpm: this rider's audio rpm / load model (OriginalVehicleSound steps it and passes its output to Update).
        public readonly EnginePackRpm AudioRpm = new EnginePackRpm();
        /// Diagnostics: the last frame's normalized pack rpm, the throttle mix, and each voice's (layer, file, volume, pitch).
        public float LastNorm => rpmNorm;
        public float LastOn { get; private set; }
        public bool LimiterOn => atLimiter;
        /// The pack's loudness gain (1 before its samples are decoded) and the output limiter's deepest gain reduction since the last frame (dB, <= 0).
        public float LoudnessGain { get; private set; } = 1f;
        public float LastReductionDb => mixer != null ? mixer.TakeReductionDb() : 0f;
        /// The output limiter's statistics: the share of mixed buffers with any gain reduction, the deepest (dB).
        public (double reducedShare, float maxReductionDb) LimiterStats => mixer != null ? mixer.Stats : (0.0, 0f);
        public IEnumerable<(int layer, string file, float volume, float pitch)> Voices()
        {
            for (int i = 0; i < voices.Count; i++) { var t = mixer != null ? mixer.Target(i) : (0f, 1f); yield return (voices[i].layer, System.IO.Path.GetFileName(voices[i].s.wav), t.Item1, t.Item2); }
        }

        /// reverb: the system reverb the pack stands in the 3D engine events' place for (OriginalAudio.Reverb); null = dry.
        public EnginePackPlayer(EnginePack pack, Transform parent, OriginalReverb reverb = null)
        {
            this.pack = pack; this.reverb = reverb;
            node = new GameObject("engine pack " + pack.name); node.transform.SetParent(parent, false);
            BankOf(pack);
        }

        static AudioClip ones;
        /// The output source plays a constant 1 (spatialised as the pack's voices used to be: linear rolloff, no doppler); EnginePackFilter
        /// multiplies it by the mixer's mono output, so the 3D pan and rolloff apply whichever side of the filter Unity applies them.
        static AudioClip Ones()
        {
            if (ones) return ones;
            int rate = Math.Max(8000, AudioSettings.outputSampleRate), n = rate;
            ones = AudioClip.Create("engine pack carrier", n, 1, rate, false);
            var d = new float[n]; for (int i = 0; i < n; i++) d[i] = 1f;
            ones.SetData(d, 0);
            return ones;
        }

        void Build(Bank b)
        {
            built = true;
            var list = new List<(float[] data, int rate, bool loop)>();
            for (int l = 0; l < 2; l++)
                foreach (var s in pack.layers[l])
                    if (b.data.TryGetValue(s.wav, out var d)) { voices.Add((s, l)); list.Add((d.samples, d.rate, true)); }
            if (!string.IsNullOrEmpty(pack.limiterWav) && b.data.TryGetValue(pack.limiterWav, out var ld)) { limiterVoice = list.Count; list.Add((ld.samples, ld.rate, true)); }
            mixer = new EnginePackMixer(list, AudioSettings.outputSampleRate, new System.Random(pack.id.GetHashCode()));
            var go = new GameObject("pack output"); go.transform.SetParent(node.transform, false);
            output = go.AddComponent<AudioSource>();
            output.clip = Ones(); output.loop = true; output.playOnAwake = false; output.dopplerLevel = 0f; output.spatialBlend = 1f;
            output.rolloffMode = AudioRolloffMode.Linear; output.minDistance = Mathf.Max(0.5f, pack.minDistance * FeetPerMetre); output.maxDistance = MaxDistance;
            output.volume = 1f; output.priority = 64;
            // w-fpaudio: the pack stands in for the 3D engine events, which the original sends to the system reverb (w-reverb), but not through the
            // zone: the zone's send is taken from the clip (the constant-1 carrier) before EnginePackFilter, see the header. The reverb filter after
            // the mixer's filter gets the real mix; it is added second, so it runs second.
            output.bypassReverbZones = true;
            go.AddComponent<EnginePackFilter>().mixer = mixer;
            if (reverb != null) { reverbFilter = go.AddComponent<AudioReverbFilter>(); reverbFilter.enabled = false; }
            output.Play();
        }

        /// The morphed system reverb (OriginalReverb.Current: the level's ambient, zones and the first-person camera's) on the pack's own reverb filter.
        void ApplyReverb()
        {
            if (!reverbFilter) return;
            var p = reverb.Current;
            if (p.IsOff) { if (reverbFilter.enabled) reverbFilter.enabled = false; reverbHave = false; return; }
            if (!reverbFilter.enabled) reverbFilter.enabled = true;
            if (reverbHave && OriginalReverb.Near(p, reverbApplied)) return;
            reverbApplied = p; reverbHave = true;
            reverbFilter.reverbPreset = AudioReverbPreset.User;
            reverbFilter.dryLevel = 0f;
            reverbFilter.room = Mathf.RoundToInt(p.room); reverbFilter.roomHF = Mathf.RoundToInt(p.roomHF); reverbFilter.roomLF = Mathf.RoundToInt(p.roomLF);
            reverbFilter.decayTime = p.decay; reverbFilter.decayHFRatio = p.decayHF;
            reverbFilter.reflectionsLevel = Mathf.RoundToInt(p.refl); reverbFilter.reflectionsDelay = p.reflDelay;
            reverbFilter.reverbLevel = Mathf.RoundToInt(p.rev); reverbFilter.reverbDelay = p.revDelay;
            reverbFilter.hfReference = p.hfRef; reverbFilter.lfReference = p.lfRef; reverbFilter.diffusion = p.diffusion; reverbFilter.density = p.density;
        }

        /// One render frame: the rider's telemetry (rpm, the engine's idle and max rpm, throttle, airborne, the engine size the pack stands in for),
        /// the position, the Engine channel gain.
        /// audioRpm / audioThrottle (w-packrpm, EnginePackRpm; -1 = the true values): what the voices follow; the limiter keeps the true rpm / throttle.
        public void Update(float rpm, float idleRpm, float maxRpm, float throttleIn, bool airborne, int engineSize, Vector3 position, float channelGain, float dt,
                           float audioRpm = -1f, float audioThrottle = -1f)
        {
            node.transform.position = position;
            var b = BankOf(pack);
            if (!Ready(b)) return;
            if (!built) Build(b);
            ApplyReverb();
            if (maxRpm <= 0f) maxRpm = pack.maxRpm;
            if (idleRpm <= 0f || idleRpm >= maxRpm) idleRpm = maxRpm * 0.12f;
            float frac = Mathf.Clamp01(((audioRpm >= 0f ? audioRpm : rpm) - idleRpm) / (maxRpm - idleRpm));
            float idleP = pack.IdleNorm * pack.maxValue, maxP = Mathf.Max(idleP + 1f, pack.maxRpm);
            float n = Mathf.Clamp01((idleP + frac * (maxP - idleP)) / pack.maxValue);
            // the core's rpm moves per game frame (60 Hz): ease it per render frame so the pitch does not step
            rpmNorm = rpmNorm < 0f ? n : Mathf.Lerp(rpmNorm, n, 1f - Mathf.Exp(-dt * 30f));
            throttle = Mathf.Lerp(throttle, Mathf.Clamp01(audioThrottle >= 0f ? audioThrottle : throttleIn), 1f - Mathf.Exp(-dt * 20f));
            float on = Mathf.Pow(Mathf.Max(throttle, pack.idleThrottle), Mathf.Max(0.01f, pack.blendExp));
            LastOn = on;
            LoudnessGain = LoudnessOf(b, pack, engineSize);
            float g = channelGain * LoudnessGain * pack.trim;
            float limiterGainBefore = limiterGain;
            // the limiter: on the ground, at the clamp, throttle held, for LimiterHoldSeconds; hysteresis out; a short fade
            float q = rpm / maxRpm, thr = Mathf.Clamp01(throttleIn);
            bool grounded = LimiterInAir || !airborne;
            bool limit;
            if (atLimiter) { limit = grounded && q >= LimiterOffFraction && thr >= LimiterReleaseThrottle; if (!limit) limiterHeld = 0f; }
            else
            {
                bool at = grounded && q >= LimiterOnFraction && thr >= LimiterMinThrottle;
                limiterHeld = at ? limiterHeld + dt : 0f;
                limit = limiterHeld >= LimiterHoldSeconds;
            }
            limiterGain = limit ? 1f : Mathf.Max(0f, limiterGain - dt / LimiterReleaseSeconds);
            // the voices' targets; the mixer ramps to them over its next buffer and limits the mixed output (EnginePackMixer)
            for (int i = 0; i < voices.Count; i++)
            {
                var (s, layer) = voices[i];
                float mix = layer == 0 ? on : 1f - on;
                // no AudioSource per voice any more: a volume is not clamped at 1 (the limiter holds the sum)
                mixer.SetTarget(i, g * mix * s.VolumeAt(rpmNorm), Mathf.Clamp(s.PitchAt(rpmNorm, pack.autoPitch), 0.05f, 3f), true);
            }
            if (limiterVoice >= 0)
            {
                bool play = limit || limiterGain > 0f;
                mixer.SetTarget(limiterVoice, g * pack.limiterVolume * limiterGain, 1f, play, restart: limit && !atLimiter && limiterGainBefore <= 0f);   // still fading out: carries on
            }
            mixer.Commit();
            atLimiter = limit;
        }

        public void Destroy() { if (node) UnityEngine.Object.Destroy(node); voices.Clear(); mixer = null; output = null; }
    }

    /// The pack's own mixer (w-packpeak), run on the audio thread by EnginePackFilter: every voice resampled with 4-point Hermite
    /// interpolation into one mono sum (volume and pitch ramp linearly to the targets the main thread set, over each buffer), then
    /// EnginePackLimiter on that sum. The Engine channel gain is in the voices' volumes, so the limiter's ceiling is the output's.
    /// Main thread: SetTarget for every voice, then Commit; audio thread: Mix. tests/engine_audio/offline/packmix.py is the same in Python.
    public sealed class EnginePackMixer
    {
        sealed class Voice { public float[] d; public double step0, pos; public bool loop, playing, play; public float vol, pitch = 1f, tv, tp = 1f; }
        readonly Voice[] v;
        readonly float[] tVol, tPitch, pVol, pPitch; readonly bool[] tPlay, pPlay, pRestart, tRestart;
        readonly object gate = new object();
        float[] mono = new float[0];
        readonly EnginePackLimiter limiter;
        long buffers, reducedBuffers; float worstDb, sinceDb;

        public EnginePackMixer(List<(float[] data, int rate, bool loop)> voices, int outputRate, System.Random random)
        {
            int n = voices.Count; outputRate = Math.Max(8000, outputRate);
            v = new Voice[n]; tVol = new float[n]; tPitch = new float[n]; pVol = new float[n]; pPitch = new float[n]; tPlay = new bool[n]; pPlay = new bool[n]; pRestart = new bool[n]; tRestart = new bool[n];
            for (int i = 0; i < n; i++)
            {
                var (d, rate, loop) = voices[i];
                v[i] = new Voice { d = d.Length >= 4 ? d : new float[4], step0 = rate / (double)outputRate, loop = loop };   // the 4-point kernel needs 4 samples
                v[i].pos = random.Next(0, Math.Max(1, v[i].d.Length - 1));   // the loops do not phase together
                tPitch[i] = pPitch[i] = 1f;
            }
            limiter = new EnginePackLimiter(outputRate);
        }

        public void SetTarget(int i, float volume, float pitch, bool play, bool restart = false) { tVol[i] = volume; tPitch[i] = pitch; tPlay[i] = play; tRestart[i] |= restart; }
        public void Commit()
        {
            lock (gate)
            {
                Array.Copy(tVol, pVol, tVol.Length); Array.Copy(tPitch, pPitch, tPitch.Length); Array.Copy(tPlay, pPlay, tPlay.Length);
                for (int i = 0; i < tRestart.Length; i++) { pRestart[i] |= tRestart[i]; tRestart[i] = false; }
            }
        }
        public (float, float) Target(int i) => (tVol[i], tPitch[i]);
        public (double, float) Stats { get { lock (gate) return (buffers > 0 ? reducedBuffers / (double)buffers : 0.0, worstDb); } }
        public float TakeReductionDb() { lock (gate) { float r = sinceDb; sinceDb = 0f; return r; } }

        /// Audio thread: data holds the carrier (the spatialised constant 1) for `channels` interleaved channels; multiplies it by the mix.
        public void Mix(float[] data, int channels)
        {
            int frames = data.Length / Math.Max(1, channels);
            if (mono.Length < frames) mono = new float[frames];
            Array.Clear(mono, 0, frames);
            lock (gate)
            {
                for (int i = 0; i < v.Length; i++)
                {
                    var x = v[i]; x.play = pPlay[i]; x.tv = pVol[i]; x.tp = pPitch[i];
                    if (pRestart[i]) { x.pos = 0; pRestart[i] = false; x.vol = 0f; }
                }
            }
            for (int i = 0; i < v.Length; i++)
            {
                var x = v[i];
                float vol1 = x.tv, pit1 = x.tp;
                if (!x.play) { x.playing = false; x.vol = 0f; x.pitch = pit1; continue; }
                if (!x.playing) { x.playing = true; x.vol = 0f; }
                float vol0 = x.vol, pit0 = x.pitch;
                if (vol0 <= 0f && vol1 <= 0f) { x.pos = Advance(x, frames, pit0, pit1); x.pitch = pit1; continue; }
                var d = x.d; int n = d.Length; double pos = x.pos;
                for (int k = 0; k < frames; k++)
                {
                    float u = (k + 0.5f) / frames, vol = vol0 + (vol1 - vol0) * u, pit = pit0 + (pit1 - pit0) * u;
                    int i0 = (int)pos; float t = (float)(pos - i0);
                    float xm1 = d[i0 == 0 ? n - 1 : i0 - 1], x0 = d[i0], x1 = d[i0 + 1 < n ? i0 + 1 : i0 + 1 - n], x2 = d[i0 + 2 < n ? i0 + 2 : i0 + 2 - n];
                    float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
                    mono[k] += vol * (((c3 * t + c2) * t + c1) * t + x0);
                    pos += pit * x.step0;
                    if (pos >= n) pos -= n;
                }
                x.pos = pos; x.vol = vol1; x.pitch = pit1;
            }
            float minGain = limiter.Process(mono, frames);
            float db = minGain < 1f ? 20f * (float)Math.Log10(Math.Max(1e-9f, minGain)) : 0f;
            lock (gate) { buffers++; if (db < -0.01f) reducedBuffers++; if (db < worstDb) worstDb = db; if (db < sinceDb) sinceDb = db; }
            for (int k = 0, o = 0; k < frames; k++) { float y = mono[k]; for (int c = 0; c < channels; c++, o++) data[o] *= y; }
        }
        static double Advance(Voice x, int frames, float p0, float p1) { double pos = x.pos + 0.5 * (p0 + p1) * x.step0 * frames; int n = x.d.Length; return pos % n; }
    }

    /// A lookahead peak limiter (w-packpeak) on a mono stream: the gain needed per sample g = min(1, Ceiling / |x|), its minimum over the
    /// Lookahead window (the current and the next L samples), an exponential ReleaseSeconds release, then a box average over the same L + 1
    /// samples; the signal is delayed by L. The box ends at the minimum just as the sample that needed it comes out, so |output| <= Ceiling
    /// (sample peaks; -1 dBFS leaves room for the inter-sample ones). The attack is the L-sample ramp; nothing happens below the ceiling.
    public sealed class EnginePackLimiter
    {
        public const float Ceiling = 0.891f;            // -1 dBFS
        public const float LookaheadSeconds = 0.003f;   // 3 ms (144 samples at 48 kHz)
        public const float ReleaseSeconds = 0.10f;
        readonly int L; readonly float a;
        readonly float[] delay; int dpos;
        readonly float[] box; int bpos; double bsum;
        readonly float[] qv; readonly long[] qi; int qh, qn; long t;   // the sliding minimum (a monotonic queue, ring of L + 1)
        float cur = 1f;
        public EnginePackLimiter(int rate)
        {
            L = Math.Max(1, (int)Math.Round(LookaheadSeconds * rate)); a = 1f - (float)Math.Exp(-1.0 / (ReleaseSeconds * rate));
            delay = new float[L]; box = new float[L + 1]; for (int i = 0; i <= L; i++) box[i] = 1f; bsum = L + 1;
            qv = new float[L + 1]; qi = new long[L + 1];
        }
        /// Limits x[0 .. n) in place; returns the smallest gain applied.
        public float Process(float[] x, int n)
        {
            float minGain = 1f;
            for (int k = 0; k < n; k++)
            {
                float s = x[k], m = Math.Abs(s), g = m > Ceiling ? Ceiling / m : 1f;
                // sliding minimum of g over [t - L, t]
                while (qn > 0 && qv[(qh + qn - 1) % (L + 1)] >= g) qn--;
                qv[(qh + qn) % (L + 1)] = g; qi[(qh + qn) % (L + 1)] = t; qn++;
                while (qi[qh] <= t - (L + 1)) { qh = (qh + 1) % (L + 1); qn--; }
                float mn = qv[qh];
                cur = mn < cur ? mn : cur + (mn - cur) * a;
                bsum += cur - box[bpos]; box[bpos] = cur; bpos = bpos == L ? 0 : bpos + 1;
                float e = (float)(bsum / (L + 1));
                float xd = delay[dpos]; delay[dpos] = s; dpos = dpos == L - 1 ? 0 : dpos + 1;
                x[k] = xd * e; if (e < minGain) minGain = e;
                t++;
            }
            bsum = 0; for (int i = 0; i <= L; i++) bsum += box[i];   // no drift
            return minGain;
        }
    }

    /// The load-time loudness estimate of an engine pack (w-packloud; tests/engine_audio/offline/loudness.py is the same in Python): the
    /// A-weighted rms the pack plays at, at gain 1, over EnginePackPlayer.RefRide. Each sample's power spectrum (Welch, 4096-point Hann,
    /// half overlap, its first MaxSeconds) gives its A-weighted mean square at any pitch (playing at pitch p moves every bin to f x p and
    /// keeps the mean square); the ride sums volume^2 x that over every sample of both layers (the loops are uncorrelated: powers add).
    public static class EnginePackLoudness
    {
        public const int Fft = 4096; public const float MaxSeconds = 10f;
        sealed class Spec { public double[] p; public double binHz, ms, total; }

        public static float Level(EnginePack pack, Dictionary<string, EngineWav.Data> data)
        {
            var specs = new Dictionary<string, Spec>();
            foreach (var l in pack.layers) foreach (var s in l) if (!specs.ContainsKey(s.wav) && data.TryGetValue(s.wav, out var d)) specs[s.wav] = Spectrum(d.samples, d.rate);
            var ride = EnginePackPlayer.RefRideTable;
            int nf = EnginePackPlayer.RefFractionBins, nt = EnginePackPlayer.RefThrottleBins;
            float idleP = pack.IdleNorm * pack.maxValue, maxP = Mathf.Max(idleP + 1f, pack.maxRpm);
            double acc = 0;
            for (int i = 0; i < nf; i++)
            {
                float frac = i / (float)(nf - 1), n = Mathf.Clamp01((idleP + frac * (maxP - idleP)) / pack.maxValue);
                for (int j = 0; j < nt; j++)
                {
                    float w = ride[i * nt + j];
                    if (w <= 0f) continue;
                    float on = Mathf.Pow(Mathf.Max(j / (float)(nt - 1), pack.idleThrottle), Mathf.Max(0.01f, pack.blendExp));
                    for (int l = 0; l < 2; l++)
                        foreach (var s in pack.layers[l])
                        {
                            float v = (l == 0 ? on : 1f - on) * s.VolumeAt(n);
                            if (v <= 0f || !specs.TryGetValue(s.wav, out var sp)) continue;
                            acc += w * v * v * AMeanSquare(sp, Mathf.Clamp(s.PitchAt(n, pack.autoPitch), 0.05f, 3f));
                        }
                }
            }
            return (float)Math.Sqrt(acc);
        }

        static double AMeanSquare(Spec s, double pitch)
        {
            if (s.total <= 0) return 0;
            double a = 0;
            for (int i = 1; i < s.p.Length; i++) { double r = AResponse(i * s.binHz * pitch); a += s.p[i] * r * r; }
            return s.ms * a / s.total;
        }

        /// IEC 61672 A-weighting magnitude, 1 at 1 kHz.
        public static double AResponse(double f)
        {
            double f2 = f * f;
            double ra = (12194.0 * 12194.0 * f2 * f2) / ((f2 + 20.6 * 20.6) * Math.Sqrt((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9)) * (f2 + 12194.0 * 12194.0));
            return ra / 0.7943;
        }

        static Spec Spectrum(float[] x, int rate)
        {
            int len = Math.Min(x.Length, (int)(MaxSeconds * rate)), n = Fft, half = n / 2;
            var sp = new Spec { p = new double[half + 1], binHz = rate / (double)n };
            double ms = 0; for (int i = 0; i < len; i++) ms += (double)x[i] * x[i];
            sp.ms = len > 0 ? ms / len : 0;
            var win = new double[n]; for (int i = 0; i < n; i++) win[i] = 0.5 - 0.5 * Math.Cos(2 * Math.PI * i / (n - 1));
            var re = new double[n]; var im = new double[n];
            int frames = 0;
            for (int at = 0; frames == 0 || at + n <= len; at += half)
            {
                for (int i = 0; i < n; i++) { re[i] = at + i < len ? x[at + i] * win[i] : 0; im[i] = 0; }
                FftInPlace(re, im);
                for (int k = 0; k <= half; k++) sp.p[k] += re[k] * re[k] + im[k] * im[k];
                frames++;
                if (at + n > len) break;
            }
            for (int k = 1; k <= half; k++) sp.total += sp.p[k];
            return sp;
        }

        static void FftInPlace(double[] re, double[] im)
        {
            int n = re.Length;
            for (int i = 1, j = 0; i < n; i++)
            {
                int bit = n >> 1;
                for (; (j & bit) != 0; bit >>= 1) j ^= bit;
                j ^= bit;
                if (i < j) { (re[i], re[j]) = (re[j], re[i]); (im[i], im[j]) = (im[j], im[i]); }
            }
            for (int size = 2; size <= n; size <<= 1)
            {
                double ang = -2 * Math.PI / size; double wr = Math.Cos(ang), wi = Math.Sin(ang);
                for (int start = 0; start < n; start += size)
                {
                    double cr = 1, ci = 0;
                    for (int k = 0; k < size / 2; k++)
                    {
                        int a = start + k, c = a + size / 2;
                        double tr = re[c] * cr - im[c] * ci, ti = re[c] * ci + im[c] * cr;
                        re[c] = re[a] - tr; im[c] = im[a] - ti; re[a] += tr; im[a] += ti;
                        double nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
                    }
                }
            }
        }
    }
}
