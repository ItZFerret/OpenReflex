using System;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;

// The bridge between the OPEN REFLEX core (Reflex.Port.RxCore, assembly Reflex.OpenReflex, no UnityEngine) and the old
// lifted/emulator plugin's result types and consumers (OriginalSimulation.Results and the Reflex* structs of ReflexWorldNative).
//
// This file is in the PROJECT assembly (Assembly-CSharp: no .asmdef here on purpose): only that assembly sees both the old
// types and the OpenReflex one (an asmdef cannot reference the predefined Assembly-CSharp).
//
// It copies what the core exports into a fresh (or recycled) OriginalSimulation.Results, field for field, so every existing
// renderer (OriginalPresentation, OriginalTerrain, OriginalBikePose, OriginalRider, OriginalOpponents, OriginalParticles,
// OriginalHud, OriginalOnboardCamera, OriginalTracks, OriginalAmbient, Content/ReflexMud ...) reads the open session exactly
// as it read the old plugin. Nothing in those consumers changes.
//
// What the open core cannot fill yet (notes/unity-api-surface.md, "not in the bridge yet"):
//  - audio commands; the matching fields are reset on every Apply so no consumer can draw stale data.
//  - ambient entities, the scene's animated objects, the vehicles' node flags and the tyre-track ribbons ARE filled (unityscene;
//    tyretracks, 2026-10-20).
//  - particles: the live array IS exported (rx_session_particles_array) and is mapped to ReflexParticle.
namespace Reflex.Port.RxCore.Bridge
{
    public static class OpenReflexResultsAdapter
    {
        /// A Results with the arrays the old consumers assume (bike, rider pose, drivers, mud, race line).
        public static OriginalSimulation.Results Create()
        {
            var r = new OriginalSimulation.Results { bike = ReflexBikeState.Create() };
            Reset(r);
            return r;
        }

        /// Clears the per-frame state of a recycled Results. Allocates nothing: the arrays keep their capacity.
        public static void Reset(OriginalSimulation.Results dst)
        {
            if (dst == null) return;
            dst.error = null; dst.replay = null; dst.nativeFrame = 0;
            dst.terrain.Clear();
            dst.audio.Clear();
            dst.laps.Clear(); dst.physClass = -1;
            dst.tracksChanged = false;
            dst.trackTriangles = 0;
            dst.hasVehicle = false;
            dst.hasHud = false; dst.hasWaypoint = false; dst.waypoint = default;
            dst.hasPowercross = false; dst.powercross = default;
            dst.resultsPage = null;
            dst.hasActivation = false; dst.activation = default; dst.hasChallenge = false; dst.challenge = default; dst.hasCompass = false; dst.compass = default; dst.hasRaceFormat = false; dst.raceFormat = default; dst.hasRaceHud = false; dst.raceHud = default; dst.hasModeResults = false; dst.modeResults = default; dst.hasStuntHud = false; dst.stuntHud = default; dst.hasReflexHud = false; dst.reflexHud = default;
            dst.vehicleAudio.Clear(); dst.sound.Clear();
            dst.eventUi = default;
            dst.hasCamera = false;
            dst.particleCount = 0;
            dst.riderBones = 0;
            dst.bikeNodes = 0;
            dst.driverCount = 0;
            dst.raceLinePoints = 0;
            dst.raceLineClosed = false; dst.onboardState = default;
            dst.bikeNodeFlagCount = 0;
            // slotNodeFlags / slotNodeFlagCounts keep their arrays: slotCount = 0 means nothing reads them (Reset allocates nothing)
            dst.slotMilliseconds = 0;
            dst.milliseconds = 0;
            dst.frameMilliseconds = 0;
            dst.capWaitMilliseconds = 0;
            // the ambient Frame (and its arrays) is kept and its count cleared: no allocation per frame
            if (dst.ambient != null) { dst.ambient.count = 0; dst.ambient.matrixCount = 0; }
            if (dst.podium != null) { dst.podium.valid = false; dst.podium.riderCount = dst.podium.slotCount = 0; }
            if (dst.sceneryModels != null) { dst.sceneryModels.valid = false; dst.sceneryModels.count = 0; }
            if (dst.objectsValid != null) Array.Clear(dst.objectsValid, 0, dst.objectsValid.Length);
            if (dst.bike.position == null) dst.bike = ReflexBikeState.Create();
        }

