// PHYSICS STUDIO's race badge: "PHYSICS MODIFIED · <config>" at the top of the screen while the running event rides a config with changes
// (RxPhysicsStudio.EventActive, set by the simulation when the core took the config), so the player always knows the physics are not the
// original's. OwnRacePause adds it to every open-core world; the pause page shows the same as a tile.
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    [DisallowMultipleComponent]
    public sealed class PhysicsStudioBadge : MonoBehaviour
    {
        GameObject host;
        VisualElement root, badge;
        Label text;
        string shown;

        void Update()
        {
            bool on = RxPhysicsStudio.EventActive;
            if (!on) { if (badge != null) badge.style.display = DisplayStyle.None; return; }
            if (root == null)
            {
                host = new GameObject("Physics Studio badge"); host.transform.SetParent(transform, false);
                root = OuUi.CreateDocument(host, 40); OuTheme.Apply(root);
                root.pickingMode = PickingMode.Ignore;
                badge = OuUi.Div("ou-chip", root);
                badge.pickingMode = PickingMode.Ignore;
                badge.style.position = Position.Absolute; badge.style.top = 14; badge.style.alignSelf = Align.Center;
                badge.style.left = new Length(50, LengthUnit.Percent); badge.style.translate = new Translate(new Length(-50, LengthUnit.Percent), 0);
                badge.style.backgroundColor = new Color(0.08f, 0.08f, 0.09f, 0.72f);
                badge.style.borderLeftWidth = 4; badge.style.borderLeftColor = new Color(0.886f, 0.675f, 0.251f);
                badge.style.paddingLeft = 14; badge.style.paddingRight = 14; badge.style.paddingTop = 4; badge.style.paddingBottom = 4;
                text = OuUi.Text("", "mxa-hint-label", badge);
                text.style.fontSize = 18; text.style.letterSpacing = 2;
            }
            string name = RxPhysicsStudio.EventName ?? "", key = name + (RxPhysicsStudio.EventRestored ? "|r" : "");
            if (shown != key)
            {
                shown = key;
                text.text = "PHYSICS MODIFIED" + (name.Length > 0 ? "  ·  " + name.ToUpperInvariant() : "") +
                            (RxPhysicsStudio.EventRestored ? "  ·  UNSTABLE: YOUR VEHICLE IS BACK ON THE ORIGINAL VALUES" : "");
            }
            bool paused = Time.timeScale == 0f;
            badge.style.display = paused ? DisplayStyle.None : DisplayStyle.Flex;
        }

        void OnDestroy() { if (host) Destroy(host); }
    }
}
