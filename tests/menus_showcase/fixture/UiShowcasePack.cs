// TEST ONLY: the startup PACK YOUR GAME FILES page (OpenShell.Pack.cs). Packing is not optional: PACK NOW is its only button.
// The page is opened by reflection with a no-op "next"; PACK NOW is never pressed (the library is only measured, read-only).
//   pack-prompt     PACK YOUR GAME FILES at startup
using System;
using System.Collections;
using System.Reflection;
using Reflex.Port.OpenFrontEnd;

namespace Reflex.Port.Showcase
{
    static class UiShowcasePack
    {
        [UnityEngine.RuntimeInitializeOnLoadMethod(UnityEngine.RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("pack-prompt", Page);

        static IEnumerator Page()
        {
            var shell = OpenShell.Instance;
            typeof(OpenShell).GetMethod("ShowPackPrompt", BindingFlags.Instance | BindingFlags.NonPublic).Invoke(shell, new object[] { (Action)(() => { }) });
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("pack-prompt");
        }
    }
}
