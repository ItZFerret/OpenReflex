using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Editor.Options
{
    // The options pages' PanelSettings: since the shell's theme became the one source (2026-10-18), OptUi instantiates the shell's
    // Resources/OpenFrontEnd/OuPanelSettings (its own instance, sorting order 200); this makes that asset (the shell's
    // OpenFrontEndAssets.EnsurePanelSettings). Run once:
    //   Unity -batchmode -projectPath <project> -executeMethod Reflex.Port.Editor.Options.OptionsAssets.Make -quit
    public static class OptionsAssets
    {
        [MenuItem("Reflex/Options/Make the options PanelSettings")]
        public static void Make() => Reflex.Port.OpenFrontEndEditor.OpenFrontEndAssets.EnsurePanelSettings();
    }
}
