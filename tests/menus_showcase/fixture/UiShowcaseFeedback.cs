// TEST ONLY (tests/menus_showcase, w-fb-models): the playtester feedback checks of the 3D menu.
//   fb-ground   the hub's hero (the equipped vehicle, then a pro ride of each other vehicle type: ATV, UTV, ...): the vehicle's lowest
//               points (the stand / tyres) and the rider's soles against the ground as drawn there (the terrain's height + RT_Bump's
//               displacement, read back from the near target), logged per vehicle, and low close shots of the stand and the boots
//   fb-facet    the hero close up under the menu's key light: the exhaust, the helmet (the bikes' polygons looked flat-shaded)
//   fb-bridge   a race level's scenery built like the menu world's (REFLEX_FB_LEVEL, default NAT_Renne: the Omnicross events' aqueduct,
//               NAT_RN_LargeBridge), the camera around REFLEX_FB_AT (x,y,z; default the bridge), its materials logged
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using UnityEngine;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseFeedback
    {
        const string Tag = "[ui-showcase] [fb] ";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("fb-ground", Ground);
            UiShowcase.Register("fb-facet", Facet);
            UiShowcase.Register("fb-bridge", Bridge);
        }

        static void Profile()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            UiShowcaseGarage.Credits(250000);
            ReflexProfile.Open();
        }

        static IEnumerator Hub()
        {
            Profile();
            OpenMenuWorld.Still = true; Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            OpenShell.Instance.ShowMain();
            float t0 = Time.realtimeSinceStartup;
            while (OpenMenuWorld.Instance == null && Time.realtimeSinceStartup - t0 < 10f) yield return null;
            var w = OpenMenuWorld.Instance; if (w == null) { Debug.Log(Tag + "FAIL no menu world"); yield break; }
            while (w.Status == OpenMenuWorld.State.Loading && Time.realtimeSinceStartup - t0 < 120f) yield return null;
            while ((w.Hero == null || w.Hero.RiderModel == null) && Time.realtimeSinceStartup - t0 < 150f) yield return null;
            for (int i = 0; i < 10; i++) { if (w.Walking) w.SetWalkTime(99f); yield return null; }
            for (int i = 0; i < 60; i++) yield return null;
        }

        // ------------------------------------------------------------------------------------------------ the ground

        static IEnumerator Ground()
        {
            yield return Hub();
            var w = OpenMenuWorld.Instance; if (w == null || w.Status != OpenMenuWorld.State.Ready) yield break;
            yield return MeasureAndShoot(w, "equipped");
            var rides = OpenGarageData.ProRides();
            Debug.Log(Tag + "pro rides: " + string.Join(", ", rides.GroupBy(r => r.vehicleType).Select(g => g.Key + ":" + g.Count())));
            foreach (var g in rides.GroupBy(r => r.vehicleType).OrderBy(g => g.Key))
            {
                var ride = g.First();
                var (bike, rider) = OpenGarageData.ProSpecs(ride);
                if (bike == null) continue;
                string kind = bike.atv ? "atv" : bike.machine ? "machine" : "bike";
                if (kind == "bike" && g.Key != rides.Min(r => r.vehicleType)) continue;   // (one motocross bike is enough: the equipped one above)
                w.ShowHero(bike, rider, "fb:" + ride.rider + ":" + ride.vehicle, false);
                for (int i = 0; i < 10; i++) { if (w.Walking) w.SetWalkTime(99f); yield return null; }
                for (int i = 0; i < 60; i++) yield return null;
                yield return MeasureAndShoot(w, "type" + g.Key + "-" + kind);
            }
        }

        static IEnumerator MeasureAndShoot(OpenMenuWorld w, string label)
        {
            var hero = w.Hero; var root = hero.root.transform;
            var riderRoot = hero.RiderModel?.root ? hero.RiderModel.root.transform : null;
            yield return new WaitForEndOfFrame();
            var near = NearReadback(w);
            var bikePts = new List<Vector3>(); float bikeLow = float.MaxValue;
            foreach (var mf in root.GetComponentsInChildren<MeshFilter>())
            {
                if (riderRoot && mf.transform.IsChildOf(riderRoot)) continue;
                var mr = mf.GetComponent<MeshRenderer>(); if (!mr || !mr.enabled || !mf.sharedMesh || !mf.sharedMesh.isReadable) continue;
                var m = mf.transform.localToWorldMatrix;
                foreach (var v in mf.sharedMesh.vertices) { var p = m.MultiplyPoint3x4(v); bikePts.Add(p); bikeLow = Mathf.Min(bikeLow, p.y); }
            }
            var soles = new List<Vector3>(); float soleLow = float.MaxValue;
            if (riderRoot && riderRoot.gameObject.activeInHierarchy)
            {
                var baked = new Mesh();
                foreach (var smr in riderRoot.GetComponentsInChildren<SkinnedMeshRenderer>())
                {
                    if (!smr.enabled || !smr.sharedMesh) continue;
                    smr.BakeMesh(baked, false); var m = smr.transform.localToWorldMatrix;
                    foreach (var v in baked.vertices) { var p = m.MultiplyPoint3x4(v); soles.Add(p); soleLow = Mathf.Min(soleLow, p.y); }
                }
                UnityEngine.Object.Destroy(baked);
            }
            Report(w, near, label + " vehicle", bikePts, bikeLow);
            Report(w, near, label + " rider", soles, soleLow);
            // the close shots: from the camera's side, low, at the vehicle's lowest points and at the soles
            UiShowcase.ShellVisible(false);
            OpenMenuWorld.ManualCamera = true;
            var cam = w.Camera; var keepPos = cam.transform.position; var keepRot = cam.transform.rotation; float keepFov = cam.fieldOfView;
            var fwd = cam.transform.forward; fwd.y = 0; fwd.Normalize();
            foreach (var (what, pts, low) in new[] { ("vehicle", bikePts, bikeLow), ("rider", soles, soleLow) })
            {
                var sel = pts.Where(p => p.y < low + 0.15f).ToList(); if (sel.Count == 0) continue;
                var c = new Vector3(sel.Average(p => p.x), low, sel.Average(p => p.z));
                cam.fieldOfView = 30;
                cam.transform.SetPositionAndRotation(c - fwd * 7f + Vector3.up * 0.9f, Quaternion.LookRotation((c + Vector3.up * 0.25f) - (c - fwd * 7f + Vector3.up * 0.9f), Vector3.up));
                yield return UiShowcase.Frames(8);
                yield return UiShowcase.Shot("fb-ground-" + label + "-" + what);
            }
            cam.fieldOfView = 40;
            var all = bikePts.Concat(soles).ToList();
            if (all.Count > 0)
            {
                var c = new Vector3(all.Average(p => p.x), Mathf.Min(bikeLow, soleLow), all.Average(p => p.z));
                cam.transform.SetPositionAndRotation(c - fwd * 13f + Vector3.up * 1.6f, Quaternion.LookRotation((c + Vector3.up * 1.4f) - (c - fwd * 13f + Vector3.up * 1.6f), Vector3.up));
                yield return UiShowcase.Frames(8);
                yield return UiShowcase.Shot("fb-ground-" + label + "-low");
            }
            cam.transform.SetPositionAndRotation(keepPos, keepRot); cam.fieldOfView = keepFov;
            OpenMenuWorld.ManualCamera = false;
            UiShowcase.ShellVisible(true);
            yield return UiShowcase.Frames(8);
            yield return UiShowcase.Shot("fb-ground-" + label + "-hub");
        }

        /// The ground as drawn under the lowest points of a model: the terrain's height there and RT_Bump's displacement (the near target's blue,
        /// averaged over two height-map cells like the displacement's mip), how far each point is below / above it.
        static void Report(OpenMenuWorld w, Func<float, float, float> near, string what, List<Vector3> pts, float low)
        {
            if (pts.Count == 0) { Debug.Log(Tag + what + ": no points"); return; }
            var sel = pts.Where(p => p.y < low + 0.15f).ToList();
            float spotY = w.GroundAt(OpenMenuWorld.HeroSpot.x, OpenMenuWorld.HeroSpot.z);
            float maxTerrain = float.MinValue, maxDrawn = float.MinValue, minTerrain = float.MaxValue, maxBump = 0;
            foreach (var p in sel)
            {
                float g = w.GroundAt(p.x, p.z), b = near != null ? near(p.x, p.z) : 0f;
                maxTerrain = Mathf.Max(maxTerrain, g - p.y); minTerrain = Mathf.Min(minTerrain, g - p.y); maxDrawn = Mathf.Max(maxDrawn, g + b - p.y); maxBump = Mathf.Max(maxBump, b);
            }
            Debug.Log(Tag + $"{what}: lowest y {low:F3} (hero spot ground {spotY:F3}, root {w.Hero.root.transform.position.y:F3}); {sel.Count} points within 0.15 ft: " +
                      $"terrain above them by {minTerrain:F3} .. {maxTerrain:F3} ft, RT_Bump displacement up to {maxBump:F3} ft, drawn ground above the lowest by up to {maxDrawn:F3} ft" +
                      (maxDrawn > 0.03f ? " SINKS" : " ok"));
        }

        /// The near RT_Bump target's displacement (blue) at world x / z, box-averaged over +-1 height-map cell; null without one.
        static Func<float, float, float> NearReadback(OpenMenuWorld w)
        {
            var rb = UnityEngine.Object.FindAnyObjectByType<Reflex.Port.Rendering.ReflexRTBump>();
            if (!rb) { Debug.Log(Tag + "no RT_Bump"); return null; }
            var rt = typeof(Reflex.Port.Rendering.ReflexRTBump).GetField("rtNear", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(rb) as RenderTexture;
            if (!rt) { Debug.Log(Tag + "no near target"); return null; }
            float span = Reflex.Port.Rendering.ReflexRTBump.NearSpan; int size = rt.width; float texel = span / size;
            var c = w.Camera; var fit = rb.LastFit;
            var centre = new Vector2(c.transform.position.x, c.transform.position.z) + fit.forward * (span * Reflex.Port.Rendering.ReflexRTBump.NearAhead);
            float x0 = Mathf.Floor((centre.x - 0.5f * span) / texel) * texel, z0 = Mathf.Floor((centre.y - 0.5f * span) / texel) * texel;
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var tex = new Texture2D(size, size, TextureFormat.RGBA32, false, true); tex.ReadPixels(new Rect(0, 0, size, size), 0, 0); tex.Apply();
            RenderTexture.active = prev;
            var px = tex.GetPixels32(); UnityEngine.Object.Destroy(tex);
            var t = Terrain.activeTerrain; float cell = t ? t.terrainData.heightmapScale.x : 1f;
            int r = Mathf.Max(0, Mathf.RoundToInt(cell / texel));
            Debug.Log(Tag + $"near target {size}x{size} over {span} at ({x0:F2}, {z0:F2}); terrain cell {cell:F3}, averaging +-{r} texels");
            return (x, z) =>
            {
                int u = Mathf.FloorToInt((x - x0) / texel), v = Mathf.FloorToInt((z - z0) / texel);
                if (u < 0 || v < 0 || u >= size || v >= size) return 0f;
                float sum = 0; int n = 0;
                for (int dv = -r; dv <= r; dv += Mathf.Max(1, r / 4)) for (int du = -r; du <= r; du += Mathf.Max(1, r / 4))
                {
                    int uu = Mathf.Clamp(u + du, 0, size - 1), vv = Mathf.Clamp(v + dv, 0, size - 1);
                    sum += px[vv * size + uu].b / 255f; n++;
                }
                return sum / n;
            };
        }

        // ------------------------------------------------------------------------------------------------ a level's scenery

        static IEnumerator Bridge()
        {
            yield return Hub();
            var w = OpenMenuWorld.Instance; if (w) w.gameObject.SetActive(false);
            UiShowcase.ShellVisible(false);
            string level = Environment.GetEnvironmentVariable("REFLEX_FB_LEVEL"); if (string.IsNullOrEmpty(level)) level = "NAT_Renne";
            var at = new Vector3(-215.4f, 100.2f, 860.7f);
            var atEnv = Environment.GetEnvironmentVariable("REFLEX_FB_AT");
            if (!string.IsNullOrEmpty(atEnv)) { var a = atEnv.Split(',').Select(float.Parse).ToArray(); at = new Vector3(a[0], a[1], a[2]); }
            var task = System.Threading.Tasks.Task.Run(() =>
            {
                var c = ReflexLevelContent.Load(level, new ReflexLevelContent.Options { activity = "FreeRide", scenery = true, terrain = true, tracks = false, ambientLight = "AmbientDirectionalLight" });
                c.TerrainHeights(out var h, out int n, out int sp);
                return (c, h, n, sp);
            });
            while (!task.IsCompleted) yield return null;
            if (task.IsFaulted) { Debug.Log(Tag + "FAIL level " + level + ": " + task.Exception); yield break; }
            var r = task.Result;
            var root = new GameObject("fb-bridge TEST " + level);
            var cgo = new GameObject("fb camera"); cgo.transform.SetParent(root.transform, false);
            var cam = cgo.AddComponent<Camera>(); cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = Color.black; cam.fieldOfView = 55; cam.nearClipPlane = 0.5f; cam.farClipPlane = 8000f; cam.allowHDR = true; cgo.tag = "MainCamera";
            var sgo = new GameObject("fb sun"); sgo.transform.SetParent(root.transform, false);
            var sun = sgo.AddComponent<Light>(); sun.type = LightType.Directional; sun.intensity = 2.3f; RenderSettings.sun = sun;
            var tgo = new GameObject("fb terrain"); tgo.transform.SetParent(root.transform, false);
            var terrain = tgo.AddComponent<OriginalTerrain>();
            var lgo = new GameObject("fb level " + level); lgo.SetActive(false); lgo.transform.SetParent(root.transform, false);
            var lv = lgo.AddComponent<OriginalLevel>(); lv.loadOnAwake = false; lv.terrain = terrain; lv.gameCamera = cam; lv.animateGates = false; lv.level = level;
            lgo.SetActive(true);
            lv.LoadFrom(r.c);
            if (r.h != null)
            {
                float half = (r.n - 1) * 0.5f * r.sp;
                terrain.Build(new ReflexTerrainInfo { samples = (uint)r.n, stride = (uint)r.n, spacing = r.sp, min_x = -half, min_z = -half, max_x = half, max_z = half, gain = 2f }, r.h, null);
            }
            for (int i = 0; i < 20; i++) yield return null;
            // the materials of the renderers near the spot whose name or material says bridge
            foreach (var mr in lv.SceneryRoot.GetComponentsInChildren<MeshRenderer>(true))
            {
                if ((mr.bounds.center - at).magnitude > 400f) continue;
                if (mr.name.IndexOf("Bridge", StringComparison.OrdinalIgnoreCase) < 0 && !mr.sharedMaterials.Any(m => m && m.name.IndexOf("Bridge", StringComparison.OrdinalIgnoreCase) >= 0)) continue;
                foreach (var m in mr.sharedMaterials)
                {
                    if (!m) continue;
                    var sb = new System.Text.StringBuilder();
                    foreach (var tn in m.GetTexturePropertyNames()) { var tx = m.GetTexture(tn); if (tx) sb.Append($" {tn}={tx.name}"); }
                    foreach (var fn in new[] { "_Mode", "_AOMode", "_AOAmount", "_DiffuseUV", "_DiffuseSrc", "_NormalMode" }) if (m.HasProperty(fn)) sb.Append($" {fn}={m.GetFloat(fn)}");
                    if (m.HasProperty("_FsParams")) sb.Append($" _FsParams={m.GetVector("_FsParams")}");
                    Debug.Log(Tag + $"bridge renderer {mr.name} at {mr.bounds.center:F1} size {mr.bounds.size:F1}: {m.name} [{m.shader.name}]{sb}");
                }
            }
            foreach (var line in lv.Content.DescribeMaterials().Split('\n')) if (line.IndexOf("Bridge", StringComparison.OrdinalIgnoreCase) >= 0) Debug.Log(Tag + "material " + line);
            foreach (var (name, off, lookY) in new[] { ("south", new Vector3(0, 25, -230), 25f), ("east", new Vector3(230, 25, 0), 25f), ("west", new Vector3(-230, 25, 0), 25f), ("north", new Vector3(0, 25, 230), 25f), ("close", new Vector3(90, 10, -90), 15f) })
            {
                var eye = at + off; var tr = terrain.UnityTerrain;
                if (tr) eye.y = Mathf.Max(eye.y, tr.SampleHeight(eye) + tr.transform.position.y + 6f);
                cam.transform.SetPositionAndRotation(eye, Quaternion.LookRotation(at + Vector3.up * lookY - eye, Vector3.up));
                yield return UiShowcase.Frames(10);
                yield return UiShowcase.Shot("fb-bridge-" + level + "-" + name);
            }
            UnityEngine.Object.Destroy(root);
            if (w) w.gameObject.SetActive(true);
            UiShowcase.ShellVisible(true);
        }

        // ------------------------------------------------------------------------------------------------ the shading

        static IEnumerator Facet()
        {
            yield return Hub();
            var w = OpenMenuWorld.Instance; if (w == null || w.Status != OpenMenuWorld.State.Ready) yield break;
            yield return UiShowcase.Shot("fb-facet-hub");
            UiShowcase.ShellVisible(false);
            OpenMenuWorld.ManualCamera = true;
            var cam = w.Camera; var fwd = cam.transform.forward; fwd.y = 0; fwd.Normalize();
            var rs = w.Hero.root.GetComponentsInChildren<Renderer>();
            foreach (var (what, key, dist) in new[] { ("exhaust", "Exh", 3.2f), ("helmet", "Helmet", 2.6f), ("tank", "Plstc", 4.5f), ("handguards", "guard", 3f) })
            {
                var hits = rs.Where(r => r.name.IndexOf(key, StringComparison.OrdinalIgnoreCase) >= 0).ToList();
                Debug.Log(Tag + $"{what}: {hits.Count} renderers ({string.Join(", ", hits.Take(4).Select(r => r.name))})");
                if (hits.Count == 0) continue;
                var b = hits[0].bounds; foreach (var h in hits) b.Encapsulate(h.bounds);
                cam.fieldOfView = 30;
                var eye = b.center - fwd * dist + Vector3.up * 0.4f;
                cam.transform.SetPositionAndRotation(eye, Quaternion.LookRotation(b.center - eye, Vector3.up));
                yield return UiShowcase.Frames(8);
                yield return UiShowcase.Shot("fb-facet-" + what);
            }
            OpenMenuWorld.ManualCamera = false;
            UiShowcase.ShellVisible(true);
        }
    }
}
