using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using Reflex.Port.Content;
using Reflex.Port.RxCore;

namespace Reflex.Port.OriginalWorld
{
    // The race's riders as the running game holds them (reflex_world_slots + its run-time tables), read once when the world is
    // made, before the simulation thread owns the plugin:
    //   slot k        player slot P = G+0x7460 + k*0x12a0 with a vehicle; P+0x10 type (0 human, 1 computer)
    //   "Rider List"  RiderId (= P+0x1b8) -> RiderOwner (0..3 a profile, 4 the AI), OwnerRiderId (AIRiderId), OwnerRiderGearSetId
    //   "Vehicle List" VehicleId (= P+0x1b0) -> VehicleOwner, OwnerVehicleId (AIVehicleId, or the profile's PlayerVehicleId)
    //   "Player List" RiderId -> PlayerName
    //   an AI         AIVehicle (-> VehicleModelId) + AIVehicleParts, RiderGearSetGear of its set (AIRider.DefaultRiderGearSetId
    //                 when the list has none)
    //   the local     PlayerVehicle (-> VehicleModelId) + PlayerVehicleParts of OwnerVehicleId, PlayerRiderGear (the profile
    //                 tables; with one local player reflex_world_table answers profile 0's)
    public sealed class ReflexRoster
    {
        public sealed class Entry
        {
            public ReflexSlot slot;
            public bool Local => slot.local != 0;
            public bool Computer => slot.type == 1;
            public string name;
            public int riderOwner = -1, ownerRiderId = -1, gearSetId = -1, vehicleOwner = -1, ownerVehicleId = -1, vehicleModelId = -1;
            public int[] partIds = new int[0], gearIds = new int[0];
            public BikeSpec bike; public RiderSpec rider;
            /// The gear / kit pack pieces drawn on this rider (KitPacks tokens: the local player's choices); empty: stock only.
            public string[] kits = new string[0];
            public ReflexPlateText plate;   // the name / number plates' text and colours as the game made them (ReflexPlates)
            public override string ToString() =>
                $"slot {slot.slot} {(Local ? "local" : Computer ? "AI" : "human")} '{name}' rider {ownerRiderId} gear set {gearSetId} [{string.Join(",", gearIds)}] vehicle {ownerVehicleId} model {vehicleModelId} parts [{string.Join(",", partIds)}]" +
                $" -> bike {(bike == null ? "none" : bike.chassisPkg + " kit " + bike.kitPkg + " tyres " + bike.tirePkg + " + " + string.Join(",", bike.partModels.Select(p => p.pkg)) + " mats " + string.Join(",", bike.materials?.Select(p => p.pkg) ?? new string[0]))}" +
                $" rider {(rider == null ? "none" : string.Join(",", rider.parts.Select(p => p.modelPkg + "/" + p.matsPkg)) + " skin " + rider.skinPkg)}";
        }
        public readonly List<Entry> entries = new List<Entry>();
        public Entry Local => entries.Find(e => e.Local);
        public double milliseconds;

        // Only immutable catalog data comes from the install. The session supplies the chosen model, parts and gear; in
        // particular the install's Player* defaults must never replace the active profile's equipment.
        sealed class InstallSource : ReflexGameTables.ISource
        {
            readonly Dictionary<string, List<Dictionary<string, object>>> tables = ReflexEvents.Tables;
            public int Rows(string table) => tables.TryGetValue(table, out var rows) ? rows.Count : -1;
            public int[] Ints(string table, string column)
            {
                if (!tables.TryGetValue(table, out var rows) || !rows.Any(r => r.ContainsKey(column))) return null;
                return rows.Select(r => Rfx.Int(r, column)).ToArray();
            }
            public string[] Strings(string table, string column)
            {
                if (!tables.TryGetValue(table, out var rows) || !rows.Any(r => r.ContainsKey(column))) return null;
                return rows.Select(r => Rfx.Str(r, column)).ToArray();
            }
        }

