// TEST ONLY (w-restyle): the shell pages the showcase did not shoot yet: the event browser, the setup page, GAME LIBRARY, the install page, the
// signing-in wait page and the session-error page (OpenSessionError), so the restyle can be looked at screen by screen.
using System.Collections;
using System.Reflection;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseRestyle
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("shell-browser", Browser);
            UiShowcase.Register("shell-setup", Setup);
            UiShowcase.Register("shell-library", Library);
            UiShowcase.Register("shell-install", Install);
            UiShowcase.Register("shell-working", Working);
            UiShowcase.Register("session-error", SessionError);
            UiShowcase.Register("hub-crash", HubCrash);
            UiShowcase.Register("options-remap", Remap);
        }

        static OpenShell Shell() => OpenShell.Instance ?? throw new System.InvalidOperationException("no shell");

        static IEnumerator Browser()
        {
            Shell().ShowBrowser(false);
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("shell-browser");
            Shell().ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator Setup()
        {
            Shell().ShowBrowser(false); yield return UiShowcase.Frames(30);
            Shell().ShowSetupPage(); yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot("shell-setup");
            Shell().ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator Library()
        {
            Shell().ShowLibrary(true); yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("shell-library");
            Shell().ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator Install()
        {
            Shell().ShowInstall(false); yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("shell-install");
            Shell().ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator Working()
        {
            var m = typeof(OpenShell).GetMethod("ShowWorking", BindingFlags.Instance | BindingFlags.NonPublic);
            m.Invoke(Shell(), new object[] { "CAREER", "SIGNING IN", "Reading your MOTOCARD from the save…" });
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("shell-working");
            Shell().ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator SessionError()
        {
            ShellHide();
            var go = new GameObject("session error TEST ONLY");
            var e = go.AddComponent<OpenSessionError>();
            typeof(OpenSessionError).GetMethod("Show", BindingFlags.Instance | BindingFlags.NonPublic).Invoke(e, null);
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("session-error");
            Object.Destroy(go);
            yield return UiShowcase.Frames(5);
        }
        static IEnumerator HubCrash()
        {
            var prop = typeof(Reflex.Port.Platform.ReflexCrashLog).GetProperty("PreviousCrashed");
            prop.GetSetMethod(true).Invoke(null, new object[] { true });
            Reflex.Port.Platform.ReflexCrashLog.PromptDismissed = false;
            Shell().ShowMain(); yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("hub-crash");
            Reflex.Port.Platform.ReflexCrashLog.PromptDismissed = true;
            prop.GetSetMethod(true).Invoke(null, new object[] { false });
            Shell().ShowMain(); yield return UiShowcase.Frames(5);
        }

        // the controls page's key-binding prompt (a click on the first binding cell)
        static IEnumerator Remap()
        {
            if (OpenShell.Instance != null) OpenShell.Instance.ShowOptions(0); else Reflex.Port.Options.ReflexOptions.Open(0);
            yield return UiShowcase.Frames(30);
            foreach (var doc in Object.FindObjectsByType<UIDocument>(FindObjectsSortMode.None))
            {
                var cell = doc.rootVisualElement.Q<VisualElement>(className: "mxa-ctl-cell");
                if (cell == null) continue;
                (cell.userData as OuNavItem)?.submit?.Invoke();
                break;
            }
            yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("options-remap");
            Reflex.Port.Options.ReflexOptions.Close();
            yield return UiShowcase.Frames(5);
        }

        static void ShellHide() => UiShowcase.ShellVisible(false);
    }
}
