using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // The static scenery models as the running game draws them (open core: rx_session_scenario_models, rx_podium_api.h v3). The level's
    // ModelRSData entities are built once from the install (ReflexScenery: one GameObject per scene entity); the game's renderer draws each
    // ModelEntity only while its ModelInstance's drawn flag (+0xa2) is set (Model_Submit 007f1b00), at the instance matrix (+0xc0). A
    // scenario script changes the flag: the Free Ride arrival hides the parked hovercraft HC_Static while the animated one comes in and shows
    // it again at the hero shot's end (FR_Alpine: DisableEntity / ModelPosition), FR_Gibson hides the dropped box FR_CH_DropBox_open until the
    // parachute box has landed, the WaypointRace podium scripts hide / show the PodiumTruck. This applies the exported flag (and the
    // matrix, when the game's differs from the level data) to the instance of the same scene entity id. Nothing is decided here.
    public sealed class OriginalSceneryModels
    {
        public const int MaxModels = 4096;

        /// One frame's copy of the export (the simulation thread fills it; the newest frame is applied).
        public sealed class Frame
        {
            public bool valid; public int count;
            public readonly int[] ids = new int[MaxModels];
            public readonly bool[] drawn = new bool[MaxModels];
            public readonly int[] kinds = new int[MaxModels];               // 0 ModelEntity, 1 ScrollingTextureEntity
            public readonly float[] offsets = new float[MaxModels * 2];     // kind 1: the game's gTextureOffset (u, v)
            public readonly float[] matrices = new float[MaxModels * 16];
        }

        sealed class Applied
        {
            public SceneryInstance instance; public Renderer renderer; public bool active = true; public bool moved; public readonly float[] matrix = new float[16]; public Vector2 offset = new Vector2(float.NaN, 0);
            float[] scaled;
            public float[] ScaledLevel(float[] level, float s)
            {
                if (scaled == null) { scaled = (float[])level.Clone(); if (scaled.Length >= 15) { scaled[12] *= s; scaled[13] *= s; scaled[14] *= s; } }
                return scaled;
            }
        }
        static readonly int ScrollId = Shader.PropertyToID("_RfxScrollOverride"), OffsetId = Shader.PropertyToID("_RfxTextureOffset");
        MaterialPropertyBlock mpb;
        /// Scrolling textures take the game's own gTextureOffset (ScrollingTextureEntity::vf19 00410dd0) from here (OriginalProps then
        /// leaves them alone instead of running the formula on Unity's clock).
        public bool GameScrolling { get; private set; }
        readonly Dictionary<int, Applied> byId = new Dictionary<int, Applied>();
        /// Entity ids a port extra keeps hidden whatever the game's drawn flag says (the parked hovercraft once the drivable one has taken its place).
        public readonly HashSet<int> suppressed = new HashSet<int>();
        ReflexLevelContent content; int lookedUpInstances = -1; float nextLookup;

        /// A new world: forget the previous level's instances.
        public void Begin(OriginalLevel level) { suppressed.Clear(); byId.Clear(); content = level ? level.Content : null; lookedUpInstances = -1; GameScrolling = false; }

        public int Hidden { get; private set; }   // instances the game does not draw now (diagnostics)
        /// Instances whose game matrix differs from the level's (moved by the scale): a scenario's moved models only (diagnostics: the
        /// MAP SCALE probe checks the scaled scenery lines up with the core's), and how many were compared.
        public int Differing { get; private set; }
        public int Compared { get; private set; }

        public void Apply(Frame f)
        {
            if (f == null || !f.valid) return;
            if (content == null)
            {
                if (Time.unscaledTime < nextLookup) return;
                nextLookup = Time.unscaledTime + 1f;
                var level = UnityEngine.Object.FindAnyObjectByType<OriginalLevel>();
                content = level ? level.Content : null;
                if (content == null) return;
            }
            if (lookedUpInstances != content.instances.Count)
            {
                byId.Clear();
                foreach (var inst in content.instances)
                    if (inst.transform && (inst.entityType == "ModelRSData" || inst.entityType == "ScrollingTextureRSData") && !byId.ContainsKey(inst.entityId))
                        byId[inst.entityId] = new Applied { instance = inst, renderer = inst.transform.GetComponent<Renderer>() };
                lookedUpInstances = content.instances.Count;
            }
            int hidden = 0, differing = 0, compared = 0;
            for (int i = 0; i < f.count; i++)
            {
                if (!byId.TryGetValue(f.ids[i], out var a) || !a.instance.transform) continue;
                var go = a.instance.transform.gameObject;
                bool on = f.drawn[i] && (suppressed.Count == 0 || !suppressed.Contains(f.ids[i]));
                if (on != a.active || go.activeSelf != on) { go.SetActive(on); a.active = on; }
                if (!on) hidden++;
                if (f.kinds[i] == 1 && a.renderer)
                {
                    var off = new Vector2(f.offsets[i * 2], f.offsets[i * 2 + 1]);
                    if (off != a.offset)
                    {
                        a.offset = off; mpb ??= new MaterialPropertyBlock();
                        var v = new Vector4(off.x, off.y, 1, 0);
                        a.renderer.GetPropertyBlock(mpb); mpb.SetVector(ScrollId, v); mpb.SetVector(OffsetId, v); a.renderer.SetPropertyBlock(mpb);
                    }
                    GameScrolling = true;
                }
                // the instance matrix: applied only when the game's differs from the level data (or from what was applied last)
                int o = i * 16; bool differs = false, changed = false;
                var level = a.instance.matrix;
                // MAP SCALE: the game's matrices are in the scaled world (the translation times the scale, the axes the level's); compare with
                // and fall back to the level matrix moved the same way (ApplyMatrix sets world values under the scaled scenery root)
                if (level != null && Reflex.Port.RxCore.RxMapScale.Scaled) level = a.ScaledLevel(level, Reflex.Port.RxCore.RxMapScale.Current);
                for (int k = 0; k < 16; k++)
                {
                    float m = f.matrices[o + k];
                    if (level != null && k < level.Length && Math.Abs(m - level[k]) > 1e-4f) differs = true;
                    if (m != a.matrix[k]) changed = true;
                }
                compared++; if (differs) differing++;
                if (!differs && !a.moved) continue;
                if (!changed && a.moved) continue;
                Array.Copy(f.matrices, o, a.matrix, 0, 16);
                ReflexLevelContent.ApplyMatrix(a.instance.transform, differs ? a.matrix : level);
                a.moved = differs;
            }
            Hidden = hidden; Differing = differing; Compared = compared;
        }
    }
}