        /// Resolves a core startup snapshot on the main thread. Each appearance belongs to the slot at the same index.
        /// The snapshot is read before the worker advances the session; this method never accesses either native world.
        /// Missing metadata or assets fail explicitly, so the model builders cannot silently draw their default outfit.
        public static ReflexRoster ReadOpen(RxSlot[] slots, RxAppearance[] appearances)
        {
            if (slots == null || appearances == null || slots.Length != appearances.Length || slots.Length == 0 || slots.Length > RxRiderCap.Max)
                throw new InvalidOperationException("Open core roster: an aligned slot / appearance snapshot is required.");
            var watch = System.Diagnostics.Stopwatch.StartNew();
            var roster = new ReflexRoster();
            var tables = new ReflexGameTables(new InstallSource());
            var seen = new HashSet<int>();
            for (int i = 0; i < slots.Length; i++)
            {
                var slot = slots[i]; var appearance = appearances[i];
                if (slot.slot < 0 || slot.slot >= RxRiderCap.Max || slot.vehicle == 0 || !seen.Add(slot.slot))
                    throw new InvalidOperationException($"Open core roster: invalid or duplicate slot {slot.slot}.");
                if (appearance.vehicle_model_id < 0 || appearance.part_count <= 0 || appearance.part_count > 17 ||
                    appearance.gear_count <= 0 || appearance.gear_count > 16)
                    throw new InvalidOperationException($"Open core roster: slot {slot.slot} has incomplete appearance metadata.");
                var e = new Entry
                {
                    slot = new ReflexSlot { slot = slot.slot, type = slot.type, local = slot.local, rider_row = slot.rider_row,
                        vehicle_row = slot.vehicle_row, vehicle = slot.vehicle, rider = slot.rider },
                    name = appearance.Name, riderOwner = appearance.rider_owner, ownerRiderId = appearance.owner_rider_id,
                    gearSetId = appearance.gear_set_id, vehicleOwner = appearance.vehicle_owner,
                    ownerVehicleId = appearance.owner_vehicle_id, vehicleModelId = appearance.vehicle_model_id,
                    partIds = appearance.PartIds, gearIds = appearance.GearIds,
                    plate = new ReflexPlateText { name = appearance.PlateName, number = appearance.PlateNumber,
                        bikeDark = appearance.bike_dark != 0, onJersey = appearance.on_jersey != 0, invert = appearance.invert != 0 }
                };
                if (slot.local != 0) e.plate.Own(appearance.vehicle_owner != 4, appearance.rider_owner != 4);   // the player's HIDE NUMBER choices (not on a pro's bike / gear)
                if (e.partIds == null || e.partIds.Length != appearance.part_count || e.gearIds == null || e.gearIds.Length != appearance.gear_count)
                    throw new InvalidOperationException($"Open core roster: slot {slot.slot} appearance arrays disagree with their counts.");
                Resolve(tables, e, slot.slot);
                if (e.Local) ApplyKits(e);   // (w-gearmods) the gear / kit pack pieces the player chose, over the core's stock ones
                roster.entries.Add(e);
            }
            if (roster.entries.Count(e => e.Local) != 1)
                throw new InvalidOperationException("Open core roster: exactly one local rider is required.");
            roster.milliseconds = watch.Elapsed.TotalMilliseconds;
            return roster;
        }

