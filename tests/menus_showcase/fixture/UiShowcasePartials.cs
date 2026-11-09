// TEST ONLY: the last PARTIAL rows of the feature audit (w-partials), each from the product's own code:
//   partials-career-reel     F14   the career's results page with HIGHLIGHT REEL (OwnCareerPanels), the pad journey: down, A (the reel alone), B (back on it), up, A (CONTINUE)
//   partials-vehicles        F02   the Free Ride pause's VEHICLE LIST: MY RIDES, Y (north) to PRO RIDERS, a pro's confirmation, B, Y back
//   partials-trick-preview   D14   the MOTOCARD's TRICK LIBRARY with the live preview: the frames of a stunt (a sweep of FrameMode), A plays it on the pad
//   partials-original-hud    HUD   the ORIGINAL HUD's Hit Discovery counter: "n/m" on a find, gone after 3 s
//   partials-narration       C08/A11 the Career Welcome and the first-run Motocard welcome ask their narration, stopped when the page goes
//   partials-alive           the GAME LIBRARY's ALIVE LEVELS page with 26 levels: the button row stays inside the panel
// Env: PARTIALS_MODES (default "0,1,2": the FrameModes of the sweep), PARTIALS_STUNTS (default "BarHop_Air,Superman_Air").
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.FrontEnd;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcasePartials
    {
        const string Tag = "[ui-showcase] [partials] ";
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("partials-career-reel", CareerReel);
            UiShowcase.Register("partials-vehicles", Vehicles);
            UiShowcase.Register("partials-trick-preview", TrickPreview);
            UiShowcase.Register("partials-original-hud", OriginalHud);
            UiShowcase.Register("partials-narration", Narration);
            UiShowcase.Register("partials-alive", Alive);
        }

        static void Open()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            OpenMenuWorld.Still = true; FeGarage.RiderPlays = false;
            ReflexProfile.Open();
        }
        static IEnumerator HubReady()
        {
            var shell = OpenShell.Instance;
            shell.ShowMain();
            float t0 = Time.realtimeSinceStartup;
            while (OpenMenuWorld.Instance == null && Time.realtimeSinceStartup - t0 < 10f) yield return null;
            var w = OpenMenuWorld.Instance;
            while (w != null && w.Status == OpenMenuWorld.State.Loading && Time.realtimeSinceStartup - t0 < 120f) yield return null;
            for (float t1 = Time.realtimeSinceStartup; w != null && (w.Hero == null || w.Hero.RiderModel == null) && Time.realtimeSinceStartup - t1 < 30f;) yield return null;
            for (float t1 = Time.realtimeSinceStartup; !shell.Portrait.Ready && Time.realtimeSinceStartup - t1 < 20f;) yield return null;
            w?.SetWalkTime(99f);
            yield return new WaitForSecondsRealtime(1.0f);
        }
        sealed class TickDriver : MonoBehaviour { public Action Action; void Update() => Action?.Invoke(); }
        static IEnumerator Tap(GamepadButton b, int wait = 6) { yield return UiShowcaseNav.PadTap(b); yield return UiShowcase.Frames(wait); }
        static string FocusedIn(VisualElement root) => (root?.panel?.focusController?.focusedElement as VisualElement)?.name;
        static void Save(RenderTexture rt, string name, bool flipped)
        {
            if (!rt) return;
            string dir = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS"); if (string.IsNullOrEmpty(dir)) return;
            var prev = RenderTexture.active; RenderTexture.active = rt;
            var t = new Texture2D(rt.width, rt.height, TextureFormat.RGBA32, false); t.ReadPixels(new Rect(0, 0, t.width, t.height), 0, 0); t.Apply();
            RenderTexture.active = prev;
            var px = t.GetPixels32();
            // over the page's dark panel colour (the picture is cleared to transparent black)
            for (int i = 0; i < px.Length; i++) { var c = px[i]; float a = c.a / 255f; px[i] = new Color32((byte)(c.r * a + 40 * (1 - a)), (byte)(c.g * a + 44 * (1 - a)), (byte)(c.b * a + 44 * (1 - a)), 255); }
            t.SetPixels32(px); t.Apply();
            File.WriteAllBytes(Path.Combine(dir, name + ".png"), t.EncodeToPNG());
            UnityEngine.Object.Destroy(t);
        }

        // ------------------------------------------------------------------------------------------------ F14: the career's HIGHLIGHT REEL

        static IEnumerator CareerReel()
        {
            UiShowcase.ShellVisible(false);
            var host = new GameObject("showcase career panels");
            var panels = new OwnCareerPanels(host.transform, () => true, () => true);
            var status = typeof(UiShowcasePages).GetMethod("Status", All).Invoke(null, null);
            bool continued = false;
            typeof(OwnCareerPanels).GetMethod("ShowResults").Invoke(panels, new[] { status, (Action)(() => continued = true) });
            var root = (VisualElement)typeof(OwnCareerPanels).GetField("root", All).GetValue(panels);
            var driver = host.AddComponent<TickDriver>(); driver.Action = () => panels.Tick();   // (the presenter ticks the panels every frame: the pad's presses reach them)
            IEnumerator Run(int n) { for (int i = 0; i < n; i++) yield return null; }
            yield return Run(60);
            UiShowcaseNav.Check(root.Q<VisualElement>("career-reel") != null, "career reel", "the results page has no HIGHLIGHT REEL entry");
            UiShowcaseNav.Check(FocusedIn(root) == "career-continue", "career reel", "the page did not open on CONTINUE (" + FocusedIn(root) + ")");
            yield return UiShowcase.Shot("partials-career-results-reel-entry");
            // down to HIGHLIGHT REEL, A: the replay alone
            IEnumerator PadTap(GamepadButton b) { yield return UiShowcaseNav.PadTap(b); yield return Run(8); }
            yield return PadTap(GamepadButton.DpadDown);
            UiShowcaseNav.Check(FocusedIn(root) == "career-reel", "career reel", "down from CONTINUE did not reach HIGHLIGHT REEL (" + FocusedIn(root) + ")");
            yield return PadTap(GamepadButton.South);
            yield return Run(30);
            UiShowcaseNav.Check(panels.ReelShown && !continued, "career reel", "A on HIGHLIGHT REEL did not show the reel (" + panels.ReelShown + ")");
            Debug.Log(Tag + "career reel shown " + panels.ReelShown + ", page " + panels.Page + ", render ready " + panels.RenderReady);
            yield return UiShowcase.Shot("partials-career-reel");
            // B: back on the entry
            yield return PadTap(GamepadButton.East);
            yield return Run(20);
            UiShowcaseNav.Check(!panels.ReelShown && FocusedIn(root) == "career-reel", "career reel", "B did not come back to the entry (" + panels.ReelShown + ", " + FocusedIn(root) + ")");
            yield return UiShowcase.Shot("partials-career-reel-back");
            // up to CONTINUE, A: continues (the reel never blocked it)
            yield return PadTap(GamepadButton.DpadUp);
            UiShowcaseNav.Check(FocusedIn(root) == "career-continue", "career reel", "up did not reach CONTINUE (" + FocusedIn(root) + ")");
            yield return PadTap(GamepadButton.South);
            UiShowcaseNav.Check(continued, "career reel", "A on CONTINUE did not continue");
            panels.Dispose(); UnityEngine.Object.Destroy(host);
            yield return UiShowcase.Frames(3);
        }

        // ------------------------------------------------------------------------------------------------ F02: the pro riders in the Free Ride's VEHICLE LIST

        static IEnumerator Vehicles()
        {
            Open();
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase pause");
            var page = go.AddComponent<OwnRacePause>(); page.enabled = false;
            typeof(OwnRacePause).GetField("open", All).SetValue(page, true);
            var model = new ReflexHudModel { mode = HudMode.FreeRide };
            var spec = OwnModes.Pause(model, new ReflexRaceState { valid = 0 }, new OwnModeContext { eventLabel = "FREE RIDE  ·  ALPINE", location = "ALPINE", ownVehicle = true, vehicle = "BLAZE 250R" });
            UiShowcasePages.Call(page, "Build"); UiShowcasePages.Call(page, "BuildMain", spec, "pause-vehicles");
            UiShowcasePages.DriveOf(go, page);
            var own = OpenGarageData.Vehicles().Where(v => v.owned).ToList();
            Debug.Log(Tag + "owned vehicles " + own.Count + ", types " + string.Join(",", own.Select(v => v.type).Distinct()));
            // what the Free Ride lists: every PlayerSelectable pro x his rides of the event's vehicle types
            var ev = ReflexEvents.Find("FreeRide/FR_Alpine");
            var types = ev != null ? OpenSetup.VehicleTypes(ev) : new List<int>();
            var all = OpenGarageData.ProRides();
            Debug.Log(Tag + "FR_Alpine vehicle types " + string.Join(",", types) + "; pro rides " + all.Count + " (" + all.Select(r => r.rider).Distinct().Count() + " riders), of those types " + all.Count(r => types.Contains(r.vehicleType)));
            UiShowcaseNav.Check(all.Count > 0, "vehicle list", "no pro ride in the install's tables");
            yield return UiShowcase.Frames(50);
            IEnumerator Tick(int n) { yield return UiShowcase.Frames(n); }
            yield return Tap(GamepadButton.South, 20);
            UiShowcaseNav.Check(page.Page == "vehicles", "vehicle list", "A on VEHICLE LIST did not open it (" + page.Page + ")");
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("partials-vehicles-mine");
            // Y (north): the PRO RIDERS list
            yield return Tap(GamepadButton.North, 20);
            var root = page.Root;
            int proRows = root.Query<VisualElement>(className: "ou-veh-row").ToList().Count;
            UiShowcaseNav.Check(proRows > 0 && root.Q<VisualElement>(className: "ou-veh-pros") != null, "vehicle list", "Y did not show the pro riders (" + proRows + " rows)");
            Debug.Log(Tag + "PRO RIDERS rows " + proRows + ", focus " + FocusedIn(root));
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("partials-vehicles-pros");
            // a pro's row: A asks first
            var first = root.Query<VisualElement>(className: "ou-veh-row").First();
            UiShowcaseNav.Check(first != null && first.name.StartsWith("pause-pro-"), "vehicle list", "the first pro row is " + first?.name);
            var inRow = first.Q<Label>(className: "ou-trick-name")?.text;
            Debug.Log(Tag + "focus before the pro's A: " + FocusedIn(root) + " (first row '" + inRow + "')");
            UiShowcaseNav.Check((FocusedIn(root) ?? "").StartsWith("pause-pro-"), "vehicle list", "the list did not open on a pro's row (" + FocusedIn(root) + ")");
            yield return Tap(GamepadButton.South, 30);
            UiShowcaseNav.Check(page.Page == "vehicle-confirm", "vehicle list", "A on a pro did not ask (" + page.Page + ", focus " + FocusedIn(root) + ")");
            yield return UiShowcase.Shot("partials-vehicles-pro-confirm");
            yield return Tap(GamepadButton.East, 20);   // B: back on the pro's row
            UiShowcaseNav.Check(page.Page == "vehicles" && (FocusedIn(root) ?? "").StartsWith("pause-pro-"), "vehicle list", "B on the question did not return to the pro's row (" + page.Page + ", " + FocusedIn(root) + ")");
            yield return Tap(GamepadButton.North, 20);   // Y: MY RIDES again
            UiShowcaseNav.Check(root.Q<VisualElement>(className: "ou-veh-pros") == null, "vehicle list", "Y did not return to MY RIDES");
            yield return Tap(GamepadButton.East, 20);    // B: the pause page, on VEHICLE LIST
            UiShowcaseNav.Check(page.Page == "main" && FocusedIn(root) == "pause-vehicles", "vehicle list", "B on the list did not return to the pause page (" + page.Page + ", " + FocusedIn(root) + ")");
            typeof(OwnRacePause).GetField("open", All).SetValue(page, false); UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
        }

        // ------------------------------------------------------------------------------------------------ D14: the live trick preview

        static IEnumerator TrickPreview()
        {
            Open();
            var shell = OpenShell.Instance;
            yield return HubReady();
            shell.ShowMotocard(OpenShell.MotocardTricks);
            float t0 = Time.realtimeSinceStartup;
            while ((shell.StuntPreview == null || !shell.StuntPreview.Ready) && Time.realtimeSinceStartup - t0 < 40f) yield return null;
            var p = shell.StuntPreview;
            UiShowcaseNav.Check(p != null && p.Ready, "trick preview", "the preview was not built (" + p?.Problem + ")");
            if (p == null || !p.Ready) yield break;
            yield return new WaitForSecondsRealtime(0.6f);
            Debug.Log(Tag + "trick preview ready: " + p.Setup + ", built in " + p.LastBuildMs.ToString("F0") + " ms, flipped " + p.Flipped + ", frames drawn " + p.Frames);
            yield return UiShowcase.Shot("partials-trick-page");
            // the frames of a stunt, by the vehicle clip's frame mapping (the picture's own render texture, 640 x 480)
            var modes = (Environment.GetEnvironmentVariable("PARTIALS_MODES") ?? "0,1,2").Split(',').Select(int.Parse).ToArray();
            var stunts = (Environment.GetEnvironmentVariable("PARTIALS_STUNTS") ?? "BarHop_Air,Superman_Air").Split(',');
            var times = new[] { 0f, 0.5f, 0.83f, 1.4f, 2.5f, 3.3f };
            OpenStuntPreview.Manual = true;
            foreach (int mode in modes)
            {
                OpenStuntPreview.FrameMode = mode;
                foreach (var stunt in stunts)
                {
                    if (!p.SetStunt(stunt)) { Debug.Log(Tag + "no performance for " + stunt); continue; }
                    float len = p.Anim.Length();
                    Debug.Log(Tag + stunt + " (" + (p.Atv ? "ATV" : "MX") + ") length " + len.ToString("F2") + " s, " + p.Anim.blocks.Count + " clips: " + string.Join(", ", p.Anim.blocks.Select(b => b.name + (b.vehicle != null ? "+v" : ""))));
                    foreach (float t in times)
                    {
                        if (t > len + 0.05f) continue;
                        p.Seek(t);
                        yield return null;
                        Save(p.Texture, "partials-trick-" + stunt + "-m" + mode + "-t" + (int)(t * 100), p.Flipped);
                    }
                }
            }
            OpenStuntPreview.Manual = false; OpenStuntPreview.FrameMode = modes[0];
            // the page itself: a physics trick (no animation), then a trick played with the pad
            var scroll = shell.Layer.Q<ScrollView>();
            string[] names = shell.Layer.Query<VisualElement>(className: "mc-trick").ToList().Select(e => e.name).ToArray();
            Debug.Log(Tag + "tricks listed " + names.Length);
            var wheelie = shell.Layer.Query<VisualElement>(className: "mc-trick").ToList().FirstOrDefault(e => e.Q<Label>(className: "mc-trick-name")?.text == "WHEELIE");
            if (wheelie != null)
            {
                shell.Nav.Focus(wheelie); yield return UiShowcase.Frames(20);
                UiShowcaseNav.Check(p.Anim == null, "trick preview", "WHEELIE is a physics trick and has no performance");
                yield return UiShowcase.Shot("partials-trick-physics");
            }
            var nac = shell.Layer.Query<VisualElement>(className: "mc-trick").ToList().FirstOrDefault(e => e.Q<Label>(className: "mc-trick-name")?.text == "NAC NAC");
            if (nac == null) nac = shell.Layer.Query<VisualElement>(className: "mc-trick").ToList().First();
            shell.Nav.Focus(nac); yield return UiShowcase.Frames(20);
            yield return UiShowcase.Shot("partials-trick-focused");
            yield return Tap(GamepadButton.South, 2);
            UiShowcaseNav.Check(p.Playing, "trick preview", "A did not play the trick");
            int f0 = p.Frames;
            for (int i = 1; i <= 4; i++) { yield return new WaitForSecondsRealtime(0.7f); Debug.Log(Tag + "playing: t " + p.Time01.ToString("F2") + " frames " + p.Frames); if (i == 1 || i == 3) yield return UiShowcase.Shot("partials-trick-playing-" + i); }
            UiShowcaseNav.Check(p.Frames > f0 + 20, "trick preview", "the picture was not drawn while playing (" + (p.Frames - f0) + " frames)");
            yield return new WaitForSecondsRealtime(2f);
            UiShowcaseNav.Check(!p.Playing, "trick preview", "the preview did not end");
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ the ORIGINAL HUD's Hit Discovery counter

        static IEnumerator OriginalHud()
        {
            UiShowcase.ShellVisible(false);
            var backHost = HudShowcase.Backdrop(5);
            var host = new GameObject("original HUD discovery gallery");
            var hud = new OriginalFreeRideHud(host.transform);
            IEnumerator Run(float seconds, bool allowed, int found, int total, int serial)
            {
                for (float t = 0; t < seconds; t += 1f / 60f)
                {
                    hud.Tick(new OriginalFreeRideHud.Frame { dt = 1f / 60f, allowed = allowed, page = OriginalFreeRideHud.Page.FreeRide, viewForward = Vector3.forward,
                                                              discoveryFound = found, discoveryTotal = total, discoverySerial = serial });
                    yield return null;
                }
            }
            yield return Run(0.3f, true, 0, 6, 0);
            UiShowcaseNav.Check(hud.Built(OriginalFreeRideHud.Page.FreeRide), "original hud", "the Free Ride layer has no Hit Discovery component / compass");
            UiShowcaseNav.Check(!hud.Shown, "original hud", "the counter shows before a find");
            yield return UiShowcase.Shot("partials-original-hud-none");
            yield return Run(0.5f, true, 2, 6, 1);   // a find: "2/6"
            UiShowcaseNav.Check(hud.Shown && hud.Text == "2/6", "original hud", "a find did not show 2/6 (" + hud.Shown + ", '" + hud.Text + "')");
            Debug.Log(Tag + "discovery text '" + hud.Text + "'");
            yield return UiShowcase.Shot("partials-original-hud-found");
            yield return Run(2.0f, true, 2, 6, 1);   // 2.5 s: still there
            UiShowcaseNav.Check(hud.Shown, "original hud", "the counter left before its 3 s");
            yield return Run(0.9f, true, 2, 6, 1);   // 3.4 s: the Out animation has run
            UiShowcaseNav.Check(!hud.Shown, "original hud", "the counter stayed past its 3 s");
            yield return UiShowcase.Shot("partials-original-hud-gone");
            yield return Run(0.4f, true, 3, 6, 2);   // the next find restarts it
            UiShowcaseNav.Check(hud.Shown && hud.Text == "3/6", "original hud", "the next find did not show 3/6 ('" + hud.Text + "')");
            yield return Run(0.2f, false, 3, 6, 2);   // a menu: gone at once
            UiShowcaseNav.Check(!hud.Shown && !hud.Canvas.enabled, "original hud", "a menu did not hide the counter");
            hud.Dispose(); UnityEngine.Object.Destroy(host); UnityEngine.Object.Destroy(backHost);
            yield return UiShowcase.Frames(3);
        }

        // ------------------------------------------------------------------------------------------------ the narration of the welcome pages

        static IEnumerator Narration()
        {
            Open();
            var shell = OpenShell.Instance;
            string Voices() { lock (FeSounds.Log) return string.Join("\n", FeSounds.Log.Where(l => l.StartsWith("vo ")).ToArray()); }
            int Count(string s) { lock (FeSounds.Log) return FeSounds.Log.Count(l => l.Contains(s)); }
            // CAREER WELCOME
            bool done = false;
            shell.ShowCareerWelcome(() => { done = true; shell.ShowMain(); });
            yield return UiShowcase.Frames(60);
            Debug.Log(Tag + "voice log after the career welcome:\n" + Voices());
            UiShowcaseNav.Check(Count("vo career-welcome: UIAudio/UI_MOTO_INTRO") == 1, "narration", "the career welcome did not ask for UI_MOTO_INTRO");
            UiShowcaseNav.Check(Count("vo career-welcome: UIAudio/UI_MOTO_INTRO UI_MOTO_INTRO_ENG/VO/INTRO -> ") == 1 && Count("(no such package)") == 0, "narration", "the narration did not resolve to an event");
            yield return UiShowcase.Shot("partials-narration-career-welcome");
            shell.ShowMain(); yield return UiShowcase.Frames(10);
            UiShowcaseNav.Check(Count("vo stop career-welcome") == 1, "narration", "the narration was not stopped with the page");
            // MOTOCARD WELCOME
            shell.ShowMotocardWelcome(() => { done = true; shell.ShowMain(); });
            yield return UiShowcase.Frames(60);
            UiShowcaseNav.Check(Count("vo motocard-welcome: UIAudio/UI_Game_Intro UI_GAME_INTRO/VO/INTRO -> ") == 1, "narration", "the Motocard welcome did not ask for UI_GAME_INTRO");
            yield return UiShowcase.Shot("partials-narration-motocard-welcome");
            yield return Tap(GamepadButton.South, 10);   // GRAB YOUR RIDER: goes on, stops
            UiShowcaseNav.Check(Count("vo stop motocard-welcome") == 1, "narration", "the narration was not stopped when the page was accepted");
            Debug.Log(Tag + "voice log at the end:\n" + Voices());
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ the ALIVE LEVELS page

        static IEnumerator Alive()
        {
            var shell = OpenShell.Instance;
            var found = new AliveSources();
            string[] kinds = { "Free Ride (Jump Park)", "Free Ride (Dunes)", "Free Ride", "Race pack (not imported)" };
            for (int i = 0; i < 26; i++)
                found.levels.Add(new AliveLevelSource { level = "FR_Alive_" + i, displayName = "ALIVE LEVEL NUMBER " + (i + 1) + (i % 3 == 0 ? " WITH A LONG NAME" : ""), packDisplay = i < 12 ? "MX vs ATV Alive" : "Downloadable level pack " + (i - 11), kind = kinds[i % 3], recognized = true, bytes = 40_000_000L + i * 3_000_000L });
            found.levels.Add(new AliveLevelSource { level = "FR_Unknown", displayName = "A LEVEL OF ANOTHER RELEASE", packDisplay = "Another pack", recognized = false, problem = "Not the original release: its files differ from the game's own." });
            for (int i = 0; i < 4; i++) found.warnings.Add("The package 0000000" + i + " could not be read: a hash differs (" + i + ")");
            typeof(OpenShell).GetField("_aliveFound", All).SetValue(shell, found);
            var chosen = (HashSet<string>)typeof(OpenShell).GetField("_aliveChosen", All).GetValue(shell);
            chosen.Clear(); for (int i = 0; i < 26; i += 2) chosen.Add("FR_Alive_" + i);
            typeof(OpenShell).GetMethod("ShowAliveLevels", All).Invoke(shell, null);
            yield return UiShowcase.Frames(120);
            Report(shell, "ou-alive-levels");
            yield return UiShowcase.Shot("partials-alive-levels");
            // the sources page, the results page
            typeof(OpenShell).GetMethod("ShowAliveSources", All).Invoke(shell, null);
            yield return UiShowcase.Frames(120);
            Report(shell, "ou-alive-sources");
            yield return UiShowcase.Shot("partials-alive-sources");
            var results = new List<AliveLevelResult>();
            for (int i = 0; i < 26; i++) results.Add(new AliveLevelResult { level = "FR_Alive_" + i, displayName = "Alive level " + (i + 1), ok = i != 5, error = i == 5 ? "The conversion stopped: a texture could not be read." : null, resources = 900 + i, starts = 3, pending = 2, bytes = 55_000_000L, seconds = 30 + i });
            typeof(OpenShell).GetField("_aliveResults", All).SetValue(shell, results);
            typeof(OpenShell).GetMethod("ShowAliveResults", All).Invoke(shell, new object[] { false });
            yield return UiShowcase.Frames(120);
            Report(shell, "ou-alive-results");
            yield return UiShowcase.Shot("partials-alive-results");
            typeof(OpenShell).GetField("_aliveFound", All).SetValue(shell, null);
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        /// The panel and its button row: the row must lie inside the panel and the panel inside the screen's layer.
        static void Report(OpenShell shell, string name)
        {
            var layer = shell.Layer; var panel = layer.Q<VisualElement>(name); var buttons = panel?.Q<VisualElement>(className: "mxa-install-buttons");
            if (panel == null || buttons == null) { UiShowcaseNav.Check(false, name, "the page or its button row is missing"); return; }
            var lb = layer.worldBound; var pb = panel.worldBound; var bb = buttons.worldBound; var root = shell.Root.worldBound;
            Debug.Log(Tag + name + ": screen " + root.width + "x" + root.height + ", layer y " + lb.yMin + ".." + lb.yMax + ", panel y " + pb.yMin + ".." + pb.yMax + ", buttons y " + bb.yMin + ".." + bb.yMax);
            UiShowcaseNav.Check(bb.yMax <= pb.yMax + 0.5f && bb.yMin >= pb.yMin, name, "the button row (" + bb.yMin + ".." + bb.yMax + ") is not inside the panel (" + pb.yMin + ".." + pb.yMax + ")");
            UiShowcaseNav.Check(pb.yMax <= root.yMax + 0.5f, name, "the panel (" + pb.yMax + ") runs past the screen (" + root.yMax + ")");
        }
    }
}
