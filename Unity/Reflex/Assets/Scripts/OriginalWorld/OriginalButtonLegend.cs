using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The original button-legend strip (layer ButtonLegend.lyr of InGame, driven as ButtonLegend.lua does): the gradient
    // backing at y 636 and a left-to-right list (AutoFormat: hidden buttons take no room; 200 px each from (128,604)) of the six
    // buttons accept / back / toggle / misc / dev / start, each an icon plus a localised label. A button comes in with its
    // <Name>In animation (0.167 s) and leaves with <Name>Out (0.2 s), then is hidden. The icons are the game's controller glyphs
    // (BaseLocalized ControllerButtons atlas, Xbox 360 set: A accept, B back, X toggle, Y misc, Start, Back = dev/select, the
    // pad bindings of KeyBind_Core.xml) when a gamepad is connected; with the keyboard the slot shows the key bound to the
    // same MapKey_UI_* action in KeyBind_Core.xml. WP-C traced the original's KeyMap image (OriginalInput.cs ReflexKeyMapGlyphs): on
    // the PC it is UIEmpty, an empty slot; that is the default now, and these glyphs / key names are a port extra (ReflexKeyHints 1).
    public sealed class OriginalButtonLegend
    {
        public static readonly string[] Buttons = { "accept", "back", "toggle", "misc", "dev", "start" };

        sealed class Entry
        {
            public string name, label, inAnim, outAnim; public float outLength, hideTimer = -1;
            public OriginalUiTree.Node container, icon, text; public bool requested;
            public RawImage glyph; public OriginalText key; public Action click;
        }

        public readonly OriginalUiTree tree;
        readonly Dictionary<string, Entry> entries = new Dictionary<string, Entry>();
        readonly Dictionary<string, float> animLength = new Dictionary<string, float>();
        readonly Dictionary<string, string> loc;
        readonly ReflexUiData data;
        bool pad;
        static Dictionary<string, string> keyboard;

        public OriginalButtonLegend(RectTransform parent)
        {
            data = OriginalUiAssets.Data; loc = data.Strings("InGame", "MXRaven_Default_Strings");
            tree = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
            var layer = data.BuildLayer("InGame", "ButtonLegend.lyr");
            foreach (var e in OriginalUi.Arr(layer, "elements")) tree.Build((Dictionary<string, object>)e, parent, null);
            var anims = OriginalUi.Arr(layer, "layerAnimations");
            tree.AddAnimations(anims);
            foreach (var a in anims) animLength[OriginalUi.S(a, "name")] = OriginalUi.F(a, "length");
            foreach (string b in Buttons)
            {
                string cap = char.ToUpperInvariant(b[0]) + b.Substring(1);
                var e = new Entry { name = b, container = tree.Find("ButtonLegend/" + b + "Container") };
                if (e.container == null) continue;
                e.icon = tree.Child(e.container, b + "Button"); e.text = tree.Child(e.container, "Text");
                e.inAnim = animLength.ContainsKey(cap + "In") ? cap + "In" : null; e.outAnim = animLength.ContainsKey(cap + "Out") ? cap + "Out" : null;
                e.outLength = e.outAnim != null ? animLength[e.outAnim] : 0;
                BuildIcon(e);
                OriginalUiTree.SetVisible(e.container, false);   // ButtonLegend.lua hides everything; layers turn buttons on
                entries[b] = e;
            }
        }

        // The icon slot: a glyph (controller sprite) and a key label (keyboard), one of them shown.
        void BuildIcon(Entry e)
        {
            if (e.icon == null) return;
            var g = new GameObject("glyph", typeof(RectTransform), typeof(RawImage)); g.transform.SetParent(e.icon.rect, false);
            var rt = (RectTransform)g.transform; rt.anchorMin = rt.anchorMax = rt.pivot = new Vector2(0.5f, 0.5f); rt.anchoredPosition = Vector2.zero; rt.sizeDelta = e.icon.rect.sizeDelta;
            e.glyph = g.GetComponent<RawImage>(); e.glyph.raycastTarget = false;
            var k = new GameObject("key", typeof(RectTransform), typeof(OriginalText)); k.transform.SetParent(e.icon.rect, false);
            var kt = (RectTransform)k.transform; kt.anchorMin = kt.anchorMax = kt.pivot = new Vector2(0.5f, 0.5f); kt.anchoredPosition = Vector2.zero;
            e.key = k.GetComponent<OriginalText>(); e.key.raycastTarget = false; e.key.color = Color.white;
            e.key.font = OriginalUiAssets.Font(FontKey()); e.key.textHeight = 22;
            // the whole 200 x 64 slot is the mouse target (MouseActivation routes a click on a legend container to its action)
            var h = new GameObject("hit", typeof(RectTransform), typeof(Image)); h.transform.SetParent(e.container.rect, false);
            var ht = (RectTransform)h.transform; ht.anchorMin = ht.anchorMax = new Vector2(0, 1); ht.pivot = new Vector2(0, 1); ht.anchoredPosition = Vector2.zero; ht.sizeDelta = e.container.rect.sizeDelta;
            var img = h.GetComponent<Image>(); img.color = new Color(0, 0, 0, 0); img.raycastTarget = true;
            var btn = h.AddComponent<Button>(); btn.transition = Selectable.Transition.None; btn.onClick.AddListener(() => e.click?.Invoke());
        }

        string fontKey;
        string FontKey()
        {
            if (fontKey != null) return fontKey;
            var t = tree.Find("ButtonLegend/backContainer/Text"); var f = t != null ? OriginalUi.Obj(OriginalUi.Obj(t.json, "style"), "font") : null;
            return fontKey = f != null ? OriginalUi.S(f, "metrics") : null;
        }

        /// What a mouse click on a button's slot does (accept / back / ...).
        public void SetAction(string button, Action click) { if (entries.TryGetValue(button, out var e)) e.click = click; }

        /// App.SetButtonLegend(button, visible, labelKey): show / hide a button, its label from the string table.
        public void Set(string button, bool visible, string labelKey = null)
        {
            if (!entries.TryGetValue(button, out var e)) return;
            if (visible)
            {
                e.hideTimer = -1;
                bool changed = labelKey != null && labelKey != e.label;
                if (changed) { e.label = labelKey; OriginalUiTree.SetText(e.text, loc.TryGetValue(labelKey, out var s) ? s : labelKey); }
                if ((!e.requested || changed) && e.inAnim != null) tree.Play(e.inAnim);
                OriginalUiTree.SetVisible(e.container, true);
                RefreshIcon(e);
            }
            else
            {
                if (e.requested && e.outAnim != null) { tree.Play(e.outAnim); e.hideTimer = e.outLength; }
                else OriginalUiTree.SetVisible(e.container, false);
            }
            e.requested = visible;
            Layout();
        }

        /// Hides every button at once (the page was left).
        public void Reset()
        {
            foreach (var e in entries.Values) { e.requested = false; e.hideTimer = -1; OriginalUiTree.SetVisible(e.container, false); }
        }

        public bool IsShown(string button) => entries.TryGetValue(button, out var e) && e.requested;

        /// ButtonLegend:OnAnimationComplete: an out animation that finished hides its button and the list re-flows.
        public void Tick(float dt)
        {
            tree.Tick(dt);
            bool devicePad = Gamepad.current != null;
            bool relayout = false;
            foreach (var e in entries.Values)
            {
                if (e.hideTimer >= 0) { e.hideTimer -= dt; if (e.hideTimer < 0) { OriginalUiTree.SetVisible(e.container, false); relayout = true; } }
                if (devicePad != pad && e.requested) RefreshIcon(e, devicePad);
            }
            pad = devicePad;
            if (relayout) Layout();
        }

        // AutoFormat list: the visible buttons from the list's left edge, each as wide as its slot.
        void Layout()
        {
            float x = 0;
            foreach (string b in Buttons)
            {
                if (!entries.TryGetValue(b, out var e) || !e.container.rect.gameObject.activeSelf) continue;
                e.container.basePos = new Vector2(x, e.container.basePos.y); e.container.rect.anchoredPosition = e.container.basePos;
                x += e.container.rect.sizeDelta.x;
            }
        }

        static readonly Dictionary<string, string> PadSprite = new Dictionary<string, string>
        {   // KeyBind_Core.xml xbox360 set: joy-a accept, joy-b back, joy-x toggle, joy-y misc, joy-start, joy-back (select)
            ["accept"] = "Xenon_Controller_Button_1", ["back"] = "Xenon_Controller_Button_2", ["toggle"] = "Xenon_Controller_Button_3",
            ["misc"] = "Xenon_Controller_Button_4", ["start"] = "Xenon_Controller_Button_7", ["dev"] = "Xenon_Controller_Button_8",
        };
        static readonly Dictionary<string, string> MapKey = new Dictionary<string, string>
        {
            ["accept"] = "Accept", ["back"] = "Back", ["toggle"] = "Toggle", ["misc"] = "Misc", ["start"] = "Start", ["dev"] = "Select",
        };

        void RefreshIcon(Entry e) => RefreshIcon(e, Gamepad.current != null);
        void RefreshIcon(Entry e, bool usePad)
        {
            if (e.icon == null) return;
            if (!ReflexKeyMapGlyphs.PortHints) { e.glyph.enabled = false; e.key.gameObject.SetActive(false); return; }   // the PC game's KeyMap image: UIEmpty (OriginalInput.cs ReflexKeyMapGlyphs)
            if (usePad && PadSprite.TryGetValue(e.name, out var sn) && data.Atlas("BaseLocalized", "ControllerButtons").TryGetValue(sn, out var sp))
            {
                e.glyph.texture = OriginalUiAssets.Texture(data.TextureKeyOf("BaseLocalized", sp.texture));
                e.glyph.uvRect = new Rect(sp.u0, 1f - sp.v1, sp.u1 - sp.u0, sp.v1 - sp.v0);
                Vector2 slot = e.icon.rect.sizeDelta; float k = Mathf.Min(slot.x / sp.w, slot.y / sp.h);
                ((RectTransform)e.glyph.transform).sizeDelta = new Vector2(sp.w * k, sp.h * k);
                e.glyph.enabled = true; e.key.gameObject.SetActive(false);
            }
            else
            {
                e.glyph.enabled = false; e.key.gameObject.SetActive(true);
                e.key.Text = KeyName(e.name);
            }
        }

        /// The key KeyBind_Core.xml binds to MapKey_UI_<Action>1 in its keyboard section (accept = enter, back = esc, start = f3 ...).
        public static string KeyName(string button) => MapKey.TryGetValue(button, out var a) ? KeyOfAction(a) : "";

        /// The key KeyBind_Core.xml binds to MapKey_UI_<action>1 (Accept, Back, Toggle, Misc, Start, Select, Pause ...), upper case; empty when unbound.
        public static string KeyOfAction(string action)
        {
            if (keyboard == null)
            {
                keyboard = new Dictionary<string, string>();
                try
                {
                    string xml = File.ReadAllText(ReflexInstall.PathOf("KeyBind_Core.xml"));
                    var sect = Regex.Match(xml, @"<keyboard>(.*?)</keyboard>", RegexOptions.Singleline);
                    foreach (Match m in Regex.Matches(sect.Value, @"<bind>\s*(\S+)\s+MapKey_UI_(\w+?)1\s*</bind>")) if (!keyboard.ContainsKey(m.Groups[2].Value)) keyboard[m.Groups[2].Value] = m.Groups[1].Value;
                }
                catch (Exception) { }
            }
            return action != null && keyboard.TryGetValue(action, out var k) ? k.ToUpperInvariant() : "";
        }
    }
}
