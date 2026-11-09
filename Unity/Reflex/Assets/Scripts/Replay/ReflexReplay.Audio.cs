// The replay's sound (ReflexReplay, w-replayphoto2): what was heard, played again with the pictures.
//
// Recording. Every sound command of the world goes through OriginalAudio (OriginalAudio.CommandIssued): the game's own FMOD event
// commands (the gate drop, the finish, the announcer, the crowd's reactions ...) and the host's sound layer (the original engines of every
// rider, OriginalEngineAudio; the drive-on, landing, collision and foley sounds, OriginalGameSound; the level's crowd emitters and
// ambience, OriginalLevelSound). Each is kept (sub-command, wrapper, path, parameter, value, position) in a ring of its own (AudioRing),
// in order, belonging to the next recorded frame. The engine packs chosen in the garage play outside FMOD (EnginePackPlayer): their
// riders' telemetry (rpm, idle / max rpm, throttle, gear, airborne, engine size, position) is kept per recorded frame.
//
// Playing. While the replay plays forward between 0.75x and 1.5x (REPLAY SOUND on, the REPLAY face), the commands of every frame the
// playback passes are issued again into OriginalAudio on wrappers of their own (OriginalAudio.ReplayWrapperBase: nobody is told they
// finished), so the same events start, follow their parameters, move and stop at the same moments: the engines through the original's
// own events and envelopes, the one-shots where they happened. An event already running when the playback starts (an engine, the crowd)
// starts on its first parameter or position command. The packs run through their own EnginePackPlayer and EnginePackRpm from the
// recorded telemetry. The paused world's own sound is isolated meanwhile (OriginalAudio.SetReplayIsolation: its voices paused where they
// are; the live packs silent), the listener follows the replay camera, and the sound player ticks on real time
// (OriginalWorldRuntime.ViewerAudio). Not pitch-shifted: at a speed outside 0.75 .. 1.5x, in reverse, scrubbing, held, or in PHOTO MODE
// the replay is silent (a seek starts the sound over from there). The music is the live one (it plays on while the replay is heard).
using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.Replay
{
    public struct ReplayAudioCmd
    {
        public int sub; public uint wrapper, serial; public string path, param; public float value; public Vector3 position; public bool hasPosition;
    }

    /// The recorded sound commands, in order.
    public sealed class AudioRing
    {
        ReplayAudioCmd[] data = Array.Empty<ReplayAudioCmd>();
        long written;
        public int Capacity => data.Length;
        public long Written => written;
        /// Bytes a command takes in the ring (the struct; the path / parameter strings are the game's interned ones, shared).
        public const int CommandBytes = 56;
        public long Bytes => data.Length * (long)CommandBytes;
        public void Reset(int commands) { if (data.Length != commands) data = commands > 0 ? new ReplayAudioCmd[commands] : Array.Empty<ReplayAudioCmd>(); written = 0; }
        public void Clear() => written = 0;
        public void Put(in ReplayAudioCmd c) { if (data.Length == 0) return; data[(int)(written % data.Length)] = c; written++; }
        public ref ReplayAudioCmd this[long at] => ref data[(int)(at % data.Length)];
        public bool Valid(long at, long count) => data.Length > 0 && at >= 0 && at >= written - data.Length && at + count <= written;
    }

    public struct ReplayPackSample
    {
        public int slot, gear, engineSize; public float rpm, idleRpm, maxRpm, throttle; public bool airborne; public Vector3 position;
    }

    public sealed partial class ReflexReplay
    {
        /// REPLAY SOUND: the replay plays what was heard (engines, landings, crashes, the gate, the crowd).
        public static bool RecordSound
        {
            get => ReflexSave.GetInt("OpenReflex.ReplaySound", 1) != 0;
            set { ReflexSave.SetInt("OpenReflex.ReplaySound", value ? 1 : 0); ReflexSave.Save(); }
        }
        public const float SoundMinSpeed = 0.75f, SoundMaxSpeed = 1.5f;

        public AudioRing Sound => Rings.sound;
        long soundPending;
        long soundCommands;   // recorded (the probe's rate)

        /// Commands the ring holds: 16 MB a minute of REPLAY LENGTH (1.6x the 10 MB a minute measured on a 12-rider National), 16 .. 64 MB.
        static int SoundCommands => (int)(Math.Clamp((long)(LengthSeconds / 60f * (16L << 20)), 16L << 20, 64L << 20) / AudioRing.CommandBytes);

        void SoundHooks(bool on)
        {
            if (on) OriginalAudio.CommandIssued += OnSoundCommand; else OriginalAudio.CommandIssued -= OnSoundCommand;
        }

        void SoundReset() { Sound.Reset(RecordSound ? SoundCommands : 0); soundPending = 0; soundCommands = 0; }

        void OnSoundCommand(OriginalAudio a, int sub, uint wrapper, uint serial, string path, string param, float value, Vector3 pos, bool hasPos)
        {
            if (Active || !Runtime || a != Runtime.gameAudio || Sound.Capacity == 0) return;
            if (sub == 7 || sub == 8) return;   // (the game's mute / pause of its own events: the world's pause, not a sound to play again)
            Sound.Put(new ReplayAudioCmd { sub = sub, wrapper = wrapper, serial = serial, path = path, param = param, value = value, position = pos, hasPosition = hasPos });
            soundCommands++;
        }

        // the packs' telemetry of the frame (the riders whose engine is a garage pack)
        void PackFill(ReplayFrame f)
        {
            f.packCount = 0;
            var vs = Runtime ? Runtime.VehicleSound : null;
            if (vs == null || !RecordSound) return;
            var st = vs.Last;
            for (int i = 0; i < st.riderCount; i++)
            {
                var r = st.riders[i];
                if (vs.PackPlayerOfSlot(r.slot) == null) continue;
                if (f.packs.Length <= f.packCount) Array.Resize(ref f.packs, f.packCount + 1);
                f.packs[f.packCount++] = new ReplayPackSample
                {
                    slot = r.slot, gear = r.gear, engineSize = r.engineSize, rpm = r.rpm, idleRpm = r.idleRpm, maxRpm = r.maxRpm, throttle = r.throttle,
                    airborne = r.airborne, position = new Vector3(r.px, r.py, r.pz),
                };
            }
        }

        // ------------------------------------------------------------------------------------------- playing

        bool audible, soundResync;
        int soundFrame;
        uint nextWrapper = OriginalAudio.ReplayWrapperBase;
        readonly Dictionary<uint, uint> wrappers = new Dictionary<uint, uint>();
        readonly Dictionary<int, EnginePackPlayer> replayPacks = new Dictionary<int, EnginePackPlayer>();
        long soundReplayed;
        /// The replay's sound is on now (tests).
        public bool Audible => audible;
        /// Commands issued again since the viewer opened (tests).
        public long SoundReplayed => soundReplayed;

        uint NewWrapper()
        {
            uint w = nextWrapper++;
            if (nextWrapper < OriginalAudio.ReplayWrapperBase) nextWrapper = OriginalAudio.ReplayWrapperBase;
            return w;
        }

        void SoundTick(float dt)
        {
            var audio = Runtime ? Runtime.gameAudio : null;
            if (!audio) return;
            bool want = Active && Face == View.Replay && Playing && Speed >= SoundMinSpeed && Speed <= SoundMaxSpeed && !Capturing && Sound.Capacity > 0;
            int idx = Buffer.IndexAt(PlayTime);
            if (want && !audible)
            {
                audible = true;
                audio.SetReplayIsolation(true);
                OriginalVehicleSound.ReplayMuted = true;
                Runtime.ViewerAudio = true;
                AudioListener.pause = false;
                wrappers.Clear(); soundFrame = idx; soundResync = false;
            }
            else if (!want && audible) { SoundStop(); return; }
            if (!audible) return;
            if (soundResync || idx < soundFrame)
            {
                audio.StopReplayInstances(); wrappers.Clear();
                soundFrame = idx; soundResync = false;
            }
            for (int k = soundFrame + 1; k <= idx; k++) ReplayCommands(audio, Buffer[k]);
            soundFrame = idx;
            PackTick(audio, dt);
        }

        void ReplayCommands(OriginalAudio audio, ReplayFrame f)
        {
            if (f.soundCount <= 0 || !Sound.Valid(f.soundStart, f.soundCount)) return;
            for (long i = f.soundStart; i < f.soundStart + f.soundCount; i++)
            {
                ref var c = ref Sound[i];
                bool have = wrappers.TryGetValue(c.wrapper, out uint mine);
                switch (c.sub)
                {
                    case 0:
                        if (!have) { mine = NewWrapper(); wrappers[c.wrapper] = mine; }
                        audio.Host(0, mine, c.serial, c.path, null, 0f, c.position, c.hasPosition);
                        break;
                    case 1: case 12:
                        if (have) { audio.Host(c.sub, mine, c.serial, c.path, null, 0f, Vector3.zero, false); wrappers.Remove(c.wrapper); }
                        break;
                    case 2: case 4: case 5: case 10:
                        if (!have)
                        {
                            // an event that was already running when the sound started: it starts with its first command
                            if (string.IsNullOrEmpty(c.path)) break;
                            mine = NewWrapper(); wrappers[c.wrapper] = mine;
                            audio.Host(0, mine, c.serial, c.path, null, 0f, c.position, c.sub == 5 || c.hasPosition);
                        }
                        audio.Host(c.sub, mine, c.serial, c.path, c.param, c.value, c.position, c.hasPosition);
                        break;
                    case 6: case 13: case 14:
                        if (!have) { mine = NewWrapper(); wrappers[c.wrapper] = mine; }
                        audio.Host(c.sub, mine, c.serial, c.path, c.param, c.value, c.position, c.hasPosition);
                        break;
                }
                soundReplayed++;
            }
        }

        void PackTick(OriginalAudio audio, float dt)
        {
            if (Buffer.Count == 0) return;
            Buffer.Locate(PlayTime, out int a, out int b, out float w);
            var fa = Buffer[a]; var fb = Buffer[b];
            float gain = audio.ChannelGain(ReflexChannel.Engine);
            var vs = Runtime.VehicleSound;
            for (int i = 0; i < fa.packCount; i++)
            {
                var s = fa.packs[i];
                ReplayPackSample t = s;
                for (int j = 0; j < fb.packCount; j++) if (fb.packs[j].slot == s.slot) { t = fb.packs[j]; break; }
                if (!replayPacks.TryGetValue(s.slot, out var player))
                {
                    var live = vs != null ? vs.PackPlayerOfSlot(s.slot) : null;
                    if (live == null || live.Pack == null) continue;
                    player = new EnginePackPlayer(live.Pack, audio.transform, audio.Reverb);
                    replayPacks[s.slot] = player;
                }
                float rpm = Mathf.Lerp(s.rpm, t.rpm, w), thr = Mathf.Lerp(s.throttle, t.throttle, w);
                var pos = Vector3.LerpUnclamped(s.position, t.position, w);
                player.AudioRpm.Step(rpm, s.idleRpm, s.maxRpm, thr, w < 0.5f ? s.gear : t.gear, w < 0.5f ? s.airborne : t.airborne, dt, out float audioRpm, out float audioThrottle);
                player.Update(rpm, s.idleRpm, s.maxRpm, thr, w < 0.5f ? s.airborne : t.airborne, s.engineSize, pos, gain, dt, audioRpm, audioThrottle);
            }
        }

        /// The replay's sound off: its events stopped, the world's sound back under the viewer's pause.
        void SoundStop()
        {
            if (!audible) return;
            audible = false;
            var audio = Runtime ? Runtime.gameAudio : null;
            if (audio) { audio.StopReplayInstances(); audio.SetReplayIsolation(false); }
            wrappers.Clear();
            foreach (var p in replayPacks.Values) p.Destroy();
            replayPacks.Clear();
            OriginalVehicleSound.ReplayMuted = false;
            if (Runtime) Runtime.ViewerAudio = false;
            if (Active) AudioListener.pause = true;
        }
    }
}
