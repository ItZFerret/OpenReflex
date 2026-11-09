// The STATUS BOARDS of the compound (OpenMenuWorld): the four big boards at its corners (UI_COM_SBoard_01 models on scaffolds; the scene's
// StatusBoardEntity: font ParalucentCondDemBol48, TextHeight 8, TextColor (0.87, 0.87, 0.87), BoardWidth 90, BoardHeight 47, PosterWidth 35).
//
// What the original does: the boards' faces are the prelit UI_COM_SBoard_BG material (Template_Prelit_Map_Ctl, usr_prelit_power 0.465: a dark slate
// texture, avg 0.17, so the board is dim by design), and the StatusBoardEntity (00413d80; vf5 00413ed0, tick vf19 00416530, text rebuild 00414900)
// draws over it a TextIcon3dBillboard that holds the EVENT THE UI HAS UNDER ITS CURSOR: the event location's title (the StoreProduct's localised title),
// the event type's name, "%i LAPS" / "%i MINUTES" (EventTypeDurations' Singular / PluralStringKey) and the event's POSTER (PosterIconId), a poster of
// PosterWidth 35 ft at the board's left. Nothing under the cursor (the hub, the garage: no event) leaves the slate bare. The port's menu shows the same:
// the event the hub continues (the last one started, else the first National), the event under the RACE browser's cursor.
//
// How here: the text and the poster are laid out by an offscreen UI Toolkit panel (the shell's font, the original's colours) into a render texture
// that a quad over each board's face shows with the board's own prelit program (Reflex/Original Scenery mode 1: the colour times usr_prelit_power,
// so the picture is as bright as the face it replaces; the quad carries the slate itself behind the text). No core, no per-frame cost but the
// panel's own (two labels).
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    internal sealed class OpenStatusBoards
    {
        public const int Width = 2048, Height = 1088;   // (BoardWidth 90 x BoardHeight 47 ft)
        const float BoardWidthFt = 90f, PosterWidthFt = 35f, TextHeightFt = 8f;
        /// The slate behind the text, as bright as the board's own face (the texture's average 0.171, read raw as the scenery program reads it, x usr_prelit_power 0.465).
        public static Color Slate = new Color(0.171f, 0.171f, 0.177f, 1f);
        public static Color TextColor = new Color(0.870588f, 0.870588f, 0.870588f, 1f);

        readonly GameObject root;
        readonly RenderTexture rt;
        readonly PanelSettings panel;
        readonly VisualElement poster, copy;
        readonly Label line1, line2, line3;
        readonly List<GameObject> faces = new List<GameObject>();
        Material material;
        string shown;
        public int Count => faces.Count;
        public string Shown => shown;

        public OpenStatusBoards(OpenMenuWorld world, OriginalWorld.OriginalLevel level)
        {
            root = new GameObject("Status boards (the event under the cursor, OpenStatusBoards)"); root.transform.SetParent(world.transform, false);
            rt = new RenderTexture(Width, Height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear) { name = "status board", hideFlags = HideFlags.DontSave, antiAliasing = 1, useMipMap = true, autoGenerateMips = true, anisoLevel = 8, filterMode = FilterMode.Trilinear, wrapMode = TextureWrapMode.Clamp };
            // the offscreen panel
            var asset = Resources.Load<PanelSettings>(OuUi.ResourceRoot + "OuPanelSettings");
            panel = asset ? UnityEngine.Object.Instantiate(asset) : ScriptableObject.CreateInstance<PanelSettings>();
            if (!asset) panel.themeStyleSheet = Resources.Load<ThemeStyleSheet>(OuUi.ResourceRoot + "OuTheme");
            panel.name = "status board panel"; panel.targetTexture = rt; panel.scaleMode = PanelScaleMode.ConstantPixelSize; panel.scale = 1f;
            panel.clearColor = true; panel.colorClearValue = Slate; panel.sortingOrder = -500;
            var go = new GameObject("status board panel"); go.transform.SetParent(root.transform, false);
            doc = go.AddComponent<UIDocument>(); doc.panelSettings = panel;
            var ui = doc.rootVisualElement; uiRoot = ui;
            ui.style.width = Width; ui.style.height = Height; ui.style.backgroundColor = Slate; ui.pickingMode = PickingMode.Ignore;
            ui.style.flexDirection = FlexDirection.Row; ui.style.alignItems = Align.Center; ui.style.paddingLeft = ui.style.paddingRight = 120; ui.style.paddingTop = ui.style.paddingBottom = 80; ui.style.justifyContent = Justify.Center;
            var bold = Resources.Load<Font>(OuUi.ResourceRoot + "Fonts/BarlowCondensed-Bold");
            var italic = Resources.Load<Font>(OuUi.ResourceRoot + "Fonts/BarlowCondensed-ExtraBoldItalic");
            poster = new VisualElement(); poster.style.flexShrink = 0; poster.style.marginRight = 96;
            poster.style.unityBackgroundScaleMode = ScaleMode.ScaleAndCrop;
            poster.style.borderTopWidth = poster.style.borderBottomWidth = poster.style.borderLeftWidth = poster.style.borderRightWidth = 8;
            poster.style.borderTopColor = poster.style.borderBottomColor = poster.style.borderLeftColor = poster.style.borderRightColor = new Color(0.9f, 0.9f, 0.85f, 1f);
            ui.Add(poster);
            copy = new VisualElement(); copy.style.flexGrow = 1; copy.style.flexShrink = 1; copy.style.flexDirection = FlexDirection.Column; copy.style.justifyContent = Justify.Center;
            ui.Add(copy);
            Label L(Font f, Color c)
            {
                var l = new Label(); l.style.color = c; l.style.whiteSpace = WhiteSpace.NoWrap; l.style.overflow = Overflow.Hidden; l.style.marginTop = l.style.marginBottom = 0; l.style.paddingTop = l.style.paddingBottom = 0;
                l.style.unityTextAlign = TextAnchor.MiddleLeft;
                if (f) l.style.unityFontDefinition = new StyleFontDefinition(FontDefinition.FromFont(f));
                copy.Add(l); return l;
            }
            line1 = L(italic ? italic : bold, TextColor);
            line2 = L(bold, new Color(0.94f, 0.47f, 0.2f, 1f));
            line3 = L(bold, new Color(TextColor.r * 0.82f, TextColor.g * 0.82f, TextColor.b * 0.82f, 1f));
            // the OPEN REFLEX logo (the menu world's boards show it; races and other levels keep their boards)
            logo = new VisualElement(); logo.pickingMode = PickingMode.Ignore;
            var tex = Resources.Load<Texture2D>(OuUi.ResourceRoot + "OpenReflexLogo");
            if (tex) { float lw = Width * 0.74f; logo.style.width = lw; logo.style.height = lw * tex.height / tex.width; logo.style.backgroundImage = tex; logo.style.unityBackgroundScaleMode = ScaleMode.ScaleToFit; ui.Add(logo); }
            logo.style.display = DisplayStyle.None;
            Relocate(level, world);
            BuildFaces(level);
            if (LogoMode) ShowLogo();
        }

        /// The board the hub's and the garage's cameras look at (the one at the compound's (+333, -335) corner, the only one in their views) is moved along
        /// its circle about the hero spot by BoardArc degrees (its scaffold with it, stood on the ground there), so the logo clears the hub's menu column
        /// and the rider (menu world only: this is the front end's own copy of the level).
        public static float BoardArc = 10f;
        /// ... and made this much bigger about its foot (taller on its scaffold: the logo clears the bike's bars in the hub and the garage's header pills).
        public static float BoardScale = 1.35f;
        void Relocate(OriginalWorld.OriginalLevel level, OpenMenuWorld world)
        {
            if (level?.SceneryRoot == null || Mathf.Abs(BoardArc) < 0.01f) return;
            var pivot = new Vector3(OpenMenuWorld.HeroSpot.x, 0, OpenMenuWorld.HeroSpot.z);
            int moved = 0;
            foreach (var t in level.SceneryRoot.GetComponentsInChildren<Transform>(true))
            {
                if (!t.name.StartsWith("UI_COM_SBoard_", StringComparison.Ordinal)) continue;
                var p = t.position;
                if (Mathf.Abs(p.x - 332.67f) > 2f || Mathf.Abs(p.z + 335.17f) > 2f) continue;
                float g0 = world.GroundAt(p.x, p.z);
                t.RotateAround(pivot, Vector3.up, BoardArc);
                t.localScale *= BoardScale;
                var q = t.position; q.y = p.y + world.GroundAt(q.x, q.z) - g0; t.position = q;
                moved++;
            }
            Debug.Log($"[StatusBoards] {moved} models of the visible board moved {BoardArc:F1} degrees about the hero spot");
        }

        /// The boards show the OPEN REFLEX logo (the lead's choice for the menu world) instead of the event under the cursor: Show(ev) is then ignored.
        public static bool LogoMode = true;
        /// The logo's slate behind it (darker than the event slate so the lit sign reads in the dark compound) and the face's brightness for it
        /// (usr_prelit_power 0.465 is the original face's; the logo is an emissive sign).
        public static Color LogoSlate = new Color(0.045f, 0.045f, 0.05f, 1f);
        public static float LogoPower = 1.55f;
        readonly VisualElement logo;
        UIDocument doc; int live = 8;
        VisualElement uiRoot;
        public void ShowLogo()
        {
            if (logo == null) return;
            poster.style.display = DisplayStyle.None; copy.style.display = DisplayStyle.None; logo.style.display = DisplayStyle.Flex;
            panel.colorClearValue = LogoSlate; uiRoot.style.backgroundColor = LogoSlate;
            if (material) material.SetFloat("_PrelitPower", LogoPower);
            shown = "logo"; Wake();
        }

        // the panel draws for a few frames after every change, then sleeps (the render texture keeps what it drew): no per-frame cost for a still board
        void Wake() { live = 8; if (doc) doc.enabled = true; }
        /// Once a frame (OpenMenuWorld.Update).
        public void Tick() { if (live > 0 && --live == 0 && doc) doc.enabled = false; }

        // the board faces: the BG submesh (material 0) of every UI_COM_SBoard_01 instance
        void BuildFaces(OriginalWorld.OriginalLevel level)
        {
            var shader = Shader.Find("Reflex/Original Scenery");
            if (!shader || level?.SceneryRoot == null) return;
            material = new Material(shader) { name = "status board face (prelit)", hideFlags = HideFlags.DontSave };
            material.SetFloat("_Mode", 1f); material.SetFloat("_PrelitPower", 0.4651163f); material.SetFloat("_EncodeBase", 1f); material.SetTexture("_BaseMap", rt);
            material.SetVector("_BaseColor", Vector4.one); material.SetFloat("_Cull", (float)UnityEngine.Rendering.CullMode.Off);
            material.SetFloat("_SrcBlend", (float)UnityEngine.Rendering.BlendMode.One); material.SetFloat("_DstBlend", (float)UnityEngine.Rendering.BlendMode.Zero); material.SetFloat("_ZWrite", 1f);
            foreach (var mr in level.SceneryRoot.GetComponentsInChildren<MeshRenderer>(true))
            {
                if (!mr.name.StartsWith("UI_COM_SBoard_01", StringComparison.Ordinal)) continue;
                // (the level's meshes are uploaded unreadable: the geometry comes from the model data the content was built from)
                if (!level.Content.sceneModels.TryGetValue("UI_COM_SBoard_01", out var md) || md.triangles.Count < 1) continue;
                try { var face = Face(mr.transform, md); if (face != null) faces.Add(face); }
                catch (Exception e) { Debug.LogWarning("[StatusBoards] " + mr.name + ": " + e.Message); }
            }
            level.Content.sceneModels.TryGetValue("UI_COM_SBoard_01", out var mdl);
            Debug.Log($"[StatusBoards] {faces.Count} board faces over the UI_COM_SBoard_01 models (bindings: {(mdl == null ? "-" : string.Join(", ", mdl.bindingMaterials))}; face = binding 0)");
        }

        GameObject Face(Transform t, ReflexLevelContent.ModelData md)
        {
            var tris = md.triangles[0]; var vs = md.positions; var ns = md.normals ?? new Vector3[0];
            if (tris.Length == 0) return null;
            var idx = tris.Distinct().ToArray();
            var n = Vector3.zero; foreach (int i in idx) if (i < ns.Length) n += ns[i];
            n = t.TransformDirection(n).normalized;
            var world = idx.Select(i => t.TransformPoint(vs[i])).ToArray();
            var centre = world.Aggregate(Vector3.zero, (a, p) => a + p) / world.Length;
            if (Vector3.Dot(n, new Vector3(-centre.x, 0, -centre.z)) < 0f) n = -n;          // (the boards face the middle of the compound)
            var up = Vector3.up; var right = Vector3.Cross(n, up).normalized;                  // (the viewer's right, looking against n)
            if (right.sqrMagnitude < 0.5f) return null;
            float r0 = float.MaxValue, r1 = float.MinValue, u0 = float.MaxValue, u1 = float.MinValue, d = 0f;
            foreach (var p in world)
            {
                float r = Vector3.Dot(p - centre, right), u = Vector3.Dot(p - centre, up);
                r0 = Mathf.Min(r0, r); r1 = Mathf.Max(r1, r); u0 = Mathf.Min(u0, u); u1 = Mathf.Max(u1, u);
                d = Mathf.Max(d, Vector3.Dot(p - centre, n));
            }
            var c = centre + right * (r0 + r1) * 0.5f + up * (u0 + u1) * 0.5f + n * (d + 0.15f);   // (a hand in front of the face)
            float hw = (r1 - r0) * 0.5f * 0.965f, hh = (u1 - u0) * 0.5f * 0.94f;
            var q = new Mesh { name = "status board face" };
            q.vertices = new[] { c - right * hw - up * hh, c + right * hw - up * hh, c + right * hw + up * hh, c - right * hw + up * hh };
            q.uv = new[] { new Vector2(0, 0), new Vector2(1, 0), new Vector2(1, 1), new Vector2(0, 1) };
            q.normals = new[] { n, n, n, n };
            q.colors = new[] { Color.white, Color.white, Color.white, Color.white };
            q.triangles = new[] { 0, 2, 1, 0, 3, 2 };
            q.RecalculateBounds();
            var go = new GameObject("Status board face"); go.transform.SetParent(root.transform, false);
            go.AddComponent<MeshFilter>().sharedMesh = q;
            var rend = go.AddComponent<MeshRenderer>(); rend.sharedMaterial = material;
            rend.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off; rend.receiveShadows = false;
            return go;
        }

        /// The board's text and poster for an event (null: the slate bare, as the original's boards are with nothing under the cursor).
        public void Show(OpenEvent ev, OpenSetup setup = null)
        {
            if (LogoMode) return;
            string key = ev?.key ?? "";
            if (key == shown) return;
            shown = key; Wake();
            if (ev == null) { poster.style.display = DisplayStyle.None; copy.style.display = DisplayStyle.None; return; }
            copy.style.display = DisplayStyle.Flex;
            string title = (ev.locationName ?? "").ToUpperInvariant(), type = (ev.typeName ?? "").ToUpperInvariant(), duration = "";
            try
            {
                var s = setup ?? OpenHistory.SetupOf(ev.key); var e = ev.ev;
                var d = s.Durations(e, out bool laps, out _, out _);
                int i = s.DurationIndex(d);
                if (i >= 0) duration = d[i] + (laps ? (d[i] == 1 ? " LAP" : " LAPS") : (d[i] == 1 ? " MINUTE" : " MINUTES"));
            }
            catch (Exception) { }
            float px = Height / 47f;                                   // pixels per foot
            Fit(line1, title, TextHeightFt * px * 1.0f, Width * 0.625f);
            Fit(line2, type, TextHeightFt * px * 0.72f, Width * 0.625f);
            Fit(line3, duration, TextHeightFt * px * 0.62f, Width * 0.625f);
            var tex = OpenContent.Icon(ev.posterIcon) ?? OpenContent.Picture(ev);
            if (tex)
            {
                float w = PosterWidthFt / BoardWidthFt * Width, h = w * tex.height / tex.width;
                float maxH = Height * 0.78f; if (h > maxH) { h = maxH; w = h * tex.width / tex.height; }
                poster.style.width = w; poster.style.height = h; poster.style.backgroundImage = tex; poster.style.display = DisplayStyle.Flex;
            }
            else poster.style.display = DisplayStyle.None;
        }

        // one line of text at most size px high, shrunk so that it is at most maxWidth px wide (the face is narrow beside the poster)
        static void Fit(Label l, string text, float size, float maxWidth)
        {
            l.text = text; l.style.display = text.Length == 0 ? DisplayStyle.None : DisplayStyle.Flex;
            float fs = Mathf.Min(size, maxWidth / Mathf.Max(1f, text.Length * 0.43f));
            l.style.fontSize = Mathf.Max(26f, fs);
        }

        public void Destroy()
        {
            if (root) UnityEngine.Object.Destroy(root);
            if (rt) { rt.Release(); UnityEngine.Object.Destroy(rt); }
            if (panel) UnityEngine.Object.Destroy(panel);
            if (material) UnityEngine.Object.Destroy(material);
            faces.Clear();
        }
    }
}
