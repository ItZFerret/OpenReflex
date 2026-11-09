// TEST ONLY: the MOTOCARD portrait and the hub's 3D rider after gear was changed under UNLOCK EVERYTHING and the mode was turned off again
// (the report: the portrait kept gear the hub's rider no longer wore).
//   motocard-unlock-gear   career hub -> garage RIDER: UNLOCK EVERYTHING on (the page's own toggle) -> GEAR: a piece of each slot the career
//                          does not own -> hub -> garage RIDER: UNLOCK EVERYTHING off -> hub. At each hub: a screenshot, the portrait's own
//                          texture, and the gear the profile, the hero and the portrait carry (they must be the same); the portrait is
//                          then compared with a fresh build of the same rider (the redraw in place must not mix the old outfit in).
// Run: run.py <Showcase.app> --seed-dir testdata/moto_career_flow --pages motocard-unlock-gear   (a scratch save; never the user's)
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseMotocardGear
    {
        const string Tag = "[ui-showcase] [unlock-gear] ";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("motocard-unlock-gear", Run);

        static int failures;

        static IEnumerator Hub(string shot)
        {
            var shell = OpenShell.Instance;
            shell.ShowMain();
            float t0 = Time.realtimeSinceStartup;
            while (OpenMenuWorld.Instance == null && Time.realtimeSinceStartup - t0 < 10f) yield return null;
            var w = OpenMenuWorld.Instance;
            while (w != null && w.Status == OpenMenuWorld.State.Loading && Time.realtimeSinceStartup - t0 < 120f) yield return null;
            for (float t1 = Time.realtimeSinceStartup; w != null && (w.Hero == null || w.Hero.RiderModel == null) && Time.realtimeSinceStartup - t1 < 30f;) yield return null;
            for (float t1 = Time.realtimeSinceStartup; !shell.Portrait.Ready && Time.realtimeSinceStartup - t1 < 20f;) yield return null;
            yield return UiShowcase.Frames(30);
            w?.SetWalkTime(99f);
            yield return new WaitForSecondsRealtime(1.5f);
            var card = shell.Layer.Q<VisualElement>("ou-garage-card"); if (card != null) shell.Nav.Focus(card);
            yield return UiShowcase.Frames(10);
            Check(shot);
            yield return UiShowcase.Shot(shot);
            var drawn = SavePortrait(shell.Portrait, shot + "-portrait");
            // the same rider built from nothing (a new portrait stage): the picture drawn after the change must be that picture, not the
            // previous rider and the new one drawn over each other (the keys matched while the picture mixed both outfits)
            shell.Portrait.Destroy(); shell.Portrait.Refresh(shell.MenuWorld);
            var fresh = SavePortrait(shell.Portrait, shot + "-portrait-fresh");
            float diff = Difference(drawn, fresh);
            bool same = diff < MixedLimit;
            if (!same) failures++;
            Debug.Log(Tag + $"{shot}: portrait against a fresh build of the same rider: mean difference {diff:F2} (limit {MixedLimit}) -> {(same ? "SAME" : "MIXED")}");
            shell.ShowMain(); yield return UiShowcase.Frames(10);   // (the hub card takes the new picture)
        }

        /// The mean difference (0..255 per channel) above which two portraits of one rider are different pictures: a mixed one measured 8.6 - 10.
        const float MixedLimit = 3f;
        static float Difference(Color32[] a, Color32[] b)
        {
            if (a == null || b == null || a.Length != b.Length) return 255f;
            double sum = 0;
            for (int i = 0; i < a.Length; i++) sum += Math.Abs(a[i].r - b[i].r) + Math.Abs(a[i].g - b[i].g) + Math.Abs(a[i].b - b[i].b);
            return (float)(sum / (3.0 * a.Length));
        }

        /// The gear the profile has equipped, the gear the hero stands in and the gear the portrait was drawn with: one and the same.
        static void Check(string at)
        {
            var shell = OpenShell.Instance; var w = OpenMenuWorld.Instance;
            string profile = string.Join(",", OpenGarageData.EquippedGearIds());
            var heroGear = w?.Hero?.ShownRider?.gearIds; string hero = heroGear == null ? "none" : string.Join(",", heroGear);
            string key = shell.Portrait.Key ?? "none";
            string portrait = key.StartsWith("own:") ? key.Substring(4, key.IndexOf('|') - 4) : key;
            bool ok = profile == hero && profile == portrait;
            if (!ok) failures++;
            Debug.Log(Tag + $"{at}: unlock {CoreGarageProfile.UnlockEverythingEnabled}, profile gear [{profile}], hero [{hero}], portrait [{portrait}] ready {shell.Portrait.Ready} -> {(ok ? "SAME" : "DIFFERENT")}");
        }

        static Color32[] SavePortrait(OpenRiderPortrait p, string name)
        {
            if (p?.Texture == null) { Debug.Log(Tag + "no portrait texture"); return null; }
            var prev = RenderTexture.active; RenderTexture.active = p.Texture;
            var t = new Texture2D(p.Texture.width, p.Texture.height, TextureFormat.RGBA32, false); t.ReadPixels(new Rect(0, 0, t.width, t.height), 0, 0); t.Apply();
            RenderTexture.active = prev;
            string dir = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (!string.IsNullOrEmpty(dir)) System.IO.File.WriteAllBytes(System.IO.Path.Combine(dir, name + ".png"), t.EncodeToPNG());
            var pixels = t.GetPixels32();
            UnityEngine.Object.Destroy(t);
            return pixels;
        }

        /// The RIDER page's own UNLOCK EVERYTHING handler (OpenShell.SetGarageUnlocks), as the toggle runs it.
        static IEnumerator Toggle(bool on)
        {
            var shell = OpenShell.Instance;
            shell.ShowGarage(OpenShell.GarageRider);
            yield return UiShowcase.Frames(60);
            typeof(OpenShell).GetMethod("SetGarageUnlocks", BindingFlags.Instance | BindingFlags.NonPublic).Invoke(shell, new object[] { on });
            yield return UiShowcase.Frames(60);
            Debug.Log(Tag + $"UNLOCK EVERYTHING {(on ? "on" : "off")}: now {CoreGarageProfile.UnlockEverythingEnabled}, error {CoreGarageProfile.Error ?? "none"}");
        }

        static IEnumerator Run()
        {
            failures = 0;
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            if (CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(false)) throw new InvalidOperationException(CoreGarageProfile.Error);
            OpenMenuWorld.Still = true; FeGarage.RiderPlays = false;
            ReflexProfile.Open();
            var career = OpenGarageData.EquippedGearIds();
            var careerOwned = new HashSet<int>(OpenGarageData.GearTypes().SelectMany(t => OpenGarageData.GearItems(t.id)).Where(i => i.Available).Select(i => i.id));
            yield return Hub("unlockgear-1-career");

            yield return Toggle(true);
            var shell = OpenShell.Instance;
            shell.ShowGarage(OpenShell.GarageGear);
            yield return UiShowcase.Frames(60);
            foreach (var type in OpenGarageData.GearTypes())
            {
                // a piece the career does not own (the reported case), else any other wearable one
                var items = OpenGarageData.GearItems(type.id).Where(i => i.Available && !i.equipped && !i.hidden).ToList();
                var pick = items.FirstOrDefault(i => !careerOwned.Contains(i.id) && i.price > 0) ?? items.FirstOrDefault(i => !careerOwned.Contains(i.id));
                if (pick == null) continue;
                bool ok = OpenGarageData.EquipGear(pick);
                Debug.Log(Tag + $"equip {type.name} {pick.id} '{pick.title}' (career owns it: {careerOwned.Contains(pick.id)}): {ok}");
            }
            shell.ShowGarage(OpenShell.GarageGear);
            yield return UiShowcase.Frames(60);
            yield return Hub("unlockgear-2-unlocked");

            yield return Toggle(false);
            yield return Hub("unlockgear-3-career-again");
            string after = string.Join(",", OpenGarageData.EquippedGearIds());
            Debug.Log(Tag + $"career gear before [{string.Join(",", career)}], after [{after}]");
            if (failures > 0) throw new InvalidOperationException(failures + " views drew other gear than the profile has");
            Debug.Log(Tag + "PASS every view drew the profile's gear");
        }
    }
}