        /// One slot's bike and rider from the core's appearance ids: throws when an id, a package, the tyres or the skin does not resolve.
        static void Resolve(ReflexGameTables tables, Entry e, int slotNo)
        {
            ValidateCatalog(tables, "VehiclePart", "VehiclePartId", e.partIds, slotNo);
            ValidateCatalog(tables, "RiderGear", "RiderGearId", e.gearIds, slotNo);

            // The core writes the plate text as 0053f3f0 does (the profile's PlayerRider Name / Number for the player, the AIRider row's for
            // an AI rider: rx_session_appearance). An AI-owned rider also has an unambiguous immutable AIRider row, so an empty text (an older
            // core) is recovered from that row. A profile rider's text stays as the session supplied it: the install's default PlayerRider
            // row cannot describe a mutable profile.
            if (e.riderOwner == 4)
            {
                int row = tables.Find("AIRider", "AIRiderId", e.ownerRiderId);
                if (string.IsNullOrEmpty(e.name)) e.name = tables.Str("AIRider", "DisplayName", row);
                if (string.IsNullOrEmpty(e.plate.name)) e.plate.name = PlateText(tables.Str("AIRider", "Name", row), 15);
                if (string.IsNullOrEmpty(e.plate.number)) e.plate.number = PlateText(tables.Str("AIRider", "Number", row), 3);
            }
            e.bike = ReflexVehicleSpecs.Bike(tables, e.vehicleModelId, e.partIds);
            e.rider = ReflexVehicleSpecs.Rider(tables, e.gearIds);
            if (e.bike == null || e.rider == null)
                throw new InvalidOperationException($"Open core roster: slot {slotNo} model {e.vehicleModelId} / gear could not be resolved from the install.");
            // These spec classes retain preview defaults for missing tyres / skin. Require the session's own package
            // to have supplied each one, rather than accidentally accepting those defaults as actual equipment.
            if (!e.bike.atv && !e.bike.machine)
            {
                var tires = e.partIds.Select(id => tables.Find("VehiclePart", "VehiclePartId", id))
                    .Where(row => tables.Int("VehiclePart", "VehiclePartTypeId", row) == 8)
                    .Select(row => tables.Package(tables.Int("VehiclePart", "ModelPackageDatabaseId", row))).FirstOrDefault(p => p != null);
                if (tires == null || e.bike.tireDb != tires.Value.db || e.bike.tirePkg != tires.Value.pkg)
                    throw new InvalidOperationException($"Open core roster: slot {slotNo} has no resolved equipped tyre models.");
            }
            var skin = e.gearIds.Select(id => tables.Find("RiderGear", "RiderGearId", id))
                .Where(row => tables.Int("RiderGear", "RiderGearTypeId", row) == ReflexVehicleSpecs.SkinGearType)
                .Select(row => tables.Package(tables.Int("RiderGear", "MaterialPackageDatabaseId", row))).FirstOrDefault(p => p != null);
            if (skin == null || e.rider.skinDb != skin.Value.db || e.rider.skinPkg != skin.Value.pkg)
                throw new InvalidOperationException($"Open core roster: slot {slotNo} has no resolved equipped rider skin.");
        }

        /// (w-gearmods) The gear / kit pack pieces (KitPacks) the local player chose over an entry's stock bike and gear (not while riding a pro
        /// rider: a computer-owned rider / vehicle). The core keeps the stock ids (partIds / gearIds stay); only the specs Unity draws change.
        /// Never throws: a piece that does not build leaves the stock.
        public static void ApplyKits(Entry e)
        {
            try
            {
                if (KitPacks.Current.byId.Count == 0) return;
                var t = KitPacks.Tables(new InstallSource());
                var used = new List<string>();
                bool OwnRider(int owner) => owner >= 0 && owner <= 3;   // (0..3 a profile's, 4 a computer rider's: a pro the player rides)
                if (OwnRider(e.riderOwner) && e.rider != null && e.gearIds.Length > 0)
                {
                    var gear = KitPacks.GearIds(t, e.gearIds);
                    if (!gear.SequenceEqual(e.gearIds))
                    {
                        var r = ReflexVehicleSpecs.Rider(t, gear);
                        if (r != null)
                        {
                            r.plateRider = e.rider.plateRider; e.rider = r;
                            used.AddRange(gear.Select(KitPacks.Item).Where(k => k != null).Select(k => k.Token));
                            // the jersey's InvertTextColors (0054bf50): the pack's jersey when the jersey carries the text
                            var jersey = gear.Select(KitPacks.Item).FirstOrDefault(k => k != null && k.type == 3);
                            if (jersey != null && e.plate != null && e.plate.onJersey) e.plate.invert = jersey.invertText;
                        }
                    }
                }
                if (OwnRider(e.vehicleOwner) && e.bike != null && e.vehicleModelId >= 0)
                {
                    var parts = KitPacks.PartIds(t, e.vehicleModelId, e.partIds);
                    if (!parts.SequenceEqual(e.partIds))
                    {
                        var b = ReflexVehicleSpecs.Bike(t, e.vehicleModelId, parts);
                        if (b != null) { e.bike = b; used.AddRange(parts.Select(KitPacks.Item).Where(k => k != null).Select(k => k.Token)); }
                    }
                }
                e.kits = used.ToArray();
                if (used.Count > 0) Debug.Log($"[KitPacks] the local rider wears {string.Join(", ", used.Select(x => KitPacks.FromToken(x)?.title + " (" + KitPacks.FromToken(x)?.pack.name + ")"))}");
            }
            catch (Exception ex) { Debug.LogWarning("[KitPacks] " + (e.name ?? "rider") + ": the pack pieces not applied (" + ex.Message + "): the stock ones"); }
        }

        static string PlateText(string text, int max) => text == null ? "" : text.Length <= max ? text : text.Substring(0, max);

