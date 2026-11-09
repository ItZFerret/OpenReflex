using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    // Unity's EditMode runner executes these fixtures serially. Its bundled NUnit
    // profile does not expose the NonParallelizable attribute.
    public class GarageUnlockOverlayTests
    {
        Type core, events;
        object previousTables;
        string previousSave, directory;
        Dictionary<string, List<Dictionary<string, object>>> catalog;
        static Type Find(string name) => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(name)).First(t => t != null);
        object Call(string method, params object[] args) => core.GetMethod(method, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, args);
        string PathOf(string name) => (string)core.GetProperty(name).GetValue(null);
        static Dictionary<string, object> Row(params (string key, object value)[] cells) => cells.ToDictionary(x => x.key, x => x.value is int n ? (object)(double)n : x.value);
        void Add(string table, params Dictionary<string, object>[] rows) => catalog[table] = rows.ToList();

        [SetUp]
        public void Setup()
        {
            core = Find("Reflex.Port.Content.CoreGarageProfile"); events = Find("Reflex.Port.Content.ReflexEvents");
            previousSave = Environment.GetEnvironmentVariable("REFLEX_SAVE_DIR");
            previousTables = events.GetField("_tables", BindingFlags.NonPublic | BindingFlags.Static).GetValue(null);
            directory = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "reflex-unlock-test-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory); Environment.SetEnvironmentVariable("REFLEX_SAVE_DIR", directory); Call("Close");
            catalog = new Dictionary<string, List<Dictionary<string, object>>>();
            Add("PlayerRider", Row(("LastEquippedVehicle", 7), ("LastRiderId", 0)));
            Add("PlayerVehicle", Row(("PlayerVehicleId", 7), ("VehicleModelId", 10), ("Favorite", 1)));
            Add("PlayerVehicleParts", Row(("PlayerVehicleId", 7), ("VehiclePartId", 100)));
            Add("PlayerVehicleTuning", Row(("PlayerVehicleId", 7), ("Engine1", 2), ("Engine2", 3), ("Braking", 50), ("Steering", 50)));
            Add("PlayerRiderGear", Row(("RiderGearId", 200)));
            Add("PlayerAssetsPurchased", Row(("StoreAssetId", 1000)));
            Add("PlayerRecords", Row(("TotalEvents", 42)));
            Add("VehicleModel", Row(("VehicleModelId", 10), ("StoreAssetId", 1000), ("DefaultEngine1", 40), ("DefaultEngine2", 40)), Row(("VehicleModelId", 11), ("StoreAssetId", 1001), ("DefaultEngine1", 40), ("DefaultEngine2", 40)));
            Add("VehicleModelParts", Row(("VehicleModelId", 10), ("VehiclePartId", 100), ("Default", 1), ("StoreAssetId", 1000)), Row(("VehicleModelId", 11), ("VehiclePartId", 101), ("Default", 1), ("StoreAssetId", 1001)));
            Add("VehiclePart", Row(("VehiclePartId", 100), ("VehiclePartTypeId", 1), ("DefaultTuneAttribute1", 2), ("DefaultTuneAttribute2", 3)), Row(("VehiclePartId", 101), ("VehiclePartTypeId", 1), ("DefaultTuneAttribute1", 4), ("DefaultTuneAttribute2", 6)));
            Add("VehiclePartType", Row(("VehiclePartTypeId", 1), ("TuningAttribute", "engine")));
            Add("RiderGear", Row(("RiderGearId", 200), ("RiderGearTypeId", 9), ("StoreAssetId", 1002)), Row(("RiderGearId", 201), ("RiderGearTypeId", 9), ("StoreAssetId", 1003)));
            Add("StoreAssets", Enumerable.Range(0, 5).Select(i => Row(("StoreAssetId", 1000 + i), ("StoreProductId", 9000 + i))).ToArray());
            Add("StoreProducts", Enumerable.Range(0, 6).Select(i => Row(("StoreProductId", 9000 + i), ("Price", 100))).ToArray());
            events.GetField("_tables", BindingFlags.NonPublic | BindingFlags.Static).SetValue(null, catalog);
            var seed = Call("FromRows", catalog.Where(x => x.Key.StartsWith("Player", StringComparison.Ordinal)).ToDictionary(x => x.Key, x => x.Value));
            File.WriteAllText(PathOf("CareerFilePath"), JsonUtility.ToJson(Call("Encode", seed), true));
        }

        [TearDown]
        public void Cleanup()
        {
            Call("Close");
            events.GetField("_tables", BindingFlags.NonPublic | BindingFlags.Static).SetValue(null, previousTables);
            Environment.SetEnvironmentVariable("REFLEX_SAVE_DIR", previousSave);
            if (Directory.Exists(directory)) Directory.Delete(directory, true);
        }
        int Cell(object profile, string table, string column, int row = 0)
        {
            object t = profile.GetType().GetMethod("T").Invoke(profile, new object[] { table });
            return (int)t.GetType().GetMethod("Int").Invoke(t, new object[] { column, row, -1 });
        }

        [Test]
        public void OverlayEditsAndRepeatedTogglesNeverChangeCareerBytesOrSelections()
        {
            byte[] career = File.ReadAllBytes(PathOf("CareerFilePath"));
            Assert.IsTrue((bool)Call("SetUnlockEverything", true));
            object p = Call("Capture");
            Assert.AreEqual(7, Cell(p, "PlayerRider", "LastEquippedVehicle"));
            Assert.AreEqual(42, Cell(p, "PlayerRecords", "TotalEvents"));
            Assert.AreEqual(11, Cell(p, "PlayerVehicle", "VehicleModelId", 1));
            Assert.AreEqual(101, Cell(p, "PlayerVehicleParts", "VehiclePartId", 1));
            Assert.AreEqual(4, Cell(p, "PlayerVehicleTuning", "Engine1", 1));
            Assert.AreEqual(6, Cell(p, "PlayerVehicleTuning", "Engine2", 1));
            Assert.AreEqual(false, p.GetType().GetMethod("IsLocked").Invoke(p, new object[] { 9005 }), "Event/parent products without an asset row are also unlocked");
            Assert.IsTrue((bool)Call("SelectRider", 0, -1, 8));
            Assert.IsTrue((bool)Call("EquipGear", 201));
            Call("Close");
            Assert.IsTrue((bool)core.GetProperty("UnlockEverythingEnabled").GetValue(null), "Mode survives reload");
            Assert.IsTrue((bool)Call("SetUnlockEverything", false));
            p = Call("Capture");
            Assert.AreEqual(7, Cell(p, "PlayerRider", "LastEquippedVehicle"));
            Assert.AreEqual(200, Cell(p, "PlayerRiderGear", "RiderGearId"));
            Assert.AreEqual(true, p.GetType().GetMethod("IsLocked").Invoke(p, new object[] { 9005 }));
            Assert.IsTrue((bool)Call("SetUnlockEverything", true));
            p = Call("Capture");
            Assert.AreEqual(8, Cell(p, "PlayerRider", "LastEquippedVehicle"));
            Assert.AreEqual(201, Cell(p, "PlayerRiderGear", "RiderGearId"));
            CollectionAssert.AreEqual(career, File.ReadAllBytes(PathOf("CareerFilePath")));
            Assert.IsFalse(File.Exists(PathOf("CareerFilePath") + ".bak"), "Even a backup rewrite would indicate touching career");
        }

        [Test]
        public void CorruptDestinationFailsWithoutChangingModeOrCareer()
        {
            byte[] career = File.ReadAllBytes(PathOf("CareerFilePath"));
            Assert.IsTrue((bool)Call("Open"));
            File.WriteAllText(PathOf("UnlockedFilePath"), "broken json");
            Assert.IsFalse((bool)Call("SetUnlockEverything", true));
            Assert.IsFalse((bool)core.GetProperty("UnlockEverythingEnabled").GetValue(null));
            Assert.IsNotEmpty((string)core.GetProperty("Error").GetValue(null));
            CollectionAssert.AreEqual(career, File.ReadAllBytes(PathOf("CareerFilePath")));
            Assert.IsFalse(File.Exists(PathOf("ModeFilePath")));
        }

        [Test]
        public void CorruptModeIsExplicitErrorAndExplicitDisableRecoversOnlyMode()
        {
            byte[] career = File.ReadAllBytes(PathOf("CareerFilePath"));
            File.WriteAllText(PathOf("ModeFilePath"), "broken json");
            Assert.IsFalse((bool)Call("Open"));
            Assert.IsNotEmpty((string)core.GetProperty("Error").GetValue(null));
            Assert.IsTrue((bool)Call("SetUnlockEverything", false));
            Assert.AreEqual(7, Cell(Call("Capture"), "PlayerRider", "LastEquippedVehicle"));
            CollectionAssert.AreEqual(career, File.ReadAllBytes(PathOf("CareerFilePath")));
        }
    }
}
