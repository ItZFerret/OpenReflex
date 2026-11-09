using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.OriginalWorld
{
    /// A vehicle the player can climb into on a level (port extra; data driven: another hovercraft-like prop is another row).
    public sealed class HoverSpec
    {
        public string level;                 // the Database name (FR_Alpine)
        public string parkedEntity;          // the level's static copy the craft replaces (a ModelRSData instance name)
        public string model;                 // the animated model the drivable craft is drawn from
        public string materialEntity;        // the scene entity whose MaterialList the model is drawn with (the arrival's animated craft)
        public string skirtMaterial;         // the binding that holds the air cushion's skirt (its vertices below skirtTop are deformed)
        public float skirtTop;               // model y where the skirt meets the hull; vertices at or above it never move
        public float skirtBottom;            // model y of its lowest vertex
        public Vector3 seat;                 // model space: where the rider's bike stands on the deck (its ground contact), facing +z
        public string boardSound;            // a scenario sound event of the level played as the fans start (the arrival's engine)
        public string[] fanNodes = { "HC_Prop1", "HC_Prop2" };     // spin about the model's z axis (opposite ways)
        public string[] rudderNodes = { "HC_Fin1", "HC_Fin2", "HC_Fin3", "HC_Fin4" };   // swing about y with the steering
        public string[] spinNodes = { "HC_Radar" };                 // turn about y when running
        public float halfLength = 26f, halfWidth = 15f;            // the hover points (model space)

        public static readonly HoverSpec[] All =
        {
            new HoverSpec { level = "FR_Alpine", parkedEntity = "HC_Static", model = "Hovercraft_BC", materialEntity = "Hovercraft",
                skirtMaterial = "Hovercraft_Base", boardSound = "scenarios_fr_al/freeride/vehicle", skirtTop = 8.9f, skirtBottom = -0.3f, seat = new Vector3(-2.5f, 9.3f, 3f) },
        };

        public static HoverSpec For(string level) => All.FirstOrDefault(s => string.Equals(s.level, level, StringComparison.OrdinalIgnoreCase));
    }

    /// The craft's model: the hull and cab, the two fans and four rudders (separate parts on their nodes' pivots), the radar, and the air
    /// cushion's skirt, the one part the game's data gives no motion: it is deformed here (the skirt's vertices scale towards the hull
    /// from flat to full over the inflation, ballooning a little past full, fluttering while the craft runs).
    public sealed class HoverBody
    {
        public GameObject root;
        readonly HoverSpec spec;
        Mesh skirtMesh; Vector3[] skirtOrig, skirtNormalsOrig, skirtPos, skirtNormals; float[] skirtWeight, skirtAzimuth, skirtHash;
        readonly List<Transform> fans = new List<Transform>(), rudders = new List<Transform>(), spinners = new List<Transform>();
        readonly List<float> fanDir = new List<float>();
        readonly List<UnityEngine.Object> made = new List<UnityEngine.Object>();
        float fanAngle, radarAngle, rudder;
        float lastK = float.NaN, lastFlutter = float.NaN; int vertexUpdates;
        public int VertexUpdates => vertexUpdates;
        public int SkirtVertices => skirtOrig?.Length ?? 0;
        public int Parts => fans.Count + rudders.Count + spinners.Count;
        public string Report { get; private set; }

        HoverBody(HoverSpec s) { spec = s; }

        /// Builds the craft from the level's content (main thread). Null when the model cannot be read.
        public static HoverBody Build(ReflexLevelContent content, HoverSpec spec, Shader sceneryShader, Transform parent)
        {
            if (content == null) return null;
            var am = content.LoadAmbientModel(spec.model, null);
            if (am == null || am.error != null) { Debug.LogWarning("Hovercraft: model " + spec.model + " not read: " + am?.error); return null; }
            var entity = content.entities?.FirstOrDefault(e => e.Type == "ScenarioAnimEntityRSData" && e.String("Name") == spec.materialEntity);
            var materials = content.AmbientMaterials(am, entity?.MaterialList(), new ReflexLevelContent.SceneryShaders { scenery = sceneryShader ? sceneryShader : Shader.Find("Reflex/Original Scenery") });
            var b = new HoverBody(spec);
            b.root = new GameObject("Hovercraft (drivable, port extra)");
            if (parent) b.root.transform.SetParent(parent, false);
            int bindings = am.triangles.Count;
            var used = new bool[bindings];
            // the rotating parts, each on its node's pivot
            for (int k = 0; k < bindings; k++)
            {
                string node = am.nodeNames[am.bindingNodes[k]];
                int kind = Array.IndexOf(spec.fanNodes, node) >= 0 ? 1 : Array.IndexOf(spec.rudderNodes, node) >= 0 ? 2 : Array.IndexOf(spec.spinNodes, node) >= 0 ? 3 : 0;
                if (kind == 0) continue;
                var pivot = (Vector3)am.bindWorld[am.bindingNodes[k]].GetColumn(3);
                var go = new GameObject(node); go.transform.SetParent(b.root.transform, false); go.transform.localPosition = pivot;
                b.Add(go, MakeMesh(am, new[] { k }, pivot, node, false), materials, new[] { k });
                (kind == 1 ? b.fans : kind == 2 ? b.rudders : b.spinners).Add(go.transform);
                if (kind == 1) b.fanDir.Add(pivot.x < 0 ? 1f : -1f);
                used[k] = true;
            }
            // the skirt: the binding of the skirt material, deformed
            var skirt = new List<int>(); var hull = new List<int>();
            for (int k = 0; k < bindings; k++)
            {
                if (used[k]) continue;
                (string.Equals(am.bindingMaterials[k], spec.skirtMaterial, StringComparison.OrdinalIgnoreCase) ? skirt : hull).Add(k);
            }
            if (hull.Count > 0)
            {
                var go = new GameObject("hull"); go.transform.SetParent(b.root.transform, false);
                b.Add(go, MakeMesh(am, hull.ToArray(), Vector3.zero, "hull", false), materials, hull.ToArray());
            }
            if (skirt.Count > 0)
            {
                var go = new GameObject("skirt"); go.transform.SetParent(b.root.transform, false);
                var mesh = MakeMesh(am, skirt.ToArray(), Vector3.zero, "skirt", true);
                b.Add(go, mesh, materials, skirt.ToArray());
                b.SetupSkirt(mesh);
            }
            b.Report = $"{bindings} bindings: hull {hull.Count}, skirt {skirt.Count} ({b.SkirtVertices} vertices), fans {b.fans.Count}, rudders {b.rudders.Count}, radar {b.spinners.Count}";
            b.SetCushion(0f, 0f, 0f);
            return b;
        }

        void Add(GameObject go, Mesh mesh, Material[] materials, int[] bindings)
        {
            made.Add(mesh);
            go.AddComponent<MeshFilter>().sharedMesh = mesh;
            var mr = go.AddComponent<MeshRenderer>();
            var mats = new Material[bindings.Length];
            for (int i = 0; i < bindings.Length; i++) mats[i] = bindings[i] < materials.Length ? materials[bindings[i]] : null;
            mr.sharedMaterials = mats;
            mr.shadowCastingMode = ShadowCastingMode.On;
        }

        // A mesh of some of the model's bindings (one submesh each), positions relative to the pivot.
        static Mesh MakeMesh(ReflexLevelContent.AmbientModel am, int[] bindings, Vector3 pivot, string name, bool dynamic)
        {
            var map = new Dictionary<int, int>();
            var pos = new List<Vector3>(); var nrm = new List<Vector3>(); var uv0 = new List<Vector2>(); var uv1 = new List<Vector2>();
            var col = new List<Color32>(); var tan = new List<Vector4>();
            var tris = new List<int[]>();
            foreach (int b in bindings)
            {
                var src = am.triangles[b]; var dst = new int[src.Length];
                for (int i = 0; i < src.Length; i++)
                {
                    int v = src[i];
                    if (!map.TryGetValue(v, out int n))
                    {
                        n = pos.Count; map[v] = n;
                        pos.Add(am.positions[v] - pivot); nrm.Add(am.normals[v]); uv0.Add(am.uv0[v]); uv1.Add(am.uv1[v]);
                        if (am.colors != null) col.Add(am.colors[v]);
                        if (am.tangents != null) tan.Add(am.tangents[v]);
                    }
                    dst[i] = n;
                }
                tris.Add(dst);
            }
            var mesh = new Mesh { name = "Hovercraft " + name, hideFlags = HideFlags.DontSave, indexFormat = pos.Count > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16 };
            mesh.SetVertices(pos); mesh.SetNormals(nrm); mesh.SetUVs(0, uv0); mesh.SetUVs(1, uv1);
            if (am.colors != null) mesh.SetColors(col);
            if (am.tangents != null) mesh.SetTangents(tan);
            mesh.subMeshCount = tris.Count;
            for (int s = 0; s < tris.Count; s++) mesh.SetTriangles(tris[s], s, false);
            mesh.RecalculateBounds();
            var bnd = mesh.bounds; bnd.Expand(8f); mesh.bounds = bnd;
            if (dynamic) mesh.MarkDynamic();
            return mesh;
        }

        void SetupSkirt(Mesh mesh)
        {
            skirtMesh = mesh;
            var v = new List<Vector3>(); mesh.GetVertices(v); skirtOrig = v.ToArray();
            var n = new List<Vector3>(); mesh.GetNormals(n); skirtNormalsOrig = n.ToArray();
            int c = skirtOrig.Length;
            skirtPos = new Vector3[c]; skirtNormals = new Vector3[c]; skirtWeight = new float[c]; skirtAzimuth = new float[c]; skirtHash = new float[c];
            float span = Mathf.Max(0.1f, spec.skirtTop - spec.skirtBottom);
            for (int i = 0; i < c; i++)
            {
                float d = spec.skirtTop - skirtOrig[i].y;
                skirtWeight[i] = d <= 0f ? 0f : Mathf.Clamp01(d / span);
                skirtAzimuth[i] = Mathf.Atan2(skirtOrig[i].z * 0.5f, skirtOrig[i].x);
                uint h = (uint)i * 2654435761u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
                skirtHash[i] = (h & 0xffff) / 65535f - 0.5f;
            }
        }

        /// The air cushion's state: k 0 flat .. 1 full (a little more: the tubes balloon), flutter 0..1 while running.
        public void SetCushion(float k, float flutter, float time)
        {
            if (skirtMesh == null) return;
            if (Mathf.Approximately(k, lastK) && flutter < 0.005f && lastFlutter < 0.005f) return;
            lastK = k; lastFlutter = flutter; vertexUpdates++;
            float kc = Mathf.Clamp(k, 0f, 1.2f);
            float sy = Mathf.Lerp(0.14f, 1f, Mathf.Min(1f, kc)) + Mathf.Max(0f, kc - 1f) * 0.7f;       // the tubes' height
            float splay = 0.11f * (1f - Mathf.Min(1f, kc)) + Mathf.Max(0f, kc - 1f) * 0.22f;            // slumped outwards when flat, bulged when over full
            float wrinkle = (1f - Mathf.Min(1f, kc)) * (1f - Mathf.Min(1f, kc)) * 0.9f;
            float top = spec.skirtTop;
            for (int i = 0; i < skirtOrig.Length; i++)
            {
                var p = skirtOrig[i]; float u = skirtWeight[i];
                if (u <= 0f) { skirtPos[i] = p; skirtNormals[i] = skirtNormalsOrig[i]; continue; }
                float local = sy * (1f + flutter * 0.035f * Mathf.Sin(skirtAzimuth[i] * 4f + time * 17f + skirtHash[i] * 6f) * u);
                float radial = 1f + splay * Mathf.Pow(u, 1.3f) + flutter * 0.012f * Mathf.Sin(skirtAzimuth[i] * 6f - time * 23f) * u;
                p.y = top - (top - p.y) * local;
                p.x = p.x * radial + skirtHash[i] * wrinkle * u * 0.8f; p.z = p.z * radial + skirtHash[(i * 7 + 3) % skirtHash.Length] * wrinkle * u * 0.8f;
                skirtPos[i] = p;
                var n = skirtNormalsOrig[i];
                var squashed = new Vector3(n.x / radial, n.y / Mathf.Max(0.05f, local), n.z / radial).normalized;
                skirtNormals[i] = Vector3.Slerp(n, squashed, u).normalized;
            }
            skirtMesh.vertices = skirtPos; skirtMesh.normals = skirtNormals;
        }

        /// The moving parts: fan speed (rad/s), the rudders' command (-1..1), whether the radar runs.
        public void Animate(float dt, float fanSpeed, float steer, bool radar)
        {
            fanAngle += fanSpeed * dt;
            for (int i = 0; i < fans.Count; i++) fans[i].localRotation = Quaternion.AngleAxis(fanAngle * Mathf.Rad2Deg * (i < fanDir.Count ? fanDir[i] : 1f), Vector3.forward);
            rudder += (steer - rudder) * (1f - Mathf.Exp(-6f * dt));
            foreach (var r in rudders) r.localRotation = Quaternion.AngleAxis(-rudder * 22f, Vector3.up);
            if (radar) radarAngle += dt * 140f;
            foreach (var s in spinners) s.localRotation = Quaternion.AngleAxis(radarAngle, Vector3.up);
        }

        /// The model-space point on the deck where a node-less thing (the rider) stands.
        public Vector3 Seat => spec.seat;

        public void SetVisible(bool on) { if (root) root.SetActive(on); }

        public void Destroy()
        {
            if (root) UnityEngine.Object.Destroy(root);
            foreach (var o in made) if (o) UnityEngine.Object.Destroy(o);
            made.Clear(); root = null;
        }
    }
}