        static void ValidateCatalog(ReflexGameTables tables, string table, string key, int[] ids, int slot)
        {
            var install = VehicleCatalog.Shared.install;
            foreach (int id in ids)
            {
                int row = tables.Find(table, key, id);
                if (id < 0 || row < 0)
                    throw new InvalidOperationException($"Open core roster: slot {slot} {table} {id} is absent from the install catalog.");
                foreach (string column in new[] { "ModelPackageDatabaseId", "MaterialPackageDatabaseId" })
                {
                    int packageId = tables.Int(table, column, row);
                    if (packageId < 0) continue; // e.g. No Chest Protector has no model or materials.
                    var package = tables.Package(packageId);
                    if (package == null || install.Pkg(package.Value.db, package.Value.pkg) == null)
                        throw new InvalidOperationException($"Open core roster: slot {slot} {table} {id} package {packageId} is unavailable.");
                }
            }
        }

        sealed class WorldSource : ReflexGameTables.ISource
        {
            readonly IntPtr world; readonly Dictionary<string, uint> handles = new Dictionary<string, uint>(StringComparer.Ordinal);
            public WorldSource(IntPtr world) { this.world = world; }
            /// The profile's Player* tables from controller 0's database (0xae10f0) instead of the first table of that name, which is the
            /// static database's default copy (0xae0910). A career world was booted with the save loaded, so its local vehicle / gear are
            /// the save's (PlayerVehicle 4 = a bought ATV ...); an arcade world was booted with the default profile (its controller database
            /// holds the player's save only after ReflexPersistence.Attach, which came later than the vehicle's build). WP-V2.
            public bool profileTables;
            uint Handle(string t) { if (!handles.TryGetValue(t, out uint h)) handles[t] = h = (profileTables && t.StartsWith("Player", StringComparison.Ordinal) && t != "Player List" ? ProfileTable(t) : 0) is uint p && p != 0 ? p : ReflexWorldNative.reflex_world_table(world, t); return h; }
            uint ProfileTable(string name)
            {
                const uint db = 0xae10f0; var b = new byte[64];
                uint U(uint a) { return ReflexWorldNative.reflex_world_read(world, a, b, 4) == 0 ? BitConverter.ToUInt32(b, 0) : 0u; }
                uint count = U(db + 0x1e8); if (count > (0x1e8 - 0x38) / 4) return 0;
                for (uint i = 0; i < count; i++)
                {
                    uint t = U(db + 0x38 + 4 * i); if (t == 0 || ReflexWorldNative.reflex_world_read(world, t + 0x3c, b, 63) != 0) continue;
                    int n = Array.IndexOf(b, (byte)0, 0, 63); if (System.Text.Encoding.ASCII.GetString(b, 0, n < 0 ? 63 : n) == name) return t;
                }
                return 0;
            }
            public int Rows(string table) { uint h = Handle(table); return h == 0 ? -1 : ReflexWorldNative.reflex_world_table_rows(world, h); }
            public int[] Ints(string table, string column)
            {
                uint h = Handle(table); int n = h == 0 ? -1 : ReflexWorldNative.reflex_world_table_rows(world, h);
                if (n < 0) return null;
                var v = new int[n];
                for (int i = 0; i < n; i++) if (ReflexWorldNative.reflex_world_table_int(world, h, column, i, out v[i]) != 0) { if (i == 0) return null; v[i] = -1; }
                return v;
            }
            public string[] Strings(string table, string column)
            {
                uint h = Handle(table); int n = h == 0 ? -1 : ReflexWorldNative.reflex_world_table_rows(world, h);
                if (n < 0) return null;
                var v = new string[n];
                for (int i = 0; i < n; i++) { v[i] = ReflexWorldNative.TableString(world, h, column, i); if (v[i] == null && i == 0) return null; }
                return v;
            }
        }

