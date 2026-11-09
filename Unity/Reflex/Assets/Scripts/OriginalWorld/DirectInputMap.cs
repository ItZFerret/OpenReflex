using System.Collections.Generic;
using UnityEngine.InputSystem;

namespace Reflex.Port.OriginalWorld
{
    // Unity Input System keys -> DirectInput scan codes (DIK_*).
    public static class DirectInputMap
    {
        public static readonly KeyValuePair<Key, byte>[] Table = Build();
        static KeyValuePair<Key, byte>[] Build()
        {
            var l = new List<KeyValuePair<Key, byte>>();
            void A(Key k, int d) => l.Add(new KeyValuePair<Key, byte>(k, (byte)d));
            A(Key.Escape, 0x01);
            Key[] digits = { Key.Digit1, Key.Digit2, Key.Digit3, Key.Digit4, Key.Digit5, Key.Digit6, Key.Digit7, Key.Digit8, Key.Digit9, Key.Digit0 };
            for (int i = 0; i < 10; i++) A(digits[i], 0x02 + i);
            A(Key.Minus, 0x0C); A(Key.Equals, 0x0D); A(Key.Backspace, 0x0E); A(Key.Tab, 0x0F);
            A(Key.Q, 0x10); A(Key.W, 0x11); A(Key.E, 0x12); A(Key.R, 0x13); A(Key.T, 0x14); A(Key.Y, 0x15); A(Key.U, 0x16); A(Key.I, 0x17); A(Key.O, 0x18); A(Key.P, 0x19);
            A(Key.LeftBracket, 0x1A); A(Key.RightBracket, 0x1B); A(Key.Enter, 0x1C); A(Key.LeftCtrl, 0x1D);
            A(Key.A, 0x1E); A(Key.S, 0x1F); A(Key.D, 0x20); A(Key.F, 0x21); A(Key.G, 0x22); A(Key.H, 0x23); A(Key.J, 0x24); A(Key.K, 0x25); A(Key.L, 0x26);
            A(Key.Semicolon, 0x27); A(Key.Quote, 0x28); A(Key.Backquote, 0x29); A(Key.LeftShift, 0x2A); A(Key.Backslash, 0x2B);
            A(Key.Z, 0x2C); A(Key.X, 0x2D); A(Key.C, 0x2E); A(Key.V, 0x2F); A(Key.B, 0x30); A(Key.N, 0x31); A(Key.M, 0x32);
            A(Key.Comma, 0x33); A(Key.Period, 0x34); A(Key.Slash, 0x35); A(Key.RightShift, 0x36); A(Key.NumpadMultiply, 0x37);
            A(Key.LeftAlt, 0x38); A(Key.Space, 0x39); A(Key.CapsLock, 0x3A);
            Key[] f = { Key.F1, Key.F2, Key.F3, Key.F4, Key.F5, Key.F6, Key.F7, Key.F8, Key.F9, Key.F10 };
            for (int i = 0; i < 10; i++) A(f[i], 0x3B + i); // F5/F6 are filtered out by the runtime
            A(Key.NumLock, 0x45); A(Key.ScrollLock, 0x46);
            A(Key.Numpad7, 0x47); A(Key.Numpad8, 0x48); A(Key.Numpad9, 0x49); A(Key.NumpadMinus, 0x4A);
            A(Key.Numpad4, 0x4B); A(Key.Numpad5, 0x4C); A(Key.Numpad6, 0x4D); A(Key.NumpadPlus, 0x4E);
            A(Key.Numpad1, 0x4F); A(Key.Numpad2, 0x50); A(Key.Numpad3, 0x51); A(Key.Numpad0, 0x52); A(Key.NumpadPeriod, 0x53);
            A(Key.F11, 0x57); A(Key.F12, 0x58);
            A(Key.NumpadEnter, 0x9C); A(Key.RightCtrl, 0x9D); A(Key.NumpadDivide, 0xB5); A(Key.RightAlt, 0xB8);
            A(Key.Home, 0xC7); A(Key.UpArrow, 0xC8); A(Key.PageUp, 0xC9); A(Key.LeftArrow, 0xCB); A(Key.RightArrow, 0xCD);
            A(Key.End, 0xCF); A(Key.DownArrow, 0xD0); A(Key.PageDown, 0xD1); A(Key.Insert, 0xD2); A(Key.Delete, 0xD3);
            return l.ToArray();
        }
    }
}
