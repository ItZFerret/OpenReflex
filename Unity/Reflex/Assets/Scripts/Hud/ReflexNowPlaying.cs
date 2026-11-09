// NOW PLAYING: when a song starts (or SKIP SONG brings one) its title, artist, album / year and where it comes from slide in for a few seconds, after Open
// Unleashed's MxaNowPlaying, in the front end's look (w-uibatch, OuRestyle.uss: the career pages' square dark plate, the ORANGE tag with dark condensed caps
// for the game's tracks / GREEN for YOUR MUSIC, Barlow Condensed with the hard drop shadow; HudGroup's eased entrance and exit, REDUCE MOTION jumps). In a
// race a compact card at the bottom left (400 x 94: it was a 560 x 144 MODERN HUD panel); over the menus, and over a page drawn on a world (the career's
// gate selection, the pause / results pages: their bottom left holds the page) a slim strip down from the top centre. Its own canvas, so it shows with
// either HUD STYLE and the original front end alike.
//   * the race's music fades in over the activity's transition time (up to 30 s): the pop-up waits until the song is audible (the caller's level >= 0.3)
//   * not with OPTIONS > AUDIO > NOW PLAYING POP-UP off, nor in a race while the HUD is hidden (H, photo mode, replays, the podium)
//   * the title / artist / album come from the file's tags or its name (ReflexSongLibrary), or for the game's tracks from MusicAudioMap.bxml
// Callers: OriginalMusic (the races), FeUiMusic (the menus' songs); Notify.
using System;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.Hud
{
    public sealed class ReflexNowPlaying : MonoBehaviour
    {
        const float Hold = 6f, MaxWait = 60f, AudibleAt = 0.3f;
        // the card (a race): the tag over a plate with the title and the artist line; the strip (the menus): one line, the tag at its left
        const float CardW = 400f, TagH = 28f, PlateH = 66f, CardH = TagH + PlateH, StripH = 48f, Pad = 16f, Bar = 6f;
        // OuRestyle.uss's palette (--oc-*)
        static readonly Color Orange = new Color(204 / 255f, 95 / 255f, 41 / 255f, 1f), GreenTag = new Color(128 / 255f, 212 / 255f, 92 / 255f, 1f);
        static readonly Color Paper = new Color(224 / 255f, 224 / 255f, 200 / 255f, 1f), DimText = new Color(176 / 255f, 175 / 255f, 156 / 255f, 1f);
        static readonly Color Ink = new Color(14 / 255f, 15 / 255f, 15 / 255f, 1f), Plate = new Color(24 / 255f, 23 / 255f, 21 / 255f, 0.92f);
        static ReflexNowPlaying s_inst;

        Canvas _canvas; RectTransform _root, _card; HudRect _plate, _tag, _bar; HudGroup _group;
        Text _kicker, _source, _title, _artist;
        ReflexSong _pending; bool _pendingSkipped; Func<float> _gate; float _pendingSince;
        ReflexSong _song; bool _skipped;
        float _shownAt = -100f; bool _strip, _ride;

        /// The pop-up is on screen (tests) and what it says.
        public static bool Visible => s_inst && s_inst._group != null && s_inst._group.Active;
        public static string Text => Visible ? s_inst._title.text + " / " + s_inst._artist.text : "";
        /// The pop-up has a song waiting for its music to become audible (tests).
        public static bool Waiting => s_inst && s_inst._pending != null;
        /// The pop-up's rectangle on screen in reference units (1920 x 1080 scaled by height; tests).
        public static Rect Bounds => Visible ? s_inst._card.rect : default;

        /// A song began. audible (optional) is the music's level 0 .. 1 now: the pop-up waits for 0.3 (a race's fade-in).
        /// The player whose level gates the pending pop-up went away (OriginalMusic with its world): the pop-up no longer waits for it, and
        /// the gate (a closure over the destroyed world) is let go (w-gcspikes: it kept a world quit early alive in the menus).
        public static void ForgetGate(object owner) { if (s_inst && s_inst._gate != null && ReferenceEquals(s_inst._gate.Target, owner)) s_inst._gate = null; }

        public static void Notify(ReflexSong song, bool skipped, Func<float> audible = null)
        {
            if (song == null || !Application.isPlaying || !ReflexSettings.NowPlayingPopup) return;
            Get().Enqueue(song, skipped, audible);
        }

        /// Shows the pop-up for a song now (tests, the UI showcase): ride = the race's card, else the menus' strip.
        public static void ShowNow(ReflexSong song, bool skipped, bool ride)
        {
            if (song == null || !Application.isPlaying) return;
            var p = Get();
            p._pending = null;
            p._forced = ride ? 1 : 2;
            p.Present(song, skipped);
        }
        int _forced;   // ShowNow's choice (1 card, 2 strip) for the song it shows; 0 = by where the game is

        static ReflexNowPlaying Get()
        {
            if (s_inst) return s_inst;
            var go = new GameObject("Now playing") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            s_inst = go.AddComponent<ReflexNowPlaying>();
            s_inst.Build(go);
            return s_inst;
        }

        void Enqueue(ReflexSong song, bool skipped, Func<float> audible)
        {
            _pending = song; _pendingSkipped = skipped; _gate = audible; _pendingSince = Time.unscaledTime;
        }

        // ------------------------------------------------------------------------------------------------ build
        static readonly Vector2 TL = new Vector2(0, 1), TC = new Vector2(0.5f, 1), BL = Vector2.zero;

        void Build(GameObject go)
        {
            _root = ReflexHudKit.CreateCanvas(go, 45, out _canvas);
            _card = ReflexHudKit.Rect(_root, "Card", BL, BL, Vector2.zero, new Vector2(CardW, CardH));
            _plate = Box("Plate", Plate, true);
            _bar = Box("Bar", Orange, false);
            _tag = Box("Tag", Orange, false);
            _kicker = Lbl("Kicker", ReflexHudKit.CondBold, 21, Ink);
            _source = Lbl("Source", ReflexHudKit.Cond, 21, DimText);
            _title = Lbl("Title", ReflexHudKit.CondBold, 32, Paper);
            _artist = Lbl("Artist", ReflexHudKit.Cond, 22, DimText);
            var sh = _title.gameObject.AddComponent<Shadow>();   // (the front end's hard drop shadow on the big words)
            sh.effectColor = new Color(0, 0, 0, 0.7f); sh.effectDistance = new Vector2(2f, -2f); sh.useGraphicAlpha = true;
            _group = new HudGroup(_card, new Vector2(-32, 0), 0.3f, 0.3f);
        }

        HudRect Box(string name, Color c, bool panel)
        {
            var b = ReflexHudKit.Panel(_card, name, TL, TL, Vector2.zero, Vector2.zero, 0f, 0f, 0f);   // square corners
            b.top = b.bottom = c; b.panel = panel;
            return b;
        }

        Text Lbl(string name, Font font, int size, Color color)
        {
            var t = ReflexHudKit.Label(_card, name, font, size, color, TextAnchor.MiddleLeft, TL, TL, Vector2.zero, new Vector2(100, size + 8));
            t.horizontalOverflow = HorizontalWrapMode.Overflow; t.verticalOverflow = VerticalWrapMode.Overflow;
            return t;
        }

        static void Place(Graphic g, float x, float y, float w, float h)
        {
            var rt = g.rectTransform;
            rt.anchorMin = rt.anchorMax = TL; rt.pivot = TL; rt.anchoredPosition = new Vector2(x, y); rt.sizeDelta = new Vector2(w, h);
        }

        /// text cut with an ellipsis to fit width w (units).
        static void Fit(Text t, string s, float w)
        {
            t.text = s;
            if (t.preferredWidth <= w) return;
            int lo = 0, hi = s.Length;
            while (lo < hi) { int mid = (lo + hi + 1) / 2; t.text = s.Substring(0, mid).TrimEnd() + "…"; if (t.preferredWidth <= w) lo = mid; else hi = mid - 1; }
            t.text = s.Substring(0, lo).TrimEnd() + "…";
        }

        // ------------------------------------------------------------------------------------------------ show
        /// Where it goes: the race's card only over the ride itself; over the menus and over a page drawn on the world, the strip.
        bool WantsCard()
        {
            if (_forced != 0) return _forced == 1;
            var rt = ReflexClientLive.Runtime;
            return rt != null && !rt.OwnMenuActive && !rt.paused && !ReflexOptions.IsOpen;
        }

        void Present(ReflexSong s, bool skipped)
        {
            _song = s; _skipped = skipped;
            _card.gameObject.SetActive(true);   // (measure the text on an active object)
            Layout(WantsCard());
            _shownAt = Time.unscaledTime;
            _group.Snap(false);
            _group.Set(true, 0f, ReflexSettings.ReduceMotion);   // the entrance starts (the object is active from here)
        }

        void Layout(bool ride)
        {
            var s = _song; if (s == null) return;
            _ride = ride; _strip = !ride;
            bool mine = s.source == ReflexSong.SourceCustom;
            Color accent = mine ? GreenTag : Orange;
            _tag.top = _tag.bottom = accent; _tag.SetVerticesDirty();
            _bar.top = _bar.bottom = accent; _bar.SetVerticesDirty();
            _kicker.text = _skipped ? "SKIPPED TO" : "NOW PLAYING";
            string title = string.IsNullOrWhiteSpace(s.title) ? "Unknown song" : s.title.Trim();
            string artist = string.IsNullOrWhiteSpace(s.artist) ? "Unknown artist" : s.artist.Trim();
            string line = string.Join("  ·  ", new[] { artist, string.IsNullOrWhiteSpace(s.album) ? null : s.album.Trim(), s.year > 0 ? s.year.ToString() : null }.Where(x => x != null));
            _source.text = mine ? "YOUR MUSIC" : "THE GAME";
            float kW = _kicker.preferredWidth + 2f, tagW = kW + Pad * 1.5f;
            var rt = _card;
            if (ride)
            {   // [ NOW PLAYING ]  THE GAME
                // |Title
                // |Artist  ·  Album  ·  Year
                _title.fontSize = 32; _artist.fontSize = 22;
                rt.anchorMin = rt.anchorMax = BL; rt.pivot = BL; rt.sizeDelta = new Vector2(CardW, CardH);
                _group.SetBase(new Vector2(ReflexHudKit.Margin, 150f));
                float inner = CardW - Bar - Pad * 2;
                Place(_tag, 0, 0, tagW, TagH); Place(_kicker, Pad * 0.75f, 0, kW, TagH);
                Place(_source, tagW + 10f, 0, CardW - tagW - 10f, TagH); _source.gameObject.SetActive(true);
                Place(_plate, 0, -TagH, CardW, PlateH); Place(_bar, 0, -TagH, Bar, PlateH); _bar.gameObject.SetActive(true);
                Fit(_title, title, inner); Place(_title, Bar + Pad, -TagH - 4f, inner, 36f);
                Fit(_artist, line, inner); Place(_artist, Bar + Pad, -TagH - 38f, inner, 26f);
            }
            else
            {   // one line at the top centre: [ NOW PLAYING ] Title  Artist
                _title.fontSize = 31; _artist.fontSize = 24;
                Fit(_title, title, 620f); Fit(_artist, artist, 380f);
                float tW = _title.preferredWidth + 4f, aW = _artist.preferredWidth + 4f;
                float x = tagW + Pad, w = x + tW + 16f + aW + Pad;
                rt.anchorMin = rt.anchorMax = TC; rt.pivot = TC; rt.sizeDelta = new Vector2(w, StripH);
                _group.SetBase(new Vector2(0, -ReflexHudKit.MarginTop));
                Place(_plate, 0, 0, w, StripH); Place(_tag, 0, 0, tagW, StripH); Place(_kicker, Pad * 0.75f, 0, kW, StripH);
                Place(_title, x, 0, tW, StripH); Place(_artist, x + tW + 16f, 0, aW, StripH);
                _source.gameObject.SetActive(false); _bar.gameObject.SetActive(false);
            }
            _canvas.sortingOrder = ride ? 45 : 150;   // a race: under the pause page (50) and the options (200); the menus and the pages: over the shell (50)
        }

        void Update()
        {
            float now = Time.unscaledTime;
            if (_pending != null)
            {
                if (!ReflexSettings.NowPlayingPopup) { _pending = null; _gate = null; }
                else
                {
                    float level = 1f;
                    if (_gate != null) { try { level = _gate(); } catch (Exception) { level = 1f; } }
                    if (level >= AudibleAt || now - _pendingSince > MaxWait)
                    {
                        var s = _pending; _pending = null; _forced = 0; _gate = null;   // (the gate is the song's player: kept, it held the last race's world, w-gcspikes)
                        Present(s, _pendingSkipped);
                    }
                }
            }
            bool show = now - _shownAt < Hold && ReflexSettings.NowPlayingPopup && !(_ride && ReflexSettings.HudHidden);
            // a page came up over the ride while it shows (the gate selection, PAUSE, the results), or went: it moves to where it does not cover the page
            if (show && _group.Active && _forced == 0 && WantsCard() != _ride) Layout(!_ride);
            _group.Set(show, Time.unscaledDeltaTime, ReflexSettings.ReduceMotion);
        }
    }
}
