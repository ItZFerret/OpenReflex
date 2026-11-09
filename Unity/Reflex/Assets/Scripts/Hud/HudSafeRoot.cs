// Part of the MODERN HUD kit (ReflexHudKit.cs).
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    /// Keeps the HUD's root inside the safe area and within MaxAspect (centred on very wide screens), pulled in a further title-safe
    /// inset (ReflexHudKit.TitleSafe of the height) so that nothing sits on the very edge of a TV or a rounded display.
    public sealed class HudSafeRoot : MonoBehaviour
    {
        RectTransform _root; Vector2Int _laid = new Vector2Int(-1, -1);
        public void Init(RectTransform root) { _root = root; Layout(); }
        void LateUpdate() { if (_root && (_laid.x != OpenFrontEnd.OuLayout.ScreenWidth || _laid.y != OpenFrontEnd.OuLayout.ScreenHeight)) Layout(); }
        void Layout()
        {
            int w = OpenFrontEnd.OuLayout.ScreenWidth, h = OpenFrontEnd.OuLayout.ScreenHeight; if (w <= 0 || h <= 0 || !_root) return;   // (the real screen except in offscreen test renders)
            _laid = new Vector2Int(w, h);
            Rect safe = OpenFrontEnd.OuLayout.SafeArea; if (safe.width <= 0 || safe.height <= 0) safe = new Rect(0, 0, w, h);
            float maxW = safe.height * ReflexHudKit.MaxAspect;
            if (safe.width > maxW) { safe.x += (safe.width - maxW) * 0.5f; safe.width = maxW; }
            float inset = safe.height * ReflexHudKit.TitleSafe;
            safe.xMin += inset; safe.xMax -= inset; safe.yMin += inset; safe.yMax -= inset;
            _root.anchorMin = new Vector2(safe.xMin / w, safe.yMin / h); _root.anchorMax = new Vector2(safe.xMax / w, safe.yMax / h);
            _root.offsetMin = _root.offsetMax = Vector2.zero;
        }
    }
}