        static uint scenarioSoundSerial;
        static unsafe ReflexAudioCommand ScenarioSound(RxScenarioSound s)
        {
            uint n = ++scenarioSoundSerial;
            return new ReflexAudioCommand
            {
                sub = 0, wrapper = 0x00fe0001u + 2u * (n & 0xffff), serial = 0x40000000u + n,   // odd wrappers: never a game object
                path = s.Event, param = "", position = new[] { s.position[0], s.position[1], s.position[2] }, has_position = s.has_position,
            };
        }

        static unsafe void CopySceneryModels(OriginalSimulation.Results dst, OpenReflexSimulation.Results src)
        {
            var f = dst.sceneryModels ??= new OriginalSceneryModels.Frame();
            int n = Math.Min(src.scenarioModelCount, OriginalSceneryModels.MaxModels);
            for (int i = 0; i < n; i++)
            {
                var m = src.scenarioModels[i];
                f.ids[i] = m.entity_id; f.drawn[i] = m.drawn != 0; f.kinds[i] = m.kind;
                f.offsets[i * 2] = m.texture_offset[0]; f.offsets[i * 2 + 1] = m.texture_offset[1];
                for (int k = 0; k < 16; k++) f.matrices[i * 16 + k] = m.matrix[k];
            }
            f.count = n; f.valid = true;
        }

        static unsafe void CopyPodium(OriginalSimulation.Results dst, OpenReflexSimulation.Results src)
        {
            var f = dst.podium ??= new OriginalPodium.Frame();
            f.valid = true; f.scenario = src.podium.scenario; f.step = src.podium.step; f.postState = src.podium.post_state;
            f.riderCount = Math.Min(src.podiumRiderCount, f.riders.Length);
            for (int i = 0; i < f.riderCount; i++)
            {
                var r = src.podiumRiders[i]; var d = f.riders[i];
                d.obj = r.obj; d.slot = r.slot; d.visible = r.visible != 0 && r.placed != 0; d.bones = r.bones == OriginalPodium.Bones ? r.bones : 0;
                if (d.bones > 0 && (r.first_matrix + d.bones) * 16 <= src.podiumMatrices.Length)
                    Array.Copy(src.podiumMatrices, r.first_matrix * 16, d.matrices, 0, d.bones * 16);
                else d.bones = 0;
                d.root = new UnityEngine.Vector3(r.root[12], r.root[13], r.root[14]);
                d.drawDistance = r.draw_distance;
                d.mudAmount = new UnityEngine.Vector4(r.mud_amount[0], r.mud_amount[1], r.mud_amount[2], r.mud_amount[3]);
                d.mudColor = new UnityEngine.Vector4(r.mud_color[0], r.mud_color[1], r.mud_color[2], r.mud_color[3]);
                d.mudWetness = new UnityEngine.Vector4(r.mud_wetness[0], r.mud_wetness[1], r.mud_wetness[2], r.mud_wetness[3]);
            }
            f.slotCount = Math.Min(src.podiumSlotCount, f.slots.Length);
            for (int i = 0; i < f.slotCount; i++)
            {
                var s = src.podiumSlots[i];
                f.slots[i] = s.slot; f.riderDrawn[i] = s.rider_drawn != 0; f.vehicleDrawn[i] = s.vehicle_drawn != 0;
            }
        }

