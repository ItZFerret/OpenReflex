// The OPEN REFLEX shell's PanelSettings asset (Assets/OpenFrontEnd/Resources/OpenFrontEnd/OuPanelSettings.asset): a PanelSettings in
// the player makes Unity include the ICU data UI Toolkit's text needs; the shell instantiates it (OuUi.Panel). Made here once (menu
// Reflex/OPEN REFLEX/Make panel settings, or batch: -executeMethod Reflex.Port.OpenFrontEndEditor.OpenFrontEndAssets.EnsurePanelSettings).
using UnityEditor;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEndEditor
{
    public static class OpenFrontEndAssets
    {
        public const string Folder = "Assets/OpenFrontEnd/Resources/OpenFrontEnd", PanelPath = Folder + "/OuPanelSettings.asset", ThemePath = Folder + "/OuTheme.tss";

        [MenuItem("Reflex/OPEN REFLEX/Make panel settings")]
        public static void EnsurePanelSettings()
        {
            AssetDatabase.Refresh();
            var theme = AssetDatabase.LoadAssetAtPath<ThemeStyleSheet>(ThemePath);
            if (!theme) { Debug.LogError("[OpenReflex] " + ThemePath + " missing"); return; }
            var ps = AssetDatabase.LoadAssetAtPath<PanelSettings>(PanelPath);
            bool make = !ps;
            if (make) ps = ScriptableObject.CreateInstance<PanelSettings>();
            ps.themeStyleSheet = theme;
            ps.scaleMode = PanelScaleMode.ScaleWithScreenSize;
            ps.referenceResolution = new Vector2Int(1920, 1080);
            ps.screenMatchMode = PanelScreenMatchMode.MatchWidthOrHeight;
            ps.match = 1f;
            ps.sortingOrder = 50;
            if (make) AssetDatabase.CreateAsset(ps, PanelPath); else EditorUtility.SetDirty(ps);
            AssetDatabase.SaveAssets();
            Debug.Log("[OpenReflex] panel settings: " + PanelPath);
        }
    }
}
