using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.RxCore;

namespace Reflex.Port.Content
{
    public static partial class CoreGarageProfile
    {
        static PlayerSnapshotLease playerLease;
        public static string CheckpointError { get; private set; }
        static PlayerProfileMode Mode(bool all) => all ? PlayerProfileMode.Unlocked : PlayerProfileMode.Career;
        static PlayerSnapshotStore PlayerStore(string dir) => new PlayerSnapshotStore(dir);
        static ProfileData ReadMode(bool all, out PlayerSnapshotLease lease)
        {
            if (PlayerStore(SaveDirectory).TryRead(Mode(all), out lease)) return PlayerProfileSnapshotAdapter.ToView(lease.Document);
            if(all && !File.Exists(UnlockedFilePath) && File.Exists(PlayerStore(SaveDirectory).FilePath(Mode(false))))
                throw new InvalidDataException("Unlocked profile is missing. Switch to Career mode, then explicitly enable Unlock Everything to clone it.");
            return all && File.Exists(UnlockedFilePath) ? ReadDocument(UnlockedFilePath) : ReadCareer();
        }
        static string Fingerprint(string path) => File.Exists(path) ? PlayerSnapshotDocument.Hash(File.ReadAllBytes(path)) : "absent";
        static void PublishCheckpoint(string directory, bool all, PlayerSnapshotLease prior, PlayerSnapshotLease next)
        {
            // Captured mode/path is authoritative even if the UI switched modes.
            // Never publish the old mode's view over a newly opened mode or revision.
            if (modeDirectory != directory || unlocked != all || (playerLease?.Digest) != (prior?.Digest)) return;
            var view = PlayerProfileSnapshotAdapter.ToView(next.Document); Validate(view); PopulateProducts(view, all);
            current = view; playerLease = next; loadedPath = next.Path; CheckpointError = null;
        }
        public static IPlayerSessionPersistence CapturePersistence(ReflexWorldFiles.Garage selected = null)
        {
            lock (gate)
            {
                if (!Open()) throw new InvalidOperationException(Error ?? "Player profile unavailable.");
                return new SessionPersistence(SaveDirectory, unlocked, playerLease, PlayerProfileSnapshotAdapter.CloneView(current), selected);
            }
        }
        /// The profile a session started now would import (CapturePersistence: the mode, the saved snapshot's generation and digest): a session
        /// started earlier with an equal stamp imported the same one (the load arena kept warm in the menus, OpenFrontEnd/OpenArenaWarm.cs).
        /// Null when there is no saved snapshot yet (a legacy profile is migrated by the first session: nothing to compare).
        public static string SessionStamp()
        {
            lock (gate)
            {
                if (!IsOpen || playerLease == null) return null;   // (not Open(): a stamp never opens the profile)
                return (unlocked ? "unlocked:" : "career:") + playerLease.Generation + ":" + playerLease.Digest + ":" + playerLease.Path;
            }
        }
        sealed class SessionPersistence : IPlayerSessionPersistence, IPlayerNonterminalPersistence, IPlayerCheckpointReceiptFeedback
        {
            readonly string directory; readonly Dictionary<string,string> legacyInputs = new Dictionary<string,string>(); readonly bool all;
            readonly ProfileData legacy; readonly ReflexWorldFiles.Garage selected;
            PlayerSnapshotLease lease; PlayerSnapshotDocument prepared; bool imported, completed; string receiptNotice;
            public SessionPersistence(string dir, bool all, PlayerSnapshotLease lease, ProfileData legacy, ReflexWorldFiles.Garage selected)
            {
                directory = dir; this.all = all; this.lease = lease; this.legacy = legacy;
                // Do not retain a mutable caller-owned selection object.
                if (selected != null) this.selected = new ReflexWorldFiles.Garage { riderOwner = selected.riderOwner, riderId = selected.riderId, riderGearSet = selected.riderGearSet, vehicleOwner = selected.vehicleOwner, vehicleId = selected.vehicleId };
                if (lease != null) return; // v2 authority never reads stale legacy files.
                string own=Path.Combine(dir,all ? "core-garage-unlocked-v1.json" : "core-garage-v1.json");
                legacyInputs.Add(own,Fingerprint(own));
                if(!File.Exists(own)) {
                    string career=Path.Combine(dir,"core-garage-v1.json");
                    if(!legacyInputs.ContainsKey(career))legacyInputs.Add(career,Fingerprint(career));
                    if(!File.Exists(career)){string original=Path.Combine(dir,"remote","savegame.bxml");legacyInputs.Add(original,Fingerprint(original));}
                }
            }
            void CheckLegacy()
            { if (lease == null) foreach(var item in legacyInputs) if(Fingerprint(item.Key)!=item.Value) throw new InvalidOperationException("Legacy player profile changed while this session was starting."); }
            void CheckCurrentLease()
            {
                if (lease == null) return;
                var latest = PlayerStore(directory).Read(Mode(all));
                if (latest.Generation != lease.Generation || latest.Digest != lease.Digest)
                    throw new InvalidOperationException("Player progress changed while this session was starting. Start the ride again to use the latest saved progress.");
            }
            public byte[] Prepare(byte[] nativeDefaults)
            {
                var wait = System.Diagnostics.Stopwatch.StartNew(); double held;
                lock (gate)
                {
                    double waited = wait.Elapsed.TotalMilliseconds;
                    try
                    {
                        if (prepared != null || imported) throw new InvalidOperationException("Player startup was prepared twice.");
                        CheckCurrentLease(); CheckLegacy(); var baseline = PlayerSnapshotDocument.Decode(nativeDefaults);
                        if (lease != null) { baseline.RequireSameSchema(lease.Document); prepared = lease.Document; }
                        else prepared = PlayerProfileSnapshotAdapter.Migrate(baseline, legacy);
                        var before = PlayerProfileSnapshotAdapter.ToView(prepared); var after = PlayerProfileSnapshotAdapter.CloneView(before);
                        ApplySessionSelection(after, selected); Validate(after);
                        prepared = PlayerProfileSnapshotAdapter.ApplyDiff(prepared, before, after); return prepared.Encode();
                    }
                    finally
                    {
                        // (w-menufps: the menus read the profile under this lock too; a session started in the menus, the load arena's, holds it here)
                        held = wait.Elapsed.TotalMilliseconds - waited;
                        if (held >= 20) UnityEngine.Debug.Log($"[Profile] a session's startup held the profile {held:F0} ms (the profile's reads on the main thread wait meanwhile)");
                    }
                }
            }
            public void Imported(byte[] validatedSnapshot)
            {
                lock (gate)
                {
                    if (prepared == null || imported) throw new InvalidOperationException("Player import receipt is out of order.");
                    var validated = PlayerSnapshotDocument.Decode(validatedSnapshot); prepared.RequireSameSchema(validated);
                    if (PlayerSnapshotDocument.Hash(prepared.Encode()) != PlayerSnapshotDocument.Hash(validated.Encode())) throw new InvalidDataException("Native Player import readback differs.");
                    CheckCurrentLease(); CheckLegacy(); var prior = lease;
                    if (lease == null) lease = PlayerStore(directory).CreateIfAbsent(Mode(all), validated);
                    PublishCheckpoint(directory, all, prior, lease); imported = true;
                }
            }
            public void Checkpoint(byte[] validatedSnapshot) { CommitSnapshot(validatedSnapshot, false); }
            public void Complete(byte[] settledSnapshot) { CommitSnapshot(settledSnapshot, true); }
            void CommitSnapshot(byte[] snapshot, bool terminal)
            {
                lock (gate)
                {
                    if (!imported || completed || lease == null) throw new InvalidOperationException("Player session was not imported or is closed.");
                    var document = PlayerSnapshotDocument.Decode(snapshot);
                    prepared.RequireSameSchema(document);
                    var view = PlayerProfileSnapshotAdapter.ToView(document); Validate(view);
                    var prior = lease;
                    // Commit checks the captured lease; assign only after durable publication succeeds.
                    var next = PlayerStore(directory).Commit(prior, document);
                    lease = next;
                    if (terminal) completed = true;
                    try { PublishCheckpoint(directory, all, prior, next); }
                    catch (Exception e)
                    {
                        // Disk publication already succeeded; never report an unsaved commit
                        // or retry its CAS merely because the optional menu view failed.
                        CheckpointError = "Player progress was saved, but the menu could not refresh: " + e.Message;
                    }
                }
            }
            public void Receipt(string message)
            {
                lock (gate)
                {
                    if (message != null) { receiptNotice = message; CheckpointError = message; }
                    else { if (CheckpointError == receiptNotice) CheckpointError = null; receiptNotice = null; }
                }
            }
            public void Failed(string error) { lock (gate) { CheckpointError = error ?? "Player progress was not saved. Previous profile retained."; } }
        }
        static void ApplySessionSelection(ProfileData p, ReflexWorldFiles.Garage g)
        {
            if (g == null || !g.Equips) return;
            var r = p.T("PlayerRider"); if (r.rows != 1) throw new InvalidDataException("Player selection requires exactly one rider.");
            if (g.riderOwner >= 0) { if (g.riderOwner != 0 && g.riderOwner != 4) throw new InvalidDataException("Unsupported rider owner."); Set(r,"LastRiderId",0,g.riderOwner == 4 ? g.riderId : 0); Set(r,"LastRiderGearSetId",0,g.riderGearSet); }
            if (g.vehicleOwner >= 0) { if (g.vehicleOwner != 0 && g.vehicleOwner != 4 || (r.Int("LastRiderId",0)>0)!=(g.vehicleOwner==4)) throw new InvalidDataException("Rider and vehicle ownership do not match."); Set(r,"LastEquippedVehicle",0,g.vehicleId); }
        }
    }
}