        /// Copies one open result into an old-typed Results (`dst` from Create() or a recycled one).
        public static void Apply(OriginalSimulation.Results dst, OpenReflexSimulation.Results src)
        {
            if (dst == null || src == null) return;
            Reset(dst);

            // presentation timeline / diagnostics (same meaning in both types)
            dst.dt = src.dt; dst.replay = src.replay; dst.nativeFrame = src.sessionFrames;
            dst.simTime = src.simTime; dst.framesRun = src.framesRun; dst.merged = src.merged;
            dst.inputClock = src.inputClock; dst.gameDt = src.gameDt; dst.gameTime = src.gameTime;
            dst.frameStartClock = src.frameStartClock; dst.collectionClock = src.collectionClock;
            dst.appliedPadCsv = src.appliedPadCsv; dst.corneringMetadata = src.corneringMetadata;
            dst.milliseconds = src.milliseconds; dst.frameMilliseconds = src.frameMilliseconds; dst.capWaitMilliseconds = src.capWaitMilliseconds;
            dst.error = src.error;

            // the local bike (four body poses, body-0 velocity)
            if (src.hasBike) CopyBike(ref dst.bike, src.bike);

            // poses
            Copy(src.riderPose, ref dst.riderPose, src.riderBones * 16); dst.riderBones = src.riderBones;
            Copy(src.bikePose, ref dst.bikePose, src.bikeNodes * 16); dst.bikeNodes = src.bikeNodes;

            // race / standings
            CopyRace(ref dst.race, src.race);
            dst.laps.AddRange(src.laps); dst.physClass = src.physClass;   // the lap log (w-lb-practice)
            dst.eventUi = src.eventUi;
            dst.waypoint = src.waypoint; dst.hasWaypoint = src.hasWaypoint;
            dst.powercross = src.powercross; dst.hasPowercross = src.hasPowercross;
            dst.resultsPage = src.resultsPage;
            dst.activation = src.activation; dst.hasActivation = src.hasActivation; dst.compass = src.compass; dst.hasCompass = src.hasCompass;
            dst.challenge = src.challenge; dst.hasChallenge = src.hasChallenge;
            dst.raceFormat = src.raceFormat; dst.hasRaceFormat = src.hasRaceFormat;
            dst.raceHud = src.raceHud; dst.hasRaceHud = src.hasRaceHud;
            dst.modeResults = src.modeResults; dst.hasModeResults = src.hasModeResults;
            dst.stuntHud = src.stuntHud; dst.hasStuntHud = src.hasStuntHud;
            dst.reflexHud = src.reflexHud; dst.hasReflexHud = src.hasReflexHud;
            dst.vehicleAudio.CopyFrom(src.vehicleAudio);
            dst.sound.CopyFrom(src.sound);
            dst.driverCount = Math.Min(src.driverCount, src.drivers.Length);
            if (dst.drivers == null || dst.drivers.Length < dst.driverCount) dst.drivers = new ReflexDriverResult[Math.Max(dst.driverCount, RxRiderCap.Max)];
            for (int i = 0; i < dst.driverCount; i++)
            {
                var d = src.drivers[i];
                dst.drivers[i] = new ReflexDriverResult
                {
                    slot = d.slot, local = d.local, place = d.place, laps_completed = d.laps_completed, finished = d.finished,
                    race_time = d.race_time, best_lap = d.best_lap, last_lap = d.last_lap,
                };
            }

            // the original HUD's other values
            if (src.hasHud)
            {
                dst.hasHud = true;
                dst.hud = new ReflexHudState
                {
                    valid = src.hud.valid, activity_state = src.hud.activity_state, on_track = src.hud.on_track, wrong_way = src.hud.wrong_way,
                    speed = src.hud.speed, time_behind = src.hud.time_behind, time_ahead = src.hud.time_ahead,
                    hold_clock = src.hud.hold_clock, hold_until = src.hud.hold_until, penalty = src.hud.penalty,
                };
            }
            else dst.hud = default;

            // the race line (node centres, 3 floats each)
            dst.onboardState = src.onboardState; dst.raceLineClosed = src.raceLineClosed;
            dst.raceLinePoints = Math.Min(src.raceLinePoints, src.raceLine.Length / 3);
            if (dst.raceLine == null || dst.raceLine.Length < dst.raceLinePoints * 3) dst.raceLine = new float[Math.Max(3 * 64, dst.raceLinePoints * 3)];
            if (dst.raceLinePoints > 0) Array.Copy(src.raceLine, dst.raceLine, dst.raceLinePoints * 3);

            // the game camera
            if (src.hasCamera) { dst.hasCamera = true; dst.camera = Camera(src.camera); }

            // the local vehicle (class, bodies; an ATV's six pose matrices)
            if (src.hasVehicle) { dst.hasVehicle = true; CopyVehicle(ref dst.vehicle, src.vehicle); }
            else if (dst.vehicle.renderPosition != null) { Array.Clear(dst.vehicle.renderPosition, 0, dst.vehicle.renderPosition.Length); Array.Clear(dst.vehicle.renderOrientation, 0, dst.vehicle.renderOrientation.Length); dst.vehicle.type = -1; }

            // terrain: hand the patches (and their pooled arrays) to the consumer, as the old simulation did
            foreach (var p in src.terrain)
                dst.terrain.Add(new OriginalSimulation.TerrainPatch { x0 = p.x0, z0 = p.z0, w = p.w, h = p.h, heights = p.heights, deform = p.deform });

            // tyre-track ribbons (OriginalTracks): the same 18 floats per triangle the old reflex_world_tracks returned
            if (src.tracksChanged && src.tracks != null && src.trackTriangles > 0)
            {
                int floats = src.trackTriangles * 18;
                if (dst.tracks == null || dst.tracks.Length < floats) dst.tracks = new float[Math.Max(floats, 65536 * 18)];
                Array.Copy(src.tracks, dst.tracks, floats);
                dst.trackTriangles = src.trackTriangles; dst.tracksChanged = true;
            }

            // particles: the info block and the live array (the renderer reads at most particleCount)
            dst.particleInfo = ParticleInfo(src.particleInfo);
            dst.particleCount = Math.Min(src.particleCount, src.particles.Length);
            if (dst.particleCount > 0)
            {
                if (dst.particles == null || dst.particles.Length < dst.particleCount) dst.particles = new ReflexParticle[Math.Max(dst.particleCount, 6000)];
                for (int i = 0; i < dst.particleCount; i++) dst.particles[i] = Particle(src.particles[i]);
            }

            // mud on every rider ([0] the local player)
            dst.mudCount = Math.Min(src.mudCount, src.mud.Length);
            if (dst.mud == null || dst.mud.Length < src.mud.Length) dst.mud = new ReflexMudState[src.mud.Length];
            for (int i = 0; i < src.mud.Length; i++)
                dst.mud[i] = i < dst.mudCount && src.mudValid[i] ? Mud(src.mud[i]) : default;

            // the other riders (tracked slots): bike nodes and the riding rider's bones
            dst.slotCount = src.slotCount;            if (dst.slotCount > 0)
            {
                if (dst.slots == null || dst.slots.Length != dst.slotCount)
                {
                    dst.slots = new int[dst.slotCount]; dst.slotBikeNodes = new int[dst.slotCount]; dst.slotRiderBones = new int[dst.slotCount];
                    dst.slotBikePose = new float[dst.slotCount][]; dst.slotRiderPose = new float[dst.slotCount][];
                    for (int i = 0; i < dst.slotCount; i++)
                    {
                        dst.slotBikePose[i] = new float[OpenReflexSimulation.MaxSlotNodes * 16];
                        dst.slotRiderPose[i] = new float[OpenReflexSimulation.RiderBoneCount * 16];
                    }
                }
                for (int i = 0; i < dst.slotCount; i++)
                {
                    dst.slots[i] = src.slots[i];
                    dst.slotBikeNodes[i] = src.slotBikeNodes[i];
                    dst.slotRiderBones[i] = src.slotRiderBones[i];
                    Copy(src.slotBikePose[i], ref dst.slotBikePose[i], src.slotBikeNodes[i] * 16);
                    Copy(src.slotRiderPose[i], ref dst.slotRiderPose[i], src.slotRiderBones[i] * 16);
                }
            }
            dst.slotMilliseconds = src.slotMilliseconds;

            // the post-race cast (OriginalPodium): the podium RiderCharacters and the live models' drawn flags (rx_session_podium)
            if (src.hasPodium) CopyPodium(dst, src);

            // the static scenery models a scenario script hides / shows (OriginalSceneryModels; rx_session_scenario_models)
            if (src.hasScenarioModels) CopySceneryModels(dst, src);

            // the scenario scripts' sound effects: one-shot plays for the race's sound player (OriginalAudio resolves [FORMAT] / [LOC])
            for (int i = 0; i < src.scenarioSoundCount; i++) dst.audio.Add(ScenarioSound(src.scenarioSounds[i]));

            // ambient animated entities (OriginalAmbient): the entity records and their node matrices, as reflex_world_ambient
            if (src.hasAmbient && src.ambientEntityCount > 0)
            {
                if (dst.ambient == null) dst.ambient = new OriginalAmbient.Frame();
                var f = dst.ambient;
                int ne = Math.Min(src.ambientEntityCount, OriginalAmbient.MaxEntities);
                f.count = ne;
                f.info = new ReflexAmbientInfo
                {
                    entities = src.ambient.entities, matrices = src.ambient.matrices, objects = src.ambient.objects,
                    fadeStart = src.ambient.fade_start, lodDivisor = src.ambient.lod_divisor,
                };
                CopyAmbientEntities(src.ambientEntities, f.entities, ne);
                // the scenario actors' node flags (SetNodeVisibility: hidden bindings)
                int nb = Math.Min(src.ambientNodeFlagBytes, f.nodeFlags.Length);
                for (int i = 0; i < ne; i++)
                {
                    int c = src.ambientNodeFlagCount[i], s = src.ambientNodeFlagStart[i];
                    if (c <= 0 || s + c > nb) { f.nodeFlagCount[i] = 0; continue; }
                    f.nodeFlagStart[i] = s; f.nodeFlagCount[i] = c;
                }
                if (nb > 0) Array.Copy(src.ambientNodeFlags, f.nodeFlags, nb);
                int nm = Math.Min(src.ambientMatrixCount, OriginalAmbient.MaxMatrices);
                f.matrixCount = nm;
                if (nm > 0) Array.Copy(src.ambientMatrices, f.matrices, nm * 16);
                // the stands crowd's camera flashes (rx_session_crowd_flashes)
                int nf = Math.Min(src.crowdFlashCount, OriginalAmbient.MaxFlashes);
                f.flashCount = nf;
                for (int i = 0; i < nf; i++)
                {
                    var q = src.crowdFlashes[i];
                    f.flashes[i] = new OriginalAmbient.CrowdFlash { obj = q.object_id, binding = q.binding, seat = q.seat, age = q.age, length = q.length, emptyRatio = q.empty_chair_ratio };
                }
            }

            // a machine's node flags (the game's damage panels) and the animated scene objects
            dst.bikeNodeFlagCount = Math.Min(src.bikeNodeFlagCount, dst.bikeNodeFlags.Length);
            if (dst.bikeNodeFlagCount > 0) Array.Copy(src.bikeNodeFlags, dst.bikeNodeFlags, dst.bikeNodeFlagCount);
            if (src.slotCount > 0 && src.slotNodeFlagCounts.Length >= src.slotCount)
            {
                if (dst.slotNodeFlags == null || dst.slotNodeFlags.Length < src.slotCount) dst.slotNodeFlags = new byte[src.slotCount][];
                if (dst.slotNodeFlagCounts == null || dst.slotNodeFlagCounts.Length < src.slotCount) dst.slotNodeFlagCounts = new int[src.slotCount];
                Array.Copy(src.slotNodeFlagCounts, dst.slotNodeFlagCounts, src.slotCount);
                for (int i = 0; i < src.slotCount; i++)
                {
                    if (dst.slotNodeFlags[i] == null) dst.slotNodeFlags[i] = new byte[OpenReflexSimulation.MaxSlotNodes];
                    Array.Copy(src.slotNodeFlags[i], dst.slotNodeFlags[i], src.slotNodeFlagCounts[i]);
                }
            }
            int no = src.objectsValid.Length;
            if (no > 0)
            {
                if (dst.objects == null || dst.objects.Length != no * 16) { dst.objects = new float[no * 16]; dst.objectsValid = new bool[no]; }
                Array.Copy(src.objects, dst.objects, no * 16);
                Array.Copy(src.objectsValid, dst.objectsValid, no);
            }
        }

