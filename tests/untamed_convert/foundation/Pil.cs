// tests/untamed_convert/foundation: UxPil against Pillow (ref_pil.py wrote the digests).
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content.AliveImport;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static partial class Program
{
    static UxImage LcgImage(int h, int w, int c, uint seed, int variant)
    {
        var a = Lcg(h * w * c, seed, 255);
        if (variant == 1) for (int i = 0; i < a.Length; i++) a[i] = (byte)(a[i] / 16 * 16);
        if (variant == 2 && c == 4) for (int i = 3; i < a.Length; i += 4) a[i] = a[i] > 200 ? (byte)255 : a[i] < 40 ? (byte)0 : a[i];
        if (variant == 3) for (int i = 0; i < a.Length; i++) a[i] = 77;
        return new UxImage(a, h, w, c);
    }

    static void Pil(JObj r)
    {
        int bad = 0, n = 0;
        foreach (JObj c in UxJson.Arr(r.Get("resize")).Cast<JObj>())
        {
            var im = LcgImage((int)UxJson.Int(c.Get("h")), (int)UxJson.Int(c.Get("w")), (int)UxJson.Int(c.Get("c")), (uint)UxJson.Int(c.Get("seed")), (int)UxJson.Int(c.Get("variant")));
            string f = (string)c.Get("filter");
            var o = UxPil.Resize(im, (int)UxJson.Int(c.Get("ow")), (int)UxJson.Int(c.Get("oh")), f == "box" ? UxPil.Filter.Box : f == "bilinear" ? UxPil.Filter.Bilinear : UxPil.Filter.Lanczos);
            n++; if (UxBin.Md5(o.data) != (string)c.Get("md5")) { bad++; if (bad <= 6) UxTest.Log("     resize " + im.h + "x" + im.w + "x" + im.c + " -> " + o.h + "x" + o.w + " " + f + " differs"); }
        }
        UxTest.Report(bad == 0, "Pillow " + r.Get("pillow") + " Image.resize (BOX, BILINEAR, LANCZOS; L, RGB, RGBA): " + (n - bad) + " of " + n + " images byte-identical");
        bad = 0; n = 0;
        foreach (JObj c in UxJson.Arr(r.Get("bc")).Cast<JObj>())
        {
            var im = LcgImage((int)UxJson.Int(c.Get("h")), (int)UxJson.Int(c.Get("w")), (int)UxJson.Int(c.Get("c")), (uint)UxJson.Int(c.Get("seed")), (int)UxJson.Int(c.Get("variant")));
            n += 2;
            if (UxBin.Md5(UxPil.BcEncode(im, false)) != (string)c.Get("dxt1")) { bad++; if (bad <= 6) UxTest.Log("     bc_encode DXT1 " + im.h + "x" + im.w + "x" + im.c + " variant " + c.Get("variant") + " differs"); }
            if (UxBin.Md5(UxPil.BcEncode(im, true)) != (string)c.Get("dxt5")) { bad++; if (bad <= 6) UxTest.Log("     bc_encode DXT5 " + im.h + "x" + im.w + "x" + im.c + " variant " + c.Get("variant") + " differs"); }
        }
        UxTest.Report(bad == 0, "rtex.bc_encode (Pillow's DXT1 / DXT5 encoder): " + (n - bad) + " of " + n + " encodings byte-identical");
        bad = 0; n = 0;
        foreach (JObj c in UxJson.Arr(r.Get("chain")).Cast<JObj>())
        {
            int h = (int)UxJson.Int(c.Get("h")), w = (int)UxJson.Int(c.Get("w")); bool dxt5 = UxJson.Bool(c.Get("dxt5"));
            var im = LcgImage(h, w, (int)UxJson.Int(c.Get("c")), (uint)UxJson.Int(c.Get("seed")), 0);
            var parts = new List<byte[]>(); int mips = (int)UxJson.Int(c.Get("mips"));
            for (int i = 0; i < mips; i++)
            {
                int mw = Math.Max(1, w >> i), mh = Math.Max(1, h >> i);
                parts.Add(UxPil.BcEncode(i == 0 ? im : UxPil.Resize(im, mw, mh, UxPil.Filter.Lanczos), dxt5));
            }
            n++; if (UxBin.Md5(UxBin.Concat(parts.ToArray())) != (string)c.Get("md5")) { bad++; UxTest.Log("     mip chain " + h + "x" + w + " " + (dxt5 ? "DXT5" : "DXT1") + " differs"); }
        }
        UxTest.Report(bad == 0, "LANCZOS mip chains encoded level by level: " + (n - bad) + " of " + n + " byte-identical");
    }
}
