// The SCREEN ADJUSTMENT page (OPTIONS > VIDEO > SCREEN ADJUSTMENT): the original's "Screen Adjustment Page" (MotocardScreenAdjustment.lua, the third page of
// its create-a-MotoCard wizard): STR_SCREENADJUSTMENT_TITLE, STR_SCREENADJUSTMENT_MESSAGE ("Please adjust your screen to ensure optimal color settings."),
// the GammaContainer list (0 = STR_OPT_GAMMA_CRT, 1 = STR_OPT_GAMMA_LCD, applied the moment it changes: App.SetUISetting "Gamma") and the accept legend
// STR_CONTINUE. The original shows it nowhere else on the PC; here it is also reachable from the options and from the first-run welcome. It shows the
// grey steps and the near-black patches the adjustment is made against (the ramp of ReflexScreenAdjustment draws over them), and also takes the options'
// BRIGHTNESS (the page's layer has a brightness list the Lua never fills; its figure is the same UISetting as the options row).
using System;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        bool _adjustmentCloses;

        /// The page. closes: opened on its own (the welcome page): CONTINUE / Back close the options pages; else they go back to VIDEO.
        public void ShowScreenAdjustment(bool closes)
        {
            _adjustmentCloses = closes;
            CloseControls();
            _layer.Clear();
            _layer.AddToClassList("mxa-dim");
            _layer.AddToClassList("mxa-options-open");
            var head = OptUi.Div("mxa-head", _layer);
            OptUi.Text(Str("STR_SCREENADJUSTMENT_TITLE", "SCREEN ADJUSTMENT"), "mxa-screen-title", head).enableRichText = false;
            var body = OptUi.Div("ou-opt-body p2-adjust-body", _layer);
            var page = OptUi.Div("mxa-panel mxa-options-page p2-adjust", body); page.name = "screen-adjustment";
            OptUi.Text(Str("STR_SCREENADJUSTMENT_TITLE", "SCREEN ADJUSTMENT"), "ou-bar-title ou-opt-page-bar ou-bar", page).enableRichText = false;
            var inner = OptUi.Div("p2-adjust-inner", page);
            OptUi.Text(Str("STR_SCREENADJUSTMENT_MESSAGE", "Please adjust your screen to ensure optimal color settings."), "p2-adjust-message", inner).enableRichText = false;
            // the adjustment's reference: 16 grey steps, and patches that should just be told from black
            var steps = OptUi.Div("p2-adjust-steps", inner); steps.name = "screen-adjustment-steps";
            for (int i = 0; i < 16; i++)
            {
                float v = i / 15f;
                var tile = OptUi.Div("p2-adjust-step", steps); tile.style.backgroundColor = new Color(v, v, v, 1f);
                OptUi.Text((i * 17).ToString(), "p2-adjust-step-label", tile).style.color = v > 0.5f ? Color.black : Color.white;
            }
            var dark = OptUi.Div("p2-adjust-dark", inner);
            foreach (int level in new[] { 4, 8, 12, 16, 24 })
            {
                var patch = OptUi.Div("p2-adjust-patch", dark); float v = level / 255f; patch.style.backgroundColor = new Color(v, v, v, 1f);
                OptUi.Text(level.ToString(), "p2-adjust-step-label", patch).style.color = Color.white;
            }
            var rows = OptUi.Div("p2-adjust-rows", inner);
            // GammaContainer: { 0, CRT }, { 1, LCD }
            var gamma = OptUi.Spinner(Str("STR_OPT_GAMMA", "GAMMA"), () => ReflexSave.GetInt("ReflexUI.Gamma", 0) != 0 ? Str("STR_OPT_GAMMA_LCD", "LCD") : Str("STR_OPT_GAMMA_CRT", "CRT"),
                d => { int v = ReflexSave.GetInt("ReflexUI.Gamma", 0) != 0 ? 1 : 0; v = (v + d + 2) % 2; ReflexSave.SetInt("ReflexUI.Gamma", v); ReflexSave.Save(); ReflexScreenAdjustment.Apply(); },
                rows, FinishScreenAdjustment);
            gamma.name = "gamma";
            var bright = OptUi.Slider(Str("STR_OPT_BRIGHTNESS", "BRIGHTNESS"), () => (ReflexSave.GetInt("ReflexUI.Brightness", 50) - 45) / 10f,
                v => { ReflexSave.SetInt("ReflexUI.Brightness", 45 + Mathf.RoundToInt(v * 10f)); ReflexSave.Save(); ReflexScreenAdjustment.Apply(); }, rows, 0f, 1f, 0.1f, null, FinishScreenAdjustment);
            bright.name = "brightness";
            var go = OptUi.Button(Str("STR_CONTINUE", "CONTINUE"), "mxa-start p2-adjust-go", FinishScreenAdjustment, inner); go.name = "screen-adjustment-continue";
            ((OptNavItem)gamma.userData).down = bright; ((OptNavItem)bright.userData).up = gamma; ((OptNavItem)bright.userData).down = go; ((OptNavItem)go.userData).up = bright;
            _nav.OnTab = null;
            _nav.OnCancel = () => { FinishScreenAdjustment(); return true; };
            _nav.DefaultItem = () => gamma;
            OptUi.Hints(_hints, (OptUi.KeySubmit, Str("STR_CONTINUE", "CONTINUE")), (OptUi.KeyCancel, Str("STR_BACK", "BACK")));
            _nav.Focus(gamma);
        }

        void FinishScreenAdjustment()
        {
            ReflexSettings.Save();
            if (_adjustmentCloses) _back(); else ShowOptions(ReflexOptions.Video);
        }
    }
}
