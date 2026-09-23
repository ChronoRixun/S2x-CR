using System;
using System.Collections.Generic;

namespace S2x.ServerManager.Models
{
    internal sealed class RotationEntry
    {
        public string Map;
        public string Gametype;

        // The entry as the file had it. Rows are rebuilt on every save, so without this a key
        // this app does not know would be dropped from the rotation it writes back.
        public Dictionary<string, object> Raw;

        public string Label(bool isZombies)
        {
            return GameData.MapName(Map) + " " + GameData.MiddleDot + " " +
                   (isZombies ? "ZM" : GameData.GametypeShort(Gametype));
        }

        public RotationEntry Copy()
        {
            return new RotationEntry
            {
                Map = Map,
                Gametype = Gametype,
                Raw = (Dictionary<string, object>)ServerPreset.CopyValue(Raw),
            };
        }
    }

    /// <summary>
    /// One <game>\s2x\presets\*.json file, written by the PowerShell launcher's Get-CurrentConfig.
    /// A server in this app is a preset plus its port.
    /// </summary>
    internal sealed class ServerPreset
    {
        public const int MaxStartDelay = 120;

        public string FilePath;
        public string FileName;          // base name, what the launcher's preset box shows
        public string ServerName = "";   // sv_hostname, colour codes included
        public string Mode = "mp";
        public int Port = 27016;
        public int BotFill = 12;
        public string BotNames = "nostalgia";
        public bool SingleRoundDom = true;
        public readonly Dictionary<string, int> ScoreLimits = new Dictionary<string, int>(StringComparer.Ordinal);
        public readonly List<RotationEntry> Rotation = new List<RotationEntry>();

        // The editor's keys, alongside the launcher's own in the same file.
        public string BotDifficulty = "regular";
        public int MaxPlayers = 18;
        public int MinPlayers = 1;
        public int StartDelay = 60;
        public bool Advertise = true;
        public bool ShuffleOnLaunch;
        public readonly List<string> ExtraLines = new List<string>();

        // Off the fleet until it is asked for. Still a preset: it keeps its port, its files and
        // its place in the presets folder, and the PowerShell launcher ignores the key.
        public bool Hidden;

        // Every key as it was read, so writing the file back does not lose anything a newer
        // launcher put there.
        public Dictionary<string, object> Raw = new Dictionary<string, object>(StringComparer.Ordinal);

        public bool IsZombies { get { return string.Equals(Mode, "zombies", StringComparison.OrdinalIgnoreCase); } }

        /// <summary>The cap the game allows: a Zombies party is four, a multiplayer one eighteen.</summary>
        public static int CapCeiling(bool zombies) { return zombies ? 4 : 18; }

        public string PlainName
        {
            get
            {
                var name = GameData.StripColorCodes(ServerName).Trim();
                return name.Length > 0 ? name : (FileName ?? "Unnamed server");
            }
        }

        /// <summary>
        /// A copy that shares nothing with this one: the launch snapshot, the editor's candidate,
        /// Save as and + New server all change one without the other seeing it.
        /// </summary>
        public ServerPreset Copy()
        {
            var copy = new ServerPreset
            {
                FilePath = FilePath,
                FileName = FileName,
                ServerName = ServerName,
                Mode = Mode,
                Port = Port,
                BotFill = BotFill,
                BotNames = BotNames,
                SingleRoundDom = SingleRoundDom,
                BotDifficulty = BotDifficulty,
                MaxPlayers = MaxPlayers,
                MinPlayers = MinPlayers,
                StartDelay = StartDelay,
                Advertise = Advertise,
                ShuffleOnLaunch = ShuffleOnLaunch,
                Hidden = Hidden,
                Raw = (Dictionary<string, object>)CopyValue(Raw) ?? new Dictionary<string, object>(StringComparer.Ordinal),
            };
            foreach (var pair in ScoreLimits) copy.ScoreLimits[pair.Key] = pair.Value;
            foreach (var entry in Rotation) copy.Rotation.Add(entry.Copy());
            copy.ExtraLines.AddRange(ExtraLines);
            return copy;
        }

        /// <summary>
        /// A deep copy of what a preset file holds: nested objects and arrays included, because
        /// writing a preset updates the dictionaries it came from.
        /// </summary>
        public static object CopyValue(object value)
        {
            var map = value as Dictionary<string, object>;
            if (map != null)
            {
                var copy = new Dictionary<string, object>(StringComparer.Ordinal);
                foreach (var pair in map) copy[pair.Key] = CopyValue(pair.Value);
                return copy;
            }
            var list = value as object[];
            if (list != null)
            {
                var copy = new object[list.Length];
                for (int i = 0; i < list.Length; i++) copy[i] = CopyValue(list[i]);
                return copy;
            }
            return value;
        }
    }
}
