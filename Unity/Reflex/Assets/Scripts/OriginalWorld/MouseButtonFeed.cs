// The mouse buttons as the game's Raw Input sees them (w-lookback). Plain C#, no Unity: the frame's physical button states and whether the game
// gets the mouse go in, the RI_MOUSE_*_BUTTON_DOWN / _UP flags to hand it come out (tests/mouse_button_feed/ runs it headless).
//   The game keeps a mouse button held from its DOWN to its UP (Mouse_AccumulateRawInput 00817cb0: held bits set by a down, cleared by an up;
//   there is no per-frame state as for the keyboard and the pads). The original's window procedure hands it every button message, so it always
//   gets the UP. The client stops feeding the mouse while a menu / the pause / the replay or photo viewer / the Dynamic FPV free look has it;
//   it used to drop the button changes of those frames, so a button that went down while riding and came up while the feed was off stayed
//   held in the game for good. The LEFT button is the stock camera's look back (UserCamQuickBack's mouse binding: KeyMap +0x4444, device 4,
//   code 0x1b = the mouse's button 0 record): the camera stayed looking backwards until the next left click (core/tools/lookback_probe.c,
//   mouse_noup). Now:
//   feeding   a press / release of the frame is its DOWN / UP (as before).
//   not       nothing new goes down; every button the game still holds gets its UP at once (the game never keeps a button the client
//             stopped showing it). A button still held when the feed comes back counts only after it is let go and pressed again.
//   Reset     a new session / world: the game holds nothing (no UP owed).
namespace Reflex.Port.OriginalWorld
{
    public sealed class MouseButtonFeed
    {
        // RAWMOUSE usButtonFlags: the down bit of button i is 1 << 2i, its up bit 1 << (2i + 1) (left 0x1 / 0x2, right 0x4 / 0x8, middle 0x10 / 0x20)
        static int Down(int i) => 1 << (2 * i);
        static int Up(int i) => 1 << (2 * i + 1);
        const int Buttons = 3;
        readonly bool[] last = new bool[Buttons];   // the physical states of the last frame
        readonly bool[] game = new bool[Buttons];   // the buttons the game holds (a DOWN sent, its UP not yet)

        /// The flags of this frame. left / right / middle: the physical buttons now; feed: the game gets the mouse this frame.
        public int Tick(bool left, bool right, bool middle, bool feed)
        {
            int flags = 0;
            for (int i = 0; i < Buttons; i++)
            {
                bool now = i == 0 ? left : i == 1 ? right : middle;
                if (feed)
                {
                    if (now != last[i]) { flags |= now ? Down(i) : Up(i); game[i] = now; }
                }
                else if (game[i]) { flags |= Up(i); game[i] = false; }   // the UP the game is owed, now (not when the button comes up)
                last[i] = now;
            }
            return flags;
        }

        /// Whether the game holds this button (0 left, 1 right, 2 middle) after the flags handed out so far.
        public bool GameHolds(int button) => button >= 0 && button < Buttons && game[button];

        /// A new session / world (its input manager holds no button). The buttons physically held stay "already down": they count after a new press.
        public void Reset(bool left, bool right, bool middle)
        {
            for (int i = 0; i < Buttons; i++) { game[i] = false; last[i] = i == 0 ? left : i == 1 ? right : middle; }
        }
    }
}
