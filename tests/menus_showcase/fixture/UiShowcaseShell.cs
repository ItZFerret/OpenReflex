// TEST ONLY: the shell's own pages in the showcase (the hub, the credits and the options tabs), opened as a player would.
using System.Collections;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseShell
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("hub", Hub);
            UiShowcase.Register("credits", Credits);
            for (int tab = 0; tab < ReflexOptions.Tabs.Length; tab++) { int t = tab; UiShowcase.Register("options-" + Slug(t), () => Options(t)); }
        }
        static string Slug(int tab) => ReflexOptions.Tabs[tab].ToLowerInvariant().Replace(" & ", "-");

        static IEnumerator Hub()
        {
            var shell = OpenShell.Instance;
            if (shell == null) throw new System.InvalidOperationException("no shell");
            shell.ShowMain();
            yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("hub");
        }

        static IEnumerator Credits()
        {
            OpenShell.Instance.ShowCredits();
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("credits");
            OpenShell.Instance.ShowMain();
            yield return UiShowcase.Frames(5);
        }

        static IEnumerator Options(int tab)
        {
            if (OpenShell.Instance != null) OpenShell.Instance.ShowOptions(tab); else ReflexOptions.Open(tab);
            yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("options-" + Slug(tab));
            ReflexOptions.Close();
            yield return UiShowcase.Frames(5);
        }
    }
}
