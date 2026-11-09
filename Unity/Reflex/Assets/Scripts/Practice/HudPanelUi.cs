// HudPanelUi: the HUD's UI Toolkit panels over the world (PRACTICE's lap list): a panel that grows with its rows, in the MODERN HUD's
// colours (ReflexHudKit.PanelBg: dark glass, a gold bar on the left), and a row list drawn from data (a rank, a name, a tag, a
// number), rebuilt only when the data changes.
//   Panel       a column that grows with its content, glass background, an amber bar on the left
//   Text        a label in the HUD's condensed type (the shell's mxa-hint-label class: font and drop shadow) at a size and colour, no margins
//   HudRows     the row list: Set(rows) redraws when the rows differ from the last ones
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Practice
{
    /// One row of a HudRows list. null / empty texts leave their column out.
    public sealed class HudRow
    {
        public string rank, name, tag, mid; public bool dim; public Color tagColor = HudPanelUi.Amber;
        public string Key => rank + "|" + name + "|" + tag + "|" + mid + "|" + dim + "|" + tagColor;
    }

    public static class HudPanelUi
    {
        public static readonly Color PanelBg = new Color(0.075f, 0.08f, 0.09f, 0.80f), Amber = new Color(0.886f, 0.675f, 0.251f), Ink = new Color(0.08f, 0.085f, 0.095f),
            White = new Color(0.97f, 0.97f, 0.96f), Dim = new Color(0.97f, 0.97f, 0.96f, 0.62f),
            Green = new Color(0.50f, 0.87f, 0.36f);

        /// A panel that grows with its content: the glass background and the amber bar on the left (minWidth: px).
        public static VisualElement Panel(VisualElement parent, float minWidth = 0f, float maxWidth = 0f)
        {
            var p = new VisualElement { pickingMode = PickingMode.Ignore };
            var s = p.style;
            s.flexDirection = FlexDirection.Column; s.alignItems = Align.Stretch; s.flexShrink = 0;
            s.backgroundColor = PanelBg; s.borderLeftWidth = 4; s.borderLeftColor = Amber;
            s.borderTopRightRadius = s.borderBottomRightRadius = 4;
            s.paddingLeft = 12; s.paddingRight = 14; s.paddingTop = 7; s.paddingBottom = 9;
            if (minWidth > 0) s.minWidth = minWidth;
            if (maxWidth > 0) s.maxWidth = maxWidth;
            parent?.Add(p); return p;
        }

        /// A label in the HUD's type: the shell's hint-label class (the condensed font and its drop shadow), without its margin and with the size, colour and spacing given.
        public static Label Text(VisualElement parent, string text, int size, Color color, float spacing = 1f, bool wrap = false)
        {
            var t = new Label(text) { enableRichText = false, pickingMode = PickingMode.Ignore };
            t.AddToClassList("mxa-hint-label");
            var s = t.style;
            s.fontSize = size; s.color = color; s.letterSpacing = spacing; s.marginLeft = 0; s.marginRight = 0; s.marginTop = 0; s.marginBottom = 0; s.paddingLeft = 0; s.paddingRight = 0; s.paddingTop = 0; s.paddingBottom = 0;
            s.whiteSpace = wrap ? WhiteSpace.Normal : WhiteSpace.NoWrap; s.unityTextAlign = TextAnchor.MiddleLeft;
            parent?.Add(t); return t;
        }

        /// A small solid tag ("SESSION BEST", "CUT"): dark type on a colour.
        public static Label Tag(VisualElement parent, string text, int size, Color color)
        {
            var t = Text(parent, text, size, Ink, 1.5f); var s = t.style;
            s.backgroundColor = color; s.paddingLeft = 6; s.paddingRight = 6; s.paddingTop = 1; s.paddingBottom = 1; s.marginLeft = 8; s.flexShrink = 0;
            s.borderTopLeftRadius = s.borderTopRightRadius = s.borderBottomLeftRadius = s.borderBottomRightRadius = 3;
            return t;
        }

    }

    /// A list of rows in a container: redrawn when the rows differ from the last ones (the HUD asks four times a second; nothing is rebuilt while the data is the same).
    public sealed class HudRows
    {
        readonly VisualElement host; readonly int size; string key = "\u0001";
        public HudRows(VisualElement parent, int size = 16) { this.size = size; host = new VisualElement { pickingMode = PickingMode.Ignore }; host.style.flexDirection = FlexDirection.Column; host.style.alignItems = Align.Stretch; parent.Add(host); }

        public void Set(IList<HudRow> rows)
        {
            var sb = new StringBuilder(); foreach (var r in rows) sb.Append(r.Key).Append('\n');
            string next = sb.ToString(); if (next == key) return; key = next;
            host.Clear();
            foreach (var r in rows) host.Add(Build(r));
        }

        VisualElement Build(HudRow r)
        {
            var row = new VisualElement { pickingMode = PickingMode.Ignore }; var s = row.style;
            s.flexDirection = FlexDirection.Row; s.alignItems = Align.Center; s.flexShrink = 0; s.marginTop = 1; s.paddingLeft = 6; s.paddingRight = 6; s.minHeight = size + 8;
            s.backgroundColor = Color.clear; s.borderLeftWidth = 3; s.borderLeftColor = Color.clear; s.marginLeft = -6; s.marginRight = -6;
            Color text = r.dim ? HudPanelUi.Dim : HudPanelUi.White;
            if (!string.IsNullOrEmpty(r.rank)) { var k = HudPanelUi.Text(row, r.rank, size, HudPanelUi.Dim, 0.5f); k.style.width = size + 12; k.style.unityTextAlign = TextAnchor.MiddleRight; k.style.marginRight = 8; k.style.flexShrink = 0; }
            var name = HudPanelUi.Text(row, r.name ?? "", size, text, 0.8f); name.style.flexGrow = 1; name.style.flexShrink = 1; name.style.overflow = Overflow.Hidden; name.style.textOverflow = TextOverflow.Ellipsis; name.style.minWidth = 70;
            if (!string.IsNullOrEmpty(r.tag)) HudPanelUi.Tag(row, r.tag, Math.Max(10, size - 5), r.tagColor);
            if (!string.IsNullOrEmpty(r.mid)) { var m = HudPanelUi.Text(row, r.mid, Math.Max(11, size - 3), HudPanelUi.Dim, 1f); m.style.marginLeft = 10; m.style.flexShrink = 0; }
            return row;
        }
    }
}
