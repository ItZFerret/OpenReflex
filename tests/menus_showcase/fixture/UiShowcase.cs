// TEST ONLY (tests/menus_showcase): the UI showcase driver. REFLEX_UI_SHOWCASE=<page,page|all> and REFLEX_PREVIEW_SHOTS=<dir> in the
// environment make the player open each registered page with synthetic data, take a screenshot of it and quit. Pages register
// themselves from other fixture files (UiShowcase.Register); nothing here rides a race or writes a save.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    public sealed class UiShowcase : MonoBehaviour
    {
        const string Tag = "[ui-showcase] ";
        static readonly List<KeyValuePair<string, Func<IEnumerator>>> Pages = new List<KeyValuePair<string, Func<IEnumerator>>>();
        static string shotDirectory;

        /// The walk of the controller test (UiShowcaseNav): run after every screenshot when REFLEX_UI_WALK=1.
        public static Func<string, IEnumerator> AfterShot;

        public static void Register(string name, Func<IEnumerator> page) => Pages.Add(new KeyValuePair<string, Func<IEnumerator>>(name, page));
        public static bool Wanted => !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("REFLEX_UI_SHOWCASE"));

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Guest()
        {
            if (!Wanted) return;
            Reflex.Port.Content.ReflexProfile.Disabled = true;
            Reflex.Port.OpenFrontEnd.OpenShell.SkipIntro = true;   // (the 3D menu's intro would hold the input of the pages under test)
            Debug.Log(Tag + "GUEST_BEFORE_AWAKE");
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if (!Wanted || FindAnyObjectByType<UiShowcase>() != null) return;
            var go = new GameObject("UI showcase TEST ONLY"); DontDestroyOnLoad(go);
            go.AddComponent<UiShowcase>(); Application.runInBackground = true;
        }

        /// A screenshot of what is on screen now, written to the shots directory as <name>.png.
        public static IEnumerator Shot(string name)
        {
            yield return new WaitForSecondsRealtime(0.25f);
            string path = Path.Combine(shotDirectory, name + ".png");
            if (Application.isBatchMode)
            {   // -batchmode (run.py --batch): no window to capture; the frame is composed offscreen (UiShowcaseOffscreen)
                yield return UiShowcaseOffscreen.Shot(path);
                Debug.Log(Tag + "screenshot " + name + " " + UiShowcaseOffscreen.Size.x + "x" + UiShowcaseOffscreen.Size.y + " offscreen (screen " + Screen.width + "x" + Screen.height + ")");
            }
            else
            {
                yield return new WaitForEndOfFrame();
                ScreenCapture.CaptureScreenshot(path);
                for (int i = 0; i < 60 && !File.Exists(path); i++) yield return null;
                Debug.Log(Tag + "screenshot " + name + " " + Screen.width + "x" + Screen.height);
            }
            if (AfterShot != null) yield return AfterShot(name);
        }

        /// The shell's own screen (the hub) steps aside for the in-race pages, which are drawn over a world, not over the front end.
        public static void ShellVisible(bool on)
        {
            var shell = Reflex.Port.OpenFrontEnd.OpenShell.Instance;
            if (shell != null && shell.Root != null) shell.Root.style.display = on ? UnityEngine.UIElements.DisplayStyle.Flex : UnityEngine.UIElements.DisplayStyle.None;
        }

        public static IEnumerator Frames(int n) { for (int i = 0; i < n; i++) yield return null; }

        IEnumerator Start()
        {
            shotDirectory = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (string.IsNullOrEmpty(shotDirectory)) { Debug.LogError(Tag + "FAIL no REFLEX_PREVIEW_SHOTS"); Application.Quit(1); yield break; }
            Directory.CreateDirectory(shotDirectory);
            var want = Environment.GetEnvironmentVariable("REFLEX_UI_SHOWCASE").Split(',').Select(x => x.Trim()).Where(x => x.Length > 0).ToList();
            // the shell (the front-end scene's OPEN REFLEX) is the host of the first pages
            for (float t = 0; t < 30f && Reflex.Port.OpenFrontEnd.OpenShell.Instance == null; t += Time.unscaledDeltaTime) yield return null;
            yield return Frames(30);
            int failed = 0;
            foreach (var page in Pages)
            {
                if (!want.Contains("all") && !want.Contains(page.Key)) continue;
                Debug.Log(Tag + "page " + page.Key);
                ShellVisible(true);
                IEnumerator run = null;
                try { run = page.Value(); } catch (Exception e) { Debug.LogError(Tag + "FAIL " + page.Key + ": " + e); failed++; continue; }
                while (true)
                {
                    object current = null; bool more = false;
                    try { more = run.MoveNext(); if (more) current = run.Current; }
                    catch (Exception e) { Debug.LogError(Tag + "FAIL " + page.Key + ": " + e); failed++; break; }
                    if (!more) break;
                    yield return current;
                }
            }
            int navFailures = UiShowcaseNav.Failures;
            if (navFailures > 0) { failed += navFailures; Debug.LogError(Tag + "NAV " + navFailures + " controller problems"); }
            Debug.Log(Tag + (failed == 0 ? "PASS" : "FAIL ") + failed + " failed");
            yield return null;
            Application.Quit(failed == 0 ? 0 : 1);
        }
    }
}