        static unsafe void CopyAmbientEntities(RxAmbientEntity[] src, byte[] dst, int count)
        {
            if (count <= 0) return;
            fixed (RxAmbientEntity* ps = src)
            fixed (byte* pd = dst)
                Buffer.MemoryCopy(ps, pd, dst.Length, (long)count * sizeof(RxAmbientEntity));
        }

        // ----------------------------------------------------------------------------------------------------------- the pieces

        static void Copy(float[] src, ref float[] dst, int count)
        {
            if (count <= 0) return;
            if (dst == null || dst.Length < count) dst = new float[count];
            Array.Copy(src, dst, count);
        }

        static unsafe void CopyBike(ref ReflexBikeState dst, in RxBikeState src)
        {
            if (dst.position == null) dst = ReflexBikeState.Create();
            var s = src;
            RxFixed.Copy(s.position, dst.position, 16);
            RxFixed.Copy(s.orientation, dst.orientation, 16);
            RxFixed.Copy(s.render_position, dst.renderPosition, 16);
            RxFixed.Copy(s.render_orientation, dst.renderOrientation, 16);
            RxFixed.Copy(s.velocity, dst.velocity, 4);
            RxFixed.Copy(s.inputs, dst.inputs, 8);
            dst.speed = s.speed; dst.gear = s.gear; dst.frames = s.frames; dst.reserved = s.reserved;
        }

