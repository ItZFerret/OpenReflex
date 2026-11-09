// TEST ONLY: w-menufix pages over the 3D menu world (OpenFrontEnd/OpenMenuWorld).
//   menufix-boards   the compound's four status boards (UI_COM_SBoard_01) seen from the front, and what their materials hold (log)
//   menufix-avatar   the hub's MOTOCARD card and the garage RIDER tab with the rider's portrait
using System;
using System.Collections;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseMenuFix
    {
        const string Tag = "[ui-showcase] [menufix] ";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("menufix-boards", Boards);
            UiShowcase.Register("menufix-avatar", Avatar);
            UiShowcase.Register("menufix-portrait", PortraitVariants);
            UiShowcase.Register("menufix-hubyaw", HubYaw);
        }

        static IEnumerator Ready()
        {
            float t0 = Time.realtimeSinceStartup;
            while (OpenMenuWorld.Instance == null && Time.realtimeSinceStartup - t0 < 10f) yield return null;
            var w = OpenMenuWorld.Instance;
            if (w == null) { Debug.Log(Tag + "no menu world"); yield break; }
            while (w.Status == OpenMenuWorld.State.Loading && Time.realtimeSinceStartup - t0 < 120f) yield return null;
        }

        static float Avg(Texture t)
        {
            if (!t) return -1f;
            var rt = RenderTexture.GetTemporary(8, 8, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
            Graphics.Blit(t, rt);
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var r = new Texture2D(8, 8, TextureFormat.RGBA32, false); r.ReadPixels(new Rect(0, 0, 8, 8), 0, 0); r.Apply();
            RenderTexture.active = prev; RenderTexture.ReleaseTemporary(rt);
            float s = 0; foreach (var c in r.GetPixels()) s += (c.r + c.g + c.b) / 3f; UnityEngine.Object.Destroy(r);
            return s / 64f;
        }

        static void Profile()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            UiShowcaseGarage.Credits(250000);
            ReflexProfile.Open();
        }

        static IEnumerator Boards()
        {
            Profile();
            OpenMenuWorld.Still = true; Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            OpenShell.Instance.ShowMain();
            yield return Ready();
            var w = OpenMenuWorld.Instance; if (w == null || w.Status != OpenMenuWorld.State.Ready) yield break;
            for (float t0 = Time.realtimeSinceStartup; w.Boards == null || w.Boards.Shown == null; ) { if (Time.realtimeSinceStartup - t0 > 20f) break; yield return null; }
            Debug.Log(Tag + $"boards: {w.Boards?.Count} faces, showing '{w.Boards?.Shown}'");
            UiShowcase.ShellVisible(false);
            OpenMenuWorld.ManualCamera = true;
            var boards = w.LevelObject.SceneryRoot.GetComponentsInChildren<MeshRenderer>(true).Where(m => m.name.StartsWith("UI_COM_SBoard_01")).ToList();
            Debug.Log(Tag + boards.Count + " board renderers");
            foreach (var mr in boards.Take(1))
                foreach (var m in mr.sharedMaterials)
                {
                    var bm = m.HasProperty("_BaseMap") ? m.GetTexture("_BaseMap") : null;
                    Debug.Log(Tag + $"material {m.name}: base {(bm ? bm.name + " " + bm.width + "x" + bm.height : "none")} avg {Avg(bm):F3}; _Mode {m.GetFloat("_Mode")} _PrelitPower {m.GetFloat("_PrelitPower")} _BaseColor {(m.HasProperty("_BaseColor") ? m.GetColor("_BaseColor").ToString() : "-")}");
                }
            int n = 0;
            foreach (var mr in boards)
            {
                var b = mr.bounds; var c = b.center;
                var toCentre = new Vector3(-c.x, 0, -c.z).normalized;
                var cam = w.Camera;
                var eye = c + toCentre * 190f + Vector3.up * 5f;
                cam.transform.SetPositionAndRotation(eye, Quaternion.LookRotation(c - eye, Vector3.up));
                cam.fieldOfView = 40;
                yield return UiShowcase.Frames(12);
                yield return UiShowcase.Shot("menufix-board-" + n++);
                if (n == 1) { cam.fieldOfView = 55; yield return UiShowcase.Frames(6); yield return UiShowcase.Shot("menufix-board-0-wide"); cam.fieldOfView = 40; }
            }
            {   // another event under the cursor
                var other = OpenContent.Events.FirstOrDefault(e => e.key != w.Boards?.Shown && e.typeName != null && e.typeName.ToUpperInvariant().Contains("SUPERCROSS")) ?? OpenContent.Events.Skip(5).FirstOrDefault();
                if (other != null && w.Boards != null)
                {
                    w.Boards.Show(other);
                    var mr = boards[0]; var b = mr.bounds; var c = b.center; var toCentre = new Vector3(-c.x, 0, -c.z).normalized;
                    var eye = c + toCentre * 190f + Vector3.up * 5f; w.Camera.transform.SetPositionAndRotation(eye, Quaternion.LookRotation(c - eye, Vector3.up));
                    yield return UiShowcase.Frames(12);
                    yield return UiShowcase.Shot("menufix-board-other");
                }
            }
            OpenMenuWorld.ManualCamera = false;
            UiShowcase.ShellVisible(true);
        }

        static IEnumerator Avatar()
        {
            Profile();
            OpenMenuWorld.Still = true; Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            var shell = OpenShell.Instance;
            shell.ShowMain();
            yield return Ready();
            var w = OpenMenuWorld.Instance;
            for (float t0 = Time.realtimeSinceStartup; w != null && (w.Hero == null || w.Hero.RiderModel == null) && Time.realtimeSinceStartup - t0 < 30f;) yield return null;
            for (float t0 = Time.realtimeSinceStartup; !shell.Portrait.Ready && Time.realtimeSinceStartup - t0 < 20f;) yield return null;
            w?.SetWalkTime(99f);
            yield return new WaitForSecondsRealtime(3f);
            Debug.Log(Tag + $"portrait ready {shell.Portrait.Ready}, key {shell.Portrait.Key}, built in {shell.Portrait.LastBuildMs:F0} ms");
            SavePortrait(shell.Portrait, "menufix-portrait-raw");
            LogBoards("hub");
            yield return UiShowcase.Shot("menufix-hub");
            shell.ShowGarage(OpenShell.GarageRider);
            yield return new WaitForSecondsRealtime(3f);
            w?.SetWalkTime(99f);
            LogBoards("garage RIDER");
            yield return UiShowcase.Shot("menufix-garage-rider");
            shell.ShowGarage(OpenShell.GarageVehicle);
            yield return new WaitForSecondsRealtime(3f);
            LogBoards("garage VEHICLE");
            yield return UiShowcase.Shot("menufix-garage-vehicle");
            shell.ShowGarage(OpenShell.GarageGear);
            yield return new WaitForSecondsRealtime(3f);
            LogBoards("garage GEAR");
            yield return UiShowcase.Shot("menufix-garage-gear");
        }

        static IEnumerator PortraitVariants()
        {
            Profile();
            OpenMenuWorld.Still = true; Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            var shell = OpenShell.Instance;
            shell.ShowMain();
            yield return Ready();
            for (float t0 = Time.realtimeSinceStartup; !shell.Portrait.Ready && Time.realtimeSinceStartup - t0 < 30f;) yield return null;
            int n = 0;
            foreach (var v in new[] { (0f, 24f, 7.2f, 0.95f, 0.35f, 20f), (25f, 24f, 7.2f, 0.95f, 0.35f, 20f), (35f, 12f, 7.6f, 1.0f, 0.35f, 22f), (45f, 0f, 7.8f, 1.1f, 0.3f, 22f), (-25f, 24f, 7.2f, 0.95f, 0.35f, 20f), (60f, 0f, 8f, 1.1f, 0.3f, 22f) })
            {
                shell.Portrait.Reframe(v.Item1, v.Item2, v.Item3, v.Item4, v.Item5, v.Item6);
                yield return UiShowcase.Frames(2);
                SavePortrait(shell.Portrait, "menufix-portrait-v" + n++);
                Debug.Log(Tag + $"variant {n - 1}: yaw {v.Item1} turn {v.Item2} dist {v.Item3} drop {v.Item4} lift {v.Item5} fov {v.Item6}");
            }
        }

        /// Where the boards are on the screen from the world's camera now (viewport 0..1; in view when inside the frame and in front).
        static void LogBoards(string what)
        {
            var w = OpenMenuWorld.Instance; if (w == null || !w.Camera) return;
            int n = 0;
            foreach (var mr in w.LevelObject.SceneryRoot.GetComponentsInChildren<MeshRenderer>(true).Where(m => m.name.StartsWith("UI_COM_SBoard_01")))
            {
                var c = mr.bounds.center; var v = w.Camera.WorldToViewportPoint(c);
                bool inView = v.z > 0 && v.x > -0.05f && v.x < 1.05f && v.y > -0.05f && v.y < 1.05f;
                Debug.Log(Tag + $"{what}: board {n++} at ({c.x:F0}, {c.z:F0}) viewport ({v.x:F2}, {v.y:F2}) depth {v.z:F0} ft " + (inView ? "IN VIEW" : "out of view"));
            }
        }

        static IEnumerator HubYaw()
        {
            Profile();
            OpenMenuWorld.Still = true; Reflex.Port.FrontEnd.FeGarage.RiderPlays = false;
            var shell = OpenShell.Instance;
            shell.ShowMain();
            yield return Ready();
            var w = OpenMenuWorld.Instance;
            for (float t0 = Time.realtimeSinceStartup; w != null && (w.Hero == null || w.Hero.RiderModel == null) && Time.realtimeSinceStartup - t0 < 30f;) yield return null;
            w?.SetWalkTime(99f);
            foreach (float yaw in new[] { 62f, 50f, 42f, 74f, 84f })
            {
                OpenMenuWorld.HubYaw = yaw;
                shell.ShowGarage(OpenShell.GarageRider); yield return UiShowcase.Frames(10);
                shell.ShowMain(); yield return UiShowcase.Frames(10); w?.SetWalkTime(99f); w?.SnapCamera();
                yield return UiShowcase.Frames(40);
                LogBoards("hub yaw " + yaw);
                yield return UiShowcase.Shot("menufix-hubyaw-" + (int)yaw);
            }
        }

        static void SavePortrait(OpenRiderPortrait p, string name)
        {
            if (p?.Texture == null) { Debug.Log(Tag + "no portrait texture"); return; }
            var prev = RenderTexture.active; RenderTexture.active = p.Texture;
            var t = new Texture2D(p.Texture.width, p.Texture.height, TextureFormat.RGBA32, false); t.ReadPixels(new Rect(0, 0, t.width, t.height), 0, 0); t.Apply();
            RenderTexture.active = prev;
            string dir = System.Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (!string.IsNullOrEmpty(dir)) System.IO.File.WriteAllBytes(System.IO.Path.Combine(dir, name + ".png"), t.EncodeToPNG());
            UnityEngine.Object.Destroy(t);
        }
    }
}
