// The KeyMap button glyphs of the open product's own in-world pages (the AAA notification, PreChallengePopup, PostChallengePopup).
//
// A UIImageComponent with a KeyMap (MapKey_UI_Accept on a popup's button, MapKey_UI_Start on the AAA notification's "press Start") is
// drawn by the PC game as UIEmpty: every MapKey_UI_* is a CompoundKeyMap whose own slot 0 is empty, so no key or button glyph is shown
// (OriginalInput.cs ReflexKeyMapGlyphs, WP-C). The open pages follow that: with ReflexKeyHints off the KeyMap images stay empty. With
// ReflexKeyHints 1 (the port extra the front end's KeyGlyph and the button legend already offer) each image shows what those pages show:
// the Xbox 360 button sprite of the BaseLocalized ControllerButtons atlas while a gamepad is connected, else the key KeyBind_Core.xml
// binds to the action, in the page's own font. The Start image of the AAA notification stands for the pause request that starts the
// challenge (MXUIManager_OnPauseRequest), so its key is the one bound to MapKey_UI_Pause (Esc).
using System;
using System.Collections.Generic;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class OwnKeyGlyphs
    {
        sealed class Slot
        {
            public OriginalUiTree.Node node; public string action;     // MapKey_UI_<action>
            public RawImage glyph; public OriginalText key;
        }

        static readonly Dictionary<string, string> Pad = new Dictionary<string, string>
        {   // KeyBind_Core.xml's xbox360 set (the same sprites as KeyGlyph / OriginalButtonLegend)
            ["Accept"] = "Xenon_Controller_Button_1", ["Back"] = "Xenon_Controller_Button_2", ["Toggle"] = "Xenon_Controller_Button_3",
            ["Misc"] = "Xenon_Controller_Button_4", ["Start"] = "Xenon_Controller_Button_7", ["Select"] = "Xenon_Controller_Button_8",
        };

        readonly List<Slot> slots = new List<Slot>();
        // the modern pages' key caps (UI Toolkit labels): the same glyph record, with the shell's button names (OuGlyphs: CONTROLLER GLYPHS)
        readonly List<(string action, UnityEngine.UIElements.Label cap)> caps = new List<(string, UnityEngine.UIElements.Label)>();
        bool pad;

        /// A record for the modern pages' caps (AddCap).
        public static OwnKeyGlyphs ForCaps() => new OwnKeyGlyphs();
        /// A key cap label that shows the button the action ("Accept", "Start") is on: the device in use (OuGlyphs), always shown.
        public void AddCap(string action, UnityEngine.UIElements.Label cap) { caps.Add((action, cap)); Refresh(true); }

        /// How many KeyMap images the layer has.
        public int Count => slots.Count + caps.Count;
        /// How many of them show a glyph or a key name now (ReflexKeyHints on).
        public int Shown { get { int n = 0; foreach (var s in slots) if (s.glyph && s.glyph.enabled || s.key && s.key.gameObject.activeSelf) n++; foreach (var c in caps) if (c.cap != null && c.cap.panel != null && !string.IsNullOrEmpty(c.cap.text)) n++; return n; } }
        /// What each image shows ("Accept=ENTER", "Start=ESC", or "Accept=pad:Xenon_Controller_Button_1"), for tests and logs.
        public string Describe()
        {
            var parts = new List<string>();
            foreach (var s in slots)
                parts.Add(s.action + "=" + (s.glyph && s.glyph.enabled ? "pad:" + (s.glyph.texture ? s.glyph.texture.name : "?") : s.key && s.key.gameObject.activeSelf ? s.key.Text : "-"));
            foreach (var c in caps) parts.Add(c.action + "=" + c.cap.text);
            return string.Join(",", parts);
        }

        /// Finds the layer's KeyMap images (layerJson: the layer's props tree; the paths are the elements', relative to the layer root).
        public static OwnKeyGlyphs Attach(OriginalUiTree tree, Dictionary<string, object> layerJson)
        {
            var g = new OwnKeyGlyphs();
            if (tree == null || layerJson == null) return g;
            void Walk(Dictionary<string, object> node, string path)
            {
                if (node.TryGetValue("props", out var po) && po is Dictionary<string, object> props && props.TryGetValue("KeyMap", out var km) && km is string s && s.StartsWith("MapKey_UI_", StringComparison.Ordinal)
                    && path != null && tree.byPath.TryGetValue(path, out var t))
                    g.slots.Add(new Slot { node = t, action = s.Substring("MapKey_UI_".Length) });
                if (node.TryGetValue("children", out var co) && co is List<object> kids)
                    foreach (var c in kids)
                        if (c is Dictionary<string, object> cd)
                        {
                            string name = cd.TryGetValue("props", out var cp) && cp is Dictionary<string, object> cpd && cpd.TryGetValue("Name", out var nm) ? nm as string : null;
                            if (name == null && cd.TryGetValue("name", out var nn)) name = nn as string;
                            Walk(cd, path == null ? name : path + "/" + name);
                        }
            }
            if (layerJson.TryGetValue("root", out var r) && r is Dictionary<string, object> root) Walk(root, null);
            // a font for the key names: the page's own text
            OriginalFont font = null;
            foreach (var n in tree.byPath.Values) if (n.text && n.text.font != null) { font = n.text.font; break; }
            foreach (var s in g.slots) g.Build(s, font);
            g.Refresh(true);
            return g;
        }

        void Build(Slot s, OriginalFont font)
        {
            var rect = s.node.rect;
            var gl = new GameObject("glyph", typeof(RectTransform), typeof(RawImage)); gl.transform.SetParent(rect, false);
            var grt = (RectTransform)gl.transform; grt.anchorMin = grt.anchorMax = grt.pivot = new Vector2(0.5f, 0.5f); grt.anchoredPosition = Vector2.zero; grt.sizeDelta = rect.sizeDelta;
            s.glyph = gl.GetComponent<RawImage>(); s.glyph.raycastTarget = false; s.glyph.enabled = false;
            var k = new GameObject("key", typeof(RectTransform), typeof(OriginalText)); k.transform.SetParent(rect, false);
            var krt = (RectTransform)k.transform; krt.anchorMin = krt.anchorMax = krt.pivot = new Vector2(0.5f, 0.5f); krt.anchoredPosition = Vector2.zero;
            s.key = k.GetComponent<OriginalText>(); s.key.raycastTarget = false; s.key.color = Color.white; s.key.font = font; s.key.textHeight = 22;
            k.SetActive(false);
        }

        /// The device may change while a page is up (a pad connected): called by the page's tick; `force` redraws.
        public void Refresh(bool force = false)
        {
            foreach (var c in caps)
            {
                string text = c.action == "Start" ? OuGlyphs.Menu : OuGlyphs.Submit;
                if (c.cap.text != text) c.cap.text = text;
                string cls = OuGlyphs.CapClass(text);
                foreach (var k in new[] { "mxa-key-pad", "mxa-key-a", "mxa-key-b", "mxa-key-x", "mxa-key-y", "mxa-key-menu", "mxa-key-shoulder" }) c.cap.RemoveFromClassList(k);
                if (cls != null) foreach (var k in cls.Split(' ')) c.cap.AddToClassList(k);
            }
            bool usePad = Gamepad.current != null;
            if (!force && usePad == pad) return;
            pad = usePad;
            var data = OriginalUiAssets.Data;
            foreach (var s in slots)
            {
                if (!s.glyph) continue;
                if (s.node.graphic) s.node.graphic.enabled = false;   // the layer's own image (1x1White) is what the game swaps for the glyph / UIEmpty
                if (!Reflex.Port.OriginalWorld.ReflexKeyMapGlyphs.PortHints) { s.glyph.enabled = false; s.key.gameObject.SetActive(false); continue; }   // the PC game's UIEmpty
                if (usePad && Pad.TryGetValue(s.action, out var sn) && data.Atlas("BaseLocalized", "ControllerButtons").TryGetValue(sn, out var sp))
                {
                    s.glyph.texture = OriginalUiAssets.Texture(data.TextureKeyOf("BaseLocalized", sp.texture));
                    s.glyph.uvRect = new Rect(sp.u0, 1f - sp.v1, sp.u1 - sp.u0, sp.v1 - sp.v0);
                    Vector2 slot = s.node.rect.sizeDelta; float k = Mathf.Min(slot.x / sp.w, slot.y / sp.h);
                    ((RectTransform)s.glyph.transform).sizeDelta = new Vector2(sp.w * k, sp.h * k);
                    s.glyph.enabled = true; s.key.gameObject.SetActive(false);
                }
                else
                {
                    string action = s.action == "Start" ? "Pause" : s.action;   // the notification's "press Start" is the pause request (OnPauseRequest)
                    string name = OriginalButtonLegend.KeyOfAction(action);
                    s.glyph.enabled = false;
                    s.key.gameObject.SetActive(name.Length > 0);
                    var size = s.node.rect.sizeDelta;
                    s.key.textHeight = Mathf.Min(24f, size.y * 0.75f);
                    s.key.Text = name;
                    float w = s.key.font != null ? s.key.font.Width(name, s.key.textHeight) : 0f;
                    if (w > size.x && w > 0) s.key.textHeight *= size.x / w;
                    s.key.Resize();
                }
            }
        }
    }
}