        static void CopyRace(ref ReflexRaceState dst, in RxRaceState src)
        {
            dst = new ReflexRaceState
            {
                valid = src.valid, state = src.state, num_laps = src.num_laps, laps_completed = src.laps_completed,
                place = src.place, riders = src.riders, lap_time = src.lap_time, last_lap = src.last_lap,
                best_lap = src.best_lap, race_time = src.race_time, lap_progress = src.lap_progress,
                start_timer = src.start_timer, reset_timer = src.reset_timer, countdown_step = src.countdown_step,
            };
        }

        static unsafe ReflexCamera Camera(in RxCamera src)
        {
            var c = src;
            var r = new ReflexCamera();
            r.px = c.position[0]; r.py = c.position[1]; r.pz = c.position[2];
            r.rx = c.right[0]; r.ry = c.right[1]; r.rz = c.right[2];
            r.ux = c.up[0]; r.uy = c.up[1]; r.uz = c.up[2];
            r.fx = c.forward[0]; r.fy = c.forward[1]; r.fz = c.forward[2];
            r.fov = c.fov; r.nearPlane = c.near_plane; r.farPlane = c.far_plane;
            return r;
        }

        static unsafe ReflexParticleInfo ParticleInfo(in RxParticleInfo src)
        {
            var i = src;
            var r = new ReflexParticleInfo { texture = "" };
            r.texture = RxFixed.GetUtf8(i.texture, 64);
            r.lum0 = i.luminance[0]; r.lum1 = i.luminance[1]; r.lum2 = i.luminance[2]; r.lum3 = i.luminance[3];
            r.delta0 = i.luminance_delta[0]; r.delta1 = i.luminance_delta[1]; r.delta2 = i.luminance_delta[2]; r.delta3 = i.luminance_delta[3];
            r.fade0 = i.fade[0]; r.fade1 = i.fade[1]; r.fade2 = i.fade[2]; r.fade3 = i.fade[3];
            r.soft = i.soft; r.normal = i.normal; r.additive = i.additive; r.live = i.live;
            r.nearPlane = i.near_plane; r.farPlane = i.far_plane;
            return r;
        }

