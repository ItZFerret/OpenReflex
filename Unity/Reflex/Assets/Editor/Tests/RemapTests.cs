using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.Editor
{
    // WP-C controller remapping (FrontEnd/FrontEndRemap.cs, OriginalWorld/OriginalInput.cs, native remap.cpp; _re/notes/input-settings.md
    // "Controller remapping"). Report and PNGs to $REMAP_SHOTS (default temporaryCachePath/remap-shots).
    //   Verify   edit mode: a fresh profile (this bench's own save deleted) has the game's default table (005b9340) with the page's names,
    //            the KeyMap images resolve to UIEmpty, a cell written through App.SetRemapTableByIndex and saved reads back from a new
    //            profile session (a new boot reading savegame.bxml), App.InitializeRemapTables( true ) restores the defaults.
    //   Page     play mode, the front end from OPTIONS: CONTROLLER OPTIONS -> the Controller Remap Page (the game's table and names),
    //            Throttle bound to T, a conflict (D) refused, Q refused, Esc cancels, the PAD column with a virtual pad (Throttle -> rs),
    //            Back travels back and the page's End saves; the save read back by a new profile session.
    //   Race     play mode, National/NAT_Concord without AI on the save Page wrote: the attach loads the table, OriginalInput's "apply"
    //            builds the KeyMaps from it; holding W does not move the bike, holding T drives it (the game's own throttle).
    //   ReadBack edit mode, a new editor process: the save still has Throttle = T / rs (run after Page).
    //   Reset    edit mode: the bench's save back to the default table (other tests drive with W).
    // Run: Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.RemapTests.Verify -quit; .Page / .Race (no -quit); .ReadBack -quit
    public static class RemapTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad, stage; static double stageAt, started; static string mode;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[REMAP] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Shots { get { var s = Environment.GetEnvironmentVariable("REMAP_SHOTS"); if (string.IsNullOrEmpty(s)) s = Path.Combine(Application.temporaryCachePath, "remap-shots"); Directory.CreateDirectory(s); return s; } }
        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static Dictionary<string, (bool has, string s, int i, float f)> saved;
        static void SavePrefs() => saved = UserKeys.ToDictionary(k => k, k => (PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN)));
        static void RestorePrefs()
        {
            if (saved == null) return;
            foreach (var kv in saved)
            {
                if (!kv.Value.has) { PlayerPrefs.DeleteKey(kv.Key); continue; }
                if (kv.Value.s != null && kv.Value.s != "") PlayerPrefs.SetString(kv.Key, kv.Value.s); else if (kv.Value.i != int.MinValue) PlayerPrefs.SetInt(kv.Key, kv.Value.i); else PlayerPrefs.SetFloat(kv.Key, kv.Value.f);
            }
            PlayerPrefs.Save();
        }

        static readonly int[] DefaultKb = { 57, 53, 35, 38, 101, 98, 134, 135, 132, 133, 37, 40, 52, 56, 58, 44, 46, 43, 45 };
        static readonly int[] DefaultJs = { 20, 19, 1, 1, 13, 14, 3, 3, 4, 4, 16, 12, 11, 10, 9, 44, 46, 43, 45 };
        const int IdT = 54, DikT = 0x14, PadRs = 18;

        static void End(string name)
        {
            File.WriteAllText(Path.Combine(Shots, "remap-" + name + ".txt"), report.ToString() + (bad == 0 ? "verify: PASS\n" : $"verify: FAIL ({bad})\n"));
            Debug.Log("[REMAP] " + name + " verify: " + (bad == 0 ? "PASS" : "FAIL " + bad));
        }

        // ------------------------------------------------------------------------------------------- edit mode
        public static void Verify()
        {
            report.Clear(); bad = 0;
            try
            {
                ReflexProfile.Disabled = false; ReflexProfile.Close();
                if (Directory.Exists(ReflexProfile.RemoteDir)) Directory.Delete(ReflexProfile.RemoteDir, true);
                Log("fresh profile: " + ReflexProfile.RemoteDir + " deleted (this bench's own)");
                var t = ReflexRemap.Read();
                Check(t != null, "the game's remap table from the profile session: " + ReflexProfile.Error);
                if (t == null) { End("verify"); return; }
                Log("columns " + string.Join(" ", t.columns));
                Log("kb " + string.Join(",", t.kb) + " = " + string.Join(" | ", t.kbText));
                Log("js " + string.Join(",", t.js) + " = " + string.Join(" | ", t.jsText));
                Check(t.kb.SequenceEqual(DefaultKb) && t.js.SequenceEqual(DefaultJs), "the defaults of InitializeRemapTables (005b9340)");
                Check(string.Join(",", t.kbText.Take(15)) == "W,S,A,D,Space,Shift,Left,Right,Up,Down,C,F,R,V,X" && string.Join(",", t.jsText.Take(15)) == "rt,lt,ls_x,ls_x,lb,rb,rs_x,rs_x,rs_y,rs_y,back,y,x,b,a",
                      "the page's texts (GetRemapTableCharByIndex: DirectInput's key names / the pad's names)");
                Check(t.scan[0x11] == 57 && t.scan[0x14] == IdT && t.scan[0xc8] == 132 && t.scan[0x1c] == 102 && t.scan[0x01] == 103, "the game's scan-code table: W 57, T 54, Up 132, Enter 102, Esc 103");
                foreach (var k in new[] { "Accept", "Back", "Start", "Misc", "Toggle", "Select" })
                {
                    string g = ReflexProfile.Command("remap glyph MapKey_UI_" + k);
                    Log("KeyMap MapKey_UI_" + k + ": " + g);
                    Check(g != null && g.Contains("\"vtable\":9915408") && g.Contains("\"image\":\"UIEmpty\""), "MapKey_UI_" + k + " is a CompoundKeyMap and its image UIEmpty (UIImageComponent::vf1)");
                }
                Check(!ReflexKeyMapGlyphs.PortHints, "the port draws the KeyMap images empty (ReflexKeyHints off)");
                Check(ReflexProfile.Command("remap mapped 57") == "1" && ReflexProfile.Command("remap mapped 54") == "0", "Remap_IsMapped: W mapped, T not");
                // write, save, a new session reads it back
                Check(ReflexRemap.Set(0, 0, IdT), "App.SetRemapTableByIndex( 0, 0, T )");
                var sv = ReflexRemap.Save(); sv.Wait(120000);
                Check(ReflexProfile.HasSave, "savegame.bxml written: " + (ReflexProfile.HasSave ? new FileInfo(ReflexProfile.SaveFile).Length + " bytes" : "none"));
                ReflexProfile.Close();
                var t2 = ReflexRemap.Read();
                Check(t2 != null && t2.kb[0] == IdT && t2.kbText[0] == "T", "a new profile session (savegame.bxml) reads Throttle = T: " + (t2 == null ? "null" : t2.kb[0] + " " + t2.kbText[0]));
                Check(ReflexRemap.Defaults(), "App.InitializeRemapTables( true )");
                var t3 = ReflexRemap.Read();
                Check(t3 != null && t3.kb.SequenceEqual(DefaultKb) && t3.js.SequenceEqual(DefaultJs), "the defaults are back");
                ReflexRemap.Save().Wait(120000);
            }
            catch (Exception e) { Check(false, "exception " + e); }
            End("verify");
        }

        /// Edit mode: the bench's save back to the default table (other tests drive with W).
        public static void Reset()
        {
            report.Clear(); bad = 0;
            try { ReflexProfile.Disabled = false; Check(ReflexRemap.Defaults(), "App.InitializeRemapTables( true )"); var sv = ReflexRemap.Save(); sv.Wait(120000); var t = ReflexRemap.Read(); Check(t != null && t.kb.SequenceEqual(DefaultKb) && t.js.SequenceEqual(DefaultJs), "the bench's save has the default table again"); }
            catch (Exception e) { Check(false, "exception " + e); }
            End("reset");
        }

        public static void ReadBack()
        {
            report.Clear(); bad = 0;
            try
            {
                ReflexProfile.Disabled = false;
                Check(ReflexProfile.HasSave, "the bench's save exists");
                var t = ReflexRemap.Read();
                Check(t != null && t.kb[0] == IdT && t.js[0] == PadRs, "after an editor restart the game reads Throttle = T / rs: " + (t == null ? "null" : t.kb[0] + "/" + t.js[0] + " " + t.kbText[0] + "/" + t.jsText[0]));
            }
            catch (Exception e) { Check(false, "exception " + e); }
            End("readback");
        }

        // ------------------------------------------------------------------------------------------- play mode
        static Gamepad vpad;
        static (InputSettings.EditorInputBehaviorInPlayMode, InputSettings.BackgroundBehavior) inputBehavior;
        public static void Page() => Begin("page");
        public static void Race() { padRun = Environment.GetEnvironmentVariable("REMAP_PAD") == "1"; speedW = speedT = 0; raceAt = runningAt = -1; Begin("race"); }
        static bool padRun;

        static void Begin(string m)
        {
            mode = m; report.Clear(); bad = 0; stage = 0; ReflexProfile.Disabled = false; SavePrefs();
            inputBehavior = (InputSystem.settings.editorInputBehaviorInPlayMode, InputSystem.settings.backgroundBehavior);
            InputSystem.settings.editorInputBehaviorInPlayMode = InputSettings.EditorInputBehaviorInPlayMode.AllDeviceInputAlwaysGoesToGameView;
            InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;
            if (m == "page")
            {
                Reflex.Port.FrontEndEditor.ReflexFrontEndSceneBuilder.Build();
                var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>(); if (fm) { fm.rememberChoice = false; fm.playIntro = false; }
            }
            else
            {
                ReflexEventSelect.Pending = "National/NAT_Concord"; ReflexEventSelect.PendingAi = 0; ReflexEventSelect.PendingGarage = null;
                EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            }
            stageAt = started = EditorApplication.timeSinceStartup;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static void Fail(string why) { Check(false, why); stage = 1000; if (EditorApplication.isPlaying) EditorApplication.ExitPlaymode(); }

        static void Shot(string name)
        {
            var fmm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
            if (fmm && fmm.assets != null) fmm.Layer("ControllerRemapping.lyr").Tick(0f);   // the focus as it is now (the layer refreshes on its next tick)
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false; c.clearFlags = CameraClearFlags.SolidColor; c.backgroundColor = Color.black;
            var target = new RenderTexture(1280, 720, 24); c.targetTexture = target;
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases(); c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(1280, 720, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1280, 720), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(Shots, mode + "_" + name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + mode + "_" + name);
        }

        static void Step()
        {
            try
            {
                if (EditorApplication.timeSinceStartup - started > 1500) { Fail("timeout"); return; }
                if (!EditorApplication.isPlaying) { if (stage >= 1000) Finish(); else if (Age > 180) { Check(false, "play mode never started"); Finish(); } return; }
                if (mode == "page") StepPage(); else StepRace();
            }
            catch (Exception e) { Fail("exception: " + e); }
        }

        static ControllerRemappingScript Remap(FrontEndManager fm) => fm.Layer("ControllerRemapping.lyr").script as ControllerRemappingScript;
        static string Row(ControllerRemappingScript s, int r) => s.ActionText(r) + " [" + s.CellText(0, r) + "] [" + s.CellText(1, r) + "]";

        static void StepPage()
        {
            var fm = UnityEngine.Object.FindAnyObjectByType<FrontEndManager>();
            bool idle = fm && fm.assets != null && !fm.Busy && fm.TopPopup == null;
            switch (stage)
            {
                case 0:
                    if (idle && fm.CurrentPage == "Title Page" && Age > 1) { fm.JumpTo("Options Page"); stageAt = EditorApplication.timeSinceStartup; }   // the main menu's OPTIONS
                    if (idle && fm.CurrentPage == "Options Page" && Age > 1.5)
                    {
                        var nav = fm.Layer("OptionsFrontEnd.lyr").Find("NavList");
                        int target = nav == null ? -1 : nav.kids.FindIndex(k => k.name == "ControllerSettingsContainer");
                        Check(target >= 0, "OPTIONS has the CONTROLLER OPTIONS row (ControllerSettingsContainer)");
                        for (int i = 0; i < 40 && nav != null && nav.focusIndex != target; i++) fm.Navigate(0, nav.focusIndex < target ? 1 : -1);
                        Shot("00_options");
                        fm.DispatchKey("Accept"); Next();
                    }
                    else if (Age > 120) Fail("the Options Page was not reached: " + fm?.CurrentPage);
                    break;
                case 1:
                    if (idle && fm.CurrentPage == "Controller Remap Page" && Age > 1.5 && Remap(fm).IsLoaded)
                    {
                        var s = Remap(fm);
                        for (int i = 0; i < ControllerRemappingScript.Rows; i++) Log("row " + i + ": " + Row(s, i));
                        Check(s.CellText(0, 0) == "W" && s.CellText(1, 0) == "rt" && s.ActionText(0).Length > 0, "the page shows the game's table: Throttle [W] [rt]");
                        Check(fm.HeaderKey == "STR_OPT_CONTROLLERSETTINGS" && fm.legend.IsShown("back") && fm.legend.IsShown("misc"), "header CONTROLLER SETTINGS, legend BACK / DEFAULT");
                        Shot("01_remap_page");
                        Next();
                    }
                    else if (Age > 60) Fail("the Controller Remap Page was not reached / loaded: " + fm?.CurrentPage + " " + ReflexProfile.Error);
                    break;
                case 2:
                    if (Age > 0.7)   // EnableKBJS after 500 ms
                    {
                        var s = Remap(fm);
                        s.KeyDown(0x1c);   // Enter: OnKeyBoardSelection 102 -> binding the KEY cell of Throttle
                        Check(s.Binding && s.CellText(0, 0) == "???", "Enter starts the binding: [" + s.CellText(0, 0) + "]");
                        Shot("02_binding");
                        s.KeyDown(0x10);   // Q: refused (the Lua's list), still binding
                        s.KeyDown(0x20);   // D: already mapped (SteerRight), refused
                        Check(s.Binding && s.CellText(0, 0) == "???", "Q and an already mapped key (D) are refused");
                        s.KeyDown(DikT);   // T: free -> bound
                        Check(!s.Binding && s.CellText(0, 0) == "T" && s.Table.kb[0] == IdT && s.NeedsSave, "T binds Throttle: [" + s.CellText(0, 0) + "]");
                        Shot("03_bound_T");
                        // Brake: start, Esc cancels (and does not leave the page)
                        fm.Navigate(0, 1);
                        Check(s.CurrentRow == 1, "Down moves to Brake");
                        s.KeyDown(0x1c); s.KeyDown(0x01);
                        Check(!s.Binding && s.CellText(0, 1) == "S", "Esc cancels a binding: Brake [" + s.CellText(0, 1) + "]");
                        fm.Navigate(0, -1);
                        Next();
                    }
                    break;
                case 3:
                    if (Age > 0.3)
                    {
                        var s = Remap(fm);
                        Check(fm.CurrentPage == "Controller Remap Page", "still on the page after the Esc that cancelled");
                        vpad = InputSystem.AddDevice<Gamepad>("RemapTestPad");
                        fm.Navigate(1, 0);
                        Check(!s.KbActive, "Right switches to the PAD column");
                        s.PadDown(9);   // A: OnJoystickSelection -> binding the PAD cell
                        Check(s.Binding && s.CellText(1, 0) == "???", "A starts a pad binding (a pad attached: " + ControllerRemappingScript.AttachedJoysticks + ")");
                        s.PadDown(10);  // B: mapped (UseResetPoint) -> refused
                        s.PadDown(PadRs);   // R3: free -> bound
                        Check(!s.Binding && s.CellText(1, 0) == "rs" && s.Table.js[0] == PadRs, "R3 binds Throttle's pad input: [" + s.CellText(1, 0) + "]");
                        Shot("04_bound_pad");
                        fm.Navigate(-1, 0);
                        Check(s.KbActive, "Left switches back to KEY");
                        Next();
                    }
                    break;
                case 4:
                    if (Age > 1.0 && Remap(fm).PendingWrite.IsCompleted)
                    {
                        Remap(fm).TravelBack();   // Esc / B: the page's travel back; End saves
                        Next();
                    }
                    break;
                case 5:
                    if (idle && fm.CurrentPage == "Options Page" && Age > 1 && !ReflexProfile.Busy && !ReflexProfile.SaveState.saving && Age > 3)
                    {
                        Check(true, "Back returns to OPTIONS");
                        ReflexProfile.Close();
                        var t = ReflexRemap.Read();
                        Check(t != null && t.kb[0] == IdT && t.js[0] == PadRs, "the save read back by a new profile session: Throttle " + (t == null ? "null" : t.kbText[0] + " / " + t.jsText[0]));
                        stage = 1000; EditorApplication.ExitPlaymode();
                    }
                    else if (Age > 60) Fail("not back on OPTIONS / the save did not finish: " + fm?.CurrentPage);
                    break;
            }
        }

        static ReflexBikeState Bike(OriginalWorldRuntime rt) => (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
        static ReflexRaceState RaceState(OriginalWorldRuntime rt) => (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
        static double raceAt = -1, runningAt = -1; static float speedW, speedT;

        static void StepRace()
        {
            var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
            double now = EditorApplication.timeSinceStartup;
            if (rt && rt.Running && runningAt < 0) runningAt = now;
            var race = rt && rt.Running ? RaceState(rt) : default;
            if (rt && raceAt < 0 && race.valid != 0 && race.state >= 2) raceAt = now;
            double inRace = raceAt < 0 ? -1 : now - raceAt;
            // keyboard (default): W for 4 s, then T. REMAP_PAD=1: the pad's RT for 4 s (the default Throttle input, now unbound), then R3 (rs).
            if (vpad == null && padRun) { vpad = InputSystem.AddDevice<Gamepad>("RemapTestPad"); vpad.MakeCurrent(); }
            if (padRun)
            {
                if (rt) rt.testHeldKeys = Array.Empty<int>();
                var st = new UnityEngine.InputSystem.LowLevel.GamepadState();
                if (inRace >= 0 && inRace < 4) st.rightTrigger = 1f; else if (inRace >= 4) st = st.WithButton(UnityEngine.InputSystem.LowLevel.GamepadButton.RightStick);
                InputSystem.QueueStateEvent(vpad, st);
            }
            else if (rt) rt.testHeldKeys = inRace < 0 ? Array.Empty<int>() : inRace < 4 ? new[] { 0x11 } : new[] { DikT };
            float speed = rt && rt.Running ? Bike(rt).speed : 0;
            if (inRace > 1 && inRace < 4) speedW = Mathf.Max(speedW, Mathf.Abs(speed));
            if (inRace > 5) speedT = Mathf.Max(speedT, Mathf.Abs(speed));
            switch (stage)
            {
                case 0:
                    if (rt && rt.Running && now - runningAt > 2)
                    {
                        Log("attach: " + ReflexPersistence.LastAttach);
                        Log("remap apply: " + OriginalInput.LastRemap);
                        Check(OriginalInput.LastRemap != null && OriginalInput.LastRemap.Contains("\"controller\":0") && OriginalInput.LastRemap.Contains("\"kb\":[" + IdT + ","),
                              "OriginalInput built the local player's KeyMaps from the saved table (Throttle = T)");
                        Check(OriginalInput.GameBinds(Key.T) && !OriginalInput.GameBinds(Key.W), "GameBinds: T bound, W free");
                        Next();
                    }
                    else if (Age > 900) Fail("the world never ran");
                    break;
                case 1:
                    if (inRace > 3 && inRace < 3.5 && Age > 0) { }
                    if (inRace > 9)
                    {
                        string a = padRun ? "the pad's RT" : "W", b = padRun ? "the pad's R3 (rs)" : "T";
                        Log($"max speed holding {a} {speedW:0.00}, holding {b} {speedT:0.00}");
                        Check(speedW < 2f, a + " (no longer bound) does not drive the bike");
                        Check(speedT > 10f, b + " (bound to Throttle on the Controller Remap Page) drives it through the game's KeyMaps");
                        Shot(padRun ? "race_pad_rs" : "race_key_T");
                        stage = 1000; EditorApplication.ExitPlaymode();
                    }
                    break;
            }
        }

        static void Finish()
        {
            EditorApplication.update -= Step;
            RestorePrefs();
            InputSystem.settings.editorInputBehaviorInPlayMode = inputBehavior.Item1; InputSystem.settings.backgroundBehavior = inputBehavior.Item2;
            if (vpad != null) { InputSystem.RemoveDevice(vpad); vpad = null; }
            ReflexProfile.Close();
            End(mode + (mode == "race" && padRun ? "-pad" : ""));
            EditorApplication.Exit(bad == 0 ? 0 : 1);
        }
    }
}
