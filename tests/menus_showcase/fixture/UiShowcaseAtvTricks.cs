// TEST ONLY: the MOTOCARD's TRICK LIBRARY preview on an ATV (OpenStuntPreview with the ATV's performances "ATVRider_<stunt>", the rider clips on the
// MXRider skeleton plus the ATV's additive blocks ATVRider_AD_StuntStart / PelvisDown / PelvisBackward, the vehicle clips MXVehicle_ST_*).
//   partials-trick-atv    the profile (a scratch save seeded with the unlocked clone: run.py --seed-save testdata/moto_career_flow/core-player-career-v2.bin)
//                         equips its ATV; the page's preview is built on it; frames of several tricks (the sampled times of each, saved from the picture's own render
//                         texture), the additive blocks' extents, a physics trick (WHEELIE: no perfdat -> "a physics trick"), a trick played with the pad
// Env: ATV_STUNTS (default "BarHop_Air,Superman_Air,NacNac_L_Air,IndianAir_L_Air,KOD_Air,Rodeo_L_Air,NoHander_Air,Shaolin_Air"), ATV_ADDITIVE (default 1: 0 skips the
// additive blocks, to compare what they change).
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseAtvTricks
    {
        const string Tag = "[ui-showcase] [atv-tricks] ";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("partials-trick-atv", Atv); }

        static void Save(RenderTexture rt, string name, bool flipped)
        {
            if (!rt) return;
            string dir = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS"); if (string.IsNullOrEmpty(dir)) return;
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var t = new Texture2D(rt.width, rt.height, TextureFormat.RGBA32, false); t.ReadPixels(new Rect(0, 0, t.width, t.height), 0, 0); t.Apply();
            RenderTexture.active = prev;
            var px = t.GetPixels32();
            for (int i = 0; i < px.Length; i++) { var c = px[i]; float a = c.a / 255f; px[i] = new Color32((byte)(c.r * a + 40 * (1 - a)), (byte)(c.g * a + 44 * (1 - a)), (byte)(c.b * a + 44 * (1 - a)), 255); }
            t.SetPixels32(px); t.Apply();
            File.WriteAllBytes(Path.Combine(dir, name + ".png"), t.EncodeToPNG());
            UnityEngine.Object.Destroy(t);
        }

        static IEnumerator Tap(GamepadButton b, int wait = 6) { yield return UiShowcaseNav.PadTap(b); yield return UiShowcase.Frames(wait); }

        static IEnumerator Atv()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            OpenMenuWorld.Still = true; FeGarage.RiderPlays = false;
            ReflexProfile.Open();
            var shell = OpenShell.Instance;
            // the profile owns and equips an ATV (type 4): the scratch save's own unlock-everything garage mode and a purchase with credits the fixture
            // puts in the scratch profile (the sandbox's save directory, never the player's)
            if (!OpenGarageData.Vehicles().Any(v => v.owned && v.type == 4))
            {
                Debug.Log(Tag + "unlock everything: " + CoreGarageProfile.SetUnlockEverything(true) + " " + CoreGarageProfile.Error);
                var cand = OpenGarageData.Vehicles().FirstOrDefault(v => v.type == 4 && !v.hidden && v.asset >= 0);
                UiShowcaseNav.Check(cand != null, "atv tricks", "the install has no ATV model in the garage");
                if (cand == null) yield break;
                const BindingFlags All = BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public;
                var set = typeof(CoreGarageProfile).GetMethod("Set", All);
                Action<ProfileData> credits = pd => set.Invoke(null, new object[] { pd.T("PlayerRider"), "AccountBalance", 0, 50000000 });
                Debug.Log(Tag + "credits: " + typeof(CoreGarageProfile).GetMethod("Change", All).Invoke(null, new object[] { credits }) + " " + CoreGarageProfile.Error);
                Debug.Log(Tag + "purchase " + cand.title + " (asset " + cand.asset + ", price " + cand.price + "): " + CoreGarageProfile.PurchaseAsset(cand.asset, cand.price) + " " + CoreGarageProfile.Error);
                // the garage's view of the profile (ReflexProfile.Current) takes the change through its own selection call
                int equipped = CoreGarageProfile.Capture().T("PlayerRider").Int("LastEquippedVehicle", 0);
                ReflexProfile.SelectRider(0, -1, equipped).Wait();
            }
            var atv = OpenGarageData.Vehicles().FirstOrDefault(v => v.owned && v.type == 4);
            UiShowcaseNav.Check(atv != null, "atv tricks", "the profile owns no ATV (seed it with the unlocked clone: --seed-save testdata/moto_career_flow/core-player-career-v2.bin)");
            if (atv == null) yield break;
            UiShowcaseNav.Check(ReflexProfile.SelectRider(0, -1, atv.playerVehicle).Result, "atv tricks", "the ATV was not equipped: " + CoreGarageProfile.Error);
            UiShowcaseNav.Check(OpenGarageData.Vehicles().Any(v => v.type == 4 && v.equipped), "atv tricks", "the ATV is not the equipped vehicle");
            shell.ShowMain();
            float t0 = Time.realtimeSinceStartup;
            while (OpenMenuWorld.Instance == null && Time.realtimeSinceStartup - t0 < 10f) yield return null;
            var w = OpenMenuWorld.Instance;
            while (w != null && w.Status == OpenMenuWorld.State.Loading && Time.realtimeSinceStartup - t0 < 120f) yield return null;
            for (float t1 = Time.realtimeSinceStartup; w != null && (w.Hero == null || w.Hero.RiderModel == null) && Time.realtimeSinceStartup - t1 < 30f;) yield return null;
            yield return new WaitForSecondsRealtime(1.0f);

            shell.ShowMotocard(OpenShell.MotocardTricks);
            t0 = Time.realtimeSinceStartup;
            while ((shell.StuntPreview == null || !shell.StuntPreview.Ready) && Time.realtimeSinceStartup - t0 < 60f) yield return null;
            var p = shell.StuntPreview;
            UiShowcaseNav.Check(p != null && p.Ready, "atv tricks", "the preview was not built (" + p?.Problem + ")");
            if (p == null || !p.Ready) yield break;
            yield return new WaitForSecondsRealtime(0.6f);
            Debug.Log(Tag + "preview ready: " + p.Setup + ", ATV " + p.Atv + ", built in " + p.LastBuildMs.ToString("F0") + " ms, flipped " + p.Flipped + ", frames " + p.Frames);
            UiShowcaseNav.Check(p.Atv, "atv tricks", "the preview was not built on the ATV (" + p.Setup + ")");
            yield return UiShowcase.Shot("atv-trick-page");

            var stunts = (Environment.GetEnvironmentVariable("ATV_STUNTS") ?? "BarHop_Air,Superman_Air,NacNac_L_Air,IndianAir_L_Air,KOD_Air,Rodeo_L_Air,NoHander_Air,Shaolin_Air").Split(',');
            bool additive = Environment.GetEnvironmentVariable("ATV_ADDITIVE") != "0";
            OpenStuntPreview.Manual = true;
            var times = new[] { 0f, 0.5f, 0.83f, 1.4f, 2.5f, 3.3f };
            foreach (var stunt in stunts)
            {
                if (!p.SetStunt(stunt)) { Debug.Log(Tag + "no performance for " + stunt + " (" + ReflexStuntAnim.Problem + ")"); continue; }
                if (!additive) foreach (var b in p.Anim.blocks) if (b.additive) b.addMax = 0f;
                float len = p.Anim.Length();
                Debug.Log(Tag + stunt + " (" + (p.Atv ? "ATV" : "MX") + ") length " + len.ToString("F2") + " s: " + string.Join(", ", p.Anim.blocks.Select(b =>
                    b.name + (b.additive ? " [additive start " + b.start.ToString("F2") + " span " + b.Span.ToString("F2") + " rider length " + b.rider.length.ToString("F2") + "]" : b.vehicle != null ? "+v" : ""))));
                foreach (var b in p.Anim.blocks.Where(b => b.additive))
                {
                    b.rider.Extent(out float tr, out float turn);
                    Debug.Log(Tag + "  additive " + b.name + ": largest translation " + tr.ToString("F3") + ", largest turn " + turn.ToString("F1") + " degrees");
                }
                foreach (float t in times)
                {
                    if (t > len + 0.05f) continue;
                    p.Seek(t);
                    yield return null;
                    Save(p.Texture, "atv-trick-" + stunt + "-t" + (int)(t * 100), p.Flipped);
                }
            }
            OpenStuntPreview.Manual = false;

            // the page: a physics trick has no animation and says so; a trick with a performance plays with the pad
            var layer = shell.Layer;
            VisualElement Trick(string label) => layer.Query<VisualElement>(className: "mc-trick").ToList().FirstOrDefault(e => e.Q<Label>(className: "mc-trick-name")?.text == label);
            var wheelie = Trick("WHEELIE");
            if (wheelie != null)
            {
                shell.Nav.Focus(wheelie); yield return UiShowcase.Frames(25);
                string note = layer.Q<Label>(className: "mc-trick-note")?.text;
                Debug.Log(Tag + "WHEELIE: anim " + (p.Anim != null ? p.Anim.name : "(none)") + ", note '" + note + "'");
                UiShowcaseNav.Check(p.Anim == null, "atv tricks", "WHEELIE is a physics trick and has no performance on the ATV");
                UiShowcaseNav.Check(note != null && note.StartsWith("A PHYSICS TRICK"), "atv tricks", "the physics trick's note on the ATV is '" + note + "'");
                yield return UiShowcase.Shot("atv-trick-physics");
            }
            else Debug.Log(Tag + "no WHEELIE row in the library");
            var nac = Trick("NAC NAC") ?? layer.Query<VisualElement>(className: "mc-trick").ToList().First();
            shell.Nav.Focus(nac); yield return UiShowcase.Frames(25);
            UiShowcaseNav.Check(p.Anim != null && p.Atv, "atv tricks", "the focused trick has no ATV performance (" + ReflexStuntAnim.Problem + ")");
            yield return UiShowcase.Shot("atv-trick-focused");
            yield return Tap(GamepadButton.South, 2);
            UiShowcaseNav.Check(p.Playing, "atv tricks", "A did not play the trick");
            int f0 = p.Frames;
            for (int i = 1; i <= 4; i++) { yield return new WaitForSecondsRealtime(0.7f); Debug.Log(Tag + "playing: t " + p.Time01.ToString("F2") + " frames " + p.Frames); if (i == 1 || i == 3) yield return UiShowcase.Shot("atv-trick-playing-" + i); }
            UiShowcaseNav.Check(p.Frames > f0 + 20, "atv tricks", "the picture was not drawn while playing (" + (p.Frames - f0) + " frames)");
            yield return new WaitForSecondsRealtime(2f);
            UiShowcaseNav.Check(!p.Playing, "atv tricks", "the preview did not end");
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }
    }
}