        static unsafe ReflexParticle Particle(in RxParticle src)
        {
            var p = src;
            var r = new ReflexParticle();
            r.px = p.position[0]; r.py = p.position[1]; r.pz = p.position[2];
            r.sizeX = p.size[0]; r.sizeY = p.size[1];
            r.pivotX = p.pivot[0]; r.pivotY = p.pivot[1];
            r.scale = p.scale; r.rotation = p.rotation; r.color = p.color; r.color1 = p.color1;
            r.uScale = p.uv[0]; r.vScale = p.uv[1]; r.uOffset = p.uv[2]; r.vOffset = p.uv[3];
            r.flags = p.flags;
            return r;
        }

        static unsafe ReflexMudState Mud(in RxMudState src)
        {
            var m = src;
            var r = new ReflexMudState { slot = m.slot, record = m.record, vehicle = m.vehicle, rider = m.rider };
            r.amount = new UnityEngine.Vector4(m.amount[0], m.amount[1], m.amount[2], m.amount[3]);
            r.wetness = new UnityEngine.Vector4(m.wetness[0], m.wetness[1], m.wetness[2], m.wetness[3]);
            r.color = new UnityEngine.Vector4(m.color[0], m.color[1], m.color[2], m.color[3]);
            return r;
        }

        static unsafe void CopyVehicle(ref ReflexVehicleInfo dst, in RxVehicleInfo src)
        {
            if (dst.renderPosition == null) { dst.renderPosition = new float[24]; dst.renderOrientation = new float[24]; }
            var v = src;
            dst.slot = v.slot; dst.type = v.type; dst.bodies = v.bodies; dst.modelNodes = v.model_nodes;
            dst.vehicle = v.vehicle; dst.sim = v.sim; dst.gear = v.gear; dst.rpm = v.rpm; dst.speed = v.speed;
            RxFixed.Copy(v.render_position, dst.renderPosition, 24);
            RxFixed.Copy(v.render_orientation, dst.renderOrientation, 24);
        }
    }
}