        /// Reads the riders; the caller must own the plugin (before OriginalSimulation starts). Bikes / riders the tables do not
        /// resolve stay null (the caller's defaults).
        public static ReflexRoster Read(IntPtr world)
        {
            var watch = System.Diagnostics.Stopwatch.StartNew();
            var roster = new ReflexRoster();
            var slots = new ReflexSlot[16];
            int n = ReflexWorldNative.reflex_world_slots(world, slots, (uint)slots.Length);
            if (n <= 0) return roster;
            var t = new ReflexGameTables(new WorldSource(world) { profileTables = ReflexCareer.Current != null });   // MOTOCAREER: the save's tables
            for (int k = 0; k < n; k++)
            {
                var e = new Entry { slot = slots[k] };
                int rr = t.Find("Rider List", "RiderId", e.slot.rider_row), vr = t.Find("Vehicle List", "VehicleId", e.slot.vehicle_row);
                e.riderOwner = t.Int("Rider List", "RiderOwner", rr); e.ownerRiderId = t.Int("Rider List", "OwnerRiderId", rr); e.gearSetId = t.Int("Rider List", "OwnerRiderGearSetId", rr);
                e.vehicleOwner = t.Int("Vehicle List", "VehicleOwner", vr); e.ownerVehicleId = t.Int("Vehicle List", "OwnerVehicleId", vr);
                int pr = t.Find("Player List", "RiderId", e.slot.rider_row);
                e.name = t.Str("Player List", "PlayerName", pr >= 0 ? pr : k) ?? "Rider " + (k + 1);
                try { e.plate = ReflexPlateText.Read(world, e.slot.slot); } catch (Exception x) { Debug.LogWarning($"Reflex roster: slot {e.slot.slot} plates: {x.Message}"); }
                if (e.slot.local != 0) e.plate?.Own(e.vehicleOwner != 4, e.riderOwner != 4);   // the player's HIDE NUMBER choices (not on a pro's bike / gear)
                List<int> parts, gear;
                // by the rows' owners (MXVehicleOwner / MXRiderOwner 4 = AI): a pro chosen on Vehicle Select rides his own AIVehicle
                // and gear set on the local (human) slot (_re/notes/vehicle-select.md)
                bool aiVehicle = e.vehicleOwner == 4 || e.vehicleOwner < 0 && e.Computer, aiRider = e.riderOwner == 4 || e.riderOwner < 0 && e.Computer;
                if (aiVehicle)
                {
                    e.vehicleModelId = t.Int("AIVehicle", "VehicleModelId", t.Find("AIVehicle", "AIVehicleId", e.ownerVehicleId));
                    parts = t.Where("AIVehicleParts", "AIVehicleId", e.ownerVehicleId).Select(r => t.Int("AIVehicleParts", "VehiclePartId", r)).ToList();
                }
                else
                {
                    int pv = t.Find("PlayerVehicle", "PlayerVehicleId", e.ownerVehicleId);
                    if (pv < 0) pv = t.Find("PlayerVehicle", "PlayerVehicleId", t.Int("PlayerRider", "LastEquippedVehicle", 0, 0));
                    if (pv < 0 && t.Rows("PlayerVehicle") > 0) pv = 0;
                    int pvId = t.Int("PlayerVehicle", "PlayerVehicleId", pv);
                    e.vehicleModelId = t.Int("PlayerVehicle", "VehicleModelId", pv);
                    parts = t.Where("PlayerVehicleParts", "PlayerVehicleId", pvId).Select(r => t.Int("PlayerVehicleParts", "VehiclePartId", r)).ToList();
                }
                if (aiRider)
                {
                    if (e.gearSetId < 0) e.gearSetId = t.Int("AIRider", "DefaultRiderGearSetId", t.Find("AIRider", "AIRiderId", e.ownerRiderId));
                    gear = t.Where("RiderGearSetGear", "RiderGearSetId", e.gearSetId).Select(r => t.Int("RiderGearSetGear", "RiderGearId", r)).ToList();
                }
                else
                    gear = e.gearSetId >= 0 ? t.Where("RiderGearSetGear", "RiderGearSetId", e.gearSetId).Select(r => t.Int("RiderGearSetGear", "RiderGearId", r)).ToList()
                                            : t.Ints("PlayerRiderGear", "RiderGearId").ToList();
                e.partIds = parts.ToArray(); e.gearIds = gear.ToArray();
                try { if (e.vehicleModelId >= 0) e.bike = ReflexVehicleSpecs.Bike(t, e.vehicleModelId, parts); }
                catch (Exception x) { Debug.LogWarning($"Reflex roster: slot {e.slot.slot} bike: {x.Message}"); }
                try { if (gear.Count > 0) e.rider = ReflexVehicleSpecs.Rider(t, gear); }
                catch (Exception x) { Debug.LogWarning($"Reflex roster: slot {e.slot.slot} gear: {x.Message}"); }
                roster.entries.Add(e);
            }
            roster.milliseconds = watch.Elapsed.TotalMilliseconds;
            return roster;
        }
    }

}
