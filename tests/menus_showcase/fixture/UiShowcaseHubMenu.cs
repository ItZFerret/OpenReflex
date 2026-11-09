// TEST ONLY: w-hubmenu pages for the hub and its PLAY page.
//   hubmenu   the hub (first row focused, then GARAGE focused, then a chip focused) and PLAY, each screenshot; the log holds every row's and chip's
//             left edge, width and label overhang (a row moving or growing on focus, a label touching its chip's edge, would show)
using System;
using System.Collections;
using System.Linq;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseHubMenu
    {
        const string Tag = "[ui-showcase] [hubmenu] ";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("hubmenu", Run);

        static void Dump(OpenShell shell, string what)
        {
            var root = shell.Layer;
            var left = root.Q<VisualElement>("hub-left"); var logo = root.Q<VisualElement>("hub-logo");
            Debug.Log(Tag + what + ": logo x " + (logo != null ? logo.worldBound.xMin.ToString("F1") : "-") + ", column x " + (left != null ? left.worldBound.xMin.ToString("F1") : "-"));
            foreach (var e in root.Query<VisualElement>(className: "mxa-button").ToList().Where(b => b.name.StartsWith("ou-hub-")))
            {
                var label = e.Q<Label>(className: "mxa-button-label");
                var wb = e.worldBound; var lb = label != null ? label.worldBound : default;
                float textW = label != null ? label.MeasureTextSize(label.text, 0, VisualElement.MeasureMode.Undefined, 0, VisualElement.MeasureMode.Undefined).x : 0;
                Debug.Log(Tag + what + ": " + e.name + " x " + wb.xMin.ToString("F1") + " w " + wb.width.ToString("F1") + " right " + wb.xMax.ToString("F1") + " y " + wb.yMin.ToString("F1")
                          + " | label x " + lb.xMin.ToString("F1") + " w " + lb.width.ToString("F1") + " text " + textW.ToString("F1") + (e.focusController?.focusedElement == e ? " FOCUSED" : "")
                          + (e.ClassListContains("ou-chip") ? " space-left " + (lb.xMin - wb.xMin).ToString("F1") + " space-right " + (wb.xMax - lb.xMax).ToString("F1") + " overhang " + Mathf.Max(0, textW - lb.width).ToString("F1") : ""));
            }
        }

        static IEnumerator Run()
        {
            var shell = OpenShell.Instance;
            shell.ShowMain(); yield return UiShowcase.Frames(60);
            Dump(shell, "hub"); yield return UiShowcase.Shot("hubmenu-hub");
            shell.Nav.Focus(shell.Layer.Q<VisualElement>("ou-hub-garage")); yield return UiShowcase.Frames(30);
            Dump(shell, "hub, GARAGE focused"); yield return UiShowcase.Shot("hubmenu-hub-garage");
            shell.Nav.Focus(shell.Layer.Q<VisualElement>("ou-hub-physics")); yield return UiShowcase.Frames(30);
            Dump(shell, "hub, PHYSICS STUDIO focused"); yield return UiShowcase.Shot("hubmenu-hub-chip");
            shell.ShowPlay(); yield return UiShowcase.Frames(60);
            Dump(shell, "play"); yield return UiShowcase.Shot("hubmenu-play");
            shell.Nav.Focus(shell.Layer.Q<VisualElement>("ou-hub-race")); yield return UiShowcase.Frames(30);
            Dump(shell, "play, RACE focused"); yield return UiShowcase.Shot("hubmenu-play-race");
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }
    }
}
