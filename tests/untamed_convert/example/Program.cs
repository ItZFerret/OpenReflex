// tests/untamed_convert/example: the shape of a stage part's test (copy this folder as <part>/ and list the stage's sources in sources.txt).
// It opens the inputs, seeds one upstream Python output, and compares a "produced" file with the reference folder.
using System;
using System.IO;
using Reflex.Port.Content.UntamedImport;
using Reflex.Port.Content.UntamedImport.Tests;

static class Program
{
    static int Main(string[] args)
    {
        using (var env = UxTest.OpenEnv(args))
        {
            if (env.disc == null || env.stock == null) { UxTest.Skip("the disc and a Reflex library are needed (UX_DISC_FOLDER / UX_DISC_ISO, UX_REFLEX / UX_CLIENT_LIB)"); return UxTest.Result("example"); }
            foreach (var lv in UxCommon.Levels)
            {
                var ctx = env.Level(lv, "example");
                env.Seed(ctx, "level.json");                              // an upstream stage's output, taken from the Python reference
                // a real stage would run here:  MyStage.Convert(ctx);  it reads ctx.disc / ctx.stock / ctx.work and writes into ctx.work
                var level = ctx.work.ReadJsonObj("level.json");
                ctx.work.WriteJson("copy/level.json", level);
                ctx.work.WriteBytes("copy/first.model", ctx.disc.Read(ctx.disc.Find("model")[0]));
                UxTest.SameBytes(lv.disc + " level.json rewritten", ctx.work.ReadBytes("copy/level.json"), File.ReadAllBytes(Path.Combine(env.PyWork(lv), "level.json")));
                UxTest.Check(env.stock.Get("GL_HayBale_Single", "model") != null && env.stock.templateNames.ContainsKey("sx"), lv.disc + ": stock library and template names");
            }
            UxTest.CompareFolder("Norton level.json against the Python folder", Path.Combine(env.scratch, "example", "Norton_STADIUM", "copy"), env.PyWork(UxCommon.Level("norton")), rel => rel == "level.json");
        }
        return UxTest.Result("example");
    }
}
