using System;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OriginalWorld
{
    /// The hovercraft's on-screen parts (port extra): the "ENTER HOVERCRAFT" prompt with the interact glyph (E on the keyboard, Y / Triangle
    /// on a pad: the device in use, OuGlyphs), the driving read-out and the fade that covers the swap. UI Toolkit on the shell's panel settings.
    public sealed class HoverUi
    {
        readonly GameObject go; readonly UIDocument document;
        readonly VisualElement prompt, cap, driving, fade, toast;
        readonly Label capText, title, sub, speed, craftName, exitCap, exitText, toastText;
        float fadeLevel, promptShown, toastUntil;
        string lastGlyph;

        public bool Ready => document != null;
        public float Fade => fadeLevel;
        public bool PromptVisible => prompt != null && prompt.resolvedStyle.opacity > 0.5f;
        public string PromptText => title?.text;
        public string GlyphText => capText?.text;

        public HoverUi(Transform parent)
        {
            var settings = Resources.Load<PanelSettings>("OpenFrontEnd/OuPanelSettings");
            if (!settings) { Debug.LogWarning("Hovercraft UI: OuPanelSettings missing."); return; }
            go = new GameObject("Hovercraft UI"); go.transform.SetParent(parent, false);
            document = go.AddComponent<UIDocument>(); document.panelSettings = settings; document.sortingOrder = 880;
            var root = document.rootVisualElement; root.pickingMode = PickingMode.Ignore;
            root.style.position = Position.Absolute; root.style.left = 0; root.style.right = 0; root.style.top = 0; root.style.bottom = 0;

            // the prompt: [E]  ENTER HOVERCRAFT, low in the middle of the screen
            prompt = Box(root); prompt.style.bottom = 150; prompt.style.alignSelf = Align.Center; prompt.style.left = new StyleLength(Length.Percent(50));
            prompt.style.translate = new Translate(Length.Percent(-50), 0);
            prompt.style.flexDirection = FlexDirection.Row; prompt.style.alignItems = Align.Center;
            prompt.style.paddingLeft = 22; prompt.style.paddingRight = 30; prompt.style.paddingTop = 14; prompt.style.paddingBottom = 14;
            prompt.style.backgroundColor = new Color(0.04f, 0.05f, 0.07f, 0.78f);
            Round(prompt, 14); prompt.style.borderBottomWidth = 3; prompt.style.borderBottomColor = new Color(1f, 0.74f, 0.12f, 1f);
            cap = new VisualElement { pickingMode = PickingMode.Ignore }; prompt.Add(cap);
            capText = new Label { pickingMode = PickingMode.Ignore }; cap.Add(capText);
            var texts = new VisualElement { pickingMode = PickingMode.Ignore }; texts.style.marginLeft = 18; prompt.Add(texts);
            title = Text(texts, "ENTER HOVERCRAFT", 30, FontStyle.Bold, Color.white);
            sub = Text(texts, "", 17, FontStyle.Normal, new Color(1f, 1f, 1f, 0.62f));
            prompt.style.opacity = 0;

            // while driving: the speed, bottom left; the way out, bottom right
            driving = Box(root); driving.style.left = 70; driving.style.bottom = 56; driving.style.flexDirection = FlexDirection.Row; driving.style.alignItems = Align.FlexEnd;
            driving.style.paddingLeft = 22; driving.style.paddingRight = 24; driving.style.paddingTop = 10; driving.style.paddingBottom = 12;
            driving.style.backgroundColor = new Color(0.04f, 0.05f, 0.07f, 0.66f); Round(driving, 12);
            speed = Text(driving, "0", 54, FontStyle.Bold, Color.white);
            var unit = new VisualElement { pickingMode = PickingMode.Ignore }; unit.style.marginLeft = 10; unit.style.marginBottom = 6; driving.Add(unit);
            Text(unit, "MPH", 20, FontStyle.Bold, new Color(1f, 0.74f, 0.12f, 1f));
            craftName = Text(unit, "HOVERCRAFT", 14, FontStyle.Normal, new Color(1f, 1f, 1f, 0.6f));
            driving.style.display = DisplayStyle.None;
            var exit = Box(root); exit.name = "exit"; exit.style.right = 70; exit.style.bottom = 56; exit.style.flexDirection = FlexDirection.Row; exit.style.alignItems = Align.Center;
            exit.style.paddingLeft = 14; exit.style.paddingRight = 20; exit.style.paddingTop = 8; exit.style.paddingBottom = 8;
            exit.style.backgroundColor = new Color(0.04f, 0.05f, 0.07f, 0.66f); Round(exit, 12); exit.style.display = DisplayStyle.None;
            exitCap = new Label { pickingMode = PickingMode.Ignore }; exit.Add(exitCap);
            exitText = Text(exit, "LEAVE HOVERCRAFT", 18, FontStyle.Bold, Color.white); exitText.style.marginLeft = 12;

            toast = Box(root); toast.style.top = 120; toast.style.left = new StyleLength(Length.Percent(50)); toast.style.translate = new Translate(Length.Percent(-50), 0);
            toast.style.paddingLeft = 24; toast.style.paddingRight = 24; toast.style.paddingTop = 10; toast.style.paddingBottom = 10;
            toast.style.backgroundColor = new Color(0.04f, 0.05f, 0.07f, 0.8f); Round(toast, 10);
            toastText = Text(toast, "", 22, FontStyle.Bold, Color.white); toast.style.opacity = 0;

            fade = Box(root); fade.style.left = 0; fade.style.right = 0; fade.style.top = 0; fade.style.bottom = 0; fade.style.backgroundColor = Color.black; fade.style.opacity = 0;
        }

        static VisualElement Box(VisualElement parent)
        {
            var v = new VisualElement { pickingMode = PickingMode.Ignore }; v.style.position = Position.Absolute; parent.Add(v); return v;
        }
        static Label Text(VisualElement parent, string text, int size, FontStyle style, Color color)
        {
            var l = new Label(text) { pickingMode = PickingMode.Ignore };
            l.style.fontSize = size; l.style.unityFontStyleAndWeight = style; l.style.color = color; l.style.whiteSpace = WhiteSpace.NoWrap;
            l.style.marginLeft = 0; l.style.marginRight = 0; l.style.marginTop = 0; l.style.marginBottom = 0; l.style.paddingLeft = 0; l.style.paddingRight = 0; l.style.paddingTop = 0; l.style.paddingBottom = 0;
            parent.Add(l); return l;
        }
        static void Round(VisualElement e, float r)
        {
            e.style.borderTopLeftRadius = r; e.style.borderTopRightRadius = r; e.style.borderBottomLeftRadius = r; e.style.borderBottomRightRadius = r;
        }

        // The key cap of the interact action: E on the keyboard, the pad's button (Y / Triangle) otherwise.
        public static string Glyph()
        {
            switch (OuGlyphs.Current) { case OuGlyphs.Device.Xbox: return "Y"; case OuGlyphs.Device.PlayStation: return "Triangle"; default: return "E"; }
        }

        void StyleCap(Label label)
        {
            string g = Glyph(); var dev = OuGlyphs.Current;
            label.text = g;
            var s = label.style; bool pad = dev != OuGlyphs.Device.Keyboard;
            s.fontSize = g.Length > 2 ? 17 : 28; s.unityFontStyleAndWeight = FontStyle.Bold; s.unityTextAlign = TextAnchor.MiddleCenter;
            s.minWidth = 52; s.height = 52; s.paddingLeft = g.Length > 2 ? 12 : 0; s.paddingRight = g.Length > 2 ? 12 : 0;
            float r = pad && g.Length <= 2 ? 26 : 10; s.borderTopLeftRadius = r; s.borderTopRightRadius = r; s.borderBottomLeftRadius = r; s.borderBottomRightRadius = r;
            s.borderTopWidth = s.borderLeftWidth = s.borderRightWidth = 2; s.borderBottomWidth = pad ? 2 : 5;
            if (!pad) { s.backgroundColor = new Color(0.93f, 0.94f, 0.95f); s.color = new Color(0.1f, 0.11f, 0.13f); var b = new Color(0.55f, 0.57f, 0.6f); s.borderTopColor = s.borderLeftColor = s.borderRightColor = s.borderBottomColor = b; }
            else
            {
                var c = dev == OuGlyphs.Device.Xbox ? new Color(0.98f, 0.78f, 0.13f) : new Color(0.25f, 0.78f, 0.5f);   // Y: yellow, Triangle: green
                s.backgroundColor = new Color(c.r * 0.22f, c.g * 0.22f, c.b * 0.22f, 1f); s.color = c;
                s.borderTopColor = s.borderLeftColor = s.borderRightColor = s.borderBottomColor = c;
            }
        }

        /// Per frame. promptOn: show the ENTER prompt (sub: its second line); drive: the driving read-out with the speed in mph.
        public void Tick(float dt, bool promptOn, string subline, bool drive, float mph, float fadeTarget)
        {
            if (document == null) return;
            promptShown = Mathf.MoveTowards(promptShown, promptOn ? 1f : 0f, dt * 6f);
            prompt.style.opacity = promptShown; prompt.style.display = promptShown > 0.01f ? DisplayStyle.Flex : DisplayStyle.None;
            if (promptShown > 0.01f || drive)
            {
                string g = Glyph() + "|" + (int)OuGlyphs.Current;
                if (g != lastGlyph) { lastGlyph = g; StyleCap(capText); StyleCap(exitCap); }
            }
            if (promptOn && sub.text != subline) sub.text = subline ?? "";
            sub.style.display = string.IsNullOrEmpty(sub.text) ? DisplayStyle.None : DisplayStyle.Flex;
            driving.style.display = drive ? DisplayStyle.Flex : DisplayStyle.None;
            var exit = document.rootVisualElement.Q("exit"); if (exit != null) exit.style.display = drive ? DisplayStyle.Flex : DisplayStyle.None;
            if (drive) speed.text = Mathf.RoundToInt(mph).ToString();
            fadeLevel = Mathf.MoveTowards(fadeLevel, fadeTarget, dt / 0.28f);
            fade.style.opacity = fadeLevel; fade.style.display = fadeLevel > 0.001f ? DisplayStyle.Flex : DisplayStyle.None;
            float ta = Time.unscaledTime < toastUntil ? 1f : 0f; toast.style.opacity = Mathf.MoveTowards(toast.style.opacity.value, ta, dt * 5f);
        }

        public void Toast(string text, float seconds = 2.2f) { if (toastText == null) return; toastText.text = text; toastUntil = Time.unscaledTime + seconds; }
        public void SetFade(float v) { fadeLevel = v; }

        public void Dispose() { if (go) UnityEngine.Object.Destroy(go); }
    }
}
