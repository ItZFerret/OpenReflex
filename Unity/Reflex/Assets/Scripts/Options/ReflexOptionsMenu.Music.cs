// OPTIONS > AUDIO > MUSIC: SOUNDTRACK (ALL SONGS / REFLEX / ALIVE / UNTAMED SONGS ONLY: ReflexSongLibrary.Pool), YOUR MUSIC (OFF / MIX IN / ONLY), the music folder and the NOW PLAYING pop-up, after Open Unleashed's MxaOptionsMenu rows of the same names
// (Content/ReflexCustomMusic.cs, Hud/ReflexNowPlaying.cs).
using Reflex.Port.Content;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        static string SongCount()
        {
            if (!ReflexCustomMusic.Scanned) return "SCANNING";
            int n = ReflexCustomMusic.CountOf(ReflexSong.SourceCustom);   // (your own songs: not the imported soundtracks)
            return n + (n == 1 ? " SONG" : " SONGS");
        }

        /// "  (23 SONGS)" for ALIVE / UNTAMED SONGS ONLY, "  (NOT IMPORTED: REFLEX'S PLAY)" when that game's songs are not there.
        static string SoundtrackCount()
        {
            int st = ReflexSettings.Soundtrack;
            if (st != ReflexSongLibrary.SoundtrackAlive && st != ReflexSongLibrary.SoundtrackUntamed || !ReflexCustomMusic.Scanned) return "";
            int n = ReflexCustomMusic.CountOf(st == ReflexSongLibrary.SoundtrackAlive ? ReflexSong.SourceAlive : ReflexSong.SourceUntamed);
            return n > 0 ? "  (" + n + " SONGS)" : "  (NOT IMPORTED: REFLEX'S PLAY)";
        }

        static void MusicRows(VisualElement panel)
        {
            ReflexCustomMusic.RescanIfOld(2f);   // (songs dropped in since the last look)
            Named(OptUi.Spinner("SOUNDTRACK", () => ReflexSongLibrary.SoundtrackNames[ReflexSettings.Soundtrack] + SoundtrackCount(), d =>
            {
                ReflexSettings.Soundtrack = (ReflexSettings.Soundtrack + d + 4) % 4;
                ReflexSettings.Save();
                ReflexCustomMusic.SettingsChanged();
            }, panel), "soundtrack");
            Note(panel, "Which games' songs play: ALL SONGS mixes Reflex's own tracks with the MX vs ATV Alive and MX vs ATV Untamed soundtracks imported from your discs " +
                        "(GAME LIBRARY); REFLEX, ALIVE or UNTAMED SONGS ONLY plays that game's alone, in the races and, for Alive and Untamed, in the menus too. " +
                        "A game whose soundtrack is not imported plays Reflex's tracks instead. YOUR MUSIC adds your own songs to any of them. Ours.");
            Named(OptUi.Spinner("YOUR MUSIC", () => ReflexCustomMusic.ModeNames[ReflexSettings.CustomMusic] + "  (" + SongCount() + ")", d =>
            {
                ReflexSettings.CustomMusic = (ReflexSettings.CustomMusic + d + 3) % 3;
                ReflexSettings.Save();
                ReflexCustomMusic.SettingsChanged();
            }, panel), "custom-music");
            Note(panel, "Your own songs (.mp3, .ogg, .wav) from the Mods/Music folder beside the game, subfolders too. MIX IN plays them among the SOUNDTRACK's songs in the races (the menus keep the game's themes); " +
                        "ONLY plays them instead of any soundtrack, in the menus and the races. The game's timing stays: the fade-ins, the pause duck, the end of the race. Ours.");
            Named(OptUi.Button("OPEN MUSIC FOLDER", "mxa-link", ReflexCustomMusic.OpenFolder, panel), "music-folder");
            Note(panel, "Opens Mods/Music (made when missing) in the file browser. The songs' title and artist come from their tags, or the file name: \"Artist - Title.mp3\". Ours.");
            Named(OptUi.Toggle("NOW PLAYING POP-UP", () => ReflexSettings.NowPlayingPopup, v => { ReflexSettings.NowPlayingPopup = v; ReflexSettings.Save(); }, panel), "now-playing");
            Note(panel, "The song's title and artist slide in when it starts, the game's tracks too (in a race once the fade-in makes it audible). N plays the next song, in the menus when they play your songs. Ours.");
        }
    }
}
