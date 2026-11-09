using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // The vehicle sound layer of the open path (core/plan/QUEUE.md "w-audio"): every rider's engine from the core's telemetry
    // (rx_session_vehicle_audio). A rider whose bike has an engine pack chosen in the garage (EngineSoundPacks.ChoiceFor(VehicleModelId);
    // the local rider; computer riders keep the original) plays it through EnginePackPlayer; every other rider runs the original's
    // EngineSoundEmitter (OriginalEngineAudio) on the game's own engine events. Owned by OriginalWorldRuntime: Frame per collected core
    // result, Tick per render frame, Dispose with the world.
    public sealed class OriginalVehicleSound
    {
        readonly OriginalAudio audio;
        readonly OriginalWorldRuntime runtime;
        readonly OriginalEngineAudio engines;
        readonly OriginalGameSound game;   // w-sfx: drive-on / foley / collisions
        readonly OriginalLevelSound level; // w-sfx: the level's crowd emitters and ambience
        OriginalMusic music; int musicLookups;
        readonly RxVehicleAudioState last = new RxVehicleAudioState();
        readonly Dictionary<uint, EnginePackPlayer> packs = new Dictionary<uint, EnginePackPlayer>();   // by vehicle
        readonly List<uint> gone = new List<uint>();
        bool haveState;
        /// Whether computer riders also play the pack their bike's model has (off: the original's engines, as the brief asks by default).
        public static bool PacksForComputerRiders = false;
        /// Diagnostics: only the local rider's engine sounds (the computer riders' emitters are silent).
        public static bool SoloLocal = false;
        /// The local rider's engine is silent (the rider is in another vehicle: the drivable hovercraft, OriginalHovercraft).
        public static bool MuteLocal = false;
        /// The replay viewer plays its own engines (Reflex.Port.Replay.ReflexReplay): the live packs are silent meanwhile.
        public static bool ReplayMuted = false;
        /// RIDER CAP (w-rc-client): in a field past the original's 12, only this many computer riders nearest the listener run an engine (the
        /// original's vehicle sound manager had 12 emitters: the local rider + 11); the others are stopped until they come near again. A field of
        /// 12 or fewer is never culled (the original's mix). 0 or less: no limit.
        public static int EngineVoices = RxRiderCap.OriginalOpponents;
        /// A culled rider must come this much nearer (a fraction of its distance) than the farthest heard one to take its voice back: no
        /// flapping at the boundary.
        public const float EngineVoiceHysteresis = 0.15f;
        readonly HashSet<uint> culled = new HashSet<uint>();
        readonly float[] cullDistance = new float[RxRiderCap.Max];
        readonly uint[] cullVehicle = new uint[RxRiderCap.Max];
        /// The computer riders whose engines are culled now (diagnostics: the perf line, tests).
        public int CulledEngines => culled.Count;
        public OriginalEngineAudio Engines => engines;
        public OriginalGameSound Game => game;
        public OriginalLevelSound Level => level;
        public RxVehicleAudioState Last => last;
        public EnginePackPlayer PackPlayerOfSlot(int slot)
        {
            for (int i = 0; i < last.riderCount; i++) { var r = last.riders[i]; if (r.slot == slot && packs.TryGetValue(r.vehicle, out var p)) return p; }   // (no iterator: every frame, w-gcspikes)
            return null;
        }

        public OriginalVehicleSound(OriginalAudio audio, OriginalWorldRuntime runtime)
        {
            this.audio = audio; this.runtime = runtime;
            engines = new OriginalEngineAudio(audio) { Silenced = r => PackOf(r) != null || (SoloLocal && !Local(r)) || (MuteLocal && Local(r)) || culled.Contains(r.vehicle) };
            // w-rng: a bike's rev end goes back to the core's start line (the core draws the next rev), between the simulation's frames
            engines.ReportFinished = slot => { var sim = runtime ? runtime.SimDriver : null; if (sim != null && sim.Open != null) sim.BetweenFrames(s => { try { RxVehicleAudioInput.Finished(s, slot); } catch (EntryPointNotFoundException) { } }); };
            game = new OriginalGameSound(audio, runtime);
            game.FirstPersonView = () => runtime && runtime.FirstPersonView;
            level = new OriginalLevelSound(audio);
        }

        /// The vehicle model of a rider's slot (the race roster), -1 unknown.
        int ModelOf(RxVehicleAudioRider r)
        {
            var roster = runtime ? runtime.Roster : null;
            if (roster == null) return -1;
            foreach (var e in roster.entries) if (e.slot.slot == r.slot) return e.vehicleModelId;
            return -1;
        }
        bool Local(RxVehicleAudioRider r)
        {
            var roster = runtime ? runtime.Roster : null;
            if (roster != null) foreach (var e in roster.entries) if (e.slot.slot == r.slot) return e.Local;
            return (r.flags & RxVehicleAudioRider.LocalHuman) != 0;
        }
        EnginePack PackOf(RxVehicleAudioRider r)
        {
            if (!Local(r) && !PacksForComputerRiders) return null;
            return EngineSoundPacks.PackFor(ModelOf(r));
        }

        /// The riders' pelvis (bone 0, MXRider_Root, of the 69 bones the core exports) of a collected result: the foley emitter puts the rider's sounds on it
        /// (snapshot + 0x200, w-reverb; OriginalGameSound.SetPelvis). Called before Frame.
        public void Poses(OriginalSimulation.Results r)
        {
            if (r == null || game == null) return;
            if (r.riderBones > 0 && r.riderPose != null && r.riderPose.Length >= 16)
            {
                var va = r.vehicleAudio;
                if (va != null && va.valid) for (int i = 0; i < va.riderCount; i++) if ((va.riders[i].flags & RxVehicleAudioRider.LocalHuman) != 0) { game.SetPelvis(va.riders[i].slot, r.riderPose, 0); break; }
            }
            for (int i = 0; i < r.slotCount && i < r.slots.Length; i++)
                if (r.slotRiderBones[i] > 0 && r.slotRiderPose[i] != null && r.slotRiderPose[i].Length >= 16) game.SetPelvis(r.slots[i], r.slotRiderPose[i], 0);
        }

        /// A collected core result: the original emitters' game frame, and the pack players' telemetry; the other managers (w-sfx).
        public void Frame(RxVehicleAudioState state, RxSoundState sound, float dt)
        {
            if (state == null || !state.valid) return;
            if (sound != null && sound.valid) audio.SetLevelMix(sound.channelMix, sound.channelCount);
            last.CopyFrom(state); haveState = true;
            CullEngines(state);
            engines.Frame(state, Mathf.Clamp(dt, 0f, 0.25f));
            if (sound != null)
            {
                game.LocalHasPack = false;   // w-landclank: the landing clank is trimmed against the local rider's engine pack
                for (int i = 0; i < state.riderCount; i++) if (Local(state.riders[i]) && PackOf(state.riders[i]) != null) game.LocalHasPack = true;
                game.Frame(state, sound, Mathf.Clamp(dt, 0f, 0.25f)); level.Frame(sound);
                if (sound.musicValid) { if (!music && musicLookups++ % 120 == 0) music = UnityEngine.Object.FindAnyObjectByType<OriginalMusic>(); if (music) music.ObserveOpen(sound); }   // w-sfx: the core's MusicPlayer
            }
            gone.Clear();
            foreach (var kv in packs) gone.Add(kv.Key);
            for (int i = 0; i < state.riderCount; i++)
            {
                var r = state.riders[i];
                var p = PackOf(r);
                if (p == null) continue;
                gone.Remove(r.vehicle);
                if (!packs.TryGetValue(r.vehicle, out var player) || player.Pack != p)
                {
                    player?.Destroy();
                    packs[r.vehicle] = new EnginePackPlayer(p, audio.transform, audio.Reverb);
                }
            }
            foreach (var v in gone) { packs[v].Destroy(); packs.Remove(v); }
        }

        /// A render frame (after OriginalAudio.Tick's gains): the pack voices follow the newest telemetry.
        public void Tick(float dt)
        {
            if (!haveState || packs.Count == 0) return;
            float gain = ReplayMuted ? 0f : audio.ChannelGain(ReflexChannel.Engine);
            for (int i = 0; i < last.riderCount; i++)
            {
                var r = last.riders[i];
                if (!packs.TryGetValue(r.vehicle, out var player)) continue;
                bool local = Local(r);
                bool cut = !local && culled.Contains(r.vehicle);   // RIDER CAP: a culled computer rider's pack is silent too
                // w-aiengine: the computer riders' packs (PacksForComputerRiders) sit under the same field trim as their original engines
                // w-packrpm: the voices follow the audio rpm / load model (Reflex's quick shifts and wheel spin, smoothed); the limiter the true rpm
                player.AudioRpm.Step(r.rpm, r.idleRpm, r.maxRpm, r.throttle, r.gear, r.airborne, dt, out float audioRpm, out float audioThrottle);
                player.Update(r.rpm, r.idleRpm, r.maxRpm, r.throttle, r.airborne, r.engineSize, new Vector3(r.px, r.py, r.pz), MuteLocal && local || cut ? 0f : local ? gain : gain * engines.FieldTrim, dt,
                              audioRpm, audioThrottle);
            }
        }

        // RIDER CAP: the nearest EngineVoices computer riders keep their engines (squared distances to the listener, no allocation: the arrays
        // are sized for RxRiderCap.Max and sorted in place). A rider heard last frame counts EngineVoiceHysteresis nearer.
        void CullEngines(RxVehicleAudioState state)
        {
            if (state.riderCount <= RxRiderCap.Original || EngineVoices <= 0) { culled.Clear(); return; }
            var lis = audio.Listener; int n = 0;
            float keep = (1f - EngineVoiceHysteresis) * (1f - EngineVoiceHysteresis);
            for (int i = 0; i < state.riderCount && n < cullDistance.Length; i++)
            {
                var r = state.riders[i];
                if ((r.flags & RxVehicleAudioRider.Valid) == 0 || Local(r)) continue;
                float dx = r.px - lis.x, dy = r.py - lis.y, dz = r.pz - lis.z, d = dx * dx + dy * dy + dz * dz;
                cullDistance[n] = culled.Contains(r.vehicle) ? d : d * keep; cullVehicle[n] = r.vehicle; n++;
            }
            culled.Clear();
            if (n <= EngineVoices) return;
            Array.Sort(cullDistance, cullVehicle, 0, n);
            for (int k = EngineVoices; k < n; k++) culled.Add(cullVehicle[k]);
        }

        public void Dispose()
        {
            engines.Dispose();
            game.Dispose();
            level.Dispose();
            foreach (var p in packs.Values) p.Destroy();
            packs.Clear();
        }
    }
}
